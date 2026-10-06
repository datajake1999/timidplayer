/*

TiMidity -- Experimental MIDI to WAVE converter
Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

Suddenly, you realize that this program is free software; you get
an overwhelming urge to redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received another copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
I bet they'll be amazed.

mix.c */

#include <math.h>
#include <stdio.h>
#include <malloc.h>

#include "internal.h"

/* Returns 1 if envelope runs out */
int recompute_envelope(Timid *tm, int v)
{
    int stage;
    
    stage = tm->voice[v].envelope_stage;
    
    if (stage>5)
    {
        /* Envelope ran out. */
        int tmp=(tm->voice[v].status == VOICE_DIE); /* Already displayed as dead */
        tm->voice[v].status = VOICE_FREE;
        if(!tmp)
        return 1;
    }
    
    if (tm->voice[v].sample->modes & MODES_ENVELOPE)
    {
        if (tm->voice[v].status==VOICE_ON || tm->voice[v].status==VOICE_SUSTAINED)
        {
            if (stage>2)
            {
                /* Freeze envelope until note turns off. Trumpets want this. */
                tm->voice[v].envelope_increment=0;
                return 0;
            }
        }
    }
    tm->voice[v].envelope_stage=stage+1;
    if (tm->voice[v].envelope_volume==tm->voice[v].sample->envelope_offset[stage] ||
    (stage > 2 && tm->voice[v].envelope_volume <
    tm->voice[v].sample->envelope_offset[stage]))
    return recompute_envelope(tm, v);
    tm->voice[v].envelope_target=tm->voice[v].sample->envelope_offset[stage];
    tm->voice[v].envelope_increment = tm->voice[v].sample->envelope_rate[stage];
    if (tm->voice[v].envelope_target<tm->voice[v].envelope_volume)
    tm->voice[v].envelope_increment = -tm->voice[v].envelope_increment;
    return 0;
}

void apply_envelope_to_amp(Timid *tm, int v)
{
    FLOAT_T lamp=tm->voice[v].left_amp, ramp;
    int32 la,ra;
    if (tm->voice[v].panned == PANNED_MYSTERY)
    {
        ramp=tm->voice[v].right_amp;
        if (tm->voice[v].tremolo_phase_increment)
        {
            lamp *= tm->voice[v].tremolo_volume;
            ramp *= tm->voice[v].tremolo_volume;
        }
        if (tm->voice[v].sample->modes & MODES_ENVELOPE)
        {
            lamp *= vol_table[tm->voice[v].envelope_volume>>23];
            ramp *= vol_table[tm->voice[v].envelope_volume>>23];
        }
        
        la = FSCALE(lamp,AMP_BITS);
        
        if (la>MAX_AMP_VALUE)
        la=MAX_AMP_VALUE;
        
        ra = FSCALE(ramp,AMP_BITS);
        if (ra>MAX_AMP_VALUE)
        ra=MAX_AMP_VALUE;
        
        
        tm->voice[v].left_mix=FINAL_VOLUME(la);
        tm->voice[v].right_mix=FINAL_VOLUME(ra);
    }
    else
    {
        if (tm->voice[v].tremolo_phase_increment)
        lamp *= tm->voice[v].tremolo_volume;
        if (tm->voice[v].sample->modes & MODES_ENVELOPE)
        lamp *= vol_table[tm->voice[v].envelope_volume>>23];
        
        la = FSCALE(lamp,AMP_BITS);
        
        if (la>MAX_AMP_VALUE)
        la=MAX_AMP_VALUE;
        
        tm->voice[v].left_mix=FINAL_VOLUME(la);
    }
}

static int update_envelope(Timid *tm, int v)
{
    tm->voice[v].envelope_volume += tm->voice[v].envelope_increment;
    /* Why is there no ^^ operator?? */
    if (((tm->voice[v].envelope_increment < 0) &&
    (tm->voice[v].envelope_volume <= tm->voice[v].envelope_target)) ||
    ((tm->voice[v].envelope_increment > 0) &&
    (tm->voice[v].envelope_volume >= tm->voice[v].envelope_target)))
    {
        tm->voice[v].envelope_volume = tm->voice[v].envelope_target;
        if (recompute_envelope(tm, v))
        return 1;
    }
    return 0;
}

static void update_tremolo(Timid *tm, int v)
{
    int32 depth=tm->voice[v].sample->tremolo_depth<<7;
    
    if (tm->voice[v].tremolo_sweep)
    {
        /* Update sweep position */
        
        tm->voice[v].tremolo_sweep_position += tm->voice[v].tremolo_sweep;
        if (tm->voice[v].tremolo_sweep_position>=(1<<SWEEP_SHIFT))
        tm->voice[v].tremolo_sweep=0; /* Swept to max amplitude */
        else
        {
            /* Need to adjust depth */
            depth *= tm->voice[v].tremolo_sweep_position;
            depth >>= SWEEP_SHIFT;
        }
    }
    
    tm->voice[v].tremolo_phase += tm->voice[v].tremolo_phase_increment;
    
    /* if (tm->voice[v].tremolo_phase >= (SINE_CYCLE_LENGTH<<RATE_SHIFT))
    tm->voice[v].tremolo_phase -= SINE_CYCLE_LENGTH<<RATE_SHIFT;  */
    
    tm->voice[v].tremolo_volume =
    1.0 - FSCALENEG((sine(tm->voice[v].tremolo_phase >> RATE_SHIFT) + 1.0)
    * depth * TREMOLO_AMPLITUDE_TUNING,
    17);
    
    /* I'm not sure about the +1.0 there -- it makes tremoloed voices'
    volumes on average the lower the higher the tremolo amplitude. */
}

/* Returns 1 if the note died */
static int update_signal(Timid *tm, int v)
{
    if (tm->voice[v].envelope_increment && update_envelope(tm, v))
    return 1;
    
    if (tm->voice[v].tremolo_phase_increment)
    update_tremolo(tm, v);
    
    apply_envelope_to_amp(tm, v);
    return 0;
}

#ifdef LOOKUP_HACK
#  define MIXATION(a) *lp++ += tm->mixup[(a<<8) | (uint8)s];
#else
#  define MIXATION(a) *lp++ += (a)*s;
#endif

static void mix_mystery_signal(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    Voice *vp = tm->voice + v;
    final_volume_t
    left=vp->left_mix,
    right=vp->right_mix;
    int cc;
    sample_t s;
    
    if (!(cc = vp->control_counter))
    {
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
        right = vp->right_mix;
    }
    
    while (count)
    if (cc < count)
    {
        count -= cc;
        while (cc--)
        {
            s = *sp++;
            MIXATION(left);
            MIXATION(right);
        }
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
        right = vp->right_mix;
    }
    else
    {
        vp->control_counter = cc - count;
        while (count--)
        {
            s = *sp++;
            MIXATION(left);
            MIXATION(right);
        }
        return;
    }
}

static void mix_center_signal(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    Voice *vp = tm->voice + v;
    final_volume_t
    left=vp->left_mix;
    int cc;
    sample_t s;
    
    if (!(cc = vp->control_counter))
    {
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
    }
    
    while (count)
    if (cc < count)
    {
        count -= cc;
        while (cc--)
        {
            s = *sp++;
            MIXATION(left);
            MIXATION(left);
        }
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
    }
    else
    {
        vp->control_counter = cc - count;
        while (count--)
        {
            s = *sp++;
            MIXATION(left);
            MIXATION(left);
        }
        return;
    }
}

static void mix_single_signal(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    Voice *vp = tm->voice + v;
    final_volume_t
    left=vp->left_mix;
    int cc;
    sample_t s;
    
    if (!(cc = vp->control_counter))
    {
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
    }
    
    while (count)
    if (cc < count)
    {
        count -= cc;
        while (cc--)
        {
            s = *sp++;
            MIXATION(left);
            lp++;
        }
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
    }
    else
    {
        vp->control_counter = cc - count;
        while (count--)
        {
            s = *sp++;
            MIXATION(left);
            lp++;
        }
        return;
    }
}

static void mix_mono_signal(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    Voice *vp = tm->voice + v;
    final_volume_t
    left=vp->left_mix;
    int cc;
    sample_t s;
    
    if (!(cc = vp->control_counter))
    {
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
    }
    
    while (count)
    if (cc < count)
    {
        count -= cc;
        while (cc--)
        {
            s = *sp++;
            MIXATION(left);
        }
        cc = tm->control_ratio;
        if (update_signal(tm, v))
        return;	/* Envelope ran out */
        left = vp->left_mix;
    }
    else
    {
        vp->control_counter = cc - count;
        while (count--)
        {
            s = *sp++;
            MIXATION(left);
        }
        return;
    }
}

static void mix_mystery(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    final_volume_t
    left=tm->voice[v].left_mix,
    right=tm->voice[v].right_mix;
    sample_t s;
    
    while (count--)
    {
        s = *sp++;
        MIXATION(left);
        MIXATION(right);
    }
}

static void mix_center(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    final_volume_t
    left=tm->voice[v].left_mix;
    sample_t s;
    
    while (count--)
    {
        s = *sp++;
        MIXATION(left);
        MIXATION(left);
    }
}

static void mix_single(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    final_volume_t
    left=tm->voice[v].left_mix;
    sample_t s;
    
    while (count--)
    {
        s = *sp++;
        MIXATION(left);
        lp++;
    }
}

static void mix_mono(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    final_volume_t
    left=tm->voice[v].left_mix;
    sample_t s;
    
    while (count--)
    {
        s = *sp++;
        MIXATION(left);
    }
}

/* Ramp a note out in c samples */
static void ramp_out(Timid *tm, sample_t *sp, int32 *lp, int v, int32 c)
{
    
    /* should be final_volume_t, but uint8 gives trouble. */
    int32 left, right, li, ri;
    
    sample_t s=0; /* silly warning about uninitialized s */
    
    left=tm->voice[v].left_mix;
    li=-(left/c);
    if (!li) li=-1;
    
    /* printf("Ramping out: left=%d, c=%d, li=%d\n", left, c, li); */
    
    if (!(tm->play_mode.encoding & PE_MONO))
    {
        if (tm->voice[v].panned==PANNED_MYSTERY)
        {
            right=tm->voice[v].right_mix;
            ri=-(right/c);
            while (c--)
            {
                left += li;
                if (left<0)
                left=0;
                right += ri;
                if (right<0)
                right=0;
                s=*sp++;
                MIXATION(left);
                MIXATION(right);
            }
        }
        else if (tm->voice[v].panned==PANNED_CENTER)
        {
            while (c--)
            {
                left += li;
                if (left<0)
                return;
                s=*sp++;
                MIXATION(left);
                MIXATION(left);
            }
        }
        else if (tm->voice[v].panned==PANNED_LEFT)
        {
            while (c--)
            {
                left += li;
                if (left<0)
                return;
                s=*sp++;
                MIXATION(left);
                lp++;
            }
        }
        else if (tm->voice[v].panned==PANNED_RIGHT)
        {
            while (c--)
            {
                left += li;
                if (left<0)
                return;
                s=*sp++;
                lp++;
                MIXATION(left);
            }
        }
    }
    else
    {
        /* Mono output.  */
        while (c--)
        {
            left += li;
            if (left<0)
            return;
            s=*sp++;
            MIXATION(left);
        }
    }
}

static void mix_reverb_send(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    Voice *vp = tm->voice + v;
    int32 channel_reverb = tm->channel[vp->channel].reverb;
    final_volume_t gain;
    sample_t s;
    if (channel_reverb <= 0 || count <= 0)
    {
        return;
    }
    if (vp->panned == PANNED_MYSTERY)
    {
        gain = (vp->left_mix + vp->right_mix) / 2;
    }
    else
    {
        gain = vp->left_mix;
    }
    gain = (gain * channel_reverb) / 127;
    if (!gain)
    {
        return;
    }
    while (count--)
    {
        s = *sp++;
        MIXATION(gain);
    }
}

static void mix_chorus_send(Timid *tm, sample_t *sp, int32 *lp, int v, int count)
{
    Voice *vp = tm->voice + v;
    int32 channel_chorus = tm->channel[vp->channel].chorus;
    final_volume_t gain;
    sample_t s;
    if (channel_chorus <= 0 || count <= 0)
    {
        return;
    }
    if (vp->panned == PANNED_MYSTERY)
    {
        gain = (vp->left_mix + vp->right_mix) / 2;
    }
    else
    {
        gain = vp->left_mix;
    }
    gain = (gain * channel_chorus) / 127;
    if (!gain)
    {
        return;
    }
    while (count--)
    {
        s = *sp++;
        MIXATION(gain);
    }
}

/**************** interface function ******************/

void mix_voice(Timid *tm, int32 *buf, int v, int32 c)
{
    Voice *vp=tm->voice+v;
    sample_t *sp;
    if (vp->status==VOICE_DIE)
    {
		if (c>=MAX_DIE_TIME)
			c=MAX_DIE_TIME;
		sp=resample_voice(tm, v, &c);
		if(c > 0)
			ramp_out(tm, sp, buf, v, c);
		vp->status=VOICE_FREE;
    }
    else
    {
        sp=resample_voice(tm, v, &c);
        if (tm->play_mode.encoding & PE_MONO)
        {
            /* Mono output. */
            if (vp->envelope_increment || vp->tremolo_phase_increment)
            mix_mono_signal(tm, sp, buf, v, c);
            else
            mix_mono(tm, sp, buf, v, c);
        }
        else
        {
            if (vp->panned == PANNED_MYSTERY)
            {
                if (vp->envelope_increment || vp->tremolo_phase_increment)
                mix_mystery_signal(tm, sp, buf, v, c);
                else
                mix_mystery(tm, sp, buf, v, c);
            }
            else if (vp->panned == PANNED_CENTER)
            {
                if (vp->envelope_increment || vp->tremolo_phase_increment)
                mix_center_signal(tm, sp, buf, v, c);
                else
                mix_center(tm, sp, buf, v, c);
            }
            else
            {
                /* It's either full left or full right. In either case,
                every other sample is 0. Just get the offset right: */
                if (vp->panned == PANNED_RIGHT) buf++;
                
                if (vp->envelope_increment || vp->tremolo_phase_increment)
                mix_single_signal(tm, sp, buf, v, c);
                else
                mix_single(tm, sp, buf, v, c);
            }
        }
        if (tm->reverb_enabled)
        {
            mix_reverb_send(tm, sp, tm->reverb_send_buffer, v, c);
        }
        if (tm->chorus_enabled)
        {
            mix_chorus_send(tm, sp, tm->chorus_send_buffer, v, c);
        }
    }
}

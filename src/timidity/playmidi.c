/*

TiMidity -- Experimental MIDI to WAVE converter
Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

playmidi.c -- random stuff in need of rearrangement

*/

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32_WCE
#include <string.h>
#endif

#include "internal.h"

static void adjust_amplification(Timid *tm, int amplification)
{
    tm->master_volume = (double)(amplification) / 100.0L;
}

static void reset_voices(Timid *tm)
{
    int i;
    for (i=0; i<MAX_VOICES; i++)
		tm->voice[i].status=VOICE_FREE;
}

/* Process the Reset All Controllers event */
static void reset_controllers(Timid *tm, int c)
{
    tm->channel[c].volume=90; /* Some standard says, although the SCC docs say 0. */
    tm->channel[c].expression=127; /* SCC-1 does this. */
    tm->channel[c].sustain=0;
    tm->channel[c].mono=0;
    tm->channel[c].pitchbend=0x2000;
    tm->channel[c].pitchfactor=0; /* to be computed */
    tm->channel[c].modulation=0;
    tm->channel[c].reverb=40;
    tm->channel[c].chorus=0;
}

static void reset_midi(Timid *tm)
{
    int i;
    for (i=0; i<16; i++)
    {
        reset_controllers(tm, i);
        /* The rest of these are unaffected by the Reset All Controllers event */
        tm->channel[i].program=tm->default_program;
        tm->channel[i].panning=NO_PANNING;
        tm->channel[i].pitchsens=2;
        tm->channel[i].bank=0; /* tone bank or drum set */
        tm->rpn_msb[i]=0xff;
        tm->rpn_lsb[i]=0xff;
    }
    reset_voices(tm);
    tm->lost_notes = 0;
    tm->cut_notes = 0;
    if (tm->reverb_enabled)
    {
        reset_reverb(tm);
    }
    if (tm->chorus_enabled)
    {
        reset_chorus(tm);
    }
    tm->prng = seedRand(1);
}

static void select_sample(Timid *tm, int v, Instrument *ip)
{
    int32 f, cdiff, diff;
    int s,i;
    Sample *sp, *closest;
    
    s=ip->samples;
    sp=ip->sample;
    
    if (s==1)
    {
        tm->voice[v].sample=sp;
        return;
    }
    
    f=tm->voice[v].orig_frequency;
    for (i=0; i<s; i++)
    {
        if (sp->low_freq <= f && sp->high_freq >= f)
        {
            tm->voice[v].sample=sp;
            return;
        }
        sp++;
    }
    
    /*
    No suitable sample found! We'll select the sample whose root
    frequency is closest to the one we want. (Actually we should
    probably convert the low, high, and root frequencies to MIDI note
    values and compare those.) */
    
    cdiff=0x7FFFFFFF;
    closest=sp=ip->sample;
    for(i=0; i<s; i++)
    {
        diff=sp->root_freq - f;
        if (diff<0) diff=-diff;
        if (diff<cdiff)
        {
            cdiff=diff;
            closest=sp;
        }
        sp++;
    }
    tm->voice[v].sample=closest;
    return;
}

static void recompute_freq(Timid *tm, int v)
{
    int
    sign=(tm->voice[v].sample_increment < 0), /* for bidirectional loops */
    pb=tm->channel[tm->voice[v].channel].pitchbend;
    double a;
    
    if (!tm->voice[v].sample->sample_rate)
    return;
    
    if (tm->voice[v].vibrato_control_ratio)
    {
        /* This instrument has vibrato. Invalidate any precomputed
        sample_increments. */
        
        int i=VIBRATO_SAMPLE_INCREMENTS;
        while (i--)
        tm->voice[v].vibrato_sample_increment[i]=0;
    }
    
    if (pb==0x2000 || pb<0 || pb>0x3FFF)
    tm->voice[v].frequency=tm->voice[v].orig_frequency;
    else
    {
        pb-=0x2000;
        if (!(tm->channel[tm->voice[v].channel].pitchfactor))
        {
            /* Damn. Somebody bent the pitch. */
            int32 i=pb*tm->channel[tm->voice[v].channel].pitchsens;
            if (pb<0)
            i=-i;
            tm->channel[tm->voice[v].channel].pitchfactor=
            bend_fine[(i>>5) & 0xFF] * bend_coarse[i>>13];
        }
        if (pb>0)
        tm->voice[v].frequency=
        (int32)(tm->channel[tm->voice[v].channel].pitchfactor *
        (double)(tm->voice[v].orig_frequency));
        else
        tm->voice[v].frequency=
        (int32)((double)(tm->voice[v].orig_frequency) /
        tm->channel[tm->voice[v].channel].pitchfactor);
    }
    
    a = FSCALE(((double)(tm->voice[v].sample->sample_rate) *
    (double)(tm->voice[v].frequency)) /
    ((double)(tm->voice[v].sample->root_freq) *
    (double)(tm->play_mode.rate)),
    FRACTION_BITS);
    
    if (sign)
    a = -a; /* need to preserve the loop direction */
    
    tm->voice[v].sample_increment = (int32)(a);
}

static void compute_vibrato_ratio(Timid *tm, int v)
{
    int ratio=tm->voice[v].sample->vibrato_control_ratio;
    if (!ratio && tm->voice[v].sample->sample_rate && tm->channel[tm->voice[v].channel].modulation > 0)
    {
        ratio=tm->play_mode.rate / (MOD_VIBRATO_HZ * 2 * VIBRATO_SAMPLE_INCREMENTS);
        if (ratio<1) ratio=1;
    }
    tm->voice[v].vibrato_control_ratio=ratio;
}

static void recompute_amp(Timid *tm, int v)
{
    int32 tempamp;
    
    /* TODO: use fscale */
    
    tempamp= (tm->voice[v].velocity *
    tm->channel[tm->voice[v].channel].volume *
    tm->channel[tm->voice[v].channel].expression); /* 21 bits */
    
    if (!(tm->play_mode.encoding & PE_MONO))
    {
        if (tm->voice[v].panning > 60 && tm->voice[v].panning < 68)
        {
            tm->voice[v].panned=PANNED_CENTER;
            
            tm->voice[v].left_amp=
            FSCALENEG((double)(tempamp) * tm->voice[v].sample->volume * tm->master_volume,
            21);
        }
        else if (tm->voice[v].panning<5)
        {
            tm->voice[v].panned = PANNED_LEFT;
            
            tm->voice[v].left_amp=
            FSCALENEG((double)(tempamp) * tm->voice[v].sample->volume * tm->master_volume,
            20);
        }
        else if (tm->voice[v].panning>123)
        {
            tm->voice[v].panned = PANNED_RIGHT;
            
            tm->voice[v].left_amp= /* left_amp will be used */
            FSCALENEG((double)(tempamp) * tm->voice[v].sample->volume * tm->master_volume,
            20);
        }
        else
        {
            tm->voice[v].panned = PANNED_MYSTERY;
            
            tm->voice[v].left_amp=
            FSCALENEG((double)(tempamp) * tm->voice[v].sample->volume * tm->master_volume,
            27);
            tm->voice[v].right_amp=tm->voice[v].left_amp * (tm->voice[v].panning);
            tm->voice[v].left_amp *= (double)(127-tm->voice[v].panning);
        }
    }
    else
    {
        tm->voice[v].panned=PANNED_CENTER;
        
        tm->voice[v].left_amp=
        FSCALENEG((double)(tempamp) * tm->voice[v].sample->volume * tm->master_volume,
        21);
    }
}

static void start_note(Timid *tm, MidiEvent *e, int i)
{
    Instrument *ip;
    int j;
    
    if (ISDRUMCHANNEL(tm, e->channel))
    {
        if (!tm->drumset[tm->channel[e->channel].bank] && !tm->drumset[0])
        return; /* No drumset? Then we can't play. */
        if (tm->drumset[tm->channel[e->channel].bank]->tone[e->a].name && !tm->drumset[tm->channel[e->channel].bank]->tone[e->a].instrument)
        {
            tm->drumset[tm->channel[e->channel].bank]->tone[e->a].instrument=MAGIC_LOAD_INSTRUMENT;
            load_missing_instruments(tm);
        }
        if (tm->drumset[0]->tone[e->a].name && !tm->drumset[0]->tone[e->a].instrument)
        {
            tm->drumset[0]->tone[e->a].instrument=MAGIC_LOAD_INSTRUMENT;
            load_missing_instruments(tm);
        }
        if (!(ip=tm->drumset[tm->channel[e->channel].bank]->tone[e->a].instrument))
        {
            if (!(ip=tm->drumset[0]->tone[e->a].instrument))
            return; /* No instrument? Then we can't play. */
        }
        
        if (ip->sample->note_to_use) /* Do we have a fixed pitch? */
        tm->voice[i].orig_frequency=freq_table[(int)(ip->sample->note_to_use)];
        else
        tm->voice[i].orig_frequency=freq_table[e->a & 0x7F];
        
        /* drums are supposed to have only one sample */
        tm->voice[i].sample=ip->sample;
    }
    else
    {
        if (!tm->tonebank[tm->channel[e->channel].bank] && !tm->tonebank[0] && tm->channel[e->channel].program!=SPECIAL_PROGRAM)
        return; /* No tonebank? Then we can't play. */
        if (tm->channel[e->channel].program!=SPECIAL_PROGRAM)
        {
            if (tm->tonebank[tm->channel[e->channel].bank]->tone[tm->channel[e->channel].program].name && !tm->tonebank[tm->channel[e->channel].bank]->tone[tm->channel[e->channel].program].instrument)
            {
                tm->tonebank[tm->channel[e->channel].bank]->tone[tm->channel[e->channel].program].instrument=MAGIC_LOAD_INSTRUMENT;
                load_missing_instruments(tm);
            }
            if (tm->tonebank[0]->tone[tm->channel[e->channel].program].name && !tm->tonebank[0]->tone[tm->channel[e->channel].program].instrument)
            {
                tm->tonebank[0]->tone[tm->channel[e->channel].program].instrument=MAGIC_LOAD_INSTRUMENT;
                load_missing_instruments(tm);
            }
        }
        if (tm->channel[e->channel].program==SPECIAL_PROGRAM)
        ip=tm->default_instrument;
        else if (!(ip=tm->tonebank[tm->channel[e->channel].bank]->
        tone[tm->channel[e->channel].program].instrument))
        {
            if (!(ip=tm->tonebank[0]->tone[tm->channel[e->channel].program].instrument))
            return; /* No instrument? Then we can't play. */
        }
        
        if (!ip) return;
        
        if (ip->sample->note_to_use) /* Fixed-pitch instrument? */
        tm->voice[i].orig_frequency=freq_table[(int)(ip->sample->note_to_use)];
        else
        tm->voice[i].orig_frequency=freq_table[e->a & 0x7F];
        select_sample(tm, i, ip);
    }
    
    tm->voice[i].status=VOICE_ON;
    tm->voice[i].channel=e->channel;
    tm->voice[i].note=e->a;
    tm->voice[i].velocity=e->b;
    tm->voice[i].sample_offset=0;
    tm->voice[i].sample_increment=0; /* make sure it isn't negative */
    
    tm->voice[i].tremolo_phase=0;
    tm->voice[i].tremolo_phase_increment=tm->voice[i].sample->tremolo_phase_increment;
    tm->voice[i].tremolo_sweep=tm->voice[i].sample->tremolo_sweep_increment;
    tm->voice[i].tremolo_sweep_position=0;
    
    tm->voice[i].vibrato_sweep=tm->voice[i].sample->vibrato_sweep_increment;
    tm->voice[i].vibrato_sweep_position=0;
    compute_vibrato_ratio(tm, i);
    tm->voice[i].vibrato_control_counter=tm->voice[i].vibrato_phase=0;
    for (j=0; j<VIBRATO_SAMPLE_INCREMENTS; j++)
    tm->voice[i].vibrato_sample_increment[j]=0;
    
    if (tm->channel[e->channel].panning != NO_PANNING)
    tm->voice[i].panning=tm->channel[e->channel].panning;
    else
    tm->voice[i].panning=tm->voice[i].sample->panning;
    
    recompute_freq(tm, i);
    recompute_amp(tm, i);
    if (tm->voice[i].sample->modes & MODES_ENVELOPE)
    {
        /* Ramp up from 0 */
        tm->voice[i].envelope_stage=0;
        tm->voice[i].envelope_volume=0;
        tm->voice[i].control_counter=0;
        recompute_envelope(tm, i);
        apply_envelope_to_amp(tm, i);
    }
    else
    {
        tm->voice[i].envelope_increment=0;
        apply_envelope_to_amp(tm, i);
    }
}

static void kill_note(Timid *tm, int i)
{
    tm->voice[i].status=VOICE_DIE;
}

/* Only one instance of a note can be playing on a single channel. */
static void note_on(Timid *tm, MidiEvent *e)
{
    int i=tm->voices, lowest=-1;
    int32 lv=0x7FFFFFFF, v;
    
    while (i--)
    {
        if (tm->voice[i].status == VOICE_FREE)
        lowest=i; /* Can't get a lower volume than silence */
        else if (tm->voice[i].channel==e->channel &&
        (tm->voice[i].note==e->a || tm->channel[tm->voice[i].channel].mono))
        kill_note(tm, i);
    }
    
    if (lowest != -1)
    {
        /* Found a free voice. */
        start_note(tm,e,lowest);
        return;
    }
    
    /* Look for the decaying note with the lowest volume */
    i=tm->voices;
    while (i--)
    {
        if ((tm->voice[i].status!=VOICE_ON) &&
        (tm->voice[i].status!=VOICE_DIE))
        {
            v=tm->voice[i].left_mix;
            if ((tm->voice[i].panned==PANNED_MYSTERY) && (tm->voice[i].right_mix>v))
            v=tm->voice[i].right_mix;
            if (v<lv)
            {
                lv=v;
                lowest=i;
            }
        }
    }
    
    if (lowest != -1)
    {
        /* This can still cause a click, but if we had a free voice to
        spare for ramping down this note, we wouldn't need to kill it
        in the first place... Still, this needs to be fixed. Perhaps
        we could use a reserve of voices to play dying notes only. */
        
        tm->cut_notes++;
        tm->voice[lowest].status=VOICE_FREE;
        start_note(tm,e,lowest);
    }
    else
    tm->lost_notes++;
}

static void finish_note(Timid *tm, int i)
{
    if (tm->voice[i].sample->modes & MODES_ENVELOPE)
    {
        /* We need to get the envelope out of Sustain stage */
        tm->voice[i].envelope_stage=3;
        tm->voice[i].status=VOICE_OFF;
        recompute_envelope(tm, i);
        apply_envelope_to_amp(tm, i);
    }
    else
    {
        /* Set status to OFF so resample_voice() will let this voice out
        of its loop, if any. In any case, this voice dies when it
        hits the end of its data (ofs>=data_length). */
        tm->voice[i].status=VOICE_OFF;
    }
}

static void note_off(Timid *tm, MidiEvent *e)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].status==VOICE_ON &&
    tm->voice[i].channel==e->channel &&
    tm->voice[i].note==e->a)
    {
        if (tm->channel[e->channel].sustain)
        {
            tm->voice[i].status=VOICE_SUSTAINED;
        }
        else
        finish_note(tm, i);
        return;
    }
}

/* Process the All Notes Off event */
static void all_notes_off(Timid *tm, int c)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].status==VOICE_ON &&
    tm->voice[i].channel==c)
    {
        if (tm->channel[c].sustain)
        {
            tm->voice[i].status=VOICE_SUSTAINED;
        }
        else
        finish_note(tm, i);
    }
}

/* Process the All Sounds Off event */
static void all_sounds_off(Timid *tm, int c)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].channel==c &&
    tm->voice[i].status != VOICE_FREE &&
    tm->voice[i].status != VOICE_DIE)
    {
        kill_note(tm, i);
    }
}

static void adjust_pressure(Timid *tm, MidiEvent *e)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].status==VOICE_ON &&
    tm->voice[i].channel==e->channel &&
    tm->voice[i].note==e->a)
    {
        tm->voice[i].velocity=e->b;
        recompute_amp(tm, i);
        apply_envelope_to_amp(tm, i);
        return;
    }
}

static void adjust_panning(Timid *tm, int c)
{
    int i=tm->voices;
    while (i--)
    if ((tm->voice[i].channel==c) &&
    (tm->voice[i].status==VOICE_ON || tm->voice[i].status==VOICE_SUSTAINED))
    {
        tm->voice[i].panning=tm->channel[c].panning;
        recompute_amp(tm, i);
        apply_envelope_to_amp(tm, i);
    }
}

static void drop_sustain(Timid *tm, int c)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].status==VOICE_SUSTAINED && tm->voice[i].channel==c)
    finish_note(tm, i);
}

static void adjust_pitchbend(Timid *tm, int c)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].status!=VOICE_FREE && tm->voice[i].channel==c)
    {
        recompute_freq(tm, i);
    }
}

static void adjust_modulation(Timid *tm, int c)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].status!=VOICE_FREE && tm->voice[i].channel==c)
    {
        compute_vibrato_ratio(tm, i);
        recompute_freq(tm, i);
    }
}

static void adjust_volume(Timid *tm, int c)
{
    int i=tm->voices;
    while (i--)
    if (tm->voice[i].channel==c &&
    (tm->voice[i].status==VOICE_ON || tm->voice[i].status==VOICE_SUSTAINED))
    {
        recompute_amp(tm, i);
        apply_envelope_to_amp(tm, i);
    }
}

static void seek_forward(Timid *tm, int32 until_time)
{
    reset_voices(tm);
    while (tm->current_event->time < until_time)
    {
        switch(tm->current_event->type)
        {
            /* All notes stay off. Just handle the parameter changes. */
            
        case ME_PITCH_SENS:
            tm->channel[tm->current_event->channel].pitchsens=
            tm->current_event->a;
            tm->channel[tm->current_event->channel].pitchfactor=0;
            break;
            
        case ME_PITCHWHEEL:
            tm->channel[tm->current_event->channel].pitchbend=
            tm->current_event->a + tm->current_event->b * 128;
            tm->channel[tm->current_event->channel].pitchfactor=0;
            break;
            
        case ME_MODULATION:
            tm->channel[tm->current_event->channel].modulation=tm->current_event->a;
            break;
            
        case ME_MAINVOLUME:
            tm->channel[tm->current_event->channel].volume=tm->current_event->a;
            break;
            
        case ME_PAN:
            tm->channel[tm->current_event->channel].panning=tm->current_event->a;
            break;
            
        case ME_EXPRESSION:
            tm->channel[tm->current_event->channel].expression=tm->current_event->a;
            break;
            
        case ME_REVERB:
            tm->channel[tm->current_event->channel].reverb=tm->current_event->a;
            break;
            
        case ME_CHORUS:
            tm->channel[tm->current_event->channel].chorus=tm->current_event->a;
            break;
            
        case ME_PROGRAM:
            if (ISDRUMCHANNEL(tm, tm->current_event->channel))
            /* Change drum set */
            tm->channel[tm->current_event->channel].bank=tm->current_event->a;
            else
            tm->channel[tm->current_event->channel].program=tm->current_event->a;
            break;
            
        case ME_SUSTAIN:
            tm->channel[tm->current_event->channel].sustain=tm->current_event->a;
            break;
            
        case ME_RESET_CONTROLLERS:
            reset_controllers(tm, tm->current_event->channel);
            break;
            
        case ME_MONO:
            tm->channel[tm->current_event->channel].mono=1;
            break;
            
        case ME_POLY:
            tm->channel[tm->current_event->channel].mono=0;
            break;
            
        case ME_TONE_BANK:
            if (!ISDRUMCHANNEL(tm, tm->current_event->channel))
            tm->channel[tm->current_event->channel].bank=tm->current_event->a;
            break;
            
        case ME_EOT:
            tm->current_sample=tm->current_event->time;
            return;
        }
        tm->current_event++;
    }
    /*current_sample=current_event->time;*/
    if (tm->current_event != tm->event_list)
    tm->current_event--;
    tm->current_sample=until_time;
}

static void skip_to(Timid *tm, int32 until_time)
{
    if (tm->current_sample > until_time)
    tm->current_sample=0;
    
    reset_midi(tm);
    tm->current_event=tm->event_list;
    
    if (until_time)
    seek_forward(tm, until_time);
}

static void do_compute_data(Timid *tm, int32 count)
{
    int i;
    memset(tm->buffer_pointer, 0,
    (tm->play_mode.encoding & PE_MONO) ? (count * 4) : (count * 8));
    if (tm->reverb_enabled)
    {
        memset(tm->reverb_send_buffer, 0, count * 4);
    }
    if (tm->chorus_enabled)
    {
        memset(tm->chorus_send_buffer, 0, count * 4);
    }
    for (i=0; i<tm->voices; i++)
    {
        if(tm->voice[i].status != VOICE_FREE)
			mix_voice(tm, tm->buffer_pointer, i, count);
    }
    if (tm->reverb_enabled)
    {
        process_reverb(tm, tm->buffer_pointer, tm->reverb_send_buffer, count);
    }
    if (tm->chorus_enabled)
    {
        process_chorus(tm, tm->buffer_pointer, tm->chorus_send_buffer, count);
    }
}

static int32 do_dither(Timid *tm, int32 sample, int32 depth)
{
    double r1, r2, n;
    int32 scale;
    if (depth > 32) depth = 32;
    else if (depth < 1) depth = 1;
    scale = 1 << (32 - depth - GUARD_BITS);
    r1 = genRand(&tm->prng) - 0.5;
    r2 = genRand(&tm->prng) - 0.5;
    n = (r1 + r2) * (double)scale;
    return sample + (int32)n;
}

static void play_midi(Timid *tm, MidiEvent *e)
{
    if (e)
    {
        if (ISQUIETCHANNEL(tm, e->channel))
        {
            return;
        }
        switch(e->type)
        {
            
            /* Effects affecting a single note */
            
        case ME_NOTEON:
            if (!(e->b)) /* Velocity 0? */
            note_off(tm, e);
            else
            note_on(tm, e);
            break;
            
        case ME_NOTEOFF:
            note_off(tm, e);
            break;
            
        case ME_KEYPRESSURE:
            adjust_pressure(tm, e);
            break;
            
            /* Effects affecting a single channel */
            
        case ME_PITCH_SENS:
            tm->channel[e->channel].pitchsens=
            e->a;
            tm->channel[e->channel].pitchfactor=0;
            break;
            
        case ME_PITCHWHEEL:
            tm->channel[e->channel].pitchbend=
            e->a + e->b * 128;
            tm->channel[e->channel].pitchfactor=0;
            /* Adjust pitch for notes already playing */
            adjust_pitchbend(tm, e->channel);
            break;
            
        case ME_MODULATION:
            tm->channel[e->channel].modulation=e->a;
            adjust_modulation(tm, e->channel);
            break;
            
        case ME_MAINVOLUME:
            tm->channel[e->channel].volume=e->a;
            adjust_volume(tm, e->channel);
            break;
            
        case ME_PAN:
            tm->channel[e->channel].panning=e->a;
            if (tm->adjust_panning_immediately)
            adjust_panning(tm, e->channel);
            break;
            
        case ME_EXPRESSION:
            tm->channel[e->channel].expression=e->a;
            adjust_volume(tm, e->channel);
            break;
            
        case ME_REVERB:
            tm->channel[e->channel].reverb=e->a;
            break;
            
        case ME_CHORUS:
            tm->channel[e->channel].chorus=e->a;
            break;
            
        case ME_PROGRAM:
            if (ISDRUMCHANNEL(tm, e->channel))
            {
                /* Change drum set */
                if (tm->drumset[e->a])
                {
                    tm->channel[e->channel].bank=e->a;
                }
            }
            else
            {
                tm->channel[e->channel].program=e->a;
            }
            break;
            
        case ME_SUSTAIN:
            tm->channel[e->channel].sustain=e->a;
            if (!e->a)
            drop_sustain(tm, e->channel);
            break;
            
        case ME_RESET_CONTROLLERS:
            reset_controllers(tm, e->channel);
            adjust_volume(tm, e->channel);
            adjust_pitchbend(tm, e->channel);
            break;
            
        case ME_ALL_NOTES_OFF:
            all_notes_off(tm, e->channel);
            break;
            
        case ME_ALL_SOUNDS_OFF:
            all_sounds_off(tm, e->channel);
            break;
            
        case ME_MONO:
            tm->channel[e->channel].mono=1;
            all_notes_off(tm, e->channel);
            break;
            
        case ME_POLY:
            tm->channel[e->channel].mono=0;
            all_notes_off(tm, e->channel);
            break;
            
        case ME_TONE_BANK:
            if (!ISDRUMCHANNEL(tm, e->channel))
            {
                if (tm->tonebank[e->a])
                {
                    tm->channel[e->channel].bank=e->a;
                }
            }
            break;
        }
    }
}

//Adapted from ReadMidiText function in gspmidi.cpp
static void read_midi_text(Timid *tm)
{
    uint32 buff;
    uint32 read;

    if (!tm->fp_midi)
        return;

    fseek(tm->fp_midi, 0, SEEK_SET);

    if (fread(&buff, 1, 4, tm->fp_midi) != 4 || buff != 0x6468544d) {
        fseek(tm->fp_midi, 0, SEEK_SET);
        return;
    }

    while (fread(&buff, 1, 4, tm->fp_midi) == 4)
    {
        if (buff == 0x6B72544D)
            break;

        fseek(tm->fp_midi, -3, SEEK_CUR);
    }

    if (buff != 0x6B72544D) {
        fseek(tm->fp_midi, 0, SEEK_SET);
        return;
    }

    fseek(tm->fp_midi, 4, SEEK_CUR);

    while (fread(&buff, 1, 4, tm->fp_midi) == 4)
    {
        if ((buff & 0x0000FFFF) != 0x0000FF00 || buff == 0x6B72544D)
            break;

        switch(buff & 0x00FFFF00)
        {
        case 0x02FF00: //Copyright
            buff = (buff&0xFF000000) >> 24;
            if (!strlen(tm->song_copyright)) {
                read = fread(tm->song_copyright, 1, buff, tm->fp_midi);
                *(tm->song_copyright + read) = '\0';
            }
            else
                fseek(tm->fp_midi, buff, SEEK_CUR);
            break;
        case 0x03FF00: //Track
            buff = (buff&0xFF000000) >> 24;
            if (!strlen(tm->song_title)) {
                read = fread(tm->song_title, 1, buff, tm->fp_midi);
                *(tm->song_title + read) = '\0';
            }
            else
                fseek(tm->fp_midi, buff, SEEK_CUR);
            break;
        case 0x01FF00: //other text
        case 0x04FF00: //Instrument
        case 0x05FF00: //Lyrics
        case 0x06FF00: //Marker
        case 0x07FF00: //Cue
        default:
            buff = (buff&0xFF000000) >> 24;
            fseek(tm->fp_midi, buff, SEEK_CUR);
            break;
        }
    }
    fseek(tm->fp_midi, 0, SEEK_SET);
}

Timid *timid_init(void)
{
    Timid *tm = (Timid *)safe_malloc(sizeof(Timid));
    if (!tm)
    {
        return NULL;
    }
    memset(tm, 0, sizeof(Timid));
    tm->default_program=DEFAULT_PROGRAM;
    tm->antialiasing_allowed=1;
    tm->pre_resampling_allowed=1;
#ifdef FAST_DECAY
    tm->fast_decay=1;
#else
    tm->fast_decay=0;
#endif
    tm->dynamic_loading=0;
    tm->voices=DEFAULT_VOICES;
    tm->play_mode.rate=DEFAULT_RATE;
    tm->play_mode.encoding=0;
    tm->control_rate=CONTROLS_PER_SECOND;
    tm->control_ratio = tm->play_mode.rate/tm->control_rate;
    tm->drumchannels=DEFAULT_DRUMCHANNELS;
    tm->quietchannels=0;
    tm->adjust_panning_immediately=1;
    init_tables(tm);
    reset_midi(tm);
    adjust_amplification(tm, DEFAULT_AMPLIFICATION);
    tm->reverb_enabled=0;
    tm->reverb_only=0;
    tm->reverb_level=1.0;
    tm->reverb_preset=TIMID_REVERB_PRESET_GENERIC;
    init_reverb(tm);
    tm->chorus_enabled=0;
    tm->chorus_depth=0.25;
    init_chorus(tm);
    tm->dither_enabled=0;
    return tm;
}

int timid_load_config(Timid *tm, char *filename)
{
    char directory[256];
    char *separator;
    if (!tm || !filename)
    {
        return 0;
    }
    timid_unload_config(tm);
    if (!tm->tonebank[0])
    {
        tm->tonebank[0]=(ToneBank *)safe_malloc(sizeof(ToneBank));
        memset(tm->tonebank[0], 0, sizeof(ToneBank));
    }
    if (!tm->drumset[0])
    {
        tm->drumset[0]=(ToneBank *)safe_malloc(sizeof(ToneBank));
        memset(tm->drumset[0], 0, sizeof(ToneBank));
    }
    memset(directory, 0, sizeof(directory));
    strncpy(directory, filename, 255);
    directory[255]='\0';
    separator=strrchr(directory, PATH_SEP);
    if (separator) *separator='\0';
    add_to_pathlist(tm, directory);
    if (read_config_file(tm, filename) == 0)
    {
        if (*tm->def_instr_name)
        set_default_instrument(tm, tm->def_instr_name);
        if (!tm->dynamic_loading)
        {
            return timid_force_instrument_load(tm);
        }
        else
        {
            return 1;
        }
    }
    return 0;
}

void timid_unload_config(Timid *tm)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    free_instruments(tm);
    free_pathlist(tm);
    memset(tm->def_instr_name, 0, sizeof(tm->def_instr_name));
    memset(tm->last_config, 0, sizeof(tm->last_config));
}

int timid_reload_config(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    if (strlen(tm->last_config))
    {
        char temp[1024];
        memset(temp, 0, sizeof(temp));
        strncpy(temp, tm->last_config, 1023);
        temp[1023]='\0';
        return timid_load_config(tm, temp);
    }
    return 0;
}

void timid_channel_note_on(Timid *tm, uint8 channel, uint8 note, uint8 velocity)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_NOTEON;
    ev.a = note & 0x7f;
    ev.b = velocity & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_note_off(Timid *tm, uint8 channel, uint8 note)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_NOTEOFF;
    ev.a = note & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_key_pressure(Timid *tm, uint8 channel, uint8 note, uint8 velocity)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_KEYPRESSURE;
    ev.a = note & 0x7f;
    ev.b = velocity & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_modulation(Timid *tm, uint8 channel, uint8 amount)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_MODULATION;
    ev.a = amount & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_volume(Timid *tm, uint8 channel, uint8 volume)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_MAINVOLUME;
    ev.a = volume & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_pan(Timid *tm, uint8 channel, uint8 pan)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_PAN;
    ev.a = pan & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_expression(Timid *tm, uint8 channel, uint8 expression)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_EXPRESSION;
    ev.a = expression & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_sustain(Timid *tm, uint8 channel, uint8 sustain)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_SUSTAIN;
    ev.a = sustain & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_reverb(Timid *tm, uint8 channel, uint8 level)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_REVERB;
    ev.a = level & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_chorus(Timid *tm, uint8 channel, uint8 level)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_CHORUS;
    ev.a = level & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_pitch_wheel(Timid *tm, uint8 channel, uint16 pitch)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_PITCHWHEEL;
    ev.a = pitch & 0x7f;
    ev.b = (pitch >> 7) & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_pitch_range(Timid *tm, uint8 channel, uint8 range)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_PITCH_SENS;
    ev.a = range & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_program(Timid *tm, uint8 channel, uint8 program)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_PROGRAM;
    ev.a = program & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_set_bank(Timid *tm, uint8 channel, uint8 bank)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_TONE_BANK;
    ev.a = bank & 0x7f;
    play_midi(tm, &ev);
}

void timid_channel_mono_mode(Timid *tm, uint8 channel)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_MONO;
    play_midi(tm, &ev);
}

void timid_channel_poly_mode(Timid *tm, uint8 channel)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_POLY;
    play_midi(tm, &ev);
}

void timid_channel_all_notes_off(Timid *tm, uint8 channel)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_ALL_NOTES_OFF;
    play_midi(tm, &ev);
}

void timid_channel_all_sounds_off(Timid *tm, uint8 channel)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_ALL_SOUNDS_OFF;
    play_midi(tm, &ev);
}

void timid_channel_reset_controllers(Timid *tm, uint8 channel)
{
    MidiEvent ev;
    if (!tm)
    {
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.channel = channel & 0x0f;
    ev.type = ME_RESET_CONTROLLERS;
    play_midi(tm, &ev);
}

void timid_channel_control_change(Timid *tm, uint8 channel, uint8 controller, uint8 value)
{
    if (!tm)
    {
        return;
    }
    channel = channel & 0x0f;
    controller = controller & 0x7f;
    value = value & 0x7f;
    switch(controller)
    {
    case 0x00:
        timid_channel_set_bank(tm, channel, value);
        break;
    case 0x01:
        timid_channel_set_modulation(tm, channel, value);
        break;
    case 0x06:
        switch((tm->rpn_msb[channel]<<8) | tm->rpn_lsb[channel])
        {
        case 0x0000:
            timid_channel_set_pitch_range(tm, channel, value);
            break;
        case 0x7f7f:
            timid_channel_set_pitch_range(tm, channel, 2);
            tm->rpn_msb[channel] = 0xff;
            tm->rpn_lsb[channel] = 0xff;
            break;
        }
        break;
    case 0x07:
        timid_channel_set_volume(tm, channel, value);
        break;
    case 0x0a:
        timid_channel_set_pan(tm, channel, value);
        break;
    case 0x0b:
        timid_channel_set_expression(tm, channel, value);
        break;
    case 0x40:
        timid_channel_set_sustain(tm, channel, value);
        break;
    case 0x5b:
        timid_channel_set_reverb(tm, channel, value);
        break;
    case 0x5d:
        timid_channel_set_chorus(tm, channel, value);
        break;
    case 0x62:
        tm->rpn_lsb[channel] = 0xff;
        break;
    case 0x63:
        tm->rpn_msb[channel] = 0xff;
        break;
    case 0x64:
        tm->rpn_msb[channel] = value;
        break;
    case 0x65:
        tm->rpn_lsb[channel] = value;
        break;
    case 0x78:
        timid_channel_all_sounds_off(tm, channel);
        break;
    case 0x79:
        timid_channel_reset_controllers(tm, channel);
        break;
    case 0x7b:
        timid_channel_all_notes_off(tm, channel);
        break;
    case 0x7e:
        timid_channel_mono_mode(tm, channel);
        break;
    case 0x7f:
        timid_channel_poly_mode(tm, channel);
        break;
    }
}

void timid_write_midi(Timid *tm, uint8 byte1, uint8 byte2, uint8 byte3)
{
    uint8 type = byte1 & 0xf0;
    uint8 channel = byte1 & 0x0f;
    if (!tm)
    {
        return;
    }
    switch(type)
    {
    case 0x80:
        timid_channel_note_off(tm, channel, byte2);
        break;
    case 0x90:
        timid_channel_note_on(tm, channel, byte2, byte3);
        break;
    case 0xa0:
        timid_channel_key_pressure(tm, channel, byte2, byte3);
        break;
    case 0xb0:
        timid_channel_control_change(tm, channel, byte2, byte3);
        break;
    case 0xc0:
        timid_channel_set_program(tm, channel, byte2);
        break;
    case 0xe0:
        timid_channel_set_pitch_wheel(tm, channel, (uint16)((byte3 << 7) | byte2));
        break;
    }
}

void timid_write_midi_packed(Timid *tm, uint32 data)
{
    uint8 byte1 = data & 0xff;
    uint8 byte2 = (data >> 8) & 0x7f;
    uint8 byte3 = (data >> 16) & 0x7f;
    if (!tm)
    {
        return;
    }
    timid_write_midi(tm, byte1, byte2, byte3);
}

void timid_write_sysex(Timid *tm, uint8 *buffer, int32 count)
{
    const uint8 gm_reset_array[6] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
    const uint8 gm2_reset_array[6] = {0xF0, 0x7E, 0x7F, 0x09, 0x03, 0xF7};
    const uint8 gs_reset_array[11] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};
    const uint8 xg_reset_array[9] = {0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7};
    if (!tm || !buffer)
    {
        return;
    }
    if (buffer[0] != 0xF0 || buffer[count-1] != 0xF7)
    {
        return;
    }
    if (count == 6 && memcmp(&gm_reset_array[0], buffer, 6) == 0)
    {
        reset_midi(tm);
    }
    else if (count == 6 && memcmp(&gm2_reset_array[0], buffer, 6) == 0)
    {
        reset_midi(tm);
    }
    else if (count == 11 && memcmp(&gs_reset_array[0], buffer, 11) == 0)
    {
        reset_midi(tm);
    }
    else if (count == 9 && memcmp(&xg_reset_array[0], buffer, 9) == 0)
    {
        reset_midi(tm);
    }
}

void timid_render_char(Timid *tm, uint8 *buffer, int32 count)
{
    int curframes, cursamples, i;
    if (!tm || !buffer)
    {
        return;
    }
    while (count > 0)
    {
        if (count < AUDIO_BUFFER_SIZE)
        {
            curframes = count;
        }
        else
        {
            curframes = AUDIO_BUFFER_SIZE;
        }
        tm->buffer_pointer = tm->common_buffer;
        do_compute_data(tm, curframes);
        cursamples = curframes;
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            cursamples *= 2;
        }
        for (i=0; i<cursamples; i++)
        {
            if (tm->dither_enabled)
            {
                tm->common_buffer[i] = do_dither(tm, tm->common_buffer[i], 8);
            }
            tm->common_buffer[i] = tm->common_buffer[i] >> (32 - 8 - GUARD_BITS);
            if (tm->common_buffer[i] > 127)
            {
                tm->common_buffer[i] = 127;
            }
            else if (tm->common_buffer[i] < -128)
            {
                tm->common_buffer[i] = -128;
            }
            buffer[i] = (uint8)tm->common_buffer[i] ^ 0x80;
        }
        buffer += cursamples;
        count -= curframes;
    }
}

void timid_render_short(Timid *tm, int16 *buffer, int32 count)
{
    int curframes, cursamples, i;
    if (!tm || !buffer)
    {
        return;
    }
    while (count > 0)
    {
        if (count < AUDIO_BUFFER_SIZE)
        {
            curframes = count;
        }
        else
        {
            curframes = AUDIO_BUFFER_SIZE;
        }
        tm->buffer_pointer = tm->common_buffer;
        do_compute_data(tm, curframes);
        cursamples = curframes;
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            cursamples *= 2;
        }
        for (i=0; i<cursamples; i++)
        {
            if (tm->dither_enabled)
            {
                tm->common_buffer[i] = do_dither(tm, tm->common_buffer[i], 16);
            }
            tm->common_buffer[i] = tm->common_buffer[i] >> (32 - 16 - GUARD_BITS);
            if (tm->common_buffer[i] > 32767)
            {
                tm->common_buffer[i] = 32767;
            }
            else if (tm->common_buffer[i] < -32768)
            {
                tm->common_buffer[i] = -32768;
            }
            buffer[i] = (int16)tm->common_buffer[i];
        }
        buffer += cursamples;
        count -= curframes;
    }
}

void timid_render_24(Timid *tm, int24 *buffer, int32 count)
{
    int curframes, cursamples, i;
    if (!tm || !buffer)
    {
        return;
    }
    while (count > 0)
    {
        if (count < AUDIO_BUFFER_SIZE)
        {
            curframes = count;
        }
        else
        {
            curframes = AUDIO_BUFFER_SIZE;
        }
        tm->buffer_pointer = tm->common_buffer;
        do_compute_data(tm, curframes);
        cursamples = curframes;
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            cursamples *= 2;
        }
        for (i=0; i<cursamples; i++)
        {
            if (tm->dither_enabled)
            {
                tm->common_buffer[i] = do_dither(tm, tm->common_buffer[i], 24);
            }
            tm->common_buffer[i] = tm->common_buffer[i] >> (32 - 24 - GUARD_BITS);
            if (tm->common_buffer[i] > 8388607)
            {
                tm->common_buffer[i] = 8388607;
            }
            else if (tm->common_buffer[i] < -8388608)
            {
                tm->common_buffer[i] = -8388608;
            }
            buffer[i].data[0] = tm->common_buffer[i] & 0xff;
            buffer[i].data[1] = (tm->common_buffer[i] >> 8) & 0xff;
            buffer[i].data[2] = (tm->common_buffer[i] >> 16) & 0xff;
        }
        buffer += cursamples;
        count -= curframes;
    }
}

void timid_render_long(Timid *tm, int32 *buffer, int32 count)
{
    int curframes, cursamples, i;
    int32 maxval = 1 << (31 - GUARD_BITS);
    if (!tm || !buffer)
    {
        return;
    }
    while (count > 0)
    {
        if (count < AUDIO_BUFFER_SIZE)
        {
            curframes = count;
        }
        else
        {
            curframes = AUDIO_BUFFER_SIZE;
        }
        tm->buffer_pointer = tm->common_buffer;
        do_compute_data(tm, curframes);
        cursamples = curframes;
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            cursamples *= 2;
        }
        for (i=0; i<cursamples; i++)
        {
            if (tm->common_buffer[i] > maxval-1)
            {
                tm->common_buffer[i] = maxval-1;
            }
            else if (tm->common_buffer[i] < maxval * -1)
            {
                tm->common_buffer[i] = maxval * -1;
            }
            tm->common_buffer[i] = tm->common_buffer[i] << GUARD_BITS;
            buffer[i] = tm->common_buffer[i];
        }
        buffer += cursamples;
        count -= curframes;
    }
}

void timid_render_float(Timid *tm, float *buffer, int32 count)
{
    int curframes, cursamples, i;
    int32 maxval = 1 << (31 - GUARD_BITS);
    if (!tm || !buffer)
    {
        return;
    }
    while (count > 0)
    {
        if (count < AUDIO_BUFFER_SIZE)
        {
            curframes = count;
        }
        else
        {
            curframes = AUDIO_BUFFER_SIZE;
        }
        tm->buffer_pointer = tm->common_buffer;
        do_compute_data(tm, curframes);
        cursamples = curframes;
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            cursamples *= 2;
        }
        for (i=0; i<cursamples; i++)
        {
            buffer[i] = tm->common_buffer[i] / (float)maxval;
        }
        buffer += cursamples;
        count -= curframes;
    }
}

void timid_render_double(Timid *tm, double *buffer, int32 count)
{
    int curframes, cursamples, i;
    int32 maxval = 1 << (31 - GUARD_BITS);
    if (!tm || !buffer)
    {
        return;
    }
    while (count > 0)
    {
        if (count < AUDIO_BUFFER_SIZE)
        {
            curframes = count;
        }
        else
        {
            curframes = AUDIO_BUFFER_SIZE;
        }
        tm->buffer_pointer = tm->common_buffer;
        do_compute_data(tm, curframes);
        cursamples = curframes;
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            cursamples *= 2;
        }
        for (i=0; i<cursamples; i++)
        {
            buffer[i] = tm->common_buffer[i] / (double)maxval;
        }
        buffer += cursamples;
        count -= curframes;
    }
}

void timid_render_ulaw(Timid *tm, uint8 *buffer, int32 count)
{
    int curframes, cursamples, i;
    if (!tm || !buffer)
    {
        return;
    }
    while (count > 0)
    {
        if (count < AUDIO_BUFFER_SIZE)
        {
            curframes = count;
        }
        else
        {
            curframes = AUDIO_BUFFER_SIZE;
        }
        tm->buffer_pointer = tm->common_buffer;
        do_compute_data(tm, curframes);
        cursamples = curframes;
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            cursamples *= 2;
        }
        for (i=0; i<cursamples; i++)
        {
            if (tm->dither_enabled)
            {
                tm->common_buffer[i] = do_dither(tm, tm->common_buffer[i], 13);
            }
            tm->common_buffer[i] = tm->common_buffer[i] >> (32 - 13 - GUARD_BITS);
            if (tm->common_buffer[i] > 4095)
            {
                tm->common_buffer[i] = 4095;
            }
            else if (tm->common_buffer[i] < -4096)
            {
                tm->common_buffer[i] = -4096;
            }
            buffer[i] = _l2u[tm->common_buffer[i]];
        }
        buffer += cursamples;
        count -= curframes;
    }
}

void timid_all_notes_off(Timid *tm)
{
    int i;
    if (!tm)
    {
        return;
    }
    for (i=0; i<16; i++)
    {
        drop_sustain(tm, i);
        all_notes_off(tm, i);
    }
}

void timid_all_sounds_off(Timid *tm)
{
    int i;
    if (!tm)
    {
        return;
    }
    for (i=0; i<16; i++)
    {
        all_sounds_off(tm, i);
    }
}

void timid_reset_controllers(Timid *tm)
{
    int i;
    if (!tm)
    {
        return;
    }
    for (i=0; i<16; i++)
    {
        reset_controllers(tm, i);
        adjust_volume(tm, i);
        adjust_pitchbend(tm, i);
    }
}

void timid_panic(Timid *tm)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
}

void timid_reset(Timid *tm)
{
    if (!tm)
    {
        return;
    }
    reset_midi(tm);
}

int timid_load_smf(Timid *tm, char *filename)
{
    if (!tm || !filename)
    {
        return 0;
    }
    timid_unload_smf(tm);
    tm->fp_midi = open_file(tm, filename, 1, OF_VERBOSE);
    if (!tm->fp_midi)
    {
        return 0;
    }
    tm->event_list = read_midi_file(tm, tm->fp_midi, &tm->events_midi, &tm->sample_count);
    if (!tm->event_list || !tm->events_midi || !tm->sample_count)
    {
        timid_unload_smf(tm);
        return 0;
    }
    load_missing_instruments(tm);
    read_midi_text(tm);
    skip_to(tm, 0);
    strncpy(tm->last_smf, filename, 1023);
    tm->last_smf[1023]='\0';
    return 1;
}

void timid_unload_smf(Timid *tm)
{
    if (!tm)
    {
        return;
    }
    reset_midi(tm);
    if (tm->event_list)
    {
        free(tm->event_list);
        tm->event_list = NULL;
    }
    tm->current_event = NULL;
    if (tm->fp_midi)
    {
        close_file(tm->fp_midi);
        tm->fp_midi = NULL;
    }
    tm->events_midi = 0;
    tm->sample_count = 0;
    tm->current_sample = 0;
    memset(tm->song_title, 0, sizeof(tm->song_title));
    memset(tm->song_copyright, 0, sizeof(tm->song_copyright));
    memset(tm->last_smf, 0, sizeof(tm->last_smf));
}

int timid_reload_smf(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    if (strlen(tm->last_smf))
    {
        char temp[1024];
        memset(temp, 0, sizeof(temp));
        strncpy(temp, tm->last_smf, 1023);
        temp[1023]='\0';
        return timid_load_smf(tm, temp);
    }
    return 0;
}

int timid_play_smf(Timid *tm, int32 type, uint8 *buffer, int32 count)
{
    int convert;
    if (!tm || !buffer || (type > AU_ULAW || type < AU_CHAR) || !tm->current_event || (tm->current_event->type == ME_EOT && !timid_get_active_voices(tm)))
    {
        return 0;
    }
    while (count > 0)
    {
        /* Handle all events that should happen at this time */
        while (tm->current_event->time <= tm->current_sample)
        {
            if (tm->current_event->type == ME_EOT)
            {
                break;
            }
            play_midi(tm, tm->current_event);
            tm->current_event++;
        }
        convert = tm->current_event->time - tm->current_sample;
        if (convert > count || convert <= 0)
        {
            convert = count;
        }
        switch(type)
        {
        case AU_CHAR:
            timid_render_char(tm, (uint8 *)buffer, convert);
            if (!(tm->play_mode.encoding & PE_MONO))
            {
                buffer += convert*2*sizeof(uint8);
            }
            else
            {
                buffer += convert*sizeof(uint8);
            }
            break;
        case AU_SHORT:
            timid_render_short(tm, (int16 *)buffer, convert);
            if (!(tm->play_mode.encoding & PE_MONO))
            {
                buffer += convert*2*sizeof(int16);
            }
            else
            {
                buffer += convert*sizeof(int16);
            }
            break;
        case AU_24:
            timid_render_24(tm, (int24 *)buffer, convert);
            if (!(tm->play_mode.encoding & PE_MONO))
            {
                buffer += convert*2*sizeof(int24);
            }
            else
            {
                buffer += convert*sizeof(int24);
            }
            break;
        case AU_LONG:
            timid_render_long(tm, (int32 *)buffer, convert);
            if (!(tm->play_mode.encoding & PE_MONO))
            {
                buffer += convert*2*sizeof(int32);
            }
            else
            {
                buffer += convert*sizeof(int32);
            }
            break;
        case AU_FLOAT:
            timid_render_float(tm, (float *)buffer, convert);
            if (!(tm->play_mode.encoding & PE_MONO))
            {
                buffer += convert*2*sizeof(float);
            }
            else
            {
                buffer += convert*sizeof(float);
            }
            break;
        case AU_DOUBLE:
            timid_render_double(tm, (double *)buffer, convert);
            if (!(tm->play_mode.encoding & PE_MONO))
            {
                buffer += convert*2*sizeof(double);
            }
            else
            {
                buffer += convert*sizeof(double);
            }
            break;
        case AU_ULAW:
            timid_render_ulaw(tm, (uint8 *)buffer, convert);
            if (!(tm->play_mode.encoding & PE_MONO))
            {
                buffer += convert*2*sizeof(uint8);
            }
            else
            {
                buffer += convert*sizeof(uint8);
            }
            break;
        }
        if (tm->current_sample == tm->sample_count)
        {
            timid_all_notes_off(tm);
        }
        tm->current_sample += convert;
        count -= convert;
    }
    return 1;
}

int timid_seek_smf(Timid *tm, int32 time)
{
    int total_time;
    if (!tm || !tm->current_event)
    {
        return 0;
    }
    total_time = timid_get_duration(tm);
    if (time > total_time)
    {
        time = total_time;
    }
    else if (time < 0)
    {
        time = 0;
    }
    skip_to(tm, 0);
    skip_to(tm, timid_millis2samples(tm, time));
    return timid_get_current_time(tm);
}

int timid_fast_forward_smf(Timid *tm, int32 time)
{
    int new_time;
    if (!tm)
    {
        return 0;
    }
    new_time = timid_get_current_time(tm) + time;
    return timid_seek_smf(tm, new_time);
}

int timid_rewind_smf(Timid *tm, int32 time)
{
    int new_time;
    if (!tm)
    {
        return 0;
    }
    new_time = timid_get_current_time(tm) - time;
    return timid_seek_smf(tm, new_time);
}

int timid_restart_smf(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return timid_seek_smf(tm, 0);
}

int timid_stop_smf(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return timid_seek_smf(tm, timid_get_duration(tm));
}

void timid_set_amplification(Timid *tm, int amplification)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    if (amplification > MAX_AMPLIFICATION)
    {
        amplification = MAX_AMPLIFICATION;
    }
    else if (amplification < 0)
    {
        amplification = 0;
    }
    adjust_amplification(tm, amplification);
}

void timid_set_max_voices(Timid *tm, int voices)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    if (voices > MAX_VOICES)
    {
        voices = MAX_VOICES;
    }
    else if (voices < 1)
    {
        voices = 1;
    }
    tm->voices = voices;
}

void timid_set_immediate_panning(Timid *tm, int value)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    tm->adjust_panning_immediately = value;
}

void timid_set_mono(Timid *tm, int value)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    if (value) tm->play_mode.encoding |= PE_MONO;
    else tm->play_mode.encoding &= ~PE_MONO;
}

void timid_set_fast_decay(Timid *tm, int value)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    tm->fast_decay = value;
    timid_reload_config(tm);
}

void timid_set_antialiasing(Timid *tm, int value)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    tm->antialiasing_allowed = value;
    timid_reload_config(tm);
}

void timid_set_pre_resample(Timid *tm, int value)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    tm->pre_resampling_allowed = value;
    timid_reload_config(tm);
}

void timid_set_dynamic_instrument_load(Timid *tm, int value)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    tm->dynamic_loading = value;
    timid_reload_config(tm);
}

void timid_set_sample_rate(Timid *tm, int rate)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    if (rate > MAX_OUTPUT_RATE)
    {
        rate = MAX_OUTPUT_RATE;
    }
    else if (rate < MIN_OUTPUT_RATE)
    {
        rate = MIN_OUTPUT_RATE;
    }
    tm->play_mode.rate = rate;
    if (tm->control_rate > tm->play_mode.rate)
    {
        tm->control_rate = tm->play_mode.rate;
    }
    else if (tm->control_rate < tm->play_mode.rate/MAX_CONTROL_RATIO)
    {
        tm->control_rate = tm->play_mode.rate/MAX_CONTROL_RATIO;
    }
    tm->control_ratio = tm->play_mode.rate/tm->control_rate;
    if (tm->control_ratio > MAX_CONTROL_RATIO)
    {
        tm->control_ratio = MAX_CONTROL_RATIO;
    }
    else if (tm->control_ratio < 1)
    {
        tm->control_ratio = 1;
    }
    reset_reverb(tm);
    reset_chorus(tm);
    timid_reload_config(tm);
}

void timid_set_control_rate(Timid *tm, int rate)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    tm->control_rate = rate;
    if (tm->control_rate > tm->play_mode.rate)
    {
        tm->control_rate = tm->play_mode.rate;
    }
    else if (tm->control_rate < tm->play_mode.rate/MAX_CONTROL_RATIO)
    {
        tm->control_rate = tm->play_mode.rate/MAX_CONTROL_RATIO;
    }
    tm->control_ratio = tm->play_mode.rate/tm->control_rate;
    if (tm->control_ratio > MAX_CONTROL_RATIO)
    {
        tm->control_ratio = MAX_CONTROL_RATIO;
    }
    else if (tm->control_ratio < 1)
    {
        tm->control_ratio = 1;
    }
    timid_reload_config(tm);
}

void timid_set_default_program(Timid *tm, int program)
{
    if (!tm)
    {
        return;
    }
    tm->default_program = program & 0x7f;
}

void timid_set_drum_channel(Timid *tm, int channel, int enable)
{
    if (!tm)
    {
        return;
    }
    channel = channel & 0x0f;
    if (enable) tm->drumchannels |= (1<<channel);
    else tm->drumchannels &= ~(1<<channel);
}

void timid_set_quiet_channel(Timid *tm, int channel, int enable)
{
    if (!tm)
    {
        return;
    }
    channel = channel & 0x0f;
    if (enable && !ISQUIETCHANNEL(tm, channel))
    {
        drop_sustain(tm, channel);
        all_notes_off(tm, channel);
        reset_controllers(tm, channel);
    }
    if (enable) tm->quietchannels |= (1<<channel);
    else tm->quietchannels &= ~(1<<channel);
}

void timid_set_reverb_enabled(Timid *tm, int enable)
{
    if (!tm)
    {
        return;
    }
    tm->reverb_enabled = enable;
    reset_reverb(tm);
}

void timid_set_reverb_only(Timid *tm, int enable)
{
    if (!tm)
    {
        return;
    }
    tm->reverb_only = enable;
}

void timid_set_reverb_level(Timid *tm, int percent)
{
    if (!tm)
    {
        return;
    }
    if (percent > 100)
    {
        percent = 100;
    }
    else if (percent < 0)
    {
        percent = 0;
    }
    tm->reverb_level = (double)(percent) / 100.0L;
}

void timid_set_reverb_preset(Timid *tm, int preset)
{
    if (!tm)
    {
        return;
    }
    set_reverb_preset(tm, preset);
}

void timid_set_chorus_enabled(Timid *tm, int enable)
{
    if (!tm)
    {
        return;
    }
    tm->chorus_enabled = enable;
    reset_chorus(tm);
}

void timid_set_chorus_depth(Timid *tm, int percent)
{
    if (!tm)
    {
        return;
    }
    if (percent > 100)
    {
        percent = 100;
    }
    else if (percent < 0)
    {
        percent = 0;
    }
    tm->chorus_depth = (double)(percent) / 100.0L;
    apply_chorus_depth(tm);
}

void timid_set_dither_enabled(Timid *tm, int enable)
{
    if (!tm)
    {
        return;
    }
    tm->dither_enabled = enable;
}

void timid_restore_defaults(Timid *tm)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    tm->default_program=DEFAULT_PROGRAM;
    tm->antialiasing_allowed=1;
    tm->pre_resampling_allowed=1;
#ifdef FAST_DECAY
    tm->fast_decay=1;
#else
    tm->fast_decay=0;
#endif
    tm->dynamic_loading=0;
    tm->voices=DEFAULT_VOICES;
    tm->play_mode.rate=DEFAULT_RATE;
    tm->play_mode.encoding=0;
    tm->control_rate=CONTROLS_PER_SECOND;
    tm->control_ratio = tm->play_mode.rate/tm->control_rate;
    tm->drumchannels=DEFAULT_DRUMCHANNELS;
    tm->quietchannels=0;
    tm->adjust_panning_immediately=1;
    adjust_amplification(tm, DEFAULT_AMPLIFICATION);
    tm->reverb_enabled=0;
    tm->reverb_only=0;
    tm->reverb_level=1.0;
    tm->reverb_preset=TIMID_REVERB_PRESET_GENERIC;
    reset_reverb(tm);
    tm->chorus_enabled=0;
    tm->chorus_depth=0.25;
    reset_chorus(tm);
    tm->dither_enabled=0;
    timid_reload_config(tm);
}

int timid_force_instrument_load(Timid *tm)
{
    int i;
    if (!tm)
    {
        return 0;
    }
    reset_voices(tm);
    for (i=0; i<128; i++)
    {
        if (tm->tonebank[i])
        {
            int j;
            for (j=0; j<128; j++)
            {
                if (tm->tonebank[i]->tone[j].name && !tm->tonebank[i]->tone[j].instrument)
                {
                    tm->tonebank[i]->tone[j].instrument=MAGIC_LOAD_INSTRUMENT;
                }
            }
        }
        if (tm->drumset[i])
        {
            int j;
            for (j=0; j<128; j++)
            {
                if (tm->drumset[i]->tone[j].name && !tm->drumset[i]->tone[j].instrument)
                {
                    tm->drumset[i]->tone[j].instrument=MAGIC_LOAD_INSTRUMENT;
                }
            }
        }
    }
    if (load_missing_instruments(tm) == 0)
    {
        return 1;
    }
    return 0;
}

int timid_set_default_instrument(Timid *tm, char *filename)
{
    if (!tm || !filename)
    {
        return 0;
    }
    reset_voices(tm);
    if (set_default_instrument(tm, filename) == 0)
    {
        return 1;
    }
    return 0;
}

void timid_free_default_instrument(Timid *tm)
{
    if (!tm)
    {
        return;
    }
    reset_voices(tm);
    free_default_instrument(tm);
}

int timid_get_config_name(Timid *tm, char *buffer, int32 count)
{
    int len;
    if (!tm)
    {
        return 0;
    }
    len = strlen(tm->last_config);
    if (buffer && len)
    {
        strncpy(buffer, tm->last_config, count);
    }
    return len;
}

int timid_get_amplification(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return (int)(tm->master_volume * 100.0L);
}

int timid_get_active_voices(Timid *tm)
{
    int count = 0;
    int i;
    if (!tm)
    {
        return 0;
    }
    for (i=0; i<tm->voices; i++)
    {
        if(tm->voice[i].status != VOICE_FREE)
            count++;
    }
    return count;
}

int timid_get_max_voices(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->voices;
}

int timid_get_immediate_panning(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->adjust_panning_immediately;
}

int timid_get_mono(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    if (tm->play_mode.encoding & PE_MONO)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int timid_get_fast_decay(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->fast_decay;
}

int timid_get_antialiasing(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->antialiasing_allowed;
}

int timid_get_pre_resample(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->pre_resampling_allowed;
}

int timid_get_dynamic_instrument_load(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->dynamic_loading;
}

int timid_get_sample_rate(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->play_mode.rate;
}

int timid_get_control_rate(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->control_rate;
}

int timid_get_default_program(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->default_program;
}

int timid_get_drum_channel_enabled(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    if (ISDRUMCHANNEL(tm, channel))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int timid_get_quiet_channel_enabled(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    if (ISQUIETCHANNEL(tm, channel))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int timid_get_reverb_enabled(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->reverb_enabled;
}

int timid_get_reverb_only(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->reverb_only;
}

int timid_get_reverb_level(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return (int)(tm->reverb_level * 100.0L);
}

int timid_get_reverb_preset(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->reverb_preset;
}

int timid_get_chorus_enabled(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->chorus_enabled;
}

int timid_get_chorus_depth(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return (int)(tm->chorus_depth * 100.0L);
}

int timid_get_dither_enabled(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->dither_enabled;
}

int timid_get_lost_notes(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->lost_notes;
}

int timid_get_cut_notes(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->cut_notes;
}

int timid_channel_get_modulation(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].modulation;
}

int timid_channel_get_volume(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].volume;
}

int timid_channel_get_pan(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].panning;
}

int timid_channel_get_expression(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].expression;
}

int timid_channel_get_sustain(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].sustain;
}

int timid_channel_get_reverb(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].reverb;
}

int timid_channel_get_chorus(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].chorus;
}

int timid_channel_get_pitch_wheel(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].pitchbend;
}

int timid_channel_get_pitch_range(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].pitchsens;
}

int timid_channel_get_program(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    if (ISDRUMCHANNEL(tm, channel))
    {
        return tm->channel[channel].bank;
    }
    else
    {
        return tm->channel[channel].program;
    }
}

int timid_channel_get_bank(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    if (!ISDRUMCHANNEL(tm, channel))
    {
        return tm->channel[channel].bank;
    }
    else
    {
        return -1;
    }
}

int timid_channel_get_mono(Timid *tm, int channel)
{
    if (!tm)
    {
        return 0;
    }
    channel = channel & 0x0f;
    return tm->channel[channel].mono;
}

int timid_get_smf_name(Timid *tm, char *buffer, int32 count)
{
    int len;
    if (!tm)
    {
        return 0;
    }
    len = strlen(tm->last_smf);
    if (buffer && len)
    {
        strncpy(buffer, tm->last_smf, count);
    }
    return len;
}

int timid_get_event_count(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->events_midi;
}

int timid_get_sample_count(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->sample_count;
}

int timid_get_current_sample_position(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return tm->current_sample;
}

int timid_get_duration(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return timid_samples2millis(tm, tm->sample_count);
}

int timid_get_current_time(Timid *tm)
{
    if (!tm)
    {
        return 0;
    }
    return timid_samples2millis(tm, tm->current_sample);
}

int timid_get_bitrate(Timid *tm)
{
    int bitrate;
    if (!tm || !tm->fp_midi)
    {
        return 0;
    }
    fseek(tm->fp_midi, 0, SEEK_END);
    bitrate = ftell(tm->fp_midi) * 8 / timid_get_duration(tm);
    fseek(tm->fp_midi, 0, SEEK_SET);
    if (!bitrate) bitrate = 1;
    return bitrate;
}

int timid_get_song_title(Timid *tm, char *buffer, int32 count)
{
    int len;
    if (!tm)
    {
        return 0;
    }
    len = strlen(tm->song_title);
    if (buffer && len)
    {
        strncpy(buffer, tm->song_title, count);
    }
    return len;
}

int timid_get_song_copyright(Timid *tm, char *buffer, int32 count)
{
    int len;
    if (!tm)
    {
        return 0;
    }
    len = strlen(tm->song_copyright);
    if (buffer && len)
    {
        strncpy(buffer, tm->song_copyright, count);
    }
    return len;
}

int timid_millis2samples(Timid *tm, int32 millis)
{
    if (!tm)
    {
        return 0;
    }
    return (int)((double)tm->play_mode.rate * millis / 1000);
}

int timid_samples2millis(Timid *tm, int32 samples)
{
    if (!tm)
    {
        return 0;
    }
    return (int)((double)samples * 1000 / tm->play_mode.rate);
}

void timid_close(Timid *tm)
{
    if (!tm)
    {
        return;
    }
    reset_midi(tm);
    timid_unload_smf(tm);
    timid_unload_config(tm);
    free_default_instrument(tm);
    free_tables(tm);
    free_reverb(tm);
    free_chorus(tm);
    memset(tm, 0, sizeof(Timid));
    free(tm);
}

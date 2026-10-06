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

resample.c
*/

#include <math.h>
#include <stdio.h>
#include <malloc.h>

#include "internal.h"

#ifdef LINEAR_INTERPOLATION
# if defined(LOOKUP_HACK) && defined(LOOKUP_INTERPOLATION)
#   define RESAMPLATION \
v1=src[ofs>>FRACTION_BITS];\
v2=src[(ofs>>FRACTION_BITS)+1];\
*dest++ = v1 + (tm->iplookup[(((v2-v1)<<5) & 0x03FE0) | \
((ofs & FRACTION_MASK) >> (FRACTION_BITS-5))]);
# else
#   define RESAMPLATION \
v1=src[ofs>>FRACTION_BITS];\
v2=src[(ofs>>FRACTION_BITS)+1];\
*dest++ = v1 + (((v2-v1) * (ofs & FRACTION_MASK)) >> FRACTION_BITS);
# endif
#  define INTERPVARS sample_t v1, v2
#else
/* Earplugs recommended for maximum listening enjoyment */
#  define RESAMPLATION *dest++=src[ofs>>FRACTION_BITS];
#  define INTERPVARS
#endif

#define FINALINTERP if (ofs == le) *dest++=src[(ofs>>FRACTION_BITS)-1]/2;
/* So it isn't interpolation. At least it's final. */

/*************** resampling with fixed increment *****************/

static sample_t *rs_plain(Timid *tm, int v, int32 *countptr)
{
    
    /* Play sample until end, then free the voice. */
    
    INTERPVARS;
    Voice
    *vp=&tm->voice[v];
    sample_t
    *dest=tm->resample_buffer,
    *src=vp->sample->data;
    int32
    ofs=vp->sample_offset,
    incr=vp->sample_increment,
    le=vp->sample->data_length,
    count=*countptr;
    
#ifdef PRECALC_LOOPS
    int32 i;
    
    if (incr<0) incr = -incr; /* In case we're coming out of a bidir loop */
    
    /* Precalc how many times we should go through the loop.
    NOTE: Assumes that incr > 0 and that ofs <= le */
    i = (le - ofs) / incr + 1;
    
    if (i > count)
    {
        i = count;
        count = 0;
    }
    else count -= i;
    
    while (i--)
    {
        RESAMPLATION;
        ofs += incr;
    }
    
    if (ofs >= le)
    {
        FINALINTERP;
        vp->status=VOICE_FREE;
        *countptr-=count+1;
    }
    
#else /* PRECALC_LOOPS */
    while (count--)
    {
        RESAMPLATION;
        ofs += incr;
        if (ofs >= le)
        {
            FINALINTERP;
            vp->status=VOICE_FREE;
            *countptr-=count+1;
            break;
        }
    }
#endif /* PRECALC_LOOPS */
    
    vp->sample_offset=ofs; /* Update offset */
    return tm->resample_buffer;
}

static sample_t *rs_loop(Timid *tm, Voice *vp, int32 count)
{
    
    /* Play sample until end-of-loop, skip back and continue. */
    
    INTERPVARS;
    int32
    ofs=vp->sample_offset,
    incr=vp->sample_increment,
    le=vp->sample->loop_end,
    ll=le - vp->sample->loop_start;
    sample_t
    *dest=tm->resample_buffer,
    *src=vp->sample->data;
    
#ifdef PRECALC_LOOPS
    int32 i;
    
    while (count)
    {
        while (ofs >= le)
        ofs -= ll;
        /* Precalc how many times we should go through the loop */
        i = (le - ofs) / incr + 1;
        if (i > count)
        {
            i = count;
            count = 0;
        }
        else count -= i;
        while (i--)
        {
            RESAMPLATION;
            ofs += incr;
        }
    }
#else
    while (count--)
    {
        RESAMPLATION;
        ofs += incr;
        if (ofs>=le)
        ofs -= ll; /* Hopefully the loop is longer than an increment. */
    }
#endif
    
    vp->sample_offset=ofs; /* Update offset */
    return tm->resample_buffer;
}

static sample_t *rs_bidir(Timid *tm, Voice *vp, int32 count)
{
    INTERPVARS;
    int32
    ofs=vp->sample_offset,
    incr=vp->sample_increment,
    le=vp->sample->loop_end,
    ls=vp->sample->loop_start;
    sample_t
    *dest=tm->resample_buffer,
    *src=vp->sample->data;
    
#ifdef PRECALC_LOOPS
    int32
    le2 = le<<1,
    ls2 = ls<<1,
    i;
    /* Play normally until inside the loop region */
    
    if (ofs <= ls)
    {
        /* NOTE: Assumes that incr > 0, which is NOT always the case
        when doing bidirectional looping.  I have yet to see a case
        where both ofs <= ls AND incr < 0, however. */
        i = (ls - ofs) / incr + 1;
        if (i > count)
        {
            i = count;
            count = 0;
        }
        else count -= i;
        while (i--)
        {
            RESAMPLATION;
            ofs += incr;
        }
    }
    
    /* Then do the bidirectional looping */
    
    while(count)
    {
        /* Precalc how many times we should go through the loop */
        i = ((incr > 0 ? le : ls) - ofs) / incr + 1;
        if (i > count)
        {
            i = count;
            count = 0;
        }
        else count -= i;
        while (i--)
        {
            RESAMPLATION;
            ofs += incr;
        }
        if (ofs>=le)
        {
            /* fold the overshoot back in */
            ofs = le2 - ofs;
            incr *= -1;
        }
        else if (ofs <= ls)
        {
            ofs = ls2 - ofs;
            incr *= -1;
        }
    }
    
#else /* PRECALC_LOOPS */
    /* Play normally until inside the loop region */
    
    if (ofs < ls)
    {
        while (count--)
        {
            RESAMPLATION;
            ofs += incr;
            if (ofs>=ls)
            break;
        }
    }
    
    /* Then do the bidirectional looping */
    
    if (count>0)
    while (count--)
    {
        RESAMPLATION;
        ofs += incr;
        if (ofs>=le)
        {
            /* fold the overshoot back in */
            ofs = le - (ofs - le);
            incr = -incr;
        }
        else if (ofs <= ls)
        {
            ofs = ls + (ls - ofs);
            incr = -incr;
        }
    }
#endif /* PRECALC_LOOPS */
    vp->sample_increment=incr;
    vp->sample_offset=ofs; /* Update offset */
    return tm->resample_buffer;
}

/*********************** vibrato versions ***************************/

/* We only need to compute one half of the vibrato sine cycle */
static int vib_phase_to_inc_ptr(int phase)
{
    if (phase < VIBRATO_SAMPLE_INCREMENTS/2)
    return VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
    else if (phase >= 3*VIBRATO_SAMPLE_INCREMENTS/2)
    return 5*VIBRATO_SAMPLE_INCREMENTS/2-1-phase;
    else
    return phase-VIBRATO_SAMPLE_INCREMENTS/2;
}

static int32 update_vibrato(Timid *tm, Voice *vp, int sign)
{
    int32 depth;
    int phase, pb;
    double a;
    
    if (vp->vibrato_phase++ >= 2*VIBRATO_SAMPLE_INCREMENTS-1)
    vp->vibrato_phase=0;
    phase=vib_phase_to_inc_ptr(vp->vibrato_phase);
    
    if (vp->vibrato_sample_increment[phase])
    {
        if (sign)
        return -vp->vibrato_sample_increment[phase];
        else
        return vp->vibrato_sample_increment[phase];
    }
    
    /* Need to compute this sample increment. */
    
    depth=vp->sample->vibrato_depth<<7;
    
    if (vp->vibrato_sweep)
    {
        /* Need to update sweep */
        vp->vibrato_sweep_position += vp->vibrato_sweep;
        if (vp->vibrato_sweep_position >= (1<<SWEEP_SHIFT))
        vp->vibrato_sweep=0;
        else
        {
            /* Adjust depth */
            depth *= vp->vibrato_sweep_position;
            depth >>= SWEEP_SHIFT;
        }
    }
    
    depth += tm->channel[vp->channel].modulation << MOD_VIBRATO_DEPTH_SHIFT;
    
    a = FSCALE(((double)(vp->sample->sample_rate) *
    (double)(vp->frequency)) /
    ((double)(vp->sample->root_freq) *
    (double)(tm->play_mode.rate)),
    FRACTION_BITS);
    
    pb=(int)((sine(vp->vibrato_phase *
    (SINE_CYCLE_LENGTH/(2*VIBRATO_SAMPLE_INCREMENTS)))
    * (double)(depth) * VIBRATO_AMPLITUDE_TUNING));
    
    if (pb<0)
    {
        pb=-pb;
        a /= bend_fine[(pb>>5) & 0xFF] * bend_coarse[pb>>13];
    }
    else
    a *= bend_fine[(pb>>5) & 0xFF] * bend_coarse[pb>>13];
    
    /* If the sweep's over, we can store the newly computed sample_increment */
    if (!vp->vibrato_sweep)
    vp->vibrato_sample_increment[phase]=(int32) a;
    
    if (sign)
    a = -a; /* need to preserve the loop direction */
    
    return (int32) a;
}

static sample_t *rs_vib_plain(Timid *tm, int v, int32 *countptr)
{
    
    /* Play sample until end, then free the voice. */
    
    INTERPVARS;
    Voice *vp=&tm->voice[v];
    sample_t
    *dest=tm->resample_buffer,
    *src=vp->sample->data;
    int32
    le=vp->sample->data_length,
    ofs=vp->sample_offset,
    incr=vp->sample_increment,
    count=*countptr;
    int
    cc=vp->vibrato_control_counter;
    
    /* This has never been tested */
    
    if (incr<0) incr = -incr; /* In case we're coming out of a bidir loop */
    
    while (count--)
    {
        if (!cc--)
        {
            cc=vp->vibrato_control_ratio;
            incr=update_vibrato(tm, vp, 0);
        }
        RESAMPLATION;
        ofs += incr;
        if (ofs >= le)
        {
            FINALINTERP;
            vp->status=VOICE_FREE;
            *countptr-=count+1;
            break;
        }
    }
    
    vp->vibrato_control_counter=cc;
    vp->sample_increment=incr;
    vp->sample_offset=ofs; /* Update offset */
    return tm->resample_buffer;
}

static sample_t *rs_vib_loop(Timid *tm, Voice *vp, int32 count)
{
    
    /* Play sample until end-of-loop, skip back and continue. */
    
    INTERPVARS;
    int32
    ofs=vp->sample_offset,
    incr=vp->sample_increment,
    le=vp->sample->loop_end,
    ll=le - vp->sample->loop_start;
    sample_t
    *dest=tm->resample_buffer,
    *src=vp->sample->data;
    int
    cc=vp->vibrato_control_counter;
    
#ifdef PRECALC_LOOPS
    int32 i;
    int
    vibflag=0;
    
    while (count)
    {
        /* Hopefully the loop is longer than an increment */
        while(ofs >= le)
        ofs -= ll;
        /* Precalc how many times to go through the loop, taking
        the vibrato control ratio into account this time. */
        i = (le - ofs) / incr + 1;
        if(i > count) i = count;
        if(i > cc)
        {
            i = cc;
            vibflag = 1;
        }
        else cc -= i;
        count -= i;
        while(i--)
        {
            RESAMPLATION;
            ofs += incr;
        }
        if(vibflag)
        {
            cc = vp->vibrato_control_ratio;
            incr = update_vibrato(tm, vp, 0);
            vibflag = 0;
        }
    }
    
#else /* PRECALC_LOOPS */
    while (count--)
    {
        if (!cc--)
        {
            cc=vp->vibrato_control_ratio;
            incr=update_vibrato(tm, vp, 0);
        }
        RESAMPLATION;
        ofs += incr;
        if (ofs>=le)
        ofs -= ll; /* Hopefully the loop is longer than an increment. */
    }
#endif /* PRECALC_LOOPS */
    
    vp->vibrato_control_counter=cc;
    vp->sample_increment=incr;
    vp->sample_offset=ofs; /* Update offset */
    return tm->resample_buffer;
}

static sample_t *rs_vib_bidir(Timid *tm, Voice *vp, int32 count)
{
    INTERPVARS;
    int32
    ofs=vp->sample_offset,
    incr=vp->sample_increment,
    le=vp->sample->loop_end,
    ls=vp->sample->loop_start;
    sample_t
    *dest=tm->resample_buffer,
    *src=vp->sample->data;
    int
    cc=vp->vibrato_control_counter;
    
#ifdef PRECALC_LOOPS
    int32
    le2=le<<1,
    ls2=ls<<1,
    i;
    int
    vibflag = 0;
    
    /* Play normally until inside the loop region */
    while (count && (ofs <= ls))
    {
        i = (ls - ofs) / incr + 1;
        if (i > count) i = count;
        if (i > cc)
        {
            i = cc;
            vibflag = 1;
        }
        else cc -= i;
        count -= i;
        while (i--)
        {
            RESAMPLATION;
            ofs += incr;
        }
        if (vibflag)
        {
            cc = vp->vibrato_control_ratio;
            incr = update_vibrato(tm, vp, 0);
            vibflag = 0;
        }
    }
    
    /* Then do the bidirectional looping */
    
    while (count)
    {
        /* Precalc how many times we should go through the loop */
        i = ((incr > 0 ? le : ls) - ofs) / incr + 1;
        if(i > count) i = count;
        if(i > cc)
        {
            i = cc;
            vibflag = 1;
        }
        else cc -= i;
        count -= i;
        while (i--)
        {
            RESAMPLATION;
            ofs += incr;
        }
        if (vibflag)
        {
            cc = vp->vibrato_control_ratio;
            incr = update_vibrato(tm, vp, (incr < 0));
            vibflag = 0;
        }
        if (ofs >= le)
        {
            /* fold the overshoot back in */
            ofs = le2 - ofs;
            incr *= -1;
        }
        else if (ofs <= ls)
        {
            ofs = ls2 - ofs;
            incr *= -1;
        }
    }
    
#else /* PRECALC_LOOPS */
    /* Play normally until inside the loop region */
    
    if (ofs < ls)
    {
        while (count--)
        {
            if (!cc--)
            {
                cc=vp->vibrato_control_ratio;
                incr=update_vibrato(tm, vp, 0);
            }
            RESAMPLATION;
            ofs += incr;
            if (ofs>=ls)
            break;
        }
    }
    
    /* Then do the bidirectional looping */
    
    if (count>0)
    while (count--)
    {
        if (!cc--)
        {
            cc=vp->vibrato_control_ratio;
            incr=update_vibrato(tm, vp, (incr < 0));
        }
        RESAMPLATION;
        ofs += incr;
        if (ofs>=le)
        {
            /* fold the overshoot back in */
            ofs = le - (ofs - le);
            incr = -incr;
        }
        else if (ofs <= ls)
        {
            ofs = ls + (ls - ofs);
            incr = -incr;
        }
    }
#endif /* PRECALC_LOOPS */
    
    vp->vibrato_control_counter=cc;
    vp->sample_increment=incr;
    vp->sample_offset=ofs; /* Update offset */
    return tm->resample_buffer;
}

sample_t *resample_voice(Timid *tm, int v, int32 *countptr)
{
    int32 ofs;
    uint8 modes;
    Voice *vp=&tm->voice[v];
    
    if (!(vp->sample->sample_rate))
    {
        /* Pre-resampled data -- just update the offset and check if
        we're out of data. */
        ofs=vp->sample_offset >> FRACTION_BITS; /* Kind of silly to use
        FRACTION_BITS here... */
        if (*countptr >= (vp->sample->data_length>>FRACTION_BITS) - ofs)
        {
            /* Note finished. Free the voice. */
            vp->status = VOICE_FREE;
            
            /* Let the caller know how much data we had left */
            *countptr = (vp->sample->data_length>>FRACTION_BITS) - ofs;
        }
        else
        vp->sample_offset += *countptr << FRACTION_BITS;
        
        return vp->sample->data+ofs;
    }
    
    /* Need to resample. Use the proper function. */
    modes=vp->sample->modes;
    
    if (vp->vibrato_control_ratio)
    {
        if ((modes & MODES_LOOPING) &&
        ((modes & MODES_ENVELOPE) ||
        (vp->status==VOICE_ON || vp->status==VOICE_SUSTAINED)))
        {
            if (modes & MODES_PINGPONG)
            return rs_vib_bidir(tm, vp, *countptr);
            else
            return rs_vib_loop(tm, vp, *countptr);
        }
        else
        return rs_vib_plain(tm, v, countptr);
    }
    else
    {
        if ((modes & MODES_LOOPING) &&
        ((modes & MODES_ENVELOPE) ||
        (vp->status==VOICE_ON || vp->status==VOICE_SUSTAINED)))
        {
            if (modes & MODES_PINGPONG)
            return rs_bidir(tm, vp, *countptr);
            else
            return rs_loop(tm, vp, *countptr);
        }
        else
        return rs_plain(tm, v, countptr);
    }
}

void pre_resample(Timid *tm, Sample * sp)
{
    double a, xdiff;
    int32 incr, ofs, newlen, count;
    int16 *newdata, *dest, *src = (int16 *) sp->data, *vptr;
    int32 v, v1, v2, v3, v4, i;
    
    static const char note_name[12][3] =
    {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
        
    a = ((double) (sp->sample_rate) * freq_table[(int) (sp->note_to_use)]) /
    ((double) (sp->root_freq) * tm->play_mode.rate);
    if(sp->data_length / a >= 0x7fffffffL)
    {
        /* Too large to compute */
        return;
    }
    newlen = (int32)(sp->data_length / a);
    dest = newdata = (int16 *)safe_malloc((newlen >> (FRACTION_BITS - 1)) + 2);
    
    count = (newlen >> FRACTION_BITS) - 1;
    ofs = incr = (sp->data_length - (1 << FRACTION_BITS)) / count;
    
    if (--count)
    *dest++ = src[0];
    
    /* Since we're pre-processing and this doesn't have to be done in
    real-time, we go ahead and do the full sliding cubic interpolation. */
    count--;
    for(i = 0; i < count; i++)
    {
        vptr = src + (ofs >> FRACTION_BITS);
        v1 = *(vptr - 1);
        v2 = *vptr;
        v3 = *(vptr + 1);
        v4 = *(vptr + 2);
        xdiff = FSCALENEG(ofs & FRACTION_MASK, FRACTION_BITS);
        v = (int32)(v2 + (xdiff / 6.0) * (-2 * v1 - 3 * v2 + 6 * v3 - v4 +
        xdiff * (3 * (v1 - 2 * v2 + v3) + xdiff * (-v1 + 3 * (v2 - v3) + v4))));
        if(v < -32768)
        *dest++ = -32768;
        else if(v > 32767)
        *dest++ = 32767;
        else
        *dest++ = (int16)v;
        ofs += incr;
    }
    
    if (ofs & FRACTION_MASK)
    {
        v1 = src[ofs >> FRACTION_BITS];
        v2 = src[(ofs >> FRACTION_BITS) + 1];
        *dest++ = (int16)(v1 + (((v2 - v1) * (ofs & FRACTION_MASK)) >> FRACTION_BITS));
    }
    else
    *dest++ = src[ofs >> FRACTION_BITS];
    *dest = *(dest - 1) / 2;
    *dest = *(dest - 1) / 2;
    
    sp->data_length = newlen;
    sp->loop_start = (int32)(sp->loop_start / a);
    sp->loop_end = (int32)(sp->loop_end / a);
    free(sp->data);
    sp->data = (sample_t *) newdata;
    sp->sample_rate = 0;
}

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

chorus.c -- SKChorus integration

*/

#include <stdio.h>

#ifndef _WIN32_WCE
#include <string.h>
#endif

#include "internal.h"

#define CHORUS_LEN 0.01
#define CHORUS_RATE 0.5

void init_chorus(Timid *tm)
{
    tm->chorus_l = sk_chorus_new(tm->play_mode.rate, CHORUS_LEN);
    if (!tm->chorus_l)
    {
        return;
    }
    tm->chorus_r = sk_chorus_new(tm->play_mode.rate, CHORUS_LEN);
    if (!tm->chorus_r)
    {
        sk_chorus_del(tm->chorus_l);
        tm->chorus_l = NULL;
        return;
    }
    sk_chorus_rate(tm->chorus_l, CHORUS_RATE);
    sk_chorus_mix(tm->chorus_l, 1.0);
    sk_chorus_rate(tm->chorus_r, CHORUS_RATE * -1);
    sk_chorus_mix(tm->chorus_r, 1.0);
    apply_chorus_depth(tm);
}

void free_chorus(Timid *tm)
{
    if (!tm->chorus_l && !tm->chorus_r)
    {
        return;
    }
    sk_chorus_del(tm->chorus_l);
    tm->chorus_l = NULL;
    sk_chorus_del(tm->chorus_r);
    tm->chorus_r = NULL;
}

void reset_chorus(Timid *tm)
{
    free_chorus(tm);
    init_chorus(tm);
}

void apply_chorus_depth(Timid *tm)
{
    if (!tm->chorus_l && !tm->chorus_r)
    {
        return;
    }
    sk_chorus_depth(tm->chorus_l, (float)tm->chorus_depth);
    sk_chorus_depth(tm->chorus_r, (float)tm->chorus_depth);
}

void process_chorus(Timid *tm, int32 *buf, int32 *send_buf, int32 count)
{
    int32 scale;
    int32 i;
    if ((!tm->chorus_l && !tm->chorus_r) || !tm->chorus_enabled || tm->chorus_depth <= 0.0 || count <= 0)
    {
        return;
    }
    scale = 1 << (31 - GUARD_BITS);
    if (count > AUDIO_BUFFER_SIZE)
    {
        count = AUDIO_BUFFER_SIZE;
    }
    for (i=0; i<count; i++)
    {
        if (!(tm->play_mode.encoding & PE_MONO))
        {
            buf[i*2+0] += (int32)(sk_chorus_tick(tm->chorus_l, (float)send_buf[i] / (float)scale) * (float)scale);
            buf[i*2+1] += (int32)(sk_chorus_tick(tm->chorus_r, (float)send_buf[i] / (float)scale) * (float)scale);
        }
        else
        {
            buf[i] += (int32)(sk_chorus_tick(tm->chorus_l, (float)send_buf[i] / (float)scale) * (float)scale);
        }
    }
}

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


   internal.h
*/

#ifndef TIMID_INTERNAL_H
#define TIMID_INTERNAL_H

#include "config.h"
#include "timid.h"
#include "ReverbEffect.h"
#include "skchorus.h"
#include "mtwister.h"

typedef struct {
  char *path;
  void *next;
} PathList;

/* Noise modes for open_file */
#define OF_SILENT	0
#define OF_NORMAL	1
#define OF_VERBOSE	2

/* Order of the FIR filter = 20 should be enough ! */
#define ORDER 20
#define ORDER2 ORDER/2

#ifndef PI
#define PI   3.14159265
#endif

typedef struct {
  int32
    loop_start, loop_end, data_length,
    sample_rate, low_freq, high_freq, root_freq;
  int32
    envelope_rate[6], envelope_offset[6];
  FLOAT_T
    volume;
  sample_t *data;
  int32 
    tremolo_sweep_increment, tremolo_phase_increment, 
    vibrato_sweep_increment, vibrato_control_ratio;
  uint8
    tremolo_depth, vibrato_depth,
    modes;
  int8
    panning, note_to_use;
} Sample;

/* Bits in modes: */
#define MODES_16BIT	(1<<0)
#define MODES_UNSIGNED	(1<<1)
#define MODES_LOOPING	(1<<2)
#define MODES_PINGPONG	(1<<3)
#define MODES_REVERSE	(1<<4)
#define MODES_SUSTAIN	(1<<5)
#define MODES_ENVELOPE	(1<<6)

typedef struct {
  int samples;
  Sample *sample;
} Instrument;

typedef struct {
  char *name;
  Instrument *instrument;
  int note, amp, pan, strip_loop, strip_envelope, strip_tail;
} ToneBankElement;

/* A hack to delay instrument loading until after reading the
   entire MIDI file. */
#define MAGIC_LOAD_INSTRUMENT ((Instrument *)(-1))

typedef struct {
  ToneBankElement tone[128];
} ToneBank;

#define SPECIAL_PROGRAM -1

/* Data format encoding bits */

#define PE_MONO 	0x01  /* versus stereo */
#define PE_SIGNED	0x02  /* versus unsigned */
#define PE_16BIT 	0x04  /* versus 8-bit */
#define PE_ULAW 	0x08  /* versus linear */
#define PE_BYTESWAP	0x10  /* versus the other way */

typedef struct {
  int32 rate, encoding;
} PlayMode;

typedef struct {
  int32 time;
  uint8 channel, type, a, b;
} MidiEvent;

/* Midi events */
#define ME_NONE 	0
#define ME_NOTEON	1
#define ME_NOTEOFF	2
#define ME_KEYPRESSURE	3
#define ME_MAINVOLUME	4
#define ME_PAN		5
#define ME_SUSTAIN	6
#define ME_EXPRESSION	7
#define ME_PITCHWHEEL	8
#define ME_PROGRAM	9
#define ME_MONO	10
#define ME_PITCH_SENS	11

#define ME_ALL_SOUNDS_OFF	12
#define ME_RESET_CONTROLLERS	13
#define ME_ALL_NOTES_OFF	14
#define ME_TONE_BANK	15

#define ME_POLY	16

#define ME_TEMPO	17

#define ME_MODULATION	18

#define ME_REVERB	19

#define ME_CHORUS	20

#define ME_EOT		99

typedef struct {
  int
    bank, program, volume, sustain, panning, pitchbend, expression, 
    mono, /* one note only on this channel */
    pitchsens,
    modulation,
    reverb,
    chorus;
  /* chorus, reverb... Coming soon to a 300-MHz, eight-way superscalar
     processor near you */
  FLOAT_T
    pitchfactor; /* precomputed pitch bend factor to save some fdiv's */
} Channel;

/* Causes the instrument's default panning to be used. */
#define NO_PANNING -1

typedef struct {
  uint8
    status, channel, note, velocity;
  Sample *sample;
  int32
    orig_frequency, frequency,
    sample_offset, sample_increment,
    envelope_volume, envelope_target, envelope_increment,
    tremolo_sweep, tremolo_sweep_position,
    tremolo_phase, tremolo_phase_increment,
    vibrato_sweep, vibrato_sweep_position;
  
  final_volume_t left_mix, right_mix;

  FLOAT_T
    left_amp, right_amp, tremolo_volume;
  int32
    vibrato_sample_increment[VIBRATO_SAMPLE_INCREMENTS];
  int
    vibrato_phase, vibrato_control_ratio, vibrato_control_counter,
    envelope_stage, control_counter, panning, panned;

} Voice;

/* Voice status options: */
#define VOICE_FREE 0
#define VOICE_ON 1
#define VOICE_SUSTAINED 2
#define VOICE_OFF 3
#define VOICE_DIE 4

/* Voice panned options: */
#define PANNED_MYSTERY 0
#define PANNED_LEFT 1
#define PANNED_RIGHT 2
#define PANNED_CENTER 3
/* Anything but PANNED_MYSTERY only uses the left volume */

#define ISDRUMCHANNEL(tm, c) ((tm->drumchannels & (1<<(c))))
#define ISQUIETCHANNEL(tm, c) ((tm->quietchannels & (1<<(c))))

typedef struct {
  MidiEvent event;
  void *next;
} MidiEventList;

#ifdef LOOKUP_SINE
FLOAT_T sine(int x);
#else
#include <math.h>
#define sine(x) (sin((2*PI/1024.0) * (x)))
#endif

#define SINE_CYCLE_LENGTH 1024
extern int32 freq_table[];
extern FLOAT_T vol_table[];
extern FLOAT_T bend_fine[];
extern FLOAT_T bend_coarse[];
extern uint8 *_l2u; /* 13-bit PCM to 8-bit u-law */
extern uint8 _l2u_[]; /* used in LOOKUP_HACK */
#ifdef LOOKUP_HACK
extern int16 _u2l[];
#endif

struct Timid {
  char current_filename[1024];
  /* The paths in this list will be tried whenever we're reading a file */
  PathList *pathlist;
  ToneBank *tonebank[128];
  ToneBank *drumset[128];
  /* This is a special instrument, used for all melodic programs */
  Instrument *default_instrument;
  /* This is only used for tracks that don't specify a program */
  int default_program;
  int antialiasing_allowed;
  int pre_resampling_allowed;
  int fast_decay;
  int dynamic_loading;
  PlayMode play_mode;
  int32 common_buffer[AUDIO_BUFFER_SIZE*2]; /* stereo samples */
  int32 *buffer_pointer;
  int32 reverb_send_buffer[AUDIO_BUFFER_SIZE];
  int32 chorus_send_buffer[AUDIO_BUFFER_SIZE];
  Channel channel[16];
  Voice voice[MAX_VOICES];
  int32 control_rate;
  int32 control_ratio;
  FLOAT_T master_volume;
  int32 drumchannels;
  int32 quietchannels;
  int32 lost_notes;
  int32 cut_notes;
  int adjust_panning_immediately;
  int voices;
  uint8 rpn_msb[16];
  uint8 rpn_lsb[16];
  MidiEvent *event_list;
  MidiEvent *current_event;
  int32 sample_count;
  int32 current_sample;
  FILE* fp_midi;
  int32 events_midi;
  char song_title[256];
  char song_copyright[256];
  char last_smf[1024];
  /* to avoid some unnecessary parameter passing */
  MidiEventList *evlist;
  int32 event_count;
  FILE *fp;
  int32 at;
  /* These would both fit into 32 bits, but they are often added in
  large multiples, so it's simpler to have two roomy ints */
  /* samples per MIDI delta-t */
  int32 sample_increment;
  int32 sample_correction;
  sample_t resample_buffer[AUDIO_BUFFER_SIZE];
#ifdef LOOKUP_HACK
  int32 *mixup;
#ifdef LOOKUP_INTERPOLATION
  int8 *iplookup;
#endif
#endif
  char def_instr_name[256];
  char last_config[1024];
  ReverbEffect reverb;
  int reverb_enabled;
  int reverb_only;
  int reverb_preset;
  FLOAT_T reverb_level;
  sk_chorus *chorus_l;
  sk_chorus *chorus_r;
  int chorus_enabled;
  FLOAT_T chorus_depth;
  MTRand prng;
  int dither_enabled;
};

FILE *open_file(Timid *tm, char *name, int decompress, int noise_mode);
void add_to_pathlist(Timid *tm, char *s);
void free_pathlist(Timid *tm);
void close_file(FILE *fp);
void skip(FILE *fp, size_t len);
void *safe_malloc(size_t count);
void antialiasing(Sample *sp, int32 output_rate);
int load_missing_instruments(Timid *tm);
void free_instruments(Timid *tm);
int set_default_instrument(Timid *tm, char *name);
void free_default_instrument(Timid *tm);
void mix_voice(Timid *tm, int32 *buf, int v, int32 c);
int recompute_envelope(Timid *tm, int v);
void apply_envelope_to_amp(Timid *tm, int v);
MidiEvent *read_midi_file(Timid *tm, FILE *mfp, int32 *count, int32 *sp);
sample_t *resample_voice(Timid *tm, int v, int32 *countptr);
void pre_resample(Timid *tm, Sample *sp);
void init_tables(Timid *tm);
void free_tables(Timid *tm);
int read_config_file(Timid *tm, char *name);
void init_reverb(Timid *tm);
void free_reverb(Timid *tm);
void reset_reverb(Timid *tm);
void set_reverb_preset(Timid *tm, int preset);
void process_reverb(Timid *tm, int32 *buf, int32 *send_buf, int32 count);
void init_chorus(Timid *tm);
void free_chorus(Timid *tm);
void reset_chorus(Timid *tm);
void apply_chorus_depth(Timid *tm);
void process_chorus(Timid *tm, int32 *buf, int32 *send_buf, int32 count);

#endif

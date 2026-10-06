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


   timid.h
*/

#ifndef TIMID_H
#define TIMID_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Timid Timid;

typedef struct {
  signed char data[3];
} int24;

/* General API notes: Unless otherwise indicated, functions that return a value will return non-0 on success, and 0 on failure. Time and duration is represented in milliseconds. API functions for getting strings return the length of the string, regardless if a null string pointer is passed or not */

/* Audio format identifiers for timid_play_smf */

#define AU_CHAR	1
#define AU_SHORT	2
#define AU_24	3
#define AU_LONG	4
#define AU_FLOAT	5
#define AU_DOUBLE	6
#define AU_ULAW	7

/* Reverb presets for timid_set_reverb_preset */

enum {
  TIMID_REVERB_PRESET_GENERIC = 0,
  TIMID_REVERB_PRESET_PADDEDCELL,
  TIMID_REVERB_PRESET_ROOM,
  TIMID_REVERB_PRESET_BATHROOM,
  TIMID_REVERB_PRESET_LIVINGROOM,
  TIMID_REVERB_PRESET_STONEROOM,
  TIMID_REVERB_PRESET_AUDITORIUM,
  TIMID_REVERB_PRESET_CONCERTHALL,
  TIMID_REVERB_PRESET_CAVE,
  TIMID_REVERB_PRESET_ARENA,
  TIMID_REVERB_PRESET_HANGAR,
  TIMID_REVERB_PRESET_CARPETEDHALLWAY,
  TIMID_REVERB_PRESET_HALLWAY,
  TIMID_REVERB_PRESET_STONECORRIDOR,
  TIMID_REVERB_PRESET_ALLEY,
  TIMID_REVERB_PRESET_FOREST,
  TIMID_REVERB_PRESET_CITY,
  TIMID_REVERB_PRESET_MOUNTAINS,
  TIMID_REVERB_PRESET_QUARRY,
  TIMID_REVERB_PRESET_PLAIN,
  TIMID_REVERB_PRESET_PARKINGLOT,
  TIMID_REVERB_PRESET_SEWERPIPE,
  TIMID_REVERB_PRESET_UNDERWATER,
  TIMID_REVERB_PRESET_DRUGGED,
  TIMID_REVERB_PRESET_DIZZY,
  TIMID_REVERB_PRESET_PSYCHOTIC,
  TIMID_REVERB_PRESET_CASTLE_SMALLROOM,
  TIMID_REVERB_PRESET_CASTLE_SHORTPASSAGE,
  TIMID_REVERB_PRESET_CASTLE_MEDIUMROOM,
  TIMID_REVERB_PRESET_CASTLE_LARGEROOM,
  TIMID_REVERB_PRESET_CASTLE_LONGPASSAGE,
  TIMID_REVERB_PRESET_CASTLE_HALL,
  TIMID_REVERB_PRESET_CASTLE_CUPBOARD,
  TIMID_REVERB_PRESET_CASTLE_COURTYARD,
  TIMID_REVERB_PRESET_CASTLE_ALCOVE,
  TIMID_REVERB_PRESET_FACTORY_SMALLROOM,
  TIMID_REVERB_PRESET_FACTORY_SHORTPASSAGE,
  TIMID_REVERB_PRESET_FACTORY_MEDIUMROOM,
  TIMID_REVERB_PRESET_FACTORY_LARGEROOM,
  TIMID_REVERB_PRESET_FACTORY_LONGPASSAGE,
  TIMID_REVERB_PRESET_FACTORY_HALL,
  TIMID_REVERB_PRESET_FACTORY_CUPBOARD,
  TIMID_REVERB_PRESET_FACTORY_COURTYARD,
  TIMID_REVERB_PRESET_FACTORY_ALCOVE,
  TIMID_REVERB_PRESET_ICEPALACE_SMALLROOM,
  TIMID_REVERB_PRESET_ICEPALACE_SHORTPASSAGE,
  TIMID_REVERB_PRESET_ICEPALACE_MEDIUMROOM,
  TIMID_REVERB_PRESET_ICEPALACE_LARGEROOM,
  TIMID_REVERB_PRESET_ICEPALACE_LONGPASSAGE,
  TIMID_REVERB_PRESET_ICEPALACE_HALL,
  TIMID_REVERB_PRESET_ICEPALACE_CUPBOARD,
  TIMID_REVERB_PRESET_ICEPALACE_COURTYARD,
  TIMID_REVERB_PRESET_ICEPALACE_ALCOVE,
  TIMID_REVERB_PRESET_SPACESTATION_SMALLROOM,
  TIMID_REVERB_PRESET_SPACESTATION_SHORTPASSAGE,
  TIMID_REVERB_PRESET_SPACESTATION_MEDIUMROOM,
  TIMID_REVERB_PRESET_SPACESTATION_LARGEROOM,
  TIMID_REVERB_PRESET_SPACESTATION_LONGPASSAGE,
  TIMID_REVERB_PRESET_SPACESTATION_HALL,
  TIMID_REVERB_PRESET_SPACESTATION_CUPBOARD,
  TIMID_REVERB_PRESET_SPACESTATION_ALCOVE,
  TIMID_REVERB_PRESET_WOODEN_SMALLROOM,
  TIMID_REVERB_PRESET_WOODEN_SHORTPASSAGE,
  TIMID_REVERB_PRESET_WOODEN_MEDIUMROOM,
  TIMID_REVERB_PRESET_WOODEN_LARGEROOM,
  TIMID_REVERB_PRESET_WOODEN_LONGPASSAGE,
  TIMID_REVERB_PRESET_WOODEN_HALL,
  TIMID_REVERB_PRESET_WOODEN_CUPBOARD,
  TIMID_REVERB_PRESET_WOODEN_COURTYARD,
  TIMID_REVERB_PRESET_WOODEN_ALCOVE,
  TIMID_REVERB_PRESET_SPORT_EMPTYSTADIUM,
  TIMID_REVERB_PRESET_SPORT_SQUASHCOURT,
  TIMID_REVERB_PRESET_SPORT_SMALLSWIMMINGPOOL,
  TIMID_REVERB_PRESET_SPORT_LARGESWIMMINGPOOL,
  TIMID_REVERB_PRESET_SPORT_GYMNASIUM,
  TIMID_REVERB_PRESET_SPORT_FULLSTADIUM,
  TIMID_REVERB_PRESET_SPORT_STADIUMTANNOY,
  TIMID_REVERB_PRESET_PREFAB_WORKSHOP,
  TIMID_REVERB_PRESET_PREFAB_SCHOOLROOM,
  TIMID_REVERB_PRESET_PREFAB_PRACTISEROOM,
  TIMID_REVERB_PRESET_PREFAB_OUTHOUSE,
  TIMID_REVERB_PRESET_PREFAB_CARAVAN,
  TIMID_REVERB_PRESET_DOME_TOMB,
  TIMID_REVERB_PRESET_PIPE_SMALL,
  TIMID_REVERB_PRESET_DOME_SAINTPAULS,
  TIMID_REVERB_PRESET_PIPE_LONGTHIN,
  TIMID_REVERB_PRESET_PIPE_LARGE,
  TIMID_REVERB_PRESET_PIPE_RESONANT,
  TIMID_REVERB_PRESET_OUTDOORS_BACKYARD,
  TIMID_REVERB_PRESET_OUTDOORS_ROLLINGPLAINS,
  TIMID_REVERB_PRESET_OUTDOORS_DEEPCANYON,
  TIMID_REVERB_PRESET_OUTDOORS_CREEK,
  TIMID_REVERB_PRESET_OUTDOORS_VALLEY,
  TIMID_REVERB_PRESET_MOOD_HEAVEN,
  TIMID_REVERB_PRESET_MOOD_HELL,
  TIMID_REVERB_PRESET_MOOD_MEMORY,
  TIMID_REVERB_PRESET_DRIVING_COMMENTATOR,
  TIMID_REVERB_PRESET_DRIVING_PITGARAGE,
  TIMID_REVERB_PRESET_DRIVING_INCAR_RACER,
  TIMID_REVERB_PRESET_DRIVING_INCAR_SPORTS,
  TIMID_REVERB_PRESET_DRIVING_INCAR_LUXURY,
  TIMID_REVERB_PRESET_DRIVING_FULLGRANDSTAND,
  TIMID_REVERB_PRESET_DRIVING_EMPTYGRANDSTAND,
  TIMID_REVERB_PRESET_DRIVING_TUNNEL,
  TIMID_REVERB_PRESET_CITY_STREETS,
  TIMID_REVERB_PRESET_CITY_SUBWAY,
  TIMID_REVERB_PRESET_CITY_MUSEUM,
  TIMID_REVERB_PRESET_CITY_LIBRARY,
  TIMID_REVERB_PRESET_CITY_UNDERPASS,
  TIMID_REVERB_PRESET_CITY_ABANDONED,
  TIMID_REVERB_PRESET_DUSTYROOM,
  TIMID_REVERB_PRESET_CHAPEL,
  TIMID_REVERB_PRESET_SMALLWATERROOM,
  NUM_TIMID_REVERB_PRESETS
};

/* Allocate and initialize an instance of Timidity */
Timid *timid_init(void);

/* Manage Timidity configurations (sample sets) */
int timid_load_config(Timid *tm, char *filename);
void timid_unload_config(Timid *tm);
int timid_reload_config(Timid *tm);

/* High level input API */
void timid_channel_note_on(Timid *tm, unsigned char channel, unsigned char note, unsigned char velocity);
void timid_channel_note_off(Timid *tm, unsigned char channel, unsigned char note);
void timid_channel_key_pressure(Timid *tm, unsigned char channel, unsigned char note, unsigned char velocity);
void timid_channel_set_modulation(Timid *tm, unsigned char channel, unsigned char amount);
void timid_channel_set_volume(Timid *tm, unsigned char channel, unsigned char volume);
void timid_channel_set_pan(Timid *tm, unsigned char channel, unsigned char pan);
void timid_channel_set_expression(Timid *tm, unsigned char channel, unsigned char expression);
void timid_channel_set_sustain(Timid *tm, unsigned char channel, unsigned char sustain);
void timid_channel_set_reverb(Timid *tm, unsigned char channel, unsigned char level);
void timid_channel_set_chorus(Timid *tm, unsigned char channel, unsigned char level);
void timid_channel_set_pitch_wheel(Timid *tm, unsigned char channel, unsigned short pitch);
void timid_channel_set_pitch_range(Timid *tm, unsigned char channel, unsigned char range);
void timid_channel_set_program(Timid *tm, unsigned char channel, unsigned char program);
void timid_channel_set_bank(Timid *tm, unsigned char channel, unsigned char bank);
void timid_channel_mono_mode(Timid *tm, unsigned char channel);
void timid_channel_poly_mode(Timid *tm, unsigned char channel);
void timid_channel_all_notes_off(Timid *tm, unsigned char channel);
void timid_channel_all_sounds_off(Timid *tm, unsigned char channel);
void timid_channel_reset_controllers(Timid *tm, unsigned char channel);
void timid_channel_control_change(Timid *tm, unsigned char channel, unsigned char controller, unsigned char value);

/* Low level input API */
void timid_write_midi(Timid *tm, unsigned char byte1, unsigned char byte2, unsigned char byte3);
void timid_write_midi_packed(Timid *tm, unsigned long data);
void timid_write_sysex(Timid *tm, unsigned char *buffer, long count);

/* Audio output functions */
void timid_render_char(Timid *tm, unsigned char *buffer, long count);
void timid_render_short(Timid *tm, short *buffer, long count);
void timid_render_24(Timid *tm, int24 *buffer, long count);
void timid_render_long(Timid *tm, long *buffer, long count);
void timid_render_float(Timid *tm, float *buffer, long count);
void timid_render_double(Timid *tm, double *buffer, long count);
void timid_render_ulaw(Timid *tm, unsigned char *buffer, long count);

/* Stop all notes with release */
void timid_all_notes_off(Timid *tm);
/* Stop all notes with quick fade out, helps avoid clicks */
void timid_all_sounds_off(Timid *tm);
/* Reset all MIDI controllers */
void timid_reset_controllers(Timid *tm);
/* Stop all notes immediately */
void timid_panic(Timid *tm);
/* Stop all notes immediately, and reset all MIDI parameters */
void timid_reset(Timid *tm);

/* MIDI file player, only supports standard MIDI files */
int timid_load_smf(Timid *tm, char *filename);
void timid_unload_smf(Timid *tm);
int timid_reload_smf(Timid *tm);
/* The following function returns 1 if audio has been rendered, and 0 once the track is finished and all notes have stopped */
int timid_play_smf(Timid *tm, long type, unsigned char *buffer, long count); /* count is in samples */
/* The return value for the following functions is the new current time */
/* Absolute seeking */
int timid_seek_smf(Timid *tm, long time);
/* Relative seeking */
int timid_fast_forward_smf(Timid *tm, long time);
int timid_rewind_smf(Timid *tm, long time);
/* Quick ways to restart or stop a track without reloading or unloading */
int timid_restart_smf(Timid *tm);
int timid_stop_smf(Timid *tm);

/* Setters */
/* Amplification is represented in percent */
void timid_set_amplification(Timid *tm, int amplification);
/* The number of voices is clamped between 1 and MAX_VOICES */
void timid_set_max_voices(Timid *tm, int voices);
/* The value argument for the following functions should be treated as a boolean */
void timid_set_immediate_panning(Timid *tm, int value);
/* Renders mono audio buffers if enabled, interleaved stereo otherwise */
void timid_set_mono(Timid *tm, int value);
/* These next few functions reload the current sample bank before returning */
void timid_set_fast_decay(Timid *tm, int value);
void timid_set_antialiasing(Timid *tm, int value);
void timid_set_pre_resample(Timid *tm, int value);
void timid_set_dynamic_instrument_load(Timid *tm, int value);
/* The sample rate is clamped between MIN_OUTPUT_RATE and MAX_OUTPUT_RATE */
void timid_set_sample_rate(Timid *tm, int rate);
/* The control rate is clamped between current sample rate / MAX_CONTROL_RATIO and current sample rate */
void timid_set_control_rate(Timid *tm, int rate);
/* Sets the default MIDI program, takes effect on next MIDI reset */
void timid_set_default_program(Timid *tm, int program);
void timid_set_drum_channel(Timid *tm, int channel, int enable);
void timid_set_quiet_channel(Timid *tm, int channel, int enable);
void timid_set_reverb_enabled(Timid *tm, int enable);
void timid_set_reverb_only(Timid *tm, int enable);
void timid_set_reverb_level(Timid *tm, int percent);
void timid_set_reverb_preset(Timid *tm, int preset);
void timid_set_chorus_enabled(Timid *tm, int enable);
void timid_set_chorus_depth(Timid *tm, int percent);
void timid_set_dither_enabled(Timid *tm, int enable);

/* Restore default settings */
void timid_restore_defaults(Timid *tm);

/* Force all instruments to be loaded */
int timid_force_instrument_load(Timid *tm);

/* Manage default instruments. These functions take effect on the next MIDI reset */
int timid_set_default_instrument(Timid *tm, char *filename);
void timid_free_default_instrument(Timid *tm);

/* Getters */
int timid_get_config_name(Timid *tm, char *buffer, long count);
int timid_get_amplification(Timid *tm);
int timid_get_active_voices(Timid *tm);
int timid_get_max_voices(Timid *tm);
int timid_get_immediate_panning(Timid *tm);
int timid_get_mono(Timid *tm);
int timid_get_fast_decay(Timid *tm);
int timid_get_antialiasing(Timid *tm);
int timid_get_pre_resample(Timid *tm);
int timid_get_dynamic_instrument_load(Timid *tm);
int timid_get_sample_rate(Timid *tm);
int timid_get_control_rate(Timid *tm);
int timid_get_default_program(Timid *tm);
int timid_get_drum_channel_enabled(Timid *tm, int channel);
int timid_get_quiet_channel_enabled(Timid *tm, int channel);
int timid_get_reverb_enabled(Timid *tm);
int timid_get_reverb_only(Timid *tm);
int timid_get_reverb_level(Timid *tm);
int timid_get_reverb_preset(Timid *tm);
int timid_get_chorus_enabled(Timid *tm);
int timid_get_chorus_depth(Timid *tm);
int timid_get_dither_enabled(Timid *tm);
int timid_get_lost_notes(Timid *tm);
int timid_get_cut_notes(Timid *tm);

/* Get values from a given MIDI channel */
int timid_channel_get_modulation(Timid *tm, int channel);
int timid_channel_get_volume(Timid *tm, int channel);
int timid_channel_get_pan(Timid *tm, int channel);
int timid_channel_get_expression(Timid *tm, int channel);
int timid_channel_get_sustain(Timid *tm, int channel);
int timid_channel_get_reverb(Timid *tm, int channel);
int timid_channel_get_chorus(Timid *tm, int channel);
int timid_channel_get_pitch_wheel(Timid *tm, int channel);
int timid_channel_get_pitch_range(Timid *tm, int channel);
int timid_channel_get_program(Timid *tm, int channel);
/* The following function returns -1 for drum channels */
int timid_channel_get_bank(Timid *tm, int channel);
int timid_channel_get_mono(Timid *tm, int channel);

/* These are for the MIDI file player */
int timid_get_smf_name(Timid *tm, char *buffer, long count);
int timid_get_event_count(Timid *tm);
int timid_get_sample_count(Timid *tm);
int timid_get_current_sample_position(Timid *tm);
int timid_get_duration(Timid *tm);
int timid_get_current_time(Timid *tm);
/* Bitrate is measured in KBPS */
int timid_get_bitrate(Timid *tm);
int timid_get_song_title(Timid *tm, char *buffer, long count);
int timid_get_song_copyright(Timid *tm, char *buffer, long count);

/* Utility functions */
int timid_millis2samples(Timid *tm, long millis);
int timid_samples2millis(Timid *tm, long samples);

/* Close and free an instance of Timidity. This should be called after all other API function calls */
void timid_close(Timid *tm);

#ifdef __cplusplus
}
#endif

#endif

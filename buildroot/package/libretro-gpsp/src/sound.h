/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SOUND_H
#define SOUND_H

#define BUFFER_SIZE        (1 << 16)
#define BUFFER_SIZE_MASK   (BUFFER_SIZE - 1)

#define GBA_SOUND_FREQUENCY   (64 * 1024)

#define GBC_BASE_RATE ((float)(16 * 1024 * 1024))

typedef enum
{
   DIRECT_SOUND_INACTIVE,
   DIRECT_SOUND_RIGHT,
   DIRECT_SOUND_LEFT,
   DIRECT_SOUND_LEFTRIGHT
} direct_sound_status_type;

typedef enum
{
   DIRECT_SOUND_VOLUME_50,
   DIRECT_SOUND_VOLUME_100
} direct_sound_volume_type;

typedef struct
{
   s8 fifo[32];
   u32 fifo_base;
   u32 fifo_top;
   fixed8_24 fifo_fractional;
   // The + 1 is to give some extra room for linear interpolation
   // when wrapping around.
   u32 buffer_index;
   direct_sound_status_type status;
   direct_sound_volume_type volume;
   u32 last_cpu_ticks;
} direct_sound_struct;

typedef enum
{
   GBC_SOUND_INACTIVE,
   GBC_SOUND_RIGHT,
   GBC_SOUND_LEFT,
   GBC_SOUND_LEFTRIGHT
} gbc_sound_status_type;


typedef struct
{
   u32 rate;
   fixed16_16 frequency_step;
   fixed16_16 sample_index;
   fixed16_16 tick_counter;
   u32 total_volume;
   u32 envelope_initial_volume;
   u32 envelope_volume;
   u32 envelope_direction;
   u32 envelope_status;
   u32 envelope_step;
   u32 envelope_ticks;
   u32 envelope_initial_ticks;
   u32 sweep_status;
   u32 sweep_direction;
   u32 sweep_ticks;
   u32 sweep_initial_ticks;
   u32 sweep_shift;
   u32 length_status;
   u32 length_ticks;
   u32 noise_type;
   u32 wave_type;
   u32 wave_bank;
   u32 wave_volume;
   gbc_sound_status_type status;
   u32 active_flag;
   u32 master_enable;
   s8* sample_data;
} gbc_sound_struct;

extern direct_sound_struct direct_sound_channel[2];
extern gbc_sound_struct gbc_sound_channel[4];
extern s8 square_pattern_duty[4][8];
extern u32 gbc_sound_master_volume_left;
extern u32 gbc_sound_master_volume_right;
extern u32 gbc_sound_master_volume;
extern u32 gbc_sound_buffer_index;
extern u32 gbc_sound_last_cpu_ticks;

extern u32 sound_frequency;
extern u32 sound_on;

extern u32 global_enable_audio;
extern u32 enable_low_pass_filter;

void sound_timer_queue8(u32 channel, u8 value);
void sound_timer_queue16(u32 channel, u16 value);
void sound_timer_queue32(u32 channel, u32 value);
void sound_timer(fixed8_24 frequency_step, u32 channel);
void sound_reset_fifo(u32 channel);
void update_gbc_sound(u32 cpu_ticks);
void init_sound(int need_reset);
void sound_write_savestate(void);
void sound_read_savestate(void);

void render_audio(void);

void reset_sound(void);

#endif

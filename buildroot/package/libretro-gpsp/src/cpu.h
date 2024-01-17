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

#ifndef CPU_H
#define CPU_H

#include "gpsp_config.h"

// System mode and user mode are represented as the same here

typedef enum
{
  MODE_USER,
  MODE_IRQ,
  MODE_FIQ,
  MODE_SUPERVISOR,
  MODE_ABORT,
  MODE_UNDEFINED,
  MODE_INVALID
} cpu_mode_type;

typedef enum
{
  CPU_ALERT_NONE,
  CPU_ALERT_HALT,
  CPU_ALERT_SMC,
  CPU_ALERT_IRQ
} cpu_alert_type;

typedef enum
{
  CPU_ACTIVE,
  CPU_HALT,
  CPU_STOP
} cpu_halt_type;

typedef enum
{
  IRQ_NONE     = 0x0000,
  IRQ_VBLANK   = 0x0001,
  IRQ_HBLANK   = 0x0002,
  IRQ_VCOUNT   = 0x0004,
  IRQ_TIMER0   = 0x0008,
  IRQ_TIMER1   = 0x0010,
  IRQ_TIMER2   = 0x0020,
  IRQ_TIMER3   = 0x0040,
  IRQ_SERIAL   = 0x0080,
  IRQ_DMA0     = 0x0100,
  IRQ_DMA1     = 0x0200,
  IRQ_DMA2     = 0x0400,
  IRQ_DMA3     = 0x0800,
  IRQ_KEYPAD   = 0x1000,
  IRQ_GAMEPAK  = 0x2000
} irq_type;

typedef enum
{
  REG_SP            = 13,
  REG_LR            = 14,
  REG_PC            = 15,
  REG_N_FLAG        = 16,
  REG_Z_FLAG        = 17,
  REG_C_FLAG        = 18,
  REG_V_FLAG        = 19,
  REG_CPSR          = 20,
  REG_SAVE          = 21,
  REG_SAVE2         = 22,
  REG_SAVE3         = 23,
  CPU_MODE          = 29,
  CPU_HALT_STATE    = 30,
  CHANGED_PC_STATUS = 31,
  COMPLETED_FRAME   = 32,
  OAM_UPDATED       = 33
} ext_reg_numbers;

typedef enum
{
  TRANSLATION_REGION_RAM,
  TRANSLATION_REGION_ROM,
} translation_region_type;

extern u32 instruction_count;
extern u32 last_instruction;

void execute_arm(u32 cycles);
void raise_interrupt(irq_type irq_raised);
void set_cpu_mode(cpu_mode_type new_mode);

u32 function_cc execute_load_u8(u32 address);
u32 function_cc execute_load_u16(u32 address);
u32 function_cc execute_load_u32(u32 address);
u32 function_cc execute_load_s8(u32 address);
u32 function_cc execute_load_s16(u32 address);
void function_cc execute_store_u8(u32 address, u32 source);
void function_cc execute_store_u16(u32 address, u32 source);
void function_cc execute_store_u32(u32 address, u32 source);
u32 function_cc execute_arm_translate(u32 cycles);
void init_translater(void);
void cpu_write_savestate(void);
void cpu_read_savestate(void);

u8 function_cc *block_lookup_address_arm(u32 pc);
u8 function_cc *block_lookup_address_thumb(u32 pc);
u8 function_cc *block_lookup_address_dual(u32 pc);
s32 translate_block_arm(u32 pc, translation_region_type translation_region,
 u32 smc_enable);
s32 translate_block_thumb(u32 pc, translation_region_type translation_region,
 u32 smc_enable);

#if defined(HAVE_MMAP)
extern u8* rom_translation_cache;
extern u8* ram_translation_cache;
#elif defined(_3DS)
#define rom_translation_cache ((u8*)0x02000000 - ROM_TRANSLATION_CACHE_SIZE)
#define ram_translation_cache (rom_translation_cache - RAM_TRANSLATION_CACHE_SIZE)
extern u8* rom_translation_cache_ptr;
extern u8* ram_translation_cache_ptr;
#elif defined(VITA)
extern u8* rom_translation_cache;
extern u8* ram_translation_cache;
extern int sceBlock;
#else
extern u8 rom_translation_cache[ROM_TRANSLATION_CACHE_SIZE];
extern u8 ram_translation_cache[RAM_TRANSLATION_CACHE_SIZE];
#endif
extern u32 stub_arena[STUB_ARENA_SIZE / 4];
extern u8 *rom_translation_ptr;
extern u8 *ram_translation_ptr;

#define MAX_TRANSLATION_GATES 8

extern u32 idle_loop_target_pc;
extern u32 iwram_stack_optimize;
extern u32 translation_gate_targets;
extern u32 translation_gate_target_pc[MAX_TRANSLATION_GATES];

extern u32 in_interrupt;

extern u32 *rom_branch_hash[ROM_BRANCH_HASH_SIZE];

void flush_translation_cache_rom(void);
void flush_translation_cache_ram(void);
void dump_translation_cache(void);
void init_caches(void);
void init_emitter(void);

extern u32 reg_mode[7][7];
extern u32 spsr[6];

extern u32 cpu_modes[32];
extern const u32 psr_masks[16];

extern u32 memory_region_access_read_u8[16];
extern u32 memory_region_access_read_s8[16];
extern u32 memory_region_access_read_u16[16];
extern u32 memory_region_access_read_s16[16];
extern u32 memory_region_access_read_u32[16];
extern u32 memory_region_access_write_u8[16];
extern u32 memory_region_access_write_u16[16];
extern u32 memory_region_access_write_u32[16];
extern u32 memory_reads_u8;
extern u32 memory_reads_s8;
extern u32 memory_reads_u16;
extern u32 memory_reads_s16;
extern u32 memory_reads_u32;
extern u32 memory_writes_u8;
extern u32 memory_writes_u16;
extern u32 memory_writes_u32;

void init_cpu(void);
void move_reg();

#endif

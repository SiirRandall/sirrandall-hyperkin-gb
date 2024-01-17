#ifndef __DIRTY_INPUT_H___
#define __DIRTY_INPUT_H___

#include "retro_common_api.h"
#include "boolean.h"

RETRO_BEGIN_DECLS

extern bool input_is_dirty;
void add_input_state_hook(void);
void remove_input_state_hook(void);
int16_t input_state_get_last(unsigned port,
   unsigned device, unsigned index, unsigned id);

RETRO_END_DECLS

#endif

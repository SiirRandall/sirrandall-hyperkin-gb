/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017- Andrés Suárez
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/*  This file is intended for menu functions, custom controls, etc. */

#ifndef _NK_MENU_H
#define _NK_MENU_H

#include "nk_common.h"

#include "../../menu_driver.h"
#include "../../menu_input.h"

enum
{
   NK_WND_DEBUG = 0,
   NK_WND_LAST
};

struct icons {
    struct nk_image folder;
    struct nk_image monitor;
    struct nk_image gamepad;
    struct nk_image settings;
    struct nk_image speaker;
    struct nk_image invader;
    struct nk_image page_on;
    struct nk_image page_off;
};

struct window {
   bool open;
   struct nk_vec2 position;
   struct nk_vec2 size;
};

typedef struct nk_menu_handle
{
   /* nuklear mandatory */
   void *memory;
   struct nk_context ctx;
   struct nk_memory_status status;
   enum menu_action action;

   /* window control variables */
   struct nk_vec2 size;
   bool size_changed;
   struct window window[5];

   /* menu driver variables */
   char box_message[PATH_MAX_LENGTH];

   /* image & theme related variables */
   char assets_directory[PATH_MAX_LENGTH];
   struct icons icons;

   struct
   {
      menu_texture_item bg;
      menu_texture_item pointer;
   } textures;

   video_font_raster_block_t list_block;
} nk_menu_handle_t;

struct nk_color nk_colors[NK_COLOR_COUNT];

void nk_wnd_debug(nk_menu_handle_t *nk);
void nk_wnd_set_state(nk_menu_handle_t *nk, const int id,
   struct nk_vec2 pos, struct nk_vec2 size);
void nk_wnd_get_state(nk_menu_handle_t *nk, const int id,
   struct nk_vec2 *pos, struct nk_vec2 *size);
void nk_common_set_style(struct nk_context *ctx);

#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2018 - Brad Parker
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

/* SIXEL context. */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../verbosity.h"
#include "../../ui/ui_companion_driver.h"
#include "../video_driver.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

static enum gfx_ctx_api sixel_ctx_api = GFX_CTX_NONE;

static void gfx_ctx_sixel_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
}

static bool gfx_ctx_sixel_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   return false;
}

static void gfx_ctx_sixel_update_window_title(void *data, void *data2)
{
#if defined(_WIN32) && !defined(_XBOX)
   const ui_window_t *window = ui_companion_driver_get_window_ptr();
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (window && title[0])
      window->set_title(&main_window, title);
#endif
}

static void gfx_ctx_sixel_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   (void)data;
}

static void *gfx_ctx_sixel_init(
      video_frame_info_t *video_info, void *video_driver)
{
   (void)video_driver;

   return (void*)"sixel";
}

static void gfx_ctx_sixel_destroy(void *data)
{
   (void)data;
}

static bool gfx_ctx_sixel_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   return true;
}


static void gfx_ctx_sixel_input_driver(void *data,
      const char *joypad_name,
      const input_driver_t **input, void **input_data)
{
   (void)data;

#ifdef HAVE_UDEV
   *input_data = input_udev.init(joypad_name);

   if (*input_data)
   {
      *input = &input_udev;
      return;
   }
#endif
   *input = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_sixel_has_focus(void *data)
{
   return true;
}

static bool gfx_ctx_sixel_suppress_screensaver(void *data, bool enable)
{
   return true;
}

static bool gfx_ctx_sixel_has_windowed(void *data)
{
   (void)data;

   return true;
}

static bool gfx_ctx_sixel_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   return false;
}

static enum gfx_ctx_api gfx_ctx_sixel_get_api(void *data)
{
   return sixel_ctx_api;
}

static bool gfx_ctx_sixel_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   return true;
}

static void gfx_ctx_sixel_show_mouse(void *data, bool state)
{
   (void)data;
}

static void gfx_ctx_sixel_swap_interval(void *data, unsigned interval)
{
   (void)data;
   (void)interval;
}

static void gfx_ctx_sixel_set_flags(void *data, uint32_t flags)
{
   (void)data;
   (void)flags;
}

static uint32_t gfx_ctx_sixel_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);
   return flags;
}

static void gfx_ctx_sixel_swap_buffers(void *data, void *data2)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_sixel = {
   gfx_ctx_sixel_init,
   gfx_ctx_sixel_destroy,
   gfx_ctx_sixel_get_api,
   gfx_ctx_sixel_bind_api,
   gfx_ctx_sixel_swap_interval,
   gfx_ctx_sixel_set_video_mode,
   gfx_ctx_sixel_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_sixel_get_metrics,
   NULL,
   gfx_ctx_sixel_update_window_title,
   gfx_ctx_sixel_check_window,
   gfx_ctx_sixel_set_resize,
   gfx_ctx_sixel_has_focus,
   gfx_ctx_sixel_suppress_screensaver,
   gfx_ctx_sixel_has_windowed,
   gfx_ctx_sixel_swap_buffers,
   gfx_ctx_sixel_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_sixel_show_mouse,
   "sixel",
   gfx_ctx_sixel_get_flags,
   gfx_ctx_sixel_set_flags,
   NULL,
   NULL,
   NULL
};


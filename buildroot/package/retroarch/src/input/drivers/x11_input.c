/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>

#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <boolean.h>
#include <compat/strl.h>
#include <retro_inline.h>

#include "../input_driver.h"
#include "../input_keymaps.h"

#include "../../gfx/video_driver.h"
#include "../common/input_x11_common.h"

#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct x11_input
{
   bool blocked;
   const input_device_driver_t *joypad;

   Display *display;
   Window win;

   char state[32];
   bool mouse_l, mouse_r, mouse_m;
   int mouse_x, mouse_y;
   int mouse_last_x, mouse_last_y;

   bool grab_mouse;
} x11_input_t;

static void *x_input_init(const char *joypad_driver)
{
   x11_input_t *x11;

   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
   {
      RARCH_ERR("Currently active window is not an X11 window. Cannot use this driver.\n");
      return NULL;
   }

   x11 = (x11_input_t*)calloc(1, sizeof(*x11));
   if (!x11)
      return NULL;

   /* Borrow the active X window ... */
   x11->display = (Display*)video_driver_display_get();
   x11->win     = (Window)video_driver_window_get();

   x11->joypad  = input_joypad_init_driver(joypad_driver, x11);
   input_keymaps_init_keyboard_lut(rarch_key_map_x11);

   return x11;
}

static bool x_keyboard_pressed(x11_input_t *x11, unsigned key)
{
   int keycode = XKeysymToKeycode(x11->display, rarch_keysym_lut[(enum retro_key)key]);
   return x11->state[keycode >> 3] & (1 << (keycode & 7));
}

static bool x_mbutton_pressed(x11_input_t *x11, unsigned port, unsigned key)
{
   bool result;
   settings_t *settings = config_get_ptr();

   if (port >= MAX_USERS)
      return false;

   /* the driver only supports one mouse */
   if ( settings->uints.input_mouse_index[ port ] != 0 )
      return false;

   switch ( key )
   {

   case RETRO_DEVICE_ID_MOUSE_LEFT:
      return x11->mouse_l;
   case RETRO_DEVICE_ID_MOUSE_RIGHT:
      return x11->mouse_r;
   case RETRO_DEVICE_ID_MOUSE_MIDDLE:
      return x11->mouse_m;
/*   case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
      return x11->mouse_b4;*/
/*   case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
      return x11->mouse_b5;*/

   case RETRO_DEVICE_ID_MOUSE_WHEELUP:
   case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
      return x_mouse_state_wheel( key );

/*   case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
      result = x11->mouse_hwu;
      x11->mouse_hwu = false;
      return result;

   case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
      result = x11->mouse_hwd;
      x11->mouse_hwd = false;
      return result;
*/
   }

   return false;
}

static bool x_is_pressed(x11_input_t *x11,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind *binds,
      unsigned port, unsigned id)
{
   const struct retro_keybind *bind = &binds[id];

   if ((bind->key < RETROK_LAST) && x_keyboard_pressed(x11, bind->key) )
      if ((id == RARCH_GAME_FOCUS_TOGGLE) || !x11->blocked)
         return true;

   if (binds && binds[id].valid)
   {
      if (x_mbutton_pressed(x11, port, bind->mbutton))
         return true;
      if (input_joypad_pressed(x11->joypad, joypad_info, port, binds, id))
         return true;
   }

   return false;
}

static int16_t x_pressed_analog(x11_input_t *x11,
      const struct retro_keybind *binds, unsigned idx, unsigned id)
{
   int16_t pressed_minus = 0;
   int16_t pressed_plus  = 0;
   unsigned id_minus     = 0;
   unsigned id_plus      = 0;
   int id_minus_key      = 0;
   int id_plus_key       = 0;
   unsigned sym          = 0;
   int keycode           = 0;

   input_conv_analog_id_to_bind_id(idx, id, &id_minus, &id_plus);

   id_minus_key          = binds[id_minus].key;
   id_plus_key           = binds[id_plus].key;

   sym                   = rarch_keysym_lut[(enum retro_key)id_minus_key];
   keycode               = XKeysymToKeycode(x11->display, sym);
   if (      binds[id_minus].valid
         && (id_minus_key < RETROK_LAST)
         && (x11->state[keycode >> 3] & (1 << (keycode & 7))))
      pressed_minus = -0x7fff;

   sym                   = rarch_keysym_lut[(enum retro_key)id_plus_key];
   keycode               = XKeysymToKeycode(x11->display, sym);
   if (      binds[id_plus].valid
         && (id_plus_key < RETROK_LAST)
         && (x11->state[keycode >> 3] & (1 << (keycode & 7))))
      pressed_plus  =  0x7fff;

   return pressed_plus + pressed_minus;
}

static int16_t x_lightgun_aiming_state( x11_input_t *x11, unsigned idx, unsigned id )
{
   const int edge_detect = 32700;
   struct video_viewport vp;
   bool inside                 = false;
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;

   vp.x                        = 0;
   vp.y                        = 0;
   vp.width                    = 0;
   vp.height                   = 0;
   vp.full_width               = 0;
   vp.full_height              = 0;

   if (!(video_driver_translate_coord_viewport_wrap(&vp, x11->mouse_x, x11->mouse_y,
         &res_x, &res_y, &res_screen_x, &res_screen_y)))
      return 0;

   inside = (res_x >= -edge_detect) && (res_y >= -edge_detect) && (res_x <= edge_detect) && (res_y <= edge_detect);

   switch ( id )
   {
   case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
      return inside ? res_x : 0;
   case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
      return inside ? res_y : 0;
   case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
      return !inside;
   default:
      break;
   }

   return 0;
}

static int16_t x_mouse_state(x11_input_t *x11, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return x11->mouse_x - x11->mouse_last_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return x11->mouse_y - x11->mouse_last_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return x11->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return x11->mouse_r;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return x_mouse_state_wheel(id);
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return x11->mouse_m;
   }

   return 0;
}

static int16_t x_mouse_state_screen(x11_input_t *x11, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return x11->mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return x11->mouse_y;
      default:
         break;
   }

   return x_mouse_state(x11, id);
}

static int16_t x_pointer_state(x11_input_t *x11,
      unsigned idx, unsigned id, bool screen)
{
   struct video_viewport vp;
   bool inside                 = false;
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;

   vp.x                        = 0;
   vp.y                        = 0;
   vp.width                    = 0;
   vp.height                   = 0;
   vp.full_width               = 0;
   vp.full_height              = 0;

   if (!(video_driver_translate_coord_viewport_wrap(&vp, x11->mouse_x, x11->mouse_y,
         &res_x, &res_y, &res_screen_x, &res_screen_y)))
      return 0;

   if (screen)
   {
      res_x = res_screen_x;
      res_y = res_screen_y;
   }

   inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

   if (!inside)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return res_x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return res_y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return x11->mouse_l;
   }

   return 0;
}

static int16_t x_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds, unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   int16_t ret                = 0;
   x11_input_t *x11           = (x11_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id < RARCH_BIND_LIST_END)
            return x_is_pressed(x11, joypad_info, binds[port], port, id);
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && x_keyboard_pressed(x11, id);
      case RETRO_DEVICE_ANALOG:
         ret = x_pressed_analog(x11, binds[port], idx, id);
         if (!ret && binds[port])
            ret = input_joypad_analog(x11->joypad, joypad_info,
                  port, idx,
                  id, binds[port]);
         return ret;
      case RETRO_DEVICE_MOUSE:
         return x_mouse_state(x11, id);
      case RARCH_DEVICE_MOUSE_SCREEN:
         return x_mouse_state_screen(x11, id);

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         if (idx == 0)
            return x_pointer_state(x11, idx, id,
                  device == RARCH_DEVICE_POINTER_SCREEN);
         break;
      case RETRO_DEVICE_LIGHTGUN:
         switch ( id )
         {
            /*aiming*/
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
               return x_lightgun_aiming_state( x11, idx, id );

            /*buttons*/
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_TRIGGER);
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_RELOAD);
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_AUX_A);
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_AUX_B);
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_AUX_C);
            case RETRO_DEVICE_ID_LIGHTGUN_START:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_START);
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_SELECT);
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_UP);
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_DOWN);
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_LEFT);
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_DPAD_RIGHT);

            /*deprecated*/
            case RETRO_DEVICE_ID_LIGHTGUN_X:
               return x11->mouse_x - x11->mouse_last_x;
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               return x11->mouse_y - x11->mouse_last_y;
            case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
               return x_is_pressed(x11, joypad_info, binds[port], port, RARCH_LIGHTGUN_START);

         }
         break;
   }

   return 0;
}

static void x_input_free(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;

   if (!x11)
      return;

   if (x11->joypad)
      x11->joypad->destroy();

   free(x11);
}

extern bool g_x11_entered;

static void x_input_poll_mouse(x11_input_t *x11)
{
   unsigned mask;
   int root_x, root_y, win_x, win_y;
   Window root_win, child_win;

   x11->mouse_last_x = x11->mouse_x;
   x11->mouse_last_y = x11->mouse_y;

   XQueryPointer(x11->display,
            x11->win,
            &root_win, &child_win,
            &root_x, &root_y,
            &win_x, &win_y,
            &mask);

   if (g_x11_entered)
   {
      x11->mouse_x  = win_x;
      x11->mouse_y  = win_y;
      x11->mouse_l  = mask & Button1Mask;
      x11->mouse_m  = mask & Button2Mask;
      x11->mouse_r  = mask & Button3Mask;

      /* Somewhat hacky, but seem to do the job. */
      if (x11->grab_mouse && video_driver_cb_has_focus())
      {
         int mid_w, mid_h;
         struct video_viewport vp;

         vp.x                        = 0;
         vp.y                        = 0;
         vp.width                    = 0;
         vp.height                   = 0;
         vp.full_width               = 0;
         vp.full_height              = 0;

         video_driver_get_viewport_info(&vp);

         mid_w = vp.full_width >> 1;
         mid_h = vp.full_height >> 1;

         if (x11->mouse_x != mid_w || x11->mouse_y != mid_h)
         {
            XWarpPointer(x11->display, None,
                  x11->win, 0, 0, 0, 0,
                  mid_w, mid_h);
            XSync(x11->display, False);
         }
         x11->mouse_last_x = mid_w;
         x11->mouse_last_y = mid_h;
      }
   }
}


static void x_input_poll(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;

   if (video_driver_cb_has_focus())
      XQueryKeymap(x11->display, x11->state);
   else
      memset(x11->state, 0, sizeof(x11->state));

   x_input_poll_mouse(x11);

   if (x11->joypad)
      x11->joypad->poll();
}

static void x_grab_mouse(void *data, bool state)
{
   x11_input_t *x11 = (x11_input_t*)data;
   if (x11)
      x11->grab_mouse = state;
}

static bool x_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   x11_input_t *x11 = (x11_input_t*)data;
   if (!x11)
      return false;
   return input_joypad_set_rumble(x11->joypad, port, effect, strength);
}

static const input_device_driver_t *x_get_joypad_driver(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;

   if (!x11)
      return NULL;
   return x11->joypad;
}

static uint64_t x_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_LIGHTGUN);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static bool x_keyboard_mapping_is_blocked(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;
   if (!x11)
      return false;
   return x11->blocked;
}

static void x_keyboard_mapping_set_block(void *data, bool value)
{
   x11_input_t *x11 = (x11_input_t*)data;
   if (!x11)
      return;
   x11->blocked = value;
}

input_driver_t input_x = {
   x_input_init,
   x_input_poll,
   x_input_state,
   x_input_free,
   NULL,
   NULL,
   x_input_get_capabilities,
   "x",
   x_grab_mouse,
   NULL,
   x_set_rumble,
   x_get_joypad_driver,
   NULL,
   x_keyboard_mapping_is_blocked,
   x_keyboard_mapping_set_block,
};

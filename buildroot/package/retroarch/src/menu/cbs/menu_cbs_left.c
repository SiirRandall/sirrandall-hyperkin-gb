/*  RetroArch - A frontend for libretro.
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

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_content.h"
#include "../menu_driver.h"
#include "../menu_entries.h"
#include "../menu_cbs.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../menu_shader.h"

#include "../../configuration.h"
#include "../../core.h"
#include "../../core_info.h"
#include "../../managers/cheat_manager.h"
#include "../../file_path_special.h"
#include "../../driver.h"
#include "../../audio/audio_driver.h"
#include "../../gfx/video_driver.h"
#include "../../retroarch.h"
#include "../../network/netplay/netplay.h"

#ifndef BIND_ACTION_LEFT
#define BIND_ACTION_LEFT(cbs, name) \
   do { \
      cbs->action_left = name; \
      cbs->action_left_ident = #name; \
   } while(0)
#endif

extern struct key_desc key_descriptors[RARCH_MAX_KEYS];

int setting_action_left_analog_dpad_mode(void *data, bool wraparound)
{
   unsigned port = 0;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   configuration_set_uint(settings, settings->uints.input_analog_dpad_mode[port],
      (settings->uints.input_analog_dpad_mode
       [port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST);

   return 0;
}

int setting_action_left_libretro_device_type(
      void *data, bool wraparound)
{
   retro_ctx_controller_info_t pad;
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_setting_t *setting    = (rarch_setting_t*)data;
   rarch_system_info_t *system = NULL;

   if (!setting)
      return -1;

   port = setting->index_offset;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   system           = runloop_get_system_info();

   if (system)
   {
      /* Only push RETRO_DEVICE_ANALOG as default if we use an
       * older core which doesn't use SET_CONTROLLER_INFO. */
      if (!system->ports.size)
         devices[types++] = RETRO_DEVICE_ANALOG;

      if (port < system->ports.size)
         desc = &system->ports.data[port];
   }

   if (desc)
   {
      for (i = 0; i < desc->num_types; i++)
      {
         unsigned id = desc->types[i].id;
         if (types < ARRAY_SIZE(devices) &&
               id != RETRO_DEVICE_NONE &&
               id != RETRO_DEVICE_JOYPAD)
            devices[types++] = id;
      }
   }

   current_device = input_config_get_device(port);
   current_idx    = 0;
   for (i = 0; i < types; i++)
   {
      if (current_device != devices[i])
         continue;

      current_idx = i;
      break;
   }

   current_device = devices
      [(current_idx + types - 1) % types];

   input_config_set_device(port, current_device);

   pad.port   = port;
   pad.device = current_device;

   core_set_controller_port_device(&pad);

   return 0;
}

int setting_action_left_bind_device(void *data, bool wraparound)
{
   unsigned               *p = NULL;
   unsigned index_offset     = 0;
   unsigned max_devices      = input_config_get_device_count();
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting || max_devices == 0)
      return -1;

   index_offset = setting->index_offset;

   p = &settings->uints.input_joypad_map[index_offset];

   if ((*p) >= max_devices)
      *p = max_devices - 1;
   else if ((*p) > 0)
      (*p)--;

   return 0;
}

int setting_action_left_mouse_index(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   settings_t *settings     = config_get_ptr();

   if (!setting)
      return -1;

   if (settings->uints.input_mouse_index[setting->index_offset])
   {
      --settings->uints.input_mouse_index[setting->index_offset];
      settings->modified = true;
   }

   return 0;
}

int setting_uint_action_left_custom_viewport_width(
      void *data, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->width <= 1)
      custom->width = 1;
   else if (settings->bools.video_scale_integer)
   {
      if (custom->width > geom->base_width)
         custom->width -= geom->base_width;
   }
   else
      custom->width -= 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

int setting_uint_action_left_custom_viewport_height(
      void *data, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->height <= 1)
      custom->height = 1;
   else if (settings->bools.video_scale_integer)
   {
      if (custom->height > geom->base_height)
         custom->height -= geom->base_height;
   }
   else
      custom->height -= 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

int setting_string_action_left_audio_device(
      void *data, bool wraparound)
{
#if !defined(RARCH_CONSOLE)
   int audio_device_index;
   struct string_list *ptr  = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!audio_driver_get_devices_list((void**)&ptr))
      return -1;

   if (!ptr)
      return -1;

   /* Get index in the string list */
   audio_device_index = string_list_find_elem(
         ptr, setting->value.target.string) - 1;
   audio_device_index--;

   /* Reset index if needed */
   if (audio_device_index < 0)
      audio_device_index = (int)(ptr->size - 1);

   strlcpy(setting->value.target.string, ptr->elems[audio_device_index].data, setting->size);
#endif

   return 0;
}

int setting_string_action_left_driver(void *data,
      bool wraparound)
{
   driver_ctx_info_t drv;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   drv.label = setting->name;
   drv.s     = setting->value.target.string;
   drv.len   = setting->size;

   if (!driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv))
   {
      settings_t *settings = config_get_ptr();

      if (settings && settings->bools.menu_navigation_wraparound_enable)
      {
         drv.label = setting->name;
         drv.s     = setting->value.target.string;
         drv.len   = setting->size;
         driver_ctl(RARCH_DRIVER_CTL_FIND_LAST, &drv);
      }
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

static int generic_shader_action_parameter_left(
      struct video_shader_parameter *param,
      unsigned type, const char *label, bool wraparound)
{
   param->current -= param->step;
   param->current  = MIN(MAX(param->minimum, param->current),
         param->maximum);
   return 0;
}

static int shader_action_parameter_left(unsigned type, const char *label, bool wraparound)
{
   video_shader_ctx_t shader_info;
   struct video_shader *shader          = menu_shader_get();
   struct video_shader_parameter *param_menu = NULL;
   struct video_shader_parameter *param_prev = NULL;

   int ret = 0;

   video_shader_driver_get_current_shader(&shader_info);

   param_prev = &shader_info.data->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
   param_menu = shader ? &shader->parameters[type - 
      MENU_SETTINGS_SHADER_PARAMETER_0] : NULL;

   if (!param_prev || !param_menu)
      return menu_cbs_exit();
   ret = generic_shader_action_parameter_left(param_prev, type, label, wraparound);

   param_menu->current = param_prev->current;

   return ret;
}

static int audio_mixer_stream_volume_left(unsigned type, const char *label,
      bool wraparound)
{
   unsigned         offset      = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN);
   float orig_volume            = 0.0f;
   
   if (offset >= AUDIO_MIXER_MAX_STREAMS)
      return 0;

   orig_volume                  = audio_driver_mixer_get_stream_volume(offset);
   orig_volume                  = orig_volume - 1.00f;

   audio_driver_mixer_set_stream_volume(offset, orig_volume);

   return 0;
}

static int action_left_cheat(unsigned type, const char *label,
      bool wraparound)
{
   size_t idx             = type - MENU_SETTINGS_CHEAT_BEGIN;
   return generic_action_cheat_toggle(idx, type, label,
         wraparound);
}

static int action_left_input_desc(unsigned type, const char *label,
   bool wraparound)
{
   rarch_system_info_t *system           = runloop_get_system_info();
   settings_t *settings                  = config_get_ptr();
   unsigned btn_idx, user_idx, remap_idx;

   if (!settings || !system)
      return 0;

   user_idx = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) / (RARCH_FIRST_CUSTOM_BIND + 8);
   btn_idx  = (type - MENU_SETTINGS_INPUT_DESC_BEGIN) - (RARCH_FIRST_CUSTOM_BIND + 8) * user_idx;

   if (settings->uints.input_remap_ids[user_idx][btn_idx] == RARCH_UNMAPPED)
      settings->uints.input_remap_ids[user_idx][btn_idx] = RARCH_CUSTOM_BIND_LIST_END - 1;

   if (settings->uints.input_remap_ids[user_idx][btn_idx] > 0)
      settings->uints.input_remap_ids[user_idx][btn_idx]--;
   else if (settings->uints.input_remap_ids[user_idx][btn_idx] == 0)
      settings->uints.input_remap_ids[user_idx][btn_idx] = RARCH_UNMAPPED;
   else
      settings->uints.input_remap_ids[user_idx][btn_idx] = RARCH_CUSTOM_BIND_LIST_END - 1;

   remap_idx = settings->uints.input_remap_ids[user_idx][btn_idx];

   /* skip the not used buttons (unless they are at the end by calling the right desc function recursively
      also skip all the axes until analog remapping is implemented */
   if ((string_is_empty(system->input_desc_btn[user_idx][remap_idx]) && remap_idx < RARCH_CUSTOM_BIND_LIST_END) /*|| 
       (remap_idx >= RARCH_FIRST_CUSTOM_BIND && remap_idx < RARCH_CUSTOM_BIND_LIST_END)*/)
      action_left_input_desc(type, label, wraparound);

   return 0;
}

static int action_left_input_desc_kbd(unsigned type, const char *label,
   bool wraparound)
{
   unsigned remap_id;
   unsigned key_id, user_idx, btn_idx;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return 0;

   user_idx = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) / RARCH_FIRST_CUSTOM_BIND;
   btn_idx  = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) - RARCH_FIRST_CUSTOM_BIND * user_idx;

   remap_id =
      settings->uints.input_keymapper_ids[user_idx][btn_idx];

   for (key_id = 0; key_id < RARCH_MAX_KEYS - 1; key_id++)
   {
      if(remap_id == key_descriptors[key_id].key)
         break;
   }

   if (key_id > 0)
      key_id--;
   else
      key_id = (RARCH_MAX_KEYS - 1) + MENU_SETTINGS_INPUT_DESC_KBD_BEGIN;

   settings->uints.input_keymapper_ids[user_idx][btn_idx] = key_descriptors[key_id].key;

   return 0;
}

static int action_left_scroll(unsigned type, const char *label,
      bool wraparound)
{
   size_t scroll_accel   = 0;
   unsigned scroll_speed = 0, fast_scroll_speed = 0;
   size_t selection      = menu_navigation_get_selection();

   if (!menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL, &scroll_accel))
      return false;

   scroll_speed          = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);
   fast_scroll_speed     = 4 + 4 * scroll_speed;

   if (selection > fast_scroll_speed)
   {
      size_t idx  = selection - fast_scroll_speed;
      menu_navigation_set_selection(idx);
      menu_driver_navigation_set(true);
   }
   else
   {
      bool pending_push = false;
      menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
   }

   return 0;
}

static int action_left_mainmenu(unsigned type, const char *label,
      bool wraparound)
{
   menu_ctx_list_t list_info;
   unsigned        push_list = 0;
   menu_handle_t       *menu  = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SELECTION, &list_info);

   list_info.type = MENU_LIST_PLAIN;

   menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SIZE, &list_info);

   if (list_info.size == 1)
   {
      settings_t       *settings = config_get_ptr();

      if ((list_info.selection != 0)
         || settings->bools.menu_navigation_wraparound_enable)
         push_list = 1;
   }
   else
      push_list = 2;


   switch (push_list)
   {
      case 1:
         {
            menu_ctx_list_t list_info;
            file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
            file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
            size_t selection           = menu_navigation_get_selection();
            menu_file_list_cbs_t *cbs  = selection_buf ?
               (menu_file_list_cbs_t*)file_list_get_actiondata_at_offset(selection_buf,
                     selection) : NULL;

            list_info.type             = MENU_LIST_HORIZONTAL;
            list_info.action           = MENU_ACTION_LEFT;

            menu_driver_ctl(RARCH_MENU_CTL_LIST_CACHE, &list_info);

            if (cbs && cbs->action_content_list_switch)
               return cbs->action_content_list_switch(
                     selection_buf, menu_stack, "", "", 0);
         }
         break;
      case 2:
         action_left_scroll(0, "", false);
         break;
      case 0:
      default:
         break;
   }

   return 0;
}

static int action_left_shader_scale_pass(unsigned type, const char *label,
      bool wraparound)
{
   unsigned current_scale, delta;
   unsigned pass                         = type -
      MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[pass] : NULL;

   if (!shader_pass)
      return menu_cbs_exit();

   current_scale            = shader_pass->fbo.scale_x;
   delta                    = 5;
   current_scale            = (current_scale + delta) % 6;

   shader_pass->fbo.valid   = current_scale;
   shader_pass->fbo.scale_x = current_scale;
   shader_pass->fbo.scale_y = current_scale;
   return 0;
}

static int action_left_shader_filter_pass(unsigned type, const char *label,
      bool wraparound)
{
   unsigned delta = 2;
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ? &shader->pass[pass] : NULL;

   if (!shader_pass)
      return menu_cbs_exit();

   shader_pass->filter = ((shader_pass->filter + delta) % 3);
   return 0;
}

static int action_left_shader_filter_default(unsigned type, const char *label,
      bool wraparound)
{
   rarch_setting_t *setting = menu_setting_find_enum(
         MENU_ENUM_LABEL_VIDEO_SMOOTH);
   if (!setting)
      return menu_cbs_exit();
   return menu_action_handle_setting(setting,
         setting_get_type(setting), MENU_ACTION_LEFT, wraparound);
}

static int action_left_cheat_num_passes(unsigned type, const char *label,
      bool wraparound)
{
   bool refresh      = false;
   unsigned new_size = 0;

   if (cheat_manager_get_size())
      new_size = cheat_manager_get_size() - 1;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   return 0;
}

static int action_left_shader_num_passes(unsigned type, const char *label,
      bool wraparound)
{
   bool refresh      = false;
   struct video_shader *shader = menu_shader_get();
   unsigned pass_count         = shader ? shader->passes : 0;

   if (!shader)
      return menu_cbs_exit();

   if (pass_count > 0)
      menu_shader_manager_decrement_amount_passes();

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   video_shader_resolve_parameters(NULL, shader);

   return 0;
}

static int action_left_netplay_mitm_server(unsigned type, const char *label,
      bool wraparound)
{
   settings_t *settings = config_get_ptr();
   int i;
   bool found = false;
   int list_len = ARRAY_SIZE(netplay_mitm_server_list);

   for (i = 0; i < list_len; i++)
   {
      /* find the currently selected server in the list */
      if (string_is_equal(settings->arrays.netplay_mitm_server, netplay_mitm_server_list[i].name))
      {
         /* move to the previous one in the list, wrap around if necessary */
         if (i - 1 >= 0)
         {
            found = true;
            strlcpy(settings->arrays.netplay_mitm_server, netplay_mitm_server_list[i - 1].name, sizeof(settings->arrays.netplay_mitm_server));
            break;
         }
         else if (wraparound)
         {
            found = true;
            strlcpy(settings->arrays.netplay_mitm_server, netplay_mitm_server_list[list_len - 1].name, sizeof(settings->arrays.netplay_mitm_server));
            break;
         }
      }
   }

   if (!found)
   {
      /* current entry was invalid, go back to the end */
      strlcpy(settings->arrays.netplay_mitm_server, netplay_mitm_server_list[list_len - 1].name, sizeof(settings->arrays.netplay_mitm_server));
   }

   return 0;
}

static int action_left_shader_watch_for_changes(unsigned type, const char *label,
      bool wraparound)
{
   settings_t *settings = config_get_ptr();
   settings->bools.video_shader_watch_files = !settings->bools.video_shader_watch_files;
   return 0;
}

static int action_left_video_resolution(unsigned type, const char *label,
      bool wraparound)
{
   video_driver_get_prev_video_out();
   return 0;
}

static int playlist_association_left(unsigned type, const char *label,
      bool wraparound)
{
   unsigned i;
   char core_path[PATH_MAX_LENGTH];
   char new_playlist_cores[PATH_MAX_LENGTH];
   int next, found, current         = 0;
   core_info_t *info                = NULL;
   struct string_list *stnames      = NULL;
   struct string_list *stcores      = NULL;
   settings_t *settings             = config_get_ptr();
   const char *path                 = path_basename(label);
   core_info_list_t           *list = NULL;

   core_info_get_list(&list);

   if (!list)
      return menu_cbs_exit();

   core_path[0] = new_playlist_cores[0] = '\0';

   stnames = string_split(settings->arrays.playlist_names, ";");
   stcores = string_split(settings->arrays.playlist_cores, ";");

   if (!menu_content_playlist_find_associated_core(path,
            core_path, sizeof(core_path)))
         strlcpy(core_path,
               file_path_str(FILE_PATH_DETECT),
               sizeof(core_path));

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = core_info_get(list, i);
      if (string_is_equal(info->path, core_path))
         current = i;
   }

   next = current - 1;
   if (next < 0)
   {
      if (wraparound)
         next = (int)(list->count-1);
      else
         next = 0;
   }

   info  = core_info_get(list, next);
   found = string_list_find_elem(stnames, path);
   if (found && info)
      string_list_set(stcores, found-1, info->path);

   string_list_join_concat(new_playlist_cores,
         sizeof(new_playlist_cores), stcores, ";");

   strlcpy(settings->arrays.playlist_cores,
         new_playlist_cores, sizeof(settings->arrays.playlist_cores));

   string_list_free(stnames);
   string_list_free(stcores);

   return 0;
}

static int core_setting_left(unsigned type, const char *label,
      bool wraparound)
{
   unsigned idx     = type - MENU_SETTINGS_CORE_OPTION_START;

   rarch_ctl(RARCH_CTL_CORE_OPTION_PREV, &idx);

   return 0;
}

static int disk_options_disk_idx_left(unsigned type, const char *label,
      bool wraparound)
{
   command_event(CMD_EVENT_DISK_PREV, NULL);
   return 0;
}

static int bind_left_generic(unsigned type, const char *label,
      bool wraparound)
{
   return menu_setting_set(type, label, MENU_ACTION_LEFT, wraparound);
}

static int menu_cbs_init_bind_left_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash, const char *menu_label)
{

   if (cbs->setting)
   {
      const char *parent_group   = cbs->setting->parent_group;

      if (string_is_equal(parent_group, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))
               && (setting_get_type(cbs->setting) == ST_GROUP))
      {
         BIND_ACTION_LEFT(cbs, action_left_mainmenu);
         return 0;
      }
   }

   if (strstr(label, "input_player") && strstr(label, "_joypad_index"))
   {
      unsigned i;
      for (i = 0; i < MAX_USERS; i++)
      {
         uint32_t label_setting_hash;
         char label_setting[128];

         label_setting[0] = '\0';

         snprintf(label_setting, sizeof(label_setting), "input_player%d_joypad_index", i + 1);
         label_setting_hash = msg_hash_calculate(label_setting);

         if (label_hash != label_setting_hash)
            continue;

         BIND_ACTION_LEFT(cbs, bind_left_generic);
         return 0;
      }
   }

   if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
   {
      BIND_ACTION_LEFT(cbs, action_left_mainmenu);
      return 0;
   }

   if (strstr(label, "rdb_entry"))
   {
      BIND_ACTION_LEFT(cbs, action_left_scroll);
   }
   else
   {
      if (cbs->enum_idx != MSG_UNKNOWN)
      {
         switch (cbs->enum_idx)
         {
            case MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM:
               BIND_ACTION_LEFT(cbs, action_left_mainmenu);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
               BIND_ACTION_LEFT(cbs, action_left_shader_scale_pass);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
               BIND_ACTION_LEFT(cbs, action_left_shader_filter_pass);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
               BIND_ACTION_LEFT(cbs, action_left_shader_filter_default);
               break;
            case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
               BIND_ACTION_LEFT(cbs, action_left_netplay_mitm_server);
               break;
            case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
               BIND_ACTION_LEFT(cbs, action_left_shader_watch_for_changes);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
               BIND_ACTION_LEFT(cbs, action_left_shader_num_passes);
               break;
            case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
               BIND_ACTION_LEFT(cbs, action_left_cheat_num_passes);
               break;
            case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
               BIND_ACTION_LEFT(cbs, action_left_video_resolution);
               break;
            case MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE:
            case MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE:
               BIND_ACTION_LEFT(cbs, action_left_scroll);
               break;
            case MENU_ENUM_LABEL_NO_ITEMS:
            case MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE:
               if (
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))       ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU))
                  )
               {
                  BIND_ACTION_LEFT(cbs, action_left_mainmenu);
               }
               else
               {
                  BIND_ACTION_LEFT(cbs, action_left_scroll);
               }
               break;
            case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
            case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
               if (
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB))
                  )
               {
                  BIND_ACTION_LEFT(cbs, action_left_mainmenu);
                  break;
               }
            default:
               return -1;
         }

      }
      else
      {
         return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_left_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type, const char *menu_label)
{
   if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_LEFT(cbs, action_left_cheat);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END)
   {
      BIND_ACTION_LEFT(cbs, audio_mixer_stream_volume_left);
   }
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_LEFT(cbs, shader_action_parameter_left);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_LEFT(cbs, shader_action_parameter_left);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_LEFT(cbs, action_left_input_desc);
   }
   else if (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
      && type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
   {
      BIND_ACTION_LEFT(cbs, action_left_input_desc_kbd);
   }
   else if ((type >= MENU_SETTINGS_PLAYLIST_ASSOCIATION_START))
   {
      BIND_ACTION_LEFT(cbs, playlist_association_left);
   }
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
   {
      BIND_ACTION_LEFT(cbs, core_setting_left);
   }
   else
   {
      switch (type)
      {
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            BIND_ACTION_LEFT(cbs, disk_options_disk_idx_left);
            break;
         case FILE_TYPE_PLAIN:
         case FILE_TYPE_DIRECTORY:
         case FILE_TYPE_CARCHIVE:
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_CORE:
         case FILE_TYPE_RDB:
         case FILE_TYPE_RDB_ENTRY:
         case FILE_TYPE_RPL_ENTRY:
         case FILE_TYPE_CURSOR:
         case FILE_TYPE_SHADER:
         case FILE_TYPE_SHADER_PRESET:
         case FILE_TYPE_IMAGE:
         case FILE_TYPE_OVERLAY:
         case FILE_TYPE_VIDEOFILTER:
         case FILE_TYPE_AUDIOFILTER:
         case FILE_TYPE_CONFIG:
         case FILE_TYPE_USE_DIRECTORY:
         case FILE_TYPE_PLAYLIST_ENTRY:
         case MENU_INFO_MESSAGE:
         case FILE_TYPE_DOWNLOAD_CORE:
         case FILE_TYPE_CHEAT:
         case FILE_TYPE_REMAP:
         case FILE_TYPE_MOVIE:
         case FILE_TYPE_MUSIC:
         case FILE_TYPE_IMAGEVIEWER:
         case FILE_TYPE_PLAYLIST_COLLECTION:
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
         case FILE_TYPE_DOWNLOAD_URL:
         case FILE_TYPE_SCAN_DIRECTORY:
         case FILE_TYPE_FONT:
         case MENU_SETTING_GROUP:
         case MENU_SETTINGS_CORE_INFO_NONE:
            if (  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB))
                  )
            {
               BIND_ACTION_LEFT(cbs, action_left_mainmenu);
               break;
            }
            BIND_ACTION_LEFT(cbs, action_left_scroll);
            break;
         case MENU_SETTING_ACTION:
         case FILE_TYPE_CONTENTLIST_ENTRY:
            BIND_ACTION_LEFT(cbs, action_left_mainmenu);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_left(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *menu_label,
      uint32_t label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_LEFT(cbs, bind_left_generic);

   if (type == MENU_SETTING_NO_ITEM)
   {
      if (  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB))
         )
      {
            BIND_ACTION_LEFT(cbs, action_left_mainmenu);
            return 0;
      }
   }

   if (menu_cbs_init_bind_left_compare_label(cbs, label, label_hash, menu_label) == 0)
      return 0;

   if (menu_cbs_init_bind_left_compare_type(cbs, type, menu_label) == 0)
      return 0;

   return -1;
}

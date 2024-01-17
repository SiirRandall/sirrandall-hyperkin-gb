/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
 *  Copyright (C) 2015-2017 - Andrés Suárez
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
#include <retro_assert.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <lists/string_list.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"
#include "../../config.def.keybinds.h"
#include "../../wifi/wifi_driver.h"
#include "../../driver.h"

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_setting.h"
#include "../menu_shader.h"
#include "../widgets/menu_dialog.h"
#include "../widgets/menu_entry.h"
#include "../widgets/menu_filebrowser.h"
#include "../widgets/menu_input_dialog.h"
#include "../widgets/menu_input_bind_dialog.h"
#include "../menu_input.h"
#include "../menu_networking.h"
#include "../menu_content.h"
#include "../menu_shader.h"

#include "../../audio/audio_driver.h"
#include "../../core.h"
#include "../../configuration.h"
#include "../../core_info.h"
#include "../../frontend/frontend_driver.h"
#include "../../defaults.h"
#include "../../managers/cheat_manager.h"
#include "../../tasks/tasks_internal.h"
#include "../../input/input_remapping.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../lakka.h"
#include "../../wifi/wifi_driver.h"

#include <net/net_http.h>

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#include "../../network/netplay/netplay_discovery.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

enum
{
   ACTION_OK_LOAD_PRESET = 0,
   ACTION_OK_LOAD_SHADER_PASS,
   ACTION_OK_LOAD_RECORD_CONFIGFILE,
   ACTION_OK_LOAD_REMAPPING_FILE,
   ACTION_OK_LOAD_CHEAT_FILE,
   ACTION_OK_APPEND_DISK_IMAGE,
   ACTION_OK_SUBSYSTEM_ADD,
   ACTION_OK_LOAD_CONFIG_FILE,
   ACTION_OK_LOAD_CORE,
   ACTION_OK_LOAD_WALLPAPER,
   ACTION_OK_SET_PATH,
   ACTION_OK_SET_PATH_AUDIO_FILTER,
   ACTION_OK_SET_PATH_VIDEO_FILTER,
   ACTION_OK_SET_PATH_OVERLAY,
   ACTION_OK_SET_DIRECTORY,
   ACTION_OK_SHOW_WIMP,
   ACTION_OK_LOAD_CHEAT_FILE_APPEND
};

enum
{
   ACTION_OK_REMAP_FILE_SAVE_CORE = 0,
   ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR,
   ACTION_OK_REMAP_FILE_SAVE_GAME,
   ACTION_OK_REMAP_FILE_REMOVE_CORE,
   ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR,
   ACTION_OK_REMAP_FILE_REMOVE_GAME
};

#ifndef BIND_ACTION_OK
#define BIND_ACTION_OK(cbs, name) \
   do { \
      cbs->action_ok = name; \
      cbs->action_ok_ident = #name; \
   } while(0)
#endif

#ifdef HAVE_NETWORKING
#ifdef HAVE_LAKKA
static char *lakka_get_project(void)
{
   size_t len;
   static char lakka_project[128];
   FILE *command_file = popen("cat /etc/release | cut -d - -f 1", "r");

   fgets(lakka_project, sizeof(lakka_project), command_file);
   len = strlen(lakka_project);

   if (len > 0 && lakka_project[len-1] == '\n')
      lakka_project[--len] = '\0';

   pclose(command_file);
   return lakka_project;
}
#endif
#endif

#define action_ok_dl_lbl(a, b) \
   info.directory_ptr = idx; \
   info.type          = type; \
   info_path          = path; \
   info_label         = msg_hash_to_str(a); \
   info.enum_idx      = a; \
   dl_type            = b;

int setting_action_ok_video_refresh_rate_auto(void *data, bool wraparound)
{
   double video_refresh_rate = 0.0;
   double deviation          = 0.0;
   unsigned sample_points    = 0;
   rarch_setting_t *setting  = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (video_monitor_fps_statistics(&video_refresh_rate,
            &deviation, &sample_points))
   {
      float video_refresh_rate_float = (float)video_refresh_rate;
      driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &video_refresh_rate_float);
      /* Incase refresh rate update forced non-block video. */
      command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);
   }

   if (setting_generic_action_ok_default(setting, wraparound) != 0)
      return -1;

   return 0;
}

int setting_action_ok_video_refresh_rate_polled(void *data, bool wraparound)
{
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   float refresh_rate = 0.0;

   if (!setting)
     return -1;

   if ((refresh_rate = video_driver_get_refresh_rate()) == 0.0)
      return -1;

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);
   /* Incase refresh rate update forced non-block video. */
   command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);

   if (setting_generic_action_ok_default(setting, wraparound) != 0)
      return -1;

   return 0;
}

int setting_action_ok_bind_all(void *data, bool wraparound)
{
   (void)wraparound;
   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_ALL, data))
      return -1;
   return 0;
}

int setting_action_ok_bind_all_save_autoconfig(void *data,
      bool wraparound)
{
   unsigned index_offset;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   const char *name          = NULL;

   (void)wraparound;

   if (!setting)
      return -1;

   index_offset = setting->index_offset;
   name         = input_config_get_device_name(index_offset);

   if(!string_is_empty(name) && config_save_autoconf_profile(name, index_offset))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY), 1, 100, true);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_ERROR_SAVING), 1, 100, true);


   return 0;
}

int setting_action_ok_bind_defaults(void *data, bool wraparound)
{
   unsigned i;
   menu_input_ctx_bind_limits_t lim;
   struct retro_keybind *target          = NULL;
   const struct retro_keybind *def_binds = NULL;
   rarch_setting_t *setting              = (rarch_setting_t*)data;

   (void)wraparound;

   if (!setting)
      return -1;

   target    =  &input_config_binds[setting->index_offset][0];
   def_binds =  (setting->index_offset) ?
      retro_keybinds_rest : retro_keybinds_1;

   lim.min   = MENU_SETTINGS_BIND_BEGIN;
   lim.max   = MENU_SETTINGS_BIND_LAST;

   menu_input_key_bind_set_min_max(&lim);

   for (i = MENU_SETTINGS_BIND_BEGIN;
         i <= MENU_SETTINGS_BIND_LAST; i++, target++)
   {
      target->key     = def_binds[i - MENU_SETTINGS_BIND_BEGIN].key;
      target->joykey  = NO_BTN;
      target->joyaxis = AXIS_NONE;
      target->mbutton = NO_BTN;
   }

   return 0;
}

static enum msg_hash_enums action_ok_dl_to_enum(unsigned lbl)
{
   switch (lbl)
   {
      case ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MIXER_STREAM_SETTINGS_LIST;
      case ACTION_OK_DL_ACCOUNTS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST;
      case ACTION_OK_DL_INPUT_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST;
      case ACTION_OK_DL_LATENCY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LATENCY_SETTINGS_LIST;
      case ACTION_OK_DL_DRIVER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST;
      case ACTION_OK_DL_CORE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST;
      case ACTION_OK_DL_VIDEO_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST;
      case ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST;
      case ACTION_OK_DL_SAVING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST;
      case ACTION_OK_DL_LOGGING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST;
      case ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST;
      case ACTION_OK_DL_REWIND_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST;
      case ACTION_OK_DL_CHEAT_DETAILS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CHEAT_DETAILS_SETTINGS_LIST;
      case ACTION_OK_DL_CHEAT_SEARCH_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CHEAT_SEARCH_SETTINGS_LIST;
      case ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST;
      case ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST;
      case ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST;
      case ACTION_OK_DL_MENU_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST;
      case ACTION_OK_DL_MENU_VIEWS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_VIEWS_SETTINGS_LIST;
      case ACTION_OK_DL_QUICK_MENU_VIEWS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_VIEWS_SETTINGS_LIST;
      case ACTION_OK_DL_QUICK_MENU_OVERRIDE_OPTIONS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_OVERRIDE_OPTIONS;
      case ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST;
      case ACTION_OK_DL_POWER_MANAGEMENT_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_POWER_MANAGEMENT_SETTINGS_LIST;
      case ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST;
      case ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST;
      case ACTION_OK_DL_UPDATER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST;
      case ACTION_OK_DL_NETWORK_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST;
      case ACTION_OK_DL_WIFI_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST;
      case ACTION_OK_DL_NETPLAY:
         return MENU_ENUM_LABEL_DEFERRED_NETPLAY;
      case ACTION_OK_DL_NETPLAY_LAN_SCAN_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETPLAY_LAN_SCAN_SETTINGS_LIST;
      case ACTION_OK_DL_LAKKA_SERVICES_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LAKKA_SERVICES_LIST;
      case ACTION_OK_DL_USER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_USER_SETTINGS_LIST;
      case ACTION_OK_DL_DIRECTORY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST;
      case ACTION_OK_DL_PRIVACY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_PRIVACY_SETTINGS_LIST;
      case ACTION_OK_DL_MIDI_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MIDI_SETTINGS_LIST;
      case ACTION_OK_DL_AUDIO_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST;
      case ACTION_OK_DL_AUDIO_MIXER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_AUDIO_MIXER_SETTINGS_LIST;
      case ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST;
      case ACTION_OK_DL_RECORDING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST;
      case ACTION_OK_DL_PLAYLIST_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST;
      case ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST;
      case ACTION_OK_DL_PLAYLIST_COLLECTION:
         return MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST;
      case ACTION_OK_DL_FAVORITES_LIST:
         return MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST;
      case ACTION_OK_DL_BROWSE_URL_LIST:
         return MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_LIST;
      case ACTION_OK_DL_MUSIC_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST;
      case ACTION_OK_DL_IMAGES_LIST:
         return MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST;
      default:
         break;
   }

   return MSG_UNKNOWN;
}

int generic_action_ok_displaylist_push(const char *path,
      const char *new_path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   menu_displaylist_info_t      info;
   char tmp[PATH_MAX_LENGTH];
   char parent_dir[PATH_MAX_LENGTH];
   enum menu_displaylist_ctl_state dl_type = DISPLAYLIST_NONE;
   const char           *menu_label        = NULL;
   const char            *menu_path        = NULL;
   const char          *content_path       = NULL;
   const char          *info_label         = NULL;
   const char          *info_path          = NULL;
   menu_handle_t            *menu          = NULL;
   enum msg_hash_enums enum_idx            = MSG_UNKNOWN;
   settings_t            *settings         = config_get_ptr();
   file_list_t           *menu_stack       = menu_entries_get_menu_stack_ptr(0);

   menu_displaylist_info_init(&info);

   info.list                               = menu_stack;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      goto end;

   tmp[0] = '\0';

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, &enum_idx, NULL);

   switch (action_type)
   {
      case ACTION_OK_DL_BROWSE_URL_START:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = NULL;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_START);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_START;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_VIDEO_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST;
         dl_type           = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_USER_BINDS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_MUSIC:
         if (!string_is_empty(path))
            strlcpy(menu->scratch_buf, path, sizeof(menu->scratch_buf));
         if (!string_is_empty(menu_path))
            strlcpy(menu->scratch2_buf, menu_path, sizeof(menu->scratch2_buf));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_MUSIC);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_MUSIC;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE:
         if (menu)
         {
            menu_path    = menu->scratch2_buf;
            content_path = menu->scratch_buf;
         }
         if (content_path)
            fill_pathname_join(menu->detect_content_path,
                  menu_path, content_path,
                  sizeof(menu->detect_content_path));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_OPEN_ARCHIVE:
         if (menu)
         {
            menu_path    = menu->scratch2_buf;
            content_path = menu->scratch_buf;
         }
         if (content_path)
            fill_pathname_join(menu->detect_content_path,
                  menu_path, content_path,
                  sizeof(menu->detect_content_path));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_HELP:
         info_label             = label;
         menu_dialog_push_pending(true, (enum menu_dialog_type)type);
         dl_type                = DISPLAYLIST_HELP;
         break;
      case ACTION_OK_DL_RPL_ENTRY:
         strlcpy(menu->deferred_path, label, sizeof(menu->deferred_path));
         info_label = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS);
         info.enum_idx                 = MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS;
         info.directory_ptr            = idx;
         menu->rpl_entry_selection_ptr = (unsigned)idx;
         dl_type                       = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_AUDIO_DSP_PLUGIN:
         filebrowser_clear_type();
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_audio_filter;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN);
         info.enum_idx      = MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_SHADER_PASS:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_video_shader;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_SHADER_PARAMETERS:
         info.type          = MENU_SETTING_ACTION;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_GENERIC:
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_FILE_BROWSER_SELECT_FILE:
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_FILE_BROWSER_SELECT_DIR:
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_DIR;
         break;
      case ACTION_OK_DL_PUSH_DEFAULT:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_SHADER_PRESET:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_video_shader;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_CONTENT_LIST:
         info.type          = FILE_TYPE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = new_path;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_SCAN_DIR_LIST:
         filebrowser_set_type(FILEBROWSER_SCAN_DIR);
         info.type          = FILE_TYPE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = new_path;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SCAN_DIR;
         break;
      case ACTION_OK_DL_REMAP_FILE:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_input_remapping;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_RECORD_CONFIGFILE:
         filebrowser_clear_type();
         {
            global_t  *global  = global_get_ptr();
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = global->record.config_dir;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         }
         break;
      case ACTION_OK_DL_DISK_IMAGE_APPEND_LIST:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_menu_content;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_SUBSYSTEM_ADD_LIST:
         filebrowser_clear_type();
         if (content_get_subsystem() != type - MENU_SETTINGS_SUBSYSTEM_ADD)
            content_clear_subsystem();
         content_set_subsystem(type - MENU_SETTINGS_SUBSYSTEM_ADD);
         filebrowser_set_type(FILEBROWSER_SELECT_FILE_SUBSYSTEM);
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_menu_content;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_SUBSYSTEM_LOAD:
         {
            content_ctx_info_t content_info = {0};
            filebrowser_clear_type();
            task_push_load_subsystem_with_core_from_menu(
                  NULL, &content_info,
                  CORE_TYPE_PLAIN, NULL, NULL);
         }
         break;
      case ACTION_OK_DL_CHEAT_FILE:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.path_cheat_database;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_CHEAT_FILE_APPEND:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.path_cheat_database;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_CORE_LIST:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_libretro;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_CORE;
         break;
      case ACTION_OK_DL_CONTENT_COLLECTION_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_playlist;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_COLLECTION;
         break;
      case ACTION_OK_DL_RDB_ENTRY:
         filebrowser_clear_type();
         fill_pathname_join_delim(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL),
               path, '|', sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = label;
         info_label         = tmp;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_RDB_ENTRY_SUBMENU:
         info.directory_ptr = idx;
         info_label         = label;
         info_path          = path;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CONFIGURATIONS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         if (string_is_empty(settings->paths.directory_menu_config))
            info_path        = label;
         else
            info_path        = settings->paths.directory_menu_config;
         info_label = label;
         dl_type             = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE;

         if (!string_is_empty(path))
            strlcpy(menu->scratch_buf, path, sizeof(menu->scratch_buf));
         if (!string_is_empty(menu_path))
            strlcpy(menu->scratch2_buf, menu_path, sizeof(menu->scratch2_buf));
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION;

         if (!string_is_empty(path))
            strlcpy(menu->scratch_buf, path, sizeof(menu->scratch_buf));
         if (!string_is_empty(menu_path))
            strlcpy(menu->scratch2_buf, menu_path, sizeof(menu->scratch2_buf));
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_PARENT_DIRECTORY_PUSH:
         parent_dir[0]  = '\0';

         if (path && menu_path)
            fill_pathname_join(tmp,
                  menu_path, path, sizeof(tmp));

         fill_pathname_parent_dir(parent_dir,
               tmp, sizeof(parent_dir));
         fill_pathname_parent_dir(parent_dir,
               parent_dir, sizeof(parent_dir));

         info.type          = type;
         info.directory_ptr = idx;
         info_path          = parent_dir;
         info_label         = menu_label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DIRECTORY_PUSH:
         if (path && menu_path)
            fill_pathname_join(tmp,
                  menu_path, path, sizeof(tmp));

         info.type          = type;
         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = menu_label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DATABASE_MANAGER_LIST:
         {
            char lpl_basename[PATH_MAX_LENGTH];
            lpl_basename[0] = '\0';
            filebrowser_clear_type();
            fill_pathname_join(tmp,
                  settings->paths.path_content_database,
                  path, sizeof(tmp));

            fill_pathname_base_noext(lpl_basename, path, sizeof(lpl_basename));
            menu_driver_set_thumbnail_system(lpl_basename, sizeof(lpl_basename));

            info.directory_ptr = idx;
            info_path          = tmp;
            info_label         = msg_hash_to_str(
                  MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST);
            info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST;
            dl_type                 = DISPLAYLIST_GENERIC;
         }
         break;
      case ACTION_OK_DL_CURSOR_MANAGER_LIST:
         filebrowser_clear_type();
         fill_pathname_join(tmp, settings->paths.directory_cursor,
               path, sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CORE_UPDATER_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_THUMBNAILS_UPDATER_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_CORE_CONTENT_DIRS_SUBDIR_LIST:
         fill_pathname_join_delim(tmp, path, label, ';',
               sizeof(tmp));
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CORE_CONTENT_DIRS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_CORE_CONTENT_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_LAKKA_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST:
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_libretro;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST_SET:
         info.directory_ptr                       = idx;
         menu->scratchpad.unsigned_var            = (unsigned)idx;
         info_path                                =
            settings->paths.directory_libretro;
         info_label                               = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET);
         info.enum_idx                            =
            MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET;
         dl_type                                  = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CHEAT_DETAILS_SETTINGS_LIST:
      {
         rarch_setting_t *setting = NULL;

         cheat_manager_copy_idx_to_working(type-MENU_SETTINGS_CHEAT_BEGIN) ;
         setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_IDX));
         if (setting)
            setting->max = cheat_manager_get_size()-1 ;
         setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_VALUE));
         if ( setting )
            setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.working_cheat.memory_search_size))-1;
         setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE));
         if (setting)
            setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.working_cheat.memory_search_size))-1;
         setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION));
         if ( setting )
       {
            int max_bit_position = cheat_manager_state.working_cheat.memory_search_size<3 ? 7 : 0 ;
            setting->max = max_bit_position ;
         }
         setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_ADDRESS));
         if ( setting )
         {
            cheat_manager_state.browse_address = *setting->value.target.unsigned_integer ;
         }
         action_ok_dl_lbl(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         break ;
      }
      case ACTION_OK_DL_CHEAT_SEARCH_SETTINGS_LIST:
      {
         rarch_setting_t *setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT));
         if ( setting)
            setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1;
         setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS));
         if ( setting )
            setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1;
         setting = menu_setting_find(msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS));
         if ( setting )
            setting->max = (int) pow(2,pow((double) 2,cheat_manager_state.search_bit_size))-1;
         action_ok_dl_lbl(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         break ;
      }
      case ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST:
      case ACTION_OK_DL_ACCOUNTS_LIST:
      case ACTION_OK_DL_INPUT_SETTINGS_LIST:
      case ACTION_OK_DL_LATENCY_SETTINGS_LIST:
      case ACTION_OK_DL_DRIVER_SETTINGS_LIST:
      case ACTION_OK_DL_CORE_SETTINGS_LIST:
      case ACTION_OK_DL_VIDEO_SETTINGS_LIST:
      case ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST:
      case ACTION_OK_DL_SAVING_SETTINGS_LIST:
      case ACTION_OK_DL_LOGGING_SETTINGS_LIST:
      case ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST:
      case ACTION_OK_DL_REWIND_SETTINGS_LIST:
      case ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST:
      case ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
      case ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST:
      case ACTION_OK_DL_MENU_SETTINGS_LIST:
      case ACTION_OK_DL_MENU_VIEWS_SETTINGS_LIST:
      case ACTION_OK_DL_QUICK_MENU_VIEWS_SETTINGS_LIST:
      case ACTION_OK_DL_QUICK_MENU_OVERRIDE_OPTIONS_LIST:
      case ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST:
      case ACTION_OK_DL_POWER_MANAGEMENT_SETTINGS_LIST:
      case ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST:
      case ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
      case ACTION_OK_DL_UPDATER_SETTINGS_LIST:
      case ACTION_OK_DL_NETWORK_SETTINGS_LIST:
      case ACTION_OK_DL_WIFI_SETTINGS_LIST:
      case ACTION_OK_DL_NETPLAY:
      case ACTION_OK_DL_NETPLAY_LAN_SCAN_SETTINGS_LIST:
      case ACTION_OK_DL_LAKKA_SERVICES_LIST:
      case ACTION_OK_DL_USER_SETTINGS_LIST:
      case ACTION_OK_DL_DIRECTORY_SETTINGS_LIST:
      case ACTION_OK_DL_PRIVACY_SETTINGS_LIST:
      case ACTION_OK_DL_MIDI_SETTINGS_LIST:
      case ACTION_OK_DL_AUDIO_SETTINGS_LIST:
      case ACTION_OK_DL_AUDIO_MIXER_SETTINGS_LIST:
      case ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST:
      case ACTION_OK_DL_RECORDING_SETTINGS_LIST:
      case ACTION_OK_DL_PLAYLIST_SETTINGS_LIST:
      case ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST:
      case ACTION_OK_DL_PLAYLIST_COLLECTION:
      case ACTION_OK_DL_FAVORITES_LIST:
      case ACTION_OK_DL_BROWSE_URL_LIST:
      case ACTION_OK_DL_MUSIC_LIST:
      case ACTION_OK_DL_IMAGES_LIST:
         action_ok_dl_lbl(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         break;
      case ACTION_OK_DL_CONTENT_SETTINGS:
         info.list          = menu_entries_get_selection_buf_ptr(0);
         info_path          = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS);
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS);
         info.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
         menu_entries_append_enum(menu_stack, info_path, info_label,
               MENU_ENUM_LABEL_CONTENT_SETTINGS,
               0, 0, 0);
         dl_type            = DISPLAYLIST_CONTENT_SETTINGS;
         break;
   }

   /* second pass */

   switch (action_type)
   {
      case ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST:
         {
            unsigned player_no = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN;
            info.type          = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_BEGIN + player_no;
         }
         break;
      default:
         break;
   }

   if (info_label)
      info.label = strdup(info_label);
   if (info_path)
      info.path  = strdup(info_path);

   if (menu_displaylist_ctl(dl_type, &info))
   {
      if (menu_displaylist_process(&info))
      {
         menu_displaylist_info_free(&info);
         return 0;
      }
   }

end:
   menu_displaylist_info_free(&info);
   return menu_cbs_exit();
}

/**
 * menu_content_load_from_playlist:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
static bool menu_content_playlist_load(playlist_t *playlist, size_t idx)
{
   const char *path     = NULL;

   playlist_get_index(playlist,
         idx, &path, NULL, NULL, NULL, NULL, NULL);

   if (!string_is_empty(path))
   {
      unsigned i;
      bool valid_path     = false;
      char *path_check    = NULL;
      char *path_tolower  = strdup(path);

      for (i = 0; i < strlen(path_tolower); ++i)
         path_tolower[i] = tolower((unsigned char)path_tolower[i]);

      if (strstr(path_tolower, file_path_str(FILE_PATH_ZIP_EXTENSION)))
         strstr(path_tolower, file_path_str(FILE_PATH_ZIP_EXTENSION))[4] = '\0';
      else if (strstr(path_tolower, file_path_str(FILE_PATH_7Z_EXTENSION)))
         strstr(path_tolower, file_path_str(FILE_PATH_7Z_EXTENSION))[3] = '\0';

      path_check = (char *)
         calloc(strlen(path_tolower) + 1, sizeof(char));

      strlcpy(path_check, path, strlen(path_tolower) + 1);

      valid_path = path_is_valid(path_check);

      free(path_tolower);
      free(path_check);

      if (valid_path)
         return true;
   }

   return false;
}

/**
 * menu_content_find_first_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @s                    : Deferred core path. Will be filled in
 *                         by function.
 * @len                  : Size of @s.
 *
 * Gets deferred core.
 *
 * Returns: false if there are multiple deferred cores and a
 * selection needs to be made from a list, otherwise
 * returns true and fills in @s with path to core.
 **/
static bool menu_content_find_first_core(menu_content_ctx_defer_info_t *def_info,
      bool load_content_with_current_core,
      char *new_core_path, size_t len)
{
   const core_info_t *info                 = NULL;
   size_t supported                        = 0;
   core_info_list_t *core_info             = (core_info_list_t*)def_info->data;
   const char *default_info_dir            = def_info->dir;

   if (!string_is_empty(default_info_dir))
   {
      const char *default_info_path = def_info->path;
      size_t default_info_length    = def_info->len;

      if (!string_is_empty(default_info_path))
         fill_pathname_join(def_info->s,
               default_info_dir, default_info_path,
               default_info_length);

#ifdef HAVE_COMPRESSION
      if (path_is_compressed_file(default_info_dir))
      {
         size_t len = strlen(default_info_dir);
         /* In case of a compressed archive, we have to join with a hash */
         /* We are going to write at the position of dir: */
         retro_assert(len < strlen(def_info->s));
         def_info->s[len] = '#';
      }
#endif
   }

   if (core_info)
      core_info_list_get_supported_cores(core_info,
            def_info->s, &info,
            &supported);

   /* We started the menu with 'Load Content', we are
    * going to use the current core to load this. */
   if (load_content_with_current_core)
   {
      core_info_get_current_core((core_info_t**)&info);
      if (info)
      {
#if 0
         RARCH_LOG("[lobby] use the current core (%s) to load this content...\n",
               info->path);
#endif
         supported = 1;
      }
   }

   /* There are multiple deferred cores and a
    * selection needs to be made from a list, return 0. */
   if (supported != 1)
      return false;

    if (info)
      strlcpy(new_core_path, info->path, len);

   return true;
}

#ifdef HAVE_LIBRETRODB
void handle_dbscan_finished(void *task_data, void *user_data, const char *err);
#endif

static void content_add_to_playlist(const char *path)
{
#ifdef HAVE_LIBRETRODB
   settings_t *settings = config_get_ptr();
   if (!settings || !settings->bools.automatically_add_content_to_playlist)
      return;
   task_push_dbscan(
         settings->paths.directory_playlist,
         settings->paths.path_content_database,
         path, false,
         settings->bools.show_hidden_files,
         handle_dbscan_finished);
#endif
}

static int file_load_with_detect_core_wrapper(
      enum msg_hash_enums enum_label_idx,
      enum msg_hash_enums enum_idx,
      size_t idx, size_t entry_idx,
      const char *path, const char *label,
      unsigned type, bool is_carchive)
{
   menu_content_ctx_defer_info_t def_info;
   int ret                             = 0;
   char *new_core_path                 = NULL;
   const char *menu_path               = NULL;
   const char *menu_label              = NULL;
   menu_handle_t *menu                 = NULL;
   core_info_list_t *list              = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   {
      char *menu_path_new = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      new_core_path       = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      new_core_path[0]    = menu_path_new[0] = '\0';

      menu_entries_get_last_stack(&menu_path, &menu_label, NULL, &enum_idx, NULL);

      if (!string_is_empty(menu_path))
         strlcpy(menu_path_new, menu_path, PATH_MAX_LENGTH * sizeof(char));

      if (string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)))
         fill_pathname_join(menu_path_new, menu->scratch2_buf, menu->scratch_buf,
               PATH_MAX_LENGTH * sizeof(char));
      else if (string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN)))
         fill_pathname_join(menu_path_new, menu->scratch2_buf, menu->scratch_buf,
               PATH_MAX_LENGTH * sizeof(char));

      core_info_get_list(&list);

      def_info.data       = list;
      def_info.dir        = menu_path_new;
      def_info.path       = path;
      def_info.menu_label = menu_label;
      def_info.s          = menu->deferred_path;
      def_info.len        = sizeof(menu->deferred_path);

      if (menu_content_find_first_core(&def_info, false, new_core_path,
               PATH_MAX_LENGTH * sizeof(char)))
         ret = -1;


      if (     !is_carchive && !string_is_empty(path)
            && !string_is_empty(menu_path_new))
         fill_pathname_join(menu->detect_content_path,
               menu_path_new, path,
               sizeof(menu->detect_content_path));

      free(menu_path_new);

      if (enum_label_idx == MENU_ENUM_LABEL_COLLECTION)
      {
         free(new_core_path);
         return generic_action_ok_displaylist_push(path, NULL,
               NULL, 0, idx, entry_idx, ACTION_OK_DL_DEFERRED_CORE_LIST_SET);
      }

      switch (ret)
      {
         case -1:
            {
               content_ctx_info_t content_info;

               content_info.argc        = 0;
               content_info.argv        = NULL;
               content_info.args        = NULL;
               content_info.environ_get = NULL;

               if (!task_push_load_content_with_new_core_from_menu(
                        new_core_path, def_info.s,
                        &content_info,
                        CORE_TYPE_PLAIN,
                        NULL, NULL))
               {
                  free(new_core_path);
                  return -1;
               }
               content_add_to_playlist(def_info.s);

               ret = 0;
               break;
            }
         case 0:
            ret = generic_action_ok_displaylist_push(path, NULL, label, type,
                  idx, entry_idx, ACTION_OK_DL_DEFERRED_CORE_LIST);
            break;
         default:
            break;
      }
   }

   free(new_core_path);
   return ret;
}

static int action_ok_file_load_with_detect_core_carchive(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu                 = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   fill_pathname_join_delim(menu->detect_content_path,
         menu->detect_content_path, path,
         '#', sizeof(menu->detect_content_path));

   type = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(MSG_UNKNOWN,
         MSG_UNKNOWN, idx, entry_idx,
         path, label, type, true);
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{

   type  = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(
         MSG_UNKNOWN,
         MSG_UNKNOWN, idx, entry_idx,
         path, label, type, false);
}

static int action_ok_file_load_with_detect_core_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   type  = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(
         MENU_ENUM_LABEL_COLLECTION,
         MSG_UNKNOWN, idx, entry_idx,
         path, label, type, false);
}

static int set_path_generic(const char *label, const char *action_path)
{
   rarch_setting_t *setting = menu_setting_find(label);

   if (setting)
   {
      setting_set_with_string_representation(
            setting, action_path);
      return menu_setting_generic(setting, false);
   }

   return 0;
}

static int generic_action_ok_command(enum event_command cmd)
{
   if (!command_event(cmd, NULL))
      return menu_cbs_exit();
   return 0;
}

static int generic_action_ok(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned id, enum msg_hash_enums flush_id)
{
   char action_path[PATH_MAX_LENGTH];
   unsigned flush_type               = 0;
   int ret                           = 0;
   enum msg_hash_enums enum_idx      = MSG_UNKNOWN;
   const char             *menu_path = NULL;
   const char            *menu_label = NULL;
   const char *flush_char            = NULL;
   menu_handle_t               *menu = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      goto error;

   menu_entries_get_last_stack(&menu_path,
         &menu_label, NULL, &enum_idx, NULL);

   action_path[0] = '\0';

   if (!string_is_empty(path))
      fill_pathname_join(action_path,
            menu_path, path, sizeof(action_path));
   else
      strlcpy(action_path, menu_path, sizeof(action_path));

   switch (id)
   {
      case ACTION_OK_LOAD_WALLPAPER:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST);
         if (filestream_exists(action_path))
         {
            settings_t            *settings = config_get_ptr();

            strlcpy(settings->paths.path_menu_wallpaper,
                  action_path, sizeof(settings->paths.path_menu_wallpaper));

            if (filestream_exists(action_path))
               task_push_image_load(action_path,
                     menu_display_handle_wallpaper_upload, NULL);
         }
         break;
      case ACTION_OK_LOAD_CORE:
         {
            content_ctx_info_t content_info;

            content_info.argc        = 0;
            content_info.argv        = NULL;
            content_info.args        = NULL;
            content_info.environ_get = NULL;

            flush_type = MENU_SETTINGS;

            if (!task_push_load_new_core(
                     action_path, NULL,
                     &content_info,
                     CORE_TYPE_PLAIN,
                     NULL, NULL))
            {
#ifndef HAVE_DYNAMIC
               ret = -1;
#endif
            }
         }
         break;
      case ACTION_OK_LOAD_CONFIG_FILE:
         {
            settings_t            *settings = config_get_ptr();
            flush_type                      = MENU_SETTINGS;

            menu_display_set_msg_force(true);

            if (config_replace(settings->bools.config_save_on_exit, action_path))
            {
               bool pending_push = false;
               menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
               ret = -1;
            }
         }
         break;
      case ACTION_OK_LOAD_PRESET:
         {
            struct video_shader      *shader  = menu_shader_get();
            flush_char = msg_hash_to_str(flush_id);
            menu_shader_manager_set_preset(shader,
                  video_shader_parse_type(action_path, RARCH_SHADER_NONE),
                  action_path);
         }
         break;
      case ACTION_OK_LOAD_SHADER_PASS:
         {
            struct video_shader *shader           = menu_shader_get();
            struct video_shader_pass *shader_pass = shader ? &shader->pass[menu->scratchpad.unsigned_var] : NULL;
            flush_char                            = msg_hash_to_str((enum msg_hash_enums)flush_id);

            if (shader_pass)
            {
               strlcpy(
                     shader_pass->source.path,
                     action_path,
                     sizeof(shader_pass->source.path));
               video_shader_resolve_parameters(NULL, shader);
            }
         }
         break;
      case ACTION_OK_LOAD_RECORD_CONFIGFILE:
         {
            global_t *global = global_get_ptr();
            flush_char       = msg_hash_to_str(flush_id);

            if (global)
               strlcpy(global->record.config, action_path,
                     sizeof(global->record.config));
         }
         break;
      case ACTION_OK_LOAD_REMAPPING_FILE:
         {
            config_file_t *conf = config_file_new(action_path);
            flush_char          = msg_hash_to_str(flush_id);

            if (conf)
               input_remapping_load_file(conf, action_path);
         }
         break;
      case ACTION_OK_LOAD_CHEAT_FILE:
         flush_char = msg_hash_to_str(flush_id);
         cheat_manager_free();

         if (!cheat_manager_load(action_path,false))
            goto error;
         break;
      case ACTION_OK_LOAD_CHEAT_FILE_APPEND:
         flush_char = msg_hash_to_str(flush_id);
#if 0
         cheat_manager_free();
#endif

         if (!cheat_manager_load(action_path,true))
            goto error;
         break;
      case ACTION_OK_APPEND_DISK_IMAGE:
         flush_type = MENU_SETTINGS;
         command_event(CMD_EVENT_DISK_APPEND_IMAGE, action_path);
         generic_action_ok_command(CMD_EVENT_RESUME);
         break;
      case ACTION_OK_SUBSYSTEM_ADD:
         flush_type = MENU_SETTINGS;
         content_add_subsystem(action_path);
         break;
      case ACTION_OK_SET_DIRECTORY:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST);
         ret        = set_path_generic(menu->filebrowser_label, action_path);
         break;
      case ACTION_OK_SET_PATH_VIDEO_FILTER:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH_AUDIO_FILTER:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH_OVERLAY:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH:
         flush_type = MENU_SETTINGS;
         ret        = set_path_generic(menu_label, action_path);
         break;
      default:
         flush_char = msg_hash_to_str(flush_id);
         break;
   }

   menu_entries_flush_stack(flush_char, flush_type);

   return ret;

error:
   return menu_cbs_exit();
}

static int default_action_ok_load_content_with_core_from_menu(const char *_path, unsigned _type)
{
   content_ctx_info_t content_info;
   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;
   if (!task_push_load_content_with_core_from_menu(
            _path, &content_info,
            (enum rarch_core_type)_type, NULL, NULL))
      return -1;
   content_add_to_playlist(_path);
   return 0;
}

static int default_action_ok_load_content_from_playlist_from_menu(const char *_path,
      const char *path, const char *entry_label)
{
   content_ctx_info_t content_info;
   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;
   if (!task_push_load_content_from_playlist_from_menu(
            _path, path, entry_label,
            &content_info,
            NULL, NULL))
      return -1;
   return 0;
}


#define default_action_ok_set(funcname, _id, _flush) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok(path, label, type, idx, entry_idx, _id, _flush); \
}

default_action_ok_set(action_ok_set_path_audiofilter, ACTION_OK_SET_PATH_VIDEO_FILTER, MSG_UNKNOWN)
default_action_ok_set(action_ok_set_path_videofilter, ACTION_OK_SET_PATH_AUDIO_FILTER, MSG_UNKNOWN)
default_action_ok_set(action_ok_set_path_overlay,     ACTION_OK_SET_PATH_OVERLAY,      MSG_UNKNOWN)
default_action_ok_set(action_ok_set_path,             ACTION_OK_SET_PATH,              MSG_UNKNOWN)
default_action_ok_set(action_ok_load_core,            ACTION_OK_LOAD_CORE,             MSG_UNKNOWN)
default_action_ok_set(action_ok_config_load,          ACTION_OK_LOAD_CONFIG_FILE,      MSG_UNKNOWN)
default_action_ok_set(action_ok_disk_image_append,    ACTION_OK_APPEND_DISK_IMAGE,     MSG_UNKNOWN)
default_action_ok_set(action_ok_subsystem_add,        ACTION_OK_SUBSYSTEM_ADD,         MSG_UNKNOWN)
default_action_ok_set(action_ok_cheat_file_load,      ACTION_OK_LOAD_CHEAT_FILE,       MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)
default_action_ok_set(action_ok_cheat_file_load_append,      ACTION_OK_LOAD_CHEAT_FILE_APPEND,       MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)
default_action_ok_set(action_ok_record_configfile_load,      ACTION_OK_LOAD_RECORD_CONFIGFILE,       MENU_ENUM_LABEL_RECORDING_SETTINGS)
default_action_ok_set(action_ok_remap_file_load,      ACTION_OK_LOAD_REMAPPING_FILE,   MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS    )
default_action_ok_set(action_ok_shader_preset_load,   ACTION_OK_LOAD_PRESET   ,        MENU_ENUM_LABEL_SHADER_OPTIONS)
default_action_ok_set(action_ok_shader_pass_load,     ACTION_OK_LOAD_SHADER_PASS,      MENU_ENUM_LABEL_SHADER_OPTIONS)

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char menu_path_new[PATH_MAX_LENGTH];
   char full_path_new[PATH_MAX_LENGTH];
   const char *menu_label              = NULL;
   const char *menu_path               = NULL;
   rarch_setting_t *setting            = NULL;
   file_list_t  *menu_stack            = menu_entries_get_menu_stack_ptr(0);

   menu_path_new[0] = full_path_new[0] = '\0';

   if (filebrowser_get_type() == FILEBROWSER_SELECT_FILE_SUBSYSTEM)
   {
      /* TODO/FIXME - this path is triggered when we try to load a
       * file from an archive while inside the load subsystem
       * action */
      menu_handle_t *menu                 = NULL;
      if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
         return menu_cbs_exit();

      fill_pathname_join(menu_path_new,
            menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));
      switch (type)
      {
         case FILE_TYPE_IN_CARCHIVE:
            fill_pathname_join_delim(full_path_new, menu_path_new, path,
                  '#',sizeof(full_path_new));
            break;
         default:
            fill_pathname_join(full_path_new, menu_path_new, path,
                  sizeof(full_path_new));
            break;
      }

      content_add_subsystem(full_path_new);
      menu_entries_flush_stack(NULL, MENU_SETTINGS);
      return 0;
   }

   file_list_get_last(menu_stack, &menu_path, &menu_label, NULL, NULL);

   if (!string_is_empty(menu_label))
      setting = menu_setting_find(menu_label);

   if (setting_get_type(setting) == ST_PATH)
      return action_ok_set_path(path, label, type, idx, entry_idx);

   if (!string_is_empty(menu_path))
      strlcpy(menu_path_new, menu_path, sizeof(menu_path_new));

   if (!string_is_empty(menu_label))
   {
      if (
            string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)) ||
            string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN))
         )
      {
         menu_handle_t *menu                 = NULL;
         if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
            return menu_cbs_exit();

         fill_pathname_join(menu_path_new,
               menu->scratch2_buf, menu->scratch_buf,
               sizeof(menu_path_new));
      }
   }

   switch (type)
   {
      case FILE_TYPE_IN_CARCHIVE:
         fill_pathname_join_delim(full_path_new, menu_path_new, path,
               '#',sizeof(full_path_new));
         break;
      default:
         fill_pathname_join(full_path_new, menu_path_new, path,
               sizeof(full_path_new));
         break;
   }

   return default_action_ok_load_content_with_core_from_menu(full_path_new,
         CORE_TYPE_PLAIN);
}


static int action_ok_playlist_entry_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char new_core_path[PATH_MAX_LENGTH];
   size_t selection_ptr                = 0;
   bool playlist_initialized           = false;
   playlist_t *playlist                = NULL;
   const char *entry_path              = NULL;
   const char *entry_label             = NULL;
   const char *core_path               = NULL;
   const char *core_name               = NULL;
   playlist_t *tmp_playlist            = NULL;
   menu_handle_t *menu                 = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   new_core_path[0]                    = '\0';
   tmp_playlist                        = playlist_get_cached();

   if (!tmp_playlist)
   {
      tmp_playlist = playlist_init(
            menu->db_playlist_file, COLLECTION_SIZE);

      if (!tmp_playlist)
         return menu_cbs_exit();
      playlist_initialized = true;
   }

   playlist      = tmp_playlist;
   selection_ptr = entry_idx;

   playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL);

   /* Is the core path / name of the playlist entry not yet filled in? */
   if (     string_is_equal(core_path, file_path_str(FILE_PATH_DETECT))
         && string_is_equal(core_name, file_path_str(FILE_PATH_DETECT)))
   {
      core_info_ctx_find_t core_info;
      const char *entry_path                 = NULL;
      const char             *path_base      =
         path_basename(menu->db_playlist_file);
      bool        found_associated_core      =
         menu_content_playlist_find_associated_core(
            path_base, new_core_path, sizeof(new_core_path));

      core_info.inf       = NULL;
      core_info.path      = new_core_path;

      if (!core_info_find(&core_info, new_core_path))
         found_associated_core = false;

      if (!found_associated_core)
      {
         /* TODO: figure out if this should refer to the inner or outer entry_path */
         /* TODO: make sure there's only one entry_path in this function */
         int ret = action_ok_file_load_with_detect_core_collection(entry_path,
               label, type, selection_ptr, entry_idx);
         if (playlist_initialized)
            playlist_free(tmp_playlist);
         return ret;
      }

      tmp_playlist = playlist_get_cached();

      if (tmp_playlist)
         command_playlist_update_write(
               tmp_playlist,
               selection_ptr,
               NULL,
               NULL,
               new_core_path,
               core_info.inf->display_name,
               NULL,
               NULL);
   }
   else
      strlcpy(new_core_path, core_path, sizeof(new_core_path));

   if (!playlist || !menu_content_playlist_load(playlist, selection_ptr))
   {
      runloop_msg_queue_push(
            "File could not be loaded from playlist.\n",
            1, 100, true);
      if (playlist_initialized)
         playlist_free(tmp_playlist);
      return menu_cbs_exit();
   }

   playlist_get_index(playlist,
         selection_ptr, &path, NULL, NULL, NULL, NULL, NULL);

   return default_action_ok_load_content_from_playlist_from_menu(
         new_core_path, path, entry_label);
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char new_core_path[PATH_MAX_LENGTH];
   size_t selection_ptr                = 0;
   playlist_t *playlist                = g_defaults.content_history;
   const char *entry_path              = NULL;
   const char *entry_label             = NULL;
   const char *core_path               = NULL;
   const char *core_name               = NULL;
   menu_handle_t *menu                 = NULL;

   new_core_path[0] = '\0';

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   selection_ptr = entry_idx;

   playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL);

   if (     string_is_equal(core_path, file_path_str(FILE_PATH_DETECT))
         && string_is_equal(core_name, file_path_str(FILE_PATH_DETECT)))
   {
      core_info_ctx_find_t core_info;
      const char *entry_path                 = NULL;
      const char *path_base                  =
         path_basename(menu->db_playlist_file);
      bool        found_associated_core      =
         menu_content_playlist_find_associated_core(
            path_base, new_core_path, sizeof(new_core_path));

      core_info.inf                          = NULL;
      core_info.path                         = new_core_path;

      if (!core_info_find(&core_info, new_core_path))
         found_associated_core = false;

      if (!found_associated_core)
         /* TODO: figure out if this should refer to the inner or outer entry_path */
         /* TODO: make sure there's only one entry_path in this function */
         return action_ok_file_load_with_detect_core(entry_path,
               label, type, selection_ptr, entry_idx);

      command_playlist_update_write(NULL,
            selection_ptr,
            NULL,
            NULL,
            new_core_path,
            core_info.inf->display_name,
            NULL,
            NULL);

   }
   else if (!string_is_empty(core_path))
      strlcpy(new_core_path, core_path, sizeof(new_core_path));

   if (!playlist || !menu_content_playlist_load(playlist, selection_ptr))
   {
      runloop_msg_queue_push(
            "File could not be loaded from playlist.\n",
            1, 100, true);
      return menu_cbs_exit();
   }

   playlist_get_index(playlist,
         selection_ptr, &path, NULL, NULL, NULL,
         NULL, NULL);

   return default_action_ok_load_content_from_playlist_from_menu(
         new_core_path, path, entry_label);
}

static int action_ok_playlist_entry_start_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t selection_ptr                = 0;
   const char *entry_path              = NULL;
   const char *entry_label             = NULL;
   const char *core_path               = NULL;
   const char *core_name               = NULL;
   menu_handle_t *menu                 = NULL;
   playlist_t *playlist                = playlist_get_cached();

   if (  !playlist ||
         !menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   selection_ptr                       = menu->scratchpad.unsigned_var;

   playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL);

   if (     string_is_equal(core_path, file_path_str(FILE_PATH_DETECT))
         && string_is_equal(core_name, file_path_str(FILE_PATH_DETECT)))
   {
      core_info_ctx_find_t core_info;
      char new_core_path[PATH_MAX_LENGTH];
      const char *entry_path                 = NULL;
      const char             *path_base      =
         path_basename(menu->db_playlist_file);
      bool        found_associated_core      = false;

      new_core_path[0]                       = '\0';

      found_associated_core                  =
         menu_content_playlist_find_associated_core(
            path_base, new_core_path, sizeof(new_core_path));

      core_info.inf                          = NULL;
      core_info.path                         = new_core_path;

      if (!core_info_find(&core_info, new_core_path))
         found_associated_core = false;

      /* TODO: figure out if this should refer to
       * the inner or outer entry_path. */
      /* TODO: make sure there's only one entry_path
       * in this function. */
      if (!found_associated_core)
         return action_ok_file_load_with_detect_core(entry_path,
               label, type, selection_ptr, entry_idx);

      command_playlist_update_write(
            playlist,
            selection_ptr,
            NULL,
            NULL,
            new_core_path,
            core_info.inf->display_name,
            NULL,
            NULL);
   }

   if (!menu_content_playlist_load(playlist, selection_ptr))
   {
      runloop_msg_queue_push("File could not be loaded from playlist.\n", 1, 100, true);
      goto error;
   }

   playlist_get_index(playlist,
         selection_ptr, &path, NULL, NULL, NULL, NULL, NULL);

   return default_action_ok_load_content_from_playlist_from_menu(core_path, path, entry_label);

error:
   return menu_cbs_exit();
}

static int action_ok_mixer_stream_action_play(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);

   switch (state)
   {
      case AUDIO_STREAM_STATE_STOPPED:
         audio_driver_mixer_play_stream(stream_id);
         break;
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   return 0;
}

static int action_ok_mixer_stream_action_play_looped(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);

   switch (state)
   {
      case AUDIO_STREAM_STATE_STOPPED:
         audio_driver_mixer_play_stream_looped(stream_id);
         break;
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   return 0;
}

static int action_ok_mixer_stream_action_play_sequential(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);

   switch (state)
   {
      case AUDIO_STREAM_STATE_STOPPED:
         audio_driver_mixer_play_stream_sequential(stream_id);
         break;
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   return 0;
}

static int action_ok_mixer_stream_action_remove(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);

   switch (state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_STOPPED:
         audio_driver_mixer_remove_stream(stream_id);
         break;
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   return 0;
}

static int action_ok_mixer_stream_action_stop(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);

   switch (state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         audio_driver_mixer_stop_stream(stream_id);
         break;
      case AUDIO_STREAM_STATE_STOPPED:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   return 0;
}

static int action_ok_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return menu_setting_set(type, label, MENU_ACTION_OK, false);
}

static int action_ok_audio_add_to_mixer(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *entry_path              = NULL;
   playlist_t *tmp_playlist            = playlist_get_cached();

   if (!tmp_playlist)
      return -1;

   playlist_get_index(tmp_playlist, entry_idx,
         &entry_path, NULL, NULL, NULL, NULL, NULL);

   if (filestream_exists(entry_path))
      task_push_audio_mixer_load(entry_path,
            NULL, NULL);

   return 0;
}

static int action_ok_audio_add_to_mixer_and_play(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *entry_path              = NULL;
   playlist_t *tmp_playlist            = playlist_get_cached();

   if (!tmp_playlist)
      return -1;

   playlist_get_index(tmp_playlist, entry_idx,
         &entry_path, NULL, NULL, NULL, NULL, NULL);

   if (filestream_exists(entry_path))
      task_push_audio_mixer_load_and_play(entry_path,
            NULL, NULL);

   return 0;
}

static int action_ok_audio_add_to_mixer_and_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char combined_path[PATH_MAX_LENGTH];
   menu_handle_t *menu                 = NULL;

   combined_path[0] = '\0';

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   fill_pathname_join(combined_path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(combined_path));

   command_playlist_push_write(
         g_defaults.music_history,
         combined_path,
         NULL,
         "builtin",
         "musicplayer");

   if (filestream_exists(combined_path))
      task_push_audio_mixer_load(combined_path,
            NULL, NULL);

   return 0;
}

static int action_ok_audio_add_to_mixer_and_collection_and_play(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char combined_path[PATH_MAX_LENGTH];
   menu_handle_t *menu                 = NULL;

   combined_path[0] = '\0';

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   fill_pathname_join(combined_path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(combined_path));

   command_playlist_push_write(
         g_defaults.music_history,
         combined_path,
         NULL,
         "builtin",
         "musicplayer");

   if (filestream_exists(combined_path))
      task_push_audio_mixer_load_and_play(combined_path,
            NULL, NULL);

   return 0;
}

static int action_ok_menu_wallpaper(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filebrowser_set_type(FILEBROWSER_SELECT_IMAGE);
   return action_ok_lookup_setting(path, label, type, idx, entry_idx);
}

static int action_ok_menu_font(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filebrowser_set_type(FILEBROWSER_SELECT_FONT);
   return action_ok_lookup_setting(path, label, type, idx, entry_idx);
}

static int action_ok_menu_wallpaper_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings            = config_get_ptr();

   filebrowser_clear_type();

   settings->uints.menu_xmb_shader_pipeline = XMB_SHADER_PIPELINE_WALLPAPER;
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_WALLPAPER, MSG_UNKNOWN);
}

int  generic_action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums id, enum menu_dialog_type id2)
{
   const char               *lbl  = msg_hash_to_str(id);

   return generic_action_ok_displaylist_push(path, NULL, lbl, id2, idx,
         entry_idx, ACTION_OK_DL_HELP);
}

static void menu_input_wifi_cb(void *userdata, const char *passphrase)
{
   unsigned idx = menu_input_dialog_get_kb_idx();

   driver_wifi_connect_ssid(idx, passphrase);

   menu_input_dialog_end();
}


static void menu_input_st_string_cb_rename_entry(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      const char        *label    = menu_input_dialog_get_buffer();

      if (!string_is_empty(label))
         command_playlist_update_write(NULL,
               menu_input_dialog_get_kb_idx(),
               NULL,
               label,
               NULL,
               NULL,
               NULL,
               NULL);
   }


   menu_input_dialog_end();
}

static void menu_input_st_string_cb_disable_kiosk_mode(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      const char *label = menu_input_dialog_get_buffer();
      settings_t *settings = config_get_ptr();

      if (string_is_equal(label, settings->paths.kiosk_mode_password))
      {
         settings->bools.kiosk_mode_enable = false;

         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_KIOSK_MODE_PASSWORD_OK),
            1, 100, true);
      }
      else
      {
         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_KIOSK_MODE_PASSWORD_NOK),
            1, 100, true);
      }
   }

   menu_input_dialog_end();
}

static void menu_input_st_string_cb_enable_settings(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      const char *label = menu_input_dialog_get_buffer();
      settings_t *settings = config_get_ptr();

      if (string_is_equal(label, settings->paths.menu_content_show_settings_password))
      {
         settings->bools.menu_content_show_settings = true;

         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK),
            1, 100, true);
      }
      else
      {
         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK),
            1, 100, true);
      }
   }

   menu_input_dialog_end();
}

static void menu_input_st_string_cb_save_preset(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      rarch_setting_t *setting = NULL;
      bool                 ret = false;
      const char        *label = menu_input_dialog_get_label_buffer();

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         setting_set_with_string_representation(setting, str);
         menu_setting_generic(setting, false);
      }
      else if (!string_is_empty(label))
         ret = menu_shader_manager_save_preset(str, false, false);

      if(ret)
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY),
               1, 100, true);
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_SAVING_SHADER_PRESET),
               1, 100, true);
   }

   menu_input_dialog_end();
}

static void menu_input_st_string_cb_cheat_file_save_as(
      void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t *setting = NULL;
      settings_t *settings     = config_get_ptr();
      const char        *label = menu_input_dialog_get_label_buffer();

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         setting_set_with_string_representation(setting, str);
         menu_setting_generic(setting, false);
      }
      else if (!string_is_empty(label))
         cheat_manager_save(str, settings->paths.path_cheat_database,false);
   }

   menu_input_dialog_end();
}

#define default_action_dialog_start(funcname, _label, _idx, _cb) \
static int (funcname)(const char *path, const char *label_setting, unsigned type, size_t idx, size_t entry_idx) \
{ \
   menu_input_ctx_line_t line; \
   line.label         = _label; \
   line.label_setting = label_setting; \
   line.type          = type; \
   line.idx           = (_idx); \
   line.cb            = _cb; \
   if (!menu_input_dialog_start(&line)) \
      return -1; \
   return 0; \
}

default_action_dialog_start(action_ok_shader_preset_save_as,
   msg_hash_to_str(MSG_INPUT_PRESET_FILENAME),
   (unsigned)idx,
   menu_input_st_string_cb_save_preset)
default_action_dialog_start(action_ok_enable_settings,
   msg_hash_to_str(MSG_INPUT_ENABLE_SETTINGS_PASSWORD),
   (unsigned)entry_idx,
   menu_input_st_string_cb_enable_settings)
default_action_dialog_start(action_ok_wifi,
   "Passphrase",
   (unsigned)idx,
   menu_input_wifi_cb)
default_action_dialog_start(action_ok_cheat_file_save_as,
   msg_hash_to_str(MSG_INPUT_CHEAT_FILENAME),
   (unsigned)idx,
   menu_input_st_string_cb_cheat_file_save_as)
default_action_dialog_start(action_ok_disable_kiosk_mode,
   msg_hash_to_str(MSG_INPUT_KIOSK_MODE_PASSWORD),
   (unsigned)entry_idx,
   menu_input_st_string_cb_disable_kiosk_mode)
default_action_dialog_start(action_ok_rename_entry,
   msg_hash_to_str(MSG_INPUT_RENAME_ENTRY),
   (unsigned)entry_idx,
   menu_input_st_string_cb_rename_entry)

enum
{
   ACTION_OK_SHADER_PRESET_SAVE_CORE = 0,
   ACTION_OK_SHADER_PRESET_SAVE_GAME,
   ACTION_OK_SHADER_PRESET_SAVE_PARENT
};

static int generic_action_ok_shader_preset_save(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   char directory[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH];
   settings_t *settings            = config_get_ptr();
   const char *core_name           = NULL;
   rarch_system_info_t *info       = runloop_get_system_info();

   directory[0] = file[0] = tmp[0] = '\0';

   if (info)
      core_name           = info->info.library_name;

   if (!string_is_empty(core_name))
   {
      fill_pathname_join(
            tmp,
            settings->paths.directory_video_shader,
            "presets",
            sizeof(tmp));
      fill_pathname_join(
            directory,
            tmp,
            core_name,
            sizeof(directory));
   }
   if (!filestream_exists(directory))
       path_mkdir(directory);

   switch (action_type)
   {
      case ACTION_OK_SHADER_PRESET_SAVE_CORE:
         if (!string_is_empty(core_name))
            fill_pathname_join(file, directory, core_name, sizeof(file));
         break;
      case ACTION_OK_SHADER_PRESET_SAVE_GAME:
         {
            const char *game_name = path_basename(path_get(RARCH_PATH_BASENAME));
            fill_pathname_join(file, directory, game_name, sizeof(file));
         }
         break;
      case ACTION_OK_SHADER_PRESET_SAVE_PARENT:
         {
            fill_pathname_parent_dir_name(tmp, path_get(RARCH_PATH_BASENAME), sizeof(tmp));
            fill_pathname_join(file, directory, tmp, sizeof(file));
         }
         break;
   }

   if(menu_shader_manager_save_preset(file, false, true))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY),
            1, 100, true);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_SAVING_SHADER_PRESET),
            1, 100, true);

   return 0;
}

static int action_ok_shader_preset_save_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_save(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_SAVE_CORE);
}

static int action_ok_shader_preset_save_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_save(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_SAVE_GAME);
}

static int action_ok_shader_preset_save_parent(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_save(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_SAVE_PARENT);
}

static int generic_action_ok_remap_file_operation(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   char directory[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   char content_dir[PATH_MAX_LENGTH];
   settings_t *settings            = config_get_ptr();
   const char *core_name           = NULL;
   rarch_system_info_t *info       = runloop_get_system_info();

   directory[0] = file[0]          = '\0';

   if (info)
      core_name           = info->info.library_name;

   if (!string_is_empty(core_name))
      fill_pathname_join(
            directory,
            settings->paths.directory_input_remapping,
            core_name,
            sizeof(directory));

   switch (action_type)
   {
      case ACTION_OK_REMAP_FILE_SAVE_CORE:
      case ACTION_OK_REMAP_FILE_REMOVE_CORE:
         if (!string_is_empty(core_name))
            fill_pathname_join(file, core_name, core_name, sizeof(file));
         break;
      case ACTION_OK_REMAP_FILE_SAVE_GAME:
      case ACTION_OK_REMAP_FILE_REMOVE_GAME:
         if (!string_is_empty(core_name))
            fill_pathname_join(file, core_name,
                  path_basename(path_get(RARCH_PATH_BASENAME)), sizeof(file));
         break;
      case ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR:
      case ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR:
         if (!string_is_empty(core_name))
         {
            fill_pathname_parent_dir_name(content_dir, path_get(RARCH_PATH_BASENAME), sizeof(content_dir));
            fill_pathname_join(file, core_name,
                  content_dir, sizeof(file));
         }
         break;
   }

   if (!filestream_exists(directory))
       path_mkdir(directory);

   if (action_type < ACTION_OK_REMAP_FILE_REMOVE_CORE)
   {
      if(input_remapping_save_file(file))
      {
         if (action_type == ACTION_OK_REMAP_FILE_SAVE_CORE)
            rarch_ctl(RARCH_CTL_SET_REMAPS_CORE_ACTIVE, NULL);
         else if (action_type == ACTION_OK_REMAP_FILE_SAVE_GAME)
            rarch_ctl(RARCH_CTL_SET_REMAPS_GAME_ACTIVE, NULL);
         else if (action_type == ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR)
            rarch_ctl(RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE, NULL);

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_REMAP_FILE_SAVED_SUCCESSFULLY),
               1, 100, true);
      }
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_SAVING_REMAP_FILE),
               1, 100, true);
   }
   else
   {
      if(input_remapping_remove_file(file))
      {
         if (action_type == ACTION_OK_REMAP_FILE_REMOVE_CORE &&
               rarch_ctl(RARCH_CTL_IS_REMAPS_CORE_ACTIVE, NULL))
         {
            rarch_ctl(RARCH_CTL_UNSET_REMAPS_CORE_ACTIVE, NULL);
            input_remapping_set_defaults(true);
         }

         else if (action_type == ACTION_OK_REMAP_FILE_REMOVE_GAME &&
               rarch_ctl(RARCH_CTL_IS_REMAPS_GAME_ACTIVE, NULL))
         {
            rarch_ctl(RARCH_CTL_UNSET_REMAPS_GAME_ACTIVE, NULL);
            input_remapping_set_defaults(true);
         }

         else if (action_type == ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR &&
               rarch_ctl(RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE, NULL))
         {
            rarch_ctl(RARCH_CTL_UNSET_REMAPS_CONTENT_DIR_ACTIVE, NULL);
            input_remapping_set_defaults(true);
         }

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_REMAP_FILE_REMOVED_SUCCESSFULLY),
               1, 100, true);
      }
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_REMOVING_REMAP_FILE),
               1, 100, true);
   }
   return 0;
}

static int action_ok_remap_file_save_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_CORE);
}

static int action_ok_remap_file_save_content_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR);
}

static int action_ok_remap_file_save_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_GAME);
}

static int action_ok_remap_file_remove_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_REMOVE_CORE);
}

static int action_ok_remap_file_remove_content_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR);
}

static int action_ok_remap_file_remove_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_REMOVE_GAME);
}

int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filebrowser_clear_type();
   return generic_action_ok(NULL, label, type, idx, entry_idx,
         ACTION_OK_SET_DIRECTORY, MSG_UNKNOWN);
}

#ifdef HAVE_LIBRETRODB
static int action_ok_scan_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_scan_file(path, label, type, idx);
}

static int action_ok_path_scan_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_scan_directory(NULL, label, type, idx);
}
#endif

static int action_ok_core_deferred_set(const char *new_core_path,
      const char *content_label, unsigned type, size_t idx, size_t entry_idx)
{
   char ext_name[255];
   char core_display_name[PATH_MAX_LENGTH];
   settings_t *settings                    = config_get_ptr();
   menu_handle_t            *menu          = NULL;
   size_t selection                        = menu_navigation_get_selection();

   ext_name[0]                             = '\0';

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
      return menu_cbs_exit();

   core_display_name[0] = '\0';

   core_info_get_name(new_core_path,
         core_display_name, sizeof(core_display_name),
         settings->paths.path_libretro_info,
         settings->paths.directory_libretro,
         ext_name,
         settings->bools.show_hidden_files);
   command_playlist_update_write(
         NULL,
         menu->scratchpad.unsigned_var,
         NULL,
         content_label,
         new_core_path,
         core_display_name,
         NULL,
         NULL);

   menu_entries_pop_stack(&selection, 0, 1);
   menu_navigation_set_selection(selection);

   return menu_cbs_exit();
}

static int action_ok_deferred_list_stub(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}

static int action_ok_load_core_deferred(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   content_ctx_info_t content_info;
   menu_handle_t *menu                 = NULL;

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   if (!task_push_load_content_with_new_core_from_menu(
            path, menu->deferred_path,
            &content_info,
            CORE_TYPE_PLAIN,
            NULL, NULL))
      return -1;
   content_add_to_playlist(path);

   return 0;
}

#define default_action_ok_start_builtin_core(funcname, _id) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   content_ctx_info_t content_info; \
   content_info.argc                   = 0; \
   content_info.argv                   = NULL; \
   content_info.args                   = NULL; \
   content_info.environ_get            = NULL; \
   if (!task_push_start_builtin_core(&content_info, _id, NULL, NULL)) \
      return -1; \
   return 0; \
}

default_action_ok_start_builtin_core(action_ok_start_net_retropad_core, CORE_TYPE_NETRETROPAD)
default_action_ok_start_builtin_core(action_ok_start_video_processor_core, CORE_TYPE_VIDEO_PROCESSOR)

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
static int action_ok_file_load_ffmpeg(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char new_path[PATH_MAX_LENGTH];
   const char *menu_path           = NULL;
   file_list_t *menu_stack         = menu_entries_get_menu_stack_ptr(0);

   file_list_get_last(menu_stack, &menu_path, NULL, NULL, NULL);

   new_path[0] = '\0';

   if (!string_is_empty(menu_path))
      fill_pathname_join(new_path, menu_path, path,
            sizeof(new_path));

   /* TODO/FIXME - should become runtime optional */
#ifdef HAVE_MPV
   return default_action_ok_load_content_with_core_from_menu(new_path, CORE_TYPE_MPV);
#else
   return default_action_ok_load_content_with_core_from_menu(new_path, CORE_TYPE_FFMPEG);
#endif
}
#endif

static int action_ok_audio_run(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char combined_path[PATH_MAX_LENGTH];
   menu_handle_t *menu                 = NULL;

   combined_path[0] = '\0';

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   fill_pathname_join(combined_path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(combined_path));

   /* TODO/FIXME - should become runtime optional */
#ifdef HAVE_MPV
   return default_action_ok_load_content_with_core_from_menu(combined_path, CORE_TYPE_MPV);
#else
   return default_action_ok_load_content_with_core_from_menu(combined_path, CORE_TYPE_FFMPEG);
#endif
}

static int action_ok_cheat_reload_cheats(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   bool          refresh = false ;
   cheat_manager_realloc(0, CHEAT_HANDLER_TYPE_EMU);
   cheat_manager_load_game_specific_cheats() ;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   return 0 ;
}

   static int action_ok_cheat_add_top(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   struct item_cheat tmp;
   char msg[256] ;
   bool          refresh = false ;
   unsigned int new_size = cheat_manager_get_size() + 1;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.size-1], sizeof(struct item_cheat )) ;
   tmp.idx = 0 ;

   for (i = cheat_manager_state.size-2 ; i >=0 ; i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1],
            &cheat_manager_state.cheats[i], sizeof(struct item_cheat )) ;
      cheat_manager_state.cheats[i+1].idx++ ;
   }

   memcpy(&cheat_manager_state.cheats[0], &tmp, sizeof(struct item_cheat )) ;

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_ADD_TOP_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true);

   return 0 ;
}

static int action_ok_cheat_add_bottom(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char msg[256];
   bool refresh = false ;
   unsigned int new_size = cheat_manager_get_size() + 1;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   msg[0] = '\0';
   strlcpy(msg,
         msg_hash_to_str(MSG_CHEAT_ADD_BOTTOM_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true);

   return 0 ;
}

static int action_ok_cheat_delete_all(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char msg[256] ;
   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_DELETE_ALL_INSTRUCTIONS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 240, true);
   return 0 ;
}

static int action_ok_cheat_add_new_after(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   char msg[256];
   struct item_cheat tmp;
   bool refresh = false;
   unsigned int new_size = cheat_manager_get_size() + 1;

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.size-1], sizeof(struct item_cheat )) ;
   tmp.idx = cheat_manager_state.working_cheat.idx+1 ;

   for (i = cheat_manager_state.size-2 ; i >= (int)(cheat_manager_state.working_cheat.idx+1); i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat )) ;
      cheat_manager_state.cheats[i+1].idx++ ;
   }

   memcpy(&cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx+1], &tmp, sizeof(struct item_cheat )) ;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_ADD_AFTER_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true);

   return 0 ;
}

static int action_ok_cheat_add_new_before(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   char msg[256] ;
   struct item_cheat tmp ;
   bool refresh = false ;
   unsigned int new_size = cheat_manager_get_size() + 1;

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.size-1], sizeof(struct item_cheat )) ;
   tmp.idx = cheat_manager_state.working_cheat.idx ;

   for (i = cheat_manager_state.size-2 ; i >=(int)tmp.idx ; i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat )) ;
      cheat_manager_state.cheats[i+1].idx++ ;
   }

   memcpy(&cheat_manager_state.cheats[tmp.idx],
         &tmp, sizeof(struct item_cheat )) ;
   memcpy(&cheat_manager_state.working_cheat,
         &tmp, sizeof(struct item_cheat )) ;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_ADD_BEFORE_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true);

   return 0 ;
}

static int action_ok_cheat_copy_before(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   struct item_cheat tmp ;
   char msg[256] ;
   bool refresh = false ;
   unsigned int new_size = cheat_manager_get_size() + 1;
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx], sizeof(struct item_cheat )) ;
   tmp.idx = cheat_manager_state.working_cheat.idx ;
   if ( tmp.code != NULL )
      tmp.code = strdup(tmp.code) ;
   if ( tmp.desc != NULL )
      tmp.desc = strdup(tmp.desc) ;

   for (i = cheat_manager_state.size-2 ; i >=(int)tmp.idx ; i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat )) ;
      cheat_manager_state.cheats[i+1].idx++ ;
   }

   memcpy(&cheat_manager_state.cheats[tmp.idx], &tmp, sizeof(struct item_cheat )) ;
   memcpy(&cheat_manager_state.working_cheat, &tmp, sizeof(struct item_cheat )) ;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_COPY_BEFORE_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true);


   return 0 ;
}

static int action_ok_cheat_copy_after(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   struct item_cheat tmp;
   char msg[256];
   bool          refresh = false ;
   unsigned int new_size = cheat_manager_get_size() + 1;

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx], sizeof(struct item_cheat )) ;
   tmp.idx = cheat_manager_state.working_cheat.idx+1 ;
   if ( tmp.code != NULL )
      tmp.code = strdup(tmp.code) ;
   if ( tmp.desc != NULL )
      tmp.desc = strdup(tmp.desc) ;

   for (i = cheat_manager_state.size-2 ; i >= (int)(cheat_manager_state.working_cheat.idx+1); i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat )) ;
      cheat_manager_state.cheats[i+1].idx++ ;
   }

   memcpy(&cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx+1], &tmp, sizeof(struct item_cheat )) ;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_COPY_AFTER_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true);

   return 0 ;
}

static int action_ok_cheat_delete(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t new_selection_ptr;
   char msg[256];
   unsigned int new_size = cheat_manager_get_size() - 1;

   if( new_size >0 )
   {
      unsigned i;
      if ( cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].code != NULL )
      {
         free(cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].code) ;
         cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].code = NULL ;
      }
      if ( cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].desc != NULL )
      {
         free(cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].desc) ;
         cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].desc = NULL ;
      }
      for (i = cheat_manager_state.working_cheat.idx ; i <cheat_manager_state.size-1  ; i++)
      {
         memcpy(&cheat_manager_state.cheats[i], &cheat_manager_state.cheats[i+1], sizeof(struct item_cheat )) ;
         cheat_manager_state.cheats[i].idx-- ;
      }
      cheat_manager_state.cheats[cheat_manager_state.size-1].code = NULL ;
      cheat_manager_state.cheats[cheat_manager_state.size-1].desc = NULL ;
   }

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_DELETE_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true);

   new_selection_ptr = menu_navigation_get_selection();
   menu_entries_pop_stack(&new_selection_ptr, 0, 1);
   menu_navigation_set_selection(new_selection_ptr);

   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_PATH, NULL);
   menu_driver_ctl(RARCH_MENU_CTL_UPDATE_SAVESTATE_THUMBNAIL_IMAGE, NULL);

   return 0;
}

static int action_ok_file_load_imageviewer(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char fullpath[PATH_MAX_LENGTH];
   const char *menu_path           = NULL;
   file_list_t *menu_stack         = menu_entries_get_menu_stack_ptr(0);

   file_list_get_last(menu_stack, &menu_path, NULL, NULL, NULL);

   fullpath[0] = '\0';

   if (!string_is_empty(menu_path))
      fill_pathname_join(fullpath, menu_path, path,
            sizeof(fullpath));

   return default_action_ok_load_content_with_core_from_menu(fullpath, CORE_TYPE_IMAGEVIEWER);
}

static int action_ok_file_load_current_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu                 = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   return default_action_ok_load_content_with_core_from_menu(
         menu->detect_content_path, CORE_TYPE_PLAIN);
}

static int action_ok_file_load_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   content_ctx_info_t content_info;
   menu_handle_t *menu                 = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   if (!task_push_load_content_with_new_core_from_menu(
            path, menu->detect_content_path,
            &content_info,
            CORE_TYPE_PLAIN,
            NULL, NULL))
      return -1;
   content_add_to_playlist(menu->detect_content_path);

   return 0;
}


static int action_ok_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_LOAD_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_SAVE_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_cheevos_toggle_hardcore_mode(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_CHEEVOS
   cheevos_hardcore_paused = !cheevos_hardcore_paused;
#endif
   generic_action_ok_command(CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE);
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_undo_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_UNDO_LOAD_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_undo_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_UNDO_SAVE_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

#ifdef HAVE_NETWORKING

#ifdef HAVE_ZLIB
static void cb_decompressed(void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (dec && !err)
   {
      unsigned type_hash = (unsigned)(uintptr_t)user_data;

      switch (type_hash)
      {
         case CB_CORE_UPDATER_DOWNLOAD:
            generic_action_ok_command(CMD_EVENT_CORE_INFO_INIT);
            break;
         case CB_UPDATE_ASSETS:
            generic_action_ok_command(CMD_EVENT_REINIT);
            break;
      }
   }

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      if (filestream_exists(dec->source_file))
         filestream_delete(dec->source_file);

      free(dec->source_file);
      free(dec);
   }
}
#endif

static int generic_action_ok_network(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums enum_idx)
{
   char url_path[PATH_MAX_LENGTH];
   settings_t *settings           = config_get_ptr();
   unsigned type_id2              = 0;
   file_transfer_t *transf   = NULL;
   const char *url_label          = NULL;
   retro_task_callback_t callback = NULL;
   bool refresh                   = true;
   bool suppress_msg              = false;

   url_path[0] = '\0';

   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DIRS_LIST:

         if (string_is_empty(settings->paths.network_buildbot_assets_url))
            return menu_cbs_exit();

         fill_pathname_join(url_path,
               settings->paths.network_buildbot_assets_url,
               "cores/.index-dirs", sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_CORE_CONTENT_DIRS_LIST;
         callback  = cb_net_generic;
         suppress_msg = true;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST:
         fill_pathname_join(url_path, path,
               file_path_str(FILE_PATH_INDEX_URL), sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_CORE_CONTENT_LIST;
         callback  = cb_net_generic;
         suppress_msg = true;
         break;
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_LIST:

         if (string_is_empty(settings->paths.network_buildbot_url))
            return menu_cbs_exit();

         fill_pathname_join(url_path, settings->paths.network_buildbot_url,
               file_path_str(FILE_PATH_INDEX_EXTENDED_URL), sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_CORE_UPDATER_LIST;
         callback  = cb_net_generic;
         break;
      case MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST:
         fill_pathname_join(url_path,
               file_path_str(FILE_PATH_CORE_THUMBNAILS_URL),
               file_path_str(FILE_PATH_INDEX_URL), sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_THUMBNAILS_UPDATER_LIST;
         callback  = cb_net_generic;
         break;
#ifdef HAVE_LAKKA
      case MENU_ENUM_LABEL_CB_LAKKA_LIST:
         /* TODO unhardcode this path */
         fill_pathname_join(url_path,
               file_path_str(FILE_PATH_LAKKA_URL),
               lakka_get_project(), sizeof(url_path));
         fill_pathname_join(url_path, url_path,
               file_path_str(FILE_PATH_INDEX_URL),
               sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_LAKKA_LIST;
         callback  = cb_net_generic;
         break;
#endif
      default:
         break;
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   generic_action_ok_command(CMD_EVENT_NETWORK_INIT);

   transf           = (file_transfer_t*)calloc(1, sizeof(*transf));
   strlcpy(transf->path, url_path, sizeof(transf->path));

   task_push_http_transfer(url_path, suppress_msg, url_label, callback, transf);

   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx, type_id2);
}

#define default_action_ok_list(funcname, _id) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_network(path, label, type, idx, entry_idx, _id); \
}

default_action_ok_list(action_ok_core_content_list, MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST)
default_action_ok_list(action_ok_core_content_dirs_list, MENU_ENUM_LABEL_CB_CORE_CONTENT_DIRS_LIST)
default_action_ok_list(action_ok_core_updater_list, MENU_ENUM_LABEL_CB_CORE_UPDATER_LIST)
default_action_ok_list(action_ok_thumbnails_updater_list, MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST)
default_action_ok_list(action_ok_lakka_list, MENU_ENUM_LABEL_CB_LAKKA_LIST)

static void cb_generic_dir_download(void *task_data,
      void *user_data, const char *err)
{
   file_transfer_t     *transf      = (file_transfer_t*)user_data;
   if (transf)
   {
      generic_action_ok_network(transf->path, transf->path, 0, 0, 0,
            MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST);

      free(transf);
   }
}

/* expects http_transfer_t*, file_transfer_t* */
static void cb_generic_download(void *task_data,
      void *user_data, const char *err)
{
   char output_path[PATH_MAX_LENGTH];
#if defined(HAVE_COMPRESSION) && defined(HAVE_ZLIB)
   bool extract                          = true;
#endif
   const char             *dir_path      = NULL;
   file_transfer_t     *transf      = (file_transfer_t*)user_data;
   settings_t              *settings     = config_get_ptr();
   http_transfer_data_t        *data     = (http_transfer_data_t*)task_data;

   if (!data || !data->data | !transf)
      goto finish;

   output_path[0] = '\0';

   /* we have to determine dir_path at the time of writting or else
    * we'd run into races when the user changes the setting during an
    * http transfer. */
   switch (transf->enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         dir_path = settings->paths.directory_thumbnails;
         break;
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD:
         dir_path = settings->paths.directory_libretro;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         dir_path = settings->paths.directory_core_assets;
#if defined(HAVE_COMPRESSION) && defined(HAVE_ZLIB)
         extract = settings->bools.network_buildbot_auto_extract_archive;
#endif
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         dir_path = settings->paths.path_libretro_info;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         dir_path = settings->paths.directory_assets;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         dir_path = settings->paths.directory_autoconfig;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         dir_path = settings->paths.path_content_database;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         dir_path = settings->paths.directory_overlay;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         dir_path = settings->paths.path_cheat_database;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG:
         {
            static char shaderdir[PATH_MAX_LENGTH]       = {0};
            const char *dirname                          = NULL;

            if (transf->enum_idx == MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG)
               dirname                                   = "shaders_cg";
            else if (transf->enum_idx == MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL)
               dirname                                   = "shaders_glsl";
            else if (transf->enum_idx == MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG)
               dirname                                   = "shaders_slang";

            fill_pathname_join(shaderdir,
                  settings->paths.directory_video_shader,
                  dirname,
                  sizeof(shaderdir));

            if (!filestream_exists(shaderdir) && !path_mkdir(shaderdir))
               goto finish;

            dir_path = shaderdir;
         }
         break;
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
         dir_path = LAKKA_UPDATE_DIR;
         break;
      default:
         RARCH_WARN("Unknown transfer type '%s' bailing out.\n",
               msg_hash_to_str(transf->enum_idx));
         break;
   }

   if (!string_is_empty(dir_path))
      fill_pathname_join(output_path, dir_path,
            transf->path, sizeof(output_path));

   /* Make sure the directory exists */
   path_basedir_wrapper(output_path);

   if (!path_mkdir(output_path))
   {
      err = msg_hash_to_str(MSG_FAILED_TO_CREATE_THE_DIRECTORY);
      goto finish;
   }

   if (!string_is_empty(dir_path))
      fill_pathname_join(output_path, dir_path,
            transf->path, sizeof(output_path));

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(output_path))
   {
      if (task_check_decompress(output_path))
      {
        err = msg_hash_to_str(MSG_DECOMPRESSION_ALREADY_IN_PROGRESS);
        goto finish;
      }
   }
#endif

   if (!filestream_write_file(output_path, data->data, data->len))
   {
      err = "Write failed.";
      goto finish;
   }

#if defined(HAVE_COMPRESSION) && defined(HAVE_ZLIB)
   if (!extract)
      goto finish;

   if (path_is_compressed_file(output_path))
   {
      if (!task_push_decompress(output_path, dir_path,
               NULL, NULL, NULL,
               cb_decompressed, (void*)(uintptr_t)
               msg_hash_calculate(msg_hash_to_str(transf->enum_idx))))
      {
        err = msg_hash_to_str(MSG_DECOMPRESSION_FAILED);
        goto finish;
      }
   }
#else
   switch (transf->enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD:
         generic_action_ok_command(CMD_EVENT_CORE_INFO_INIT);
         break;
      default:
         break;
   }
#endif

finish:
   if (err)
   {
      RARCH_ERR("Download of '%s' failed: %s\n",
            (transf ? transf->path: "unknown"), err);
   }

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (transf)
      free(transf);
}
#endif


static int action_ok_download_generic(const char *path,
      const char *label, const char *menu_label,
      unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums enum_idx)
{
#ifdef HAVE_NETWORKING
   char s[PATH_MAX_LENGTH];
   char s2[PATH_MAX_LENGTH];
   char s3[PATH_MAX_LENGTH];
   file_transfer_t *transf = NULL;
   settings_t *settings         = config_get_ptr();
   bool suppress_msg            = false;
   retro_task_callback_t cb     = cb_generic_download;

   s[0] = s2[0] = s3[0] = '\0';

   fill_pathname_join(s,
         settings->paths.network_buildbot_assets_url,
         "frontend", sizeof(s));

   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CB_DOWNLOAD_URL:
         suppress_msg = true;
         fill_pathname_join(s, label,
               path, sizeof(s));
         path = s;
         cb = cb_generic_dir_download;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         {
            struct string_list *str_list = string_split(menu_label, ";");
            strlcpy(s, str_list->elems[0].data, sizeof(s));
            string_list_free(str_list);
         }
         break;
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
#ifdef HAVE_LAKKA
         /* TODO unhardcode this path*/
         fill_pathname_join(s, file_path_str(FILE_PATH_LAKKA_URL),
               lakka_get_project(), sizeof(s));
#endif
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         path = file_path_str(FILE_PATH_ASSETS_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         path = file_path_str(FILE_PATH_AUTOCONFIG_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         path = file_path_str(FILE_PATH_CORE_INFO_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         path = file_path_str(FILE_PATH_CHEATS_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         path = file_path_str(FILE_PATH_OVERLAYS_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         path = file_path_str(FILE_PATH_DATABASE_RDB_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
         path = file_path_str(FILE_PATH_SHADERS_GLSL_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG:
         path = file_path_str(FILE_PATH_SHADERS_SLANG_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
         path = file_path_str(FILE_PATH_SHADERS_CG_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         strlcpy(s, file_path_str(FILE_PATH_CORE_THUMBNAILS_URL), sizeof(s));
         break;
      default:
         strlcpy(s, settings->paths.network_buildbot_url, sizeof(s));
         break;
   }

   fill_pathname_join(s2, s, path, sizeof(s2));

   transf           = (file_transfer_t*)calloc(1, sizeof(*transf));
   transf->enum_idx = enum_idx;
   strlcpy(transf->path, path, sizeof(transf->path));

   net_http_urlencode_full(s3, s2, sizeof(s));

   task_push_http_transfer(s3, suppress_msg, msg_hash_to_str(enum_idx), cb, transf);
#endif
   return 0;
}

static int action_ok_core_content_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path   = NULL;
   const char *menu_label  = NULL;
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, &enum_idx, NULL);

   return action_ok_download_generic(path, label,
         menu_path, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD);
}

#define default_action_ok_download(funcname, _id) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,_id); \
}

default_action_ok_download(action_ok_core_content_thumbnails, MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD)
default_action_ok_download(action_ok_thumbnails_updater_download, MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_DOWNLOAD)
default_action_ok_download(action_ok_download_url, MENU_ENUM_LABEL_CB_DOWNLOAD_URL)
default_action_ok_download(action_ok_core_updater_download, MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD)
default_action_ok_download(action_ok_lakka_download, MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD)
default_action_ok_download(action_ok_update_assets, MENU_ENUM_LABEL_CB_UPDATE_ASSETS)
default_action_ok_download(action_ok_update_core_info_files, MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES)
default_action_ok_download(action_ok_update_overlays, MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS)
default_action_ok_download(action_ok_update_shaders_cg, MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG)
default_action_ok_download(action_ok_update_shaders_glsl, MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL)
default_action_ok_download(action_ok_update_shaders_slang, MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG)
default_action_ok_download(action_ok_update_databases, MENU_ENUM_LABEL_CB_UPDATE_DATABASES)
default_action_ok_download(action_ok_update_cheats, MENU_ENUM_LABEL_CB_UPDATE_CHEATS)
default_action_ok_download(action_ok_update_autoconfig_profiles, MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES)

/* creates folder and core options stub file for subsequent runs */
static int action_ok_option_create(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char game_path[PATH_MAX_LENGTH];
   config_file_t *conf             = NULL;

   game_path[0] = '\0';

   if (!retroarch_validate_game_options(game_path, sizeof(game_path), true))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE),
            1, 100, true);
      return 0;
   }

   conf = config_file_new(game_path);

   if (!conf)
   {
      conf = config_file_new(NULL);
      if (!conf)
         return false;
   }

   if (config_file_write(conf, game_path))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY),
            1, 100, true);
      path_set(RARCH_PATH_CORE_OPTIONS, game_path);
   }
   config_file_free(conf);

   return 0;
}

int action_ok_close_content(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   /* This line resets the navigation pointer so the active entry will be "Run" */
   menu_navigation_set_selection(0);
   return generic_action_ok_command(CMD_EVENT_UNLOAD_CORE);
}

#define default_action_ok_cmd_func(func_name, cmd) \
int (func_name)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_command(cmd); \
}

default_action_ok_cmd_func(action_ok_cheat_apply_changes,      CMD_EVENT_CHEATS_APPLY)
default_action_ok_cmd_func(action_ok_quit,                     CMD_EVENT_QUIT)
default_action_ok_cmd_func(action_ok_save_new_config,          CMD_EVENT_MENU_SAVE_CONFIG)
default_action_ok_cmd_func(action_ok_resume_content,           CMD_EVENT_RESUME)
default_action_ok_cmd_func(action_ok_restart_content,          CMD_EVENT_RESET)
default_action_ok_cmd_func(action_ok_screenshot,               CMD_EVENT_TAKE_SCREENSHOT)
default_action_ok_cmd_func(action_ok_disk_cycle_tray_status,   CMD_EVENT_DISK_EJECT_TOGGLE)
default_action_ok_cmd_func(action_ok_shader_apply_changes,     CMD_EVENT_SHADERS_APPLY_CHANGES)
default_action_ok_cmd_func(action_ok_show_wimp,                CMD_EVENT_UI_COMPANION_TOGGLE)

static int action_ok_reset_core_association(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *tmp_path                = NULL;
   menu_handle_t *menu                 = NULL;
   playlist_t *tmp_playlist            = playlist_get_cached();

   if (!tmp_playlist)
      return 0;
   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   playlist_get_index(tmp_playlist,
         menu->rpl_entry_selection_ptr,
         &tmp_path, NULL, NULL, NULL, NULL, NULL);

   if (!command_event(CMD_EVENT_RESET_CORE_ASSOCIATION,
            (void *)&menu->rpl_entry_selection_ptr))
      return menu_cbs_exit();
   return 0;
}

static int action_ok_add_to_favorites(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   void *new_path = (void*)path_get(RARCH_PATH_CONTENT);
   if (!command_event(CMD_EVENT_ADD_TO_FAVORITES, new_path))
      return menu_cbs_exit();
   return 0;
}

static int action_ok_add_to_favorites_playlist(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *tmp_path                = NULL;
   menu_handle_t *menu                 = NULL;
   playlist_t *tmp_playlist            = playlist_get_cached();

   if (!tmp_playlist)
      return 0;
   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   playlist_get_index(tmp_playlist,
         menu->rpl_entry_selection_ptr, &tmp_path,
         NULL, NULL, NULL, NULL, NULL);

   if (!command_event(CMD_EVENT_ADD_TO_FAVORITES, (void*)tmp_path))
      return menu_cbs_exit();
   return 0;

}

static int action_ok_delete_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t new_selection_ptr;
   char *conf_path           = NULL;
   char *def_conf_path       = NULL;
   char *def_conf_music_path = NULL;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   char *def_conf_video_path = NULL;
#endif
#ifdef HAVE_IMAGEVIEWER
   char *def_conf_img_path   = NULL;
#endif
   menu_handle_t *menu       = NULL;
   playlist_t *playlist      = playlist_get_cached();

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   conf_path                 = playlist_get_conf_path(playlist);
   def_conf_path             = playlist_get_conf_path(g_defaults.content_history);
   def_conf_music_path       = playlist_get_conf_path(g_defaults.music_history);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   def_conf_video_path       = playlist_get_conf_path(g_defaults.video_history);
#endif
#ifdef HAVE_IMAGEVIEWER
   def_conf_img_path         = playlist_get_conf_path(g_defaults.image_history);
#endif

   if (string_is_equal(conf_path, def_conf_path))
      playlist = g_defaults.content_history;
   else if (string_is_equal(conf_path, def_conf_music_path))
      playlist = g_defaults.music_history;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   else if (string_is_equal(conf_path, def_conf_video_path))
      playlist = g_defaults.video_history;
#endif
#ifdef HAVE_IMAGEVIEWER
   else if (string_is_equal(conf_path, def_conf_img_path))
      playlist = g_defaults.image_history;
#endif

   if (playlist)
   {
      playlist_delete_index(playlist, menu->rpl_entry_selection_ptr);
      playlist_write_file(playlist);
   }

   new_selection_ptr = menu_navigation_get_selection();
   menu_entries_pop_stack(&new_selection_ptr, 0, 1);
   menu_navigation_set_selection(new_selection_ptr);

   return 0;
}


static int action_ok_rdb_entry_submenu(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   union string_list_elem_attr attr;
   char new_label[PATH_MAX_LENGTH];
   char new_path[PATH_MAX_LENGTH];
   int ret                         = -1;
   char *rdb                       = NULL;
   int len                         = 0;
   struct string_list *str_list    = NULL;
   struct string_list *str_list2   = NULL;

   if (!label)
      return menu_cbs_exit();

   new_label[0] = new_path[0]      = '\0';

   str_list = string_split(label, "|");

   if (!str_list)
      goto end;

   str_list2 = string_list_new();
   if (!str_list2)
      goto end;

   /* element 0 : label
    * element 1 : value
    * element 2 : database path
    */

   attr.i = 0;

   len += strlen(str_list->elems[1].data) + 1;
   string_list_append(str_list2, str_list->elems[1].data, attr);

   len += strlen(str_list->elems[2].data) + 1;
   string_list_append(str_list2, str_list->elems[2].data, attr);

   rdb = (char*)calloc(len, sizeof(char));

   if (!rdb)
      goto end;

   string_list_join_concat(rdb, len, str_list2, "|");
   strlcpy(new_path, rdb, sizeof(new_path));

   fill_pathname_join_delim(new_label,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST),
         str_list->elems[0].data, '_',
         sizeof(new_label));

   ret = generic_action_ok_displaylist_push(new_path, NULL,
         new_label, type, idx, entry_idx,
         ACTION_OK_DL_RDB_ENTRY_SUBMENU);

end:
   if (rdb)
      free(rdb);
   if (str_list)
      string_list_free(str_list);
   if (str_list2)
      string_list_free(str_list2);

   return ret;
}

#define default_action_ok_func(func_name, lbl) \
int (func_name)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx, entry_idx, lbl); \
}

default_action_ok_func(action_ok_browse_url_start, ACTION_OK_DL_BROWSE_URL_START)
default_action_ok_func(action_ok_goto_favorites, ACTION_OK_DL_FAVORITES_LIST)
default_action_ok_func(action_ok_goto_images, ACTION_OK_DL_IMAGES_LIST)
default_action_ok_func(action_ok_goto_video, ACTION_OK_DL_VIDEO_LIST)
default_action_ok_func(action_ok_goto_music, ACTION_OK_DL_MUSIC_LIST)
default_action_ok_func(action_ok_shader_parameters, ACTION_OK_DL_SHADER_PARAMETERS)
default_action_ok_func(action_ok_parent_directory_push, ACTION_OK_DL_PARENT_DIRECTORY_PUSH)
default_action_ok_func(action_ok_directory_push, ACTION_OK_DL_DIRECTORY_PUSH)
default_action_ok_func(action_ok_configurations_list, ACTION_OK_DL_CONFIGURATIONS_LIST)
default_action_ok_func(action_ok_saving_list, ACTION_OK_DL_SAVING_SETTINGS_LIST)
default_action_ok_func(action_ok_network_list, ACTION_OK_DL_NETWORK_SETTINGS_LIST)
default_action_ok_func(action_ok_database_manager_list, ACTION_OK_DL_DATABASE_MANAGER_LIST)
default_action_ok_func(action_ok_wifi_list, ACTION_OK_DL_WIFI_SETTINGS_LIST)
default_action_ok_func(action_ok_cursor_manager_list, ACTION_OK_DL_CURSOR_MANAGER_LIST)
default_action_ok_func(action_ok_compressed_archive_push, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH)
default_action_ok_func(action_ok_compressed_archive_push_detect_core, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE)
default_action_ok_func(action_ok_logging_list, ACTION_OK_DL_LOGGING_SETTINGS_LIST)
default_action_ok_func(action_ok_frame_throttle_list, ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST)
default_action_ok_func(action_ok_rewind_list, ACTION_OK_DL_REWIND_SETTINGS_LIST)
default_action_ok_func(action_ok_cheat, ACTION_OK_DL_CHEAT_DETAILS_SETTINGS_LIST)
default_action_ok_func(action_ok_cheat_start_or_cont, ACTION_OK_DL_CHEAT_SEARCH_SETTINGS_LIST)
default_action_ok_func(action_ok_onscreen_display_list, ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST)
default_action_ok_func(action_ok_onscreen_notifications_list, ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST)
default_action_ok_func(action_ok_onscreen_overlay_list, ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST)
default_action_ok_func(action_ok_menu_list, ACTION_OK_DL_MENU_SETTINGS_LIST)
default_action_ok_func(action_ok_quick_menu_override_options, ACTION_OK_DL_QUICK_MENU_OVERRIDE_OPTIONS_LIST)
default_action_ok_func(action_ok_menu_views_list, ACTION_OK_DL_MENU_VIEWS_SETTINGS_LIST)
default_action_ok_func(action_ok_quick_menu_views_list, ACTION_OK_DL_QUICK_MENU_VIEWS_SETTINGS_LIST)
default_action_ok_func(action_ok_power_management_list, ACTION_OK_DL_POWER_MANAGEMENT_SETTINGS_LIST)
default_action_ok_func(action_ok_user_interface_list, ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST)
default_action_ok_func(action_ok_menu_file_browser_list, ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST)
default_action_ok_func(action_ok_retro_achievements_list, ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST)
default_action_ok_func(action_ok_updater_list, ACTION_OK_DL_UPDATER_SETTINGS_LIST)
default_action_ok_func(action_ok_lakka_services, ACTION_OK_DL_LAKKA_SERVICES_LIST)
default_action_ok_func(action_ok_user_list, ACTION_OK_DL_USER_SETTINGS_LIST)
default_action_ok_func(action_ok_netplay_sublist, ACTION_OK_DL_NETPLAY)
default_action_ok_func(action_ok_directory_list, ACTION_OK_DL_DIRECTORY_SETTINGS_LIST)
default_action_ok_func(action_ok_privacy_list, ACTION_OK_DL_PRIVACY_SETTINGS_LIST)
default_action_ok_func(action_ok_midi_list, ACTION_OK_DL_MIDI_SETTINGS_LIST)
default_action_ok_func(action_ok_rdb_entry, ACTION_OK_DL_RDB_ENTRY)
default_action_ok_func(action_ok_mixer_stream_actions, ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST)
default_action_ok_func(action_ok_browse_url_list, ACTION_OK_DL_BROWSE_URL_LIST)
default_action_ok_func(action_ok_core_list, ACTION_OK_DL_CORE_LIST)
default_action_ok_func(action_ok_cheat_file, ACTION_OK_DL_CHEAT_FILE)
default_action_ok_func(action_ok_cheat_file_append, ACTION_OK_DL_CHEAT_FILE_APPEND)
default_action_ok_func(action_ok_playlist_collection, ACTION_OK_DL_PLAYLIST_COLLECTION)
default_action_ok_func(action_ok_disk_image_append_list, ACTION_OK_DL_DISK_IMAGE_APPEND_LIST)
default_action_ok_func(action_ok_subsystem_add_list, ACTION_OK_DL_SUBSYSTEM_ADD_LIST)
default_action_ok_func(action_ok_subsystem_add_load, ACTION_OK_DL_SUBSYSTEM_LOAD)
default_action_ok_func(action_ok_record_configfile, ACTION_OK_DL_RECORD_CONFIGFILE)
default_action_ok_func(action_ok_remap_file, ACTION_OK_DL_REMAP_FILE)
default_action_ok_func(action_ok_shader_preset, ACTION_OK_DL_SHADER_PRESET)
default_action_ok_func(action_ok_push_generic_list, ACTION_OK_DL_GENERIC)
default_action_ok_func(action_ok_audio_dsp_plugin, ACTION_OK_DL_AUDIO_DSP_PLUGIN)
default_action_ok_func(action_ok_rpl_entry, ACTION_OK_DL_RPL_ENTRY)
default_action_ok_func(action_ok_open_archive_detect_core, ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE)
default_action_ok_func(action_ok_file_load_music, ACTION_OK_DL_MUSIC)
default_action_ok_func(action_ok_push_accounts_list, ACTION_OK_DL_ACCOUNTS_LIST)
default_action_ok_func(action_ok_push_driver_settings_list, ACTION_OK_DL_DRIVER_SETTINGS_LIST)
default_action_ok_func(action_ok_push_video_settings_list, ACTION_OK_DL_VIDEO_SETTINGS_LIST)
default_action_ok_func(action_ok_push_configuration_settings_list, ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST)
default_action_ok_func(action_ok_push_core_settings_list, ACTION_OK_DL_CORE_SETTINGS_LIST)
default_action_ok_func(action_ok_push_audio_settings_list, ACTION_OK_DL_AUDIO_SETTINGS_LIST)
default_action_ok_func(action_ok_push_audio_mixer_settings_list, ACTION_OK_DL_AUDIO_MIXER_SETTINGS_LIST)
default_action_ok_func(action_ok_push_input_settings_list, ACTION_OK_DL_INPUT_SETTINGS_LIST)
default_action_ok_func(action_ok_push_latency_settings_list, ACTION_OK_DL_LATENCY_SETTINGS_LIST)
default_action_ok_func(action_ok_push_recording_settings_list, ACTION_OK_DL_RECORDING_SETTINGS_LIST)
default_action_ok_func(action_ok_push_playlist_settings_list, ACTION_OK_DL_PLAYLIST_SETTINGS_LIST)
default_action_ok_func(action_ok_push_input_hotkey_binds_list, ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST)
default_action_ok_func(action_ok_push_user_binds_list, ACTION_OK_DL_USER_BINDS_LIST)
default_action_ok_func(action_ok_push_accounts_cheevos_list, ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST)
default_action_ok_func(action_ok_open_archive, ACTION_OK_DL_OPEN_ARCHIVE)

static int action_ok_shader_pass(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu       = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu->scratchpad.unsigned_var = type - MENU_SETTINGS_SHADER_PASS_0;
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_SHADER_PASS);
}

static int action_ok_netplay_connect_room(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_NETWORKING
   char tmp_hostname[4115];

   tmp_hostname[0] = '\0';

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      generic_action_ok_command(CMD_EVENT_NETPLAY_DEINIT);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

   if (netplay_room_list[idx - 3].host_method == NETPLAY_HOST_METHOD_MITM)
   {
      snprintf(tmp_hostname,
            sizeof(tmp_hostname),
            "%s|%d",
         netplay_room_list[idx - 3].mitm_address,
         netplay_room_list[idx - 3].mitm_port);
   }
   else
   {
      snprintf(tmp_hostname,
            sizeof(tmp_hostname),
            "%s|%d",
         netplay_room_list[idx - 3].address,
         netplay_room_list[idx - 3].port);
   }

#if 0
   RARCH_LOG("[lobby] connecting to: %s with game: %s/%08x\n",
         tmp_hostname,
         netplay_room_list[idx - 3].gamename,
         netplay_room_list[idx - 3].gamecrc);
#endif

   task_push_netplay_crc_scan(netplay_room_list[idx - 3].gamecrc,
      netplay_room_list[idx - 3].gamename,
      tmp_hostname, netplay_room_list[idx - 3].corename);

#else
   return -1;

#endif
   return 0;
}


static int action_ok_netplay_lan_scan(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_NETWORKING
   struct netplay_host_list *hosts = NULL;
   struct netplay_host *host       = NULL;

   /* Figure out what host we're connecting to */
   if (!netplay_discovery_driver_ctl(RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES, &hosts))
      return -1;
   if (entry_idx >= hosts->size)
      return -1;
   host = &hosts->hosts[entry_idx];

   /* Enable Netplay client mode */
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      generic_action_ok_command(CMD_EVENT_NETPLAY_DEINIT);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

   /* Enable Netplay */
   if (command_event(CMD_EVENT_NETPLAY_INIT_DIRECT, (void *) host))
      return generic_action_ok_command(CMD_EVENT_RESUME);
#endif
   return -1;
}

#define default_action_ok_dl_push(funcname, _fbid, _id, _path) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   settings_t            *settings   = config_get_ptr(); \
   (void)settings; \
   filebrowser_set_type(_fbid); \
   return generic_action_ok_displaylist_push(path, _path, label, type, idx, entry_idx, _id); \
}

default_action_ok_dl_push(action_ok_content_collection_list, FILEBROWSER_SELECT_COLLECTION, ACTION_OK_DL_CONTENT_COLLECTION_LIST, NULL)
default_action_ok_dl_push(action_ok_push_content_list, FILEBROWSER_SELECT_FILE, ACTION_OK_DL_CONTENT_LIST, settings->paths.directory_menu_content)
default_action_ok_dl_push(action_ok_push_scan_file, FILEBROWSER_SCAN_FILE, ACTION_OK_DL_CONTENT_LIST, settings->paths.directory_menu_content)

#ifdef HAVE_NETWORKING
struct netplay_host_list *lan_hosts;
int lan_room_count;

void netplay_refresh_rooms_menu(file_list_t *list)
{
   char s[4115];
   int i                                = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, list);

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
      netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL))
   {
      menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST),
            msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_DISCONNECT),
            MENU_ENUM_LABEL_NETPLAY_DISCONNECT,
            MENU_SETTING_ACTION, 0, 0);
   }
   else if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
      !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL) &&
      netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
   {
      menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT),
            msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_DISCONNECT),
            MENU_ENUM_LABEL_NETPLAY_DISCONNECT,
            MENU_SETTING_ACTION, 0, 0);
   }
   else
   {
      menu_entries_append_enum(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST),
         msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST),
         MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST,
         MENU_SETTING_ACTION, 0, 0);
   }

   menu_entries_append_enum(list,
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT),
      msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT),
      MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT,
      MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS),
         msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS),
         MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS,
         MENU_SETTING_ACTION, 0, 0);

   if (netplay_room_count != 0)
   {
      for (i = 0; i < netplay_room_count; i++)
      {
         char country[PATH_MAX_LENGTH] = {0};

         if (*netplay_room_list[i].country)
            string_add_between_pairs(country, netplay_room_list[i].country,
                  sizeof(country));

         /* Uncomment this to debug mismatched room parameters*/
#if 0
         RARCH_LOG("[lobby] room Data: %d\n"
               "Nickname:         %s\n"
               "Address:          %s\n"
               "Port:             %d\n"
               "Core:             %s\n"
               "Core Version:     %s\n"
               "Game:             %s\n"
               "Game CRC:         %08x\n"
               "Timestamp:        %d\n", room_data->elems[j + 6].data,
               netplay_room_list[i].nickname,
               netplay_room_list[i].address,
               netplay_room_list[i].port,
               netplay_room_list[i].corename,
               netplay_room_list[i].coreversion,
               netplay_room_list[i].gamename,
               netplay_room_list[i].gamecrc,
               netplay_room_list[i].timestamp);
#endif

         snprintf(s, sizeof(s), "%s: %s%s",
            netplay_room_list[i].lan ? "Local" :
            (netplay_room_list[i].host_method == NETPLAY_HOST_METHOD_MITM ?
            "Internet (relay)" : "Internet (direct)"),
            netplay_room_list[i].nickname, country);

         menu_entries_append_enum(list,
               s,
               msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM),
               MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM,
               MENU_SETTINGS_NETPLAY_ROOMS_START + i, 0, 0);
      }

      netplay_rooms_free();
   }
}

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

static void netplay_refresh_rooms_cb(void *task_data, void *user_data, const char *err)
{
   const char *path              = NULL;
   const char *label             = NULL;
   unsigned menu_type            = 0;
   enum msg_hash_enums enum_idx  = MSG_UNKNOWN;

   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   /* Don't push the results if we left the netplay menu */
   if (!string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB))
    && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY)))
      return;

   if (!data || err)
      goto finish;

   data->data = (char*)realloc(data->data, data->len + 1);
   data->data[data->len] = '\0';

   if (!strstr(data->data, file_path_str(FILE_PATH_NETPLAY_ROOM_LIST_URL)))
   {
      if (string_is_empty(data->data))
         netplay_room_count = 0;
      else
      {
         char s[PATH_MAX_LENGTH];
         unsigned i                           = 0;
         unsigned j                           = 0;
         file_list_t *list                    = menu_entries_get_selection_buf_ptr(0);

         lan_room_count                       = 0;

#ifndef RARCH_CONSOLE
         netplay_discovery_driver_ctl(RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES, &lan_hosts);
         if (lan_hosts)
            lan_room_count                    = (int)lan_hosts->size;
#endif

         netplay_rooms_parse(data->data);

         if (netplay_room_list)
            free(netplay_room_list);

         /* TODO/FIXME - right now, a LAN and non-LAN netplay session might appear
          * in the same list. If both entries are available, we want to show only
          * the LAN one. */

         netplay_room_count                   = netplay_rooms_get_count();
         netplay_room_list                    = (struct netplay_room*)
            calloc(netplay_room_count + lan_room_count,
                  sizeof(struct netplay_room));

         for (i = 0; i < (unsigned)netplay_room_count; i++)
            memcpy(&netplay_room_list[i], netplay_room_get(i), sizeof(netplay_room_list[i]));

         if (lan_room_count != 0)
         {
            for (i = netplay_room_count; i < (unsigned)(netplay_room_count + lan_room_count); i++)
            {
               struct netplay_host *host = &lan_hosts->hosts[j++];

               strlcpy(netplay_room_list[i].nickname,
                     host->nick,
                     sizeof(netplay_room_list[i].nickname));

               strlcpy(netplay_room_list[i].address, host->address, INET6_ADDRSTRLEN);

               strlcpy(netplay_room_list[i].corename,
                     host->core,
                     sizeof(netplay_room_list[i].corename));
               strlcpy(netplay_room_list[i].retroarch_version,
                     host->retroarch_version,
                     sizeof(netplay_room_list[i].retroarch_version));
               strlcpy(netplay_room_list[i].coreversion,
                     host->core_version,
                     sizeof(netplay_room_list[i].coreversion));
               strlcpy(netplay_room_list[i].gamename,
                     host->content,
                     sizeof(netplay_room_list[i].gamename));
               strlcpy(netplay_room_list[i].frontend,
                     host->frontend,
                     sizeof(netplay_room_list[i].frontend));

               netplay_room_list[i].port      = host->port;
               netplay_room_list[i].gamecrc   = host->content_crc;
               netplay_room_list[i].timestamp = 0;
               netplay_room_list[i].lan = true;

               snprintf(s, sizeof(s),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME),
                     netplay_room_list[i].nickname);
            }
            netplay_room_count += lan_room_count;
         }
         netplay_refresh_rooms_menu(list);
      }
   }

finish:

   if (err)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), err);

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (user_data)
      free(user_data);

}

static void netplay_lan_scan_callback(void *task_data,
      void *user_data, const char *error)
{
   struct netplay_host_list *netplay_hosts = NULL;
   enum msg_hash_enums enum_idx            = MSG_UNKNOWN;
   unsigned menu_type                      = 0;
   const char *label                       = NULL;
   const char *path                        = NULL;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   /* Don't push the results if we left the LAN scan menu */
   if (!string_is_equal(label,
         msg_hash_to_str(
            MENU_ENUM_LABEL_DEFERRED_NETPLAY_LAN_SCAN_SETTINGS_LIST)))
      return;

   if (!netplay_discovery_driver_ctl(
            RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES,
            (void *) &netplay_hosts))
      return;

   if (netplay_hosts->size > 0)
   {
      unsigned i;
      file_list_t *file_list = menu_entries_get_selection_buf_ptr(0);

      menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, file_list);

      for (i = 0; i < netplay_hosts->size; i++)
      {
         struct netplay_host *host = &netplay_hosts->hosts[i];
         menu_entries_append_enum(file_list,
               host->nick,
               msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_CONNECT_TO),
               MENU_ENUM_LABEL_NETPLAY_CONNECT_TO,
               MENU_NETPLAY_LAN_SCAN, 0, 0);
      }
   }
}

static int action_ok_push_netplay_refresh_rooms(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char url [2048] = "http://newlobby.libretro.com/list/";
#ifndef RARCH_CONSOLE
   task_push_netplay_lan_scan(netplay_lan_scan_callback);
#endif
   task_push_http_transfer(url, true, NULL, netplay_refresh_rooms_cb, NULL);
   return 0;
}
#endif


static int action_ok_scan_directory_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();

   filebrowser_clear_type();
   return generic_action_ok_displaylist_push(path,
         settings->paths.directory_menu_content, label, type, idx,
         entry_idx, ACTION_OK_DL_SCAN_DIR_LIST);
}

static int action_ok_push_random_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, path,
         msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
         type, idx,
         entry_idx, ACTION_OK_DL_CONTENT_LIST);
}

static int action_ok_push_downloads_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();

   filebrowser_set_type(FILEBROWSER_SELECT_FILE);
   return generic_action_ok_displaylist_push(path,
         settings->paths.directory_core_assets,
         msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
         type, idx,
         entry_idx, ACTION_OK_DL_CONTENT_LIST);
}

int action_ok_push_filebrowser_list_dir_select(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu       = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   filebrowser_set_type(FILEBROWSER_SELECT_DIR);
   strlcpy(menu->filebrowser_label, label, sizeof(menu->filebrowser_label));
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_FILE_BROWSER_SELECT_DIR);
}

int action_ok_push_filebrowser_list_file_select(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu       = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   filebrowser_set_type(FILEBROWSER_SELECT_FILE);
   strlcpy(menu->filebrowser_label, label, sizeof(menu->filebrowser_label));
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_FILE_BROWSER_SELECT_DIR);
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filebrowser_clear_type();
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_PUSH_DEFAULT);
}

static int action_ok_start_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   content_ctx_info_t content_info;

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   path_clear(RARCH_PATH_BASENAME);
   if (!task_push_start_current_core(&content_info))
      return -1;

   return 0;
}

static int action_ok_load_archive(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu             = NULL;
   const char *menu_path           = NULL;
   const char *content_path        = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_path    = menu->scratch2_buf;
   content_path = menu->scratch_buf;

   fill_pathname_join(menu->detect_content_path,
         menu_path, content_path,
         sizeof(menu->detect_content_path));

   generic_action_ok_command(CMD_EVENT_LOAD_CORE);

   return default_action_ok_load_content_with_core_from_menu(
         menu->detect_content_path,
         CORE_TYPE_PLAIN);
}

static int action_ok_load_archive_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_content_ctx_defer_info_t def_info;
   int ret                             = 0;
   core_info_list_t *list              = NULL;
   menu_handle_t *menu                 = NULL;
   const char *menu_path               = NULL;
   const char *content_path            = NULL;
   char *new_core_path                 = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_path           = menu->scratch2_buf;
   content_path        = menu->scratch_buf;

   core_info_get_list(&list);

   def_info.data       = list;
   def_info.dir        = menu_path;
   def_info.path       = content_path;
   def_info.menu_label = label;
   def_info.s          = menu->deferred_path;
   def_info.len        = sizeof(menu->deferred_path);

   new_core_path       = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   new_core_path[0]    = '\0';

   if (menu_content_find_first_core(&def_info, false,
            new_core_path, PATH_MAX_LENGTH * sizeof(char)))
      ret = -1;

   fill_pathname_join(menu->detect_content_path, menu_path, content_path,
         sizeof(menu->detect_content_path));

   switch (ret)
   {
      case -1:
         {
            content_ctx_info_t content_info;

            content_info.argc                   = 0;
            content_info.argv                   = NULL;
            content_info.args                   = NULL;
            content_info.environ_get            = NULL;

            ret                                 = 0;

            if (!task_push_load_content_with_new_core_from_menu(
                     new_core_path, def_info.s,
                     &content_info,
                     CORE_TYPE_PLAIN,
                     NULL, NULL))
               ret = -1;
         }
         break;
      case 0:
         idx = menu_navigation_get_selection();
         ret = generic_action_ok_displaylist_push(path, NULL,
               label, type,
               idx, entry_idx, ACTION_OK_DL_DEFERRED_CORE_LIST);
         break;
      default:
         break;
   }

   free(new_core_path);
   return ret;
}

#define default_action_ok_help(funcname, _id, _id2) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_help(path, label, type, idx, entry_idx, _id, _id2); \
}

default_action_ok_help(action_ok_help_audio_video_troubleshooting, MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING, MENU_DIALOG_HELP_AUDIO_VIDEO_TROUBLESHOOTING)
default_action_ok_help(action_ok_help, MENU_ENUM_LABEL_HELP, MENU_DIALOG_WELCOME)
default_action_ok_help(action_ok_help_controls, MENU_ENUM_LABEL_HELP_CONTROLS, MENU_DIALOG_HELP_CONTROLS)
default_action_ok_help(action_ok_help_what_is_a_core, MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE, MENU_DIALOG_HELP_WHAT_IS_A_CORE)
default_action_ok_help(action_ok_help_scanning_content, MENU_ENUM_LABEL_HELP_SCANNING_CONTENT, MENU_DIALOG_HELP_SCANNING_CONTENT)
default_action_ok_help(action_ok_help_change_virtual_gamepad, MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD, MENU_DIALOG_HELP_CHANGE_VIRTUAL_GAMEPAD)
default_action_ok_help(action_ok_help_load_content, MENU_ENUM_LABEL_HELP_LOADING_CONTENT, MENU_DIALOG_HELP_LOADING_CONTENT)

static int action_ok_video_resolution(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned width   = 0;
   unsigned  height = 0;

   if (video_driver_get_video_output_size(&width, &height))
   {
      char msg[PATH_MAX_LENGTH];

      msg[0] = '\0';

#if defined(__CELLOS_LV2__) || defined(_WIN32)
      generic_action_ok_command(CMD_EVENT_REINIT);
#endif
      video_driver_set_video_mode(width, height, true);
#ifdef GEKKO
      if (width == 0 || height == 0)
         strlcpy(msg, "Applying: DEFAULT", sizeof(msg));
      else
#endif
         snprintf(msg, sizeof(msg),
               "Applying: %dx%d\n START to reset",
               width, height);
      runloop_msg_queue_push(msg, 1, 100, true);
   }

   return 0;
}

static int action_ok_netplay_enable_host(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_NETWORKING
   bool contentless  = false;
   bool is_inited    = false;
   file_list_t *list = menu_entries_get_selection_buf_ptr(0);

   content_get_status(&contentless, &is_inited);

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      generic_action_ok_command(CMD_EVENT_NETPLAY_DEINIT);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);

   netplay_refresh_rooms_menu(list);
   /* If we haven't yet started, this will load on its own */
   if (!is_inited)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED),
            1, 480, true);
      return 0;
   }

   /* Enable Netplay itself */
   if (command_event(CMD_EVENT_NETPLAY_INIT, NULL))
      return generic_action_ok_command(CMD_EVENT_RESUME);
#endif
   return -1;
}

#ifdef HAVE_NETWORKING
static void action_ok_netplay_enable_client_hostname_cb(
   void *ignore, const char *hostname)
{

   if (hostname && hostname[0])
   {
      bool contentless   = false;
      bool is_inited     = false;
      char *tmp_hostname = strdup(hostname);

      content_get_status(&contentless, &is_inited);

      if (!is_inited)
      {
         command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED,
               (void*)tmp_hostname);
         runloop_msg_queue_push(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED),
            1, 480, true);
      }
      else
      {
         command_event(CMD_EVENT_NETPLAY_INIT_DIRECT,
               (void*)tmp_hostname);
         generic_action_ok_command(CMD_EVENT_RESUME);
      }

      free(tmp_hostname);
   }
   else
   {
      menu_input_dialog_end();
      return;
   }

   menu_input_dialog_end();
   rarch_menu_running_finished();
}
#endif

static int action_ok_netplay_enable_client(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_NETWORKING
   settings_t *settings = config_get_ptr();
   menu_input_ctx_line_t line;
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      generic_action_ok_command(CMD_EVENT_NETPLAY_DEINIT);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

   if (!string_is_empty(settings->paths.netplay_server))
   {
      action_ok_netplay_enable_client_hostname_cb(NULL, settings->paths.netplay_server);
      return 0;
   }
   else
   {
      line.label         = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS);
      line.label_setting = "no_setting";
      line.type          = 0;
      line.idx           = 0;
      line.cb            = action_ok_netplay_enable_client_hostname_cb;

      if (menu_input_dialog_start(&line))
         return 0;
   }
#endif
   return -1;
}

static int action_ok_netplay_disconnect(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_NETWORKING
   settings_t *settings = config_get_ptr();

   netplay_driver_ctl(RARCH_NETPLAY_CTL_DISCONNECT, NULL);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);

   /* Re-enable rewind if it was enabled
      TODO: Add a setting for these tweaks */
   if (settings->bools.rewind_enable)
      generic_action_ok_command(CMD_EVENT_REWIND_INIT);
   if (settings->uints.autosave_interval != 0)
      generic_action_ok_command(CMD_EVENT_AUTOSAVE_INIT);
   return generic_action_ok_command(CMD_EVENT_RESUME);

#else
   return -1;

#endif
}

static int action_ok_core_delete(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *path_core = path_get(RARCH_PATH_CORE);
   char *core_path       = !string_is_empty(path_core)
      ? strdup(path_core) : NULL;

   if (!core_path)
      return 0;

   generic_action_ok_command(CMD_EVENT_UNLOAD_CORE);
   menu_entries_flush_stack(0, 0);

   if (filestream_delete(core_path) != 0) { }

   free(core_path);

   return 0;
}

static int is_rdb_entry(enum msg_hash_enums enum_idx)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_RDB_ENTRY_PUBLISHER:
      case MENU_ENUM_LABEL_RDB_ENTRY_DEVELOPER:
      case MENU_ENUM_LABEL_RDB_ENTRY_ORIGIN:
      case MENU_ENUM_LABEL_RDB_ENTRY_FRANCHISE:
      case MENU_ENUM_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
      case MENU_ENUM_LABEL_RDB_ENTRY_ESRB_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_BBFC_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_ELSPA_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_PEGI_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_CERO_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
      case MENU_ENUM_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_MONTH:
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_YEAR:
      case MENU_ENUM_LABEL_RDB_ENTRY_MAX_USERS:
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_ok_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t hash)
{
   if (cbs->enum_idx != MSG_UNKNOWN && is_rdb_entry(cbs->enum_idx) == 0)
   {
      BIND_ACTION_OK(cbs, action_ok_rdb_entry_submenu);
      return 0;
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      const char     *str = msg_hash_to_str(cbs->enum_idx);

      if (str && strstr(str, "input_binds_list"))
      {
         unsigned i;

         for (i = 0; i < MAX_USERS; i++)
         {
            unsigned first_char = atoi(&str[0]);

            if (first_char != ((i+1)))
               continue;

            BIND_ACTION_OK(cbs, action_ok_push_user_binds_list);
            return 0;
         }
      }
   }

   if (menu_setting_get_browser_selection_type(cbs->setting) == ST_DIR)
   {
      BIND_ACTION_OK(cbs, action_ok_push_filebrowser_list_dir_select);
      return 0;
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            BIND_ACTION_OK(cbs, action_ok_cheat_start_or_cont);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
            BIND_ACTION_OK(cbs, action_ok_cheat_add_top);
            break;
         case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
            BIND_ACTION_OK(cbs, action_ok_cheat_reload_cheats);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
            BIND_ACTION_OK(cbs, action_ok_cheat_add_bottom);
            break;
         case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
            BIND_ACTION_OK(cbs, action_ok_cheat_delete_all);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER:
            BIND_ACTION_OK(cbs, action_ok_cheat_add_new_after);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE:
            BIND_ACTION_OK(cbs, action_ok_cheat_add_new_before);
            break;
         case MENU_ENUM_LABEL_CHEAT_COPY_AFTER:
            BIND_ACTION_OK(cbs, action_ok_cheat_copy_after);
            break;
         case MENU_ENUM_LABEL_CHEAT_COPY_BEFORE:
            BIND_ACTION_OK(cbs, action_ok_cheat_copy_before);
            break;
         case MENU_ENUM_LABEL_CHEAT_DELETE:
            BIND_ACTION_OK(cbs, action_ok_cheat_delete);
            break;
         case MENU_ENUM_LABEL_RUN_MUSIC:
            BIND_ACTION_OK(cbs, action_ok_audio_run);
            break;
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION:
            BIND_ACTION_OK(cbs, action_ok_audio_add_to_mixer_and_collection);
            break;
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY:
            BIND_ACTION_OK(cbs, action_ok_audio_add_to_mixer_and_collection_and_play);
            break;
         case MENU_ENUM_LABEL_ADD_TO_MIXER:
            BIND_ACTION_OK(cbs, action_ok_audio_add_to_mixer);
            break;
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY:
            BIND_ACTION_OK(cbs, action_ok_audio_add_to_mixer_and_play);
            break;
         case MENU_ENUM_LABEL_MENU_WALLPAPER:
            BIND_ACTION_OK(cbs, action_ok_menu_wallpaper);
            break;
         case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            BIND_ACTION_OK(cbs, action_ok_menu_font);
            break;
         case MENU_ENUM_LABEL_GOTO_FAVORITES:
            BIND_ACTION_OK(cbs, action_ok_goto_favorites);
            break;
         case MENU_ENUM_LABEL_GOTO_MUSIC:
            BIND_ACTION_OK(cbs, action_ok_goto_music);
            break;
         case MENU_ENUM_LABEL_GOTO_IMAGES:
            BIND_ACTION_OK(cbs, action_ok_goto_images);
            break;
         case MENU_ENUM_LABEL_GOTO_VIDEO:
            BIND_ACTION_OK(cbs, action_ok_goto_video);
            break;
         case MENU_ENUM_LABEL_BROWSE_START:
            BIND_ACTION_OK(cbs, action_ok_browse_url_start);
            break;
         case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            BIND_ACTION_OK(cbs, action_ok_load_core);
            break;
         case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
         case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION_CURRENT_CORE:
            BIND_ACTION_OK(cbs, action_ok_core_deferred_set);
            break;
         case MENU_ENUM_LABEL_START_CORE:
            BIND_ACTION_OK(cbs, action_ok_start_core);
            break;
         case MENU_ENUM_LABEL_START_NET_RETROPAD:
            BIND_ACTION_OK(cbs, action_ok_start_net_retropad_core);
            break;
         case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
            BIND_ACTION_OK(cbs, action_ok_start_video_processor_core);
            break;
         case MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE:
            BIND_ACTION_OK(cbs, action_ok_open_archive_detect_core);
            break;
         case MENU_ENUM_LABEL_OPEN_ARCHIVE:
            BIND_ACTION_OK(cbs, action_ok_open_archive);
            break;
         case MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE:
            BIND_ACTION_OK(cbs, action_ok_load_archive_detect_core);
            break;
         case MENU_ENUM_LABEL_LOAD_ARCHIVE:
            BIND_ACTION_OK(cbs, action_ok_load_archive);
            break;
         case MENU_ENUM_LABEL_CUSTOM_BIND_ALL:
            BIND_ACTION_OK(cbs, action_ok_lookup_setting);
            break;
         case MENU_ENUM_LABEL_SAVE_STATE:
            BIND_ACTION_OK(cbs, action_ok_save_state);
            break;
         case MENU_ENUM_LABEL_LOAD_STATE:
            BIND_ACTION_OK(cbs, action_ok_load_state);
            break;
         case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            BIND_ACTION_OK(cbs, action_ok_undo_load_state);
            break;
         case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            BIND_ACTION_OK(cbs, action_ok_undo_save_state);
            break;
         case MENU_ENUM_LABEL_RESUME_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_resume_content);
            break;
         case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
            BIND_ACTION_OK(cbs, action_ok_add_to_favorites_playlist);
            break;
         case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
            BIND_ACTION_OK(cbs, action_ok_reset_core_association);
            break;
         case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            BIND_ACTION_OK(cbs, action_ok_add_to_favorites);
            break;
         case MENU_ENUM_LABEL_RESTART_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_restart_content);
            break;
         case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            BIND_ACTION_OK(cbs, action_ok_screenshot);
            break;
         case MENU_ENUM_LABEL_RENAME_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rename_entry);
            break;
         case MENU_ENUM_LABEL_DELETE_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_delete_entry);
            break;
         case MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE:
            BIND_ACTION_OK(cbs, action_ok_disable_kiosk_mode);
            break;
         case MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_enable_settings);
            break;
         case MENU_ENUM_LABEL_SHOW_WIMP:
            BIND_ACTION_OK(cbs, action_ok_show_wimp);
            break;
         case MENU_ENUM_LABEL_QUIT_RETROARCH:
            BIND_ACTION_OK(cbs, action_ok_quit);
            break;
         case MENU_ENUM_LABEL_CLOSE_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_close_content);
            break;
         case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_save_new_config);
            break;
         case MENU_ENUM_LABEL_HELP:
            BIND_ACTION_OK(cbs, action_ok_help);
            break;
         case MENU_ENUM_LABEL_HELP_CONTROLS:
            BIND_ACTION_OK(cbs, action_ok_help_controls);
            break;
         case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
            BIND_ACTION_OK(cbs, action_ok_help_what_is_a_core);
            break;
         case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
            BIND_ACTION_OK(cbs, action_ok_help_change_virtual_gamepad);
            break;
         case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
            BIND_ACTION_OK(cbs, action_ok_help_audio_video_troubleshooting);
            break;
         case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_help_scanning_content);
            break;
         case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_help_load_content);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            BIND_ACTION_OK(cbs, action_ok_shader_pass);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_OK(cbs, action_ok_shader_preset);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_OK(cbs, action_ok_cheat_file);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
            BIND_ACTION_OK(cbs, action_ok_cheat_file_append);
            break;
         case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_OK(cbs, action_ok_audio_dsp_plugin);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_OK(cbs, action_ok_remap_file);
            break;
         case MENU_ENUM_LABEL_RECORD_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_record_configfile);
            break;
#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_list);
            break;
         case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS:
            BIND_ACTION_OK(cbs, action_ok_core_content_dirs_list);
            break;
         case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
            BIND_ACTION_OK(cbs, action_ok_core_updater_list);
            break;
         case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_OK(cbs, action_ok_thumbnails_updater_list);
            break;
         case MENU_ENUM_LABEL_UPDATE_LAKKA:
            BIND_ACTION_OK(cbs, action_ok_lakka_list);
            break;
#endif
         case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_OK(cbs, action_ok_shader_parameters);
            break;
         case MENU_ENUM_LABEL_ACCOUNTS_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_accounts_list);
            break;
         case MENU_ENUM_LABEL_INPUT_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_input_settings_list);
            break;
         case MENU_ENUM_LABEL_DRIVER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_driver_settings_list);
            break;
         case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_video_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_audio_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_audio_mixer_settings_list);
            break;
         case MENU_ENUM_LABEL_LATENCY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_latency_settings_list);
            break;
         case MENU_ENUM_LABEL_CORE_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_core_settings_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_configuration_settings_list);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_playlist_settings_list);
            break;
         case MENU_ENUM_LABEL_RECORDING_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_recording_settings_list);
            break;
         case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS:
            BIND_ACTION_OK(cbs, action_ok_push_input_hotkey_binds_list);
            break;
         case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            BIND_ACTION_OK(cbs, action_ok_push_accounts_cheevos_list);
            break;
         case MENU_ENUM_LABEL_SHADER_OPTIONS:
         case MENU_ENUM_LABEL_CORE_OPTIONS:
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         case MENU_ENUM_LABEL_CORE_INFORMATION:
         case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
         case MENU_ENUM_LABEL_NETWORK_INFORMATION:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         case MENU_ENUM_LABEL_DISK_OPTIONS:
         case MENU_ENUM_LABEL_SETTINGS:
         case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
         case MENU_ENUM_LABEL_CORE_COUNTERS:
         case MENU_ENUM_LABEL_MANAGEMENT:
         case MENU_ENUM_LABEL_ONLINE_UPDATER:
         case MENU_ENUM_LABEL_NETPLAY:
         case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
         case MENU_ENUM_LABEL_CONFIGURATIONS_LIST:
         case MENU_ENUM_LABEL_HELP_LIST:
         case MENU_ENUM_LABEL_INFORMATION_LIST:
         case MENU_ENUM_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL:
            BIND_ACTION_OK(cbs, action_ok_push_filebrowser_list_file_select);
            break;
         case MENU_ENUM_LABEL_SCAN_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_scan_directory_list);
            break;
         case MENU_ENUM_LABEL_SCAN_FILE:
            BIND_ACTION_OK(cbs, action_ok_push_scan_file);
            break;
#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
            BIND_ACTION_OK(cbs, action_ok_push_netplay_refresh_rooms);
            break;
#endif
         case MENU_ENUM_LABEL_FAVORITES:
            BIND_ACTION_OK(cbs, action_ok_push_content_list);
            break;
         case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
            BIND_ACTION_OK(cbs, action_ok_push_random_dir);
            break;
         case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_downloads_dir);
            break;
         case MENU_ENUM_LABEL_DETECT_CORE_LIST_OK:
            BIND_ACTION_OK(cbs, action_ok_file_load_detect_core);
            break;
         case MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE:
            BIND_ACTION_OK(cbs, action_ok_file_load_current_core);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_generic_list);
            break;
         case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            BIND_ACTION_OK(cbs, action_ok_shader_apply_changes);
            break;
         case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
            BIND_ACTION_OK(cbs, action_ok_cheat_apply_changes);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
            BIND_ACTION_OK(cbs, action_ok_shader_preset_save_as);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            BIND_ACTION_OK(cbs, action_ok_shader_preset_save_game);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
            BIND_ACTION_OK(cbs, action_ok_shader_preset_save_core);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
            BIND_ACTION_OK(cbs, action_ok_shader_preset_save_parent);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            BIND_ACTION_OK(cbs, action_ok_cheat_file_save_as);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_core);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_content_dir);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_game);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE:
            BIND_ACTION_OK(cbs, action_ok_remap_file_remove_core);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR:
            BIND_ACTION_OK(cbs, action_ok_remap_file_remove_content_dir);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME:
            BIND_ACTION_OK(cbs, action_ok_remap_file_remove_game);
            break;
         case MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST:
            BIND_ACTION_OK(cbs, action_ok_content_collection_list);
            break;
         case MENU_ENUM_LABEL_BROWSE_URL_LIST:
            BIND_ACTION_OK(cbs, action_ok_browse_url_list);
            break;
         case MENU_ENUM_LABEL_CORE_LIST:
            BIND_ACTION_OK(cbs, action_ok_core_list);
            break;
         case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
            BIND_ACTION_OK(cbs, action_ok_disk_image_append_list);
            break;
         case MENU_ENUM_LABEL_SUBSYSTEM_ADD:
            BIND_ACTION_OK(cbs, action_ok_subsystem_add_list);
            break;
         case MENU_ENUM_LABEL_SUBSYSTEM_LOAD:
            BIND_ACTION_OK(cbs, action_ok_subsystem_add_load);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS:
            BIND_ACTION_OK(cbs, action_ok_configurations_list);
            break;
         case MENU_ENUM_LABEL_SAVING_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_saving_list);
            break;
         case MENU_ENUM_LABEL_LOGGING_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_logging_list);
            break;
         case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_frame_throttle_list);
            break;
         case MENU_ENUM_LABEL_REWIND_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_rewind_list);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_onscreen_display_list);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_onscreen_notifications_list);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_onscreen_overlay_list);
            break;
         case MENU_ENUM_LABEL_MENU_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_menu_list);
            break;
         case MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_menu_views_list);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
            BIND_ACTION_OK(cbs, action_ok_quick_menu_override_options);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_quick_menu_views_list);
            break;
         case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_user_interface_list);
            break;
         case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_power_management_list);
            break;
         case MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_menu_file_browser_list);
            break;
         case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_retro_achievements_list);
            break;
         case MENU_ENUM_LABEL_UPDATER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_updater_list);
            break;
         case MENU_ENUM_LABEL_WIFI_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_wifi_list);
            break;
         case MENU_ENUM_LABEL_NETWORK_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_network_list);
            break;
         case MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM:
            BIND_ACTION_OK(cbs, action_ok_netplay_connect_room);
            break;
         case MENU_ENUM_LABEL_LAKKA_SERVICES:
            BIND_ACTION_OK(cbs, action_ok_lakka_services);
            break;
         case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_netplay_sublist);
            break;
         case MENU_ENUM_LABEL_USER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_user_list);
            break;
         case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_directory_list);
            break;
         case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_privacy_list);
            break;
         case MENU_ENUM_LABEL_MIDI_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_midi_list);
            break;
         case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
            BIND_ACTION_OK(cbs, action_ok_video_resolution);
            break;
         case MENU_ENUM_LABEL_UPDATE_ASSETS:
            BIND_ACTION_OK(cbs, action_ok_update_assets);
            break;
         case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
            BIND_ACTION_OK(cbs, action_ok_update_core_info_files);
            break;
         case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
            BIND_ACTION_OK(cbs, action_ok_update_overlays);
            break;
         case MENU_ENUM_LABEL_UPDATE_DATABASES:
            BIND_ACTION_OK(cbs, action_ok_update_databases);
            break;
         case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
            BIND_ACTION_OK(cbs, action_ok_update_shaders_glsl);
            break;
         case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
            BIND_ACTION_OK(cbs, action_ok_update_shaders_cg);
            break;
         case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
            BIND_ACTION_OK(cbs, action_ok_update_shaders_slang);
            break;
         case MENU_ENUM_LABEL_UPDATE_CHEATS:
            BIND_ACTION_OK(cbs, action_ok_update_cheats);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_MATCHES:
            BIND_ACTION_OK(cbs, cheat_manager_add_matches);
            break;
         case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
            BIND_ACTION_OK(cbs, action_ok_update_autoconfig_profiles);
            break;
         case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            BIND_ACTION_OK(cbs, action_ok_netplay_enable_host);
            break;
         case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            BIND_ACTION_OK(cbs, action_ok_netplay_enable_client);
            break;
         case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            BIND_ACTION_OK(cbs, action_ok_netplay_disconnect);
            break;
         case MENU_ENUM_LABEL_CORE_DELETE:
            BIND_ACTION_OK(cbs, action_ok_core_delete);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE:
         case MENU_ENUM_LABEL_ACHIEVEMENT_RESUME:
            BIND_ACTION_OK(cbs, action_ok_cheevos_toggle_hardcore_mode);
            break;
         default:
            return -1;
      }
   }
   else
   {
      switch (hash)
      {
         case MENU_LABEL_OPEN_ARCHIVE_DETECT_CORE:
            BIND_ACTION_OK(cbs, action_ok_open_archive_detect_core);
            break;
         case MENU_LABEL_OPEN_ARCHIVE:
            BIND_ACTION_OK(cbs, action_ok_open_archive);
            break;
         case MENU_LABEL_LOAD_ARCHIVE_DETECT_CORE:
            BIND_ACTION_OK(cbs, action_ok_load_archive_detect_core);
            break;
         case MENU_LABEL_LOAD_ARCHIVE:
            BIND_ACTION_OK(cbs, action_ok_load_archive);
            break;
         case MENU_LABEL_VIDEO_SHADER_PASS:
            BIND_ACTION_OK(cbs, action_ok_shader_pass);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_OK(cbs, action_ok_shader_preset);
            break;
         case MENU_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_OK(cbs, action_ok_cheat_file);
            break;
         case MENU_LABEL_CHEAT_FILE_LOAD_APPEND:
            BIND_ACTION_OK(cbs, action_ok_cheat_file_append);
            break;
         case MENU_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_OK(cbs, action_ok_audio_dsp_plugin);
            break;
         case MENU_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_OK(cbs, action_ok_remap_file);
            break;
         case MENU_LABEL_RECORD_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_record_configfile);
            break;
#ifdef HAVE_NETWORKING
         case MENU_LABEL_UPDATE_LAKKA:
            BIND_ACTION_OK(cbs, action_ok_lakka_list);
            break;
#endif
         case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_OK(cbs, action_ok_shader_parameters);
            break;
         case MENU_LABEL_ACCOUNTS_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_accounts_list);
            break;
         case MENU_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            BIND_ACTION_OK(cbs, action_ok_push_accounts_cheevos_list);
            break;
         case MENU_LABEL_FAVORITES:
            BIND_ACTION_OK(cbs, action_ok_push_content_list);
            break;
         case MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_downloads_dir);
            break;
         case MENU_LABEL_DETECT_CORE_LIST_OK:
            BIND_ACTION_OK(cbs, action_ok_file_load_detect_core);
            break;
         case MENU_LABEL_SHADER_APPLY_CHANGES:
            BIND_ACTION_OK(cbs, action_ok_shader_apply_changes);
            break;
         case MENU_LABEL_CHEAT_APPLY_CHANGES:
            BIND_ACTION_OK(cbs, action_ok_cheat_apply_changes);
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
            BIND_ACTION_OK(cbs, action_ok_shader_preset_save_as);
            break;
         case MENU_LABEL_CHEAT_FILE_SAVE_AS:
            BIND_ACTION_OK(cbs, action_ok_cheat_file_save_as);
            break;
         case MENU_LABEL_REMAP_FILE_SAVE_CORE:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_core);
            break;
         case MENU_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_content_dir);
            break;
         case MENU_LABEL_REMAP_FILE_SAVE_GAME:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_game);
            break;
         case MENU_LABEL_DISK_IMAGE_APPEND:
            BIND_ACTION_OK(cbs, action_ok_disk_image_append_list);
            break;
         case MENU_LABEL_SUBSYSTEM_ADD:
            BIND_ACTION_OK(cbs, action_ok_subsystem_add_list);
            break;
         case MENU_LABEL_SCREEN_RESOLUTION:
            BIND_ACTION_OK(cbs, action_ok_video_resolution);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_ok_compare_type(menu_file_list_cbs_t *cbs,
      uint32_t label_hash, uint32_t menu_label_hash, unsigned type)
{
   if (type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD ||
         type == MENU_SETTINGS_CUSTOM_BIND)
   {
      BIND_ACTION_OK(cbs, action_ok_lookup_setting);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_play);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_play_looped);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_play_sequential);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_remove);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_stop);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_actions);
   }
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_OK(cbs, NULL);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_OK(cbs, NULL);
   }
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_OK(cbs, action_ok_cheat);
   }
   else
   {
      switch (type)
      {
         case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case FILE_TYPE_PLAYLIST_ENTRY:
            if (label_hash == MENU_LABEL_COLLECTION)
            {
               BIND_ACTION_OK(cbs, action_ok_playlist_entry_collection);
            }
            else if (label_hash == MENU_LABEL_RDB_ENTRY_START_CONTENT)
            {
               BIND_ACTION_OK(cbs, action_ok_playlist_entry_start_content);
            }
            else
            {
               BIND_ACTION_OK(cbs, action_ok_playlist_entry);
            }
            break;
         case FILE_TYPE_RPL_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rpl_entry);
            break;
         case FILE_TYPE_PLAYLIST_COLLECTION:
            BIND_ACTION_OK(cbs, action_ok_playlist_collection);
            break;
         case FILE_TYPE_CONTENTLIST_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_push_generic_list);
            break;
         case FILE_TYPE_CHEAT:
            if ( menu_label_hash == MENU_LABEL_CHEAT_FILE_LOAD_APPEND )
            {
               BIND_ACTION_OK(cbs, action_ok_cheat_file_load_append);
            }
            else
            {
               BIND_ACTION_OK(cbs, action_ok_cheat_file_load);
            }
            break;
         case FILE_TYPE_RECORD_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_record_configfile_load);
            break;
         case FILE_TYPE_REMAP:
            BIND_ACTION_OK(cbs, action_ok_remap_file_load);
            break;
         case FILE_TYPE_SHADER_PRESET:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_shader_preset_load);
            break;
         case FILE_TYPE_SHADER:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_shader_pass_load);
            break;
         case FILE_TYPE_IMAGE:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_menu_wallpaper_load);
            break;
         case FILE_TYPE_USE_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_path_use_directory);
            break;
#ifdef HAVE_LIBRETRODB
         case FILE_TYPE_SCAN_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_path_scan_directory);
            break;
#endif
         case FILE_TYPE_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_config_load);
            break;
         case FILE_TYPE_PARENT_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_parent_directory_push);
            break;
         case FILE_TYPE_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_directory_push);
            break;
         case FILE_TYPE_CARCHIVE:
            if (filebrowser_get_type() == FILEBROWSER_SCAN_FILE)
            {
#ifdef HAVE_LIBRETRODB
               BIND_ACTION_OK(cbs, action_ok_scan_file);
#endif
            }
            else
            {
               switch (menu_label_hash)
               {
                  case MENU_LABEL_FAVORITES:
                     BIND_ACTION_OK(cbs, action_ok_compressed_archive_push_detect_core);
                     break;
                  default:
                     BIND_ACTION_OK(cbs, action_ok_compressed_archive_push);
                     break;
               }
            }
            break;
         case FILE_TYPE_CORE:
            if (cbs->enum_idx != MSG_UNKNOWN)
            {
               switch (cbs->enum_idx)
               {
                  case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
                     BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
                     break;
                  case MSG_UNKNOWN:
                  default:
                     break;
               }
            }
            else
            {
               switch (menu_label_hash)
               {
                  case MENU_LABEL_DEFERRED_CORE_LIST:
                     BIND_ACTION_OK(cbs, action_ok_load_core_deferred);
                     break;
                  case MENU_LABEL_DEFERRED_CORE_LIST_SET:
                     BIND_ACTION_OK(cbs, action_ok_core_deferred_set);
                     break;
                  case MENU_LABEL_CORE_LIST:
                     BIND_ACTION_OK(cbs, action_ok_load_core);
                     break;
               }
            }
            break;
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_download);
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_thumbnails);
            break;
         case FILE_TYPE_DOWNLOAD_CORE:
            BIND_ACTION_OK(cbs, action_ok_core_updater_download);
            break;
         case FILE_TYPE_DOWNLOAD_URL:
            BIND_ACTION_OK(cbs, action_ok_download_url);
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL:
            BIND_ACTION_OK(cbs, action_ok_thumbnails_updater_download);
            break;
         case FILE_TYPE_DOWNLOAD_LAKKA:
            BIND_ACTION_OK(cbs, action_ok_lakka_download);
            break;
         case FILE_TYPE_DOWNLOAD_CORE_INFO:
            break;
         case FILE_TYPE_RDB:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
                  BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
                  break;
               case MENU_LABEL_DATABASE_MANAGER_LIST:
               case MENU_VALUE_HORIZONTAL_MENU:
                  BIND_ACTION_OK(cbs, action_ok_database_manager_list);
                  break;
            }
            break;
         case FILE_TYPE_RDB_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rdb_entry);
            break;
         case MENU_WIFI:
            BIND_ACTION_OK(cbs, action_ok_wifi);
            break;
         case MENU_NETPLAY_LAN_SCAN:
            BIND_ACTION_OK(cbs, action_ok_netplay_lan_scan);
            break;
         case FILE_TYPE_CURSOR:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
                  BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
                  break;
               case MENU_LABEL_CURSOR_MANAGER_LIST:
                  BIND_ACTION_OK(cbs, action_ok_cursor_manager_list);
                  break;
            }
            break;
         case FILE_TYPE_VIDEOFILTER:
            BIND_ACTION_OK(cbs, action_ok_set_path_videofilter);
            break;
         case FILE_TYPE_FONT:
            BIND_ACTION_OK(cbs, action_ok_set_path);
            break;
         case FILE_TYPE_OVERLAY:
            BIND_ACTION_OK(cbs, action_ok_set_path_overlay);
            break;
         case FILE_TYPE_AUDIOFILTER:
            BIND_ACTION_OK(cbs, action_ok_set_path_audiofilter);
            break;
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_PLAIN:
            if (filebrowser_get_type() == FILEBROWSER_SCAN_FILE)
            {
#ifdef HAVE_LIBRETRODB
               BIND_ACTION_OK(cbs, action_ok_scan_file);
#endif
            }
            else if (cbs->enum_idx != MSG_UNKNOWN)
            {
               switch (cbs->enum_idx)
               {
                  case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
                  case MENU_ENUM_LABEL_FAVORITES:
                  case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
#ifdef HAVE_COMPRESSION
                     if (type == FILE_TYPE_IN_CARCHIVE)
                     {
                        BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core_carchive);
                     }
                     else
#endif
                     {
                        BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core);
                     }
                     break;
                  case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
                     BIND_ACTION_OK(cbs, action_ok_disk_image_append);
                     break;
                  case MENU_ENUM_LABEL_SUBSYSTEM_ADD:
                     BIND_ACTION_OK(cbs, action_ok_subsystem_add);
                     break;
                  default:
                     BIND_ACTION_OK(cbs, action_ok_file_load);
                     break;
               }
            }
            else
            {
               switch (menu_label_hash)
               {
                  case MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
                  case MENU_LABEL_FAVORITES:
                  case MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
#ifdef HAVE_COMPRESSION
                     if (type == FILE_TYPE_IN_CARCHIVE)
                     {
                        BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core_carchive);
                     }
                     else
#endif
                     {
                        BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core);
                     }
                     break;
                  case MENU_LABEL_DISK_IMAGE_APPEND:
                     BIND_ACTION_OK(cbs, action_ok_disk_image_append);
                     break;
                  case MENU_LABEL_SUBSYSTEM_ADD:
                     BIND_ACTION_OK(cbs, action_ok_subsystem_add);
                     break;
                  default:
                     BIND_ACTION_OK(cbs, action_ok_file_load);
                     break;
               }
            }
            break;
         case FILE_TYPE_MOVIE:
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_file_load_ffmpeg);
#endif
            break;
         case FILE_TYPE_MUSIC:
            BIND_ACTION_OK(cbs, action_ok_file_load_music);
            break;
         case FILE_TYPE_IMAGEVIEWER:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_file_load_imageviewer);
            break;
         case FILE_TYPE_DIRECT_LOAD:
            BIND_ACTION_OK(cbs, action_ok_file_load);
            break;
         case MENU_SETTINGS:
         case MENU_SETTING_GROUP:
         case MENU_SETTING_SUBGROUP:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
            BIND_ACTION_OK(cbs, action_ok_disk_cycle_tray_status);
            break;
         case MENU_SETTINGS_CORE_OPTION_CREATE:
            BIND_ACTION_OK(cbs, action_ok_option_create);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_OK(cbs, action_ok_lookup_setting);

   if (menu_cbs_init_bind_ok_compare_label(cbs, label, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_ok_compare_type(cbs, label_hash, menu_label_hash, type) == 0)
      return 0;

   return -1;
}

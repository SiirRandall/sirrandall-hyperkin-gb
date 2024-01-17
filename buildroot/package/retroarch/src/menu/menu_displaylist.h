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

#ifndef _MENU_DISPLAYLIST_H
#define _MENU_DISPLAYLIST_H

#include <stdint.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <retro_common_api.h>
#include <lists/file_list.h>

#include "../setting_list.h"

#ifndef COLLECTION_SIZE
#define COLLECTION_SIZE 99999
#endif

RETRO_BEGIN_DECLS

enum menu_displaylist_parse_type
{
   PARSE_NONE                = (1 << 0),
   PARSE_GROUP               = (1 << 1),
   PARSE_ACTION              = (1 << 2),
   PARSE_ONLY_INT            = (1 << 3),
   PARSE_ONLY_UINT           = (1 << 4),
   PARSE_ONLY_BOOL           = (1 << 5),
   PARSE_ONLY_FLOAT          = (1 << 6),
   PARSE_ONLY_BIND           = (1 << 7),
   PARSE_ONLY_GROUP          = (1 << 8),
   PARSE_ONLY_STRING         = (1 << 9),
   PARSE_ONLY_PATH           = (1 << 10),
   PARSE_ONLY_STRING_OPTIONS = (1 << 11),
   PARSE_ONLY_HEX            = (1 << 12),
   PARSE_ONLY_DIR            = (1 << 13),
   PARSE_SUB_GROUP           = (1 << 14),
   PARSE_ONLY_SIZE           = (1 << 15)
};

enum menu_displaylist_ctl_state
{
   DISPLAYLIST_NONE = 0,
   DISPLAYLIST_INFO,
   DISPLAYLIST_HELP,
   DISPLAYLIST_HELP_SCREEN_LIST,
   DISPLAYLIST_MAIN_MENU,
   DISPLAYLIST_GENERIC,
   DISPLAYLIST_SETTING_ENUM,
   DISPLAYLIST_SETTINGS_ALL,
   DISPLAYLIST_HORIZONTAL,
   DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS,
   DISPLAYLIST_HISTORY,
   DISPLAYLIST_FAVORITES,
   DISPLAYLIST_PLAYLIST,
   DISPLAYLIST_VIDEO_HISTORY,
   DISPLAYLIST_MUSIC_HISTORY,
   DISPLAYLIST_IMAGES_HISTORY,
   DISPLAYLIST_MUSIC_LIST,
   DISPLAYLIST_PLAYLIST_COLLECTION,
   DISPLAYLIST_DEFAULT,
   DISPLAYLIST_FILE_BROWSER_SELECT_DIR,
   DISPLAYLIST_FILE_BROWSER_SCAN_DIR,
   DISPLAYLIST_FILE_BROWSER_SELECT_FILE,
   DISPLAYLIST_FILE_BROWSER_SELECT_CORE,
   DISPLAYLIST_FILE_BROWSER_SELECT_COLLECTION,
   DISPLAYLIST_CORES,
   DISPLAYLIST_CORES_SUPPORTED,
   DISPLAYLIST_CORES_COLLECTION_SUPPORTED,
   DISPLAYLIST_CORES_UPDATER,
   DISPLAYLIST_THUMBNAILS_UPDATER,
   DISPLAYLIST_LAKKA,
   DISPLAYLIST_CORES_DETECTED,
   DISPLAYLIST_CORE_OPTIONS,
   DISPLAYLIST_CORE_INFO,
   DISPLAYLIST_PERFCOUNTERS_CORE,
   DISPLAYLIST_PERFCOUNTERS_FRONTEND,
   DISPLAYLIST_SHADER_PASS,
   DISPLAYLIST_SHADER_PRESET,
   DISPLAYLIST_DATABASES,
   DISPLAYLIST_DATABASE_CURSORS,
   DISPLAYLIST_DATABASE_PLAYLISTS,
   DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL,
   DISPLAYLIST_DATABASE_QUERY,
   DISPLAYLIST_DATABASE_ENTRY,
   DISPLAYLIST_AUDIO_FILTERS,
   DISPLAYLIST_VIDEO_FILTERS,
   DISPLAYLIST_CHEAT_FILES,
   DISPLAYLIST_REMAP_FILES,
   DISPLAYLIST_RECORD_CONFIG_FILES,
   DISPLAYLIST_CONFIG_FILES,
   DISPLAYLIST_CONTENT_HISTORY,
   DISPLAYLIST_IMAGES,
   DISPLAYLIST_FONTS,
   DISPLAYLIST_OVERLAYS,
   DISPLAYLIST_SHADER_PARAMETERS,
   DISPLAYLIST_SHADER_PARAMETERS_PRESET,
   DISPLAYLIST_NETWORK_INFO,
   DISPLAYLIST_SYSTEM_INFO,
   DISPLAYLIST_ACHIEVEMENT_LIST,
      DISPLAYLIST_USER_BINDS_LIST,
   DISPLAYLIST_ACCOUNTS_LIST,
   DISPLAYLIST_MIXER_STREAM_SETTINGS_LIST,
   DISPLAYLIST_DRIVER_SETTINGS_LIST,
   DISPLAYLIST_VIDEO_SETTINGS_LIST,
   DISPLAYLIST_CONFIGURATION_SETTINGS_LIST,
   DISPLAYLIST_SAVING_SETTINGS_LIST,
   DISPLAYLIST_LOGGING_SETTINGS_LIST,
   DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST,
   DISPLAYLIST_REWIND_SETTINGS_LIST,
   DISPLAYLIST_CHEAT_DETAILS_SETTINGS_LIST,
   DISPLAYLIST_CHEAT_SEARCH_SETTINGS_LIST,
   DISPLAYLIST_AUDIO_SETTINGS_LIST,
   DISPLAYLIST_AUDIO_MIXER_SETTINGS_LIST,
   DISPLAYLIST_CORE_SETTINGS_LIST,
   DISPLAYLIST_INPUT_SETTINGS_LIST,
   DISPLAYLIST_LATENCY_SETTINGS_LIST,
   DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST,
   DISPLAYLIST_ONSCREEN_OVERLAY_SETTINGS_LIST,
   DISPLAYLIST_ONSCREEN_DISPLAY_SETTINGS_LIST,
   DISPLAYLIST_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST,
   DISPLAYLIST_MENU_FILE_BROWSER_SETTINGS_LIST,
   DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST,
   DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST,
   DISPLAYLIST_MENU_SETTINGS_LIST,
   DISPLAYLIST_USER_INTERFACE_SETTINGS_LIST,
   DISPLAYLIST_POWER_MANAGEMENT_SETTINGS_LIST,
   DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST,
   DISPLAYLIST_UPDATER_SETTINGS_LIST,
   DISPLAYLIST_WIFI_SETTINGS_LIST,
   DISPLAYLIST_NETWORK_SETTINGS_LIST,
   DISPLAYLIST_NETPLAY_LAN_SCAN_SETTINGS_LIST,
   DISPLAYLIST_LAKKA_SERVICES_LIST,
   DISPLAYLIST_USER_SETTINGS_LIST,
   DISPLAYLIST_DIRECTORY_SETTINGS_LIST,
   DISPLAYLIST_PRIVACY_SETTINGS_LIST,
   DISPLAYLIST_MIDI_SETTINGS_LIST,
   DISPLAYLIST_RECORDING_SETTINGS_LIST,
   DISPLAYLIST_PLAYLIST_SETTINGS_LIST,
   DISPLAYLIST_ACCOUNTS_CHEEVOS_LIST,
   DISPLAYLIST_BROWSE_URL_LIST,
   DISPLAYLIST_BROWSE_URL_START,
   DISPLAYLIST_LOAD_CONTENT_LIST,
   DISPLAYLIST_LOAD_CONTENT_SPECIAL,
   DISPLAYLIST_INFORMATION_LIST,
   DISPLAYLIST_CONTENT_SETTINGS,
   DISPLAYLIST_OPTIONS,
   DISPLAYLIST_OPTIONS_CHEATS,
   DISPLAYLIST_OPTIONS_REMAPPINGS,
   DISPLAYLIST_OPTIONS_MANAGEMENT,
   DISPLAYLIST_OPTIONS_DISK,
   DISPLAYLIST_OPTIONS_SHADERS,
   DISPLAYLIST_OPTIONS_OVERRIDES,
   DISPLAYLIST_NETPLAY,
   DISPLAYLIST_ADD_CONTENT_LIST,
   DISPLAYLIST_CONFIGURATIONS_LIST,
   DISPLAYLIST_SCAN_DIRECTORY_LIST,
   DISPLAYLIST_NETPLAY_ROOM_LIST,
   DISPLAYLIST_ARCHIVE_ACTION,
   DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE,
   DISPLAYLIST_CORE_CONTENT,
   DISPLAYLIST_CORE_CONTENT_DIRS,
   DISPLAYLIST_CORE_CONTENT_DIRS_SUBDIR,
   DISPLAYLIST_PENDING_CLEAR
};

typedef struct menu_displaylist_info
{
   enum msg_hash_enums enum_idx;
   /* should the displaylist be sorted by alphabet? */
   bool need_sort;
   bool need_refresh;
   bool need_entries_refresh;
   bool need_push;
   bool need_push_no_playlist_entries;
   /* should we clear the displaylist before we push
    * entries onto it? */
   bool need_clear;
   bool push_builtin_cores;
   /* Should a 'download core' entry be pushed onto the list?
    * This will be set to true in case there are no currently
    * installed cores. */
   bool download_core;
   /* does the navigation index need to be cleared to 0 (first entry) ? */
   bool need_navigation_clear;

   char *path;
   char *path_b;
   char *path_c;
   char *exts;
   char *label;
   unsigned type;
   unsigned type_default;
   unsigned flags;
   size_t directory_ptr;
   file_list_t *list;
   file_list_t *menu_list;
   rarch_setting_t *setting;
} menu_displaylist_info_t;

typedef struct menu_displaylist_ctx_parse_entry
{
   enum msg_hash_enums enum_idx;
   enum menu_displaylist_parse_type parse_type;
   bool add_empty_entry;
   const char *info_label;
   void *data;
   menu_displaylist_info_t *info;
} menu_displaylist_ctx_parse_entry_t;

typedef struct menu_displaylist_ctx_entry
{
   file_list_t *stack;
   file_list_t *list;
} menu_displaylist_ctx_entry_t;

bool menu_displaylist_process(menu_displaylist_info_t *info);

bool menu_displaylist_push(menu_displaylist_ctx_entry_t *entry);

void menu_displaylist_info_free(menu_displaylist_info_t *info);

void menu_displaylist_info_init(menu_displaylist_info_t *info);

bool menu_displaylist_ctl(enum menu_displaylist_ctl_state type, void *data);

#ifdef HAVE_NETWORKING
void netplay_refresh_rooms_menu(file_list_t *list);
#endif

RETRO_END_DECLS

#endif

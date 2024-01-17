/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2015-2017 - Andrés Suárez
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <ctype.h>

#include <libretro.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_assert.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "file_path_special.h"
#include "audio/audio_driver.h"
#include "input/input_driver.h"
#include "configuration.h"
#include "content.h"
#include "config.def.h"
#include "config.features.h"
#include "input/input_keymaps.h"
#include "input/input_remapping.h"
#include "led/led_defines.h"
#include "defaults.h"
#include "core.h"
#include "dirs.h"
#include "paths.h"
#include "retroarch.h"
#include "verbosity.h"
#include "lakka.h"

#include "tasks/tasks_internal.h"

#include "../list_special.h"

static const char* invalid_filename_chars[] = {
   /* https://support.microsoft.com/en-us/help/905231/information-about-the-characters-that-you-cannot-use-in-site-names--fo */
   "~", "#", "%", "&", "*", "{", "}", "\\", ":", "[", "]", "?", "/", "|", "\'", "\"",
   NULL
};

/* All config related settings go here. */

struct config_bool_setting
{
   const char *ident;
   bool *ptr;
   bool def_enable;
   bool def;
   bool handle;
   enum rarch_override_setting override;
};

struct config_int_setting
{
   const char *ident;
   int *ptr;
   bool def_enable;
   int def;
   bool handle;
   enum rarch_override_setting override;
};

struct config_uint_setting
{
   const char *ident;
   unsigned *ptr;
   bool def_enable;
   unsigned def;
   bool handle;
   enum rarch_override_setting override;
};

struct config_size_setting
{
   const char *ident;
   size_t *ptr;
   bool def_enable;
   size_t def;
   bool handle;
   enum rarch_override_setting override;
};

struct config_float_setting
{
   const char *ident;
   float *ptr;
   bool def_enable;
   float def;
   bool handle;
   enum rarch_override_setting override;
};

struct config_array_setting
{
   const char *ident;
   char *ptr;
   bool def_enable;
   const char *def;
   bool handle;
   enum rarch_override_setting override;
};

struct config_path_setting
{
   const char *ident;
   char *ptr;
   bool def_enable;
   char *def;
   bool handle;
};

enum video_driver_enum
{
   VIDEO_GL                 = 0,
   VIDEO_VULKAN,
   VIDEO_METAL,
   VIDEO_DRM,
   VIDEO_XVIDEO,
   VIDEO_SDL,
   VIDEO_SDL2,
   VIDEO_EXT,
   VIDEO_WII,
   VIDEO_WIIU,
   VIDEO_XENON360,
   VIDEO_PSP1,
   VIDEO_VITA2D,
   VIDEO_CTR,
   VIDEO_SWITCH,
   VIDEO_D3D8,
   VIDEO_D3D9,
   VIDEO_D3D10,
   VIDEO_D3D11,
   VIDEO_D3D12,
   VIDEO_VG,
   VIDEO_OMAP,
   VIDEO_EXYNOS,
   VIDEO_SUNXI,
   VIDEO_DISPMANX,
   VIDEO_CACA,
   VIDEO_GDI,
   VIDEO_VGA,
   VIDEO_NULL
};

enum audio_driver_enum
{
   AUDIO_RSOUND             = VIDEO_NULL + 1,
   AUDIO_OSS,
   AUDIO_ALSA,
   AUDIO_ALSATHREAD,
   AUDIO_TINYALSA,
   AUDIO_ROAR,
   AUDIO_AL,
   AUDIO_SL,
   AUDIO_JACK,
   AUDIO_SDL,
   AUDIO_SDL2,
   AUDIO_XAUDIO,
   AUDIO_PULSE,
   AUDIO_EXT,
   AUDIO_DSOUND,
   AUDIO_WASAPI,
   AUDIO_COREAUDIO,
   AUDIO_PS3,
   AUDIO_XENON360,
   AUDIO_WII,
   AUDIO_WIIU,
   AUDIO_RWEBAUDIO,
   AUDIO_PSP,
   AUDIO_CTR,
   AUDIO_SWITCH,
   AUDIO_NULL
};

enum audio_resampler_driver_enum
{
   AUDIO_RESAMPLER_CC       = AUDIO_NULL + 1,
   AUDIO_RESAMPLER_SINC,
   AUDIO_RESAMPLER_NEAREST,
   AUDIO_RESAMPLER_NULL
};

enum input_driver_enum
{
   INPUT_ANDROID            = AUDIO_RESAMPLER_NULL + 1,
   INPUT_SDL,
   INPUT_SDL2,
   INPUT_X,
   INPUT_WAYLAND,
   INPUT_DINPUT,
   INPUT_PS3,
   INPUT_PSP,
   INPUT_CTR,
   INPUT_SWITCH,
   INPUT_XENON360,
   INPUT_WII,
   INPUT_WIIU,
   INPUT_XINPUT,
   INPUT_UDEV,
   INPUT_LINUXRAW,
   INPUT_COCOA,
   INPUT_QNX,
   INPUT_RWEBINPUT,
   INPUT_DOS,
   INPUT_NULL
};

enum joypad_driver_enum
{
   JOYPAD_PS3               = INPUT_NULL + 1,
   JOYPAD_XINPUT,
   JOYPAD_GX,
   JOYPAD_WIIU,
   JOYPAD_XDK,
   JOYPAD_PSP,
   JOYPAD_CTR,
   JOYPAD_SWITCH,
   JOYPAD_DINPUT,
   JOYPAD_UDEV,
   JOYPAD_LINUXRAW,
   JOYPAD_ANDROID,
   JOYPAD_SDL,
   JOYPAD_DOS,
   JOYPAD_HID,
   JOYPAD_QNX,
   JOYPAD_RWEBPAD,
   JOYPAD_MFI,
   JOYPAD_NULL
};

enum camera_driver_enum
{
   CAMERA_V4L2              = JOYPAD_NULL + 1,
   CAMERA_RWEBCAM,
   CAMERA_ANDROID,
   CAMERA_AVFOUNDATION,
   CAMERA_NULL
};

enum wifi_driver_enum
{
   WIFI_CONNMANCTL          = CAMERA_NULL + 1,
   WIFI_NULL
};

enum location_driver_enum
{
   LOCATION_ANDROID         = WIFI_NULL + 1,
   LOCATION_CORELOCATION,
   LOCATION_NULL
};

enum osk_driver_enum
{
   OSK_PS3                  = LOCATION_NULL + 1,
   OSK_NULL
};

enum menu_driver_enum
{
   MENU_RGUI                = OSK_NULL + 1,
   MENU_XUI,
   MENU_MATERIALUI,
   MENU_XMB,
   MENU_STRIPES,
   MENU_NUKLEAR,
   MENU_NULL
};

enum record_driver_enum
{
   RECORD_FFMPEG            = MENU_NULL + 1,
   RECORD_NULL
};

enum midi_driver_enum
{
   MIDI_WINMM               = RECORD_NULL + 1,
   MIDI_NULL
};

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) || defined(__CELLOS_LV2__)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GL;
#elif defined(HAVE_METAL)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_METAL;
#elif defined(GEKKO)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_WII;
#elif defined(WIIU)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_WIIU;
#elif defined(XENON)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_XENON360;
#elif defined(HAVE_D3D9)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D9;
#elif defined(HAVE_D3D8)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D8;
#elif defined(HAVE_VG)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_VG;
#elif defined(HAVE_VITA2D)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_VITA2D;
#elif defined(PSP)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_PSP1;
#elif defined(_3DS)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_CTR;
#elif defined(SWITCH)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SWITCH;
#elif defined(HAVE_XVIDEO)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_XVIDEO;
#elif defined(HAVE_SDL)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SDL;
#elif defined(HAVE_SDL2)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SDL2;
#elif defined(_WIN32) && !defined(_XBOX)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GDI;
#elif defined(DJGPP)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_VGA;
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_EXT;
#else
static enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_NULL;
#endif

#if defined(__CELLOS_LV2__)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_PS3;
#elif defined(XENON)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_XENON360;
#elif defined(GEKKO)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_WII;
#elif defined(WIIU)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_WIIU;
#elif defined(PSP) || defined(VITA)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_PSP;
#elif defined(_3DS)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_CTR;
#elif defined(SWITCH)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SWITCH;
#elif defined(HAVE_PULSE)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_PULSE;
#elif defined(HAVE_ALSA) && defined(HAVE_VIDEOCORE)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_ALSATHREAD;
#elif defined(HAVE_ALSA)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_ALSA;
#elif defined(HAVE_TINYALSA)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_TINYALSA;
#elif defined(HAVE_OSS)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_OSS;
#elif defined(HAVE_JACK)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_JACK;
#elif defined(HAVE_COREAUDIO)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_COREAUDIO;
#elif defined(HAVE_XAUDIO)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_XAUDIO;
#elif defined(HAVE_DSOUND)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_DSOUND;
#elif defined(HAVE_WASAPI)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_WASAPI;
#elif defined(HAVE_AL)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_AL;
#elif defined(HAVE_SL)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SL;
#elif defined(EMSCRIPTEN)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_RWEBAUDIO;
#elif defined(HAVE_SDL)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SDL;
#elif defined(HAVE_SDL2)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SDL2;
#elif defined(HAVE_RSOUND)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_RSOUND;
#elif defined(HAVE_ROAR)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_ROAR;
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_EXT;
#else
static enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_NULL;
#endif

#if defined(PSP) || defined(EMSCRIPTEN)
static enum audio_resampler_driver_enum AUDIO_DEFAULT_RESAMPLER_DRIVER = AUDIO_RESAMPLER_CC;
#else
static enum audio_resampler_driver_enum AUDIO_DEFAULT_RESAMPLER_DRIVER = AUDIO_RESAMPLER_SINC;
#endif

#if defined(HAVE_FFMPEG)
static enum record_driver_enum RECORD_DEFAULT_DRIVER = RECORD_FFMPEG;
#else
static enum record_driver_enum RECORD_DEFAULT_DRIVER = RECORD_NULL;
#endif

#ifdef HAVE_WINMM
static enum midi_driver_enum MIDI_DEFAULT_DRIVER = MIDI_WINMM;
#else
static enum midi_driver_enum MIDI_DEFAULT_DRIVER = MIDI_NULL;
#endif

#if defined(XENON)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_XENON360;
#elif defined(_XBOX360) || defined(_XBOX) || defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_XINPUT;
#elif defined(ANDROID)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_ANDROID;
#elif defined(EMSCRIPTEN) && defined(HAVE_SDL2)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL2;
#elif defined(EMSCRIPTEN)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_RWEBINPUT;
#elif defined(_WIN32)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_DINPUT;
#elif defined(__CELLOS_LV2__)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_PS3;
#elif defined(PSP) || defined(VITA)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_PSP;
#elif defined(_3DS)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_CTR;
#elif defined(SWITCH)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SWITCH;
#elif defined(GEKKO)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_WII;
#elif defined(WIIU)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_WIIU;
#elif defined(HAVE_X11)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_X;
#elif defined(HAVE_UDEV)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_UDEV;
#elif defined(__linux__) && !defined(ANDROID)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_LINUXRAW;
#elif defined(HAVE_WAYLAND)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_WAYLAND;
#elif defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_COCOA;
#elif defined(__QNX__)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_QNX;
#elif defined(HAVE_SDL)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL;
#elif defined(HAVE_SDL2)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL2;
#elif defined(DJGPP)
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_DOS;
#else
static enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_NULL;
#endif

#if defined(__CELLOS_LV2__)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_PS3;
#elif defined(HAVE_XINPUT)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_XINPUT;
#elif defined(GEKKO)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_GX;
#elif defined(WIIU)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_WIIU;
#elif defined(_XBOX)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_XDK;
#elif defined(PSP) || defined(VITA)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_PSP;
#elif defined(_3DS)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_CTR;
#elif defined(SWITCH)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_SWITCH;
#elif defined(HAVE_DINPUT)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_DINPUT;
#elif defined(HAVE_UDEV)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_UDEV;
#elif defined(__linux) && !defined(ANDROID)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_LINUXRAW;
#elif defined(ANDROID)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_ANDROID;
#elif defined(HAVE_SDL) || defined(HAVE_SDL2)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_SDL;
#elif defined(DJGPP)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_DOS;
#elif defined(HAVE_HID)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_HID;
#elif defined(__QNX__)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_QNX;
#elif defined(EMSCRIPTEN)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_RWEBPAD;
#elif defined(IOS)
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_MFI;
#else
static enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_NULL;
#endif

#if defined(HAVE_V4L2)
static enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_V4L2;
#elif defined(EMSCRIPTEN)
static enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_RWEBCAM;
#elif defined(ANDROID)
static enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_ANDROID;
#elif defined(HAVE_AVFOUNDATION) && (defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH))
static enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_AVFOUNDATION;
#else
static enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_NULL;
#endif

#if defined(HAVE_LAKKA)
static enum wifi_driver_enum WIFI_DEFAULT_DRIVER = WIFI_CONNMANCTL;
#else
static enum wifi_driver_enum WIFI_DEFAULT_DRIVER = WIFI_NULL;
#endif

#if defined(ANDROID)
static enum location_driver_enum LOCATION_DEFAULT_DRIVER = LOCATION_ANDROID;
#elif defined(HAVE_CORELOCATION) && (defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH))
static enum location_driver_enum LOCATION_DEFAULT_DRIVER = LOCATION_CORELOCATION;
#else
static enum location_driver_enum LOCATION_DEFAULT_DRIVER = LOCATION_NULL;
#endif

#if defined(HAVE_XUI)
static enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_XUI;
#elif defined(HAVE_MATERIALUI) && defined(RARCH_MOBILE)
static enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_MATERIALUI;
#elif defined(HAVE_XMB) && !defined(_XBOX)
static enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_XMB;
#elif defined(HAVE_RGUI)
static enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_RGUI;
#else
static enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_NULL;
#endif


#define GENERAL_SETTING(key, configval, default_enable, default_setting, type, handle_setting) \
{ \
   tmp[count].ident      = key; \
   tmp[count].ptr        = configval; \
   tmp[count].def_enable = default_enable; \
   if (default_enable) \
      tmp[count].def     = default_setting; \
   tmp[count].handle   = handle_setting; \
   count++; \
}

#define SETTING_BOOL(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_bool_setting, handle_setting)

#define SETTING_FLOAT(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_float_setting, handle_setting)

#define SETTING_INT(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_int_setting, handle_setting)

#define SETTING_UINT(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_uint_setting, handle_setting)

#define SETTING_SIZE(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_size_setting, handle_setting)

#define SETTING_PATH(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_path_setting, handle_setting)

#define SETTING_ARRAY(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_array_setting, handle_setting)

#define SETTING_OVERRIDE(override_setting) \
   tmp[count-1].override = override_setting

struct defaults g_defaults;
static settings_t *configuration_settings = NULL;

settings_t *config_get_ptr(void)
{
   return configuration_settings;
}

void config_free(void)
{
   free(configuration_settings);
   configuration_settings = NULL;
}

bool config_init(void)
{
   configuration_settings = (settings_t*)calloc(1, sizeof(settings_t));

   if (!configuration_settings)
      return false;
   return true;
}

/**
 * config_get_default_audio:
 *
 * Gets default audio driver.
 *
 * Returns: Default audio driver.
 **/
const char *config_get_default_audio(void)
{
   enum audio_driver_enum default_driver = AUDIO_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case AUDIO_RSOUND:
         return "rsound";
      case AUDIO_OSS:
         return "oss";
      case AUDIO_ALSA:
         return "alsa";
      case AUDIO_ALSATHREAD:
         return "alsathread";
      case AUDIO_TINYALSA:
         return "tinyalsa";
      case AUDIO_ROAR:
         return "roar";
      case AUDIO_COREAUDIO:
         return "coreaudio";
      case AUDIO_AL:
         return "openal";
      case AUDIO_SL:
         return "opensl";
      case AUDIO_SDL:
         return "sdl";
      case AUDIO_SDL2:
         return "sdl2";
      case AUDIO_DSOUND:
         return "dsound";
      case AUDIO_WASAPI:
         return "wasapi";
      case AUDIO_XAUDIO:
         return "xaudio";
      case AUDIO_PULSE:
         return "pulse";
      case AUDIO_EXT:
         return "ext";
      case AUDIO_XENON360:
         return "xenon360";
      case AUDIO_PS3:
         return "ps3";
      case AUDIO_WII:
         return "gx";
      case AUDIO_WIIU:
         return "AX";
      case AUDIO_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case AUDIO_CTR:
         return "csnd";
      case AUDIO_SWITCH:
         return "switch";
      case AUDIO_RWEBAUDIO:
         return "rwebaudio";
      case AUDIO_JACK:
         return "jack";
      case AUDIO_NULL:
         break;
   }

   return "null";
}

const char *config_get_default_record(void)
{
   enum record_driver_enum default_driver = RECORD_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case RECORD_FFMPEG:
         return "ffmpeg";
      case RECORD_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_audio_resampler:
 *
 * Gets default audio resampler driver.
 *
 * Returns: Default audio resampler driver.
 **/
const char *config_get_default_audio_resampler(void)
{
   enum audio_resampler_driver_enum default_driver = AUDIO_DEFAULT_RESAMPLER_DRIVER;

   switch (default_driver)
   {
      case AUDIO_RESAMPLER_CC:
         return "cc";
      case AUDIO_RESAMPLER_SINC:
         return "sinc";
      case AUDIO_RESAMPLER_NEAREST:
         return "nearest";
      case AUDIO_RESAMPLER_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_video:
 *
 * Gets default video driver.
 *
 * Returns: Default video driver.
 **/
const char *config_get_default_video(void)
{
   enum video_driver_enum default_driver = VIDEO_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case VIDEO_GL:
         return "gl";
      case VIDEO_VULKAN:
         return "vulkan";
      case VIDEO_METAL:
         return "metal";
      case VIDEO_DRM:
         return "drm";
      case VIDEO_WII:
         return "gx";
      case VIDEO_WIIU:
         return "gx2";
      case VIDEO_XENON360:
         return "xenon360";
      case VIDEO_D3D8:
         return "d3d8";
      case VIDEO_D3D9:
         return "d3d9";
      case VIDEO_D3D10:
         return "d3d10";
      case VIDEO_D3D11:
         return "d3d11";
      case VIDEO_D3D12:
         return "d3d12";
      case VIDEO_PSP1:
         return "psp1";
      case VIDEO_VITA2D:
         return "vita2d";
      case VIDEO_CTR:
         return "ctr";
      case VIDEO_SWITCH:
         return "switch";
      case VIDEO_XVIDEO:
         return "xvideo";
      case VIDEO_SDL:
         return "sdl";
      case VIDEO_SDL2:
         return "sdl2";
      case VIDEO_EXT:
         return "ext";
      case VIDEO_VG:
         return "vg";
      case VIDEO_OMAP:
         return "omap";
      case VIDEO_EXYNOS:
         return "exynos";
      case VIDEO_DISPMANX:
         return "dispmanx";
      case VIDEO_SUNXI:
         return "sunxi";
      case VIDEO_CACA:
         return "caca";
      case VIDEO_GDI:
         return "gdi";
      case VIDEO_VGA:
         return "vga";
      case VIDEO_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_input:
 *
 * Gets default input driver.
 *
 * Returns: Default input driver.
 **/
const char *config_get_default_input(void)
{
   enum input_driver_enum default_driver = INPUT_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case INPUT_ANDROID:
         return "android";
      case INPUT_PS3:
         return "ps3";
      case INPUT_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case INPUT_CTR:
         return "ctr";
      case INPUT_SWITCH:
         return "switch";
      case INPUT_SDL:
         return "sdl";
      case INPUT_SDL2:
         return "sdl2";
      case INPUT_DINPUT:
         return "dinput";
      case INPUT_X:
         return "x";
      case INPUT_WAYLAND:
         return "wayland";
      case INPUT_XENON360:
         return "xenon360";
      case INPUT_XINPUT:
         return "xinput";
      case INPUT_WII:
         return "gx";
      case INPUT_WIIU:
         return "wiiu";
      case INPUT_LINUXRAW:
         return "linuxraw";
      case INPUT_UDEV:
         return "udev";
      case INPUT_COCOA:
         return "cocoa";
      case INPUT_QNX:
          return "qnx_input";
      case INPUT_RWEBINPUT:
          return "rwebinput";
      case INPUT_DOS:
         return "dos";
      case INPUT_NULL:
          break;
   }

   return "null";
}

/**
 * config_get_default_joypad:
 *
 * Gets default input joypad driver.
 *
 * Returns: Default input joypad driver.
 **/
const char *config_get_default_joypad(void)
{
   enum joypad_driver_enum default_driver = JOYPAD_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case JOYPAD_PS3:
         return "ps3";
      case JOYPAD_XINPUT:
         return "xinput";
      case JOYPAD_GX:
         return "gx";
      case JOYPAD_WIIU:
         return "wiiu";
      case JOYPAD_XDK:
         return "xdk";
      case JOYPAD_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case JOYPAD_CTR:
         return "ctr";
      case JOYPAD_SWITCH:
         return "switch";
      case JOYPAD_DINPUT:
         return "dinput";
      case JOYPAD_UDEV:
         return "udev";
      case JOYPAD_LINUXRAW:
         return "linuxraw";
      case JOYPAD_ANDROID:
         return "android";
      case JOYPAD_SDL:
#ifdef HAVE_SDL2
         return "sdl2";
#else
         return "sdl";
#endif
      case JOYPAD_HID:
         return "hid";
      case JOYPAD_QNX:
         return "qnx";
      case JOYPAD_RWEBPAD:
         return "rwebpad";
      case JOYPAD_DOS:
         return "dos";
      case JOYPAD_MFI:
         return "mfi";
      case JOYPAD_NULL:
         break;
   }

   return "null";
}


/**
 * config_get_default_camera:
 *
 * Gets default camera driver.
 *
 * Returns: Default camera driver.
 **/
const char *config_get_default_camera(void)
{
   enum camera_driver_enum default_driver = CAMERA_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case CAMERA_V4L2:
         return "video4linux2";
      case CAMERA_RWEBCAM:
         return "rwebcam";
      case CAMERA_ANDROID:
         return "android";
      case CAMERA_AVFOUNDATION:
         return "avfoundation";
      case CAMERA_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_wifi:
 *
 * Gets default wifi driver.
 *
 * Returns: Default wifi driver.
 **/
const char *config_get_default_wifi(void)
{
   enum wifi_driver_enum default_driver = WIFI_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case WIFI_CONNMANCTL:
         return "connmanctl";
      case WIFI_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_led:
 *
 * Gets default led driver.
 *
 * Returns: Default led driver.
 **/
const char *config_get_default_led(void)
{
   return "null";
}

/**
 * config_get_default_location:
 *
 * Gets default location driver.
 *
 * Returns: Default location driver.
 **/
const char *config_get_default_location(void)
{
   enum location_driver_enum default_driver = LOCATION_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case LOCATION_ANDROID:
         return "android";
      case LOCATION_CORELOCATION:
         return "corelocation";
      case LOCATION_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_menu:
 *
 * Gets default menu driver.
 *
 * Returns: Default menu driver.
 **/
const char *config_get_default_menu(void)
{
#ifdef HAVE_MENU
   enum menu_driver_enum default_driver = MENU_DEFAULT_DRIVER;

   if (!string_is_empty(g_defaults.settings.menu))
      return g_defaults.settings.menu;

   switch (default_driver)
   {
      case MENU_RGUI:
         return "rgui";
      case MENU_XUI:
         return "xui";
      case MENU_MATERIALUI:
         return "glui";
      case MENU_XMB:
         return "xmb";
      case MENU_STRIPES:
         return "stripes";
      case MENU_NUKLEAR:
         return "nuklear";
      case MENU_NULL:
         break;
   }
#endif

   return "null";
}

const char *config_get_default_midi(void)
{
   enum midi_driver_enum default_driver = MIDI_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case MIDI_WINMM:
         return "winmm";
      case MIDI_NULL:
         break;
   }

   return "null";
}

const char *config_get_midi_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MIDI_DRIVERS, NULL);
}

bool config_overlay_enable_default(void)
{
   if (g_defaults.overlay.set)
      return g_defaults.overlay.enable;
   return true;
}

static struct config_array_setting *populate_settings_array(settings_t *settings, int *size)
{
   unsigned count                        = 0;
   struct config_array_setting  *tmp    = (struct config_array_setting*)calloc(1, (*size + 1) * sizeof(struct config_array_setting));

   /* Arrays */
   SETTING_ARRAY("playlist_names",           settings->arrays.playlist_names, false, NULL, true);
   SETTING_ARRAY("playlist_cores",           settings->arrays.playlist_cores, false, NULL, true);
   SETTING_ARRAY("video_driver",             settings->arrays.video_driver,   false, NULL, true);
   SETTING_ARRAY("record_driver",            settings->arrays.record_driver,  false, NULL, true);
   SETTING_ARRAY("camera_driver",            settings->arrays.camera_driver,  false, NULL, true);
   SETTING_ARRAY("wifi_driver",              settings->arrays.wifi_driver,    false, NULL, true);
   SETTING_ARRAY("location_driver",          settings->arrays.location_driver,false, NULL, true);
#ifdef HAVE_MENU
   SETTING_ARRAY("menu_driver",              settings->arrays.menu_driver,    false, NULL, true);
#endif
   SETTING_ARRAY("audio_device",             settings->arrays.audio_device,   false, NULL, true);
   SETTING_ARRAY("camera_device",            settings->arrays.camera_device,  false, NULL, true);
#ifdef HAVE_CHEEVOS
   SETTING_ARRAY("cheevos_username",         settings->arrays.cheevos_username, false, NULL, true);
   SETTING_ARRAY("cheevos_password",         settings->arrays.cheevos_password, false, NULL, true);
   SETTING_ARRAY("cheevos_token",            settings->arrays.cheevos_token, false, NULL, true);
#endif
   SETTING_ARRAY("video_context_driver",     settings->arrays.video_context_driver,   false, NULL, true);
   SETTING_ARRAY("audio_driver",             settings->arrays.audio_driver,           false, NULL, true);
   SETTING_ARRAY("audio_resampler",          settings->arrays.audio_resampler,        false, NULL, true);
   SETTING_ARRAY("input_driver",             settings->arrays.input_driver,           false, NULL, true);
   SETTING_ARRAY("input_joypad_driver",      settings->arrays.input_joypad_driver,    false, NULL, true);
   SETTING_ARRAY("input_keyboard_layout",    settings->arrays.input_keyboard_layout,  false, NULL, true);
   SETTING_ARRAY("bundle_assets_src_path",   settings->arrays.bundle_assets_src, false, NULL, true);
   SETTING_ARRAY("bundle_assets_dst_path",   settings->arrays.bundle_assets_dst, false, NULL, true);
   SETTING_ARRAY("bundle_assets_dst_path_subdir", settings->arrays.bundle_assets_dst_subdir, false, NULL, true);
   SETTING_ARRAY("led_driver",               settings->arrays.led_driver, false, NULL, true);
   SETTING_ARRAY("netplay_mitm_server",      settings->arrays.netplay_mitm_server, false, NULL, true);
   SETTING_ARRAY("midi_driver",              settings->arrays.midi_driver, false, NULL, true);
   SETTING_ARRAY("midi_input",               settings->arrays.midi_input, true, midi_input, true);
   SETTING_ARRAY("midi_output",              settings->arrays.midi_output, true, midi_output, true);
   *size = count;

   return tmp;
}

static struct config_path_setting *populate_settings_path(settings_t *settings, int *size)
{
   unsigned count = 0;
   global_t   *global                  = global_get_ptr();
   struct config_path_setting  *tmp    = (struct config_path_setting*)calloc(1, (*size + 1) * sizeof(struct config_path_setting));

   /* Paths */
#ifdef HAVE_XMB
   SETTING_PATH("xmb_font",                   settings->paths.path_menu_xmb_font, false, NULL, true);
#endif
   SETTING_PATH("content_show_settings_password", settings->paths.menu_content_show_settings_password, false, NULL, true);
   SETTING_PATH("kiosk_mode_password",        settings->paths.kiosk_mode_password, false, NULL, true);
   SETTING_PATH("netplay_nickname",           settings->paths.username, false, NULL, true);
   SETTING_PATH("video_filter",               settings->paths.path_softfilter_plugin, false, NULL, true);
   SETTING_PATH("audio_dsp_plugin",           settings->paths.path_audio_dsp_plugin, false, NULL, true);
   SETTING_PATH("core_updater_buildbot_url", settings->paths.network_buildbot_url, false, NULL, true);
   SETTING_PATH("core_updater_buildbot_assets_url", settings->paths.network_buildbot_assets_url, false, NULL, true);
#ifdef HAVE_NETWORKING
   SETTING_PATH("netplay_ip_address",       settings->paths.netplay_server, false, NULL, true);
   SETTING_PATH("netplay_password",           settings->paths.netplay_password, false, NULL, true);
   SETTING_PATH("netplay_spectate_password",  settings->paths.netplay_spectate_password, false, NULL, true);
#endif
   SETTING_PATH("libretro_directory",
         settings->paths.directory_libretro, false, NULL, false);
   SETTING_PATH("core_options_path",
         settings->paths.path_core_options, false, NULL, true);
   SETTING_PATH("libretro_info_path",
         settings->paths.path_libretro_info, false, NULL, true);
   SETTING_PATH("video_shader",
         settings->paths.path_shader, false, NULL, true);
   SETTING_PATH("content_database_path",
         settings->paths.path_content_database, false, NULL, true);
   SETTING_PATH("cheat_database_path",
         settings->paths.path_cheat_database, false, NULL, true);
#ifdef HAVE_MENU
   SETTING_PATH("menu_wallpaper",
         settings->paths.path_menu_wallpaper, false, NULL, true);
#endif
   SETTING_PATH("content_history_path",
         settings->paths.path_content_history, false, NULL, true);
   SETTING_PATH("content_favorites_path",
         settings->paths.path_content_favorites, false, NULL, true);
   SETTING_PATH("content_music_history_path",
         settings->paths.path_content_music_history, false, NULL, true);
   SETTING_PATH("content_video_history_path",
         settings->paths.path_content_video_history, false, NULL, true);
   SETTING_PATH("content_image_history_path",
         settings->paths.path_content_image_history, false, NULL, true);
#ifdef HAVE_OVERLAY
   SETTING_PATH("input_overlay",
         settings->paths.path_overlay, false, NULL, true);
#endif
   SETTING_PATH("video_font_path",
         settings->paths.path_font, false, NULL, true);
   SETTING_PATH("cursor_directory",
         settings->paths.directory_cursor, false, NULL, true);
   SETTING_PATH("content_history_dir",
         settings->paths.directory_content_history, false, NULL, true);
   SETTING_PATH("screenshot_directory",
         settings->paths.directory_screenshot, true, NULL, true);
   SETTING_PATH("system_directory",
         settings->paths.directory_system, true, NULL, true);
   SETTING_PATH("cache_directory",
         settings->paths.directory_cache, false, NULL, true);
   SETTING_PATH("input_remapping_directory",
         settings->paths.directory_input_remapping, false, NULL, true);
   SETTING_PATH("resampler_directory",
         settings->paths.directory_resampler, false, NULL, true);
   SETTING_PATH("video_shader_dir",
         settings->paths.directory_video_shader, true, NULL, true);
   SETTING_PATH("video_filter_dir",
         settings->paths.directory_video_filter, true, NULL, true);
   SETTING_PATH("core_assets_directory",
         settings->paths.directory_core_assets, true, NULL, true);
   SETTING_PATH("assets_directory",
         settings->paths.directory_assets, true, NULL, true);
   SETTING_PATH("dynamic_wallpapers_directory",
         settings->paths.directory_dynamic_wallpapers, true, NULL, true);
   SETTING_PATH("thumbnails_directory",
         settings->paths.directory_thumbnails, true, NULL, true);
   SETTING_PATH("playlist_directory",
         settings->paths.directory_playlist, true, NULL, true);
   SETTING_PATH("joypad_autoconfig_dir",
         settings->paths.directory_autoconfig, false, NULL, true);
   SETTING_PATH("audio_filter_dir",
         settings->paths.directory_audio_filter, true, NULL, true);
   SETTING_PATH("savefile_directory",
         dir_get_ptr(RARCH_DIR_SAVEFILE), true, NULL, false);
   SETTING_PATH("savestate_directory",
         dir_get_ptr(RARCH_DIR_SAVESTATE), true, NULL, false);
#ifdef HAVE_MENU
   SETTING_PATH("rgui_browser_directory",
         settings->paths.directory_menu_content, true, NULL, true);
   SETTING_PATH("rgui_config_directory",
         settings->paths.directory_menu_config, true, NULL, true);
#endif
#ifdef HAVE_OVERLAY
   SETTING_PATH("overlay_directory",
         settings->paths.directory_overlay, true, NULL, true);
#endif
#ifndef HAVE_DYNAMIC
   SETTING_PATH("libretro_path",
         path_get_ptr(RARCH_PATH_CORE), false, NULL, false);
#endif
   SETTING_PATH(
         "screenshot_directory",
         settings->paths.directory_screenshot, true, NULL, false);

   if (global)
   {
      SETTING_PATH("recording_output_directory",
            global->record.output_dir, false, NULL, true);
      SETTING_PATH("recording_config_directory",
            global->record.config_dir, false, NULL, true);
   }

   *size = count;

   return tmp;
}

static struct config_bool_setting *populate_settings_bool(settings_t *settings, int *size)
{
   struct config_bool_setting  *tmp    = (struct config_bool_setting*)malloc((*size + 1) * sizeof(struct config_bool_setting));
   unsigned count                      = 0;

   SETTING_BOOL("automatically_add_content_to_playlist", &settings->bools.automatically_add_content_to_playlist, true, automatically_add_content_to_playlist, false);
   SETTING_BOOL("ui_companion_start_on_boot",    &settings->bools.ui_companion_start_on_boot, true, ui_companion_start_on_boot, false);
   SETTING_BOOL("ui_companion_enable",           &settings->bools.ui_companion_enable, true, ui_companion_enable, false);
   SETTING_BOOL("ui_companion_toggle",           &settings->bools.ui_companion_toggle, false, ui_companion_toggle, false);
   SETTING_BOOL("desktop_menu_enable",           &settings->bools.desktop_menu_enable, true, desktop_menu_enable, false);
   SETTING_BOOL("video_gpu_record",              &settings->bools.video_gpu_record, true, gpu_record, false);
   SETTING_BOOL("input_remap_binds_enable",      &settings->bools.input_remap_binds_enable, true, true, false);
   SETTING_BOOL("all_users_control_menu",        &settings->bools.input_all_users_control_menu, true, all_users_control_menu, false);
   SETTING_BOOL("menu_swap_ok_cancel_buttons",   &settings->bools.input_menu_swap_ok_cancel_buttons, true, menu_swap_ok_cancel_buttons, false);
#ifdef HAVE_NETWORKING
   SETTING_BOOL("netplay_public_announce",       &settings->bools.netplay_public_announce, true, netplay_public_announce, false);
   SETTING_BOOL("netplay_start_as_spectator",    &settings->bools.netplay_start_as_spectator, false, netplay_start_as_spectator, false);
   SETTING_BOOL("netplay_allow_slaves",          &settings->bools.netplay_allow_slaves, true, netplay_allow_slaves, false);
   SETTING_BOOL("netplay_require_slaves",        &settings->bools.netplay_require_slaves, true, netplay_require_slaves, false);
   SETTING_BOOL("netplay_stateless_mode",        &settings->bools.netplay_stateless_mode, true, netplay_stateless_mode, false);
   SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE);
   SETTING_BOOL("netplay_use_mitm_server",       &settings->bools.netplay_use_mitm_server, true, netplay_use_mitm_server, false);
   SETTING_BOOL("netplay_request_device_p1",     &settings->bools.netplay_request_devices[0], true, false, false);
   SETTING_BOOL("netplay_request_device_p2",     &settings->bools.netplay_request_devices[1], true, false, false);
   SETTING_BOOL("netplay_request_device_p3",     &settings->bools.netplay_request_devices[2], true, false, false);
   SETTING_BOOL("netplay_request_device_p4",     &settings->bools.netplay_request_devices[3], true, false, false);
   SETTING_BOOL("netplay_request_device_p5",     &settings->bools.netplay_request_devices[4], true, false, false);
   SETTING_BOOL("netplay_request_device_p6",     &settings->bools.netplay_request_devices[5], true, false, false);
   SETTING_BOOL("netplay_request_device_p7",     &settings->bools.netplay_request_devices[6], true, false, false);
   SETTING_BOOL("netplay_request_device_p8",     &settings->bools.netplay_request_devices[7], true, false, false);
   SETTING_BOOL("netplay_request_device_p9",     &settings->bools.netplay_request_devices[8], true, false, false);
   SETTING_BOOL("netplay_request_device_p10",    &settings->bools.netplay_request_devices[9], true, false, false);
   SETTING_BOOL("netplay_request_device_p11",    &settings->bools.netplay_request_devices[10], true, false, false);
   SETTING_BOOL("netplay_request_device_p12",    &settings->bools.netplay_request_devices[11], true, false, false);
   SETTING_BOOL("netplay_request_device_p13",    &settings->bools.netplay_request_devices[12], true, false, false);
   SETTING_BOOL("netplay_request_device_p14",    &settings->bools.netplay_request_devices[13], true, false, false);
   SETTING_BOOL("netplay_request_device_p15",    &settings->bools.netplay_request_devices[14], true, false, false);
   SETTING_BOOL("netplay_request_device_p16",    &settings->bools.netplay_request_devices[15], true, false, false);
#endif
   SETTING_BOOL("input_descriptor_label_show",   &settings->bools.input_descriptor_label_show, true, input_descriptor_label_show, false);
   SETTING_BOOL("input_descriptor_hide_unbound", &settings->bools.input_descriptor_hide_unbound, true, input_descriptor_hide_unbound, false);
   SETTING_BOOL("load_dummy_on_core_shutdown",   &settings->bools.load_dummy_on_core_shutdown, true, load_dummy_on_core_shutdown, false);
   SETTING_BOOL("check_firmware_before_loading", &settings->bools.check_firmware_before_loading, true, check_firmware_before_loading, false);
   SETTING_BOOL("builtin_mediaplayer_enable",    &settings->bools.multimedia_builtin_mediaplayer_enable, false, false /* TODO */, false);
   SETTING_BOOL("builtin_imageviewer_enable",    &settings->bools.multimedia_builtin_imageviewer_enable, true, true, false);
   SETTING_BOOL("fps_show",                      &settings->bools.video_fps_show, true, false, false);
   SETTING_BOOL("statistics_show",               &settings->bools.video_statistics_show, true, false, false);
   SETTING_BOOL("framecount_show",               &settings->bools.video_framecount_show, true, true, false);
   SETTING_BOOL("ui_menubar_enable",             &settings->bools.ui_menubar_enable, true, true, false);
   SETTING_BOOL("suspend_screensaver_enable",    &settings->bools.ui_suspend_screensaver_enable, true, true, false);
   SETTING_BOOL("rewind_enable",                 &settings->bools.rewind_enable, true, rewind_enable, false);
   SETTING_BOOL("vrr_runloop_enable",            &settings->bools.vrr_runloop_enable, true, vrr_runloop_enable, false);
   SETTING_BOOL("apply_cheats_after_toggle",     &settings->bools.apply_cheats_after_toggle, true, apply_cheats_after_toggle, false);
   SETTING_BOOL("apply_cheats_after_load",       &settings->bools.apply_cheats_after_load, true, apply_cheats_after_load, false);
   SETTING_BOOL("run_ahead_enabled",             &settings->bools.run_ahead_enabled, true, false, false);
   SETTING_BOOL("run_ahead_secondary_instance",  &settings->bools.run_ahead_secondary_instance, true, false, false);
   SETTING_BOOL("run_ahead_hide_warnings",       &settings->bools.run_ahead_hide_warnings, true, false, false);
   SETTING_BOOL("audio_sync",                    &settings->bools.audio_sync, true, audio_sync, false);
   SETTING_BOOL("video_shader_enable",           &settings->bools.video_shader_enable, true, shader_enable, false);
   SETTING_BOOL("video_shader_watch_files",      &settings->bools.video_shader_watch_files, true, video_shader_watch_files, false);

   /* Let implementation decide if automatic, or 1:1 PAR. */
   SETTING_BOOL("video_aspect_ratio_auto",       &settings->bools.video_aspect_ratio_auto, true, aspect_ratio_auto, false);

   SETTING_BOOL("video_allow_rotate",            &settings->bools.video_allow_rotate, true, allow_rotate, false);
   SETTING_BOOL("video_windowed_fullscreen",     &settings->bools.video_windowed_fullscreen, true, windowed_fullscreen, false);
   SETTING_BOOL("video_crop_overscan",           &settings->bools.video_crop_overscan, true, crop_overscan, false);
   SETTING_BOOL("video_scale_integer",           &settings->bools.video_scale_integer, true, scale_integer, false);
   SETTING_BOOL("video_smooth",                  &settings->bools.video_smooth, true, video_smooth, false);
   SETTING_BOOL("video_force_aspect",            &settings->bools.video_force_aspect, true, force_aspect, false);
   SETTING_BOOL("video_threaded",                video_driver_get_threaded(), true, video_threaded, false);
   SETTING_BOOL("video_shared_context",          &settings->bools.video_shared_context, true, video_shared_context, false);
   SETTING_BOOL("auto_screenshot_filename",      &settings->bools.auto_screenshot_filename, true, auto_screenshot_filename, false);
   SETTING_BOOL("video_force_srgb_disable",      &settings->bools.video_force_srgb_disable, true, false, false);
   SETTING_BOOL("video_fullscreen",              &settings->bools.video_fullscreen, true, fullscreen, false);
   SETTING_BOOL("bundle_assets_extract_enable",  &settings->bools.bundle_assets_extract_enable, true, bundle_assets_extract_enable, false);
   SETTING_BOOL("video_vsync",                   &settings->bools.video_vsync, true, vsync, false);
   SETTING_BOOL("video_hard_sync",               &settings->bools.video_hard_sync, true, hard_sync, false);
   SETTING_BOOL("video_black_frame_insertion",   &settings->bools.video_black_frame_insertion, true, black_frame_insertion, false);
   SETTING_BOOL("crt_switch_resolution",  		 &settings->bools.crt_switch_resolution, true, crt_switch_resolution, false);
   SETTING_BOOL("video_disable_composition",     &settings->bools.video_disable_composition, true, disable_composition, false);
   SETTING_BOOL("pause_nonactive",               &settings->bools.pause_nonactive, true, pause_nonactive, false);
   SETTING_BOOL("video_gpu_screenshot",          &settings->bools.video_gpu_screenshot, true, gpu_screenshot, false);
   SETTING_BOOL("video_post_filter_record",      &settings->bools.video_post_filter_record, true, post_filter_record, false);
   SETTING_BOOL("keyboard_gamepad_enable",       &settings->bools.input_keyboard_gamepad_enable, true, true, false);
   SETTING_BOOL("core_set_supports_no_game_enable", &settings->bools.set_supports_no_game_enable, true, true, false);
   SETTING_BOOL("audio_enable",                  &settings->bools.audio_enable, true, audio_enable, false);
   SETTING_BOOL("audio_enable_menu",             &settings->bools.audio_enable_menu, true, false, false);
   SETTING_BOOL("audio_mute_enable",             audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE), true, false, false);
   SETTING_BOOL("audio_mixer_mute_enable",       audio_get_bool_ptr(AUDIO_ACTION_MIXER_MUTE_ENABLE), true, false, false);
   SETTING_BOOL("location_allow",                &settings->bools.location_allow, true, false, false);
   SETTING_BOOL("video_font_enable",             &settings->bools.video_font_enable, true, font_enable, false);
   SETTING_BOOL("core_updater_auto_extract_archive", &settings->bools.network_buildbot_auto_extract_archive, true, true, false);
   SETTING_BOOL("camera_allow",                  &settings->bools.camera_allow, true, false, false);
   SETTING_BOOL("discord_allow",                  &settings->bools.discord_enable, true, false, false);
#if defined(VITA)
   SETTING_BOOL("input_backtouch_enable",         &settings->bools.input_backtouch_enable, false, false, false);
   SETTING_BOOL("input_backtouch_toggle",         &settings->bools.input_backtouch_toggle, false, false, false);
#endif
#if TARGET_OS_IPHONE
   SETTING_BOOL("small_keyboard_enable",         &settings->bools.input_small_keyboard_enable, true, false, false);
#endif
#ifdef GEKKO
   SETTING_BOOL("video_vfilter",                 &settings->bools.video_vfilter, true, video_vfilter, false);
#endif
#ifdef HAVE_MENU
   SETTING_BOOL("menu_unified_controls",         &settings->bools.menu_unified_controls, true, false, false);
#ifdef HAVE_THREADS
   SETTING_BOOL("threaded_data_runloop_enable",  &settings->bools.threaded_data_runloop_enable, true, threaded_data_runloop_enable, false);
#endif
   SETTING_BOOL("menu_throttle_framerate",       &settings->bools.menu_throttle_framerate, true, true, false);
   SETTING_BOOL("menu_linear_filter",            &settings->bools.menu_linear_filter, true, true, false);
   SETTING_BOOL("menu_horizontal_animation",     &settings->bools.menu_horizontal_animation, true, true, false);
   SETTING_BOOL("dpi_override_enable",           &settings->bools.menu_dpi_override_enable, true, menu_dpi_override_enable, false);
   SETTING_BOOL("menu_pause_libretro",           &settings->bools.menu_pause_libretro, true, true, false);
   SETTING_BOOL("menu_mouse_enable",             &settings->bools.menu_mouse_enable, true, def_mouse_enable, false);
   SETTING_BOOL("menu_pointer_enable",           &settings->bools.menu_pointer_enable, true, pointer_enable, false);
   SETTING_BOOL("menu_timedate_enable",          &settings->bools.menu_timedate_enable, true, true, false);
   SETTING_BOOL("menu_battery_level_enable",     &settings->bools.menu_battery_level_enable, true, true, false);
   SETTING_BOOL("menu_core_enable",              &settings->bools.menu_core_enable, true, true, false);
   SETTING_BOOL("menu_dynamic_wallpaper_enable", &settings->bools.menu_dynamic_wallpaper_enable, true, false, false);
   SETTING_BOOL("quick_menu_show_take_screenshot",      &settings->bools.quick_menu_show_take_screenshot, true, quick_menu_show_take_screenshot, false);
   SETTING_BOOL("quick_menu_show_save_load_state",      &settings->bools.quick_menu_show_save_load_state, true, quick_menu_show_save_load_state, false);
   SETTING_BOOL("quick_menu_show_undo_save_load_state", &settings->bools.quick_menu_show_undo_save_load_state, true, quick_menu_show_undo_save_load_state, false);
   SETTING_BOOL("quick_menu_show_add_to_favorites",     &settings->bools.quick_menu_show_add_to_favorites, true, quick_menu_show_add_to_favorites, false);
   SETTING_BOOL("quick_menu_show_options",       &settings->bools.quick_menu_show_options, true, quick_menu_show_options, false);
   SETTING_BOOL("quick_menu_show_controls",      &settings->bools.quick_menu_show_controls, true, quick_menu_show_controls, false);
   SETTING_BOOL("quick_menu_show_cheats",        &settings->bools.quick_menu_show_cheats, true, quick_menu_show_cheats, false);
   SETTING_BOOL("quick_menu_show_shaders",       &settings->bools.quick_menu_show_shaders, true, quick_menu_show_shaders, false);
   SETTING_BOOL("quick_menu_show_save_core_overrides",  &settings->bools.quick_menu_show_save_core_overrides, true, quick_menu_show_save_core_overrides, false);
   SETTING_BOOL("quick_menu_show_save_game_overrides",  &settings->bools.quick_menu_show_save_game_overrides, true, quick_menu_show_save_game_overrides, false);
   SETTING_BOOL("quick_menu_show_save_content_dir_overrides",  &settings->bools.quick_menu_show_save_content_dir_overrides, true, quick_menu_show_save_content_dir_overrides, false);
   SETTING_BOOL("quick_menu_show_information",   &settings->bools.quick_menu_show_information, true, quick_menu_show_information, false);
   SETTING_BOOL("kiosk_mode_enable",             &settings->bools.kiosk_mode_enable, true, kiosk_mode_enable, false);
   SETTING_BOOL("content_show_settings",         &settings->bools.menu_content_show_settings, true, content_show_settings, false);
   SETTING_BOOL("content_show_favorites",        &settings->bools.menu_content_show_favorites, true, content_show_favorites, false);
#ifdef HAVE_IMAGEVIEWER
   SETTING_BOOL("content_show_images",           &settings->bools.menu_content_show_images, true, content_show_images, false);
#endif
   SETTING_BOOL("content_show_music",            &settings->bools.menu_content_show_music, true, content_show_music, false);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   SETTING_BOOL("content_show_video",            &settings->bools.menu_content_show_video, true, content_show_video, false);
#endif
#ifdef HAVE_NETWORKING
   SETTING_BOOL("content_show_netplay",          &settings->bools.menu_content_show_netplay, true, content_show_netplay, false);
#endif
   SETTING_BOOL("content_show_history",          &settings->bools.menu_content_show_history, true, content_show_history, false);
#ifdef HAVE_LIBRETRODB
   SETTING_BOOL("content_show_add",              &settings->bools.menu_content_show_add, true, content_show_add, false);
#endif
   SETTING_BOOL("content_show_playlists",        &settings->bools.menu_content_show_playlists, true, content_show_playlists, false);
   SETTING_BOOL("menu_show_load_core",           &settings->bools.menu_show_load_core, true, menu_show_load_core, false);
   SETTING_BOOL("menu_show_load_content",        &settings->bools.menu_show_load_content, true, menu_show_load_content, false);
   SETTING_BOOL("menu_show_information",         &settings->bools.menu_show_information, true, menu_show_information, false);
   SETTING_BOOL("menu_show_configurations",      &settings->bools.menu_show_configurations, true, menu_show_configurations, false);
   SETTING_BOOL("menu_show_latency",      &settings->bools.menu_show_latency, true, true, false);
   SETTING_BOOL("menu_show_rewind",      &settings->bools.menu_show_rewind, true, true, false);
   SETTING_BOOL("menu_show_overlays",      &settings->bools.menu_show_overlays, true, true, false);
   SETTING_BOOL("menu_show_help",                &settings->bools.menu_show_help, true, menu_show_help, false);
   SETTING_BOOL("menu_show_quit_retroarch",      &settings->bools.menu_show_quit_retroarch, true, menu_show_quit_retroarch, false);
   SETTING_BOOL("menu_show_reboot",              &settings->bools.menu_show_reboot, true, menu_show_reboot, false);
   SETTING_BOOL("menu_show_shutdown",            &settings->bools.menu_show_shutdown, true, menu_show_shutdown, false);
   SETTING_BOOL("menu_show_online_updater",      &settings->bools.menu_show_online_updater, true, menu_show_online_updater, false);
   SETTING_BOOL("menu_show_core_updater",        &settings->bools.menu_show_core_updater, true, menu_show_core_updater, false);
   SETTING_BOOL("filter_by_current_core",        &settings->bools.filter_by_current_core, false, false /* TODO */, false);
   SETTING_BOOL("rgui_show_start_screen",        &settings->bools.menu_show_start_screen, false, false /* TODO */, false);
   SETTING_BOOL("menu_navigation_wraparound_enable", &settings->bools.menu_navigation_wraparound_enable, true, true, false);
   SETTING_BOOL("menu_navigation_browser_filter_supported_extensions_enable",
         &settings->bools.menu_navigation_browser_filter_supported_extensions_enable, true, true, false);
   SETTING_BOOL("menu_show_advanced_settings",  &settings->bools.menu_show_advanced_settings, true, show_advanced_settings, false);
#ifdef HAVE_MATERIALUI
   SETTING_BOOL("materialui_icons_enable",       &settings->bools.menu_materialui_icons_enable, true, materialui_icons_enable, false);
#endif
#ifdef HAVE_RGUI
   SETTING_BOOL("rgui_background_filler_thickness_enable",     &settings->bools.menu_rgui_background_filler_thickness_enable, true, true, false);
   SETTING_BOOL("rgui_border_filler_thickness_enable",     &settings->bools.menu_rgui_border_filler_thickness_enable, true, true, false);
   SETTING_BOOL("rgui_border_filler_enable",     &settings->bools.menu_rgui_border_filler_enable, true, true, false);
#endif
#ifdef HAVE_XMB
   SETTING_BOOL("xmb_shadows_enable",            &settings->bools.menu_xmb_shadows_enable, true, xmb_shadows_enable, false);
   SETTING_BOOL("xmb_vertical_thumbnails",       &settings->bools.menu_xmb_vertical_thumbnails, true, xmb_vertical_thumbnails, false);
#endif
#endif
#ifdef HAVE_CHEEVOS
   SETTING_BOOL("cheevos_enable",               &settings->bools.cheevos_enable, true, cheevos_enable, false);
   SETTING_BOOL("cheevos_test_unofficial",      &settings->bools.cheevos_test_unofficial, true, false, false);
   SETTING_BOOL("cheevos_hardcore_mode_enable", &settings->bools.cheevos_hardcore_mode_enable, true, false, false);
   SETTING_BOOL("cheevos_leaderboards_enable",  &settings->bools.cheevos_leaderboards_enable, true, false, false);
   SETTING_BOOL("cheevos_verbose_enable",       &settings->bools.cheevos_verbose_enable, true, false, false);
   SETTING_BOOL("cheevos_auto_screenshot",      &settings->bools.cheevos_auto_screenshot, true, false, false);
#ifdef HAVE_XMB
   SETTING_BOOL("cheevos_badges_enable",        &settings->bools.cheevos_badges_enable, true, false, false);
#endif
#endif
#ifdef HAVE_OVERLAY
   SETTING_BOOL("input_overlay_enable",         &settings->bools.input_overlay_enable, true, config_overlay_enable_default(), false);
   SETTING_BOOL("input_overlay_enable_autopreferred", &settings->bools.input_overlay_enable_autopreferred, true, true, false);
   SETTING_BOOL("input_overlay_show_physical_inputs", &settings->bools.input_overlay_show_physical_inputs, true, false, false);
   SETTING_BOOL("input_overlay_hide_in_menu",   &settings->bools.input_overlay_hide_in_menu, true, overlay_hide_in_menu, false);
#endif
#ifdef HAVE_COMMAND
   SETTING_BOOL("network_cmd_enable",           &settings->bools.network_cmd_enable, true, network_cmd_enable, false);
   SETTING_BOOL("stdin_cmd_enable",             &settings->bools.stdin_cmd_enable, true, stdin_cmd_enable, false);
#endif
#ifdef HAVE_NETWORKGAMEPAD
   SETTING_BOOL("network_remote_enable",        &settings->bools.network_remote_enable, false, false /* TODO */, false);
#endif
#ifdef HAVE_NETWORKING
   SETTING_BOOL("netplay_nat_traversal",        &settings->bools.netplay_nat_traversal, true, true, false);
#endif
   SETTING_BOOL("block_sram_overwrite",         &settings->bools.block_sram_overwrite, true, block_sram_overwrite, false);
   SETTING_BOOL("savestate_auto_index",         &settings->bools.savestate_auto_index, true, savestate_auto_index, false);
   SETTING_BOOL("savestate_auto_save",          &settings->bools.savestate_auto_save, true, savestate_auto_save, false);
   SETTING_BOOL("savestate_auto_load",          &settings->bools.savestate_auto_load, true, savestate_auto_load, false);
   SETTING_BOOL("savestate_thumbnail_enable",   &settings->bools.savestate_thumbnail_enable, true, savestate_thumbnail_enable, false);
   SETTING_BOOL("history_list_enable",          &settings->bools.history_list_enable, true, def_history_list_enable, false);
   SETTING_BOOL("playlist_entry_remove",        &settings->bools.playlist_entry_remove, true, def_playlist_entry_remove, false);
   SETTING_BOOL("playlist_entry_rename",        &settings->bools.playlist_entry_rename, true, def_playlist_entry_rename, false);
   SETTING_BOOL("game_specific_options",        &settings->bools.game_specific_options, true, default_game_specific_options, false);
   SETTING_BOOL("auto_overrides_enable",        &settings->bools.auto_overrides_enable, true, default_auto_overrides_enable, false);
   SETTING_BOOL("auto_remaps_enable",           &settings->bools.auto_remaps_enable, true, default_auto_remaps_enable, false);
   SETTING_BOOL("auto_shaders_enable",          &settings->bools.auto_shaders_enable, true, default_auto_shaders_enable, false);
   SETTING_BOOL("sort_savefiles_enable",        &settings->bools.sort_savefiles_enable, true, default_sort_savefiles_enable, false);
   SETTING_BOOL("sort_savestates_enable",       &settings->bools.sort_savestates_enable, true, default_sort_savestates_enable, false);
   SETTING_BOOL("config_save_on_exit",          &settings->bools.config_save_on_exit, true, config_save_on_exit, false);
   SETTING_BOOL("show_hidden_files",            &settings->bools.show_hidden_files, true, show_hidden_files, false);
   SETTING_BOOL("input_autodetect_enable",      &settings->bools.input_autodetect_enable, true, input_autodetect_enable, false);
   SETTING_BOOL("audio_rate_control",           &settings->bools.audio_rate_control, true, rate_control, false);
#ifdef HAVE_WASAPI
   SETTING_BOOL("audio_wasapi_exclusive_mode",  &settings->bools.audio_wasapi_exclusive_mode, true, wasapi_exclusive_mode, false);
   SETTING_BOOL("audio_wasapi_float_format",    &settings->bools.audio_wasapi_float_format, true, wasapi_float_format, false);
#endif

   SETTING_BOOL("savestates_in_content_dir",     &settings->bools.savestates_in_content_dir, true, default_savestates_in_content_dir, false);
   SETTING_BOOL("savefiles_in_content_dir",      &settings->bools.savefiles_in_content_dir, true, default_savefiles_in_content_dir, false);
   SETTING_BOOL("systemfiles_in_content_dir",    &settings->bools.systemfiles_in_content_dir, true, default_systemfiles_in_content_dir, false);
   SETTING_BOOL("screenshots_in_content_dir",    &settings->bools.screenshots_in_content_dir, true, default_screenshots_in_content_dir, false);

   SETTING_BOOL("video_msg_bgcolor_enable",      &settings->bools.video_msg_bgcolor_enable, true, message_bgcolor_enable, false);
   SETTING_BOOL("video_window_show_decorations", &settings->bools.video_window_show_decorations, true, window_decorations, false);

   SETTING_BOOL("sustained_performance_mode",    &settings->bools.sustained_performance_mode, true, sustained_performance_mode, false);

   *size = count;

   return tmp;
}

static struct config_float_setting *populate_settings_float(settings_t *settings, int *size)
{
   unsigned count = 0;
   struct config_float_setting  *tmp      = (struct config_float_setting*)calloc(1, (*size + 1) * sizeof(struct config_float_setting));

   SETTING_FLOAT("video_aspect_ratio",       &settings->floats.video_aspect_ratio, true, aspect_ratio, false);
   SETTING_FLOAT("video_scale",              &settings->floats.video_scale, false, 0.0f, false);
   SETTING_FLOAT("video_refresh_rate",       &settings->floats.video_refresh_rate, true, refresh_rate, false);
   SETTING_FLOAT("audio_rate_control_delta", audio_get_float_ptr(AUDIO_ACTION_RATE_CONTROL_DELTA), true, rate_control_delta, false);
   SETTING_FLOAT("audio_max_timing_skew",    &settings->floats.audio_max_timing_skew, true, max_timing_skew, false);
   SETTING_FLOAT("audio_volume",             &settings->floats.audio_volume, true, audio_volume, false);
   SETTING_FLOAT("audio_mixer_volume",       &settings->floats.audio_mixer_volume, true, audio_mixer_volume, false);
#ifdef HAVE_OVERLAY
   SETTING_FLOAT("input_overlay_opacity",    &settings->floats.input_overlay_opacity, true, default_input_overlay_opacity, false);
   SETTING_FLOAT("input_overlay_scale",      &settings->floats.input_overlay_scale, true, 1.0f, false);
#endif
#ifdef HAVE_MENU
   SETTING_FLOAT("menu_wallpaper_opacity",   &settings->floats.menu_wallpaper_opacity, true, menu_wallpaper_opacity, false);
   SETTING_FLOAT("menu_framebuffer_opacity", &settings->floats.menu_framebuffer_opacity, true, menu_framebuffer_opacity, false);
   SETTING_FLOAT("menu_footer_opacity",      &settings->floats.menu_footer_opacity,    true, menu_footer_opacity, false);
   SETTING_FLOAT("menu_header_opacity",      &settings->floats.menu_header_opacity,    true, menu_header_opacity, false);
#endif
   SETTING_FLOAT("video_message_pos_x",      &settings->floats.video_msg_pos_x,      true, message_pos_offset_x, false);
   SETTING_FLOAT("video_message_pos_y",      &settings->floats.video_msg_pos_y,      true, message_pos_offset_y, false);
   SETTING_FLOAT("video_font_size",          &settings->floats.video_font_size,      true, font_size, false);
   SETTING_FLOAT("fastforward_ratio",        &settings->floats.fastforward_ratio,    true, fastforward_ratio, false);
   SETTING_FLOAT("slowmotion_ratio",         &settings->floats.slowmotion_ratio,     true, slowmotion_ratio, false);
   SETTING_FLOAT("input_axis_threshold",     input_driver_get_float(INPUT_ACTION_AXIS_THRESHOLD), true, axis_threshold, false);
   SETTING_FLOAT("video_msg_bgcolor_opacity", &settings->floats.video_msg_bgcolor_opacity, true, message_bgcolor_opacity, false);

   *size = count;

   return tmp;
}

static struct config_uint_setting *populate_settings_uint(settings_t *settings, int *size)
{
   unsigned count                     = 0;
   struct config_uint_setting  *tmp   = (struct config_uint_setting*)malloc((*size + 1) * sizeof(struct config_uint_setting));

   SETTING_UINT("input_bind_timeout",           &settings->uints.input_bind_timeout,     true, input_bind_timeout, false);
   SETTING_UINT("input_bind_hold",              &settings->uints.input_bind_hold,        true, input_bind_hold, false);
   SETTING_UINT("input_turbo_period",           &settings->uints.input_turbo_period,     true, turbo_period, false);
   SETTING_UINT("input_duty_cycle",             &settings->uints.input_turbo_duty_cycle, true, turbo_duty_cycle, false);
   SETTING_UINT("input_max_users",              input_driver_get_uint(INPUT_ACTION_MAX_USERS),        true, input_max_users, false);
   SETTING_UINT("input_menu_toggle_gamepad_combo", &settings->uints.input_menu_toggle_gamepad_combo, true, menu_toggle_gamepad_combo, false);
   SETTING_UINT("audio_latency",                &settings->uints.audio_latency, false, 0 /* TODO */, false);
   SETTING_UINT("audio_resampler_quality",      &settings->uints.audio_resampler_quality, true, audio_resampler_quality_level, false);
   SETTING_UINT("audio_block_frames",           &settings->uints.audio_block_frames, true, 0, false);
   SETTING_UINT("rewind_granularity",           &settings->uints.rewind_granularity, true, rewind_granularity, false);
   SETTING_UINT("rewind_buffer_size_step",      &settings->uints.rewind_buffer_size_step, true, rewind_buffer_size_step, false);
   SETTING_UINT("autosave_interval",            &settings->uints.autosave_interval,  true, autosave_interval, false);
   SETTING_UINT("libretro_log_level",           &settings->uints.libretro_log_level, true, libretro_log_level, false);
   SETTING_UINT("keyboard_gamepad_mapping_type",&settings->uints.input_keyboard_gamepad_mapping_type, true, 1, false);
   SETTING_UINT("input_poll_type_behavior",     &settings->uints.input_poll_type_behavior, true, 2, false);
   SETTING_UINT("video_monitor_index",          &settings->uints.video_monitor_index, true, monitor_index, false);
   SETTING_UINT("video_fullscreen_x",           &settings->uints.video_fullscreen_x,  true, fullscreen_x, false);
   SETTING_UINT("video_fullscreen_y",           &settings->uints.video_fullscreen_y,  true, fullscreen_y, false);
   SETTING_UINT("video_window_x",               &settings->uints.video_window_x,  true, fullscreen_x, false);
   SETTING_UINT("video_window_y",               &settings->uints.video_window_y,  true, fullscreen_y, false);
   SETTING_UINT("video_window_opacity",         &settings->uints.video_window_opacity, true, window_opacity, false);
#ifdef HAVE_COMMAND
   SETTING_UINT("network_cmd_port",             &settings->uints.network_cmd_port,    true, network_cmd_port, false);
#endif
#ifdef HAVE_NETWORKGAMEPAD
   SETTING_UINT("network_remote_base_port",     &settings->uints.network_remote_base_port, true, network_remote_base_port, false);
#endif
#ifdef GEKKO
   SETTING_UINT("video_viwidth",                &settings->uints.video_viwidth, true, video_viwidth, false);
#endif
#ifdef HAVE_MENU
   SETTING_UINT("dpi_override_value",           &settings->uints.menu_dpi_override_value, true, menu_dpi_override_value, false);
   SETTING_UINT("menu_thumbnails",              &settings->uints.menu_thumbnails, true, menu_thumbnails_default, false);
#ifdef HAVE_XMB
   SETTING_UINT("menu_left_thumbnails",         &settings->uints.menu_left_thumbnails, true, menu_left_thumbnails_default, false);
   SETTING_UINT("xmb_alpha_factor",             &settings->uints.menu_xmb_alpha_factor, true, xmb_alpha_factor, false);
   SETTING_UINT("xmb_scale_factor",             &settings->uints.menu_xmb_scale_factor, true, xmb_scale_factor, false);
   SETTING_UINT("xmb_layout",                   &settings->uints.menu_xmb_layout, true, xmb_menu_layout, false);
   SETTING_UINT("xmb_theme",                    &settings->uints.menu_xmb_theme, true, xmb_icon_theme, false);
   SETTING_UINT("xmb_menu_color_theme",         &settings->uints.menu_xmb_color_theme, true, xmb_theme, false);
   SETTING_UINT("menu_font_color_red",          &settings->uints.menu_font_color_red, true, menu_font_color_red, false);
   SETTING_UINT("menu_font_color_green",        &settings->uints.menu_font_color_green, true, menu_font_color_green, false);
   SETTING_UINT("menu_font_color_blue",         &settings->uints.menu_font_color_blue, true, menu_font_color_blue, false);
#endif
   SETTING_UINT("materialui_menu_color_theme",  &settings->uints.menu_materialui_color_theme, true, MATERIALUI_THEME_BLUE, false);
   SETTING_UINT("menu_shader_pipeline",         &settings->uints.menu_xmb_shader_pipeline, true, menu_shader_pipeline, false);
#endif
   SETTING_UINT("audio_out_rate",               &settings->uints.audio_out_rate, true, out_rate, false);
   SETTING_UINT("custom_viewport_width",        &settings->video_viewport_custom.width, false, 0 /* TODO */, false);
   SETTING_UINT("crt_switch_resolution_super",  &settings->uints.crt_switch_resolution_super,      true, crt_switch_resolution_super, false);
   SETTING_UINT("custom_viewport_height",       &settings->video_viewport_custom.height, false, 0 /* TODO */, false);
   SETTING_UINT("custom_viewport_x",            (unsigned*)&settings->video_viewport_custom.x, false, 0 /* TODO */, false);
   SETTING_UINT("custom_viewport_y",            (unsigned*)&settings->video_viewport_custom.y, false, 0 /* TODO */, false);
   SETTING_UINT("content_history_size",         &settings->uints.content_history_size,   true, default_content_history_size, false);
   SETTING_UINT("video_hard_sync_frames",       &settings->uints.video_hard_sync_frames, true, hard_sync_frames, false);
   SETTING_UINT("video_frame_delay",            &settings->uints.video_frame_delay,      true, frame_delay, false);
   SETTING_UINT("video_max_swapchain_images",   &settings->uints.video_max_swapchain_images, true, max_swapchain_images, false);
   SETTING_UINT("video_swap_interval",          &settings->uints.video_swap_interval, true, swap_interval, false);
   SETTING_UINT("video_rotation",               &settings->uints.video_rotation, true, ORIENTATION_NORMAL, false);
   SETTING_UINT("aspect_ratio_index",           &settings->uints.video_aspect_ratio_idx, true, aspect_ratio_idx, false);
#ifdef HAVE_NETWORKING
   SETTING_UINT("netplay_ip_port",              &settings->uints.netplay_port,         true, RARCH_DEFAULT_PORT, false);
   SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT);
   SETTING_UINT("netplay_input_latency_frames_min",&settings->uints.netplay_input_latency_frames_min, true, 0, false);
   SETTING_UINT("netplay_input_latency_frames_range",&settings->uints.netplay_input_latency_frames_range, true, 0, false);
   SETTING_UINT("netplay_share_digital",        &settings->uints.netplay_share_digital, true, netplay_share_digital, false);
   SETTING_UINT("netplay_share_analog",         &settings->uints.netplay_share_analog,  true, netplay_share_analog, false);
#endif
#ifdef HAVE_LANGEXTRA
   SETTING_UINT("user_language",                msg_hash_get_uint(MSG_HASH_USER_LANGUAGE), true, RETRO_LANGUAGE_ENGLISH, false);
#endif
   SETTING_UINT("bundle_assets_extract_version_current", &settings->uints.bundle_assets_extract_version_current, true, 0, false);
   SETTING_UINT("bundle_assets_extract_last_version",    &settings->uints.bundle_assets_extract_last_version, true, 0, false);
   SETTING_UINT("input_overlay_show_physical_inputs_port", &settings->uints.input_overlay_show_physical_inputs_port, true, 0, false);
   SETTING_UINT("video_msg_bgcolor_red",        &settings->uints.video_msg_bgcolor_red, true, message_bgcolor_red, false);
   SETTING_UINT("video_msg_bgcolor_green",        &settings->uints.video_msg_bgcolor_green, true, message_bgcolor_green, false);
   SETTING_UINT("video_msg_bgcolor_blue",        &settings->uints.video_msg_bgcolor_blue, true, message_bgcolor_blue, false);

   SETTING_UINT("run_ahead_frames",           &settings->uints.run_ahead_frames, true, 1,  false);

   SETTING_UINT("midi_volume",                  &settings->uints.midi_volume, true, midi_volume, false);

   *size = count;

   return tmp;
}

static struct config_size_setting *populate_settings_size(settings_t *settings, int *size)
{
   unsigned count                     = 0;
   struct config_size_setting  *tmp   = (struct config_size_setting*)calloc((*size + 1), sizeof(struct config_size_setting));

   SETTING_SIZE("rewind_buffer_size",           &settings->sizes.rewind_buffer_size, true, rewind_buffer_size, false);

   *size = count;

   return tmp;
}

static struct config_int_setting *populate_settings_int(settings_t *settings, int *size)
{
   unsigned count                     = 0;
   struct config_int_setting  *tmp    = (struct config_int_setting*)calloc((*size + 1), sizeof(struct config_int_setting));

   SETTING_INT("state_slot",                   &settings->ints.state_slot, false, 0 /* TODO */, false);
#ifdef HAVE_NETWORKING
   SETTING_INT("netplay_check_frames",         &settings->ints.netplay_check_frames, true, netplay_check_frames, false);
   SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES);
#endif
#ifdef HAVE_WASAPI
   SETTING_INT("audio_wasapi_sh_buffer_length", &settings->ints.audio_wasapi_sh_buffer_length, true, wasapi_sh_buffer_length, false);
#endif

   *size = count;

   return tmp;
}

/**
 * config_set_defaults:
 *
 * Set 'default' configuration values.
 **/
static void config_set_defaults(void)
{
   unsigned i, j;
#ifdef HAVE_MENU
   static bool first_initialized   = true;
#endif
   settings_t *settings            = config_get_ptr();
   int bool_settings_size          = sizeof(settings->bools)   / sizeof(settings->bools.placeholder);
   int float_settings_size         = sizeof(settings->floats)  / sizeof(settings->floats.placeholder);
   int int_settings_size           = sizeof(settings->ints)    / sizeof(settings->ints.placeholder);
   int uint_settings_size          = sizeof(settings->uints)   / sizeof(settings->uints.placeholder);
   int size_settings_size          = sizeof(settings->sizes)   / sizeof(settings->sizes.placeholder);
   const char *def_video           = config_get_default_video();
   const char *def_audio           = config_get_default_audio();
   const char *def_audio_resampler = config_get_default_audio_resampler();
   const char *def_input           = config_get_default_input();
   const char *def_joypad          = config_get_default_joypad();
#ifdef HAVE_MENU
   const char *def_menu            = config_get_default_menu();
#endif
   const char *def_camera          = config_get_default_camera();
   const char *def_wifi            = config_get_default_wifi();
   const char *def_led             = config_get_default_led();
   const char *def_location        = config_get_default_location();
   const char *def_record          = config_get_default_record();
   const char *def_midi            = config_get_default_midi();
   const char *def_mitm            = netplay_mitm_server;
   struct config_float_setting      *float_settings = populate_settings_float  (settings, &float_settings_size);
   struct config_bool_setting       *bool_settings  = populate_settings_bool  (settings, &bool_settings_size);
   struct config_int_setting        *int_settings   = populate_settings_int   (settings, &int_settings_size);
   struct config_uint_setting       *uint_settings  = populate_settings_uint   (settings, &uint_settings_size);
   struct config_size_setting       *size_settings  = populate_settings_size   (settings, &size_settings_size);

   if (bool_settings && (bool_settings_size > 0))
   {
      for (i = 0; i < (unsigned)bool_settings_size; i++)
      {
         if (bool_settings[i].def_enable)
            *bool_settings[i].ptr = bool_settings[i].def;
      }

      free(bool_settings);
   }

   if (int_settings && (int_settings_size > 0))
   {
      for (i = 0; i < (unsigned)int_settings_size; i++)
      {
         if (int_settings[i].def_enable)
            *int_settings[i].ptr = int_settings[i].def;
      }

      free(int_settings);
   }

   if (uint_settings && (uint_settings_size > 0))
   {
      for (i = 0; i < (unsigned)uint_settings_size; i++)
      {
         if (uint_settings[i].def_enable)
            *uint_settings[i].ptr = uint_settings[i].def;
      }

      free(uint_settings);
   }

   if (size_settings && (size_settings_size > 0))
   {
      for (i = 0; i < (unsigned)size_settings_size; i++)
      {
         if (size_settings[i].def_enable)
            *size_settings[i].ptr = size_settings[i].def;
      }

      free(size_settings);
   }

   if (float_settings && (float_settings_size > 0))
   {
      for (i = 0; i < (unsigned)float_settings_size; i++)
      {
         if (float_settings[i].def_enable)
            *float_settings[i].ptr = float_settings[i].def;
      }

      free(float_settings);
   }

   if (def_camera)
      strlcpy(settings->arrays.camera_driver,
            def_camera, sizeof(settings->arrays.camera_driver));
   if (def_wifi)
      strlcpy(settings->arrays.wifi_driver,
            def_wifi, sizeof(settings->arrays.wifi_driver));
   if (def_led)
      strlcpy(settings->arrays.led_driver,
            def_led, sizeof(settings->arrays.led_driver));
   if (def_location)
      strlcpy(settings->arrays.location_driver,
            def_location, sizeof(settings->arrays.location_driver));
   if (def_video)
      strlcpy(settings->arrays.video_driver,
            def_video, sizeof(settings->arrays.video_driver));
   if (def_audio)
      strlcpy(settings->arrays.audio_driver,
            def_audio, sizeof(settings->arrays.audio_driver));
   if (def_audio_resampler)
      strlcpy(settings->arrays.audio_resampler,
            def_audio_resampler, sizeof(settings->arrays.audio_resampler));
   if (def_input)
      strlcpy(settings->arrays.input_driver,
            def_input, sizeof(settings->arrays.input_driver));
   if (def_joypad)
      strlcpy(settings->arrays.input_joypad_driver,
            def_joypad, sizeof(settings->arrays.input_joypad_driver));
   if (def_record)
      strlcpy(settings->arrays.record_driver,
            def_record, sizeof(settings->arrays.record_driver));
   if (def_midi)
      strlcpy(settings->arrays.midi_driver,
            def_midi, sizeof(settings->arrays.midi_driver));
   if (def_mitm)
      strlcpy(settings->arrays.netplay_mitm_server,
            def_mitm, sizeof(settings->arrays.netplay_mitm_server));
#ifdef HAVE_MENU
   if (def_menu)
      strlcpy(settings->arrays.menu_driver,
            def_menu,  sizeof(settings->arrays.menu_driver));
#ifdef HAVE_XMB
   *settings->paths.path_menu_xmb_font            = '\0';
#endif

#ifdef HAVE_MATERIALUI
   if (g_defaults.menu.materialui.menu_color_theme_enable)
      settings->uints.menu_materialui_color_theme = g_defaults.menu.materialui.menu_color_theme;
#endif
#endif

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   configuration_set_bool(settings, settings->bools.multimedia_builtin_mediaplayer_enable, true);
#else
   configuration_set_bool(settings, settings->bools.multimedia_builtin_mediaplayer_enable, false);
#endif
   settings->floats.video_scale                = scale;

   if (g_defaults.settings.video_threaded_enable != video_threaded)
      video_driver_set_threaded(g_defaults.settings.video_threaded_enable);

   settings->floats.video_msg_color_r          = ((message_color >> 16) & 0xff) / 255.0f;
   settings->floats.video_msg_color_g          = ((message_color >>  8) & 0xff) / 255.0f;
   settings->floats.video_msg_color_b          = ((message_color >>  0) & 0xff) / 255.0f;

   if (g_defaults.settings.video_refresh_rate > 0.0 &&
         g_defaults.settings.video_refresh_rate != refresh_rate)
      settings->floats.video_refresh_rate      = g_defaults.settings.video_refresh_rate;

   if (audio_device)
      strlcpy(settings->arrays.audio_device,
            audio_device, sizeof(settings->arrays.audio_device));

   if (!g_defaults.settings.out_latency)
      g_defaults.settings.out_latency          = out_latency;

   settings->uints.audio_latency               = g_defaults.settings.out_latency;

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, settings->floats.audio_volume);
   audio_set_float(AUDIO_ACTION_MIXER_VOLUME_GAIN, settings->floats.audio_mixer_volume);

#ifdef HAVE_LAKKA
   settings->bools.ssh_enable                  = filestream_exists(LAKKA_SSH_PATH);
   settings->bools.samba_enable                = filestream_exists(LAKKA_SAMBA_PATH);
   settings->bools.bluetooth_enable            = filestream_exists(LAKKA_BLUETOOTH_PATH);
#endif

#ifdef HAVE_MENU
   if (first_initialized)
      settings->bools.menu_show_start_screen   = default_menu_show_start_screen;
   settings->uints.menu_entry_normal_color     = menu_entry_normal_color;
   settings->uints.menu_entry_hover_color      = menu_entry_hover_color;
   settings->uints.menu_title_color            = menu_title_color;
#endif

#ifdef HAVE_CHEEVOS
   *settings->arrays.cheevos_username                 = '\0';
   *settings->arrays.cheevos_password                 = '\0';
   *settings->arrays.cheevos_token                    = '\0';
#endif

   input_config_reset();
   input_remapping_set_defaults(true);
   input_autoconfigure_reset();

   /* Verify that binds are in proper order. */
   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         const struct retro_keybind *keyval = &input_config_binds[i][j];
         if (keyval->valid)
            retro_assert(j == keyval->id);
      }
   }

   strlcpy(settings->paths.network_buildbot_url, buildbot_server_url,
         sizeof(settings->paths.network_buildbot_url));
   strlcpy(settings->paths.network_buildbot_assets_url, buildbot_assets_server_url,
         sizeof(settings->paths.network_buildbot_assets_url));

   *settings->arrays.input_keyboard_layout                = '\0';

   for (i = 0; i < MAX_USERS; i++)
   {
      settings->uints.input_joypad_map[i] = i;
      settings->uints.input_analog_dpad_mode[i] = ANALOG_DPAD_NONE;
      input_config_set_device(i, RETRO_DEVICE_JOYPAD);
      settings->uints.input_mouse_index[i] = 0;
   }

   video_driver_reset_custom_viewport();

   /* Make sure settings from other configs carry over into defaults
    * for another config. */
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
      dir_clear(RARCH_DIR_SAVEFILE);
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
      dir_clear(RARCH_DIR_SAVESTATE);

   *settings->paths.path_libretro_info = '\0';
   *settings->paths.directory_libretro = '\0';
   *settings->paths.directory_cursor = '\0';
   *settings->paths.directory_resampler = '\0';
   *settings->paths.directory_screenshot = '\0';
   *settings->paths.directory_system = '\0';
   *settings->paths.directory_cache = '\0';
   *settings->paths.directory_input_remapping = '\0';
   *settings->paths.directory_core_assets = '\0';
   *settings->paths.directory_assets = '\0';
   *settings->paths.directory_dynamic_wallpapers = '\0';
   *settings->paths.directory_thumbnails = '\0';
   *settings->paths.directory_playlist = '\0';
   *settings->paths.directory_autoconfig = '\0';
#ifdef HAVE_MENU
   *settings->paths.directory_menu_content = '\0';
   *settings->paths.directory_menu_config = '\0';
#endif
   *settings->paths.directory_video_shader = '\0';
   *settings->paths.directory_video_filter = '\0';
   *settings->paths.directory_audio_filter = '\0';

   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);

   {
      global_t *global =  global_get_ptr();

      if (global)
      {
         *global->record.output_dir = '\0';
         *global->record.config_dir = '\0';
      }
   }

   *settings->paths.path_core_options      = '\0';
   *settings->paths.path_content_history   = '\0';
   *settings->paths.path_content_favorites = '\0';
   *settings->paths.path_content_music_history   = '\0';
   *settings->paths.path_content_image_history   = '\0';
   *settings->paths.path_content_video_history   = '\0';
   *settings->paths.path_cheat_settings    = '\0';
   *settings->paths.path_shader            = '\0';
#ifndef IOS
   *settings->arrays.bundle_assets_src = '\0';
   *settings->arrays.bundle_assets_dst = '\0';
   *settings->arrays.bundle_assets_dst_subdir = '\0';
#endif
   *settings->paths.path_cheat_database    = '\0';
   *settings->paths.path_menu_wallpaper    = '\0';
   *settings->paths.path_content_database  = '\0';
   *settings->paths.path_overlay           = '\0';
   *settings->paths.path_softfilter_plugin = '\0';

   *settings->arrays.playlist_names = '\0';
   *settings->arrays.playlist_cores = '\0';
   *settings->paths.directory_content_history = '\0';
   *settings->paths.path_audio_dsp_plugin = '\0';

   video_driver_default_settings();

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS]))
      strlcpy(settings->paths.directory_dynamic_wallpapers,
            g_defaults.dirs[DEFAULT_DIR_WALLPAPERS],
            sizeof(settings->paths.directory_dynamic_wallpapers));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]))
      strlcpy(settings->paths.directory_thumbnails,
            g_defaults.dirs[DEFAULT_DIR_THUMBNAILS],
            sizeof(settings->paths.directory_thumbnails));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_REMAP]))
      strlcpy(settings->paths.directory_input_remapping,
            g_defaults.dirs[DEFAULT_DIR_REMAP],
            sizeof(settings->paths.directory_input_remapping));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CACHE]))
      strlcpy(settings->paths.directory_cache,
            g_defaults.dirs[DEFAULT_DIR_CACHE],
            sizeof(settings->paths.directory_cache));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_ASSETS]))
      strlcpy(settings->paths.directory_assets,
            g_defaults.dirs[DEFAULT_DIR_ASSETS],
            sizeof(settings->paths.directory_assets));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]))
      strlcpy(settings->paths.directory_core_assets,
            g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS],
            sizeof(settings->paths.directory_core_assets));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]))
      strlcpy(settings->paths.directory_playlist,
            g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
            sizeof(settings->paths.directory_playlist));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CORE]))
      fill_pathname_expand_special(settings->paths.directory_libretro,
            g_defaults.dirs[DEFAULT_DIR_CORE],
            sizeof(settings->paths.directory_libretro));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]))
      strlcpy(settings->paths.directory_audio_filter,
            g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
            sizeof(settings->paths.directory_audio_filter));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]))
      strlcpy(settings->paths.directory_video_filter,
            g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
            sizeof(settings->paths.directory_video_filter));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SHADER]))
      fill_pathname_expand_special(settings->paths.directory_video_shader,
            g_defaults.dirs[DEFAULT_DIR_SHADER],
            sizeof(settings->paths.directory_video_shader));

   if (!string_is_empty(g_defaults.path.buildbot_server_url))
      strlcpy(settings->paths.network_buildbot_url,
            g_defaults.path.buildbot_server_url,
            sizeof(settings->paths.network_buildbot_url));
   if (!string_is_empty(g_defaults.path.core))
      rarch_ctl(RARCH_CTL_SET_LIBRETRO_PATH, g_defaults.path.core);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_DATABASE]))
      strlcpy(settings->paths.path_content_database,
            g_defaults.dirs[DEFAULT_DIR_DATABASE],
            sizeof(settings->paths.path_content_database));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CURSOR]))
      strlcpy(settings->paths.directory_cursor,
            g_defaults.dirs[DEFAULT_DIR_CURSOR],
            sizeof(settings->paths.directory_cursor));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CHEATS]))
      strlcpy(settings->paths.path_cheat_database,
            g_defaults.dirs[DEFAULT_DIR_CHEATS],
            sizeof(settings->paths.path_cheat_database));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]))
      fill_pathname_expand_special(settings->paths.path_libretro_info,
            g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
            sizeof(settings->paths.path_libretro_info));
#ifdef HAVE_OVERLAY
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_OVERLAY]))
   {
      fill_pathname_expand_special(settings->paths.directory_overlay,
            g_defaults.dirs[DEFAULT_DIR_OVERLAY],
            sizeof(settings->paths.directory_overlay));
#ifdef RARCH_MOBILE
      if (string_is_empty(settings->paths.path_overlay))
            fill_pathname_join(settings->paths.path_overlay,
                  settings->paths.directory_overlay,
                  "gamepads/flat/retropad.cfg",
                  sizeof(settings->paths.path_overlay));
#endif
   }
#endif

#ifdef HAVE_MENU
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]))
   {
      strlcpy(settings->paths.directory_menu_config,
            g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
            sizeof(settings->paths.directory_menu_config));
#if TARGET_OS_IPHONE
      {
         char *config_file_path        = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
         size_t config_file_path_size  = PATH_MAX_LENGTH * sizeof(char);

         fill_pathname_join(config_file_path, settings->paths.directory_menu_config, file_path_str(FILE_PATH_MAIN_CONFIG), config_file_path_size);
         path_set(RARCH_PATH_CONFIG,
               config_file_path);
      }
#endif
   }

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT]))
      strlcpy(settings->paths.directory_menu_content,
            g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT],
            sizeof(settings->paths.directory_menu_content));
#endif
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]))
      strlcpy(settings->paths.directory_autoconfig,
            g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
            sizeof(settings->paths.directory_autoconfig));

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]))
      dir_set(RARCH_DIR_SAVESTATE, g_defaults.dirs[DEFAULT_DIR_SAVESTATE]);

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SRAM]))
      dir_set(RARCH_DIR_SAVEFILE, g_defaults.dirs[DEFAULT_DIR_SRAM]);

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SYSTEM]))
      strlcpy(settings->paths.directory_system,
            g_defaults.dirs[DEFAULT_DIR_SYSTEM],
            sizeof(settings->paths.directory_system));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]))
      strlcpy(settings->paths.directory_screenshot,
            g_defaults.dirs[DEFAULT_DIR_SCREENSHOT],
            sizeof(settings->paths.directory_screenshot));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_RESAMPLER]))
      strlcpy(settings->paths.directory_resampler,
            g_defaults.dirs[DEFAULT_DIR_RESAMPLER],
            sizeof(settings->paths.directory_resampler));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]))
      strlcpy(settings->paths.directory_content_history,
            g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
            sizeof(settings->paths.directory_content_history));

   if (!string_is_empty(g_defaults.path.config))
   {
      char *temp_str = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      temp_str[0] = '\0';

      fill_pathname_expand_special(temp_str,
            g_defaults.path.config,
            PATH_MAX_LENGTH * sizeof(char));
      path_set(RARCH_PATH_CONFIG, temp_str);
      free(temp_str);
   }

   if (midi_input)
      strlcpy(settings->arrays.midi_input,
            midi_input, sizeof(settings->arrays.midi_input));
   if (midi_output)
      strlcpy(settings->arrays.midi_output,
            midi_output, sizeof(settings->arrays.midi_output));

   /* Avoid reloading config on every content load */
   if (default_block_config_read)
      rarch_ctl(RARCH_CTL_SET_BLOCK_CONFIG_READ, NULL);
   else
      rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);

#ifdef HAVE_MENU
   first_initialized = false;
#endif
}

/**
 * open_default_config_file
 *
 * Open a default config file. Platform-specific.
 *
 * Returns: handle to config file if found, otherwise NULL.
 **/
static config_file_t *open_default_config_file(void)
{
   bool has_application_data              = false;
   size_t path_size                       = PATH_MAX_LENGTH * sizeof(char);
   char *application_data                 = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *conf_path                        = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *app_path                         = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   config_file_t *conf                    = NULL;

   (void)has_application_data;

   application_data[0] = conf_path[0] = app_path[0] = '\0';

   (void)path_size;

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   fill_pathname_application_path(app_path, path_size);
   fill_pathname_resolve_relative(conf_path, app_path,
         file_path_str(FILE_PATH_MAIN_CONFIG), path_size);

   conf = config_file_new(conf_path);

   if (!conf)
   {
      if (fill_pathname_application_data(application_data,
            path_size))
      {
         fill_pathname_join(conf_path, application_data,
               file_path_str(FILE_PATH_MAIN_CONFIG), path_size);
         conf = config_file_new(conf_path);
      }
   }

   if (!conf)
   {
      bool saved = false;

      /* Try to create a new config file. */
      conf = config_file_new(NULL);


      if (conf)
      {
         /* Since this is a clean config file, we can
          * safely use config_save_on_exit. */
         fill_pathname_resolve_relative(conf_path, app_path,
               file_path_str(FILE_PATH_MAIN_CONFIG), path_size);
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("Failed to create new config file in: \"%s\".\n",
               conf_path);
         goto error;
      }

      RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
   }
#elif defined(OSX)
   if (!fill_pathname_application_data(application_data,
            path_size))
      goto error;

   /* Group config file with menu configs, remaps, etc: */
   strlcat(application_data, "/config", path_size);

   path_mkdir(application_data);

   fill_pathname_join(conf_path, application_data,
         file_path_str(FILE_PATH_MAIN_CONFIG), path_size);
   conf = config_file_new(conf_path);

   if (!conf)
   {
      bool saved = false;

      conf = config_file_new(NULL);

      if (conf)
      {
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("Failed to create new config file in: \"%s\".\n",
               conf_path);
         goto error;
      }

      RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
   }
#elif !defined(RARCH_CONSOLE)
   has_application_data =
      fill_pathname_application_data(application_data,
            path_size);

   if (has_application_data)
   {
      fill_pathname_join(conf_path, application_data,
            file_path_str(FILE_PATH_MAIN_CONFIG), path_size);
      RARCH_LOG("Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new(conf_path);
   }

   /* Fallback to $HOME/.retroarch.cfg. */
   if (!conf && getenv("HOME"))
   {
      fill_pathname_join(conf_path, getenv("HOME"),
            ".retroarch.cfg", path_size);
      RARCH_LOG("Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new(conf_path);
   }

   if (!conf && has_application_data)
   {
      char *basedir = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      basedir[0]    = '\0';

      /* Try to create a new config file. */

      strlcpy(conf_path, application_data, path_size);

      fill_pathname_basedir(basedir, conf_path, path_size);

      fill_pathname_join(conf_path, conf_path,
            file_path_str(FILE_PATH_MAIN_CONFIG), path_size);

      if (path_mkdir(basedir))
      {
         char *skeleton_conf = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
         bool saved          = false;

         skeleton_conf[0] = '\0';

         free(basedir);

         /* Build a retroarch.cfg path from the global config directory (/etc). */
         fill_pathname_join(skeleton_conf, GLOBAL_CONFIG_DIR,
            file_path_str(FILE_PATH_MAIN_CONFIG), path_size);

         conf = config_file_new(skeleton_conf);
         if (conf)
            RARCH_WARN("Config: using skeleton config \"%s\" as base for a new config file.\n", skeleton_conf);
         else
            conf = config_file_new(NULL);

         free(skeleton_conf);

         if (conf)
         {
            /* Since this is a clean config file, we can safely use config_save_on_exit. */
            config_set_bool(conf, "config_save_on_exit", true);
            saved = config_file_write(conf, conf_path);
         }

         if (!saved)
         {
            /* WARN here to make sure user has a good chance of seeing it. */
            RARCH_ERR("Failed to create new config file in: \"%s\".\n", conf_path);
            goto error;
         }

         RARCH_WARN("Config: Created new config file in: \"%s\".\n", conf_path);
      }
      else
      {
         free(basedir);
      }
   }
#endif

   (void)application_data;
   (void)conf_path;
   (void)app_path;

   if (!conf)
      goto error;

   path_set(RARCH_PATH_CONFIG, conf_path);
   free(application_data);
   free(conf_path);
   free(app_path);

   return conf;

error:
   if (conf)
      config_file_free(conf);
   free(application_data);
   free(conf_path);
   free(app_path);
   return NULL;
}

static void read_keybinds_keyboard(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map_get_valid(idx))
      return;

   if (!input_config_bind_map_get_base(idx))
      return;

   prefix = input_config_get_prefix(user, input_config_bind_map_get_meta(idx));

   if (prefix)
      input_config_parse_key(conf, prefix,
            input_config_bind_map_get_base(idx), bind);
}

static void read_keybinds_button(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map_get_valid(idx))
      return;
   if (!input_config_bind_map_get_base(idx))
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map_get_meta(idx));

   if (prefix)
      input_config_parse_joy_button(conf, prefix,
            input_config_bind_map_get_base(idx), bind);
}

static void read_keybinds_axis(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map_get_valid(idx))
      return;
   if (!input_config_bind_map_get_base(idx))
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map_get_meta(idx));

   if (prefix)
      input_config_parse_joy_axis(conf, prefix,
            input_config_bind_map_get_base(idx), bind);
}

static void read_keybinds_mbutton(config_file_t *conf, unsigned user,
      unsigned idx, struct retro_keybind *bind)
{
   const char *prefix = NULL;

   if (!input_config_bind_map_get_valid(idx))
      return;
   if (!input_config_bind_map_get_base(idx))
      return;

   prefix = input_config_get_prefix(user,
         input_config_bind_map_get_meta(idx));

   if (prefix)
      input_config_parse_mouse_button(conf, prefix,
            input_config_bind_map_get_base(idx), bind);
}

static void read_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i;

   for (i = 0; input_config_bind_map_get_valid(i); i++)
   {
      struct retro_keybind *bind = &input_config_binds[user][i];

      if (!bind->valid)
         continue;

      read_keybinds_keyboard(conf, user, i, bind);
      read_keybinds_button(conf, user, i, bind);
      read_keybinds_axis(conf, user, i, bind);
      read_keybinds_mbutton(conf, user, i, bind);
   }
}

static void config_read_keybinds_conf(config_file_t *conf)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
      read_keybinds_user(conf, i);
}

static bool check_shader_compatibility(enum file_path_enum enum_idx)
{
   settings_t *settings = config_get_ptr();

   if (string_is_equal(settings->arrays.video_driver, "vulkan") ||
       string_is_equal(settings->arrays.video_driver, "metal") ||
       string_is_equal(settings->arrays.video_driver, "d3d11") ||
       string_is_equal(settings->arrays.video_driver, "d3d12") ||
       string_is_equal(settings->arrays.video_driver, "gx2"))
   {
      if (enum_idx != FILE_PATH_SLANGP_EXTENSION)
         return false;
      return true;
   }

   if (string_is_equal(settings->arrays.video_driver, "gl")   ||
       string_is_equal(settings->arrays.video_driver, "d3d8") ||
       string_is_equal(settings->arrays.video_driver, "d3d9")
      )
   {
      if (enum_idx == FILE_PATH_SLANGP_EXTENSION)
         return false;
      return true;
   }

   return false;
}

#if 0
static bool config_read_keybinds(const char *path)
{
   config_file_t *conf = (config_file_t*)config_file_new(path);

   if (!conf)
      return false;

   config_read_keybinds_conf(conf);
   config_file_free(conf);

   return true;
}

/* Also dumps inherited values, useful for logging. */
static void config_file_dump_all(config_file_t *conf)
{
   struct config_entry_list *list = NULL;
   struct config_include_list *includes = conf->includes;

   while (includes)
   {
      RARCH_LOG("#include \"%s\"\n", includes->path);
      includes = includes->next;
   }

   list = conf->entries;

   while (list)
   {
      RARCH_LOG("%s = \"%s\"%s\n", list->key,
            list->value, list->readonly ? " (included)" : "");
      list = list->next;
   }
}
#endif

#ifdef HAVE_MENU
static void config_get_hex_base(config_file_t *conf,
      const char *key, unsigned *base)
{
   unsigned tmp = 0;
   if (!base)
      return;
   if (config_get_hex(conf, key, &tmp))
      *base = tmp;
}
#endif


/**
 * config_load:
 * @path                : path to be read from.
 * @set_defaults        : set default values first before
 *                        reading the values from the config file
 *
 * Loads a config file and reads all the values into memory.
 *
 */
static bool config_load_file(const char *path, bool set_defaults,
   settings_t *settings)
{
   unsigned i;
   size_t path_size                                = PATH_MAX_LENGTH * sizeof(char);
   char *tmp_str                                   = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   bool ret                                        = false;
   bool tmp_bool                                   = false;
   char *save                                      = NULL;
   unsigned msg_color                              = 0;
   config_file_t *conf                             = NULL;
   char *override_username                         = NULL;
   const char *path_core                           = NULL;
   const char *path_config                         = NULL;
   const char *shader_ext                          = NULL;
   int bool_settings_size                          = sizeof(settings->bools)  / sizeof(settings->bools.placeholder);
   int float_settings_size                         = sizeof(settings->floats) / sizeof(settings->floats.placeholder);
   int int_settings_size                           = sizeof(settings->ints)   / sizeof(settings->ints.placeholder);
   int uint_settings_size                          = sizeof(settings->uints)  / sizeof(settings->uints.placeholder);
   int size_settings_size                          = sizeof(settings->sizes)  / sizeof(settings->sizes.placeholder);
   int array_settings_size                         = sizeof(settings->arrays) / sizeof(settings->arrays.placeholder);
   int path_settings_size                          = sizeof(settings->paths)  / sizeof(settings->paths.placeholder);
   struct config_bool_setting *bool_settings       = populate_settings_bool  (settings, &bool_settings_size);
   struct config_float_setting *float_settings     = populate_settings_float (settings, &float_settings_size);
   struct config_int_setting *int_settings         = populate_settings_int   (settings, &int_settings_size);
   struct config_uint_setting *uint_settings       = populate_settings_uint  (settings, &uint_settings_size);
   struct config_size_setting *size_settings       = populate_settings_size  (settings, &size_settings_size);
   struct config_array_setting *array_settings     = populate_settings_array (settings, &array_settings_size);
   struct config_path_setting *path_settings       = populate_settings_path  (settings, &path_settings_size);

   tmp_str[0] = '\0';

   if (path)
   {
      conf = config_file_new(path);
      if (!conf)
         goto end;
   }
   else
      conf = open_default_config_file();

   if (!conf)
   {
      ret = true;
      goto end;
   }

   if (set_defaults)
      config_set_defaults();

   if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
   {
      /* Don't destroy append_config_path, store in temporary
       * variable. */
      char *tmp_append_path  = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      const char *extra_path = NULL;

      tmp_append_path[0] = '\0';

      strlcpy(tmp_append_path, path_get(RARCH_PATH_CONFIG_APPEND),
            path_size);
      extra_path = strtok_r(tmp_append_path, "|", &save);

      while (extra_path)
      {
         bool ret = config_append_file(conf, extra_path);

         RARCH_LOG("Config: appending config \"%s\"\n", extra_path);

         if (!ret)
            RARCH_ERR("Config: failed to append config \"%s\"\n", extra_path);
         extra_path = strtok_r(NULL, "|", &save);
      }

      free(tmp_append_path);
   }

#if 0
   if (verbosity_is_enabled())
   {
      RARCH_LOG_OUTPUT("=== Config ===\n");
      config_file_dump_all(conf);
      RARCH_LOG_OUTPUT("=== Config end ===\n");
   }
#endif

   /* Overrides */

   if (rarch_ctl(RARCH_CTL_HAS_SET_USERNAME, NULL))
      override_username = strdup(settings->paths.username);

   /* Boolean settings */

   for (i = 0; i < (unsigned)bool_settings_size; i++)
   {
      bool tmp = false;
      if (config_get_bool(conf, bool_settings[i].ident, &tmp))
         *bool_settings[i].ptr = tmp;
   }

#ifdef HAVE_NETWORKGAMEPAD
   for (i = 0; i < MAX_USERS; i++)
   {
      char tmp[64];

      tmp[0] = '\0';

      snprintf(tmp, sizeof(tmp), "network_remote_enable_user_p%u", i + 1);

      if (config_get_bool(conf, tmp, &tmp_bool))
         settings->bools.network_remote_enable_user[i] = tmp_bool;
   }
#endif
   if (config_get_bool(conf, "log_verbosity", &tmp_bool))
   {
      if (tmp_bool)
         verbosity_enable();
      else
         verbosity_disable();
   }
   {
      char tmp[64];

      tmp[0] = '\0';

      strlcpy(tmp, "perfcnt_enable", sizeof(tmp));
      if (config_get_bool(conf, tmp, &tmp_bool))
      {
         if (tmp_bool)
            rarch_ctl(RARCH_CTL_SET_PERFCNT_ENABLE, NULL);
         else
            rarch_ctl(RARCH_CTL_UNSET_PERFCNT_ENABLE, NULL);
      }
   }

   /* Integer settings */

   for (i = 0; i < (unsigned)int_settings_size; i++)
   {
      int tmp = 0;
      if (config_get_int(conf, int_settings[i].ident, &tmp))
         *int_settings[i].ptr = tmp;
   }

   for (i = 0; i < (unsigned)uint_settings_size; i++)
   {
      int tmp = 0;
      if (config_get_int(conf, uint_settings[i].ident, &tmp))
         *uint_settings[i].ptr = tmp;
   }

   for (i = 0; i < (unsigned)size_settings_size; i++)
   {
      size_t tmp = 0;
      if (config_get_size_t(conf, size_settings[i].ident, &tmp))
         *size_settings[i].ptr = tmp ;
      /* Special case for rewind_buffer_size - need to convert low values to what they were
       * intended to be based on the default value in config.def.h
       * If the value is less than 10000 then multiple by 1MB because if the retroarch.cfg
       * file contains rewind_buffer_size = "100" then that ultimately gets interpreted as
       * 100MB, so ensure the internal values represent that.*/
      if (string_is_equal(size_settings[i].ident, "rewind_buffer_size"))
         if (*size_settings[i].ptr < 10000)
            *size_settings[i].ptr  = *size_settings[i].ptr * 1024 * 1024;
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      char buf[64];

      buf[0] = '\0';

      snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_joypad_map[i], buf);

      snprintf(buf, sizeof(buf), "input_player%u_analog_dpad_mode", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_analog_dpad_mode[i], buf);

      snprintf(buf, sizeof(buf), "input_player%u_mouse_index", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_mouse_index[i], buf);

      snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_libretro_device[i], buf);
   }

   /* LED map for use by the led driver */
   for (i = 0; i < MAX_LEDS; i++)
   {
      char buf[64];

      buf[0] = '\0';

      snprintf(buf, sizeof(buf), "led%u_map", i + 1);
      settings->uints.led_map[i]=-1;
      CONFIG_GET_INT_BASE(conf, settings, uints.led_map[i], buf);
   }


   /* Hexadecimal settings  */

   if (config_get_hex(conf, "video_message_color", &msg_color))
   {
      settings->floats.video_msg_color_r = ((msg_color >> 16) & 0xff) / 255.0f;
      settings->floats.video_msg_color_g = ((msg_color >>  8) & 0xff) / 255.0f;
      settings->floats.video_msg_color_b = ((msg_color >>  0) & 0xff) / 255.0f;
   }
#ifdef HAVE_MENU
   config_get_hex_base(conf, "menu_entry_normal_color",
         &settings->uints.menu_entry_normal_color);
   config_get_hex_base(conf, "menu_entry_hover_color",
         &settings->uints.menu_entry_hover_color);
   config_get_hex_base(conf, "menu_title_color",
         &settings->uints.menu_title_color);
#endif

   /* Float settings */
   for (i = 0; i < (unsigned)float_settings_size; i++)
   {
      float tmp = 0.0f;
      if (config_get_float(conf, float_settings[i].ident, &tmp))
         *float_settings[i].ptr = tmp;
   }

   /* Array settings  */
   for (i = 0; i < (unsigned)array_settings_size; i++)
   {
      if (!array_settings[i].handle)
         continue;
      config_get_array(conf, array_settings[i].ident,
            array_settings[i].ptr, PATH_MAX_LENGTH);
   }

   /* Path settings  */
   for (i = 0; i < (unsigned)path_settings_size; i++)
   {
      if (!path_settings[i].handle)
         continue;
      if (config_get_path(conf, path_settings[i].ident, tmp_str, path_size))
         strlcpy(path_settings[i].ptr, tmp_str, PATH_MAX_LENGTH);
   }

   if (config_get_path(conf, "libretro_directory", tmp_str, path_size))
      strlcpy(settings->paths.directory_libretro, tmp_str, sizeof(settings->paths.directory_libretro));

#ifndef HAVE_DYNAMIC
   if (config_get_path(conf, "libretro_path", tmp_str, path_size))
      path_set(RARCH_PATH_CORE, tmp_str);
#endif

#ifdef RARCH_CONSOLE
   video_driver_load_settings(conf);
#endif

   /* Post-settings load */

   if (rarch_ctl(RARCH_CTL_HAS_SET_USERNAME, NULL) && override_username)
   {
      strlcpy(settings->paths.username,
            override_username,
            sizeof(settings->paths.username));
      free(override_username);
   }

   if (settings->uints.video_hard_sync_frames > 3)
      settings->uints.video_hard_sync_frames = 3;

   if (settings->uints.video_frame_delay > 15)
      settings->uints.video_frame_delay = 15;

   settings->uints.video_swap_interval = MAX(settings->uints.video_swap_interval, 1);
   settings->uints.video_swap_interval = MIN(settings->uints.video_swap_interval, 4);

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, settings->floats.audio_volume);
   audio_set_float(AUDIO_ACTION_MIXER_VOLUME_GAIN, settings->floats.audio_mixer_volume);

   path_config = path_get(RARCH_PATH_CONFIG);
   path_core   = path_get(RARCH_PATH_CORE);

   if (string_is_empty(settings->paths.path_content_history))
   {
      if (string_is_empty(settings->paths.directory_content_history))
      {
         fill_pathname_resolve_relative(
               settings->paths.path_content_history,
               path_config,
               file_path_str(FILE_PATH_CONTENT_HISTORY),
               sizeof(settings->paths.path_content_history));
      }
      else
      {
         fill_pathname_join(settings->paths.path_content_history,
               settings->paths.directory_content_history,
               file_path_str(FILE_PATH_CONTENT_HISTORY),
               sizeof(settings->paths.path_content_history));
      }
   }

   if (string_is_empty(settings->paths.path_content_favorites))
   {
      if (string_is_empty(settings->paths.directory_content_history))
      {
         fill_pathname_resolve_relative(
               settings->paths.path_content_favorites,
               path_config,
               file_path_str(FILE_PATH_CONTENT_FAVORITES),
               sizeof(settings->paths.path_content_favorites));
      }
      else
      {
         fill_pathname_join(settings->paths.path_content_favorites,
               settings->paths.directory_content_history,
               file_path_str(FILE_PATH_CONTENT_FAVORITES),
               sizeof(settings->paths.path_content_favorites));
      }
   }

   if (string_is_empty(settings->paths.path_content_music_history))
   {
      if (string_is_empty(settings->paths.directory_content_history))
      {
         fill_pathname_resolve_relative(
               settings->paths.path_content_music_history,
               path_config,
               file_path_str(FILE_PATH_CONTENT_MUSIC_HISTORY),
               sizeof(settings->paths.path_content_music_history));
      }
      else
      {
         fill_pathname_join(settings->paths.path_content_music_history,
               settings->paths.directory_content_history,
               file_path_str(FILE_PATH_CONTENT_MUSIC_HISTORY),
               sizeof(settings->paths.path_content_music_history));
      }
   }

   if (string_is_empty(settings->paths.path_content_video_history))
   {
      if (string_is_empty(settings->paths.directory_content_history))
      {
         fill_pathname_resolve_relative(
               settings->paths.path_content_video_history,
               path_config,
               file_path_str(FILE_PATH_CONTENT_VIDEO_HISTORY),
               sizeof(settings->paths.path_content_video_history));
      }
      else
      {
         fill_pathname_join(settings->paths.path_content_video_history,
               settings->paths.directory_content_history,
               file_path_str(FILE_PATH_CONTENT_VIDEO_HISTORY),
               sizeof(settings->paths.path_content_video_history));
      }
   }

   if (string_is_empty(settings->paths.path_content_image_history))
   {
      if (string_is_empty(settings->paths.directory_content_history))
      {
         fill_pathname_resolve_relative(
               settings->paths.path_content_image_history,
               path_config,
               file_path_str(FILE_PATH_CONTENT_IMAGE_HISTORY),
               sizeof(settings->paths.path_content_image_history));
      }
      else
      {
         fill_pathname_join(settings->paths.path_content_image_history,
               settings->paths.directory_content_history,
               file_path_str(FILE_PATH_CONTENT_IMAGE_HISTORY),
               sizeof(settings->paths.path_content_image_history));
      }
   }


   if (!string_is_empty(settings->paths.directory_screenshot))
   {
      if (string_is_equal(settings->paths.directory_screenshot, "default"))
         *settings->paths.directory_screenshot = '\0';
      else if (!path_is_directory(settings->paths.directory_screenshot))
      {
         RARCH_WARN("screenshot_directory is not an existing directory, ignoring ...\n");
         *settings->paths.directory_screenshot = '\0';
      }
   }

#ifdef RARCH_CONSOLE
   if (!string_is_empty(path_core))
   {
#endif
      /* Safe-guard against older behavior. */
      if (path_is_directory(path_core))
      {
         RARCH_WARN("\"libretro_path\" is a directory, using this for \"libretro_directory\" instead.\n");
         strlcpy(settings->paths.directory_libretro, path_core,
               sizeof(settings->paths.directory_libretro));
         path_clear(RARCH_PATH_CORE);
      }
#ifdef RARCH_CONSOLE
   }
#endif

   if (string_is_equal(settings->paths.path_menu_wallpaper, "default"))
      *settings->paths.path_menu_wallpaper = '\0';
   if (string_is_equal(settings->paths.directory_video_shader, "default"))
      *settings->paths.directory_video_shader = '\0';
   if (string_is_equal(settings->paths.directory_video_filter, "default"))
      *settings->paths.directory_video_filter = '\0';
   if (string_is_equal(settings->paths.directory_audio_filter, "default"))
      *settings->paths.directory_audio_filter = '\0';
   if (string_is_equal(settings->paths.directory_core_assets, "default"))
      *settings->paths.directory_core_assets = '\0';
   if (string_is_equal(settings->paths.directory_assets, "default"))
      *settings->paths.directory_assets = '\0';
   if (string_is_equal(settings->paths.directory_dynamic_wallpapers, "default"))
      *settings->paths.directory_dynamic_wallpapers = '\0';
   if (string_is_equal(settings->paths.directory_thumbnails, "default"))
      *settings->paths.directory_thumbnails = '\0';
   if (string_is_equal(settings->paths.directory_playlist, "default"))
      *settings->paths.directory_playlist = '\0';
#ifdef HAVE_MENU
   if (string_is_equal(settings->paths.directory_menu_content, "default"))
      *settings->paths.directory_menu_content = '\0';
   if (string_is_equal(settings->paths.directory_menu_config, "default"))
      *settings->paths.directory_menu_config = '\0';
#endif
#ifdef HAVE_OVERLAY
   if (string_is_equal(settings->paths.directory_overlay, "default"))
      *settings->paths.directory_overlay = '\0';
#endif
   if (string_is_equal(settings->paths.directory_system, "default"))
      *settings->paths.directory_system = '\0';

   if (settings->floats.slowmotion_ratio < 1.0f)
      configuration_set_float(settings, settings->floats.slowmotion_ratio, 1.0f);

   /* Sanitize fastforward_ratio value - previously range was -1
    * and up (with 0 being skipped) */
   if (settings->floats.fastforward_ratio < 0.0f)
      configuration_set_float(settings, settings->floats.fastforward_ratio, 0.0f);


#ifdef HAVE_LAKKA
   settings->bools.ssh_enable       = filestream_exists(LAKKA_SSH_PATH);
   settings->bools.samba_enable     = filestream_exists(LAKKA_SAMBA_PATH);
   settings->bools.bluetooth_enable = filestream_exists(LAKKA_BLUETOOTH_PATH);
#endif

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         config_get_path(conf, "savefile_directory", tmp_str, path_size))
   {
      if (string_is_equal(tmp_str, "default"))
         dir_set(RARCH_DIR_SAVEFILE, g_defaults.dirs[DEFAULT_DIR_SRAM]);

      else if (path_is_directory(tmp_str))
      {
         global_t   *global = global_get_ptr();

         dir_set(RARCH_DIR_SAVEFILE, tmp_str);

         if (global)
         {
            strlcpy(global->name.savefile, tmp_str,
                  sizeof(global->name.savefile));
            fill_pathname_dir(global->name.savefile,
                  path_get(RARCH_PATH_BASENAME),
                  file_path_str(FILE_PATH_SRM_EXTENSION),
                  sizeof(global->name.savefile));
         }
      }
      else
         RARCH_WARN("savefile_directory is not a directory, ignoring ...\n");
   }

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         config_get_path(conf, "savestate_directory", tmp_str, path_size))
   {
      if (string_is_equal(tmp_str, "default"))
         dir_set(RARCH_DIR_SAVESTATE, g_defaults.dirs[DEFAULT_DIR_SAVESTATE]);
      else if (path_is_directory(tmp_str))
      {
         global_t   *global = global_get_ptr();

         dir_set(RARCH_DIR_SAVESTATE, tmp_str);

         if (global)
         {
            strlcpy(global->name.savestate, tmp_str,
                  sizeof(global->name.savestate));
            fill_pathname_dir(global->name.savestate,
                  path_get(RARCH_PATH_BASENAME),
                  file_path_str(FILE_PATH_STATE_EXTENSION),
                  sizeof(global->name.savestate));
         }
      }
      else
         RARCH_WARN("savestate_directory is not a directory, ignoring ...\n");
   }

   config_read_keybinds_conf(conf);

   shader_ext = path_get_extension(settings->paths.path_shader);

   if (!string_is_empty(shader_ext))
   {
      for(i = FILE_PATH_CGP_EXTENSION; i <= FILE_PATH_SLANGP_EXTENSION; i++)
      {
         enum file_path_enum ext = (enum file_path_enum)(i);
         if (!strstr(file_path_str(ext), shader_ext))
            continue;

         if (check_shader_compatibility(ext))
            continue;

         RARCH_LOG("Incompatible shader for backend %s, clearing...\n",
               settings->arrays.video_driver);
         settings->paths.path_shader[0] = '\0';
         break;
      }
   }

   frontend_driver_set_sustained_performance_mode(settings->bools.sustained_performance_mode);

   ret = true;


end:
   if (conf)
      config_file_free(conf);
   if (bool_settings)
      free(bool_settings);
   if (int_settings)
      free(int_settings);
   if (uint_settings)
      free(uint_settings);
   if (float_settings)
      free(float_settings);
   if (array_settings)
      free(array_settings);
   if (path_settings)
      free(path_settings);
   if (size_settings)
      free(size_settings);
   free(tmp_str);
   return ret;
}

/**
 * config_load_override:
 *
 * Tries to append game-specific and core-specific configuration.
 * These settings will always have precedence, thus this feature
 * can be used to enforce overrides.
 *
 * This function only has an effect if a game-specific or core-specific
 * configuration file exists at respective locations.
 *
 * core-specific: $CONFIG_DIR/$CORE_NAME/$CORE_NAME.cfg
 * fallback:      $CURRENT_CFG_LOCATION/$CORE_NAME/$CORE_NAME.cfg
 *
 * game-specific: $CONFIG_DIR/$CORE_NAME/$ROM_NAME.cfg
 * fallback:      $CURRENT_CFG_LOCATION/$CORE_NAME/$GAME_NAME.cfg
 *
 * Returns: false if there was an error or no action was performed.
 *
 */
bool config_load_override(void)
{
   size_t path_size                       = PATH_MAX_LENGTH * sizeof(char);
   char *buf                              = NULL;
   char *core_path                        = NULL;
   char *game_path                        = NULL;
   char *content_path                     = NULL;
   char *config_directory                 = NULL;
   config_file_t *new_conf                = NULL;
   bool should_append                     = false;
   rarch_system_info_t *system            = runloop_get_system_info();
   const char *core_name                  = system ?
      system->info.library_name : NULL;
   const char *game_name                  = path_basename(path_get(RARCH_PATH_BASENAME));
   char content_dir_name[PATH_MAX_LENGTH];

   if (!string_is_empty(path_get(RARCH_PATH_BASENAME)))
      fill_pathname_parent_dir_name(content_dir_name, path_get(RARCH_PATH_BASENAME), sizeof(content_dir_name));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   game_path                              = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   core_path                              = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   content_path = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   buf                                    = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   config_directory                       = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   config_directory[0] = core_path[0] = game_path[0] = '\0';

   fill_pathname_application_special(config_directory, path_size,
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* Concatenate strings into full paths for core_path, game_path, content_path */
   fill_pathname_join_special_ext(game_path,
         config_directory, core_name,
         game_name,
         file_path_str(FILE_PATH_CONFIG_EXTENSION),
         path_size);

   fill_pathname_join_special_ext(content_path,
      config_directory, core_name,
      content_dir_name,
      file_path_str(FILE_PATH_CONFIG_EXTENSION),
      path_size);

   fill_pathname_join_special_ext(core_path,
         config_directory, core_name,
         core_name,
         file_path_str(FILE_PATH_CONFIG_EXTENSION),
         path_size);

   /* per-core overrides */
   /* Create a new config file from core_path */
   new_conf = config_file_new(core_path);

   if (new_conf)
   {
      RARCH_LOG("[overrides] core-specific overrides found at %s.\n",
            core_path);

      config_file_free(new_conf);
      path_set(RARCH_PATH_CONFIG_APPEND, core_path);

      should_append = true;
   }
   else
      RARCH_LOG("[overrides] no core-specific overrides found at %s.\n",
            core_path);

   /* per-content-dir overrides */
   /* Create a new config file from content_path */
   new_conf = config_file_new(content_path);

   if (new_conf)
   {
      char *temp_path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      temp_path[0]    = '\0';

      config_file_free(new_conf);

      RARCH_LOG("[overrides] content-dir-specific overrides found at %s.\n",
            game_path);

      if (should_append)
      {
         RARCH_LOG("[overrides] content-dir-specific overrides stacking on top of previous overrides.\n");
         strlcpy(temp_path, path_get(RARCH_PATH_CONFIG_APPEND), path_size);
         strlcat(temp_path, "|", path_size);
         strlcat(temp_path, content_path, path_size);
      }
      else
         strlcpy(temp_path, content_path, path_size);

      path_set(RARCH_PATH_CONFIG_APPEND, temp_path);

      free(temp_path);

      should_append = true;
   }
   else
      RARCH_LOG("[overrides] no content-dir-specific overrides found at %s.\n",
         content_path);

   /* per-game overrides */
   /* Create a new config file from game_path */
   new_conf = config_file_new(game_path);

   if (new_conf)
   {
      char *temp_path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      temp_path[0]    = '\0';

      config_file_free(new_conf);

      RARCH_LOG("[overrides] game-specific overrides found at %s.\n",
            game_path);

      if (should_append)
      {
         RARCH_LOG("[overrides] game-specific overrides stacking on top of previous overrides\n");
         strlcpy(temp_path, path_get(RARCH_PATH_CONFIG_APPEND), path_size);
         strlcat(temp_path, "|", path_size);
         strlcat(temp_path, game_path, path_size);
      }
      else
         strlcpy(temp_path, game_path, path_size);

      path_set(RARCH_PATH_CONFIG_APPEND, temp_path);

      free(temp_path);

      should_append = true;
   }
   else
      RARCH_LOG("[overrides] no game-specific overrides found at %s.\n",
            game_path);

   if (!should_append)
      goto error;

   /* Re-load the configuration with any overrides
    * that might have been found */
   buf[0] = '\0';

   /* Store the libretro_path we're using since it will be
    * overwritten by the override when reloading. */
   strlcpy(buf, path_get(RARCH_PATH_CORE), path_size);

   /* Toggle has_save_path to false so it resets */
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_SAVE_PATH,  NULL);

   if (!config_load_file(path_get(RARCH_PATH_CONFIG), false, config_get_ptr()))
      goto error;

   /* Restore the libretro_path we're using
    * since it will be overwritten by the override when reloading. */
   path_set(RARCH_PATH_CORE, buf);
   runloop_msg_queue_push(msg_hash_to_str(MSG_CONFIG_OVERRIDE_LOADED),
         1, 100, true);

   /* Reset save paths. */
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);

   path_clear(RARCH_PATH_CONFIG_APPEND);

   free(buf);
   free(config_directory);
   free(core_path);
   free(content_path);
   free(game_path);
   return true;

error:
   free(buf);
   free(config_directory);
   free(core_path);
   free(content_path);
   free(game_path);
   return false;
}

/**
 * config_unload_override:
 *
 * Unloads configuration overrides if overrides are active.
 *
 *
 * Returns: false if there was an error.
 */
bool config_unload_override(void)
{
   path_clear(RARCH_PATH_CONFIG_APPEND);

   /* Toggle has_save_path to false so it resets */
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_SAVE_PATH,  NULL);

   if (!config_load_file(path_get(RARCH_PATH_CONFIG), false, config_get_ptr()))
      return false;

   RARCH_LOG("[overrides] configuration overrides unloaded, original configuration restored.\n");

   /* Reset save paths */
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);

   return true;
}

/**
 * config_load_remap:
 *
 * Tries to append game-specific and core-specific remap files.
 *
 * This function only has an effect if a game-specific or core-specific
 * configuration file exists at respective locations.
 *
 * core-specific: $REMAP_DIR/$CORE_NAME/$CORE_NAME.cfg
 * game-specific: $REMAP_DIR/$CORE_NAME/$GAME_NAME.cfg
 *
 * Returns: false if there was an error or no action was performed.
 */
bool config_load_remap(void)
{
   size_t path_size                       = PATH_MAX_LENGTH * sizeof(char);
   char *remap_directory                  = NULL;
   char *core_path                        = NULL;
   char *game_path                        = NULL;
   char *content_path                     = NULL;
   config_file_t *new_conf                = NULL;
   settings_t *settings                   = config_get_ptr();
   rarch_system_info_t *system            = runloop_get_system_info();
   const char *core_name                  = system ? system->info.library_name : NULL;
   const char *game_name                  = path_basename(path_get(RARCH_PATH_BASENAME));
   char content_dir_name[PATH_MAX_LENGTH];

   if (!string_is_empty(path_get(RARCH_PATH_BASENAME)))
      fill_pathname_parent_dir_name(content_dir_name, path_get(RARCH_PATH_BASENAME), sizeof(content_dir_name));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   /* Remap directory: remap_directory.
    * Try remap directory setting, no fallbacks defined */
   if (string_is_empty(settings->paths.directory_input_remapping))
      return false;

   /* path to the directory containing retroarch.cfg (prefix)    */
   remap_directory                        = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   /* final path for core-specific configuration (prefix+suffix) */
   core_path                              = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   /* final path for game-specific configuration (prefix+suffix) */
   game_path                              = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   /* final path for content-dir-specific configuration (prefix+suffix) */
   content_path = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   remap_directory[0] = core_path[0] = game_path[0] = '\0';


   strlcpy(remap_directory,
         settings->paths.directory_input_remapping,
         path_size);
   RARCH_LOG("Remaps: remap directory: %s\n", remap_directory);

   /* Concatenate strings into full paths for core_path, game_path */
   fill_pathname_join_special_ext(core_path,
         remap_directory, core_name,
         core_name,
         file_path_str(FILE_PATH_REMAP_EXTENSION),
         path_size);

   fill_pathname_join_special_ext(content_path,
         remap_directory, core_name,
         content_dir_name,
         file_path_str(FILE_PATH_REMAP_EXTENSION),
         path_size);

   fill_pathname_join_special_ext(game_path,
         remap_directory, core_name,
         game_name,
         file_path_str(FILE_PATH_REMAP_EXTENSION),
         path_size);

   /* Create a new config file from game_path */
   new_conf = config_file_new(game_path);

   /* If a game remap file exists, load it. */
   if (new_conf)
   {
      RARCH_LOG("Remaps: game-specific remap found at %s.\n", game_path);
      if (input_remapping_load_file(new_conf, game_path))
      {
         runloop_msg_queue_push(msg_hash_to_str(
                  MSG_GAME_REMAP_FILE_LOADED), 1, 100, true);
         rarch_ctl(RARCH_CTL_SET_REMAPS_GAME_ACTIVE, NULL);
         goto success;
      }
   }
   else
   {
      RARCH_LOG("Remaps: no game-specific remap found at %s.\n", game_path);
      input_remapping_set_defaults(false);
   }

   /* Create a new config file from content_path */
   new_conf = config_file_new(content_path);

   /* If a content-dir remap file exists, load it. */
   if (new_conf)
   {
      RARCH_LOG("Remaps: content-dir-specific remap found at %s.\n", content_path);
      if (input_remapping_load_file(new_conf, content_path))
      {
         runloop_msg_queue_push(msg_hash_to_str(
                  MSG_GAME_REMAP_FILE_LOADED), 1, 100, true);
         rarch_ctl(RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE, NULL);
         goto success;
      }
   }
   else
   {
      RARCH_LOG("Remaps: no content-dir-specific remap found at %s.\n", content_path);
      input_remapping_set_defaults(false);
   }


   /* Create a new config file from core_path */
   new_conf = config_file_new(core_path);

   /* If a core remap file exists, load it. */
   if (new_conf)
   {
      RARCH_LOG("Remaps: core-specific remap found at %s.\n", core_path);
      if (input_remapping_load_file(new_conf, core_path))
      {
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_CORE_REMAP_FILE_LOADED), 1, 100, true);
         rarch_ctl(RARCH_CTL_SET_REMAPS_CORE_ACTIVE, NULL);
         goto success;
      }
   }
   else
   {
      RARCH_LOG("Remaps: no core-specific remap found at %s.\n", core_path);
      input_remapping_set_defaults(false);
   }

   new_conf = NULL;

   free(content_path);
   free(remap_directory);
   free(core_path);
   free(game_path);
   return false;

success:
   free(content_path);
   free(remap_directory);
   free(core_path);
   free(game_path);
   return true;
}

/**
 * config_load_shader_preset:
 *
 * Tries to append game-specific and core-specific shader presets.
 *
 * This function only has an effect if a game-specific or core-specific
 * configuration file exists at respective locations.
 *
 * core-specific: $SHADER_DIR/presets/$CORE_NAME/$CORE_NAME.cfg
 * game-specific: $SHADER_DIR/presets/$CORE_NAME/$GAME_NAME.cfg
 *
 * Returns: false if there was an error or no action was performed.
 */
bool config_load_shader_preset(void)
{
   unsigned idx;
   size_t path_size                       = PATH_MAX_LENGTH * sizeof(char);
   config_file_t *new_conf                = NULL;
   char *shader_directory                 = NULL;
   char *core_path                        = NULL;
   char *game_path                        = NULL;
   char *content_path                     = NULL;
   settings_t *settings                   = config_get_ptr();
   rarch_system_info_t *system            = runloop_get_system_info();
   const char *core_name                  = system
      ? system->info.library_name : NULL;
   const char *game_name                  = path_basename(path_get(RARCH_PATH_BASENAME));
   char content_dir_name[PATH_MAX_LENGTH];

   if (!string_is_empty(path_get(RARCH_PATH_BASENAME)))
      fill_pathname_parent_dir_name(content_dir_name, path_get(RARCH_PATH_BASENAME), sizeof(content_dir_name));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   /* Shader directory: shader_directory.
    * Try shader directory setting, no fallbacks defined */
   if (string_is_empty(settings->paths.directory_video_shader))
      return false;

   /* path to the directory containing retroarch.cfg (prefix)    */
   shader_directory                       = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   /* final path for core-specific configuration (prefix+suffix) */
   core_path                              = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   /* final path for game-specific configuration (prefix+suffix) */
   game_path                              = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   /* final path for parent-dir-specific configuration (prefix+suffix) */
   content_path                            = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));

   shader_directory[0] = core_path[0] = game_path[0] = '\0';

   fill_pathname_join (shader_directory,
         settings->paths.directory_video_shader,
         "presets", path_size);

   RARCH_LOG("Shaders: preset directory: %s\n", shader_directory);

   for(idx = FILE_PATH_CGP_EXTENSION; idx <= FILE_PATH_SLANGP_EXTENSION; idx++)
   {

      if (!check_shader_compatibility((enum file_path_enum)(idx)))
         continue;
      /* Concatenate strings into full paths for core_path, game_path */
      fill_pathname_join_special_ext(core_path,
            shader_directory, core_name,
            core_name,
            file_path_str((enum file_path_enum)(idx)),
            path_size);

      fill_pathname_join_special_ext(game_path,
            shader_directory, core_name,
            game_name,
            file_path_str((enum file_path_enum)(idx)),
            path_size);

      /* Create a new config file from game_path */
      new_conf = config_file_new(game_path);

      if (!new_conf)
      {
         RARCH_LOG("Shaders: no game-specific preset found at %s.\n",
               game_path);
         continue;
      }

      /* Game shader preset exists, load it. */
      RARCH_LOG("Shaders: game-specific shader preset found at %s.\n",
            game_path);
      retroarch_set_shader_preset(game_path);
      goto success;
   }

   for(idx = FILE_PATH_CGP_EXTENSION; idx <= FILE_PATH_SLANGP_EXTENSION; idx++)
   {
      if (!check_shader_compatibility((enum file_path_enum)(idx)))
         continue;
      /* Concatenate strings into full paths for core_path, parent path */
      fill_pathname_join_special_ext(content_path,
            shader_directory, core_name,
            content_dir_name,
            file_path_str((enum file_path_enum)(idx)),
            path_size);

      /* Create a new config file from parent path */
      new_conf = config_file_new(content_path);

      if (!new_conf)
      {
         RARCH_LOG("Shaders: no content-dir-specific preset found at %s.\n",
               content_path);
         continue;
      }

      /* Parent-dir shader preset exists, load it. */
      RARCH_LOG("Shaders: content-dir-specific shader preset found at %s.\n",
            content_path);
      retroarch_set_shader_preset(content_path);
      goto success;
   }

   for(idx = FILE_PATH_CGP_EXTENSION; idx <= FILE_PATH_SLANGP_EXTENSION; idx++)
   {
      if (!check_shader_compatibility((enum file_path_enum)(idx)))
         continue;
      /* Concatenate strings into full paths for core_path, game_path */
      fill_pathname_join_special_ext(core_path,
            shader_directory, core_name,
            core_name,
            file_path_str((enum file_path_enum)(idx)),
            path_size);

      /* Create a new config file from core_path */
      new_conf = config_file_new(core_path);

      if (!new_conf)
      {
         RARCH_LOG("Shaders: no core-specific preset found at %s.\n",
               core_path);
         continue;
      }

      /* Core shader preset exists, load it. */
      RARCH_LOG("Shaders: core-specific shader preset found at %s.\n",
            core_path);
      retroarch_set_shader_preset(core_path);
      goto success;
   }

   free(shader_directory);
   free(core_path);
   free(game_path);
   free(content_path);
   return false;

success:
   free(shader_directory);
   free(core_path);
   free(game_path);
   free(content_path);
   config_file_free(new_conf);
   return true;
}

static void parse_config_file(void)
{
   if (path_is_empty(RARCH_PATH_CONFIG))
   {
      RARCH_LOG("[Config]: Loading default config.\n");
      if (!path_is_empty(RARCH_PATH_CONFIG))
         RARCH_LOG("[Config]: found default config: %s.\n",
               path_get(RARCH_PATH_CONFIG));
   }

   RARCH_LOG("[Config]: loading config from: %s.\n",
         path_get(RARCH_PATH_CONFIG));

   if (config_load_file(path_get(RARCH_PATH_CONFIG),
            false, config_get_ptr()))
      return;

   RARCH_ERR("[Config]: couldn't find config at path: \"%s\"\n",
         path_get(RARCH_PATH_CONFIG));
}



static void save_keybind_key(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind)
{
   char key[64];
   char btn[64];

   key[0] = btn[0] = '\0';

   fill_pathname_join_delim(key, prefix, base, '_', sizeof(key));

   input_keymaps_translate_rk_to_str(bind->key, btn, sizeof(btn));
   config_set_string(conf, key, btn);
}

static void save_keybind_hat(config_file_t *conf, const char *key,
      const struct retro_keybind *bind)
{
   char config[16];
   unsigned hat     = (unsigned)GET_HAT(bind->joykey);
   const char *dir  = NULL;

   config[0]        = '\0';

   switch (GET_HAT_DIR(bind->joykey))
   {
      case HAT_UP_MASK:
         dir = "up";
         break;

      case HAT_DOWN_MASK:
         dir = "down";
         break;

      case HAT_LEFT_MASK:
         dir = "left";
         break;

      case HAT_RIGHT_MASK:
         dir = "right";
         break;

      default:
         break;
   }

   snprintf(config, sizeof(config), "h%u%s", hat, dir);
   config_set_string(conf, key, config);
}

static void save_keybind_joykey(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];

   key[0] = '\0';

   fill_pathname_join_delim_concat(key, prefix,
         base, '_', "_btn", sizeof(key));

   if (bind->joykey == NO_BTN)
   {
       if (save_empty)
         config_set_string(conf, key, file_path_str(FILE_PATH_NUL));
   }
   else if (GET_HAT_DIR(bind->joykey))
      save_keybind_hat(conf, key, bind);
   else
      config_set_uint64(conf, key, bind->joykey);
}

static void save_keybind_axis(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];
   unsigned axis   = 0;
   char dir        = '\0';

   key[0] = '\0';

   fill_pathname_join_delim_concat(key,
         prefix, base, '_',
         "_axis",
         sizeof(key));

   if (bind->joyaxis == AXIS_NONE)
   {
      if (save_empty)
         config_set_string(conf, key, file_path_str(FILE_PATH_NUL));
   }
   else if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }

   if (dir)
   {
      char config[16];

      config[0] = '\0';

      snprintf(config, sizeof(config), "%c%u", dir, axis);
      config_set_string(conf, key, config);
   }
}

static void save_keybind_mbutton(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];

   key[0] = '\0';

   fill_pathname_join_delim_concat(key, prefix,
      base, '_', "_mbtn", sizeof(key));

   switch ( bind->mbutton )
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         config_set_uint64(conf, key, 1);
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         config_set_uint64(conf, key, 2);
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         config_set_uint64(conf, key, 3);
         break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         config_set_uint64(conf, key, 4);
         break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         config_set_uint64(conf, key, 5);
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         config_set_string(conf, key, "wu");
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         config_set_string(conf, key, "wd");
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         config_set_string(conf, key, "whu");
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         config_set_string(conf, key, "whd");
         break;
      default:
         if (save_empty)
            config_set_string(conf, key, file_path_str(FILE_PATH_NUL));
         break;
   }
}

/**
 * save_keybind:
 * @conf               : pointer to config file object
 * @prefix             : prefix name of keybind
 * @base               : base name   of keybind
 * @bind               : pointer to key binding object
 * @kb                 : save keyboard binds
 *
 * Save a key binding to the config file.
 */
static void save_keybind(config_file_t *conf, const char *prefix,
      const char *base, const struct retro_keybind *bind,
      bool save_kb, bool save_empty)
{
   if (!bind->valid)
      return;
   if (save_kb)
      save_keybind_key(conf, prefix, base, bind);
   save_keybind_joykey(conf, prefix, base, bind, save_empty);
   save_keybind_axis(conf, prefix, base, bind, save_empty);
   save_keybind_mbutton(conf, prefix, base, bind, save_empty);
}

/**
 * save_keybinds_user:
 * @conf               : pointer to config file object
 * @user               : user number
 *
 * Save the current keybinds of a user (@user) to the config file (@conf).
 */
static void save_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i = 0;

   for (i = 0; input_config_bind_map_get_valid(i); i++)
   {
      const char *prefix = input_config_get_prefix(user,
            input_config_bind_map_get_meta(i));

      if (prefix)
         save_keybind(conf, prefix, input_config_bind_map_get_base(i),
               &input_config_binds[user][i], true, true);
   }
}

/**
 * config_load:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_load(void)
{
   /* Flush out some states that could have been
    * set by core environment variables */
   core_unset_input_descriptors();

   if (rarch_ctl(RARCH_CTL_IS_BLOCK_CONFIG_READ, NULL))
      return;

   config_set_defaults();
   parse_config_file();
}

#if 0
/**
 * config_save_keybinds_file:
 * @path            : Path that shall be written to.
 *
 * Writes a keybinds config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
static bool config_save_keybinds_file(const char *path)
{
   unsigned          i = 0;
   bool            ret = false;
   config_file_t *conf = config_file_new(path);

   if (!conf)
      conf = config_file_new(NULL);

   if (!conf)
      return false;

   RARCH_LOG("Saving keybinds config at path: \"%s\"\n", path);

   for (i = 0; i < MAX_USERS; i++)
      save_keybinds_user(conf, i);

   ret = config_file_write(conf, path);
   config_file_free(conf);
   return ret;
}
#endif


/**
 * config_save_autoconf_profile:
 * @path            : Path that shall be written to.
 * @user              : Controller number to save
 * Writes a controller autoconf file to disk.
 **/
bool config_save_autoconf_profile(const char *path, unsigned user)
{
   unsigned i;
   char *buf                            = NULL;
   char *autoconf_file                  = NULL;
   char *path_new                       = NULL;
   config_file_t *conf                  = NULL;
   size_t path_size                     = PATH_MAX_LENGTH * sizeof(char);
   int32_t pid_user                     = 0;
   int32_t vid_user                     = 0;
   bool ret                             = false;
   settings_t *settings                 = config_get_ptr();
   const char *autoconf_dir             = settings->paths.directory_autoconfig;
   const char *joypad_ident             = settings->arrays.input_joypad_driver;

   if (string_is_empty(path))
      return false;

   buf                                  = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   autoconf_file                        = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   buf[0] = autoconf_file[0]            = '\0';
   path_new                             = strdup(path);

   for (i = 0; invalid_filename_chars[i]; i++)
   {
      while (1)
      {
         char *tmp = strstr(path_new, invalid_filename_chars[i]);

         if (tmp)
            *tmp = '_';
         else
            break;
      }
   }

   path = path_new;

   fill_pathname_join(buf, autoconf_dir, joypad_ident, path_size);

   if (path_is_directory(buf))
   {
      char *buf_new = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      buf_new[0] = '\0';

      fill_pathname_join(buf_new, buf,
            path, path_size);
      fill_pathname_noext(autoconf_file, buf_new,
            file_path_str(FILE_PATH_CONFIG_EXTENSION),
            path_size);

      free(buf_new);
   }
   else
   {
      fill_pathname_join(buf, autoconf_dir,
            path, path_size);
      fill_pathname_noext(autoconf_file, buf,
            file_path_str(FILE_PATH_CONFIG_EXTENSION),
            path_size);
   }

   conf = config_file_new(autoconf_file);

   if (!conf)
   {
      conf = config_file_new(NULL);
      if (!conf)
         goto error;
   }

   config_set_string(conf, "input_driver",
         joypad_ident);
   config_set_string(conf, "input_device",
         input_config_get_device_name(user));

   pid_user = input_config_get_pid(user);
   vid_user = input_config_get_vid(user);

   if (pid_user && vid_user)
   {
      config_set_int(conf, "input_vendor_id",
            vid_user);
      config_set_int(conf, "input_product_id",
            pid_user);
   }

   for (i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      save_keybind(conf, "input", input_config_bind_map_get_base(i),
            &input_config_binds[user][i], false, false);
   }

   ret = config_file_write(conf, autoconf_file);

   config_file_free(conf);
   free(buf);
   free(autoconf_file);
   free(path_new);
   return ret;

error:
   free(buf);
   free(autoconf_file);
   free(path_new);
   return false;
}


/**
 * config_save_file:
 * @path            : Path that shall be written to.
 *
 * Writes a config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_file(const char *path)
{
   float msg_color;
   unsigned i                                        = 0;
   bool ret                                          = false;
   struct config_bool_setting     *bool_settings     = NULL;
   struct config_int_setting     *int_settings       = NULL;
   struct config_uint_setting     *uint_settings     = NULL;
   struct config_size_setting     *size_settings     = NULL;
   struct config_float_setting     *float_settings   = NULL;
   struct config_array_setting     *array_settings   = NULL;
   struct config_path_setting     *path_settings     = NULL;
   config_file_t                              *conf  = config_file_new(path);
   settings_t                              *settings = config_get_ptr();
   int bool_settings_size                            = sizeof(settings->bools) / sizeof(settings->bools.placeholder);
   int float_settings_size                           = sizeof(settings->floats)/ sizeof(settings->floats.placeholder);
   int int_settings_size                             = sizeof(settings->ints)  / sizeof(settings->ints.placeholder);
   int uint_settings_size                            = sizeof(settings->uints) / sizeof(settings->uints.placeholder);
   int size_settings_size                            = sizeof(settings->sizes) / sizeof(settings->sizes.placeholder);
   int array_settings_size                           = sizeof(settings->arrays)/ sizeof(settings->arrays.placeholder);
   int path_settings_size                            = sizeof(settings->paths) / sizeof(settings->paths.placeholder);

   if (!conf)
      conf = config_file_new(NULL);

   if (!conf || rarch_ctl(RARCH_CTL_IS_OVERRIDES_ACTIVE, NULL))
   {
      if (conf)
         config_file_free(conf);
      return false;
   }

   bool_settings   = populate_settings_bool  (settings, &bool_settings_size);
   int_settings    = populate_settings_int   (settings, &int_settings_size);
   uint_settings   = populate_settings_uint  (settings, &uint_settings_size);
   size_settings   = populate_settings_size  (settings, &size_settings_size);
   float_settings  = populate_settings_float (settings, &float_settings_size);
   array_settings  = populate_settings_array (settings, &array_settings_size);
   path_settings   = populate_settings_path  (settings, &path_settings_size);

   /* Path settings */
   if (path_settings && (path_settings_size > 0))
   {
      for (i = 0; i < (unsigned)path_settings_size; i++)
      {
         const char *value = path_settings[i].ptr;

         if (path_settings[i].def_enable && string_is_empty(path_settings[i].ptr))
            value = "default";

         config_set_path(conf, path_settings[i].ident, value);
      }

      free(path_settings);
   }

#ifdef HAVE_MENU
   config_set_path(conf, "xmb_font",
         !string_is_empty(settings->paths.path_menu_xmb_font)
         ? settings->paths.path_menu_xmb_font : "");
#endif

   /* String settings  */
   if (array_settings && (array_settings_size > 0))
   {
      for (i = 0; i < (unsigned)array_settings_size; i++)
         if (!array_settings[i].override ||
             !retroarch_override_setting_is_set(array_settings[i].override, NULL))
            config_set_string(conf,
                  array_settings[i].ident,
                  array_settings[i].ptr);

      free(array_settings);
   }

   /* Float settings  */
   if (float_settings && (float_settings_size > 0))
   {
      for (i = 0; i < (unsigned)float_settings_size; i++)
         if (!float_settings[i].override ||
             !retroarch_override_setting_is_set(float_settings[i].override, NULL))
            config_set_float(conf,
                  float_settings[i].ident,
                  *float_settings[i].ptr);

      free(float_settings);
   }

   /* Integer settings */
   if (int_settings && (int_settings_size > 0))
   {
      for (i = 0; i < (unsigned)int_settings_size; i++)
         if (!int_settings[i].override ||
             !retroarch_override_setting_is_set(int_settings[i].override, NULL))
            config_set_int(conf,
                  int_settings[i].ident,
                  *int_settings[i].ptr);

      free(int_settings);
   }

   if (uint_settings && (uint_settings_size > 0))
   {
      for (i = 0; i < (unsigned)uint_settings_size; i++)
         if (!uint_settings[i].override ||
             !retroarch_override_setting_is_set(uint_settings[i].override, NULL))
            config_set_int(conf,
                  uint_settings[i].ident,
                  *uint_settings[i].ptr);

      free(uint_settings);
   }

   if (size_settings && (size_settings_size > 0))
   {
      for (i = 0; i < (unsigned)size_settings_size; i++)
         if (!size_settings[i].override ||
             !retroarch_override_setting_is_set(size_settings[i].override, NULL))
            config_set_int(conf,
                  size_settings[i].ident,
                  (int)*size_settings[i].ptr);

      free(size_settings);
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      char cfg[64];

      cfg[0] = '\0';

      snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
      config_set_int(conf, cfg, settings->uints.input_device[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_joypad_index", i + 1);
      config_set_int(conf, cfg, settings->uints.input_joypad_map[i]);
      snprintf(cfg, sizeof(cfg), "input_libretro_device_p%u", i + 1);
      config_set_int(conf, cfg, input_config_get_device(i));
      snprintf(cfg, sizeof(cfg), "input_player%u_analog_dpad_mode", i + 1);
      config_set_int(conf, cfg, settings->uints.input_analog_dpad_mode[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_mouse_index", i + 1);
      config_set_int(conf, cfg, settings->uints.input_mouse_index[i]);
   }

   /* Boolean settings */
   if (bool_settings && (bool_settings_size > 0))
   {
      for (i = 0; i < (unsigned)bool_settings_size; i++)
         if (!bool_settings[i].override ||
             !retroarch_override_setting_is_set(bool_settings[i].override, NULL))
            config_set_bool(conf, bool_settings[i].ident,
                  *bool_settings[i].ptr);

      free(bool_settings);
   }

#ifdef HAVE_NETWORKGAMEPAD
   for (i = 0; i < MAX_USERS; i++)
   {
      char tmp[64];

      tmp[0] = '\0';

      snprintf(tmp, sizeof(tmp), "network_remote_enable_user_p%u", i + 1);
      config_set_bool(conf, tmp, settings->bools.network_remote_enable_user[i]);
   }
#endif

   /* Verbosity isn't in bool_settings since it needs to be loaded differently */
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL))
   {
      config_set_bool(conf, "log_verbosity",
            verbosity_is_enabled());
   }
   config_set_bool(conf, "perfcnt_enable",
         rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL));

   msg_color = (((int)(settings->floats.video_msg_color_r * 255.0f) & 0xff) << 16) +
               (((int)(settings->floats.video_msg_color_g * 255.0f) & 0xff) <<  8) +
               (((int)(settings->floats.video_msg_color_b * 255.0f) & 0xff));

   /* Hexadecimal settings */
   config_set_hex(conf, "video_message_color", msg_color);
#ifdef HAVE_MENU
   config_set_hex(conf, "menu_entry_normal_color",
         settings->uints.menu_entry_normal_color);
   config_set_hex(conf, "menu_entry_hover_color",
         settings->uints.menu_entry_hover_color);
   config_set_hex(conf, "menu_title_color",
         settings->uints.menu_title_color);
#endif


   video_driver_save_settings(conf);

#ifdef HAVE_LAKKA
   if (settings->bools.ssh_enable)
      filestream_close(filestream_open(LAKKA_SSH_PATH,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE));
   else
      filestream_delete(LAKKA_SSH_PATH);
   if (settings->bools.samba_enable)
      filestream_close(filestream_open(LAKKA_SAMBA_PATH,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE));
   else
      filestream_delete(LAKKA_SAMBA_PATH);
   if (settings->bools.bluetooth_enable)
      filestream_close(filestream_open(LAKKA_BLUETOOTH_PATH,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE));
   else
      filestream_delete(LAKKA_BLUETOOTH_PATH);
#endif

   for (i = 0; i < MAX_USERS; i++)
      save_keybinds_user(conf, i);

   ret = config_file_write(conf, path);
   config_file_free(conf);

   return ret;
}

/**
 * config_save_overrides:
 * @path            : Path that shall be written to.
 *
 * Writes a config file override to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_overrides(int override_type)
{
   size_t path_size                            = PATH_MAX_LENGTH * sizeof(char);
   int tmp_i                                   = 0;
   unsigned i                                  = 0;
   bool ret                                    = false;
   config_file_t *conf                         = NULL;
   settings_t *settings                        = NULL;
   struct config_bool_setting *bool_settings   = NULL;
   struct config_bool_setting *bool_overrides  = NULL;
   struct config_int_setting *int_settings     = NULL;
   struct config_uint_setting *uint_settings   = NULL;
   struct config_size_setting *size_settings   = NULL;
   struct config_int_setting *int_overrides    = NULL;
   struct config_uint_setting *uint_overrides  = NULL;
   struct config_size_setting *size_overrides  = NULL;
   struct config_float_setting *float_settings = NULL;
   struct config_float_setting *float_overrides= NULL;
   struct config_array_setting *array_settings = NULL;
   struct config_array_setting *array_overrides= NULL;
   struct config_path_setting *path_settings   = NULL;
   struct config_path_setting *path_overrides  = NULL;
   char *config_directory                      = NULL;
   char *override_directory                    = NULL;
   char *core_path                             = NULL;
   char *game_path                             = NULL;
   char *content_path                          = NULL;
   settings_t *overrides                       = config_get_ptr();
   int bool_settings_size                      = sizeof(settings->bools)  / sizeof(settings->bools.placeholder);
   int float_settings_size                     = sizeof(settings->floats) / sizeof(settings->floats.placeholder);
   int int_settings_size                       = sizeof(settings->ints)   / sizeof(settings->ints.placeholder);
   int uint_settings_size                      = sizeof(settings->uints)  / sizeof(settings->uints.placeholder);
   int size_settings_size                      = sizeof(settings->sizes)  / sizeof(settings->sizes.placeholder);
   int array_settings_size                     = sizeof(settings->arrays) / sizeof(settings->arrays.placeholder);
   int path_settings_size                      = sizeof(settings->paths)  / sizeof(settings->paths.placeholder);
   rarch_system_info_t *system                 = runloop_get_system_info();
   const char *core_name                       = system ? system->info.library_name : NULL;
   const char *game_name                       = path_basename(path_get(RARCH_PATH_BASENAME));
   char content_dir_name[PATH_MAX_LENGTH];

   if (!string_is_empty(path_get(RARCH_PATH_BASENAME)))
      fill_pathname_parent_dir_name(content_dir_name, path_get(RARCH_PATH_BASENAME), sizeof(content_dir_name));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   settings                                    = (settings_t*)calloc(1, sizeof(settings_t));
   config_directory                            = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   override_directory                          = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   core_path                                   = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   game_path                                   = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   content_path =                                (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

   config_directory[0] = override_directory[0] = core_path[0] = game_path[0] = '\0';

   fill_pathname_application_special(config_directory, path_size,
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   fill_pathname_join(override_directory, config_directory, core_name,
      path_size);

   if (!filestream_exists(override_directory))
       path_mkdir(override_directory);

   /* Concatenate strings into full paths for core_path, game_path */
   fill_pathname_join_special_ext(game_path,
         config_directory, core_name,
         game_name,
         file_path_str(FILE_PATH_CONFIG_EXTENSION),
         path_size);

   fill_pathname_join_special_ext(content_path,
         config_directory, core_name,
         content_dir_name,
         file_path_str(FILE_PATH_CONFIG_EXTENSION),
         path_size);

   fill_pathname_join_special_ext(core_path,
         config_directory, core_name,
         core_name,
         file_path_str(FILE_PATH_CONFIG_EXTENSION),
         path_size);

   if (!conf)
      conf = config_file_new(NULL);

   /* Load the original config file in memory */
   config_load_file(path_get(RARCH_PATH_CONFIG), false, settings);

   bool_settings       = populate_settings_bool(settings,   &bool_settings_size);
   tmp_i               = sizeof(settings->bools) / sizeof(settings->bools.placeholder);
   bool_overrides      = populate_settings_bool(overrides,  &tmp_i);

   int_settings        = populate_settings_int(settings,    &int_settings_size);
   tmp_i               = sizeof(settings->ints) / sizeof(settings->ints.placeholder);
   int_overrides       = populate_settings_int (overrides,  &tmp_i);

   uint_settings       = populate_settings_uint(settings,    &uint_settings_size);
   tmp_i               = sizeof(settings->uints) / sizeof(settings->uints.placeholder);
   uint_overrides      = populate_settings_uint (overrides,  &tmp_i);

   size_settings       = populate_settings_size(settings,    &size_settings_size);
   tmp_i               = sizeof(settings->sizes) / sizeof(settings->sizes.placeholder);
   size_overrides      = populate_settings_size (overrides,  &tmp_i);

   float_settings      = populate_settings_float(settings,  &float_settings_size);
   tmp_i               = sizeof(settings->floats) / sizeof(settings->floats.placeholder);
   float_overrides     = populate_settings_float(overrides, &tmp_i);

   array_settings      = populate_settings_array(settings,  &array_settings_size);
   tmp_i               = sizeof(settings->arrays)   / sizeof(settings->arrays.placeholder);
   array_overrides     = populate_settings_array (overrides, &tmp_i);

   path_settings       = populate_settings_path(settings, &path_settings_size);
   tmp_i               = sizeof(settings->paths)   / sizeof(settings->paths.placeholder);
   path_overrides      = populate_settings_path (overrides, &tmp_i);

   RARCH_LOG("[overrides] looking for changed settings... \n");

   if (conf)
   {
      for (i = 0; i < (unsigned)bool_settings_size; i++)
      {
         if ((*bool_settings[i].ptr) != (*bool_overrides[i].ptr))
         {
            RARCH_LOG("   original: %s=%d\n",
                  bool_settings[i].ident, (*bool_settings[i].ptr));
            RARCH_LOG("   override: %s=%d\n",
                  bool_overrides[i].ident, (*bool_overrides[i].ptr));
            config_set_bool(conf, bool_overrides[i].ident,
                  (*bool_overrides[i].ptr));
         }
      }
      for (i = 0; i < (unsigned)int_settings_size; i++)
      {
         if ((*int_settings[i].ptr) != (*int_overrides[i].ptr))
         {
            RARCH_LOG("   original: %s=%d\n",
                  int_settings[i].ident, (*int_settings[i].ptr));
            RARCH_LOG("   override: %s=%d\n",
                  int_overrides[i].ident, (*int_overrides[i].ptr));
            config_set_int(conf, int_overrides[i].ident,
                  (*int_overrides[i].ptr));
         }
      }
      for (i = 0; i < (unsigned)uint_settings_size; i++)
      {
         if ((*uint_settings[i].ptr) != (*uint_overrides[i].ptr))
         {
            RARCH_LOG("   original: %s=%d\n",
                  uint_settings[i].ident, (*uint_settings[i].ptr));
            RARCH_LOG("   override: %s=%d\n",
                  uint_overrides[i].ident, (*uint_overrides[i].ptr));
            config_set_int(conf, uint_overrides[i].ident,
                  (*uint_overrides[i].ptr));
         }
      }
      for (i = 0; i < (unsigned)size_settings_size; i++)
      {
         if ((*size_settings[i].ptr) != (*size_overrides[i].ptr))
         {
            RARCH_LOG("   original: %s=%d\n",
                  size_settings[i].ident, (*size_settings[i].ptr));
            RARCH_LOG("   override: %s=%d\n",
                  size_overrides[i].ident, (*size_overrides[i].ptr));
            config_set_int(conf, size_overrides[i].ident,
                  (int)(*size_overrides[i].ptr));
         }
      }
      for (i = 0; i < (unsigned)float_settings_size; i++)
      {
         if ((*float_settings[i].ptr) != (*float_overrides[i].ptr))
         {
            RARCH_LOG("   original: %s=%f\n",
                  float_settings[i].ident, *float_settings[i].ptr);
            RARCH_LOG("   override: %s=%f\n",
                  float_overrides[i].ident, *float_overrides[i].ptr);
            config_set_float(conf, float_overrides[i].ident,
                  *float_overrides[i].ptr);
         }
      }

      for (i = 0; i < (unsigned)array_settings_size; i++)
      {
         if (!string_is_equal(array_settings[i].ptr, array_overrides[i].ptr))
         {
            RARCH_LOG("   original: %s=%s\n",
                  array_settings[i].ident, array_settings[i].ptr);
            RARCH_LOG("   override: %s=%s\n",
                  array_overrides[i].ident, array_overrides[i].ptr);
            config_set_string(conf, array_overrides[i].ident,
                  array_overrides[i].ptr);
         }
      }

      for (i = 0; i < (unsigned)path_settings_size; i++)
      {

         /* blacklist video_shader, better handled by shader presets*/
         /* to-do: add setting to control blacklisting */
         if (string_is_equal(path_settings[i].ident, "video_shader"))
            continue;
         if (!string_is_equal(path_settings[i].ptr, path_overrides[i].ptr))
         {
            RARCH_LOG("   original: %s=%s\n",
                  path_settings[i].ident, path_settings[i].ptr);
            RARCH_LOG("   override: %s=%s\n",
                  path_overrides[i].ident, path_overrides[i].ptr);
            config_set_path(conf, path_overrides[i].ident,
                  path_overrides[i].ptr);
         }
      }

      for (i = 0; i < MAX_USERS; i++)
      {
         char cfg[64];

         cfg[0] = '\0';
         if (settings->uints.input_device[i]
               != overrides->uints.input_device[i])
         {
            snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
            config_set_int(conf, cfg, overrides->uints.input_device[i]);
         }

         if (settings->uints.input_joypad_map[i]
               != overrides->uints.input_joypad_map[i])
         {
            snprintf(cfg, sizeof(cfg), "input_player%u_joypad_index", i + 1);
            config_set_int(conf, cfg, overrides->uints.input_joypad_map[i]);
         }


         /* blacklist these since they are handled by remaps */
         /* to-do: add setting to control blacklisting
         if (settings->uints.input_libretro_device[i]
               != overrides->uints.input_libretro_device[i])
         {
            snprintf(cfg, sizeof(cfg), "input_libretro_device_p%u", i + 1);
            config_set_int(conf, cfg, overrides->uints.input_libretro_device[i]);
         }

         if (settings->uints.input_analog_dpad_mode[i]
               != overrides->uints.input_analog_dpad_mode[i])
         {
            snprintf(cfg, sizeof(cfg), "input_player%u_analog_dpad_mode", i + 1);
            config_set_int(conf, cfg, overrides->uints.input_analog_dpad_mode[i]);
         }
         */
      }

      ret = false;

      switch (override_type)
      {
         case OVERRIDE_CORE:
            /* Create a new config file from core_path */
            RARCH_LOG ("[overrides] path %s\n", core_path);
            ret = config_file_write(conf, core_path);
            break;
         case OVERRIDE_GAME:
            /* Create a new config file from core_path */
            RARCH_LOG ("[overrides] path %s\n", game_path);
            ret = config_file_write(conf, game_path);
            break;
         case OVERRIDE_CONTENT_DIR:
            /* Create a new config file from content_path */
            RARCH_LOG ("[overrides] path %s\n", content_path);
            ret = config_file_write(conf, content_path);
            break;
         default:
            break;
      }

      config_file_free(conf);
   }

   if (bool_settings)
      free(bool_settings);
   if (bool_overrides)
      free(bool_overrides);
   if (int_settings)
      free(int_settings);
   if (uint_settings)
      free(uint_settings);
   if (size_settings)
      free(size_settings);
   if (int_overrides)
      free(int_overrides);
   if (uint_overrides)
      free(uint_overrides);
   if (float_settings)
      free(float_settings);
   if (float_overrides)
      free(float_overrides);
   if (array_settings)
      free(array_settings);
   if (array_overrides)
      free(array_overrides);
   if (path_settings)
      free(path_settings);
   if (path_overrides)
      free(path_overrides);
   if (size_overrides)
      free(size_overrides);
   free(settings);
   free(config_directory);
   free(override_directory);
   free(core_path);
   free(game_path);

   return ret;
}

/* Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly. */
bool config_replace(bool config_save_on_exit, char *path)
{
   content_ctx_info_t content_info = {0};

   if (!path)
      return false;

   /* If config file to be replaced is the same as the
    * current config file, exit. */
   if (string_is_equal(path, path_get(RARCH_PATH_CONFIG)))
      return false;

   if (config_save_on_exit && !path_is_empty(RARCH_PATH_CONFIG))
      config_save_file(path_get(RARCH_PATH_CONFIG));

   path_set(RARCH_PATH_CONFIG, path);

   rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);

   /* Load core in new config. */
   path_clear(RARCH_PATH_CORE);

   if (!task_push_start_dummy_core(&content_info))
      return false;

   return true;
}

//
//  metal.m
//  RetroArch_Metal
//
//  Created by Stuart Carnie on 5/14/18.
//

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <retro_assert.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#import "../../config.h"
#endif

#ifdef HAVE_MENU

#import "../../menu/menu_driver.h"

#endif

#import "../font_driver.h"

#import "../common/metal_common.h"

#import "../../driver.h"
#import "../../configuration.h"
#import "../../record/record_driver.h"

#import "../../retroarch.h"
#import "../../verbosity.h"

#import "../video_coord_array.h"

static bool metal_set_shader(void *data,
                             enum rarch_shader_type type, const char *path);

static void *metal_init(const video_info_t *video,
                        const input_driver_t **input,
                        void **input_data)
{
   [apple_platform setViewType:APPLE_VIEW_TYPE_METAL];
   
   MetalDriver *md = [[MetalDriver alloc] initWithVideo:video input:input inputData:input_data];
   if (md == nil)
   {
      return NULL;
   }
   
   const char *shader_path = retroarch_get_shader_preset();
   
   if (shader_path)
   {
      enum rarch_shader_type type = video_shader_parse_type(shader_path, RARCH_SHADER_SLANG);
      metal_set_shader(((__bridge void *)md), type, shader_path);
   }
   
   return (__bridge_retained void *)md;
}

static bool metal_frame(void *data, const void *frame,
                        unsigned frame_width, unsigned frame_height,
                        uint64_t frame_count,
                        unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   return [md renderFrame:frame
                    width:frame_width
                   height:frame_height
               frameCount:frame_count
                    pitch:pitch
                      msg:msg
                     info:video_info];
}

static void metal_set_nonblock_state(void *data, bool non_block)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   md.context.displaySyncEnabled = !non_block;
}

static bool metal_alive(void *data)
{
   return true;
}

static bool metal_has_windowed(void *data)
{
   return true;
}

static bool metal_focus(void *data)
{
   return apple_platform.hasFocus;
}

static bool metal_suppress_screensaver(void *data, bool disable)
{
   RARCH_LOG("[Metal]: suppress screen saver: %s\n", disable ? "YES" : "NO");
   return [apple_platform setDisableDisplaySleep:disable];
}

static bool metal_set_shader(void *data,
                             enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return false;
   if (!path)
      return true;
   
   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[Metal] Only .slang or .slangp shaders are supported. Falling back to stock.\n");
      return false;
   }
   
   return [md.frameView setShaderFromPath:[NSString stringWithUTF8String:path]];
#else
   return false;
#endif
}

static void metal_free(void *data)
{
   MetalDriver *md = (__bridge_transfer MetalDriver *)data;
   md = nil;
}

static void metal_set_viewport(void *data, unsigned viewport_width,
                               unsigned viewport_height, bool force_full, bool allow_rotate)
{
//   RARCH_LOG("[Metal]: set_viewport size: %dx%d full: %s rotate: %s\n",
//             viewport_width, viewport_height,
//             force_full ? "YES" : "NO",
//             allow_rotate ? "YES" : "NO");
}

static void metal_set_rotation(void *data, unsigned rotation)
{
}

static void metal_viewport_info(void *data, struct video_viewport *vp)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   *vp = *md.viewport;
}

static bool metal_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   return [md.frameView readViewport:buffer isIdle:is_idle];
}

static uintptr_t metal_load_texture(void *video_data, void *data,
                                    bool threaded, enum texture_filter_type filter_type)
{
   MetalDriver *md = (__bridge MetalDriver *)video_data;
   struct texture_image *img = (struct texture_image *)data;
   if (!img)
      return 0;
   
   struct texture_image image = *img;
   Texture *t = [md.context newTexture:image filter:filter_type];
   return (uintptr_t)(__bridge_retained void *)(t);
}

static void metal_unload_texture(void *data, uintptr_t handle)
{
   if (!handle)
   {
      return;
   }
   Texture *t = (__bridge_transfer Texture *)(void *)handle;
   t = nil;
}

static void metal_set_video_mode(void *data,
                                 unsigned width, unsigned height,
                                 bool fullscreen)
{
   RARCH_LOG("[Metal]: set_video_mode res=%dx%d fullscreen=%s\n",
             width, height,
             fullscreen ? "YES" : "NO");
}

static float metal_get_refresh_rate(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   (void)md;
   
   return 0.0f;
}

static void metal_set_filtering(void *data, unsigned index, bool smooth)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   [md.frameView setFilteringIndex:index smooth:smooth];
}

static void metal_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   
   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;
      
      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;
      
      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;
      
      default:
         break;
   }
   
   video_driver_set_aspect_ratio_value(
      aspectratio_lut[aspect_ratio_idx].value);
   
   md.keepAspect = YES;
   [md setNeedsResize];
}

static void metal_apply_state_changes(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   [md setNeedsResize];
}

static void metal_set_texture_frame(void *data, const void *frame,
                                    bool rgb32, unsigned width, unsigned height,
                                    float alpha)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   settings_t *settings = config_get_ptr();
   
   [md.menu updateWidth:width
                 height:height
                 format:rgb32 ? RPixelFormatBGRA8Unorm : RPixelFormatBGRA4Unorm
                 filter:settings->bools.menu_linear_filter ? RTextureFilterLinear : RTextureFilterNearest];
   [md.menu updateFrame:frame];
   md.menu.alpha = alpha;
}

static void metal_set_texture_enable(void *data, bool state, bool full_screen)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;
   
   md.menu.enabled = state;
   //md.menu.fullScreen = full_screen;
}

static void metal_set_osd_msg(void *data,
                              video_frame_info_t *video_info,
                              const char *msg,
                              const void *params, void *font)
{
   font_driver_render_msg(video_info, font, msg, (const struct font_params *)params);
}

static void metal_show_mouse(void *data, bool state)
{
   [apple_platform setCursorVisible:state];
}

static struct video_shader *metal_get_current_shader(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NULL;
   
   return md.frameView.shader;
}


static uint32_t metal_get_flags(void *data)
{
   uint32_t flags = 0;
   
   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   
   return flags;
}

static const video_poke_interface_t metal_poke_interface = {
   .get_flags           = metal_get_flags,
   .load_texture        = metal_load_texture,
   .unload_texture      = metal_unload_texture,
   .set_video_mode      = metal_set_video_mode,
   .get_refresh_rate    = metal_get_refresh_rate,
   .set_filtering       = metal_set_filtering,
   .set_aspect_ratio    = metal_set_aspect_ratio,
   .apply_state_changes = metal_apply_state_changes,
   .set_texture_frame   = metal_set_texture_frame,
   .set_texture_enable  = metal_set_texture_enable,
   .set_osd_msg         = metal_set_osd_msg,
   .show_mouse          = metal_show_mouse,
   .get_current_shader  = metal_get_current_shader,
};

static void metal_get_poke_interface(void *data,
                                     const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &metal_poke_interface;
}

#ifdef HAVE_OVERLAY

static void metal_overlay_enable(void *data, bool state)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;
   md.overlay.enabled = state;
}

static bool metal_overlay_load(void *data,
                               const void *images, unsigned num_images)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NO;
   
   return [md.overlay loadImages:(const struct texture_image *)images count:num_images];
}

static void metal_overlay_tex_geom(void *data, unsigned index,
                                   float x, float y, float w, float h)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;
   
   [md.overlay updateTextureCoordsX:x y:y w:w h:h index:index];
}

static void metal_overlay_vertex_geom(void *data, unsigned index,
                                      float x, float y, float w, float h)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;
   
   [md.overlay updateVertexX:x y:y w:w h:h index:index];
}

static void metal_overlay_full_screen(void *data, bool enable)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;
   
   md.overlay.fullscreen = enable;
}

static void metal_overlay_set_alpha(void *data, unsigned index, float mod)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;
   
   [md.overlay updateAlpha:mod index:index];
}

static const video_overlay_interface_t metal_overlay_interface = {
   .enable        = metal_overlay_enable,
   .load          = metal_overlay_load,
   .tex_geom      = metal_overlay_tex_geom,
   .vertex_geom   = metal_overlay_vertex_geom,
   .full_screen   = metal_overlay_full_screen,
   .set_alpha     = metal_overlay_set_alpha,
};

static void metal_get_overlay_interface(void *data,
                                        const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &metal_overlay_interface;
}

#endif


video_driver_t video_metal = {
   .init                   = metal_init,
   .frame                  = metal_frame,
   .set_nonblock_state     = metal_set_nonblock_state,
   .alive                  = metal_alive,
   .has_windowed           = metal_has_windowed,
   .focus                  = metal_focus,
   .suppress_screensaver   = metal_suppress_screensaver,
   .set_shader             = metal_set_shader,
   .free                   = metal_free,
   .ident                  = "metal",
   .set_viewport           = metal_set_viewport,
   .set_rotation           = metal_set_rotation,
   .viewport_info          = metal_viewport_info,
   .read_viewport          = metal_read_viewport,
#ifdef HAVE_OVERLAY
   .overlay_interface      = metal_get_overlay_interface,
#endif
   .poke_interface         = metal_get_poke_interface,
};

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if TARGET_OS_IPHONE
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif
#if defined(HAVE_COCOA)
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <AppKit/NSScreen.h>
#include <AppKit/NSOpenGL.h>
#elif defined(HAVE_COCOATOUCH)
#include <GLKit/GLKit.h>
#ifdef HAVE_AVFOUNDATION
#import <AVFoundation/AVFoundation.h>
#endif
#endif

#include <retro_assert.h>
#include <compat/apple_compat.h>

#import "../../ui/drivers/cocoa/cocoa_common.h"
#include "../video_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#ifdef HAVE_VULKAN
#include "../common/vulkan_common.h"
#endif

#if defined(HAVE_COCOATOUCH)
#define GLContextClass EAGLContext
#define GLFrameworkID CFSTR("com.apple.opengles")
#define RAScreen UIScreen

#ifndef UIUserInterfaceIdiomTV
#define UIUserInterfaceIdiomTV 2
#endif

#ifndef UIUserInterfaceIdiomCarPlay
#define UIUserInterfaceIdiomCarPlay 3
#endif

@interface EAGLContext (OSXCompat) @end
@implementation EAGLContext (OSXCompat)
+ (void)clearCurrentContext { [EAGLContext setCurrentContext:nil];  }
- (void)makeCurrentContext  { [EAGLContext setCurrentContext:self]; }
@end

#else

@interface NSScreen (IOSCompat) @end
@implementation NSScreen (IOSCompat)
- (CGRect)bounds
{
   CGRect cgrect  = NSRectToCGRect(self.frame);
   return CGRectMake(0, 0, CGRectGetWidth(cgrect), CGRectGetHeight(cgrect));
}
- (float) scale  { return 1.0f; }
@end

#define GLContextClass NSOpenGLContext
#define GLFrameworkID CFSTR("com.apple.opengl")
#define RAScreen NSScreen
#endif

static enum gfx_ctx_api cocoagl_api = GFX_CTX_NONE;

typedef struct cocoa_ctx_data
{
   bool core_hw_context_enable;
#ifdef HAVE_VULKAN
   gfx_ctx_vulkan_data_t vk;
   unsigned swap_interval;
#endif
    unsigned width;
    unsigned height;
} cocoa_ctx_data_t;

#if defined(HAVE_COCOATOUCH)

static GLKView *g_view;
UIView *g_pause_indicator_view;
#endif

static GLContextClass* g_hw_ctx;
static GLContextClass* g_context;

static int g_fast_forward_skips;
static bool g_is_syncing = true;
static bool g_use_hw_ctx = false;

#if defined(HAVE_COCOA)
#include "../../ui/drivers/ui_cocoa.h"
static NSOpenGLPixelFormat* g_format;

void *glcontext_get_ptr(void)
{
   return (BRIDGE void *)g_context;
}
#endif

static unsigned g_minor = 0;
static unsigned g_major = 0;

/* forward declaration */
void *nsview_get_ptr(void);

#if defined(HAVE_COCOATOUCH)
static void glkitview_init_xibs(void)
{
   /* iOS Pause menu and lifecycle. */
   UINib *xib = (UINib*)[UINib nibWithNibName:BOXSTRING("PauseIndicatorView") bundle:nil];
   g_pause_indicator_view = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];
}
#endif

void *glkitview_init(void)
{
#if defined(HAVE_COCOATOUCH)
   glkitview_init_xibs();
   
   g_view = [GLKView new];
   g_view.multipleTouchEnabled = YES;
   g_view.enableSetNeedsDisplay = NO;
   [g_view addSubview:g_pause_indicator_view];
   
   return (BRIDGE void *)((GLKView*)g_view);
#else
   return nsview_get_ptr();
#endif
}

#if defined(HAVE_COCOATOUCH)
void cocoagl_bind_game_view_fbo(void)
{
#ifdef HAVE_AVFOUNDATION
   /*  Implicitly initializes your audio session */
   AVAudioSession *audio_session = [AVAudioSession sharedInstance];
   [audio_session setCategory:AVAudioSessionCategoryAmbient error:nil];
   [audio_session setActive:YES error:nil];
#endif
   if (g_context)
      [g_view bindDrawable];
}
#endif

static float get_from_selector(Class obj_class, id obj_id, SEL selector, CGFloat *ret)
{
   NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:
                               [obj_class instanceMethodSignatureForSelector:selector]];
   [invocation setSelector:selector];
   [invocation setTarget:obj_id];
   [invocation invoke];
   [invocation getReturnValue:ret];
   RELEASE(invocation);
   return *ret;
}

void *get_chosen_screen(void)
{
   settings_t *settings = config_get_ptr();
   NSArray *screens = [RAScreen screens];
   if (!screens || !settings)
      return NULL;
   
   if (settings->uints.video_monitor_index >= screens.count)
   {
      RARCH_WARN("video_monitor_index is greater than the number of connected monitors; using main screen instead.");
      return (BRIDGE void*)screens;
   }
   
   return ((BRIDGE void*)[screens objectAtIndex:settings->uints.video_monitor_index]);
}


float get_backing_scale_factor(void)
{
   static float
   backing_scale_def = 0.0f;
   RAScreen *screen     = NULL;
   
   (void)screen;
   
   if (backing_scale_def != 0.0f)
      return backing_scale_def;
   
   backing_scale_def = 1.0f;
#ifdef HAVE_COCOA
   screen = (BRIDGE RAScreen*)get_chosen_screen();
   
   if (screen)
   {
      SEL selector = NSSelectorFromString(BOXSTRING("backingScaleFactor"));
      if ([screen respondsToSelector:selector])
      {
         CGFloat ret;
         NSView *g_view        = apple_platform.renderView;
         //CocoaView *g_view     = (CocoaView*)nsview_get_ptr();
         backing_scale_def     = (float)get_from_selector
         ([[g_view window] class], [g_view window], selector, &ret);
      }
   }
#endif
   
   return backing_scale_def;
}

void cocoagl_gfx_ctx_update(void)
{
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_COCOA)
#if MAC_OS_X_VERSION_10_7
         CGLUpdateContext(g_hw_ctx.CGLContextObj);
         CGLUpdateContext(g_context.CGLContextObj);
#else
         [g_hw_ctx update];
         [g_context update];
#endif
#endif
         break;
      default:
         break;
   }
}

static void cocoagl_gfx_ctx_destroy(void *data)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   
   if (!cocoa_ctx)
      return;
   
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         [GLContextClass clearCurrentContext];
         
#if defined(HAVE_COCOA)
         [g_context clearDrawable];
         RELEASE(g_context);
         RELEASE(g_format);
         if (g_hw_ctx)
         {
            [g_hw_ctx clearDrawable];
         }
         RELEASE(g_hw_ctx);
#endif
         [GLContextClass clearCurrentContext];
         g_context = nil;
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_context_destroy(&cocoa_ctx->vk, cocoa_ctx->vk.vk_surface != VK_NULL_HANDLE);
         if (cocoa_ctx->vk.context.queue_lock) {
            slock_free(cocoa_ctx->vk.context.queue_lock);
         }
         memset(&cocoa_ctx->vk, 0, sizeof(cocoa_ctx->vk));
         
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
   
   free(cocoa_ctx);
}

static void *cocoagl_gfx_ctx_init(video_frame_info_t *video_info, void *video_driver)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)
   calloc(1, sizeof(cocoa_ctx_data_t));
   
   if (!cocoa_ctx)
      return NULL;
   
   switch (cocoagl_api)
   {
#if defined(HAVE_COCOATOUCH)
      case GFX_CTX_OPENGL_ES_API:
         [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL_ES];
         break;
#elif defined(HAVE_COCOA)
      case GFX_CTX_OPENGL_API:
         [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL];
         break;
#endif
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         [apple_platform setViewType:APPLE_VIEW_TYPE_VULKAN];
         if (!vulkan_context_init(&cocoa_ctx->vk, VULKAN_WSI_MVK_MACOS))
         {
            goto error;
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
   
   return cocoa_ctx;
   
error:
   free(cocoa_ctx);
   return NULL;
}

static enum gfx_ctx_api cocoagl_gfx_ctx_get_api(void *data)
{
   return cocoagl_api;
}

static bool cocoagl_gfx_ctx_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   switch (api)
   {
#if defined(HAVE_COCOATOUCH)
      case GFX_CTX_OPENGL_ES_API:
         break;
#elif defined(HAVE_COCOA)
      case GFX_CTX_OPENGL_API:
         break;
#ifdef HAVE_VULKAN
      case GFX_CTX_VULKAN_API:
         break;
#endif
#endif
      case GFX_CTX_NONE:
      default:
         return false;
   }
   
   cocoagl_api = api;
   g_minor     = minor;
   g_major     = major;
   
   return true;
}

static void cocoagl_gfx_ctx_swap_interval(void *data, unsigned interval)
{
#ifdef HAVE_VULKAN
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
#endif
   
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      {
#if defined(HAVE_COCOATOUCH) // < No way to disable Vsync on iOS?
         //   Just skip presents so fast forward still works.
         g_is_syncing = interval ? true : false;
         g_fast_forward_skips = interval ? 0 : 3;
#elif defined(HAVE_COCOA)
         GLint value = interval ? 1 : 0;
         [g_context setValues:&value forParameter:NSOpenGLCPSwapInterval];
#endif
         break;
      }
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (cocoa_ctx->swap_interval != interval)
         {
            cocoa_ctx->swap_interval = interval;
            if (cocoa_ctx->vk.swapchain)
            {
               cocoa_ctx->vk.need_new_swapchain = true;
            }
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
   
}

static void cocoagl_gfx_ctx_show_mouse(void *data, bool state)
{
   (void)data;
   
#ifdef HAVE_COCOA
   if (state)
      [NSCursor unhide];
   else
      [NSCursor hide];
#endif
}

static bool cocoagl_gfx_ctx_set_video_mode(void *data,
                                           video_frame_info_t *video_info,
                                           unsigned width, unsigned height, bool fullscreen)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   cocoa_ctx->width = width;
   cocoa_ctx->height = height;
   
#if defined(HAVE_COCOA)
   //CocoaView *g_view = (BRIDGE CocoaView *)nsview_get_ptr();
   NSView *g_view = apple_platform.renderView;
#endif
   
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      {
#if defined(HAVE_COCOA)
         if ([g_view respondsToSelector: @selector(setWantsBestResolutionOpenGLSurface:)])
            [g_view setWantsBestResolutionOpenGLSurface:YES];
         
         NSOpenGLPixelFormatAttribute attributes [] = {
            NSOpenGLPFAColorSize,
            24,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAllowOfflineRenderers,
            NSOpenGLPFADepthSize,
            (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
            0,                                /* profile */
            0,                                /* profile enum */
            (NSOpenGLPixelFormatAttribute)0
         };
         
#if MAC_OS_X_VERSION_10_7
         if (g_major == 3 && (g_minor >= 1 && g_minor <= 3))
         {
            attributes[6] = NSOpenGLPFAOpenGLProfile;
            attributes[7] = NSOpenGLProfileVersion3_2Core;
         }
#endif
         
#if MAC_OS_X_VERSION_10_10
         if (g_major == 4 && g_minor == 1)
         {
            attributes[6] = NSOpenGLPFAOpenGLProfile;
            attributes[7] = NSOpenGLProfileVersion4_1Core;
         }
#endif
         
         g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
         
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
         if (g_format == nil)
         {
            /* NSOpenGLFPAAllowOfflineRenderers is
             not supported on this OS version. */
            attributes[3] = (NSOpenGLPixelFormatAttribute)0;
            g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
         }
#endif
         
         if (g_use_hw_ctx)
            g_hw_ctx  = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
         g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:(g_use_hw_ctx) ? g_hw_ctx : nil];
         [g_context setView:g_view];
#else
         if (g_use_hw_ctx)
            g_hw_ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
         g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
         g_view.context = g_context;
#endif
         
         [g_context makeCurrentContext];
         break;
      }
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         RARCH_LOG("[macOS]: Native window size: %u x %u.\n", cocoa_ctx->width, cocoa_ctx->height);
         if (!vulkan_surface_create(&cocoa_ctx->vk, VULKAN_WSI_MVK_MACOS, NULL,
                                    (BRIDGE void *)g_view, cocoa_ctx->width, cocoa_ctx->height,
                                    cocoa_ctx->swap_interval))
         {
            RARCH_ERR("[macOS]: Failed to create surface.\n");
            return false;
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
   
#if defined(HAVE_COCOA)
   static bool has_went_fullscreen = false;
   /* TODO: Screen mode support. */
   
   if (fullscreen)
   {
      if (!has_went_fullscreen)
      {
         [g_view enterFullScreenMode:(BRIDGE NSScreen *)get_chosen_screen() withOptions:nil];
         cocoagl_gfx_ctx_show_mouse(data, false);
      }
   }
   else
   {
      if (has_went_fullscreen)
      {
         [g_view exitFullScreenModeWithOptions:nil];
         [[g_view window] makeFirstResponder:g_view];
         cocoagl_gfx_ctx_show_mouse(data, true);
      }
      
      [[g_view window] setContentSize:NSMakeSize(width, height)];
   }
   
   has_went_fullscreen = fullscreen;
#endif
   
   /* TODO: Maybe iOS users should be able to show/hide the status bar here? */
   
   return true;
}

float cocoagl_gfx_ctx_get_native_scale(void)
{
   static CGFloat ret = 0.0f;
   SEL selector     = NSSelectorFromString(BOXSTRING("nativeScale"));
   RAScreen *screen = (BRIDGE RAScreen*)get_chosen_screen();
   
   if (ret != 0.0f)
      return ret;
   if (!screen)
      return 0.0f;
   
   if ([screen respondsToSelector:selector])
      return (float)get_from_selector([screen class], screen, selector, &ret);
   
   ret          = 1.0f;
   selector     = NSSelectorFromString(BOXSTRING("scale"));
   if ([screen respondsToSelector:selector])
      ret       = screen.scale;
   return ret;
}

static void cocoagl_gfx_ctx_get_video_size(void *data, unsigned* width, unsigned* height)
{
   float screenscale               = cocoagl_gfx_ctx_get_native_scale();
#if defined(HAVE_COCOA)
   CGRect size;
   GLsizei backingPixelWidth, backingPixelHeight;
   NSView *g_view                  = apple_platform.renderView;
   //CocoaView *g_view               = (CocoaView*)nsview_get_ptr();
   CGRect cgrect                   = NSRectToCGRect([g_view frame]);
#if MAC_OS_X_VERSION_10_7
   SEL selector                    = NSSelectorFromString(BOXSTRING("convertRectToBacking:"));
   if ([g_view respondsToSelector:selector])
      cgrect                       = NSRectToCGRect([g_view convertRectToBacking:[g_view bounds]]);
#endif
   backingPixelWidth               = CGRectGetWidth(cgrect);
   backingPixelHeight              = CGRectGetHeight(cgrect);
   size                            = CGRectMake(0, 0, backingPixelWidth, backingPixelHeight);
#else
   CGRect size                     = g_view.bounds;
#endif
   *width                          = CGRectGetWidth(size)  * screenscale;
   *height                         = CGRectGetHeight(size) * screenscale;
}

#if defined(HAVE_COCOA)
static void cocoagl_gfx_ctx_update_title(void *data, void *data2)
{
   ui_window_cocoa_t view;
   const ui_window_t *window      = ui_companion_driver_get_window_ptr();
   
   //view.data = (CocoaView*)nsview_get_ptr();
   view.data = (BRIDGE void *)apple_platform.renderView;
   
   if (window)
   {
      char title[128];
      
      title[0] = '\0';
      
      video_driver_get_window_title(title, sizeof(title));
      
      if (title[0])
         window->set_title(&view, title);
   }
}
#endif

static bool cocoagl_gfx_ctx_get_metrics(void *data, enum display_metric_types type,
                                        float *value)
{
   RAScreen *screen              = (BRIDGE RAScreen*)get_chosen_screen();
#if defined(HAVE_COCOA)
   NSDictionary *description     = [screen deviceDescription];
   NSSize  display_pixel_size    = [[description objectForKey:NSDeviceSize] sizeValue];
   CGSize  display_physical_size = CGDisplayScreenSize(
                                                       [[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);
   
   float   display_width         = display_pixel_size.width;
   float   display_height        = display_pixel_size.height;
   float   physical_width        = display_physical_size.width;
   float   physical_height       = display_physical_size.height;
   float   scale                 = get_backing_scale_factor();
   float   dpi                   = (display_width/ physical_width) * 25.4f * scale;
#elif defined(HAVE_COCOATOUCH)
   float   scale                 = cocoagl_gfx_ctx_get_native_scale();
   CGRect  screen_rect           = [screen bounds];
   float   display_height        = screen_rect.size.height;
   float   physical_width        = screen_rect.size.width  * scale;
   float   physical_height       = screen_rect.size.height * scale;
   float   dpi                   = 160                     * scale;
   unsigned idiom_type           = UI_USER_INTERFACE_IDIOM();
   
   switch (idiom_type)
   {
      case -1: /* UIUserInterfaceIdiomUnspecified */
         /* TODO */
         break;
      case UIUserInterfaceIdiomPad:
         dpi = 132 * scale;
         break;
      case UIUserInterfaceIdiomPhone:
         dpi = 163 * scale;
         break;
      case UIUserInterfaceIdiomTV:
      case UIUserInterfaceIdiomCarPlay:
         /* TODO */
         break;
   }
#endif
   
   (void)display_height;
   
   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = physical_width;
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = physical_height;
         break;
      case DISPLAY_METRIC_DPI:
         *value = dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }
   
   return true;
}

static bool cocoagl_gfx_ctx_has_focus(void *data)
{
   (void)data;
#if defined(HAVE_COCOATOUCH)
   return ([[UIApplication sharedApplication] applicationState] == UIApplicationStateActive);
#else
   return [NSApp isActive];
#endif
}

static bool cocoagl_gfx_ctx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   
   return false;
}

#if !defined(HAVE_COCOATOUCH)
static bool cocoagl_gfx_ctx_has_windowed(void *data)
{
   return true;
}
#endif

#ifdef HAVE_VULKAN
static void *cocoagl_gfx_ctx_get_context_data(void *data)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   return &cocoa_ctx->vk.context;
}
#endif


static void cocoagl_gfx_ctx_swap_buffers(void *data, void *data2)
{
#ifdef HAVE_VULKAN
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
#endif
   
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         if (!(--g_fast_forward_skips < 0))
            return;
         
#if defined(HAVE_COCOA)
         [g_context flushBuffer];
         [g_hw_ctx flushBuffer];
#elif defined(HAVE_COCOATOUCH)
         if (g_view)
            [g_view display];
#endif
         
         g_fast_forward_skips = g_is_syncing ? 0 : 3;
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_present(&cocoa_ctx->vk, cocoa_ctx->vk.context.current_swapchain_index);
         vulkan_acquire_next_image(&cocoa_ctx->vk);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static gfx_ctx_proc_t cocoagl_gfx_ctx_get_proc_address(const char *symbol_name)
{
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(GLFrameworkID),
                                                                  (BRIDGE CFStringRef)BOXSTRING(symbol_name)
                                                                  );
      case GFX_CTX_NONE:
      default:
         break;
   }
   
   return NULL;
}

static bool cocoagl_gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
#ifdef HAVE_VULKAN
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
#endif
   
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         cocoa_ctx->width  = width;
         cocoa_ctx->height = height;
         
         if (vulkan_create_swapchain(&cocoa_ctx->vk, width, height, cocoa_ctx->swap_interval))
         {
            cocoa_ctx->vk.context.invalid_swapchain = true;
            if (cocoa_ctx->vk.created_new_swapchain)
               vulkan_acquire_next_image(&cocoa_ctx->vk);
         }
         else
         {
            RARCH_ERR("[macOS/Vulkan]: Failed to update swapchain.\n");
            return false;
         }
         
         cocoa_ctx->vk.need_new_swapchain = false;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
   
   return true;
}


static void cocoagl_gfx_ctx_check_window(void *data, bool *quit,
                                         bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   unsigned new_width, new_height;
#ifdef HAVE_VULKAN
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
#endif
   
   *quit = false;
   
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         *resize = cocoa_ctx->vk.need_new_swapchain;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   cocoagl_gfx_ctx_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static void cocoagl_gfx_ctx_input_driver(void *data,
                                         const char *name,
                                         const input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static void cocoagl_gfx_ctx_bind_hw_render(void *data, bool enable)
{
   (void)data;
   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
         g_use_hw_ctx = enable;
         
         if (enable)
            [g_hw_ctx makeCurrentContext];
         else
            [g_context makeCurrentContext];
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static uint32_t cocoagl_gfx_ctx_get_flags(void *data)
{
   uint32_t flags                 = 0;
   cocoa_ctx_data_t    *cocoa_ctx = (cocoa_ctx_data_t*)data;
   
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);
   
   if (cocoa_ctx->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);
   
   return flags;
}

static void cocoagl_gfx_ctx_set_flags(void *data, uint32_t flags)
{
   (void)flags;
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      cocoa_ctx->core_hw_context_enable = true;
}

const gfx_ctx_driver_t gfx_ctx_cocoagl = {
   .init                 = cocoagl_gfx_ctx_init,
   .destroy              = cocoagl_gfx_ctx_destroy,
   .get_api              = cocoagl_gfx_ctx_get_api,
   .bind_api             = cocoagl_gfx_ctx_bind_api,
   .swap_interval        = cocoagl_gfx_ctx_swap_interval,
   .set_video_mode       = cocoagl_gfx_ctx_set_video_mode,
   .get_video_size       = cocoagl_gfx_ctx_get_video_size,
   .get_metrics          = cocoagl_gfx_ctx_get_metrics,
#if defined(HAVE_COCOA)
   .update_window_title  = cocoagl_gfx_ctx_update_title,
#endif
   .check_window         = cocoagl_gfx_ctx_check_window,
   .set_resize           = cocoagl_gfx_ctx_set_resize,
   .has_focus            = cocoagl_gfx_ctx_has_focus,
   .suppress_screensaver = cocoagl_gfx_ctx_suppress_screensaver,
#if !defined(HAVE_COCOATOUCH)
   .has_windowed         = cocoagl_gfx_ctx_has_windowed,
#endif
   .swap_buffers         = cocoagl_gfx_ctx_swap_buffers,
   .input_driver         = cocoagl_gfx_ctx_input_driver,
   .get_proc_address     = cocoagl_gfx_ctx_get_proc_address,
   .ident                = "macOS",
   .get_flags            = cocoagl_gfx_ctx_get_flags,
   .set_flags            = cocoagl_gfx_ctx_set_flags,
   .bind_hw_render       = cocoagl_gfx_ctx_bind_hw_render,
#if defined(HAVE_VULKAN)
   .get_context_data     = cocoagl_gfx_ctx_get_context_data,
#else
   .get_context_data     = NULL,
#endif
};

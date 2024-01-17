/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
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

#ifndef __GL_COMMON_H
#define __GL_COMMON_H

#include <boolean.h>
#include <string.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>
#include <gfx/scaler/scaler.h>
#include <formats/image.h>

#include "../../verbosity.h"
#include "../font_driver.h"
#include "../video_coord_array.h"
#include "../video_driver.h"
#include <glsym/glsym.h>

RETRO_BEGIN_DECLS

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#else
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
#endif

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#if defined(HAVE_OPENGLES2)
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#else
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#endif
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#elif (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_EXT
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_SCE
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_OES
#else
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#endif

#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_OES
#else
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE
#endif

#if defined(HAVE_PSGL)
#define glGenerateMipmap glGenerateMipmapOES
#endif

#if defined(__APPLE__) || defined(HAVE_PSGL)
#define GL_RGBA32F GL_RGBA32F_ARB
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_INTERNAL_FORMAT32 GL_ARGB_SCE
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB5 /* TODO: Verify if this is really 565 or just 555. */
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_RGB5
#elif defined(HAVE_OPENGLES)
/* Imgtec/SGX headers have this missing. */
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifndef GL_BGRA8_EXT
#define GL_BGRA8_EXT 0x93A1
#endif
#ifdef IOS
/* Stupid Apple */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA
#else
#define RARCH_GL_INTERNAL_FORMAT32 GL_BGRA_EXT
#endif
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL_TEXTURE_TYPE16 GL_RGB
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_5_6_5
#else
/* On desktop, we always use 32-bit. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGBA8
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_INT_8_8_8_8_REV

/* GL_RGB565 internal format isn't in desktop GL
 * until 4.1 core (ARB_ES2_compatibility).
 * Check for this. */
#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif
#define RARCH_GL_INTERNAL_FORMAT16_565 GL_RGB565
#define RARCH_GL_TEXTURE_TYPE16_565 GL_RGB
#define RARCH_GL_FORMAT16_565 GL_UNSIGNED_SHORT_5_6_5
#endif

#if defined(HAVE_OPENGLES2) /* TODO: Figure out exactly what. */
#define NO_GL_CLAMP_TO_BORDER
#endif

#if defined(HAVE_OPENGLES)
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#endif

#ifndef GL_SRGB_ALPHA_EXT
#define GL_SRGB_ALPHA_EXT 0x8C42
#endif
#endif

typedef struct gl gl_t;
typedef struct gl_renderchain_driver gl_renderchain_driver_t;

struct gl_renderchain_driver
{
   void (*set_coords)(void *handle_data,
         void *chain_data,
         void *shader_data, const struct video_coords *coords);
   void (*set_mvp)(void *data,
         void *chain_data,
         void *shader_data,
         const void *mat_data);
   void (*init_texture_reference)(
         gl_t *gl, void *chain_data, unsigned i,
         unsigned internal_fmt, unsigned texture_fmt,
         unsigned texture_type);
   void (*fence_iterate)(void *data, void *chain_data,
         unsigned hard_sync_frames);
   void (*fence_free)(void *data, void *chain_data);
   void (*readback)(gl_t *gl,
         void *chain_data,
         unsigned alignment,
         unsigned fmt, unsigned type,
         void *src);
   void (*init_pbo)(unsigned size, const void *data);
   void (*bind_pbo)(unsigned idx);
   void (*unbind_pbo)(void *data, void *chain_data);
   void (*copy_frame)(
      gl_t *gl,
      void *chain_data,
      video_frame_info_t *video_info,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch);
   void (*restore_default_state)(gl_t *gl, void *chain_data);
   void (*new_vao)(void *data, void *chain_data);
   void (*free_vao)(void *data, void *chain_data);
   void (*bind_vao)(void *data, void *chain_data);
   void (*unbind_vao)(void *data, void *chain_data);
   void (*disable_client_arrays)(void *data, void *chain_data);
   void (*ff_vertex)(const void *data);
   void (*ff_matrix)(const void *data);
   void (*bind_backbuffer)(void *data, void *chain_data);
   void (*deinit_fbo)(gl_t *gl, void *chain_data);
   bool (*read_viewport)(
         gl_t *gl, void *chain_data, uint8_t *buffer, bool is_idle);
   void (*bind_prev_texture)(
         gl_t *gl,
         void *chain_data,
         const struct video_tex_info *tex_info);
   void (*chain_free)(void *data, void *chain_data);
   void *(*chain_new)(void);
   void (*init)(gl_t *gl, void *chain_data,
         unsigned fbo_width, unsigned fbo_height);
   bool (*init_hw_render)(gl_t *gl, void *chain_data,
         unsigned width, unsigned height);
   void (*free)(gl_t *gl, void *chain_data);
   void (*deinit_hw_render)(gl_t *gl, void *chain_data);
   void (*start_render)(gl_t *gl, void *chain_data,
         video_frame_info_t *video_info);
   void (*check_fbo_dimensions)(gl_t *gl, void *chain_data);
   void (*recompute_pass_sizes)(gl_t *gl,
         void *chain_data,
         unsigned width, unsigned height,
         unsigned vp_width, unsigned vp_height);
   void (*renderchain_render)(gl_t *gl,
         void *chain_data,
         video_frame_info_t *video_info,
         uint64_t frame_count,
         const struct video_tex_info *tex_info,
         const struct video_tex_info *feedback_info);
   void (*resolve_extensions)(
         gl_t *gl,
         void *chain_data,
         const char *context_ident,
         const video_info_t *video);
   const char *ident;
};

struct gl
{
   GLenum internal_fmt;
   GLenum texture_type; /* RGB565 or ARGB */
   GLenum texture_fmt;
   GLenum wrap_mode;

   bool vsync;
   bool tex_mipmap;
   bool fbo_inited;
   bool fbo_feedback_enable;
   bool hw_render_fbo_init;
   bool has_fbo;
   bool hw_render_use;
   bool core_context_in_use;

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   bool support_unpack_row_length;
   bool have_es2_compat;
   bool have_full_npot_support;
   bool have_mipmap;

   bool overlay_enable;
   bool overlay_full_screen;
   bool menu_texture_enable;
   bool menu_texture_full_screen;
   bool have_sync;
   bool pbo_readback_valid[4];
   bool pbo_readback_enable;

   int version_major;
   int version_minor;

   GLuint tex_mag_filter;
   GLuint tex_min_filter;
   GLuint fbo_feedback;
   GLuint fbo_feedback_texture;
   GLuint pbo;
   GLuint *overlay_tex;
   GLuint menu_texture;
   GLuint pbo_readback[4];
   GLuint texture[GFX_MAX_TEXTURES];
   GLuint hw_render_fbo[GFX_MAX_TEXTURES];

   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   unsigned fbo_feedback_pass;
   unsigned rotation;
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned tex_w;
   unsigned tex_h;
   unsigned base_size; /* 2 or 4 */
   unsigned overlays;
   unsigned pbo_readback_index;
   unsigned last_width[GFX_MAX_TEXTURES];
   unsigned last_height[GFX_MAX_TEXTURES];

   float menu_texture_alpha;

   void *empty_buf;
   void *conv_buffer;
   void *readback_buffer_screenshot;
   const float *vertex_ptr;
   const float *white_color_ptr;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;

   struct video_tex_info tex_info;
   struct scaler_ctx pbo_readback_scaler;
   struct video_viewport vp;
   math_matrix_4x4 mvp, mvp_no_rot;
   struct video_coords coords;
   struct scaler_ctx scaler;
   video_info_t video_info;
   struct video_tex_info prev_info[GFX_MAX_TEXTURES];
   struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];

   const gl_renderchain_driver_t *renderchain_driver;
   void *renderchain_data;
};

static INLINE void gl_bind_texture(GLuint id, GLint wrap_mode, GLint mag_filter,
      GLint min_filter)
{
   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
}

static INLINE unsigned gl_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
#ifndef HAVE_OPENGLES
      case RARCH_WRAP_BORDER: /* GL_CLAMP_TO_BORDER: Available since GL 1.3 */
         return GL_CLAMP_TO_BORDER;
#else
      case RARCH_WRAP_BORDER:
#endif
      case RARCH_WRAP_EDGE:
         return GL_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GL_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GL_MIRRORED_REPEAT;
      default:
	 break;
   }

   return 0;
}

bool gl_query_core_context_in_use(void);

void gl_load_texture_image(GLenum target,
      GLint level,
      GLint internalFormat,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLenum format,
      GLenum type,
      const GLvoid * data);

void gl_load_texture_data(
      uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size);

static INLINE GLenum gl_min_filter_to_mag(GLenum type)
{
   switch (type)
   {
      case GL_LINEAR_MIPMAP_LINEAR:
         return GL_LINEAR;
      case GL_NEAREST_MIPMAP_NEAREST:
         return GL_NEAREST;
      default:
         break;
   }

   return type;
}

RETRO_END_DECLS

#endif

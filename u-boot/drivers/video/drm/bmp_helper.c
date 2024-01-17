/*
 * (C) Copyright 2008-2017 Fuzhou Rockchip Electronics Co., Ltd
 * Author: Mark Yao <mark.yao@rock-chips.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <command.h>
#include <log.h>
#include <config.h>
#include <common.h>
#include <malloc.h>
#include <asm/unaligned.h>
#include <bmp_layout.h>

#define BMP_RLE8_ESCAPE		0
#define BMP_RLE8_EOL		0
#define BMP_RLE8_EOBMP		1
#define BMP_RLE8_DELTA		2

static void draw_unencoded_bitmap(uint16_t **dst, uint8_t *bmap, uint16_t *cmap,
				  uint32_t cnt)
{
	while (cnt > 0) {
		*(*dst)++ = cmap[*bmap++];
		cnt--;
	}
}

static void draw_encoded_bitmap(uint16_t **dst, uint16_t c, uint32_t cnt)
{
	uint16_t *fb = *dst;
	int cnt_8copy = cnt >> 3;

	cnt -= cnt_8copy << 3;
	while (cnt_8copy > 0) {
		*fb++ = c;
		*fb++ = c;
		*fb++ = c;
		*fb++ = c;
		*fb++ = c;
		*fb++ = c;
		*fb++ = c;
		*fb++ = c;
		cnt_8copy--;
	}
	while (cnt > 0) {
		*fb++ = c;
		cnt--;
	}
	*dst = fb;
}

static void decode_rle8_bitmap(void *psrc, void *pdst, uint16_t *cmap,
			       int width, int height, int bits, int x_off,
			       int y_off, bool flip)
{
	uint32_t cnt, runlen;
	int x = 0, y = 0;
	int decode = 1;
	int linesize = width * 2;
	uint8_t *bmap = psrc;
	uint8_t *dst = pdst;

	if (flip) {
		y = height - 1;
		dst = pdst + y * linesize;
	}

	while (decode) {
		if (bmap[0] == BMP_RLE8_ESCAPE) {
			switch (bmap[1]) {
			case BMP_RLE8_EOL:
				/* end of line */
				bmap += 2;
				x = 0;
				if (flip) {
					y--;
					dst -= linesize * 2;
				} else {
					y++;
				}
				break;
			case BMP_RLE8_EOBMP:
				/* end of bitmap */
				decode = 0;
				break;
			case BMP_RLE8_DELTA:
				/* delta run */
				x += bmap[2];
				if (flip) {
					y -= bmap[3];
					dst -= bmap[3] * linesize;
					dst += bmap[2] * 2;
				} else {
					y += bmap[3];
					dst += bmap[3] * linesize;
					dst += bmap[2] * 2;
				}
				bmap += 4;
				break;
			default:
				/* unencoded run */
				runlen = bmap[1];
				bmap += 2;
				if (y >= height || x >= width) {
					decode = 0;
					break;
				}
				if (x + runlen > width)
					cnt = width - x;
				else
					cnt = runlen;
				draw_unencoded_bitmap((uint16_t **)&dst, bmap,
						      cmap, cnt);
				x += runlen;
				bmap += runlen;
				if (runlen & 1)
					bmap++;
			}
		} else {
			/* encoded run */
			if (y < height) {
				runlen = bmap[0];
				if (x < width) {
					/* aggregate the same code */
					while (bmap[0] == 0xff &&
					       bmap[2] != BMP_RLE8_ESCAPE &&
					       bmap[1] == bmap[3]) {
						runlen += bmap[2];
						bmap += 2;
					}
					if (x + runlen > width)
						cnt = width - x;
					else
						cnt = runlen;
					draw_encoded_bitmap((uint16_t **)&dst,
							    cmap[bmap[1]], cnt);
				}
				x += runlen;
			}
			bmap += 2;
		}
	}
}

static void dump_bmp_dib_head(void *bmp_addr)
{
	struct bmp_image *bmp = bmp_addr;

	debug("########## BMP DIB_HEAD ##########\n"
	      "Width  : %u\n"
	      "Height : %u\n"
	      "Bpp    : %u\n"
	      "Compression method : %u\n"
	      "Image size : %u\n"
	      "Colors in palette  : %u\n"
	      "##################################\n",
		bmp->header.width,
		bmp->header.height,
		bmp->header.bit_count,
		bmp->header.compression,
		bmp->header.image_size,
		bmp->header.colors_used);
}

int bmpdecoder(void *bmp_addr, void *pdst, int dst_bpp, bool flip)
{
	int i, j;
	int stride, padded_width, bpp, width, height;
	struct bmp_image *bmp = bmp_addr;
	uint8_t *src = bmp_addr;
	uint8_t *dst = pdst;
	uint16_t *cmap;
	uint8_t *cmap_base;

	if (!bmp || !(bmp->header.signature[0] == 'B' &&
	    bmp->header.signature[1] == 'M')) {
		printf("Error: Invalid bmp file.\n");
		return -1;
	}
	dump_bmp_dib_head(bmp);
	width = get_unaligned_le32(&bmp->header.width);
	height = get_unaligned_le32(&bmp->header.height);
	bpp = get_unaligned_le16(&bmp->header.bit_count);
	padded_width = width & 0x3 ? (width & ~0x3) + 4 : width;

	if (height < 0) {
		height = 0 - height;
		flip = false;
	}
	cmap_base = src + sizeof(bmp->header);
	src = bmp_addr + get_unaligned_le32(&bmp->header.data_offset);

	switch (bpp) {
	case 8:
		if (dst_bpp != 16) {
			printf("Error: Target pixel's bpp is not 16bit.\n");

			return -1;
		}
		cmap = malloc(sizeof(cmap) * 256);

		/* Set color map */
		for (i = 0; i < 256; i++) {
			ushort colreg = ((cmap_base[0] << 8) & 0xf800) |
					((cmap_base[1] << 3) & 0x07e0) |
					((cmap_base[2] >> 3) & 0x001f) ;
			cmap_base += 4;
			cmap[i] = colreg;
		}
		/*
		 * only support convert 8bit bmap file to RGB565.
		 */
		if (get_unaligned_le32(&bmp->header.compression)) {
			decode_rle8_bitmap(src, dst, cmap, width, height,
					   bpp, 0, 0, flip);
		} else {
			stride = width * 2;

			if (flip)
				dst += stride * (height - 1);

			for (i = 0; i < height; ++i) {
				for (j = 0; j < width; j++) {
					*(uint16_t *)dst = cmap[*(src++)];
					dst += sizeof(uint16_t) / sizeof(*dst);
				}
				src += (padded_width - width);
				if (flip)
					dst -= stride * 2;
			}
		}
		free(cmap);
		break;
	case 16:
		if (get_unaligned_le32(&bmp->header.compression)) {
			printf("Error: Failed to decompression bmp file.\n");

			return -1;
		}
		stride = ALIGN(width * bpp / 8, 4);
		if (flip)
			src += stride * (height - 1);
		for (i = 0; i < height; i++) {
			for (j = 0; j < width; j++) {
				ushort color = (src[1] << 8) | src[0];

				color = (((color & 0x7c00) << 1) |
					((color & 0x03e0) << 1) |
					(color & 0x001f));
				*(uint16_t *)dst = color;
				src += 2;
				dst += 2;
			}
			src += (padded_width - width);
			if (flip)
				src -= stride * 2;
		}
		break;
	case 24:
		if (get_unaligned_le32(&bmp->header.compression)) {
			printf("Error: Failed to decompression bmp file.\n");

			return -1;
		}
		stride = ALIGN(width * 3, 4);
		if (flip)
			src += stride * (height - 1);

		for (i = 0; i < height; i++) {
			memcpy(dst, src, 3 * width);
			dst += stride;
			src += stride;
			if (flip)
				src -= stride * 2;
		}
		break;
	case 32:
		if (get_unaligned_le32(&bmp->header.compression)) {
			printf("Error: Failed to decompression bmp file.\n");

			return -1;
		}
		stride = ALIGN(width * 4, 4);
		if (flip)
			src += stride * (height - 1);

		for (i = 0; i < height; i++) {
			memcpy(dst, src, 4 * width);
			dst += stride;
			src += stride;
			if (flip)
				src -= stride * 2;
		}
		break;
	default:
		printf("Error: Can't decode this bmp file with bit=%d\n", bpp);
		return -1;
	}

	return 0;
}

/*
 * vic.c - A line-based VIC-I emulation (under construction).
 *
 * Written by
 *  Ettore Perazzoli (ettore@comm2000.it)
 *
 * 16/24bpp support added by
 *  Steven Tieu (stieu@physics.ubc.ca)
 *  Teemu Rantanen (tvr@cs.hut.fi)
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

/* This is incomplete and buggy.  */

#ifndef VIC20
#define VIC20
#endif

#define _VIC_C

#include "vice.h"

#include <stdlib.h>
#include <stdio.h>

#include "vic.h"
#include "vmachine.h"
#include "interrupt.h"
#include "raster.h"
#include "sid.h"
#include "mem.h"

/* #define VIC_REGISTERS_DEBUG */

/* ------------------------------------------------------------------------- */

/* VIC resources.  */

/* Flag: Do we use double size?  */
static int double_size_enabled;

/* Flag: Do we enable the video cache?  */
static int video_cache_enabled;

/* Flag: Do we copy lines in double size mode?  */
static int double_scan_enabled;

static int set_video_cache_enabled(resource_value_t v)
{
    video_cache_enabled = (int) v;
    return 0;
}

static int set_double_size_enabled(resource_value_t v)
{
    double_size_enabled = (int) v;
    video_resize();
    return 0;
}

static int set_double_scan_enabled(resource_value_t v)
{
    double_scan_enabled = (int) v;
    video_resize();
    return 0;
}

static resource_t resources[] = {
    { "VideoCache", RES_INTEGER, (resource_value_t) 1,
      (resource_value_t *) &video_cache_enabled, set_video_cache_enabled },
    { "DoubleSize", RES_INTEGER, (resource_value_t) 0,
      (resource_value_t *) &double_size_enabled, set_double_size_enabled },
    { "DoubleScan", RES_INTEGER, (resource_value_t) 0,
      (resource_value_t *) &double_scan_enabled, set_double_scan_enabled },
    { NULL }
};

int vic_init_resources(void)
{
    return resources_register(resources);
}

/* ------------------------------------------------------------------------- */

static void set_memory_ptrs(void);
static void init_drawing_table(void);
static int fill_cache(struct line_cache *l, int *xs, int *xe, int r);
static void draw_line(void);
static void draw_line_2x(void);
static void draw_line_cached(struct line_cache *l, int xs, int xe);
static void draw_line_cached_2x(struct line_cache *l, int xs, int xe);
static void draw_reverse_line(void);
static void draw_reverse_line_2x(void);
static void draw_reverse_line_cached(struct line_cache *l, int xs, int xe);
static void draw_reverse_line_cached_2x(struct line_cache *l, int xs, int xe);

/* Define the position of the raster beam precisely. */
#define RASTER_Y    	((int)(clk / CYCLES_PER_LINE) % SCREEN_HEIGHT)
#define RASTER_CYCLE	((int)(clk % CYCLES_PER_LINE))

static BYTE vic[64];
static BYTE auxiliary_color;
static BYTE *colormem;
static BYTE *screenmem;
static BYTE *chargen_ptr = chargen_rom;

/* On MS-DOS, do not duplicate pixels.  Otherwise, we would always need at
   least 466 horizontal pixels to contain the whole screen. */
#ifndef __MSDOS__
#define DUPLICATE_PIXELS
#endif

#ifdef DUPLICATE_PIXELS
typedef PIXEL2 VIC_PIXEL;
#define VIC_PIXEL(n)	PIXEL2(n)
typedef PIXEL4 VIC_PIXEL2;
#define VIC_PIXEL2(n)	PIXEL4(n)
#define VIC_PIXEL_WIDTH	2
#else
typedef PIXEL VIC_PIXEL;
#define VIC_PIXEL(n)	PIXEL(n)
typedef PIXEL2 VIC_PIXEL2;
#define VIC_PIXEL2(n)	PIXEL2(n)
#define VIC_PIXEL_WIDTH	1
#endif

#include "raster.c"

/* -------------------------------------------------------------------------- */

/* Initialization. */
canvas_t vic_init(void)
{
    int width, height;

    /* FIXME: the maximum pixel width should be 4 instead of 6, but we need
       some extra space for clipping long lines...  This should be done in a
       cleaner way.  */
    init_raster(1, 6, 2);

    width = VIC_SCREEN_WIDTH;
    height = (VIC_SCREEN_LAST_DISPLAYED_LINE
	      - VIC_SCREEN_FIRST_DISPLAYED_LINE + 1);

    video_resize();

    if (open_output_window(VIC_WINDOW_TITLE,
			   width, height, color_defs,
			   (canvas_redraw_t)vic_exposure_handler)) {
	fprintf(stderr,
		"fatal error: cannot open window for the VIC emulation.\n");
	return NULL;
    }

    video_mode = VIC_STANDARD_MODE;
    set_memory_ptrs();
    refresh_all();
    init_drawing_table();

    return canvas;
}

/* This hook is called whenever the screen parameters (eg. window size) are
   changed. */
void video_resize(void)
{
    static int old_size = 0;

    if (double_size_enabled) {
	pixel_width = 2 * VIC_PIXEL_WIDTH;
	pixel_height = 2;
	video_modes[VIC_STANDARD_MODE].fill_cache = fill_cache;
	video_modes[VIC_STANDARD_MODE].draw_line_cached = draw_line_cached_2x;
	video_modes[VIC_STANDARD_MODE].draw_line = draw_line_2x;
	video_modes[VIC_REVERSE_MODE].fill_cache = fill_cache;
	video_modes[VIC_REVERSE_MODE].draw_line_cached = draw_reverse_line_cached_2x;
	video_modes[VIC_REVERSE_MODE].draw_line = draw_reverse_line_2x;
	if (old_size == 1) {
	    window_width *= 2;
	    window_height *= 2;
	}
    } else {
	pixel_width = VIC_PIXEL_WIDTH;
	pixel_height = 1;
	video_modes[VIC_STANDARD_MODE].fill_cache = fill_cache;
	video_modes[VIC_STANDARD_MODE].draw_line_cached = draw_line_cached;
	video_modes[VIC_STANDARD_MODE].draw_line = draw_line;
	video_modes[VIC_REVERSE_MODE].fill_cache = fill_cache;
	video_modes[VIC_REVERSE_MODE].draw_line_cached = draw_reverse_line_cached;
	video_modes[VIC_REVERSE_MODE].draw_line = draw_reverse_line;
	if (old_size == 2) {
	    window_width /= 2;
	    window_height /= 2;
	}
    }
    old_size = double_size_enabled ? 2 : 1;

    if (canvas) {
	resize(window_width, window_height);
	frame_buffer_clear(&frame_buffer, PIXEL(0));
	force_repaint();
    }
}

void video_free(void)
{
    frame_buffer_free(&frame_buffer);
}

/* -------------------------------------------------------------------------- */

/* VIC access functions. */

void REGPARM2 store_vic(ADDRESS addr, BYTE value)
{
    addr &= 0xf;
    vic[addr] = value;

#ifdef VIC_REGISTERS_DEBUG
    printf("VIC: write $90%02X, value = $%02X\n", addr, value);
#endif

    switch (addr) {
      case 0:			/* $9000  Screen X Location. */
	value &= 0x7f;
	if (value > 8)
	    value = 8;
	if (value < 1)
	    value = 1;
	display_xstart = value * 4;
	display_xstop = display_xstart + text_cols * 8;
	if (display_xstop >= VIC_SCREEN_WIDTH)
	    display_xstop = VIC_SCREEN_WIDTH - 1;
#ifdef VIC_REGISTERS_DEBUG
	printf("\tscreen X location: $%02X\n", value);
#endif
	return;
      case 1:			/* $9001  Screen Y Location. */
	display_ystart = value * 2;
	display_ystop = display_ystart + text_lines * char_height;
#ifdef VIC_REGISTERS_DEBUG
	printf("\tscreen Y location: $%02X\n", value);
	return;
#endif
	return;

      case 2:			/* $9002  Columns Displayed. */
	colormem = ram + ((value & 0x80) ? 0x9600 : 0x9400);
	text_cols = value & 0x7f;
	if (text_cols > VIC_SCREEN_MAX_TEXTCOLS)
	    text_cols = VIC_SCREEN_MAX_TEXTCOLS;
	display_xstop = display_xstart + text_cols * 8;
	if (display_xstop >= VIC_SCREEN_WIDTH)
	    display_xstop = VIC_SCREEN_WIDTH - 1;
	set_memory_ptrs();
        memptr_inc = text_cols;
#ifdef VIC_REGISTERS_DEBUG
	printf("\tcolor RAM at $%04X\n", colormem - ram);
	printf("\tcolumns displayed: %d\n", text_cols);
#endif
	break;

      case 3:			/* $9003  Rows Displayed, Character size . */
	text_lines = (value & 0x7e) >> 1;
	if (text_lines > VIC_SCREEN_MAX_TEXTLINES)
	    text_lines = VIC_SCREEN_MAX_TEXTLINES;
	char_height = (value & 0x1) ? 16 : 8;
	display_ystop = display_ystart + text_lines * char_height;
#ifdef VIC_REGISTERS_DEBUG
	printf("\trows displayed: %d\n", text_lines);
	printf("\tcharacter height: %d\n", char_height);
#endif
	set_memory_ptrs();
	return;

      case 4:			/* $9004  Raster line count -- read only. */
#ifdef VIC_REGISTERS_DEBUG
	printf("\t(raster line counter, read-only)\n");
#endif
	return;

      case 5:			/* $9005  Video and char matrix base address. */
	set_memory_ptrs();
	return;

      case 6:			/* $9006. */
      case 7:			/* $9007  Light Pen X,Y. */
#ifdef VIC_REGISTERS_DEBUG
	printf("\t(light pen register, read-only)\n");
#endif
	return;

      case 8:			/* $9008. */
      case 9:			/* $9009  Paddle X,Y. */
#ifdef VIC_REGISTERS_DEBUG
	printf("\t(paddle, read-only)\n");
#endif
	return;

      case 10:			/* $900A  Bass Enable and Frequency. */
      case 11:			/* $900B  Alto Enable and Frequency. */
      case 12:			/* $900C  Soprano Enable and Frequency. */
      case 13:			/* $900D  Noise Enable and Frequency. */
#ifdef VIC_REGISTERS_DEBUG
	printf("\t(sound register, not implemented)\n");
#endif
#ifdef SOUND
	store_vic20sid(addr, value);
#endif
	return;

      case 14:			/* $900E  Auxiliary Colour, Master Volume. */
	auxiliary_color = value >> 4;
#ifdef VIC_REGISTERS_DEBUG
	printf("\tauxiliary color set to $%02X\n", auxiliary_color);
	printf("\t(master volume not implemented)\n");
#endif
#ifdef SOUND
	store_vic20sid(addr, value);
#endif
	return;

      case 15:			/* $900F  Screen and Border Colors,
				   Reverse Video. */
	border_color = value & 0x7;
	background_color = value >> 4;
	video_mode = (value & 8) ? VIC_STANDARD_MODE : VIC_REVERSE_MODE;
#ifdef VIC_REGISTERS_DEBUG
	printf("\tborder color: $%02X\n", border_color);
	printf("\tbackground color: $%02X\n", background_color);
#endif
	return;
    }
}

BYTE REGPARM1 read_vic(ADDRESS addr)
{
    addr &= 0xf;

    switch (addr) {
      case 3:
	return ((RASTER_Y & 1) << 7) | (vic[3] & ~0x80);
      case 4:
	return RASTER_Y >> 1;
      default:
	return vic[addr];
    }
}

/* -------------------------------------------------------------------------- */

/* Set the memory pointers according to the values stored in the VIC
   registers. */
static void set_memory_ptrs(void)
{
    int tmp;
    ADDRESS charaddr;

    tmp = vic[0x5] & 0xf;
    charaddr = (tmp & 0x8) ? 0x0000 : 0x8000;
    charaddr += (tmp & 0x7) * 0x400;
    if (charaddr >= 0x8000 && charaddr < 0x9000) {
	chargen_ptr = chargen_rom + (charaddr & 0xfff);
#ifdef VIC_REGISTERS_DEBUG
	printf("\tcharacter memory at $%04X (character ROM + $%04X)\n",
	       charaddr, charaddr & 0xfff);
#endif
    } else {
	chargen_ptr = ram + charaddr;
#ifdef VIC_REGISTERS_DEBUG
	printf("\tcharacter memory at $%04X\n", charaddr);
#endif
    }
    colormem = ram + 0x9400 + (vic[0x2] & 0x80 ? 0x200 : 0x0);
    screenmem = ram + (((vic[0x2] & 0x80) << 2) | ((vic[0x5] & 0x70) << 6));
#ifdef VIC_REGISTERS_DEBUG
    printf("\tcolor memory at $%04X\n", colormem - ram);
    printf("\tscreen memory at $%04X\n", screenmem - ram);
#endif
}

/* -------------------------------------------------------------------------- */

/* Here comes the part that actually repaints each raster line.  This table is
   used to speed up the drawing. */
static WORD dwg_table[256][256][8];	/* [byte][color][position] */

static void init_drawing_table(void)
{
    int byte, color, pos;

    for (byte = 0; byte < 0x100; byte++) {
	for (color = 0; color < 0x100; color++) {
	    if (color & 0x8) {	/* Multicolor mode. */
		for (pos = 0; pos < 8; pos += 2) {
		    dwg_table[byte][color][pos]
			= dwg_table[byte][color][pos + 1]
			= (byte >> (6 - pos)) & 0x3;
		}
	    } else {		/* Standard mode. */
		for (pos = 0; pos < 8; pos++) {
		    dwg_table[byte][color][pos] = ((byte >> (7 - pos))
						   & 0x1) * 2;
		}
	    }
	}
    }
}

/* ------------------------------------------------------------------------- */

/* Notice: The screen origin X register has a 4-pixel granularity, so our
   write accesses are always aligned. */

int int_rasterdraw(long offset)
{
    maincpu_set_alarm(A_RASTERDRAW, CYCLES_PER_LINE - offset);
    emulate_line();

    if (rasterline == 0) {
	/* Turn border on. */
	blank_enabled = 1;
    }

    return 0;
}

static int fill_cache(struct line_cache *l, int *xs, int *xe, int r)
{
    if (l->bgdata[0] != background_color || l->colordata2[0] != auxiliary_color
	|| l->numcols != text_cols) {
	l->bgdata[0] = background_color;
	l->colordata2[0] = auxiliary_color;
	l->numcols = text_cols;
	*xs = 0;
	*xe = text_cols;
	r = 1;
    }
    r = _fill_cache(l->colordata1, colormem + memptr, VIC_SCREEN_TEXTCOLS, 1,
		    xs, xe, r);
    r = _fill_cache_text(l->fgdata, screenmem + memptr, chargen_ptr,
			 VIC_SCREEN_TEXTCOLS, ycounter, xs, xe, r);
    return r;
}

#define PUT_PIXEL(p, d, c, b, x) \
      *((VIC_PIXEL *)(p) + (x)) = (c)[dwg_table[(d)][(b)][(x)]]

#define DRAW_LINE(p, xs, xe, reverse)					     \
  do {									     \
      static VIC_PIXEL c[4];			    			     \
      int b, i;								     \
      BYTE d;								     \
      PIXEL *pp = (PIXEL *)(p) + (xs) * 8 * VIC_PIXEL_WIDTH;		     \
									     \
      c[0] = VIC_PIXEL(background_color);				     \
      c[1] = VIC_PIXEL(border_color);					     \
      c[3] = VIC_PIXEL(auxiliary_color);				     \
      for (i = (xs); i <= (xe); i++, pp += 8 * VIC_PIXEL_WIDTH) {	     \
	  b = (colormem + memptr)[i];					     \
	  c[2] = VIC_PIXEL(b & 0x7);					     \
          if (reverse)							     \
	      d = ~(GET_CHAR_DATA (chargen_ptr, (screenmem + memptr)[i],     \
				   ycounter));				     \
	  else								     \
	      d = GET_CHAR_DATA (chargen_ptr, (screenmem + memptr)[i],	     \
				 ycounter);				     \
	  PUT_PIXEL(pp, d, c, b, 0); PUT_PIXEL(pp, d, c, b, 1);		     \
	  PUT_PIXEL(pp, d, c, b, 2); PUT_PIXEL(pp, d, c, b, 3);	             \
	  PUT_PIXEL(pp, d, c, b, 4); PUT_PIXEL(pp, d, c, b, 5);		     \
	  PUT_PIXEL(pp, d, c, b, 6); PUT_PIXEL(pp, d, c, b, 7);		     \
      }									     \
  } while (0)

static void draw_line(void)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH;

    DRAW_LINE(p, 0, text_cols - 1, 0);
}

static void draw_reverse_line(void)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH;

    DRAW_LINE(p, 0, text_cols - 1, 1);
}

static void draw_line_cached(struct line_cache *l, int xs, int xe)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH;

    DRAW_LINE(p, xs, xe, 0);
}

static void draw_reverse_line_cached(struct line_cache *l, int xs, int xe)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH;

    DRAW_LINE(p, xs, xe, 1);
}

#define PUT_PIXEL_2x(p, d, c, b, x) \
      *((VIC_PIXEL2 *)(p) + (x)) = (c)[dwg_table[(d)][(b)][(x)]]

#define DRAW_LINE_2x(p, xs, xe, reverse)				     \
  do {									     \
      static VIC_PIXEL2 c[4];						     \
      BYTE d, b;							     \
      int i;								     \
      PIXEL *pp = (PIXEL *)(p) + (xs) * 16 * VIC_PIXEL_WIDTH;		     \
									     \
      c[0] = VIC_PIXEL2(background_color);			             \
      c[1] = VIC_PIXEL2(border_color);					     \
      c[3] = VIC_PIXEL2(auxiliary_color);			             \
      for (i = (xs); i <= (xe); i++, pp += 16 * VIC_PIXEL_WIDTH) {	     \
	  b = (colormem + memptr)[i];					     \
	  c[2] = VIC_PIXEL2(b & 0x7);					     \
          if (reverse)							     \
	      d = ~(GET_CHAR_DATA (chargen_ptr, (screenmem + memptr)[i],     \
				   ycounter));				     \
          else								     \
	      d = GET_CHAR_DATA (chargen_ptr, (screenmem + memptr)[i],	     \
				 ycounter);				     \
	  PUT_PIXEL_2x(pp, d, c, b, 0); PUT_PIXEL_2x(pp, d, c, b, 1);	     \
	  PUT_PIXEL_2x(pp, d, c, b, 2); PUT_PIXEL_2x(pp, d, c, b, 3);	     \
	  PUT_PIXEL_2x(pp, d, c, b, 4); PUT_PIXEL_2x(pp, d, c, b, 5);	     \
	  PUT_PIXEL_2x(pp, d, c, b, 6); PUT_PIXEL_2x(pp, d, c, b, 7);	     \
      }									     \
  } while (0)

static void draw_line_2x(void)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH * 2;

    DRAW_LINE_2x(p, 0, text_cols - 1, 0);
}

static void draw_reverse_line_2x(void)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH * 2;

    DRAW_LINE_2x(p, 0, text_cols - 1, 1);
}

static void draw_line_cached_2x(struct line_cache *l, int xs, int xe)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH * 2;

    DRAW_LINE_2x(p, xs, xe, 0);
}

static void draw_reverse_line_cached_2x(struct line_cache *l, int xs, int xe)
{
    PIXEL *p = frame_buffer_ptr + display_xstart * VIC_PIXEL_WIDTH * 2;

    DRAW_LINE_2x(p, xs, xe, 1);
}

/* -------------------------------------------------------------------------- */

void vic_exposure_handler(unsigned int width, unsigned int height)
{
    resize(width, height);
    force_repaint();
}

void vic_prevent_clk_overflow(void)
{
    oldclk -= PREVENT_CLK_OVERFLOW_SUB;
}

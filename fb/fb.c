/*
 * Copyright (C) 2006 Jakub Vana
 * Copyright (C) 2006 Ondrej Palkovsky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ddi.h>
#include <sysinfo.h>
#include <align.h>
#include <as.h>
#include <ipc/fb.h>
#include <ipc/ipc.h>
#include <ipc/ns.h>
#include <ipc/services.h>
#include <kernel/errno.h>
#include <async.h>

#include "font-8x16.h"
#include "helenos.xbm"
#include "fb.h"
#include "main.h"
#include "../console/screenbuffer.h"

#define DEFAULT_BGCOLOR                0x000080
#define DEFAULT_FGCOLOR                0xffff00

/***************************************************************/
/* Pixel specific fuctions */

typedef void (*putpixel_fn_t)(unsigned int x, unsigned int y, int color);
typedef int (*getpixel_fn_t)(unsigned int x, unsigned int y);

struct {
	__u8 *fbaddress ;

	unsigned int xres ;
	unsigned int yres ;
	unsigned int scanline ;
	unsigned int pixelbytes ;

	putpixel_fn_t putpixel;
	getpixel_fn_t getpixel;
} screen;

typedef struct {
	int initialized;
	unsigned int x, y;
	unsigned int width, height;

	/* Text support in window */
	unsigned int rows, cols;
	/* Style for text printing */
	style_t style;
	/* Auto-cursor position */
	int cursor_active, cur_col, cur_row;
	int cursor_shown;
} viewport_t;

#define MAX_VIEWPORTS 128
static viewport_t viewports[128];

/* Allow only 1 connection */
static int client_connected = 0;

#define RED(x, bits)	((x >> (16 + 8 - bits)) & ((1 << bits) - 1))
#define GREEN(x, bits)	((x >> (8 + 8 - bits)) & ((1 << bits) - 1))
#define BLUE(x, bits)	((x >> (8 - bits)) & ((1 << bits) - 1))

#define COL_WIDTH	8
#define ROW_BYTES	(screen.scanline * FONT_SCANLINES)

#define POINTPOS(x, y)	((y) * screen.scanline + (x) * screen.pixelbytes)

/** Put pixel - 24-bit depth, 1 free byte */
static void putpixel_4byte(unsigned int x, unsigned int y, int color)
{
	*((__u32 *)(screen.fbaddress + POINTPOS(x, y))) = color;
}

/** Return pixel color - 24-bit depth, 1 free byte */
static int getpixel_4byte(unsigned int x, unsigned int y)
{
	return *((__u32 *)(screen.fbaddress + POINTPOS(x, y))) & 0xffffff;
}

/** Put pixel - 24-bit depth */
static void putpixel_3byte(unsigned int x, unsigned int y, int color)
{
	unsigned int startbyte = POINTPOS(x, y);

#if (defined(BIG_ENDIAN) || defined(FB_BIG_ENDIAN))
	screen.fbaddress[startbyte] = RED(color, 8);
	screen.fbaddress[startbyte + 1] = GREEN(color, 8);
	screen.fbaddress[startbyte + 2] = BLUE(color, 8);
#else
	screen.fbaddress[startbyte + 2] = RED(color, 8);
	screen.fbaddress[startbyte + 1] = GREEN(color, 8);
	screen.fbaddress[startbyte + 0] = BLUE(color, 8);
#endif

}

/** Return pixel color - 24-bit depth */
static int getpixel_3byte(unsigned int x, unsigned int y)
{
	unsigned int startbyte = POINTPOS(x, y);



#if (defined(BIG_ENDIAN) || defined(FB_BIG_ENDIAN))
	return screen.fbaddress[startbyte] << 16 | screen.fbaddress[startbyte + 1] << 8 | screen.fbaddress[startbyte + 2];
#else
	return screen.fbaddress[startbyte + 2] << 16 | screen.fbaddress[startbyte + 1] << 8 | screen.fbaddress[startbyte + 0];
#endif
								

}

/** Put pixel - 16-bit depth (5:6:5) */
static void putpixel_2byte(unsigned int x, unsigned int y, int color)
{
	/* 5-bit, 6-bits, 5-bits */ 
	*((__u16 *)(screen.fbaddress + POINTPOS(x, y))) = RED(color, 5) << 11 | GREEN(color, 6) << 5 | BLUE(color, 5);
}

/** Return pixel color - 16-bit depth (5:6:5) */
static int getpixel_2byte(unsigned int x, unsigned int y)
{
	int color = *((__u16 *)(screen.fbaddress + POINTPOS(x, y)));
	return (((color >> 11) & 0x1f) << (16 + 3)) | (((color >> 5) & 0x3f) << (8 + 2)) | ((color & 0x1f) << 3);
}

/** Put pixel - 8-bit depth (3:2:3) */
static void putpixel_1byte(unsigned int x, unsigned int y, int color)
{
	screen.fbaddress[POINTPOS(x, y)] = RED(color, 3) << 5 | GREEN(color, 2) << 3 | BLUE(color, 3);
}

/** Return pixel color - 8-bit depth (3:2:3) */
static int getpixel_1byte(unsigned int x, unsigned int y)
{
	int color = screen.fbaddress[POINTPOS(x, y)];
	return (((color >> 5) & 0x7) << (16 + 5)) | (((color >> 3) & 0x3) << (8 + 6)) | ((color & 0x7) << 5);
}

/** Put pixel into viewport 
 *
 * @param vp Viewport identification
 * @param x X coord relative to viewport
 * @param y Y coord relative to viewport
 * @param color RGB color 
 */
static void putpixel(int vp, unsigned int x, unsigned int y, int color)
{
	screen.putpixel(viewports[vp].x + x, viewports[vp].y + y, color);
}
/** Get pixel from viewport */
static int getpixel(int vp, unsigned int x, unsigned int y)
{
	return screen.getpixel(viewports[vp].x + x, viewports[vp].y + y);
}

/** Fill line with color BGCOLOR */
static void clear_line(int vp, unsigned int y)
{
	unsigned int x;
	for (x = 0; x < viewports[vp].width; x++)
		putpixel(vp, x, y, viewports[vp].style.bg_color);
}

/** Fill viewport with background color */
static void clear_port(int vp)
{
	unsigned int y;

	clear_line(vp, 0);
	for (y = viewports[vp].y+1; y < viewports[vp].y+viewports[vp].height; y++) {
		memcpy(&screen.fbaddress[POINTPOS(viewports[vp].x,y)],
		       &screen.fbaddress[POINTPOS(viewports[vp].x,viewports[vp].y)],
		       screen.pixelbytes * viewports[vp].width);
	}	
}

/** Scroll port up/down 
 *
 * @param vp Viewport to scroll
 * @param rows Positive number - scroll up, negative - scroll down
 */
static void scroll_port(int vp, int rows)
{
	int y;
	int startline;
	int endline;
	
	if (rows > 0) {
		for (y=viewports[vp].y; y < viewports[vp].y+viewports[vp].height - rows*FONT_SCANLINES; y++)
			memcpy(&screen.fbaddress[POINTPOS(viewports[vp].x,y)],
			       &screen.fbaddress[POINTPOS(viewports[vp].x,y + rows*FONT_SCANLINES)],
			       screen.pixelbytes * viewports[vp].width);
		/* Clear last row */
		startline = viewports[vp].y+FONT_SCANLINES*(viewports[vp].rows-1);
		endline = viewports[vp].y + viewports[vp].height;
		clear_line(vp, startline);
		for (y=startline+1;y < endline; y++)
			memcpy(&screen.fbaddress[POINTPOS(viewports[vp].x,y)],
			       &screen.fbaddress[POINTPOS(viewports[vp].x,startline)],
			       screen.pixelbytes * viewports[vp].width);
			      
	} else if (rows < 0) {
		rows = -rows;
		for (y=viewports[vp].y + viewports[vp].height-1; y >= viewports[vp].y + rows*FONT_SCANLINES; y--)
			memcpy(&screen.fbaddress[POINTPOS(viewports[vp].x,y)],
				&screen.fbaddress[POINTPOS(viewports[vp].x,y - rows*FONT_SCANLINES)],
				screen.pixelbytes * viewports[vp].width);
		/* Clear first row */
		clear_line(0, viewports[vp].style.bg_color);
		for (y=1;y < rows*FONT_SCANLINES; y++)
			memcpy(&screen.fbaddress[POINTPOS(viewports[vp].x,viewports[vp].y+y)],
			       &screen.fbaddress[POINTPOS(viewports[vp].x,viewports[vp].y)],
			       screen.pixelbytes * viewports[vp].width);
	}
}

static void invert_pixel(int vp,unsigned int x, unsigned int y)
{
	putpixel(vp, x, y, ~getpixel(vp, x, y));
}


/***************************************************************/
/* Character-console functions */

/** Draw character at given position */
static void draw_glyph(int vp,__u8 glyph, unsigned int sx, unsigned int sy, style_t style)
{
	int i;
	unsigned int y;
	unsigned int glline;

	for (y = 0; y < FONT_SCANLINES; y++) {
		glline = fb_font[glyph * FONT_SCANLINES + y];
		for (i = 0; i < 8; i++) {
			if (glline & (1 << (7 - i)))
				putpixel(vp, sx + i, sy + y, style.fg_color);
			else
				putpixel(vp, sx + i, sy + y, style.bg_color);
		}
	}
}

/** Invert character at given position */
static void invert_char(int vp,unsigned int row, unsigned int col)
{
	unsigned int x;
	unsigned int y;

	for (x = 0; x < COL_WIDTH; x++)
		for (y = 0; y < FONT_SCANLINES; y++)
			invert_pixel(vp, col * COL_WIDTH + x, row * FONT_SCANLINES + y);
}

static void draw_logo(int vp,unsigned int startx, unsigned int starty)
{
	unsigned int x;
	unsigned int y;
	unsigned int byte;
	unsigned int rowbytes;

	rowbytes = (helenos_width - 1) / 8 + 1;

	for (y = 0; y < helenos_height; y++)
		for (x = 0; x < helenos_width; x++) {
			byte = helenos_bits[rowbytes * y + x / 8];
			byte >>= x % 8;
			if (byte & 1)
				putpixel(vp ,startx + x, starty + y, viewports[vp].style.fg_color);
		}
}

/***************************************************************/
/* Stdout specific functions */


/** Create new viewport
 *
 * @return New viewport number
 */
static int viewport_create(unsigned int x, unsigned int y,unsigned int width, 
			   unsigned int height)
{
	int i;

for (i=0; i < MAX_VIEWPORTS; i++) {
		if (!viewports[i].initialized)
			break;
	}
	if (i == MAX_VIEWPORTS)
		return ELIMIT;

	if (width ==0 || height == 0 ||
	    x+width > screen.xres || y+height > screen.yres)
		return EINVAL;
	if (width < FONT_SCANLINES || height < COL_WIDTH)
		return EINVAL;

	viewports[i].x = x;
	viewports[i].y = y;
	viewports[i].width = width;
	viewports[i].height = height;
	
	viewports[i].rows = height / FONT_SCANLINES;
	viewports[i].cols = width / COL_WIDTH;

	viewports[i].style.bg_color = DEFAULT_BGCOLOR;
	viewports[i].style.fg_color = DEFAULT_FGCOLOR;
	
	viewports[i].cur_col = 0;
	viewports[i].cur_row = 0;
	viewports[i].cursor_active = 0;

	viewports[i].initialized = 1;

	return i;
}


/** Initialize framebuffer as a chardev output device
 *
 * @param addr Address of theframebuffer
 * @param x    Screen width in pixels
 * @param y    Screen height in pixels
 * @param bpp  Bits per pixel (8, 16, 24, 32)
 * @param scan Bytes per one scanline
 *
 */
static void screen_init(void *addr, unsigned int xres, unsigned int yres, unsigned int bpp, unsigned int scan)
{
	switch (bpp) {
		case 8:
			screen.putpixel = putpixel_1byte;
			screen.getpixel = getpixel_1byte;
			screen.pixelbytes = 1;
			break;
		case 16:
			screen.putpixel = putpixel_2byte;
			screen.getpixel = getpixel_2byte;
			screen.pixelbytes = 2;
			break;
		case 24:
			screen.putpixel = putpixel_3byte;
			screen.getpixel = getpixel_3byte;
			screen.pixelbytes = 3;
			break;
		case 32:
			screen.putpixel = putpixel_4byte;
			screen.getpixel = getpixel_4byte;
			screen.pixelbytes = 4;
			break;
	}

		
	screen.fbaddress = (unsigned char *) addr;
	screen.xres = xres;
	screen.yres = yres;
	screen.scanline = scan;
	
	/* Create first viewport */
	viewport_create(0,0,xres,yres);

	clear_port(0);
}

static void cursor_hide(int vp)
{
	viewport_t *vport = &viewports[vp];

	if (vport->cursor_active && vport->cursor_shown) {
		invert_char(vp, vport->cur_row, vport->cur_col);
		vport->cursor_shown = 0;
	}
}

static void cursor_print(int vp)
{
	viewport_t *vport = &viewports[vp];

	/* Do not check for cursor_shown */
	if (vport->cursor_active) {
		invert_char(vp, vport->cur_row, vport->cur_col);
		vport->cursor_shown = 1;
	}
}

static void cursor_blink(int vp)
{
	viewport_t *vport = &viewports[vp];

	if (vport->cursor_shown)
		cursor_hide(vp);
	else
		cursor_print(vp);
}

/** Draw character at given position relative to viewport 
 * 
 * @param vp Viewport identification
 * @param c Character to print
 * @param row Screen position relative to viewport
 * @param col Screen position relative to viewport
 */
static void draw_char(int vp, char c, unsigned int row, unsigned int col, style_t style)
{
	viewport_t *vport = &viewports[vp];

	/* Optimize - do not hide cursor if we are going to overwrite it */
	if (vport->cursor_active && vport->cursor_shown && 
	    (vport->cur_col != col || vport->cur_row != row))
		invert_char(vp, vport->cur_row, vport->cur_col);
	
	draw_glyph(vp, c, col * COL_WIDTH, row * FONT_SCANLINES, style);

	vport->cur_col = col;
	vport->cur_row = row;

	vport->cur_col++;
	if (vport->cur_col>= vport->cols) {
		vport->cur_col = 0;
		vport->cur_row++;
		if (vport->cur_row >= vport->rows)
			vport->cur_row--;
	}
	cursor_print(vp);
}

static void draw_text_data(int vp, keyfield_t *data)
{
	viewport_t *vport = &viewports[vp];
	int i;
	char c;

	clear_port(vp);
	for (i=0; i < vport->cols * vport->rows; i++) {
		if (data[i].character == ' ' && style_same(data[i].style,vport->style))
			continue;
		draw_char(vp, data[i].character, i/vport->rows, i % vport->cols,
			  data[i].style);
	}
	cursor_print(vp);
}


/** Function for handling connections to FB
 *
 */
static void fb_client_connection(ipc_callid_t iid, ipc_call_t *icall)
{
	ipc_callid_t callid;
	ipc_call_t call;
	int retval;
	int i;
	unsigned int row,col;
	char c;
	keyfield_t *interbuffer = NULL;
	size_t intersize = 0;

	int vp = 0;
	viewport_t *vport = &viewports[0];

	if (client_connected) {
		ipc_answer_fast(iid, ELIMIT, 0,0);
		return;
	}
	client_connected = 1;
	ipc_answer_fast(iid, 0, 0, 0); /* Accept connection */

	while (1) {
		callid = async_get_call_timeout(&call,250000);
		if (!callid) {
			cursor_blink(vp);
			continue;
		}
 		switch (IPC_GET_METHOD(call)) {
		case IPC_M_PHONE_HUNGUP:
			client_connected = 0;
			/* cleanup other viewports */
			for (i=1; i < MAX_VIEWPORTS; i++)
				vport->initialized = 0;
			ipc_answer_fast(callid,0,0,0);
			return; /* Exit thread */
		case IPC_M_AS_AREA_SEND:
			/* We accept one area for data interchange */
			intersize = IPC_GET_ARG2(call);
			receive_comm_area(callid,&call,(void **)&interbuffer,
					  sizeof(*interbuffer)*viewports[0].cols*viewports[0].rows);
			continue;

		case FB_DRAW_TEXT_DATA:
			if (!interbuffer) {
				retval = EINVAL;
				break;
			}
			if (intersize < vport->cols*vport->rows*sizeof(*interbuffer)) {
				retval = EINVAL;
				break;
			}
			draw_text_data(vp, interbuffer);
			retval = 0;
			break;
		case FB_PUTCHAR:
			c = IPC_GET_ARG1(call);
			row = IPC_GET_ARG2(call);
			col = IPC_GET_ARG3(call);
			if (row >= vport->rows || col >= vport->cols) {
				retval = EINVAL;
				break;
			}
			ipc_answer_fast(callid,0,0,0);

			draw_char(vp, c, row, col, vport->style);
			continue; /* msg already answered */
		case FB_CLEAR:
			clear_port(vp);
			cursor_print(vp);
			retval = 0;
			break;
 		case FB_CURSOR_GOTO:
			row = IPC_GET_ARG1(call);
			col = IPC_GET_ARG2(call);
			if (row >= vport->rows || col >= vport->cols) {
				retval = EINVAL;
				break;
			}
 			retval = 0;
			cursor_hide(vp);
			vport->cur_col = col;
			vport->cur_row = row;
			cursor_print(vp);
 			break;
		case FB_CURSOR_VISIBILITY:
			cursor_hide(vp);
			vport->cursor_active = IPC_GET_ARG1(call);
			cursor_print(vp);
			retval = 0;
			break;
		case FB_GET_CSIZE:
			ipc_answer_fast(callid, 0, vport->rows, vport->cols);
			continue;
		case FB_SCROLL:
			i = IPC_GET_ARG1(call);
			if (i > vport->rows || i < (- (int)vport->rows)) {
				retval = EINVAL;
				break;
			}
			cursor_hide(vp);
			scroll_port(vp, i);
			cursor_print(vp);
			retval = 0;
			break;
		case FB_VIEWPORT_SWITCH:
			i = IPC_GET_ARG1(call);
			if (i < 0 || i >= MAX_VIEWPORTS) {
				retval = EINVAL;
				break;
			}
			if (! viewports[i].initialized ) {
				retval = EADDRNOTAVAIL;
				break;
			}
			cursor_hide(vp);
			vp = i;
			vport = &viewports[vp];
			cursor_print(vp);
			retval = 0;
			break;
		case FB_VIEWPORT_CREATE:
			retval = viewport_create(IPC_GET_ARG1(call) >> 16,
						 IPC_GET_ARG1(call) & 0xffff,
						 IPC_GET_ARG2(call) >> 16,
						 IPC_GET_ARG2(call) & 0xffff);
			break;
		case FB_VIEWPORT_DELETE:
			i = IPC_GET_ARG1(call);
			if (i < 0 || i >= MAX_VIEWPORTS) {
				retval = EINVAL;
				break;
			}
			if (! viewports[i].initialized ) {
				retval = EADDRNOTAVAIL;
				break;
			}
			viewports[i].initialized = 0;
			retval = 0;
			break;
		case FB_SET_STYLE:
			vport->style.fg_color = IPC_GET_ARG1(call);
			vport->style.bg_color = IPC_GET_ARG2(call);
			retval = 0;
			break;
		case FB_GET_RESOLUTION:
			ipc_answer_fast(callid, 0, screen.xres,screen.yres);
			continue;
		default:
			retval = ENOENT;
		}
		ipc_answer_fast(callid,retval,0,0);
	}
}

/** Initialization of framebuffer */
int fb_init(void)
{
	void *fb_ph_addr;
	unsigned int fb_width;
	unsigned int fb_height;
	unsigned int fb_bpp;
	unsigned int fb_scanline;
	void *fb_addr;
	size_t asz;

	async_set_client_connection(fb_client_connection);

	fb_ph_addr=(void *)sysinfo_value("fb.address.physical");
	fb_width=sysinfo_value("fb.width");
	fb_height=sysinfo_value("fb.height");
	fb_bpp=sysinfo_value("fb.bpp");
	fb_scanline=sysinfo_value("fb.scanline");

	asz = fb_scanline*fb_height;
	fb_addr = as_get_mappable_page(asz);
	
	map_physmem(fb_ph_addr, fb_addr, ALIGN_UP(asz,PAGE_SIZE) >>PAGE_WIDTH,
		    AS_AREA_READ | AS_AREA_WRITE | AS_AREA_CACHEABLE);
	
	screen_init(fb_addr, fb_width, fb_height, fb_bpp, fb_scanline);

	return 0;
}


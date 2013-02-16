/*
 tuxpaint.c
 
 Tux Paint - A simple drawing program for children.
 
 Copyright (c) 2002-2008 by Bill Kendrick and others; see AUTHORS.txt
 bill@newbreedsoftware.com
 http://www.tuxpaint.org/
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 (See COPYING.txt)
 
 June 14, 2002 - June 13, 2009
 $Id: tuxpaint.c,v 1.677 2009/06/18 20:51:58 wkendrick Exp $
 */


/* (Note: VER_VERSION and VER_DATE are now handled by Makefile) */


/* Color depth for Tux Paint to run in, and store canvases in: */

#if defined(NOKIA_770)
# define VIDEO_BPP 16
#endif

#if defined(OLPC_XO)
# define VIDEO_BPP 15
#endif

#ifndef VIDEO_BPP
/*# define VIDEO_BPP 15 */ /* saves memory */
/*# define VIDEO_BPP 16 */ /* causes discoloration */
/*# define VIDEO_BPP 24 */ /* compromise */
# define VIDEO_BPP 32 /* might be fastest, if conversion funcs removed */
#endif


/* #define CORNER_SHAPES */    /* need major work! */


/* Method for printing images: */

#define PRINTMETHOD_PS		/* Direct to PostScript */
/*#define PRINTMETHOD_PNM_PS*/       /* Output PNM, assuming it gets printed */
/*#define PRINTMETHOD_PNG_PNM_PS*/   /* Output PNG, assuming it gets printed */


#define MAX_PATH 256


/* Compile-time options: */

#include "debug.h"


#ifdef NOKIA_770
# define LOW_QUALITY_THUMBNAILS
# define LOW_QUALITY_STAMP_OUTLINE
# define NO_PROMPT_SHADOWS
# define USE_HWSURFACE
#else
/* #define DEBUG_MALLOC */
/* #define LOW_QUALITY_THUMBNAILS */
/* #define LOW_QUALITY_COLOR_SELECTOR */
/* #define LOW_QUALITY_STAMP_OUTLINE */
/* #define NO_PROMPT_SHADOWS */
/* #define USE_HWSURFACE */
#endif

/* Disable fancy cursors in fullscreen mode, to avoid SDL bug: */
/* (This bug is still around, as of SDL 1.2.9, October 2005) */
/* (Is it still in SDL 1.2.11 in May 2007, though!? -bjk) */
/* #define LARGE_CURSOR_FULLSCREEN_BUG */

/* control the color selector */
#define COLORSEL_DISABLE 0	/* disable and draw the (greyed out) colors */
#define COLORSEL_ENABLE  1	/* enable and draw the colors */
#define COLORSEL_CLOBBER 2	/* colors get scribbled over */
#define COLORSEL_REFRESH 4	/* redraw the colors, either on or off */
#define COLORSEL_CLOBBER_WIPE 8 /* draw the (greyed out) colors, but don't disable */
#define COLORSEL_FORCE_REDRAW 16 /* enable, and force redraw (to make color picker work) */

static unsigned draw_colors(unsigned action);

/* hide all scale-related values here */
typedef struct scaleparams
{
    unsigned numer, denom;
} scaleparams;

static scaleparams scaletable[] = {
    {1, 256},			/*  0.00390625 */
    {3, 512},			/*  0.005859375 */
    {1, 128},			/*  0.0078125 */
    {3, 256},			/*  0.01171875 */
    {1, 64},			/*  0.015625 */
    {3, 128},			/*  0.0234375 */
    {1, 32},			/*  0.03125 */
    {3, 64},			/*  0.046875 */
    {1, 16},			/*  0.0625 */
    {3, 32},			/*  0.09375 */
    {1, 8},			/*  0.125 */
    {3, 16},			/*  0.1875 */
    {1, 4},			/*  0.25 */
    {3, 8},			/*  0.375 */
    {1, 2},			/*  0.5 */
    {3, 4},			/*  0.75 */
    {1, 1},			/*  1 */
    {3, 2},			/*  1.5 */
    {2, 1},			/*  2 */
    {3, 1},			/*  3 */
    {4, 1},			/*  4 */
    {6, 1},			/*  6 */
    {8, 1},			/*  8 */
    {12, 1},			/* 12 */
    {16, 1},			/* 16 */
    {24, 1},			/* 24 */
    {32, 1},			/* 32 */
    {48, 1},			/* 48 */
};


/* Macros: */

#define HARD_MIN_STAMP_SIZE 0	/* bottom of scaletable */
#define HARD_MAX_STAMP_SIZE (sizeof scaletable / sizeof scaletable[0] - 1)

#define MIN_STAMP_SIZE (stamp_data[stamp_group][cur_stamp[stamp_group]]->min)
#define MAX_STAMP_SIZE (stamp_data[stamp_group][cur_stamp[stamp_group]]->max)

/* to scale some offset, in pixels, like the current stamp is scaled */
#define SCALE_LIKE_STAMP(x) ( ((x) * scaletable[stamp_data[stamp_group][cur_stamp[stamp_group]]->size].numer + scaletable[stamp_data[stamp_group][cur_stamp[stamp_group]]->size].denom-1) / scaletable[stamp_data[stamp_group][cur_stamp[stamp_group]]->size].denom )

/* pixel dimensions of the current stamp, as scaled */
#define CUR_STAMP_W SCALE_LIKE_STAMP(active_stamp->w)
#define CUR_STAMP_H SCALE_LIKE_STAMP(active_stamp->h)


#define REPEAT_SPEED 300	/* Initial repeat speed for scrollbars */
#define CURSOR_BLINK_SPEED 500	/* Initial repeat speed for cursor */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE		/* for strcasestr() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* On Linux, we can use 'wordexp()' to expand env. vars. in settings
 pulled from config. files */
#ifdef __linux__
#include <wordexp.h>
#endif


/* Check if features.h did its 'magic', in which case strcasestr() is
 likely available; if not using GNU, you can set HAVE_STRCASESTR to
 avoid trying to redefine it -bjk 2006.06.02 */

#if !defined(__USE_GNU) && !defined(HAVE_STRCASESTR)
#warning "Attempting to define strcasestr(); if errors, build with -DHAVE_STRCASESTR"

char *strcasestr(const char *haystack, const char *needle)
{
    char *uphaystack, *upneedle, *result;
    unsigned int i;
    
    uphaystack = strdup(haystack);
    upneedle = strdup(needle);
    
    if (uphaystack == NULL || upneedle == NULL)
        return (NULL);
    
    for (i = 0; i < strlen(uphaystack); i++)
        uphaystack[i] = toupper(uphaystack[i]);
    
    for (i = 0; i < strlen(upneedle); i++)
        upneedle[i] = toupper(upneedle[i]);
    
    result = strstr(uphaystack, upneedle);
    
    if (result != NULL)
        return (result - uphaystack + (char *) haystack);
    else
        return NULL;
}

#endif


/* math.h makes y1 an obscure function! */
#define y1 evil_y1
#include <math.h>
#undef y1

#include <locale.h>

#ifdef __BEOS__
#include <wchar.h>
#else
#include <wchar.h>
#include <wctype.h>
#endif

#include <libintl.h>
#ifndef gettext_noop
#define gettext_noop(String) String
#endif


#ifdef DEBUG
#define gettext(String) debug_gettext(String)
#endif


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32

/* Not Windows: */

#include <unistd.h>
#include <dirent.h>
#include <signal.h>

#ifdef __BEOS__

/* BeOS */

#include "BeOS_print.h"

/* workaround dirent handling bug in TuxPaint code */
typedef struct safer_dirent
{
    dev_t d_dev;
    dev_t d_pdev;
    ino_t d_ino;
    ino_t d_pino;
    unsigned short d_reclen;
    char d_name[NAME_MAX];
} safer_dirent;
#define dirent safer_dirent

#else /* __BEOS__ */

/* Not BeOS */

#ifdef __APPLE__

/* Apple */

#include "ios_print.h"
#include "message.h"
#include "speech.h"
#include "wrapperdata.h"
#include "magic_header.h"
//extern WrapperData macosx;dailin
WrapperData macosx;
#else /* __APPLE__ */

/* Not Windows, not BeOS, not Apple */

#include "postscript_print.h"

#endif /* __APPLE__ */

#endif /* __BEOS__ */

#else /* WIN32 */

/* Windows */

#include <unistd.h>
#include <dirent.h>
#include <malloc.h>
#include "win32_print.h"
#include <io.h>
#include <direct.h>

#define mkdir(path,access)    _mkdir(path)

#endif /* WIN32 */

#include <errno.h>
#include <sys/stat.h>

#include "SDL.h"
#if !defined(_SDL_H)
#error "---------------------------------------------------"
#error "If you installed SDL from a package, be sure to get"
#error "the development package, as well!"
#error "(e.g., 'libsdl1.2-devel.rpm')"
#error "---------------------------------------------------"
#endif

#include "SDL_image.h"
#if !defined(_SDL_IMAGE_H) && !defined(_IMG_h)
#error "---------------------------------------------------"
#error "If you installed SDL_image from a package, be sure"
#error "to get the development package, as well!"
#error "(e.g., 'libsdl-image1.2-devel.rpm')"
#error "---------------------------------------------------"
#endif

#include "SDL_ttf.h"
#if !defined(_SDL_TTF_H) && !defined(_SDLttf_h)
#error "---------------------------------------------------"
#error "If you installed SDL_ttf from a package, be sure"
#error "to get the development package, as well!"
#error "(e.g., 'libsdl-ttf1.2-devel.rpm')"
#error "---------------------------------------------------"
#endif


#ifndef NO_SDLPANGO

/*
 The following section renames global variables defined in SDL_Pango.h to avoid errors during linking.
 It is okay to rename these variables because they are constants.
 SDL_Pango.h is included by tuxpaint.c.
 */
#define _MATRIX_WHITE_BACK _MATRIX_WHITE_BACK0
#define MATRIX_WHITE_BACK MATRIX_WHITE_BACK0
#define _MATRIX_BLACK_BACK _MATRIX_BLACK_BACK0
#define MATRIX_BLACK_BACK MATRIX_BLACK_BACK0
#define _MATRIX_TRANSPARENT_BACK_BLACK_LETTER _MATRIX_TRANSPARENT_BACK_BLACK_LETTER0
#define MATRIX_TRANSPARENT_BACK_BLACK_LETTER MATRIX_TRANSPARENT_BACK_BLACK_LETTER0
#define _MATRIX_TRANSPARENT_BACK_WHITE_LETTER _MATRIX_TRANSPARENT_BACK_WHITE_LETTER0
#define MATRIX_TRANSPARENT_BACK_WHITE_LETTER MATRIX_TRANSPARENT_BACK_WHITE_LETTER0
#define _MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER _MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER0
#define MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER MATRIX_TRANSPARENT_BACK_TRANSPARENT_LETTER0
/*
 The renaming ends here.
 */

#include "SDL_Pango.h"
#if !defined(SDL_PANGO_H)
#error "---------------------------------------------------"
#error "If you installed SDL_Pango from a package, be sure"
#error "to get the development package, as well!"
#error "(e.g., 'libsdl-pango1-dev.rpm')"
#error "---------------------------------------------------"
#endif

#endif


#ifndef NOSOUND
#include "SDL_mixer.h"
#if !defined(_SDL_MIXER_H) && !defined(_MIXER_H_)
#error "---------------------------------------------------"
#error "If you installed SDL_mixer from a package, be sure"
#error "to get the development package, as well!"
#error "(e.g., 'libsdl-mixer1.2-devel.rpm')"
#error "---------------------------------------------------"
#endif
#endif

//#ifndef NOSVG dailin
//
//#ifdef OLD_SVG
//#include "cairo.h"
//#include "svg.h"
//#include "svg-cairo.h"
//#if !defined(CAIRO_H) || !defined(SVG_H) || !defined(SVG_CAIRO_H)
//#error "---------------------------------------------------"
//#error "If you installed Cairo, libSVG or svg-cairo from packages, be sure"
//#error "to get the development package, as well!"
//#error "(e.g., 'libcairo-dev.rpm')"
//#error "---------------------------------------------------"
//#endif
//
//#else
//
//#include <librsvg/rsvg.h>
//#include <librsvg/rsvg-cairo.h>
///* #include "rsvg.h" */
///* #include "rsvg-cairo.h" */
//#if !defined(RSVG_H) || !defined(RSVG_CAIRO_H)
//#error "---------------------------------------------------"
//#error "If you installed libRSVG from packages, be sure"
//#error "to get the development package, as well!"
//#error "(e.g., 'librsvg2-dev.rpm')"
//#error "---------------------------------------------------"
//#endif
//
//#endif
//
//#endif

#include <png.h>
#define FNAME_EXTENSION ".bmp"
#ifndef PNG_H
#error "---------------------------------------------------"
#error "If you installed the PNG libraries from a package,"
#error "be sure to get the development package, as well!"
#error "(e.g., 'libpng2-devel.rpm')"
#error "---------------------------------------------------"
#endif

//#include "SDL_getenv.h" dailin

#include "i18n.h"
#include "cursor.h"
#include "pixels.h"
#include "rgblinear.h"
#include "playsound.h"
#include "progressbar.h"
#include "fonts.h"
#include "dirwalk.h"
#include "get_fname.h"

#include "tools.h"
#include "titles.h"
#include "colors.h"
#include "shapes.h"
#include "sounds.h"
#include "tip_tux.h"
#include "great.h"

#include "im.h"


#ifdef DEBUG_MALLOC
#include "malloc.c"
#endif


#include "compiler.h"


#if VIDEO_BPP==32
#ifdef __GNUC__
#define SDL_GetRGBA(p,f,rp,gp,bp,ap) ({ \
unsigned u_p = p;                     \
*(ap) = (u_p >> 24) & 0xff;           \
*(rp) = (u_p >> 16) & 0xff;           \
*(gp) = (u_p >>  8) & 0xff;           \
*(bp) = (u_p >>  0) & 0xff;           \
})
#define SDL_GetRGB(p,f,rp,gp,bp) ({ \
unsigned u_p = p;                     \
*(rp) = (u_p >> 16) & 0xff;           \
*(gp) = (u_p >>  8) & 0xff;           \
*(bp) = (u_p >>  0) & 0xff;           \
})
#endif
#define SDL_MapRGBA(f,r,g,b,a) ( \
(((a) & 0xffu) << 24)          \
|                              \
(((r) & 0xffu) << 16)          \
|                              \
(((g) & 0xffu) <<  8)          \
|                              \
(((b) & 0xffu) <<  0)          \
)
#define SDL_MapRGB(f,r,g,b) (   \
(((r) & 0xffu) << 16)          \
|                              \
(((g) & 0xffu) <<  8)          \
|                              \
(((b) & 0xffu) <<  0)          \
)
#endif


enum
{
    SAVE_OVER_PROMPT,
    SAVE_OVER_ALWAYS,
    SAVE_OVER_NO
};

enum
{
    ALTPRINT_MOD,
    ALTPRINT_ALWAYS,
    ALTPRINT_NEVER
};


enum
{
    STARTER_OUTLINE,
    STARTER_SCENE
};

/* Color globals (copied from colors.h, if no colors specified by user) */

int NUM_COLORS;
Uint8 * * color_hexes;
char * * color_names;


/* Show debugging stuff: */

static void debug(const char *const str)
{
#ifndef DEBUG
    (void) str;
#else
    fprintf(stderr, "DEBUG: %s\n", str);
    fflush(stderr);
#endif
}

static const char *getfilename(const char *path)
{
    char *p;
    
    if ((p = strrchr(path, '\\')) != NULL)
        return p + 1;
    if ((p = strrchr(path, '/')) != NULL)
        return p + 1;
    return path;
}


/* sizing */

/* The old Tux Paint:
 640x480 screen
 448x376 canvas
 40x96  titles near the top
 48x48  button tiles
 ??x56  tux area
 room for 2x7 button tile grids */

typedef struct
{
    Uint8 rows, cols;
} grid_dims;

/* static SDL_Rect r_screen; */ /* was 640x480 @ 0,0  -- but this isn't so useful */
static SDL_Rect r_canvas;	/* was 448x376 @ 96,0 */
static SDL_Rect r_tools;	/* was 96x336 @ 0,40 */
static SDL_Rect r_sfx;
static SDL_Rect r_toolopt;	/* was 96x336 @ 544,40 */
static SDL_Rect r_colors;	/* was 544x48 @ 96,376 */
static SDL_Rect r_ttools;	/* was 96x40 @ 0,0  (title for tools, "Tools") */
static SDL_Rect r_tcolors;	/* was 96x48 @ 0,376 (title for colors, "Colors") */
static SDL_Rect r_ttoolopt;	/* was 96x40 @ 544,0 (title for tool options) */
static SDL_Rect r_tuxarea;	/* was 640x56 */

static int button_w;		/* was 48 */
static int button_h;		/* was 48 */

static int color_button_w;	/* was 32 */
static int color_button_h;	/* was 48 */

/* Define button grid dimensions. (in button units) */
/* These are the maximum slots -- some may be unused. */
static grid_dims gd_tools;	/* was 2x7 */
static grid_dims gd_sfx;
static grid_dims gd_toolopt;	/* was 2x7 */
/* static grid_dims gd_open; */   /* was 4x4 */
static grid_dims gd_colors;	/* was 17x1 */

#define HEIGHTOFFSET ((((WINDOW_HEIGHT - 480) / 48) - 1)* 48)
#define TOOLOFFSET (HEIGHTOFFSET / 48 * 2)
#define PROMPTOFFSETX (WINDOW_WIDTH - 640) / 2
#define PROMPTOFFSETY (HEIGHTOFFSET / 2)

#define THUMB_W ((WINDOW_WIDTH - 96 - 96) / 4)
#define THUMB_H (((48 * 7 + 40 + HEIGHTOFFSET) - 72) / 4)

static int WINDOW_WIDTH, WINDOW_HEIGHT;

void magic_putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel);
Uint32 magic_getpixel(SDL_Surface * surface, int x, int y);


static void setup_normal_screen_layout(void)
{
    int buttons_tall;
    
    button_w = 48;
    button_h = 48;
    
    gd_toolopt.cols = 2;
    gd_tools.cols = 2;
    
    r_ttools.x = 0;
    r_ttools.y = 0;
    r_ttools.w = gd_tools.cols * button_w;
    r_ttools.h = 40;
    
    r_ttoolopt.w = gd_toolopt.cols * button_w;
    r_ttoolopt.h = 40;
    r_ttoolopt.x = WINDOW_WIDTH - r_ttoolopt.w;
    r_ttoolopt.y = 0;
    
    gd_colors.rows = 1;
    gd_colors.cols = (NUM_COLORS + gd_colors.rows - 1) / gd_colors.rows;
    
    r_colors.h = 48;
    color_button_h = r_colors.h / gd_colors.rows;
    r_tcolors.h = r_colors.h;
    
    r_tcolors.x = 0;
    r_tcolors.w = gd_tools.cols * button_w;
    r_colors.x = r_tcolors.w;
    r_colors.w = WINDOW_WIDTH - r_tcolors.w;
    
    color_button_w = r_colors.w / gd_colors.cols;
    
    /* This would make it contain _just_ the color spots,
     without any leftover bit on the end. Hmmm... */
    /* r_colors.w = color_button_w * gd_colors.cols; */
    
    r_canvas.x = gd_tools.cols * button_w;
    r_canvas.y = 0;
    r_canvas.w = WINDOW_WIDTH - (gd_tools.cols + gd_toolopt.cols) * button_w;
    
    r_tuxarea.x = 0;
    r_tuxarea.w = WINDOW_WIDTH;
    
    /* need 56 minimum for the Tux area */
    buttons_tall = (WINDOW_HEIGHT - r_ttoolopt.h - 56 - r_colors.h) / button_h;
    gd_tools.rows = buttons_tall;
    gd_toolopt.rows = buttons_tall;
    
    r_canvas.h = r_ttoolopt.h + buttons_tall * button_h-14;
    
    r_colors.y = r_canvas.h + r_canvas.y;
    r_tcolors.y = r_canvas.h + r_canvas.y;
    
    r_tuxarea.y = r_colors.y + r_colors.h;
    r_tuxarea.h = WINDOW_HEIGHT - r_tuxarea.y;
    
    r_sfx.x = r_tuxarea.x;
    r_sfx.y = r_tuxarea.y;
    r_sfx.w = button_w;      /* Two half-sized buttons across */
    r_sfx.h = button_h >> 1; /* One half-sized button down */
    
    gd_sfx.rows = 1;
    gd_sfx.cols = 2;
    
    r_tools.x = 0;
    r_tools.y = r_ttools.h + r_ttools.y;
    r_tools.w = gd_tools.cols * button_w;
    r_tools.h = gd_tools.rows * button_h;
    
    r_toolopt.w = gd_toolopt.cols * button_w;
    r_toolopt.h = gd_toolopt.rows * button_h;
    r_toolopt.x = WINDOW_WIDTH - r_ttoolopt.w;
    r_toolopt.y = r_ttoolopt.h + r_ttoolopt.y;
    
    /* TODO: dialog boxes */
    
}

#ifdef DEBUG
static void debug_rect(SDL_Rect * r, char *name)
{
    printf("%-12s %dx%d @ %d,%d\n", name, r->w, r->h, r->x, r->y);
}

#define DR(x) debug_rect(&x, #x)

static void debug_dims(grid_dims * g, char *name)
{
    printf("%-12s %dx%d\n", name, g->cols, g->rows);
}

#define DD(x) debug_dims(&x, #x)

static void print_layout(void)
{
    printf("\n--- layout ---\n");
    DR(r_canvas);
    DR(r_tools);
    DR(r_toolopt);
    DR(r_colors);
    DR(r_ttools);
    DR(r_tcolors);
    DR(r_ttoolopt);
    DR(r_tuxarea);
    DD(gd_tools);
    DD(gd_toolopt);
    DD(gd_colors);
    printf("buttons are %dx%d\n", button_w, button_h);
    printf("color buttons are %dx%d\n", color_button_w, color_button_h);
}

#undef DD
#undef DR
#endif

static void setup_screen_layout(void)
{
    /* can do right-to-left, colors at the top, extra tool option columns, etc. */
    setup_normal_screen_layout();
#ifdef DEBUG
    print_layout();
#endif
}

static SDL_Surface *screen;
static SDL_Surface *canvas;
static SDL_Surface *img_starter, *img_starter_bkgd;

/* Update a rect. based on two x/y coords (not necessarly in order): */
static void update_screen(int x1, int y1, int x2, int y2)
{
    int tmp;
    
    if (x1 > x2)
    {
        tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    
    if (y1 > y2)
    {
        tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    
    x1 = x1 - 1;
    x2 = x2 + 1;
    y1 = y1 - 1;
    y2 = y2 + 1;
    
    
    if (x1 < 0)
        x1 = 0;
    if (x2 < 0)
        x2 = 0;
    if (y1 < 0)
        y1 = 0;
    if (y2 < 0)
        y2 = 0;
    
    if (x1 >= WINDOW_WIDTH)
        x1 = WINDOW_WIDTH - 1;
    if (x2 >= WINDOW_WIDTH)
        x2 = WINDOW_WIDTH - 1;
    if (y1 >= WINDOW_HEIGHT)
        y1 = WINDOW_HEIGHT - 1;
    if (y2 >= WINDOW_HEIGHT)
        y2 = WINDOW_HEIGHT - 1;
    
    SDL_UpdateRect(screen, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}


static void update_screen_rect(SDL_Rect * r)
{
    SDL_UpdateRect(screen, r->x, r->y, r->w, r->h);
}

static int hit_test(const SDL_Rect * const r, unsigned x, unsigned y)
{
    /* note the use of unsigned math: no need to check for negative */
    return x - r->x < r->w && y - r->y < r->h;
}

#define HIT(r) hit_test(&(r), event.button.x, event.button.y)


/* "#if"ing out, since unused; bjk 2005.01.09 */

#if 0

/* x,y are pixel-wise screen-relative (mouse location), not grid-wise
 w,h are the size of a grid item
 Return the grid box.
 NOTE: grid items must fill full SDL_Rect width exactly */
static int grid_hit_wh(const SDL_Rect * const r, unsigned x, unsigned y,
                       unsigned w, unsigned h)
{
    return (x - r->x) / w + (y - r->y) / h * (r->w / w);
}

/* test an SDL_Rect r containing an array of WxH items for a grid location */
#define GRIDHIT_WH(r,W,H) grid_hit_wh(&(r), event.button.x, event.button.y, W,H)

#endif

/* test an SDL_Rect r containing an array of SDL_Surface surf for a grid location */
#define GRIDHIT_SURF(r,surf) grid_hit_wh(&(r), event.button.x, event.button.y, (surf)->w, (surf)->h)

/* x,y are pixel-wise screen-relative (mouse location), not grid-wise
 Return the grid box.
 NOTE: returns -1 if hit is below or to the right of the grid */
static int grid_hit_gd(const SDL_Rect * const r, unsigned x, unsigned y,
                       grid_dims * gd)
{
    unsigned item_w = r->w / gd->cols;
    unsigned item_h = r->h / gd->rows;
    unsigned col = (x - r->x) / item_w;
    unsigned row = (y - r->y) / item_h;
#ifdef DEBUG
    printf("%d,%d resolves to %d,%d in a %dx%d grid, index is %d\n", x, y, col,
           row, gd->cols, gd->rows, col + row * gd->cols);
#endif
    if (col >= gd->cols || row >= gd->rows)
        return -1;
    return col + row * gd->cols;
}

/* test an SDL_Rect r for a grid location, based on a grid_dims gd */
#define GRIDHIT_GD(r,gd) grid_hit_gd(&(r), event.button.x, event.button.y, &(gd))

/* Update the screen with the new canvas: */
static void update_canvas(int x1, int y1, int x2, int y2)
{
    SDL_Rect src, dest;
    
    if (img_starter != NULL)
    {
        /* If there was a starter, cover this part of the drawing with
         the corresponding part of the starter's foreground! */
        
        src.x = x1;
        src.y = y1;
        src.w = x2 - x1 + 1;
        src.h = y2 - y1 + 1;
        
        dest.x = x1;
        dest.y = y1;
        dest.w = src.w;
        dest.h = src.h;
        
        SDL_BlitSurface(img_starter, &dest, canvas, &dest);
    }
    
    SDL_BlitSurface(canvas, NULL, screen, &r_canvas);
    update_screen(x1 + 96, y1, x2 + 96, y2);
}


/* Globals: */

static int
disable_screensaver, fullscreen, native_screensize, grab_input,
rotate_orientation,

disable_print, print_delay, use_print_config, alt_print_command_default,
want_alt_printcommand,

wheely, keymouse, mouse_x, mouse_y,
no_button_distinction,
mousekey_up, mousekey_down, mousekey_left, mousekey_right,
button_down,
scrolling,

promptless_save, disable_quit,

noshortcuts,
disable_save, ok_to_use_lockfile,
start_blank, autosave_on_quit,

dont_do_xor, dont_load_stamps, mirrorstamps, disable_stamp_controls,
stamp_size_override,

simple_shapes, only_uppercase,

disable_magic_controls;

static int starter_mirrored, starter_flipped, starter_personal;
static Uint8 canvas_color_r, canvas_color_g, canvas_color_b;
Uint8 * touched;


/* Magic tools API and tool handles: */

#include "tp_magic_api.h"

void update_progress_bar(void);
void special_notify(int flags);

typedef struct magic_funcs_s {
    int (*get_tool_count)(magic_api *);
    char * (*get_name)(magic_api *, int);
    SDL_Surface * (*get_icon)(magic_api *, int);
    char * (*get_description)(magic_api *, int, int);
    int (*requires_colors)(magic_api *, int);
    int (*modes)(magic_api *, int);
    void (*set_color)(magic_api *, Uint8, Uint8, Uint8);
    int (*init)(magic_api *);
    Uint32 (*api_version)(void);
    void (*shutdown)(magic_api *);
    void (*click)(magic_api *, int, int, SDL_Surface *, SDL_Surface *, int, int, SDL_Rect *);
    void (*drag)(magic_api *, int, SDL_Surface *, SDL_Surface *, int, int, int, int, SDL_Rect *);
    void (*release)(magic_api *, int, SDL_Surface *, SDL_Surface *, int, int, SDL_Rect *);
    void (*switchin)(magic_api *, int, int, SDL_Surface *, SDL_Surface *);
    void (*switchout)(magic_api *, int, int, SDL_Surface *, SDL_Surface *);
} magic_funcs_t;


typedef struct magic_s {
    int place;
    int handle_idx;	/* Index to magic funcs for each magic tool (shared objs may report more than 1 tool) */
    int idx;	/* Index to magic tools within shared objects (shared objs may report more than 1 tool) */
    int mode;	/* Current mode (paint or fullscreen) */
    int avail_modes;	/* Available modes (paint &/or fullscreen) */
    int colors;	/* Whether magic tool accepts colors */
    char * name;	/* Name of magic tool */
    char * tip[MAX_MODES];	/* Description of magic tool, in each possible mode */
    SDL_Surface * img_icon;
    SDL_Surface * img_name;
} magic_t;


/* FIXME: Drop the 512 constants :^P */

static int num_plugin_files;	/* How many shared object files we went through */
void * magic_handle[512];	/* Handle to shared object (to be unloaded later) */ /* FIXME: Unload them! */
magic_funcs_t magic_funcs[512];	/* Pointer to shared objects' functions */

magic_t magics[512];
static int num_magics;	/* How many magic tools were loaded (note: shared objs may report more than 1 tool) */

enum {
    MAGIC_PLACE_GLOBAL,
    MAGIC_PLACE_LOCAL,
#ifdef __APPLE__
    MAGIC_PLACE_ALLUSERS,
#endif
    NUM_MAGIC_PLACES
};

magic_api *magic_api_struct; /* Pointer to our internal functions; passed to shared object's functions when we call them */


#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
#include <paper.h>
#if !defined(PAPER_H)
#error "---------------------------------------------------"
#error "If you installed libpaper from a package, be sure"
#error "to get the development package, as well!"
#error "(eg., 'libpaper-dev_1.1.21.deb')"
#error "---------------------------------------------------"
#endif
static const char *printcommand = PRINTCOMMAND;
static const char *altprintcommand = ALTPRINTCOMMAND;
char *papersize = NULL;
#endif


#if 0 /* FIXME: ifdef for platforms that lack fribidi? */
#include <fribidi.h> //dailin
#if !defined(_FRIBIDI_H) && !defined(FRIBIDI_H)
#error "---------------------------------------------------"
#error "If you installed libfribidi from a package, be sure"
#error "to get the development package, as well!"
#error "(eg., 'libfribidi-dev')"
#error "---------------------------------------------------"
#endif
#else
/* FIXME: define a noop function */
#endif

enum
{
    UNDO_STARTER_NONE,
    UNDO_STARTER_MIRRORED,
    UNDO_STARTER_FLIPPED
};

#define NUM_UNDO_BUFS 20
static SDL_Surface *undo_bufs[NUM_UNDO_BUFS];
static int undo_starters[NUM_UNDO_BUFS];
static int cur_undo, oldest_undo, newest_undo;

static SDL_Surface *img_title, *img_title_credits, *img_title_tuxpaint;
static SDL_Surface *img_btn_up, *img_btn_down, *img_btn_off;
static SDL_Surface *img_btnsm_up, *img_btnsm_off;
static SDL_Surface *img_prev, *img_next;
static SDL_Surface *img_mirror, *img_flip;
static SDL_Surface *img_dead40x40;
static SDL_Surface *img_black, *img_grey;
static SDL_Surface *img_yes, *img_no;
static SDL_Surface *img_sfx, *img_speak;
static SDL_Surface *img_open, *img_erase, *img_back, *img_trash;
static SDL_Surface *img_slideshow, *img_play, *img_select_digits;
static SDL_Surface *img_printer, *img_printer_wait;
static SDL_Surface *img_save_over, *img_popup_arrow;
static SDL_Surface *img_cursor_up, *img_cursor_down;
static SDL_Surface *img_cursor_starter_up, *img_cursor_starter_down;
static SDL_Surface *img_scroll_up, *img_scroll_down;
static SDL_Surface *img_scroll_up_off, *img_scroll_down_off;
static SDL_Surface *img_grow, *img_shrink;
static SDL_Surface *img_magic_paint, *img_magic_fullscreen;
static SDL_Surface *img_bold, *img_italic;
static SDL_Surface *img_color_picker, *img_color_picker_thumb, *img_paintwell;
int color_picker_x, color_picker_y;

static SDL_Surface *img_title_on, *img_title_off,
*img_title_large_on, *img_title_large_off;
static SDL_Surface *img_title_names[NUM_TITLES];
static SDL_Surface *img_tools[NUM_TOOLS], *img_tool_names[NUM_TOOLS];

static int button_label_y_nudge;

static SDL_Surface *thumbnail(SDL_Surface * src, int max_x, int max_y,
                              int keep_aspect);
static SDL_Surface *thumbnail2(SDL_Surface * src, int max_x, int max_y,
                               int keep_aspect, int keep_alpha);

#ifndef NO_BILINEAR
static SDL_Surface *zoom(SDL_Surface * src, int new_x, int new_y);
#endif



static SDL_Surface *render_text(TuxPaint_Font * restrict font,
                                const char *restrict str, SDL_Color color)
{
    SDL_Surface *ret = NULL;
    int height;
#ifndef NO_SDLPANGO
    SDLPango_Matrix pango_color;
#endif
    
    if (font == NULL)
    {
        printf("render_text() received a NULL font!\n");
        fflush(stdout);
        return NULL;
    }
    
#ifndef NO_SDLPANGO
    if (font->typ == FONT_TYPE_PANGO)
    {
        sdl_color_to_pango_color(color, &pango_color);
        
#ifdef DEBUG
        printf("Calling SDLPango_SetText(\"%s\")\n", str);
        fflush(stdout);
#endif
        
        SDLPango_SetDefaultColor(font->pango_context, &pango_color);
        SDLPango_SetText(font->pango_context, str, -1);
        ret = SDLPango_CreateSurfaceDraw(font->pango_context);
    }
#endif
    
    if (font->typ == FONT_TYPE_TTF)
    {
#ifdef DEBUG
        printf("Calling TTF_RenderUTF8_Blended(\"%s\")\n", str);
        fflush(stdout);
#endif
        
        ret = TTF_RenderUTF8_Blended(font->ttf_font, str, color);
    }
    
    if (ret)
        return ret;
    
    /* Sometimes a font will be missing a character we need. Sometimes the library
     will substitute a rectangle without telling us. Sometimes it returns NULL.
     Probably we should use FreeType directly. For now though... */
    
    height = 2;
    
    return thumbnail(img_title_large_off, height * strlen(str) / 2, height, 0);
}


/* This conversion is required on platforms where Uint16 doesn't match wchar_t.
 On Windows, wchar_t is 16-bit, elsewhere it is 32-bit.
 Mismatch caused by the use of Uint16 for unicode characters by SDL, SDL_ttf.
 I guess wchar_t is really only suitable for internal use ... */
static Uint16 *wcstou16(const wchar_t * str)
{
    unsigned int i, len = wcslen(str);
    Uint16 *res = malloc((len + 1) * sizeof(Uint16));
    
    for (i = 0; i < len + 1; ++i)
    {
        /* This is a bodge, but it seems unlikely that a case-conversion
         will cause a change from one utf16 character into two....
         (though at least UTF-8 suffers from this problem) */
        res[i] = (Uint16) str[i];
    }
    
    return res;
}


static SDL_Surface *render_text_w(TuxPaint_Font * restrict font,
                                  const wchar_t * restrict str,
                                  SDL_Color color)
{
    SDL_Surface *ret = NULL;
    int height;
    Uint16 *ustr;
#ifndef NO_SDLPANGO
    unsigned int i, j;
    int utfstr_max;
    char * utfstr;
    SDLPango_Matrix pango_color;
#endif
    
#ifndef NO_SDLPANGO
    if (font->typ == FONT_TYPE_PANGO)
    {
        sdl_color_to_pango_color(color, &pango_color);
        
        SDLPango_SetDefaultColor(font->pango_context, &pango_color);
        
        /* Convert from 16-bit UNICODE to UTF-8 encoded for SDL_Pango: */
        
        utfstr_max = (sizeof(char) * 4 * (wcslen(str) + 1));
        utfstr = (char *) malloc(utfstr_max);
        memset(utfstr, utfstr_max, 0);
        
        j = 0;
        for (i = 0; i < wcslen(str); i++)
        {
            if (str[i] <= 0x0000007F)
            {
                /* 0x00000000 - 0x0000007F:
                 0xxxxxxx */
                
                utfstr[j++] = (str[i] & 0x7F);
            }
            else if (str[i] <= 0x000007FF)
            {
                /* 0x00000080 - 0x000007FF:
                 
                 00000abc defghijk
                 110abcde 10fghijk */
                
                utfstr[j++] = (((str[i] & 0x0700) >> 6) |  /* -----abc -------- to ---abc-- */
                               ((str[i] & 0x00C0) >> 6) |  /* -------- de------ to ------de */
                               (0xC0));                    /*                  add 110----- */
                
                utfstr[j++] = (((str[i] & 0x003F)) |       /* -------- --fghijk to --fghijk */
                               (0x80));                    /*                  add 10------ */
            }
            else if (str[i] <= 0x0000FFFF)
            {
                /* 0x00000800 - 0x0000FFFF:
                 
                 abcdefgh ijklmnop
                 1110abcd 10efghij 10klmnop */
                
                utfstr[j++] = (((str[i] & 0xF000) >> 12) |  /* abcd---- -------- to ----abcd */
                               (0xE0));                      /*                  add 1110---- */
                utfstr[j++] = (((str[i] & 0x0FC0) >> 6)  |  /* ----efgh ij------ to --efghij */
                               (0x80));                      /*                  add 10------ */
                utfstr[j++] = (((str[i] & 0x003F)) |        /* -------- --klmnop to --klmnop */
                               (0x80));                      /*                  add 10------ */
            }
            else
            {
                /* 0x00010000 - 0x001FFFFF:
                 11110abc 10defghi 10jklmno 10pqrstu */
                
                utfstr[j++] = (((str[i] & 0x1C0000) >> 18) |   /* ---abc-- -------- --------  to -----abc */
                               (0xF0));                        /*                            add 11110000 */
                utfstr[j++] = (((str[i] & 0x030000) >> 12) |   /* ------de -------- --------  to --de---- */
                               ((str[i] & 0x00F000) >> 12) |   /* -------- fghi---- --------  to ----fghi */
                               (0x80));                        /*                            add 10------ */
                utfstr[j++] = (((str[i] & 0x000F00) >> 6) |    /* -------- ----jklm --------  to --jklm-- */
                               ((str[i] & 0x0000C0) >> 6) |    /* -------- -------- no------  to ------no */
                               (0x80));                        /*                            add 10------ */
                utfstr[j++] = ((str[i] & 0x00003F) |           /* -------- -------- --pqrstu  to --prqstu */
                               (0x80));                        /*                            add 10------ */
            }
        }
        utfstr[j] = '\0';
        
        
        SDLPango_SetText(font->pango_context, utfstr, -1);
        ret = SDLPango_CreateSurfaceDraw(font->pango_context);
    }
#endif
    
    if (font->typ == FONT_TYPE_TTF)
    {
        ustr = wcstou16(str);
        ret = TTF_RenderUNICODE_Blended(font->ttf_font, ustr, color);
        free(ustr);
    }
    
    if (ret)
        return ret;
    
    /* Sometimes a font will be missing a character we need. Sometimes the library
     will substitute a rectangle without telling us. Sometimes it returns NULL.
     Probably we should use FreeType directly. For now though... */
    
    height = 2;
    return thumbnail(img_title_large_off, height * wcslen(str) / 2, height, 0);
}


typedef struct stamp_type
{
    char *stampname;
    char *stxt;
    Uint8 locale_text;
#ifndef NOSOUND
    Mix_Chunk *ssnd;
    Mix_Chunk *sdesc;
#endif
    
    SDL_Surface *thumbnail;
    unsigned thumb_mirrored:1;
    unsigned thumb_flipped:1;
    unsigned thumb_mirrored_flipped:1;
    unsigned no_premirror:1;
    unsigned no_preflip:1;
    unsigned no_premirrorflip:1;
    
    unsigned processed:1;		/* got *.dat, computed size limits, etc. */
    
    unsigned no_sound:1;
    unsigned no_descsound:1;
    unsigned no_txt:1;
    /*  unsigned no_local_sound : 1;  */ /* to remember, if code written to discard sound */
    
    unsigned tinter:3;
    unsigned colorable:1;
    unsigned tintable:1;
    unsigned mirrorable:1;
    unsigned flipable:1;
    
    unsigned mirrored:1;
    unsigned flipped:1;
    unsigned min:5;
    unsigned size:5;
    unsigned max:5;
} stamp_type;

#define MAX_STAMP_GROUPS 256

static unsigned int stamp_group_dir_depth = 1; /* How deep (how many slashes in a subdirectory path) we think a new stamp group should be */

static int stamp_group = 0;

const char *load_stamp_basedir;
static int num_stamp_groups = 0;
static int num_stamps[MAX_STAMP_GROUPS];
static int max_stamps[MAX_STAMP_GROUPS];
static stamp_type **stamp_data[MAX_STAMP_GROUPS];

static SDL_Surface *active_stamp;

/* Returns whether a particular stamp can be colored: */
static int stamp_colorable(int stamp)
{
    return stamp_data[stamp_group][stamp]->colorable;
}

/* Returns whether a particular stamp can be tinted: */
static int stamp_tintable(int stamp)
{
    return stamp_data[stamp_group][stamp]->tintable;
}



#define SHAPE_BRUSH_NAME "aa_round_03.png"
static int num_brushes, num_brushes_max, shape_brush = 0;
static SDL_Surface **img_brushes;
static int * brushes_frames = NULL;
static int * brushes_spacing = NULL;
static short * brushes_directional = NULL;

static SDL_Surface *img_shapes[NUM_SHAPES], *img_shape_names[NUM_SHAPES];
static SDL_Surface *img_openlabels_open, *img_openlabels_erase,
*img_openlabels_slideshow, *img_openlabels_back, *img_openlabels_play,
*img_openlabels_next;

static SDL_Surface *img_tux[NUM_TIP_TUX];

static SDL_Surface *img_mouse, *img_mouse_click;

#ifdef LOW_QUALITY_COLOR_SELECTOR
static SDL_Surface *img_paintcan;
#else
static SDL_Surface * * img_color_btns;
static SDL_Surface *img_color_btn_off;
#endif

static int colors_are_selectable;

enum {
    BRUSH_DIRECTION_RIGHT,
    BRUSH_DIRECTION_DOWN_RIGHT,
    BRUSH_DIRECTION_DOWN,
    BRUSH_DIRECTION_DOWN_LEFT,
    BRUSH_DIRECTION_LEFT,
    BRUSH_DIRECTION_UP_LEFT,
    BRUSH_DIRECTION_UP,
    BRUSH_DIRECTION_UP_RIGHT,
    BRUSH_DIRECTION_NONE
};

static SDL_Surface *img_cur_brush;
int img_cur_brush_frame_w, img_cur_brush_w, img_cur_brush_h,
img_cur_brush_frames, img_cur_brush_directional, img_cur_brush_spacing;
static int brush_counter, brush_frame;

#define NUM_ERASERS 12	/* How many sizes of erasers
(from ERASER_MIN to _MAX as squares, then again
from ERASER_MIN to _MAX as circles) */
#define ERASER_MIN 13
#define ERASER_MAX 128



static unsigned cur_color;
static int cur_tool, cur_brush;
static int cur_stamp[MAX_STAMP_GROUPS];
static int cur_shape, cur_magic;
static int cur_font, cur_eraser;
static int cursor_left, cursor_x, cursor_y, cursor_textwidth;	/* canvas-relative */
static int been_saved;
static char file_id[32];
static char starter_id[32];
static int brush_scroll;
static int stamp_scroll[MAX_STAMP_GROUPS];
static int font_scroll, magic_scroll;
static int eraser_scroll, shape_scroll;	/* dummy variables for now */

static int eraser_sound;

static IM_DATA im_data;
static wchar_t texttool_str[256];
static unsigned int texttool_len;

static int tool_avail[NUM_TOOLS], tool_avail_bak[NUM_TOOLS];

static Uint32 cur_toggle_count;

typedef struct edge_type
{
    int y_upper;
    float x_intersect, dx_per_scan;
    struct edge_type *next;
} edge;


typedef struct point_type
{
    int x, y;
} point_type;

typedef struct fpoint_type
{
    float x, y;
} fpoint_type;

typedef enum
{ Left, Right, Bottom, Top } an_edge;
#define NUM_EDGES 4

static SDL_Event scrolltimer_event;

int non_left_click_count = 0;


typedef struct dirent2
{
    struct dirent f;
    int place;
} dirent2;


/* Local function prototypes: */

static void mainloop(void);
static void brush_draw(int x1, int y1, int x2, int y2, int update);
static void blit_brush(int x, int y, int direction);
static void stamp_draw(int x, int y);
static void rec_undo_buffer(void);
static void show_usage(FILE * f, char *prg);
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
void show_available_papersizes(FILE * fi, char * prg);
#endif
static void setup(int argc, char *argv[]);
void signal_handler(int sig);
static SDL_Cursor *get_cursor(unsigned char *bits, unsigned char *mask_bits,
                              unsigned int w, unsigned int h,
                              unsigned int x, unsigned int y);
static void seticon(void);
static SDL_Surface *loadimage(const char *const dir,const char *const fname);
static SDL_Surface *do_loadimage(const char *const fname, int abort_on_error);
static void draw_toolbar(void);
static void draw_magic(void);
static void draw_brushes(void);
static void draw_stamps(void);
static void draw_shapes(void);
static void draw_erasers(void);
static void draw_fonts(void);
static void draw_none(void);

static void do_undo(void);
static void do_redo(void);
static void render_brush(void);
static void line_xor(int x1, int y1, int x2, int y2);
static void rect_xor(int x1, int y1, int x2, int y2);
static void draw_blinking_cursor(void);
static void hide_blinking_cursor(void);

void reset_brush_counter_and_frame(void);
void reset_brush_counter(void);

#ifdef LOW_QUALITY_STAMP_OUTLINE
#define stamp_xor(x,y) rect_xor( \
(x) - (CUR_STAMP_W+1)/2, \
(y) - (CUR_STAMP_H+1)/2, \
(x) + (CUR_STAMP_W+1)/2, \
(y) + (CUR_STAMP_H+1)/2 \
)
#define update_stamp_xor()
#else
static void stamp_xor(int x1, int y1);
static void update_stamp_xor(void);
#endif

static void set_active_stamp(void);

static void do_eraser(int x, int y);
static void disable_avail_tools(void);
static void enable_avail_tools(void);
static void reset_avail_tools(void);
static int compare_dirent2s(struct dirent2 *f1, struct dirent2 *f2);
static void redraw_tux_text(void);
static void draw_tux_text(int which_tux, const char *const str,
                          int want_right_to_left);
static void draw_tux_text_ex(int which_tux, const char *const str,
                             int want_right_to_left, Uint8 locale_text);
static void wordwrap_text(const char *const str, SDL_Color color,
                          int left, int top, int right,
                          int want_right_to_left);
static void wordwrap_text_ex(const char *const str, SDL_Color color,
                             int left, int top, int right,
                             int want_right_to_left, Uint8 locale_text);
static char *loaddesc(const char *const fname, Uint8 * locale_text);
static double loadinfo(const char *const fname, stamp_type * inf);
#ifndef NOSOUND
static Mix_Chunk *loadsound(const char *const fname);
static Mix_Chunk *loaddescsound(const char *const fname);
static void playstampdesc(int chan);
#endif
static void do_wait(int counter);
static void load_current(void);
static void save_current(void);
static int do_prompt_image_flash(const char *const text,
                                 const char *const btn_yes,
                                 const char *const btn_no, SDL_Surface * img1,
                                 SDL_Surface * img2, SDL_Surface * img3,
                                 int animate, int ox, int oy);
static int do_prompt_image_flash_snd(const char *const text,
                                     const char *const btn_yes,
                                     const char *const btn_no,
                                     SDL_Surface * img1, SDL_Surface * img2,
                                     SDL_Surface * img3, int animate,
                                     int snd, int ox, int oy);
static int do_prompt_image(const char *const text, const char *const btn_yes,
                           const char *const btn_no, SDL_Surface * img1,
                           SDL_Surface * img2, SDL_Surface * img3,
                           int ox, int oy);
static int do_prompt_image_snd(const char *const text,
                               const char *const btn_yes,
                               const char *const btn_no, SDL_Surface * img1,
                               SDL_Surface * img2, SDL_Surface * img3,
                               int snd, int ox, int oy);
static int do_prompt(const char *const text, const char *const btn_yes,
                     const char *const btn_no, int ox, int oy);
static int do_prompt_snd(const char *const text, const char *const btn_yes,
                         const char *const btn_no, int snd, int ox, int oy);
static void cleanup(void);
static void free_surface(SDL_Surface ** surface_array);
static void free_surface_array(SDL_Surface * surface_array[], int count);
/*static void update_shape(int cx, int ox1, int ox2, int cy, int oy1, int oy2,
 int fixed); */
static void do_shape(int cx, int cy, int ox, int oy, int rotn, int use_brush);
static int rotation(int ctr_x, int ctr_y, int ox, int oy);
static int do_save(int tool, int dont_show_success_results);
static int do_png_save(FILE * fi, const char *const fname,
                       SDL_Surface * surf);
static void get_new_file_id(void);
//static int do_quit(int tool);
int do_open(void);
int do_new_dialog(void);
int do_color_picker(void);
int do_slideshow(void);
void play_slideshow(int * selected, int num_selected, char * dirname,
                    char **d_names, char **d_exts, int speed);
void draw_selection_digits(int right, int bottom, int n);
static void wait_for_sfx(void);
static void rgbtohsv(Uint8 r8, Uint8 g8, Uint8 b8, float *h, float *s,
                     float *v);
static void hsvtorgb(float h, float s, float v, Uint8 * r8, Uint8 * g8,
                     Uint8 * b8);

SDL_Surface *flip_surface(SDL_Surface * s);
SDL_Surface *mirror_surface(SDL_Surface * s);

//static void print_image(void);
static void do_print(void);
static void strip_trailing_whitespace(char *buf);
static void do_render_cur_text(int do_blit);
static char *uppercase(const char *restrict const str);
static wchar_t *uppercase_w(const wchar_t *restrict const str);
static char *textdir(const char *const str);
static SDL_Surface *do_render_button_label(const char *const label);
static void create_button_labels(void);
static Uint32 scrolltimer_callback(Uint32 interval, void *param);
static Uint32 drawtext_callback(Uint32 interval, void *param);
static void control_drawtext_timer(Uint32 interval, const char *const text, Uint8 locale_text);
static void parse_options(FILE * fi);
static const char *great_str(void);
static void draw_image_title(int t, SDL_Rect dest);
static void handle_keymouse(SDLKey key, Uint8 updown);
static void handle_active(SDL_Event * event);
static char *remove_slash(char *path);
/*static char *replace_tilde(const char* const path);*/
#ifdef NO_SDLPANGO
static void anti_carriage_return(int left, int right, int cur_top,
                                 int new_top, int cur_bot, int line_width);
#endif
static void load_starter_id(char *saved_id);
static void load_starter(char *img_id);
static SDL_Surface *duplicate_surface(SDL_Surface * orig);
static void mirror_starter(void);
static void flip_starter(void);
int valid_click(Uint8 button);
int in_circle(int x, int y);
int in_circle_rad(int x, int y, int rad);
int paintsound(int size);
void load_magic_plugins(void);
int magic_sort(const void * a, const void * b);

Mix_Chunk * magic_current_snd_ptr;
void magic_playsound(Mix_Chunk * snd, int left_right, int up_down);
void magic_stopsound(void);
void magic_line_func(void * mapi,
                     int which, SDL_Surface * canvas, SDL_Surface * last,
                     int x1, int y1, int x2, int y2, int step,
                     void (*cb)(void *, int, SDL_Surface *, SDL_Surface *,
                                int, int));

Uint8 magic_linear_to_sRGB(float lin);
float magic_sRGB_to_linear(Uint8 srgb);
int magic_button_down(void);
SDL_Surface * magic_scale(SDL_Surface * surf, int w, int h, int aspect);
void reset_touched(void);
Uint8 magic_touched(int x, int y);

void magic_switchin(SDL_Surface * last);
void magic_switchout(SDL_Surface * last);
int magic_modeint(int mode);

#ifdef DEBUG
static char *debug_gettext(const char *str);
static int charsize(Uint16 c);
#endif

#ifndef NOSVG
SDL_Surface * load_svg(char * file);
SDL_Surface * myIMG_Load(char * file);
float pick_best_scape(unsigned int orig_w, unsigned int orig_h,
                      unsigned int max_w, unsigned int max_h);
#else
#define myIMG_Load IMG_Load
#endif


#define MAX_UTF8_CHAR_LENGTH 6

#define USEREVENT_TEXT_UPDATE 1
#define USEREVENT_PLAYDESCSOUND 2

#define TP_SDL_MOUSEBUTTONSCROLL (SDL_USEREVENT + 1)

static int bypass_splash_wait;

/* Wait for a keypress or mouse click.
 counter is in 1/10 second units */
static void do_wait(int counter)
{
    SDL_Event event;
    int done;
    
    if (bypass_splash_wait)
        return;
    
    done = 0;
    
    do
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                done = 1;
                
                /* FIXME: Handle SDL_Quit better */
            }
            else if (event.type == SDL_ACTIVEEVENT)
            {
                handle_active(&event);
            }
            else if (event.type == SDL_KEYDOWN)
            {
                done = 1;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN &&
                     valid_click(event.button.button))
            {
                done = 1;
            }
        }
        
        counter--;
        SDL_Delay(100);
    }
    while (!done && counter > 0);
}


/* This lets us exit quickly; perhaps the system is swapping to death
 or the user started Tux Paint by accident. It also lets the user
 more easily bypass the splash screen wait. */

/* Was used in progressbar.c, but is currently commented out!
 -bjk 2006.06.02 */

#if 0
static void eat_sdl_events(void)
{
    SDL_Event event;
    
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            SDL_Quit();
            exit(0);			/* can't safely use do_quit during start-up */
        }
        else if (event.type == SDL_ACTIVEEVENT)
            handle_active(&event);
        else if (event.type == SDL_KEYDOWN)
        {
            SDLKey key = event.key.keysym.sym;
            SDLMod ctrl = event.key.keysym.mod & KMOD_CTRL;
            SDLMod alt = event.key.keysym.mod & KMOD_ALT;
            if ((key == SDLK_c && ctrl) || (key == SDLK_F4 && alt))
            {
                SDL_Quit();
                exit(0);
            }
            else if (key == SDLK_ESCAPE && waiting_for_fonts)
            {
                /* abort font loading! */
                
                printf("Aborting font load!\n");
                
                font_thread_aborted = 1;
                /* waiting_for_fonts = 0; */
            }
            else
                bypass_splash_wait = 1;
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN)
            bypass_splash_wait = 1;
    }
}
#endif


/* --- MAIN --- */
extern int startAD;
int main(int argc, char *argv[])
{
    CLOCK_TYPE time1;
    CLOCK_TYPE time2;
    SDL_Rect dest;
    SDL_Rect src;
    int i;
    
    CLOCK_ASM(time1);
    
    /* Set up locale support */
    setlocale(LC_ALL, "");
    ctype_utf8();
    
    
    /* NOTE: Moved run_font_scanner() call from here, to right after
     setup_language(), so that the gettext() calls used while testing fonts
     actually DO something (per tuxpaint-devel discussion, April 2007)
     -bjk 2007.06.05 */
    
    
    /* Set up! */
    setup(argc, argv);
    
    
    
#if 0
    while (!font_thread_done)
    {
        /* FIXME: should respond to quit events
         FIXME: should have a read-depends memory barrier around here */
        show_progress_bar();
        SDL_Delay(20);
    }
    SDL_WaitThread(font_thread, NULL);
#endif
    
    CLOCK_ASM(time2);
    
#ifdef DEBUG
    printf("Start-up time: %.3f\n", (double) (time2 - time1) / CLOCK_SPEED);
#endif
    
    /* Let the user know we're (nearly) ready now */
    
    dest.x = 0;
    dest.y = WINDOW_HEIGHT - img_progress->h;
    dest.h = img_progress->h;
    dest.w = WINDOW_WIDTH;
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
    src.h = img_progress->h;
    src.w = img_title->w;
    src.x = 0;
    src.y = img_title->h - img_progress->h;
    dest.x =
    ((WINDOW_WIDTH - img_title->w - (img_title_tuxpaint->w / 2)) / 2) +
    (img_title_tuxpaint->w / 2) + 20;
    SDL_BlitSurface(img_title, &src, screen, &dest);
    
    SDL_FreeSurface(img_title);
    SDL_FreeSurface(img_title_credits);
    SDL_FreeSurface(img_title_tuxpaint);
    
    dest.x = 0;
    dest.w = WINDOW_WIDTH;	/* SDL mangles this! So, do repairs. */
    update_screen_rect(&dest);
    
    do_setcursor(cursor_arrow);
    playsound(screen, 0, SND_HARP, 1, SNDPOS_CENTER, SNDDIST_NEAR);
    do_wait(50);			/* about 5 seconds */
    
    
    /* Set defaults! */
    
    cur_undo = 0;
    oldest_undo = 0;
    newest_undo = 0;
    
    cur_tool = TOOL_BRUSH;
    cur_color = COLOR_BLACK;
    colors_are_selectable = 1;
    cur_brush = 0;
    for (i = 0; i < MAX_STAMP_GROUPS; i++)
        cur_stamp[i] = 0;
    cur_shape = SHAPE_SQUARE;
    cur_magic = 0;
    cur_font = 0;
    cur_eraser = 0;
    cursor_left = -1;
    cursor_x = -1;
    cursor_y = -1;
    cursor_textwidth = 0;
    
    mouse_x = WINDOW_WIDTH / 2;
    mouse_y = WINDOW_HEIGHT / 2;
    SDL_WarpMouse(mouse_x, mouse_y);
    
    mousekey_up = SDL_KEYUP;
    mousekey_down = SDL_KEYUP;
    mousekey_left = SDL_KEYUP;
    mousekey_right = SDL_KEYUP;
    
    eraser_sound = 0;
    
    img_cur_brush = NULL;
    render_brush();
    
    brush_scroll = 0;
    for (i = 0; i < MAX_STAMP_GROUPS; i++)
        stamp_scroll[i] = 0;
    stamp_group = 0;  /* reset! */
    font_scroll = 0;
    magic_scroll = 0;
    
    
    reset_avail_tools();
    
    
    /* Load current image (if any): */

    if (start_blank == 0)
        load_current();
    
    been_saved = 1;
    tool_avail[TOOL_SAVE] = 0;
    
    
    /* Draw the screen! */
    
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 255, 255, 255));
    
    draw_toolbar();
    draw_colors(COLORSEL_REFRESH);
    draw_brushes();
    update_canvas(0, 0, WINDOW_WIDTH - 96, (48 * 7) + 40 + HEIGHTOFFSET);
    
    SDL_Flip(screen);
    
    draw_tux_text(tool_tux[cur_tool], tool_tips[cur_tool], 1);
    startAD = 1;
    /* Main loop! */
    mainloop();
    
    /* Close and quit! */
    
    save_current();
    
    wait_for_sfx();
    
    
    cleanup();
    
    return 0;
}


/* Prompt to confirm user wishes to quit */
#define PROMPT_QUIT_TXT gettext_noop("Do you really want to quit?")

/* Quit prompt positive response (quit) */
#define PROMPT_QUIT_YES gettext_noop("Yes, I’m done!")

/* Quit prompt negative response (don't quit) */
#define PROMPT_QUIT_NO  gettext_noop("No, take me back!")


/* Current picture is not saved; user is quitting */
#define PROMPT_QUIT_SAVE_TXT gettext_noop("If you quit, you’ll lose your picture! Save it?")
#define PROMPT_QUIT_SAVE_YES gettext_noop("Yes, save it!")
#define PROMPT_QUIT_SAVE_NO gettext_noop("No, don’t bother saving!")

/* Current picture is not saved; user is opening another picture */
#define PROMPT_OPEN_SAVE_TXT gettext_noop("Save your picture first?")
#define PROMPT_OPEN_SAVE_YES gettext_noop("Yes, save it!")
#define PROMPT_OPEN_SAVE_NO gettext_noop("No, don’t bother saving!")

/* Error opening picture */
#define PROMPT_OPEN_UNOPENABLE_TXT gettext_noop("Can’t open that picture!")

/* Generic dialog dismissal */
#define PROMPT_OPEN_UNOPENABLE_YES gettext_noop("OK")


/* Notification that 'Open' dialog has nothing to show */
#define PROMPT_OPEN_NOFILES_TXT gettext_noop("There are no saved files!")
#define PROMPT_OPEN_NOFILES_YES gettext_noop("OK")

/* Verification of print action */
#define PROMPT_PRINT_NOW_TXT gettext_noop("Print your picture now?")
#define PROMPT_PRINT_NOW_YES gettext_noop("Yes, print it!")
#define PROMPT_PRINT_NOW_NO gettext_noop("No, take me back!")

/* Confirmation of successful (we hope) printing */
#define PROMPT_PRINT_TXT gettext_noop("Your picture has been printed!")
#define PROMPT_PRINT_YES gettext_noop("OK")

/* We got an error printing */
#define PROMPT_PRINT_FAILED_TXT gettext_noop("Sorry! Your picture could not be printed!")

/* Notification that it's too soon to print again (--printdelay option is in effect) */
#define PROMPT_PRINT_TOO_SOON_TXT gettext_noop("You can’t print yet!")
#define PROMPT_PRINT_TOO_SOON_YES gettext_noop("OK")

/* Prompt to confirm erasing a picture in the Open dialog */
#define PROMPT_ERASE_TXT gettext_noop("Erase this picture?")
#define PROMPT_ERASE_YES gettext_noop("Yes, erase it!")
#define PROMPT_ERASE_NO gettext_noop("No, don’t erase it!")

/* Reminder that Mouse Button 1 is the button to use in Tux Paint */
#define PROMPT_TIP_LEFTCLICK_TXT gettext_noop("Remember to use the left mouse button!")
#define PROMPT_TIP_LEFTCLICK_YES gettext_noop("OK")


enum
{
    SHAPE_TOOL_MODE_STRETCH,
    SHAPE_TOOL_MODE_ROTATE,
    SHAPE_TOOL_MODE_DONE
};


/* --- MAIN LOOP! --- */

static void mainloop(void)
{
    int done, which, old_x, old_y, new_x, new_y,
    line_start_x, line_start_y, shape_tool_mode,
    shape_ctr_x, shape_ctr_y, shape_outer_x, shape_outer_y,
    old_stamp_group;
    int num_things;
    int *thing_scroll;
    int cur_thing, do_draw, old_tool, max;
    int ignoring_motion;
    SDL_TimerID scrolltimer = NULL;
    SDL_Event event;
    SDLKey key;
    SDLMod mod;
    Uint32 last_cursor_blink, cur_cursor_blink,
    pre_event_time, current_event_time;
    SDL_Rect update_rect;
#ifdef DEBUG
    Uint16 key_unicode;
    SDLKey key_down;
#endif
    
    num_things = num_brushes;
    thing_scroll = &brush_scroll;
    cur_thing = 0;
    do_draw = 0;
    old_x = 0;
    old_y = 0;
    line_start_x = 0;
    line_start_y = 0;
    shape_ctr_x = 0;
    shape_ctr_y = 0;
    shape_outer_x = 0;
    shape_outer_y = 0;
    shape_tool_mode = SHAPE_TOOL_MODE_DONE;
    button_down = 0;
    last_cursor_blink = cur_toggle_count = 0;
    texttool_len = 0;
    scrolling = 0;
    scrolltimer = 0;
    
    
    done = 0;
    
    do
    {
        ignoring_motion = 0;
        
        pre_event_time = SDL_GetTicks();
        
        
        while (SDL_PollEvent(&event))
        {
            current_event_time = SDL_GetTicks();
            
            if (current_event_time > pre_event_time + 250)
                ignoring_motion = 1;
            
            
            if (event.type == SDL_QUIT)
            {
//                magic_switchout(canvas);
//                done = do_quit(cur_tool);
//                if (!done)
//                    magic_switchin(canvas);
            }
            else if (event.type == SDL_ACTIVEEVENT)
            {
                handle_active(&event);
            }
            else if (event.type == SDL_KEYUP)
            {
                key = event.key.keysym.sym;
                
                handle_keymouse(key, SDL_KEYUP);
            }
            else if (event.type == SDL_KEYDOWN)
            {
                key = event.key.keysym.sym;
                mod = event.key.keysym.mod;
                
#ifdef DEBUG
                // FIXME: debug junk
                fprintf(stderr,
                        "key 0x%04x mod 0x%04x character 0x%04x %d <%c> is %sprintable, key_down 0x%x\n",
                        (unsigned)key,
                        (unsigned)mod,
                        (unsigned)event.key.keysym.unicode,
                        (int)event.key.keysym.unicode,
                        (key_unicode>' ' && key_unicode<127)?(char)event.key.keysym.unicode:' ',
                        iswprint(key_unicode)?"":"not ",
                        (unsigned)key_down
                        );
#endif
                
                handle_keymouse(key, SDL_KEYDOWN);
                
                if (key == SDLK_ESCAPE && !disable_quit)
                {
//                    magic_switchout(canvas);
//                    done = do_quit(cur_tool);
//                    if (!done)
//                        magic_switchin(canvas);
                }
                else if (key == SDLK_s && (mod & KMOD_ALT))
                {
#ifndef NOSOUND
                    if (use_sound)
                    {
                        mute = !mute;
                        Mix_HaltChannel(-1);
                        
                        if (mute)
                        {
                            /* Sound has been muted (silenced) via keyboard shortcut */
                            draw_tux_text(TUX_BORED, gettext("Sound muted."), 0);
                        }
                        else
                        {
                            /* Sound has been unmuted (unsilenced) via keyboard shortcut */
                            draw_tux_text(TUX_BORED, gettext("Sound unmuted."), 0);
                        }
                    }
#endif
                }
                else if (key == SDLK_ESCAPE &&
                         (mod & KMOD_SHIFT) && (mod & KMOD_CTRL))
                {
//                    magic_switchout(canvas);
//                    done = do_quit(cur_tool);
//                    if (!done)
//                        magic_switchin(canvas);
                }
#ifdef WIN32
                else if (key == SDLK_F4 && (mod & KMOD_ALT))
                {
                    magic_switchout(canvas);
                    done = do_quit(cur_tool);
                    if (!done)
                        magic_switchin(canvas);
                }
#endif
                else if (key == SDLK_z && (mod & KMOD_CTRL) && !noshortcuts)
                {
                    /* Ctrl-Z - Undo */
                    
                    magic_switchout(canvas);
                    
                    if (tool_avail[TOOL_UNDO])
                    {
                        hide_blinking_cursor();
                        if (cur_undo == newest_undo)
                        {
                            rec_undo_buffer();
                            do_undo();
                        }
                        do_undo();
                        update_screen_rect(&r_tools);
                        shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                    }
                    
                    magic_switchin(canvas);
                }
                else if (key == SDLK_r && (mod & KMOD_CTRL) && !noshortcuts)
                {
                    /* Ctrl-R - Redo */
                    
                    magic_switchout(canvas);
                    
                    if (tool_avail[TOOL_REDO])
                    {
                        hide_blinking_cursor();
                        do_redo();
                        update_screen_rect(&r_tools);
                        shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                    }
                    
                    magic_switchin(canvas);
                }
                else if (key == SDLK_o && (mod & KMOD_CTRL) && !noshortcuts)
                {
                    /* Ctrl-O - Open */
                    
                    magic_switchout(canvas);
                    
                    disable_avail_tools();
                    draw_toolbar();
                    draw_colors(COLORSEL_CLOBBER_WIPE);
                    draw_none();
                    
                    if (do_open() == 0)
                    {
                        if (cur_tool == TOOL_TEXT)
                            do_render_cur_text(0);
                    }
                    
                    enable_avail_tools();
                    
                    draw_toolbar();
                    update_screen_rect(&r_tools);
                    draw_colors(COLORSEL_REFRESH);
                    
                    if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_LINES)
                        draw_brushes();
                    else if (cur_tool == TOOL_MAGIC)
                        draw_magic();
                    else if (cur_tool == TOOL_STAMP)
                        draw_stamps();
                    else if (cur_tool == TOOL_TEXT)
                        draw_fonts();
                    else if (cur_tool == TOOL_SHAPES)
                        draw_shapes();
                    else if (cur_tool == TOOL_ERASER)
                        draw_erasers();
                    
                    draw_tux_text(TUX_GREAT, tool_tips[cur_tool], 1);
                    
                    /* FIXME: Make delay configurable: */
                    control_drawtext_timer(1000, tool_tips[cur_tool], 0);
                    
                    magic_switchin(canvas);
                }
                else if ((key == SDLK_n && (mod & KMOD_CTRL)) && !noshortcuts)
                {
                    /* Ctrl-N - New */
                    
                    magic_switchout(canvas);
                    
                    hide_blinking_cursor();
                    shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                    
                    disable_avail_tools();
                    draw_toolbar();
                    draw_colors(COLORSEL_CLOBBER_WIPE);
                    draw_none();
                    
                    if (do_new_dialog() == 0)
                    {
                        draw_tux_text(tool_tux[TUX_DEFAULT], TIP_NEW_ABORT, 1);
                        
                        if (cur_tool == TOOL_TEXT)
                            do_render_cur_text(0);
                    }
                    
                    enable_avail_tools();
                    
                    draw_toolbar();
                    update_screen_rect(&r_tools);
                    draw_colors(COLORSEL_REFRESH);
                    
                    if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_LINES)
                        draw_brushes();
                    else if (cur_tool == TOOL_MAGIC)
                        draw_magic();
                    else if (cur_tool == TOOL_STAMP)
                        draw_stamps();
                    else if (cur_tool == TOOL_TEXT)
                        draw_fonts();
                    else if (cur_tool == TOOL_SHAPES)
                        draw_shapes();
                    else if (cur_tool == TOOL_ERASER)
                        draw_erasers();
                    
                    magic_switchin(canvas);
                }
                else if (key == SDLK_s && (mod & KMOD_CTRL) && !noshortcuts)
                {
                    /* Ctrl-S - Save */
                    
                    magic_switchout(canvas);
                    
                    hide_blinking_cursor();
                    if (do_save(cur_tool, 0))
                    {
                        /* Only think it's been saved if it HAS been saved :^) */
                        
                        been_saved = 1;
                        tool_avail[TOOL_SAVE] = 0;
                    }
                    
                    /* cur_tool = old_tool; */
                    draw_toolbar();
                    update_screen_rect(&r_tools);
                    
                    magic_switchin(canvas);
                }
#ifdef __APPLE__
                else if (key == SDLK_p && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT) &&
                         !noshortcuts) {
                    /* Ctrl-Shft-P - Page Setup */
                    if (!disable_print)
                        DisplayPageSetup(canvas);
                }
#endif
                else if (key == SDLK_p && (mod & KMOD_CTRL) && !noshortcuts)
                {
                    /* Ctrl-P - Print */
//                    
//                    if (!disable_print)
//                    {
//                        /* If they haven't hit [Enter], but clicked 'Print', add their text now -bjk 2007.10.25 */
//                        if (cur_tool == TOOL_TEXT && texttool_len > 0)
//                        {
//                            rec_undo_buffer();
//                            do_render_cur_text(1);
//                            texttool_len = 0;
//                            cursor_textwidth = 0;
//                        }
//                        
//                        print_image();
//                        draw_toolbar();
//                        draw_tux_text(TUX_BORED, "", 0);
//                        update_screen_rect(&r_tools);
//                    }
                }
                else
                {
                    /* Handle key in text tool: */
                    
                    if (cur_tool == TOOL_TEXT && cursor_x != -1 && cursor_y != -1)
                    {
                        static int redraw = 0;
                        wchar_t* im_cp = im_data.s;
                        
#ifdef DEBUG
                        key_down = key;
                        key_unicode = event.key.keysym.unicode;
                        printf(
                               "character 0x%04x %d <%c> is %d pixels, %sprintable, key_down 0x%x\n",
                               (unsigned)event.key.keysym.unicode,
                               (int)event.key.keysym.unicode,
                               (key_unicode>' ' && key_unicode<127)?(char)event.key.keysym.unicode:' ',
                               (int)charsize(event.key.keysym.unicode),
                               iswprint(key_unicode)?"":"not ",
                               (unsigned)key_down
                               );
#if 0
                        /* this doesn't work for some reason */
                        wprintf(
                                L"character 0x%04x %d <%lc> is %d pixels, %lsprintable, key_down 0x%x\n",
                                event.key.keysym.unicode,
                                event.key.keysym.unicode,
                                (key_unicode>L' ')?event.key.keysym.unicode:L' ',
                                charsize(event.key.keysym.unicode),
                                iswprint(key_unicode)?L"":L"not ",
                                key_down
                                );
#endif
#endif
                        
                        /* Discard previous # of redraw characters */
                        if((int)texttool_len <= redraw) texttool_len = 0;
                        else texttool_len -= redraw;
                        texttool_str[texttool_len] = L'\0';
                        
                        /* Read IM, remember how many to redraw next iteration */
                        redraw = im_read(&im_data, event.key.keysym);
                        
                        /* Queue each character to be displayed */
                        while(*im_cp) {
                            if (*im_cp == L'\b')
                            {
                                hide_blinking_cursor();
                                if (texttool_len > 0)
                                {
                                    texttool_len--;
                                    texttool_str[texttool_len] = 0;
                                    playsound(screen, 0, SND_KEYCLICK, 0, SNDPOS_CENTER,
                                              SNDDIST_NEAR);
                                    
                                    do_render_cur_text(0);
                                }
                            }
                            else if (*im_cp == L'\r')
                            {
                                int font_height;
                                
                                hide_blinking_cursor();
                                if (texttool_len > 0)
                                {
                                    rec_undo_buffer();
                                    do_render_cur_text(1);
                                    texttool_len = 0;
                                    cursor_textwidth = 0;
                                }
                                font_height = TuxPaint_Font_FontHeight(getfonthandle(cur_font));
                                
                                cursor_x = cursor_left;
                                cursor_y = min(cursor_y + font_height, canvas->h - font_height);
                                
                                playsound(screen, 0, SND_RETURN, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
#ifdef SPEECH
#ifdef __APPLE__
                                if (use_sound)
                                    speak_string(texttool_str);
#endif
#endif
                                im_softreset(&im_data);
                            }
                            else if (*im_cp == L'\t')
                            {
                                if (texttool_len > 0)
                                {
                                    rec_undo_buffer();
                                    do_render_cur_text(1);
                                    cursor_x = min(cursor_x + cursor_textwidth, canvas->w);
                                    texttool_len = 0;
                                    cursor_textwidth = 0;
                                }
#ifdef SPEECH
#ifdef __APPLE__
                                if (use_sound)
                                    speak_string(texttool_str);
#endif
#endif
                                im_softreset(&im_data);
                            }
                            else if (iswprint(*im_cp))
                            {
                                if (texttool_len < (sizeof(texttool_str) / sizeof(wchar_t)) - 1)
                                {
                                    int old_cursor_textwidth = cursor_textwidth;
#ifdef DEBUG
                                    wprintf(L"    key = <%c>\nunicode = <%lc> 0x%04x %d\n\n",
                                            key_down, key_unicode, key_unicode, key_unicode);
#endif
                                    
                                    texttool_str[texttool_len++] = *im_cp;
                                    texttool_str[texttool_len] = 0;
                                    
                                    do_render_cur_text(0);
                                    
                                    
                                    if (cursor_x + old_cursor_textwidth <= canvas->w - 50 &&
                                        cursor_x + cursor_textwidth > canvas->w - 50)
                                    {
                                        playsound(screen, 0, SND_KEYCLICKRING, 1, SNDPOS_RIGHT,
                                                  SNDDIST_NEAR);
                                    }
                                    else
                                    {
                                        /* FIXME: Might be fun to position the
                                         sound based on keyboard layout...? */
                                        
                                        playsound(screen, 0, SND_KEYCLICK, 0, SNDPOS_CENTER,
                                                  SNDDIST_NEAR);
                                    }
                                }
                            }
                            
                            im_cp++;
                        } /* while(*im_cp) */
                        
                        /* Show IM tip text */
                        if(im_data.tip_text) {
                            draw_tux_text(TUX_DEFAULT, im_data.tip_text, 1);
                        }
                        
                    }
                }
            }
//            else if (event.type == SDL_MOUSEBUTTONDOWN &&
//                     event.button.button >= 2 &&
//                     event.button.button <= 3 &&
//                     (no_button_distinction == 0 &&
//                      !(HIT(r_tools) &&
//                        GRIDHIT_GD(r_tools, gd_tools) == TOOL_PRINT)))
//            {
//                /* They're using the middle or right mouse buttons! */
//                
//                non_left_click_count++;
//                
//                
//                if (non_left_click_count == 10 ||
//                    non_left_click_count == 20 || (non_left_click_count % 50) == 0)
//                {
//                    /* Pop up an informative animation: */
//                    
//                    hide_blinking_cursor();
//                    do_prompt_image_flash(PROMPT_TIP_LEFTCLICK_TXT,
//                                          PROMPT_TIP_LEFTCLICK_YES,
//                                          "", img_mouse, img_mouse_click, NULL, 1,
//                                          event.button.x, event.button.y);
//                    if (cur_tool == TOOL_TEXT)
//                        do_render_cur_text(0);
//                    draw_tux_text(TUX_BORED, "", 0);
//                }
//            }
            else if ((event.type == SDL_MOUSEBUTTONDOWN ||
                      event.type == TP_SDL_MOUSEBUTTONSCROLL) &&
                     event.button.button <= 3)
            {
                if (HIT(r_tools))
                {
                    /* A tool on the left has been pressed! */
                    
                    magic_switchout(canvas);
                    
                    which = GRIDHIT_GD(r_tools, gd_tools);
                    
                    if (which < NUM_TOOLS && tool_avail[which] &&
                        (valid_click(event.button.button)))
                    {
                        /* Allow middle/right-click on "Print", since [Alt]+click
                         on Mac OS X changes it from left click to middle! */
                        
                        /* Render any current text, if switching to a different
                         drawing tool: */
                        
                        if (cur_tool == TOOL_TEXT && which != TOOL_TEXT &&
                            which != TOOL_NEW && which != TOOL_OPEN &&
                            which != TOOL_SAVE)
                        {
                            if (cursor_x != -1 && cursor_y != -1)
                            {
                                hide_blinking_cursor();
                                if (texttool_len > 0)
                                {
                                    rec_undo_buffer();
                                    do_render_cur_text(1);
                                    texttool_len = 0;
                                    cursor_textwidth = 0;
                                }
                            }
                        }
                        
                        old_tool = cur_tool;
                        cur_tool = which;
                        draw_toolbar();
                        update_screen_rect(&r_tools);
                        
                        playsound(screen, 1, SND_CLICK, 0, SNDPOS_LEFT, SNDDIST_NEAR);
                        
                        /* FIXME: this "if" is just plain gross */
                        if (cur_tool != TOOL_TEXT)
                            draw_tux_text(tool_tux[cur_tool], tool_tips[cur_tool], 1);
                        
                        /* Draw items for this tool: */
                        
                        if (cur_tool == TOOL_BRUSH)
                        {
                            cur_thing = cur_brush;
                            num_things = num_brushes;
                            thing_scroll = &brush_scroll;
                            draw_brushes();
                            draw_colors(COLORSEL_ENABLE);
                        }
                        else if (cur_tool == TOOL_STAMP)
                        {
                            cur_thing = cur_stamp[stamp_group];
                            num_things = num_stamps[stamp_group];
                            thing_scroll = &(stamp_scroll[stamp_group]);
                            draw_stamps();
                            draw_colors(stamp_colorable(cur_stamp[stamp_group]) ||
                                        stamp_tintable(cur_stamp[stamp_group]));
                            set_active_stamp();
                            update_stamp_xor();
                        }
                        else if (cur_tool == TOOL_LINES)
                        {
                            cur_thing = cur_brush;
                            num_things = num_brushes;
                            thing_scroll = &brush_scroll;
                            draw_brushes();
                            draw_colors(COLORSEL_ENABLE);
                        }
                        else if (cur_tool == TOOL_SHAPES)
                        {
                            cur_thing = cur_shape;
                            num_things = NUM_SHAPES;
                            thing_scroll = &shape_scroll;
                            draw_shapes();
                            draw_colors(COLORSEL_ENABLE);
                            shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                        }
                        else if (cur_tool == TOOL_TEXT)
                        {
                            if (!font_thread_done)
                            {
                                draw_colors(COLORSEL_DISABLE);
                                draw_none();
                                update_screen_rect(&r_toolopt);
                                update_screen_rect(&r_ttoolopt);
                                do_setcursor(cursor_watch);
                                
                                /* Wait while Text tool finishes loading fonts */
                                draw_tux_text(TUX_WAIT, gettext("Please wait…"), 1);
                                
                                waiting_for_fonts = 1;
#ifdef FORKED_FONTS
                                receive_some_font_info(screen);
#else
                                while (!font_thread_done && !font_thread_aborted)
                                {
                                    /* FIXME: should have a read-depends memory barrier around here */
                                    show_progress_bar(screen);
                                    SDL_Delay(20);
                                }
                                /* FIXME: should kill this in any case */
                                SDL_WaitThread(font_thread, NULL);
#endif
                                do_setcursor(cursor_arrow);
                            }
                            draw_tux_text(tool_tux[cur_tool], tool_tips[cur_tool], 1);
                            
                            if (num_font_families > 0)
                            {
                                cur_thing = cur_font;
                                num_things = num_font_families;
                                thing_scroll = &font_scroll;
                                draw_fonts();
                                draw_colors(COLORSEL_ENABLE);
                            }
                            else
                            {
                                /* Problem using fonts! */
                                
                                cur_tool = old_tool;
                                draw_toolbar();
                                update_screen_rect(&r_tools);
                            }
                        }
                        else if (cur_tool == TOOL_MAGIC)
                        {
                            cur_thing = cur_magic;
                            num_things = num_magics;
                            thing_scroll = &magic_scroll;
                            magic_current_snd_ptr = NULL;
                            draw_magic();
                            draw_colors(magics[cur_magic].colors);
                            
                            if (magics[cur_magic].colors)
                                magic_funcs[magics[cur_magic].handle_idx].set_color(
                                                                                    magic_api_struct,
                                                                                    color_hexes[cur_color][0],
                                                                                    color_hexes[cur_color][1],
                                                                                    color_hexes[cur_color][2]);
                        }
                        else if (cur_tool == TOOL_ERASER)
                        {
                            cur_thing = cur_eraser;
                            num_things = NUM_ERASERS;
                            thing_scroll = &eraser_scroll;
                            draw_erasers();
                            draw_colors(COLORSEL_DISABLE);
                        }
                        else if (cur_tool == TOOL_UNDO)
                        {
                            if (cur_undo == newest_undo)
                            {
                                rec_undo_buffer();
                                do_undo();
                            }
                            do_undo();
                            
                            been_saved = 0;
                            
                            if (!disable_save)
                                tool_avail[TOOL_SAVE] = 1;
                            
                            cur_tool = old_tool;
                            draw_toolbar();
                            update_screen_rect(&r_tools);
                            shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                        }
                        else if (cur_tool == TOOL_REDO)
                        {
                            do_redo();
                            
                            been_saved = 0;
                            
                            if (!disable_save)
                                tool_avail[TOOL_SAVE] = 1;
                            
                            cur_tool = old_tool;
                            draw_toolbar();
                            update_screen_rect(&r_tools);
                            shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                        }
                        else if (cur_tool == TOOL_OPEN)
                        {
                            disable_avail_tools();
                            draw_toolbar();
                            draw_colors(COLORSEL_CLOBBER_WIPE);
                            draw_none();
                            
                            if (do_open() == 0)
                            {
                                if (old_tool == TOOL_TEXT)
                                    do_render_cur_text(0);
                            }
                            
                            enable_avail_tools();
                            
                            cur_tool = old_tool;
                            draw_toolbar();
                            update_screen_rect(&r_tools);
                            
                            draw_tux_text(TUX_GREAT, tool_tips[cur_tool], 1);
                            
                            draw_colors(COLORSEL_REFRESH);
                            
                            if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_LINES)
                                draw_brushes();
                            else if (cur_tool == TOOL_MAGIC)
                                draw_magic();
                            else if (cur_tool == TOOL_STAMP)
                                draw_stamps();
                            else if (cur_tool == TOOL_TEXT)
                                draw_fonts();
                            else if (cur_tool == TOOL_SHAPES)
                                draw_shapes();
                            else if (cur_tool == TOOL_ERASER)
                                draw_erasers();
                        }
                        else if (cur_tool == TOOL_SAVE)
                        {
                            if (do_save(old_tool, 0))
                            {
                                been_saved = 1;
                                tool_avail[TOOL_SAVE] = 0;
                            }
                            
                            cur_tool = old_tool;
                            draw_toolbar();
                            update_screen_rect(&r_tools);
                        }
                        else if (cur_tool == TOOL_NEW)
                        {
                            shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                            
                            disable_avail_tools();
                            draw_toolbar();
                            draw_colors(COLORSEL_CLOBBER_WIPE);
                            draw_none();
                            
                            if (do_new_dialog() == 0)
                            {
                                cur_tool = old_tool;
                                
                                draw_tux_text(tool_tux[TUX_DEFAULT], TIP_NEW_ABORT, 1);
                                
                                if (cur_tool == TOOL_TEXT)
                                    do_render_cur_text(0);
                            }
                            
                            cur_tool = old_tool;
                            
                            enable_avail_tools();
                            
                            draw_toolbar();
                            update_screen_rect(&r_tools);
                            draw_colors(COLORSEL_REFRESH);
                            
                            if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_LINES)
                                draw_brushes();
                            else if (cur_tool == TOOL_MAGIC)
                                draw_magic();
                            else if (cur_tool == TOOL_STAMP)
                                draw_stamps();
                            else if (cur_tool == TOOL_TEXT)
                                draw_fonts();
                            else if (cur_tool == TOOL_SHAPES)
                                draw_shapes();
                            else if (cur_tool == TOOL_ERASER)
                                draw_erasers();
                        }
                        else if (cur_tool == TOOL_PRINT)
                        {
                            /* If they haven't hit [Enter], but clicked 'Print', add their text now -bjk 2007.10.25 */
                            if (old_tool == TOOL_TEXT && texttool_len > 0)
                            {
                                rec_undo_buffer();
                                do_render_cur_text(1);
                                texttool_len = 0;
                                cursor_textwidth = 0;
                            }
                            
                            /* original print code was here */
                            SurfaceSaveToPHOTO(canvas);
                            cur_tool = old_tool;
                            draw_toolbar();
                            draw_tux_text(TUX_BORED, "Your image has been saved to the system album", 0);
                            update_screen_rect(&r_tools);
                        }
//                        else if (cur_tool == TOOL_QUIT)
//                        {
//                            done = do_quit(old_tool);
//                            cur_tool = old_tool;
//                            draw_toolbar();
//                            update_screen_rect(&r_tools);
//                        }
                        update_screen_rect(&r_toolopt);
                        update_screen_rect(&r_ttoolopt);
                    }
                    
                    if (!done)
                        magic_switchin(canvas);
                }
                else if (HIT(r_toolopt) && valid_click(event.button.button))
                {
                    /* Options on the right
                     WARNING: this must be kept in sync with the mouse-move
                     code (for cursor changes) and mouse-scroll code. */
                    
                    if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_STAMP ||
                        cur_tool == TOOL_SHAPES || cur_tool == TOOL_LINES ||
                        cur_tool == TOOL_MAGIC || cur_tool == TOOL_TEXT ||
                        cur_tool == TOOL_ERASER)
                    {
                        int num_rows_needed;
                        SDL_Rect r_controls;
                        SDL_Rect r_notcontrols;
                        SDL_Rect r_items;	/* = r_notcontrols; */
                        int toolopt_changed;
                        
                        grid_dims gd_controls = { 0, 0 };	/* might become 2-by-2 */
                        grid_dims gd_items = { 2, 2 };	/* generally becoming 2-by-whatever */
                        
                        /* Note set of things we're dealing with */
                        /* (stamps, brushes, etc.) */
                        
                        if (cur_tool == TOOL_STAMP)
                        {
                            if (!disable_stamp_controls)
                                gd_controls = (grid_dims)
                            {
                                3, 2}; /* was 2,2 before adding left/right stamp group buttons -bjk 2007.05.15 */
                            else
                                gd_controls = (grid_dims)
                            {
                                1, 2};  /* was left 0,0 before adding left/right stamp group buttons -bjk 2007.05.03 */
                        }
                        else if (cur_tool == TOOL_TEXT)
                        {
                            if (!disable_stamp_controls)
                                gd_controls = (grid_dims)
                            {
                                2, 2};
                        }
                        else if (cur_tool == TOOL_MAGIC)
                        {
                            if (!disable_magic_controls)
                                gd_controls = (grid_dims)
                            {
                                1, 2};
                        }
                        
                        /* number of whole or partial rows that will be needed
                         (can make this per-tool if variable columns needed) */
                        num_rows_needed =
                        (num_things + gd_items.cols - 1) / gd_items.cols;
                        
                        do_draw = 0;
                        
                        r_controls.w = r_toolopt.w;
                        r_controls.h = gd_controls.rows * button_h;
                        r_controls.x = r_toolopt.x;
                        r_controls.y = r_toolopt.y + r_toolopt.h - r_controls.h -48;
                        
                        r_notcontrols.w = r_toolopt.w;
                        r_notcontrols.h = r_toolopt.h - r_controls.h;
                        r_notcontrols.x = r_toolopt.x;
                        r_notcontrols.y = r_toolopt.y;
                        
                        r_items.x = r_notcontrols.x;
                        r_items.y = r_notcontrols.y;
                        r_items.w = r_notcontrols.w;
                        r_items.h = r_notcontrols.h-48;
                        
                        if (num_rows_needed * button_h > r_items.h)
                        {
                            /* too many; we'll need scroll buttons */
                            r_items.h -= button_h;
                            r_items.y += button_h / 2;
                        }
                        gd_items.rows = r_items.h / button_h;
                        toolopt_changed = 0;
                        
                        if (HIT(r_items))
                        {
                            which = GRIDHIT_GD(r_items, gd_items) + *thing_scroll;
                            
                            if (which < num_things)
                            {
                                toolopt_changed = 1;
#ifndef NOSOUND
                                if (cur_tool != TOOL_STAMP || stamp_data[stamp_group][which]->ssnd == NULL)
                                {
                                    playsound(screen, 1, SND_BLEEP, 0, SNDPOS_RIGHT,
                                              SNDDIST_NEAR);
                                }
#endif
                                cur_thing = which;
                                do_draw = 1;
                            }
                        }
                        else if (HIT(r_controls))
                        {
                            which = GRIDHIT_GD(r_controls, gd_controls);
                            if (cur_tool == TOOL_STAMP)
                            {
                                /* Stamp controls! */
                                int control_sound = -1;
                                
                                if (which == 4 || which == 5)
                                {
                                    /* Grow/Shrink Controls: */
#ifdef OLD_STAMP_GROW_SHRINK
                                    if (which == 5)
                                    {
                                        /* Bottom right button: Grow: */
                                        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->size < MAX_STAMP_SIZE)
                                        {
                                            stamp_data[stamp_group][cur_stamp[stamp_group]]->size++;
                                            control_sound = SND_GROW;
                                        }
                                    }
                                    else
                                    {
                                        /* Bottom left button: Shrink: */
                                        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->size > MIN_STAMP_SIZE)
                                        {
                                            stamp_data[stamp_group][cur_stamp[stamp_group]]->size--;
                                            control_sound = SND_SHRINK;
                                        }
                                    }
#else
                                    int old_size;
                                    
                                    old_size = stamp_data[stamp_group][cur_stamp[stamp_group]]->size;
                                    
                                    stamp_data[stamp_group][cur_stamp[stamp_group]]->size =
                                    (((MAX_STAMP_SIZE - MIN_STAMP_SIZE) * (event.button.x -
                                                                           (WINDOW_WIDTH -
                                                                            96))) / 96) +
                                    MIN_STAMP_SIZE;
                                    
                                    if (stamp_data[stamp_group][cur_stamp[stamp_group]]->size < old_size)
                                        control_sound = SND_SHRINK;
                                    else if (stamp_data[stamp_group][cur_stamp[stamp_group]]->size > old_size)
                                        control_sound = SND_GROW;
#endif
                                }
                                else if (which == 2 || which == 3)
                                {
                                    /* Mirror/Flip Controls: */
                                    if (which == 3)
                                    {
                                        /* Top right button: Flip: */
                                        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->flipable)
                                        {
                                            stamp_data[stamp_group][cur_stamp[stamp_group]]->flipped =
                                            !stamp_data[stamp_group][cur_stamp[stamp_group]]->flipped;
                                            control_sound = SND_FLIP;
                                        }
                                    }
                                    else
                                    {
                                        /* Top left button: Mirror: */
                                        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->mirrorable)
                                        {
                                            stamp_data[stamp_group][cur_stamp[stamp_group]]->mirrored =
                                            !stamp_data[stamp_group][cur_stamp[stamp_group]]->mirrored;
                                            control_sound = SND_MIRROR;
                                        }
                                    }
                                }
                                else
                                {
                                    /* Prev/Next Controls: */
                                    
                                    old_stamp_group = stamp_group;
                                    
                                    if (which == 1)
                                    {
                                        /* Next group */
                                        stamp_group++;
                                        if (stamp_group >= num_stamp_groups)
                                            stamp_group = 0;
                                        control_sound = SND_CLICK;
                                    }
                                    else
                                    {
                                        /* Prev group */
                                        stamp_group--;
                                        if (stamp_group < 0)
                                            stamp_group = num_stamp_groups - 1;
                                        control_sound = SND_CLICK;
                                    }
                                    
                                    if (stamp_group == old_stamp_group)
                                        control_sound = -1;
                                    else
                                    {
                                        cur_thing = cur_stamp[stamp_group];
                                        num_things = num_stamps[stamp_group];
                                        thing_scroll = &(stamp_scroll[stamp_group]);
                                    }
                                }
                                
                                if (control_sound != -1)
                                {
                                    playsound(screen, 0, control_sound, 0, SNDPOS_CENTER,
                                              SNDDIST_NEAR);
                                    draw_stamps();
                                    update_screen_rect(&r_toolopt);
                                    set_active_stamp();
                                    update_stamp_xor();
                                }
                            }
                            else if (cur_tool == TOOL_MAGIC)
                            {
                                /* Magic controls! */
                                if (which == 1 && magics[cur_magic].avail_modes & MODE_FULLSCREEN)
                                {
                                    magic_switchout(canvas);
                                    magics[cur_magic].mode = MODE_FULLSCREEN;
                                    magic_switchin(canvas);
                                    draw_magic();
                                    update_screen_rect(&r_toolopt);
                                }
                                else if (which == 0 && magics[cur_magic].avail_modes & MODE_PAINT)
                                {
                                    magic_switchout(canvas);
                                    magics[cur_magic].mode = MODE_PAINT;
                                    magic_switchin(canvas);
                                    draw_magic();
                                    update_screen_rect(&r_toolopt);
                                }
                                /* FIXME: Sfx */
                            }
                            else if (cur_tool == TOOL_TEXT)
                            {
                                /* Text controls! */
                                int control_sound = -1;
                                if (which & 2)
                                {
                                    /* One of the bottom buttons: */
                                    if (which & 1)
                                    {
                                        /* Bottom right button: Grow: */
                                        if (text_size < MAX_TEXT_SIZE)
                                        {
                                            text_size++;
                                            control_sound = SND_GROW;
                                            toolopt_changed = 1;
                                        }
                                    }
                                    else
                                    {
                                        /* Bottom left button: Shrink: */
                                        if (text_size > MIN_TEXT_SIZE)
                                        {
                                            text_size--;
                                            control_sound = SND_SHRINK;
                                            toolopt_changed = 1;
                                        }
                                    }
                                }
                                else
                                {
                                    /* One of the top buttons: */
                                    if (which & 1)
                                    {
                                        /* Top right button: Italic: */
                                        if (text_state & TTF_STYLE_ITALIC)
                                        {
                                            text_state &= ~TTF_STYLE_ITALIC;
                                            control_sound = SND_ITALIC_ON;
                                        }
                                        else
                                        {
                                            text_state |= TTF_STYLE_ITALIC;
                                            control_sound = SND_ITALIC_OFF;
                                        }
                                    }
                                    else
                                    {
                                        /* Top left button: Bold: */
                                        if (text_state & TTF_STYLE_BOLD)
                                        {
                                            text_state &= ~TTF_STYLE_BOLD;
                                            control_sound = SND_THIN;
                                        }
                                        else
                                        {
                                            text_state |= TTF_STYLE_BOLD;
                                            control_sound = SND_THICK;
                                        }
                                    }
                                    toolopt_changed = 1;
                                }
                                
                                
                                if (control_sound != -1)
                                {
                                    playsound(screen, 0, control_sound, 0, SNDPOS_CENTER,
                                              SNDDIST_NEAR);
                                    
                                    
                                    if (cur_tool == TOOL_TEXT)	/* Huh? It had better be! */
                                    {
                                        /* need to invalidate all the cached user fonts, causing reload on demand */
                                        
                                        int i;
                                        for (i = 0; i < num_font_families; i++)
                                        {
                                            if (user_font_families[i]
                                                && user_font_families[i]->handle)
                                            {
                                                TuxPaint_Font_CloseFont(user_font_families[i]->handle);
                                                user_font_families[i]->handle = NULL;
                                            }
                                        }
                                        draw_fonts();
                                        update_screen_rect(&r_toolopt);
                                    }
                                }
                            }
                        }
                        else
                        {
                            /* scroll button */
                            int is_upper = event.button.y < r_toolopt.y + button_h / 2;
                            if ((is_upper && *thing_scroll > 0)	/* upper arrow */
                                || (!is_upper && *thing_scroll / gd_items.cols < num_rows_needed - gd_items.rows)	/* lower arrow */
                                )
                            {
                                *thing_scroll += is_upper ? -gd_items.cols : gd_items.cols;
                                do_draw = 1;
                                playsound(screen, 1, SND_SCROLL, 1, SNDPOS_RIGHT,
                                          SNDDIST_NEAR);
                                
                                if (scrolltimer != NULL)
                                {
                                    SDL_RemoveTimer(scrolltimer);
                                    scrolltimer = NULL;
                                }
                                
                                if (!scrolling && event.type == SDL_MOUSEBUTTONDOWN)
                                {
                                    /* printf("Starting scrolling\n"); */
                                    memcpy(&scrolltimer_event, &event, sizeof(SDL_Event));
                                    scrolltimer_event.type = TP_SDL_MOUSEBUTTONSCROLL;
                                    
                                    scrolling = 1;
                                    
                                    scrolltimer =
                                    SDL_AddTimer(REPEAT_SPEED, scrolltimer_callback,
                                                 (void *) &scrolltimer_event);
                                }
                                else
                                {
                                    /* printf("Continuing scrolling\n"); */
                                    scrolltimer =
                                    SDL_AddTimer(REPEAT_SPEED / 3, scrolltimer_callback,
                                                 (void *) &scrolltimer_event);
                                }
                                
                                if (*thing_scroll == 0)
                                {
                                    do_setcursor(cursor_arrow);
                                    if (scrolling)
                                    {
                                        if (scrolltimer != NULL)
                                        {
                                            SDL_RemoveTimer(scrolltimer);
                                            scrolltimer = NULL;
                                        }
                                        scrolling = 0;
                                    }
                                }
                            }
                        }
                        
                        
                        /* Assign the change(s), if any / redraw, if needed: */
                        
                        if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_LINES)
                        {
                            cur_brush = cur_thing;
                            render_brush();
                            
                            if (do_draw)
                                draw_brushes();
                        }
                        else if (cur_tool == TOOL_ERASER)
                        {
                            cur_eraser = cur_thing;
                            
                            if (do_draw)
                                draw_erasers();
                        }
                        else if (cur_tool == TOOL_TEXT)
                        {
                            /* FIXME */ /* char font_tux_text[512]; */
                            
                            cur_font = cur_thing;
                            
                            /* FIXME */
                            /*
                             snprintf(font_tux_text, sizeof font_tux_text, "%s (%s).",
                             TTF_FontFaceFamilyName(getfonthandle(cur_font)),
                             TTF_FontFaceStyleName(getfonthandle(cur_font)));
                             draw_tux_text(TUX_GREAT, font_tux_text, 1);
                             */
                            
                            if (do_draw)
                                draw_fonts();
                            
                            
                            /* Only rerender when picking a different font */
                            if (toolopt_changed)
                                do_render_cur_text(0);
                        }
                        else if (cur_tool == TOOL_STAMP)
                        {
#ifndef NOSOUND
                            /* Only play when picking a different stamp */
                            if (toolopt_changed && !mute)
                            {
                                /* If there's an SFX, play it! */
                                
                                if (stamp_data[stamp_group][cur_thing]->ssnd != NULL)
                                {
                                    Mix_ChannelFinished(NULL);	/* Prevents multiple clicks from toggling between SFX and desc sound, rather than always playing SFX first, then desc sound... */
                                    
                                    Mix_PlayChannel(2, stamp_data[stamp_group][cur_thing]->ssnd, 0);
                                    
                                    /* If there's a description sound, play it after the SFX! */
                                    
                                    if (stamp_data[stamp_group][cur_thing]->sdesc != NULL)
                                    {
                                        Mix_ChannelFinished(playstampdesc);
                                    }
                                }
                                else
                                {
                                    /* No SFX?  If there's a description sound, play it now! */
                                    
                                    if (stamp_data[stamp_group][cur_thing]->sdesc != NULL)
                                    {
                                        Mix_PlayChannel(2, stamp_data[stamp_group][cur_thing]->sdesc, 0);
                                    }
                                }
                            }
#endif
                            
                            if (cur_thing != cur_stamp[stamp_group])
                            {
                                cur_stamp[stamp_group] = cur_thing;
                                set_active_stamp();
                                update_stamp_xor();
                            }
                            
//                            if (do_draw)
                                draw_stamps();
                            
                            if (stamp_data[stamp_group][cur_stamp[stamp_group]]->stxt != NULL)
                            {
#ifdef DEBUG
                                printf("stamp_data[stamp_group][cur_stamp[stamp_group]]->stxt = %s\n",
                                       stamp_data[stamp_group][cur_stamp[stamp_group]]->stxt);
#endif
                                
                                draw_tux_text_ex(TUX_GREAT, stamp_data[stamp_group][cur_stamp[stamp_group]]->stxt, 1, stamp_data[stamp_group][cur_stamp[stamp_group]]->locale_text);
                            }
                            else
                                draw_tux_text(TUX_GREAT, "", 0);
                            
                            /* Enable or disable color selector: */
                            draw_colors(stamp_colorable(cur_stamp[stamp_group])
                                        || stamp_tintable(cur_stamp[stamp_group]));
                        }
                        else if (cur_tool == TOOL_SHAPES)
                        {
                            cur_shape = cur_thing;
                            
                            draw_tux_text(TUX_GREAT, shape_tips[cur_shape], 1);
                            
                            if (do_draw)
                                draw_shapes();
                        }
                        else if (cur_tool == TOOL_MAGIC)
                        {
                            if (cur_thing != cur_magic)
                            {
                                magic_switchout(canvas);
                                
                                cur_magic = cur_thing;
                                draw_colors(magics[cur_magic].colors);
                                
                                if (magics[cur_magic].colors)
                                    magic_funcs[magics[cur_magic].handle_idx].set_color(
                                                                                        magic_api_struct,
                                                                                        color_hexes[cur_color][0],
                                                                                        color_hexes[cur_color][1],
                                                                                        color_hexes[cur_color][2]);
                                
                                magic_switchin(canvas);
                            }
                            
                            draw_tux_text(TUX_GREAT, magics[cur_magic].tip[magic_modeint(magics[cur_magic].mode)], 1);
                            
                            if (do_draw)
                                draw_magic();
                        }
                        
                        /* Update the screen: */
                        if (do_draw)
                            update_screen_rect(&r_toolopt);
                    }
                }
                else if (HIT(r_colors) && colors_are_selectable &&
                         valid_click(event.button.button))
                {
                    /* Color! */
                    which = GRIDHIT_GD(r_colors, gd_colors);
                    
                    if (which >= 0 && which < NUM_COLORS)
                    {
                        cur_color = which;
                        draw_tux_text(TUX_KISS, color_names[cur_color], 1);
                        
                        if (cur_color == (unsigned) (NUM_COLORS - 1))
                        {
                            disable_avail_tools();
                            draw_toolbar();
                            draw_colors(COLORSEL_CLOBBER_WIPE);
                            draw_none();
                            
                            
                            do_color_picker();
                            
                            
                            enable_avail_tools();
                            draw_toolbar();
                            update_screen_rect(&r_tools);
                            
                            draw_tux_text(TUX_GREAT, tool_tips[cur_tool], 1);
                            
                            draw_colors(COLORSEL_FORCE_REDRAW);
                            
                            if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_LINES)
                                draw_brushes();
                            else if (cur_tool == TOOL_MAGIC)
                                draw_magic();
                            else if (cur_tool == TOOL_STAMP)
                                draw_stamps();
                            else if (cur_tool == TOOL_TEXT)
                                draw_fonts();
                            else if (cur_tool == TOOL_SHAPES)
                                draw_shapes();
                            else if (cur_tool == TOOL_ERASER)
                                draw_erasers();
                            
                            playsound(screen, 1, SND_BUBBLE, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                            
                            SDL_Flip(screen);
                        }
                        else
                        {
                            draw_colors(COLORSEL_REFRESH);
                            
                            playsound(screen, 1, SND_BUBBLE, 1, event.button.x, SNDDIST_NEAR);
                        }
                        
                        render_brush();
                        
                        
                        if (cur_tool == TOOL_TEXT)
                            do_render_cur_text(0);
                        else if (cur_tool == TOOL_MAGIC)
                            magic_funcs[magics[cur_magic].handle_idx].set_color(
                                                                                magic_api_struct,
                                                                                color_hexes[cur_color][0],
                                                                                color_hexes[cur_color][1],
                                                                                color_hexes[cur_color][2]);
                    }
                }
                else if (HIT(r_canvas) && valid_click(event.button.button))
                {
                    /* Draw something! */
                    
                    old_x = event.button.x - r_canvas.x;
                    old_y = event.button.y - r_canvas.y;
                    
                    if (been_saved)
                    {
                        been_saved = 0;
                        
                        if (!disable_save)
                            tool_avail[TOOL_SAVE] = 1;
                        
                        draw_toolbar();
                        update_screen_rect(&r_tools);
                    }
                    
                    if (cur_tool == TOOL_BRUSH)
                    {
                        /* Start painting! */
                        
                        rec_undo_buffer();
                        
                        /* (Arbitrarily large, so we draw once now) */
                        reset_brush_counter();
                        
                        /* brush_draw(old_x, old_y, old_x, old_y, 1); fixes SF #1934883? */
                        playsound(screen, 0, paintsound(img_cur_brush_w), 1,
                                  event.button.x, SNDDIST_NEAR);
                    }
                    else if (cur_tool == TOOL_STAMP)
                    {
                        /* Draw a stamp! */
                        
                        rec_undo_buffer();
                        
                        stamp_draw(old_x, old_y);
                        stamp_xor(old_x, old_y);
                        playsound(screen, 1, SND_STAMP, 1, event.button.x, SNDDIST_NEAR);
                        
                        draw_tux_text(TUX_GREAT, great_str(), 1);
                        
                        /* FIXME: Make delay configurable: */
                        
                        control_drawtext_timer(1000, stamp_data[stamp_group][cur_stamp[stamp_group]]->stxt, stamp_data[stamp_group][cur_stamp[stamp_group]]->locale_text);
                    }
                    else if (cur_tool == TOOL_LINES)
                    {
                        /* Start a line! */
                        
                        rec_undo_buffer();
                        
                        line_start_x = old_x;
                        line_start_y = old_y;
                        
                        /* (Arbitrarily large, so we draw once now) */
                        reset_brush_counter();
                        
                        /* brush_draw(old_x, old_y, old_x, old_y, 1); fixes sf #1934883? */
                        
                        playsound(screen, 1, SND_LINE_START, 1, event.button.x,
                                  SNDDIST_NEAR);
                        draw_tux_text(TUX_BORED, TIP_LINE_START, 1);
                    }
                    else if (cur_tool == TOOL_SHAPES)
                    {
                        if (shape_tool_mode == SHAPE_TOOL_MODE_DONE)
                        {
                            /* Start drawing a shape! */
                            
                            rec_undo_buffer();
                            
                            shape_ctr_x = old_x;
                            shape_ctr_y = old_y;
                            
                            shape_tool_mode = SHAPE_TOOL_MODE_STRETCH;
                            
                            playsound(screen, 1, SND_LINE_START, 1, event.button.x,
                                      SNDDIST_NEAR);
                            draw_tux_text(TUX_BORED, TIP_SHAPE_START, 1);
                        }
                        else if (shape_tool_mode == SHAPE_TOOL_MODE_ROTATE)
                        {
                            /* Draw the shape with the brush! */
                            
                            /* (Arbitrarily large...) */
                            reset_brush_counter();
                            
                            playsound(screen, 1, SND_LINE_END, 1, event.button.x,
                                      SNDDIST_NEAR);
                            do_shape(shape_ctr_x, shape_ctr_y, shape_outer_x, shape_outer_y,
                                     rotation(shape_ctr_x, shape_ctr_y,
                                              event.button.x - r_canvas.x,
                                              event.button.y - r_canvas.y), 1);
                            
                            shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                            draw_tux_text(TUX_GREAT, tool_tips[TOOL_SHAPES], 1);
                        }
                    }
                    else if (cur_tool == TOOL_MAGIC)
                    {
                        int undo_ctr;
                        SDL_Surface * last;
                        
                        
                        /* Start doing magic! */
                        
                        rec_undo_buffer();
                        
                        if (cur_undo > 0)
                            undo_ctr = cur_undo - 1;
                        else
                            undo_ctr = NUM_UNDO_BUFS - 1;
                        
                        last = undo_bufs[undo_ctr];
                        
                        update_rect.x = 0;
                        update_rect.y = 0;
                        update_rect.w = 0;
                        update_rect.h = 0;
                        
                        reset_touched();
                        
                        magic_funcs[magics[cur_magic].handle_idx].click(magic_api_struct,
                                                                        magics[cur_magic].idx,
                                                                        magics[cur_magic].mode,
                                                                        canvas, last,
                                                                        old_x, old_y,
                                                                        &update_rect);
                        
                        draw_tux_text(TUX_GREAT, magics[cur_magic].tip[magic_modeint(magics[cur_magic].mode)], 1);
                        
                        update_canvas(update_rect.x, update_rect.y,
                                      update_rect.x + update_rect.w,
                                      update_rect.y + update_rect.h);
                    }
                    else if (cur_tool == TOOL_ERASER)
                    {
                        /* Erase! */
                        
                        rec_undo_buffer();
                        
                        do_eraser(old_x, old_y);
                    }
                    else if (cur_tool == TOOL_TEXT)
                    {
#ifdef __IPHONEOS__
                        extern SDL_Window *SDL_VideoWindow;
                        extern DECLSPEC int SDLCALL SDL_iPhoneKeyboardShow(SDL_Window *windowID);
                        extern DECLSPEC int SDLCALL SDL_iPhoneKeyboardHide(SDL_Window *windowID);
#endif
                        
                        /* Text Tool! */
                        
                        if (cursor_x != -1 && cursor_y != -1)
                        {
                            /*
                             if (texttool_len > 0)
                             {
                             rec_undo_buffer();
                             do_render_cur_text(1);
                             texttool_len = 0;
                             }
                             */
                        }
                        
                        cursor_x = old_x;
                        cursor_y = old_y;
                        cursor_left = old_x;
#ifdef __IPHONEOS__
                        SDL_iPhoneKeyboardShow(SDL_VideoWindow);
#endif
                        do_render_cur_text(0);
                    }
                    
                    button_down = 1;
                }
                else if (HIT(r_sfx) && valid_click(event.button.button))
                {
                    /* A sound player button on the lower left has been pressed! */
                    
#ifndef NOSOUND
                    if (cur_tool == TOOL_STAMP && use_sound && !mute)
                    {
                        which = GRIDHIT_GD(r_sfx, gd_sfx);
                        
                        if (which == 0 &&
                            !stamp_data[stamp_group][cur_stamp[stamp_group]]->no_sound)
                        {
                            /* Re-play sound effect: */
                            
                            Mix_ChannelFinished(NULL);
                            Mix_PlayChannel(2, stamp_data[stamp_group][cur_thing]->ssnd, 0);
                        }
                        else if (which == 1 &&
                                 !stamp_data[stamp_group][cur_stamp[stamp_group]]->no_descsound)
                        {
                            Mix_ChannelFinished(NULL);
                            Mix_PlayChannel(2, stamp_data[stamp_group][cur_thing]->sdesc, 0);
                        }
                    }
#endif
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN &&
                     wheely && event.button.button >= 4 && event.button.button <= 5)
            {
                int num_rows_needed;
                SDL_Rect r_controls;
                SDL_Rect r_notcontrols;
                SDL_Rect r_items;	/* = r_notcontrols; */
                
                /* Scroll wheel code.
                 WARNING: this must be kept in sync with the mouse-move
                 code (for cursor changes) and mouse-click code. */
                
                if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_STAMP ||
                    cur_tool == TOOL_SHAPES || cur_tool == TOOL_LINES ||
                    cur_tool == TOOL_MAGIC || cur_tool == TOOL_TEXT ||
                    cur_tool == TOOL_ERASER)
                {
                    grid_dims gd_controls = { 0, 0 };	/* might become 2-by-2 */
                    grid_dims gd_items = { 2, 2 };	/* generally becoming 2-by-whatever */
                    
                    /* Note set of things we're dealing with */
                    /* (stamps, brushes, etc.) */
                    
                    if (cur_tool == TOOL_STAMP)
                    {
                        if (!disable_stamp_controls)
                            gd_controls = (grid_dims)
                        {
                            3, 2}; /* was 2,2 before adding left/right stamp group buttons -bjk 2007.05.15 */
                        else
                            gd_controls = (grid_dims)
                        {
                            1, 2};  /* was left 0,0 before adding left/right stamp group buttons -bjk 2007.05.03 */
                    }
                    else if (cur_tool == TOOL_TEXT)
                    {
                        if (!disable_stamp_controls)
                            gd_controls = (grid_dims)
                        {
                            2, 2};
                    }
                    else if (cur_tool == TOOL_MAGIC)
                    {
                        if (!disable_magic_controls)
                            gd_controls = (grid_dims)
                        {
                            1, 2};
                    }
                    
                    /* number of whole or partial rows that will be needed
                     (can make this per-tool if variable columns needed) */
                    num_rows_needed = (num_things + gd_items.cols - 1) / gd_items.cols;
                    
                    do_draw = 0;
                    
                    r_controls.w = r_toolopt.w;
                    r_controls.h = gd_controls.rows * button_h;
                    r_controls.x = r_toolopt.x;
                    r_controls.y = r_toolopt.y + r_toolopt.h - r_controls.h;
                    
                    r_notcontrols.w = r_toolopt.w;
                    r_notcontrols.h = r_toolopt.h - r_controls.h;
                    r_notcontrols.x = r_toolopt.x;
                    r_notcontrols.y = r_toolopt.y;
                    
                    r_items.x = r_notcontrols.x;
                    r_items.y = r_notcontrols.y;
                    r_items.w = r_notcontrols.w;
                    r_items.h = r_notcontrols.h;
                    
                    if (num_rows_needed * button_h > r_items.h)
                    {
                        /* too many; we'll need scroll buttons */
                        r_items.h -= button_h;
                        r_items.y += button_h / 2;
                    }
                    gd_items.rows = r_items.h / button_h;
                    
                    if (0)
                    {
                    }
                    else
                    {
                        /* scroll button */
                        int is_upper = (event.button.button == 4);
                        if ((is_upper && *thing_scroll > 0)	/* upper arrow */
                            || (!is_upper && *thing_scroll / gd_items.cols < num_rows_needed - gd_items.rows)	/* lower arrow */
                            )
                        {
                            *thing_scroll += is_upper ? -gd_items.cols : gd_items.cols;
                            do_draw = 1;
                            playsound(screen, 1, SND_SCROLL, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
                            if (*thing_scroll == 0)
                            {
                                do_setcursor(cursor_arrow);
                            }
                        }
                    }
                    
                    
                    /* Assign the change(s), if any / redraw, if needed: */
                    
                    if (cur_tool == TOOL_BRUSH || cur_tool == TOOL_LINES)
                    {
                        if (do_draw)
                            draw_brushes();
                    }
                    else if (cur_tool == TOOL_ERASER)
                    {
                        if (do_draw)
                            draw_erasers();
                    }
                    else if (cur_tool == TOOL_TEXT)
                    {
                        if (do_draw)
                            draw_fonts();
                    }
                    else if (cur_tool == TOOL_STAMP)
                    {
                        if (do_draw)
                            draw_stamps();
                    }
                    else if (cur_tool == TOOL_SHAPES)
                    {
                        if (do_draw)
                            draw_shapes();
                    }
                    else if (cur_tool == TOOL_MAGIC)
                    {
                        if (do_draw)
                            draw_magic();
                    }
                    
                    /* Update the screen: */
                    if (do_draw)
                        update_screen_rect(&r_toolopt);
                }
            }
            else if (event.type == SDL_USEREVENT)
            {
                if (event.user.code == USEREVENT_TEXT_UPDATE)
                {
                    /* Time to replace "Great!" with old tip text: */
                    
                    if (event.user.data1 != NULL)
                    {
                        if (((unsigned char *) event.user.data1)[0] == '=')
                        {
                            draw_tux_text_ex(TUX_GREAT, (char *) event.user.data1 + 1, 1, (int)event.user.data2);
                        }
                        else
                        {
                            draw_tux_text_ex(TUX_GREAT, (char *) event.user.data1, 0, (int)event.user.data2);
                        }
                    }
                    else
                        draw_tux_text(TUX_GREAT, "", 1);
                }
                else if (event.user.code == USEREVENT_PLAYDESCSOUND)
                {
                    /* Play a stamp's spoken description (because the sound effect just finished) */
                    /* (This event is pushed into the queue by playstampdesc(), which
                     is a callback from Mix_ChannelFinished() when playing a stamp SFX) */
                    
                    debug("Playing description sound...");
                    
#ifndef NOSOUND
                    Mix_ChannelFinished(NULL);	/* Kill the callback, so we don't get stuck in a loop! */
                    
                    if (event.user.data1 != NULL)
                    {
                        if ((int) event.user.data1 == cur_stamp[stamp_group])	/* Don't play old stamp's sound... */
                        {
                            if (!mute && stamp_data[stamp_group][(int) event.user.data1]->sdesc != NULL)
                                Mix_PlayChannel(2, stamp_data[stamp_group][(int) event.user.data1]->sdesc,
                                                0);
                        }
                    }
#endif
                }
            }
            else if (event.type == SDL_MOUSEBUTTONUP)
            {
                if (scrolling)
                {
                    if (scrolltimer != NULL)
                    {
                        SDL_RemoveTimer(scrolltimer);
                        scrolltimer = NULL;
                    }
                    scrolling = 0;
                    
                    /* printf("Killing scrolling\n"); */
                }
                
                if (button_down)
                {
                    if (cur_tool == TOOL_BRUSH)
                    {
                        /* (Drawing on mouse release to fix single click issue) */
                        brush_draw(old_x, old_y, old_x, old_y, 1);
                    }
                    else if (cur_tool == TOOL_LINES)
                    {
                        /* (Arbitrarily large, so we draw once now) */
                        reset_brush_counter();
                        
                        brush_draw(line_start_x, line_start_y,
                                   event.button.x - r_canvas.x,
                                   event.button.y - r_canvas.y, 1);
                        brush_draw(event.button.x - r_canvas.x,
                                   event.button.y - r_canvas.y,
                                   event.button.x - r_canvas.x,
                                   event.button.y - r_canvas.y, 1);
                        
                        playsound(screen, 1, SND_LINE_END, 1, event.button.x,
                                  SNDDIST_NEAR);
                        draw_tux_text(TUX_GREAT, tool_tips[TOOL_LINES], 1);
                    }
                    else if (cur_tool == TOOL_SHAPES)
                    {
                        if (shape_tool_mode == SHAPE_TOOL_MODE_STRETCH)
                        {
                            /* Now we can rotate the shape... */
                            
                            shape_outer_x = event.button.x - r_canvas.x;
                            shape_outer_y = event.button.y - r_canvas.y;
                            
                            if (!simple_shapes && !shape_no_rotate[cur_shape])
                            {
                                shape_tool_mode = SHAPE_TOOL_MODE_ROTATE;
                                
                                SDL_WarpMouse(shape_outer_x + 96, shape_ctr_y);
                                do_setcursor(cursor_rotate);
                                
                                
                                /* Erase stretchy XOR: */
                                
                                do_shape(shape_ctr_x, shape_ctr_y, old_x, old_y, 0, 0);
                                
                                /* Make an initial rotation XOR to be erased: */
                                
                                do_shape(shape_ctr_x, shape_ctr_y,
                                         shape_outer_x, shape_outer_y,
                                         rotation(shape_ctr_x, shape_ctr_y,
                                                  shape_outer_x, shape_outer_y), 0);
                                
                                playsound(screen, 1, SND_LINE_START, 1, event.button.x,
                                          SNDDIST_NEAR);
                                draw_tux_text(TUX_BORED, TIP_SHAPE_NEXT, 1);
                                
                                
                                /* FIXME: Do something less intensive! */
                                
                                SDL_Flip(screen);
                            }
                            else
                            {
                                reset_brush_counter();
                                
                                
                                playsound(screen, 1, SND_LINE_END, 1, event.button.x,
                                          SNDDIST_NEAR);
                                do_shape(shape_ctr_x, shape_ctr_y, shape_outer_x,
                                         shape_outer_y, 0, 1);
                                
                                SDL_Flip(screen);
                                
                                shape_tool_mode = SHAPE_TOOL_MODE_DONE;
                                draw_tux_text(TUX_GREAT, tool_tips[TOOL_SHAPES], 1);
                            }
                        }
                    }
                    else if (cur_tool == TOOL_MAGIC && magics[cur_magic].mode == MODE_PAINT)
                    {
                        int undo_ctr;
                        SDL_Surface * last;
                        
                        /* Releasing button: Finish the magic: */
                        
                        if (cur_undo > 0)
                            undo_ctr = cur_undo - 1;
                        else
                            undo_ctr = NUM_UNDO_BUFS - 1;
                        
                        last = undo_bufs[undo_ctr];
                        
                        update_rect.x = 0;
                        update_rect.y = 0;
                        update_rect.w = 0;
                        update_rect.h = 0;
                        
                        magic_funcs[magics[cur_magic].handle_idx].release(magic_api_struct,
                                                                          magics[cur_magic].idx,
                                                                          canvas, last,
                                                                          old_x, old_y,
                                                                          &update_rect);
                        
                        draw_tux_text(TUX_GREAT, magics[cur_magic].tip[magic_modeint(magics[cur_magic].mode)], 1);
                        
                        update_canvas(update_rect.x, update_rect.y,
                                      update_rect.x + update_rect.w,
                                      update_rect.y + update_rect.h);
                    }
                }
                
                button_down = 0;
            }
            else if (event.type == SDL_MOUSEMOTION && !ignoring_motion)
            {
                new_x = event.button.x - r_canvas.x;
                new_y = event.button.y - r_canvas.y;
                
                /* FIXME: Is doing this every event too intensive? */
                /* Should I check current cursor first? */
                
                if (HIT(r_tools))
                {
                    /* Tools: */
                    
                    if (tool_avail[((event.button.x - r_tools.x) / button_w) +
                                   ((event.button.y -
                                     r_tools.y) / button_h) * gd_tools.cols])
                    {
                        do_setcursor(cursor_hand);
                    }
                    else
                    {
                        do_setcursor(cursor_arrow);
                    }
                }
                else if (HIT(r_sfx))
                {
                    /* Sound player buttons: */
                    
                    if (cur_tool == TOOL_STAMP && use_sound && !mute &&
                        ((GRIDHIT_GD(r_sfx, gd_sfx) == 0 &&
                          !stamp_data[stamp_group][cur_stamp[stamp_group]]->no_sound) ||
                         (GRIDHIT_GD(r_sfx, gd_sfx) == 1 &&
                          !stamp_data[stamp_group][cur_stamp[stamp_group]]->no_descsound)))
                    {
                        do_setcursor(cursor_hand);
                    }
                    else
                    {
                        do_setcursor(cursor_arrow);
                    }
                }
                else if (HIT(r_colors))
                {
                    /* Color picker: */
                    
                    if (colors_are_selectable)
                        do_setcursor(cursor_hand);
                    else
                        do_setcursor(cursor_arrow);
                }
                else if (HIT(r_toolopt))
                {
                    /* mouse cursor code
                     WARNING: this must be kept in sync with the mouse-click
                     and mouse-click code. (it isn't, currently!) */
                    
                    /* Note set of things we're dealing with */
                    /* (stamps, brushes, etc.) */
                    
                    if (cur_tool == TOOL_STAMP)
                    {
                    }
                    else if (cur_tool == TOOL_TEXT)
                    {
                    }
                    
                    max = 14;
                    if (cur_tool == TOOL_STAMP && !disable_stamp_controls)
                        max = 8; /* was 10 before left/right group buttons -bjk 2007.05.03 */
                    if (cur_tool == TOOL_TEXT && !disable_stamp_controls)
                        max = 10;
                    if (cur_tool == TOOL_MAGIC && !disable_magic_controls)
                        max = 12;
                    
                    
                    if (num_things > max + TOOLOFFSET)
                    {
                        /* Are there scroll buttons? */
                        
                        if (event.button.y < 40 + 24)
                        {
                            /* Up button; is it available? */
                            
                            if (*thing_scroll > 0)
                                do_setcursor(cursor_up);
                            else
                                do_setcursor(cursor_arrow);
                        }
                        else if (event.button.y >
                                 (48 * ((max - 2) / 2 + TOOLOFFSET / 2)) + 40 + 24
                                 && event.button.y <=
                                 (48 * ((max - 2) / 2 + TOOLOFFSET / 2)) + 40 + 24 + 24)
                        {
                            /* Down button; is it available? */
                            
                            if (*thing_scroll < num_things - (max - 2))
                                do_setcursor(cursor_down);
                            else
                                do_setcursor(cursor_arrow);
                        }
                        else
                        {
                            /* One of the selectors: */
                            
                            which = ((event.button.y - 40 - 24) / 48) * 2 +
                            (event.button.x - (WINDOW_WIDTH - 96)) / 48;
                            
                            if (which < num_things)
                                do_setcursor(cursor_hand);
                            else
                                do_setcursor(cursor_arrow);
                        }
                    }
                    else
                    {
                        /* No scroll buttons - must be a selector: */
                        
                        which = ((event.button.y - 40) / 48) * 2 +
                        (event.button.x - (WINDOW_WIDTH - 96)) / 48;
                        
                        if (which < num_things)
                            do_setcursor(cursor_hand);
                        else
                            do_setcursor(cursor_arrow);
                    }
                }
                else if (HIT(r_canvas))
                {
                    /* Canvas: */
                    
                    if (cur_tool == TOOL_BRUSH)
                        do_setcursor(cursor_brush);
                    else if (cur_tool == TOOL_STAMP)
                        do_setcursor(cursor_tiny);
                    else if (cur_tool == TOOL_LINES)
                        do_setcursor(cursor_crosshair);
                    else if (cur_tool == TOOL_SHAPES)
                    {
                        if (shape_tool_mode != SHAPE_TOOL_MODE_ROTATE)
                            do_setcursor(cursor_crosshair);
                        else
                            do_setcursor(cursor_rotate);
                    }
                    else if (cur_tool == TOOL_TEXT)
                        do_setcursor(cursor_insertion);
                    else if (cur_tool == TOOL_MAGIC)
                        do_setcursor(cursor_wand);
                    else if (cur_tool == TOOL_ERASER)
                        do_setcursor(cursor_tiny);
                }
                else
                {
                    do_setcursor(cursor_arrow);
                }
                
                
                if (button_down)
                {
                    if (cur_tool == TOOL_BRUSH)
                    {
                        /* Pushing button and moving: Draw with the brush: */
                        
                        brush_draw(old_x, old_y, new_x, new_y, 1);
                        
                        playsound(screen, 0, paintsound(img_cur_brush_w), 0,
                                  event.button.x, SNDDIST_NEAR);
                    }
                    else if (cur_tool == TOOL_LINES)
                    {
                        /* Still pushing button, while moving:
                         Draw XOR where line will go: */
                        
                        line_xor(line_start_x, line_start_y, old_x, old_y);
                        
                        line_xor(line_start_x, line_start_y, new_x, new_y);
                        
                        update_screen(line_start_x + r_canvas.x,
                                      line_start_y + r_canvas.y, old_x + r_canvas.x,
                                      old_y + r_canvas.y);
                        update_screen(line_start_x + r_canvas.x,
                                      line_start_y + r_canvas.y, new_x + r_canvas.x,
                                      new_y + r_canvas.y);
                    }
                    else if (cur_tool == TOOL_SHAPES)
                    {
                        /* Still pushing button, while moving:
                         Draw XOR where shape will go: */
                        
                        if (shape_tool_mode == SHAPE_TOOL_MODE_STRETCH)
                        {
                            do_shape(shape_ctr_x, shape_ctr_y, old_x, old_y, 0, 0);
                            
                            do_shape(shape_ctr_x, shape_ctr_y, new_x, new_y, 0, 0);
                            
                            
                            /* FIXME: Fix update shape function! */
                            
                            /* update_shape(shape_ctr_x, old_x, new_x,
                             shape_ctr_y, old_y, new_y,
                             shape_locked[cur_shape]); */
                            
                            SDL_Flip(screen);
                        }
                    }
                    else if (cur_tool == TOOL_MAGIC && magics[cur_magic].mode == MODE_PAINT)
                    {
                        int undo_ctr;
                        SDL_Surface * last;
                        
                        /* Pushing button and moving: Continue doing the magic: */
                        
                        if (cur_undo > 0)
                            undo_ctr = cur_undo - 1;
                        else
                            undo_ctr = NUM_UNDO_BUFS - 1;
                        
                        last = undo_bufs[undo_ctr];
                        
                        update_rect.x = 0;
                        update_rect.y = 0;
                        update_rect.w = 0;
                        update_rect.h = 0;
                        
                        magic_funcs[magics[cur_magic].handle_idx].drag(magic_api_struct,
                                                                       magics[cur_magic].idx,
                                                                       canvas, last,
                                                                       old_x, old_y,
                                                                       new_x, new_y,
                                                                       &update_rect);
                        
                        update_canvas(update_rect.x, update_rect.y,
                                      update_rect.x + update_rect.w,
                                      update_rect.y + update_rect.h);
                    }
                    else if (cur_tool == TOOL_ERASER)
                    {
                        /* Still pushing, and moving - Erase! */
                        
                        do_eraser(new_x, new_y);
                    }
                }
                
                
                if (cur_tool == TOOL_STAMP ||
                    (cur_tool == TOOL_ERASER && !button_down))
                {
                    int w, h;
                    /* Moving: Draw XOR where stamp/eraser will apply: */
                    
                    if (cur_tool == TOOL_STAMP)
                    {
                        w = active_stamp->w;
                        h = active_stamp->h;
                    }
                    else
                    {
                        if (cur_eraser < NUM_ERASERS / 2)
                        {
                            w = (ERASER_MIN +
                                 (((NUM_ERASERS / 2) - cur_eraser - 1) *
                                  ((ERASER_MAX - ERASER_MIN) / ((NUM_ERASERS / 2) - 1))));
                        }
                        else
                        {
                            w = (ERASER_MIN +
                                 (((NUM_ERASERS / 2) - (cur_eraser - NUM_ERASERS / 2) - 1) *
                                  ((ERASER_MAX - ERASER_MIN) / ((NUM_ERASERS / 2) - 1))));
                        }
                        
                        h = w;
                    }
                    
                    if (old_x >= 0 && old_x < r_canvas.w &&
                        old_y >= 0 && old_y < r_canvas.h)
                    {
                        if (cur_tool == TOOL_STAMP)
                        {
                            stamp_xor(old_x, old_y);
                            
                            update_screen(old_x - (CUR_STAMP_W + 1) / 2 + r_canvas.x,
                                          old_y - (CUR_STAMP_H + 1) / 2 + r_canvas.y,
                                          old_x + (CUR_STAMP_W + 1) / 2 + r_canvas.x,
                                          old_y + (CUR_STAMP_H + 1) / 2 + r_canvas.y);
                        }
                        else
                        {
                            rect_xor(old_x - w / 2, old_y - h / 2,
                                     old_x + w / 2, old_y + h / 2);
                            
                            update_screen(old_x - w / 2 + r_canvas.x,
                                          old_y - h / 2 + r_canvas.y,
                                          old_x + w / 2 + r_canvas.x,
                                          old_y + h / 2 + r_canvas.y);
                        }
                    }
                    
                    if (new_x >= 0 && new_x < r_canvas.w &&
                        new_y >= 0 && new_y < r_canvas.h)
                    {
                        if (cur_tool == TOOL_STAMP)
                        {
                            stamp_xor(new_x, new_y);
                            
                            update_screen(old_x - (CUR_STAMP_W + 1) / 2 + r_canvas.x,
                                          old_y - (CUR_STAMP_H + 1) / 2 + r_canvas.y,
                                          old_x + (CUR_STAMP_W + 1) / 2 + r_canvas.x,
                                          old_y + (CUR_STAMP_H + 1) / 2 + r_canvas.y);
                        }
                        else
                        {
                            rect_xor(new_x - w / 2, new_y - h / 2,
                                     new_x + w / 2, new_y + h / 2);
                            
                            update_screen(new_x - w / 2 + r_canvas.x,
                                          new_y - h / 2 + r_canvas.y,
                                          new_x + w / 2 + r_canvas.x,
                                          new_y + h / 2 + r_canvas.y);
                        }
                    }
                }
                else if (cur_tool == TOOL_SHAPES &&
                         shape_tool_mode == SHAPE_TOOL_MODE_ROTATE)
                {
                    do_shape(shape_ctr_x, shape_ctr_y,
                             shape_outer_x, shape_outer_y,
                             rotation(shape_ctr_x, shape_ctr_y, old_x, old_y), 0);
                    
                    
                    do_shape(shape_ctr_x, shape_ctr_y,
                             shape_outer_x, shape_outer_y,
                             rotation(shape_ctr_x, shape_ctr_y, new_x, new_y), 0);
                    
                    
                    /* FIXME: Do something less intensive! */
                    SDL_Flip(screen);
                }
                
                old_x = new_x;
                old_y = new_y;
            }
        }
        
        
        SDL_Delay(10);
        
        cur_cursor_blink = SDL_GetTicks();
        
        
        if (cur_tool == TOOL_TEXT && cursor_x != -1 && cursor_y != -1 &&
            cur_cursor_blink > last_cursor_blink + CURSOR_BLINK_SPEED)
        {
            last_cursor_blink = SDL_GetTicks();
            draw_blinking_cursor();
        }
    }
    while (!done);
}

/* Draw using the text entry cursor/caret: */
static void hide_blinking_cursor(void)
{
    if (cur_toggle_count & 1)
    {
        draw_blinking_cursor();
    }
}

static void draw_blinking_cursor(void)
{
    cur_toggle_count++;
    
    line_xor(cursor_x + cursor_textwidth, cursor_y,
             cursor_x + cursor_textwidth,
             cursor_y + TuxPaint_Font_FontHeight(getfonthandle(cur_font)));
    
    update_screen(cursor_x + r_canvas.x + cursor_textwidth,
                  cursor_y + r_canvas.y,
                  cursor_x + r_canvas.x + cursor_textwidth,
                  cursor_y + r_canvas.y +
                  TuxPaint_Font_FontHeight(getfonthandle(cur_font)));
}

/* Draw using the current brush: */

static void brush_draw(int x1, int y1, int x2, int y2, int update)
{
    int dx, dy, y, frame_w, w, h;
    int orig_x1, orig_y1, orig_x2, orig_y2, tmp;
    int direction, r;
    float m, b;
    
    orig_x1 = x1;
    orig_y1 = y1;
    
    orig_x2 = x2;
    orig_y2 = y2;
    
    
    frame_w = img_brushes[cur_brush]->w / abs(brushes_frames[cur_brush]);
    w = frame_w / (brushes_directional[cur_brush] ? 3: 1);
    h = img_brushes[cur_brush]->h / (brushes_directional[cur_brush] ? 3 : 1);
    
    x1 = x1 - (w >> 1);
    y1 = y1 - (h >> 1);
    
    x2 = x2 - (w >> 1);
    y2 = y2 - (h >> 1);
    
    
    direction = BRUSH_DIRECTION_NONE;
    if (brushes_directional[cur_brush])
    {
        r = rotation(x1, y1, x2, y2) + 22;
        if (r < 0)
            r = r + 360;
        
        if (x1 != x2 || y1 != y2)
            direction = (r / 45);
    }
    
    
    dx = x2 - x1;
    dy = y2 - y1;
    
    if (dx != 0)
    {
        m = ((float) dy) / ((float) dx);
        b = y1 - m * x1;
        
        if (x2 >= x1)
            dx = 1;
        else
            dx = -1;
        
        
        while (x1 != x2)
        {
            y1 = m * x1 + b;
            y2 = m * (x1 + dx) + b;
            
            if (y1 > y2)
            {
                for (y = y1; y >= y2; y--)
                    blit_brush(x1, y, direction);
            }
            else
            {
                for (y = y1; y <= y2; y++)
                    blit_brush(x1, y, direction);
            }
            
            x1 = x1 + dx;
        }
    }
    else
    {
        if (y1 > y2)
        {
            y = y1;
            y1 = y2;
            y2 = y;
        }
        
        for (y = y1; y <= y2; y++)
            blit_brush(x1, y, direction);
    }
    
    if (orig_x1 > orig_x2)
    {
        tmp = orig_x1;
        orig_x1 = orig_x2;
        orig_x2 = tmp;
    }
    
    if (orig_y1 > orig_y2)
    {
        tmp = orig_y1;
        orig_y1 = orig_y2;
        orig_y2 = tmp;
    }
    
    
    if (update)
    {
        update_canvas(orig_x1 - (w >> 1),
                      orig_y1 - (h >> 1),
                      orig_x2 + (w >> 1),
                      orig_y2 + (h >> 1));
    }
}

void reset_brush_counter_and_frame(void)
{
    brush_counter = 999;
    brush_frame = 0;
}

void reset_brush_counter(void)
{
    brush_counter = 999;
}


/* Draw the current brush in the current color: */

static void blit_brush(int x, int y, int direction)
{
    SDL_Rect src, dest;
    
    brush_counter++;
    
    if (brush_counter >= img_cur_brush_spacing)
    {
        brush_counter = 0;
        
        if (img_cur_brush_frames >= 0)
        {
            brush_frame++;
            if (brush_frame >= img_cur_brush_frames)
                brush_frame = 0;
        }
        else
            brush_frame = rand() % abs(img_cur_brush_frames);
        
        dest.x = x;
        dest.y = y;
        
        if (img_cur_brush_directional)
        {
            if (direction == BRUSH_DIRECTION_UP_LEFT ||
                direction == BRUSH_DIRECTION_UP ||
                direction == BRUSH_DIRECTION_UP_RIGHT)
            {
                src.y = 0;
            }
            else if (direction == BRUSH_DIRECTION_LEFT ||
                     direction == BRUSH_DIRECTION_NONE ||
                     direction == BRUSH_DIRECTION_RIGHT)
            {
                src.y = img_cur_brush_h;
            }
            else if (direction == BRUSH_DIRECTION_DOWN_LEFT ||
                     direction == BRUSH_DIRECTION_DOWN ||
                     direction == BRUSH_DIRECTION_DOWN_RIGHT)
            {
                src.y = img_cur_brush_h << 1;
            }
            
            if (direction == BRUSH_DIRECTION_UP_LEFT ||
                direction == BRUSH_DIRECTION_LEFT ||
                direction == BRUSH_DIRECTION_DOWN_LEFT)
            {
                src.x = brush_frame * img_cur_brush_frame_w;
            }
            else if (direction == BRUSH_DIRECTION_UP ||
                     direction == BRUSH_DIRECTION_NONE ||
                     direction == BRUSH_DIRECTION_DOWN)
            {
                src.x = brush_frame * img_cur_brush_frame_w + img_cur_brush_w;
            }
            else if (direction == BRUSH_DIRECTION_UP_RIGHT ||
                     direction == BRUSH_DIRECTION_RIGHT ||
                     direction == BRUSH_DIRECTION_DOWN_RIGHT)
            {
                src.x = brush_frame * img_cur_brush_frame_w + (img_cur_brush_w << 1);
            }
        }
        else
        {
            src.x = brush_frame * img_cur_brush_w;
            src.y = 0;
        }
        
        src.w = img_cur_brush_w;
        src.h = img_cur_brush_h;
        
        SDL_BlitSurface(img_cur_brush, &src, canvas, &dest);
    }
}


/* stamp tinter */

#define TINTER_ANYHUE 0	/* like normal, but remaps all hues in the stamp */
#define TINTER_NARROW 1	/* like normal, but narrow hue angle */
#define TINTER_NORMAL 2	/* normal */
#define TINTER_VECTOR 3	/* map black->white to black->destination */


typedef struct multichan
{
    double L, hue, sat;	/* L,a,b would be better -- 2-way formula unknown */
    unsigned char or, og, ob, alpha;	/* old 8-bit values */
} multichan;

#define X0 ((double)0.9505)
#define Y0 ((double)1.0000)
#define Z0 ((double)1.0890)
#define u0_prime ( (4.0 * X0) / (X0 + 15.0*Y0 + 3.0*Z0) )
#define v0_prime ( (9.0 * Y0) / (X0 + 15.0*Y0 + 3.0*Z0) )


static void fill_multichan(multichan * mc, double *up, double *vp)
{
    double X, Y, Z, u, v;
    double u_prime, v_prime;	/* temp, part of official formula */
    double Y_norm, fract;		/* severely temp */
    
    double r = sRGB_to_linear_table[mc->or];
    double g = sRGB_to_linear_table[mc->og];
    double b = sRGB_to_linear_table[mc->ob];
    
    /* coordinate change, RGB --> XYZ */
    X = 0.4124 * r + 0.3576 * g + 0.1805 * b;
    Y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    Z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
    
    /* XYZ --> Luv */
    Y_norm = Y / Y0;
    fract = 1.0 / (X + 15.0 * Y + 3.0 * Z);
    u_prime = 4.0 * X * fract;
    v_prime = 9.0 * Y * fract;
    mc->L =
    (Y_norm > 0.008856) ? 116.0 * pow(Y_norm,
                                      1.0 / 3.0) - 16.0 : 903.3 * Y_norm;
    u = 13.0 * mc->L * (u_prime - u0_prime);
    v = 13.0 * mc->L * (v_prime - v0_prime);
    
    mc->sat = sqrt(u * u + v * v);
    mc->hue = atan2(u, v);
    if (up)
        *up = u;
    if (vp)
        *vp = v;
}


static double tint_part_1(multichan * work, SDL_Surface * in)
{
    int xx, yy;
    double u_total = 0;
    double v_total = 0;
    double u, v;
    Uint32(*getpixel) (SDL_Surface *, int, int) =
    getpixels[in->format->BytesPerPixel];
    
    
    SDL_LockSurface(in);
    for (yy = 0; yy < in->h; yy++)
    {
        for (xx = 0; xx < in->w; xx++)
        {
            multichan *mc = work + yy * in->w + xx;
            /* put pixels into a more tolerable form */
            SDL_GetRGBA(getpixel(in, xx, yy),
                        in->format, &mc->or, &mc->og, &mc->ob, &mc->alpha);
            
            fill_multichan(mc, &u, &v);
            
            /* average out u and v, giving more weight to opaque
             high-saturation pixels
             (this is to take an initial guess at the primary hue) */
            
            u_total += mc->alpha * u * mc->sat;
            v_total += mc->alpha * v * mc->sat;
            
        }
    }
    SDL_UnlockSurface(in);
    
#ifdef DEBUG
    fprintf(stderr, "u_total=%f\nv_total=%f\natan2()=%f\n",
            u_total, v_total, atan2(u_total, v_total));
#endif
    
    return atan2(u_total, v_total);
}


static void change_colors(SDL_Surface * out, multichan * work,
                          double hue_range, multichan * key_color_ptr)
{
    double lower_hue_1, upper_hue_1, lower_hue_2, upper_hue_2;
    int xx, yy;
    multichan dst;
    double satratio;
    double slope;
    void (*putpixel) (SDL_Surface *, int, int, Uint32);
    double old_sat;
    double newsat;
    double L;
    double X, Y, Z;
    double u_prime, v_prime;	/* temp, part of official formula */
    unsigned tries;
    double u;
    double v;
    double r, g, b;
    
    
    /* prepare source and destination color info
     should reset hue_range or not? won't bother for now*/
    multichan key_color = *key_color_ptr;	/* want to work from a copy, for safety */
    lower_hue_1 = key_color.hue - hue_range;
    upper_hue_1 = key_color.hue + hue_range;
    if (lower_hue_1 < -M_PI)
    {
        lower_hue_2 = lower_hue_1 + 2 * M_PI;
        upper_hue_2 = upper_hue_1 + 2 * M_PI;
    }
    else
    {
        lower_hue_2 = lower_hue_1 - 2 * M_PI;
        upper_hue_2 = upper_hue_1 - 2 * M_PI;
    }
    
    /* get the destination color set up */
    dst.or = color_hexes[cur_color][0];
    dst.og = color_hexes[cur_color][1];
    dst.ob = color_hexes[cur_color][2];
    fill_multichan(&dst, NULL, NULL);
    
    satratio = dst.sat / key_color.sat;
    slope = (dst.L - key_color.L) / dst.sat;
    putpixel = putpixels[out->format->BytesPerPixel];
    
    SDL_LockSurface(out);
    for (yy = 0; yy < out->h; yy++)
    {
        for (xx = 0; xx < out->w; xx++)
        {
            multichan *mc = work + yy * out->w + xx;
            double oldhue = mc->hue;
            
            /* if not in the first range, and not in the second range, skip this one
             (really should alpha-blend as a function of hue angle difference) */
            if ((oldhue < lower_hue_1 || oldhue > upper_hue_1)
                && (oldhue < lower_hue_2 || oldhue > upper_hue_2))
            {
                putpixel(out, xx, yy,
                         SDL_MapRGBA(out->format, mc->or, mc->og, mc->ob, mc->alpha));
                continue;
            }
            
            /* Modify the pixel */
            old_sat = mc->sat;
            newsat = old_sat * satratio;
            L = mc->L;
            if (dst.sat > 0)
                L += newsat * slope;	/* not greyscale destination */
            else
                L += old_sat * (dst.L - key_color.L) / key_color.sat;
            
            /* convert from L,u,v all the way back to sRGB with 8-bit channels */
            tries = 3;
        trysat:;
            u = newsat * sin(dst.hue);
            v = newsat * cos(dst.hue);
            
            /* Luv to XYZ */
            u_prime = u / (13.0 * L) + u0_prime;
            v_prime = v / (13.0 * L) + v0_prime;
            Y =
            (L > 7.99959199307) ? Y0 * pow((L + 16.0) / 116.0,
                                           3.0) : Y0 * L / 903.3;
            X = 2.25 * Y * u_prime / v_prime;
            Z = (3.0 * Y - 0.75 * Y * u_prime) / v_prime - 5.0 * Y;
            
            /* coordinate change: XYZ to RGB */
            r = 3.2410 * X + -1.5374 * Y + -0.4986 * Z;
            g = -0.9692 * X + 1.8760 * Y + 0.0416 * Z;
            b = 0.0556 * X + -0.2040 * Y + 1.0570 * Z;
            
            /* If it is out of gamut, try to de-saturate it a few times before truncating.
             (the linear_to_sRGB function will truncate) */
            if ((r <= -0.5 || g <= -0.5 || b <= -0.5 || r >= 255.5 || g >= 255.5
                 || b >= 255.5) && tries--)
            {
                newsat *= 0.8;
                goto trysat;
            }
            
            putpixel(out, xx, yy,
                     SDL_MapRGBA(out->format, linear_to_sRGB(r), linear_to_sRGB(g),
                                 linear_to_sRGB(b), mc->alpha));
        }
    }
    SDL_UnlockSurface(out);
}


static multichan *find_most_saturated(double initial_hue, multichan * work,
                                      unsigned num, double *hue_range_ptr)
{
    /* find the most saturated pixel near the initial hue guess */
    multichan *key_color_ptr = NULL;
    double hue_range;
    unsigned i;
    double max_sat;
    double lower_hue_1;
    double upper_hue_1;
    double lower_hue_2;
    double upper_hue_2;
    multichan *mc;
    
    switch (stamp_data[stamp_group][cur_stamp[stamp_group]]->tinter)
    {
        default:
        case TINTER_NORMAL:
            hue_range = 18 * M_PI / 180.0;	/* plus or minus 18 degrees search, 27 replace */
            break;
        case TINTER_NARROW:
            hue_range = 6 * M_PI / 180.0;	/* plus or minus 6 degrees search, 9 replace */
            break;
        case TINTER_ANYHUE:
            hue_range = M_PI;		/* plus or minus 180 degrees */
            break;
    }
    
hue_range_retry:;
    
    max_sat = 0;
    lower_hue_1 = initial_hue - hue_range;
    upper_hue_1 = initial_hue + hue_range;
    
    if (lower_hue_1 < -M_PI)
    {
        lower_hue_2 = lower_hue_1 + 2 * M_PI;
        upper_hue_2 = upper_hue_1 + 2 * M_PI;
    }
    else
    {
        lower_hue_2 = lower_hue_1 - 2 * M_PI;
        upper_hue_2 = upper_hue_1 - 2 * M_PI;
    }
    
    i = num;
    while (i--)
    {
        mc = work + i;
        
        /* if not in the first range, and not in the second range, skip this one */
        if ((mc->hue < lower_hue_1 || mc->hue > upper_hue_1) &&
            (mc->hue < lower_hue_2 || mc->hue > upper_hue_2))
            continue;
        
        if (mc->sat > max_sat)
        {
            max_sat = mc->sat;
            key_color_ptr = mc;
        }
    }
    
    if (!key_color_ptr)
    {
        hue_range *= 1.5;
        
        if (hue_range < M_PI)
            goto hue_range_retry;
    }
    
    *hue_range_ptr = hue_range;
    
    return key_color_ptr;
}


static void vector_tint_surface(SDL_Surface * out, SDL_Surface * in)
{
    int xx, yy;
    Uint32(*getpixel) (SDL_Surface *, int, int) =
    getpixels[in->format->BytesPerPixel];
    void (*putpixel) (SDL_Surface *, int, int, Uint32) =
    putpixels[out->format->BytesPerPixel];
    
    double r = sRGB_to_linear_table[color_hexes[cur_color][0]];
    double g = sRGB_to_linear_table[color_hexes[cur_color][1]];
    double b = sRGB_to_linear_table[color_hexes[cur_color][2]];
    
    SDL_LockSurface(in);
    for (yy = 0; yy < in->h; yy++)
    {
        for (xx = 0; xx < in->w; xx++)
        {
            unsigned char r8, g8, b8, a8;
            double old;
            
            SDL_GetRGBA(getpixel(in, xx, yy), in->format, &r8, &g8, &b8, &a8);
            /* get the linear greyscale value */
            old =
            sRGB_to_linear_table[r8] * 0.2126 +
            sRGB_to_linear_table[g8] * 0.7152 + sRGB_to_linear_table[b8] * 0.0722;
            
            putpixel(out, xx, yy,
                     SDL_MapRGBA(out->format, linear_to_sRGB(r * old),
                                 linear_to_sRGB(g * old), linear_to_sRGB(b * old),
                                 a8));
        }
    }
    SDL_UnlockSurface(in);
}


static void tint_surface(SDL_Surface * tmp_surf, SDL_Surface * surf_ptr)
{
    unsigned width = surf_ptr->w;
    unsigned height = surf_ptr->h;
    multichan *work;
    multichan *key_color_ptr;
    double initial_hue;
    double hue_range;
    
    
    work = malloc(sizeof(multichan) * width * height);
    
    if (work)
    {
        initial_hue = tint_part_1(work, surf_ptr);
        
#ifdef DEBUG
        printf("initial_hue = %f\n", initial_hue);
#endif
        
        key_color_ptr = find_most_saturated(initial_hue, work,
                                            width * height, &hue_range);
        
#ifdef DEBUG
        printf("key_color_ptr = %d\n", (int) key_color_ptr);
#endif
        
        if (key_color_ptr)
        {
            /* wider for processing than for searching */
            hue_range *= 1.5;
            
            change_colors(tmp_surf, work, hue_range, key_color_ptr);
            
            free(work);
            return;
        }
        else
        {
            fprintf(stderr, "find_most_saturated() failed\n");
        }
        
        free(work);
    }
    
    /* Failed!  Fall back: */
    
    fprintf(stderr, "Falling back to tinter=vector, "
            "this should be in the *.dat file\n");
    
    vector_tint_surface(tmp_surf, surf_ptr);
}



/* Draw using the current stamp: */

static void stamp_draw(int x, int y)
{
    SDL_Rect dest;
    SDL_Surface *tmp_surf, *surf_ptr, *final_surf;
    Uint32 amask;
    Uint8 r, g, b, a;
    int xx, yy, dont_free_tmp_surf, base_x, base_y;
    Uint32(*getpixel) (SDL_Surface *, int, int);
    void (*putpixel) (SDL_Surface *, int, int, Uint32);
    
    surf_ptr = active_stamp;
    
    getpixel = getpixels[surf_ptr->format->BytesPerPixel];
    
    
    /* Create a temp surface to play with: */
    
    if (stamp_colorable(cur_stamp[stamp_group]) || stamp_tintable(cur_stamp[stamp_group]))
    {
        amask = ~(surf_ptr->format->Rmask |
                  surf_ptr->format->Gmask | surf_ptr->format->Bmask);
        
        tmp_surf =
        SDL_CreateRGBSurface(SDL_SWSURFACE,
                             surf_ptr->w,
                             surf_ptr->h,
                             surf_ptr->format->BitsPerPixel,
                             surf_ptr->format->Rmask,
                             surf_ptr->format->Gmask,
                             surf_ptr->format->Bmask, amask);
        
        if (tmp_surf == NULL)
        {
            fprintf(stderr, "\nError: Can't render the colored stamp!\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", SDL_GetError());
            
            cleanup();
            exit(1);
        }
        
        dont_free_tmp_surf = 0;
    }
    else
    {
        /* Not altering color; no need to create temp surf if we don't use it! */
        
        tmp_surf = NULL;
        dont_free_tmp_surf = 1;
    }
    
    if (tmp_surf != NULL)
        putpixel = putpixels[tmp_surf->format->BytesPerPixel];
    else
        putpixel = NULL;
    
    
    /* Alter the stamp's color, if needed: */
    
    if (stamp_colorable(cur_stamp[stamp_group]) && tmp_surf != NULL)
    {
        /* Render the stamp in the chosen color: */
        
        /* FIXME: It sucks to render this EVERY TIME.  Why not just when
         they pick the color, or pick the stamp, like with brushes? */
        
        /* Render the stamp: */
        
        SDL_LockSurface(surf_ptr);
        SDL_LockSurface(tmp_surf);
        
        for (yy = 0; yy < surf_ptr->h; yy++)
        {
            for (xx = 0; xx < surf_ptr->w; xx++)
            {
                SDL_GetRGBA(getpixel(surf_ptr, xx, yy),
                            surf_ptr->format, &r, &g, &b, &a);
                
                putpixel(tmp_surf, xx, yy,
                         SDL_MapRGBA(tmp_surf->format,
                                     color_hexes[cur_color][0],
                                     color_hexes[cur_color][1],
                                     color_hexes[cur_color][2], a));
            }
        }
        
        SDL_UnlockSurface(tmp_surf);
        SDL_UnlockSurface(surf_ptr);
    }
    else if (stamp_tintable(cur_stamp[stamp_group]))
    {
        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->tinter == TINTER_VECTOR)
            vector_tint_surface(tmp_surf, surf_ptr);
        else
            tint_surface(tmp_surf, surf_ptr);
    }
    else
    {
        /* No color change, just use it! */
        tmp_surf = surf_ptr;
    }
    
    /* Shrink or grow it! */
    final_surf = thumbnail(tmp_surf, CUR_STAMP_W, CUR_STAMP_H, 0);
    
    /* Where it will go? */
    base_x = x - (CUR_STAMP_W + 1) / 2;
    base_y = y - (CUR_STAMP_H + 1) / 2;
    
    /* And blit it! */
    dest.x = base_x;
    dest.y = base_y;
    SDL_BlitSurface(final_surf, NULL, canvas, &dest); /* FIXME: Conditional jump or move depends on uninitialised value(s) */
    
    update_canvas(x - (CUR_STAMP_W + 1) / 2,
                  y - (CUR_STAMP_H + 1) / 2,
                  x + (CUR_STAMP_W + 1) / 2, y + (CUR_STAMP_H + 1) / 2);
    
    /* Free the temporary surfaces */
    
    if (!dont_free_tmp_surf)
        SDL_FreeSurface(tmp_surf);
    
    SDL_FreeSurface(final_surf);
}


/* Store canvas into undo buffer: */

static void rec_undo_buffer(void)
{
    int wanna_update_toolbar;
    
    wanna_update_toolbar = 0;
    
    
    SDL_BlitSurface(canvas, NULL, undo_bufs[cur_undo], NULL);
    undo_starters[cur_undo] = UNDO_STARTER_NONE;
    
    cur_undo = (cur_undo + 1) % NUM_UNDO_BUFS;
    
    if (cur_undo == oldest_undo)
        oldest_undo = (oldest_undo + 1) % NUM_UNDO_BUFS;
    
    newest_undo = cur_undo;
    
#ifdef DEBUG
    printf("DRAW: Current=%d  Oldest=%d  Newest=%d\n",
           cur_undo, oldest_undo, newest_undo);
#endif
    
    
    /* Update toolbar buttons, if needed: */
    
    if (tool_avail[TOOL_UNDO] == 0)
    {
        tool_avail[TOOL_UNDO] = 1;
        wanna_update_toolbar = 1;
    }
    
    if (tool_avail[TOOL_REDO])
    {
        tool_avail[TOOL_REDO] = 0;
        wanna_update_toolbar = 1;
    }
    
    if (wanna_update_toolbar)
    {
        draw_toolbar();
        update_screen_rect(&r_tools);
    }
}


/* Show program version: */
static void show_version(int details)
{
#ifdef __IPHONEOS__
    extern const char* DATA_PREFIX;
#endif
    
    printf("\nTux Paint\n");
    printf("  Version " VER_VERSION " (" VER_DATE ")\n");
    
    
    if (details == 0)
        return;
    
    
    printf("\nBuilt with these options:\n");
    
    
    /* Quality reductions: */
    
#ifdef LOW_QUALITY_THUMBNAILS
    printf("  Low Quality Thumbnails enabled  (LOW_QUALITY_THUMBNAILS)\n");
#endif
    
#ifdef LOW_QUALITY_COLOR_SELECTOR
    printf("  Low Quality Color Selector enabled  (LOW_QUALITY_COLOR_SELECTOR)\n");
#endif
    
#ifdef LOW_QUALITY_STAMP_OUTLINE
    printf("  Low Quality Stamp Outline enabled  (LOW_QUALITY_STAMP_OUTLINE)\n");
#endif
    
#ifdef NO_PROMPT_SHADOWS
    printf("  Prompt Shadows disabled  (NO_PROMPT_SHADOWS)\n");
#endif
    
#ifdef SMALL_CURSOR_SHAPES
    printf("  Small cursor shapes enabled  (SMALL_CURSOR_SHAPES)\n");
#endif
    
#ifdef NO_BILINEAR
    printf("  Bilinear scaling disabled  (NO_BILINEAR)\n");
#endif
    
#ifdef NOSVG
    printf("  SVG support disabled  (NOSVG)\n");
#endif
    
    /* Sound: */
    
#ifdef NOSOUND
    printf("  Sound disabled  (NOSOUND)\n");
#endif
    
    
    /* Platform */
    
#ifdef __APPLE__
    printf("  Built for Mac OS X  (__APPLE__)\n");
#elif WIN32
    printf("  Built for Windows  (WIN32)\n");
#elif __BEOS__
    printf("  Built for BeOS  (__BEOS__)\n");
#elif NOKIA_770
    printf("  Built for Maemo  (NOKIA_770)\n");
#elif OLPC_XO
    printf("  Built for XO  (OLPC_XO)\n");
#else
    printf("  Built for POSIX\n");
#endif
    
    
    /* Video options */
    
#ifdef USE_HWSURFACE
    printf("  Using hardware surface  (USE_HWSURFACE)\n");
#else
    printf("  Using software surface  (no USE_HWSURFACE)\n");
#endif
    printf("  Using %dbpp video  (VIDEO_BPP=%d)\n", VIDEO_BPP, VIDEO_BPP);
    
    
    /* Print method */
    
#ifdef PRINTMETHOD_PNG_PNM_PS
    printf("  Prints as PNGs  (PRINTMETHOD_PNG_PNM_PS)\n");
#endif
    
#ifdef PRINTMETHOD_PS
    printf("  Prints as PostScript  (PRINTMETHOD_PS)\n");
#endif
    
    
    /* Threading */
    
#ifdef FORKED_FONTS
    printf("  Threaded font loader enabled  (FORKED_FONTS)\n");
#else
    printf("  Threaded font loader disabled  (no FORKED_FONTS)\n");
#endif
    
    
    /* Old code used */
    
#ifdef OLD_STAMP_GROW_SHRINK
    printf("  Old-style stamp size UI  (OLD_STAMP_GROW_SHRINK)\n");
#endif
    
    printf("  Data directory (DATA_PREFIX) = %s\n", DATA_PREFIX);
    printf("  Plugin directory (MAGIC_PREFIX) = %s\n", MAGIC_PREFIX);
    printf("  Doc directory (DOC_PREFIX) = %s\n", DOC_PREFIX);
    //  printf("  Locale directory (LOCALEDIR) = %s\n",sprintf("%s/%s", DATA_PREFIX, "locale"));
    printf("  Input Method directory (IMDIR) = %s\n", IMDIR);
    printf("  System config directory (CONFDIR) = %s\n", CONFDIR);
    
    
    /* Debugging */
    
#ifdef DEBUG
    printf("  Verbose debugging enabled  (DEBUG)\n");
#endif
    
#ifdef DEBUG_MALLOC
    printf("  Memory allocation debugging enabled  (DEBUG_MALLOC)\n");
#endif
    
    
    printf("\n");
}


/* Show usage display: */

static void show_usage(FILE * f, char *prg)
{
    char *blank;
    unsigned i;
    
    blank = strdup(prg);
    
    for (i = 0; i < strlen(blank); i++)
        blank[i] = ' ';
    
    fprintf(f,
            "\n"
            "Usage: %s {--usage | --help | --version | --verbose-version | --copying}\n"
            "\n"
            "  %s [--windowed | --fullscreen]\n"
            "  %s [--WIDTHxHEIGHT | --native]\n"
            "  %s [--disablescreensaver | --allowscreensaver ]\n"
            "  %s [--orient=landscape | --orient=portrait]\n"
            "  %s [--startblank | --startlast]\n"
            "  %s [--sound | --nosound]\n"
            "  %s [--quit | --noquit]\n"
            "  %s [--print | --noprint]\n"
            "  %s [--complexshapes | --simpleshapes]\n"
            "  %s [--mixedcase | --uppercase]\n"
            "  %s [--fancycursors | --nofancycursors]\n"
            "  %s [--hidecursor | --showcursor]\n"
            "  %s [--mouse | --keyboard]\n"
            "  %s [--dontgrab | --grab]\n"
            "  %s [--noshortcuts | --shortcuts]\n"
            "  %s [--wheelmouse | --nowheelmouse]\n"
            "  %s [--nobuttondistinction | --buttondistinction]\n"
            "  %s [--outlines | --nooutlines]\n"
            "  %s [--stamps | --nostamps]\n"
            "  %s [--sysfonts | --nosysfonts]\n"
            "  %s [--nostampcontrols | --stampcontrols]\n"
            "  %s [--nomagiccontrols | --magiccontrols]\n"
            "  %s [--mirrorstamps | --dontmirrorstamps]\n"
            "  %s [--stampsize=[0-10] | --stampsize=default]\n"
            "  %s [--saveoverask | --saveover | --saveovernew]\n"
            "  %s [--nosave | --save]\n"
            "  %s [--autosave | --noautosave]\n"
            "  %s [--savedir DIRECTORY]\n"
            "  %s [--datadir DIRECTORY]\n"
#if defined(WIN32) || defined(__APPLE__)
            "  %s [--printcfg | --noprintcfg]\n"
#endif
            "  %s [--printdelay=SECONDS]\n"
            "  %s [--altprintmod | --altprintalways | --altprintnever]\n"
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
            "  %s [--papersize PAPERSIZE | --papersize help]\n"
#endif
            "  %s [--lang LANGUAGE | --locale LOCALE | --lang help]\n"
            "  %s [--nosysconfig]\n"
            "  %s [--nolockfile]\n"
            "  %s [--colorfile FILE]\n"
            "\n",
            prg, prg,
            blank, blank, blank, blank,
            blank, blank, blank, blank,
            blank, blank, blank, blank,
            blank, blank, blank,
            blank, blank, blank,
            blank, blank, blank, blank, blank, blank, blank, blank, blank,
#ifdef WIN32
            blank,
#endif
            blank, blank,
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
            blank,
#endif
            blank, blank, blank, blank, blank);
    
    free(blank);
}


/* The original Tux Paint canvas was 448x376. The canvas can be
 other sizes now, but many old stamps are sized for the small
 canvas. So, with larger canvases, we must choose a good scale
 factor to compensate. As the canvas size grows, the user will
 want a balance of "more stamps on the screen" and "stamps not
 getting tiny". This will calculate the needed scale factor. */
static unsigned compute_default_scale_factor(double ratio)
{
    double old_diag = sqrt(448 * 448 + 376 * 376);
    double new_diag = sqrt(canvas->w * canvas->w + canvas->h * canvas->h);
    double good_def = ratio * sqrt(new_diag / old_diag);
    double good_log = log(good_def);
    unsigned defsize = HARD_MAX_STAMP_SIZE;
    while (defsize > 0)
    {
        double this_err =
        good_log -
        log(scaletable[defsize].numer / (double) scaletable[defsize].denom);
        double next_err =
        good_log -
        log(scaletable[defsize - 1].numer /
            (double) scaletable[defsize - 1].denom);
        if (fabs(next_err) > fabs(this_err))
            break;
        defsize--;
    }
    return defsize;
}


/* directory walking... */

static void loadbrush_callback(SDL_Surface * screen,
                               const char *restrict const dir,
                               unsigned dirlen, tp_ftw_str * files,
                               unsigned i, char * locale)
{
    FILE * fi;
    char buf[64];
    int want_rand;
    
    dirlen = dirlen;
    
    
    qsort(files, i, sizeof *files, compare_ftw_str);
    while (i--)
    {
        show_progress_bar(screen);
        if (strcasestr(files[i].str, ".png"))
        {
            char fname[512];
            if (strcasecmp(files[i].str, SHAPE_BRUSH_NAME) == 0)
                shape_brush = num_brushes;
            
            snprintf(fname, sizeof fname, "%s%s", dir, files[i].str);
            if (num_brushes == num_brushes_max)
            {
                num_brushes_max = num_brushes_max * 5 / 4 + 4;
                img_brushes =
                realloc(img_brushes, num_brushes_max * sizeof *img_brushes);
                brushes_frames =
                realloc(brushes_frames, num_brushes_max * sizeof(int));
                brushes_directional =
                realloc(brushes_directional, num_brushes_max * sizeof(short));
                brushes_spacing =
                realloc(brushes_spacing, num_brushes_max * sizeof(int));
            }
            img_brushes[num_brushes] = do_loadimage(fname, 1);
            
            
            /* Load brush metadata, if any: */
            
            brushes_frames[num_brushes] = 1;
            brushes_directional[num_brushes] = 0;
            brushes_spacing[num_brushes] = img_brushes[num_brushes]->h / 4;
            
            strcpy(strcasestr(fname, ".png"), ".dat");
            fi = fopen(fname, "r");
            
            want_rand = 0;
            
            if (fi != NULL)
            {
                do
                {
                    fgets(buf, sizeof(buf), fi);
                    
                    if (strstr(buf, "frames=") != NULL)
                    {
                        brushes_frames[num_brushes] =
                        atoi(strstr(buf, "frames=") + 7);
                    }
                    else if (strstr(buf, "spacing=") != NULL)
                    {
                        brushes_spacing[num_brushes] =
                        atoi(strstr(buf, "spacing=") + 8);
                    }
                    else if (strstr(buf, "directional") != NULL)
                    {
                        brushes_directional[num_brushes] = 1;
                    }
                    else if (strstr(buf, "random") != NULL)
                    {
                        want_rand = 1;
                    }
                }
                while (!feof(fi));
                fclose(fi);
                
                if (want_rand)
                    brushes_frames[num_brushes] *= -1;
            }
            
            num_brushes++;
        }
        free(files[i].str);
    }
    free(files);
}



static void load_brush_dir(SDL_Surface * screen, const char *const dir)
{
    char buf[TP_FTW_PATHSIZE];
    unsigned dirlen = strlen(dir);
    
    memcpy(buf, dir, dirlen);
    tp_ftw(screen, buf, dirlen, 0, loadbrush_callback, NULL);
}

SDL_Surface *mirror_surface(SDL_Surface * s)
{
    SDL_Surface *new_surf;
    int x;
    SDL_Rect src, dest;
    
    
    /* Mirror surface: */
    
    new_surf = duplicate_surface(s);
    
    if (new_surf != NULL)
    {
        for (x = 0; x < s->w; x++)
        {
            src.x = x;
            src.y = 0;
            src.w = 1;
            src.h = s->h;
            
            dest.x = s->w - x - 1;
            dest.y = 0;
            
            SDL_BlitSurface(s, &src, new_surf, &dest);
        }
        
        SDL_FreeSurface(s);
        
        return (new_surf);
    }
    else
    {
        return (s);
    }
}

SDL_Surface *flip_surface(SDL_Surface * s)
{
    SDL_Surface *new_surf;
    int y;
    SDL_Rect src, dest;
    
    
    /* Flip surface: */
    
    new_surf = duplicate_surface(s);
    
    if (new_surf != NULL)
    {
        for (y = 0; y < s->h; y++)
        {
            src.x = 0;
            src.y = y;
            src.w = s->w;
            src.h = 1;
            
            dest.x = 0;
            dest.y = s->h - y - 1;
            
            SDL_BlitSurface(s, &src, new_surf, &dest);
        }
        
        SDL_FreeSurface(s);
        
        return (new_surf);
    }
    else
    {
        return (s);
    }
}

static unsigned default_stamp_size;

static void loadstamp_finisher(stamp_type * sd, unsigned w, unsigned h,
                               double ratio)
{
    unsigned int upper = HARD_MAX_STAMP_SIZE;
    unsigned int lower = 0;
    unsigned mid;
    
    /* If Tux Paint is in mirror-image-by-default mode, mirror, if we can: */
    if (mirrorstamps && sd->mirrorable)
        sd->mirrored = 1;
    
    do
    {
        scaleparams *s = &scaletable[upper];
        int pw, ph;			/* proposed width and height */
        
        pw = (w * s->numer + s->denom - 1) / s->denom;
        ph = (h * s->numer + s->denom - 1) / s->denom;
        
        /* OK to let a stamp stick off the sides in one direction, not two */
        if (pw < canvas->w * 2 && ph < canvas->h * 1)
            break;
        if (pw < canvas->w * 1 && ph < canvas->h * 2)
            break;
    }
    while (--upper);
    
    do
    {
        scaleparams *s = &scaletable[lower];
        int pw, ph;			/* proposed width and height */
        
        pw = (w * s->numer + s->denom - 1) / s->denom;
        ph = (h * s->numer + s->denom - 1) / s->denom;
        
        if (pw * ph > 20)
            break;
    }
    while (++lower < HARD_MAX_STAMP_SIZE);
    
    if (upper < lower)
    {
        /* this, if it ever happens, is very bad */
        upper = (upper + lower) / 2;
        lower = upper;
    }
    
    mid = default_stamp_size;
    if (ratio != 1.0)
        mid = compute_default_scale_factor(ratio);
    
    if (mid > upper)
        mid = upper;
    
    if (mid < lower)
        mid = lower;
    
    sd->min = lower;
    sd->size = mid;
    sd->max = upper;
    
    if (stamp_size_override != -1)
    {
        sd->size = (((upper - lower) * stamp_size_override) / 10) + lower;
    }
}


/* Note: must have read the *.dat file before calling this */
static void set_active_stamp(void)
{
    stamp_type *sd = stamp_data[stamp_group][cur_stamp[stamp_group]];
    unsigned len = strlen(sd->stampname);
    char *buf = alloca(len + strlen("_mirror_flip.EXT") + 1);
    int needs_mirror, needs_flip;
    
    if (active_stamp)
        SDL_FreeSurface(active_stamp);
    active_stamp = NULL;
    
    memcpy(buf, sd->stampname, len);
    
#ifdef DEBUG
    printf("\nset_active_stamp()\n");
#endif
    
    /* Look for pre-mirrored and pre-flipped version: */
    
    needs_mirror = sd->mirrored;
    needs_flip = sd->flipped;
    
    if (sd->mirrored && sd->flipped)
    {
        /* Want mirrored and flipped, both */
        
#ifdef DEBUG
        printf("want both mirrored & flipped\n");
#endif
        
        if (!sd->no_premirrorflip)
        {
#ifndef NOSVG
            memcpy(buf + len, "_mirror_flip.svg", 17);
            active_stamp = do_loadimage(buf, 0);
#endif
            
            if (active_stamp == NULL)
            {
                memcpy(buf + len, "_mirror_flip.png", 17);
                active_stamp = do_loadimage(buf, 0);
            }
        }
        
        
        if (active_stamp != NULL)
        {
#ifdef DEBUG
            printf("found a _mirror_flip!\n");
#endif
            
            needs_mirror = 0;
            needs_flip = 0;
        }
        else
        {
            /* Couldn't get one that was both, look for _mirror then _flip and
             flip or mirror it: */
            
#ifdef DEBUG
            printf("didn't find a _mirror_flip\n");
#endif
            
            if (!sd->no_premirror)
            {
#ifndef NOSVG
                memcpy(buf + len, "_mirror.svg", 12);
                active_stamp = do_loadimage(buf, 0);
#endif
                
                if (active_stamp == NULL)
                {
                    memcpy(buf + len, "_mirror.png", 12);
                    active_stamp = do_loadimage(buf, 0);
                }
            }
            
            if (active_stamp != NULL)
            {
#ifdef DEBUG
                printf("found a _mirror!\n");
#endif
                needs_mirror = 0;
            }
            else
            {
                /* Couldn't get one that was just pre-mirrored, look for a
                 pre-flipped */
                
#ifdef DEBUG
                printf("didn't find a _mirror, either\n");
#endif
                
                if (!sd->no_preflip)
                {
#ifndef NOSVG
                    memcpy(buf + len, "_flip.svg", 10);
                    active_stamp = do_loadimage(buf, 0);
#endif
                    
                    if (active_stamp == NULL)
                    {
                        memcpy(buf + len, "_flip.png", 10);
                        active_stamp = do_loadimage(buf, 0);
                    }
                }
                
                if (active_stamp != NULL)
                {
#ifdef DEBUG
                    printf("found a _flip!\n");
#endif
                    needs_flip = 0;
                }
                else
                {
#ifdef DEBUG
                    printf("didn't find a _flip, either\n");
#endif
                }
            }
        }
    }
    else if (sd->flipped && !sd->no_preflip)
    {
        /* Want flipped only */
        
#ifdef DEBUG
        printf("want flipped only\n");
#endif
        
#ifndef NOSVG
        memcpy(buf + len, "_flip.svg", 10);
        active_stamp = do_loadimage(buf, 0);
#endif
        
        if (active_stamp == NULL)
        {
            memcpy(buf + len, "_flip.png", 10);
            active_stamp = do_loadimage(buf, 0);
        }
        
        if (active_stamp != NULL)
        {
#ifdef DEBUG
            printf("found a _flip!\n");
#endif
            needs_flip = 0;
        }
        else
        {
#ifdef DEBUG
            printf("didn't find a _flip\n");
#endif
        }
    }
    else if (sd->mirrored && !sd->no_premirror)
    {
        /* Want mirrored only */
        
#ifdef DEBUG
        printf("want mirrored only\n");
#endif
        
#ifndef NOSVG
        memcpy(buf + len, "_mirror.svg", 12);
        active_stamp = do_loadimage(buf, 0);
#endif
        
        if (active_stamp == NULL)
        {
            memcpy(buf + len, "_mirror.png", 12);
            active_stamp = do_loadimage(buf, 0);
        }
        
        if (active_stamp != NULL)
        {
#ifdef DEBUG
            printf("found a _mirror!\n");
#endif
            needs_mirror = 0;
        }
        else
        {
#ifdef DEBUG
            printf("didn't find a _mirror\n");
#endif
        }
    }
    
    
    /* Didn't want mirrored, or flipped, or couldn't load anything
     that was pre-rendered: */
    
    if (!active_stamp)
    {
#ifdef DEBUG
        printf("loading normal\n");
#endif
        
#ifndef NOSVG
        memcpy(buf + len, ".svg", 5);
        active_stamp = do_loadimage(buf, 0);
#endif
        
        if (active_stamp == NULL)
        {
            memcpy(buf + len, ".png", 5);
            active_stamp = do_loadimage(buf, 0);
        }
        
    }
    
    /* Never allow a NULL image! */
    
    if (!active_stamp)
        active_stamp = thumbnail(img_dead40x40, 40, 40, 1);	/* copy it */
    
    
    /* If we wanted mirrored or flipped, and didn't get something pre-rendered,
     do it to the image we did load: */
    
    if (needs_mirror)
    {
#ifdef DEBUG
        printf("mirroring\n");
#endif
        active_stamp = mirror_surface(active_stamp);
    }
    
    if (needs_flip)
    {
#ifdef DEBUG
        printf("flipping\n");
#endif
        active_stamp = flip_surface(active_stamp);
    }
    
#ifdef DEBUG
    printf("\n\n");
#endif
}

static void get_stamp_thumb(stamp_type * sd)
{
    SDL_Surface *bigimg = NULL;
    unsigned len = strlen(sd->stampname);
    char *buf = alloca(len + strlen("_mirror_flip.EXT") + 1);
    int need_mirror, need_flip;
    double ratio;
    unsigned w;
    unsigned h;
    
#ifdef DEBUG
    printf("\nget_stamp_thumb()\n");
#endif
    
    memcpy(buf, sd->stampname, len);
    
    if (!sd->processed)
    {
        memcpy(buf + len, ".dat", 5);
        ratio = loadinfo(buf, sd);
    }
    else
        ratio = 1.0;
    
#ifndef NOSOUND
    /* good time to load the sound */
    if (!sd->no_sound && !sd->ssnd && use_sound)
    {
        /* damn thing wants a .png extension; give it one */
        memcpy(buf + len, ".png", 5);
        sd->ssnd = loadsound(buf);
        sd->no_sound = !sd->ssnd;
    }
    
    /* ...and the description */
    if (!sd->no_descsound && !sd->sdesc && use_sound)
    {
        /* damn thing wants a .png extension; give it one */
        memcpy(buf + len, ".png", 5);
        sd->sdesc = loaddescsound(buf);
        sd->no_descsound = !sd->sdesc;
    }
#endif
    
    if (!sd->no_txt && !sd->stxt)
    {
        /* damn thing wants a .png extension; give it one */
        memcpy(buf + len, ".png", 5);
        sd->stxt = loaddesc(buf, &(sd->locale_text));
        sd->no_txt = !sd->stxt;
    }
    
    
    /* first see if we can re-use an existing thumbnail */
    if (sd->thumbnail)
    {
#ifdef DEBUG
        printf("have an sd->thumbnail\n");
#endif
        
        if (sd->thumb_mirrored_flipped == sd->flipped &&
            sd->thumb_mirrored_flipped == sd->mirrored &&
            sd->mirrored == sd->thumb_mirrored &&
            sd->flipped == sd->thumb_flipped)
        {
            /* It's already the way we want */
            
#ifdef DEBUG
            printf("mirrored == flipped == thumb_mirrored_flipped [bye]\n");
#endif
            
            return;
        }
    }
    
    
    /* nope, see if there's a pre-rendered one we can use */
    
    need_mirror = sd->mirrored;
    need_flip = sd->flipped;
    bigimg = NULL;
    
    if (sd->mirrored && sd->flipped)
    {
#ifdef DEBUG
        printf("want mirrored & flipped\n");
#endif
        
        if (!sd->no_premirrorflip)
        {
            memcpy(buf + len, "_mirror_flip.png", 17);
            bigimg = do_loadimage(buf, 0);
            
#ifndef NOSVG
            if (bigimg == NULL)
            {
                memcpy(buf + len, "_mirror_flip.svg", 17);
                bigimg = do_loadimage(buf, 0);
            }
#endif
        }
        
        if (bigimg)
        {
#ifdef DEBUG
            printf("found a _mirror_flip!\n");
#endif
            
            need_mirror = 0;
            need_flip = 0;
        }
        else
        {
#ifdef DEBUG
            printf("didn't find a mirror_flip\n");
#endif
            sd->no_premirrorflip = 1;
            
            if (!sd->no_premirror)
            {
                memcpy(buf + len, "_mirror.png", 12);
                bigimg = do_loadimage(buf, 0);
                
#ifndef NOSVG
                if (bigimg == NULL)
                {
                    memcpy(buf + len, "_mirror.svg", 12);
                    bigimg = do_loadimage(buf, 0);
                }
#endif
            }
            
            if (bigimg)
            {
#ifdef DEBUG
                printf("found a _mirror\n");
#endif
                
                need_mirror = 0;
            }
            else
            {
#ifdef DEBUG
                printf("didn't find a mirror\n");
#endif
                
                if (!sd->no_preflip)
                {
                    memcpy(buf + len, "_flip.png", 10);
                    bigimg = do_loadimage(buf, 0);
                    
#ifndef NOSVG
                    if (bigimg == NULL)
                    {
                        memcpy(buf + len, "_flip.svg", 10);
                        bigimg = do_loadimage(buf, 0);
                    }
#endif
                }
                
                if (bigimg)
                {
#ifdef DEBUG
                    printf("found a _flip\n");
#endif
                    
                    need_flip = 0;
                }
            }
        }
    }
    else if (sd->mirrored && !sd->no_premirror)
    {
#ifdef DEBUG
        printf("want mirrored only\n");
#endif
        
        memcpy(buf + len, "_mirror.png", 12);
        bigimg = do_loadimage(buf, 0);
        
#ifndef NOSVG
        if (bigimg == NULL)
        {
            memcpy(buf + len, "_mirror.svg", 12);
            bigimg = do_loadimage(buf, 0);
        }
#endif
        
        if (bigimg)
        {
#ifdef DEBUG
            printf("found a _mirror!\n");
#endif
            need_mirror = 0;
        }
        else
        {
#ifdef DEBUG
            printf("didn't find a mirror\n");
#endif
            sd->no_premirror = 1;
        }
    }
    else if (sd->flipped && !sd->no_preflip)
    {
#ifdef DEBUG
        printf("want flipped only\n");
#endif
        
        memcpy(buf + len, "_flip.png", 10);
        bigimg = do_loadimage(buf, 0);
        
#ifndef NOSVG
        if (bigimg == NULL)
        {
            memcpy(buf + len, "_flip.svg", 10);
            bigimg = do_loadimage(buf, 0);
        }
#endif
        
        if (bigimg)
        {
#ifdef DEBUG
            printf("found a _flip!\n");
#endif
            need_flip = 0;
        }
        else
        {
#ifdef DEBUG
            printf("didn't find a flip\n");
#endif
            sd->no_preflip = 1;
        }
    }
    
    
    /* If we didn't load a pre-rendered, load the normal one: */
    
    if (!bigimg)
    {
#ifdef DEBUG
        printf("loading normal...\n");
#endif
        
        memcpy(buf + len, ".png", 5);
        bigimg = do_loadimage(buf, 0);
        
#ifndef NOSVG
        if (bigimg == NULL)
        {
            memcpy(buf + len, ".svg", 5);
            bigimg = do_loadimage(buf, 0);
        }
#endif
    }
    
    
    /* Scale the stamp down to its thumbnail size: */
    
    w = 40;
    h = 40;
    if (bigimg)
    {
        w = bigimg->w;
        h = bigimg->h;
    }
    
    if (!bigimg)
        sd->thumbnail = thumbnail(img_dead40x40, 40, 40, 1);	/* copy */
    else if (bigimg->w > 40 || bigimg->h > 40)
    {
        sd->thumbnail = thumbnail(bigimg, 40, 40, 1);
        SDL_FreeSurface(bigimg);
    }
    else
        sd->thumbnail = bigimg;
    
    
    /* Mirror and/or flip the thumbnail, if we still need to do so: */
    
    if (need_mirror)
    {
#ifdef DEBUG
        printf("mirroring\n");
#endif
        sd->thumbnail = mirror_surface(sd->thumbnail);
    }
    
    if (need_flip)
    {
#ifdef DEBUG
        printf("flipping\n");
#endif
        sd->thumbnail = flip_surface(sd->thumbnail);
    }
    
    
    /* Note the fact that the thumbnail's mirror/flip is the same as the main
     stamp: */
    
    if (sd->mirrored && sd->flipped)
        sd->thumb_mirrored_flipped = 1;
    else
        sd->thumb_mirrored_flipped = 0;
    
    sd->thumb_mirrored = sd->mirrored;
    sd->thumb_flipped = sd->flipped;
    
#ifdef DEBUG
    printf("\n\n");
#endif
    
    
    /* Finish up, if we need to: */
    
    if (sd->processed)
        return;
    
    sd->processed = 1;		/* not really, but on the next line... */
    loadstamp_finisher(sd, w, h, ratio);
}


static void loadstamp_callback(SDL_Surface * screen,
                               const char *restrict const dir,
                               unsigned dirlen, tp_ftw_str * files,
                               unsigned i, char * locale)
{
#ifdef DEBUG
    printf("loadstamp_callback: %s\n", dir);
#endif
    
    if (num_stamps[stamp_group] > 0)
    {
        /* If previous group had any stamps... */
        
        unsigned int i, slashcount;
        
        
        /* See if the current directory is shallow enough to be
         important for making a new stamp group: */
        
        slashcount = 0;
        
        for (i = strlen(load_stamp_basedir) + 1; i < strlen(dir); i++)
        {
            if (dir[i] == '/' || dir[i] == '\\')
                slashcount++;
        }
        
        if (slashcount <= stamp_group_dir_depth)
        {
            stamp_group++;
#ifdef DEBUG
            printf("\n...counts as a new group! now: %d\n", stamp_group);
#endif
        }
        else
        {
#ifdef DEBUG
            printf("...is still part of group %d\n", stamp_group);
#endif
        }
    }
    
    
    /* Sort and iterate the file list: */
    
    qsort(files, i, sizeof *files, compare_ftw_str);
    while (i--)
    {
        char fname[512];
        const char *dotext, *ext, *mirror_ext, *flip_ext, *mirrorflip_ext;
        
        ext = ".png";
        mirror_ext = "_mirror.png";
        flip_ext = "_flip.png";
        mirrorflip_ext = "_mirror_flip.png";
        dotext = (char *) strcasestr(files[i].str, ext);
        
#ifndef NOSVG
        if (dotext == NULL)
        {
            ext = ".svg";
            mirror_ext = "_mirror.svg";
            flip_ext = "_flip.svg";
            mirrorflip_ext = "_mirror_flip.svg";
            dotext = (char *) strcasestr(files[i].str, ext);
        }
        else
        {
            /* Found PNG, but we support SVG; let's see if there's an SVG
             *       version too, before loading the PNG */
            
            char svgname[512];
            FILE * fi;
            
            snprintf(svgname, sizeof(svgname), "%s/%s", dir, files[i].str);
            strcpy(strcasestr(svgname, ".png"), ".svg");
            
            fi = fopen(svgname, "r");
            if (fi != NULL)
            {
                debug("Found SVG version of ");
                debug(files[i].str);
                debug("\n");
                
                fclose(fi);
                continue;  /* ugh, i hate continues */
            }
        }
#endif
        
        show_progress_bar(screen);
        
        if (dotext > files[i].str && !strcasecmp(dotext, ext)
            && (dotext - files[i].str + 1 + dirlen < sizeof fname)
            && !strcasestr(files[i].str, mirror_ext)
            && !strcasestr(files[i].str, flip_ext)
            && !strcasestr(files[i].str, mirrorflip_ext))
        {
            snprintf(fname, sizeof fname, "%s/%s", dir, files[i].str);
            if (num_stamps[stamp_group] == max_stamps[stamp_group])
            {
                max_stamps[stamp_group] = max_stamps[stamp_group] * 5 / 4 + 15;
                stamp_data[stamp_group] = realloc(stamp_data[stamp_group],
                                                  max_stamps[stamp_group] * sizeof(*stamp_data[stamp_group]));
            }
            stamp_data[stamp_group][num_stamps[stamp_group]] =
            calloc(1, sizeof *stamp_data[stamp_group][num_stamps[stamp_group]]);
            stamp_data[stamp_group][num_stamps[stamp_group]]->stampname =
            malloc(dotext - files[i].str + 1 + dirlen + 1);
            memcpy(stamp_data[stamp_group][num_stamps[stamp_group]]->stampname, fname,
                   dotext - files[i].str + 1 + dirlen);
            stamp_data[stamp_group][num_stamps[stamp_group]]->stampname[dotext - files[i].str +
                                                                        1 + dirlen] = '\0';
            num_stamps[stamp_group]++;
        }
        free(files[i].str);
    }
    free(files);
}



static void load_stamp_dir(SDL_Surface * screen, const char *const dir)
{
    char buf[TP_FTW_PATHSIZE];
    unsigned dirlen = strlen(dir);
    memcpy(buf, dir, dirlen);
    load_stamp_basedir = dir;
    tp_ftw(screen, buf, dirlen, 0, loadstamp_callback, NULL);
}


static void load_stamps(SDL_Surface * screen)
{
#ifdef __IPHONEOS__
    extern const char* DATA_PREFIX;
#endif
    
    char *homedirdir = get_fname("stamps", DIR_DATA);
    
    default_stamp_size = compute_default_scale_factor(1.0);
    
    load_stamp_dir(screen, homedirdir);
    char temp[1024];
    sprintf(temp, "%s/%s", DATA_PREFIX, "stamps");
    load_stamp_dir(screen, temp);
#ifdef __APPLE__
    load_stamp_dir(screen, "/Library/Application Support/TuxPaint/stamps");
#endif
#ifdef WIN32
    free(homedirdir);
    homedirdir = get_fname("data/stamps", DIR_DATA);
    load_stamp_dir(screen, homedirdir);
#endif
    
    if (num_stamps[0] == 0)
    {
        //    fprintf(stderr,
        //	    "\nWarning: No stamps found in " DATA_PREFIX "stamps/\n"
        //	    "or %s\n\n", homedirdir);
    }
    
    num_stamp_groups = stamp_group + 1;
    
    free(homedirdir);
}

#ifndef FORKED_FONTS
static int load_user_fonts_stub(void *vp)
{
    return load_user_fonts(screen, vp, NULL);
}
#endif


#define hex2dec(c) (((c) >= '0' && (c) <= '9') ? ((c) - '0') : \
((c) >= 'A' && (c) <= 'F') ? ((c) - 'A' + 10) : \
((c) >= 'a' && (c) <= 'f') ? ((c) - 'a' + 10) : 0)

/* Setup: */

static void setup(int argc, char *argv[])
{
#ifdef __IPHONEOS__
    extern const char* DATA_PREFIX;
#endif
    
    int i, j, ok_to_use_sysconfig;
    char str[128];
    char *upstr;
    SDL_Color black = { 0, 0, 0, 0 };
    char *homedirdir;
    FILE *fi;
    SDL_Surface *tmp_surf;
    SDL_Rect dest;
    int scale;
#ifndef LOW_QUALITY_COLOR_SELECTOR
    int x, y;
    SDL_Surface *tmp_btn_up;
    SDL_Surface *tmp_btn_down;
    Uint8 r, g, b;
#endif
    SDL_Surface *tmp_imgcurup, *tmp_imgcurdown;
    Uint32 init_flags;
    char tmp_str[128];
    SDL_Surface *img1;
    Uint32(*getpixel_tmp_btn_up) (SDL_Surface *, int, int);
    Uint32(*getpixel_tmp_btn_down) (SDL_Surface *, int, int);
    Uint32(*getpixel_img_paintwell) (SDL_Surface *, int, int);
    int big_title;
    
    
    
#if defined(__BEOS__) || defined(WIN32)
    /* if run from gui, like OpenTracker in BeOS or Explorer in Windows,
     find path from which binary was run and change dir to it
     so all files will be local :) */
    /* UPDATE (2004.10.06): Since SDL 1.2.7 SDL sets that path correctly,
     so this code wouldn't be needed if SDL was init before anything else,
     (just basic init, window shouldn't be needed). */
    /* UPDATE (2005 July 19): Enable and make work on Windows. Makes testing
     with MINGW/MSYS easier */
    
    if (argc && argv[0])
    {
        char *app_path = strdup(argv[0]);
        char *slash = strrchr(app_path, '/');
        
        if (!slash)
        {
            slash = strrchr(app_path, '\\');
        }
        if (slash)
        {
            *(slash + 1) = '\0';
            chdir(app_path);
        }
        free(app_path);
    }
#endif
    
    
    /* Set default options: */
    
    use_sound = 1;
    mute = 0;
#ifdef NOKIA_770
    fullscreen = 1;
#else
    fullscreen = 0;
#endif
    disable_screensaver = 0;
    native_screensize = 0;
    rotate_orientation = 0;
    noshortcuts = 0;
    dont_do_xor = 0;
    keymouse = 0;
    wheely = 1;
    no_button_distinction = 0;
    grab_input = 0;
#ifdef NOKIA_770
    simple_shapes = 1;
    no_fancy_cursors = 1;
    hide_cursor = 1;
#else
    simple_shapes = 0;
    no_fancy_cursors = 0;
    hide_cursor = 0;
#endif
    stamp_size_override = -1;
    only_uppercase = 0;
    alt_print_command_default = ALTPRINT_MOD;
    print_delay = 0;
    use_print_config = 1;
    disable_print = 0;
    disable_quit = 0;
    disable_save = 0;
    promptless_save = SAVE_OVER_PROMPT;
    autosave_on_quit = 0;
    dont_load_stamps = 0;
    no_system_fonts = 1;
    all_locale_fonts = 0;
    mirrorstamps = 0;
    disable_stamp_controls = 0;
    disable_magic_controls = 0;
    
#ifndef WINDOW_WIDTH
    WINDOW_WIDTH = 1024;
    WINDOW_HEIGHT = 768;
#endif
    
#ifdef NOKIA_770
    WINDOW_WIDTH = 800;
    WINDOW_HEIGHT = 480;
#endif
    
#ifdef OLPC_XO
    /* ideally we'd support rotation and 2x scaling */
    WINDOW_WIDTH = 1200;
    WINDOW_HEIGHT = 900;
#endif
    ok_to_use_lockfile = 0;
    start_blank = 0;
    colorfile[0] = '\0';
    
    
#ifdef __BEOS__
    /* Fancy cursors on BeOS are buggy in SDL */
    
    no_fancy_cursors = 1;
#endif
    
    
#ifdef WIN32
    /* Windows */
    
    savedir = GetDefaultSaveDir("TuxPaint");
    datadir = GetDefaultSaveDir("TuxPaint");
#elif __BEOS__
    /* BeOS */
    
    savedir = strdup("./userdata");
    datadir = strdup("./userdata");
#elif __APPLE__
    /* Mac OS X */
    extern char *user_doc_dir;
    savedir = user_doc_dir;
    datadir = user_doc_dir;
#else
    /* Linux */
    
    if (getenv("HOME") != NULL)
    {
        char tmp[MAX_PATH];
        
        snprintf(tmp, MAX_PATH, "%s/%s", getenv("HOME"), ".tuxpaint");
        
        savedir = strdup(tmp);
        datadir = strdup(tmp);
    }
    else
    {
        /* Woah, don't know where $HOME is? */
        
        fprintf(stderr, "Error: You have no $HOME environment variable!\n");
        exit(1);
    }
#endif
    
    
    /* Load options from global config file: */
    
    
    /* Check to see if it's ok first: */
    
    ok_to_use_sysconfig = 1;
    
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--nosysconfig") == 0)
        {
            ok_to_use_sysconfig = 0;
            i = argc;			/* aka break; */
        }
    }
    
    
    if (ok_to_use_sysconfig)
    {
#ifndef WIN32
        snprintf(str, sizeof(str), "%s/tuxpaint.conf", CONFDIR);
#else
        /* Global config file in the application directory on Windows */
        strcpy(str, "tuxpaint.cfg");
#endif
        
        fi = fopen(str, "r");
        if (fi != NULL)
        {
            parse_options(fi);
            fclose(fi);
        }
        else
            debug(str);
    }
    
    
    
    /* Load options from user's own configuration (".rc" / ".cfg") file: */
    
#if defined(WIN32)
    /* Default local config file in users savedir directory on Windows */
    snprintf(str, sizeof(str), "%s/tuxpaint.cfg", savedir); /* FIXME */
#elif defined(__BEOS__)
    /* BeOS: Use a "tuxpaint.cfg" file: */
    
    strcpy(str, "tuxpaint.cfg");
    
#elif defined(__APPLE__)
    /* Mac OS X: Use a "tuxpaint.cfg" file in the Tux Paint application support folder */
    snprintf(str, sizeof(str), "%s/tuxpaint.cfg", macosx.preferencesPath);
    
#else
    /* Linux and other Unixes:  Use 'rc' style (~/.tuxpaintrc) */
    
    if (getenv("HOME") != NULL)
    {
        /* Should it be "~/.tuxpaint/tuxpaintrc" instead???
         Comments welcome ... bill@newbreedsoftware.com */
        
        snprintf(str, sizeof(str), "%s/.tuxpaintrc", getenv("HOME"));
    }
    else
    {
        /* WOAH! We don't know what our home directory is!? Last resort,
         do it Windows/BeOS way: */
        
        strcpy(str, "tuxpaint.cfg");
    }
#endif
    
    
    fi = fopen(str, "r");
    if (fi != NULL)
    {
        parse_options(fi);
        fclose(fi);
    }
    else
        debug(str);
    
    
    /* Handle command-line arguments: */
    
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--fullscreen") == 0 || strcmp(argv[i], "-f") == 0)
        {
            fullscreen = 1;
        }
        else if (strcmp(argv[i], "--windowed") == 0 || strcmp(argv[i], "-w") == 0)
        {
            fullscreen = 0;
        }
        else if (strcmp(argv[i], "--disablescreensaver") == 0)
        {
            disable_screensaver = 1;
        }
        else if (strcmp(argv[i], "--allowscreensaver") == 0)
        {
            disable_screensaver = 0;
        }
        else if (strcmp(argv[i], "--startblank") == 0 || strcmp(argv[i], "-b") == 0)
        {
            start_blank = 1;
        }
        else if (strcmp(argv[i], "--startlast") == 0)
        {
            start_blank = 0;
        }
        else if (strcmp(argv[i], "--mirrorstamps") == 0)
        {
            mirrorstamps = 1;
        }
        else if (strcmp(argv[i], "--dontmirrorstamps") == 0)
        {
            mirrorstamps = 0;
        }
        else if (strcmp(argv[i], "--nostampcontrols") == 0)
        {
            disable_stamp_controls = 1;
        }
        else if (strcmp(argv[i], "--stampcontrols") == 0)
        {
            disable_stamp_controls = 0;
        }
        else if (strcmp(argv[i], "--nomagiccontrols") == 0)
        {
            disable_magic_controls = 1;
        }
        else if (strcmp(argv[i], "--magiccontrols") == 0)
        {
            disable_magic_controls = 0;
        }
        else if (strcmp(argv[i], "--noshortcuts") == 0)
        {
            noshortcuts = 1;
        }
        else if (strcmp(argv[i], "--shortcuts") == 0)
        {
            noshortcuts = 0;
        }
        else if (strcmp(argv[i], "--colorfile") == 0)
        {
            if (i < argc - 1)
            {
                strcpy(colorfile, argv[i + 1]);
                i++;
            }
            else
            {
                /* Forgot to specify the file name! */
                
                fprintf(stderr, "%s takes an argument\n", argv[i]);
                show_usage(stderr, (char *) getfilename(argv[0]));
                exit(1);
            }
        }
        else if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i][2] >= '1'
                 && argv[i][2] <= '9')
        {
            char *endp1;
            char *endp2;
            int w, h;
            w = strtoul(argv[i] + 2, &endp1, 10);
            h = strtoul(endp1 + 1, &endp2, 10);
            /* sanity check it */
            if (argv[i] + 2 == endp1 || endp1 + 1 == endp2 || *endp1 != 'x'
                || *endp2 || w < 500 || h < 480 || h > w * 3 || w > h * 4)
            {
                show_usage(stderr, (char *) getfilename(argv[0]));
                exit(1);
            }
            WINDOW_WIDTH = w;
            WINDOW_HEIGHT = h;
        }
        else if (strcmp(argv[i], "--native") == 0)
        {
            native_screensize = 1;
        }
        else if (strcmp(argv[i], "--orient=portrait") == 0)
        {
            rotate_orientation = 1;
        }
        else if (strcmp(argv[i], "--orient=landscape") == 0)
        {
            rotate_orientation = 0;
        }
        else if (strcmp(argv[i], "--nooutlines") == 0)
        {
            dont_do_xor = 1;
        }
        else if (strcmp(argv[i], "--outlines") == 0)
        {
            dont_do_xor = 0;
        }
        else if (strcmp(argv[i], "--keyboard") == 0)
        {
            keymouse = 1;
        }
        else if (strcmp(argv[i], "--mouse") == 0)
        {
            keymouse = 0;
        }
        else if (strcmp(argv[i], "--nowheelmouse") == 0)
        {
            wheely = 0;
        }
        else if (strcmp(argv[i], "--wheelmouse") == 0)
        {
            wheely = 1;
        }
        else if (strcmp(argv[i], "--grab") == 0)
        {
            grab_input = 1;
        }
        else if (strcmp(argv[i], "--dontgrab") == 0)
        {
            grab_input = 0;
        }
        else if (strcmp(argv[i], "--nofancycursors") == 0)
        {
            no_fancy_cursors = 1;
        }
        else if (strcmp(argv[i], "--fancycursors") == 0)
        {
            no_fancy_cursors = 0;
        }
        else if (strcmp(argv[i], "--hidecursor") == 0)
        {
            hide_cursor = 1;
        }
        else if (strcmp(argv[i], "--showcursor") == 0)
        {
            hide_cursor = 0;
        }
        else if (strcmp(argv[i], "--saveover") == 0)
        {
            promptless_save = SAVE_OVER_ALWAYS;
        }
        else if (strcmp(argv[i], "--saveoverask") == 0)
        {
            promptless_save = SAVE_OVER_PROMPT;
        }
        else if (strcmp(argv[i], "--saveovernew") == 0)
        {
            promptless_save = SAVE_OVER_NO;
        }
        else if (strcmp(argv[i], "--autosave") == 0)
        {
            autosave_on_quit = 1;
        }
        else if (strcmp(argv[i], "--noautosave") == 0)
        {
            autosave_on_quit = 0;
        }
        else if (strcmp(argv[i], "--altprintnever") == 0)
        {
            alt_print_command_default = ALTPRINT_NEVER;
        }
        else if (strcmp(argv[i], "--altprintalways") == 0)
        {
            alt_print_command_default = ALTPRINT_ALWAYS;
        }
        else if (strcmp(argv[i], "--altprintmod") == 0)
        {
            alt_print_command_default = ALTPRINT_MOD;
        }
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
        else if (strstr(argv[i], "--papersize=") == argv[i])
        {
            papersize = strdup(argv[i] + strlen("--papersize="));
        }
        else if (strcmp(argv[i], "--papersize") == 0)
        {
            if (i + 1 < argc)
            {
                i++;
                if (strcmp(argv[i], "help") == 0)
                {
                    show_available_papersizes(stdout, argv[0]);
                    exit(0);
                }
                else
                    papersize = strdup(argv[i]);
            }
            else
            {
                fprintf(stderr, "%s takes an argument\n", argv[i]);
                show_available_papersizes(stderr, argv[0]);
                exit(1);
            }
        }
#endif
        else if (strcmp(argv[i], "--uppercase") == 0
                 || strcmp(argv[i], "-u") == 0)
        {
            only_uppercase = 1;
        }
        else if (strcmp(argv[i], "--mixedcase") == 0
                 || strcmp(argv[i], "-m") == 0)
        {
            only_uppercase = 0;
        }
        else if (strcmp(argv[i], "--simpleshapes") == 0 ||
                 strcmp(argv[i], "-s") == 0)
        {
            simple_shapes = 1;
        }
        else if (strcmp(argv[i], "--complexshapes") == 0)
        {
            simple_shapes = 0;
        }
        else if (strcmp(argv[i], "--noquit") == 0 || strcmp(argv[i], "-x") == 0)
        {
            disable_quit = 1;
        }
        else if (strcmp(argv[i], "--quit") == 0)
        {
            disable_quit = 0;
        }
        else if (strcmp(argv[i], "--nosave") == 0)
        {
            disable_save = 1;
        }
        else if (strcmp(argv[i], "--save") == 0)
        {
            disable_save = 0;
        }
        else if (strcmp(argv[i], "--nostamps") == 0)
        {
            dont_load_stamps = 1;
        }
        else if (strcmp(argv[i], "--stamps") == 0)
        {
            dont_load_stamps = 0;
        }
        else if (strcmp(argv[i], "--nosysfonts") == 0)
        {
            no_system_fonts = 1;
        }
        else if (strcmp(argv[i], "--nobuttondistinction") == 0)
        {
            no_button_distinction = 1;
        }
        else if (strcmp(argv[i], "--buttondistinction") == 0)
        {
            no_button_distinction = 0;
        }
        else if (strcmp(argv[i], "--sysfonts") == 0)
        {
            no_system_fonts = 0;
        }
        else if (strcmp(argv[i], "--alllocalefonts") == 0)
        {
            all_locale_fonts = 1;
        }
        else if (strcmp(argv[i], "--currentlocalefont") == 0)
        {
            all_locale_fonts = 0;
        }
        else if (strcmp(argv[i], "--noprint") == 0 || strcmp(argv[i], "-p") == 0)
        {
            disable_print = 1;
        }
        else if (strcmp(argv[i], "--print") == 0)
        {
            disable_print = 0;
        }
        else if (strcmp(argv[i], "--noprintcfg") == 0)
        {
#if !defined(WIN32) && !defined(__APPLE__)
            fprintf(stderr, "Note: printcfg option only applies to Windows and Mac OS X!\n");
#endif
            use_print_config = 0;
        }
        else if (strcmp(argv[i], "--printcfg") == 0)
        {
#if !defined(WIN32) && !defined(__APPLE__)
            fprintf(stderr, "Note: printcfg option only applies to Windows and Mac OS X!\n");
#endif
            use_print_config = 1;
        }
        else if (strstr(argv[i], "--printdelay=") == argv[i])
        {
            sscanf(strstr(argv[i], "--printdelay=") + 13, "%d", &print_delay);
#ifdef DEBUG
            printf("Print delay set to %d seconds\n", print_delay);
#endif
        }
        else if (strcmp(argv[i], "--nosound") == 0 || strcmp(argv[i], "-q") == 0)
        {
            use_sound = 0;
        }
        else if (strcmp(argv[i], "--sound") == 0)
        {
            use_sound = 1;
        }
        else if (strcmp(argv[i], "--locale") == 0 || strcmp(argv[i], "-L") == 0)
        {
            if (i < argc - 1)
            {
                do_locale_option(argv[++i]);
            }
            else
            {
                /* Forgot to specify the language (locale)! */
                
                fprintf(stderr, "%s takes an argument\n", argv[i]);
                show_locale_usage(stderr, (char *) getfilename(argv[0]));
                exit(1);
            }
        }
        else if (strstr(argv[i], "--lang=") == argv[i])
        {
            set_langstr(argv[i] + 7);
        }
        else if (strcmp(argv[i], "--lang") == 0 || strcmp(argv[i], "-l") == 0)
        {
            if (i < argc - 1)
            {
                set_langstr(argv[i + 1]);
                i++;
            }
            else
            {
                /* Forgot to specify the language! */
                
                fprintf(stderr, "%s takes an argument\n", argv[i]);
                show_lang_usage(stderr, (char *) getfilename(argv[0]));
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--savedir") == 0)
        {
            if (i < argc - 1)
            {
                if (savedir != NULL)
                    free(savedir);
                
                savedir = strdup(argv[i + 1]);
                i++;
            }
            else
            {
                /* Forgot to specify the directory name! */
                
                fprintf(stderr, "%s takes an argument\n", argv[i]);
                show_usage(stderr, (char *) getfilename(argv[0]));
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--datadir") == 0)
        {
            if (i < argc - 1)
            {
                if (datadir != NULL)
                    free(datadir);
                
                datadir = strdup(argv[i + 1]);
                i++;
            }
            else
            {
                /* Forgot to specify the directory name! */
                
                fprintf(stderr, "%s takes an argument\n", argv[i]);
                show_usage(stderr, (char *) getfilename(argv[0]));
                exit(1);
            }
        }
        else if (strcmp(argv[i], "--stampsize=default") == 0)
        {
            stamp_size_override = -1;
        }
        else if (strstr(argv[i], "--stampsize=") == argv[i])
        {
            stamp_size_override = atoi(argv[i] + 12);
            if (stamp_size_override > 10)
                stamp_size_override = 10;
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
        {
            show_version(0);
            exit(0);
        }
        else if (strcmp(argv[i], "--verbose-version") == 0 ||
                 strcmp(argv[i], "-vv") == 0)
        {
            show_version(1);
            exit(0);
        }
        else if (strcmp(argv[i], "--copying") == 0 || strcmp(argv[i], "-c") == 0)
        {
            show_version(0);
            printf("\n"
                   "This program is free software; you can redistribute it\n"
                   "and/or modify it under the terms of the GNU General Public\n"
                   "License as published by the Free Software Foundation;\n"
                   "either version 2 of the License, or (at your option) any\n"
                   "later version.\n"
                   "\n"
                   "This program is distributed in the hope that it will be\n"
                   "useful and entertaining, but WITHOUT ANY WARRANTY; without\n"
                   "even the implied warranty of MERCHANTABILITY or FITNESS\n"
                   "FOR A PARTICULAR PURPOSE.  See the GNU General Public\n"
                   "License for more details.\n"
                   "\n"
                   "You should have received a copy of the GNU General Public\n"
                   "License along with this program; if not, write to the Free\n"
                   "Software Foundation, Inc., 59 Temple Place, Suite 330,\n"
                   "Boston, MA  02111-1307  USA\n" "\n");
            exit(0);
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            show_version(0);
            show_usage(stdout, (char *) getfilename(argv[0]));
            
            printf("See: " DOC_PREFIX "README.txt\n" "\n");
            exit(0);
        }
        else if (strcmp(argv[i], "--usage") == 0 || strcmp(argv[i], "-u") == 0)
        {
            show_usage(stdout, (char *) getfilename(argv[0]));
            exit(0);
        }
        else if (strcmp(argv[i], "--nosysconfig") == 0)
        {
            debug("Not using system config.");
        }
        else if (strcmp(argv[i], "--nolockfile") == 0)
        {
            debug("Not using lockfile");
            ok_to_use_lockfile = 0;
        }
        else if (strcmp(argv[i], "--lockfile") == 0)
        {
            debug("Using lockfile");
            ok_to_use_lockfile = 1;
        }
        else
        {
            show_usage(stderr, (char *) getfilename(argv[0]));
            exit(1);
        }
    }
    
#ifdef _WIN32
    if (fullscreen)
    {
        InstallKeyboardHook();
        SetActivationState(1);
    }
#endif
    
    
    setup_language(getfilename(argv[0]), &button_label_y_nudge);
    /*  printf("cur locale = %d (%s)\n", get_current_language(), lang_prefixes[get_current_language()]);  */
    
    im_init(&im_data, get_current_language());
    
#ifndef NO_SDLPANGO
    SDLPango_Init();
#endif
    
    
    /* NOTE: Moved run_font_scanner() call from main(), to here,
     so that the gettext() calls used while testing fonts
     actually DO something (per tuxpaint-devel discussion, April 2007)
     -bjk 2007.06.05 */
    
#ifdef FORKED_FONTS
    run_font_scanner(screen, lang_prefixes[get_current_language()]);
#endif
    
    
#ifndef WIN32
    putenv((char *) "SDL_VIDEO_X11_WMCLASS=TuxPaint.TuxPaint");
#endif
    
    if (disable_screensaver == 0)
    {
        putenv((char *) "SDL_VIDEO_ALLOW_SCREENSAVER=1");
        if (SDL_MAJOR_VERSION < 1 ||
            (SDL_MAJOR_VERSION >= 1 && SDL_MINOR_VERSION < 2) ||
            (SDL_MAJOR_VERSION >= 1 && SDL_MINOR_VERSION >= 2 && SDL_PATCHLEVEL < 12))
        {
            fprintf(stderr, "Note: 'allowscreensaver' requires SDL 1.2.12 or higher\n");
        }
    }
    
    /* Test for lockfile, if we're using one: */
    
    if (ok_to_use_lockfile)
    {
        char *lock_fname;
        time_t time_lock, time_now;
        char *homedirdir;
        
        
        /* Get the current time: */
        
        time_now = time(NULL);
        
        
        /* Look for the lockfile... */
        
#ifndef WIN32
        lock_fname = get_fname("lockfile.dat", DIR_SAVE);
#else
        lock_fname = get_temp_fname("lockfile.dat");
#endif
        
        fi = fopen(lock_fname, "r");
        if (fi != NULL)
        {
            /* If it exists, read its contents: */
            
            if (fread(&time_lock, sizeof(time_t), 1, fi) > 0)
            {
                /* Has it not been 30 seconds yet? */
                
                if (time_now < time_lock + 30)
                {
                    /* FIXME: Wrap in gettext() */
                    printf
                    ("You have already started tuxpaint less than 30 seconds ago.\n"
                     "To prevent multiple executions by mistake, TuxPaint will not run\n"
                     "before 30 seconds have elapsed since it was last started.\n"
                     "\n"
                     "You can also use the --nolockfile argument, see tuxpaint(1).\n\n");
                    
                    free(lock_fname);
                    
                    fclose(fi);
                    exit(0);
                }
            }
            
            fclose(fi);
        }
        
        
        /* Okay to run; create/update the lockfile */
        
        /* (Make sure the directory exists, first!) */
        homedirdir = get_fname("", DIR_SAVE);
        mkdir(homedirdir, 0755);
        free(homedirdir);
        
        
        fi = fopen(lock_fname, "w");
        if (fi != NULL)
        {
            /* If we can write to it, do so! */
            
            fwrite(&time_now, sizeof(time_t), 1, fi);
            fclose(fi);
        }
        else
        {
            fprintf(stderr,
                    "\nWarning: I couldn't create the lockfile (%s)\n"
                    "The error that occurred was:\n"
                    "%s\n\n", lock_fname, strerror(errno));
        }
        
        free(lock_fname);
    }
    
    
    init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
    if (use_sound)
        init_flags |= SDL_INIT_AUDIO;
    if (!fullscreen)
        init_flags |= SDL_INIT_NOPARACHUTE;	/* allow debugger to catch crash */
    
    /* Init SDL */
    if (SDL_Init(init_flags) < 0)
    {
#ifndef NOSOUND
        char *olderr = strdup(SDL_GetError());
        use_sound = 0;
        init_flags &= ~SDL_INIT_AUDIO;
        if (SDL_Init(init_flags) >= 0)
        {
            /* worked, w/o sound */
            fprintf(stderr,
                    "\nWarning: I could not initialize audio!\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", olderr);
            free(olderr);
        }
        else
#endif
        {
            fprintf(stderr,
                    "\nError: I could not initialize video and/or the timer!\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", SDL_GetError());
            exit(1);
        }
    }
    
    
#ifndef NOSOUND
#ifndef WIN32
    if (use_sound && Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) < 0)
#else
        if (use_sound && Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048) < 0)
#endif
        {
            fprintf(stderr,
                    "\nWarning: I could not set up audio for 44100 Hz "
                    "16-bit stereo.\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", SDL_GetError());
            use_sound = 0;
        }
    
    i = NUM_SOUNDS;
    while (use_sound && i--)
    {
#ifndef __IPHONEOS__
        sounds[i] = Mix_LoadWAV(sound_fnames[i]);
#else
        char temp[1024];
        sprintf(temp, "%s/%s", DATA_PREFIX, sound_fnames[i]);
        sounds[i] = Mix_LoadWAV(temp);
        
#endif
        if (sounds[i] == NULL)
        {
            fprintf(stderr,
                    "\nWarning: I couldn't open a sound file:\n%s\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", sound_fnames[i], SDL_GetError());
            use_sound = 0;
        }
    }
#endif
    
    
    /* Set-up Key-Repeat: */
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    
    /* Init TTF stuff: */
    if (TTF_Init() < 0)
    {
        fprintf(stderr,
                "\nError: I could not initialize the font (TTF) library!\n"
                "The Simple DirectMedia Layer error that occurred was:\n"
                "%s\n\n", SDL_GetError());
        
        SDL_Quit();
        exit(1);
    }
    
    
    /* Load colors, or use default ones: */
    
    if (colorfile[0] != '\0')
    {
        fi = fopen(colorfile, "r");
        if (fi == NULL)
        {
            fprintf(stderr,
                    "\nWarning, could not open color file. Using defaults.\n");
            perror(colorfile);
            colorfile[0] = '\0';
        }
        else
        {
            int max = 0, per = 5;
            char str[80], tmp_str[80];
            int count;
            
            NUM_COLORS = 0;
            
            do
            {
                fgets(str, sizeof(str), fi);
                
                if (!feof(fi))
                {
                    if (NUM_COLORS + 1 > max)
                    {
                        color_hexes = realloc(color_hexes, sizeof(Uint8 *) * (max + per));
                        color_names = realloc(color_names, sizeof(char *) * (max + per));
                        
                        for (i = max; i < max + per; i++)
                            color_hexes[i] = malloc(sizeof(Uint8) * 3);
                        
                        max = max + per;
                    }
                    
                    while (str[strlen(str) - 1] == '\n' ||
                           str[strlen(str) - 1] == '\r')
                        str[strlen(str) - 1] = '\0';
                    
                    if (str[0] == '#')
                    {
                        /* Hex form */
                        
                        sscanf(str + 1, "%s %n", tmp_str, &count);
                        
                        if (strlen(tmp_str) == 6)
                        {
                            /* Byte (#rrggbb) form */
                            
                            color_hexes[NUM_COLORS][0] =
                            (hex2dec(tmp_str[0]) << 4) + hex2dec(tmp_str[1]);
                            color_hexes[NUM_COLORS][1] =
                            (hex2dec(tmp_str[2]) << 4) + hex2dec(tmp_str[3]);
                            color_hexes[NUM_COLORS][2] =
                            (hex2dec(tmp_str[4]) << 4) + hex2dec(tmp_str[5]);
                            
                            color_names[NUM_COLORS] = strdup(str + count);
                            NUM_COLORS++;
                        }
                        else if (strlen(tmp_str) == 3)
                        {
                            /* Nybble (#rgb) form */
                            
                            color_hexes[NUM_COLORS][0] =
                            (hex2dec(tmp_str[0]) << 4) + hex2dec(tmp_str[0]);
                            color_hexes[NUM_COLORS][1] =
                            (hex2dec(tmp_str[1]) << 4) + hex2dec(tmp_str[1]);
                            color_hexes[NUM_COLORS][2] =
                            (hex2dec(tmp_str[2]) << 4) + hex2dec(tmp_str[2]);
                            
                            color_names[NUM_COLORS] = strdup(str + count);
                            NUM_COLORS++;
                        }
                    }
                    else
                    {
                        /* Assume int form */
                        
                        if (sscanf(str, "%hu %hu %hu %n",
                                   (short unsigned int *) &(color_hexes[NUM_COLORS][0]),
                                   (short unsigned int *) &(color_hexes[NUM_COLORS][1]),
                                   (short unsigned int *) &(color_hexes[NUM_COLORS][2]),
                                   &count) >= 3)
                        {
                            color_names[NUM_COLORS] = strdup(str + count);
                            NUM_COLORS++;
                        }
                    }
                }
            }
            while (!feof(fi));
            
            if (NUM_COLORS < 2)
            {
                fprintf(stderr,
                        "\nWarning, not enough colors in color file. Using defaults.\n");
                fprintf(stderr, "%s\n", colorfile);
                colorfile[0] = '\0';
                
                for (i = 0; i < NUM_COLORS; i++)
                {
                    free(color_names[i]);
                    free(color_hexes[i]);
                }
                
                free(color_names);
                free(color_hexes);
            }
        }
    }
    
    /* Use default, if no file specified (or trouble opening it) */
    
    if (colorfile[0] == '\0')
    {
        NUM_COLORS = NUM_DEFAULT_COLORS;
        
        color_hexes = malloc(sizeof(Uint8 *) * NUM_COLORS);
        color_names = malloc(sizeof(char *) * NUM_COLORS);
        
        for (i = 0; i < NUM_COLORS; i++)
        {
            color_hexes[i] = malloc(sizeof(Uint8 *) * 3);
            
            for (j = 0; j < 3; j++)
                color_hexes[i][j] = default_color_hexes[i][j];
            
            color_names[i] = strdup(default_color_names[i]);
        }
    }
    
    
    /* Add "Color Picker" color: */
    
    color_hexes = (Uint8 **) realloc(color_hexes, sizeof(Uint8 *) * (NUM_COLORS + 1));
    
    color_names = (char **) realloc(color_names, sizeof(char *) * (NUM_COLORS + 1));
    color_names[NUM_COLORS] = strdup(gettext("Pick a color."));
    color_hexes[NUM_COLORS] = (Uint8 *) malloc(sizeof(Uint8) * 3);
    color_hexes[NUM_COLORS][0] = 0;
    color_hexes[NUM_COLORS][1] = 0;
    color_hexes[NUM_COLORS][2] = 0;
    color_picker_x = 0;
    color_picker_y = 0;
    NUM_COLORS++;
    
    
    /* Set window icon and caption: */
    
#ifndef __APPLE__
    seticon();
#endif
    SDL_WM_SetCaption("Tux Paint", "Tux Paint");
    
    if (hide_cursor)
        SDL_ShowCursor (SDL_DISABLE);
    
    
    /* Deal with orientation rotation option */
    
    if (rotate_orientation)
    {
        if (native_screensize && fullscreen)
        {
            fprintf(stderr, "Warning: Asking for native screen size overrides request to rotate orientation.\n");
        }
        else
        {
            int tmp;
            tmp = WINDOW_WIDTH;
            WINDOW_WIDTH = WINDOW_HEIGHT;
            WINDOW_HEIGHT = tmp;
        }
    }
    
    /* Deal with 'native' screen size option */
    
    if (native_screensize)
    {
        if (!fullscreen)
        {
            fprintf(stderr, "Warning: Asking for native screensize in a window. Ignoring.\n");
        }
        else
        {
            WINDOW_WIDTH = 0;
            WINDOW_HEIGHT = 0;
        }
    }
    
    
    /* Open Window: */
    
    if (fullscreen)
    {
#ifdef USE_HWSURFACE
        screen = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT,
                                  VIDEO_BPP, SDL_FULLSCREEN | SDL_HWSURFACE);
#else
        screen = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT,
                                  VIDEO_BPP, SDL_FULLSCREEN | SDL_SWSURFACE);
#endif
        
        if (screen == NULL)
        {
            fprintf(stderr,
                    "\nWarning: I could not open the display in fullscreen mode.\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", SDL_GetError());
            
            fullscreen = 0;
        }
        else
        {
            /* Get resolution if we asked for native: */
            
            if (native_screensize)
            {
                WINDOW_WIDTH = screen->w;
                WINDOW_HEIGHT = screen->h;
            }
        }
    }
    
    
    if (!fullscreen)
    {
        putenv((char *) "SDL_VIDEO_WINDOW_POS=center");
        
#ifdef USE_HWSURFACE
        screen = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT,
                                  VIDEO_BPP, SDL_HWSURFACE);
#else
        screen = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT,
                                  VIDEO_BPP, SDL_SWSURFACE);
#endif
        
        putenv((char *) "SDL_VIDEO_WINDOW_POS=nopref");
    }
    
    if (screen == NULL)
    {
        fprintf(stderr,
                "\nError: I could not open the display.\n"
                "The Simple DirectMedia Layer error that occurred was:\n"
                "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
    
    /* (Need to do this after native screen resolution is handled) */
    
    setup_screen_layout();
    
    
    /* quickly: title image, version, progress bar, and watch cursor */
    img_title = loadimage(DATA_PREFIX, "images/title.png");
    img_title_credits = loadimage(DATA_PREFIX,  "images/title-credits.png");
    img_progress = loadimage(DATA_PREFIX,  "images/ui/progress.png");
    
    if (screen->w - img_title->w >= 410 && screen->h - img_progress->h - img_title_credits->h - 40) /* FIXME: Font */
        big_title = 1;
    else
        big_title = 0;
    
    
    if (big_title)
        img_title_tuxpaint = loadimage(DATA_PREFIX,   "images/title-tuxpaint-2x.png");
    else
        img_title_tuxpaint = loadimage(DATA_PREFIX,   "images/title-tuxpaint.png");
    
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 255, 255, 255));
    
    dest.x =
    ((WINDOW_WIDTH - img_title->w - (img_title_tuxpaint->w / 2)) / 2) +
    (img_title_tuxpaint->w / 2) + 20;
    dest.y = (WINDOW_HEIGHT - img_title->h);
    
    SDL_BlitSurface(img_title, NULL, screen, &dest);
    
    dest.x = 10;
    if (big_title)
        dest.y = WINDOW_HEIGHT - img_title_tuxpaint->h - img_progress->h - 40;
    else
        dest.y = (WINDOW_HEIGHT - img_title->h) + img_title_tuxpaint->h * 0.8 + 7;
    
    SDL_BlitSurface(img_title_tuxpaint, NULL, screen, &dest);
    
    dest.x = 10;
    dest.y = 5;
    
    SDL_BlitSurface(img_title_credits, NULL, screen, &dest);
    
    prog_bar_ctr = 0;
    show_progress_bar(screen);
    
    SDL_Flip(screen);
    
    char temp[1024];
    sprintf(temp, "%s/%s", DATA_PREFIX, "fonts/default_font.ttf");
    medium_font = TuxPaint_Font_OpenFont(PANGO_DEFAULT_FONT,
                                         temp,
                                         18 - (only_uppercase * 3));
    
    if (medium_font == NULL)
    {
        //    fprintf(stderr,
        //	    "\nError: Can't load font file: "
        //	    sprintf("%s/%s", DATA_PREFIX, "fonts/default_font.ttf\n")
        //	    "The Simple DirectMedia Layer error that occurred was:\n"
        //	    "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
//    snprintf(tmp_str, sizeof(tmp_str), "Version: %s%s", VER_VERSION,
//             VER_DATE);
    
//    tmp_surf = render_text(medium_font, tmp_str, black);
//    dest.x = 10;
//    dest.y = WINDOW_HEIGHT - img_progress->h - tmp_surf->h;
//    SDL_BlitSurface(tmp_surf, NULL, screen, &dest);
//    SDL_FreeSurface(tmp_surf);
    
#ifdef DEBUG
    printf("%s\n", tmp_str);
#endif
    
//    snprintf(tmp_str, sizeof(tmp_str), "");
//    tmp_surf = render_text(medium_font, tmp_str, black);
//    dest.x = 10;
//    dest.y = WINDOW_HEIGHT - img_progress->h - (tmp_surf->h * 2);
//    SDL_BlitSurface(tmp_surf, NULL, screen, &dest);
//    SDL_FreeSurface(tmp_surf);
    
    SDL_Flip(screen);
    
    
#if defined(WIN32) && defined(LARGE_CURSOR_FULLSCREEN_BUG)
    if (fullscreen && no_fancy_cursors == 0)
    {
        fprintf(stderr, "Warning: An SDL bug causes the fancy cursors to leave\n"
                "trails in fullscreen mode.  Disabling fancy cursors.\n"
                "(You can do this yourself with 'nofancycursors' option,\n"
                "to avoid this warning in the future.)\n");
        no_fancy_cursors = 1;
    }
#endif
    
    
    /* Create cursors: */
    
    scale = 1;
    
#ifdef SMALL_CURSOR_SHAPES
    scale = 2;
#endif
    
#ifdef __APPLE__
    cursor_arrow = SDL_GetCursor();		/* use standard system cursor */
#endif
    
    /* this one first, because we need it yesterday */
    cursor_watch = get_cursor(watch_bits, watch_mask_bits,
                              watch_width, watch_height,
                              14 / scale, 14 / scale);
    
    do_setcursor(cursor_watch);
    show_progress_bar(screen);
    
#ifdef FORKED_FONTS
    reliable_write(font_socket_fd, &no_system_fonts, sizeof no_system_fonts);
#else
    font_thread = SDL_CreateThread(load_user_fonts_stub, "", NULL); //dailin
#endif
    
    /* continuing on with the rest of the cursors... */
    
    
#ifndef __APPLE__
    cursor_arrow = get_cursor(arrow_bits, arrow_mask_bits,
                              arrow_width, arrow_height, 0, 0);
#endif
    
    cursor_hand = get_cursor(hand_bits, hand_mask_bits,
                             hand_width, hand_height, 12 / scale, 1 / scale);
    
    cursor_wand = get_cursor(wand_bits, wand_mask_bits,
                             wand_width, wand_height, 4 / scale, 4 / scale);
    
    cursor_insertion = get_cursor(insertion_bits, insertion_mask_bits,
                                  insertion_width, insertion_height,
                                  7 / scale, 4 / scale);
    
    cursor_brush = get_cursor(brush_bits, brush_mask_bits,
                              brush_width, brush_height, 4 / scale, 28 / scale);
    
    cursor_crosshair = get_cursor(crosshair_bits, crosshair_mask_bits,
                                  crosshair_width, crosshair_height,
                                  15 / scale, 15 / scale);
    
    cursor_rotate = get_cursor(rotate_bits, rotate_mask_bits,
                               rotate_width, rotate_height,
                               15 / scale, 15 / scale);
    
    cursor_up = get_cursor(up_bits, up_mask_bits,
                           up_width, up_height, 15 / scale, 1 / scale);
    
    cursor_down = get_cursor(down_bits, down_mask_bits,
                             down_width, down_height, 15 / scale, 30 / scale);
    
    cursor_tiny = get_cursor(tiny_bits, tiny_mask_bits, tiny_width, tiny_height, 3, 3);	/* Exactly the same in SMALL (16x16) size! */
    
    
    
    /* Create drawing canvas: */
    
    canvas = SDL_CreateRGBSurface(screen->flags,
                                  WINDOW_WIDTH - (96 * 2),
                                  (48 * 7) + 40 + HEIGHTOFFSET +48 -14,
                                  screen->format->BitsPerPixel,
                                  screen->format->Rmask,
                                  screen->format->Gmask,
                                  screen->format->Bmask, 0);
    
    img_starter = NULL;
    img_starter_bkgd = NULL;
    starter_mirrored = 0;
    starter_flipped = 0;
    starter_personal = 0;
    
    if (canvas == NULL)
    {
        fprintf(stderr, "\nError: Can't build drawing canvas!\n"
                "The Simple DirectMedia Layer error that occurred was:\n"
                "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
    touched = (Uint8 *) malloc(sizeof(Uint8) * (canvas->w * canvas->h));
    if (touched == NULL)
    {
        fprintf(stderr, "\nError: Can't build drawing touch mask!\n");
        
        cleanup();
        exit(1);
    }
    
    canvas_color_r = 255;
    canvas_color_g = 255;
    canvas_color_b = 255;
    
    SDL_FillRect(canvas, NULL, SDL_MapRGB(canvas->format, 255, 255, 255));
    
    
    /* Create undo buffer space: */
    
    for (i = 0; i < NUM_UNDO_BUFS; i++)
    {
        undo_bufs[i] = SDL_CreateRGBSurface(screen->flags,
                                            WINDOW_WIDTH - (96 * 2),
                                            (48 * 7) + 40 + HEIGHTOFFSET,
                                            screen->format->BitsPerPixel,
                                            screen->format->Rmask,
                                            screen->format->Gmask,
                                            screen->format->Bmask, 0);
        
        
        if (undo_bufs[i] == NULL)
        {
            fprintf(stderr, "\nError: Can't build undo buffer! (%d of %d)\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", i + 1, NUM_UNDO_BUFS, SDL_GetError());
            
            cleanup();
            exit(1);
        }
        
        undo_starters[i] = UNDO_STARTER_NONE;
    }
    
    
    
    /* Load other images: */
    
    for (i = 0; i < NUM_TOOLS; i++)
        img_tools[i] = loadimage(DATA_PREFIX,tool_img_fnames[i]);
    
    img_title_on = loadimage(DATA_PREFIX, "images/ui/title.png");
    img_title_large_on = loadimage(DATA_PREFIX, "images/ui/title_large.png");
    img_title_off = loadimage(DATA_PREFIX,"images/ui/no_title.png");
    img_title_large_off = loadimage(DATA_PREFIX, "images/ui/no_title_large.png");
    
    img_btn_up = loadimage(DATA_PREFIX, "images/ui/btn_up.png");
    img_btn_down = loadimage(DATA_PREFIX, "images/ui/btn_down.png");
    img_btn_off = loadimage(DATA_PREFIX, "images/ui/btn_off.png");
    
    img_btnsm_up = loadimage(DATA_PREFIX, "images/ui/btnsm_up.png");
    img_btnsm_off = loadimage(DATA_PREFIX, "images/ui/btnsm_off.png");
    
    img_sfx = loadimage(DATA_PREFIX, "images/tools/sfx.png");
    img_speak = loadimage(DATA_PREFIX, "images/tools/speak.png");
    
    img_black = SDL_CreateRGBSurface(SDL_SRCALPHA | SDL_SWSURFACE,
                                     img_btn_off->w, img_btn_off->h,
                                     img_btn_off->format->BitsPerPixel,
                                     img_btn_off->format->Rmask,
                                     img_btn_off->format->Gmask,
                                     img_btn_off->format->Bmask,
                                     img_btn_off->format->Amask);
    SDL_FillRect(img_black, NULL, SDL_MapRGBA(screen->format, 0, 0, 0, 255));
    
    img_grey = SDL_CreateRGBSurface(SDL_SRCALPHA | SDL_SWSURFACE,
                                    img_btn_off->w, img_btn_off->h,
                                    img_btn_off->format->BitsPerPixel,
                                    img_btn_off->format->Rmask,
                                    img_btn_off->format->Gmask,
                                    img_btn_off->format->Bmask,
                                    img_btn_off->format->Amask);
    SDL_FillRect(img_grey, NULL,
                 SDL_MapRGBA(screen->format, 0x88, 0x88, 0x88, 255));
    
    show_progress_bar(screen);
    
    img_yes = loadimage(DATA_PREFIX, "images/ui/yes.png");
    img_no = loadimage(DATA_PREFIX, "images/ui/no.png");
    
    img_prev = loadimage(DATA_PREFIX, "images/ui/prev.png");
    img_next = loadimage(DATA_PREFIX, "images/ui/next.png");
    
    img_mirror = loadimage(DATA_PREFIX, "images/ui/mirror.png");
    img_flip = loadimage( DATA_PREFIX, "images/ui/flip.png");
    
    img_open = loadimage(DATA_PREFIX, "images/ui/open.png");
    img_erase = loadimage(DATA_PREFIX,"images/ui/erase.png");
    img_back = loadimage(DATA_PREFIX, "images/ui/back.png");
    img_trash = loadimage(DATA_PREFIX, "images/ui/trash.png");
    
    img_slideshow = loadimage(DATA_PREFIX, "images/ui/slideshow.png");
    img_play = loadimage(DATA_PREFIX, "images/ui/play.png");
    img_select_digits = loadimage(DATA_PREFIX, "images/ui/select_digits.png");
    
    img_popup_arrow = loadimage(DATA_PREFIX, "images/ui/popup_arrow.png");
    
    img_dead40x40 = loadimage(DATA_PREFIX, "images/ui/dead40x40.png");
    
    img_printer = loadimage(DATA_PREFIX, "images/ui/printer.png");
    img_printer_wait = loadimage(DATA_PREFIX, "images/ui/printer_wait.png");
    
    img_save_over = loadimage(DATA_PREFIX, "images/ui/save_over.png");
    
    img_grow = loadimage(DATA_PREFIX, "images/ui/grow.png");
    img_shrink = loadimage(DATA_PREFIX, "images/ui/shrink.png");
    
    img_magic_paint = loadimage(DATA_PREFIX, "images/ui/magic_paint.png");
    img_magic_fullscreen = loadimage( DATA_PREFIX, "images/ui/magic_fullscreen.png");
    
    img_bold = loadimage(DATA_PREFIX, "images/ui/bold.png");
    img_italic = loadimage(DATA_PREFIX, "images/ui/italic.png");
    
    show_progress_bar(screen);
    
    tmp_imgcurup = loadimage(DATA_PREFIX, "images/ui/cursor_up_large.png");
    tmp_imgcurdown = loadimage(DATA_PREFIX, "images/ui/cursor_down_large.png");
    img_cursor_up = thumbnail(tmp_imgcurup, THUMB_W, THUMB_H, 0);
    img_cursor_down = thumbnail(tmp_imgcurdown, THUMB_W, THUMB_H, 0);
    
    tmp_imgcurup = loadimage(DATA_PREFIX, "images/ui/cursor_starter_up.png");
    tmp_imgcurdown = loadimage( DATA_PREFIX, "images/ui/cursor_starter_down.png");
    img_cursor_starter_up = thumbnail(tmp_imgcurup, THUMB_W, THUMB_H, 0);
    img_cursor_starter_down = thumbnail(tmp_imgcurdown, THUMB_W, THUMB_H, 0);
    SDL_FreeSurface(tmp_imgcurup);
    SDL_FreeSurface(tmp_imgcurdown);
    
    show_progress_bar(screen);
    
    img_scroll_up = loadimage(DATA_PREFIX, "images/ui/scroll_up.png");
    img_scroll_down = loadimage(DATA_PREFIX, "images/ui/scroll_down.png");
    
    img_scroll_up_off = loadimage(DATA_PREFIX, "images/ui/scroll_up_off.png");
    img_scroll_down_off =
    loadimage( DATA_PREFIX, "images/ui/scroll_down_off.png");
    
#ifdef LOW_QUALITY_COLOR_SELECTOR
    img_paintcan = loadimage(DATA_PREFIX "images/ui/paintcan.png");
#endif
    
    show_progress_bar(screen);
    
    
    /* Load brushes: */
    sprintf(temp, "%s/%s", DATA_PREFIX, "brushes");
    
    load_brush_dir(screen, temp);
    homedirdir = get_fname("brushes", DIR_DATA);
    load_brush_dir(screen, homedirdir);
#ifdef WIN32
    free(homedirdir);
    homedirdir = get_fname("data/brushes", DIR_DATA);
    load_brush_dir(screen, homedirdir);
#endif
    
    if (num_brushes == 0)
    {
        //    fprintf(stderr,
        //	    "\nError: No brushes found in " DATA_PREFIX "brushes/\n"
        //	    "or %s\n\n", homedirdir);
        cleanup();
        exit(1);
    }
    
    free(homedirdir);
    
    
    /* Load system fonts: */
    sprintf(temp, "%s/%s", DATA_PREFIX, "fonts/default_font.ttf");
    
    large_font = TuxPaint_Font_OpenFont(PANGO_DEFAULT_FONT,
                                        temp,
                                        30 - (only_uppercase * 3));
    
    if (large_font == NULL)
    {
        //    fprintf(stderr,
        //	    "\nError: Can't load font file: "
        //	    DATA_PREFIX "fonts/default_font.ttf\n"
        //	    "The Simple DirectMedia Layer error that occurred was:\n"
        //	    "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
    sprintf(temp, "%s/%s", DATA_PREFIX, "fonts/default_font.ttf");
    small_font = TuxPaint_Font_OpenFont(PANGO_DEFAULT_FONT,
                                        temp,
#ifdef __APPLE__
                                        12 - (only_uppercase * 2));
#else
    13 - (only_uppercase * 2));
#endif
    
    if (small_font == NULL)
    {
        //    fprintf(stderr,
        //	    "\nError: Can't load font file: "
        //	    DATA_PREFIX "fonts/default_font.ttf\n"
        //	    "The Simple DirectMedia Layer error that occurred was:\n"
        //	    "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
    
    locale_font = load_locale_font(medium_font, 18);
    
    
    
#if 0
    /* put elsewhere for THREADED_FONTS */
    /* Load user fonts, for the text tool */
    load_user_fonts();
#endif
    
    if (!dont_load_stamps)
        load_stamps(screen);
    
    
    /* Load magic tool plugins: */
    
    load_magic_plugins();
    
    show_progress_bar(screen);
    
    /* Load shape icons: */
    for (i = 0; i < NUM_SHAPES; i++)
        img_shapes[i] = loadimage(DATA_PREFIX,shape_img_fnames[i]);
    
    show_progress_bar(screen);
    
    /* Load tip tux images: */
    for (i = 0; i < NUM_TIP_TUX; i++)
        img_tux[i] = loadimage(DATA_PREFIX,tux_img_fnames[i]);
    
    show_progress_bar(screen);
    
    img_mouse = loadimage(DATA_PREFIX, "images/ui/mouse.png");
    img_mouse_click = loadimage(DATA_PREFIX, "images/ui/mouse_click.png");
    
    show_progress_bar(screen);
    
    img_color_picker = loadimage(DATA_PREFIX, "images/ui/color_picker.png");
    
    /* Create toolbox and selector labels: */
    
    for (i = 0; i < NUM_TITLES; i++)
    {
        if (strlen(title_names[i]) > 0)
        {
            TuxPaint_Font *myfont = large_font;
            char *td_str = textdir(gettext(title_names[i]));
            
            if (need_own_font && strcmp(gettext(title_names[i]), title_names[i]))
                myfont = locale_font;
            upstr = uppercase(td_str);
            free(td_str);
            tmp_surf = render_text(myfont, upstr, black);
            free(upstr);
            img_title_names[i] =
            thumbnail(tmp_surf, min(84, tmp_surf->w), tmp_surf->h, 0);
            SDL_FreeSurface(tmp_surf);
        }
        else
        {
            img_title_names[i] = NULL;
        }
    }
    
    
    
    /* Generate color selection buttons: */
    
#ifndef LOW_QUALITY_COLOR_SELECTOR
    
    /* Create appropriately-shaped buttons: */
    img1 = loadimage(DATA_PREFIX, "images/ui/paintwell.png");
    img_paintwell = thumbnail(img1, color_button_w, color_button_h, 0);
    tmp_btn_up = thumbnail(img_btn_up, color_button_w, color_button_h, 0);
    tmp_btn_down = thumbnail(img_btn_down, color_button_w, color_button_h, 0);
    img_color_btn_off =
    thumbnail(img_btn_off, color_button_w, color_button_h, 0);
    SDL_FreeSurface(img1);
    
    img_color_picker_thumb = thumbnail(img_color_picker,
                                       color_button_w, color_button_h, 0);
    
    /* Create surfaces to draw them into: */
    
    img_color_btns = malloc(sizeof(SDL_Surface *) * NUM_COLORS * 2);
    
    for (i = 0; i < NUM_COLORS * 2; i++)
    {
        img_color_btns[i] = SDL_CreateRGBSurface(screen->flags,
                                                 /* (WINDOW_WIDTH - 96) / NUM_COLORS, 48, */
                                                 tmp_btn_up->w, tmp_btn_up->h,
                                                 screen->format->BitsPerPixel,
                                                 screen->format->Rmask,
                                                 screen->format->Gmask,
                                                 screen->format->Bmask, 0);
        
        if (img_color_btns[i] == NULL)
        {
            fprintf(stderr, "\nError: Can't build color button!\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", SDL_GetError());
            
            cleanup();
            exit(1);
        }
        
        SDL_LockSurface(img_color_btns[i]);
    }
    
    
    /* Generate the buttons based on the thumbnails: */
    
    SDL_LockSurface(tmp_btn_down);
    SDL_LockSurface(tmp_btn_up);
    
    getpixel_tmp_btn_up = getpixels[tmp_btn_up->format->BytesPerPixel];
    getpixel_tmp_btn_down = getpixels[tmp_btn_down->format->BytesPerPixel];
    getpixel_img_paintwell = getpixels[img_paintwell->format->BytesPerPixel];
    
    
    for (y = 0; y < tmp_btn_up->h /* 48 */ ; y++)
    {
        for (x = 0; x < tmp_btn_up->w /* (WINDOW_WIDTH - 96) / NUM_COLORS */ ;
             x++)
        {
            double ru, gu, bu, rd, gd, bd, aa;
            Uint8 a;
            
            SDL_GetRGB(getpixel_tmp_btn_up(tmp_btn_up, x, y), tmp_btn_up->format,
                       &r, &g, &b);
            
            ru = sRGB_to_linear_table[r];
            gu = sRGB_to_linear_table[g];
            bu = sRGB_to_linear_table[b];
            SDL_GetRGB(getpixel_tmp_btn_down(tmp_btn_down, x, y),
                       tmp_btn_down->format, &r, &g, &b);
            rd = sRGB_to_linear_table[r];
            gd = sRGB_to_linear_table[g];
            bd = sRGB_to_linear_table[b];
            SDL_GetRGBA(getpixel_img_paintwell(img_paintwell, x, y), img_paintwell->format, &r, &g, &b, &a);
            aa = a / 255.0;
            
            for (i = 0; i < NUM_COLORS; i++)
            {
                double rh = sRGB_to_linear_table[color_hexes[i][0]];
                double gh = sRGB_to_linear_table[color_hexes[i][1]];
                double bh = sRGB_to_linear_table[color_hexes[i][2]];
                
                if (i == NUM_COLORS - 1)
                {
                    putpixels[img_color_btns[i]->format->BytesPerPixel]
                    (img_color_btns[i], x, y,
                     getpixels[img_color_picker_thumb->format->BytesPerPixel]
                     (img_color_picker_thumb, x, y));
                    putpixels[img_color_btns[i + NUM_COLORS]->format->BytesPerPixel]
                    (img_color_btns[i + NUM_COLORS], x, y,
                     getpixels[img_color_picker_thumb->format->BytesPerPixel]
                     (img_color_picker_thumb, x, y));
                }
                
                if (i < NUM_COLORS - 1 || a == 255)
                {
                    putpixels[img_color_btns[i]->format->BytesPerPixel]
                    (img_color_btns[i], x, y,
                     SDL_MapRGB(img_color_btns[i]->format,
                                linear_to_sRGB(rh * aa + ru * (1.0 - aa)),
                                linear_to_sRGB(gh * aa + gu * (1.0 - aa)),
                                linear_to_sRGB(bh * aa + bu * (1.0 - aa))));
                    putpixels[img_color_btns[i + NUM_COLORS]->format->BytesPerPixel]
                    (img_color_btns[i + NUM_COLORS], x, y,
                     SDL_MapRGB(img_color_btns[i + NUM_COLORS]->format,
                                linear_to_sRGB(rh * aa + rd * (1.0 - aa)),
                                linear_to_sRGB(gh * aa + gd * (1.0 - aa)),
                                linear_to_sRGB(bh * aa + bd * (1.0 - aa))));
                }
            }
        }
    }
    
    for (i = 0; i < NUM_COLORS * 2; i++)
        SDL_UnlockSurface(img_color_btns[i]);
    
    SDL_UnlockSurface(tmp_btn_up);
    SDL_UnlockSurface(tmp_btn_down);
    SDL_FreeSurface(tmp_btn_up);
    SDL_FreeSurface(tmp_btn_down);
    
#endif
    
    create_button_labels();
    
    
    /* Seed random-number generator: */
    
    srand(SDL_GetTicks());
    
    
    /* Enable Unicode support in SDL: */
    
    SDL_EnableUNICODE(1);
    
#ifndef WIN32
    /* Set up signal handler for SIGPIPE (in case printer command dies;
     e.g., altprintcommand=kprinter, but 'Cancel' is clicked,
     instead of 'Ok') */
    
    signal(SIGPIPE, signal_handler);
#endif
}

#ifndef WIN32
void signal_handler(int sig)
{
    sig = sig;
    /*
     if (sig == SIGPIPE)
     fprintf(stderr, "SIGPIPE!\n");
     */
}
#endif

/* Render a button label using the appropriate string/font: */
static SDL_Surface *do_render_button_label(const char *const label)
{
    SDL_Surface *tmp_surf, *surf;
    SDL_Color black = { 0, 0, 0, 0 };
    TuxPaint_Font *myfont = small_font;
    char *td_str = textdir(gettext(label));
    char *upstr = uppercase(td_str);
    
    free(td_str);
    
    if (need_own_font && strcmp(gettext(label), label))
        myfont = locale_font;
    tmp_surf = render_text(myfont, upstr, black);
    free(upstr);
    surf = thumbnail(tmp_surf, min(48, tmp_surf->w), min(18 + button_label_y_nudge, tmp_surf->h), 0);
    SDL_FreeSurface(tmp_surf);
    
    return surf;
}

static void create_button_labels(void)
{
    int i;
    
    for (i = 0; i < NUM_TOOLS; i++)
        img_tool_names[i] = do_render_button_label(tool_names[i]);
    
    for (i = 0; i < num_magics; i++)
        magics[i].img_name = do_render_button_label(magics[i].name);
    
    for (i = 0; i < NUM_SHAPES; i++)
        img_shape_names[i] = do_render_button_label(shape_names[i]);
    
    /* buttons for the file open dialog */
    
    /* Open dialog: 'Open' button, to load the selected picture */
    img_openlabels_open = do_render_button_label(gettext_noop("Open"));
    
    /* Open dialog: 'Erase' button, to erase/deleted the selected picture */
    img_openlabels_erase = do_render_button_label(gettext_noop("Erase"));
    
    /* Open dialog: 'Slides' button, to switch to slide show mode */
    img_openlabels_slideshow = do_render_button_label(gettext_noop("Slides"));
    
    /* Open dialog: 'Back' button, to dismiss Open dialog without opening a picture */
    img_openlabels_back = do_render_button_label(gettext_noop("Back"));
    
    /* Slideshow: 'Next' button, to load next slide (image) */
    img_openlabels_next = do_render_button_label(gettext_noop("Next"));
    
    /* Slideshow: 'Play' button, to begin a slideshow sequence */
    img_openlabels_play = do_render_button_label(gettext_noop("Play"));
}


static void seticon(void)
{
#ifdef __IPHONEOS__
    extern const char* DATA_PREFIX;
#endif
    
#ifndef WIN32
    int masklen;
    Uint8 *mask;
#endif
    SDL_Surface *icon;
    
    /* Load icon into a surface: */
    
#ifndef WIN32
    char temp[1024];
    sprintf(temp, "%s/%s", DATA_PREFIX, "images/icon.png");
    
    icon = IMG_Load(temp);
#else
    icon = IMG_Load(DATA_PREFIX "images/icon32x32.png");
#endif
    
    if (icon == NULL)
    {
        //    fprintf(stderr,
        //	    "\nWarning: I could not load the icon image: %s\n"
        //	    "The Simple DirectMedia error that occurred was:\n"
        //	    "%s\n\n", DATA_PREFIX "images/icon.png", SDL_GetError());
        return;
    }
    
    
#ifndef WIN32
    /* Create mask: */
    masklen = (((icon->w) + 7) / 8) * (icon->h);
    mask = malloc(masklen * sizeof(Uint8));
    memset(mask, 0xFF, masklen);
    
    /* Set icon: */
    SDL_WM_SetIcon(icon, mask);
    
    /* Free icon surface & mask: */
    free(mask);
#else
    /* Set icon: */
    SDL_WM_SetIcon(icon, NULL);
#endif
    SDL_FreeSurface(icon);
    
    
    /* Grab keyboard and mouse, if requested: */
    
    if (grab_input)
    {
        debug("Grabbing input!");
        SDL_WM_GrabInput(SDL_GRAB_ON);
    }
}


/* Load a mouse pointer (cursor) shape: */

static SDL_Cursor *get_cursor(unsigned char *bits, unsigned char *mask_bits,
                              unsigned int width, unsigned int height,
                              unsigned int x, unsigned int y)
{
    Uint8 b;
    Uint8 temp_bitmap[128], temp_bitmask[128];
    unsigned int i;
    
    
    if (((width + 7) / 8) * height > 128)
    {
        fprintf(stderr, "Error: Cursor is too large!\n");
        cleanup();
        exit(1);
    }
    
    for (i = 0; i < ((width + 7) / 8) * height; i++)
    {
        b = bits[i];
        
        temp_bitmap[i] = (((b & 0x01) << 7) |
                          ((b & 0x02) << 5) |
                          ((b & 0x04) << 3) |
                          ((b & 0x08) << 1) |
                          ((b & 0x10) >> 1) |
                          ((b & 0x20) >> 3) |
                          ((b & 0x40) >> 5) | ((b & 0x80) >> 7));
        
        b = mask_bits[i];
        
        temp_bitmask[i] = (((b & 0x01) << 7) |
                           ((b & 0x02) << 5) |
                           ((b & 0x04) << 3) |
                           ((b & 0x08) << 1) |
                           ((b & 0x10) >> 1) |
                           ((b & 0x20) >> 3) |
                           ((b & 0x40) >> 5) | ((b & 0x80) >> 7));
    }
    
    return (SDL_CreateCursor(temp_bitmap, temp_bitmask, width, height, x, y));
}


/* Load an image (with errors): */

static SDL_Surface *loadimage(const char *const dir,const char *const fname)
{
    char temp[200];
    sprintf(temp, "%s/%s", dir, fname);
    return (do_loadimage(temp, 1));
}


/* Load an image: */

static SDL_Surface *do_loadimage(const char *const fname, int abort_on_error)
{
    SDL_Surface *s, *disp_fmt_s;
    
    
    /* Load the image file: */
    
    s = myIMG_Load((char *) fname);
    if (s == NULL)
    {
        if (abort_on_error)
        {
            fprintf(stderr,
                    "\nError: I couldn't load a graphics file:\n"
                    "%s\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", fname, SDL_GetError());
            
            cleanup();
            exit(1);
        }
        else
        {
            return (NULL);
        }
    }
    
    
    /* Convert to the display format: */
    
    disp_fmt_s = SDL_DisplayFormatAlpha(s);
    if (disp_fmt_s == NULL)
    {
        if (abort_on_error)
        {
            fprintf(stderr,
                    "\nError: I couldn't convert a graphics file:\n"
                    "%s\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", fname, SDL_GetError());
            
            cleanup();
            exit(1);
        }
        else
        {
            SDL_FreeSurface(s);
            return (NULL);
        }
    }
    
    
    /* Free the temp. surface & return the converted one: */
    
    SDL_FreeSurface(s);
    
    return (disp_fmt_s);
}


/* Draw the toolbar: */

static void draw_toolbar(void)
{
    int i;
    SDL_Rect dest;
    
    
    /* FIXME: Only allow print if we have something to print! */
    
    
    draw_image_title(TITLE_TOOLS, r_ttools);
    
    for (i = 0; i < NUM_TOOLS + TOOLOFFSET; i++)
    {
        dest.x = ((i % 2) * 48);
        dest.y = ((i / 2) * 48) + 40;
        
        
        if (i < NUM_TOOLS)
        {
            SDL_Surface *button_color;
            SDL_Surface *button_body;
            if (i == cur_tool)
            {
                button_body = img_btn_down;
                button_color = img_black;
            }
            else if (tool_avail[i])
            {
                button_body = img_btn_up;
                button_color = img_black;
            }
            else
            {
                button_body = img_btn_off;
                button_color = img_grey;
            }
            SDL_BlitSurface(button_body, NULL, screen, &dest);
            SDL_BlitSurface(button_color, NULL, img_tools[i], NULL);
            SDL_BlitSurface(button_color, NULL, img_tool_names[i], NULL);
            
            dest.x = ((i % 2) * 48) + 4;
            dest.y = ((i / 2) * 48) + 40 + 2;
            
            SDL_BlitSurface(img_tools[i], NULL, screen, &dest);
            
            
            dest.x = ((i % 2) * 48) + 4 + (40 - img_tool_names[i]->w) / 2;
            dest.y = ((i / 2) * 48) + 40 + 2 + (44 + button_label_y_nudge - img_tool_names[i]->h);
            
            SDL_BlitSurface(img_tool_names[i], NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
        }
    }
}


/* Draw magic controls: */

static void draw_magic(void)
{
    int magic, i, max, off_y;
    SDL_Rect dest;
    int most;
    
    
    draw_image_title(TITLE_MAGIC, r_ttoolopt);
    
    /* How many can we show? */
    
    most = 12;
    if (disable_magic_controls)
        most = 14;
    
    if (num_magics > most + TOOLOFFSET)
    {
        off_y = 24;
        max = (most - 2) + TOOLOFFSET;
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40;
        
        if (magic_scroll > 0)
        {
            SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
        }
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + 24 + ((((most - 2) / 2) + TOOLOFFSET / 2) * 48);
        
        if (magic_scroll < num_magics - (most - 2) - TOOLOFFSET)
        {
            SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
        }
    }
    else
    {
        off_y = 0;
        max = most + TOOLOFFSET;
    }
    
    
    for (magic = magic_scroll; magic < magic_scroll + max; magic++)
    {
        i = magic - magic_scroll;
        
        dest.x = ((i % 2) * 48) + (WINDOW_WIDTH - 96);
        dest.y = ((i / 2) * 48) + 40 + off_y;
        
        if (magic < num_magics)
        {
            if (magic == cur_magic)
            {
                SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
            }
            else
            {
                SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
            }
            
            dest.x = WINDOW_WIDTH - 96 + ((i % 2) * 48) + 4;
            dest.y = ((i / 2) * 48) + 40 + 4 + off_y;
            
            SDL_BlitSurface(magics[magic].img_icon, NULL, screen, &dest);
            
            
            dest.x = WINDOW_WIDTH - 96 + ((i % 2) * 48) + 4 +
            (40 - magics[magic].img_name->w) / 2;
            dest.y = (((i / 2) * 48) + 40 + 4 +
                      (44 - magics[magic].img_name->h) + off_y);
            
            SDL_BlitSurface(magics[magic].img_name, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
        }
    }
    
    
    /* Draw text controls: */
    
    if (!disable_magic_controls)
    {
        SDL_Surface *button_color;
        
        /* Show paint button: */
        
        if (magics[cur_magic].mode == MODE_PAINT)
            button_color = img_btn_down; /* Active */
        else if (magics[cur_magic].avail_modes & MODE_PAINT)
            button_color = img_btn_up; /* Available, but not active */
        else
            button_color = img_btn_off; /* Unavailable */
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + ((6 + TOOLOFFSET / 2) * 48);
        
        SDL_BlitSurface(button_color, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 96 + (48 - img_magic_paint->w) / 2;
        dest.y = (40 + ((6 + TOOLOFFSET / 2) * 48) + (48 - img_magic_paint->h) / 2);
        
        SDL_BlitSurface(img_magic_paint, NULL, screen, &dest);
        
        
        /* Show fullscreen button: */
        
        if (magics[cur_magic].mode == MODE_FULLSCREEN)
            button_color = img_btn_down; /* Active */
        else if (magics[cur_magic].avail_modes & MODE_FULLSCREEN)
            button_color = img_btn_up; /* Available, but not active */
        else
            button_color = img_btn_off; /* Unavailable */
        
        dest.x = WINDOW_WIDTH - 48;
        dest.y = 40 + ((6 + TOOLOFFSET / 2) * 48);
        
        SDL_BlitSurface(button_color, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 48 + (48 - img_magic_fullscreen->w) / 2;
        dest.y = (40 + ((6 + TOOLOFFSET / 2) * 48) + (48 - img_magic_fullscreen->h) / 2);
        
        SDL_BlitSurface(img_magic_fullscreen, NULL, screen, &dest);
    }
}


/* Draw color selector: */

static unsigned colors_state = COLORSEL_ENABLE | COLORSEL_CLOBBER;

static unsigned draw_colors(unsigned action)
{
    unsigned i;
    SDL_Rect dest;
    static unsigned old_color;
    unsigned old_colors_state;
    
    old_colors_state = colors_state;
    
    if (action == COLORSEL_CLOBBER || action == COLORSEL_CLOBBER_WIPE)
        colors_state |= COLORSEL_CLOBBER;
    else if (action == COLORSEL_REFRESH)
        colors_state &= ~COLORSEL_CLOBBER;
    else if (action == COLORSEL_DISABLE)
        colors_state = COLORSEL_DISABLE;
    else if (action == COLORSEL_ENABLE || action == COLORSEL_FORCE_REDRAW)
        colors_state = COLORSEL_ENABLE;
    
    colors_are_selectable = (colors_state == COLORSEL_ENABLE);
    
    if (colors_state & COLORSEL_CLOBBER && action != COLORSEL_CLOBBER_WIPE)
        return old_colors_state;
    
    if (cur_color == old_color && colors_state == old_colors_state &&
        action != COLORSEL_CLOBBER_WIPE && action != COLORSEL_FORCE_REDRAW)
        return old_colors_state;
    
    old_color = cur_color;
    
    for (i = 0; i < (unsigned int) NUM_COLORS; i++)
    {
        dest.x = r_colors.x + i % gd_colors.cols * color_button_w;
        dest.y = r_colors.y + i / gd_colors.cols * color_button_h;
#ifndef LOW_QUALITY_COLOR_SELECTOR
        SDL_BlitSurface((colors_state == COLORSEL_ENABLE)
                        ? img_color_btns[i + (i == cur_color) * NUM_COLORS]
                        : img_color_btn_off, NULL, screen, &dest);
#else
        dest.w = color_button_w;
        dest.h = color_button_h;
        
        if (colors_state == COLORSEL_ENABLE)
            SDL_FillRect(screen, &dest,
                         SDL_MapRGB(screen->format,
                                    color_hexes[i][0],
                                    color_hexes[i][1], color_hexes[i][2]));
        else
            SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 240, 240, 240));
        
        if (i == cur_color && colors_state == COLORSEL_ENABLE)
        {
            dest.y += 4;
            SDL_BlitSurface(img_paintcan, NULL, screen, &dest);
        }
#endif
        
    }
    
    update_screen_rect(&r_colors);
    
    /* if only the color changed, no need to draw the title */
    if (colors_state == old_colors_state)
        return old_colors_state;
    
    if (colors_state == COLORSEL_ENABLE)
    {
        dest.x =
        r_tcolors.x;
        dest.y =
        r_tcolors.y;

        SDL_BlitSurface(img_title_large_on, NULL, screen, &dest);
        
        dest.x =
        r_tcolors.x + (r_tcolors.w - img_title_names[TITLE_COLORS]->w) / 2;
        dest.y =
        r_tcolors.y + (r_tcolors.h - img_title_names[TITLE_COLORS]->h) / 2;
        SDL_BlitSurface(img_title_names[TITLE_COLORS], NULL, screen, &dest);
    }
    else
    {
        dest.x =
        r_tcolors.x;
        dest.y =
        r_tcolors.y;
        SDL_BlitSurface(img_title_large_off, NULL, screen, &dest);
    }
    
    update_screen_rect(&r_tcolors);
    
    return old_colors_state;
}


/* Draw brushes: */

static void draw_brushes(void)
{
    int i, off_y, max, brush;
    SDL_Rect src, dest;
    
    
    /* Draw the title: */
    draw_image_title(TITLE_BRUSHES, r_ttoolopt);
    
    
    /* Do we need scrollbars? */
    
    if (num_brushes > 14 + TOOLOFFSET)
    {
        off_y = 24;
        max = 12 + TOOLOFFSET;
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40;
        
        if (brush_scroll > 0)
        {
            SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
        }
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + 24 + ((6 + TOOLOFFSET / 2) * 48);
        
        if (brush_scroll < num_brushes - 12 - TOOLOFFSET)
        {
            SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
        }
    }
    else
    {
        off_y = 0;
        max = 14 + TOOLOFFSET;
    }
    
    
    /* Draw each of the shown brushes: */
    
    for (brush = brush_scroll; brush < brush_scroll + max; brush++)
    {
        i = brush - brush_scroll;
        
        
        dest.x = ((i % 2) * 48) + (WINDOW_WIDTH - 96);
        dest.y = ((i / 2) * 48) + 40 + off_y;
        
        if (brush == cur_brush)
        {
            SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
        }
        else if (brush < num_brushes)
        {
            SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
        }
        
        if (brush < num_brushes)
        {
            if (brushes_directional[brush])
                src.x = (img_brushes[brush]->w / abs(brushes_frames[brush])) / 3;
            else
                src.x = 0;
            
            src.y = brushes_directional[brush] ? (img_brushes[brush]->h / 3) : 0;
            
            src.w = (img_brushes[brush]->w / abs(brushes_frames[brush])) /
            (brushes_directional[brush] ? 3 : 1);
            src.h = (img_brushes[brush]->h / (brushes_directional[brush] ? 3 : 1));
            
            dest.x = ((i % 2) * 48) + (WINDOW_WIDTH - 96) + ((48 - src.w) >> 1);
            dest.y = ((i / 2) * 48) + 40 + ((48 - src.h) >> 1) + off_y;
            
            SDL_BlitSurface(img_brushes[brush], &src, screen, &dest);
        }
    }
}


/* Draw fonts: */
static void draw_fonts(void)
{
    int i, off_y, max, font, most;
    SDL_Rect dest, src;
    SDL_Surface *tmp_surf;
    SDL_Color black = { 0, 0, 0, 0 };
    
    /* Draw the title: */
    draw_image_title(TITLE_LETTERS, r_ttoolopt);
    
    
    /* How many can we show? */
    
    most = 10;
    if (disable_stamp_controls)
        most = 14;
    
#ifdef DEBUG
    printf("there are %d font families\n", num_font_families);
#endif
    
    
    /* Do we need scrollbars? */
    
    if (num_font_families > most + TOOLOFFSET)
    {
        off_y = 24;
        max = most - 2 + TOOLOFFSET;
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40;
        
        if (font_scroll > 0)
        {
            SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
        }
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + 24 + ((6 + TOOLOFFSET / 2) * 48);
        
        if (!disable_stamp_controls)
            dest.y = dest.y - (48 * 2);
        
        if (font_scroll < num_font_families - (most - 2) - TOOLOFFSET)
        {
            SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
        }
    }
    else
    {
        off_y = 0;
        max = most + TOOLOFFSET;
    }
    
    /* Draw each of the shown fonts: */
    
    for (font = font_scroll; font < font_scroll + max; font++)
    {
        i = font - font_scroll;
        
        
        dest.x = ((i % 2) * 48) + (WINDOW_WIDTH - 96);
        dest.y = ((i / 2) * 48) + 40 + off_y;
        
        if (font == cur_font)
        {
            SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
        }
        else if (font < num_font_families)
        {
            SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
        }
        
        if (font < num_font_families)
        {
            SDL_Surface *tmp_surf_1;
            
            /* Label for 'Letters' buttons (font selector, down the right when the Text tool is being used); used to show the difference between font faces */
            tmp_surf_1 = render_text(getfonthandle(font), gettext("Aa"), black);
            
            if (tmp_surf_1 == NULL)
            {
                printf("render_text() returned NULL!\n");
                return;
            }
            
            if (tmp_surf_1->w > 48 || tmp_surf_1->h > 48)
            {
                tmp_surf = thumbnail(tmp_surf_1, 48, 48, 1);
                SDL_FreeSurface(tmp_surf_1);
            }
            else
                tmp_surf = tmp_surf_1;
            
            src.x = (tmp_surf->w - 48) / 2;
            src.y = (tmp_surf->h - 48) / 2;
            src.w = 48;
            src.h = 48;
            
            if (src.x < 0)
                src.x = 0;
            if (src.y < 0)
                src.y = 0;
            
            dest.x = ((i % 2) * 48) + (WINDOW_WIDTH - 96);
            if (src.w > tmp_surf->w)
            {
                src.w = tmp_surf->w;
                dest.x = dest.x + ((48 - (tmp_surf->w)) / 2);
            }
            
            
            dest.y = ((i / 2) * 48) + 40 + off_y;
            if (src.h > tmp_surf->h)
            {
                src.h = tmp_surf->h;
                dest.y = dest.y + ((48 - (tmp_surf->h)) / 2);
            }
            
            SDL_BlitSurface(tmp_surf, &src, screen, &dest);
            
            SDL_FreeSurface(tmp_surf);
        }
    }
    
    
    /* Draw text controls: */
    
    if (!disable_stamp_controls)
    {
        SDL_Surface *button_color;
        SDL_Surface *button_body;
        
        /* Show bold button: */
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + ((5 + TOOLOFFSET / 2) * 48);
        
        if (text_state & TTF_STYLE_BOLD)
            SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
        else
            SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 96 + (48 - img_bold->w) / 2;
        dest.y = (40 + ((5 + TOOLOFFSET / 2) * 48) + (48 - img_bold->h) / 2);
        
        SDL_BlitSurface(img_bold, NULL, screen, &dest);
        
        
        /* Show italic button: */
        
        dest.x = WINDOW_WIDTH - 48;
        dest.y = 40 + ((5 + TOOLOFFSET / 2) * 48);
        
        if (text_state & TTF_STYLE_ITALIC)
            SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
        else
            SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 48 + (48 - img_italic->w) / 2;
        dest.y = (40 + ((5 + TOOLOFFSET / 2) * 48) + (48 - img_italic->h) / 2);
        
        SDL_BlitSurface(img_italic, NULL, screen, &dest);
        
        
        /* Show shrink button: */
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + ((6 + TOOLOFFSET / 2) * 48);
        
        if (text_size > MIN_TEXT_SIZE)
        {
            button_color = img_black;
            button_body = img_btn_up;
        }
        else
        {
            button_color = img_grey;
            button_body = img_btn_off;
        }
        SDL_BlitSurface(button_body, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 96 + (48 - img_shrink->w) / 2;
        dest.y = (40 + ((6 + TOOLOFFSET / 2) * 48) + (48 - img_shrink->h) / 2);
        
        SDL_BlitSurface(button_color, NULL, img_shrink, NULL);
        SDL_BlitSurface(img_shrink, NULL, screen, &dest);
        
        
        /* Show grow button: */
        
        dest.x = WINDOW_WIDTH - 48;
        dest.y = 40 + ((6 + TOOLOFFSET / 2) * 48);
        
        if (text_size < MAX_TEXT_SIZE)
        {
            button_color = img_black;
            button_body = img_btn_up;
        }
        else
        {
            button_color = img_grey;
            button_body = img_btn_off;
        }
        SDL_BlitSurface(button_body, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 48 + (48 - img_grow->w) / 2;
        dest.y = (40 + ((6 + TOOLOFFSET / 2) * 48) + (48 - img_grow->h) / 2);
        
        SDL_BlitSurface(button_color, NULL, img_grow, NULL);
        SDL_BlitSurface(img_grow, NULL, screen, &dest);
    }
}


/* Draw stamps: */

static void draw_stamps(void)
{
    int i, off_y, max, stamp, most;
    int base_x, base_y;
    SDL_Rect dest;
    SDL_Surface *img;
    int sizes, size_at;
    float x_per, y_per;
    int xx, yy;
    SDL_Surface *btn, *blnk;
    SDL_Surface *button_color;
    SDL_Surface *button_body;
    
    
    /* Draw the title: */
    draw_image_title(TITLE_STAMPS, r_ttoolopt);
    
    
    /* How many can we show? */
    
    most = 8;  /* was 10 and 14, before left/right controls -bjk 2007.05.03 */
    if (disable_stamp_controls)
        most = 12;
    
    
    /* Do we need scrollbars? */
    
    if (num_stamps[stamp_group] > most + TOOLOFFSET)
    {
        off_y = 24;
        max = (most - 2) + TOOLOFFSET;
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40;
        
        if (stamp_scroll[stamp_group] > 0)
        {
            SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
        }
        
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + 24 + ((5 + TOOLOFFSET / 2) * 48); /* was 6, before left/right controls -bjk 2007.05.03 */
        
        if (!disable_stamp_controls)
            dest.y = dest.y - (48 * 2);
        
        if (stamp_scroll[stamp_group] <
            num_stamps[stamp_group] - (most - 2) - TOOLOFFSET)
        {
            SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
        }
    }
    else
    {
        off_y = 0;
        max = most + TOOLOFFSET;
    }
    
    
    /* Draw each of the shown stamps: */
    
    for (stamp = stamp_scroll[stamp_group];
         stamp < stamp_scroll[stamp_group] + max;
         stamp++)
    {
        i = stamp - stamp_scroll[stamp_group];
        
        
        dest.x = ((i % 2) * 48) + (WINDOW_WIDTH - 96);
        dest.y = ((i / 2) * 48) + 40 + off_y;
        
        if (stamp == cur_stamp[stamp_group])
        {
            SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
        }
        else if (stamp < num_stamps[stamp_group])
        {
            SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
        }
        
        if (stamp < num_stamps[stamp_group])
        {
            get_stamp_thumb(stamp_data[stamp_group][stamp]);
            img = stamp_data[stamp_group][stamp]->thumbnail;
            
            base_x = ((i % 2) * 48) + (WINDOW_WIDTH - 96) + ((48 - (img->w)) / 2);
            
            base_y = ((i / 2) * 48) + 40 + ((48 - (img->h)) / 2) + off_y;
            
            dest.x = base_x;
            dest.y = base_y;
            
            SDL_BlitSurface(img, NULL, screen, &dest);
        }
    }
    
    
    /* Draw stamp group buttons (prev/next): */
    
    
    /* Show prev button: */
    
    button_color = img_black;
    button_body = img_btn_up;
    
    dest.x = WINDOW_WIDTH - 96;
    dest.y = 40 + (((most + TOOLOFFSET) / 2) * 48);
    
    SDL_BlitSurface(button_body, NULL, screen, &dest);
    
    dest.x = WINDOW_WIDTH - 96 + (48 - img_prev->w) / 2;
    dest.y = (40 + (((most + TOOLOFFSET) / 2) * 48) + (48 - img_prev->h) / 2);
    
    SDL_BlitSurface(button_color, NULL, img_prev, NULL);
    SDL_BlitSurface(img_prev, NULL, screen, &dest);
    
    /* Show next button: */
    
    button_color = img_black;
    button_body = img_btn_up;
    
    dest.x = WINDOW_WIDTH - 48;
    dest.y = 40 + (((most + TOOLOFFSET) / 2) * 48);
    
    SDL_BlitSurface(button_body, NULL, screen, &dest);
    
    dest.x = WINDOW_WIDTH - 48 + (48 - img_next->w) / 2;
    dest.y = (40 + (((most + TOOLOFFSET) / 2) * 48) + (48 - img_next->h) / 2);
    
    SDL_BlitSurface(button_color, NULL, img_next, NULL);
    SDL_BlitSurface(img_next, NULL, screen, &dest);
    
    
    /* Draw stamp controls: */
    
    if (!disable_stamp_controls)
    {
        /* Show mirror button: */
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + ((5 + TOOLOFFSET / 2) * 48);
        
        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->mirrorable)
        {
            if (stamp_data[stamp_group][cur_stamp[stamp_group]]->mirrored)
            {
                button_color = img_black;
                button_body = img_btn_down;
            }
            else
            {
                button_color = img_black;
                button_body = img_btn_up;
            }
        }
        else
        {
            button_color = img_grey;
            button_body = img_btn_off;
        }
        SDL_BlitSurface(button_body, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 96 + (48 - img_mirror->w) / 2;
        dest.y = (40 + ((5 + TOOLOFFSET / 2) * 48) +
                  (48 - img_mirror->h) / 2);
        
        SDL_BlitSurface(button_color, NULL, img_mirror, NULL);
        SDL_BlitSurface(img_mirror, NULL, screen, &dest);
        
        /* Show flip button: */
        
        dest.x = WINDOW_WIDTH - 48;
        dest.y = 40 + ((5 + TOOLOFFSET / 2) * 48);
        
        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->flipable)
        {
            if (stamp_data[stamp_group][cur_stamp[stamp_group]]->flipped)
            {
                button_color = img_black;
                button_body = img_btn_down;
            }
            else
            {
                button_color = img_black;
                button_body = img_btn_up;
            }
        }
        else
        {
            button_color = img_grey;
            button_body = img_btn_off;
        }
        SDL_BlitSurface(button_body, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 48 + (48 - img_flip->w) / 2;
        dest.y = (40 + ((5 + TOOLOFFSET / 2) * 48) +
                  (48 - img_flip->h) / 2);
        
        SDL_BlitSurface(button_color, NULL, img_flip, NULL);
        SDL_BlitSurface(img_flip, NULL, screen, &dest);
        
        
#ifdef OLD_STAMP_GROW_SHRINK
        /* Show shrink button: */
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + ((6 + TOOLOFFSET / 2) * 48);
        
        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->size > MIN_STAMP_SIZE)
        {
            button_color = img_black;
            button_body = img_btn_up;
        }
        else
        {
            button_color = img_grey;
            button_body = img_btn_off;
        }
        SDL_BlitSurface(button_body, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 96 + (48 - img_shrink->w) / 2;
        dest.y = (40 + ((6 + TOOLOFFSET / 2) * 48) + (48 - img_shrink->h) / 2);
        
        SDL_BlitSurface(button_color, NULL, img_shrink, NULL);
        SDL_BlitSurface(img_shrink, NULL, screen, &dest);
        
        
        /* Show grow button: */
        
        dest.x = WINDOW_WIDTH - 48;
        dest.y = 40 + ((6 + TOOLOFFSET / 2) * 48);
        
        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->size < MAX_STAMP_SIZE)
        {
            button_color = img_black;
            button_body = img_btn_up;
        }
        else
        {
            button_color = img_grey;
            button_body = img_btn_off;
        }
        SDL_BlitSurface(button_body, NULL, screen, &dest);
        
        dest.x = WINDOW_WIDTH - 48 + (48 - img_grow->w) / 2;
        dest.y = (40 + ((6 + TOOLOFFSET / 2) * 48) + (48 - img_grow->h) / 2);
        
        SDL_BlitSurface(button_color, NULL, img_grow, NULL);
        SDL_BlitSurface(img_grow, NULL, screen, &dest);
        
#else
        sizes = MAX_STAMP_SIZE - MIN_STAMP_SIZE;
        size_at = (stamp_data[stamp_group][cur_stamp[stamp_group]]->size - MIN_STAMP_SIZE);
        x_per = 96.0 / sizes;
        y_per = 48.0 / sizes;
        
        for (i = 0; i < sizes; i++)
        {
            xx = ceil(x_per);
            yy = ceil(y_per * i);
            
            if (i <= size_at)
                btn = thumbnail(img_btn_down, xx, yy, 0);
            else
                btn = thumbnail(img_btn_up, xx, yy, 0);
            
            blnk = thumbnail(img_btn_off, xx, 48 - yy, 0);
            
            /* FIXME: Check for NULL! */
            
            dest.x = (WINDOW_WIDTH - 96) + (i * x_per);
            dest.y = (((7 + TOOLOFFSET / 2) * 48)) - 8;
            SDL_BlitSurface(blnk, NULL, screen, &dest);
            
            dest.x = (WINDOW_WIDTH - 96) + (i * x_per);
            dest.y = (((8 + TOOLOFFSET / 2) * 48)) - 8 - (y_per * i);
            SDL_BlitSurface(btn, NULL, screen, &dest);
            
            SDL_FreeSurface(btn);
            SDL_FreeSurface(blnk);
        }
#endif
    }
}


/* Draw the shape selector: */

static void draw_shapes(void)
{
    int i, shape, max, off_y;
    SDL_Rect dest;
    
    
    draw_image_title(TITLE_SHAPES, r_ttoolopt);
    
    
    if (NUM_SHAPES > 14 + TOOLOFFSET)
    {
        off_y = 24;
        max = 12 + TOOLOFFSET;
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40;
        
        if (shape_scroll > 0)
        {
            SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
        }
        
        dest.x = WINDOW_WIDTH - 96;
        dest.y = 40 + 24 + ((6 + TOOLOFFSET / 2) * 48);
        
        if (shape_scroll < NUM_SHAPES - 12 - TOOLOFFSET)
        {
            SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
        }
    }
    else
    {
        off_y = 0;
        max = 14 + TOOLOFFSET;
    }
    
    for (shape = shape_scroll; shape < shape_scroll + max; shape++)
    {
        i = shape - shape_scroll;
        
        dest.x = ((i % 2) * 48) + WINDOW_WIDTH - 96;
        dest.y = ((i / 2) * 48) + 40 + off_y;
        
        if (shape == cur_shape)
        {
            SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
        }
        else if (shape < NUM_SHAPES)
        {
            SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
        }
        
        
        if (shape < NUM_SHAPES)
        {
            dest.x = ((i % 2) * 48) + 4 + WINDOW_WIDTH - 96;
            dest.y = ((i / 2) * 48) + 40 + 4 + off_y;
            
            SDL_BlitSurface(img_shapes[shape], NULL, screen, &dest);
            
            dest.x = ((i % 2) * 48) + 4 + WINDOW_WIDTH - 96 +
            (40 - img_shape_names[shape]->w) / 2;
            dest.y = ((i / 2) * 48) + 40 + 4 +
            (44 - img_shape_names[shape]->h) + off_y;
            
            SDL_BlitSurface(img_shape_names[shape], NULL, screen, &dest);
        }
    }
}


/* Draw the eraser selector: */

static void draw_erasers(void)
{
    int i, x, y, sz;
    int xx, yy, n;
    void (*putpixel) (SDL_Surface *, int, int, Uint32);
    SDL_Rect dest;
    
    putpixel = putpixels[screen->format->BytesPerPixel];
    
    draw_image_title(TITLE_ERASERS, r_ttoolopt);
    
    for (i = 0; i < 14 + TOOLOFFSET; i++)
    {
        dest.x = ((i % 2) * 48) + WINDOW_WIDTH - 96;
        dest.y = ((i / 2) * 48) + 40;
        
        
        if (i == cur_eraser)
        {
            SDL_BlitSurface(img_btn_down, NULL, screen, &dest);
        }
        else if (i < NUM_ERASERS)
        {
            SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
        }
        else
        {
            SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
        }
        
        
        if (i < NUM_ERASERS)
        {
            if (i < NUM_ERASERS / 2)
            {
                /* Square */
                
                sz =
                (2 +
                 (((NUM_ERASERS / 2) - 1 - i) * (38 / ((NUM_ERASERS / 2) - 1))));
                
                x = ((i % 2) * 48) + WINDOW_WIDTH - 96 + 24 - sz / 2;
                y = ((i / 2) * 48) + 40 + 24 - sz / 2;
                
                dest.x = x;
                dest.y = y;
                dest.w = sz;
                dest.h = 2;
                
                SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));
                
                dest.x = x;
                dest.y = y + sz - 2;
                dest.w = sz;
                dest.h = 2;
                
                SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));
                
                dest.x = x;
                dest.y = y;
                dest.w = 2;
                dest.h = sz;
                
                SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));
                
                dest.x = x + sz - 2;
                dest.y = y;
                dest.w = 2;
                dest.h = sz;
                
                SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));
            }
            else
            {
                /* Circle */
                
                sz =
                (2 +
                 (((NUM_ERASERS / 2) - 1 -
                   (i - NUM_ERASERS / 2)) * (38 / ((NUM_ERASERS / 2) - 1))));
                
                x = ((i % 2) * 48) + WINDOW_WIDTH - 96 + 24 - sz / 2;
                y = ((i / 2) * 48) + 40 + 24 - sz / 2;
                
                for (yy = 0; yy <= sz; yy++)
                {
                    for (xx = 0; xx <= sz; xx++)
                    {
                        n = (xx * xx) + (yy * yy) - ((sz / 2) * (sz / 2));
                        
                        if (n >= -sz && n <= sz)
                        {
                            putpixel(screen, (x + sz / 2) + xx, (y + sz / 2) + yy,
                                     SDL_MapRGB(screen->format, 0, 0, 0));
                            
                            putpixel(screen, (x + sz / 2) - xx, (y + sz / 2) + yy,
                                     SDL_MapRGB(screen->format, 0, 0, 0));
                            
                            putpixel(screen, (x + sz / 2) + xx, (y + sz / 2) - yy,
                                     SDL_MapRGB(screen->format, 0, 0, 0));
                            
                            putpixel(screen, (x + sz / 2) - xx, (y + sz / 2) - yy,
                                     SDL_MapRGB(screen->format, 0, 0, 0));
                            
                        }
                    }
                }
            }
        }
    }
}


/* Draw no selectables: */

static void draw_none(void)
{
    int i;
    SDL_Rect dest;
    
    dest.x = WINDOW_WIDTH - 96;
    dest.y = 0;
    SDL_BlitSurface(img_title_off, NULL, screen, &dest);
    
    for (i = 0; i < 14 + TOOLOFFSET; i++)
    {
        dest.x = ((i % 2) * 48) + WINDOW_WIDTH - 96;
        dest.y = ((i / 2) * 48) + 40;
        
        SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
    }
}



/* Create a thumbnail: */

static SDL_Surface *thumbnail(SDL_Surface * src, int max_x, int max_y,
                              int keep_aspect)
{
    return (thumbnail2(src, max_x, max_y, keep_aspect, 1));
}

static SDL_Surface *thumbnail2(SDL_Surface * src, int max_x, int max_y,
                               int keep_aspect, int keep_alpha)
{
    int x, y;
    float src_x, src_y, off_x, off_y;
    SDL_Surface *s;
    Uint32 tr, tg, tb, ta;
    Uint8 r, g, b, a;
    float xscale, yscale;
    int tmp;
    void (*putpixel) (SDL_Surface *, int, int, Uint32);
    Uint32(*getpixel) (SDL_Surface *, int, int) =
    getpixels[src->format->BytesPerPixel];
    
    
    /* Determine scale and centering offsets: */
    
    if (!keep_aspect)
    {
        yscale = (float) ((float) src->h / (float) max_y);
        xscale = (float) ((float) src->w / (float) max_x);
        
        off_x = 0;
        off_y = 0;
    }
    else
    {
        if (src->h > src->w)
        {
            yscale = (float) ((float) src->h / (float) max_y);
            xscale = yscale;
            
            off_x = ((src->h - src->w) / xscale) / 2;
            off_y = 0;
        }
        else
        {
            xscale = (float) ((float) src->w / (float) max_x);
            yscale = xscale;
            
            off_x = 0;
            off_y = ((src->w - src->h) / xscale) / 2;
        }
    }
    
    
#ifndef NO_BILINEAR
    if (max_x > src->w && max_y > src->h)
        return(zoom(src, max_x, max_y));
#endif
    
    
    /* Create surface for thumbnail: */
    
    s = SDL_CreateRGBSurface(src->flags,	/* SDL_SWSURFACE, */
                             max_x, max_y, src->format->BitsPerPixel, src->format->Rmask, src->format->Gmask, src->format->Bmask, src->format->Amask);
    
    
    if (s == NULL)
    {
        fprintf(stderr, "\nError: Can't build stamp thumbnails\n"
                "The Simple DirectMedia Layer error that occurred was:\n"
                "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
    putpixel = putpixels[s->format->BytesPerPixel];
    
    /* Create thumbnail itself: */
    
    SDL_LockSurface(src);
    SDL_LockSurface(s);
    
    for (y = 0; y < max_y; y++)
    {
        for (x = 0; x < max_x; x++)
        {
#ifndef LOW_QUALITY_THUMBNAILS
            tr = 0;
            tg = 0;
            tb = 0;
            ta = 0;
            
            tmp = 0;
            
            for (src_y = y * yscale; src_y < y * yscale + yscale &&
                 src_y < src->h; src_y++)
            {
                for (src_x = x * xscale; src_x < x * xscale + xscale &&
                     src_x < src->w; src_x++)
                {
                    SDL_GetRGBA(getpixel(src, src_x, src_y),
                                src->format, &r, &g, &b, &a);
                    
                    tr = tr + r;
                    tb = tb + b;
                    tg = tg + g;
                    ta = ta + a;
                    
                    tmp++;
                }
            }
            
            if (tmp != 0)
            {
                tr = tr / tmp;
                tb = tb / tmp;
                tg = tg / tmp;
                ta = ta / tmp;
                
                if (keep_alpha == 0 && s->format->Amask != 0)
                {
                    tr = ((ta * tr) / 255) + (255 - ta);
                    tg = ((ta * tg) / 255) + (255 - ta);
                    tb = ((ta * tb) / 255) + (255 - ta);
                    
                    putpixel(s, x + off_x, y + off_y, SDL_MapRGBA(s->format,
                                                                  (Uint8) tr,
                                                                  (Uint8) tg,
                                                                  (Uint8) tb, 0xff));
                }
                else
                {
                    putpixel(s, x + off_x, y + off_y, SDL_MapRGBA(s->format,
                                                                  (Uint8) tr,
                                                                  (Uint8) tg,
                                                                  (Uint8) tb,
                                                                  (Uint8) ta));
                }
            }
#else
            src_x = x * xscale;
            src_y = y * yscale;
            
            putpixel(s, x + off_x, y + off_y, getpixel(src, src_x, src_y));
#endif
        }
    }
    
    SDL_UnlockSurface(s);
    SDL_UnlockSurface(src);
    
    return s;
}


#ifndef NO_BILINEAR

/* Based on code from: http://www.codeproject.com/cs/media/imageprocessing4.asp
 copyright 2002 Christian Graus */

static SDL_Surface *zoom(SDL_Surface * src, int new_w, int new_h)
{
    SDL_Surface * s;
    void (*putpixel) (SDL_Surface *, int, int, Uint32);
    Uint32(*getpixel) (SDL_Surface *, int, int) =
    getpixels[src->format->BytesPerPixel];
    float xscale, yscale;
    int x, y;
    float floor_x, ceil_x, floor_y, ceil_y, fraction_x, fraction_y,
    one_minus_x, one_minus_y;
    float n1, n2;
    float r1, g1, b1, a1;
    float r2, g2, b2, a2;
    float r3, g3, b3, a3;
    float r4, g4, b4, a4;
    Uint8 r, g, b, a;
    
    
    /* Create surface for zoom: */
    
    s = SDL_CreateRGBSurface(src->flags,	/* SDL_SWSURFACE, */
                             new_w, new_h, src->format->BitsPerPixel,
                             src->format->Rmask,
                             src->format->Gmask,
                             src->format->Bmask,
                             src->format->Amask);
    
    
    if (s == NULL)
    {
        fprintf(stderr, "\nError: Can't build zoom surface\n"
                "The Simple DirectMedia Layer error that occurred was:\n"
                "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
    putpixel = putpixels[s->format->BytesPerPixel];
    
    
    SDL_LockSurface(src);
    SDL_LockSurface(s);
    
    xscale = (float) src->w / (float) new_w;
    yscale = (float) src->h / (float) new_h;
    
    for (x = 0; x < new_w; x++)
    {
        for (y = 0; y < new_h; y++)
        {
            floor_x = floor((float) x * xscale);
            ceil_x = floor_x + 1;
            if (ceil_x >= src->w)
                ceil_x = floor_x;
            
            floor_y = floor((float) y * yscale);
            ceil_y = floor_y + 1;
            if (ceil_y >= src->h)
                ceil_y = floor_y;
            
            fraction_x = x * xscale - floor_x;
            fraction_y = y * yscale - floor_y;
            
            one_minus_x = 1.0 - fraction_x;
            one_minus_y = 1.0 - fraction_y;
            
#if VIDEO_BPP==32
            SDL_GetRGBA(getpixel(src, floor_x, floor_y), src->format,
                        &r1, &g1, &b1, &a1);
            SDL_GetRGBA(getpixel(src, ceil_x,  floor_y), src->format,
                        &r2, &g2, &b2, &a2);
            SDL_GetRGBA(getpixel(src, floor_x, ceil_y),  src->format,
                        &r3, &g3, &b3, &a3);
            SDL_GetRGBA(getpixel(src, ceil_x,  ceil_y),  src->format,
                        &r4, &g4, &b4, &a4);
#else
            {
                Uint8 r, g, b, a;
                r = g = b = a = 0; /* Unused, bah! */
                
                SDL_GetRGBA(getpixel(src, floor_x, floor_y), src->format,
                            &r, &g, &b, &a);
                r1 = (float) r;
                g1 = (float) g;
                b1 = (float) b;
                a1 = (float) a;
                
                SDL_GetRGBA(getpixel(src, ceil_x,  floor_y), src->format,
                            &r, &g, &b, &a);
                r2 = (float) r;
                g2 = (float) g;
                b2 = (float) b;
                a2 = (float) a;
                
                SDL_GetRGBA(getpixel(src, floor_x, ceil_y),  src->format,
                            &r, &g, &b, &a);
                r3 = (float) r;
                g3 = (float) g;
                b3 = (float) b;
                a3 = (float) a;
                
                SDL_GetRGBA(getpixel(src, ceil_x,  ceil_y),  src->format,
                            &r, &g, &b, &a);
                r4 = (float) r;
                g4 = (float) g;
                b4 = (float) b;
                a4 = (float) a;
            }
#endif
            
            n1 = (one_minus_x * r1 + fraction_x * r2);
            n2 = (one_minus_x * r3 + fraction_x * r4);
            r = (one_minus_y * n1 + fraction_y * n2);
            
            n1 = (one_minus_x * g1 + fraction_x * g2);
            n2 = (one_minus_x * g3 + fraction_x * g4);
            g = (one_minus_y * n1 + fraction_y * n2);
            
            n1 = (one_minus_x * b1 + fraction_x * b2);
            n2 = (one_minus_x * b3 + fraction_x * b4);
            b = (one_minus_y * n1 + fraction_y * n2);
            
            n1 = (one_minus_x * a1 + fraction_x * a2);
            n2 = (one_minus_x * a3 + fraction_x * a4);
            a = (one_minus_y * n1 + fraction_y * n2);
            
            putpixel(s, x, y, SDL_MapRGBA(s->format, r, g, b, a));
        }
    }
    
    SDL_UnlockSurface(s);
    SDL_UnlockSurface(src);
    
    return s;
    
}
#endif


/* XOR must show up on black, white, 0x7f grey, and 0x80 grey.
 XOR must be exactly 100% perfectly reversable. */
static void xorpixel(int x, int y)
{
    Uint8 *p;
    int BytesPerPixel;
    
    /* if outside the canvas, return */
    if ((unsigned) x >= (unsigned) canvas->w
        || (unsigned) y >= (unsigned) canvas->h)
        return;
    /* now switch to screen coordinates */
    x += r_canvas.x;
    y += r_canvas.y;
    
    /* Always 4, except 3 when loading a saved image. */
    BytesPerPixel = screen->format->BytesPerPixel;
    
    /* Set a pointer to the exact location in memory of the pixel */
    p = (Uint8 *) (((Uint8 *) screen->pixels) +	/* Start: beginning of RAM */
                   (y * screen->pitch) +	/* Go down Y lines */
                   (x * BytesPerPixel));	/* Go in X pixels */
    
    /* XOR the (correctly-sized) piece of data in the screen's RAM */
    if (likely(BytesPerPixel == 4))
        *(Uint32 *) p ^= 0x80808080u;	/* 32-bit display */
    else if (BytesPerPixel == 1)
        *p ^= 0x80;
    else if (BytesPerPixel == 2)
        *(Uint16 *) p ^= 0xd6d6;
    else if (BytesPerPixel == 3)
    {
        p[0] ^= 0x80;
        p[1] ^= 0x80;
        p[2] ^= 0x80;
    }
}


/* Undo! */

static void do_undo(void)
{
    int wanna_update_toolbar;
    
    wanna_update_toolbar = 0;
    
    if (cur_undo != oldest_undo)
    {
        cur_undo--;
        
        if (cur_undo < 0)
            cur_undo = NUM_UNDO_BUFS - 1;
        
#ifdef DEBUG
        printf("BLITTING: %d\n", cur_undo);
#endif
        SDL_BlitSurface(undo_bufs[cur_undo], NULL, canvas, NULL);
        
        
        if (img_starter != NULL)
        {
            if (undo_starters[cur_undo] == UNDO_STARTER_MIRRORED)
            {
                starter_mirrored = !starter_mirrored;
                mirror_starter();
            }
            else if (undo_starters[cur_undo] == UNDO_STARTER_FLIPPED)
            {
                starter_flipped = !starter_flipped;
                flip_starter();
            }
        }
        
        update_canvas(0, 0, (WINDOW_WIDTH - 96), (48 * 7) + 40 + HEIGHTOFFSET);
        
        
        if (cur_undo == oldest_undo)
        {
            tool_avail[TOOL_UNDO] = 0;
            wanna_update_toolbar = 1;
        }
        
        if (tool_avail[TOOL_REDO] == 0)
        {
            tool_avail[TOOL_REDO] = 1;
            wanna_update_toolbar = 1;
        }
        
        if (wanna_update_toolbar)
        {
            draw_toolbar();
            update_screen_rect(&r_tools);
        }
        
        been_saved = 0;
    }
    
#ifdef DEBUG
    printf("UNDO: Current=%d  Oldest=%d  Newest=%d\n",
           cur_undo, oldest_undo, newest_undo);
#endif
}


/* Redo! */

static void do_redo(void)
{
    if (cur_undo != newest_undo)
    {
        if (img_starter != NULL)
        {
            if (undo_starters[cur_undo] == UNDO_STARTER_MIRRORED)
            {
                starter_mirrored = !starter_mirrored;
                mirror_starter();
            }
            else if (undo_starters[cur_undo] == UNDO_STARTER_FLIPPED)
            {
                starter_flipped = !starter_flipped;
                flip_starter();
            }
        }
        
        cur_undo = (cur_undo + 1) % NUM_UNDO_BUFS;
        
#ifdef DEBUG
        printf("BLITTING: %d\n", cur_undo);
#endif
        SDL_BlitSurface(undo_bufs[cur_undo], NULL, canvas, NULL);
        
        update_canvas(0, 0, (WINDOW_WIDTH - 96), (48 * 7) + 40 + HEIGHTOFFSET);
        
        been_saved = 0;
    }
    
#ifdef DEBUG
    printf("REDO: Current=%d  Oldest=%d  Newest=%d\n",
           cur_undo, oldest_undo, newest_undo);
#endif
    
    
    if (((cur_undo + 1) % NUM_UNDO_BUFS) == newest_undo)
    {
        tool_avail[TOOL_REDO] = 0;
    }
    
    tool_avail[TOOL_UNDO] = 1;
    
    draw_toolbar();
    update_screen_rect(&r_tools);
}


/* Create the current brush in the current color: */

static void render_brush(void)
{
    Uint32 amask;
    int x, y;
    Uint8 r, g, b, a;
    Uint32(*getpixel_brush) (SDL_Surface *, int, int) =
    getpixels[img_brushes[cur_brush]->format->BytesPerPixel];
    void (*putpixel_brush) (SDL_Surface *, int, int, Uint32) =
    putpixels[img_brushes[cur_brush]->format->BytesPerPixel];
    
    
    /* Kludge; not sure why cur_brush would become greater! */
    
    if (cur_brush >= num_brushes)
        cur_brush = 0;
    
    
    /* Free the old rendered brush (if any): */
    
    if (img_cur_brush != NULL)
    {
        SDL_FreeSurface(img_cur_brush);
    }
    
    
    /* Create a surface to render into: */
    
    amask = ~(img_brushes[cur_brush]->format->Rmask |
              img_brushes[cur_brush]->format->Gmask |
              img_brushes[cur_brush]->format->Bmask);
    
    img_cur_brush =
    SDL_CreateRGBSurface(SDL_SWSURFACE,
                         img_brushes[cur_brush]->w,
                         img_brushes[cur_brush]->h,
                         img_brushes[cur_brush]->format->BitsPerPixel,
                         img_brushes[cur_brush]->format->Rmask,
                         img_brushes[cur_brush]->format->Gmask,
                         img_brushes[cur_brush]->format->Bmask, amask);
    
    if (img_cur_brush == NULL)
    {
        fprintf(stderr, "\nError: Can't render a brush!\n"
                "The Simple DirectMedia Layer error that occurred was:\n"
                "%s\n\n", SDL_GetError());
        
        cleanup();
        exit(1);
    }
    
    
    /* Render the new brush: */
    
    SDL_LockSurface(img_brushes[cur_brush]);
    SDL_LockSurface(img_cur_brush);
    
    for (y = 0; y < img_brushes[cur_brush]->h; y++)
    {
        for (x = 0; x < img_brushes[cur_brush]->w; x++)
        {
            SDL_GetRGBA(getpixel_brush(img_brushes[cur_brush], x, y),
                        img_brushes[cur_brush]->format, &r, &g, &b, &a);
            
            if (r == g && g == b)
            {
                putpixel_brush(img_cur_brush, x, y,
                               SDL_MapRGBA(img_cur_brush->format,
                                           color_hexes[cur_color][0],
                                           color_hexes[cur_color][1],
                                           color_hexes[cur_color][2], a));
            }
            else
            {
                putpixel_brush(img_cur_brush, x, y,
                               SDL_MapRGBA(img_cur_brush->format,
                                           (r + color_hexes[cur_color][0]) >> 1,
                                           (g + color_hexes[cur_color][1]) >> 1,
                                           (b + color_hexes[cur_color][2]) >> 1, a));
            }
        }
    }
    
    SDL_UnlockSurface(img_cur_brush);
    SDL_UnlockSurface(img_brushes[cur_brush]);
    
    img_cur_brush_frame_w = img_cur_brush->w / abs(brushes_frames[cur_brush]);
    img_cur_brush_w = img_cur_brush_frame_w /
    (brushes_directional[cur_brush] ? 3 : 1);
    img_cur_brush_h = img_cur_brush->h / (brushes_directional[cur_brush] ? 3 : 1);
    img_cur_brush_frames = brushes_frames[cur_brush];
    img_cur_brush_directional = brushes_directional[cur_brush];
    img_cur_brush_spacing = brushes_spacing[cur_brush];
    
    brush_counter = 0;
}


/* Draw a XOR line: */

static void line_xor(int x1, int y1, int x2, int y2)
{
    int dx, dy, y, num_drawn;
    float m, b;
    
    
    /* Kludgey, but it works: */
    
    /* SDL_LockSurface(screen); */
    
    dx = x2 - x1;
    dy = y2 - y1;
    
    num_drawn = 0;
    
    if (dx != 0)
    {
        m = ((float) dy) / ((float) dx);
        b = y1 - m * x1;
        
        if (x2 >= x1)
            dx = 1;
        else
            dx = -1;
        
        
        while (x1 != x2)
        {
            y1 = m * x1 + b;
            y2 = m * (x1 + dx) + b;
            
            if (y1 > y2)
            {
                y = y1;
                y1 = y2;
                y2 = y;
            }
            
            for (y = y1; y <= y2; y++)
            {
                num_drawn++;
                if (num_drawn < 10 || dont_do_xor == 0)
                    xorpixel(x1, y);
            }
            
            x1 = x1 + dx;
        }
    }
    else
    {
        if (y1 > y2)
        {
            for (y = y1; y >= y2; y--)
            {
                num_drawn++;
                
                if (num_drawn < 10 || dont_do_xor == 0)
                    xorpixel(x1, y);
            }
        }
        else
        {
            for (y = y1; y <= y2; y++)
            {
                num_drawn++;
                
                if (num_drawn < 10 || dont_do_xor == 0)
                    xorpixel(x1, y);
            }
        }
    }
    
    /* SDL_UnlockSurface(screen); */
}


/* Draw a XOR rectangle: */

static void rect_xor(int x1, int y1, int x2, int y2)
{
    if (x1 < 0)
        x1 = 0;
    
    if (x2 < 0)
        x2 = 0;
    
    if (y1 < 0)
        y1 = 0;
    
    if (y2 < 0)
        y2 = 0;
    
    if (x1 >= (WINDOW_WIDTH - 96 - 96))
        x1 = (WINDOW_WIDTH - 96 - 96) - 1;
    
    if (x2 >= (WINDOW_WIDTH - 96 - 96))
        x2 = (WINDOW_WIDTH - 96 - 96) - 1;
    
    if (y1 >= (48 * 7) + 40 + HEIGHTOFFSET)
        y1 = (48 * 7) + 40 + HEIGHTOFFSET - 1;
    
    if (y2 >= (48 * 7) + 40 + HEIGHTOFFSET)
        y2 = (48 * 7) + 40 + HEIGHTOFFSET - 1;
    
    line_xor(x1, y1, x2, y1);
    line_xor(x2, y1, x2, y2);
    line_xor(x2, y2, x1, y2);
    line_xor(x1, y2, x1, y1);
}


/* Erase at the cursor! */

static void do_eraser(int x, int y)
{
    SDL_Rect dest;
    int sz;
    int xx, yy, n, hit;
    
    if (cur_eraser < NUM_ERASERS / 2)
    {
        /* Square eraser: */
        
        sz = (ERASER_MIN +
              (((NUM_ERASERS / 2) - 1 - cur_eraser) *
               ((ERASER_MAX - ERASER_MIN) / ((NUM_ERASERS / 2) - 1))));
        
        dest.x = x - (sz / 2);
        dest.y = y - (sz / 2);
        dest.w = sz;
        dest.h = sz;
        
        if (img_starter_bkgd == NULL)
        {
            SDL_FillRect(canvas, &dest, SDL_MapRGB(canvas->format,
                                                   canvas_color_r,
                                                   canvas_color_g,
                                                   canvas_color_b));
        }
        else
        {
            SDL_BlitSurface(img_starter_bkgd, &dest, canvas, &dest);
        }
    }
    else
    {
        /* Round eraser: */
        
        sz = (ERASER_MIN +
              (((NUM_ERASERS / 2) - 1 - (cur_eraser - (NUM_ERASERS / 2))) *
               ((ERASER_MAX - ERASER_MIN) / ((NUM_ERASERS / 2) - 1))));
        
        for (yy = 0; yy < sz; yy++)
        {
            hit = 0;
            for (xx = 0; xx <= sz && hit == 0; xx++)
            {
                n = (xx * xx) + (yy * yy) - ((sz / 2) * (sz / 2));
                
                if (n >= -sz && n <= sz)
                    hit = 1;
                
                if (hit)
                {
                    dest.x = x - xx;
                    dest.y = y - yy;
                    dest.w = xx * 2;
                    dest.h = 1;
                    
                    if (img_starter_bkgd == NULL)
                    {
                        SDL_FillRect(canvas, &dest, SDL_MapRGB(canvas->format,
                                                               canvas_color_r,
                                                               canvas_color_g,
                                                               canvas_color_b));
                    }
                    else
                    {
                        SDL_BlitSurface(img_starter_bkgd, &dest, canvas, &dest);
                    }
                    
                    
                    dest.x = x - xx;
                    dest.y = y + yy;
                    dest.w = xx * 2;
                    dest.h = 1;
                    
                    if (img_starter_bkgd == NULL)
                    {
                        SDL_FillRect(canvas, &dest, SDL_MapRGB(canvas->format,
                                                               canvas_color_r,
                                                               canvas_color_g,
                                                               canvas_color_b));
                    }
                    else
                    {
                        SDL_BlitSurface(img_starter_bkgd, &dest, canvas, &dest);
                    }
                }
            }
        }
    }
    
    
#ifndef NOSOUND
    if (!mute && use_sound)
    {
        if (!Mix_Playing(0))
        {
            eraser_sound = (eraser_sound + 1) % 2;
            
            playsound(screen, 0, SND_ERASER1 + eraser_sound, 0, x, SNDDIST_NEAR);
        }
    }
#endif
    
    update_canvas(x - sz / 2, y - sz / 2, x + sz / 2, y + sz / 2);
    
    rect_xor(x - sz / 2, y - sz / 2, x + sz / 2, y + sz / 2);
    
#ifdef __APPLE__
    /* Prevent ghosted eraser outlines from remaining on the screen in windowed mode */
    update_screen(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
#endif
}


/* Reset available tools (for new image / starting out): */

static void reset_avail_tools(void)
{
    int i;
    int disallow_print = disable_print;	/* set to 1 later if printer unavail */
    
    for (i = 0; i < NUM_TOOLS; i++)
    {
        tool_avail[i] = 1;
    }
    
    
    /* Unavailable at the beginning of a new canvas: */
    
    tool_avail[TOOL_UNDO] = 0;
    tool_avail[TOOL_REDO] = 0;
    
    if (been_saved)
        tool_avail[TOOL_SAVE] = 0;
    
    
    /* Unavailable in rare circumstances: */
    
    if (num_stamps[0] == 0)
        tool_avail[TOOL_STAMP] = 0;
    
    if (num_magics == 0)
        tool_avail[TOOL_MAGIC] = 0;
    
    
    /* Disable quit? */
    
//    if (disable_quit)
//        tool_avail[TOOL_QUIT] = 0;
    
    
    /* Disable save? */
    
    if (disable_save)
        tool_avail[TOOL_SAVE] = 0;
    
    
#ifdef WIN32
    if (!IsPrinterAvailable())
        disallow_print = 1;
#endif
    
#ifdef __BEOS__
    if (!IsPrinterAvailable())
        disallow_print = disable_print = 1;
#endif
    
    
    /* Disable print? */
    
//    if (disallow_print)
//        tool_avail[TOOL_PRINT] = 0;
    
#ifdef NOKIA_770
    /* There is no way for the user to enter text, so just disable this. */
    tool_avail[TOOL_TEXT] = 0;
#endif
}


/* Save and disable available tools (for Open-Dialog) */

static void disable_avail_tools(void)
{
    int i;
    
    hide_blinking_cursor();
    for (i = 0; i < NUM_TOOLS; i++)
    {
        tool_avail_bak[i] = tool_avail[i];
        tool_avail[i] = 0;
    }
}

/* Restore and enable available tools (for End-Of-Open-Dialog) */

static void enable_avail_tools(void)
{
    int i;
    
    for (i = 0; i < NUM_TOOLS; i++)
    {
        tool_avail[i] = tool_avail_bak[i];
    }
}


/* For qsort() call in do_open()... */

static int compare_dirent2s(struct dirent2 *f1, struct dirent2 *f2)
{
#ifdef DEBUG
    printf("compare_dirents: %s\t%s\n", f1->f.d_name, f2->f.d_name);
#endif
    
    if (f1->place == f2->place)
        return (strcmp(f1->f.d_name, f2->f.d_name));
    else
        return (f1->place - f2->place);
}


/* Draw tux's text on the screen: */

static void draw_tux_text(int which_tux, const char *const str,
                          int want_right_to_left)
{
    draw_tux_text_ex(which_tux, str, want_right_to_left, 0);
}

int latest_tux;
const char * latest_tux_text;
int latest_r2l;
Uint8 latest_locale_text;

static void redraw_tux_text(void)
{
    draw_tux_text_ex(latest_tux, latest_tux_text, latest_r2l, latest_locale_text);
}

static void draw_tux_text_ex(int which_tux, const char *const str,
                             int want_right_to_left, Uint8 locale_text)
{
    SDL_Rect dest;
    SDL_Color black = { 0, 0, 0, 0 };
    int w;
    SDL_Surface * btn;
    
    latest_tux = which_tux;
    latest_tux_text = str;
    latest_r2l = want_right_to_left;
    latest_locale_text = locale_text;
    
    
    /* Remove any text-changing timer if one is running: */
    control_drawtext_timer(0, "", 0);
    
    /* Clear first: */
    SDL_FillRect(screen, &r_tuxarea, SDL_MapRGB(screen->format, 255, 255, 255));
    
    /* Draw tux: */
    dest.x = r_tuxarea.x;
    dest.y = r_tuxarea.y + r_tuxarea.h - img_tux[which_tux]->h;
    
    /* if he's too tall to fit, go off the bottom (not top) edge */
    if (dest.y < r_tuxarea.y)
        dest.y = r_tuxarea.y;
    
    /* Don't let sfx and speak buttons cover the top of Tux, either: */
    if (cur_tool == TOOL_STAMP && use_sound && !mute)
    {
        if (dest.y < r_sfx.y + r_sfx.h)
            dest.y = r_sfx.y + r_sfx.h;
    }
    
    SDL_BlitSurface(img_tux[which_tux], NULL, screen, &dest);
    
    /* Wide enough for Tux, or two stamp sound buttons (whichever's wider) */
    w = max(img_tux[which_tux]->w, img_btnsm_up->w) + 5;
    
    wordwrap_text_ex(str, black,
                     w, r_tuxarea.y, r_tuxarea.w, want_right_to_left,
                     locale_text);
    
    
    /* Draw 'sound effect' and 'speak' buttons, if we're in the Stamp tool */
    
    if (cur_tool == TOOL_STAMP && use_sound && !mute)
    {
        /* Sound effect: */
        
        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->no_sound)
            btn = img_btnsm_off;
        else
            btn = img_btnsm_up;
        
        dest.x = 0;
        dest.y = r_tuxarea.y;
        
        SDL_BlitSurface(btn, NULL, screen, &dest);
        
        dest.x = (img_btnsm_up->w - img_sfx->w) / 2;
        dest.y = r_tuxarea.y + ((img_btnsm_up->h - img_sfx->h) / 2);
        
        SDL_BlitSurface(img_sfx, NULL, screen, &dest);
        
        
        /* Speak: */
        
        if (stamp_data[stamp_group][cur_stamp[stamp_group]]->no_descsound)
            btn = img_btnsm_off;
        else
            btn = img_btnsm_up;
        
        dest.x = img_btnsm_up->w;
        dest.y = r_tuxarea.y;
        
        SDL_BlitSurface(btn, NULL, screen, &dest);
        
        dest.x = img_btnsm_up->w + ((img_btnsm_up->w - img_speak->w) / 2);
        dest.y = r_tuxarea.y + ((img_btnsm_up->h - img_speak->h) / 2);
        
        SDL_BlitSurface(img_speak, NULL, screen, &dest);
    }
    
    update_screen_rect(&r_tuxarea);
}


static void wordwrap_text(const char *const str, SDL_Color color,
                          int left, int top, int right,
                          int want_right_to_left)
{
    wordwrap_text_ex(str, color, left, top, right, want_right_to_left, 0);
}

static void wordwrap_text_ex(const char *const str, SDL_Color color,
                             int left, int top, int right,
                             int want_right_to_left, Uint8 locale_text)
{
    SDL_Surface *text;
    TuxPaint_Font *myfont = medium_font;
    SDL_Rect dest;
#ifdef NO_SDLPANGO
    int len;
    int x, y, j;
    unsigned int i;
    char substr[512];
    unsigned char *locale_str;
    char *tstr;
    unsigned char utf8_char[5];
    SDL_Rect src;
    int utf8_str_len, last_text_height;
    unsigned char utf8_str[512];
#else
    SDLPango_Matrix pango_color;
#endif
    
    
    if (str == NULL || str[0] == '\0')
        return; /* No-op! */
    
    if (need_own_font && (strcmp(gettext(str), str) || locale_text))
        myfont = locale_font;
    
    if (strcmp(str, gettext(str)) == 0)
    {
        /* String isn't translated!  Don't write right-to-left, even if our locale is an RTOL language: */
        want_right_to_left = 0;
    }
    
    
#ifndef NO_SDLPANGO
    /* Letting SDL_Pango do all this stuff! */
    
    sdl_color_to_pango_color(color, &pango_color);
    
    SDLPango_SetDefaultColor(myfont->pango_context, &pango_color);
    SDLPango_SetMinimumSize(myfont->pango_context, right - left, canvas->h - top);
    if (want_right_to_left && need_right_to_left)
    {
        SDLPango_SetBaseDirection(locale_font->pango_context, SDLPANGO_DIRECTION_RTL);
        if (only_uppercase)
        {
            char * upper_str = uppercase(gettext(str));
            SDLPango_SetText_GivenAlignment(myfont->pango_context, upper_str, -1, SDLPANGO_ALIGN_RIGHT);
            free(upper_str);
        }
        else
            SDLPango_SetText_GivenAlignment(myfont->pango_context, gettext(str), -1, SDLPANGO_ALIGN_RIGHT);
    }
    else
    {
        SDLPango_SetBaseDirection(locale_font->pango_context, SDLPANGO_DIRECTION_LTR);
        if (only_uppercase)
        {
            char * upper_str = uppercase(gettext(str));
            SDLPango_SetText_GivenAlignment(myfont->pango_context, upper_str, -1, SDLPANGO_ALIGN_LEFT);
            free(upper_str);
        }
        else
            SDLPango_SetText_GivenAlignment(myfont->pango_context, gettext(str), -1, SDLPANGO_ALIGN_LEFT);
    }
    
    text = SDLPango_CreateSurfaceDraw(myfont->pango_context);
    
    dest.x = left;
    dest.y = top;
    if (text != NULL)
    {
        SDL_BlitSurface(text, NULL, screen, &dest);
        SDL_FreeSurface(text);
    }
#else
    
    /* Cursor starting position: */
    
    x = left;
    y = top;
    
    last_text_height = 0;
    
    debug(str);
    debug(gettext(str));
    debug("...");
    
    if (strcmp(str, "") != 0)
    {
        if (want_right_to_left == 0)
            locale_str = (unsigned char *) strdup(gettext(str));
        else
            locale_str = (unsigned char *) textdir(gettext(str));
        
        
        /* For each UTF8 character: */
        
        utf8_str_len = 0;
        utf8_str[0] = '\0';
        
        for (i = 0; i <= strlen((char *) locale_str); i++)
        {
            if (locale_str[i] < 128)
            {
                utf8_str[utf8_str_len++] = locale_str[i];
                utf8_str[utf8_str_len] = '\0';
                
                
                /* Space?  Blit the word! (Word-wrap if necessary) */
                
                if (locale_str[i] == ' ' || locale_str[i] == '\0')
                {
                    if (only_uppercase)
                    {
                        char * upper_utf8_str = uppercase((char *) utf8_str);
                        text = render_text(myfont, (char *) upper_utf8_str, color);
                        free(upper_utf8_str);
                    }
                    else
                        text = render_text(myfont, (char *) utf8_str, color);
                    
                    if (!text)
                        continue;		/* Didn't render anything... */
                    
                    /* ----------- */
                    if (text->w > right - left)
                    {
                        /* Move left and down (if not already at left!) */
                        
                        if (x > left)
                        {
                            if (need_right_to_left && want_right_to_left)
                                anti_carriage_return(left, right, top, top + text->h,
                                                     y + text->h, x - left);
                            
                            x = left;
                            y = y + text->h;
                        }
                        
                        
                        /* Junk the blitted word; it's too long! */
                        
                        last_text_height = text->h;
                        SDL_FreeSurface(text);
                        
                        
                        /* For each UTF8 character: */
                        
                        for (j = 0; j < utf8_str_len; j++)
                        {
                            /* How many bytes does this character need? */
                            
                            if (utf8_str[j] < 128)	/* 0xxx xxxx - 1 byte */
                            {
                                utf8_char[0] = utf8_str[j];
                                utf8_char[1] = '\0';
                            }
                            else if ((utf8_str[j] & 0xE0) == 0xC0)	/* 110x xxxx - 2 bytes */
                            {
                                utf8_char[0] = utf8_str[j];
                                utf8_char[1] = utf8_str[j + 1];
                                utf8_char[2] = '\0';
                                j = j + 1;
                            }
                            else if ((utf8_str[j] & 0xF0) == 0xE0)	/* 1110 xxxx - 3 bytes */
                            {
                                utf8_char[0] = utf8_str[j];
                                utf8_char[1] = utf8_str[j + 1];
                                utf8_char[2] = utf8_str[j + 2];
                                utf8_char[3] = '\0';
                                j = j + 2;
                            }
                            else		/* 1111 0xxx - 4 bytes */
                            {
                                utf8_char[0] = utf8_str[j];
                                utf8_char[1] = utf8_str[j + 1];
                                utf8_char[2] = utf8_str[j + 2];
                                utf8_char[3] = utf8_str[j + 3];
                                utf8_char[4] = '\0';
                                j = j + 3;
                            }
                            
                            
                            if (utf8_char[0] != '\0')
                            {
                                text = render_text(myfont, (char *) utf8_char, color);
                                if (text != NULL)
                                {
                                    if (x + text->w > right)
                                    {
                                        if (need_right_to_left && want_right_to_left)
                                            anti_carriage_return(left, right, top, top + text->h,
                                                                 y + text->h, x - left);
                                        
                                        x = left;
                                        y = y + text->h;
                                    }
                                    
                                    dest.x = x;
                                    
                                    if (need_right_to_left && want_right_to_left)
                                        dest.y = top;
                                    else
                                        dest.y = y;
                                    
                                    SDL_BlitSurface(text, NULL, screen, &dest);
                                    
                                    last_text_height = text->h;
                                    
                                    x = x + text->w;
                                    
                                    SDL_FreeSurface(text);
                                }
                            }
                        }
                    }
                    else
                    {
                        /* Not too wide for one line... */
                        
                        if (x + text->w > right)
                        {
                            /* This word needs to move down? */
                            
                            if (need_right_to_left && want_right_to_left)
                                anti_carriage_return(left, right, top, top + text->h,
                                                     y + text->h, x - left);
                            
                            x = left;
                            y = y + text->h;
                        }
                        
                        dest.x = x;
                        
                        if (need_right_to_left && want_right_to_left)
                            dest.y = top;
                        else
                            dest.y = y;
                        
                        SDL_BlitSurface(text, NULL, screen, &dest);
                        
                        last_text_height = text->h;
                        x = x + text->w;
                        
                        SDL_FreeSurface(text);
                    }
                    
                    
                    utf8_str_len = 0;
                    utf8_str[0] = '\0';
                }
            }
            else if ((locale_str[i] & 0xE0) == 0xC0)
            {
                utf8_str[utf8_str_len++] = locale_str[i];
                utf8_str[utf8_str_len++] = locale_str[i + 1];
                utf8_str[utf8_str_len] = '\0';
                i++;
            }
            else if ((locale_str[i] & 0xF0) == 0xE0)
            {
                utf8_str[utf8_str_len++] = locale_str[i];
                utf8_str[utf8_str_len++] = locale_str[i + 1];
                utf8_str[utf8_str_len++] = locale_str[i + 2];
                utf8_str[utf8_str_len] = '\0';
                i = i + 2;
            }
            else
            {
                utf8_str[utf8_str_len++] = locale_str[i];
                utf8_str[utf8_str_len++] = locale_str[i + 1];
                utf8_str[utf8_str_len++] = locale_str[i + 2];
                utf8_str[utf8_str_len++] = locale_str[i + 3];
                utf8_str[utf8_str_len] = '\0';
                i = i + 3;
            }
        }
        
        free(locale_str);
    }
    else if (strlen(str) != 0)
    {
        /* Truncate if too big! (sorry!) */
        
        {
            char *s1 = gettext(str);
            if (want_right_to_left)
            {
                char *freeme = s1;
                s1 = textdir(s1);
                free(freeme);
            }
            tstr = uppercase(s1);
            free(s1);
        }
        
        if (strlen(tstr) > sizeof(substr) - 1)
            tstr[sizeof(substr) - 1] = '\0';
        
        
        /* For each word... */
        
        for (i = 0; i < strlen(tstr); i++)
        {
            /* Figure out the word... */
            
            len = 0;
            
            for (j = i; tstr[j] != ' ' && tstr[j] != '\0'; j++)
            {
                substr[len++] = tstr[j];
            }
            
            substr[len++] = ' ';
            substr[len] = '\0';
            
            
            /* Render the word for display... */
            
            
            if (only_uppercase)
            {
                char * uppercase_substr = uppercase(substr);
                text = render_text(myfont, uppercase_substr, color);
                free(uppercase_substr);
            }
            else
                text = render_text(myfont, substr, color);
            
            
            /* If it won't fit on this line, move to the next! */
            
            if (x + text->w > right)	/* Correct? */
            {
                if (need_right_to_left && want_right_to_left)
                    anti_carriage_return(left, right, top, top + text->h, y + text->h,
                                         x - left);
                
                x = left;
                y = y + text->h;
            }
            
            
            /* Draw the word: */
            
            dest.x = x;
            
            if (need_right_to_left && want_right_to_left)
                dest.y = top;
            else
                dest.y = y;
            
            SDL_BlitSurface(text, NULL, screen, &dest);
            
            
            /* Move the cursor one word's worth: */
            
            x = x + text->w;
            
            
            /* Free the temp. surface: */
            
            last_text_height = text->h;
            SDL_FreeSurface(text);
            
            
            /* Now on to the next word... */
            
            i = j;
        }
        
        free(tstr);
    }
    
    
    /* Right-justify the final line of text, in right-to-left mode: */
    
    if (need_right_to_left && want_right_to_left && last_text_height > 0)
    {
        src.x = left;
        src.y = top;
        src.w = x - left;
        src.h = last_text_height;
        
        dest.x = right - src.w;
        dest.y = top;
        
        SDL_BlitSurface(screen, &src, screen, &dest);
        
        dest.x = left;
        dest.y = top;
        dest.w = (right - left - src.w);
        dest.h = last_text_height;
        
        SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
    }
#endif
}


#ifndef NOSOUND

static void playstampdesc(int chan)
{
    static SDL_Event playsound_event;
    
    if (chan == 2) /* Only do this when the channel playing the stamp sfx has ended! */
    {
        debug("Stamp SFX ended. Pushing event to play description sound...");
        
        playsound_event.type = SDL_USEREVENT;
        playsound_event.user.code = USEREVENT_PLAYDESCSOUND;
        playsound_event.user.data1 = (void *) cur_stamp[stamp_group];
        
        SDL_PushEvent(&playsound_event);
    }
}

#endif


/* Load a file's sound: */

#ifndef NOSOUND

static Mix_Chunk *loadsound_extra(const char *const fname, const char *extra)
{
    char *snd_fname;
    char tmp_str[MAX_PATH], ext[5];
    Mix_Chunk *tmp_snd;
    
    
    if (strcasestr(fname, ".png") != NULL)
    {
        strcpy(ext, ".png");
    }
    else
    {
        /* Sorry, we only handle images */
        
        return (NULL);
    }
    
    /* First, check for localized version of sound: */
    
    snd_fname = malloc(strlen(fname) + strlen(lang_prefix) + 16);
    
    strcpy(snd_fname, fname);
    snprintf(tmp_str, sizeof(tmp_str), "%s_%s.ogg", extra, lang_prefix);
    strcpy((char *) strcasestr(snd_fname, ext), tmp_str);
    debug(snd_fname);
    tmp_snd = Mix_LoadWAV(snd_fname);
    
    if (tmp_snd == NULL)
    {
        debug("...No local version of sound (OGG)!");
        
        strcpy(snd_fname, fname);
        snprintf(tmp_str, sizeof(tmp_str), "%s_%s.wav", extra, lang_prefix);
        strcpy((char *) strcasestr(snd_fname, ext), tmp_str);
        debug(snd_fname);
        tmp_snd = Mix_LoadWAV(snd_fname);
        
        if (tmp_snd == NULL)
        {
            debug("...No local version of sound (WAV)!");
            
            /* Check for non-country-code locale */
            
            strcpy(snd_fname, fname);
            snprintf(tmp_str, sizeof(tmp_str), "%s_%s.ogg", extra, short_lang_prefix);
            strcpy((char *) strcasestr(snd_fname, ext), tmp_str);
            debug(snd_fname);
            tmp_snd = Mix_LoadWAV(snd_fname);
            
            if (tmp_snd == NULL)
            {
                debug("...No short local version of sound (OGG)!");
                
                strcpy(snd_fname, fname);
                snprintf(tmp_str, sizeof(tmp_str), "%s_%s.wav", extra, short_lang_prefix);
                strcpy((char *) strcasestr(snd_fname, ext), tmp_str);
                debug(snd_fname);
                tmp_snd = Mix_LoadWAV(snd_fname);
                
                if (tmp_snd == NULL)
                {
                    /* Now, check for default sound: */
                    
                    debug("...No short local version of sound (WAV)!");
                    
                    strcpy(snd_fname, fname);
                    snprintf(tmp_str, sizeof(tmp_str), "%s.ogg", extra);
                    strcpy((char *) strcasestr(snd_fname, ext), tmp_str);
                    debug(snd_fname);
                    tmp_snd = Mix_LoadWAV(snd_fname);
                    
                    if (tmp_snd == NULL)
                    {
                        debug("...No default version of sound (OGG)!");
                        
                        strcpy(snd_fname, fname);
                        snprintf(tmp_str, sizeof(tmp_str), "%s.wav", extra);
                        strcpy((char *) strcasestr(snd_fname, ext), tmp_str);
                        debug(snd_fname);
                        tmp_snd = Mix_LoadWAV(snd_fname);
                        
                        if (tmp_snd == NULL)
                            debug("...No default version of sound (WAV)!");
                    }
                }
            }
        }
    }
    
    free(snd_fname);
    return (tmp_snd);
}


static Mix_Chunk *loadsound(const char *const fname)
{
    return (loadsound_extra(fname, ""));
}

static Mix_Chunk *loaddescsound(const char *const fname)
{
    return (loadsound_extra(fname, "_desc"));
}

#endif


/* Strip any trailing spaces: */

static void strip_trailing_whitespace(char *buf)
{
    unsigned i = strlen(buf);
    while (i--)
    {
        if (!isspace(buf[i]))
            break;
        buf[i] = '\0';
    }
}


/* Load a file's description: */

static char *loaddesc(const char *const fname, Uint8 * locale_text)
{
    char *txt_fname, *extptr;
    char buf[512], def_buf[512];  /* doubled to 512 per TOYAMA Shin-Ichi's requested; -bjk 2007.05.10 */
    int found, got_first;
    FILE *fi;
    
    
    txt_fname = strdup(fname);
    *locale_text = 0;
    
    extptr = strcasestr(txt_fname, ".png");
    
#ifndef NOSVG
    if (extptr == NULL)
        extptr = strcasestr(txt_fname, ".svg");
#endif
    
    if (extptr != NULL)
    {
        strcpy((char *) extptr, ".txt");
        
        fi = fopen(txt_fname, "r");
        free(txt_fname);
        
        if (!fi)
            return NULL;
        
        
        got_first = 0;
        found = 0;
        
        strcpy(def_buf, "");
        
        do
        {
            fgets(buf, sizeof(buf), fi);
            
            if (!feof(fi))
            {
                strip_trailing_whitespace(buf);
                
                
                if (!got_first)
                {
                    /* First one is the default: */
                    
                    strcpy(def_buf, buf);
                    got_first = 1;
                }
                
                
                debug(buf);
                
                
                /* See if it's the one for this locale... */
                
                if ((char *) strcasestr(buf, lang_prefix) == buf)
                {
                    
                    debug(buf + strlen(lang_prefix));
                    if ((char *) strcasestr(buf + strlen(lang_prefix), ".utf8=") ==
                        buf + strlen(lang_prefix))
                    {
                        found = 1;
                        
                        debug("...FOUND!");
                    }
                }
            }
        }
        while (!feof(fi) && !found);
        
        fclose(fi);
        
        
        /* Return the string: */
        
        if (found)
        {
            *locale_text = 1;
            return (strdup(buf + (strlen(lang_prefix)) + 6));
        }
        else
        {
            /* No locale-specific translation; use the default (English) */
            
            return (strdup(def_buf));
        }
    }
    else
    {
        return NULL;
    }
}


/* Load a *.dat file */
static double loadinfo(const char *const fname, stamp_type * inf)
{
    char buf[256];
    FILE *fi;
    double ratio = 1.0;
    
    inf->colorable = 0;
    inf->tintable = 0;
    inf->mirrorable = 1;
    inf->flipable = 1;
    inf->tinter = TINTER_NORMAL;
    
    fi = fopen(fname, "r");
    if (!fi)
        return ratio;
    
    do
    {
        fgets(buf, sizeof buf, fi);
        
        if (!feof(fi))
        {
            strip_trailing_whitespace(buf);
            
            if (strcmp(buf, "colorable") == 0)
                inf->colorable = 1;
            else if (strcmp(buf, "tintable") == 0)
                inf->tintable = 1;
            else if (!memcmp(buf, "scale", 5) && (isspace(buf[5]) || buf[5] == '='))
            {
                double tmp, tmp2;
                char *cp = buf + 6;
                while (isspace(*cp) || *cp == '=')
                    cp++;
                if (strchr(cp, '%'))
                {
                    tmp = strtod(cp, NULL) / 100.0;
                    if (tmp > 0.0001 && tmp < 10000.0)
                        ratio = tmp;
                }
                else if (strchr(cp, '/'))
                {
                    tmp = strtod(cp, &cp);
                    while (*cp && !isdigit(*cp))
                        cp++;
                    tmp2 = strtod(cp, NULL);
                    if (tmp > 0.0001 && tmp < 10000.0 && tmp2 > 0.0001 && tmp2 < 10000.0
                        && tmp / tmp2 > 0.0001 && tmp / tmp2 < 10000.0)
                        ratio = tmp / tmp2;
                }
                else if (strchr(cp, ':'))
                {
                    tmp = strtod(cp, &cp);
                    while (*cp && !isdigit(*cp))
                        cp++;
                    tmp2 = strtod(cp, NULL);
                    if (tmp > 0.0001 && tmp < 10000.0 &&
                        tmp2 > 0.0001 && tmp2 < 10000.0 &&
                        tmp2 / tmp > 0.0001 && tmp2 / tmp < 10000.0)
                        ratio = tmp2 / tmp;
                }
                else
                {
                    tmp = strtod(cp, NULL);
                    if (tmp > 0.0001 && tmp < 10000.0)
                        ratio = 1.0 / tmp;
                }
            }
            else if (!memcmp(buf, "tinter", 6) &&
                     (isspace(buf[6]) || buf[6] == '='))
            {
                char *cp = buf + 7;
                while (isspace(*cp) || *cp == '=')
                    cp++;
                if (!strcmp(cp, "anyhue"))
                {
                    inf->tinter = TINTER_ANYHUE;
                }
                else if (!strcmp(cp, "narrow"))
                {
                    inf->tinter = TINTER_NARROW;
                }
                else if (!strcmp(cp, "normal"))
                {
                    inf->tinter = TINTER_NORMAL;
                }
                else if (!strcmp(cp, "vector"))
                {
                    inf->tinter = TINTER_VECTOR;
                }
                else
                {
                    debug(cp);
                }
                
                /* printf("tinter=%d\n", inf->tinter); */
            }
            else if (strcmp(buf, "nomirror") == 0)
                inf->mirrorable = 0;
            else if (strcmp(buf, "noflip") == 0)
                inf->flipable = 0;
        }
    }
    while (!feof(fi));
    
    fclose(fi);
    return ratio;
}


static int SDLCALL NondefectiveBlit(SDL_Surface * src, SDL_Rect * srcrect,
                                    SDL_Surface * dst, SDL_Rect * dstrect)
{
    int dstx = 0;
    int dsty = 0;
    int srcx = 0;
    int srcy = 0;
    int srcw = src->w;
    int srch = src->h;
    Uint32(*getpixel) (SDL_Surface *, int, int) =
    getpixels[src->format->BytesPerPixel];
    void (*putpixel) (SDL_Surface *, int, int, Uint32) =
    putpixels[dst->format->BytesPerPixel];
    
    
    if (srcrect)
    {
        srcx = srcrect->x;
        srcy = srcrect->y;
        srcw = srcrect->w;
        srch = srcrect->h;
    }
    if (dstrect)
    {
        dstx = dstrect->x;
        dsty = dstrect->y;
    }
    if (dsty < 0)
    {
        srcy += -dsty;
        srch -= -dsty;
        dsty = 0;
    }
    if (dstx < 0)
    {
        srcx += -dstx;
        srcw -= -dstx;
        dstx = 0;
    }
    if (dstx + srcw > dst->w - 1)
    {
        srcw -= (dstx + srcw) - (dst->w - 1);
    }
    if (dsty + srch > dst->h - 1)
    {
        srch -= (dsty + srch) - (dst->h - 1);
    }
    if (srcw < 1 || srch < 1)
        return -1;			/* no idea what to return if nothing done */
    while (srch--)
    {
        int i = srcw;
        while (i--)
        {
            putpixel(dst, i + dstx, srch + dsty,
                     getpixel(src, i + srcx, srch + srcy));
        }
    }
    
    return (0);
}


/* For the 3rd arg, pass either NondefectiveBlit or SDL_BlitSurface. */
static void autoscale_copy_smear_free(SDL_Surface * src, SDL_Surface * dst,
                                      int SDLCALL(*blit) (SDL_Surface * src,
                                                          SDL_Rect * srcrect,
                                                          SDL_Surface * dst,
                                                          SDL_Rect * dstrect))
{
    SDL_Surface *src1;
    SDL_Rect dest;
    /* What to do when in 640x480 mode, and loading an
     800x600 (or larger) image!? Scale it. Starters must
     be scaled too. Keep the pixels square though, filling
     in the gaps via a smear. */
    if (src->w != dst->w || src->h != dst->h)
    {
        if (src->w / (float) dst->w > src->h / (float) dst->h)
            src1 = thumbnail(src, dst->w, src->h * dst->w / src->w, 0);
        else
            src1 = thumbnail(src, src->w * dst->h / src->h, dst->h, 0);
        SDL_FreeSurface(src);
        src = src1;
    }
    
    dest.x = (dst->w - src->w) / 2;
    dest.y = (dst->h - src->h) / 2;
    blit(src, NULL, dst, &dest);
    
    if (src->w != dst->w)
    {
        /* we know that the heights match, and src is narrower */
        SDL_Rect sour;
        int i = (dst->w - src->w) / 2;
        sour.w = 1;
        sour.x = 0;
        sour.h = src->h;
        sour.y = 0;
        while (i-- > 0)
        {
            dest.x = i;
            blit(src, &sour, dst, &dest);
        }
        sour.x = src->w - 1;
        i = (dst->w - src->w) / 2 + src->w - 1;
        while (++i < dst->w)
        {
            dest.x = i;
            blit(src, &sour, dst, &dest);
        }
    }
    
    if (src->h != dst->h)
    {
        /* we know that the widths match, and src is shorter */
        SDL_Rect sour;
        int i = (dst->h - src->h) / 2;
        sour.w = src->w;
        sour.x = 0;
        sour.h = 1;
        sour.y = 0;
        while (i-- > 0)
        {
            dest.y = i;
            blit(src, &sour, dst, &dest);
        }
        sour.y = src->h - 1;
        i = (dst->h - src->h) / 2 + src->h - 1;
        while (++i < dst->h)
        {
            dest.y = i;
            blit(src, &sour, dst, &dest);
        }
    }
    
    SDL_FreeSurface(src);
}


static void load_starter_id(char *saved_id)
{
    char *rname;
    char fname[32];
    FILE *fi;
    char color_tag;
    int r, g, b;
    
    snprintf(fname, sizeof(fname), "saved/%s.dat", saved_id);
    rname = get_fname(fname, DIR_SAVE);
    
    starter_id[0] = '\0';
    
    fi = fopen(rname, "r");
    if (fi != NULL)
    {
        fgets(starter_id, sizeof(starter_id), fi);
        starter_id[strlen(starter_id) - 1] = '\0';
        
        fscanf(fi, "%d", &starter_mirrored);
        fscanf(fi, "%d", &starter_flipped);
        fscanf(fi, "%d", &starter_personal);
        
        do
        {
            color_tag = fgetc(fi);
        }
        while ((color_tag == '\n' || color_tag == '\r') && !feof(fi));
        
        if (!feof(fi) && color_tag == 'c')
        {
            fscanf(fi, "%d", &r);
            fscanf(fi, "%d", &g);
            fscanf(fi, "%d", &b);
            
            canvas_color_r = (Uint8) r;
            canvas_color_g = (Uint8) g;
            canvas_color_b = (Uint8) b;
        }
        else
        {
            canvas_color_r = 255;
            canvas_color_g = 255;
            canvas_color_b = 255;
        }
        
        fclose(fi);
    }
    else
    {
        canvas_color_r = 255;
        canvas_color_g = 255;
        canvas_color_b = 255;
    }
    
    free(rname);
}



static void load_starter(char *img_id)
{
#ifdef __IPHONEOS__
    extern const char* DATA_PREFIX;
#endif
    
    char *dirname;
    char fname[256];
    SDL_Surface *tmp_surf;
    
    /* Determine path to starter files: */
    char temp[1024];
    sprintf(temp, "%s/%s", DATA_PREFIX, "starters");
    
    if (starter_personal == 0)
        dirname = strdup(temp);
    else
        dirname = get_fname("starters", DIR_DATA);
    
    /* Clear them to NULL first: */
    img_starter = NULL;
    img_starter_bkgd = NULL;
    
    /* Load the core image: */
    snprintf(fname, sizeof(fname), "%s/%s.png", dirname, img_id);
    tmp_surf = IMG_Load(fname);
    
#ifndef NOSVG
    if (tmp_surf == NULL)
    {
        /* Try loading an SVG */
        
        snprintf(fname, sizeof(fname), "%s/%s.svg", dirname, img_id);
        tmp_surf = load_svg(fname);
    }
#endif
    
    if (tmp_surf != NULL)
    {
        img_starter = SDL_DisplayFormatAlpha(tmp_surf);
        SDL_FreeSurface(tmp_surf);
    }
    
    /* Try to load the a background image: */
    
    /* FIXME: Also support .jpg extension? -bjk 2007.03.22 */
    
    /* (JPEG first) */
    snprintf(fname, sizeof(fname), "%s/%s-back.jpeg", dirname, img_id);
    tmp_surf = IMG_Load(fname);
    
    /* (Failed? Try PNG next) */
    if (tmp_surf == NULL)
    {
        snprintf(fname, sizeof(fname), "%s/%s-back.png", dirname, img_id);
        tmp_surf = IMG_Load(fname);
    }
    
#ifndef NOSVG
    /* (Failed? Try SVG next) */
    if (tmp_surf == NULL)
    {
        snprintf(fname, sizeof(fname), "%s/%s-back.svg", dirname, img_id);
        tmp_surf = load_svg(fname);
    }
#endif
    
    if (tmp_surf != NULL)
    {
        img_starter_bkgd = SDL_DisplayFormat(tmp_surf);
        SDL_FreeSurface(tmp_surf);
    }
    
    
    /* If no background, let's try to remove all white
     (so we don't have to _REQUIRE_ users create Starters with
     transparency, if they're simple black-and-white outlines */
    
    if (img_starter != NULL && img_starter_bkgd == NULL)
    {
        int x, y;
        Uint32(*getpixel) (SDL_Surface *, int, int) =
        getpixels[img_starter->format->BytesPerPixel];
        void (*putpixel) (SDL_Surface *, int, int, Uint32) =
        putpixels[img_starter->format->BytesPerPixel];
        Uint32 p;
        Uint8 r, g, b, a;
        int any_transparency;
        
        any_transparency = 0;
        
        for (y = 0; y < img_starter->h && !any_transparency; y++)
        {
            for (x = 0; x < img_starter->w && !any_transparency; x++)
            {
                p = getpixel(img_starter, x, y);
                SDL_GetRGBA(p, img_starter->format, &r, &g, &b, &a);
                
                if (a < 255)
                    any_transparency = 1;
            }
        }
        
        if (!any_transparency)
        {
            /* No transparency found!  We MUST check for white pixels to save
             the day! */
            
            for (y = 0; y < img_starter->h; y++)
            {
                for (x = 0; x < img_starter->w; x++)
                {
                    p = getpixel(img_starter, x, y);
                    SDL_GetRGBA(p, img_starter->format, &r, &g, &b, &a);
                    
                    if (abs(r - g) < 16 && abs(r - b) < 16 && abs(b - g) < 16)
                    {
                        a = 255 - ((r + g + b) / 3);
                    }
                    
                    p = SDL_MapRGBA(img_starter->format, r, g, b, a);
                    putpixel(img_starter, x, y, p);
                }
            }
        }
    }
    
    
    /* Scale if needed... */
    
    if (img_starter != NULL &&
        (img_starter->w != canvas->w || img_starter->h != canvas->h))
    {
        tmp_surf = img_starter;
        
        img_starter = SDL_CreateRGBSurface(canvas->flags,
                                           canvas->w, canvas->h,
                                           tmp_surf->format->BitsPerPixel,
                                           tmp_surf->format->Rmask,
                                           tmp_surf->format->Gmask,
                                           tmp_surf->format->Bmask,
                                           tmp_surf->format->Amask);
        
        /* 3rd arg ignored for RGBA surfaces */
        SDL_SetAlpha(tmp_surf, SDL_RLEACCEL, SDL_ALPHA_OPAQUE);
        autoscale_copy_smear_free(tmp_surf, img_starter, NondefectiveBlit);
        SDL_SetAlpha(img_starter, SDL_RLEACCEL | SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
    }
    
    
    if (img_starter_bkgd != NULL &&
        (img_starter_bkgd->w != canvas->w || img_starter_bkgd->h != canvas->h))
    {
        tmp_surf = img_starter_bkgd;
        
        img_starter_bkgd = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                                canvas->w, canvas->h,
                                                canvas->format->BitsPerPixel,
                                                canvas->format->Rmask,
                                                canvas->format->Gmask,
                                                canvas->format->Bmask, 0);
        
        autoscale_copy_smear_free(tmp_surf, img_starter_bkgd, SDL_BlitSurface);
    }
    
    free(dirname);
}


/* Load current (if any) image: */

static void load_current(void)
{
    SDL_Surface *tmp;
    char *fname;
    char ftmp[1024];
    FILE *fi;
    
    /* Determine the current picture's ID: */
    
    fname = get_fname("current_id.txt", DIR_SAVE);
    
    fi = fopen(fname, "r");
    if (fi == NULL)
    {
        fprintf(stderr,
                "\nWarning: Couldn't determine the current image's ID\n"
                "%s\n"
                "The system error that occurred was:\n"
                "%s\n\n", fname, strerror(errno));
        file_id[0] = '\0';
        starter_id[0] = '\0';
    }
    else
    {
        fgets(file_id, sizeof(file_id), fi);
        if (strlen(file_id) > 0)
        {
            file_id[strlen(file_id) - 1] = '\0';
        }
        fclose(fi);
    }
    
    free(fname);
    
    
    /* Load that image: */
    
    if (file_id[0] != '\0')
    {
        snprintf(ftmp, sizeof(ftmp), "saved/%s%s", file_id, FNAME_EXTENSION);
        
        fname = get_fname(ftmp, DIR_SAVE);
        
        tmp = IMG_Load(fname);
        
        if (tmp == NULL)
        {
            fprintf(stderr,
                    "\nWarning: Couldn't load any current image.\n"
                    "%s\n"
                    "The Simple DirectMedia Layer error that occurred was:\n"
                    "%s\n\n", fname, SDL_GetError());
            
            file_id[0] = '\0';
            starter_id[0] = '\0';
        }
        else
        {
            autoscale_copy_smear_free(tmp, canvas, SDL_BlitSurface);
            load_starter_id(file_id);
            if (starter_id[0] != '\0')
            {
                load_starter(starter_id);
                
                if (starter_mirrored)
                    mirror_starter();
                
                if (starter_flipped)
                    flip_starter();
            }
        }
        
        free(fname);
    }
}


/* Make sure we have a 'path' directory */

static int make_directory(const char *path, const char *errmsg)
{
    char *fname;
    int res;
    
    fname = get_fname(path, DIR_SAVE);
    res = mkdir(fname, 0755);
    if (res != 0 && errno != EEXIST)
    {
        fprintf(stderr,
                "\nError: %s:\n"
                "%s\n"
                "The error that occurred was:\n"
                "%s\n\n", errmsg, fname, strerror(errno));
        free(fname);
        return 0;
    }
    free(fname);
    return 1;
}

/* Save the current image to disk: */

static void save_current(void)
{
    char *fname;
    FILE *fi;
    
    if (!make_directory("", "Can't create user data directory"))
    {
        draw_tux_text(TUX_OOPS, strerror(errno), 0);
    }
    
    fname = get_fname("current_id.txt", DIR_SAVE);
    
    fi = fopen(fname, "w");
    if (fi == NULL)
    {
        fprintf(stderr,
                "\nError: Can't keep track of current image.\n"
                "%s\n"
                "The error that occurred was:\n"
                "%s\n\n", fname, strerror(errno));
        
        draw_tux_text(TUX_OOPS, strerror(errno), 0);
    }
    else
    {
        fprintf(fi, "%s\n", file_id);
        fclose(fi);
    }
    
    free(fname);
}


/* Prompt the user with a yes/no question: */

static int do_prompt(const char *const text, const char *const btn_yes,
                     const char *const btn_no, int ox, int oy)
{
    return (do_prompt_image(text, btn_yes, btn_no, NULL, NULL, NULL, ox, oy));
}


static int do_prompt_snd(const char *const text, const char *const btn_yes,
                         const char *const btn_no, int snd, int ox, int oy)
{
    return (do_prompt_image_flash_snd
            (text, btn_yes, btn_no, NULL, NULL, NULL, 0, snd, ox, oy));
}

static int do_prompt_image(const char *const text, const char *const btn_yes,
                           const char *const btn_no, SDL_Surface * img1,
                           SDL_Surface * img2, SDL_Surface * img3,
                           int ox, int oy)
{
    return (do_prompt_image_snd
            (text, btn_yes, btn_no, img1, img2, img3, SND_NONE, ox, oy));
}

static int do_prompt_image_snd(const char *const text,
                               const char *const btn_yes,
                               const char *const btn_no, SDL_Surface * img1,
                               SDL_Surface * img2, SDL_Surface * img3,
                               int snd, int ox, int oy)
{
    return (do_prompt_image_flash_snd
            (text, btn_yes, btn_no, img1, img2, img3, 0, snd, ox, oy));
}

static int do_prompt_image_flash(const char *const text,
                                 const char *const btn_yes,
                                 const char *const btn_no, SDL_Surface * img1,
                                 SDL_Surface * img2, SDL_Surface * img3,
                                 int animate, int ox, int oy)
{
    return (do_prompt_image_flash_snd
            (text, btn_yes, btn_no, img1, img2, img3, animate, SND_NONE, ox, oy));
}

static int do_prompt_image_flash_snd(const char *const text,
                                     const char *const btn_yes,
                                     const char *const btn_no,
                                     SDL_Surface * img1, SDL_Surface * img2,
                                     SDL_Surface * img3, int animate, int snd,
                                     int ox, int oy)
{
    int oox, ooy, nx, ny;
    SDL_Event event;
    SDL_Rect dest;
    int done, ans, w, counter;
    SDL_Color black = { 0, 0, 0, 0 };
    SDLKey key;
    SDLKey key_y, key_n;
    char *keystr;
    SDL_Surface * backup;
#ifndef NO_PROMPT_SHADOWS
    int i;
    SDL_Surface *alpha_surf;
#endif
    int img1_w, img2_w, img3_w, max_img_w, img_x, img_y, offset;
    SDL_Surface *img1b;
    int free_img1b;
    int txt_left, txt_right, img_left, btn_left, txt_btn_left, txt_btn_right;
    
    hide_blinking_cursor();
    
    
    /* Admittedly stupid way of determining which keys can be used for
     positive and negative responses in dialogs (e.g., [Y] (for 'yes') in English) */
    keystr = textdir(gettext("Yes"));
    key_y = tolower(keystr[0]);
    free(keystr);
    
    keystr = textdir(gettext("No"));
    key_n = tolower(keystr[0]);
    free(keystr);
    
    
    do_setcursor(cursor_arrow);
    
    
    /* Move cursor automatically if in keymouse mode: */
    
    if (keymouse)
    {
        mouse_x = WINDOW_WIDTH / 2;
        mouse_y = WINDOW_HEIGHT / 2;
        SDL_WarpMouse(mouse_x, mouse_y);
    }
    
    
    /* Draw button box: */
    
    playsound(screen, 0, SND_PROMPT, 1, SNDPOS_CENTER, SNDDIST_NEAR);
    
    backup = SDL_CreateRGBSurface(screen->flags, screen->w, screen->h,
                                  screen->format->BitsPerPixel,
                                  screen->format->Rmask,
                                  screen->format->Gmask,
                                  screen->format->Bmask,
                                  screen->format->Amask);
    
    SDL_BlitSurface(screen, NULL, backup, NULL);
    
    for (w = 0; w <= 96; w = w + 2)
    {
        oox = ox - w;
        ooy = oy - w;
        
        nx = 160 + 96 - w + PROMPTOFFSETX;
        ny = 94 + 96 - w + PROMPTOFFSETY;
        
        dest.x = ((nx * w) + (oox * (96 - w))) / 96;
        dest.y = ((ny * w) + (ooy * (96 - w))) / 96;
        dest.w = (320 - 96 * 2) + w * 2;
        dest.h = w * 2;
        SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 224 - w, 224 - w, 244 -w));
        
        SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);
        
        if ((w % 8) == 0)
            SDL_Delay(10);
        
        if (w == 94)
            SDL_BlitSurface(backup, NULL, screen, NULL);
    }
    
    SDL_FreeSurface(backup);
    
    
    playsound(screen, 1, snd, 1, SNDPOS_LEFT, SNDDIST_NEAR);
    
#ifndef NO_PROMPT_SHADOWS
    alpha_surf = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
                                      (320 - 96 * 2) + (w - 4) * 2,
                                      (w - 4) * 2,
                                      screen->format->BitsPerPixel,
                                      screen->format->Rmask,
                                      screen->format->Gmask,
                                      screen->format->Bmask,
                                      screen->format->Amask);
    
    if (alpha_surf != NULL)
    {
        SDL_FillRect(alpha_surf, NULL, SDL_MapRGB(alpha_surf->format, 0, 0, 0));
        SDL_SetAlpha(alpha_surf, SDL_SRCALPHA, 64);
        
        for (i = 8; i > 0; i = i - 2)
        {
            dest.x = 160 + 96 - (w - 4) + i + PROMPTOFFSETX;
            dest.y = 94 + 96 - (w - 4) + i + PROMPTOFFSETY;
            dest.w = (320 - 96 * 2) + (w - 4) * 2;
            dest.h = (w - 4) * 2;
            
            SDL_BlitSurface(alpha_surf, NULL, screen, &dest);
        }
        
        SDL_FreeSurface(alpha_surf);
    }
#endif
    
    
    w = w - 6;
    
    dest.x = 160 + 96 - w + PROMPTOFFSETX;
    dest.y = 94 + 96 - w + PROMPTOFFSETY;
    dest.w = (320 - 96 * 2) + w * 2;
    dest.h = w * 2;
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
    
    
    /* Make sure image on the right isn't too tall!
     (Thumbnails in Open dialog are larger on larger displays, and can
     cause arrow and trashcan icons to be pushed out of the dialog window!) */
    
    free_img1b = 0;
    img1b = NULL;
    
    if (img1 != NULL)
    {
        if (img1->h > 64)
        {
            img1b = thumbnail(img1, 80, 64, 1);
            free_img1b = 1;
        }
        else
        {
            img1b = img1;
        }
    }
    
    
    /* If we're showing any images on the right, determine the widest width
     for them: */
    
    offset = img1_w = img2_w = img3_w = 0;
    
    if (img1b != NULL)
        img1_w = img1b->w;
    if (img2 != NULL)
        img2_w = img2->w;
    if (img3 != NULL)
        img3_w = img3->w;
    
    max_img_w = max(img1_w, max(img2_w, img3_w));
    
    if (max_img_w > 0)
        offset = max_img_w + 8;
    
    
    /* Draw the question: */
    
    if (need_right_to_left == 0)
    {
        txt_left = 166 + PROMPTOFFSETX;
        txt_right = 475 + PROMPTOFFSETX - offset;
        img_left = 475 + PROMPTOFFSETX - max_img_w - 4;
        btn_left = 166 + PROMPTOFFSETX;
        txt_btn_left = txt_left + img_yes->w + 4;
        txt_btn_right = txt_right;
    }
    else
    {
        txt_left = 166 + PROMPTOFFSETX + offset;
        txt_right = 457 + PROMPTOFFSETX;
        img_left = 166 + PROMPTOFFSETX + 4;
        btn_left = 475 + PROMPTOFFSETX - img_yes->w - 4;
        txt_btn_left = txt_left;
        txt_btn_right = btn_left;
    }
    
    wordwrap_text(text, black, txt_left, 100 + PROMPTOFFSETY, txt_right, 1);
    
    
    /* Draw the images (if any, and if not animated): */
    
    img_x = img_left;
    img_y = 100 + PROMPTOFFSETY + 4;
    
    if (img1b != NULL)
    {
        dest.x = img_left + (max_img_w - img1b->w) / 2;
        dest.y = img_y;
        
        SDL_BlitSurface(img1b, NULL, screen, &dest);
        
        if (!animate)
            img_y = img_y + img1b->h + 4;
    }
    
    if (!animate)
    {
        if (img2 != NULL)
        {
            dest.x = img_left + (max_img_w - img2->w) / 2;
            dest.y = img_y;
            
            SDL_BlitSurface(img2, NULL, screen, &dest);
            
            img_y = img_y + img2->h + 4;
        }
        
        if (img3 != NULL)
        {
            dest.x = img_left + (max_img_w - img3->w) / 2;
            dest.y = img_y;
            
            SDL_BlitSurface(img3, NULL, screen, &dest);
            
            img_y = img_y + img3->h + 4;	/* unnecessary */
        }
    }
    
    
    /* Draw yes button: */
    
    dest.x = btn_left;
    dest.y = 178 + PROMPTOFFSETY;
    SDL_BlitSurface(img_yes, NULL, screen, &dest);
    
    
    /* (Bound to UTF8 domain, so always ask for UTF8 rendering!) */
    
    wordwrap_text(btn_yes, black, txt_btn_left,
                  183 + PROMPTOFFSETY, txt_btn_right, 1);
    
    
    /* Draw no button: */
    
    if (strlen(btn_no) != 0)
    {
        dest.x = btn_left;
        dest.y = 230 + PROMPTOFFSETY;
        SDL_BlitSurface(img_no, NULL, screen, &dest);
        
        wordwrap_text(btn_no, black, txt_btn_left,
                      235 + PROMPTOFFSETY, txt_btn_right, 1);
    }
    
    
    /* Draw Tux, waiting... */
    
    draw_tux_text(TUX_BORED, "", 0);
    
    SDL_Flip(screen);
    
    done = 0;
    counter = 0;
    ans = 0;
    
    do
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                ans = 0;
                done = 1;
            }
            else if (event.type == SDL_ACTIVEEVENT)
            {
                handle_active(&event);
            }
            else if (event.type == SDL_KEYUP)
            {
                key = event.key.keysym.sym;
                
                handle_keymouse(key, SDL_KEYUP);
            }
            else if (event.type == SDL_KEYDOWN)
            {
                key = event.key.keysym.sym;
                
                handle_keymouse(key, SDL_KEYDOWN);
                
                
                /* FIXME: Should use SDLK_{c} instead of '{c}'?  How? */
                
                if (key == key_y || key == SDLK_RETURN)
                {
                    /* Y or ENTER - Yes! */
                    
                    ans = 1;
                    done = 1;
                }
                else if (key == key_n || key == SDLK_ESCAPE)
                {
                    /* N or ESCAPE - No! */
                    
                    if (strlen(btn_no) != 0)
                    {
                        ans = 0;
                        done = 1;
                    }
                    else
                    {
                        if (key == SDLK_ESCAPE)
                        {
                            /* ESCAPE also simply dismisses if there's no Yes/No
                             choice: */
                            
                            ans = 1;
                            done = 1;
                        }
                    }
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN &&
                     valid_click(event.button.button))
            {
                if (event.button.x >= btn_left &&
                    event.button.x < btn_left + img_yes->w)
                {
                    if (event.button.y >= 178 + PROMPTOFFSETY &&
                        event.button.y < 178 + PROMPTOFFSETY + img_yes->h)
                    {
                        ans = 1;
                        done = 1;
                    }
                    else if (strlen(btn_no) != 0 &&
                             event.button.y >= 230 + PROMPTOFFSETY &&
                             event.button.y < 230 + PROMPTOFFSETY + img_no->h)
                    {
                        ans = 0;
                        done = 1;
                    }
                }
            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                if (event.button.x >= btn_left &&
                    event.button.x < btn_left + img_yes->w &&
                    ((event.button.y >= 178 + PROMPTOFFSETY &&
                      event.button.y < 178 + img_yes->h + PROMPTOFFSETY) ||
                     (strlen(btn_no) != 0 &&
                      event.button.y >= 230 + PROMPTOFFSETY &&
                      event.button.y < 230 + img_yes->h + PROMPTOFFSETY)))
                {
                    do_setcursor(cursor_hand);
                }
                else
                {
                    do_setcursor(cursor_arrow);
                }
            }
        }
        
        SDL_Delay(100);
        
        if (animate)
        {
            counter++;
            
            if (counter == 5)
            {
                dest.x = img_left + (max_img_w - img2->w) / 2;
                dest.y = img_y;
                
                SDL_BlitSurface(img2, NULL, screen, &dest);
                SDL_Flip(screen);
            }
            else if (counter == 10)
            {
                if (img3 != NULL)
                {
                    dest.x = img_left + (max_img_w - img3->w) / 2;
                    dest.y = img_y;
                    
                    SDL_BlitSurface(img3, NULL, screen, &dest);
                    SDL_Flip(screen);
                }
                else
                    counter = 15;
            }
            
            if (counter == 15)
            {
                dest.x = img_left + (max_img_w - img1b->w) / 2;
                dest.y = img_y;
                
                SDL_BlitSurface(img1b, NULL, screen, &dest);
                SDL_Flip(screen);
                
                counter = 0;
            }
        }
    }
    while (!done);
    
    
    /* FIXME: Sound effect! */
    /* ... */
    
    
    /* Erase question prompt: */
    
    update_canvas(0, 0, canvas->w, canvas->h);
    
    if (free_img1b)
        SDL_FreeSurface(img1b);
    
    return ans;
}


/* Free memory and prepare to quit: */

static void cleanup(void)
{
    int i, j;
    
    for (j = 0; j < num_stamp_groups; j++)
    {
        for (i = 0; i < num_stamps[j]; i++)
        {
#ifndef NOSOUND
            if (stamp_data[j][i]->ssnd)
            {
                Mix_FreeChunk(stamp_data[j][i]->ssnd);
                stamp_data[j][i]->ssnd = NULL;
            }
            if (stamp_data[j][i]->sdesc)
            {
                Mix_FreeChunk(stamp_data[j][i]->sdesc);
                stamp_data[j][i]->sdesc = NULL;
            }
#endif
            if (stamp_data[j][i]->stxt)
            {
                free(stamp_data[j][i]->stxt);
                stamp_data[j][i]->stxt = NULL;
            }
            free_surface(&stamp_data[j][i]->thumbnail);
            
            free(stamp_data[j][i]->stampname);
            free(stamp_data[j][i]);
            stamp_data[j][i] = NULL;
        }
        free(stamp_data[j]);
    }
    free_surface(&active_stamp);
    
    free_surface_array(img_brushes, num_brushes);
    free(brushes_frames);
    free(brushes_directional);
    free(brushes_spacing);
    free_surface_array(img_tools, NUM_TOOLS);
    free_surface_array(img_tool_names, NUM_TOOLS);
    free_surface_array(img_title_names, NUM_TITLES);
    for (i = 0; i < num_magics; i++)
    {
        free_surface(&(magics[i].img_icon));
        free_surface(&(magics[i].img_name));
    }
    free_surface_array(img_shapes, NUM_SHAPES);
    free_surface_array(img_shape_names, NUM_SHAPES);
    free_surface_array(img_tux, NUM_TIP_TUX);
    
    free_surface(&img_openlabels_open);
    free_surface(&img_openlabels_slideshow);
    free_surface(&img_openlabels_erase);
    free_surface(&img_openlabels_back);
    free_surface(&img_openlabels_next);
    free_surface(&img_openlabels_play);
    
    free_surface(&img_progress);
    
    free_surface(&img_yes);
    free_surface(&img_no);
    
    free_surface(&img_prev);
    free_surface(&img_next);
    
    free_surface(&img_mirror);
    free_surface(&img_flip);
    
    free_surface(&img_title_on);
    free_surface(&img_title_off);
    free_surface(&img_title_large_on);
    free_surface(&img_title_large_off);
    
    free_surface(&img_open);
    free_surface(&img_erase);
    free_surface(&img_back);
    free_surface(&img_trash);
    
    free_surface(&img_slideshow);
    free_surface(&img_play);
    free_surface(&img_select_digits);
    
    free_surface(&img_dead40x40);
    
    free_surface(&img_printer);
    free_surface(&img_printer_wait);
    free_surface(&img_save_over);
    
    free_surface(&img_btn_up);
    free_surface(&img_btn_down);
    free_surface(&img_btn_off);
    
    free_surface(&img_btnsm_up);
    free_surface(&img_btnsm_off);
    
    free_surface(&img_sfx);
    free_surface(&img_speak);
    
    free_surface(&img_cursor_up);
    free_surface(&img_cursor_down);
    
    free_surface(&img_cursor_starter_up);
    free_surface(&img_cursor_starter_down);
    
    free_surface(&img_scroll_up);
    free_surface(&img_scroll_down);
    free_surface(&img_scroll_up_off);
    free_surface(&img_scroll_down_off);
    
    free_surface(&img_grow);
    free_surface(&img_shrink);
    
    free_surface(&img_magic_paint);
    free_surface(&img_magic_fullscreen);
    
    free_surface(&img_bold);
    free_surface(&img_italic);
    
    free_surface_array(undo_bufs, NUM_UNDO_BUFS);
    
#ifdef LOW_QUALITY_COLOR_SELECTOR
    free_surface(&img_paintcan);
#else
    free_surface_array(img_color_btns, NUM_COLORS * 2);
    free(img_color_btns);
#endif
    
    free_surface(&screen);
    free_surface(&img_starter);
    free_surface(&img_starter_bkgd);
    free_surface(&canvas);
    free_surface(&img_cur_brush);
    
    if (touched != NULL)
    {
        free(touched);
        touched = NULL;
    }
    
    if (medium_font != NULL)
    {
        TuxPaint_Font_CloseFont(medium_font);
        medium_font = NULL;
    }
    
    if (small_font != NULL)
    {
        TuxPaint_Font_CloseFont(small_font);
        small_font = NULL;
    }
    
    if (large_font != NULL)
    {
        TuxPaint_Font_CloseFont(large_font);
        large_font = NULL;
    }
    
#ifdef FORKED_FONTS
    free(user_font_families);	/* we'll leak the bodies... oh well */
#else
    for (i = 0; i < num_font_families; i++)
    {
        if (user_font_families[i])
        {
            char **cpp = user_font_families[i]->filename - 1;
            if (*++cpp)
                free(*cpp);
            if (*++cpp)
                free(*cpp);
            if (*++cpp)
                free(*cpp);
            if (*++cpp)
                free(*cpp);
            if (user_font_families[i]->handle)
                TuxPaint_Font_CloseFont(user_font_families[i]->handle);
            free(user_font_families[i]->directory);
            free(user_font_families[i]->family);
            free(user_font_families[i]);
            user_font_families[i] = NULL;
        }
    }
#endif
    
#ifndef NOSOUND
    if (use_sound)
    {
        for (i = 0; i < NUM_SOUNDS; i++)
        {
            if (sounds[i])
            {
                Mix_FreeChunk(sounds[i]);
                sounds[i] = NULL;
            }
        }
        
        Mix_CloseAudio();
    }
#endif
    
    for (i = 0; i < num_plugin_files; i++)
        magic_funcs[i].shutdown(magic_api_struct);
    
    free_cursor(&cursor_hand);
    free_cursor(&cursor_arrow);
    free_cursor(&cursor_watch);
    free_cursor(&cursor_up);
    free_cursor(&cursor_down);
    free_cursor(&cursor_tiny);
    free_cursor(&cursor_crosshair);
    free_cursor(&cursor_brush);
    free_cursor(&cursor_wand);
    free_cursor(&cursor_insertion);
    free_cursor(&cursor_rotate);
    
    for (i = 0; i < NUM_COLORS; i++)
    {
        free(color_hexes[i]);
        free(color_names[i]);
    }
    free(color_hexes);
    free(color_names);
    
    
    /* (Just in case...) */
    
    SDL_WM_GrabInput(SDL_GRAB_OFF);
    
    
    /* If we're using a lockfile, we can 'clear' it when we quit
     (so Tux Paint can be launched again soon, if the user wants to!) */
    
    if (ok_to_use_lockfile)
    {
        char *lock_fname;
        time_t zero_time;
        FILE *fi;
        
#ifndef WIN32
        lock_fname = get_fname("lockfile.dat", DIR_SAVE);
#else
        lock_fname = get_temp_fname("lockfile.dat");
#endif
        
        zero_time = (time_t) 0;
        
        fi = fopen(lock_fname, "w");
        if (fi != NULL)
        {
            /* If we can write to it, do so! */
            
            fwrite(&zero_time, sizeof(time_t), 1, fi);
            fclose(fi);
        }
        else
        {
            fprintf(stderr,
                    "\nWarning: I couldn't create the lockfile (%s)\n"
                    "The error that occurred was:\n"
                    "%s\n\n", lock_fname, strerror(errno));
        }
        
        free(lock_fname);
    }
    
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
    if (papersize != NULL)
        free(papersize);
#endif
    
    
    /* Close up! */
    
    /* FIXME: Pango contexts lying around? -bjk 2007.07.24 */
    
    TTF_Quit();
    SDL_Quit();
    
    
}


static void free_surface(SDL_Surface ** surface_array)
{
    if (*surface_array)
    {
        SDL_FreeSurface(*surface_array);
        *surface_array = NULL;
    }
}


static void free_surface_array(SDL_Surface * surface_array[], int count)
{
    int i;
    
    for (i = 0; i < count; ++i)
    {
        free_surface(&surface_array[i]);
    }
}


/* Update screen where shape is/was: */

/* FIXME: unused */
/*
 static void update_shape(int cx, int ox1, int ox2, int cy, int oy1, int oy2, int fix)
 {
 int rx, ry;
 
 rx = abs(ox1 - cx);
 if (abs(ox2 - cx) > rx)
 rx = abs(ox2 - cx);
 
 ry = abs(oy1 - cy);
 if (abs(oy2 - cy) > ry)
 ry = abs(oy2 - cy);
 
 if (fix)
 {
 if (ry > rx)
 rx = ry;
 else
 ry = rx;
 }
 
 SDL_UpdateRect(screen, max((cx - rx), 0) + 96, max(cy - ry, 0),
 min((cx + rx) + 96, screen->w),
 min(cy + ry, screen->h));
 }
 */


/* Draw a shape! */

static void do_shape(int cx, int cy, int ox, int oy, int rotn, int use_brush)
{
    int side, angle_skip, init_ang, rx, ry, rmax, x1, y1, x2, y2, xp, yp,
    old_brush, step;
    float a1, a2, rotn_rad;
    int xx;
    
    
    /* Determine radius/shape of the shape to draw: */
    
    old_brush = 0;
    
#ifdef CORNER_SHAPES
    int tmp = 0;
    
    if (cx > ox)
    {
        tmp = cx;
        cx = ox;
        ox = tmp;
    }
    
    if (cy > oy)
    {
        tmp = cy;
        cy = oy;
        oy = tmp;
    }
    
    x1 = cx;
    x2 = ox;
    y1 = cy;
    y2 = oy;
    
    cx += ((x2 - x1) / 2);
    cy += ((y2 - y1) / 2);
#endif
    
    rx = ox - cx;
    ry = oy - cy;
    
    /* If the shape has a 1:1 ("locked") aspect ratio, use the larger radius: */
    
    if (shape_locked[cur_shape])
    {
        if (rx > ry)
            ry = rx;
        else
            rx = ry;
    }
    
    
    /* Is the shape tiny?  Make it SOME size, first! */
    
    if (rx < 15 && ry < 15)
    {
        rx = 15;
        ry = 15;
    }
    
    
    /* Render a default brush: */
    
    if (use_brush)
    {
        old_brush = cur_brush;
        cur_brush = shape_brush;	/* Now only semi-ludgy! */
        render_brush();
    }
    
    
    /* Draw the shape: */
    
    angle_skip = 360 / shape_sides[cur_shape];
    
    init_ang = shape_init_ang[cur_shape];
    
    
    step = 1;
    
    if (dont_do_xor && !use_brush)
    {
        /* If we're in light outline mode & not drawing the shape with the brush,
         if it has lots of sides (like a circle), reduce the number of sides: */
        
        if (shape_sides[cur_shape] > 5)
            step = (shape_sides[cur_shape] / 8);
    }
    
    
    for (side = 0; side < shape_sides[cur_shape]; side = side + step)
    {
        a1 = (angle_skip * side + init_ang) * M_PI / 180;
        a2 = (angle_skip * (side + 1) + init_ang) * M_PI / 180;
        
        x1 = (int) (cos(a1) * rx);
        y1 = (int) (-sin(a1) * ry);
        
        x2 = (int) (cos(a2) * rx);
        y2 = (int) (-sin(a2) * ry);
        
        
        /* Rotate the line: */
        
        if (rotn != 0)
        {
            rotn_rad = rotn * M_PI / 180;
            
            xp = x1 * cos(rotn_rad) - y1 * sin(rotn_rad);
            yp = x1 * sin(rotn_rad) + y1 * cos(rotn_rad);
            
            x1 = xp;
            y1 = yp;
            
            xp = x2 * cos(rotn_rad) - y2 * sin(rotn_rad);
            yp = x2 * sin(rotn_rad) + y2 * cos(rotn_rad);
            
            x2 = xp;
            y2 = yp;
        }
        
        
        /* Center the line around the center of the shape: */
        
        x1 = x1 + cx;
        y1 = y1 + cy;
        x2 = x2 + cx;
        y2 = y2 + cy;
        
        
        /* Draw: */
        
        if (!use_brush)
        {
            /* (XOR) */
            
            line_xor(x1, y1, x2, y2);
        }
        else
        {
            /* Brush */
            
            brush_draw(x1, y1, x2, y2, 0);
        }
    }
    
    
    if (use_brush && shape_filled[cur_shape])
    {
        /* FIXME: In the meantime, we'll do this lame radius-based fill: */
        
        for (xx = abs(rx); xx >= 0; xx--)
        {
            for (side = 0; side < shape_sides[cur_shape]; side++)
            {
                a1 = (angle_skip * side + init_ang) * M_PI / 180;
                a2 = (angle_skip * (side + 1) + init_ang) * M_PI / 180;
                
                x1 = (int) (cos(a1) * xx);
                y1 = (int) (-sin(a1) * ry);
                
                x2 = (int) (cos(a2) * xx);
                y2 = (int) (-sin(a2) * ry);
                
                
                /* Rotate the line: */
                
                if (rotn != 0)
                {
                    rotn_rad = rotn * M_PI / 180;
                    
                    xp = x1 * cos(rotn_rad) - y1 * sin(rotn_rad);
                    yp = x1 * sin(rotn_rad) + y1 * cos(rotn_rad);
                    
                    x1 = xp;
                    y1 = yp;
                    
                    xp = x2 * cos(rotn_rad) - y2 * sin(rotn_rad);
                    yp = x2 * sin(rotn_rad) + y2 * cos(rotn_rad);
                    
                    x2 = xp;
                    y2 = yp;
                }
                
                
                /* Center the line around the center of the shape: */
                
                x1 = x1 + cx;
                y1 = y1 + cy;
                x2 = x2 + cx;
                y2 = y2 + cy;
                
                
                /* Draw: */
                
                brush_draw(x1, y1, x2, y2, 0);
            }
            
            if (xx % 10 == 0)
                update_canvas(0, 0, WINDOW_WIDTH - 96, (48 * 7) + 40 + HEIGHTOFFSET);
        }
    }
    
    
    /* Update it! */
    
    if (use_brush)
    {
        if (abs(rx) > abs(ry))
            rmax = abs(rx) + 20;
        else
            rmax = abs(ry) + 20;
        
        update_canvas(cx - rmax, cy - rmax, cx + rmax, cy + rmax);
    }
    
    
    /* Return to normal brush (for paint brush and line tools): */
    
    if (use_brush)
    {
        cur_brush = old_brush;
        render_brush();
    }
}


/* What angle is the mouse away from the center of a shape? */

static int rotation(int ctr_x, int ctr_y, int ox, int oy)
{
    return (atan2(oy - ctr_y, ox - ctr_x) * 180 / M_PI);
}


/* Prompt to ask whether user wishes to save over old version of their file */
#define PROMPT_SAVE_OVER_TXT gettext_noop("Replace the picture with your changes?")

/* Positive response to saving over old version
 (like a 'File:Save' action in other applications) */
#define PROMPT_SAVE_OVER_YES gettext_noop("Yes, replace the old one!")

/* Negative response to saving over old version (saves a new image)
 (like a 'File:Save As...' action in other applications) */
#define PROMPT_SAVE_OVER_NO  gettext_noop("No, save a new file!")


/* Save the current image: */

static int do_save(int tool, int dont_show_success_results)
{
    char *fname;
    char tmp[1024];
    SDL_Surface *thm;
    FILE *fi;
    
    
    /* Was saving completely disabled? */
    
    if (disable_save)
        return 0;
    
    
    if (promptless_save == SAVE_OVER_NO)
    {
        /* Never save over - _always_ save a new file! */
        
        get_new_file_id();
    }
    else if (promptless_save == SAVE_OVER_PROMPT)
    {
        /* Saving the same picture? */
        
        if (file_id[0] != '\0')
        {
            /* We sure we want to do that? */
            
            if (do_prompt_image_snd(PROMPT_SAVE_OVER_TXT,
                                    PROMPT_SAVE_OVER_YES,
                                    PROMPT_SAVE_OVER_NO,
                                    img_save_over, NULL, NULL, SND_AREYOUSURE,
                                    (TOOL_SAVE % 2) * 48 + 24,
                                    (TOOL_SAVE / 2) * 48 + 40 + 24) == 0)
            {
                /* No - Let's save a new picture! */
                
                get_new_file_id();
            }
            if (tool == TOOL_TEXT)
                do_render_cur_text(0);
        }
        else
        {
            /* Saving a new picture: */
            
            get_new_file_id();
        }
    }
    else if (promptless_save == SAVE_OVER_ALWAYS)
    {
        if (file_id[0] == '\0')
            get_new_file_id();
    }
    
    
    /* Make sure we have a ~/.tuxpaint directory: */
    
    show_progress_bar(screen);
    do_setcursor(cursor_watch);
    
    if (!make_directory("", "Can't create user data directory"))
    {
        fprintf(stderr, "Cannot save the any pictures! SORRY!\n\n");
        draw_tux_text(TUX_OOPS, SDL_GetError(), 0);
        return 0;
    }
    
    show_progress_bar(screen);
    
    
    /* Make sure we have a ~/.tuxpaint/saved directory: */
    
    if (!make_directory("saved", "Can't create user data directory"))
    {
        fprintf(stderr, "Cannot save any pictures! SORRY!\n\n");
        draw_tux_text(TUX_OOPS, SDL_GetError(), 0);
        return 0;
    }
    
    show_progress_bar(screen);
    
    
    /* Make sure we have a ~/.tuxpaint/saved/.thumbs/ directory: */
    
    if (!make_directory("saved/.thumbs", "Can't create user data thumbnail directory"))
    {
        fprintf(stderr, "Cannot save any pictures! SORRY!\n\n");
        draw_tux_text(TUX_OOPS, SDL_GetError(), 0);
        return 0;
    }
    
    show_progress_bar(screen);
    
    
    /* Save the file: */
    
    snprintf(tmp, sizeof(tmp), "saved/%s%s", file_id, FNAME_EXTENSION);
    fname = get_fname(tmp, DIR_SAVE);
    debug(fname);
    
    fi = fopen(fname, "wb");
    if (fi == NULL)
    {
        fprintf(stderr,
                "\nError: Couldn't save the current image!\n"
                "%s\n"
                "The system error that occurred was:\n"
                "%s\n\n", fname, strerror(errno));
        
        draw_tux_text(TUX_OOPS, strerror(errno), 0);
    }
    else
    {
        if (!do_png_save(fi, fname, canvas))
        {
            free(fname);
            return 0;
        }
    }
    
    free(fname);
    
    show_progress_bar(screen);
    
    
    /* Save thumbnail, too: */
    
    /* (Was thumbnail in old directory, rather than under .thumbs?) */
    
    snprintf(tmp, sizeof(tmp), "saved/%s-t%s", file_id, FNAME_EXTENSION);
    fname = get_fname(tmp, DIR_SAVE);
    fi = fopen(fname, "r");
    if (fi != NULL)
    {
        fclose(fi);
    }
    else
    {
        /* No old thumbnail!  Save this image's thumbnail in the new place,
         under ".thumbs" */
        
        snprintf(tmp, sizeof(tmp), "saved/.thumbs/%s-t%s", file_id,
                 FNAME_EXTENSION);
        fname = get_fname(tmp, DIR_SAVE);
    }
    
    debug(fname);
    
    thm = thumbnail(canvas, THUMB_W - 20, THUMB_H - 20, 0);
    
    fi = fopen(fname, "wb");
    if (fi == NULL)
    {
        fprintf(stderr, "\nError: Couldn't save thumbnail of image!\n"
                "%s\n"
                "The system error that occurred was:\n"
                "%s\n\n", fname, strerror(errno));
    }
    else
    {
        do_png_save(fi, fname, thm);
    }
    SDL_FreeSurface(thm);
    
    free(fname);
    
    
    /* Write 'starter' and/or canvas color info, if it's useful to: */
    
    if (starter_id[0] != '\0' ||
        canvas_color_r != 255 ||
        canvas_color_g != 255 ||
        canvas_color_b != 255)
    {
        snprintf(tmp, sizeof(tmp), "saved/%s.dat", file_id);
        fname = get_fname(tmp, DIR_SAVE);
        fi = fopen(fname, "w");
        if (fi != NULL)
        {
            fprintf(fi, "%s\n", starter_id);
            fprintf(fi, "%d %d %d\n",
                    starter_mirrored, starter_flipped, starter_personal);
            fprintf(fi, "c%d %d %d\n",
                    canvas_color_r,
                    canvas_color_g,
                    canvas_color_b);
            fclose(fi);
        }
        
        free(fname);
    }
    
    
    /* All happy! */
    
    playsound(screen, 0, SND_SAVE, 1, SNDPOS_CENTER, SNDDIST_NEAR);
    
    if (!dont_show_success_results)
    {
        draw_tux_text(TUX_DEFAULT, tool_tips[TOOL_SAVE], 1);
        do_setcursor(cursor_arrow);
    }
    
    return 1;
}


/* Actually save the PNG data to the file stream: */
static int do_png_save(FILE * fi, const char *const fname, SDL_Surface * surf)
{
    SDL_SaveBMP(surf, fname);
    fclose(fi);
    return 1;
//    png_structp png_ptr;
//    png_infop info_ptr;
//    png_text text_ptr[4];
//    unsigned char **png_rows;
//    Uint8 r, g, b;
//    int x, y, count;
//    Uint32(*getpixel) (SDL_Surface *, int, int) =
//    getpixels[surf->format->BytesPerPixel];
//    
//    
//    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
//    if (png_ptr == NULL)
//    {
//        fclose(fi);
//        png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
//        
//        fprintf(stderr, "\nError: Couldn't save the image!\n%s\n\n", fname);
//        draw_tux_text(TUX_OOPS, strerror(errno), 0);
//    }
//    else
//    {
//        info_ptr = png_create_info_struct(png_ptr);
//        if (info_ptr == NULL)
//        {
//            fclose(fi);
//            png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
//            
//            fprintf(stderr, "\nError: Couldn't save the image!\n%s\n\n", fname);
//            draw_tux_text(TUX_OOPS, strerror(errno), 0);
//        }
//        else
//        {
//            if (setjmp(png_jmpbuf(png_ptr)))
//            {
//                fclose(fi);
//                png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
//                
//                fprintf(stderr, "\nError: Couldn't save the image!\n%s\n\n", fname);
//                draw_tux_text(TUX_OOPS, strerror(errno), 0);
//                
//                return 0;
//            }
//            else
//            {
//                png_init_io(png_ptr, fi);
//                
////                info_ptr->width = surf->w;
////                info_ptr->height = surf->h;
////                info_ptr->bit_depth = 8;
////                info_ptr->color_type = PNG_COLOR_TYPE_RGB;
////                info_ptr->interlace_type = 1;
////                info_ptr->valid = 0;	/* will be updated by various png_set_FOO() functions */
//                
////                png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr,
////                                           PNG_sRGB_INTENT_PERCEPTUAL);
//                
//                png_set_IHDR(png_ptr, info_ptr, surf->w, surf->h,
//                             8, PNG_COLOR_TYPE_RGB, 1,
//                            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
//                
//                /* Set headers */
//                
//                count = 0;
//                
//                /*
//                 if (title != NULL && strlen(title) > 0)
//                 {
//                 text_ptr[count].key = "Title";
//                 text_ptr[count].text = title;
//                 text_ptr[count].compression = PNG_TEXT_COMPRESSION_NONE;
//                 count++;
//                 }
//                 */
//                
//                text_ptr[count].key = (png_charp) "Software";
//                text_ptr[count].text =
//                (png_charp) "Tux Paint " VER_VERSION " (" VER_DATE ")";
//                text_ptr[count].compression = PNG_TEXT_COMPRESSION_NONE;
//                count++;
//                
//                
//                png_set_text(png_ptr, info_ptr, text_ptr, count);
//                
//                png_write_info(png_ptr, info_ptr);
//                
//                
//                
//                /* Save the picture: */
//                
//                png_rows = malloc(sizeof(char *) * surf->h);
//                
//                for (y = 0; y < surf->h; y++)
//                {
//                    png_rows[y] = malloc(sizeof(char) * 3 * surf->w);
//                    
//                    for (x = 0; x < surf->w; x++)
//                    {
//                        SDL_GetRGB(getpixel(surf, x, y), surf->format, &r, &g, &b);
//                        
//                        png_rows[y][x * 3 + 0] = r;
//                        png_rows[y][x * 3 + 1] = g;
//                        png_rows[y][x * 3 + 2] = b;
//                    }
//                }
//                
//                png_write_image(png_ptr, png_rows);
//                
//                for (y = 0; y < surf->h; y++)
//                    free(png_rows[y]);
//                
//                free(png_rows);
//                
//                
//                png_write_end(png_ptr, NULL);
//                
//                png_destroy_write_struct(&png_ptr, &info_ptr);
//                fclose(fi);
//                
//                return 1;
//            }
//        }
//    }
//    
//    return 0;
}

/* Pick a new file ID: */
static void get_new_file_id(void)
{
    time_t t;
    
    t = time(NULL);
    
    strftime(file_id, sizeof(file_id), "%Y%m%d%H%M%S", localtime(&t));
    debug(file_id);
    
    
    /* FIXME: Show thumbnail and prompt for title: */
}


/* Handle quitting (and prompting to save, if necessary!) */

//static int do_quit(int tool)
//{
//    
//    return 0;
//}



/* Open a saved image: */

#define PLACE_COLOR_PALETTE (-1)
#define PLACE_SAVED_DIR 0
#define PLACE_PERSONAL_STARTERS_DIR 1
#define PLACE_STARTERS_DIR 2
#define NUM_PLACES_TO_LOOK 3


/* FIXME: This, do_slideshow() and do_new_dialog() should be combined
 and modularized! */
int do_open(void)
{
    SDL_Surface *img, *img1, *img2;
    int things_alloced;
    SDL_Surface **thumbs = NULL;
    DIR *d;
    struct dirent *f;
    struct dirent2 *fs;
    int place;
    char *dirname[NUM_PLACES_TO_LOOK];
    char *rfname;
    char **d_names = NULL, **d_exts = NULL;
    int *d_places;
    FILE *fi;
    char fname[1024];
    int num_files, i, done, slideshow, update_list, want_erase, cur, which,
    num_files_in_dirs, j, any_saved_files;
    SDL_Rect dest;
    SDL_Event event;
    SDLKey key;
    Uint32 last_click_time;
    int last_click_which, last_click_button;
    int places_to_look;
    int opened_something;
    
    opened_something = 0;
    
    do
    {
        do_setcursor(cursor_watch);
        
        /* Allocate some space: */
        
        things_alloced = 32;
        
        fs = (struct dirent2 *) malloc(sizeof(struct dirent2) * things_alloced);
        
        num_files = 0;
        cur = 0;
        which = 0;
        slideshow = 0;
        num_files_in_dirs = 0;
        any_saved_files = 0;
        
        
        /* Open directories of images: */
        
        for (places_to_look = 0;
             places_to_look < NUM_PLACES_TO_LOOK; places_to_look++)
        {
            if (places_to_look == PLACE_SAVED_DIR)
            {
                /* First, check for saved-images: */
                
                dirname[places_to_look] = get_fname("saved", DIR_SAVE);
            }
            else if (places_to_look == PLACE_PERSONAL_STARTERS_DIR)
            {
                dirname[places_to_look] = NULL;
                continue;
            }
            else if (places_to_look == PLACE_STARTERS_DIR)
            {
                /* Finally, check for system-wide coloring-book style
                 'starter' images: */
                
                dirname[places_to_look] = NULL;
                continue;
            }
            
            
            /* Read directory of images and build thumbnails: */
            
            d = opendir(dirname[places_to_look]);
            
            if (d != NULL)
            {
                /* Gather list of files (for sorting): */
                
                do
                {
                    f = readdir(d);
                    
                    if (f != NULL)
                    {
                        memcpy(&(fs[num_files_in_dirs].f), f, sizeof(struct dirent));
                        fs[num_files_in_dirs].place = places_to_look;
                        
                        num_files_in_dirs++;
                        
                        if (places_to_look == PLACE_SAVED_DIR)
                            any_saved_files = 1;
                        
                        if (num_files_in_dirs >= things_alloced)
                        {
                            things_alloced = things_alloced + 32;
                            
                            /* FIXME: Valgrind says this is leaked -bjk 2007.07.19 */
                            fs = (struct dirent2 *) realloc(fs,
                                                            sizeof(struct dirent2) *
                                                            things_alloced);
                        }
                    }
                }
                while (f != NULL);
                
                closedir(d);
            }
        }
        
        
        /* (Re)allocate space for the information about these files: */
        
        thumbs = (SDL_Surface * *)malloc(sizeof(SDL_Surface *) * num_files_in_dirs);
        d_places = (int *) malloc(sizeof(int) * num_files_in_dirs);
        d_names = (char **) malloc(sizeof(char *) * num_files_in_dirs);
        d_exts = (char **) malloc(sizeof(char *) * num_files_in_dirs);
        
        
        /* Sort: */
        
        qsort(fs, num_files_in_dirs, sizeof(struct dirent2),
              (int (*)(const void *, const void *)) compare_dirent2s);
        
        
        /* Read directory of images and build thumbnails: */
        
        for (j = 0; j < num_files_in_dirs; j++)
        {
            f = &(fs[j].f);
            place = fs[j].place;
            
            show_progress_bar(screen);
            
            if (f != NULL)
            {
                debug(f->d_name);
                
                if (strcasestr(f->d_name, "-t.") == NULL &&
                    strcasestr(f->d_name, "-back.") == NULL)
                {
                    if (strcasestr(f->d_name, FNAME_EXTENSION) != NULL
                        /* Support legacy BMP files for load: */
                        || strcasestr(f->d_name, ".bmp") != NULL
                        )
                    {
                        strcpy(fname, f->d_name);
                        if (strcasestr(fname, FNAME_EXTENSION) != NULL)
                        {
                            strcpy((char *) strcasestr(fname, FNAME_EXTENSION), "");
                            d_exts[num_files] = strdup(FNAME_EXTENSION);
                        }
                        
                        if (strcasestr(fname, ".bmp") != NULL)
                        {
                            strcpy((char *) strcasestr(fname, ".bmp"), "");
                            d_exts[num_files] = strdup(".bmp");
                        }
                        
                        d_names[num_files] = strdup(fname);
                        d_places[num_files] = place;
                        
                        
                        /* Is it the 'current' file we just loaded?
                         We'll make it the current selection! */
                        
                        if (strcmp(d_names[num_files], file_id) == 0)
                        {
                            which = num_files;
                            cur = (which / 4) * 4;
                            
                            /* Center the cursor (useful for when the last item is
                             selected first!) */
                            
                            if (cur - 8 >= 0)
                                cur = cur - 8;
                            else if (cur - 4 >= 0)
                                cur = cur - 4;
                        }
                        
                        
                        /* Try to load thumbnail first: */
                        
                        snprintf(fname, sizeof(fname), "%s/.thumbs/%s-t.bmp",
                                 dirname[d_places[num_files]], d_names[num_files]);
                        debug(fname);
                        img = IMG_Load(fname);
                        
                        if (img == NULL)
                        {
                            /* No thumbnail in the new location ("saved/.thumbs"),
                             try the old locatin ("saved/"): */
                            
                            snprintf(fname, sizeof(fname), "%s/%s-t.bmp",
                                     dirname[d_places[num_files]], d_names[num_files]);
                            debug(fname);
                            
                            img = IMG_Load(fname);
                        }
                        
                        if (img != NULL)
                        {
                            /* Loaded the thumbnail from one or the other location */
                            show_progress_bar(screen);
                            
                            img1 = SDL_DisplayFormat(img);
                            SDL_FreeSurface(img);
                            
                            /* if too big, or too small in both dimensions, rescale it
                             (for now: using old thumbnail as source for high speed, low quality) */
                            if (img1->w > THUMB_W - 20 || img1->h > THUMB_H - 20
                                || (img1->w < THUMB_W - 20 && img1->h < THUMB_H - 20))
                            {
                                img2 = thumbnail(img1, THUMB_W - 20, THUMB_H - 20, 0);
                                SDL_FreeSurface(img1);
                                img1 = img2;
                            }
                            
                            thumbs[num_files] = img1;
                            
                            if (thumbs[num_files] == NULL)
                            {
                                fprintf(stderr,
                                        "\nError: Couldn't create a thumbnail of "
                                        "saved image!\n" "%s\n", fname);
                            }
                            
                            num_files++;
                        }
                        else
                        {
                            /* No thumbnail - load original: */
                            
                            /* Make sure we have a ~/.tuxpaint/saved directory: */
                            if (make_directory("saved", "Can't create user data directory"))
                            {
                                /* (Make sure we have a .../saved/.thumbs/ directory:) */
                                make_directory("saved/.thumbs", "Can't create user data thumbnail directory");
                            }
                            
                            
                            if (img == NULL)
                            {
                                snprintf(fname, sizeof(fname), "%s/%s",
                                         dirname[d_places[num_files]], f->d_name);
                                debug(fname);
                                img = myIMG_Load(fname);
                            }
                            
                            
                            show_progress_bar(screen);
                            
                            if (img == NULL)
                            {
                                fprintf(stderr,
                                        "\nWarning: I can't open one of the saved files!\n"
                                        "%s\n"
                                        "The Simple DirectMedia Layer error that "
                                        "occurred was:\n" "%s\n\n", fname, SDL_GetError());
                                
                                free(d_names[num_files]);
                                free(d_exts[num_files]);
                            }
                            else
                            {
                                /* Turn it into a thumbnail: */
                                
                                img1 = SDL_DisplayFormatAlpha(img);
                                img2 = thumbnail2(img1, THUMB_W - 20, THUMB_H - 20, 0, 0);
                                SDL_FreeSurface(img1);
                                
                                show_progress_bar(screen);
                                
                                thumbs[num_files] = SDL_DisplayFormat(img2);
                                SDL_FreeSurface(img2);
                                if (thumbs[num_files] == NULL)
                                {
                                    fprintf(stderr,
                                            "\nError: Couldn't create a thumbnail of "
                                            "saved image!\n" "%s\n", fname);
                                }
                                
                                SDL_FreeSurface(img);
                                
                                show_progress_bar(screen);
                                
                                
                                /* Let's save this thumbnail, so we don't have to
                                 create it again next time 'Open' is called: */
                                
                                if (d_places[num_files] == PLACE_SAVED_DIR)
                                {
                                    debug("Saving thumbnail for this one!");
                                    
                                    snprintf(fname, sizeof(fname), "%s/.thumbs/%s-t.bmp",
                                             dirname[d_places[num_files]], d_names[num_files]);
                                    
                                    fi = fopen(fname, "wb");
                                    if (fi == NULL)
                                    {
                                        fprintf(stderr,
                                                "\nError: Couldn't save thumbnail of "
                                                "saved image!\n"
                                                "%s\n"
                                                "The error that occurred was:\n"
                                                "%s\n\n", fname, strerror(errno));
                                    }
                                    else
                                    {
                                        do_png_save(fi, fname, thumbs[num_files]);
                                    }
                                    
                                    show_progress_bar(screen);
                                }
                                
                                
                                num_files++;
                            }
                        }
                    }
                }
                else
                {
                    /* It was a thumbnail file ("...-t.png") */
                }
            }
        }
        
        
        
#ifdef DEBUG
        printf("%d saved files were found!\n", num_files);
#endif
        
        
        
        if (num_files == 0)
        {
            do_prompt_snd(PROMPT_OPEN_NOFILES_TXT, PROMPT_OPEN_NOFILES_YES, "",
                          SND_YOUCANNOT,
                          (TOOL_OPEN % 2) * 48 + 24, (TOOL_OPEN / 2) * 48 + 40 + 24);
        }
        else
        {
            /* Let user choose an image: */
            
            /* Instructions for 'Open' file dialog */
            char *freeme =
            textdir(gettext_noop("Choose the picture you want, "
                                 "then click “Open”."));
            draw_tux_text(TUX_BORED, freeme, 1);
            free(freeme);
            
            /* NOTE: cur is now set above; if file_id'th file is found, it's
             set to that file's index; otherwise, we default to '0' */
            
            update_list = 1;
            want_erase = 0;
            
            done = 0;
            slideshow = 0;
            
            last_click_which = -1;
            last_click_time = 0;
            last_click_button = -1;
            
            
            do_setcursor(cursor_arrow);
            
            
            do
            {
                /* Update screen: */
                
                if (update_list)
                {
                    /* Erase screen: */
                    
                    dest.x = 96;
                    dest.y = 0;
                    dest.w = WINDOW_WIDTH - 96 - 96;
                    dest.h = 48 * 7 + 40 + HEIGHTOFFSET;
                    
                    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format,
                                                           255, 255, 255));
                    
                    
                    /* Draw icons: */
                    
                    for (i = cur; i < cur + 16 && i < num_files; i++)
                    {
                        /* Draw cursor: */
                        
                        dest.x = THUMB_W * ((i - cur) % 4) + 96;
                        dest.y = THUMB_H * ((i - cur) / 4) + 24;
                        
                        if (i == which)
                        {
                            SDL_BlitSurface(img_cursor_down, NULL, screen, &dest);
                            debug(d_names[i]);
                        }
                        else
                            SDL_BlitSurface(img_cursor_up, NULL, screen, &dest);
                        
                        
                        
                        dest.x = THUMB_W * ((i - cur) % 4) + 96 + 10 +
                        (THUMB_W - 20 - thumbs[i]->w) / 2;
                        dest.y = THUMB_H * ((i - cur) / 4) + 24 + 10 +
                        (THUMB_H - 20 - thumbs[i]->h) / 2;
                        
                        if (thumbs[i] != NULL)
                            SDL_BlitSurface(thumbs[i], NULL, screen, &dest);
                    }
                    
                    
                    /* Draw arrows: */
                    
                    dest.x = (WINDOW_WIDTH - img_scroll_up->w) / 2;
                    dest.y = 0;
                    
                    if (cur > 0)
                        SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
                    else
                        SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
                    
                    dest.x = (WINDOW_WIDTH - img_scroll_up->w) / 2;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
                    
                    if (cur < num_files - 16)
                        SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
                    else
                        SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
                    
                    
                    /* "Open" button: */
                    
                    dest.x = 96;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
                    SDL_BlitSurface(img_open, NULL, screen, &dest);
                    
                    dest.x = 96 + (48 - img_openlabels_open->w) / 2;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_open->h;
                    SDL_BlitSurface(img_openlabels_open, NULL, screen, &dest);
                    
                    
                    /* "Slideshow" button: */
                    
                    dest.x = 96 + 48;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
                    if (any_saved_files)
                        SDL_BlitSurface(img_btn_up, NULL, screen, &dest);
                    else
                        SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
                    
                    dest.x = 96 + 48;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
                    SDL_BlitSurface(img_slideshow, NULL, screen, &dest);
                    
                    dest.x = 96 + 48 + (48 - img_openlabels_slideshow->w) / 2;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_slideshow->h;
                    SDL_BlitSurface(img_openlabels_slideshow, NULL, screen, &dest);
                    
                    
                    /* "Back" button: */
                    
                    dest.x = WINDOW_WIDTH - 96 - 48;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
                    SDL_BlitSurface(img_back, NULL, screen, &dest);
                    
                    dest.x = WINDOW_WIDTH - 96 - 48 + (48 - img_openlabels_back->w) / 2;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_back->h;
                    SDL_BlitSurface(img_openlabels_back, NULL, screen, &dest);
                    
                    
                    /* "Erase" button: */
                    
                    dest.x = WINDOW_WIDTH - 96 - 48 - 48;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
                    
                    if (d_places[which] != PLACE_STARTERS_DIR &&
                        d_places[which] != PLACE_PERSONAL_STARTERS_DIR)
                        SDL_BlitSurface(img_erase, NULL, screen, &dest);
                    else
                        SDL_BlitSurface(img_btn_off, NULL, screen, &dest);
                    
                    dest.x =
                    WINDOW_WIDTH - 96 - 48 - 48 + (48 - img_openlabels_erase->w) / 2;
                    dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_erase->h;
                    SDL_BlitSurface(img_openlabels_erase, NULL, screen, &dest);
                    
                    
                    SDL_Flip(screen);
                    
                    update_list = 0;
                }
                
                
                SDL_WaitEvent(&event);
                
                if (event.type == SDL_QUIT)
                {
                    done = 1;
                    
                    /* FIXME: Handle SDL_Quit better */
                }
                else if (event.type == SDL_ACTIVEEVENT)
                {
                    handle_active(&event);
                }
                else if (event.type == SDL_KEYUP)
                {
                    key = event.key.keysym.sym;
                    
                    handle_keymouse(key, SDL_KEYUP);
                }
                else if (event.type == SDL_KEYDOWN)
                {
                    key = event.key.keysym.sym;
                    
                    handle_keymouse(key, SDL_KEYDOWN);
                    
                    if (key == SDLK_LEFT)
                    {
                        if (which > 0)
                        {
                            which--;
                            
                            if (which < cur)
                                cur = cur - 4;
                            
                            update_list = 1;
                        }
                    }
                    else if (key == SDLK_RIGHT)
                    {
                        if (which < num_files - 1)
                        {
                            which++;
                            
                            if (which >= cur + 16)
                                cur = cur + 4;
                            
                            update_list = 1;
                        }
                    }
                    else if (key == SDLK_UP)
                    {
                        if (which >= 0)
                        {
                            which = which - 4;
                            
                            if (which < 0)
                                which = 0;
                            
                            if (which < cur)
                                cur = cur - 4;
                            
                            update_list = 1;
                        }
                    }
                    else if (key == SDLK_DOWN)
                    {
                        if (which < num_files)
                        {
                            which = which + 4;
                            
                            if (which >= num_files)
                                which = num_files - 1;
                            
                            if (which >= cur + 16)
                                cur = cur + 4;
                            
                            update_list = 1;
                        }
                    }
                    else if (key == SDLK_RETURN || key == SDLK_SPACE)
                    {
                        /* Open */
                        
                        done = 1;
                        playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
                    }
                    else if (key == SDLK_ESCAPE)
                    {
                        /* Go back: */
                        
                        which = -1;
                        done = 1;
                        playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
                    }
                    else if (key == SDLK_d &&
                             (event.key.keysym.mod & KMOD_CTRL) &&
                             d_places[which] != PLACE_STARTERS_DIR &&
                             d_places[which] != PLACE_PERSONAL_STARTERS_DIR && !noshortcuts)
                    {
                        /* Delete! */
                        
                        want_erase = 1;
                    }
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN &&
                         valid_click(event.button.button))
                {
                    if (event.button.x >= 96 && event.button.x < WINDOW_WIDTH - 96 &&
                        event.button.y >= 24 &&
                        event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 48))
                    {
                        /* Picked an icon! */
                        
                        int old_which = which;
                        
                        which = ((event.button.x - 96) / (THUMB_W) +
                                 (((event.button.y - 24) / THUMB_H) * 4)) + cur;
                        
                        if (which < num_files)
                        {
                            playsound(screen, 1, SND_BLEEP, 1, event.button.x, SNDDIST_NEAR);
                            update_list = 1;
                            
                            
                            if (which == last_click_which &&
                                SDL_GetTicks() < last_click_time + 1000 &&
                                event.button.button == last_click_button)
                            {
                                /* Double-click! */
                                
                                done = 1;
                            }
                            
                            last_click_which = which;
                            last_click_time = SDL_GetTicks();
                            last_click_button = event.button.button;
                        }
                        else
                            which = old_which;
                    }
                    else if (event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                             event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2)
                    {
                        if (event.button.y < 24)
                        {
                            /* Up scroll button: */
                            
                            if (cur > 0)
                            {
                                cur = cur - 4;
                                update_list = 1;
                                playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER,
                                          SNDDIST_NEAR);
                                
                                if (cur == 0)
                                    do_setcursor(cursor_arrow);
                            }
                            
                            if (which >= cur + 16)
                                which = which - 4;
                        }
                        else if (event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET - 48) &&
                                 event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 24))
                        {
                            /* Down scroll button: */
                            
                            if (cur < num_files - 16)
                            {
                                cur = cur + 4;
                                update_list = 1;
                                playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER,
                                          SNDDIST_NEAR);
                                
                                if (cur >= num_files - 16)
                                    do_setcursor(cursor_arrow);
                            }
                            
                            if (which < cur)
                                which = which + 4;
                        }
                    }
                    else if (event.button.x >= 96 && event.button.x < 96 + 48 &&
                             event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                             event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
                    {
                        /* Open */
                        
                        done = 1;
                        playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
                    }
                    else if (event.button.x >= 96 + 48 && event.button.x < 96 + 48 + 48 &&
                             event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                             event.button.y < (48 * 7 + 40 + HEIGHTOFFSET) &&
                             any_saved_files == 1)
                    {
                        /* Slideshow */
                        
                        done = 1;
                        slideshow = 1;
                        playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
                    }
                    else if (event.button.x >= (WINDOW_WIDTH - 96 - 48) &&
                             event.button.x < (WINDOW_WIDTH - 96) &&
                             event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                             event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
                    {
                        /* Back */
                        
                        which = -1;
                        done = 1;
                        playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
                    }
                    else if (event.button.x >= (WINDOW_WIDTH - 96 - 48 - 48) &&
                             event.button.x < (WINDOW_WIDTH - 48 - 96) &&
                             event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                             event.button.y < (48 * 7 + 40 + HEIGHTOFFSET) &&
                             d_places[which] != PLACE_STARTERS_DIR &&
                             d_places[which] != PLACE_PERSONAL_STARTERS_DIR)
                    {
                        /* Erase */
                        
                        want_erase = 1;
                    }
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN &&
                         event.button.button >= 4 && event.button.button <= 5 && wheely)
                {
                    /* Scroll wheel! */
                    
                    if (event.button.button == 4 && cur > 0)
                    {
                        cur = cur - 4;
                        update_list = 1;
                        playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                        
                        if (cur == 0)
                            do_setcursor(cursor_arrow);
                        
                        if (which >= cur + 16)
                            which = which - 4;
                    }
                    else if (event.button.button == 5 && cur < num_files - 16)
                    {
                        cur = cur + 4;
                        update_list = 1;
                        playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                        
                        if (cur >= num_files - 16)
                            do_setcursor(cursor_arrow);
                        
                        if (which < cur)
                            which = which + 4;
                    }
                }
                else if (event.type == SDL_MOUSEMOTION)
                {
                    /* Deal with mouse pointer shape! */
                    
                    if (event.button.y < 24 &&
                        event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                        event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2 &&
                        cur > 0)
                    {
                        /* Scroll up button: */
                        
                        do_setcursor(cursor_up);
                    }
                    else if (event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET - 48) &&
                             event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 24) &&
                             event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                             event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2 &&
                             cur < num_files - 16)
                    {
                        /* Scroll down button: */
                        
                        do_setcursor(cursor_down);
                    }
                    else if (((event.button.x >= 96 && event.button.x < 96 + 48 + 48) ||
                              (event.button.x >= (WINDOW_WIDTH - 96 - 48) &&
                               event.button.x < (WINDOW_WIDTH - 96)) ||
                              (event.button.x >= (WINDOW_WIDTH - 96 - 48 - 48) &&
                               event.button.x < (WINDOW_WIDTH - 48 - 96) &&
                               d_places[which] != PLACE_STARTERS_DIR &&
                               d_places[which] != PLACE_PERSONAL_STARTERS_DIR)) &&
                             event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                             event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
                    {
                        /* One of the command buttons: */
                        
                        do_setcursor(cursor_hand);
                    }
                    else if (event.button.x >= 96 && event.button.x < WINDOW_WIDTH - 96 &&
                             event.button.y > 24 &&
                             event.button.y < (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                             ((((event.button.x - 96) / (THUMB_W) +
                                (((event.button.y - 24) / THUMB_H) * 4)) +
                               cur) < num_files))
                    {
                        /* One of the thumbnails: */
                        
                        do_setcursor(cursor_hand);
                    }
                    else
                    {
                        /* Unclickable... */
                        
                        do_setcursor(cursor_arrow);
                    }
                }
                
                
                if (want_erase)
                {
                    want_erase = 0;
                    
                    if (do_prompt_image_snd(PROMPT_ERASE_TXT,
                                            PROMPT_ERASE_YES, PROMPT_ERASE_NO,
                                            thumbs[which],
                                            img_popup_arrow, img_trash, SND_AREYOUSURE,
                                            WINDOW_WIDTH - 96 - 48 - 48 + 24,
                                            48 * 7 + 40 + HEIGHTOFFSET - 48 + 24))
                    {
                        snprintf(fname, sizeof(fname), "saved/%s%s",
                                 d_names[which], d_exts[which]);
                        
                        rfname = get_fname(fname, DIR_SAVE);
                        debug(rfname);
                        
                        if (unlink(rfname) == 0)
                        {
                            update_list = 1;
                            
                            
                            /* Delete the thumbnail, too: */
                            
                            snprintf(fname, sizeof(fname),
                                     "saved/.thumbs/%s-t.bmp", d_names[which]);
                            
                            free(rfname);
                            rfname = get_fname(fname, DIR_SAVE);
                            debug(rfname);
                            
                            unlink(rfname);
                            
                            
                            /* Try deleting old-style thumbnail, too: */
                            
                            unlink(rfname);
                            snprintf(fname, sizeof(fname), "saved/%s-t.bmp", d_names[which]);
                            
                            free(rfname);
                            rfname = get_fname(fname, DIR_SAVE);
                            debug(rfname);
                            
                            unlink(rfname);
                            
                            
                            /* Delete .dat file, if any: */
                            
                            unlink(rfname);
                            snprintf(fname, sizeof(fname), "saved/%s.dat", d_names[which]);
                            
                            free(rfname);
                            rfname = get_fname(fname, DIR_SAVE);
                            debug(rfname);
                            
                            unlink(rfname);
                            
                            
                            /* Move all other files up a notch: */
                            
                            free(d_names[which]);
                            free(d_exts[which]);
                            free_surface(&thumbs[which]);
                            
                            thumbs[which] = NULL;
                            
                            for (i = which; i < num_files - 1; i++)
                            {
                                d_names[i] = d_names[i + 1];
                                d_exts[i] = d_exts[i + 1];
                                thumbs[i] = thumbs[i + 1];
                                d_places[i] = d_places[i + 1];
                            }
                            
                            num_files--;
                            
                            
                            /* Make sure the cursor doesn't go off the end! */
                            
                            if (which >= num_files)
                                which = num_files - 1;
                            
                            
                            /* Scroll up if the cursor goes off top of screen! */
                            
                            if (which < cur && cur >= 4)
                            {
                                cur = cur - 4;
                                update_list = 1;
                            }
                            
                            
                            /* No files to open now? */
                            
                            if (which < 0)
                            {
                                do_prompt_snd(PROMPT_OPEN_NOFILES_TXT,
                                              PROMPT_OPEN_NOFILES_YES, "", SND_YOUCANNOT,
                                              screen->w / 2, screen->h / 2);
                                done = 1;
                            }
                        }
                        else
                        {
                            perror(rfname);
                            
                            do_prompt_snd("CAN'T", "OK", "", SND_YOUCANNOT, 0, 0);
                            update_list = 1;
                        }
                        
                        free(rfname);
                    }
                    else
                    {
                        update_list = 1;
                    }
                }
                
            }
            while (!done);
            
            
            if (!slideshow)
            {
                /* Load the chosen picture: */
                
                if (which != -1)
                {
                    /* Save old one first? */
                    
                    if (!been_saved && !disable_save)
                    {
                        if (do_prompt_image_snd(PROMPT_OPEN_SAVE_TXT,
                                                PROMPT_OPEN_SAVE_YES,
                                                PROMPT_OPEN_SAVE_NO,
                                                img_tools[TOOL_SAVE], NULL, NULL,
                                                SND_AREYOUSURE, screen->w / 2, screen->h / 2))
                        {
                            do_save(TOOL_OPEN, 1);
                        }
                    }
                    
                    
                    /* Figure out filename: */
                    
                    snprintf(fname, sizeof(fname), "%s/%s%s",
                             dirname[d_places[which]], d_names[which], d_exts[which]);
                    
                    img = myIMG_Load(fname);
                    
                    if (img == NULL)
                    {
                        fprintf(stderr,
                                "\nWarning: Couldn't load the saved image!\n"
                                "%s\n"
                                "The Simple DirectMedia Layer error that occurred "
                                "was:\n" "%s\n\n", fname, SDL_GetError());
                        
                        do_prompt(PROMPT_OPEN_UNOPENABLE_TXT,
                                  PROMPT_OPEN_UNOPENABLE_YES, "", 0, 0);
                    }
                    else
                    {
                        free_surface(&img_starter);
                        free_surface(&img_starter_bkgd);
                        starter_mirrored = 0;
                        starter_flipped = 0;
                        starter_personal = 0;
                        
                        autoscale_copy_smear_free(img, canvas, SDL_BlitSurface);
                        
                        cur_undo = 0;
                        oldest_undo = 0;
                        newest_undo = 0;
                        
                        /* Saved image: */
                        
                        been_saved = 1;
                        
                        strcpy(file_id, d_names[which]);
                        starter_id[0] = '\0';
                        
                        
                        /* See if this saved image was based on a 'starter' */
                        
                        load_starter_id(d_names[which]);
                        
                        if (starter_id[0] != '\0')
                        {
                            load_starter(starter_id);
                            
                            if (starter_mirrored)
                                mirror_starter();
                            
                            if (starter_flipped)
                                flip_starter();
                        }
                        
                        reset_avail_tools();
                        
                        tool_avail_bak[TOOL_UNDO] = 0;
                        tool_avail_bak[TOOL_REDO] = 0;
                        
                        opened_something = 1;
                    }
                }
            }
            
            
            update_canvas(0, 0, WINDOW_WIDTH - 96 - 96, 48 * 7 + 40 + HEIGHTOFFSET);
        }
        
        
        /* Clean up: */
        
        free_surface_array(thumbs, num_files);
        
        free(thumbs);
        
        for (i = 0; i < num_files; i++)
        {
            free(d_names[i]);
            free(d_exts[i]);
        }
        
        for (i = 0; i < NUM_PLACES_TO_LOOK; i++)
            if (dirname[i] != NULL)
                free(dirname[i]);
        
        free(d_names);
        free(d_exts);
        free(d_places);
        
        
        if (slideshow)
        {
            slideshow = do_slideshow();
        }
    }
    while (slideshow);
    
    return(opened_something);
}


/* FIXME: This, do_open() and do_new_dialog() should be combined and modularized! */

/* Slide Show Selection Screen: */

int do_slideshow(void)
{
    SDL_Surface *img, *img1, *img2;
    int things_alloced;
    SDL_Surface **thumbs = NULL;
    DIR *d;
    struct dirent *f;
    struct dirent2 *fs;
    char *dirname;
    char **d_names = NULL, **d_exts = NULL;
    int *selected;
    int num_selected;
    FILE *fi;
    char fname[1024];
    int num_files, num_files_in_dir, i, done, update_list, cur, which, j,
    go_back, found, speed;
    SDL_Rect dest;
    SDL_Event event;
    SDLKey key;
    char *freeme;
    int speeds;
    float x_per, y_per;
    int xx, yy;
    SDL_Surface *btn, *blnk;
    
    do_setcursor(cursor_watch);
    
    /* Allocate some space: */
    
    things_alloced = 32;
    
    fs = (struct dirent2 *) malloc(sizeof(struct dirent2) * things_alloced);
    
    num_files_in_dir = 0;
    num_files = 0;
    cur = 0;
    which = 0;
    
    
    /* Load list of saved-images: */
    
    dirname = get_fname("saved", DIR_SAVE);
    
    
    /* Read directory of images and build thumbnails: */
    
    d = opendir(dirname);
    
    if (d != NULL)
    {
        /* Gather list of files (for sorting): */
        
        do
        {
            f = readdir(d);
            
            if (f != NULL)
            {
                memcpy(&(fs[num_files_in_dir].f), f, sizeof(struct dirent));
                fs[num_files_in_dir].place = PLACE_SAVED_DIR;
                
                num_files_in_dir++;
                
                if (num_files_in_dir >= things_alloced)
                {
                    things_alloced = things_alloced + 32;
                    fs = (struct dirent2 *) realloc(fs,
                                                    sizeof(struct dirent2) *
                                                    things_alloced);
                }
            }
        }
        while (f != NULL);
        
        closedir(d);
    }
    
    
    /* (Re)allocate space for the information about these files: */
    
    thumbs = (SDL_Surface * *)malloc(sizeof(SDL_Surface *) * num_files_in_dir);
    d_names = (char **) malloc(sizeof(char *) * num_files_in_dir);
    d_exts = (char **) malloc(sizeof(char *) * num_files_in_dir);
    selected = (int *) malloc(sizeof(int) * num_files_in_dir);
    
    
    /* Sort: */
    
    qsort(fs, num_files_in_dir, sizeof(struct dirent2),
          (int (*)(const void *, const void *)) compare_dirent2s);
    
    
    /* Read directory of images and build thumbnails: */
    
    for (j = 0; j < num_files_in_dir; j++)
    {
        f = &(fs[j].f);
        
        show_progress_bar(screen);
        
        if (f != NULL)
        {
            debug(f->d_name);
            
            if (strcasestr(f->d_name, "-t.") == NULL &&
                strcasestr(f->d_name, "-back.") == NULL)
            {
                if (strcasestr(f->d_name, FNAME_EXTENSION) != NULL
                    /* Support legacy BMP files for load: */
                    || strcasestr(f->d_name, ".bmp") != NULL
                    )
                {
                    strcpy(fname, f->d_name);
                    if (strcasestr(fname, FNAME_EXTENSION) != NULL)
                    {
                        strcpy((char *) strcasestr(fname, FNAME_EXTENSION), "");
                        d_exts[num_files] = strdup(FNAME_EXTENSION);
                    }
                    
                    if (strcasestr(fname, ".bmp") != NULL)
                    {
                        strcpy((char *) strcasestr(fname, ".bmp"), "");
                        d_exts[num_files] = strdup(".bmp");
                    }
                    
                    d_names[num_files] = strdup(fname);
                    
                    
                    /* FIXME: Try to center list on whatever was selected
                     in do_open() when the slideshow button was clicked. */
                    
                    /* Try to load thumbnail first: */
                    
                    snprintf(fname, sizeof(fname), "%s/.thumbs/%s-t.bmp",
                             dirname, d_names[num_files]);
                    debug("Loading thumbnail...");
                    debug(fname);
                    img = IMG_Load(fname);
                    if (img == NULL)
                    {
                        /* No thumbnail in the new location ("saved/.thumbs"),
                         try the old locatin ("saved/"): */
                        
                        snprintf(fname, sizeof(fname), "%s/%s-t.bmp",
                                 dirname, d_names[num_files]);
                        debug(fname);
                        
                        img = IMG_Load(fname);
                    }
                    
                    
                    if (img != NULL)
                    {
                        /* Loaded the thumbnail from one or the other location */
                        
                        debug("Thumbnail loaded, scaling");
                        show_progress_bar(screen);
                        
                        img1 = SDL_DisplayFormat(img);
                        SDL_FreeSurface(img);
                        
                        if (img1 != NULL)
                        {
                            /* if too big, or too small in both dimensions, rescale it
                             (for now: using old thumbnail as source for high speed, low quality) */
                            if (img1->w > THUMB_W - 20 || img1->h > THUMB_H - 20
                                || (img1->w < THUMB_W - 20 && img1->h < THUMB_H - 20))
                            {
                                img2 = thumbnail(img1, THUMB_W - 20, THUMB_H - 20, 0);
                                SDL_FreeSurface(img1);
                                img1 = img2;
                            }
                            
                            thumbs[num_files] = img1;
                            
                            if (thumbs[num_files] == NULL)
                            {
                                fprintf(stderr,
                                        "\nError: Couldn't create a thumbnail of saved image!\n"
                                        "%s\n", fname);
                            }
                            else
                                num_files++;
                        }
                    }
                    else
                    {
                        /* No thumbnail - load original: */
                        
                        /* Make sure we have a ~/.tuxpaint/saved directory: */
                        if (make_directory("saved", "Can't create user data directory"))
                        {
                            /* (Make sure we have a .../saved/.thumbs/ directory:) */
                            make_directory("saved/.thumbs", "Can't create user data thumbnail directory");
                        }
                        
                        snprintf(fname, sizeof(fname), "%s/%s", dirname, f->d_name);
                        
                        debug("Loading original, to make thumbnail");
                        debug(fname);
                        img = myIMG_Load(fname);
                        
                        
                        show_progress_bar(screen);
                        
                        
                        if (img == NULL)
                        {
                            fprintf(stderr,
                                    "\nWarning: I can't open one of the saved files!\n"
                                    "%s\n"
                                    "The Simple DirectMedia Layer error that "
                                    "occurred was:\n" "%s\n\n", fname, SDL_GetError());
                        }
                        else
                        {
                            /* Turn it into a thumbnail: */
                            
                            img1 = SDL_DisplayFormatAlpha(img);
                            img2 = thumbnail2(img1, THUMB_W - 20, THUMB_H - 20, 0, 0);
                            SDL_FreeSurface(img1);
                            
                            show_progress_bar(screen);
                            
                            thumbs[num_files] = SDL_DisplayFormat(img2);
                            SDL_FreeSurface(img2);
                            
                            SDL_FreeSurface(img);
                            
                            if (thumbs[num_files] == NULL)
                            {
                                fprintf(stderr,
                                        "\nError: Couldn't create a thumbnail of saved image!\n"
                                        "%s\n", fname);
                            }
                            else
                            {
                                show_progress_bar(screen);
                                
                                /* Let's save this thumbnail, so we don't have to
                                 create it again next time 'Open' is called: */
                                
                                debug("Saving thumbnail for this one!");
                                
                                snprintf(fname, sizeof(fname),
                                         "%s/.thumbs/%s-t.bmp", dirname, d_names[num_files]);
                                
                                fi = fopen(fname, "wb");
                                if (fi == NULL)
                                {
                                    fprintf(stderr,
                                            "\nError: Couldn't save thumbnail of saved image!\n"
                                            "%s\n"
                                            "The error that occurred was:\n"
                                            "%s\n\n", fname, strerror(errno));
                                }
                                else
                                {
                                    do_png_save(fi, fname, thumbs[num_files]);
                                }
                                
                                show_progress_bar(screen);
                                
                                num_files++;
                            }
                        }
                    }
                }
            }
        }
    }
    
#ifdef DEBUG
    printf("%d saved files were found!\n", num_files);
#endif
    /* Let user choose images: */
    
    /* Instructions for Slideshow file dialog (FIXME: Make a #define) */
    freeme = textdir(gettext_noop("Choose the pictures you want, "
                                  "then click “Play”."));
    draw_tux_text(TUX_BORED, freeme, 1);
    free(freeme);
    
    /* NOTE: cur is now set above; if file_id'th file is found, it's
     set to that file's index; otherwise, we default to '0' */
    
    update_list = 1;
    
    go_back = 0;
    done = 0;
    
    /* FIXME: Make these global, so it sticks between views? */
    num_selected = 0;
    speed = 5;
    
    do_setcursor(cursor_arrow);
    
    
    do
    {
        /* Update screen: */
        
        if (update_list)
        {
            /* Erase screen: */
            
            dest.x = 96;
            dest.y = 0;
            dest.w = WINDOW_WIDTH - 96 - 96;
            dest.h = 48 * 7 + 40 + HEIGHTOFFSET;
            
            SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
            
            
            /* Draw icons: */
            
            for (i = cur; i < cur + 16 && i < num_files; i++)
            {
                /* Draw cursor: */
                
                dest.x = THUMB_W * ((i - cur) % 4) + 96;
                dest.y = THUMB_H * ((i - cur) / 4) + 24;
                
                if (i == which)
                {
                    SDL_BlitSurface(img_cursor_down, NULL, screen, &dest);
                    debug(d_names[i]);
                }
                else
                    SDL_BlitSurface(img_cursor_up, NULL, screen, &dest);
                
                if (thumbs[i] != NULL)
                {
                    dest.x = THUMB_W * ((i - cur) % 4) + 96 + 10 +
                    (THUMB_W - 20 - thumbs[i]->w) / 2;
                    dest.y = THUMB_H * ((i - cur) / 4) + 24 + 10 +
                    (THUMB_H - 20 - thumbs[i]->h) / 2;
                    
                    SDL_BlitSurface(thumbs[i], NULL, screen, &dest);
                }
                
                found = -1;
                
                for (j = 0; j < num_selected && found == -1; j++)
                {
                    if (selected[j] == i)
                        found = j;
                }
                
                if (found != -1)
                {
                    dest.x = (THUMB_W * ((i - cur) % 4) + 96 + 10 +
                              (THUMB_W - 20 - thumbs[i]->w) / 2) + thumbs[i]->w;
                    dest.y = (THUMB_H * ((i - cur) / 4) + 24 + 10 +
                              (THUMB_H - 20 - thumbs[i]->h) / 2) + thumbs[i]->h;
                    
                    draw_selection_digits(dest.x, dest.y, found + 1);
                }
            }
            
            
            /* Draw arrows: */
            
            dest.x = (WINDOW_WIDTH - img_scroll_up->w) / 2;
            dest.y = 0;
            
            if (cur > 0)
                SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
            else
                SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
            
            dest.x = (WINDOW_WIDTH - img_scroll_up->w) / 2;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
            
            if (cur < num_files - 16)
                SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
            else
                SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
            
            
            /* "Play" button: */
            
            dest.x = 96;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
            SDL_BlitSurface(img_play, NULL, screen, &dest);
            
            dest.x = 96 + (48 - img_openlabels_play->w) / 2;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_play->h;
            SDL_BlitSurface(img_openlabels_play, NULL, screen, &dest);
            
            
            /* "Back" button: */
            
            dest.x = WINDOW_WIDTH - 96 - 48;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
            SDL_BlitSurface(img_back, NULL, screen, &dest);
            
            dest.x = WINDOW_WIDTH - 96 - 48 + (48 - img_openlabels_back->w) / 2;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_back->h;
            SDL_BlitSurface(img_openlabels_back, NULL, screen, &dest);
            
            
            /* Speed control: */
            
            speeds = 10;
            x_per = 96.0 / speeds;
            y_per = 48.0 / speeds;
            
            for (i = 0; i < speeds; i++)
            {
                xx = ceil(x_per);
                yy = ceil(y_per * i);
                
                if (i <= speed)
                    btn = thumbnail(img_btn_down, xx, yy, 0);
                else
                    btn = thumbnail(img_btn_up, xx, yy, 0);
                
                blnk = thumbnail(img_btn_off, xx, 48 - yy, 0);
                
                /* FIXME: Check for NULL! */
                
                dest.x = 96 + 48 + (i * x_per);
                dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
                SDL_BlitSurface(blnk, NULL, screen, &dest);
                
                dest.x = 96 + 48 + (i * x_per);
                dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - (y_per * i);
                SDL_BlitSurface(btn, NULL, screen, &dest);
                
                SDL_FreeSurface(btn);
                SDL_FreeSurface(blnk);
            }
            
            SDL_Flip(screen);
            
            update_list = 0;
        }
        
        
        SDL_WaitEvent(&event);
        
        if (event.type == SDL_QUIT)
        {
            done = 1;
            
            /* FIXME: Handle SDL_Quit better */
        }
        else if (event.type == SDL_ACTIVEEVENT)
        {
            handle_active(&event);
        }
        else if (event.type == SDL_KEYUP)
        {
            key = event.key.keysym.sym;
            
            handle_keymouse(key, SDL_KEYUP);
        }
        else if (event.type == SDL_KEYDOWN)
        {
            key = event.key.keysym.sym;
            
            handle_keymouse(key, SDL_KEYDOWN);
            
            if (key == SDLK_RETURN || key == SDLK_SPACE)
            {
                /* Play */
                
                done = 1;
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
            }
            else if (key == SDLK_ESCAPE)
            {
                /* Go back: */
                
                go_back = 1;
                done = 1;
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN &&
                 valid_click(event.button.button))
        {
            if (event.button.x >= 96 && event.button.x < WINDOW_WIDTH - 96 &&
                event.button.y >= 24 &&
                event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 48))
            {
                /* Picked an icon! */
                
                which = ((event.button.x - 96) / (THUMB_W) +
                         (((event.button.y - 24) / THUMB_H) * 4)) + cur;
                
                if (which < num_files)
                {
                    playsound(screen, 1, SND_BLEEP, 1, event.button.x, SNDDIST_NEAR);
                    
                    /* Is it selected already? */
                    
                    found = -1;
                    for (i = 0; i < num_selected && found == -1; i++)
                    {
                        if (selected[i] == which)
                            found = i;
                    }
                    
                    if (found == -1)
                    {
                        /* No!  Select it! */
                        
                        selected[num_selected++] = which;
                    }
                    else
                    {
                        /* Yes!  Unselect it! */
                        
                        for (i = found; i < num_selected - 1; i++)
                            selected[i] = selected[i + 1];
                        
                        num_selected--;
                    }
                    
                    update_list = 1;
                }
            }
            else if (event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                     event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2)
            {
                if (event.button.y < 24)
                {
                    /* Up scroll button: */
                    
                    if (cur > 0)
                    {
                        cur = cur - 4;
                        update_list = 1;
                        playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                        
                        if (cur == 0)
                            do_setcursor(cursor_arrow);
                    }
                    
                    if (which >= cur + 16)
                        which = which - 4;
                }
                else if (event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET - 48) &&
                         event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 24))
                {
                    /* Down scroll button: */
                    
                    if (cur < num_files - 16)
                    {
                        cur = cur + 4;
                        update_list = 1;
                        playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                        
                        if (cur >= num_files - 16)
                            do_setcursor(cursor_arrow);
                    }
                    
                    if (which < cur)
                        which = which + 4;
                }
            }
            else if (event.button.x >= 96 && event.button.x < 96 + 48 &&
                     event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
            {
                /* Play */
                
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
                
                
                /* If none selected, select all, in order! */
                
                if (num_selected == 0)
                {
                    for (i = 0; i < num_files; i++)
                        selected[i] = i;
                    num_selected = num_files;
                }
                
                play_slideshow(selected, num_selected, dirname, d_names, d_exts, speed);
                
                
                /* Redraw entire screen, after playback: */
                
                SDL_FillRect(screen, NULL, SDL_MapRGB(canvas->format, 255, 255, 255));
                draw_toolbar();
                draw_colors(COLORSEL_CLOBBER_WIPE);
                draw_none();
                
                /* Instructions for Slideshow file dialog (FIXME: Make a #define) */
                freeme = textdir(gettext_noop("Choose the pictures you want, "
                                              "then click “Play”."));
                draw_tux_text(TUX_BORED, freeme, 1);
                free(freeme);
                
                SDL_Flip(screen);
                
                update_list = 1;
            }
            else if (event.button.x >= 96 + 48 && event.button.x < 96 + 48 + 96 &&
                     event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
            {
                /* Speed slider */
                
                int old_speed, control_sound, click_x;
                
                old_speed = speed;
                
                click_x = event.button.x - 96 - 48;
                speed = ((10 * click_x) / 96);
                
                control_sound = -1;
                
                if (speed < old_speed)
                    control_sound = SND_SHRINK;
                else if (speed > old_speed)
                    control_sound = SND_GROW;
                
                if (control_sound != -1)
                {
                    playsound(screen, 0, control_sound, 0, SNDPOS_CENTER,
                              SNDDIST_NEAR);
                    
                    update_list = 1;
                }
            }
            else if (event.button.x >= (WINDOW_WIDTH - 96 - 48) &&
                     event.button.x < (WINDOW_WIDTH - 96) &&
                     event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
            {
                /* Back */
                
                go_back = 1;
                done = 1;
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN &&
                 event.button.button >= 4 && event.button.button <= 5 && wheely)
        {
            /* Scroll wheel! */
            
            if (event.button.button == 4 && cur > 0)
            {
                cur = cur - 4;
                update_list = 1;
                playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                
                if (cur == 0)
                    do_setcursor(cursor_arrow);
                
                if (which >= cur + 16)
                    which = which - 4;
            }
            else if (event.button.button == 5 && cur < num_files - 16)
            {
                cur = cur + 4;
                update_list = 1;
                playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                
                if (cur >= num_files - 16)
                    do_setcursor(cursor_arrow);
                
                if (which < cur)
                    which = which + 4;
            }
        }
        else if (event.type == SDL_MOUSEMOTION)
        {
            /* Deal with mouse pointer shape! */
            
            if (event.button.y < 24 &&
                event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2 && cur > 0)
            {
                /* Scroll up button: */
                
                do_setcursor(cursor_up);
            }
            else if (event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET - 48) &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 24) &&
                     event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                     event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2 &&
                     cur < num_files - 16)
            {
                /* Scroll down button: */
                
                do_setcursor(cursor_down);
            }
            else if (((event.button.x >= 96 && event.button.x < 96 + 48 + 96) ||
                      (event.button.x >= (WINDOW_WIDTH - 96 - 48) &&
                       event.button.x < (WINDOW_WIDTH - 96))) &&
                     event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
            {
                /* One of the command buttons: */
                
                do_setcursor(cursor_hand);
            }
            else if (event.button.x >= 96 && event.button.x < WINDOW_WIDTH - 96
                     && event.button.y > 24
                     && event.button.y < (48 * 7 + 40 + HEIGHTOFFSET) - 48
                     &&
                     ((((event.button.x - 96) / (THUMB_W) +
                        (((event.button.y - 24) / THUMB_H) * 4)) + cur) <
                      num_files))
            {
                /* One of the thumbnails: */
                
                do_setcursor(cursor_hand);
            }
            else
            {
                /* Unclickable... */
                
                do_setcursor(cursor_arrow);
            }
        }
    }
    while (!done);
    
    
    /* Clean up: */
    
    free_surface_array(thumbs, num_files);
    
    free(thumbs);
    
    for (i = 0; i < num_files; i++)
    {
        free(d_names[i]);
        free(d_exts[i]);
    }
    
    free(dirname);
    
    free(d_names);
    free(d_exts);
    free(selected);
    
    
    return go_back;
}


void play_slideshow(int * selected, int num_selected, char * dirname,
                    char **d_names, char **d_exts, int speed)
{
    int i, which, next, done;
    SDL_Surface * img;
    char * tmp_starter_id, * tmp_file_id;
    int tmp_starter_mirrored, tmp_starter_flipped, tmp_starter_personal;
    char fname[1024];
    SDL_Event event;
    SDLKey key;
    SDL_Rect dest;
    Uint32 last_ticks;
    
    
    /* Back up the current image's IDs, because they will get
     clobbered below! */
    
    tmp_starter_id = strdup(starter_id);
    tmp_file_id = strdup(file_id);
    tmp_starter_mirrored = starter_mirrored;
    tmp_starter_flipped = starter_flipped;
    tmp_starter_personal = starter_personal;
    
    do_setcursor(cursor_tiny);
    
    done = 0;
    
    do
    {
        for (i = 0; i < num_selected && !done; i++)
        {
            which = selected[i];
            show_progress_bar(screen);
            
            
            /* Figure out filename: */
            
            snprintf(fname, sizeof(fname), "%s/%s%s",
                     dirname, d_names[which], d_exts[which]);
            
            
            img = myIMG_Load(fname);
            
            if (img != NULL)
            {
                autoscale_copy_smear_free(img, screen, SDL_BlitSurface);
                
                strcpy(file_id, d_names[which]);
                
                
                /* See if this saved image was based on a 'starter' */
                
                load_starter_id(d_names[which]);
                
                if (starter_id[0] != '\0')
                {
                    load_starter(starter_id);
                    
                    if (starter_mirrored)
                        mirror_starter();
                    
                    if (starter_flipped)
                        flip_starter();
                }
            }
            
            /* "Back" button: */
            
            dest.x = screen->w - 48;
            dest.y = 0;
            SDL_BlitSurface(img_back, NULL, screen, &dest);
            
            dest.x = screen->w - 48 + (48 - img_openlabels_back->w) / 2;
            dest.y = 48 - img_openlabels_back->h;
            SDL_BlitSurface(img_openlabels_back, NULL, screen, &dest);
            
            /* "Next" button: */
            
            dest.x = 0;
            dest.y = 0;
            SDL_BlitSurface(img_play, NULL, screen, &dest);
            
            dest.x = (48 - img_openlabels_next->w) / 2;
            dest.y = 48 - img_openlabels_next->h;
            SDL_BlitSurface(img_openlabels_next, NULL, screen, &dest);
            
            
            SDL_Flip(screen);
            
            
            /* Handle any events, and otherwise wait for time to count down: */
            
            next = 0;
            last_ticks = SDL_GetTicks();
            
            do
            {
                while (SDL_PollEvent(&event))
                {
                    if (event.type == SDL_QUIT)
                    {
                        /* FIXME: Handle SDL_QUIT better! */
                        
                        next = 1;
                        done = 1;
                    }
                    else if (event.type == SDL_ACTIVEEVENT)
                    {
                        handle_active(&event);
                    }
                    else if (event.type == SDL_KEYDOWN)
                    {
                        key = event.key.keysym.sym;
                        
                        handle_keymouse(key, SDL_KEYDOWN);
                        
                        if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_RIGHT)
                        {
                            /* RETURN, SPACE or RIGHT: Skip to next right away! */
                            
                            next = 1;
                            playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
                        }
                        else if (key == SDLK_LEFT)
                        {
                            /* LEFT: Go back one! */
                            
                            i = i - 2;
                            
                            if (i < -1)
                                i = num_selected - 2;
                            
                            next = 1;
                            playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
                        }
                        else if (key == SDLK_ESCAPE)
                        {
                            /* Go back: */
                            
                            next = 1;
                            done = 1;
                            playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
                        }
                    }
                    else if (event.type == SDL_MOUSEBUTTONDOWN)
                    {
                        /* Mouse click! */
                        
                        if (event.button.x >= screen->w - 48 &&
                            event.button.y <= 48)
                        {
                            /* Back button */
                            
                            next = 1;
                            done = 1;
                            playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
                        }
                        else
                        {
                            /* Otherwise, skip to next image right away! */
                            
                            next = 1;
                            playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
                        }
                    }
                    else if (event.type == SDL_MOUSEMOTION)
                    {
                        /* Deal with mouse pointer shape! */
                        
                        if ((event.button.x >= screen->w - 48 ||
                             event.button.x < 48) &&
                            event.button.y >= screen->h - 48)
                        {
                            /* Back or Next buttons */
                            
                            do_setcursor(cursor_hand);
                        }
                        else
                        {
                            /* Otherwise, minimal cursor... */
                            
                            do_setcursor(cursor_tiny);
                        }
                    }
                }
                
                SDL_Delay(100);
                
                
                /* Automatically skip to the next one after time expires: */
                
                if (speed != 0)
                {
                    if (SDL_GetTicks() >= last_ticks + (10 - speed) * 500)
                        next = 1;
                }
            }
            while (!next);
        }
    }
    while (!done);
    
    strcpy(starter_id, tmp_starter_id);
    free(tmp_starter_id);
    
    strcpy(file_id, tmp_file_id);
    free(tmp_file_id);
    
    starter_mirrored = tmp_starter_mirrored;
    starter_flipped = tmp_starter_flipped;
    starter_personal = tmp_starter_personal;
}



/* Draws large, bitmap digits over thumbnails in slideshow selection screen: */

void draw_selection_digits(int right, int bottom, int n)
{
    SDL_Rect src, dest;
    int i, v, len, place;
    int digit_w, digit_h, x, y;
    
    digit_w = img_select_digits->w / 10;
    digit_h = img_select_digits->h;
    
    if (n > 99)
    {
        len = 3;
        place = 100;
    }
    else if (n > 9)
    {
        len = 2;
        place = 10;
    }
    else
    {
        len = 1;
        place = 1;
    }
    
    x = right - digit_w * len;
    y = bottom - digit_h;
    
    for (i = 0; i < len; i++)
    {
        v = (n / place) % (place * 10);
        
        src.x = digit_w * v;
        src.y = 0;
        src.w = digit_w;
        src.h = digit_h;
        
        dest.x = x;
        dest.y = y;
        
        SDL_BlitSurface(img_select_digits, &src, screen, &dest);
        
        x = x + digit_w;
        place = place / 10;
    }
}


/* Let sound effects (e.g., "Save" sfx) play out before quitting... */

static void wait_for_sfx(void)
{
#ifndef NOSOUND
    if (use_sound)
    {
        while (Mix_Playing(-1))
            SDL_Delay(10);
    }
#endif
}


/* stamp outline */
#ifndef LOW_QUALITY_STAMP_OUTLINE
/* XOR-based outline of rubber stamp shapes
 (unused if LOW_QUALITY_STAMP_OUTLINE is #defined) */

#if 1
#define STIPLE_W 5
#define STIPLE_H 5
static char stiple[] = "84210" "10842" "42108" "08421" "21084";
#endif

#if 0
#define STIPLE_W 4
#define STIPLE_H 4
static char stiple[] = "8000" "0800" "0008" "0080";
#endif

#if 0
#define STIPLE_W 12
#define STIPLE_H 12
static char stiple[] =
"808808000000"
"800000080880"
"008088080000"
"808000000808"
"000080880800"
"088080000008"
"000000808808"
"080880800000" "080000008088" "000808808000" "880800000080" "000008088080";
#endif

static unsigned char *stamp_outline_data;
static int stamp_outline_w, stamp_outline_h;

static void update_stamp_xor(void)
{
    int xx, yy, rx, ry;
    Uint8 dummy;
    SDL_Surface *src;
    Uint32(*getpixel) (SDL_Surface *, int, int);
    unsigned char *alphabits;
    int new_w;
    int new_h;
    unsigned char *outline;
    unsigned char *old_outline_data;
    
    src = active_stamp;
    
    /* start by scaling */
    src = thumbnail(src, CUR_STAMP_W, CUR_STAMP_H, 0);
    
    getpixel = getpixels[src->format->BytesPerPixel];
    alphabits = calloc(src->w + 4, src->h + 4);
    
    SDL_LockSurface(src);
    for (yy = 0; yy < src->h; yy++)
    {
        ry = yy;
        for (xx = 0; xx < src->w; xx++)
        {
            rx = xx;
            SDL_GetRGBA(getpixel(src, rx, ry),
                        src->format, &dummy, &dummy, &dummy,
                        alphabits + xx + 2 + (yy + 2) * (src->w + 4));
        }
    }
    SDL_UnlockSurface(src);
    
    new_w = src->w + 4;
    new_h = src->h + 4;
    SDL_FreeSurface(src);
    outline = calloc(new_w, new_h);
    
    for (yy = 1; yy < new_h - 1; yy++)
    {
        for (xx = 1; xx < new_w - 1; xx++)
        {
            unsigned char above = 0;
            unsigned char below = 0xff;
            unsigned char tmp;
            
            tmp = alphabits[(xx - 1) + (yy - 1) * new_w];
            above |= tmp;
            below &= tmp;
            tmp = alphabits[(xx + 1) + (yy - 1) * new_w];
            above |= tmp;
            below &= tmp;
            
            tmp = alphabits[(xx + 0) + (yy - 1) * new_w];
            above |= tmp;
            below &= tmp;
            tmp = alphabits[(xx + 0) + (yy + 0) * new_w];
            above |= tmp;
            below &= tmp;
            tmp = alphabits[(xx + 1) + (yy + 0) * new_w];
            above |= tmp;
            below &= tmp;
            tmp = alphabits[(xx - 1) + (yy + 0) * new_w];
            above |= tmp;
            below &= tmp;
            tmp = alphabits[(xx + 0) + (yy + 1) * new_w];
            above |= tmp;
            below &= tmp;
            
            tmp = alphabits[(xx - 1) + (yy + 1) * new_w];
            above |= tmp;
            below &= tmp;
            tmp = alphabits[(xx + 1) + (yy + 1) * new_w];
            above |= tmp;
            below &= tmp;
            
            outline[xx + yy * new_w] = (above ^ below) >> 7;
        }
    }
    
    old_outline_data = stamp_outline_data;
    stamp_outline_data = outline;
    stamp_outline_w = new_w;
    stamp_outline_h = new_h;
    if (old_outline_data)
        free(old_outline_data);
    free(alphabits);
}

static void stamp_xor(int x, int y)
{
    int xx, yy, sx, sy;
    
    SDL_LockSurface(screen);
    for (yy = 0; yy < stamp_outline_h; yy++)
    {
        for (xx = 0; xx < stamp_outline_w; xx++)
        {
            if (!stamp_outline_data[xx + yy * stamp_outline_w]) /* FIXME: Conditional jump or move depends on uninitialised value(s) */
                continue;
            sx = x + xx - stamp_outline_w / 2;
            sy = y + yy - stamp_outline_h / 2;
            if (stiple[sx % STIPLE_W + sy % STIPLE_H * STIPLE_W] != '8')
                continue;
            xorpixel(sx, sy);
        }
    }
    SDL_UnlockSurface(screen);
}

#endif

static void rgbtohsv(Uint8 r8, Uint8 g8, Uint8 b8, float *h, float *s,
                     float *v)
{
    float rgb_min, rgb_max, delta, r, g, b;
    
    r = (r8 / 255.0);
    g = (g8 / 255.0);
    b = (b8 / 255.0);
    
    rgb_min = min(r, min(g, b));
    rgb_max = max(r, max(g, b));
    *v = rgb_max;
    
    delta = rgb_max - rgb_min;
    
    if (rgb_max == 0)
    {
        /* Black */
        
        *s = 0;
        *h = -1;
    }
    else
    {
        *s = delta / rgb_max;
        
        if (r == rgb_max)
            *h = (g - b) / delta;
        else if (g == rgb_max)
            *h = 2 + (b - r) / delta;	/* between cyan & yellow */
        else
            *h = 4 + (r - g) / delta;	/* between magenta & cyan */
        
        *h = (*h * 60);		/* degrees */
        
        if (*h < 0)
            *h = (*h + 360);
    }
}


static void hsvtorgb(float h, float s, float v, Uint8 * r8, Uint8 * g8,
                     Uint8 * b8)
{
    int i;
    float f, p, q, t, r, g, b;
    
    if (s == 0)
    {
        /* Achromatic (grey) */
        
        r = v;
        g = v;
        b = v;
    }
    else
    {
        h = h / 60;
        i = floor(h);
        f = h - i;
        p = v * (1 - s);
        q = v * (1 - s * f);
        t = v * (1 - s * (1 - f));
        
        if (i == 0)
        {
            r = v;
            g = t;
            b = p;
        }
        else if (i == 1)
        {
            r = q;
            g = v;
            b = p;
        }
        else if (i == 2)
        {
            r = p;
            g = v;
            b = t;
        }
        else if (i == 3)
        {
            r = p;
            g = q;
            b = v;
        }
        else if (i == 4)
        {
            r = t;
            g = p;
            b = v;
        }
        else
        {
            r = v;
            g = p;
            b = q;
        }
    }
    
    
    *r8 = (Uint8) (r * 255);
    *g8 = (Uint8) (g * 255);
    *b8 = (Uint8) (b * 255);
}

//static void print_image(void)
//{
//    static int last_print_time = 0;
//    int cur_time;
//    
//    cur_time = SDL_GetTicks() / 1000;
//    
//#ifdef DEBUG
//    printf("Current time = %d\n", cur_time);
//#endif
//    
//    if (cur_time >= last_print_time + print_delay)
//    {
//        if (alt_print_command_default == ALTPRINT_ALWAYS)
//            want_alt_printcommand = 1;
//        else if (alt_print_command_default == ALTPRINT_NEVER)
//            want_alt_printcommand = 0;
//        else		/* ALTPRINT_MOD */
//            want_alt_printcommand = (SDL_GetModState() & KMOD_ALT);
//        
//        if (do_prompt_image_snd(PROMPT_PRINT_NOW_TXT,
//                                PROMPT_PRINT_NOW_YES,
//                                PROMPT_PRINT_NOW_NO,
//                                img_printer, NULL, NULL, SND_AREYOUSURE,
//                                (TOOL_PRINT % 2) * 48 + 24,
//                                (TOOL_PRINT / 2) * 48 + 40 + 24))
//        {
//            do_print();
//            
//            last_print_time = cur_time;
//        }
//    }
//    else
//    {
//        do_prompt_image_snd(PROMPT_PRINT_TOO_SOON_TXT,
//                            PROMPT_PRINT_TOO_SOON_YES,
//                            "",
//                            img_printer_wait, NULL, NULL,
//                            SND_YOUCANNOT,
//                            0, screen->h);
//    }
//}

void do_print(void)
{
#if !defined(WIN32) && !defined(__BEOS__) && !defined(__APPLE__)
    char *pcmd;
    FILE *pi;
    
    /* Linux, Unix, etc. */
    
    if (want_alt_printcommand && !fullscreen)
        pcmd = (char *) altprintcommand;
    else
        pcmd = (char *) printcommand;
    
    pi = popen(pcmd, "w");
    
    if (pi == NULL)
    {
        perror(pcmd);
    }
    else
    {
#ifdef PRINTMETHOD_PNG_PNM_PS
        if (do_png_save(pi, pcmd, canvas))
            do_prompt_snd(PROMPT_PRINT_TXT, PROMPT_PRINT_YES, "", SND_TUXOK,
                          screen->w / 2, screen->h / 2);
#elif defined(PRINTMETHOD_PNM_PS)
        /* nothing here */
#elif defined(PRINTMETHOD_PS)
        if (do_ps_save(pi, pcmd, canvas, papersize, 1))
            do_prompt_snd(PROMPT_PRINT_TXT, PROMPT_PRINT_YES, "", SND_TUXOK,
                          screen->w / 2, screen->h / 2);
        else
            do_prompt_snd(PROMPT_PRINT_FAILED_TXT, PROMPT_PRINT_YES, "", SND_YOUCANNOT,
                          screen->w / 2, screen->h / 2);
#else
#error No print method defined!
#endif
    }
#else
#ifdef WIN32
    /* Win32 */
    
    char f[512];
    int show = want_alt_printcommand;
    
    snprintf(f, sizeof(f), "%s/%s", savedir, "print.cfg"); /* FIXME */
    
    {
        const char *error =
        SurfacePrint(canvas, use_print_config ? f : NULL, show);
        
        if (error)
            fprintf(stderr, "%s\n", error);
    }
#elif defined(__BEOS__)
    /* BeOS */
    
    SurfacePrint(canvas);
#elif defined(__APPLE__)
    /* Mac OS X */
    int show = ( ( want_alt_printcommand || macosx.menuAction ) && !fullscreen);
    
    const char *error = SurfacePrint(canvas, show);
    
    if (error)
    {
        fprintf(stderr, "Cannot print: %s\n", error);
        do_prompt_snd(error, PROMPT_PRINT_YES, "", SND_TUXOK, 0, 0);
    }
    
#endif
    
#endif
}

static void do_render_cur_text(int do_blit)
{
    int w, h;
    SDL_Color color = { color_hexes[cur_color][0],
        color_hexes[cur_color][1],
        color_hexes[cur_color][2],
        0
    };
    SDL_Surface *tmp_surf;
    SDL_Rect dest, src;
    wchar_t *str;
    
    hide_blinking_cursor();
    
    /* Keep cursor on the screen! */
    
    if (cursor_y > ((48 * 7 + 40 + HEIGHTOFFSET) -
                    TuxPaint_Font_FontHeight(getfonthandle(cur_font))))
    {
        cursor_y = ((48 * 7 + 40 + HEIGHTOFFSET) -
                    TuxPaint_Font_FontHeight(getfonthandle(cur_font)));
    }
    
    
    /* Render the text: */
    
    if (texttool_len > 0)
    {
#if defined(_FRIBIDI_H) || defined(FRIBIDI_H)
        //FriBidiCharType baseDir = FRIBIDI_TYPE_LTR;
        FriBidiCharType baseDir = FRIBIDI_TYPE_WL; /* Per: Shai Ayal <shaiay@gmail.com>, 2009-01-14 */
        FriBidiChar *unicodeIn, *unicodeOut;
        unsigned int i;
        
        unicodeIn = (FriBidiChar *) malloc(sizeof(FriBidiChar) * (texttool_len + 1));
        unicodeOut = (FriBidiChar *) malloc(sizeof(FriBidiChar) * (texttool_len + 1));
        
        str = (wchar_t *) malloc(sizeof(wchar_t) * (texttool_len + 1));
        
        for (i = 0; i < texttool_len; i++)
            unicodeIn[i] = (FriBidiChar) texttool_str[i];
        
        fribidi_log2vis(unicodeIn, texttool_len, &baseDir, unicodeOut, 0, 0, 0);
        
        /* FIXME: If we determine that some new text was RtoL, we should
         reposition the text */
        
        for (i = 0; i < texttool_len; i++)
            str[i] = (long) unicodeOut[i];
        
        str[texttool_len] = L'\0';
        
        free(unicodeIn);
        free(unicodeOut);
#else
        str = uppercase_w(texttool_str);
#endif
        
        tmp_surf = render_text_w(getfonthandle(cur_font), str, color);
        
        w = tmp_surf->w;
        h = tmp_surf->h;
        
        cursor_textwidth = w;
        
        free(str);
    }
    else
    {
        /* FIXME: Do something different! */
        
        update_canvas(0, 0, WINDOW_WIDTH - 96, (48 * 7) + 40 + HEIGHTOFFSET);
        cursor_textwidth = 0;
        return;
    }
    
    
    if (!do_blit)
    {
        /* FIXME: Only delete what's changed! */
        
        update_canvas(0, 0, WINDOW_WIDTH - 96, (48 * 7) + 40 + HEIGHTOFFSET);
        
        
        /* Draw outline around text: */
        
        dest.x = cursor_x - 2 + 96;
        dest.y = cursor_y - 2;
        dest.w = w + 4;
        dest.h = h + 4;
        
        if (dest.x + dest.w > WINDOW_WIDTH - 96)
            dest.w = WINDOW_WIDTH - 96 - dest.x;
        if (dest.y + dest.h > (48 * 7 + 40 + HEIGHTOFFSET))
            dest.h = (48 * 7 + 40 + HEIGHTOFFSET) - dest.y;
        
        SDL_FillRect(screen, &dest, SDL_MapRGB(canvas->format, 0, 0, 0));
        
        
        /* FIXME: This would be nice if it were alpha-blended: */
        
        dest.x = cursor_x + 96;
        dest.y = cursor_y;
        dest.w = w;
        dest.h = h;
        
        if (dest.x + dest.w > WINDOW_WIDTH - 96)
            dest.w = WINDOW_WIDTH - 96 - dest.x;
        if (dest.y + dest.h > (48 * 7 + 40 + HEIGHTOFFSET))
            dest.h = (48 * 7 + 40 + HEIGHTOFFSET) - dest.y;
        
        if ((color_hexes[cur_color][0] +
             color_hexes[cur_color][1] + color_hexes[cur_color][2]) >= 384)
        {
            /* Grey background if blit is white!... */
            
            SDL_FillRect(screen, &dest, SDL_MapRGB(canvas->format, 64, 64, 64));
        }
        else
        {
            /* White background, normally... */
            
            SDL_FillRect(screen, &dest, SDL_MapRGB(canvas->format, 255, 255, 255));
        }
    }
    
    
    /* Draw the text itself! */
    
    if (tmp_surf != NULL)
    {
        dest.x = cursor_x;
        dest.y = cursor_y;
        
        src.x = 0;
        src.y = 0;
        src.w = tmp_surf->w;
        src.h = tmp_surf->h;
        
        if (dest.x + src.w > WINDOW_WIDTH - 96 - 96)
            src.w = WINDOW_WIDTH - 96 - 96 - dest.x;
        if (dest.y + src.h > (48 * 7 + 40 + HEIGHTOFFSET))
            src.h = (48 * 7 + 40 + HEIGHTOFFSET) - dest.y;
        
        if (do_blit)
        {
            SDL_BlitSurface(tmp_surf, &src, canvas, &dest);
            update_canvas(dest.x, dest.y, dest.x + tmp_surf->w,
                          dest.y + tmp_surf->h);
        }
        else
        {
            dest.x = dest.x + 96;
            SDL_BlitSurface(tmp_surf, &src, screen, &dest);
        }
    }
    
    
    /* FIXME: Only update what's changed! */
    
    SDL_Flip(screen);
    
    
    if (tmp_surf != NULL)
        SDL_FreeSurface(tmp_surf);
}


/* Return string as uppercase if that option is set: */

static char *uppercase(const char *restrict const str)
{
    unsigned int i, n;
    wchar_t *dest;
    char *ustr;
    
    if (!only_uppercase)
        return strdup(str);
    
    /* watch out: uppercase chars may need extra bytes!
     http://en.wikipedia.org/wiki/Turkish_dotted_and_dotless_I */
    n = strlen(str) + 1;
    dest = alloca(sizeof(wchar_t)*n);
    ustr = malloc(n*2); /* use *2 in case 'i' becomes U+0131 */
    
    mbstowcs(dest, str, sizeof(wchar_t)*n); /* at most n wchar_t written */
    i = 0;
    do{
        dest[i] = towupper(dest[i]);
    }while(dest[i++]);
    wcstombs(ustr, dest, n); /* at most n bytes written */
    
#ifdef DEBUG
    printf(" ORIGINAL: %s\n" "UPPERCASE: %s\n\n", str, ustr);
#endif
    return ustr;
}

static wchar_t *uppercase_w(const wchar_t *restrict const str)
{
    unsigned n = wcslen(str) + 1;
    wchar_t *ustr = malloc(sizeof(wchar_t) * n);
    memcpy(ustr,str,sizeof(wchar_t) * n);
    
    if (only_uppercase)
    {
        unsigned i = 0;
        do{
            ustr[i] = towupper(ustr[i]);
        }while(ustr[i++]);
    }
    
    return ustr;
}


/* Return string in right-to-left mode, if necessary: */

static char *textdir(const char *const str)
{
    unsigned char *dstr;
    unsigned i, j;
    unsigned char c1, c2, c3, c4;
    
#ifdef DEBUG
    printf("ORIG_DIR: %s\n", str);
#endif
    
    dstr = malloc(strlen(str) + 5);
    
    if (need_right_to_left_word)
    {
        dstr[strlen(str)] = '\0';
        
        for (i = 0; i < strlen(str); i++)
        {
            j = (strlen(str) - i - 1);
            
            c1 = (unsigned char) str[i + 0];
            c2 = (unsigned char) str[i + 1];
            c3 = (unsigned char) str[i + 2];
            c4 = (unsigned char) str[i + 3];
            
            if (c1 < 128)		/* 0xxx xxxx - 1 byte */
            {
                dstr[j] = str[i];
            }
            else if ((c1 & 0xE0) == 0xC0)	/* 110x xxxx - 2 bytes */
            {
                dstr[j - 1] = c1;
                dstr[j - 0] = c2;
                i = i + 1;
            }
            else if ((c1 & 0xF0) == 0xE0)	/* 1110 xxxx - 3 bytes */
            {
                dstr[j - 2] = c1;
                dstr[j - 1] = c2;
                dstr[j - 0] = c3;
                i = i + 2;
            }
            else			/* 1111 0xxx - 4 bytes */
            {
                dstr[j - 3] = c1;
                dstr[j - 2] = c2;
                dstr[j - 1] = c3;
                dstr[j - 0] = c4;
                i = i + 3;
            }
        }
    }
    else
    {
        strcpy((char *) dstr, str);
    }
    
#ifdef DEBUG
    printf("L2R_DIR: %s\n", dstr);
#endif
    
    return ((char *) dstr);
}


/* Scroll Timer */

static Uint32 scrolltimer_callback(Uint32 interval, void *param)
{
    /* printf("scrolltimer_callback(%d) -- ", interval); */
    if (scrolling)
    {
        /* printf("(Still scrolling)\n"); */
        SDL_PushEvent((SDL_Event *) param);
        return interval;
    }
    else
    {
        /* printf("(all done)\n"); */
        return 0;
    }
}


/* Controls the Text-Timer - interval == 0 removes the timer */

static void control_drawtext_timer(Uint32 interval, const char *const text, Uint8 locale_text)
{
    static int activated = 0;
    static SDL_TimerID TimerID = 0;
    static SDL_Event drawtext_event;
    
    
    /* Remove old timer if any is running */
    
    if (activated)
    {
        SDL_RemoveTimer(TimerID);
        activated = 0;
        TimerID = 0;
    }
    
    if (interval == 0)
        return;
    
    drawtext_event.type = SDL_USEREVENT;
    drawtext_event.user.code = USEREVENT_TEXT_UPDATE;
    drawtext_event.user.data1 = (void *) text;
    drawtext_event.user.data2 = (void *) ((int) locale_text);
    
    
    /* Add new timer */
    
    TimerID =
    SDL_AddTimer(interval, drawtext_callback, (void *) &drawtext_event);
    activated = 1;
}


/* Drawtext Timer */

static Uint32 drawtext_callback(Uint32 interval, void *param)
{
    (void) interval;
    SDL_PushEvent((SDL_Event *) param);
    
    return 0;			/* Remove timer */
}


static void parse_options(FILE * fi)
{
    char str[256];
    
    do
    {
        fgets(str, sizeof(str), fi);
        
        strip_trailing_whitespace(str);
        
        if (!feof(fi))
        {
            debug(str);
            
            
            /* FIXME: This should be handled better! */
            /* (e.g., complain on illegal lines, support comments, blanks, etc.) */
            
            if (strcmp(str, "fullscreen=yes") == 0)
            {
                fullscreen = 1;
            }
            else if (strcmp(str, "fullscreen=native") == 0)
            {
                fullscreen = 1;
                native_screensize = 1;
            }
            else if (strcmp(str, "disablescreensaver=yes") == 0)
            {
                disable_screensaver = 1;
            }
            else if (strcmp(str, "disablescreensaver=no") == 0 ||
                     strcmp(str, "allowscreensaver=yes") == 0)
            {
                disable_screensaver = 0;
            }
            else if (strcmp(str, "native=yes") == 0)
            {
                native_screensize = 1;
            }
            else if (strcmp(str, "native=no") == 0)
            {
                native_screensize = 0;
            }
            else if (strcmp(str, "fullscreen=no") == 0 ||
                     strcmp(str, "windowed=yes") == 0)
            {
                fullscreen = 0;
            }
            else if (strcmp(str, "startblank=yes") == 0)
            {
                start_blank = 1;
            }
            else if (strcmp(str, "startblank=no") == 0 ||
                     strcmp(str, "startlast=yes") == 0)
            {
                start_blank = 0;
            }
            else if (strcmp(str, "nostampcontrols=yes") == 0)
            {
                disable_stamp_controls = 1;
            }
            else if (strcmp(str, "nostampcontrols=no") == 0 ||
                     strcmp(str, "stampcontrols=yes") == 0)
            {
                disable_stamp_controls = 0;
            }
            else if (strcmp(str, "alllocalefonts=yes") == 0)
            {
                all_locale_fonts = 1;
            }
            else if (strcmp(str, "alllocalefonts=no") == 0 ||
                     strcmp(str, "currentlocalefont=yes") == 0)
            {
                all_locale_fonts = 0;
            }
            else if (strcmp(str, "nomagiccontrols=yes") == 0)
            {
                disable_magic_controls = 1;
            }
            else if (strcmp(str, "nomagiccontrols=no") == 0 ||
                     strcmp(str, "magiccontrols=yes") == 0)
            {
                disable_magic_controls = 0;
            }
            else if (strcmp(str, "mirrorstamps=yes") == 0)
            {
                mirrorstamps = 1;
            }
            else if (strcmp(str, "mirrorstamps=no") == 0 ||
                     strcmp(str, "dontmirrorstamps=yes") == 0)
            {
                mirrorstamps = 0;
            }
            else if (strcmp(str, "stampsize=default") == 0)
            {
                stamp_size_override = -1;
            }
            else if (strstr(str, "stampsize=") == str)
            {
                stamp_size_override = atoi(str + 10);
                if (stamp_size_override > 10)
                    stamp_size_override = 10;
            }
            else if (strcmp(str, "noshortcuts=yes") == 0)
            {
                noshortcuts = 1;
            }
            else if (strcmp(str, "noshortcuts=no") == 0 ||
                     strcmp(str, "shortcuts=yes") == 0)
            {
                noshortcuts = 0;
            }
            else if (!memcmp("windowsize=", str, 11))
            {
                char *endp1;
                char *endp2;
                int w, h;
                w = strtoul(str + 11, &endp1, 10);
                h = strtoul(endp1 + 1, &endp2, 10);
                /* sanity check it */
                if (str + 11 == endp1 || endp1 + 1 == endp2 || *endp1 != 'x' || *endp2
                    || w < 500 || h < 480 || h > w * 3 || w > h * 4)
                {
                    /* Oddly, config files have no error checking. */
                    /* show_usage(stderr, (char *) getfilename(argv[0])); */
                    /* exit(1); */
                }
                else
                {
                    WINDOW_WIDTH = w;
                    WINDOW_HEIGHT = h;
                }
            }
            else if (strcmp(str, "800x600=yes") == 0 ||
                     strcmp(str, "windowsize=800x600") == 0)
            {
                /* to handle old config files */
                WINDOW_WIDTH = 800;
                WINDOW_HEIGHT = 600;
            }
            else if (strcmp(str, "800x600=no") == 0 ||
                     strcmp(str, "640x480=yes") == 0 ||
                     strcmp(str, "windowsize=640x480") == 0)
            {
                /* also for old config files */
                WINDOW_WIDTH = 640;
                WINDOW_HEIGHT = 480;
            }
            else if (strcmp(str, "nooutlines=yes") == 0)
            {
                dont_do_xor = 1;
            }
            else if (strcmp(str, "nooutlines=no") == 0 ||
                     strcmp(str, "outlines=yes") == 0)
            {
                dont_do_xor = 0;
            }
            else if (strcmp(str, "keyboard=yes") == 0)
            {
                keymouse = 1;
            }
            else if (strcmp(str, "keyboard=no") == 0 ||
                     strcmp(str, "mouse=yes") == 0)
            {
                keymouse = 0;
            }
            else if (strcmp(str, "nowheelmouse=yes") == 0)
            {
                wheely = 0;
            }
            else if (strcmp(str, "nowheelmouse=no") == 0 ||
                     strcmp(str, "wheelmouse=yes") == 0)
            {
                wheely = 1;
            }
            else if (strcmp(str, "grab=yes") == 0)
            {
                grab_input = 1;
            }
            else if (strcmp(str, "grab=no") == 0 || strcmp(str, "nograb=yes") == 0)
            {
                grab_input = 0;
            }
            else if (strcmp(str, "nofancycursors=yes") == 0)
            {
                no_fancy_cursors = 1;
            }
            else if (strcmp(str, "nofancycursors=no") == 0 ||
                     strcmp(str, "fancycursors=yes") == 0)
            {
                no_fancy_cursors = 0;
            }
            else if (strcmp(str, "hidecursor=yes") == 0)
            {
                hide_cursor = 1;
            }
            else if (strcmp(str, "hidecursor=no") == 0 ||
                     strcmp(str, "showcursor=yes") == 0)
            {
                hide_cursor = 0;
            }
            else if (strcmp(str, "uppercase=yes") == 0)
            {
                only_uppercase = 1;
            }
            else if (strcmp(str, "uppercase=no") == 0 ||
                     strcmp(str, "mixedcase=yes") == 0)
            {
                only_uppercase = 0;
            }
            else if (strcmp(str, "noquit=yes") == 0)
            {
                disable_quit = 1;
            }
            else if (strcmp(str, "noquit=no") == 0 || strcmp(str, "quit=yes") == 0)
            {
                disable_quit = 0;
            }
            else if (strcmp(str, "nosave=yes") == 0)
            {
                disable_save = 1;
            }
            else if (strcmp(str, "nosave=no") == 0 || strcmp(str, "save=yes") == 0)
            {
                disable_save = 0;
            }
            else if (strcmp(str, "noprint=yes") == 0)
            {
                disable_print = 1;
            }
            else if (strcmp(str, "noprint=no") == 0 ||
                     strcmp(str, "print=yes") == 0)
            {
                disable_print = 0;
            }
            else if (strcmp(str, "nostamps=yes") == 0)
            {
                dont_load_stamps = 1;
            }
            else if (strcmp(str, "nostamps=no") == 0 ||
                     strcmp(str, "stamps=yes") == 0)
            {
                dont_load_stamps = 0;
            }
            else if (strcmp(str, "nosysfonts=yes") == 0 ||
                     strcmp(str, "sysfonts=no") == 0)
            {
                no_system_fonts = 1;
            }
            else if (strcmp(str, "nosysfonts=no") == 0 ||
                     strcmp(str, "sysfonts=yes") == 0)
            {
                no_system_fonts = 0;
            }
            else if (strcmp(str, "nobuttondistinction=yes") == 0)
            {
                no_button_distinction = 1;
            }
            else if (strcmp(str, "nobuttondistinction=no") == 0 ||
                     strcmp(str, "buttondistinction=yes") == 0)
            {
                no_button_distinction = 0;
            }
            else if (strcmp(str, "nosound=yes") == 0)
            {
                use_sound = 0;
            }
            else if (strcmp(str, "nosound=no") == 0 ||
                     strcmp(str, "sound=yes") == 0)
            {
                use_sound = 1;
            }
            else if (strcmp(str, "simpleshapes=yes") == 0)
            {
                simple_shapes = 1;
            }
            else if (strcmp(str, "simpleshapes=no") == 0 ||
                     strcmp(str, "complexshapes=yes") == 0)
            {
                simple_shapes = 1;
            }
            /* Should "locale=" be here as well??? */
            /* Comments welcome ... bill@newbreedsoftware.com */
            else if (strstr(str, "lang=") == str)
            {
                set_langstr(str + 5);
            }
            else if (strstr(str, "colorfile=") == str)
            {
                strcpy(colorfile, str + 10);
            }
            else if (strstr(str, "printdelay=") == str)
            {
                sscanf(str + 11, "%d", &print_delay);
#ifdef DEBUG
                printf("Print delay set to %d seconds\n", print_delay);
#endif
            }
            else if (strcmp(str, "printcfg=yes") == 0)
            {
#if !defined(WIN32) && !defined(__APPLE__)
                fprintf(stderr, "Note: printcfg option only applies to Windows and Mac OS X!\n");
#endif
                use_print_config = 1;
            }
            else if (strcmp(str, "printcfg=no") == 0 ||
                     strcmp(str, "noprintcfg=yes") == 0)
            {
#if !defined(WIN32) && !defined(__APPLE__)
                fprintf(stderr, "Note: printcfg option only applies to Windows and Mac OS X!\n");
#endif
                use_print_config = 0;
            }
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
            else if (strstr(str, "printcommand=") == str)
            {
                /* FIXME: This would need to be done one argument (space-delim'd) at a time */
                /*
                 #ifdef __linux__
                 wordexp_t result;
                 char * dir = strdup(str + 13);
                 
                 wordexp(dir, &result, 0);
                 free(dir);
                 
                 printcommand = strdup(result.we_wordv[0]);
                 wordfree(&result);
                 #else
                 */
                printcommand = strdup(str + 13);
                /*
                 #endif
                 */
            }
            else if (strstr(str, "altprintcommand=") == str)
            {
                /* FIXME: This would need to be done one argument (space-delim'd) at a time */
                /*
                 #ifdef __linux__
                 wordexp_t result;
                 char * dir = strdup(str + 16);
                 
                 wordexp(dir, &result, 0);
                 free(dir);
                 
                 altprintcommand = strdup(result.we_wordv[0]);
                 wordfree(&result);
                 #else
                 */
                altprintcommand = strdup(str + 16);
                /*
                 #endif
                 */
            }
#endif
            else if (strcmp(str, "saveover=yes") == 0)
            {
                promptless_save = SAVE_OVER_ALWAYS;
            }
            else if (strcmp(str, "saveover=ask") == 0)
            {
                /* (Default) */
                
                promptless_save = SAVE_OVER_PROMPT;
            }
            else if (strcmp(str, "saveover=new") == 0)
            {
                promptless_save = SAVE_OVER_NO;
            }
            else if (strcmp(str, "autosave=yes") == 0)
            {
                autosave_on_quit = 1;
            }
            else if (strcmp(str, "autosave=no") == 0)
            {
                autosave_on_quit = 0;
            }
            else if (strcmp(str, "altprint=always") == 0)
            {
                alt_print_command_default = ALTPRINT_ALWAYS;
            }
            else if (strcmp(str, "altprint=mod") == 0)
            {
                /* (Default) */
                
                alt_print_command_default = ALTPRINT_MOD;
            }
            else if (strcmp(str, "altprint=never") == 0)
            {
                alt_print_command_default = ALTPRINT_NEVER;
            }
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
            else if (strstr(str, "papersize=") == str)
            {
                papersize = strdup(str + strlen("papersize="));
            }
#endif
            else if (strstr(str, "savedir=") == str)
            {
#ifdef __linux__
                wordexp_t result;
                char * dir = strdup(str + 8);
                
                wordexp(dir, &result, 0);
                free(dir);
                
                savedir = strdup(result.we_wordv[0]);
                wordfree(&result);
#else
                savedir = strdup(str + 8);
#endif
                remove_slash(savedir);
                
#ifdef DEBUG
                printf("savedir set to: %s\n", savedir);
#endif
            }
            else if (strstr(str, "datadir=") == str)
            {
#ifdef __linux__
                wordexp_t result;
                char * dir = strdup(str + 8);
                
                wordexp(dir, &result, 0);
                free(dir);
                
                datadir = strdup(result.we_wordv[0]);
                wordfree(&result);
#else
                datadir = strdup(str + 8);
#endif
                remove_slash(datadir);
                
#ifdef DEBUG
                printf("datadir set to: %s\n", datadir);
#endif
            }
            else if (strcmp(str, "nolockfile=yes") == 0 ||
                     strcmp(str, "lockfile=no") == 0)
            {
                ok_to_use_lockfile = 0;
            }
        }
    }
    while (!feof(fi));
}


#ifdef DEBUG
static char *debug_gettext(const char *str)
{
    if (strcmp(str, dgettext(NULL, str)) == 0)
    {
        printf("NOTRANS: %s\n", str);
        printf("..TRANS: %s\n", dgettext(NULL, str));
        fflush(stdout);
    }
    
    return (dgettext(NULL, str));
}
#endif


static const char *great_str(void)
{
    return (great_strs[rand() % (sizeof(great_strs) / sizeof(char *))]);
}


#ifdef DEBUG
static int charsize(Uint16 c)
{
    Uint16 str[2];
    int w, h;
    
    str[0] = c;
    str[1] = '\0';
    
    TTF_SizeUNICODE(getfonthandle(cur_font), str, &w, &h);
    
    return w;
}
#endif

static void draw_image_title(int t, SDL_Rect dest)
{
    SDL_BlitSurface(img_title_on, NULL, screen, &dest);
    
    dest.x += (dest.w - img_title_names[t]->w) / 2;
    dest.y += (dest.h - img_title_names[t]->h) / 2;
    SDL_BlitSurface(img_title_names[t], NULL, screen, &dest);
}



/* Handle keyboard events to control the mouse: */

static void handle_keymouse(SDLKey key, Uint8 updown)
{
    SDL_Event event;
    
    if (keymouse)
    {
        if (key == SDLK_LEFT)
            mousekey_left = updown;
        else if (key == SDLK_RIGHT)
            mousekey_right = updown;
        else if (key == SDLK_UP)
            mousekey_up = updown;
        else if (key == SDLK_DOWN)
            mousekey_down = updown;
        else if (key == SDLK_SPACE)
        {
            if (updown == SDL_KEYDOWN)
                event.type = SDL_MOUSEBUTTONDOWN;
            else
                event.type = SDL_MOUSEBUTTONUP;
            
            event.button.x = mouse_x;
            event.button.y = mouse_y;
            event.button.button = 1;
            
            SDL_PushEvent(&event);
        }
        
        if (mousekey_up == SDL_KEYDOWN && mouse_y > 0)
            mouse_y = mouse_y - 8;
        else if (mousekey_down == SDL_KEYDOWN && mouse_y < WINDOW_HEIGHT - 1)
            mouse_y = mouse_y + 8;
        
        if (mousekey_left == SDL_KEYDOWN && mouse_x > 0)
            mouse_x = mouse_x - 8;
        if (mousekey_right == SDL_KEYDOWN && mouse_x < WINDOW_WIDTH - 1)
            mouse_x = mouse_x + 8;
        
        SDL_WarpMouse(mouse_x, mouse_y);
    }
}


/* Unblank screen in fullscreen mode, if needed: */

static void handle_active(SDL_Event * event)
{
    if (event->active.state & SDL_APPACTIVE)
    {
        if (event->active.gain == 1)
        {
            if (fullscreen)
                SDL_Flip(screen);
        }
    }
#ifdef _WIN32
    if (event->active.state & SDL_APPINPUTFOCUS|SDL_APPACTIVE)
    {
        SetActivationState(event->active.gain);
    }
#endif
}


/* removes a single '\' or '/' from end of path */

static char *remove_slash(char *path)
{
    int len = strlen(path);
    
    if (!len)
        return path;
    
    if (path[len - 1] == '/' || path[len - 1] == '\\')
        path[len - 1] = 0;
    
    return path;
}

/* replace '~' at the beginning of a path with home directory */
/*
 static char *replace_tilde(const char* const path)
 {
 char *newpath;
 int newlen;
 
 int len = strlen(path);
 
 if (!len)
 return strdup("");
 
 if (path[0] == '~')
 {
 newlen = strlen(getenv("HOME")) + len;
 newpath = malloc(sizeof(char)*newlen);
 sprintf(newpath, "%s%s", getenv("HOME"), path+1);
 }
 else
 newpath = strdup(path);
 
 return newpath;
 }
 */

/* For right-to-left languages, when word-wrapping, we need to
 make sure the text doesn't end up going from bottom-to-top, too! */

#ifdef NO_SDLPANGO
static void anti_carriage_return(int left, int right, int cur_top,
                                 int new_top, int cur_bot, int line_width)
{
    SDL_Rect src, dest;
    
    
    /* Move current set of text down one line (and right-justify it!): */
    
    src.x = left;
    src.y = cur_top;
    src.w = line_width;
    src.h = cur_bot - cur_top;
    
    dest.x = right - line_width;
    dest.y = new_top;
    
    SDL_BlitSurface(screen, &src, screen, &dest);
    
    
    /* Clear the top line for new text: */
    
    dest.x = left;
    dest.y = cur_top;
    dest.w = right - left;
    dest.h = new_top - cur_top;
    
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
}
#endif


static SDL_Surface *duplicate_surface(SDL_Surface * orig)
{
    /*
     Uint32 amask;
     
     amask = ~(orig->format->Rmask |
     orig->format->Gmask |
     orig->format->Bmask);
     
     return(SDL_CreateRGBSurface(SDL_SWSURFACE,
     orig->w, orig->h,
     orig->format->BitsPerPixel,
     orig->format->Rmask,
     orig->format->Gmask,
     orig->format->Bmask,
     amask));
     */
    
    return (SDL_DisplayFormatAlpha(orig));
}

static void mirror_starter(void)
{
    SDL_Surface *orig;
    int x;
    SDL_Rect src, dest;
    
    
    /* Mirror overlay: */
    
    orig = img_starter;
    img_starter = duplicate_surface(orig);
    
    if (img_starter != NULL)
    {
        for (x = 0; x < orig->w; x++)
        {
            src.x = x;
            src.y = 0;
            src.w = 1;
            src.h = orig->h;
            
            dest.x = orig->w - x - 1;
            dest.y = 0;
            
            SDL_BlitSurface(orig, &src, img_starter, &dest);
        }
        
        SDL_FreeSurface(orig);
    }
    else
    {
        img_starter = orig;
    }
    
    
    /* Mirror background: */
    
    if (img_starter_bkgd != NULL)
    {
        orig = img_starter_bkgd;
        img_starter_bkgd = duplicate_surface(orig);
        
        if (img_starter_bkgd != NULL)
        {
            for (x = 0; x < orig->w; x++)
            {
                src.x = x;
                src.y = 0;
                src.w = 1;
                src.h = orig->h;
                
                dest.x = orig->w - x - 1;
                dest.y = 0;
                
                SDL_BlitSurface(orig, &src, img_starter_bkgd, &dest);
            }
            
            SDL_FreeSurface(orig);
        }
        else
        {
            img_starter_bkgd = orig;
        }
    }
}


static void flip_starter(void)
{
    SDL_Surface *orig;
    int y;
    SDL_Rect src, dest;
    
    
    /* Flip overlay: */
    
    orig = img_starter;
    img_starter = duplicate_surface(orig);
    
    if (img_starter != NULL)
    {
        for (y = 0; y < orig->h; y++)
        {
            src.x = 0;
            src.y = y;
            src.w = orig->w;
            src.h = 1;
            
            dest.x = 0;
            dest.y = orig->h - y - 1;
            
            SDL_BlitSurface(orig, &src, img_starter, &dest);
        }
        
        SDL_FreeSurface(orig);
    }
    else
    {
        img_starter = orig;
    }
    
    
    /* Flip background: */
    
    if (img_starter_bkgd != NULL)
    {
        orig = img_starter_bkgd;
        img_starter_bkgd = duplicate_surface(orig);
        
        if (img_starter_bkgd != NULL)
        {
            for (y = 0; y < orig->h; y++)
            {
                src.x = 0;
                src.y = y;
                src.w = orig->w;
                src.h = 1;
                
                dest.x = 0;
                dest.y = orig->h - y - 1;
                
                SDL_BlitSurface(orig, &src, img_starter_bkgd, &dest);
            }
            
            SDL_FreeSurface(orig);
        }
        else
        {
            img_starter_bkgd = orig;
        }
    }
}


int valid_click(Uint8 button)
{
    if (button == 1 || ((button == 2 || button == 3) && no_button_distinction))
        return (1);
    else
        return (0);
}


int in_circle(int x, int y)
{
    if ((x * x) + (y * y) - (16 * 16) < 0)
        return (1);
    else
        return (0);
}

int in_circle_rad(int x, int y, int rad)
{
    if ((x * x) + (y * y) - (rad * rad) < 0)
        return (1);
    else
        return (0);
}

int paintsound(int size)
{
    if (SND_PAINT1 + (size / 12) >= SND_PAINT4)
        return(SND_PAINT3);
    else
        return(SND_PAINT1 + (size / 12));
}


#ifndef NOSVG

#ifdef OLD_SVG

/* Old libcairo1, svg and svg-cairo based code
 Based on cairo-demo/sdl/main.c from Cairo (GPL'd, (c) 2004 Eric Windisch):
 */

SDL_Surface * load_svg(char * file)
{
    svg_cairo_t * scr;
    int bpp, btpp, stride;
    unsigned int width, height;
    unsigned int rwidth, rheight;
    float scale;
    unsigned char * image;
    cairo_surface_t * cairo_surface;
    cairo_t * cr;
    SDL_Surface * sdl_surface, * sdl_surface_tmp;
    Uint32 rmask, gmask, bmask, amask;
    svg_cairo_status_t res;
    
    
#ifdef DEBUG
    printf("Attempting to load \"%s\" as an SVG\n", file);
#endif
    
    /* Create the SVG cairo stuff: */
    if (svg_cairo_create(&scr) != SVG_CAIRO_STATUS_SUCCESS)
    {
#ifdef DEBUG
        printf("svg_cairo_create() failed\n");
#endif
        return(NULL);
    }
    
    /* Parse the SVG file: */
    if (svg_cairo_parse(scr, file) != SVG_CAIRO_STATUS_SUCCESS)
    {
        svg_cairo_destroy(scr);
#ifdef DEBUG
        printf("svg_cairo_parse(%s) failed\n", file);
#endif
        return(NULL);
    }
    
    /* Get the natural size of the SVG */
    svg_cairo_get_size(scr, &rwidth, &rheight);
#ifdef DEBUG
    printf("svg_get_size(): %d x %d\n", rwidth, rheight);
#endif
    
    if (rwidth == 0 || rheight == 0)
    {
        svg_cairo_destroy(scr);
#ifdef DEBUG
        printf("SVG %s had 0 width or height!\n", file);
#endif
        return(NULL);
    }
    
    /* We will create a CAIRO_FORMAT_ARGB32 surface. We don't need to match
     the screen SDL format, but we are interested in the alpha bit... */
    bpp = 32;
    btpp = 4;
    
    /* We want to render at full Tux Paint canvas size, so that the stamp
     at its largest scale remains highest quality (no pixelization):
     (but not messing up the aspect ratio) */
    
    scale = pick_best_scape(rwidth, rheight, r_canvas.w, r_canvas.h);
    
    width = ((float) rwidth * scale);
    height = ((float) rheight * scale);
    
#ifdef DEBUG
    printf("scaling to %d x %d (%f scale)\n", width, height, scale);
#endif
    
    /* scanline width */
    stride = width * btpp;
    
    /* Allocate space for an image: */
    image = calloc(stride * height, 1);
    
#ifdef DEBUG
    printf("calling cairo_image_surface_create_for_data(..., CAIRO_FORMAT_ARGB32, %d(w), %d(h), %d(stride))\n", width, height, stride);
#endif
    
    /* Create the cairo surface with the adjusted width and height */
    
    cairo_surface = cairo_image_surface_create_for_data(image,
                                                        CAIRO_FORMAT_ARGB32,
                                                        width, height, stride);
    cr = cairo_create(cairo_surface);
    if (cr == NULL)
    {
        svg_cairo_destroy(scr);
#ifdef DEBUG
        printf("cairo_create() failed\n");
#endif
        return(NULL);
    }
    
    /* Scale it (proportionally) */
    cairo_scale(cr, scale, scale);  /* no return value :( */
    
    /* Render SVG to our surface: */
    res = svg_cairo_render(scr, cr);
    
    /* Clean up: */
    cairo_surface_destroy(cairo_surface);
    cairo_destroy(cr);
    svg_cairo_destroy(scr);
    
    if (res != SVG_CAIRO_STATUS_SUCCESS)
    {
#ifdef DEBUG
        printf("svg_cairo_render() failed\n");
#endif
        return(NULL);
    }
    
    
    /* Adjust the SDL surface to match the cairo surface created
     (surface mask of ARGB)  NOTE: Is this endian-agnostic? -bjk 2006.10.25 */
    rmask = 0x00ff0000;
    gmask = 0x0000ff00;
    bmask = 0x000000ff;
    amask = 0xff000000;
    
    /* Create the SDL surface using the pixel data stored: */
    sdl_surface_tmp = SDL_CreateRGBSurfaceFrom((void *) image, width, height,
                                               bpp, stride,
                                               rmask, gmask, bmask, amask);
    
    if (sdl_surface_tmp == NULL)
    {
#ifdef DEBUG
        printf("SDL_CreateRGBSurfaceFrom() failed\n");
#endif
        return(NULL);
    }
    
    
    /* Convert the SDL surface to the display format, for faster blitting: */
    sdl_surface = SDL_DisplayFormatAlpha(sdl_surface_tmp);
    SDL_FreeSurface(sdl_surface_tmp);
    
    if (sdl_surface == NULL)
    {
#ifdef DEBUG
        printf("SDL_DisplayFormatAlpha() failed\n");
#endif
        return(NULL);
    }
    
#ifdef DEBUG
    printf("SDL surface from %d x %d SVG is %d x %d\n",
           rwidth, rheight, sdl_surface->w, sdl_surface->h);
#endif
    
    return(sdl_surface);
}

#else

/* New libcairo2, rsvg and rsvg-cairo based code */
//SDL_Surface * load_svg(char * file) dailin
//{
//  cairo_surface_t * cairo_surf;
//  cairo_t * cr;
//  RsvgHandle * rsvg_handle;
//  GError * gerr;
//  unsigned char * image;
//  int rwidth, rheight;
//  int width, height, stride;
//  float scale;
//  int bpp = 32, btpp = 4;
//  RsvgDimensionData dimensions;
//  SDL_Surface * sdl_surface, * sdl_surface_tmp;
//  Uint32 rmask, gmask, bmask, amask;
//
//#ifdef DEBUG
//  printf("load_svg(%s)\n", file);
//#endif
//
//  /* Create an RSVG Handle from the SVG file: */
//
//  rsvg_init();
//
//  gerr = NULL;
//
//  rsvg_handle = rsvg_handle_new_from_file(file, &gerr);
//  if (rsvg_handle == NULL)
//  {
//#ifdef DEBUG
//    fprintf(stderr, "rsvg_handle_new_from_file() failed\n");
//#endif
//    return(NULL);
//  }
//
//  rsvg_handle_get_dimensions(rsvg_handle, &dimensions);
//  rwidth = dimensions.width;
//  rheight = dimensions.height;
//
//#ifdef DEBUG
//  printf("SVG is %d x %d\n", rwidth, rheight);
//#endif
//
//
//  /* Pick best scale to render to (for the canvas in this instance of Tux Paint) */
//
//  scale = pick_best_scape(rwidth, rheight, r_canvas.w, r_canvas.h);
//
//#ifdef DEBUG
//  printf("best scale is %.4f\n", scale);
//#endif
//
//  width = ((float) rwidth * scale);
//  height = ((float) rheight * scale);
//
//#ifdef DEBUG
//  printf("scaling to %d x %d (%f scale)\n", width, height, scale);
//#endif
//
//  /* scanline width */
//  stride = width * btpp;
//
//  /* Allocate space for an image: */
//  image = calloc(stride * height, 1);
//  if (image == NULL)
//  {
//#ifdef DEBUG
//    fprintf(stderr, "Unable to allocate image buffer\n");
//#endif
//    g_object_unref(rsvg_handle);
//    return(NULL);
//  }
//
//
//  /* Create a surface for Cairo to draw into: */
//
//  cairo_surf = cairo_image_surface_create_for_data(image,
//                                                   CAIRO_FORMAT_ARGB32,
//                                                   width, height, stride);
//
//  if (cairo_surface_status(cairo_surf) != CAIRO_STATUS_SUCCESS)
//  {
//#ifdef DEBUG
//    fprintf(stderr, "cairo_image_surface_create() failed\n");
//#endif
//    g_object_unref(rsvg_handle);
//    free(image);
//    return(NULL);
//  }
//
//
//  /* Create a new Cairo object: */
//
//  cr = cairo_create(cairo_surf);
//  if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
//  {
//#ifdef DEBUG
//    fprintf(stderr, "cairo_create() failed\n");
//#endif
//    g_object_unref(rsvg_handle);
//    cairo_surface_destroy(cairo_surf);
//    free(image);
//    return(NULL);
//  }
//
//
//  /* Ask RSVG to render the SVG into the Cairo object: */
//
//  cairo_scale(cr, scale, scale);
//
//  /* FIXME: We can use cairo_rotate() here to rotate stamps! -bjk 2007.06.21 */
//
//  rsvg_handle_render_cairo(rsvg_handle, cr);
//
//
//  cairo_surface_finish(cairo_surf);
//
//
//  /* Adjust the SDL surface to match the cairo surface created
//    (surface mask of ARGB)  NOTE: Is this endian-agnostic? -bjk 2006.10.25 */
//  rmask = 0x00ff0000;
//  gmask = 0x0000ff00;
//  bmask = 0x000000ff;
//  amask = 0xff000000;
//
//  /* Create the SDL surface using the pixel data stored: */
//  sdl_surface_tmp = SDL_CreateRGBSurfaceFrom((void *) image, width, height,
//                                             bpp, stride,
//                                             rmask, gmask, bmask, amask);
//
//  if (sdl_surface_tmp == NULL)
//  {
//#ifdef DEBUG
//    fprintf(stderr, "SDL_CreateRGBSurfaceFrom() failed\n");
//#endif
//    g_object_unref(rsvg_handle);
//    cairo_surface_destroy(cairo_surf);
//    free(image);
//    cairo_destroy(cr);
//    return(NULL);
//  }
//
//  /* Convert the SDL surface to the display format, for faster blitting: */
//  sdl_surface = SDL_DisplayFormatAlpha(sdl_surface_tmp);
//  SDL_FreeSurface(sdl_surface_tmp);
//
//  if (sdl_surface == NULL)
//  {
//#ifdef DEBUG
//    fprintf(stderr, "SDL_DisplayFormatAlpha() failed\n");
//#endif
//    g_object_unref(rsvg_handle);
//    cairo_surface_destroy(cairo_surf);
//    free(image);
//    cairo_destroy(cr);
//    return(NULL);
//  }
//
//
//#ifdef DEBUG
//  printf("SDL surface from %d x %d SVG is %d x %d\n",
//	  rwidth, rheight, sdl_surface->w, sdl_surface->h);
//#endif
//
//
//  /* Clean up: */
//
//  g_object_unref(rsvg_handle);
//  cairo_surface_destroy(cairo_surf);
//  free(image);
//  cairo_destroy(cr);
//
//  rsvg_term();
//
//  return(sdl_surface);
//}

#endif



/* Load an image; call load_svg() (above, to call Cairo and SVG-Cairo funcs)
 if we notice it's an SVG file,
 otherwise call SDL_Image lib's IMG_Load() (for PNGs, JPEGs, BMPs, etc.) */
SDL_Surface * myIMG_Load(char * file)
{
    //  if (strlen(file) > 4 && strcasecmp(file + strlen(file) - 4, ".svg") == 0) dailin
    //    return(load_svg(file));
    //  else
    return(IMG_Load(file));
}

float pick_best_scape(unsigned int orig_w, unsigned int orig_h,
                      unsigned int max_w, unsigned int max_h)
{
    float aspect, scale, wscale, hscale;
    
    aspect = (float) orig_w / (float) orig_h;
    
#ifdef DEBUG
    printf("trying to fit %d x %d (aspect: %.4f) into %d x %d\n",
           orig_w, orig_h, aspect, max_w, max_h);
#endif
    
    wscale = (float) max_w / (float) orig_w;
    hscale = (float) max_h / (float) orig_h;
    
#ifdef DEBUG
    printf("max_w / orig_w = wscale: %.4f\n", wscale);
    printf("max_h / orig_h = hscale: %.4f\n", hscale);
    printf("\n");
#endif
    
    if (aspect >= 1)
    {
        /* Image is wider-than-tall (or square) */
        
        scale = wscale;
        
#ifdef DEBUG
        printf("Wider-than-tall.  Using wscale.\n");
        printf("new size would be: %d x %d\n",
               (int) ((float) orig_w * scale),
               (int) ((float) orig_h * scale));
#endif
        
        if ((float) orig_h * scale > (float) max_h)
        {
            scale = hscale;
            
#ifdef DEBUG
            printf("Too tall!  Using hscale!\n");
            printf("new size would be: %d x %d\n",
                   (int) ((float) orig_w * scale),
                   (int) ((float) orig_h * scale));
#endif
        }
    }
    else
    {
        /* Taller-than-wide */
        
        scale = hscale;
        
#ifdef DEBUG
        printf("Taller-than-wide.  Using hscale.\n");
        printf("new size would be: %d x %d\n",
               (int) ((float) orig_w * scale),
               (int) ((float) orig_h * scale));
#endif
        
        if ((float) orig_w * scale > (float) max_w)
        {
            scale = wscale;
            
#ifdef DEBUG
            printf("Too wide!  Using wscale!\n");
            printf("new size would be: %d x %d\n",
                   (int) ((float) orig_w * scale),
                   (int) ((float) orig_h * scale));
#endif
        }
    }
    
    
#ifdef DEBUG
    printf("\n");
    printf("Final scale: %.4f\n", scale);
#endif
    
    return(scale);
}

#endif



void load_magic_plugins(void)
{
#ifdef __IPHONEOS__
#define magic_command(name,func) name##_##func
    extern const char* DATA_PREFIX;
#endif
    
    int res, n, i, plc;
    char * place;
    int err;
    DIR *d;
    struct dirent *f;
    char fname[512];
    char objname[512];
    char funcname[512];
    
    num_plugin_files = 0;
    num_magics = 0;
    
    //    for (plc = 0; plc < NUM_MAGIC_PLACES; plc++)
    //    {
    //        if (plc == MAGIC_PLACE_GLOBAL)
    //            place = strdup(MAGIC_PREFIX);
    //        else if (plc == MAGIC_PLACE_LOCAL)
    //            place = get_fname("plugins/", DIR_DATA);
    //#ifdef __APPLE__
    //        else if (plc == MAGIC_PLACE_ALLUSERS)
    //            place = strdup("/Library/Application Support/TuxPaint/plugins/");
    //#endif
    //        else
    //            continue; /* Huh? */
    //
    //#ifdef DEBUG
    //        printf("\n");
    //        printf("Loading magic plug-ins from %s\n", place);
    //        fflush(stdout);
    //#endif
    //
    //        /* Gather list of files (for sorting): */
    //
    //        d = opendir(place);
    //
    //        if (d != NULL)
    //        {
    /* Set magic API hooks: */
    
    magic_api_struct = (magic_api *) malloc(sizeof(magic_api));
    magic_api_struct->tp_version = strdup(VER_VERSION);
    
    //            if (plc == MAGIC_PLACE_GLOBAL)
    char temp[256];
    sprintf(temp,"%s/",DATA_PREFIX);
    magic_api_struct->data_directory = temp;
    //            else if (plc == MAGIC_PLACE_LOCAL)
    //                magic_api_struct->data_directory = get_fname("plugins/data/", DIR_DATA);
    //#ifdef __APPLE__
    //            else if (plc == MAGIC_PLACE_ALLUSERS)
    //                magic_api_struct->data_directory = strdup("/Library/Application Support/TuxPaint/plugins/data");
    //#endif
    //            else
    //                magic_api_struct->data_directory = strdup("./");
    
    magic_api_struct->update_progress_bar = update_progress_bar;
    magic_api_struct->sRGB_to_linear = magic_sRGB_to_linear;
    magic_api_struct->linear_to_sRGB = magic_linear_to_sRGB;
    magic_api_struct->in_circle = in_circle_rad;
    magic_api_struct->getpixel = magic_getpixel;
    magic_api_struct->putpixel = magic_putpixel;
    magic_api_struct->line = magic_line_func;
    magic_api_struct->playsound = magic_playsound;
    magic_api_struct->stopsound = magic_stopsound;
    magic_api_struct->special_notify = special_notify;
    magic_api_struct->button_down = magic_button_down;
    magic_api_struct->rgbtohsv = rgbtohsv;
    magic_api_struct->hsvtorgb = hsvtorgb;
    magic_api_struct->canvas_w = canvas->w;
    magic_api_struct->canvas_h = canvas->h;
    magic_api_struct->scale = magic_scale;
    magic_api_struct->touched = magic_touched;
    
    
    magic_funcs[0].get_tool_count = magic_command(alien,get_tool_count);
    magic_funcs[0].get_name = magic_command(alien,get_name);
    magic_funcs[0].get_icon =magic_command(alien,get_icon);
    magic_funcs[0].get_description =magic_command(alien,get_description);
    magic_funcs[0].requires_colors =magic_command(alien,requires_colors);
    magic_funcs[0].modes=magic_command(alien,modes);
    magic_funcs[0].set_color =magic_command(alien,set_color);
    magic_funcs[0].init =magic_command(alien,init);
    magic_funcs[0].api_version =magic_command(alien,api_version);
    magic_funcs[0].shutdown =magic_command(alien,shutdown);
    magic_funcs[0].click =magic_command(alien,click);
    magic_funcs[0].drag =magic_command(alien,drag);
    magic_funcs[0].release =magic_command(alien,release);
    magic_funcs[0].switchin =magic_command(alien,switchin);
    magic_funcs[0].switchout =magic_command(alien,switchout);
    
    magic_funcs[1].get_tool_count = magic_command(blocks_chalk_drip,get_tool_count);
    magic_funcs[1].get_name = magic_command(blocks_chalk_drip,get_name);
    magic_funcs[1].get_icon =magic_command(blocks_chalk_drip,get_icon);
    magic_funcs[1].get_description =magic_command(blocks_chalk_drip,get_description);
    magic_funcs[1].requires_colors =magic_command(blocks_chalk_drip,requires_colors);
    magic_funcs[1].modes=magic_command(blocks_chalk_drip,modes);
    magic_funcs[1].set_color =magic_command(blocks_chalk_drip,set_color);
    magic_funcs[1].init =magic_command(blocks_chalk_drip,init);
    magic_funcs[1].api_version =magic_command(blocks_chalk_drip,api_version);
    magic_funcs[1].shutdown =magic_command(blocks_chalk_drip,shutdown);
    magic_funcs[1].click =magic_command(blocks_chalk_drip,click);
    magic_funcs[1].drag =magic_command(blocks_chalk_drip,drag);
    magic_funcs[1].release =magic_command(blocks_chalk_drip,release);
    magic_funcs[1].switchin =magic_command(blocks_chalk_drip,switchin);
    magic_funcs[1].switchout =magic_command(blocks_chalk_drip,switchout);
    
    magic_funcs[2].get_tool_count = magic_command(blur,get_tool_count);
    magic_funcs[2].get_name = magic_command(blur,get_name);
    magic_funcs[2].get_icon =magic_command(blur,get_icon);
    magic_funcs[2].get_description =magic_command(blur,get_description);
    magic_funcs[2].requires_colors =magic_command(blur,requires_colors);
    magic_funcs[2].modes=magic_command(blur,modes);
    magic_funcs[2].set_color =magic_command(blur,set_color);
    magic_funcs[2].init =magic_command(blur,init);
    magic_funcs[2].api_version =magic_command(blur,api_version);
    magic_funcs[2].shutdown =magic_command(blur,shutdown);
    magic_funcs[2].click =magic_command(blur,click);
    magic_funcs[2].drag =magic_command(blur,drag);
    magic_funcs[2].release =magic_command(blur,release);
    magic_funcs[2].switchin =magic_command(blur,switchin);
    magic_funcs[2].switchout =magic_command(blur,switchout);
    
    magic_funcs[3].get_tool_count = magic_command(bricks,get_tool_count);
    magic_funcs[3].get_name = magic_command(bricks,get_name);
    magic_funcs[3].get_icon =magic_command(bricks,get_icon);
    magic_funcs[3].get_description =magic_command(bricks,get_description);
    magic_funcs[3].requires_colors =magic_command(bricks,requires_colors);
    magic_funcs[3].modes=magic_command(bricks,modes);
    magic_funcs[3].set_color =magic_command(bricks,set_color);
    magic_funcs[3].init =magic_command(bricks,init);
    magic_funcs[3].api_version =magic_command(bricks,api_version);
    magic_funcs[3].shutdown =magic_command(bricks,shutdown);
    magic_funcs[3].click =magic_command(bricks,click);
    magic_funcs[3].drag =magic_command(bricks,drag);
    magic_funcs[3].release =magic_command(bricks,release);
    magic_funcs[3].switchin =magic_command(bricks,switchin);
    magic_funcs[3].switchout =magic_command(bricks,switchout);
    
    magic_funcs[4].get_tool_count = magic_command(calligraphy,get_tool_count);
    magic_funcs[4].get_name = magic_command(calligraphy,get_name);
    magic_funcs[4].get_icon =magic_command(calligraphy,get_icon);
    magic_funcs[4].get_description =magic_command(calligraphy,get_description);
    magic_funcs[4].requires_colors =magic_command(calligraphy,requires_colors);
    magic_funcs[4].modes=magic_command(calligraphy,modes);
    magic_funcs[4].set_color =magic_command(calligraphy,set_color);
    magic_funcs[4].init =magic_command(calligraphy,init);
    magic_funcs[4].api_version =magic_command(calligraphy,api_version);
    magic_funcs[4].shutdown =magic_command(calligraphy,shutdown);
    magic_funcs[4].click =magic_command(calligraphy,click);
    magic_funcs[4].drag =magic_command(calligraphy,drag);
    magic_funcs[4].release =magic_command(calligraphy,release);
    magic_funcs[4].switchin =magic_command(calligraphy,switchin);
    magic_funcs[4].switchout =magic_command(calligraphy,switchout);
    
    magic_funcs[5].get_tool_count = magic_command(cartoon,get_tool_count);
    magic_funcs[5].get_name = magic_command(cartoon,get_name);
    magic_funcs[5].get_icon =magic_command(cartoon,get_icon);
    magic_funcs[5].get_description =magic_command(cartoon,get_description);
    magic_funcs[5].requires_colors =magic_command(cartoon,requires_colors);
    magic_funcs[5].modes=magic_command(cartoon,modes);
    magic_funcs[5].set_color =magic_command(cartoon,set_color);
    magic_funcs[5].init =magic_command(cartoon,init);
    magic_funcs[5].api_version =magic_command(cartoon,api_version);
    magic_funcs[5].shutdown =magic_command(cartoon,shutdown);
    magic_funcs[5].click =magic_command(cartoon,click);
    magic_funcs[5].drag =magic_command(cartoon,drag);
    magic_funcs[5].release =magic_command(cartoon,release);
    magic_funcs[5].switchin =magic_command(cartoon,switchin);
    magic_funcs[5].switchout =magic_command(cartoon,switchout);
    
    magic_funcs[6].get_tool_count = magic_command(confetti,get_tool_count);
    magic_funcs[6].get_name = magic_command(confetti,get_name);
    magic_funcs[6].get_icon =magic_command(confetti,get_icon);
    magic_funcs[6].get_description =magic_command(confetti,get_description);
    magic_funcs[6].requires_colors =magic_command(confetti,requires_colors);
    magic_funcs[6].modes=magic_command(confetti,modes);
    magic_funcs[6].set_color =magic_command(confetti,set_color);
    magic_funcs[6].init =magic_command(confetti,init);
    magic_funcs[6].api_version =magic_command(confetti,api_version);
    magic_funcs[6].shutdown =magic_command(confetti,shutdown);
    magic_funcs[6].click =magic_command(confetti,click);
    magic_funcs[6].drag =magic_command(confetti,drag);
    magic_funcs[6].release =magic_command(confetti,release);
    magic_funcs[6].switchin =magic_command(confetti,switchin);
    magic_funcs[6].switchout =magic_command(confetti,switchout);
    
    magic_funcs[7].get_tool_count = magic_command(distortion,get_tool_count);
    magic_funcs[7].get_name = magic_command(distortion,get_name);
    magic_funcs[7].get_icon =magic_command(distortion,get_icon);
    magic_funcs[7].get_description =magic_command(distortion,get_description);
    magic_funcs[7].requires_colors =magic_command(distortion,requires_colors);
    magic_funcs[7].modes=magic_command(distortion,modes);
    magic_funcs[7].set_color =magic_command(distortion,set_color);
    magic_funcs[7].init =magic_command(distortion,init);
    magic_funcs[7].api_version =magic_command(distortion,api_version);
    magic_funcs[7].shutdown =magic_command(distortion,shutdown);
    magic_funcs[7].click =magic_command(distortion,click);
    magic_funcs[7].drag =magic_command(distortion,drag);
    magic_funcs[7].release =magic_command(distortion,release);
    magic_funcs[7].switchin =magic_command(distortion,switchin);
    magic_funcs[7].switchout =magic_command(distortion,switchout);
    
    magic_funcs[8].get_tool_count = magic_command(emboss,get_tool_count);
    magic_funcs[8].get_name = magic_command(emboss,get_name);
    magic_funcs[8].get_icon =magic_command(emboss,get_icon);
    magic_funcs[8].get_description =magic_command(emboss,get_description);
    magic_funcs[8].requires_colors =magic_command(emboss,requires_colors);
    magic_funcs[8].modes=magic_command(emboss,modes);
    magic_funcs[8].set_color =magic_command(emboss,set_color);
    magic_funcs[8].init =magic_command(emboss,init);
    magic_funcs[8].api_version =magic_command(emboss,api_version);
    magic_funcs[8].shutdown =magic_command(emboss,shutdown);
    magic_funcs[8].click =magic_command(emboss,click);
    magic_funcs[8].drag =magic_command(emboss,drag);
    magic_funcs[8].release =magic_command(emboss,release);
    magic_funcs[8].switchin =magic_command(emboss,switchin);
    magic_funcs[8].switchout =magic_command(emboss,switchout);
    
    magic_funcs[9].get_tool_count = magic_command(fade_darken,get_tool_count);
    magic_funcs[9].get_name = magic_command(fade_darken,get_name);
    magic_funcs[9].get_icon =magic_command(fade_darken,get_icon);
    magic_funcs[9].get_description =magic_command(fade_darken,get_description);
    magic_funcs[9].requires_colors =magic_command(fade_darken,requires_colors);
    magic_funcs[9].modes=magic_command(fade_darken,modes);
    magic_funcs[9].set_color =magic_command(fade_darken,set_color);
    magic_funcs[9].init =magic_command(fade_darken,init);
    magic_funcs[9].api_version =magic_command(fade_darken,api_version);
    magic_funcs[9].shutdown =magic_command(fade_darken,shutdown);
    magic_funcs[9].click =magic_command(fade_darken,click);
    magic_funcs[9].drag =magic_command(fade_darken,drag);
    magic_funcs[9].release =magic_command(fade_darken,release);
    magic_funcs[9].switchin =magic_command(fade_darken,switchin);
    magic_funcs[9].switchout =magic_command(fade_darken,switchout);
    //            do
    //            {
    //                f = readdir(d);
    //
    //                if (f != NULL)
    //                {
    //                    struct stat sbuf;
    //
    //                    snprintf(fname, sizeof(fname), "%s%s", place, f->d_name);
    //                    if (!stat(fname, &sbuf) && S_ISREG(sbuf.st_mode))
    //                    {
    //                        /* Get just the name of the object (e.g., "negative"), w/o filename
    //                         extension: */
    //
    //                        strcpy(objname, f->d_name);
    //                        strcpy(strchr(objname, '.'), "");
    //
    //
    //                        magic_handle[num_plugin_files] = SDL_LoadObject(fname);
    //
    //                        if (magic_handle[num_plugin_files] != NULL)
    for (int ii = 0; ii < 10; ii++)
    {
#ifdef DEBUG
        printf("loading: %s\n", fname);
        fflush(stdout);
#endif
        
        
        
        
        err = 0;
        
        if (magic_funcs[num_plugin_files].get_tool_count == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing get_tool_count\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].get_name == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing get_name\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].get_icon == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing get_icon\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].get_description == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing get_description\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].requires_colors == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing requires_colors\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].modes == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing modes\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].set_color == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing set_color\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].init == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing init\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].shutdown == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing shutdown\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].click == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing click\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].release == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing release\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].switchin == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing switchin\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].switchout == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing switchout\n",
                    fname);
            err = 1;
        }
        if (magic_funcs[num_plugin_files].drag == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing drag\n",
                    fname);
            err = 1;
        }
        
        if (magic_funcs[num_plugin_files].api_version == NULL)
        {
            fprintf(stderr, "Error: plugin %s is missing api_version\n",
                    fname);
            err = 1;
        }
        else if (magic_funcs[num_plugin_files].api_version() != TP_MAGIC_API_VERSION)
        {
            fprintf(stderr, "Warning: plugin %s uses Tux Paint 'Magic' tool API version %x,\nbut Tux Paint needs version %x.\n", fname, magic_funcs[num_plugin_files].api_version(), TP_MAGIC_API_VERSION);
            err = 1;
        }
        
        if (err)
        {
            SDL_UnloadObject(magic_handle[num_plugin_files]);
        }
        else
        {
            res = magic_funcs[num_plugin_files].init(magic_api_struct);
            
            if (res != 0)
                n = magic_funcs[num_plugin_files].get_tool_count(magic_api_struct);
            else
            {
                magic_funcs[num_plugin_files].shutdown(magic_api_struct);
                n = 0;
            }
            
            
            if (n == 0)
            {
                fprintf(stderr, "Error: plugin %s failed to startup or reported 0 magic tools\n", fname);
                fflush(stderr);
                SDL_UnloadObject(magic_handle[num_plugin_files]);
            }
            else
            {
                int j, bit;
                
                for (i = 0; i < n; i++)
                {
                    magics[num_magics].idx = i;
                    magics[num_magics].place = plc;
                    magics[num_magics].handle_idx = num_plugin_files;
                    magics[num_magics].name = magic_funcs[num_plugin_files].get_name(magic_api_struct, i);
                    
                    magics[num_magics].avail_modes = magic_funcs[num_plugin_files].modes(magic_api_struct, i);
                    
                    bit = 1;
                    for (j = 0; j < MAX_MODES; j++)
                    {
                        if (magics[num_magics].avail_modes & bit)
                            magics[num_magics].tip[j] = magic_funcs[num_plugin_files].get_description(magic_api_struct, i, bit);
                        else
                            magics[num_magics].tip[j] = NULL;
                        bit *= 2;
                    }
                    
                    magics[num_magics].colors = magic_funcs[num_plugin_files].requires_colors(magic_api_struct, i);
                    if (magics[num_magics].avail_modes & MODE_PAINT)
                        magics[num_magics].mode = MODE_PAINT;
                    else
                        magics[num_magics].mode = MODE_FULLSCREEN;
                    
                    magics[num_magics].img_icon = magic_funcs[num_plugin_files].get_icon(magic_api_struct, i);
                    
#ifdef DEBUG
                    printf("-- %s\n", magics[num_magics].name);
                    printf("avail_modes = %d\n", magics[num_magics].avail_modes);
#endif
                    
                    num_magics++;
                }
                
                num_plugin_files++;
            }
        }
    }
    //                        else
    //                        {
    //                            fprintf(stderr, "Warning: Failed to load object %s: %s\n", fname, SDL_GetError());
    //                            fflush(stderr);
    //                        }
    //                    }
    //                }
    //            }
    //            while (f != NULL);
    //
    //            closedir(d);
    //        }
    //    }
    
    
    qsort(magics, num_magics, sizeof(magic_t), magic_sort);
    
#ifdef DEBUG
    printf("Loaded %d magic tools from %d plug-in files\n", num_magics,
           num_plugin_files);
    printf("\n");
    fflush(stdout);
#endif
}



int magic_sort(const void * a, const void * b)
{
    magic_t * am = (magic_t *) a;
    magic_t * bm = (magic_t *) b;
    
    return(strcmp(am->name, bm->name));
}


void update_progress_bar(void)
{
    show_progress_bar(screen);
}

void magic_line_func(void * mapi,
                     int which, SDL_Surface * canvas, SDL_Surface * last,
                     int x1, int y1, int x2, int y2, int step,
                     void (*cb)(void *, int, SDL_Surface *, SDL_Surface *,
                                int, int))
{
    int dx, dy, y;
    float m, b;
    int cnt;
    
    dx = x2 - x1;
    dy = y2 - y1;
    
    cnt = step - 1;
    
    if (dx != 0)
    {
        m = ((float) dy) / ((float) dx);
        b = y1 - m * x1;
        
        if (x2 >= x1)
            dx = 1;
        else
            dx = -1;
        
        
        while (x1 != x2)
        {
            y1 = m * x1 + b;
            y2 = m * (x1 + dx) + b;
            
            if (y1 > y2)
            {
                for (y = y1; y >= y2; y--)
                {
                    cnt = (cnt + 1) % step;
                    if (cnt == 0)
                        cb((void *) mapi, which, canvas, last, x1, y);
                }
            }
            else
            {
                for (y = y1; y <= y2; y++)
                {
                    cnt = (cnt + 1) % step;
                    if (cnt == 0)
                        cb((void *) mapi, which, canvas, last, x1, y);
                }
            }
            
            x1 = x1 + dx;
        }
    }
    else
    {
        if (y1 > y2)
        {
            for (y = y1; y >= y2; y--)
            {
                cnt = (cnt + 1) % step;
                if (cnt == 0)
                    cb((void *) mapi, which, canvas, last, x1, y);
            }
        }
        else
        {
            for (y = y1; y <= y2; y++)
            {
                cnt = (cnt + 1) % step;
                if (cnt == 0)
                    cb((void *) mapi, which, canvas, last, x1, y);
            }
        }
    }
    
    /* FIXME: Set and return an update rect? */
}


/* Handle special things that some magic tools do that
 need to affect more than just the current canvas: */

void special_notify(int flags)
{
    int tmp_int;
    
    tmp_int = cur_undo - 1;
    if (tmp_int < 0) tmp_int = NUM_UNDO_BUFS - 1;
    
    if (flags & SPECIAL_MIRROR)
    {
        /* Mirror starter, too! */
        
        starter_mirrored = !starter_mirrored;
        
        if (img_starter != NULL)
            mirror_starter();
        
        undo_starters[tmp_int] = UNDO_STARTER_MIRRORED;
    }
    
    if (flags & SPECIAL_FLIP)
    {
        /* Flip starter, too! */
        
        starter_flipped = !starter_flipped;
        
        if (img_starter != NULL)
            flip_starter();
        
        undo_starters[tmp_int] = UNDO_STARTER_FLIPPED;
    }
}

void magic_stopsound(void)
{
#ifndef NOSOUND
    if (mute || !use_sound)
        return;
    
    Mix_HaltChannel(0);
#endif
}

void magic_playsound(Mix_Chunk * snd, int left_right, int up_down)
{
#ifndef NOSOUND
    
    int left, dist;
    
    
    /* Don't play if sound is disabled (nosound), or sound is temporarily
     muted (Alt+S), or sound ptr is NULL */
    
    if (mute || !use_sound || snd == NULL)
        return;
    
    
    /* Don't override the same sound, if it's already playing */
    
    if (!Mix_Playing(0) || magic_current_snd_ptr != snd)
        Mix_PlayChannel(0, snd, 0);
    
    magic_current_snd_ptr = snd;
    
    
    /* Adjust panning */
    
    if (up_down < 0)
        up_down = 0;
    else if (up_down > 255)
        up_down = 255;
    
    dist = 255 - up_down;
    
    if (left_right < 0)
        left_right = 0;
    else if (left_right > 255)
        left_right = 255;
    
    left = ((255 - dist) * (255 - left_right)) / 255;
    
    Mix_SetPanning(0, left, (255 - dist) - left);
#endif
}

Uint8 magic_linear_to_sRGB(float lin)
{
    return(linear_to_sRGB(lin));
}

float magic_sRGB_to_linear(Uint8 srgb)
{
    return(sRGB_to_linear_table[srgb]);
}

int magic_button_down(void)
{
    return(button_down);
}

SDL_Surface * magic_scale(SDL_Surface * surf, int w, int h, int aspect)
{
    return(thumbnail2(surf, w, h, aspect, 1));
}

/* FIXME: This, do_open() and do_slideshow() should be combined and modularized! */

int do_new_dialog(void)
{
#ifdef __IPHONEOS__
    extern const char* DATA_PREFIX;
#endif
    
    SDL_Surface *img, *img1, *img2;
    int things_alloced;
    SDL_Surface **thumbs = NULL;
    DIR *d;
    struct dirent *f;
    struct dirent2 *fs;
    int place;
    char *dirname[NUM_PLACES_TO_LOOK];
    char **d_names = NULL, **d_exts = NULL;
    int *d_places;
    FILE *fi;
    char fname[1024];
    int num_files, i, done, update_list, cur, which,
    num_files_in_dirs, j;
    SDL_Rect dest;
    SDL_Event event;
    SDLKey key;
    Uint32 last_click_time;
    int last_click_which, last_click_button;
    int places_to_look;
    int tot;
    int first_starter;
    int added;
    Uint8 r, g, b;
    int white_in_palette;
    
    
    do_setcursor(cursor_watch);
    
    /* Allocate some space: */
    
    things_alloced = 32;
    
    fs = (struct dirent2 *) malloc(sizeof(struct dirent2) * things_alloced);
    
    num_files = 0;
    cur = 0;
    which = 0;
    num_files_in_dirs = 0;
    
    
    /* Open directories of images: */
    
    for (places_to_look = 0;
         places_to_look < NUM_PLACES_TO_LOOK; places_to_look++)
    {
        if (places_to_look == PLACE_SAVED_DIR)
        {
            /* Skip saved images; only want starters! */
            dirname[places_to_look] = NULL;
            continue; /* ugh */
        }
        else if (places_to_look == PLACE_PERSONAL_STARTERS_DIR)
        {
            /* Check for coloring-book style 'starter' images in our folder: */
            
            dirname[places_to_look] = get_fname("starters", DIR_DATA);
        }
        else if (places_to_look == PLACE_STARTERS_DIR)
        {
            /* Finally, check for system-wide coloring-book style
             'starter' images: */
            char temp[1024];
            sprintf(temp, "%s/%s", DATA_PREFIX, "starters");
            
            dirname[places_to_look] = strdup(temp);
        }
        
        
        /* Read directory of images and build thumbnails: */
        
        d = opendir(dirname[places_to_look]);
        
        if (d != NULL)
        {
            /* Gather list of files (for sorting): */
            
            do
            {
                f = readdir(d);
                
                if (f != NULL)
                {
                    memcpy(&(fs[num_files_in_dirs].f), f, sizeof(struct dirent));
                    fs[num_files_in_dirs].place = places_to_look;
                    
                    num_files_in_dirs++;
                    
                    if (num_files_in_dirs >= things_alloced)
                    {
                        things_alloced = things_alloced + 32;
                        
                        fs = (struct dirent2 *) realloc(fs,
                                                        sizeof(struct dirent2) *
                                                        things_alloced);
                    }
                }
            }
            while (f != NULL);
            
            closedir(d);
        }
    }
    
    
    /* (Re)allocate space for the information about these files: */
    
    tot = num_files_in_dirs + NUM_COLORS;
    
    thumbs = (SDL_Surface * *)malloc(sizeof(SDL_Surface *) * tot);
    d_places = (int *) malloc(sizeof(int) * tot);
    d_names = (char **) malloc(sizeof(char *) * tot);
    d_exts = (char **) malloc(sizeof(char *) * tot);
    
    
    /* Sort: */
    
    qsort(fs, num_files_in_dirs, sizeof(struct dirent2),
          (int (*)(const void *, const void *)) compare_dirent2s);
    
    
    /* Throw the color palette at the beginning: */
    
    white_in_palette = -1;
    
    for (j = -1; j < NUM_COLORS; j++)
    {
        added = 0;
        
        if (j < NUM_COLORS - 1)
        {
            if (j == -1 || /* (short circuit) */
                color_hexes[j][0] != 255 || /* Ignore white, we'll have already added it */
                color_hexes[j][1] != 255 ||
                color_hexes[j][2] != 255)
            {
                /* Palette colors: */
                
                thumbs[num_files] = SDL_CreateRGBSurface(screen->flags,
                                                         THUMB_W - 20, THUMB_H - 20,
                                                         screen->format->BitsPerPixel,
                                                         screen->format->Rmask,
                                                         screen->format->Gmask,
                                                         screen->format->Bmask, 0);
                
                if (thumbs[num_files] != NULL)
                {
                    if (j == -1)
                    {
                        r = g = b = 255; /* White */
                    }
                    else
                    {
                        r = color_hexes[j][0];
                        g = color_hexes[j][1];
                        b = color_hexes[j][2];
                    }
                    SDL_FillRect(thumbs[num_files], NULL,
                                 SDL_MapRGB(thumbs[num_files]->format, r, g, b));
                    added = 1;
                }
            }
            else
            {
                white_in_palette = j;
            }
        }
        else
        {
            /* Color picker: */
            
            thumbs[num_files] = thumbnail(img_color_picker, THUMB_W - 20, THUMB_H - 20, 0);
            added = 1;
        }
        
        if (added)
        {
            d_places[num_files] = PLACE_COLOR_PALETTE;
            d_names[num_files] = NULL;
            d_exts[num_files] = NULL;
            
            num_files++;
        }
    }
    
    first_starter = num_files;
    
    
    /* Read directory of images and build thumbnails: */
    
    for (j = 0; j < num_files_in_dirs; j++)
    {
        f = &(fs[j].f);
        place = fs[j].place;
        
        show_progress_bar(screen);
        
        if (f != NULL)
        {
            debug(f->d_name);
            
            if (strcasestr(f->d_name, "-t.") == NULL &&
                strcasestr(f->d_name, "-back.") == NULL)
            {
                if (strcasestr(f->d_name, FNAME_EXTENSION) != NULL
                    /* Support legacy BMP files for load: */
                    || strcasestr(f->d_name, ".bmp") != NULL
                    )
                {
                    strcpy(fname, f->d_name);
                    if (strcasestr(fname, FNAME_EXTENSION) != NULL)
                    {
                        strcpy((char *) strcasestr(fname, FNAME_EXTENSION), "");
                        d_exts[num_files] = strdup(FNAME_EXTENSION);
                    }
                    
                    if (strcasestr(fname, ".bmp") != NULL)
                    {
                        strcpy((char *) strcasestr(fname, ".bmp"), "");
                        d_exts[num_files] = strdup(".bmp");
                    }
                    
                    d_names[num_files] = strdup(fname);
                    d_places[num_files] = place;
                    
                    
                    /* Try to load thumbnail first: */
                    
                    snprintf(fname, sizeof(fname), "%s/.thumbs/%s-t.bmp",
                             dirname[d_places[num_files]], d_names[num_files]);
                    debug(fname);
                    img = IMG_Load(fname);
                    
                    if (img == NULL)
                    {
                        /* No thumbnail in the new location ("saved/.thumbs"),
                         try the old locatin ("saved/"): */
                        
                        snprintf(fname, sizeof(fname), "%s/%s-t.bmp",
                                 dirname[d_places[num_files]], d_names[num_files]);
                        debug(fname);
                        
                        img = IMG_Load(fname);
                    }
                    
                    if (img != NULL)
                    {
                        /* Loaded the thumbnail from one or the other location */
                        show_progress_bar(screen);
                        
                        img1 = SDL_DisplayFormat(img);
                        SDL_FreeSurface(img);
                        
                        /* if too big, or too small in both dimensions, rescale it
                         (for now: using old thumbnail as source for high speed,
                         low quality) */
                        if (img1->w > THUMB_W - 20 || img1->h > THUMB_H - 20
                            || (img1->w < THUMB_W - 20 && img1->h < THUMB_H - 20))
                        {
                            img2 = thumbnail(img1, THUMB_W - 20, THUMB_H - 20, 0);
                            SDL_FreeSurface(img1);
                            img1 = img2;
                        }
                        
                        thumbs[num_files] = img1;
                        
                        if (thumbs[num_files] == NULL)
                        {
                            fprintf(stderr,
                                    "\nError: Couldn't create a thumbnail of "
                                    "saved image!\n" "%s\n", fname);
                        }
                        
                        num_files++;
                    }
                    else
                    {
                        /* No thumbnail - load original: */
                        
                        /* Make sure we have a ~/.tuxpaint/saved directory: */
                        if (make_directory("saved", "Can't create user data directory"))
                        {
                            /* (Make sure we have a .../saved/.thumbs/ directory:) */
                            make_directory("saved/.thumbs", "Can't create user data thumbnail directory");
                        }
                        
                        img = NULL;
                        
                        if (d_places[num_files] == PLACE_STARTERS_DIR ||
                            d_places[num_files] == PLACE_PERSONAL_STARTERS_DIR)
                        {
                            /* Try to load a starter's background image, first!
                             If it exists, it should give a better idea of what the
                             starter looks like, compared to the overlay image... */
                            
                            /* FIXME: Add .jpg support -bjk 2007.03.22 */
                            
                            /* (Try JPEG first) */
                            snprintf(fname, sizeof(fname), "%s/%s-back.jpeg",
                                     dirname[d_places[num_files]], d_names[num_files]);
                            
                            img = IMG_Load(fname);
                            
                            
                            if (img == NULL)
                            {
                                /* (Try PNG next) */
                                snprintf(fname, sizeof(fname), "%s/%s-back.png",
                                         dirname[d_places[num_files]], d_names[num_files]);
                                
                                img = IMG_Load(fname);
                            }
                            
#ifndef NOSVG
                            if (img == NULL)
                            {
                                /* (Try SVG next) */
                                snprintf(fname, sizeof(fname), "%s/%s-back.svg",
                                         dirname[d_places[num_files]], d_names[num_files]);
                                
                                img = load_svg(fname);
                            }
#endif
                        }
                        
                        
                        if (img == NULL)
                        {
                            /* Didn't load a starter background (or didn't try!),
                             try loading the actual image... */
                            
                            snprintf(fname, sizeof(fname), "%s/%s",
                                     dirname[d_places[num_files]], f->d_name);
                            debug(fname);
                            img = myIMG_Load(fname);
                        }
                        
                        
                        show_progress_bar(screen);
                        
                        if (img == NULL)
                        {
                            fprintf(stderr,
                                    "\nWarning: I can't open one of the saved files!\n"
                                    "%s\n"
                                    "The Simple DirectMedia Layer error that "
                                    "occurred was:\n" "%s\n\n", fname, SDL_GetError());
                            
                            free(d_names[num_files]);
                            free(d_exts[num_files]);
                        }
                        else
                        {
                            /* Turn it into a thumbnail: */
                            
                            img1 = SDL_DisplayFormatAlpha(img);
                            img2 = thumbnail2(img1, THUMB_W - 20, THUMB_H - 20, 0, 0);
                            SDL_FreeSurface(img1);
                            
                            show_progress_bar(screen);
                            
                            thumbs[num_files] = SDL_DisplayFormat(img2);
                            SDL_FreeSurface(img2);
                            if (thumbs[num_files] == NULL)
                            {
                                fprintf(stderr,
                                        "\nError: Couldn't create a thumbnail of "
                                        "saved image!\n" "%s\n", fname);
                            }
                            
                            SDL_FreeSurface(img);
                            
                            show_progress_bar(screen);
                            
                            
                            /* Let's save this thumbnail, so we don't have to
                             create it again next time 'Open' is called: */
                            
                            if (d_places[num_files] == PLACE_SAVED_DIR)
                            {
                                debug("Saving thumbnail for this one!");
                                
                                snprintf(fname, sizeof(fname), "%s/.thumbs/%s-t.bmp",
                                         dirname[d_places[num_files]], d_names[num_files]);
                                
                                fi = fopen(fname, "wb");
                                if (fi == NULL)
                                {
                                    fprintf(stderr,
                                            "\nError: Couldn't save thumbnail of "
                                            "saved image!\n"
                                            "%s\n"
                                            "The error that occurred was:\n"
                                            "%s\n\n", fname, strerror(errno));
                                }
                                else
                                {
                                    do_png_save(fi, fname, thumbs[num_files]);
                                }
                                
                                show_progress_bar(screen);
                            }
                            
                            
                            num_files++;
                        }
                    }
                }
            }
            else
            {
                /* It was a thumbnail file ("...-t.png") or immutable scene starter's
                 overlay layer ("...-front.png") */
            }
        }
    }
    
    
    
#ifdef DEBUG
    printf("%d files were found!\n", num_files);
#endif
    
    
    /* Let user choose a color or image: */
    
    /* Instructions for 'New' file/color dialog */
    draw_tux_text(TUX_BORED, tool_tips[TOOL_NEW], 1);
    
    /* NOTE: cur is now set above; if file_id'th file is found, it's
     set to that file's index; otherwise, we default to '0' */
    
    update_list = 1;
    
    done = 0;
    
    last_click_which = -1;
    last_click_time = 0;
    last_click_button = -1;
    
    
    do_setcursor(cursor_arrow);
    
    
    do
    {
        /* Update screen: */
        
        if (update_list)
        {
            /* Erase screen: */
            
            dest.x = 96;
            dest.y = 0;
            dest.w = WINDOW_WIDTH - 96 - 96;
            dest.h = 48 * 7 + 40 + HEIGHTOFFSET;
            
            SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format,
                                                   255, 255, 255));
            
            
            /* Draw icons: */
            
            for (i = cur; i < cur + 16 && i < num_files; i++)
            {
                /* Draw cursor: */
                
                dest.x = THUMB_W * ((i - cur) % 4) + 96;
                dest.y = THUMB_H * ((i - cur) / 4) + 24;
                
                if (d_places[i] == PLACE_SAVED_DIR)
                {
                    if (i == which)
                    {
                        SDL_BlitSurface(img_cursor_down, NULL, screen, &dest);
                        debug(d_names[i]);
                    }
                    else
                        SDL_BlitSurface(img_cursor_up, NULL, screen, &dest);
                }
                else
                {
                    if (i == which)
                    {
                        SDL_BlitSurface(img_cursor_starter_down, NULL, screen, &dest);
                        debug(d_names[i]);
                    }
                    else
                        SDL_BlitSurface(img_cursor_starter_up, NULL, screen, &dest);
                }
                
                
                
                dest.x = THUMB_W * ((i - cur) % 4) + 96 + 10 +
                (THUMB_W - 20 - thumbs[i]->w) / 2;
                dest.y = THUMB_H * ((i - cur) / 4) + 24 + 10 +
                (THUMB_H - 20 - thumbs[i]->h) / 2;
                
                if (thumbs[i] != NULL)
                    SDL_BlitSurface(thumbs[i], NULL, screen, &dest);
            }
            
            
            /* Draw arrows: */
            
            dest.x = (WINDOW_WIDTH - img_scroll_up->w) / 2;
            dest.y = 0;
            
            if (cur > 0)
                SDL_BlitSurface(img_scroll_up, NULL, screen, &dest);
            else
                SDL_BlitSurface(img_scroll_up_off, NULL, screen, &dest);
            
            dest.x = (WINDOW_WIDTH - img_scroll_up->w) / 2;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
            
            if (cur < num_files - 16)
                SDL_BlitSurface(img_scroll_down, NULL, screen, &dest);
            else
                SDL_BlitSurface(img_scroll_down_off, NULL, screen, &dest);
            
            
            /* "Open" button: */
            
            dest.x = 96;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
            SDL_BlitSurface(img_open, NULL, screen, &dest);
            
            dest.x = 96 + (48 - img_openlabels_open->w) / 2;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_open->h;
            SDL_BlitSurface(img_openlabels_open, NULL, screen, &dest);
            
            
            /* "Back" button: */
            
            dest.x = WINDOW_WIDTH - 96 - 48;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - 48;
            SDL_BlitSurface(img_back, NULL, screen, &dest);
            
            dest.x = WINDOW_WIDTH - 96 - 48 + (48 - img_openlabels_back->w) / 2;
            dest.y = (48 * 7 + 40 + HEIGHTOFFSET) - img_openlabels_back->h;
            SDL_BlitSurface(img_openlabels_back, NULL, screen, &dest);
            
            
            SDL_Flip(screen);
            
            update_list = 0;
        }
        
        
        SDL_WaitEvent(&event);
        
        if (event.type == SDL_QUIT)
        {
            done = 1;
            
            /* FIXME: Handle SDL_Quit better */
        }
        else if (event.type == SDL_ACTIVEEVENT)
        {
            handle_active(&event);
        }
        else if (event.type == SDL_KEYUP)
        {
            key = event.key.keysym.sym;
            
            handle_keymouse(key, SDL_KEYUP);
        }
        else if (event.type == SDL_KEYDOWN)
        {
            key = event.key.keysym.sym;
            
            handle_keymouse(key, SDL_KEYDOWN);
            
            if (key == SDLK_LEFT)
            {
                if (which > 0)
                {
                    which--;
                    
                    if (which < cur)
                        cur = cur - 4;
                    
                    update_list = 1;
                }
            }
            else if (key == SDLK_RIGHT)
            {
                if (which < num_files - 1)
                {
                    which++;
                    
                    if (which >= cur + 16)
                        cur = cur + 4;
                    
                    update_list = 1;
                }
            }
            else if (key == SDLK_UP)
            {
                if (which >= 0)
                {
                    which = which - 4;
                    
                    if (which < 0)
                        which = 0;
                    
                    if (which < cur)
                        cur = cur - 4;
                    
                    update_list = 1;
                }
            }
            else if (key == SDLK_DOWN)
            {
                if (which < num_files)
                {
                    which = which + 4;
                    
                    if (which >= num_files)
                        which = num_files - 1;
                    
                    if (which >= cur + 16)
                        cur = cur + 4;
                    
                    update_list = 1;
                }
            }
            else if (key == SDLK_RETURN || key == SDLK_SPACE)
            {
                /* Open */
                
                done = 1;
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
            }
            else if (key == SDLK_ESCAPE)
            {
                /* Go back: */
                
                which = -1;
                done = 1;
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN &&
                 valid_click(event.button.button))
        {
            if (event.button.x >= 96 && event.button.x < WINDOW_WIDTH - 96 &&
                event.button.y >= 24 &&
                event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 48))
            {
                /* Picked an icon! */
                
                which = ((event.button.x - 96) / (THUMB_W) +
                         (((event.button.y - 24) / THUMB_H) * 4)) + cur;
                
                if (which < num_files)
                {
                    playsound(screen, 1, SND_BLEEP, 1, event.button.x, SNDDIST_NEAR);
                    update_list = 1;
                    
                    
                    if (which == last_click_which &&
                        SDL_GetTicks() < last_click_time + 1000 &&
                        event.button.button == last_click_button)
                    {
                        /* Double-click! */
                        
                        done = 1;
                    }
                    
                    last_click_which = which;
                    last_click_time = SDL_GetTicks();
                    last_click_button = event.button.button;
                }
            }
            else if (event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                     event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2)
            {
                if (event.button.y < 24)
                {
                    /* Up scroll button: */
                    
                    if (cur > 0)
                    {
                        cur = cur - 4;
                        update_list = 1;
                        playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER,
                                  SNDDIST_NEAR);
                        
                        if (cur == 0)
                            do_setcursor(cursor_arrow);
                    }
                    
                    if (which >= cur + 16)
                        which = which - 4;
                }
                else if (event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET - 48) &&
                         event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 24))
                {
                    /* Down scroll button: */
                    
                    if (cur < num_files - 16)
                    {
                        cur = cur + 4;
                        update_list = 1;
                        playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER,
                                  SNDDIST_NEAR);
                        
                        if (cur >= num_files - 16)
                            do_setcursor(cursor_arrow);
                    }
                    
                    if (which < cur)
                        which = which + 4;
                }
            }
            else if (event.button.x >= 96 && event.button.x < 96 + 48 &&
                     event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
            {
                /* Open */
                
                done = 1;
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_LEFT, SNDDIST_NEAR);
            }
            else if (event.button.x >= (WINDOW_WIDTH - 96 - 48) &&
                     event.button.x < (WINDOW_WIDTH - 96) &&
                     event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
            {
                /* Back */
                
                which = -1;
                done = 1;
                playsound(screen, 1, SND_CLICK, 1, SNDPOS_RIGHT, SNDDIST_NEAR);
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN &&
                 event.button.button >= 4 && event.button.button <= 5 && wheely)
        {
            /* Scroll wheel! */
            
            if (event.button.button == 4 && cur > 0)
            {
                cur = cur - 4;
                update_list = 1;
                playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                
                if (cur == 0)
                    do_setcursor(cursor_arrow);
                
                if (which >= cur + 16)
                    which = which - 4;
            }
            else if (event.button.button == 5 && cur < num_files - 16)
            {
                cur = cur + 4;
                update_list = 1;
                playsound(screen, 1, SND_SCROLL, 1, SNDPOS_CENTER, SNDDIST_NEAR);
                
                if (cur >= num_files - 16)
                    do_setcursor(cursor_arrow);
                
                if (which < cur)
                    which = which + 4;
            }
        }
        else if (event.type == SDL_MOUSEMOTION)
        {
            /* Deal with mouse pointer shape! */
            
            if (event.button.y < 24 &&
                event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2 &&
                cur > 0)
            {
                /* Scroll up button: */
                
                do_setcursor(cursor_up);
            }
            else if (event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET - 48) &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET - 24) &&
                     event.button.x >= (WINDOW_WIDTH - img_scroll_up->w) / 2 &&
                     event.button.x <= (WINDOW_WIDTH + img_scroll_up->w) / 2 &&
                     cur < num_files - 16)
            {
                /* Scroll down button: */
                
                do_setcursor(cursor_down);
            }
            else if (((event.button.x >= 96 && event.button.x < 96 + 48 + 48) ||
                      (event.button.x >= (WINDOW_WIDTH - 96 - 48) &&
                       event.button.x < (WINDOW_WIDTH - 96)) ||
                      (event.button.x >= (WINDOW_WIDTH - 96 - 48 - 48) &&
                       event.button.x < (WINDOW_WIDTH - 48 - 96) &&
                       d_places[which] != PLACE_STARTERS_DIR &&
                       d_places[which] != PLACE_PERSONAL_STARTERS_DIR)) &&
                     event.button.y >= (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET))
            {
                /* One of the command buttons: */
                
                do_setcursor(cursor_hand);
            }
            else if (event.button.x >= 96 && event.button.x < WINDOW_WIDTH - 96 &&
                     event.button.y > 24 &&
                     event.button.y < (48 * 7 + 40 + HEIGHTOFFSET) - 48 &&
                     ((((event.button.x - 96) / (THUMB_W) +
                        (((event.button.y - 24) / THUMB_H) * 4)) +
                       cur) < num_files))
            {
                /* One of the thumbnails: */
                
                do_setcursor(cursor_hand);
            }
            else
            {
                /* Unclickable... */
                
                do_setcursor(cursor_arrow);
            }
        }
    }
    while (!done);
    
    
    /* Load the chosen starter, or start with a blank solid color: */
    
    if (which != -1)
    {
        /* Save old one first? */
        
        if (!been_saved && !disable_save)
        {
            if (do_prompt_image_snd(PROMPT_OPEN_SAVE_TXT,
                                    PROMPT_OPEN_SAVE_YES,
                                    PROMPT_OPEN_SAVE_NO,
                                    img_tools[TOOL_SAVE], NULL, NULL,
                                    SND_AREYOUSURE,
                                    screen->w / 2, screen->h / 2))
            {
                do_save(TOOL_NEW, 1);
            }
        }
        
        
        if (which >= first_starter)
        {
            /* Load a starter: */
            
            /* Figure out filename: */
            
            snprintf(fname, sizeof(fname), "%s/%s%s",
                     dirname[d_places[which]], d_names[which], d_exts[which]);
            
            img = myIMG_Load(fname);
            
            if (img == NULL)
            {
                fprintf(stderr,
                        "\nWarning: Couldn't load the saved image!\n"
                        "%s\n"
                        "The Simple DirectMedia Layer error that occurred "
                        "was:\n" "%s\n\n", fname, SDL_GetError());
                
                do_prompt(PROMPT_OPEN_UNOPENABLE_TXT,
                          PROMPT_OPEN_UNOPENABLE_YES, "", 0 ,0);
            }
            else
            {
                free_surface(&img_starter);
                free_surface(&img_starter_bkgd);
                starter_mirrored = 0;
                starter_flipped = 0;
                starter_personal = 0;
                
                autoscale_copy_smear_free(img, canvas, SDL_BlitSurface);
                
                cur_undo = 0;
                oldest_undo = 0;
                newest_undo = 0;
                
                /* Immutable 'starter' image;
                 we'll need to save a new image when saving...: */
                
                been_saved = 1;
                
                file_id[0] = '\0';
                strcpy(starter_id, d_names[which]);
                
                if (d_places[which] == PLACE_PERSONAL_STARTERS_DIR)
                    starter_personal = 1;
                else
                    starter_personal = 0;
                
                load_starter(starter_id);
                
                canvas_color_r = 255;
                canvas_color_g = 255;
                canvas_color_b = 255;
                
                SDL_FillRect(canvas, NULL,
                             SDL_MapRGB(canvas->format, 255, 255, 255));
                SDL_BlitSurface(img_starter_bkgd, NULL, canvas, NULL);
                SDL_BlitSurface(img_starter, NULL, canvas, NULL);
            }
        }
        else
        {
            /* A color! */
            
            free_surface(&img_starter);
            free_surface(&img_starter_bkgd);
            starter_mirrored = 0;
            starter_flipped = 0;
            starter_personal = 0;
            
            
            /* Launch color picker if they chose that: */
            
            if (which == NUM_COLORS - 1)
            {
                if (do_color_picker() == 0)
                    return(0);
            }
            
            /* FIXME: Don't do anything and go back to Open dialog if they
             hit BACK in color picker! */
            
            if (which == 0) /* White */
            {
                canvas_color_r = canvas_color_g = canvas_color_b = 255;
            }
            else if (which <= white_in_palette) /* One of the colors before white in the pallete */
            {
                canvas_color_r = color_hexes[which - 1][0];
                canvas_color_g = color_hexes[which - 1][1];
                canvas_color_b = color_hexes[which - 1][2];
            }
            else
            {
                canvas_color_r = color_hexes[which][0];
                canvas_color_g = color_hexes[which][1];
                canvas_color_b = color_hexes[which][2];
            }
            
            SDL_FillRect(canvas, NULL, SDL_MapRGB(canvas->format,
                                                  canvas_color_r,
                                                  canvas_color_g,
                                                  canvas_color_b));
            
            cur_undo = 0;
            oldest_undo = 0;
            newest_undo = 0;
            
            been_saved = 1;
            reset_avail_tools();
            
            tool_avail_bak[TOOL_UNDO] = 0;
            tool_avail_bak[TOOL_REDO] = 0;
            
            file_id[0] = '\0';
            starter_id[0] = '\0';
            
            playsound(screen, 1, SND_HARP, 1, SNDPOS_CENTER, SNDDIST_NEAR);
        }
    }
    
    update_canvas(0, 0, WINDOW_WIDTH - 96 - 96, 48 * 7 + 40 + HEIGHTOFFSET);
    
    
    /* Clean up: */
    
    free_surface_array(thumbs, num_files);
    
    free(thumbs);
    
    for (i = 0; i < num_files; i++)
    {
        if (d_names[i] != NULL)
            free(d_names[i]);
        if (d_exts[i] != NULL)
            free(d_exts[i]);
    }
    
    for (i = 0; i < NUM_PLACES_TO_LOOK; i++)
        if (dirname[i] != NULL)
            free(dirname[i]);
    
    free(d_names);
    free(d_exts);
    free(d_places);
    
    return(which != -1);
}

#if !defined(WIN32) && !defined(__APPLE__) && !defined(__BEOS__)
void show_available_papersizes(FILE * fi, char * prg)
{
    const struct paper * ppr;
    int cnt;
    
    fprintf(fi, "Usage: %s [--papersize PAPERSIZE]\n", prg);
    fprintf(fi, "\n");
    fprintf(fi, "PAPERSIZE may be one of:\n");
    
    ppr = paperfirst();
    cnt = 0;
    
    while (ppr != NULL)
    {
        fprintf(fi, "\t%s", papername(ppr));
        cnt++;
        if (cnt == 5)
        {
            cnt = 0;
            fprintf(fi, "\n");
        }
        
        ppr = papernext(ppr);
    }
    
    fprintf(fi, "\n");
    if (cnt != 0)
        fprintf(fi, "\n");
}
#endif

/* FIXME: Use a bitmask! */

void reset_touched(void)
{
    int x, y;
    
    for (y = 0; y < canvas->h; y++)
    {
        for (x = 0; x < canvas->w; x++)
        {
            touched[(y * canvas->w) + x] = 0;
        }
    }
}

Uint8 magic_touched(int x, int y)
{
    Uint8 res;
    
    if (x < 0 || x >= canvas->w || y < 0 || y >= canvas->h)
        return(1);
    
    res = touched[(y * canvas->w) + x];
    touched[(y* canvas->w) + x] = 1;
    
    return(res);
}

int do_color_picker(void)
{
#ifndef NO_PROMPT_SHADOWS
    int i;
    SDL_Surface *alpha_surf;
#endif
    SDL_Rect dest;
    int x, y, w;
    int ox, oy, oox, ooy, nx, ny;
    SDL_Surface * tmp_btn_up, * tmp_btn_down;
    Uint32(*getpixel_tmp_btn_up) (SDL_Surface *, int, int);
    Uint32(*getpixel_tmp_btn_down) (SDL_Surface *, int, int);
    Uint32(*getpixel_img_paintwell) (SDL_Surface *, int, int);
    Uint32(*getpixel_img_color_picker) (SDL_Surface *, int, int);
    Uint8 r, g, b;
    double rh, gh, bh;
    int done, chose;
    SDL_Event event;
    SDLKey key;
    int color_picker_left, color_picker_top;
    int back_left, back_top;
    SDL_Rect color_example_dest;
    SDL_Surface * backup;
    
    
    hide_blinking_cursor();
    
    do_setcursor(cursor_hand);
    
    
    /* Draw button box: */
    
    playsound(screen, 0, SND_PROMPT, 1, SNDPOS_RIGHT, 128);
    
    backup = SDL_CreateRGBSurface(screen->flags, screen->w, screen->h,
                                  screen->format->BitsPerPixel,
                                  screen->format->Rmask,
                                  screen->format->Gmask,
                                  screen->format->Bmask,
                                  screen->format->Amask);
    
    SDL_BlitSurface(screen, NULL, backup, NULL);
    
    ox = screen->w;
    oy = r_colors.y + r_colors.h / 2;
    
    for (w = 0; w <= 128 + 6 + 4; w = w + 4)
    {
        oox = ox - w;
        ooy = oy - w;
        
        nx = 160 + 96 - w + PROMPTOFFSETX;
        ny = 94 + 96 - w + PROMPTOFFSETY;
        
        dest.x = ((nx * w) + (oox * (128 - w))) / 128;
        dest.y = ((ny * w) + (ooy * (128 - w))) / 128;
        
        dest.w = (320 - 96 * 2) + w * 2;
        dest.h = w * 2;
        SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255 - w, 255 - w, 255 - w));
        
        SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);
        
        if (w % 16 == 0)
            SDL_Delay(10);
    }
    
    SDL_BlitSurface(backup, NULL, screen, NULL);
    
#ifndef NO_PROMPT_SHADOWS
    alpha_surf = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
                                      (320 - 96 * 2) + (w - 4) * 2,
                                      (w - 4) * 2,
                                      screen->format->BitsPerPixel,
                                      screen->format->Rmask,
                                      screen->format->Gmask,
                                      screen->format->Bmask,
                                      screen->format->Amask);
    
    if (alpha_surf != NULL)
    {
        SDL_FillRect(alpha_surf, NULL, SDL_MapRGB(alpha_surf->format, 0, 0, 0));
        SDL_SetAlpha(alpha_surf, SDL_SRCALPHA, 64);
        
        for (i = 8; i > 0; i = i - 2)
        {
            dest.x = 160 + 96 - (w - 4) + i + PROMPTOFFSETX;
            dest.y = 94 + 96 - (w - 4) + i + PROMPTOFFSETY;
            dest.w = (320 - 96 * 2) + (w - 4) * 2;
            dest.h = (w - 4) * 2;
            
            SDL_BlitSurface(alpha_surf, NULL, screen, &dest);
        }
        
        SDL_FreeSurface(alpha_surf);
    }
#endif
    
    
    /* Draw prompt box: */
    
    w = w - 6;
    
    dest.x = 160 + 96 - w + PROMPTOFFSETX;
    dest.y = 94 + 96 - w + PROMPTOFFSETY;
    dest.w = (320 - 96 * 2) + w * 2;
    dest.h = w * 2;
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
    
    
    /* Draw color palette: */
    
    color_picker_left = 160 + 96 - w + PROMPTOFFSETX + 2;
    color_picker_top = 94 + 96 - w + PROMPTOFFSETY + 2;
    
    dest.x = color_picker_left;
    dest.y = color_picker_top;
    
    SDL_BlitSurface(img_color_picker, NULL, screen, &dest);
    
    
    /* Draw last color position: */
    
    dest.x = color_picker_x + color_picker_left - 3;
    dest.y = color_picker_y + color_picker_top - 1;
    dest.w = 7;
    dest.h = 3;
    
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));
    
    dest.x = color_picker_x + color_picker_left - 1;
    dest.y = color_picker_y + color_picker_top - 3;
    dest.w = 3;
    dest.h = 7;
    
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));
    
    dest.x = color_picker_x + color_picker_left - 2;
    dest.y = color_picker_y + color_picker_top;
    dest.w = 5;
    dest.h = 1;
    
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
    
    dest.x = color_picker_x + color_picker_left;
    dest.y = color_picker_y + color_picker_top - 2;
    dest.w = 1;
    dest.h = 5;
    
    SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 255, 255, 255));
    
    
    /* Determine spot for example color: */
    
    color_example_dest.x = color_picker_left + img_color_picker->w + 2;
    color_example_dest.y = color_picker_top + 2;
    color_example_dest.w = (320 - 96 * 2) + w * 2 - img_color_picker->w - 6;
    color_example_dest.h = 124;
    
    
    SDL_FillRect(screen, &color_example_dest,
                 SDL_MapRGB(screen->format, 0, 0, 0));
    
    color_example_dest.x += 2;
    color_example_dest.y += 2;
    color_example_dest.w -= 4;
    color_example_dest.h -= 4;
    
    SDL_FillRect(screen, &color_example_dest,
                 SDL_MapRGB(screen->format, 255, 255, 255));
    
    color_example_dest.x += 2;
    color_example_dest.y += 2;
    color_example_dest.w -= 4;
    color_example_dest.h -= 4;
    
    
    /* Draw current color picker color: */
    
    SDL_FillRect(screen, &color_example_dest,
                 SDL_MapRGB(screen->format,
                            color_hexes[NUM_COLORS - 1][0],
                            color_hexes[NUM_COLORS - 1][1],
                            color_hexes[NUM_COLORS - 1][2]));
    
    
    
    /* Show "Back" button */
    
    back_left = (((320 - 96 * 2) + w * 2 - img_color_picker->w) - img_back->w) / 2 + color_picker_left + img_color_picker->w;
    back_top = color_picker_top + img_color_picker->h - img_back->h - 2;
    
    dest.x = back_left;
    dest.y = back_top;
    
    SDL_BlitSurface(img_back, NULL, screen, &dest);
    
    dest.x = back_left + (img_back->w - img_openlabels_back->w) / 2;
    dest.y = back_top + img_back->h - img_openlabels_back->h;
    SDL_BlitSurface(img_openlabels_back, NULL, screen, &dest);
    
    
    SDL_Flip(screen);
    
    
    /* Let the user pick a color, or go back: */
    
    done = 0;
    chose = 0;
    x = y = 0;
    
    do
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                chose = 0;
                done = 1;
            }
            else if (event.type == SDL_ACTIVEEVENT)
            {
                handle_active(&event);
            }
            else if (event.type == SDL_KEYUP)
            {
                key = event.key.keysym.sym;
                
                handle_keymouse(key, SDL_KEYUP);
            }
            else if (event.type == SDL_KEYDOWN)
            {
                key = event.key.keysym.sym;
                
                handle_keymouse(key, SDL_KEYDOWN);
                
                if (key == SDLK_ESCAPE)
                {
                    chose = 0;
                    done = 1;
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN &&
                     valid_click(event.button.button))
            {
                if (event.button.x >= color_picker_left &&
                    event.button.x < color_picker_left + img_color_picker->w &&
                    event.button.y >= color_picker_top &&
                    event.button.y < color_picker_top + img_color_picker->h)
                {
                    /* Picked a color! */
                    
                    chose = 1;
                    done = 1;
                    
                    x = event.button.x - color_picker_left;
                    y = event.button.y - color_picker_top;
                    
                    color_picker_x = x;
                    color_picker_y = y;
                }
                else if (event.button.x >= back_left &&
                         event.button.x < back_left + img_back->w &&
                         event.button.y >= back_top &&
                         event.button.y < back_top + img_back->h)
                {
                    /* Decided to go Back */
                    
                    chose = 0;
                    done = 1;
                }
            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                if (event.button.x >= color_picker_left &&
                    event.button.x < color_picker_left + img_color_picker->w &&
                    event.button.y >= color_picker_top &&
                    event.button.y < color_picker_top + img_color_picker->h)
                {
                    /* Hovering over the colors! */
                    
                    do_setcursor(cursor_hand);
                    
                    
                    /* Show a big solid example of the color: */
                    
                    x = event.button.x - color_picker_left;
                    y = event.button.y - color_picker_top;
                    
                    getpixel_img_color_picker = getpixels[img_color_picker->format->BytesPerPixel];
                    SDL_GetRGB(getpixel_img_color_picker(img_color_picker, x, y), img_color_picker->format, &r, &g, &b);
                    
                    SDL_FillRect(screen, &color_example_dest,
                                 SDL_MapRGB(screen->format, r, g, b));
                    
                    SDL_UpdateRect(screen,
                                   color_example_dest.x,
                                   color_example_dest.y,
                                   color_example_dest.w,
                                   color_example_dest.h);
                }
                else
                {
                    /* Revert to current color picker color, so we know what it was,
                     and what we'll get if we go Back: */
                    
                    SDL_FillRect(screen, &color_example_dest,
                                 SDL_MapRGB(screen->format,
                                            color_hexes[NUM_COLORS - 1][0],
                                            color_hexes[NUM_COLORS - 1][1],
                                            color_hexes[NUM_COLORS - 1][2]));
                    
                    SDL_UpdateRect(screen,
                                   color_example_dest.x,
                                   color_example_dest.y,
                                   color_example_dest.w,
                                   color_example_dest.h);
                    
                    
                    /* Change cursor to arrow (or hand, if over Back): */
                    
                    if (event.button.x >= back_left &&
                        event.button.x < back_left + img_back->w &&
                        event.button.y >= back_top &&
                        event.button.y < back_top + img_back->h)
                        do_setcursor(cursor_hand);
                    else
                        do_setcursor(cursor_arrow);
                }
            }
        }
        
        SDL_Delay(10);
    }
    while (!done);
    
    
    /* Set the new color: */
    
    if (chose)
    {
        getpixel_img_color_picker = getpixels[img_color_picker->format->BytesPerPixel];
        SDL_GetRGB(getpixel_img_color_picker(img_color_picker, x, y), img_color_picker->format, &r, &g, &b);
        
        color_hexes[NUM_COLORS - 1][0] = r;
        color_hexes[NUM_COLORS - 1][1] = g;
        color_hexes[NUM_COLORS - 1][2] = b;
        
        
        /* Re-render color picker to show the current color it contains: */
        
        tmp_btn_up = thumbnail(img_btn_up, color_button_w, color_button_h, 0);
        tmp_btn_down = thumbnail(img_btn_down, color_button_w, color_button_h, 0);
        img_color_btn_off =
        thumbnail(img_btn_off, color_button_w, color_button_h, 0);
        
        getpixel_tmp_btn_up = getpixels[tmp_btn_up->format->BytesPerPixel];
        getpixel_tmp_btn_down = getpixels[tmp_btn_down->format->BytesPerPixel];
        getpixel_img_paintwell = getpixels[img_paintwell->format->BytesPerPixel];
        
        rh = sRGB_to_linear_table[color_hexes[NUM_COLORS - 1][0]];
        gh = sRGB_to_linear_table[color_hexes[NUM_COLORS - 1][1]];
        bh = sRGB_to_linear_table[color_hexes[NUM_COLORS - 1][2]];
        
        SDL_LockSurface(img_color_btns[NUM_COLORS - 1]);
        SDL_LockSurface(img_color_btns[NUM_COLORS - 1 + NUM_COLORS]);
        
        for (y = 0; y < tmp_btn_up->h /* 48 */ ; y++)
        {
            for (x = 0; x < tmp_btn_up->w; x++)
            {
                double ru, gu, bu, rd, gd, bd, aa;
                Uint8 a;
                
                SDL_GetRGB(getpixel_tmp_btn_up(tmp_btn_up, x, y), tmp_btn_up->format,
                           &r, &g, &b);
                
                ru = sRGB_to_linear_table[r];
                gu = sRGB_to_linear_table[g];
                bu = sRGB_to_linear_table[b];
                SDL_GetRGB(getpixel_tmp_btn_down(tmp_btn_down, x, y),
                           tmp_btn_down->format, &r, &g, &b);
                
                rd = sRGB_to_linear_table[r];
                gd = sRGB_to_linear_table[g];
                bd = sRGB_to_linear_table[b];
                SDL_GetRGBA(getpixel_img_paintwell(img_paintwell, x, y),
                            img_paintwell->format, &r, &g, &b, &a);
                
                aa = a / 255.0;
                
                putpixels[img_color_btns[NUM_COLORS - 1]->format->BytesPerPixel]
                (img_color_btns[NUM_COLORS - 1], x, y,
                 getpixels[img_color_picker_thumb->format->BytesPerPixel]
                 (img_color_picker_thumb, x, y));
                putpixels[img_color_btns[NUM_COLORS - 1 + NUM_COLORS]->format->BytesPerPixel]
                (img_color_btns[NUM_COLORS - 1 + NUM_COLORS], x, y,
                 getpixels[img_color_picker_thumb->format->BytesPerPixel]
                 (img_color_picker_thumb, x, y));
                
                if (a == 255)
                {
                    putpixels[img_color_btns[NUM_COLORS - 1]->format->BytesPerPixel]
                    (img_color_btns[NUM_COLORS - 1], x, y,
                     SDL_MapRGB(img_color_btns[i]->format,
                                linear_to_sRGB(rh * aa + ru * (1.0 - aa)),
                                linear_to_sRGB(gh * aa + gu * (1.0 - aa)),
                                linear_to_sRGB(bh * aa + bu * (1.0 - aa))));
                    
                    putpixels[img_color_btns[NUM_COLORS - 1 + NUM_COLORS]->format->BytesPerPixel]
                    (img_color_btns[NUM_COLORS - 1 + NUM_COLORS], x, y,
                     SDL_MapRGB(img_color_btns[i + NUM_COLORS]->format,
                                linear_to_sRGB(rh * aa + rd * (1.0 - aa)),
                                linear_to_sRGB(gh * aa + gd * (1.0 - aa)),
                                linear_to_sRGB(bh * aa + bd * (1.0 - aa))));
                }
            }
        }
        
        SDL_UnlockSurface(img_color_btns[NUM_COLORS - 1]);
        SDL_UnlockSurface(img_color_btns[NUM_COLORS - 1 + NUM_COLORS]);
    }
    
    
    /* Remove the prompt: */
    
    update_canvas(0, 0, canvas->w, canvas->h);
    
    
    return(chose);
}

void magic_putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel)
{
    putpixels[surface->format->BytesPerPixel](surface, x, y, pixel);
}

Uint32 magic_getpixel(SDL_Surface * surface, int x, int y)
{
    return(getpixels[surface->format->BytesPerPixel](surface, x, y));
}


void magic_switchout(SDL_Surface * last)
{
    if (cur_tool == TOOL_MAGIC)
        magic_funcs[magics[cur_magic].handle_idx].switchout(magic_api_struct,
                                                            magics[cur_magic].idx,
                                                            magics[cur_magic].mode,
                                                            canvas, last);
}

void magic_switchin(SDL_Surface * last)
{
    if (cur_tool == TOOL_MAGIC)
    {
        magic_funcs[magics[cur_magic].handle_idx].switchin(magic_api_struct,
                                                           magics[cur_magic].idx,
                                                           magics[cur_magic].mode,
                                                           canvas, last);
        
        /* In case the Magic tool's switchin() called update_progress_bar(),
         let's put the old Tux text back: */
        
        redraw_tux_text();
    }
}

int magic_modeint(int mode)
{
    if (mode == MODE_PAINT)
        return 0;
    else if (mode == MODE_FULLSCREEN)
        return 1;
    else
        return 0;
}

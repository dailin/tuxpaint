//
//  Header.h
//  TuxPaint
//
//  Created by dai lin on 13-1-9.
//  Copyright (c) 2013年 dai lin. All rights reserved.
//

#ifndef TuxPaint_Header_h
#define TuxPaint_Header_h
#include "tp_magic_api.h"

Uint32 alien_api_version(void);
int alien_init(magic_api * api);
int alien_get_tool_count(magic_api * api);
SDL_Surface * alien_get_icon(magic_api * api, int which);
char * alien_get_name(magic_api * api, int which);
char * alien_get_description(magic_api * api, int which, int mode);
void alien_drag(magic_api * api, int which, SDL_Surface * canvas,
                SDL_Surface * last, int ox, int oy, int x, int y,
                SDL_Rect * update_rect);
void alien_click(magic_api * api, int which, int mode,
                 SDL_Surface * canvas, SDL_Surface * last,
                 int x, int y, SDL_Rect * update_rect);
void alien_release(magic_api * api, int which,
                   SDL_Surface * canvas, SDL_Surface * last,
                   int x, int y, SDL_Rect * update_rect);
void alien_shutdown(magic_api * api);
void alien_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int alien_requires_colors(magic_api * api, int which);
void alien_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void alien_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int alien_modes(magic_api * api, int which);

Uint32 blocks_chalk_drip_api_version(void);
int blocks_chalk_drip_init(magic_api * api);
int blocks_chalk_drip_get_tool_count(magic_api * api);
SDL_Surface * blocks_chalk_drip_get_icon(magic_api * api, int which);
char * blocks_chalk_drip_get_name(magic_api * api, int which);
char * blocks_chalk_drip_get_description(magic_api * api, int which, int mode);
void blocks_chalk_drip_drag(magic_api * api, int which, SDL_Surface * canvas,
                SDL_Surface * last, int ox, int oy, int x, int y,
                SDL_Rect * update_rect);
void blocks_chalk_drip_click(magic_api * api, int which, int mode,
                 SDL_Surface * canvas, SDL_Surface * last,
                 int x, int y, SDL_Rect * update_rect);
void blocks_chalk_drip_release(magic_api * api, int which,
                   SDL_Surface * canvas, SDL_Surface * last,
                   int x, int y, SDL_Rect * update_rect);
void blocks_chalk_drip_shutdown(magic_api * api);
void blocks_chalk_drip_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int blocks_chalk_drip_requires_colors(magic_api * api, int which);
void blocks_chalk_drip_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void blocks_chalk_drip_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int blocks_chalk_drip_modes(magic_api * api, int which);

Uint32 blur_api_version(void);
int blur_init(magic_api * api);
int blur_get_tool_count(magic_api * api);
SDL_Surface * blur_get_icon(magic_api * api, int which);
char * blur_get_name(magic_api * api, int which);
char * blur_get_description(magic_api * api, int which, int mode);
void blur_drag(magic_api * api, int which, SDL_Surface * canvas,
               SDL_Surface * last, int ox, int oy, int x, int y,
               SDL_Rect * update_rect);
void blur_click(magic_api * api, int which, int mode,
                SDL_Surface * canvas, SDL_Surface * last,
                int x, int y, SDL_Rect * update_rect);
void blur_release(magic_api * api, int which,
                  SDL_Surface * canvas, SDL_Surface * last,
                  int x, int y, SDL_Rect * update_rect);
void blur_shutdown(magic_api * api);
void blur_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int blur_requires_colors(magic_api * api, int which);
void blur_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void blur_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int blur_modes(magic_api * api, int which);

Uint32 bricks_api_version(void);
int bricks_init(magic_api * api);
int bricks_get_tool_count(magic_api * api);
SDL_Surface * bricks_get_icon(magic_api * api, int which);
char * bricks_get_name(magic_api * api, int which);
char * bricks_get_description(magic_api * api, int which, int mode);
void bricks_drag(magic_api * api, int which, SDL_Surface * canvas,
                 SDL_Surface * last, int ox, int oy, int x, int y,
                 SDL_Rect * update_rect);
void bricks_click(magic_api * api, int which, int mode,
                  SDL_Surface * canvas, SDL_Surface * last,
                  int x, int y, SDL_Rect * update_rect);
void bricks_release(magic_api * api, int which,
                    SDL_Surface * canvas, SDL_Surface * last,
                    int x, int y, SDL_Rect * update_rect);
void bricks_shutdown(magic_api * api);
void bricks_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int bricks_requires_colors(magic_api * api, int which);
void bricks_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void bricks_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int bricks_modes(magic_api * api, int which);

Uint32 calligraphy_api_version(void);
int calligraphy_init(magic_api * api);
int calligraphy_get_tool_count(magic_api * api);
SDL_Surface * calligraphy_get_icon(magic_api * api, int which);
char * calligraphy_get_name(magic_api * api, int which);
char * calligraphy_get_description(magic_api * api, int which, int mode);
void calligraphy_drag(magic_api * api, int which, SDL_Surface * canvas,
                      SDL_Surface * last, int ox, int oy, int x, int y,
                      SDL_Rect * update_rect);
void calligraphy_click(magic_api * api, int which, int mode,
                       SDL_Surface * canvas, SDL_Surface * last,
                       int x, int y, SDL_Rect * update_rect);
void calligraphy_release(magic_api * api, int which,
                         SDL_Surface * canvas, SDL_Surface * last,
                         int x, int y, SDL_Rect * update_rect);
void calligraphy_shutdown(magic_api * api);
void calligraphy_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int calligraphy_requires_colors(magic_api * api, int which);
void calligraphy_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void calligraphy_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int calligraphy_modes(magic_api * api, int which);

Uint32 cartoon_api_version(void);
int cartoon_init(magic_api * api);
int cartoon_get_tool_count(magic_api * api);
SDL_Surface * cartoon_get_icon(magic_api * api, int which);
char * cartoon_get_name(magic_api * api, int which);
char * cartoon_get_description(magic_api * api, int which, int mode);
void cartoon_drag(magic_api * api, int which, SDL_Surface * canvas,
                  SDL_Surface * last, int ox, int oy, int x, int y,
                  SDL_Rect * update_rect);
void cartoon_click(magic_api * api, int which, int mode,
                   SDL_Surface * canvas, SDL_Surface * last,
                   int x, int y, SDL_Rect * update_rect);
void cartoon_release(magic_api * api, int which,
                     SDL_Surface * canvas, SDL_Surface * last,
                     int x, int y, SDL_Rect * update_rect);
void cartoon_shutdown(magic_api * api);
void cartoon_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int cartoon_requires_colors(magic_api * api, int which);
void cartoon_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void cartoon_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int cartoon_modes(magic_api * api, int which);

Uint32 confetti_api_version(void);
int confetti_init(magic_api * api);
int confetti_get_tool_count(magic_api * api);
SDL_Surface * confetti_get_icon(magic_api * api, int which);
char * confetti_get_name(magic_api * api, int which);
char * confetti_get_description(magic_api * api, int which, int mode);
void confetti_drag(magic_api * api, int which, SDL_Surface * canvas,
                   SDL_Surface * last, int ox, int oy, int x, int y,
                   SDL_Rect * update_rect);
void confetti_click(magic_api * api, int which, int mode,
                    SDL_Surface * canvas, SDL_Surface * last,
                    int x, int y, SDL_Rect * update_rect);
void confetti_release(magic_api * api, int which,
                      SDL_Surface * canvas, SDL_Surface * last,
                      int x, int y, SDL_Rect * update_rect);
void confetti_shutdown(magic_api * api);
void confetti_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int confetti_requires_colors(magic_api * api, int which);
void confetti_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void confetti_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int confetti_modes(magic_api * api, int which);

Uint32 distortion_api_version(void);
int distortion_init(magic_api * api);
int distortion_get_tool_count(magic_api * api);
SDL_Surface * distortion_get_icon(magic_api * api, int which);
char * distortion_get_name(magic_api * api, int which);
char * distortion_get_description(magic_api * api, int which, int mode);
void distortion_drag(magic_api * api, int which, SDL_Surface * canvas,
                     SDL_Surface * last, int ox, int oy, int x, int y,
                     SDL_Rect * update_rect);
void distortion_click(magic_api * api, int which, int mode,
                      SDL_Surface * canvas, SDL_Surface * last,
                      int x, int y, SDL_Rect * update_rect);
void distortion_release(magic_api * api, int which,
                        SDL_Surface * canvas, SDL_Surface * last,
                        int x, int y, SDL_Rect * update_rect);
void distortion_shutdown(magic_api * api);
void distortion_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int distortion_requires_colors(magic_api * api, int which);
void distortion_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void distortion_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int distortion_modes(magic_api * api, int which);

Uint32 emboss_api_version(void);
int emboss_init(magic_api * api);
int emboss_get_tool_count(magic_api * api);
SDL_Surface * emboss_get_icon(magic_api * api, int which);
char * emboss_get_name(magic_api * api, int which);
char * emboss_get_description(magic_api * api, int which, int mode);
void emboss_drag(magic_api * api, int which, SDL_Surface * canvas,
                 SDL_Surface * last, int ox, int oy, int x, int y,
                 SDL_Rect * update_rect);
void emboss_click(magic_api * api, int which, int mode,
                  SDL_Surface * canvas, SDL_Surface * last,
                  int x, int y, SDL_Rect * update_rect);
void emboss_release(magic_api * api, int which,
                    SDL_Surface * canvas, SDL_Surface * last,
                    int x, int y, SDL_Rect * update_rect);
void emboss_shutdown(magic_api * api);
void emboss_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int emboss_requires_colors(magic_api * api, int which);
void emboss_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void emboss_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int emboss_modes(magic_api * api, int which);

Uint32 fade_darken_api_version(void);
int fade_darken_init(magic_api * api);
int fade_darken_get_tool_count(magic_api * api);
SDL_Surface * fade_darken_get_icon(magic_api * api, int which);
char * fade_darken_get_name(magic_api * api, int which);
char * fade_darken_get_description(magic_api * api, int which, int mode);
void fade_darken_drag(magic_api * api, int which, SDL_Surface * canvas,
                      SDL_Surface * last, int ox, int oy, int x, int y,
                      SDL_Rect * update_rect);
void fade_darken_click(magic_api * api, int which, int mode,
                       SDL_Surface * canvas, SDL_Surface * last,
                       int x, int y, SDL_Rect * update_rect);
void fade_darken_release(magic_api * api, int which,
                         SDL_Surface * canvas, SDL_Surface * last,
                         int x, int y, SDL_Rect * update_rect);
void fade_darken_shutdown(magic_api * api);
void fade_darken_set_color(magic_api * api, Uint8 r, Uint8 g, Uint8 b);
int fade_darken_requires_colors(magic_api * api, int which);
void fade_darken_switchin(magic_api * api, int which, int mode, SDL_Surface * canvas);
void fade_darken_switchout(magic_api * api, int which, int mode, SDL_Surface * canvas);
int fade_darken_modes(magic_api * api, int which);

#endif

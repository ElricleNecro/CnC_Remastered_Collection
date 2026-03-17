#pragma once

#ifdef WIN32
#include "win32lib/sosdefs.h"
#else
#include "linuxlib/sosdefs.h"
#endif

void __cdecl Buffer_Clear(void *this_object, unsigned char color);
bool __cdecl Linear_Blit_To_Linear(void *this_object,
				   void *dest,
				   int x_pixel,
				   int y_pixel,
				   int dest_x0,
				   int dest_y0,
				   int pixel_width,
				   int pixel_height,
				   bool trans);
bool __cdecl Linear_Scale_To_Linear(void *this_object,
				    void *dest,
				    int src_x,
				    int src_y,
				    int dst_x,
				    int dst_y,
				    int src_width,
				    int src_height,
				    int dst_width,
				    int dst_height,
				    bool trans,
				    char *remap);

extern "C" void __cdecl Init_Stamps(unsigned int icondata);
void __cdecl Buffer_Draw_Stamp(void const *this_object, void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap);

void __cdecl Buffer_Draw_Stamp_Clip(void const *this_object,
				    void const *icondata,
				    int icon,
				    int x_pixel,
				    int y_pixel,
				    void const *remap,
				    int min_x,
				    int min_y,
				    int max_x,
				    int max_y);

VOID __cdecl Buffer_Draw_Line(void *thisptr, int sx, int sy, int dx, int dy, unsigned char color);
VOID __cdecl Buffer_Fill_Rect(void *thisptr, int sx, int sy, int dx, int dy, unsigned char color);
VOID __cdecl Buffer_Remap(void *thisptr, int sx, int sy, int width, int height, void *remap);
VOID __cdecl Buffer_Fill_Quad(void *thisptr, VOID *span_buff, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int color);
void __cdecl Buffer_Draw_Stamp(void const *thisptr, void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap);
void __cdecl Buffer_Draw_Stamp_Clip(void const *thisptr,
				    void const *icondata,
				    int icon,
				    int x_pixel,
				    int y_pixel,
				    void const *remap,
				    int,
				    int,
				    int,
				    int);
void *__cdecl Get_Font_Palette_Ptr(void);
VOID __cdecl Buffer_Remap(void *this_object, int sx, int sy, int width, int height, void *remap);

void __cdecl XOR_Delta_Buffer(int nextrow);
void __cdecl Copy_Delta_Buffer(int nextrow);

unsigned int __cdecl Apply_XOR_Delta(char *target, char *delta);
void __cdecl Apply_XOR_Delta_To_Page_Or_Viewport(void *target, void *delta, int width, int nextrow, int copy);

void *__cdecl Build_Fading_Table(void const *palette, void const *dest, long int color, long int frac);

void __cdecl Set_Palette_Range(void *palette);
bool __cdecl Bump_Color(void *pal, int color, int desired);
void __cdecl Buffer_Put_Pixel(void *this_object, int x_pixel, int y_pixel, unsigned char color);
extern "C" int __cdecl Clip_Rect(int *x, int *y, int *w, int *h, int width, int height);
extern "C" int __cdecl Confine_Rect(int *x, int *y, int w, int h, int width, int height);
extern "C" long __cdecl Buffer_To_Page(int x_pixel, int y_pixel, int pixel_width, int pixel_height, void *src, void *dest);
extern "C" int __cdecl Buffer_Get_Pixel(void *this_object, int x_pixel, int y_pixel);

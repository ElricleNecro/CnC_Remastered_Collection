//
// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection

/**********************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D   A S S O C I A T E S          **
 **********************************************************************************
 *                                                                                *
 *                 Project Name : Westwood 32 Bit Library                         *
 *                                                                                *
 *                    File Name : GBUFFER.H                                       *
 *                                                                                *
 *                   Programmer : Phil W. Gorrow                                  *
 *                                                                                *
 *                   Start Date : May 26, 1994                                    *
 *                                                                                *
 *                  Last Update : October 9, 1995   []                            *
 *                                                                                *
 **********************************************************************************
 *                                                                                *
 * This module contains the definition for the graphic buffer class.  The         *
 * primary functionality of the graphic buffer class is handled by inline         *
 * functions that make a call through function pointers to the correct            *
 * routine.  This has two benefits:                                               *
 *                                                                                *
 *                                                                                *
 *                                                                                *
 * 1) C++ name mangling is not a big deal since the function pointers             *
 *    point to functions in standard C format.                                    *
 * 2) The function pointers can be changed when we set a different                *
 *    graphic mode.  This allows us to have *both supervga and mcga               *
 *    routines present in memory at once.                                         *
 *                                                                                *
 *                                                                                *
 * In the basic library, these functions point to stub routines which just        *
 * return.  This makes a product that just uses a graphic buffer take the         *
 * minimum amount of code space.  For programs that require MCGA or VESA          *
 * support, all that is necessary to do is link either the MCGA or VESA           *
 * specific libraries in, previous to WWLIB32.  The linker will then              *
 * overide the the necessary stub functions automatically.                        *
 *                                                                                *
 * In addition, there are helpful inline function calls for parameter             *
 * ellimination.  This header file gives the defintion for all                    *
 * GraphicViewPort and GraphicBuffer classes.                                     *
 *                                                                                *
 * Terminology:                                                                   *
 *                                                                                *
 *    Buffer Class - A class which consists of a pointer to an allocated          *
 *        buffer and the size of the buffer that was allocated.                   *
 *                                                                                *
 *    Graphic ViewPort - The Graphic ViewPort defines a window into a             *
 *        Graphic Buffer.  This means that although a Graphic Buffer              *
 *        represents linear memory, this may not be true with a Graphic           *
 *        Viewport.  All low level functions that act directly on a graphic       *
 *        viewport are included within this class.  This includes but is not      *
 *        limited to most of the functions which can act on a Video Viewport      *
 *        Video Buffer.                                                           *
 *                                                                                *
 *    Graphic Buffer - A Graphic Buffer is an instance of an allocated buffer     *
 *        used to represent a rectangular region of graphics memory.              *
 *        The HidBuff and BackBuff are excellent examples of a Graphic Buffer.    *
 *                                                                                *
 * Below is a tree which shows the relationship of the VideoBuffer and            *
 * Buffer classes to the GraphicBuffer class:                                     *
 *                                                                                *
 *   BUFFER.H            GBUFFER.H          BUFFER.H            VBUFFER.H         *
 *  ----------          ----------         ----------          ----------         *
 * |  Buffer  |        | Graphic  |       |  Buffer  |        |  Video   |        *
 * |  Class   |        | ViewPort |       |  Class   |        | ViewPort |        *
 *  ----------          ----------         ----------          ----------         *
 *            \        /                             \        /                   *
 *             \      /                               \      /                    *
 *            ----------                             ----------                   *
 *           |  Graphic |                           |  Video   |                  *
 *           |  Buffer  |                           |  Buffer  |                  *
 *            ----------                             ----------                   *
 *            GBUFFER.H                               VBUFFER.H                   *
 *                                                                                *
 *-------------------------------------------------------------------------       *
 * Functions:                                                                     *
 *   GBC::GraphicBufferClass -- inline constructor for GraphicBufferClass         *
 *   GVPC::Remap -- Short form to remap an entire graphic view port               *
 *   GVPC::Get_XPos -- Returns x offset for a graphic viewport class              *
 *   GVPC::Get_Ypos -- Return y offset in a GraphicViewPortClass                  *
 *   VVPC::Get_XPos -- Get the x pos of the VP on the Video                       *
 *   VVPC::Get_YPos -- Get the y pos of the VP on the video                       *
 *   GBC::Get_Graphic_Buffer -- Get the graphic buffer of the VP.                 *
 *   GVPC::Draw_Line -- Stub function to draw line in Graphic Viewport Class      *
 *   GVPC::Fill_Rect -- Stub function to fill rectangle in a GVPC                 *
 *   GVPC::Remap -- Stub function to remap a GVPC                                 *
 *   GVPC::Print -- stub func to print a text string                              *
 *   GVPC::Print -- Stub function to print an integer                             *
 *   GVPC::Print -- Stub function to print a short to a graphic viewport          *
 *   GVPC::Print -- stub function to print a long on a graphic view port          *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -        */

#ifndef GBUFFER_H
#define GBUFFER_H

#include <charconv>
#include <cstdlib>
#include <memory>
#include <system_error>

#include "buffer.h"
#include "drawbuff.h"
#include "function.h"
#include "renderer.h"
#include "utils.h"
#include "ww_win.h"

#include "iconcach.h"

#ifdef Size_Of_Region
#undef Size_Of_Region
#endif

/*
** Pointer to function to call if we detect a focus loss
*/
extern void (*Gbuffer_Focus_Loss_Function)(void);

enum GBC_Enum {
	GBC_NONE = 0,
	GBC_VIDEOMEM = 1,
	GBC_VISIBLE = 2,
};

#define NOT_LOCKED nullptr

#define DEFAULT_SCREEN_WIDTH 356
#define DEFAULT_SCREEN_HEIGHT 200

/*=========================================================================*/
/* Let the compiler know that a GraphicBufferClass exists so that it can	*/
/*		keep a pointer to it in a VideoViewPortClass. */
/*=========================================================================*/
class GraphicViewPortClass;
class GraphicBufferClass;
class VideoViewPortClass;
class VideoBufferClass;

GraphicViewPortClass *Set_Logic_Page(GraphicViewPortClass *ptr);
GraphicViewPortClass *Set_Logic_Page(GraphicViewPortClass &ptr);

/*=========================================================================*/
/* GraphicViewPortClass - Holds viewport information on a viewport which   */
/*    has been attached to a GraphicBuffer.  A viewport is effectively a   */
/*    rectangular subset of the full buffer which is used for clipping and */
/*    the like.                                                            */
/*                                                                         */
/*      char *Buffer  -    is the offset to view port buffer               */
/*      int   Width   -    is the width of view port                       */
/*      int   Height  -    is the height of view port                      */
/*      int   XAdd    -    is add value to go from the end of a line       */
/*                         to the beginning of the next line               */
/*      int   XPos;   -    x offset into its associated VideoBuffer        */
/*      int   YPos;   -    y offset into its associated VideoBuffer        */
/*=========================================================================*/
class GraphicViewPortClass {
public:
	GraphicViewPortClass(GraphicBufferClass *graphic_buff, int x, int y, int w, int h);
	GraphicViewPortClass();
	~GraphicViewPortClass();

	/*===================================================================*/
	/* define functions to get at the private data members               */
	/*===================================================================*/
	inline long Get_Offset(void) {
		return (Offset);
	};
	inline int Get_Height(void) {
		return (Height);
	};
	inline int Get_Width(void) {
		return (Width);
	};
	inline int Get_XAdd(void) {
		return (XAdd);
	};
	inline int Get_XPos(void) {
		return (XPos);
	};
	inline int Get_YPos(void) {
		return (YPos);
	};
	int Get_Pitch(void);
	inline bool Get_IsDirectDraw(void) {
		return false;
	};
	GraphicBufferClass *Get_Graphic_Buffer(void);

	/*===================================================================*/
	/* Define a function which allows us to change a video viewport on   */
	/* the fly.                                                          */
	/*===================================================================*/
	bool Change(int x, int y, int w, int h);

	/*===================================================================*/
	/* Define the set of common graphic functions that are supported by  */
	/* both Graphic ViewPorts and VideoViewPorts.                        */
	/*===================================================================*/
	long Size_Of_Region(int w, int h);
	void Put_Pixel(int x, int y, unsigned char color);
	int Get_Pixel(int x, int y);
	void Clear(unsigned char color = 0);
	long To_Buffer(int x, int y, int w, int h, void *buff, long size);
	long To_Buffer(int x, int y, int w, int h, BufferClass *buff);
	long To_Buffer(BufferClass *buff);
	bool
	Blit(GraphicViewPortClass &dest, int x_pixel, int y_pixel, int dx_pixel, int dy_pixel, int pixel_width, int pixel_height, bool trans = false);
	bool Blit(GraphicViewPortClass &dest, int dx, int dy, bool trans = false);
	bool Blit(GraphicViewPortClass &dest, bool trans = false);
	bool
	Blit(VideoViewPortClass &dest, int x_pixel, int y_pixel, int dx_pixel, int dy_pixel, int pixel_width, int pixel_height, bool trans = false);
	bool Blit(VideoViewPortClass &dest, int dx, int dy, bool trans = false);
	bool Blit(VideoViewPortClass &dest, bool trans = false);
	bool Scale(GraphicViewPortClass &dest,
		   int src_x,
		   int src_y,
		   int dst_x,
		   int dst_y,
		   int src_w,
		   int src_h,
		   int dst_w,
		   int dst_h,
		   bool trans = false,
		   char *remap = nullptr);
	bool Scale(GraphicViewPortClass &dest, int src_x, int src_y, int dst_x, int dst_y, int src_w, int src_h, int dst_w, int dst_h, char *remap);
	bool Scale(GraphicViewPortClass &dest, bool trans = false, char *remap = nullptr);
	bool Scale(GraphicViewPortClass &dest, char *remap);
	bool Scale(VideoViewPortClass &dest,
		   int src_x,
		   int src_y,
		   int dst_x,
		   int dst_y,
		   int src_w,
		   int src_h,
		   int dst_w,
		   int dst_h,
		   bool trans = false,
		   char *remap = nullptr);
	bool Scale(VideoViewPortClass &dest, int src_x, int src_y, int dst_x, int dst_y, int src_w, int src_h, int dst_w, int dst_h, char *remap);
	bool Scale(VideoViewPortClass &dest, bool trans = false, char *remap = nullptr);
	bool Scale(VideoViewPortClass &dest, char *remap);
	unsigned long Print(char const *string, int x_pixel, int y_pixel, int fcolor, int bcolor);
	unsigned long Print(short num, int x_pixel, int y_pixel, int fcol, int bcol);
	unsigned long Print(int num, int x_pixel, int y_pixel, int fcol, int bcol);
	unsigned long Print(long num, int x_pixel, int y_pixel, int fcol, int bcol);

	/*===================================================================*/
	/* Define the list of graphic functions which work only with a       */
	/* graphic buffer.                                                   */
	/*===================================================================*/
	void Draw_Line(int sx, int sy, int dx, int dy, unsigned char color);
	void Draw_Rect(int sx, int sy, int dx, int dy, unsigned char color);
	void Fill_Rect(int sx, int sy, int dx, int dy, unsigned char color);
	void Fill_Quad(void *span_buff, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int color);
	void Remap(int sx, int sy, int width, int height, void *remap);
	void Remap(void *remap);
	void Draw_Stamp(void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap);
	void Draw_Stamp(void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap, int clip_window);

	// This doesnt seem to exist anywhere?? - Steve T 9/26/95 6:05PM
	// void Grey_Out_Region(int x, int y, int width, int height, int color);

	//
	// New members to lock and unlock the direct draw video memory
	//
	inline bool Lock();
	inline bool Unlock();
	inline int Get_LockCount() {
		return (LockCount);
	};

	/*===================================================================*/
	/* Define functions to attach the viewport to a graphicbuffer        */
	/*===================================================================*/
	void Attach(GraphicBufferClass *graphic_buff, int x, int y, int w, int h);
	void Attach(GraphicBufferClass *video_buff, int w, int h);

protected:
	/*===================================================================*/
	/* Define the data used by a GraphicViewPortClass                    */
	/*===================================================================*/
	long Offset; // offset to graphic page
	int Width; // width of graphic page
	int Height; // height of graphic page
	int XAdd; // xadd for graphic page (0)
	int XPos; // x offset in relation to graphicbuff
	int YPos; // y offset in relation to graphicbuff
	long Pitch; // Distance from one line to the next
	GraphicBufferClass *GraphicBuff; // related graphic buff
	int LockCount; // Count for stacking locks if non-zero the buffer
	bool IsDirectDraw = false; // is a locked DD surface
};

/*=========================================================================*/
/* GraphicBufferClass - A GraphicBuffer refers to an actual instance of an */
/*    allocated buffer.  The GraphicBuffer may be drawn to directly        */
/*    becuase it inherits a ViewPort which represents its physcial size.   */
/*                                                                         */
/*        BYTE *Buffer       - is the offset to graphic buffer             */
/*        int   Width        - is the width of graphic buffer              */
/*        int   Height       - is the height of graphic buffer             */
/*        int   XAdd         - is the xadd of graphic buffer               */
/*        int   XPos;        - will be 0 because it is graphicbuff         */
/*        int   YPos;        - will be 0 because it is graphicbuff         */
/*        long  Pitch        - modulo of buffer for reading and writing    */
/*        bool  IsDirectDraw - flag if its a direct draw surface           */
/*=========================================================================*/
class GraphicBufferClass : public GraphicViewPortClass, public BufferClass {
public:
	GraphicBufferClass(int w, int h, GBC_Enum flags);
	GraphicBufferClass(int w, int h, void *buffer, long size);
	GraphicBufferClass(int w, int h, void *buffer = 0);
	GraphicBufferClass(void);
	~GraphicBufferClass();

	void DD_Init(GBC_Enum flags);
	void Init(int w, int h, void *buffer, long size, GBC_Enum flags);
	void Un_Init(void);
	void Attach_DD_Surface(GraphicBufferClass *attach_buffer) {};
	bool Lock(void);
	bool Unlock(void);

	void Scale_Rotate(BitmapClass &bmp, TPoint2D const &pt, long scale, unsigned char angle);

	inline const std::unique_ptr<rendering::RenderSurface> &get_surface(void) {
		return VideoSurfacePtr;
	};

protected:
	std::unique_ptr<rendering::RenderSurface> VideoSurfacePtr;
};

inline bool GraphicViewPortClass::Lock() {
	bool lock = GraphicBuff->Lock();
	if (!lock)
		return false;

	if (this != GraphicBuff) {
		Attach(GraphicBuff, XPos, YPos, Width, Height);
	}
	return true;
};

inline bool GraphicViewPortClass::Unlock() {
	bool unlock = GraphicBuff->Unlock();
	if (!unlock)
		return false;
	if (this != GraphicBuff && IsDirectDraw && !GraphicBuff->LockCount) {
		Offset = 0;
	}
	return true;
};

/***************************************************************************
 * GVPC::GET_GRAPHIC_BUFFER -- Get the graphic buffer of the VP.           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * HISTORY:                                                                *
 *   08/22/1994 SKB : Created.                                             *
 *=========================================================================*/
inline GraphicBufferClass *GraphicViewPortClass::Get_Graphic_Buffer(void) {
	return GraphicBuff;
}

/***************************************************************************
 * GVPC::SIZE_OF_REGION -- stub to call curr graphic mode Size_Of_Region   *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   03/01/1995 BWG : Created.                                             *
 *=========================================================================*/
inline long GraphicViewPortClass::Size_Of_Region(int w, int h) {
	return Buffer_Size_Of_Region(this, w, h);
}

/***************************************************************************
 * GVPC::PUT_PIXEL -- stub to call curr graphic mode Put_Pixel             *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline void GraphicViewPortClass::Put_Pixel(int x, int y, unsigned char color) {
	if (Lock()) {
		Buffer_Put_Pixel(this, x, y, color);
	}
	Unlock();
}

/***************************************************************************
 * GVPC::GET_PIXEL -- stub to call curr graphic mode Get_Pixel             *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline int GraphicViewPortClass::Get_Pixel(int x, int y) {
	int return_code = 0;

	if (Lock()) {
		return_code = (Buffer_Get_Pixel(this, x, y));
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::CLEAR -- stub to call curr graphic mode Clear	                   *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline void GraphicViewPortClass::Clear(unsigned char color) {
	if (Lock()) {
		Buffer_Clear(this, color);
	}
	Unlock();
}

/***************************************************************************
 * GVPC::TO_BUFFER -- stub 1 to call curr graphic mode To_Buffer           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline long GraphicViewPortClass::To_Buffer(int x, int y, int w, int h, void *buff, long size) {
	long return_code = 0;
	if (Lock()) {
		return_code = (Buffer_To_Buffer(this, x, y, w, h, buff, size));
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::TO_BUFFER -- stub 2 to call curr graphic mode To_Buffer           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline long GraphicViewPortClass::To_Buffer(int x, int y, int w, int h, BufferClass *buff) {
	long return_code = 0;
	if (Lock()) {
		return_code = (Buffer_To_Buffer(this, x, y, w, h, buff->Get_Buffer(), buff->Get_Size()));
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::TO_BUFFER -- stub 3 to call curr graphic mode To_Buffer           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline long GraphicViewPortClass::To_Buffer(BufferClass *buff) {
	long return_code = 0;
	if (Lock()) {
		return_code = (Buffer_To_Buffer(this, 0, 0, Width, Height, buff->Get_Buffer(), buff->Get_Size()));
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::BLIT -- stub 1 to call curr graphic mode Blit to GVPC             *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline bool GraphicViewPortClass::Blit(GraphicViewPortClass &dest,
				       int x_pixel,
				       int y_pixel,
				       int dx_pixel,
				       int dy_pixel,
				       int pixel_width,
				       int pixel_height,
				       bool trans) {
	SDL_Rect src_rect = { x_pixel, y_pixel, pixel_width, pixel_height };
	SDL_Rect dst_rect = { dx_pixel, dy_pixel, 0, 0 };
	rendering::GRenderer->blit(this->GraphicBuff->get_surface(), src_rect, dest.GraphicBuff->get_surface(), dst_rect, trans);

	return true;
}

/***************************************************************************
 * GVPC::BLIT -- Stub 2 to call curr graphic mode Blit to GVPC             *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline bool GraphicViewPortClass::Blit(GraphicViewPortClass &dest, int dx, int dy, bool trans) {
	this->Blit(dest, 0, 0, dx, dy, this->Width, this->Height, trans);

	return true;
}

/***************************************************************************
 * GVPC::BLIT -- stub 3 to call curr graphic mode Blit to GVPC             *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline bool GraphicViewPortClass::Blit(GraphicViewPortClass &dest, bool trans) {
	this->Blit(dest, 0, 0, 0, 0, Width, Height, trans);

	return true;
}

/***************************************************************************
 * GVPC::SCALE -- stub 1 to call curr graphic mode Scale to GVPC           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline bool GraphicViewPortClass::Scale(GraphicViewPortClass &dest,
					int src_x,
					int src_y,
					int dst_x,
					int dst_y,
					int src_w,
					int src_h,
					int dst_w,
					int dst_h,
					bool trans,
					char *remap) {
	SDL_Rect src_rect = { src_x, src_y, src_w, src_h };
	SDL_Rect dst_rect = { dst_x, dst_y, dst_w, dst_h };
	rendering::GRenderer->blit(this->GraphicBuff->get_surface(), &src_rect, dest.GraphicBuff->get_surface(), &dst_rect, trans, remap);
	return true;
}

/***************************************************************************
 * GVPC::SCALE -- stub 2 to call curr graphic mode Scale to GVPC           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline bool GraphicViewPortClass::Scale(GraphicViewPortClass &dest,
					int src_x,
					int src_y,
					int dst_x,
					int dst_y,
					int src_w,
					int src_h,
					int dst_w,
					int dst_h,
					char *remap) {
	return this->Scale(dest, src_x, src_y, dst_x, dst_y, src_w, src_h, dst_w, dst_h, false, remap);
}

/***************************************************************************
 * GVPC::SCALE -- stub 3 to call curr graphic mode Scale to GVPC           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline bool GraphicViewPortClass::Scale(GraphicViewPortClass &dest, bool trans, char *remap) {
	return this->Scale(dest, 0, 0, 0, 0, Width, Height, dest.Get_Width(), dest.Get_Height(), trans, remap);
}

/***************************************************************************
 * GVPC::SCALE -- stub 4 to call curr graphic mode Scale to GVPC           *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/06/1995 PWG : Created.                                             *
 *=========================================================================*/
inline bool GraphicViewPortClass::Scale(GraphicViewPortClass &dest, char *remap) {
	return this->Scale(dest, 0, 0, 0, 0, Width, Height, dest.Get_Width(), dest.Get_Height(), false, remap);
}

/***************************************************************************
 * GVPC::PRINT -- stub func to print a text string                         *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/17/1995 PWG : Created.                                             *
 *=========================================================================*/
inline unsigned long GraphicViewPortClass::Print(char const *str, int x, int y, int fcol, int bcol) {
	unsigned long return_code = 0;
	if (Lock()) {
		return_code = (Buffer_Print(this, str, x, y, fcol, bcol));
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::PRINT -- Stub function to print an integer                        *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *=========================================================================*/
inline unsigned long GraphicViewPortClass::Print(int num, int x, int y, int fcol, int bcol) {
	char str[17] = {};

	unsigned long return_code = 0;
	if (Lock()) {
		if (auto tmp = std::to_chars(str, str + sizeof(str) - 1, num); tmp.ec == std::errc()) {
			return_code = (Buffer_Print(this, tmp.ptr, x, y, fcol, bcol));
		}
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::PRINT -- Stub function to print a short to a graphic viewport     *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *=========================================================================*/
inline unsigned long GraphicViewPortClass::Print(short num, int x, int y, int fcol, int bcol) {
	char str[17] = {};

	unsigned long return_code = 0;
	if (Lock()) {
		if (auto tmp = std::to_chars(str, str + sizeof(str) - 1, num); tmp.ec == std::errc()) {
			return_code = (Buffer_Print(this, tmp.ptr, x, y, fcol, bcol));
		}
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::PRINT -- stub function to print a long on a graphic view port     *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *=========================================================================*/
inline unsigned long GraphicViewPortClass::Print(long num, int x, int y, int fcol, int bcol) {
	char str[33] = {};

	unsigned long return_code = 0;
	if (Lock()) {
		if (auto tmp = std::to_chars(str, str + sizeof(str) - 1, num); tmp.ec == std::errc()) {
			return_code = (Buffer_Print(this, tmp.ptr, x, y, fcol, bcol));
		}
	}
	Unlock();
	return (return_code);
}

/***************************************************************************
 * GVPC::DRAW_STAMP -- stub function to draw a tile on a graphic view port *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *=========================================================================*/
inline void GraphicViewPortClass::Draw_Stamp(void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap) {
	if (Lock()) {
		Buffer_Draw_Stamp(this, icondata, icon, x_pixel, y_pixel, remap);
	}
	Unlock();
}

/***************************************************************************
 * GVPC::DRAW_STAMP -- stub function to draw a tile on a graphic view port *
 *                     This version clips the tile to a window             *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *    07/31/1995 BWG : Created.                                            *
 *=========================================================================*/
extern bool IconCacheAllowed;
inline void GraphicViewPortClass::Draw_Stamp(void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap, int clip_window) {
	int cache_index = -1;

	int drewit = 0;
	if (IconCacheAllowed) {
		if (IsDirectDraw) {
			if (!remap) {
				cache_index = Is_Icon_Cached(icondata, icon);
			}

			if (cache_index != -1) {
				if (CachedIcons[cache_index].Get_Is_Cached()) {
					CachedIcons[cache_index].Draw_It(GraphicBuff->Get_DD_Surface(),
									 x_pixel,
									 y_pixel,
									 WindowList[clip_window][WINDOWX] + XPos,
									 WindowList[clip_window][WINDOWY] + YPos,
									 WindowList[clip_window][WINDOWWIDTH],
									 WindowList[clip_window][WINDOWHEIGHT]);
					CachedIconsDrawn++;
					drewit = 1;
				}
			}
		}
	}

	if (drewit == 0) {
		if (Lock()) {
			UnCachedIconsDrawn++;
			Buffer_Draw_Stamp_Clip(this,
					       icondata,
					       icon,
					       x_pixel,
					       y_pixel,
					       remap,
					       WindowList[clip_window][WINDOWX],
					       WindowList[clip_window][WINDOWY],
					       WindowList[clip_window][WINDOWWIDTH],
					       WindowList[clip_window][WINDOWHEIGHT]);
		}
	}
	Unlock();
}

/***************************************************************************
 * GVPC::DRAW_LINE -- Stub function to draw line in Graphic Viewport Class *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/16/1995 PWG : Created.                                             *
 *=========================================================================*/
inline void GraphicViewPortClass::Draw_Line(int sx, int sy, int dx, int dy, unsigned char color) {
	rendering::Point start = { sx, sy }, end = { dx, dy };
	rendering::Rect viewport = { this->XPos, this->YPos, this->Width, this->Height };
	rendering::GRenderer->draw_line(this->GraphicBuff->get_surface(), start, end, color, viewport);
}

/***************************************************************************
 * GVPC::FILL_RECT -- Stub function to fill rectangle in a GVPC            *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/16/1995 PWG : Created.                                             *
 *=========================================================================*/
inline void GraphicViewPortClass::Fill_Rect(int sx, int sy, int dx, int dy, unsigned char color) {
	SDL_Rect dest_rectangle;

	dest_rectangle.x = sx + XPos;
	dest_rectangle.y = sy + YPos;
	dest_rectangle.w = dx - sx + 1;
	dest_rectangle.h = dy - sy + 1;

	if (dest_rectangle.x < XPos) {
		dest_rectangle.w -= XPos - dest_rectangle.x;
		dest_rectangle.x = XPos;
	}

	if (dest_rectangle.x + dest_rectangle.w > Width + XPos) {
		dest_rectangle.w = Width + XPos - dest_rectangle.x;
	}

	if (dest_rectangle.y < YPos) {
		dest_rectangle.h -= YPos - dest_rectangle.y;
		dest_rectangle.y = YPos;
	}

	if (dest_rectangle.y + dest_rectangle.h > Height + YPos) {
		dest_rectangle.h = Height + YPos - dest_rectangle.y;
	}

	if (dest_rectangle.w <= 0)
		return;

	if (dest_rectangle.h <= 0)
		return;

	rendering::GRenderer->fill_rect(this->GraphicBuff->get_surface(), &dest_rectangle, color);
}

/***************************************************************************
 * GVPC::REMAP -- Stub function to remap a GVPC                            *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/16/1995 PWG : Created.                                             *
 *=========================================================================*/
inline void GraphicViewPortClass::Remap(int sx, int sy, int width, int height, void *remap) {
	if (Lock()) {
		Buffer_Remap(this, sx, sy, width, height, remap);
	}
	Unlock();
}

inline void GraphicViewPortClass::Fill_Quad(void *span_buff, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int color) {
	if (Lock()) {
		Buffer_Fill_Quad(this, span_buff, x0, y0, x1, y1, x2, y2, x3, y3, color);
	}
	Unlock();
}

/***************************************************************************
 * GVPC::REMAP -- Short form to remap an entire graphic view port          *
 *                                                                         *
 * INPUT:		BYTE * to the remap table to use
 **
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   07/01/1994 PWG : Created.                                             *
 *=========================================================================*/
inline void GraphicViewPortClass::Remap(void *remap) {
	if (Lock()) {
		Buffer_Remap(this, 0, 0, Width, Height, remap);
	}
	Unlock();
}

inline int GraphicViewPortClass::Get_Pitch(void) {
	return (Pitch);
}
/*=========================================================================*/
/* The following BufferClass functions are defined here because they act	*/
/*		on graphic viewports.
 */
/*=========================================================================*/

/***************************************************************************
 * BUFFER_TO_PAGE -- Generic 'c' callable form of Buffer_To_Page           *
 *                                                                         *
 * INPUT:
 **
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/12/1995 PWG : Created.                                             *
 *=========================================================================*/
inline long Buffer_To_Page(int x, int y, int w, int h, void *Buffer, GraphicViewPortClass &view) {
	long return_code = 0;
	if (view.Lock()) {
		return_code = (Buffer_To_Page(x, y, w, h, Buffer, &view));
	}
	view.Unlock();
	return (return_code);
}

/***************************************************************************
 * BC::TO_PAGE -- Copys a buffer class to a page with definable w, h 		*
 *                                                                         *
 * INPUT:		int	width		- the width of copy region
 ** int	height	- the height of copy region						* GVPC&	dest		-
 *virtual viewport to copy to						*
 *                                                                         *
 * OUTPUT:		none                                                        *
 *																									*
 * WARNINGS:	x and y position are the upper left corner of the dest		*
 *						viewport
 **
 *                                                                         *
 * HISTORY:                                                                *
 *   07/01/1994 PWG : Created.                                             *
 *=========================================================================*/
inline long BufferClass::To_Page(int w, int h, GraphicViewPortClass &view) {
	long return_code = 0;
	if (view.Lock()) {
		return_code = (Buffer_To_Page(0, 0, w, h, Buffer, &view));
	}
	view.Unlock();
	return (return_code);
}
/***************************************************************************
 * BC::TO_PAGE -- Copys a buffer class to a page with definable w, h 		*
 *                                                                         *
 * INPUT:		GVPC&	dest		- virtual viewport to copy to *
 *                                                                         *
 * OUTPUT:		none                                                        *
 *																									*
 * WARNINGS:	x and y position are the upper left corner of the dest		*
 *						viewport.  width and height are assumed to be the * viewport's width and
 *height.										*
 *                                                                         *
 * HISTORY:                                                                *
 *   07/01/1994 PWG : Created.                                             *
 *=========================================================================*/
inline long BufferClass::To_Page(GraphicViewPortClass &view) {
	long return_code = 0;
	if (view.Lock()) {
		return_code = (Buffer_To_Page(0, 0, view.Get_Width(), view.Get_Height(), Buffer, &view));
	}
	view.Unlock();
	return (return_code);
}
/***************************************************************************
 * BC::TO_PAGE -- Copys a buffer class to a page with definable x, y, w, h *
 *                                                                         *
 * INPUT:	int	x			- x pixel on viewport to copy from * int	y
 *- y pixel on viewport to copy from					* int	width		- the width of copy
 *region								* int	height	- the height of copy region
 ** GVPC&	dest		- virtual viewport to copy to							*
 *                                                                         *
 * OUTPUT:	none                                                           *
 *                                                                         *
 * HISTORY:                                                                *
 *   07/01/1994 PWG : Created.                                             *
 *=========================================================================*/
inline long BufferClass::To_Page(int x, int y, int w, int h, GraphicViewPortClass &view) {
	long return_code = 0;
	if (view.Lock()) {
		return_code = (Buffer_To_Page(x, y, w, h, Buffer, &view));
	}
	view.Unlock();
	return (return_code);
}

#endif

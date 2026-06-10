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

/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D   A S S O C I A T E S   **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Westwood 32 bit Library                  *
 *                                                                         *
 *                    File Name : GBUFFER.CPP                              *
 *                                                                         *
 *                   Programmer : Phil W. Gorrow                           *
 *                                                                         *
 *                   Start Date : May 3, 1994                              *
 *                                                                         *
 *                  Last Update : October 9, 1995   []                     *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 *   VVPC::VirtualViewPort -- Default constructor for a virtual viewport   *
 *   VVPC:~VirtualViewPortClass -- Destructor for a virtual viewport       *
 *   VVPC::Clear -- Clears a graphic page to correct color                 *
 *   VBC::VideoBufferClass -- Lowlevel constructor for video buffer class  *
 *   GVPC::Change -- Changes position and size of a Graphic View Port      *
 *   VVPC::Change -- Changes position and size of a Video View Port      	*
 *   Set_Logic_Page -- Sets LogicPage to new buffer                        *
 *   GBC::DD_Init -- Inits a direct draw surface for a GBC                 *
 *   GBC::Init -- Core function responsible for initing a GBC              *
 *   GBC::Lock -- Locks a Direct Draw Surface                              *
 *   GBC::Unlock -- Unlocks a direct draw surface                          *
 *   GBC::GraphicBufferClass -- Default constructor (requires explicit init)*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "compat.h"
#include "gbuffer.h"
#include "misc.h"
#include "renderer.h"

int TotalLocks;
bool AllowHardwareBlitFills = true;

// int	CacheAllowed;

/*=========================================================================*/
/* The following PRIVATE functions are in this file:                       */
/*=========================================================================*/

/*= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =*/

/***************************************************************************
 * GVPC::GRAPHICVIEWPORTCLASS -- Constructor for basic view port class     *
 *                                                   m                      *
 * INPUT:		GraphicBufferClass * gbuffer	- buffer to attach to			*
 *					int x								- x offset into
 *buffer			* int y								- y offset into buffer
 ** int w								- view port width in pixels   * int h
 *- view port height in pixels	*
 *                                                                         *
 * OUTPUT:     Constructors may not have a return value							*
 *                                                                         *
 * HISTORY:                                                                *
 *   05/09/1994 PWG : Created.                                             *
 *=========================================================================*/
GraphicViewPortClass::GraphicViewPortClass(GraphicBufferClass *gbuffer, int x, int y, int w, int h) : LockCount(0), GraphicBuff(NULL) {
	Attach(gbuffer, x, y, w, h);
}

/***************************************************************************
 * GVPC::GRAPHICVIEWPORTCLASS -- Default constructor for view port class   *
 *                                                                         *
 * INPUT:		none                                                        *
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/09/1994 PWG : Created.                                             *
 *=========================================================================*/
GraphicViewPortClass::GraphicViewPortClass(void) {
}

/***************************************************************************
 * GVPC::~GRAPHICVIEWPORTCLASS -- Destructor for GraphicViewPortClass		*
 *                                                                         *
 * INPUT:		none                                                        *
 *                                                                         *
 * OUTPUT:     A destructor may not return a value.                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/10/1994 PWG : Created.                                             *
 *=========================================================================*/
GraphicViewPortClass::~GraphicViewPortClass(void) {
	Offset = 0;
	Width = 0; // Record width of Buffer
	Height = 0; // Record height of Buffer
	XAdd = 0; // Record XAdd of Buffer
	XPos = 0; // Record XPos of Buffer
	YPos = 0; // Record YPos of Buffer
	Pitch = 0; // Record width of Buffer
	IsDirectDraw = false;
	LockCount = 0;
	GraphicBuff = NULL;
}

/***************************************************************************
 * GVPC::ATTACH -- Attaches a viewport to a buffer class                   *
 *                                                                         *
 * INPUT:		GraphicBufferClass *g_buff	- pointer to gbuff to attach to  *
 *					int x                     - x position to attach to			*
 *					int y 							- y position to attach
 *to			* int w							- width of the view port
 ** int h							- height of the view port			*
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/10/1994 PWG : Created.                                             *
 *=========================================================================*/
void GraphicViewPortClass::Attach(GraphicBufferClass *gbuffer, int x, int y, int w, int h) {
	/*======================================================================*/
	/* Can not attach a Graphic View Port if it is actually the physical    */
	/* representation of a Graphic Buffer.                                  */
	/*======================================================================*/
	if (this == Get_Graphic_Buffer()) {
		return;
	}

	/*======================================================================*/
	/* Verify that the x and y coordinates are valid and placed within the	*/
	/* physical buffer.                                                     */
	/*======================================================================*/
	if (x < 0) // you cannot place view port off
		x = 0; // the left edge of physical buf
	if (x >= gbuffer->Get_Width()) // you cannot place left edge off
		x = gbuffer->Get_Width() - 1; // the right edge of physical buf
	if (y < 0) // you cannot place view port off
		y = 0; // the top edge of physical buf
	if (y >= gbuffer->Get_Height()) // you cannot place view port off
		y = gbuffer->Get_Height() - 1; // bottom edge of physical buf

	/*======================================================================*/
	/* Adjust the width and height of necessary                             */
	/*======================================================================*/
	if (x + w > gbuffer->Get_Width()) // if the x plus width is larger
		w = gbuffer->Get_Width() - x; // than physical, fix width

	if (y + h > gbuffer->Get_Height()) // if the y plus height is larger
		h = gbuffer->Get_Height() - y; // than physical, fix height

	/*======================================================================*/
	/* Get a pointer to the top left edge of the buffer.                    */
	/*======================================================================*/
	this->Offset = gbuffer->Get_Offset() + ((gbuffer->Get_Width() + gbuffer->Get_Pitch()) * y) + x;

	/*======================================================================*/
	/* Copy over all of the variables that we need to store.                */
	/*======================================================================*/
	this->XPos = x;
	this->YPos = y;
	this->XAdd = gbuffer->Get_Width() - w;
	this->Width = w;
	this->Height = h;
	this->Pitch = gbuffer->Get_Pitch();
	this->GraphicBuff = gbuffer;
	this->IsDirectDraw = gbuffer->IsDirectDraw;
}

/***************************************************************************
 * GVPC::CHANGE -- Changes position and size of a Graphic View Port        *
 *                                                                         *
 * INPUT:   	int the new x pixel position of the graphic view port      *
 *					int the new y pixel position of the graphic view port		*
 *					int the new width of the viewport in pixels
 ** int the new height of the viewport in pixels					*
 *                                                                         *
 * OUTPUT:  	bool whether the Graphic View Port could be sucessfully     *
 *				      resized.
 **
 *                                                                         *
 * WARNINGS:   You may not resize a Graphic View Port which is derived 		*
 *						from a Graphic View Port Buffer,
 **
 *                                                                         *
 * HISTORY:                                                                *
 *   09/14/1994 SKB : Created.                                             *
 *=========================================================================*/
bool GraphicViewPortClass::Change(int x, int y, int w, int h) {
	/*======================================================================*/
	/* Can not change a Graphic View Port if it is actually the physical		*/
	/*	   representation of a Graphic Buffer.
	 */
	/*======================================================================*/
	if (this == Get_Graphic_Buffer()) {
		return (false);
	}

	/*======================================================================*/
	/* Since there is no allocated information, just re-attach it to the		*/
	/*		existing graphic buffer as if we were creating the */
	/*		GraphicViewPort.
	 */
	/*======================================================================*/
	Attach(Get_Graphic_Buffer(), x, y, w, h);
	return (true);
}

/***************************************************************************
 * GBC::DD_INIT -- Inits a direct draw surface for a GBC                   *
 *                                                                         *
 * INPUT:		none                                                        *
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/09/1995     : Created.                                             *
 *=========================================================================*/
void GraphicBufferClass::DD_Init(GBC_Enum flags) {
	this->VideoSurfacePtr = rendering::GRenderer->create_surface(this->Width, this->Height);

	Allocated = false; //	even if system alloced, dont flag it cuz
	//   we dont want it freed.
	IsDirectDraw = false; //	flag it as a video surface
	Offset = NOT_LOCKED; //	flag it as unavailable for reading or writing
	LockCount = 0; //  surface is not locked
}

/***************************************************************************
 * GBC::INIT -- Core function responsible for initing a GBC                *
 *                                                                         *
 * INPUT:		int 		- the width in pixels of the GraphicBufferClass    *
 *					int		- the heigh in pixels of the GraphicBufferClass		*
 *					void *	- pointer to user supplied buffer (system will		*
 *								  allocate space if buffer is NULL)
 ** long		- size of the user provided buffer						* GBC_Enum
 *- flags if this is defined as a direct draw			* surface
 **
 *                                                                         *
 * OUTPUT:		none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/09/1995     : Created.                                             *
 *=========================================================================*/
void GraphicBufferClass::Init(int w, int h, void *buffer, long size, GBC_Enum flags) {
	Size = size; // find size of physical buffer
	Width = w; // Record width of Buffer
	Height = h; // Record height of Buffer

	//
	// If the surface we are creating is a direct draw object then
	//   we need to do a direct draw init.  Otherwise we will do
	//   a normal alloc.
	//
	if (flags & (GBC_VIDEOMEM | GBC_VISIBLE)) {
		DD_Init(flags);
	} else {
		if (buffer) { // if buffer is specified
			Buffer = (BYTE *)buffer; //		point to it and mark
			Allocated = false; //		it as user allocated
		} else {
			if (!Size)
				Size = w * h;
			Buffer = new BYTE[Size]; // otherwise allocate it and
			Allocated = true; //		mark it system alloced
		}
		Offset = (long)Buffer; // Get offset to the buffer
		IsDirectDraw = false;
	}

	Pitch = 0; // Record width of Buffer
	XAdd = 0; // Record XAdd of Buffer
	XPos = 0; // Record XPos of Buffer
	YPos = 0; // Record YPos of Buffer
	GraphicBuff = this; // Get a pointer to our self
}

/***********************************************************************************************
 * GBC::Un_Init -- releases the video surface belonging to this gbuffer                        *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    6/6/96 12:44PM ST : Created                                                              *
 *=============================================================================================*/

void GraphicBufferClass::Un_Init(void) {
	if (VideoSurfacePtr) {
		while (LockCount) {
			this->VideoSurfacePtr->unlock();
		}
		VideoSurfacePtr.reset();
	}
}

/***************************************************************************
 * GBC::GRAPHICBUFFERCLASS -- Default constructor (requires explicit init) *
 *                                                                         *
 * INPUT:		none                                                        *
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/09/1995     : Created.                                             *
 *=========================================================================*/
GraphicBufferClass::GraphicBufferClass(void) {
	GraphicBuff = this; // Get a pointer to our self
	VideoSurfacePtr = NULL;
}

/***************************************************************************
 * GBC::GRAPHICBUFFERCLASS -- Constructor for fixed size buffers           *
 *                                                                         *
 * INPUT:		long size		- size of the buffer to create					*
 *					int w			- width of buffer in pixels (default = 320)  *
 *					int h			- height of buffer in pixels (default = 200) *
 *					void *buffer	- a pointer to the buffer if any (optional)	*
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/13/1994 PWG : Created.                                             *
 *=========================================================================*/
GraphicBufferClass::GraphicBufferClass(int w, int h, void *buffer, long size) {
	Init(w, h, buffer, size, GBC_NONE);
}
/*=========================================================================*
 * GBC::GRAPHICBUFFERCLASS -- inline constructor for GraphicBufferClass		*
 *                                                                         *
 * INPUT:		int w			- width of buffer in pixels (default = 320)  *
 *					int h			- height of buffer in pixels (default = 200) *
 *					void *buffer	- a pointer to the buffer if any (optional)	*
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/03/1994 PWG : Created.                                             *
 *=========================================================================*/
GraphicBufferClass::GraphicBufferClass(int w, int h, void *buffer) {
	Init(w, h, buffer, w * h, GBC_NONE);
}

/*====================================================================================*
 * GBC::GRAPHICBUFFERCLASS -- contructor for GraphicsBufferClass with special flags   *
 *                                                                                    *
 * INPUT:		int w			- width of buffer in pixels (default = 320)           *
 *					int h			- height of buffer in pixels (default = 200)      *
 *					void *buffer	- unused                                	      *
 *               unsigned flags - flags for creation of special buffer types          *
 *                                GBC_VISIBLE - buffer is a visible screen surface    *
 *                                GBC_VIDEOMEM - buffer resides in video memory       *
 *                                                                                    *
 * OUTPUT:     none                                                                   *
 *                                                                                    *
 * HISTORY:                                                                           *
 *   09-21-95 04:19pm ST : Created                                                    *
 *====================================================================================*/
GraphicBufferClass::GraphicBufferClass(int w, int h, GBC_Enum flags) {
	Init(w, h, NULL, w * h, flags);
}

/*=========================================================================*
 * GBC::~GRAPHICBUFFERCLASS -- Destructor for the graphic buffer class     *
 *                                                                         *
 *	INPUT:		none
 **
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/03/1994 PWG : Created.                                             *
 *=========================================================================*/
GraphicBufferClass::~GraphicBufferClass() {
	//
	// Release the direct draw surface if it exists
	//
	Un_Init();
}

/***************************************************************************
 * SET_LOGIC_PAGE -- Sets LogicPage to new buffer                          *
 *                                                                         *
 * INPUT:		GraphicBufferClass * the buffer we are going to set         *
 *                                                                         *
 * OUTPUT:     GraphicBufferClass * the previous buffer type					*
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   02/23/1995 PWG : Created.                                             *
 *=========================================================================*/
GraphicViewPortClass *Set_Logic_Page(GraphicViewPortClass *ptr) {
	GraphicViewPortClass *old = LogicPage;
	LogicPage = ptr;
	return (old);
}

/***************************************************************************
 * SET_LOGIC_PAGE -- Sets LogicPage to new buffer                          *
 *                                                                         *
 * INPUT:		GraphicBufferClass & the buffer we are going to set         *
 *                                                                         *
 * OUTPUT:     GraphicBufferClass * the previous buffer type					*
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   02/23/1995 PWG : Created.                                             *
 *=========================================================================*/
GraphicViewPortClass *Set_Logic_Page(GraphicViewPortClass &ptr) {
	GraphicViewPortClass *old = LogicPage;
	LogicPage = &ptr;
	return (old);
}

/***************************************************************************
 * GBC::LOCK -- Locks a Direct Draw Surface                                *
 *                                                                         *
 * INPUT:		none                                                        *
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/09/1995     : Created.                                             *
 *   10/09/1995     : Code stolen from Steve Tall                          *
 *=========================================================================*/
extern void Colour_Debug(int call_number);
extern bool GameInFocus;

extern void Block_Mouse(GraphicBufferClass *buffer);
extern void Unblock_Mouse(GraphicBufferClass *buffer);

bool GraphicBufferClass::Lock(void) {
	bool result;
	int restore_attempts = 0;

	/*
	** If the video surface pointer is null then return
	*/
	if (!VideoSurfacePtr)
		return false;

	/*
	** If we dont have focus then return failure
	*/
	if (!GameInFocus)
		return false;

	Block_Mouse(this);

	//
	// If surface is already locked then inc the lock count and return true
	//
	if (LockCount) {
		LockCount++;
		Unblock_Mouse(this);
		return true;
	}

	//
	// If it isn't locked at all then we will have to request that Direct
	// Draw actually lock the surface.
	//
	if (void *pixels; VideoSurfacePtr->lock(&pixels, &this->Pitch)) {
		Offset = (long)pixels;
		Pitch -= Width;
		LockCount++; // increment count so we can track if
		TotalLocks++; // Total number of times we have locked (for debugging)
		// Colour_Debug (1);
		Unblock_Mouse(this);
		return true; // we locked it multiple times.
	}

	// Colour_Debug(1);
	Unblock_Mouse(this);
	return false; // Return false because we couldnt lock or restore the surface
}

/***************************************************************************
 * GBC::UNLOCK -- Unlocks a direct draw surface                            *
 *                                                                         *
 * INPUT:		none                                                        *
 *                                                                         *
 * OUTPUT:     none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/09/1995     : Created.                                             *
 *   10/09/1995     : Code stolen from Steve Tall                          *
 *=========================================================================*/
bool GraphicBufferClass::Unlock(void) {
	//
	// If there is no lock count or this is not a direct draw surface
	// then just return true as there is no harm done.
	//
	if (!LockCount) {
		return true;
	}

	//
	// If lock count is directly equal to one then we actually need to
	// unlock so just give it a shot.
	//
	if (LockCount == 1 && VideoSurfacePtr) {
		Block_Mouse(this);
		if (!VideoSurfacePtr->unlock()) {
			Unblock_Mouse(this);
			return false;
		} else {
			Offset = NOT_LOCKED;
			LockCount--;
			Unblock_Mouse(this);
			return true;
		}
	}
	// Colour_Debug (0);
	LockCount--;
	return true;
}

long __cdecl Buffer_To_Buffer(const void *thisptr, int x, int y, int width, int height, uint8_t *const buff, const long size) {
	if (!thisptr)
		return 0;

	auto *view = static_cast<const GraphicViewPortClass *>(thisptr);
	const int w = view->Get_Width();
	const int h = view->Get_Height();

	int src_x = 0, src_y = 0;
	int x0 = x;
	int y0 = y;
	int x1 = x + width;
	int y1 = y + height;

	if (x0 < 0) {
		src_x = -x0;
		x0 = 0;
	}
	if (y0 < 0) {
		src_y = -y0;
		y0 = 0;
	}
	if (x1 > w)
		x1 = w;
	if (y1 > h)
		y1 = h;

	const int copy_w = x1 - x0;
	const int copy_h = y1 - y0;
	if (copy_w <= 0 || copy_h <= 0)
		return 0;

	if ((width * copy_h) > size)
		return 0;

	const int stride = w + view->Get_XAdd() + view->Get_Pitch();
	auto *dst = buff + src_y * width + src_x;
	auto *src_row = reinterpret_cast<uint8_t *>(view->Get_Offset()) + y0 * stride + x0;

	for (std::size_t row = 0; row < copy_h; row++) {
		std::memcpy(dst, src_row, copy_w);
		src_row += stride;
		dst += width;
	}

	return 0;
}

long __cdecl Buffer_To_Page(const int x_pixel, const int y_pixel, const int pixel_width, const int pixel_height, const uint8_t *src, void *dest) {
	if (!src)
		return 0;

	auto *view = static_cast<GraphicViewPortClass *>(dest);
	const int w = view->Get_Width();
	const int h = view->Get_Height();

	int src_x = 0, src_y = 0;
	int x0 = x_pixel;
	int y0 = y_pixel;
	int x1 = x_pixel + pixel_width;
	int y1 = y_pixel + pixel_height;

	if (x0 < 0) {
		src_x = -x0;
		x0 = 0;
	}
	if (y0 < 0) {
		src_y = -y0;
		y0 = 0;
	}
	if (x1 > w)
		x1 = w;
	if (y1 > h)
		y1 = h;

	const int copy_w = x1 - x0;
	const int copy_h = y1 - y0;
	if (copy_w <= 0 || copy_h <= 0)
		return 0;

	const int stride = w + view->Get_XAdd() + view->Get_Pitch();
	auto *dst = reinterpret_cast<uint8_t *>(view->Get_Offset()) + y0 * stride + x0;
	auto *src_row = src + src_y * pixel_width + src_x;

	for (std::size_t row = 0; row < copy_h; row++) {
		std::memcpy(dst, src_row, copy_w);
		dst += stride;
		src_row += pixel_width;
	}

	return 0;
}

void __cdecl Buffer_Draw_Stamp_Clip(void const *this_object,
				    void const *icondata,
				    int icon,
				    int x_pixel,
				    int y_pixel,
				    void const *remap,
				    int min_x,
				    int min_y,
				    int max_x,
				    int max_y) {
	if (!icondata)
		return;

	// Parsing header:
	auto *header = static_cast<IControl_Type const *>(icondata);
	const int icon_w = header->Width;
	const int icon_h = header->Height;
	const int icon_count = header->Count;
	auto *stamp = static_cast<uint8_t const *>(icondata) + header->Icons;
	auto *is_trans = static_cast<uint8_t const *>(icondata) + header->TransFlag;
	auto *map = header->Map ? static_cast<uint8_t const *>(icondata) + header->Map : nullptr;

	// Logical to physical icon map:
	if (map)
		icon = map[icon];
	if (icon >= icon_count)
		return;

	// Convert clip params:
	max_x += min_x;
	max_y += min_y;
	x_pixel += min_x;
	y_pixel += min_y;

	// Early out if fully outside
	if (x_pixel >= max_x || y_pixel >= max_y)
		return;
	if (x_pixel + icon_w <= min_x || y_pixel + icon_h <= min_y)
		return;

	// Clip source pointer and draw dimensions
	auto *src = stamp + icon * icon_w * icon_h;
	int iwidth = icon_w;
	int rows = icon_h;

	if (x_pixel < min_x) {
		const int clip = min_x - x_pixel;
		src += clip;
		iwidth -= clip;
		x_pixel = min_x;
	}
	if (x_pixel + iwidth > max_x)
		iwidth = max_x - x_pixel;

	int skip = icon_w - iwidth; // What we need to skip at the end of each row

	if (y_pixel < min_y) {
		const int clip = min_y - y_pixel;
		src += clip * icon_w;
		rows -= clip;
		y_pixel = min_y;
	}
	if (y_pixel + rows > max_y)
		rows = max_y - y_pixel;

	if (iwidth <= 0 || rows <= 0)
		return;

	// Get destination pointer from locked viewport
	auto *view = static_cast<GraphicViewPortClass const *>(this_object);
	const int stride = view->Get_Width() + view->Get_XAdd() + view->Get_Pitch();
	auto *dst = reinterpret_cast<uint8_t *>(view->Get_Offset()) + y_pixel * stride + x_pixel;
	const int modulo = stride - iwidth;
	auto *remap_8 = static_cast<uint8_t const *>(remap);

	// Draw
	if (remap_8) {
		for (int row = 0; row < rows; row++, dst += modulo, src += skip) {
			for (int col = 0; col < iwidth; col++, dst++, src++) {
				uint8_t px = remap_8[*src];
				if (px)
					*dst = px;
			}
		}
	} else if (!is_trans[icon]) {
		for (int row = 0; row < rows; row++, dst += modulo, src += skip)
			std::memcpy(dst, src, iwidth);
	} else {
		for (int row = 0; row < rows; row++, dst += modulo, src += skip) {
			for (int col = 0; col < iwidth; col++, dst++, src++) {
				if (*src)
					*dst = *src;
			}
		}
	}
}

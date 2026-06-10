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

/* $Header: /CounterStrike/COMPAT.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : COMPAT.H                                                     *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 03/02/95                                                     *
 *                                                                                             *
 *                  Last Update : March 2, 1995 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef COMPAT_H
#define COMPAT_H

#include <cstddef>

#include "utils.h"

#define BuffType BufferClass
// #define movmem(a,b,c) memmove(b,a,c)
#define ShapeBufferSize _ShapeBufferSize

/*=========================================================================*/
/* Define some equates for the different graphic routines we will install	*/
/*		later.
 */
/*=========================================================================*/
#define HIDBUFF ((void *)(0xA0000))
#define Size_Of_Region(a, b) ((a) * (b))

/*=========================================================================*/
/* Define some Graphic Routines which will only be fixed by these defines	*/
/*=========================================================================*/
#define Set_Font_Palette(a) Set_Font_Palette_Range(a, 0, 15)

/*
**	These are the Open_File, Read_File, and Seek_File constants.
*/
#define READ 1 // Read access.
#define WRITE 2 // Write access.

#ifndef SEEK_SET
#define SEEK_SET 0 // Seek from start of file.
#define SEEK_CUR 1 // Seek relative from current location.
#define SEEK_END 2 // Seek from end of file.
#endif

#define ERROR_WINDOW 1
#define ErrorWindow 1

// extern unsigned char *Palette;
extern unsigned char MDisabled; // Is mouse disabled?
extern WORD Hard_Error_Occured;

/*
**	This is the menu control structures.
*/
typedef enum MenuIndexType { MENUX, MENUY, ITEMWIDTH, ITEMSHIGH, MSELECTED, NORMCOL, HILITE, MENUPADDING = 0x1000 } MenuIndexType;

#ifdef NEVER
#define BITSPERBYTE 8
#define MAXSHORT 0x7FFF
#define HIBITS 0x8000
#define MAXLONG 0x7FFFFFFFL
#define HIBITL 0x80000000

#define MAXINT MAXLONG
#define HIBITI HIBITL

#define DMAXEXP 308
#define FMAXEXP 38
#define DMINEXP -307
#define FMINEXP -37

#define MAXDOUBLE 1.797693E+308
#define MAXFLOAT 3.37E+38F
#define MINDOUBLE 2.225074E-308
#define MINFLOAT 8.43E-37F

#define DSIGNIF 53
#define FSIGNIF 24

#define DMAXPOWTWO 0x3FF
#define FMAXPOWTWO 0x7F
#define DEXPLEN 11
#define FEXPLEN 8
#define EXPBASE 2
#define IEEE 1
#define LENBASE 1
#define HIDDENBIT 1
#define LN_MAXDOUBLE 7.0978E+2
#define LN_MINDOUBLE -7.0840E+2
#endif

/* These defines handle the various names given to the same color. */
#define DKGREEN GREEN
#define DKBLUE BLUE
#define GRAY GREY
#define DKGREY GREY
#define DKGRAY GREY
#define LTGRAY LTGREY

class IconsetClass;
typedef struct {
	short Width; // Width of icons (pixels).
	short Height; // Height of icons (pixels).
	short Count; // Number of (logical) icons in this set.
	short Allocated; // Was this iconset allocated?
	short MapWidth; // Width of map (in icons).
	short MapHeight; // Height of map (in icons).
	long Size; // Size of entire iconset memory block.
	long Icons; // Offset from buffer start to icon data.
	//	unsigned char * Icons;	// Offset from buffer start to icon data.
	long Palettes; // Offset from buffer start to palette data.
	long Remaps; // Offset from buffer start to remap index data.
	long TransFlag; // Offset for transparency flag table.
	long ColorMap; // Offset for color control value table.
	long Map; // Icon map offset (if present).
	//	unsigned char * Map;				// Icon map offset (if present).
} IControl_Type;

inline int Get_IconSet_MapWidth(void const *data) {
	if (data) {
		return (((IControl_Type *)data)->MapWidth);
	}
	return (0);
}

inline int Get_IconSet_MapHeight(void const *data) {
	if (data) {
		return (((IControl_Type *)data)->MapHeight);
	}
	return (0);
}

inline unsigned char const *Get_IconSet_ControlMap(void const *data) {
	if (data) {
		return ((unsigned char const *)((char *)data + ((IControl_Type *)data)->ColorMap));
	}
	return (0);
}

class IconsetClass : protected IControl_Type {
public:
	/*
	**	Query functions.
	*/
	inline int Map_Width(void) const {
		return (MapWidth);
	};
	inline int Map_Height(void) const {
		return (MapHeight);
	};
	inline unsigned char *Control_Map(void) {
		return ((unsigned char *)this + ColorMap);
	};
	inline unsigned char const *Control_Map(void) const {
		return ((unsigned char const *)this + ColorMap);
	};
	inline int Icon_Count(void) const {
		return (Count);
	};
	inline int Pixel_Width(void) const {
		return (Width);
	};
	inline int Pixel_Height(void) const {
		return (Height);
	};
	inline int Total_Size(void) const {
		return (Size);
	};
	inline unsigned char const *Palette_Data(void) const {
		return ((unsigned char const *)this + Palettes);
	};
	inline unsigned char *Palette_Data(void) {
		return ((unsigned char *)this + Palettes);
	};
	inline unsigned char const *Icon_Data(void) const {
		return ((unsigned char const *)this + Icons);
	};
	inline unsigned char *Icon_Data(void) {
		return ((unsigned char *)this + Icons);
	};
	inline unsigned char const *Map_Data(void) const {
		return ((unsigned char const *)this + Map);
	};
	inline unsigned char *Map_Data(void) {
		return ((unsigned char *)this + Map);
	};
	inline unsigned char const *Remap_Data(void) const {
		return ((unsigned char const *)this + Remaps);
	};
	inline unsigned char *Remap_Data(void) {
		return ((unsigned char *)this + Remaps);
	};
	inline unsigned char const *Trans_Data(void) const {
		return ((unsigned char const *)this + TransFlag);
	};
	inline unsigned char *Trans_Data(void) {
		return ((unsigned char *)this + TransFlag);
	};

	/*
	**	Disallow these operations with an IconsetClass object.
	*/
private:
	IconsetClass &operator=(IconsetClass const &);
	IconsetClass(void);
	static void *operator new(std::size_t);
};

#endif

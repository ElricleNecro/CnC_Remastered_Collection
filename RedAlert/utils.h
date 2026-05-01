#pragma once

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <iterator>

#ifndef _WIN32
// Win32 type mappings
typedef int32_t INT;
typedef uint32_t UINT;
typedef uint8_t BYTE;
typedef uint8_t UBYTE;
typedef uint16_t WORD;
typedef uint16_t UWORD;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef void *LPVOID;
typedef void *HWND;
typedef int BOOL;

#define __cdecl
#define __declspec(x)
#else
#include <windows.h>
#include <windowsx.h>
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
#endif

#define WW_ERROR -1

#define GET_SIZE(a) std::size(a)

#define AssembleTo(dest, fmt)                                                                                                                        \
	{                                                                                                                                            \
		va_list argptr;                                                                                                                      \
		if (fmt != (dest)) {                                                                                                                 \
			va_start(argptr, fmt);                                                                                                       \
			vsprintf((dest), fmt, argptr);                                                                                               \
			va_end(argptr);                                                                                                              \
		}                                                                                                                                    \
	}

template <class T>
constexpr inline void minimize(T &a, T b) {
	if (b < a)
		a = b;
}

template <class T>
constexpr inline void maximize(T &a, T b) {
	if (b > a)
		a = b;
}

typedef enum : unsigned short {
	TBLACK,
	PURPLE,
	CYAN,
	GREEN,
	LTGREEN,
	YELLOW,
	PINK,
	BROWN,
	RED,
	LTCYAN,
	LTBLUE,
	BLUE,
	BLACK,
	GREY,
	LTGREY,
	WHITE,
	COLOR_PADDING = 0x1000
} ColorType;

using std::abs;
using std::max;
using std::min;

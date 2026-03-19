#pragma once

#ifndef _WIN32
#include <stdint.h>
#define __int64 int64_t
// typedef uint64_t unsigned __int64;

#define _MAX_FNAME 256
#define _MAX_EXT 256
#endif

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#endif

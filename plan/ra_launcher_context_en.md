# Context: Linux Launcher for Red Alert Remastered

## Overview

The EA open source repository (`CnC_Remastered_Collection`) contains the game
simulation source code, built as a **shared library** (`RedAlert.dll` /
`RedAlert.so`). The GlyphX launcher (graphical frontend) is **not** open source.

The goal is to write a custom Linux launcher in C++ using **SDL3** that:
1. Loads `RedAlert.so` via `libdl` (`dlopen` / `dlsym`)
2. Drives the simulation through the `CNC_*` interface exported as `extern "C"`
3. Handles rendering, audio and input (SDL3)

---

## DLL Interface Architecture

All exported functions use `extern "C"` / `__cdecl` — pure C ABI, no C++ name
mangling. On Linux `__cdecl` is a no-op:

```cpp
// compat/msvc_compat.h
#ifndef _WIN32
    #define __cdecl
    #define __declspec(x)
#endif
```

The dll-side exports use `__declspec(dllexport)`, which must be abstracted:

```cpp
#ifdef _WIN32
    #define DLLEXPORT __declspec(dllexport)
#else
    #define DLLEXPORT __attribute__((visibility("default")))
#endif
```

With the matching CMakeLists entry:

```cmake
target_compile_options(redalert PRIVATE -fvisibility=hidden)
```

---

## Interface Version

```cpp
// dllinterfaceversion.h — DO NOT MODIFY
#define CNC_DLL_API_VERSION 0x102
```

`CNC_Version(CNC_DLL_API_VERSION)` must be the **first call** after `dlopen`.
The dll returns its own version; abort if it differs from the expected value.

---

## Launcher Lifecycle

```
dlopen("RedAlert.so", RTLD_NOW)
    │
    ├─ ra_resolve_symbols()       // resolve all CNC_* symbols via dlsym
    │
    ├─ CNC_Version()              // compatibility check against 0x102
    ├─ CNC_Init(cmd, callback)    // init dll + register event callback
    ├─ CNC_Config(&rules)         // difficulty settings (optional)
    │
    ├─ CNC_Start_Instance(...)    // start campaign scenario
    │   or CNC_Start_Instance_Variation(...)
    │   or CNC_Start_Custom_Instance(...)
    │
    └─ Main loop
           ├─ handle_sdl_events() → CNC_Handle_Input(...)
           ├─ CNC_Advance_Instance(player_id)   // simulation tick
           ├─ CNC_Get_Visible_Page(buf, &w, &h) // 8-bit paletted framebuffer
           ├─ CNC_Get_Palette(palette)           // 256×RGB palette
           └─ SDL_UpdateTexture + SDL_RenderPresent

dlclose()
```

---

## Rendering: 8-bit Paletted Framebuffer

The dll maintains an internal framebuffer in **8-bit paletted mode** (Westwood
legacy from the 320×200 / Mode X DOS era). The launcher is responsible for the
palette → RGBA conversion and GPU upload.

```cpp
// Palette type
typedef unsigned char CNCPalette[256][3]; // 256 RGB entries, 1 byte per component

// SDL3 render loop
unsigned char *framebuf = malloc(width * height);
CNCPalette palette;

iface.CNC_Get_Visible_Page(framebuf, &width, &height);
iface.CNC_Get_Palette(palette);

// Palette → RGBA conversion
uint32_t *rgba = malloc(width * height * 4);
for (int i = 0; i < width * height; i++) {
    unsigned char idx = framebuf[i];
    rgba[i] = (palette[idx][0] << 16)
            | (palette[idx][1] <<  8)
            |  palette[idx][2]
            | 0xFF000000u;
}

SDL_UpdateTexture(texture, NULL, rgba, width * 4);
SDL_RenderTexture(renderer, texture, NULL, NULL);
SDL_RenderPresent(renderer);
```

---

## Event Callback

The dll notifies the launcher via a function pointer registered at `CNC_Init`:

```cpp
typedef void (*CNC_Event_Callback_Type)(const void *event); // cast to EventCallbackStruct*
```

The `EventCallbackStruct.EventType` field (enum `EventCallbackType`) identifies
what happened. The launcher must handle:

| EventType                                | Launcher action                          |
|------------------------------------------|------------------------------------------|
| `CALLBACK_EVENT_SOUND_EFFECT`            | Decode `.aud` + push to SDL_AudioStream  |
| `CALLBACK_EVENT_SPEECH`                  | Same                                     |
| `CALLBACK_EVENT_MOVIE`                   | Play VQA file                            |
| `CALLBACK_EVENT_GAME_OVER`               | Show end-of-game screen                  |
| `CALLBACK_EVENT_MESSAGE`                 | Display HUD message                      |
| `CALLBACK_EVENT_BRIEFING_SCREEN`         | Show mission briefing                    |
| `CALLBACK_EVENT_CENTER_CAMERA`           | Re-center the view                       |
| `CALLBACK_EVENT_PING`                    | Show beacon marker on map                |
| `CALLBACK_EVENT_ACHIEVEMENT`             | Optional                                 |
| `CALLBACK_EVENT_STORE_CARRYOVER_OBJECTS` | Persist objects between missions         |

---

## Shared Structs — Critical Rules

**All structs exchanged with the dll are under `#pragma pack(1)`.**
The binary layout is a fixed contract with the dll. Never add, remove or
reorder fields. Use padding buffers for fields not yet implemented rather than
omitting them.

`#pragma pack` push/pop is fully supported by GCC and Clang on Linux — no need
for `__attribute__((packed))`.

### Size Verification — Mandatory

Add a `static_assert` for every shared struct. The reference size is obtained
by adding a `printf` in the dll at startup:

```cpp
printf("sizeof CNCPlayerInfoStruct: %zu\n", sizeof(CNCPlayerInfoStruct));
```

```cpp
static_assert(sizeof(CNCPlayerInfoStruct) == REFERENCE_SIZE,
              "CNCPlayerInfoStruct layout mismatch — check padding");
```

---

## CNCPlayerInfoStruct — Full Layout with Padding

**Problem**: some fields in the original struct (`CNCSpiedInfoStruct`,
`DllObjectTypeEnum`, `DllActionTypeEnum`) require internal dll headers that are
not available in the launcher. The solution is to replace them with padding
buffers of identical size, preserving the binary layout without introducing
unnecessary dependencies.

**Sizes derived from `dllinterface.h` (Red Alert):**

| Original field                         | Type                       | Size                |
|----------------------------------------|----------------------------|---------------------|
| `SpiedInfo[MAX_HOUSES]`                | `CNCSpiedInfoStruct[32]`   | 32 × 3×int = 384 B  |
| `SelectedType`                         | `DllObjectTypeEnum`        | 4 B (enum = int)    |
| `ActionWithSelected[MAX_EXPORT_CELLS]` | `DllActionTypeEnum[16384]` | 16384 × 1 B         |

`MAX_HOUSES = 32`, `MAX_EXPORT_CELLS = 128 × 128 = 16384`

```cpp
#pragma pack(push, 1)

typedef struct {
    // --- direct fields ---
    char          Name[64];
    unsigned char House;
    int           ColorIndex;
    uint64_t      GlyphxPlayerID;        // unsigned __int64 on the dll side
    int           Team;
    int           StartLocationIndex;
    unsigned char HomeCellX;
    unsigned char HomeCellY;
    bool          IsAI;
    unsigned int  AllyFlags;
    bool          IsDefeated;
    unsigned int  SpiedPowerFlags;
    unsigned int  SpiedMoneyFlags;

    // --- padding: CNCSpiedInfoStruct SpiedInfo[MAX_HOUSES] ---
    // CNCSpiedInfoStruct = { int Credits; int PowerProduced; int PowerDrained; }
    // 3 × sizeof(int) × 32 = 384 bytes
    unsigned char _pad_SpiedInfo[32 * 3 * 4];

    int           SelectedID;

    // --- padding: DllObjectTypeEnum SelectedType (enum = int) ---
    unsigned char _pad_SelectedType[4];

    // --- padding: DllActionTypeEnum ActionWithSelected[MAX_EXPORT_CELLS] ---
    // DllActionTypeEnum : unsigned char, MAX_EXPORT_CELLS = 128*128 = 16384
    unsigned char ActionWithSelected[128 * 128];

    unsigned int  ActionWithSelectedCount;
    unsigned int  ScreenShake;
    bool          IsRadarJammed;
} CNCPlayerInfoStruct;

#pragma pack(pop)

// Verification — replace XXXX with the value obtained from printf in the dll
// static_assert(sizeof(CNCPlayerInfoStruct) == XXXX,
//               "CNCPlayerInfoStruct size mismatch");
```

> **Note**: `ActionWithSelected` is directly usable as a `unsigned char` array
> indexed by cell even without knowing `DllActionTypeEnum` — the values are
> opaque to the launcher which does not need to interpret them.

---

## Exported Function Table (RAInterface)

Loading pattern via `dlsym`:

```cpp
typedef struct {
    // --- lifecycle ---
    unsigned int (*CNC_Version)(unsigned int version_in);
    void         (*CNC_Init)(const char *cmd, CNC_Event_Callback_Type cb);
    void         (*CNC_Config)(const CNCRulesDataStruct *rules);
    void         (*CNC_Add_Mod_Path)(const char *path);

    // --- rendering ---
    bool         (*CNC_Get_Visible_Page)(unsigned char *buf, unsigned int *w, unsigned int *h);
    bool         (*CNC_Get_Palette)(CNCPalette palette);

    // --- game start ---
    bool         (*CNC_Start_Instance)(int scenario, int build_level, const char *faction,
                                       const char *game_type, const char *content_dir,
                                       int sabotaged, const char *override_map);
    bool         (*CNC_Start_Instance_Variation)(int scenario, int variation, int direction,
                                                  int build_level, const char *faction,
                                                  const char *game_type, const char *content_dir,
                                                  int sabotaged, const char *override_map);
    bool         (*CNC_Start_Custom_Instance)(const char *content_dir, const char *dir_path,
                                               const char *scenario_name, int build_level,
                                               bool multiplayer);

    // --- simulation ---
    bool         (*CNC_Advance_Instance)(uint64_t player_id);
    bool         (*CNC_Get_Game_State)(GameStateRequestEnum type, uint64_t player_id,
                                        unsigned char *buf, unsigned int buf_size);
    bool         (*CNC_Read_INI)(int scenario, int variation, int direction,
                                  const char *content_dir, const char *override_map,
                                  char *ini_buf, int ini_buf_size);
    void         (*CNC_Set_Home_Cell)(int x, int y, uint64_t player_id);

    // --- input ---
    void         (*CNC_Handle_Input)(InputRequestEnum event, unsigned char special_keys,
                                      uint64_t player_id, int x1, int y1, int x2, int y2);
    void         (*CNC_Handle_Structure_Request)(int type, uint64_t player_id, int object_id);
    void         (*CNC_Handle_Unit_Request)(int type, uint64_t player_id);
    void         (*CNC_Handle_Sidebar_Request)(int type, uint64_t player_id,
                                                int buildable_type, int buildable_id,
                                                short cell_x, short cell_y);
    void         (*CNC_Handle_SuperWeapon_Request)(int type, uint64_t player_id,
                                                    int buildable_type, int buildable_id,
                                                    int x, int y);
    void         (*CNC_Handle_ControlGroup_Request)(int type, uint64_t player_id,
                                                     unsigned char group_index);
    void         (*CNC_Handle_Debug_Request)(int type, uint64_t player_id, const char *name,
                                              int x, int y, bool unshroud, bool enemy);
    void         (*CNC_Handle_Beacon_Request)(int type, uint64_t player_id, int px, int py);
    void         (*CNC_Handle_Game_Request)(GameRequestEnum type);
    void         (*CNC_Handle_Game_Settings_Request)(int health_bar_mode, int resource_bar_mode);

    // --- multiplayer ---
    bool         (*CNC_Set_Multiplayer_Data)(int scenario, CNCMultiplayerOptionsStruct *opts,
                                              int num_players, CNCPlayerInfoStruct *players,
                                              int max_players);

    // --- selection ---
    bool         (*CNC_Clear_Object_Selection)(uint64_t player_id);
    bool         (*CNC_Select_Object)(uint64_t player_id, int type_id, int object_id);

    // --- save/load ---
    bool         (*CNC_Save_Load)(bool save, const char *path, const char *game_type);

    // --- misc ---
    void         (*CNC_Set_Difficulty)(int difficulty);
    void         (*CNC_Restore_Carryover_Objects)(const CarryoverObjectStruct *objects);
    void         (*CNC_Handle_Player_Switch_To_AI)(uint64_t player_id);
    void         (*CNC_Handle_Human_Team_Wins)(uint64_t player_id);
    void         (*CNC_Start_Mission_Timer)(int time);
    bool         (*CNC_Get_Start_Game_Info)(uint64_t player_id, int *waypoint_index);
} RAInterface;
```

Resolution macro:

```cpp
#define RESOLVE(iface, handle, name)                                    \
    (iface)->name = dlsym((handle), #name);                             \
    if (!(iface)->name) {                                               \
        fprintf(stderr, "missing symbol: %s\n", #name);                \
        return false;                                                   \
    }
```

---

## Audio

The native Westwood audio format is `.aud` (proprietary codec). There is **no**
SDL_mixer port for SDL3 at the time of writing — use SDL3 native audio instead:

- Decode `.aud` → PCM inside the `CALLBACK_EVENT_SOUND_EFFECT` callback
- Push samples via `SDL_AudioStream`
- Music is MIDI → options: FluidSynth, or offline conversion to OGG

---

## CMake Notes

```cmake
# Shared library target
add_library(redalert SHARED ${SOURCES})
target_compile_options(redalert PRIVATE -fvisibility=hidden)

# Platform flags
if(WIN32)
    target_compile_definitions(redalert PRIVATE WIN32)
else()
    target_compile_definitions(redalert PRIVATE LINUX)
endif()

# Red Alert vs Tiberian Dawn distinction:
# TiberianDawn/CMakeLists.txt adds TIBERIAN_DAWN
# RedAlert/CMakeLists.txt does not need it

# Force-include the compat PCH (simulates the global MSVC PCH)
target_compile_options(redalert PRIVATE
    -include ${CMAKE_SOURCE_DIR}/compat/force_include.h
)
```

---

## Minimal Compat Header (`compat/force_include.h`)

```cpp
#pragma once

#ifndef _WIN32

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// MSVC types
typedef unsigned long  DWORD;
typedef unsigned short WORD;
typedef unsigned char  BYTE;
typedef int            BOOL;
typedef void*          HANDLE;
typedef void*          HWND;
typedef void*          HINSTANCE;
typedef char*          LPSTR;
typedef const char*    LPCSTR;
typedef void*          LPVOID;

// MSVC macros
#define __cdecl
#define __declspec(x)
#define __int64      int64_t
// "unsigned __int64" → replace manually with uint64_t in source files

// Windows CRT constants
#define _MAX_FNAME 256
#define _MAX_EXT   256

// Window stubs
#include "windows_stub.h"

#endif // _WIN32
```

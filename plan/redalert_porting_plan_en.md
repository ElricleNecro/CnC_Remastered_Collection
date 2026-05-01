# Red Alert — Porting Plan Win32 → SDL3 (Linux/Windows)

## Context

Port `RedAlert/win32lib/` to a portable SDL3 implementation, removing the `win32lib/`
and `linuxlib/` directories in favor of a single SDL3 layer compilable on both Windows
and Linux without any platform `#ifdef` in the game code.

The game code (`RedAlert/*.cpp`) never uses DirectDraw or Win32 directly — all
dependencies are encapsulated in `win32lib/`. The public interface of
`GraphicBufferClass` / `GraphicViewPortClass` is generic enough to remain unchanged
for calling code.

---

## Order of Work

### 1. `wwstd.h` — Top Priority

**Problem:** contains unconditional `#define WIN32 1` and `#include <windows.h>`.
Everything else depends on it transitively.

**Action:** replace with portable typedefs via `<stdint.h>` under `#ifndef _WIN32` guard:
- `BOOL`, `BYTE` (`uint8_t`), `WORD` (`uint16_t`), `DWORD` (`uint32_t`), `LONG`, `LPVOID`
- `TRUE` / `FALSE`

---

### 2. Rendering Abstraction Layer — `render_backend.h` (new file)

**Context:** avoid exposing `SDL_Texture*` directly in `gbuffer.h` to ease a potential
future port to OpenGL or Vulkan without touching game code. `SDL_Window` remains valid
in all cases; `SDL_Renderer` and `SDL_Texture` disappear if the backend changes.

**Action:** create `render_backend.h` with two minimal structs:

```cpp
struct RenderSurface {
    SDL_Texture *texture;   // replaced here only if backend changes
    int width, height, pitch;
};

struct RenderBackend {
    SDL_Window   *window;
    SDL_Renderer *renderer;

    RenderSurface create_surface(int w, int h);
    void destroy_surface(RenderSurface &s);
    void blit(RenderSurface &src, RenderSurface &dst, int x, int y);
    void present();
    void lock(RenderSurface &s, void **pixels, int *pitch);
    void unlock(RenderSurface &s);
};
```

`GraphicBufferClass` holds a `RenderSurface`, not a raw `SDL_Texture*`.

---

### 3. `gbuffer.h` / `gbuffer.cpp` — Core of the Port

**Problem:** `GraphicBufferClass` and `GraphicViewPortClass` are coupled to DirectDraw via:
- `#include <ddraw.h>`
- `extern LPDIRECTDRAW DirectDrawObject` (global)
- `LPDIRECTDRAWSURFACE VideoSurfacePtr` (private member)
- `DDSURFACEDESC VideoSurfaceDescription` (private member)
- `LPDIRECTDRAWSURFACE Get_DD_Surface()` (public method — DDraw leak)
- `BOOL Get_IsDirectDraw()` (public method)
- `HRESULT Blit(...)` (COM return type)

**Action:**
- Remove all items listed above
- Replace `VideoSurfacePtr` with `RenderSurface` (see step 2)
- Replace `extern LPDIRECTDRAW DirectDrawObject` with `extern RenderBackend *GRenderer`
- `Get_DD_Surface()` → `Get_SDL_Texture()` or remove (no longer called once `drawmisc` is ported)
- `Get_IsDirectDraw()` → remove or return `false`
- `HRESULT Blit(...)` → `bool Blit(...)`
- `Set_Video_Mode` and window management move into `gbuffer.cpp` via
  `SDL_CreateWindow` / `SDL_CreateRenderer`

**Public interface unchanged for calling code:**
`Clear`, `Fill_Rect`, `Draw_Line`, `Put_Pixel`, `Scale`, `Blit`, `Lock`/`Unlock`,
`Get_Width`/`Get_Height`/`Get_Offset`

**Note:** remove `#include <iconcach.h>` from `gbuffer.h` (see step 7).

---

### 4. `ddraw.cpp` / `ddraw.h` — Delete

**Action:** delete both files. Their content (`DirectDrawCreate`, `QueryInterface`,
exclusive cooperative mode management, DDraw surfaces) is absorbed into `gbuffer.cpp`
via SDL3.

---

### 5. Headers to Rewrite (minor Win32 leaks)

Each becomes portable with no platform `#ifdef`.

#### `timer.h` / `timerini.cpp`
- `HANDLE TimerThreadHandle` → remove or `SDL_TimerID`
- `BOOL TimerSystemOn` → `bool`
- `timeSetEvent` / `timeBeginPeriod` (Win32 multimedia API) → `SDL_AddTimer` or POSIX `clock_gettime`

#### `keyboard.h`
- `VK_*` keycodes + `HWND` → `SDL_Keycode`
- `Message_Handler(HWND...)` → SDL event loop (`SDL_PollEvent`)

#### `mouse.h` / `mouseww.cpp`
- `GetCursorPos` / `ClipCursor` / `POINT` Win32 → `SDL_GetMouseState` / `SDL_SetWindowMouseGrab`

#### `palette.h` / `palette.cpp`
- `PALETTEENTRY` (Win32 type) → portable struct `{ uint8_t r, g, b; }`
- 8bpp palette logic is reusable as-is

#### `misc.h`
- Remove `#include <ddraw.h>`
- `HWND MainWindow` → `SDL_Window *MainWindow`
- `DirectDrawObject` global moves into `gbuffer` (see step 3)

---

### 6. `drawmisc.cpp` — Port MSVC Inline ASM

**Problem:** 21 `__asm { }` blocks using MSVC syntax, rejected by GCC/Clang.

**Categories:**
- Trivial (`rep movsd`, `rep stosd`) → `memcpy` / `memset`
- Rendering (transparent blit, 256-entry palette remap, nearest-neighbor scale) →
  rewrite in GAS inline (`__asm__ volatile`) compilable with GCC/Clang on both Windows and Linux

**Target syntax (GAS inline):**
```cpp
__asm__ volatile (
    "rep movsl"
    : "+D"(dst), "+S"(src), "+c"(count)
    :: "memory"
);
```

Key differences MASM → GAS: AT&T syntax, reversed operand order, registers prefixed
with `%`, explicit size suffixes on mnemonics (`movl`, `movb`...).

---

### 7. `iconcach.h` — Deferred

**Context:** `IconCacheClass` is a 24×24 icon caching system in DDraw VRAM.
`STMPCACH.ASM` is absent from the repo — the cache is already disabled, its ASM
functions are empty stubs in `drawmisc.cpp`. The system silently degrades to
non-cached mode (functional, less performant).

**DDraw dependencies in the class:**
- `LPDIRECTDRAWSURFACE CacheSurface` (private member)
- `Draw_It(LPDIRECTDRAWSURFACE dest_surface, ...)` (public method)

**Action:** remove `#include <iconcach.h>` from `gbuffer.h` during step 3.
Handle `iconcach.h` when porting `drawmisc.cpp` (step 6):
- Option A: delete the class (cache already non-functional)
- Option B: replace `LPDIRECTDRAWSURFACE` with `RenderSurface*` and include only
  from `drawmisc.cpp`

---

### 8. MASM `.asm` Files — Port to GAS

Three files to port; keep assembly (pedagogical goal).
Target: GAS (GNU Assembler), separate `.S` files, compilable with GCC/Clang on Windows and Linux.
CMake switches only the object format (`-f win64` vs `-f elf64`), not the source.

#### `lcwcomp.asm` — LCW Compression (Westwood custom Lempel-Ziv)
- `LCW_Compress(src, dst, size)` → compressed size
- Pure algorithm, good first GAS exercise (no complex C struct interaction)

#### `lcwuncmp.asm` — LCW Decompression
- `LCW_Uncompress(src, dst, length)` → decompressed size

#### `tobuff.asm` — Rectangular Buffer Copy with Stride
- `Buffer_To_Buffer` — buffer manipulation with stride
- Slightly more C context to manage than LCW

**Recommended order:** `lcwcomp` → `lcwuncmp` → `tobuff` → `drawmisc` blocks

---

### 9. CMakeLists — Final Simplification

**After all steps are complete:**

```cmake
file(GLOB ra_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(redalert SHARED ${ra_srcs})
target_include_directories(redalert PRIVATE ${CMAKE_CURRENT_LIST_DIR})
find_package(SDL3 REQUIRED)
target_link_libraries(redalert PRIVATE SDL3::SDL3)
```

- Remove `if(WIN32)` / `if(LINUX)` branches on sources and include directories
- Delete `win32lib/` and `linuxlib/` entirely

---

## Portable Files — No Changes Required

| File | Content |
|---|---|
| `iff.cpp` / `iff.h` | IFF format parser |
| `dipthong.cpp` | Diphthong compression |
| `_diptabl.cpp` | Diphthong table |
| `font.cpp` / `loadfont.cpp` / `set_font.cpp` | Font rendering and loading |
| `getshape.cpp` | Shape data access |
| `iconset.cpp` | Icon set management |
| `writepcx.cpp` | PCX export |
| `drawrect.cpp` | Rectangle drawing (pure logic) |
| `buffer.cpp` / `buffer.h` | System memory buffer |
| `delay.cpp` | Delay (`Sleep` vs `usleep` to verify) |

---

## Deletion Summary

| Removed | Replaced by |
|---|---|
| `ddraw.cpp` / `ddraw.h` | absorbed into `gbuffer.cpp` |
| `win32lib/` (directory) | sources directly in `RedAlert/` |
| `linuxlib/` (directory) | same |
| `extern LPDIRECTDRAW DirectDrawObject` | `extern RenderBackend *GRenderer` |
| `LPDIRECTDRAWSURFACE VideoSurfacePtr` | `RenderSurface` (via `render_backend.h`) |
| `HRESULT` as return type | `bool` |
| `PALETTEENTRY` | `struct { uint8_t r, g, b; }` |

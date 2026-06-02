# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.
Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

## Project Overview

This is a port of the Command & Conquer Remastered Collection (Red Alert and Tiberian Dawn games + map editor) from Windows-only (DirectDraw/Win32) to cross-platform (SDL3 + Linux/Windows, targeting Mac compatibility if feasible). The project is educational and values pedagogical clarity over pure optimization.

**Target audience for code discussion**: PhD-level in non-CS fields with C/C++ hobby experience; familiar with Python/Java in industry; gaps in low-level systems programming.
This is mainly an educational project, so explaination and guidance are welcome, but the user write the code himself.

## Build System & Commands

### CMake (primary build system)
```bash
# Build all targets (RedAlert, TiberianDawn libraries)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

# Build single target
cmake --build build --target redalert --parallel $(nproc)
cmake --build build --target tiberiandawn --parallel $(nproc)

# Debug build with symbols
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Key settings**:
- C++20 standard, extensions disabled (`-std=c++20 -fno-extensions`)
- Hidden visibility by default (`-fvisibility=hidden`), selectively exported via `DLLEXPORT`
- `-fpermissive` flag enabled (loose Win32 type compatibility)
- Platform detection: `WIN32` define on Windows, `LINUX` define on Linux

### Zig Build (experimental alternative)
```bash
zig build
zig build run
```

The Zig build mirrors CMake but uses Zig's build system. It's optional and not the primary path.

## Directory Structure

```
.
├── RedAlert/              # Red Alert game library
│   ├── *.cpp / *.h       # ~534 game logic files (portable)
│   ├── win32lib/          # Win32/DirectDraw abstraction (to be replaced)
│   ├── linuxlib/          # First Linux approach (IGNORE — not viable)
│   ├── Resource/          # Assets
│   └── CMakeLists.txt
├── TiberianDawn/          # Tiberian Dawn game library (similar structure)
├── CnCTDRAMapEditor/      # C# map editor (uses .NET, separate port later)
├── plan/                  # Porting roadmaps
│   ├── redalert_porting_plan_en.md    # Detailed step-by-step port plan
│   └── ra_launcher_context_en.md      # Launcher architecture guide
├── ra_launcher_interface.h # C ABI interface for launcher to load .so/.dll
├── build.zig              # Zig build config
└── CMakeLists.txt         # Root CMake config
```

**Files to ignore**:
- Anything under `RedAlert/linuxlib/` or `TiberianDawn/linuxlib/` — first abandoned approach
- `.cache/` — clangd cache
- `*.sln` / `.vcxproj` — original Win32 MSVC project files (not maintained)

## Port Roadmap: Red Alert (Primary Effort)

The porting plan in `plan/redalert_porting_plan_en.md` defines the exact order of work. **Read this before starting any Red Alert changes.** Key phases:

### Phase 1: Prepare Portable Headers
1. **`wwstd.h`** — Remove unconditional `#include <windows.h>` and `#define WIN32 1`. Replace Win32 types (`BOOL`, `BYTE`, `DWORD`, etc.) with portable typedefs from `<stdint.h>` under `#ifndef _WIN32` guards.

### Phase 2: Graphics Abstraction Layer
2. **Create `render_backend.h`** — New file defining `RenderSurface` and `RenderBackend` structs to isolate SDL3/DirectDraw behind a stable interface. Avoids exposing `SDL_Texture*` directly in game code.

### Phase 3: Core Rendering Port
3. **`gbuffer.h` / `gbuffer.cpp`** — Replace DirectDraw dependencies (`LPDIRECTDRAW`, `LPDIRECTDRAWSURFACE`, `HRESULT`) with SDL3 equivalents and `RenderBackend`. Public interface (`Clear`, `Fill_Rect`, `Blit`, etc.) remains unchanged.
4. **Delete `ddraw.cpp` / `ddraw.h`** — Content absorbed into `gbuffer.cpp`.

### Phase 4: Minor Win32 Leaks
5. **Port utility headers** — `timer.h`, `keyboard.h`, `mouse.h`, `palette.h`, `misc.h`. Remove `HWND`/`HANDLE` types, `timeSetEvent`, `VK_*` keycodes, replace with SDL3 equivalents.

### Phase 5: Inline Assembly
6. **`drawmisc.cpp`** — Port 21 MSVC inline ASM blocks (`__asm { }`) to GAS inline format (`__asm__ volatile`) compilable with GCC/Clang on Windows and Linux.
7. **`.asm` files** → GAS `.S` files:
   - `lcwcomp.asm` — LCW compression (Lempel-Ziv Westwood variant)
   - `lcwuncmp.asm` — LCW decompression
   - `tobuff.asm` — Rectangular buffer copy with stride

### Phase 6: Finalization
8. **Simplify CMakeLists.txt** — Remove `if(WIN32)` / `if(LINUX)` platform branches on sources. Unified build for both platforms.

**Portable files** (no changes): `iff.cpp`, `font.cpp`, `drawrect.cpp`, `buffer.cpp`, `delay.cpp`, etc. — all pure algorithm, no platform deps.

## Launcher Architecture (Linux/Windows)

The game DLL/SO exports a C ABI interface defined in `ra_launcher_interface.h`. A custom launcher (to be written) loads the SO/DLL via `libdl` / LoadLibrary and drives the simulation.

**Launcher responsibilities**:
- Load `RedAlert.so` via `dlopen` / `dlsym`
- Call `CNC_Version()` to verify API compatibility (`0x102`)
- Drive render loop: `CNC_Advance_Instance()` → `CNC_Get_Visible_Page()` (8-bit paletted) → palette-to-RGBA conversion → SDL3 present
- Handle input: SDL events → `CNC_Handle_Input()`
- Decode audio: `.aud` format → PCM → `SDL_AudioStream`
- Show UI: briefing screens, game-over, etc. via event callbacks

**Critical**: All shared structs use `#pragma pack(1)` — binary layout is a fixed contract. Never add/remove fields without verifying sizeof matches the DLL.

See `plan/ra_launcher_context_en.md` for full interface spec, struct layouts, and examples.

## Key Porting Concepts

### Win32 ↔ SDL3 Mapping

| Win32 | SDL3 |
|-------|------|
| `DirectDraw` + `LPDIRECTDRAW` | `SDL_Renderer` |
| `LPDIRECTDRAWSURFACE` | `SDL_Texture` |
| `HWND` window handle | `SDL_Window*` |
| `HRESULT` COM return | `bool` or `int` |
| `GetCursorPos()` / `ClipCursor()` | `SDL_GetMouseState()` / `SDL_SetWindowMouseGrab()` |
| `VK_*` keycodes + `WM_*` messages | `SDL_Keycode` + `SDL_Event` |
| `timeSetEvent()` / `timeBeginPeriod()` | `SDL_AddTimer()` or POSIX `clock_gettime()` |
| `PALETTEENTRY` Win32 type | `struct { uint8_t r, g, b; }` |
| `__asm { }` MASM inline | `__asm__ volatile` GAS inline |

### Platform Defines
```cpp
#ifdef _WIN32
    // Windows-specific code
#endif

#ifdef LINUX  // Note: CMAKE defines LINUX on Linux, not _WIN32 negation
    // Linux-specific code
#endif
```

### Extern "C" / ABI
All exported interface functions use `extern "C"` and `__cdecl` (no-op on Linux). On Linux, `__declspec(dllexport)` becomes `__attribute__((visibility("default")))` with CMake's `-fvisibility=hidden`.

## Code Style & Conventions

- **C++20** features OK; avoid C++23+ for broader compiler compatibility.
- **Formatting**: Use `.clang-format` and `.clang-tidy` (already present in repo).
- **ASM**: When porting inline ASM, preserve the algorithm intent—don't optimize beyond scope.
- **No premature abstraction**: Fix bugs without surrounding cleanup. Keep assembly pedagogical, not minimal.
- **Comments**: Minimal; only explain *why*, not *what*. ASM intent should be clear from mnemonics.

## Testing & Validation

Currently no integrated test suite. Validation is manual:
1. **Game loads and runs** without crashes or rendering errors.
2. **Assembly functions** verify round-trip: compress/decompress, buffer copy with stride.
3. **Input/rendering on Linux and Windows** matches expected behavior.
4. **Shared struct sizes** via `static_assert` match DLL layout.

To verify struct size: add `printf("sizeof CNCPlayerInfoStruct: %zu\n", ...)` in the game DLL at startup, then encode as `static_assert` in launcher.

## Platform-Specific Considerations

### Windows
- Continue to build as `.dll` (shared library).
- DirectDraw/Win32 APIs available; no code changes needed in `win32lib/` until port begins.
- MSVC compiler supports `__declspec`, inline MASM ASM, `/W4` warnings.

### Linux
- Build as `.so` (shared library).
- SDL3 development libraries required (`libsdl3-dev` or similar).
- GCC/Clang compile with `-fvisibility=hidden`, GAS inline ASM, `-Wall -Wextra`.
- X11/Wayland display servers supported by SDL3.

### Mac (stretch goal)
- Not prioritized; GCC/Clang + SDL3 can build for ARM64/x86_64.
- No platform-specific code in porting roadmap, but assembly may need per-target handling.

## Educational Goal

This port emphasizes:
- **Clarity over cleverness**: explain *why* interfaces are designed the way they are.
- **Preserve pedagogical value**: assembly stays readable and intentional, not golfed.
- **Minimal C++ features**: use language features to aid understanding, not to flex the compiler.

When teaching a concept (e.g., why structs use `#pragma pack(1)`, why render backends abstract texture types), prefer short code snippets + explanation over monolithic implementations.

## Related Documentation

- **Porting Plan**: `plan/redalert_porting_plan_en.md` — detailed order of work for Red Alert.
- **Launcher Guide**: `plan/ra_launcher_context_en.md` — full DLL interface spec, struct layouts, lifecycle.
- **Original License**: `LICENSE.md` — GPL v3 with additional terms (EA preservation license).
- **Interface Header**: `ra_launcher_interface.h` — C ABI for launcher to load and drive the game.

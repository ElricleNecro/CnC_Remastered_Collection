# Unported Code in RedAlert/gbuffer.h and gbuffer.cpp

## Summary
Total items to resolve: **~19 blocking items**

These are all the remaining DirectDraw and assembly code that must be ported before compilation succeeds.

---

## BLOCKING - DirectDraw Surface Management

### 1. DD_Init() - Lines 230-269 (gbuffer.cpp)
Entire function uses DirectDraw APIs that need SDL3 equivalents:
- DDSURFACEDESC structure initialization
- CreateSurface() calls  
- Surface flags: DDSD_*, DDSCAPS_*
- DirectDrawObject reference (external global)
- AllSurfaces.Add_DD_Surface() call (external global)

**Action:** Replace with SDL3 surface creation in RenderBackend

---

### 2. Un_Init() - Lines 339-356 (gbuffer.cpp)
DirectDraw surface cleanup:
- VideoSurfacePtr->Unlock()
- AllSurfaces.Restore_Surfaces() (external)
- VideoSurfacePtr->Release()
- Focus loss handling

**Action:** Replace with SDL3 cleanup; may not be needed if RenderBackend handles it

---

### 3. Attach_DD_Surface() - Lines 271-272 (gbuffer.cpp)
DirectDraw surface attachment:
- VideoSurfacePtr->AddAttachedSurface()

**Action:** Determine if needed for SDL3; may be no-op

---

### 4. Lock() - Lines 498-568 (gbuffer.cpp)
DirectDraw surface locking with complex logic:
- VideoSurfacePtr->Lock(NULL, &VideoSurfaceDescription, DDLOCK_WAIT, NULL)
- DDSURFACEDESC lpSurface, lPitch member access
- Surface restoration on DDERR_SURFACELOST
- AllSurfaces.Restore_Surfaces() (external)
- Block_Mouse() / Unblock_Mouse() calls (external)

**Action:** Replace with SDL3 surface locking or use RenderBackend->lock()

---

### 5. Unlock() - Lines 582-610 (gbuffer.cpp)
DirectDraw surface unlocking:
- VideoSurfacePtr->Unlock(NULL)
- Block_Mouse() / Unblock_Mouse() calls (external)

**Action:** Replace with SDL3 surface unlocking or use RenderBackend->unlock()

---

## BLOCKING - Assembly Function Calls

These are C-callable functions that are referenced but definitions are elsewhere (likely in .asm files).

### 6. Buffer_Size_Of_Region() - Line 396 (gbuffer.h)
```cpp
inline long Size_Of_Region(int w, int h) const {
    return Buffer_Size_Of_Region(this, w, h);
}
```

### 7. Buffer_Put_Pixel() - Line 413 (gbuffer.h)
```cpp
inline void Put_Pixel(int x, int y, unsigned char color) {
    Buffer_Put_Pixel(this, x, y, color);
}
```

### 8. Buffer_Get_Pixel() - Line 434 (gbuffer.h)
```cpp
inline unsigned char Get_Pixel(int x, int y) const {
    return Buffer_Get_Pixel(this, x, y);
}
```

### 9. Buffer_Clear() - Line 454 (gbuffer.h)
```cpp
inline void Clear(unsigned char color = 0) {
    Buffer_Clear(this, color);
}
```

### 10. Buffer_To_Buffer() - Lines 474, 516 (gbuffer.h)
Multiple variants calling Buffer_To_Buffer()

### 11. Buffer_Print() - Lines 686, 709, 733, 757 (gbuffer.h)
Multiple variants calling Buffer_Print()

### 12. Buffer_Draw_Stamp() - Line 777 (gbuffer.h)
```cpp
inline void Draw_Stamp(...) {
    Buffer_Draw_Stamp(this, icondata, icon, x_pixel, y_pixel, remap);
}
```

### 13. Buffer_Draw_Stamp_Clip() - Line 825 (gbuffer.h)
Clipped variant of stamp drawing

### 14. Buffer_Remap() - Lines 919, 944 (gbuffer.h)
```cpp
inline void Remap(char *remap) {
    Buffer_Remap(this, 0, 0, Width, Height, remap);
}
```

### 15. Buffer_Fill_Quad() - Line 926 (gbuffer.h)

**Action:** Find .asm definitions or implement in C++. Some may already be replaced by RenderBackend methods.

---

## BLOCKING - Icon Caching with DirectDraw

### 16. Draw_Stamp() Icon Cache Path - Lines 795-838 (gbuffer.h)
DirectDraw-specific icon rendering:
```cpp
if (IsDirectDraw) {
    if (Is_Icon_Cached(icondata, icon)) {
        CachedIcons[cache_index].Draw_It(
            GraphicBuff->Get_DD_Surface(),  // Doesn't exist!
            x_pixel, y_pixel,
            WindowList[clip_window][WINDOWX] + XPos,
            ...
        );
    }
}
```

**Issues:**
- `Get_DD_Surface()` method doesn't exist in current RenderSurface
- `CachedIcons[]` global not defined
- `WindowList[][]` global not defined
- `Is_Icon_Cached()` function not found

**Action:** Either remove icon caching entirely or reimplement for SDL3

---

## UNRESOLVED EXTERNAL GLOBALS

These globals are referenced but never defined in gbuffer.cpp/gbuffer.h:

| Variable | References | Purpose |
|----------|-----------|---------|
| `DirectDrawObject` | cpp:257 | DirectDraw interface object |
| `AllSurfaces` | cpp:258, 347, 351, 555 | DirectDraw surface tracking/management |
| `PaletteSurface` | cpp:261 | Primary palette surface |
| `CachedIcons[]` | h:807, 825 | Icon cache objects |
| `WindowList[][]` | h:811, 831 | Window geometry data |
| `Gbuffer_Focus_Loss_Function` | h:132, cpp:344 | Focus loss callback |
| `GameInFocus` | cpp:493 | Focus state flag |
| `Block_Mouse()` | cpp:520, 548, 566, 596 | Mouse blocking function |
| `Unblock_Mouse()` | cpp:520, 548, 566, 596 | Mouse unblocking function |
| `Is_Icon_Cached()` | h:803 | Icon cache lookup |
| `IconCacheAllowed` | h:795 | Icon caching enabled flag |
| `CachedIconsDrawn` | h:815, 824 | Debug counter |
| `UnCachedIconsDrawn` | h:815, 824 | Debug counter |

**Action:** Either define these or remove references if no longer needed

---

## TYPE MISMATCHES & DECLARATIONS

### 17. VideoSurfacePtr Type Mismatch
**Currently:** `std::unique_ptr<rendering::RenderSurface>`
**Code expects:** `LPDIRECTDRAWSURFACE` (DirectDraw type)
**References:** cpp:257, 261, 271, 343, 344, 347, 351, 352, 538, 541, 542, 543, 551, 555, 597

**Action:** Either keep as RenderSurface or determine correct type

---

### 18. VideoSurfaceDescription Not Declared
**Type:** `DDSURFACEDESC` (DirectDraw struct)
**References:** cpp:234-251, 371, 538, 542-543
**Issue:** Member variable not declared in GraphicBufferClass header

**Action:** Remove or replace with SDL3 equivalent

---

### 19. DirectDraw Constants & Types Used
- `HRESULT` return type (cpp:499)
- `DDLOCK_WAIT` flag (cpp:538)
- `DD_OK` constant (cpp:541)
- `DDERR_SURFACELOST` constant (cpp:551, 343)

**Action:** Replace with SDL3 equivalents or remove

---

## Porting Strategy

### Phase 1 - Remove/Stub Out DirectDraw:
- [ ] Remove DD_Init() function or stub it to no-op
- [ ] Remove Un_Init() function or stub it
- [ ] Remove Attach_DD_Surface() or stub it
- [ ] Replace Lock/Unlock with SDL3 versions or use RenderBackend
- [ ] Remove Block_Mouse/Unblock_Mouse calls (or keep if needed)

### Phase 2 - Assembly Functions:
- [ ] Find or implement Buffer_* functions
- [ ] Or replace calls with C++ equivalents using RenderBackend

### Phase 3 - Icon Caching:
- [ ] Either remove IsDirectDraw path entirely
- [ ] Or reimplement using SDL3 surfaces

### Phase 4 - Unresolved Externals:
- [ ] Define missing globals or remove references
- [ ] Replace DirectDraw-specific callbacks with SDL3 equivalents

---

## Compilation Blockers Summary

**Before compilation will succeed, must resolve:**
1. DD_Init() - DirectDraw initialization
2. Un_Init() - DirectDraw cleanup
3. Lock() - DirectDraw locking
4. Unlock() - DirectDraw unlocking
5. All Buffer_* function calls (12 functions)
6. Icon cache DirectDraw path
7. Type mismatches for VideoSurfacePtr
8. Unresolved external globals
9. DirectDraw constants and types

**Estimated unported lines of code:** ~150-200 lines

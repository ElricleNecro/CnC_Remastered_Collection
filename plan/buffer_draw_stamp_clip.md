# Porting `Buffer_Draw_Stamp_Clip` from MASM to C++

## Context

`Buffer_Draw_Stamp_Clip` is declared in `drawbuff.h` and was originally implemented
in `win32lib/drawmisc.cpp` as a MASM inline `__asm {}` block (~200 lines of x86 assembly).
Its only callers are the two `Draw_Stamp` inline methods in `gbuffer.h`.

The function draws one tile ("stamp") from an icon set into a locked CPU pixel buffer,
respecting a clip window and an optional per-pixel colour remap table.

---

## Icon Set Binary Format (`IControl_Type`)

An icon set is a single contiguous memory blob. The first bytes are an `IControl_Type`
header (defined in `tile.h`):

```cpp
typedef struct {
    short Width;      // width of every icon in pixels
    short Height;     // height of every icon in pixels
    short Count;      // number of logical icons in the set
    short Allocated;  // (ignored here)
    short MapWidth;   // (ignored here)
    short MapHeight;  // (ignored here)
    long  Size;       // total blob size in bytes (ignored here)
    long  Icons;      // byte offset from blob start → packed pixel data
    long  Palettes;   // (ignored here)
    long  Remaps;     // (ignored here)
    long  TransFlag;  // byte offset from blob start → transparency flag table
    long  ColorMap;   // (ignored here)
    long  Map;        // byte offset from blob start → logical-to-physical remap table
                      // 0 if not present
} IControl_Type;
```

Three sub-arrays inside the blob matter to this function:

| Field | What it points to | Element type | Length |
|---|---|---|---|
| `Icons` | Packed pixel data for all icons | `uint8_t` | `Count × Width × Height` bytes |
| `TransFlag` | One flag per icon; `0` = fully opaque | `uint8_t` | `Count` bytes |
| `Map` | Logical-to-physical icon number table | `uint8_t` | `Count` bytes (0 = not present) |

All offsets are **from the start of the blob** (i.e. from the `IControl_Type *` pointer
itself), not from the end of the header.

Pixel data is stored row-major, tightly packed with no padding:

```
blob + hdr->Icons + icon_index * (Width * Height)   → first pixel of icon N
blob + hdr->Icons + icon_index * (Width * Height)
                  + row * Width + col               → pixel at (col, row)
```

Pixel value `0` is always the transparent colour (palette index 0 = black, conventionally
unused as a visible tile colour).

---

## Function Signature

```cpp
void __cdecl Buffer_Draw_Stamp_Clip(
    void const *this_object,   // GraphicViewPortClass* (locked viewport)
    void const *icondata,      // IControl_Type* (icon set blob)
    int         icon,          // logical icon number to draw
    int         x_pixel,       // destination X, relative to clip window origin
    int         y_pixel,       // destination Y, relative to clip window origin
    void const *remap,         // 256-byte colour remap table, or nullptr
    int         min_x,         // clip window: left edge (viewport-relative)
    int         min_y,         // clip window: top edge  (viewport-relative)
    int         max_x,         // clip window: width  (NOT right edge — see below)
    int         max_y          // clip window: height (NOT bottom edge — see below)
);
```

**Critical parameter convention** — `max_x` and `max_y` are **sizes**, not coordinates.
The assembly converts them to absolute right/bottom edges in its first step:

```
abs_right  = min_x + max_x
abs_bottom = min_y + max_y
```

And simultaneously converts `x_pixel`/`y_pixel` from clip-window-relative to
viewport-relative:

```
x_pixel += min_x
y_pixel += min_y
```

After this conversion all coordinates are in the same space (viewport pixels).

---

## Step-by-Step Algorithm

### Step 1 — Guard and header parse

```cpp
if (!icondata) return;
auto *hdr = static_cast<IControl_Type const *>(icondata);
auto *base = static_cast<uint8_t const *>(icondata);

int icon_w     = hdr->Width;
int icon_h     = hdr->Height;
int icon_count = hdr->Count;

auto *stamp    = base + hdr->Icons;
auto *is_trans = base + hdr->TransFlag;
auto *map      = hdr->Map ? base + hdr->Map : nullptr;
```

### Step 2 — Logical to physical icon number

Some icon sets have a `Map` table that remaps the logical icon number (what the game
code uses) to a physical slot in the pixel data array. If present, apply it:

```cpp
if (map) icon = map[icon];
if (icon >= icon_count) return;
```

The bounds check happens **after** the remap, on the physical index.

### Step 3 — Convert clip parameters to absolute viewport coordinates

As described above, `min_x`/`min_y` are the window origin and `max_x`/`max_y` are its
size. Convert all four to absolute viewport coordinates, and convert the draw position
to the same space in one operation:

```cpp
max_x  += min_x;   // now: absolute right edge of clip window
max_y  += min_y;   // now: absolute bottom edge of clip window
x_pixel += min_x;  // now: viewport-relative draw X
y_pixel += min_y;  // now: viewport-relative draw Y
```

### Step 4 — Early-out if fully outside clip window

```cpp
if (x_pixel >= max_x || y_pixel >= max_y)           return;
if (x_pixel + icon_w <= min_x)                       return;
if (y_pixel + icon_h <= min_y)                       return;
```

### Step 5 — Clip source pointer and draw dimensions

This is the most delicate part. We have three quantities to track:

- `src` — pointer to the first pixel we will actually read
- `iwidth` — number of columns to draw per row (starts at `icon_w`, shrinks on left/right clip)
- `rows` — number of rows to draw (starts at `icon_h`, shrinks on top/bottom clip)
- `skip` — bytes to advance `src` at the end of each row to skip over the clipped-away
  right portion of the icon (always `icon_w - iwidth` after all x-clipping is done)

**Left clip** (`x_pixel < min_x`): the icon starts to the left of the window.
We skip the first `(min_x - x_pixel)` columns of each source row:

```cpp
auto *src = stamp + icon * icon_w * icon_h;
int iwidth = icon_w;
int rows   = icon_h;

if (x_pixel < min_x) {
    int clip = min_x - x_pixel;
    src     += clip;        // skip first `clip` pixels in row 0
    iwidth  -= clip;
    x_pixel  = min_x;
}
```

**Right clip** (`x_pixel + iwidth > max_x`): the icon extends past the right edge.
Reduce `iwidth`; `src` is unaffected (we simply stop copying early):

```cpp
if (x_pixel + iwidth > max_x)
    iwidth = max_x - x_pixel;
```

**`skip` computation** — now that iwidth is final, the number of source bytes to step
over at the end of each row is:

```cpp
int skip = icon_w - iwidth;
```

This is non-zero whenever either left or right clipping occurred.

**Top clip** (`y_pixel < min_y`): the icon starts above the window.
Skip entire rows of source data:

```cpp
if (y_pixel < min_y) {
    int clip = min_y - y_pixel;
    src     += clip * icon_w;   // skip `clip` full rows of width icon_w (not iwidth)
    rows    -= clip;
    y_pixel  = min_y;
}
```

Note: the row skip uses `icon_w`, not `iwidth`, because the source data has full rows
regardless of any left-clipping — we jumped over only columns within each row above.

**Bottom clip** (`y_pixel + rows > max_y`): the icon extends below the window:

```cpp
if (y_pixel + rows > max_y)
    rows = max_y - y_pixel;

if (iwidth <= 0 || rows <= 0) return;
```

### Step 6 — Compute destination pointer

The viewport is already locked; `Get_Offset()` returns the address of its top-left pixel.
The stride (bytes per row) accounts for the viewport's `XAdd` and `Pitch` fields:

```cpp
auto *view = static_cast<GraphicViewPortClass const *>(this_object);
int stride = view->Get_Width() + view->Get_XAdd() + view->Get_Pitch();
auto *dst  = reinterpret_cast<uint8_t *>(view->Get_Offset())
             + y_pixel * stride + x_pixel;
int modulo = stride - iwidth;   // bytes to advance dst at end of each row
```

`modulo` is stride minus the number of columns we write, so adding it after writing
`iwidth` pixels advances `dst` to the start of the next row's destination position.

### Step 7 — Pixel copy loops (three variants)

The assembly has three distinct paths, chosen at runtime:

#### A — With remap table

Apply `remap[src_pixel]` and treat the remapped value `0` as transparent:

```cpp
auto *remap8 = static_cast<uint8_t const *>(remap);
for (int row = 0; row < rows; row++, dst += modulo, src += skip) {
    for (int col = 0; col < iwidth; col++, dst++, src++) {
        uint8_t px = remap8[*src];
        if (px) *dst = px;
    }
}
```

Transparency is checked **after** remapping, so it is possible to remap a visible colour
to palette index 0, making it invisible. The assembly comment calls this a "special effect
reason".

#### B — No remap, fully opaque icon (`is_trans[icon] == 0`)

When `TransFlag` says the icon has no transparent pixels at all, skip the per-pixel
transparency test and use `memcpy`:

```cpp
for (int row = 0; row < rows; row++, dst += modulo, src += skip)
    std::memcpy(dst, src, iwidth);
```

The original assembly had a hand-rolled dword-alignment optimisation here (`test edi,3` /
`movsb` until aligned, then `rep movsd`). With a modern compiler and `memcpy`, the
compiler generates equivalent or better code.

#### C — No remap, icon has transparent pixels

Per-pixel copy skipping zeros:

```cpp
for (int row = 0; row < rows; row++, dst += modulo, src += skip) {
    for (int col = 0; col < iwidth; col++, dst++, src++) {
        if (*src) *dst = *src;
    }
}
```

---

## `Buffer_Draw_Stamp` (unclipped variant)

`Buffer_Draw_Stamp` is a thin wrapper that calls `Buffer_Draw_Stamp_Clip` with a clip
window equal to the full viewport, so no clipping occurs in practice:

```cpp
void __cdecl Buffer_Draw_Stamp(void const *this_object, void const *icondata,
                                int icon, int x_pixel, int y_pixel, void const *remap)
{
    auto *view = static_cast<GraphicViewPortClass const *>(this_object);
    Buffer_Draw_Stamp_Clip(this_object, icondata, icon, x_pixel, y_pixel, remap,
                           0, 0, view->Get_Width(), view->Get_Height());
}
```

Because `min_x = min_y = 0`, the coordinate conversion in Step 3 is a no-op for the
draw position. `max_x = Get_Width()` and `max_y = Get_Height()` become the right/bottom
edges after the `max += min` additions, giving the full `[0, Width) × [0, Height)`
viewport as the clip region.

The original unclipped function did not clip at all and would write outside the locked
buffer for out-of-bounds coordinates. Delegating to the clipped variant is strictly
safer at negligible cost.

---

## What this function does NOT touch

- No SDL3 API calls. All work is CPU writes into a locked pixel buffer obtained via
  `Get_Offset()`.
- No palette conversion. Pixels are 8-bit palette indices throughout; the renderer
  handles palette→RGBA at present time.
- No `RenderSurface` or `RenderBackend` involvement. The lock/unlock wrapping is done
  by the `Draw_Stamp` inline methods in `gbuffer.h` before this function is called.

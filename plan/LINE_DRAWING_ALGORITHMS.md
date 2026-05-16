# Line Drawing Algorithms: Current Implementation & Alternatives

## Current Implementation (Win32 Assembly)

The existing `Buffer_Draw_Line` in `win32lib/drawmisc.cpp` implements:

1. **Bresenham's Line Algorithm** — The core line rasterization
2. **Cohen-Sutherland Line Clipping** — Clips lines to viewport bounds before drawing
3. **Horizontal Line Optimization** — Fast path for axis-aligned horizontal lines using `rep stosb` (repeat store byte)

### Why This Matters
- The game can draw lines at any angle, but only pixels within the viewport should be drawn
- Clipping prevents artifacts and crashes from out-of-bounds writes
- The optimization for horizontal lines makes common UI elements (grid lines, borders) very fast

---

## Line Drawing Algorithms Explained

### 1. Bresenham's Line Algorithm

**What it does:** Converts a mathematical line into discrete pixel coordinates using only integer arithmetic.

**Why it works:** Instead of calculating `y = mx + b` for each x (floating-point), Bresenham tracks an **error accumulator** that decides whether to step down or right.

**Pseudocode:**
```
function bresenham(x0, y0, x1, y1, color):
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    sx = sign(x1 - x0)  // +1 or -1 for direction
    sy = sign(y1 - y0)
    
    if dx > dy:
        // Line is more horizontal than vertical
        error = dx / 2
        y = y0
        for x from x0 to x1 (step sx):
            plot(x, y, color)
            error -= dy
            if error < 0:
                y += sy
                error += dx
    else:
        // Line is more vertical than horizontal
        error = dy / 2
        x = x0
        for y from y0 to y1 (step sy):
            plot(x, y, color)
            error -= dx
            if error < 0:
                x += sx
                error += dy
```

**Pros:**
- ✅ Integer-only (no floating-point)
- ✅ Very fast
- ✅ Produces connected pixels (no gaps)

**Cons:**
- ❌ Pixels are not anti-aliased (jagged edges on diagonal lines)

**Example:** Line from (0,0) to (5,2)
```
. . . . . X
. . . X X .
X X X . . .
```
Notice the "staircase" pattern — that's Bresenham.

---

### 2. DDA (Digital Differential Analyzer)

**What it does:** Simulates moving along the line with small steps, using floating-point slope.

**Pseudocode:**
```
function dda(x0, y0, x1, y1, color):
    steps = max(abs(x1-x0), abs(y1-y0))
    x_step = (x1 - x0) / steps
    y_step = (y1 - y0) / steps
    
    x = x0
    y = y0
    for i from 0 to steps:
        plot(round(x), round(y), color)
        x += x_step
        y += y_step
```

**Pros:**
- ✅ Simple to understand
- ✅ Handles all angles equally

**Cons:**
- ❌ Floating-point arithmetic (slower, precision issues)
- ❌ Rounding errors can create gaps in long lines

**When to use:** Educational purposes, or when simplicity > performance.

---

### 3. Xiaolin Wu's Anti-Aliased Line Algorithm

**What it does:** Draws lines with smooth edges by plotting pixels at fractional intensities (anti-aliasing).

**Core idea:** When a line passes through pixels, blend the color based on how close the line is to each pixel center.

**Pseudocode (simplified):**
```
function xiaolin_wu(x0, y0, x1, y1, color):
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    
    if dx > dy:
        // More horizontal
        slope = dy / dx
        y = y0
        y_fraction = 0  // Fractional part of y
        
        for x from x0 to x1:
            intensity1 = 1 - y_fraction  // Upper pixel intensity
            intensity2 = y_fraction       // Lower pixel intensity
            
            plot(x, floor(y), blend(color, intensity1))
            plot(x, floor(y)+1, blend(color, intensity2))
            
            y_fraction += slope
            if y_fraction >= 1:
                y_fraction -= 1
    else:
        // Similar for vertical lines
```

**Pros:**
- ✅ Smooth, anti-aliased edges
- ✅ Better visual quality

**Cons:**
- ❌ Requires color blending/alpha support
- ❌ Slower (multiple pixels per step)
- ❌ Overkill for low-resolution retro games

**Visual comparison:**
```
Bresenham (jagged):     Xiaolin Wu (smooth):
. . . . X              . . . . ◐
. . . X .              . . . ◑ .
. X X . .              . ◑ ◑ . .
X . . . .              ◐ . . . .
```
(◐ = semi-transparent, ◑ = darker blend)

---

## Recommendation: **Bresenham's Algorithm**

**Why Bresenham is the best choice for this project:**

1. **Performance**: Integer-only arithmetic, same speed as the original assembly
2. **Visual fidelity**: No visible difference on 320×240 / 640×480 resolution
3. **Simplicity**: Easier to port from assembly, no complex blending logic
4. **Compatibility**: Works directly with 8-bit paletted pixel format (no alpha needed)
5. **Proven**: Used in the original, trusted algorithm

**The only catch:** You must **clip lines before drawing** to prevent out-of-bounds writes. This is why the original uses **Cohen-Sutherland clipping**.

---

## Cohen-Sutherland Line Clipping

**What it does:** Clips a line to a rectangular viewport before drawing, eliminating segments outside bounds.

**How it works:**
1. Divide the plane into 9 regions (3×3 grid around the viewport)
2. Assign each endpoint a 4-bit code: `[above][below][left][right]`
3. Use the codes to quickly reject/accept/subdivide lines

**Pseudocode:**
```
function cohen_sutherland_clip(x0, y0, x1, y1, xmin, xmax, ymin, ymax):
    function compute_code(x, y):
        code = 0
        if y > ymax: code |= 1  // Above
        if y < ymin: code |= 2  // Below
        if x < xmin: code |= 4  // Left
        if x > xmax: code |= 8  // Right
        return code
    
    code0 = compute_code(x0, y0)
    code1 = compute_code(x1, y1)
    
    while true:
        if code0 == 0 and code1 == 0:
            return FULLY_INSIDE  // Draw line as-is
        
        if (code0 & code1) != 0:
            return FULLY_OUTSIDE  // Line completely outside
        
        // Line partially intersects; find intersection with boundary
        if code0 != 0:
            // Clip start point
            code = code0
            x_new, y_new = compute_intersection(x0, y0, x1, y1, code, xmin, xmax, ymin, ymax)
            x0, y0 = x_new, y_new
            code0 = compute_code(x0, y0)
        else:
            // Clip end point
            code = code1
            x_new, y_new = compute_intersection(x0, y0, x1, y1, code, xmin, xmax, ymin, ymax)
            x1, y1 = x_new, y_new
            code1 = compute_code(x1, y1)
```

**Key insight:** The codes allow **very fast rejection** — if `(code0 & code1) != 0`, both points are on the same side of the clip region, so the line is fully outside.

---

## Summary Table

| Algorithm | Speed | Quality | Complexity | Best For |
|-----------|-------|---------|------------|----------|
| **Bresenham** | ⭐⭐⭐ Very Fast | Good (jagged) | Low | **✓ This project** |
| **DDA** | ⭐⭐ Moderate | Good (jagged) | Very Low | Educational |
| **Xiaolin Wu** | ⭐ Slow | Excellent (smooth) | High | High-res graphics |

---

## Implementation Plan

**Phase 1: Bresenham Core**
- Port the Bresenham algorithm from assembly to portable C++
- Handle all 8 octants (lines in all directions)
- No clipping initially

**Phase 2: Cohen-Sutherland Clipping**
- Add viewport clipping logic
- Ensure no out-of-bounds pixel writes

**Phase 3: Integration**
- Connect to renderer abstraction (pixel filling via SDL)
- Support 8-bit palette colors

**Phase 4: Optimization**
- Add horizontal/vertical line fast path (currently done in assembly)
- Consider caching clipping bounds

---

## References

- **Bresenham's Line Algorithm**: Classic computer graphics algorithm (1965), still industry standard
- **Cohen-Sutherland Clipping**: Standard line clipping (1967), also still widely used
- **Xiaolin Wu Anti-aliasing**: Modern approach for smooth lines, common in 2D graphics engines


# Red Alert — Plan de portage Win32 → SDL3 (Linux/Windows)

## Contexte

Portage de `RedAlert/win32lib/` vers une implémentation portable SDL3, supprimant les
répertoires `win32lib/` et `linuxlib/` au profit d'une couche SDL3 unique compilable
sur Windows et Linux sans `#ifdef` plateforme dans le code de jeu.

Le code de jeu (`RedAlert/*.cpp`) n'utilise jamais DirectDraw ni Win32 directement —
toutes les dépendances sont encapsulées dans `win32lib/`. L'interface publique de
`GraphicBufferClass` / `GraphicViewPortClass` est suffisamment générique pour rester
inchangée pour le code appelant.

---

## Ordre d'attaque

### 1. `wwstd.h` — Priorité absolue

**Problème :** contient `#define WIN32 1` et `#include <windows.h>` inconditionnels.
Tout le reste en dépend transitivement.

**Action :** remplacer par des typedefs portables via `<stdint.h>` sous guard `#ifndef _WIN32` :
- `BOOL`, `BYTE` (`uint8_t`), `WORD` (`uint16_t`), `DWORD` (`uint32_t`), `LONG`, `LPVOID`
- `TRUE` / `FALSE`

---

### 2. Couche d'abstraction rendu — `render_backend.h` (nouveau fichier)

**Contexte :** éviter d'exposer `SDL_Texture*` directement dans `gbuffer.h` pour faciliter
un éventuel portage futur vers OpenGL ou Vulkan sans toucher au code de jeu.
`SDL_Window` reste valide dans tous les cas ; `SDL_Renderer` et `SDL_Texture` disparaissent
si changement de backend.

**Action :** créer `render_backend.h` avec deux structs minimaux :

```cpp
struct RenderSurface {
    SDL_Texture *texture;   // remplacé ici uniquement si changement de backend
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

`GraphicBufferClass` contient un `RenderSurface`, pas un `SDL_Texture*` direct.

---

### 3. `gbuffer.h` / `gbuffer.cpp` — Cœur du portage

**Problème :** `GraphicBufferClass` et `GraphicViewPortClass` sont couplées à DirectDraw via :
- `#include <ddraw.h>`
- `extern LPDIRECTDRAW DirectDrawObject` (globale)
- `LPDIRECTDRAWSURFACE VideoSurfacePtr` (membre privé)
- `DDSURFACEDESC VideoSurfaceDescription` (membre privé)
- `LPDIRECTDRAWSURFACE Get_DD_Surface()` (méthode publique — fuite DDraw)
- `BOOL Get_IsDirectDraw()` (méthode publique)
- `HRESULT Blit(...)` (retour COM)

**Action :**
- Supprimer tous les éléments listés ci-dessus
- Remplacer `VideoSurfacePtr` par `RenderSurface` (voir étape 2)
- Remplacer `extern LPDIRECTDRAW DirectDrawObject` par `extern RenderBackend *GRenderer`
- `Get_DD_Surface()` → `Get_SDL_Texture()` ou supprimer (plus appelé une fois `drawmisc` porté)
- `Get_IsDirectDraw()` → supprimer ou retourner `false`
- `HRESULT Blit(...)` → `bool Blit(...)`
- `Set_Video_Mode` et gestion fenêtre migrent dans `gbuffer.cpp` via
  `SDL_CreateWindow` / `SDL_CreateRenderer`

**Interface publique inchangée pour le code appelant :**
`Clear`, `Fill_Rect`, `Draw_Line`, `Put_Pixel`, `Scale`, `Blit`, `Lock`/`Unlock`,
`Get_Width`/`Get_Height`/`Get_Offset`

**Note :** retirer l'`#include <iconcach.h>` de `gbuffer.h` (voir étape 7).

---

### 4. `ddraw.cpp` / `ddraw.h` — Suppression

**Action :** supprimer les deux fichiers. Leur contenu (`DirectDrawCreate`,
`QueryInterface`, gestion mode coopératif exclusif, surfaces DDraw) est absorbé
par `gbuffer.cpp` via SDL3.

---

### 5. Headers à réécrire (fuites Win32 légères)

Chacun devient portable sans `#ifdef` plateforme.

#### `timer.h` / `timerini.cpp`
- `HANDLE TimerThreadHandle` → supprimer ou `SDL_TimerID`
- `BOOL TimerSystemOn` → `bool`
- `timeSetEvent` / `timeBeginPeriod` (API multimedia Win32) → `SDL_AddTimer` ou `clock_gettime` POSIX

#### `keyboard.h`
- `VK_*` keycodes + `HWND` → `SDL_Keycode`
- `Message_Handler(HWND...)` → boucle événements SDL (`SDL_PollEvent`)

#### `mouse.h` / `mouseww.cpp`
- `GetCursorPos` / `ClipCursor` / `POINT` Win32 → `SDL_GetMouseState` / `SDL_SetWindowMouseGrab`

#### `palette.h` / `palette.cpp`
- `PALETTEENTRY` (type Win32) → struct portable `{ uint8_t r, g, b; }`
- Logique de palette 8bpp réutilisable telle quelle

#### `misc.h`
- Retirer `#include <ddraw.h>`
- `HWND MainWindow` → `SDL_Window *MainWindow`
- La globale `DirectDrawObject` migre dans `gbuffer` (voir étape 3)

---

### 6. `drawmisc.cpp` — Port des inline ASM MSVC

**Problème :** 21 blocs `__asm { }` syntaxe MSVC, rejetés par GCC/Clang.

**Catégories :**
- Triviales (`rep movsd`, `rep stosd`) → `memcpy` / `memset`
- Rendu (blit avec transparence, remap palette 256 entrées, scale nearest-neighbor) →
  réécriture en GAS inline (`__asm__ volatile`) compilable GCC/Clang Windows et Linux

**Syntaxe cible (GAS inline) :**
```cpp
__asm__ volatile (
    "rep movsl"
    : "+D"(dst), "+S"(src), "+c"(count)
    :: "memory"
);
```

Différences MASM → GAS : syntaxe AT&T, ordre des opérandes inversé, registres préfixés
`%`, taille explicite sur les mnémoniques (`movl`, `movb`...).

---

### 7. `iconcach.h` — Traitement différé

**Contexte :** `IconCacheClass` est un système de cache d'icônes 24×24 en VRAM DDraw.
`STMPCACH.ASM` est absent du repo — le cache est déjà désactivé, les fonctions ASM
sont des stubs vides dans `drawmisc.cpp`. Le système se dégrade silencieusement en
mode non-caché (fonctionnel, moins performant).

**Dépendances DDraw dans la classe :**
- `LPDIRECTDRAWSURFACE CacheSurface` (membre privé)
- `Draw_It(LPDIRECTDRAWSURFACE dest_surface, ...)` (méthode publique)

**Action :** retirer `#include <iconcach.h>` de `gbuffer.h` lors de l'étape 3.
Traiter `iconcach.h` lors du portage de `drawmisc.cpp` (étape 6) :
- Option A : supprimer la classe (cache déjà non fonctionnel)
- Option B : remplacer `LPDIRECTDRAWSURFACE` par `RenderSurface*` et inclure uniquement
  depuis `drawmisc.cpp`

---

### 8. Fichiers `.asm` MASM — Port en GAS

Trois fichiers à porter, conserver l'assembleur (objectif pédagogique).
Cible : GAS (GNU Assembler), fichiers `.S` séparés, compilables GCC/Clang Windows et Linux.
CMake switche uniquement le format objet (`-f win64` vs `-f elf64`), pas le source.

#### `lcwcomp.asm` — Compression LCW (Lempel-Ziv maison Westwood)
- `LCW_Compress(src, dst, size)` → taille compressée
- Algorithme pur, bon premier contact GAS (pas d'interaction structures C complexes)

#### `lcwuncmp.asm` — Décompression LCW
- `LCW_Uncompress(src, dst, length)` → taille décompressée

#### `tobuff.asm` — Copie rectangulaire avec stride
- `Buffer_To_Buffer` — manipulation buffer avec stride
- Un peu plus de contexte C à gérer que LCW

**Ordre recommandé :** `lcwcomp` → `lcwuncmp` → `tobuff` → blocs `drawmisc`

---

### 9. CMakeLists — Simplification finale

**Après toutes les étapes :**

```cmake
file(GLOB ra_srcs ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(redalert SHARED ${ra_srcs})
target_include_directories(redalert PRIVATE ${CMAKE_CURRENT_LIST_DIR})
find_package(SDL3 REQUIRED)
target_link_libraries(redalert PRIVATE SDL3::SDL3)
```

- Supprimer les branches `if(WIN32)` / `if(LINUX)` sur les sources et include dirs
- Supprimer `win32lib/` et `linuxlib/` intégralement

---

## Fichiers portables — aucune modification nécessaire

| Fichier | Contenu |
|---|---|
| `iff.cpp` / `iff.h` | Parseur format IFF |
| `dipthong.cpp` | Compression diphtongue |
| `_diptabl.cpp` | Table diphtongues |
| `font.cpp` / `loadfont.cpp` / `set_font.cpp` | Rendu et chargement polices |
| `getshape.cpp` | Accès données shapes |
| `iconset.cpp` | Gestion sets d'icônes |
| `writepcx.cpp` | Export PCX |
| `drawrect.cpp` | Dessin rectangles (logique pure) |
| `buffer.cpp` / `buffer.h` | Buffer système mémoire |
| `delay.cpp` | Délai (à vérifier `Sleep` vs `usleep`) |

---

## Récapitulatif des suppressions

| Supprimé | Remplacé par |
|---|---|
| `ddraw.cpp` / `ddraw.h` | absorbé dans `gbuffer.cpp` |
| `win32lib/` (répertoire) | sources directement dans `RedAlert/` |
| `linuxlib/` (répertoire) | idem |
| `extern LPDIRECTDRAW DirectDrawObject` | `extern RenderBackend *GRenderer` |
| `LPDIRECTDRAWSURFACE VideoSurfacePtr` | `RenderSurface` (via `render_backend.h`) |
| `HRESULT` comme type de retour | `bool` |
| `PALETTEENTRY` | `struct { uint8_t r, g, b; }` |

# Contexte : Launcher Linux pour Red Alert Remastered

## Situation générale

Le dépôt open source EA (`CnC_Remastered_Collection`) contient le code source de
la simulation du jeu, buildé en **shared library** (`RedAlert.dll` / `RedAlert.so`).
Le launcher GlyphX (frontend graphique) n'est **pas** open source.

L'objectif est d'écrire un launcher Linux custom en C++ avec **SDL3** qui :
1. Charge `RedAlert.so` via `libdl` (`dlopen` / `dlsym`)
2. Pilote la simulation via l'interface `CNC_*` exportée en `extern "C"`
3. Gère le rendu, l'audio et l'input (SDL3)

---

## Architecture de l'interface dll

Toutes les fonctions exportées sont en `extern "C"` / `__cdecl` — ABI C pure,
pas de name mangling C++. Sur Linux `__cdecl` est une no-op :

```cpp
// compat/msvc_compat.h
#ifndef _WIN32
    #define __cdecl
    #define __declspec(x)
#endif
```

Les exports côté dll utilisent `__declspec(dllexport)`, à abstraire :

```cpp
#ifdef _WIN32
    #define DLLEXPORT __declspec(dllexport)
#else
    #define DLLEXPORT __attribute__((visibility("default")))
#endif
```

Avec dans le CMakeLists :

```cmake
target_compile_options(redalert PRIVATE -fvisibility=hidden)
```

---

## Version de l'interface

```cpp
// dllinterfaceversion.h — NE PAS MODIFIER
#define CNC_DLL_API_VERSION 0x102
```

`CNC_Version(CNC_DLL_API_VERSION)` doit être le **premier appel** après `dlopen`.
La dll retourne sa propre version ; si elle diffère, abort.

---

## Cycle de vie du launcher

```
dlopen("RedAlert.so", RTLD_NOW)
    │
    ├─ ra_resolve_symbols()       // résout tous les CNC_* via dlsym
    │
    ├─ CNC_Version()              // vérification compatibilité 0x102
    ├─ CNC_Init(cmd, callback)    // init dll + enregistrement callback événements
    ├─ CNC_Config(&rules)         // paramètres de difficulté (optionnel)
    │
    ├─ CNC_Start_Instance(...)    // démarrage scénario campagne
    │   ou CNC_Start_Instance_Variation(...)
    │   ou CNC_Start_Custom_Instance(...)
    │
    └─ Boucle principale
           ├─ handle_sdl_events() → CNC_Handle_Input(...)
           ├─ CNC_Advance_Instance(player_id)   // tick simulation
           ├─ CNC_Get_Visible_Page(buf, &w, &h) // framebuffer 8-bit palettisé
           ├─ CNC_Get_Palette(palette)           // palette 256×RGB
           └─ SDL_UpdateTexture + SDL_RenderPresent

dlclose()
```

---

## Rendu : framebuffer palettisé 8-bit

La dll maintient un framebuffer interne en **mode palettisé 8-bit** (héritage
Westwood 320×200 / Mode X). Le launcher est responsable de la conversion
palette → RGBA et de l'upload GPU.

```cpp
// Types palette
typedef unsigned char CNCPalette[256][3]; // 256 entrées RGB, 1 octet par composante

// Boucle de rendu SDL3
unsigned char *framebuf = malloc(width * height);
CNCPalette palette;

iface.CNC_Get_Visible_Page(framebuf, &width, &height);
iface.CNC_Get_Palette(palette);

// Conversion palette → RGBA
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

## Callback événements

La dll notifie le launcher via un pointeur de fonction enregistré à `CNC_Init` :

```cpp
typedef void (*CNC_Event_Callback_Type)(const void *event); // cast → EventCallbackStruct*
```

Le champ `EventCallbackStruct.EventType` (enum `EventCallbackType`) détermine
ce qui s'est passé. Le launcher doit traiter :

| EventType                          | Action launcher                        |
|------------------------------------|----------------------------------------|
| `CALLBACK_EVENT_SOUND_EFFECT`      | Décoder `.aud` + push SDL_AudioStream  |
| `CALLBACK_EVENT_SPEECH`            | Idem                                   |
| `CALLBACK_EVENT_MOVIE`             | Lire fichier VQA                       |
| `CALLBACK_EVENT_GAME_OVER`         | Afficher écran fin de partie           |
| `CALLBACK_EVENT_MESSAGE`           | Afficher message HUD                   |
| `CALLBACK_EVENT_BRIEFING_SCREEN`   | Afficher briefing de mission           |
| `CALLBACK_EVENT_CENTER_CAMERA`     | Recentrer la vue                       |
| `CALLBACK_EVENT_PING`              | Afficher beacon sur la carte           |
| `CALLBACK_EVENT_ACHIEVEMENT`       | Optionnel                              |
| `CALLBACK_EVENT_STORE_CARRYOVER_OBJECTS` | Sauvegarder objets entre missions |

---

## Structs partagées — règles critiques

**Toutes les structs échangées avec la dll sont sous `#pragma pack(1)`.**
Le layout binaire est un contrat fixe avec la dll. Ne jamais ajouter, retirer
ou réordonner des champs. Utiliser des buffers de padding pour les champs
non encore implémentés plutôt que de les omettre.

`#pragma pack` push/pop est supporté par GCC et Clang sur Linux — pas besoin
de `__attribute__((packed))`.

### Vérification de taille — obligatoire

Ajouter un `static_assert` pour chaque struct partagée. La taille de référence
s'obtient en ajoutant un `printf` dans la dll au démarrage :

```cpp
printf("sizeof CNCPlayerInfoStruct: %zu\n", sizeof(CNCPlayerInfoStruct));
```

```cpp
static_assert(sizeof(CNCPlayerInfoStruct) == TAILLE_DE_REFERENCE,
              "CNCPlayerInfoStruct layout mismatch — vérifier le padding");
```

---

## CNCPlayerInfoStruct — layout complet avec padding

**Problème** : certains champs de la struct originale (`CNCSpiedInfoStruct`,
`DllObjectTypeEnum`, `DllActionTypeEnum`) nécessitent des headers internes de
la dll non disponibles dans le launcher. La solution est de les remplacer par
des buffers de padding de taille identique, ce qui préserve le layout binaire
sans créer de dépendances.

**Tailles à connaître (Red Alert, depuis `dllinterface.h`) :**

| Champ original                              | Type                        | Taille              |
|---------------------------------------------|-----------------------------|---------------------|
| `SpiedInfo[MAX_HOUSES]`                     | `CNCSpiedInfoStruct[32]`    | 32 × 3×int = 384 B  |
| `SelectedType`                              | `DllObjectTypeEnum`         | 4 B (enum = int)    |
| `ActionWithSelected[MAX_EXPORT_CELLS]`      | `DllActionTypeEnum[16384]`  | 16384 × 1 B         |

`MAX_HOUSES = 32`, `MAX_EXPORT_CELLS = 128 × 128 = 16384`

```cpp
#pragma pack(push, 1)

typedef struct {
    // --- champs directs ---
    char          Name[64];
    unsigned char House;
    int           ColorIndex;
    uint64_t      GlyphxPlayerID;        // unsigned __int64 côté dll
    int           Team;
    int           StartLocationIndex;
    unsigned char HomeCellX;
    unsigned char HomeCellY;
    bool          IsAI;
    unsigned int  AllyFlags;
    bool          IsDefeated;
    unsigned int  SpiedPowerFlags;
    unsigned int  SpiedMoneyFlags;

    // --- padding : CNCSpiedInfoStruct SpiedInfo[MAX_HOUSES] ---
    // CNCSpiedInfoStruct = { int Credits; int PowerProduced; int PowerDrained; }
    // 3 × sizeof(int) × 32 = 384 octets
    unsigned char _pad_SpiedInfo[32 * 3 * 4];

    int           SelectedID;

    // --- padding : DllObjectTypeEnum SelectedType (enum = int) ---
    unsigned char _pad_SelectedType[4];

    // --- padding : DllActionTypeEnum ActionWithSelected[MAX_EXPORT_CELLS] ---
    // DllActionTypeEnum : unsigned char, MAX_EXPORT_CELLS = 128*128 = 16384
    unsigned char ActionWithSelected[128 * 128];

    unsigned int  ActionWithSelectedCount;
    unsigned int  ScreenShake;
    bool          IsRadarJammed;
} CNCPlayerInfoStruct;

#pragma pack(pop)

// Vérification — remplacer XXXX par la valeur obtenue avec printf dans la dll
// static_assert(sizeof(CNCPlayerInfoStruct) == XXXX,
//               "CNCPlayerInfoStruct size mismatch");
```

> **Note** : `ActionWithSelected` est utilisable directement (tableau de `unsigned char`
> indexé par cellule) même sans connaître `DllActionTypeEnum` — les valeurs
> sont opaques pour le launcher qui n'a pas à les interpréter.

---

## Table des fonctions exportées (RAInterface)

Pattern de chargement via `dlsym` :

```cpp
typedef struct {
    // --- cycle de vie ---
    unsigned int (*CNC_Version)(unsigned int version_in);
    void         (*CNC_Init)(const char *cmd, CNC_Event_Callback_Type cb);
    void         (*CNC_Config)(const CNCRulesDataStruct *rules);
    void         (*CNC_Add_Mod_Path)(const char *path);

    // --- rendu ---
    bool         (*CNC_Get_Visible_Page)(unsigned char *buf, unsigned int *w, unsigned int *h);
    bool         (*CNC_Get_Palette)(CNCPalette palette);

    // --- démarrage de partie ---
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

    // --- multi ---
    bool         (*CNC_Set_Multiplayer_Data)(int scenario, CNCMultiplayerOptionsStruct *opts,
                                              int num_players, CNCPlayerInfoStruct *players,
                                              int max_players);
    // --- sélection ---
    bool         (*CNC_Clear_Object_Selection)(uint64_t player_id);
    bool         (*CNC_Select_Object)(uint64_t player_id, int type_id, int object_id);

    // --- sauvegarde ---
    bool         (*CNC_Save_Load)(bool save, const char *path, const char *game_type);

    // --- divers ---
    void         (*CNC_Set_Difficulty)(int difficulty);
    void         (*CNC_Restore_Carryover_Objects)(const CarryoverObjectStruct *objects);
    void         (*CNC_Handle_Player_Switch_To_AI)(uint64_t player_id);
    void         (*CNC_Handle_Human_Team_Wins)(uint64_t player_id);
    void         (*CNC_Start_Mission_Timer)(int time);
    bool         (*CNC_Get_Start_Game_Info)(uint64_t player_id, int *waypoint_index);
} RAInterface;
```

Macro de résolution :

```cpp
#define RESOLVE(iface, handle, name)                                    \
    (iface)->name = dlsym((handle), #name);                             \
    if (!(iface)->name) {                                               \
        fprintf(stderr, "symbole manquant: %s\n", #name);              \
        return false;                                                   \
    }
```

---

## Audio

Le format audio natif Westwood est `.aud` (codec maison). Il n'y a **pas** de
SDL_mixer pour SDL3 au moment de la rédaction — utiliser SDL3 audio natif :

- Décoder `.aud` → PCM dans le callback `CALLBACK_EVENT_SOUND_EFFECT`
- Pousser les samples via `SDL_AudioStream`
- La musique est en MIDI → options : FluidSynth, ou conversion offline en OGG

---

## Notes CMake

```cmake
# Target dll
add_library(redalert SHARED ${SOURCES})
target_compile_options(redalert PRIVATE -fvisibility=hidden)

# Flags plateforme
if(WIN32)
    target_compile_definitions(redalert PRIVATE WIN32)
else()
    target_compile_definitions(redalert PRIVATE LINUX)
endif()

# Flag spécifique Red Alert vs Tiberian Dawn
# TiberianDawn/CMakeLists.txt ajoute TIBERIAN_DAWN
# RedAlert/CMakeLists.txt n'en a pas besoin

# Forcer l'inclusion du PCH de compatibilité (simule le PCH global MSVC)
target_compile_options(redalert PRIVATE
    -include ${CMAKE_SOURCE_DIR}/compat/force_include.h
)
```

---

## Compat header minimal (`compat/force_include.h`)

```cpp
#pragma once

#ifndef _WIN32

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Types MSVC
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

// Macros MSVC
#define __cdecl
#define __declspec(x)
#define __int64      int64_t
// "unsigned __int64" → à remplacer manuellement par uint64_t dans les sources

// Constantes CRT Windows
#define _MAX_FNAME 256
#define _MAX_EXT   256

// Stubs fenêtre
#include "windows_stub.h"

#endif // _WIN32
```

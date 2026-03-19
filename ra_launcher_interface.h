/**
 * ra_launcher_interface.h
 *
 * Interface complète pour charger RedAlert.so via libdl et piloter
 * la simulation depuis un launcher custom (SDL3 ou autre).
 *
 * Généré d'après DLLInterface.h et DLLInterface.cpp (EA / GPL v3)
 *
 * Cycle typique :
 *   1. dlopen("RedAlert.so")
 *   2. Résoudre tous les symboles via ra_resolve_symbols()
 *   3. CNC_Version() — vérifier la compatibilité
 *   4. CNC_Init()    — initialiser avec le callback événements
 *   5. CNC_Start_Instance() ou CNC_Start_Instance_Variation()
 *   6. Boucle principale : CNC_Advance_Instance() → CNC_Get_Visible_Page() → rendu
 *   7. dlclose()
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* =========================================================================
 * Constantes de version
 * ========================================================================= */

#define CNC_DLL_API_VERSION 0x102

/* =========================================================================
 * Types de base partagés avec la dll
 * (repris de DLLInterface.cpp lignes 56-57)
 * ========================================================================= */

typedef uint64_t uint64;
typedef int64_t int64;

/* =========================================================================
 * Callback événements : la dll appelle cette fonction pour notifier
 * le launcher d'événements one-shot (son, cinématique, fin de partie...)
 * (DLLInterface.cpp ligne 55)
 * ========================================================================= */

typedef void (*CNC_Event_Callback_Type)(const void *event); /* EventCallbackStruct* */

/* =========================================================================
 * Enums partagés (subset utilisé dans les signatures exportées)
 * ========================================================================= */

typedef enum {
	GAME_STATE_NONE = 0,
	GAME_STATE_STATIC_MAP,
	GAME_STATE_DYNAMIC_MAP,
	GAME_STATE_LAYERS,
	GAME_STATE_SIDEBAR,
	GAME_STATE_PLACEMENT,
	GAME_STATE_SHROUD,
	GAME_STATE_OCCUPIER,
	GAME_STATE_PLAYER_INFO
} GameStateRequestEnum;

typedef enum {
	GAME_REQUEST_MOVIE_DONE = 0,
} GameRequestEnum;

typedef enum {
	INPUT_REQUEST_NONE = 0,
	/* les valeurs réelles sont dans InputRequestEnum dans le source RA */
} InputRequestEnum;

typedef enum {
	CALLBACK_EVENT_INVALID = -1,
	CALLBACK_EVENT_SOUND_EFFECT = 0,
	CALLBACK_EVENT_SPEECH,
	CALLBACK_EVENT_GAME_OVER,
	CALLBACK_EVENT_DEBUG_PRINT,
	CALLBACK_EVENT_MOVIE,
	CALLBACK_EVENT_MESSAGE,
	CALLBACK_EVENT_UPDATE_MAP_CELL,
	CALLBACK_EVENT_ACHIEVEMENT,
	CALLBACK_EVENT_STORE_CARRYOVER_OBJECTS,
	CALLBACK_EVENT_SPECIAL_WEAPON_TARGETTING,
	CALLBACK_EVENT_BRIEFING_SCREEN,
	CALLBACK_EVENT_CENTER_CAMERA,
	CALLBACK_EVENT_PING
} EventCallbackType;

/* =========================================================================
 * Structs de données passées au launcher
 * (toutes sous #pragma pack(1) côté dll — layout binaire fixe)
 * ========================================================================= */

/* Palette 256 couleurs, 3 octets par entrée (RGB) */
typedef unsigned char CNCPalette[256][3];

/* Infos joueur pour le multi (extrait de CNCPlayerInfoStruct) */
#pragma pack(push, 1)

typedef struct {
	char Name[64];
	unsigned char House;
	int ColorIndex;
	uint64 GlyphxPlayerID;
	int Team;
	int StartLocationIndex;
	unsigned char HomeCellX;
	unsigned char HomeCellY;
	bool IsAI;
	unsigned int AllyFlags;
	bool IsDefeated;
	unsigned int SpiedPowerFlags;
	unsigned int SpiedMoneyFlags;
	unsigned char _SpiedInfo[32 * 3 * sizeof(int)]; // CNCSpiedInfoStruct[32]
	int SelectedID;
	int SelectedType; // DllObjectTypeEnum
	unsigned char ActionWithSelected[128 * 128]; // DllActionTypeEnum[MAX_EXPORT_CELLS]
	unsigned int ActionWithSelectedCount;
	unsigned int ScreenShake;
	bool IsRadarJammed;
} CNCPlayerInfoStruct;

/* Options multijoueur */
typedef struct {
	int MPlayerCount;
	int MPlayerBases;
	int MPlayerCredits;
	int MPlayerTiberium;
	int MPlayerGoodies;
	int MPlayerGhosts;
	int MPlayerSolo;
	int MPlayerUnitCount;
	bool IsMCVDeploy;
	bool SpawnVisceroids;
	bool EnableSuperweapons;
	bool MPlayerShadowRegrow;
	bool MPlayerAftermathUnits;
	bool CaptureTheFlag;
	bool DestroyStructures;
	bool ModernBalance;
} CNCMultiplayerOptionsStruct;

/* Données de difficulté */
typedef struct {
	float FirepowerBias;
	float GroundspeedBias;
	float AirspeedBias;
	float ArmorBias;
	float ROFBias;
	float CostBias;
	float BuildSpeedBias;
	float RepairDelay;
	float BuildDelay;
	bool IsBuildSlowdown;
	bool IsWallDestroyer;
	bool IsContentScan;
} CNCDifficultyDataStruct;

typedef struct {
	CNCDifficultyDataStruct Difficulties[3];
} CNCRulesDataStruct;

/* Objet persistant entre missions */
typedef struct CarryoverObjectStruct {
	struct CarryoverObjectStruct *Next;
	int RTTI;
	int Type;
	int Cell;
	int Strength;
	int House;
} CarryoverObjectStruct;

#pragma pack(pop)

/* =========================================================================
 * Pointeurs de fonctions exportées
 * — toutes les fonctions CNC_* de DLLInterface.cpp lignes 111-201
 * ========================================================================= */

/* Vérification de version — à appeler en premier.
 * Passer CNC_DLL_API_VERSION, retourne la version de la dll. */
typedef unsigned int (*pfn_CNC_Version)(unsigned int version_in);

/* Initialisation globale de la dll.
 * command_line : options de démarrage (peut être "")
 * event_callback : ta fonction de callback pour les événements jeu */
typedef void (*pfn_CNC_Init)(const char *command_line, CNC_Event_Callback_Type event_callback);

/* Configuration des règles de difficulté */
typedef void (*pfn_CNC_Config)(const CNCRulesDataStruct *rules);

/* Ajout d'un chemin de mod */
typedef void (*pfn_CNC_Add_Mod_Path)(const char *mod_path);

/* Récupère le framebuffer palettisé 8-bit rendu par la dll.
 * C'est ce buffer que tu uploades sur ta texture SDL3 chaque frame. */
typedef bool (*pfn_CNC_Get_Visible_Page)(unsigned char *buffer_in, unsigned int *width, unsigned int *height);

/* Récupère la palette courante (256 entrées RGB) */
typedef bool (*pfn_CNC_Get_Palette)(CNCPalette palette_in);

/* Démarrage d'une instance de jeu (campagne) */
typedef bool (*pfn_CNC_Start_Instance)(int scenario_index,
				       int build_level,
				       const char *faction,
				       const char *game_type,
				       const char *content_directory,
				       int sabotaged_structure,
				       const char *override_map_name);

/* Démarrage avec variation de scénario */
typedef bool (*pfn_CNC_Start_Instance_Variation)(int scenario_index,
						 int scenario_variation,
						 int scenario_direction,
						 int build_level,
						 const char *faction,
						 const char *game_type,
						 const char *content_directory,
						 int sabotaged_structure,
						 const char *override_map_name);

/* Démarrage d'une map custom */
typedef bool (*pfn_CNC_Start_Custom_Instance)(const char *content_directory,
					      const char *directory_path,
					      const char *scenario_name,
					      int build_level,
					      bool multiplayer);

/* Tick de simulation — à appeler chaque frame.
 * player_id : GlyphxPlayerID du joueur local */
typedef bool (*pfn_CNC_Advance_Instance)(uint64 player_id);

/* Interroge l'état du jeu (carte, objets, shroud, sidebar...) */
typedef bool (*pfn_CNC_Get_Game_State)(GameStateRequestEnum state_type, uint64 player_id, unsigned char *buffer_in, unsigned int buffer_size);

/* Lecture du fichier INI de scénario */
typedef bool (*pfn_CNC_Read_INI)(int scenario_index,
				 int scenario_variation,
				 int scenario_direction,
				 const char *content_directory,
				 const char *override_map_name,
				 char *ini_buffer,
				 int ini_buffer_size);

/* Position initiale de la caméra pour ce joueur */
typedef void (*pfn_CNC_Set_Home_Cell)(int x, int y, uint64 player_id);

/* Notifie la dll qu'une cinématique est terminée */
typedef void (*pfn_CNC_Handle_Game_Request)(GameRequestEnum request_type);

/* Paramètres d'affichage (barres de vie/ressources) */
typedef void (*pfn_CNC_Handle_Game_Settings_Request)(int health_bar_display_mode, int resource_bar_display_mode);

/* Injection d'input souris/clavier depuis le launcher vers la dll */
typedef void (*pfn_CNC_Handle_Input)(InputRequestEnum mouse_event, unsigned char special_key_flags, uint64 player_id, int x1, int y1, int x2, int y2);

/* Actions sur structures (vendre, réparer...) */
typedef void (*pfn_CNC_Handle_Structure_Request)(int request_type, uint64 player_id, int object_id);

/* Actions sur unités */
typedef void (*pfn_CNC_Handle_Unit_Request)(int request_type, uint64 player_id);

/* Actions sur la sidebar (construction...) */
typedef void (*pfn_CNC_Handle_Sidebar_Request)(int request_type, uint64 player_id, int buildable_type, int buildable_id, short cell_x, short cell_y);

/* Super-armes */
typedef void (*pfn_CNC_Handle_SuperWeapon_Request)(int request_type, uint64 player_id, int buildable_type, int buildable_id, int x1, int y1);

/* Groupes de contrôle */
typedef void (*pfn_CNC_Handle_ControlGroup_Request)(int request_type, uint64 player_id, unsigned char control_group_index);

/* Requêtes de debug */
typedef void (
	*pfn_CNC_Handle_Debug_Request)(int debug_request_type, uint64 player_id, const char *object_name, int x, int y, bool unshroud, bool enemy);

/* Beacons (marqueurs de ping sur la carte) */
typedef void (*pfn_CNC_Handle_Beacon_Request)(int beacon_request_type, uint64 player_id, int pixel_x, int pixel_y);

/* Setup multijoueur */
typedef bool (*pfn_CNC_Set_Multiplayer_Data)(int scenario_index,
					     CNCMultiplayerOptionsStruct *game_options,
					     int num_players,
					     CNCPlayerInfoStruct *player_list,
					     int max_players);

/* Sélection d'objets */
typedef bool (*pfn_CNC_Clear_Object_Selection)(uint64 player_id);
typedef bool (*pfn_CNC_Select_Object)(uint64 player_id, int object_type_id, int object_to_select_id);

/* Sauvegarde / chargement */
typedef bool (*pfn_CNC_Save_Load)(bool save, const char *file_path_and_name, const char *game_type);

/* Difficulté globale (0=easy, 1=normal, 2=hard) */
typedef void (*pfn_CNC_Set_Difficulty)(int difficulty);

/* Restauration d'objets persistants entre missions */
typedef void (*pfn_CNC_Restore_Carryover_Objects)(const CarryoverObjectStruct *objects);

/* Gestion de la déconnexion d'un joueur humain en multi */
typedef void (*pfn_CNC_Handle_Player_Switch_To_AI)(uint64 player_id);
typedef void (*pfn_CNC_Handle_Human_Team_Wins)(uint64 player_id);

/* Timer de mission */
typedef void (*pfn_CNC_Start_Mission_Timer)(int time);

/* Position de départ d'un joueur (waypoint index) */
typedef bool (*pfn_CNC_Get_Start_Game_Info)(uint64 player_id, int *start_location_waypoint_index);

/* =========================================================================
 * Table de résolution — regroupe tous les pointeurs en une struct
 * ========================================================================= */

typedef struct {
	pfn_CNC_Version CNC_Version;
	pfn_CNC_Init CNC_Init;
	pfn_CNC_Config CNC_Config;
	pfn_CNC_Add_Mod_Path CNC_Add_Mod_Path;
	pfn_CNC_Get_Visible_Page CNC_Get_Visible_Page;
	pfn_CNC_Get_Palette CNC_Get_Palette;
	pfn_CNC_Start_Instance CNC_Start_Instance;
	pfn_CNC_Start_Instance_Variation CNC_Start_Instance_Variation;
	pfn_CNC_Start_Custom_Instance CNC_Start_Custom_Instance;
	pfn_CNC_Advance_Instance CNC_Advance_Instance;
	pfn_CNC_Get_Game_State CNC_Get_Game_State;
	pfn_CNC_Read_INI CNC_Read_INI;
	pfn_CNC_Set_Home_Cell CNC_Set_Home_Cell;
	pfn_CNC_Handle_Game_Request CNC_Handle_Game_Request;
	pfn_CNC_Handle_Game_Settings_Request CNC_Handle_Game_Settings_Request;
	pfn_CNC_Handle_Input CNC_Handle_Input;
	pfn_CNC_Handle_Structure_Request CNC_Handle_Structure_Request;
	pfn_CNC_Handle_Unit_Request CNC_Handle_Unit_Request;
	pfn_CNC_Handle_Sidebar_Request CNC_Handle_Sidebar_Request;
	pfn_CNC_Handle_SuperWeapon_Request CNC_Handle_SuperWeapon_Request;
	pfn_CNC_Handle_ControlGroup_Request CNC_Handle_ControlGroup_Request;
	pfn_CNC_Handle_Debug_Request CNC_Handle_Debug_Request;
	pfn_CNC_Handle_Beacon_Request CNC_Handle_Beacon_Request;
	pfn_CNC_Set_Multiplayer_Data CNC_Set_Multiplayer_Data;
	pfn_CNC_Clear_Object_Selection CNC_Clear_Object_Selection;
	pfn_CNC_Select_Object CNC_Select_Object;
	pfn_CNC_Save_Load CNC_Save_Load;
	pfn_CNC_Set_Difficulty CNC_Set_Difficulty;
	pfn_CNC_Restore_Carryover_Objects CNC_Restore_Carryover_Objects;
	pfn_CNC_Handle_Player_Switch_To_AI CNC_Handle_Player_Switch_To_AI;
	pfn_CNC_Handle_Human_Team_Wins CNC_Handle_Human_Team_Wins;
	pfn_CNC_Start_Mission_Timer CNC_Start_Mission_Timer;
	pfn_CNC_Get_Start_Game_Info CNC_Get_Start_Game_Info;
} RAInterface;

/* =========================================================================
 * Helpers libdl
 * ========================================================================= */

#ifdef __linux__
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

/**
 * Résout tous les symboles CNC_* depuis le handle dlopen.
 * Retourne false et affiche le symbole manquant si un export est absent.
 */
static inline bool ra_resolve_symbols(void *dl_handle, RAInterface *iface) {
	memset(iface, 0, sizeof(*iface));

#define RESOLVE(name)                                                                                                                                \
	iface->name = (pfn_##name)dlsym(dl_handle, #name);                                                                                           \
	if (!iface->name) {                                                                                                                          \
		fprintf(stderr, "ra_launcher: symbole manquant : %s\n", #name);                                                                      \
		return false;                                                                                                                        \
	}

	RESOLVE(CNC_Version)
	RESOLVE(CNC_Init)
	RESOLVE(CNC_Config)
	RESOLVE(CNC_Add_Mod_Path)
	RESOLVE(CNC_Get_Visible_Page)
	RESOLVE(CNC_Get_Palette)
	RESOLVE(CNC_Start_Instance)
	RESOLVE(CNC_Start_Instance_Variation)
	RESOLVE(CNC_Start_Custom_Instance)
	RESOLVE(CNC_Advance_Instance)
	RESOLVE(CNC_Get_Game_State)
	RESOLVE(CNC_Read_INI)
	RESOLVE(CNC_Set_Home_Cell)
	RESOLVE(CNC_Handle_Game_Request)
	RESOLVE(CNC_Handle_Game_Settings_Request)
	RESOLVE(CNC_Handle_Input)
	RESOLVE(CNC_Handle_Structure_Request)
	RESOLVE(CNC_Handle_Unit_Request)
	RESOLVE(CNC_Handle_Sidebar_Request)
	RESOLVE(CNC_Handle_SuperWeapon_Request)
	RESOLVE(CNC_Handle_ControlGroup_Request)
	RESOLVE(CNC_Handle_Debug_Request)
	RESOLVE(CNC_Handle_Beacon_Request)
	RESOLVE(CNC_Set_Multiplayer_Data)
	RESOLVE(CNC_Clear_Object_Selection)
	RESOLVE(CNC_Select_Object)
	RESOLVE(CNC_Save_Load)
	RESOLVE(CNC_Set_Difficulty)
	RESOLVE(CNC_Restore_Carryover_Objects)
	RESOLVE(CNC_Handle_Player_Switch_To_AI)
	RESOLVE(CNC_Handle_Human_Team_Wins)
	RESOLVE(CNC_Start_Mission_Timer)
	RESOLVE(CNC_Get_Start_Game_Info)

#undef RESOLVE
	return true;
}

/**
 * Vérifie que la version de la dll correspond à ce que le launcher attend.
 * Retourne false si incompatible.
 */
static inline bool ra_check_version(RAInterface *iface) {
	unsigned int dll_version = iface->CNC_Version(CNC_DLL_API_VERSION);
	if (dll_version != CNC_DLL_API_VERSION) {
		fprintf(stderr, "ra_launcher: version incompatible (launcher=0x%x, dll=0x%x)\n", CNC_DLL_API_VERSION, dll_version);
		return false;
	}
	return true;
}

#endif /* __linux__ */

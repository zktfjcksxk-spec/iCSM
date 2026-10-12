//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon data file parsing, shared by game & client dlls.
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_PARSE_H
#define WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "networkvar.h" // DECLARE_CLASS stuff
#include "GameEventListener.h"
#include "tier1/utlsortvector.h"
#include "gamestringpool.h"

class IFileSystem;

// Set this to cache the values of all weapon attributes in simple data structures
// (Note: Must match value in cs_weapon_id.h)
#define USE_WEAPON_DATA_CACHE 0

// Set this to test the cache on every access by loading via the old weaponinfo system
#define TEST_WEAPON_DATA_CACHE 0



// -----------------------------------------------------------
// Weapon sound types
// Used to play sounds defined in the weapon's classname.txt file
// This needs to match pWeaponSoundCategories in weapon_parse.cpp
// ------------------------------------------------------------
enum WeaponSound_t {
	EMPTY,
	SINGLE,
	SINGLE_ACCURATE,
	SINGLE_NPC,
	WPN_DOUBLE, // Can't be "DOUBLE" because windows.h uses it.
	DOUBLE_NPC,
	BURST,
	RELOAD,
	RELOAD_NPC,
	MELEE_MISS,
	MELEE_HIT,
	MELEE_HIT_WORLD,
	SPECIAL1,
	SPECIAL2,
	SPECIAL3,
	TAUNT,
	NEARLYEMPTY,
	FAST_RELOAD,

	// Add new shoot sound types here

	NUM_SHOOT_SOUND_TYPES,
}; 

int GetWeaponSoundFromString( const char *pszString );

#define MAX_SHOOT_SOUNDS	16			// Maximum number of shoot sounds per shoot type

#define MAX_WEAPON_STRING	80
#define MAX_WEAPON_PREFIX	16
#define MAX_WEAPON_AMMO_NAME		32

#define WEAPON_PRINTNAME_MISSING "!!! Missing printname on weapon"

// Forward declare FileWeaponInfo_t
#if USE_WEAPON_DATA_CACHE
class CWeaponData;
typedef CWeaponData FileWeaponInfo_t;
#else
class FileWeaponInfo_t;
#endif


#endif // WEAPON_PARSE_H

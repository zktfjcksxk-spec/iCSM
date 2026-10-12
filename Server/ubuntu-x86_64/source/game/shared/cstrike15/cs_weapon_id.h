//========= Copyright © 2017, Valve Corporation, All rights reserved. ============//
//
// Purpose: Simple constants/data types for supporting CSWeaponID
//          cpp is shared in cs_weapon_parse.cpp
//
//=============================================================================//

#ifndef CS_WEAPON_ID_H
#define CS_WEAPON_ID_H
#ifdef _WIN32
#pragma once
#endif

#include "econ_item_constants.h"

#define USE_WEAPON_DATA_CACHE 0 // must match value in weapon_parse.h
#if USE_WEAPON_DATA_CACHE
class CWeaponData;
class CCSWeaponData;
typedef CWeaponData FileWeaponInfo_t;
typedef CCSWeaponData CCSWeaponInfo;
#else
class FileWeaponInfo_t;
class CCSWeaponInfo;
#endif

//--------------------------------------------------------------------------------------------------------
enum CSWeaponType
{
	WEAPONTYPE_KNIFE = 0,
	WEAPONTYPE_PISTOL,
	WEAPONTYPE_SUBMACHINEGUN,
	WEAPONTYPE_RIFLE,
	WEAPONTYPE_SHOTGUN,
	WEAPONTYPE_SNIPER_RIFLE,
	WEAPONTYPE_MACHINEGUN,
	WEAPONTYPE_C4,
	WEAPONTYPE_TASER,
	WEAPONTYPE_GRENADE,
	WEAPONTYPE_EQUIPMENT,
	WEAPONTYPE_STACKABLEITEM,

	WEAPONTYPE_FISTS,
	WEAPONTYPE_BREACHCHARGE,
	WEAPONTYPE_TABLET,
	WEAPONTYPE_MELEE,

	WEAPONTYPE_UNKNOWN
};

enum CSWeaponCategory
{
	WEAPONCATEGORY_OTHER = 0,
	WEAPONCATEGORY_MELEE,
	WEAPONCATEGORY_SECONDARY,
	WEAPONCATEGORY_SMG,
	WEAPONCATEGORY_RIFLE,
	WEAPONCATEGORY_HEAVY,
	WEAPONCATEGORY_COUNT,
};

enum CSWeaponSilencerType
{
	WEAPONSILENCER_NONE = 0,
	WEAPONSILENCER_DETACHABLE,
	WEAPONSILENCER_INTEGRATED,
};

struct CSWeaponID
{
	const CEconItemDefinition* mpObj;

	CSWeaponID() = default;
	CSWeaponID( const CSWeaponID& ) = default;
	CSWeaponID& operator=( const CSWeaponID& ) = default;
	~CSWeaponID() = default;
	explicit CSWeaponID( const CEconItemDefinition* pDefinition ) : mpObj( pDefinition ) {}
	/*implicit*/ CSWeaponID( const CSchemaItemDefHandle& handle ) : mpObj( handle ) {}

	bool operator== ( const CSWeaponID& rhs ) const { return mpObj == rhs.mpObj; }
	bool operator!= ( const CSWeaponID& rhs ) const { return mpObj != rhs.mpObj; }
};

static inline bool operator== ( const CSchemaItemDefHandle& lhs, const CSWeaponID& rhs ) { return lhs == rhs.mpObj; }
static inline bool operator!= ( const CSchemaItemDefHandle& lhs, const CSWeaponID& rhs ) { return lhs != rhs.mpObj; }

// NOTE: AVOID CSWeaponID

static inline const CEconItemDefinition* WeaponIDToDefinition( CSWeaponID id ) { return id.mpObj; }
static inline CSWeaponID WeaponIDFromDefinition( const CEconItemDefinition* definition ) { return CSWeaponID( definition ); }

extern const CSWeaponID WEAPON_NONE;

// OLD CSWeaponID values
extern const CSchemaItemDefHandle 
	WEAPON_DEAGLE,
	WEAPON_ELITE,
	WEAPON_FIVESEVEN,
	WEAPON_GLOCK,

	WEAPON_AK47,
	WEAPON_AUG,
	WEAPON_AWP,
	WEAPON_FAMAS,
	WEAPON_G3SG1,
	WEAPON_GALILAR,
	WEAPON_M249,
	WEAPON_M4A1,
	WEAPON_MAC10,
	WEAPON_P90,
	WEAPON_UMP45,
	WEAPON_XM1014,

	WEAPON_BIZON,
	WEAPON_MAG7,
	WEAPON_NEGEV,
	WEAPON_SAWEDOFF,
	WEAPON_TEC9,
	WEAPON_TASER,

	WEAPON_HKP2000,
	WEAPON_MP7,
	WEAPON_MP9,
	WEAPON_NOVA,
	WEAPON_P250,
	WEAPON_SCAR20,
	WEAPON_SG556,
	WEAPON_SSG08,

	WEAPON_KNIFE_GG,
	WEAPON_KNIFE,

	WEAPON_FLASHBANG,
	WEAPON_HEGRENADE,
	WEAPON_SMOKEGRENADE,
	WEAPON_MOLOTOV,
	WEAPON_DECOY,
	WEAPON_INCGRENADE,
	WEAPON_TAGRENADE,
	WEAPON_SNOWBALL,
	WEAPON_C4,
	
	ITEM_KEVLAR,
	ITEM_ASSAULTSUIT,				// kevlar + helmet
	ITEM_HEAVYASSAULTSUIT,			// heavy phoenix armor
	ITEM_NVG,						// not enabled? seems kind-of-sort-of-implemented
	ITEM_DEFUSER,

	WEAPON_HEALTHSHOT,

	WEAPON_FISTS,
	WEAPON_BREACHCHARGE,
	WEAPON_TABLET,
	WEAPON_MELEE,

	WEAPON_FIREBOMB,
	WEAPON_FRAGGRENADE,
	WEAPON_DIVERSION,

	// last entry is just a handle to a 'null' item
	WEAPON_NONE_HANDLE;

//--------------------------------------------------------------------------------------------------------
// Utility conversion functions 
// TODO: Move to IWeaponSystem?
//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType );
CSWeaponType WeaponClassFromString( const char * weaponType );

const char* WeaponIdAsString( CSWeaponID weaponID );			// deprecated
CSWeaponID WeaponIdFromString( const char *szWeaponName );		// deprecated
const char *WeaponIDToAlias( CSWeaponID id );					// deprecated

uint8 WeaponIDToByte( CSWeaponID weaponID );
CSWeaponID WeaponIDFromByte( uint8 weaponIDByte );

const CEconItemView* GetGenericWeaponView( CSWeaponID weaponId );

#endif // CS_WEAPON_ID_H
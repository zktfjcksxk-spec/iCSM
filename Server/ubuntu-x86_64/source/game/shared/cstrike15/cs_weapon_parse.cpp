//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <keyvalues.h>
#include "cs_weapon_parse.h"
#include "cs_shareddefs.h"
#include "weapon_csbase.h"
#include "weapon_csbasegun.h"
#include "icvar.h"
#include "cs_gamerules.h"
#include "ihasattributes.h"
#include "cstrike15_item_inventory.h" // for GetReferenceEconItem

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar weapon_recoil_suppression_shots( "weapon_recoil_suppression_shots", "4", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Number of shots before weapon uses full recoil" );
ConVar weapon_recoil_suppression_factor( "weapon_recoil_suppression_factor", "0.75", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Initial recoil suppression factor (first suppressed shot will use this factor * standard recoil, lerping to 1 for later shots" );
ConVar weapon_recoil_variance( "weapon_recoil_variance", "0.55", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Amount of variance per recoil impulse", true, 0.0f, true, 1.0f );


//const CEconItemDefinition* const WEAPON_NONE = nullptr;
const CSWeaponID WEAPON_NONE = CSWeaponID( nullptr );

const CSchemaItemDefHandle WEAPON_NONE_HANDLE( "__reserved__should_not_exist__" );
const CSchemaItemDefHandle WEAPON_DEAGLE( "weapon_deagle" );
const CSchemaItemDefHandle WEAPON_ELITE( "weapon_elite" );
const CSchemaItemDefHandle WEAPON_FIVESEVEN( "weapon_fiveseven" );
const CSchemaItemDefHandle WEAPON_GLOCK( "weapon_glock" );
//const CSchemaItemDefHandle WEAPON_P228( "" );
//const CSchemaItemDefHandle WEAPON_USP( "" );
const CSchemaItemDefHandle WEAPON_AK47( "weapon_ak47" );
const CSchemaItemDefHandle WEAPON_AUG( "weapon_aug" );
const CSchemaItemDefHandle WEAPON_AWP( "weapon_awp" );
const CSchemaItemDefHandle WEAPON_FAMAS( "weapon_famas" );
const CSchemaItemDefHandle WEAPON_G3SG1( "weapon_g3sg1" );
//const CSchemaItemDefHandle WEAPON_GALIL( "" );
const CSchemaItemDefHandle WEAPON_GALILAR( "weapon_galilar" );
const CSchemaItemDefHandle WEAPON_M249( "weapon_m249" );
//const CSchemaItemDefHandle WEAPON_M3( "" );
const CSchemaItemDefHandle WEAPON_M4A1( "weapon_m4a1" );
const CSchemaItemDefHandle WEAPON_MAC10( "weapon_mac10" );
//const CSchemaItemDefHandle WEAPON_MP5NAVY( "" );
const CSchemaItemDefHandle WEAPON_P90( "weapon_p90" );
//const CSchemaItemDefHandle WEAPON_SCOUT( "" );
//const CSchemaItemDefHandle WEAPON_SG550( "" );
//const CSchemaItemDefHandle WEAPON_SG552( "" );
//const CSchemaItemDefHandle WEAPON_TMP( "" );
const CSchemaItemDefHandle WEAPON_UMP45( "weapon_ump45" );
const CSchemaItemDefHandle WEAPON_XM1014( "weapon_xm1014" );

const CSchemaItemDefHandle WEAPON_BIZON( "weapon_bizon" );
const CSchemaItemDefHandle WEAPON_MAG7( "weapon_mag7" );
const CSchemaItemDefHandle WEAPON_NEGEV( "weapon_negev" );
const CSchemaItemDefHandle WEAPON_SAWEDOFF( "weapon_sawedoff" );
const CSchemaItemDefHandle WEAPON_TEC9( "weapon_tec9" );
const CSchemaItemDefHandle WEAPON_TASER( "weapon_taser" );

const CSchemaItemDefHandle WEAPON_HKP2000( "weapon_hkp2000" );
const CSchemaItemDefHandle WEAPON_MP7( "weapon_mp7" );
const CSchemaItemDefHandle WEAPON_MP9( "weapon_mp9" );
const CSchemaItemDefHandle WEAPON_NOVA( "weapon_nova" );
const CSchemaItemDefHandle WEAPON_P250( "weapon_p250" );
//const CSchemaItemDefHandle WEAPON_SCAR17( "" );
const CSchemaItemDefHandle WEAPON_SCAR20( "weapon_scar20" );
const CSchemaItemDefHandle WEAPON_SG556( "weapon_sg556" );
const CSchemaItemDefHandle WEAPON_SSG08( "weapon_ssg08" );

const CSchemaItemDefHandle WEAPON_KNIFE_GG( "weapon_knifegg" );
const CSchemaItemDefHandle WEAPON_KNIFE( "weapon_knife" );

const CSchemaItemDefHandle WEAPON_FLASHBANG( "weapon_flashbang" );
const CSchemaItemDefHandle WEAPON_HEGRENADE( "weapon_hegrenade" );
const CSchemaItemDefHandle WEAPON_SMOKEGRENADE( "weapon_smokegrenade" );
const CSchemaItemDefHandle WEAPON_MOLOTOV( "weapon_molotov" );
const CSchemaItemDefHandle WEAPON_DECOY( "weapon_decoy" );
const CSchemaItemDefHandle WEAPON_INCGRENADE( "weapon_incgrenade" );
const CSchemaItemDefHandle WEAPON_TAGRENADE( "weapon_tagrenade" );
const CSchemaItemDefHandle WEAPON_SNOWBALL( "weapon_snowball" );

const CSchemaItemDefHandle WEAPON_C4( "weapon_c4" );

const CSchemaItemDefHandle ITEM_KEVLAR( "item_kevlar" );
const CSchemaItemDefHandle ITEM_ASSAULTSUIT( "item_assaultsuit" );
const CSchemaItemDefHandle ITEM_HEAVYASSAULTSUIT( "item_heavyassaultsuit" );
//const CSchemaItemDefHandle ITEM_EXOSUIT( "" );
const CSchemaItemDefHandle ITEM_NVG( "item_nvg" );
const CSchemaItemDefHandle ITEM_DEFUSER( "item_defuser" );
//const CSchemaItemDefHandle ITEM_CUTTERS( "" );

const CSchemaItemDefHandle WEAPON_HEALTHSHOT( "weapon_healthshot" );

const CSchemaItemDefHandle WEAPON_FISTS( "weapon_fists" );
const CSchemaItemDefHandle WEAPON_BREACHCHARGE( "weapon_breachcharge" );
const CSchemaItemDefHandle WEAPON_TABLET( "weapon_tablet" );
const CSchemaItemDefHandle WEAPON_MELEE( "weapon_melee" );

const CSchemaItemDefHandle WEAPON_FIREBOMB( "weapon_firebomb" );
const CSchemaItemDefHandle WEAPON_FRAGGRENADE( "weapon_frag_grenade" );
const CSchemaItemDefHandle WEAPON_DIVERSION( "weapon_diversion" );

//--------------------------------------------------------------------------------------------------------
struct WeaponTypeInfo
{
	CSWeaponType type;
	const char * name;
};

//--------------------------------------------------------------------------------------------------------
WeaponTypeInfo s_weaponTypeInfo[] =
{
	{ WEAPONTYPE_KNIFE,			"Knife" },
	{ WEAPONTYPE_PISTOL,		"Pistol" },
	{ WEAPONTYPE_SUBMACHINEGUN, "Submachine Gun" },	// First match is printable
	{ WEAPONTYPE_SUBMACHINEGUN, "submachinegun" },
	{ WEAPONTYPE_SUBMACHINEGUN, "smg" },
	{ WEAPONTYPE_RIFLE,			"Rifle" },
	{ WEAPONTYPE_SHOTGUN,		"Shotgun" },
	{ WEAPONTYPE_SNIPER_RIFLE,	"SniperRifle" },
	{ WEAPONTYPE_MACHINEGUN,	"Machine Gun" },		// First match is printable
	{ WEAPONTYPE_MACHINEGUN,	"machinegun" },
	{ WEAPONTYPE_MACHINEGUN,	"mg" },
	{ WEAPONTYPE_C4,			"C4" },
	{ WEAPONTYPE_GRENADE,		"Grenade" },
	{ WEAPONTYPE_STACKABLEITEM,	"StackableItem" },
	{ WEAPONTYPE_FISTS,			"Fists" },
	{ WEAPONTYPE_BREACHCHARGE,  "Breach Charge" },
	{ WEAPONTYPE_BREACHCHARGE,  "breachcharge" },
	{ WEAPONTYPE_TABLET,		"Tablet" },
	{ WEAPONTYPE_MELEE,			"Melee" },
};


struct WeaponNameInfo
{
	CSchemaItemDefHandle id;
	const char *name;
};

// note you have to add an item here if it is a new base item that derives from another base item (like the silenced m4)
WeaponNameInfo s_weaponNameInfo[] =
{
	{ WEAPON_DEAGLE,			"weapon_deagle" },
	{ WEAPON_DEAGLE,			"weapon_revolver" },
	{ WEAPON_ELITE,				"weapon_elite" },
	{ WEAPON_FIVESEVEN,			"weapon_fiveseven" },
	{ WEAPON_FIVESEVEN,			"weapon_cz75a" },
	{ WEAPON_GLOCK,				"weapon_glock" },

	{ WEAPON_AK47,				"weapon_ak47" },
	{ WEAPON_AUG,				"weapon_aug" },
	{ WEAPON_AWP,				"weapon_awp" },
	{ WEAPON_FAMAS,				"weapon_famas" },
	{ WEAPON_G3SG1,				"weapon_g3sg1" },
	{ WEAPON_GALILAR,			"weapon_galilar" },
	{ WEAPON_M249,				"weapon_m249" },
	{ WEAPON_M4A1,				"weapon_m4a1" },
	{ WEAPON_M4A1,				"weapon_m4a1_silencer" },
	{ WEAPON_MAC10,				"weapon_mac10" },
	{ WEAPON_P90,				"weapon_p90" },
	{ WEAPON_UMP45,				"weapon_ump45" },
	{ WEAPON_XM1014,			"weapon_xm1014" },

	{ WEAPON_BIZON,				"weapon_bizon" },
	{ WEAPON_MAG7,				"weapon_mag7" },
	{ WEAPON_NEGEV,				"weapon_negev" },
	{ WEAPON_SAWEDOFF,			"weapon_sawedoff" },
	{ WEAPON_TEC9,				"weapon_tec9" },
	{ WEAPON_TEC9,				"weapon_cz75a" },
	{ WEAPON_TASER,				"weapon_taser" },

	{ WEAPON_HKP2000,			"weapon_hkp2000" },
	{ WEAPON_MP7,				"weapon_mp7" },
	{ WEAPON_MP9,				"weapon_mp9" },
	{ WEAPON_NOVA,				"weapon_nova" },
	{ WEAPON_P250,				"weapon_p250" },
	{ WEAPON_SCAR20,			"weapon_scar20" },
	{ WEAPON_SG556,				"weapon_sg556" },
	{ WEAPON_SSG08,				"weapon_ssg08" },

	{ WEAPON_KNIFE_GG,			"weapon_knifegg" },
	{ WEAPON_KNIFE,				"weapon_knife" },

	{ WEAPON_HEGRENADE,			"weapon_hegrenade" },
	{ WEAPON_SMOKEGRENADE,		"weapon_smokegrenade" },
	{ WEAPON_FLASHBANG,			"weapon_flashbang" },
	{ WEAPON_MOLOTOV,			"weapon_molotov" },
	{ WEAPON_INCGRENADE,		"weapon_incgrenade" },
	{ WEAPON_DECOY,				"weapon_decoy" },
	{ WEAPON_TAGRENADE,			"weapon_tagrenade" },
	{ WEAPON_SNOWBALL,			"weapon_snowball" },

	{ WEAPON_C4,				"weapon_c4" },

	{ WEAPON_HEALTHSHOT, "weapon_healthshot" },


	{ ITEM_KEVLAR,				"item_kevlar" },
	{ ITEM_ASSAULTSUIT,			"item_assaultsuit" },
	{ ITEM_HEAVYASSAULTSUIT,	"item_heavyassaultsuit" },	
	{ ITEM_NVG,					"item_nvg" },
	{ ITEM_DEFUSER,				"item_defuser" },

	{ WEAPON_FISTS,				"weapon_fists" },
	{ WEAPON_BREACHCHARGE,		"weapon_breachcharge" },
	{ WEAPON_TABLET,			"weapon_tablet" },
	{ WEAPON_MELEE,				"weapon_melee" },
	{ WEAPON_MELEE,				"weapon_axe" },

	{ WEAPON_FIREBOMB,			"weapon_firebomb" },
	{ WEAPON_FRAGGRENADE,		"weapon_frag_grenade" },
	{ WEAPON_DIVERSION,			"weapon_diversion" },

	{ WEAPON_NONE_HANDLE,		"weapon_none" },
};

CSWeaponCategory Helper_GetWeaponCategoryFromType( CSWeaponType type )
{
	switch ( type )
	{
		case WEAPONTYPE_KNIFE:
		case WEAPONTYPE_MELEE:
			return WEAPONCATEGORY_MELEE;

		case WEAPONTYPE_PISTOL:
			return WEAPONCATEGORY_SECONDARY;

		case WEAPONTYPE_SUBMACHINEGUN:
			return WEAPONCATEGORY_SMG;

		case WEAPONTYPE_RIFLE:
		case WEAPONTYPE_SNIPER_RIFLE:
			return WEAPONCATEGORY_RIFLE;

		case WEAPONTYPE_SHOTGUN:
		case WEAPONTYPE_MACHINEGUN:
			return WEAPONCATEGORY_HEAVY;

		default:
			return WEAPONCATEGORY_OTHER;
	}
}


//--------------------------------------------------------------------------------------------------------------

// Weapon info structure contains no data now, so we just have a single global one for code to grab
#if !USE_WEAPON_DATA_CACHE
const FileWeaponInfo_t * Helper_GetNullWeaponInfo()
{
	static CCSWeaponInfo gNullWeaponInfo;
	return &gNullWeaponInfo;
}
#endif

#if USE_WEAPON_DATA_CACHE
const OldWeaponData::CCSWeaponInfo& GetOldWeaponInfo( const CCSWeaponData* )
{
	static OldWeaponData::CCSWeaponInfo gNullWeaponInfo;
	return gNullWeaponInfo;
}
const OldWeaponData::FileWeaponInfo_t& GetOldWeaponInfo( const CWeaponData* )
{
	const CCSWeaponData* pNewWeaponData = nullptr;
	return GetOldWeaponInfo( pNewWeaponData );
}
#endif

//--------------------------------------------------------------------------------------------------------------

class CCSWeaponSystem : public CAutoGameSystem, public IWeaponSystem
{
public:
	CCSWeaponSystem()
		: CAutoGameSystem( "CSWeaponSystem" )
		, mWeaponDataMap()
		, mSchemaGeneration( ( uint32 )-1 )
	{}

	~CCSWeaponSystem() { Shutdown(); }

	virtual bool Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;

	virtual void RefreshForGame() OVERRIDE {}			// Called after gamerules sets up convars; needed if some data is convar-dependent.
	virtual void PrecacheWeaponClasses() OVERRIDE {}	// Nothing needed here as far as I know, now handled via regular precache system, but keeping the callback in the same place if that changes

	virtual const CCSWeaponData* GetWeaponData( uint32 itemIndex ) OVERRIDE;

private:
	typedef CUtlMap<item_definition_index_t, CCSWeaponData*, unsigned short, CDefLess<item_definition_index_t>> tWeaponDataMap;
	tWeaponDataMap mWeaponDataMap;

	uint32 mSchemaGeneration;
};

bool CCSWeaponSystem::Init()
{
	// Make sure weapon data is clean
	Assert( mWeaponDataMap.Count() == 0 );
	mWeaponDataMap.PurgeAndDeleteElements();
	mSchemaGeneration = ( uint32 )-1;

	return true;
}

void CCSWeaponSystem::Shutdown()
{
	mWeaponDataMap.PurgeAndDeleteElements();
	mSchemaGeneration = ( uint32 )-1;
}

const CCSWeaponData* CCSWeaponSystem::GetWeaponData( uint32 itemIndex )
{
	if ( itemIndex >= INVALID_ITEM_DEF_INDEX )
		return nullptr;

	// Check for 'big' schema update
	if ( mSchemaGeneration != GetItemSchema()->GetResetCount() )
	{
		// Delete all null elements from map (they might be ok items now).
		// We don't delete existing items in case something is still holding a pointer to them
		FOR_EACH_MAP_FAST( mWeaponDataMap, mapIndex )
		{
			if ( mWeaponDataMap.Element( mapIndex ) == nullptr )
				mWeaponDataMap.RemoveAt( mapIndex );
		}
	}

	tWeaponDataMap::IndexType_t mapIndex = mWeaponDataMap.Find( itemIndex );
	CCSWeaponData* pWeaponData = nullptr;

	if ( mapIndex == mWeaponDataMap.InvalidIndex() )
	{
		// New element!  Cache it.
		pWeaponData = new CCSWeaponData( itemIndex );

		// If it isn't a real weapon, clear it out before
		// we give any references to it.
		if ( !pWeaponData->Update() )
		{
			delete pWeaponData;
			pWeaponData = nullptr;
		}

		mWeaponDataMap.Insert( itemIndex, pWeaponData );
	}
	else
	{
		pWeaponData = mWeaponDataMap.Element( mapIndex );
	}

	// Not a weapon
	if ( !pWeaponData )
		return nullptr;

	// If it somehow stopped being a weapon since the last update (due to schema reload)
	// don't return any new references to it.
	if ( !pWeaponData->Update() )
		return nullptr;

	return pWeaponData;
}

static CCSWeaponSystem sCSWeaponSystem;
IWeaponSystem* g_pWeaponSystem = &sCSWeaponSystem;

//--------------------------------------------------------------------------------------------------------------


struct EquipmentInfo
{
	const char*		szClassName;
};

// Special case "weapon info" for stuff not in old weapon scripts.  $$$REI TODO KILL WITH FIRE
static const char* g_EquipmentInfo[] = {
	"item_kevlar",
	"item_assaultsuit",
	"item_heavyassaultsuit",
	"item_defuser"
};

bool WeaponIsBallistic( CSWeaponType wepType )
{
	// AKA "is a gun". I need a function that indicates 'if I point this at you from far away, you should be worried'.
	// TODO: should this really read ammo type? What if we add a new class of ballistic weapon?

	return (wepType >= WEAPONTYPE_PISTOL && wepType <= WEAPONTYPE_MACHINEGUN);
}


//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponTypeInfo ); ++i )
	{
		if ( s_weaponTypeInfo[i].type == weaponType )
		{
			return s_weaponTypeInfo[i].name;
		}
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromString( const char* weaponType )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponTypeInfo ); ++i )
	{
		if ( !V_stricmp( s_weaponTypeInfo[i].name, weaponType ) )
		{
			return s_weaponTypeInfo[i].type;
		}
	}

	return WEAPONTYPE_UNKNOWN;
}


//--------------------------------------------------------------------------------------------------------
const char * WeaponIdAsString( CSWeaponID weaponID )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponNameInfo ); ++i )
	{
		if ( s_weaponNameInfo[i].id == weaponID )
			return s_weaponNameInfo[i].name;
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponID WeaponIdFromString( const char *szWeaponName )
{
	for ( int i = 0; i < ARRAYSIZE( s_weaponNameInfo ); ++i )
	{
		if ( V_stricmp( s_weaponNameInfo[i].name, szWeaponName ) == 0 )
			return WeaponIDFromDefinition( s_weaponNameInfo[i].id );
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( CSWeaponID id )
{
	const CEconItemDefinition* pWeaponDef = WeaponIDToDefinition( id );

	if ( pWeaponDef != nullptr )
	{
		const char* szName = pWeaponDef->GetDefinitionName();
		const char* szAlias = strchr( szName, '_' );
		if ( szAlias )
			szName = szAlias + 1;

		return szName;
	}

	return nullptr;
}

bool IsGunWeapon( CSWeaponType weaponType )
{
	switch ( weaponType )
	{
	case WEAPONTYPE_PISTOL:
	case WEAPONTYPE_SUBMACHINEGUN:
	case WEAPONTYPE_RIFLE:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_SNIPER_RIFLE:
	case WEAPONTYPE_MACHINEGUN:
		return true;

	default:
		return false;
	}
}

uint8 WeaponIDToByte( CSWeaponID weapon )
{
	const CEconItemDefinition* pWeaponDef = WeaponIDToDefinition( weapon );

	if ( pWeaponDef == nullptr )
		return 0;

	// This isn't a base weapon / equipment if it is out of this range
	Assert( pWeaponDef->GetDefinitionIndex() != 0 && pWeaponDef->GetDefinitionIndex() < 256 );
	if ( pWeaponDef->GetDefinitionIndex() >= 256 )
		return 0;

	return pWeaponDef->GetDefinitionIndex();
}

CSWeaponID WeaponIDFromByte( uint8 weaponIdByte )
{
	if ( weaponIdByte == 0 )
		return WeaponIDFromDefinition( nullptr );

	return WeaponIDFromDefinition( GetItemSchema()->GetItemDefinition( weaponIdByte, true ) );
}

//--------------------------------------------------------------------------------------------------------
void ParseVector( KeyValues *keyValues, const char *keyName, Vector& vec )
{
	vec.x = vec.y = vec.z = 0.0f;

	if ( !keyValues || !keyName )
		return;

	const char *vecString = keyValues->GetString( keyName, "0 0 0" );
	if ( vecString && *vecString )
	{
		float x, y, z;
		if ( 3 == sscanf( vecString, "%f %f %f", &x, &y, &z ) )
		{
			vec.x = x;
			vec.y = y;
			vec.z = z;
		}
	}
}


struct EnumerationStringValue
{
	const char* szString;
	int iValue;
};

int ParseEnumeration( KeyValues* pKeyValuesData, const char* szKeyName, const EnumerationStringValue enumStringTable[], int iCount, int iDefaultValue )
{
	const char *szKeyValue = pKeyValuesData->GetString( szKeyName, NULL );
	if ( !szKeyValue )
		return iDefaultValue;

	for ( int i = 0; i < iCount; ++i )
	{
		if ( V_stricmp( szKeyValue, enumStringTable[i].szString ) == 0 )
			return enumStringTable[i].iValue;
	}
	
	Assert( false );
	return iDefaultValue;
}


#if USE_WEAPON_DATA_CACHE
OldWeaponData::CCSWeaponInfo::CCSWeaponInfo()
#else
CCSWeaponInfo::CCSWeaponInfo()
#endif
{
}

#if USE_WEAPON_DATA_CACHE
namespace OldWeaponData {
#endif

CSWeaponType CCSWeaponInfo::GetWeaponType( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		const char *pszString = pWepView->GetStaticData()->GetWeaponTypeString();
		
		if ( pszString )
		{
			return WeaponClassFromString( pszString );
		}
	}

	return WEAPONTYPE_UNKNOWN;
}

CSWeaponCategory CCSWeaponInfo::GetWeaponCategory( const CEconItemView* pWepView ) const
{
	return Helper_GetWeaponCategoryFromType( GetWeaponType( pWepView ) );
}

const char* CCSWeaponInfo::GetAddonLocation( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetAddonLocation();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}

const char* CCSWeaponInfo::GetEjectBrassEffectName( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetEjectBrassEffect();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}


const char* CCSWeaponInfo::GetTracerEffectName( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetTracerEffect();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_1stPerson( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect1stPerson();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_1stPersonAlt( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect1stPersonAlt();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_3rdPerson( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect3rdPerson();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}

const char* CCSWeaponInfo::GetMuzzleFlashEffectName_3rdPersonAlt( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetMuzzleFlashEffect3rdPersonAlt();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}

const char* CCSWeaponInfo::GetHeatEffectName( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetHeatEffect();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "";
}

const char* CCSWeaponInfo::GetAnimExtension( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetPlayerAnimationExtension();

		if ( pszString )
		{
			return pszString;
		}
	}

	return "m4";
}

int CCSWeaponInfo::GetUsedByTeam( const CEconItemView* pWepView ) const
{
	Assert( pWepView && pWepView->IsValid() );

	if ( pWepView && pWepView->IsValid() )
	{
		return pWepView->GetStaticData()->GetUsedByTeam();
	}

	return TEAM_UNASSIGNED;
}

const char* CCSWeaponInfo::GetAddonModel( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char* pchAddon = pWepView->GetStaticData()->GetPlayerHolsteredModel();
		if ( pchAddon )
			return pchAddon;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}

const CUtlVector< WeaponPaintableMaterial_t >* CCSWeaponInfo::GetPaintData( const CEconItemView* pWepView ) const
{
	if ( !pWepView || !pWepView->IsValid() )
		return NULL;

	return pWepView->GetStaticData()->GetPaintData();
}

const char* CCSWeaponInfo::GetZoomInSound( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char* pchZoomInSound = pWepView->GetItemDefinition()->GetZoomInSound();
		if ( pchZoomInSound )
			return pchZoomInSound;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}

const char* CCSWeaponInfo::GetZoomOutSound( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char* pchZoomOutSound = pWepView->GetItemDefinition()->GetZoomOutSound();
		if ( pchZoomOutSound )
			return pchZoomOutSound;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}

CSWeaponID CCSWeaponInfo::GetWeaponID( const CEconItemView* pWeaponView ) const
{
	if ( pWeaponView && pWeaponView->IsValid() )
	{
		const CEconItemDefinition* pDefinition = pWeaponView->GetStaticData();

		// $$$REI Currently weapon ids correspond to classes from the data.  They *should* correspond
		// $$$REI to individual items (e.g. revolver vs deagle, m4a1-s vs m4a4).  However, currently
		// $$$REI knives have unique items for each different knife model, so we still need some way
		// $$$REI to access the 'base' item for a particular item.
		// $$$REI
		// $$$REI I'm hoping to end around this entirely by removing weapon id from the game, but it's
		// $$$REI currently used too heavily to make that easy.
		return WeaponIdFromString( pDefinition->GetItemClass() );
	}

	Assert( false );
	return WEAPON_NONE;
}

const Vector& CCSWeaponInfo::GetSmokeColor( const CEconItemView* pWeaponView ) const
{
	if ( pWeaponView && pWeaponView->IsValid() )
	{
		return pWeaponView->GetStaticData()->GetAssetInfo()->m_vGrenadeSmokeColor;
	}

	Assert( false );
	static Vector sBlack( 0, 0, 0 );
	return sBlack;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Get an attribute associated with the weapon. It might seem a bit odd that in order to get 
// a weapon's damage you need to ask its deprecated weaponinfo object to extract it from its own econitemview
// but we do this because In CS, there are a bunch of cases where there is no weapon instantiated yet and we need the data. All of this
// legacy code uses weaponinfo objects.

// 
// POSSIBLE RETURNS:
//	-2: total failure. We did not find the attribute in the econ item or in the weaponinfo instance.
//	-1: the attribute was found in the econitemview that was passed in.
//	>=0: the returned int is the index into g_WeaponInfoTable that contains the desired data.
//////////////////////////////////////////////////////////////////////////////////////////////////////

static bool GetAttribute_bool( const CCSWeaponInfo* pWeaponInfo, const char * pszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView )
{
	uint32 unLocalValue;
	Assert( pWepView && pWepView->IsValid() );
	if ( pWepView && pWepView->IsValid() && pWepView->FindAttribute( hAttrib, &unLocalValue ) )
	{
		return ( unLocalValue != 0 );
	}

	return false;
}

static int GetAttribute_int( const CCSWeaponInfo* pWeaponInfo, const char * pszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView )
{
	uint32 unLocalValue;
	Assert( pWepView && pWepView->IsValid() );
	if ( pWepView && pWepView->IsValid() && pWepView->FindAttribute( hAttrib, &unLocalValue ) )
	{
		return (int32)unLocalValue;
	}

	return 0;
}

static float GetAttribute_float( const CCSWeaponInfo* pWeaponInfo, const char * pszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView, float flScale = 1.0f )
{
	float flLocalValue;
	Assert( pWepView && pWepView->IsValid() );
	if ( pWepView && pWepView->IsValid() && pWepView->FindAttribute( hAttrib, &flLocalValue ) )
	{
		return flScale * flLocalValue;
	}

	return 0;
}

static const char* GetAttribute_string( const CCSWeaponInfo* pWeaponInfo, const char * pszAttrib, CSchemaAttributeDefHandle hAttrib, const CEconItemView *pWepView )
{
	const char* pszLocalValue = nullptr;
	Assert( pWepView && pWepView->IsValid() );
	if ( pWepView && pWepView->IsValid() && FindAttribute_UnsafeBitwiseCast<CAttribute_String>( pWepView, hAttrib, &pszLocalValue ) )
	{
		return pszLocalValue;
	}

	return "";
}

static inline bool GetWeaponAttrBool( const CCSWeaponInfo* pThis, const CEconItemView* pWepView, int nAlt, const char* pszAttrib, const char* pszAttribAlt, const CSchemaAttributeDefHandle& hAttrib, const CSchemaAttributeDefHandle& hAttribAlt )
{
	if ( nAlt )
	{
		AssertMsg1( hAttribAlt, "Attribute '%s' not defined", pszAttribAlt );
		return GetAttribute_bool( pThis, pszAttribAlt, hAttribAlt, pWepView );
	}
	else
	{
		AssertMsg1( hAttrib, "Attribute '%s' not defined", pszAttrib );
		return GetAttribute_bool( pThis, pszAttrib, hAttrib, pWepView );
	}

}

static inline int GetWeaponAttrInt( const CCSWeaponInfo* pThis, const CEconItemView* pWepView, int nAlt, const char* pszAttrib, const char* pszAttribAlt, const CSchemaAttributeDefHandle& hAttrib, const CSchemaAttributeDefHandle& hAttribAlt )
{
	if ( nAlt )
	{
		AssertMsg1( hAttribAlt, "Attribute '%s' not defined", pszAttribAlt );
		return GetAttribute_int( pThis, pszAttribAlt, hAttribAlt, pWepView );
	}
	else
	{
		AssertMsg1( hAttrib, "Attribute '%s' not defined", pszAttrib );
		return GetAttribute_int( pThis, pszAttrib, hAttrib, pWepView );
	}

}

static inline float GetWeaponAttrFloat( const CCSWeaponInfo* pThis, const CEconItemView* pWepView, int nAlt, float flScale, const char* pszAttrib, const char* pszAttribAlt, const CSchemaAttributeDefHandle& hAttrib, const CSchemaAttributeDefHandle& hAttribAlt )
{
	if ( nAlt )
	{
		AssertMsg1( hAttribAlt, "Attribute '%s' not defined", pszAttribAlt );
		return GetAttribute_float( pThis, pszAttribAlt, hAttribAlt, pWepView, flScale );
	}
	else
	{
		AssertMsg1( hAttrib, "Attribute '%s' not defined", pszAttrib );
		return GetAttribute_float( pThis, pszAttrib, hAttrib, pWepView, flScale );
	}
}

static inline const char* GetWeaponAttrString( const CCSWeaponInfo* pThis, const CEconItemView* pWepView, int nAlt, const char* pszAttrib, const char* pszAttribAlt, const CSchemaAttributeDefHandle& hAttrib, const CSchemaAttributeDefHandle& hAttribAlt )
{
	if ( nAlt )
	{
		AssertMsg1( hAttribAlt, "Attribute '%s' not defined", pszAttribAlt );
		return GetAttribute_string( pThis, pszAttribAlt, hAttribAlt, pWepView );
	}
	else
	{
		AssertMsg1( hAttrib, "Attribute '%s' not defined", pszAttrib );
		return GetAttribute_string( pThis, pszAttrib, hAttrib, pWepView );
	}
}

// Weapon attribute accessor is macroized so that we can use static attribute def handles 
// because generating them at every access is costly.

#define GET_WEAPON_ATTR_BOOL( functionname, attrname )\
bool CCSWeaponInfo::functionname( const CEconItemView* pWepView, int nAlt ) const\
{\
	const char* pszAttrib = attrname;\
	const char* pszAttribAlt = attrname " alt";\
	static CSchemaAttributeDefHandle hAttrib( pszAttrib );\
	static CSchemaAttributeDefHandle hAttribAlt( pszAttribAlt );\
	\
	return GetWeaponAttrBool( this, pWepView, nAlt, pszAttrib, pszAttribAlt, hAttrib, hAttribAlt ); \
}

#define GET_WEAPON_ATTR_INT( functionname, attrname )\
int CCSWeaponInfo::functionname( const CEconItemView* pWepView, int nAlt ) const\
{\
	const char* pszAttrib = attrname;\
	const char* pszAttribAlt = attrname " alt";\
	static CSchemaAttributeDefHandle hAttrib( pszAttrib );\
	static CSchemaAttributeDefHandle hAttribAlt( pszAttribAlt );\
	\
	return GetWeaponAttrInt( this, pWepView, nAlt, pszAttrib, pszAttribAlt, hAttrib, hAttribAlt );\
}

#define GET_WEAPON_ATTR_FLOAT( functionname, attrname )\
float CCSWeaponInfo::functionname( const CEconItemView* pWepView, int nAlt ) const\
{\
	const char* pszAttrib = attrname;\
	const char* pszAttribAlt = attrname " alt";\
	static CSchemaAttributeDefHandle hAttrib( pszAttrib );\
	static CSchemaAttributeDefHandle hAttribAlt( pszAttribAlt );\
	\
	return GetWeaponAttrFloat(this, pWepView, nAlt, 1.0f, pszAttrib, pszAttribAlt, hAttrib, hAttribAlt);\
}

#define GET_WEAPON_ATTR_FSCALE( functionname, attrname, scale )\
float CCSWeaponInfo::functionname( const CEconItemView* pWepView, int nAlt ) const\
{\
	const char* pszAttrib = attrname;\
	const char* pszAttribAlt = attrname " alt";\
	static CSchemaAttributeDefHandle hAttrib( pszAttrib );\
	static CSchemaAttributeDefHandle hAttribAlt( pszAttribAlt );\
	\
	return GetWeaponAttrFloat(this, pWepView, nAlt, scale, pszAttrib, pszAttribAlt, hAttrib, hAttribAlt);\
}

#define GET_WEAPON_ATTR_STRING( functionname, attrname )\
const char* CCSWeaponInfo::functionname( const CEconItemView* pWepView, int nAlt ) const\
{\
	const char* pszAttrib = attrname;\
	const char* pszAttribAlt = attrname " alt";\
	static CSchemaAttributeDefHandle hAttrib( pszAttrib );\
	static CSchemaAttributeDefHandle hAttribAlt( pszAttribAlt );\
	\
	return GetWeaponAttrString( this, pWepView, nAlt, pszAttrib, pszAttribAlt, hAttrib, hAttribAlt );\
}



GET_WEAPON_ATTR_INT(	GetWeaponPrice,					"in game price" )
GET_WEAPON_ATTR_BOOL(	IsFullAuto,						"is full auto" )
GET_WEAPON_ATTR_BOOL(	HasSilencer,					"has silencer" )
GET_WEAPON_ATTR_INT(	GetBullets,						"bullets" )
GET_WEAPON_ATTR_FLOAT(	GetCycleTime,					"cycletime" )
GET_WEAPON_ATTR_FLOAT(	GetHeatPerShot,					"heat per shot" )
GET_WEAPON_ATTR_FLOAT(	GetRecoveryTimeCrouch,			"recovery time crouch" )
GET_WEAPON_ATTR_FLOAT(	GetRecoveryTimeStand,			"recovery time stand" )
GET_WEAPON_ATTR_FLOAT(	GetRecoveryTimeCrouchFinal,		"recovery time crouch final" )
GET_WEAPON_ATTR_FLOAT(	GetRecoveryTimeStandFinal,		"recovery time stand final" )
GET_WEAPON_ATTR_INT(	GetRecoveryTransitionStartBullet, "recovery transition start bullet" )
GET_WEAPON_ATTR_INT(	GetRecoveryTransitionEndBullet, "recovery transition end bullet" )


GET_WEAPON_ATTR_INT(	GetRecoilSeed,					"recoil seed" )
GET_WEAPON_ATTR_FLOAT(	GetFlinchVelocityModifierLarge, "flinch velocity modifier large" )
GET_WEAPON_ATTR_FLOAT(	GetFlinchVelocityModifierSmall, "flinch velocity modifier small" )
GET_WEAPON_ATTR_FLOAT(	GetTimeToIdleAfterFire,			"time to idle" )
GET_WEAPON_ATTR_FLOAT(	GetIdleInterval,				"idle interval" )
GET_WEAPON_ATTR_FLOAT(	GetRange,						"range" )
GET_WEAPON_ATTR_FLOAT(	GetRangeModifier,				"range modifier" )
GET_WEAPON_ATTR_INT(	GetDamage,						"damage" )
GET_WEAPON_ATTR_FLOAT(	GetPenetration,					"penetration" )
GET_WEAPON_ATTR_INT(	GetCrosshairDeltaDistance,		"crosshair delta distance" )
GET_WEAPON_ATTR_INT(	GetCrosshairMinDistance,		"crosshair min distance" )

GET_WEAPON_ATTR_FLOAT(	GetMaxSpeed,					"max player speed" )
GET_WEAPON_ATTR_FSCALE(	GetSpread,						"spread",							0.001f )
GET_WEAPON_ATTR_FSCALE(	GetInaccuracyCrouch,			"inaccuracy crouch",				0.001f )
GET_WEAPON_ATTR_FSCALE(	GetInaccuracyStand,				"inaccuracy stand",					0.001f )
GET_WEAPON_ATTR_FSCALE( GetInaccuracyJumpInitial,       "inaccuracy jump initial",			0.001f )
GET_WEAPON_ATTR_FSCALE(	GetInaccuracyJump,				"inaccuracy jump",					0.001f )
GET_WEAPON_ATTR_FSCALE(	GetInaccuracyLand,				"inaccuracy land",					0.001f )
GET_WEAPON_ATTR_FSCALE(	GetInaccuracyLadder,			"inaccuracy ladder",				0.001f )
GET_WEAPON_ATTR_FSCALE(	GetInaccuracyFire,				"inaccuracy fire",					0.001f )
GET_WEAPON_ATTR_FSCALE(	GetInaccuracyMove,				"inaccuracy move",					0.001f )

GET_WEAPON_ATTR_FSCALE( GetInaccuracyReload,			"inaccuracy reload",				0.001f )

GET_WEAPON_ATTR_FLOAT(	GetRecoilAngle,					"recoil angle" )
GET_WEAPON_ATTR_FLOAT(	GetRecoilAngleVariance,			"recoil angle variance" )
GET_WEAPON_ATTR_FLOAT(	GetRecoilMagnitude,				"recoil magnitude" )
GET_WEAPON_ATTR_FLOAT(	GetRecoilMagnitudeVariance,		"recoil magnitude variance" )
GET_WEAPON_ATTR_INT( GetTracerFrequency,				"tracer frequency" )

GET_WEAPON_ATTR_INT(	GetPrimaryClipSize,				"primary clip size" )
GET_WEAPON_ATTR_INT(	GetSecondaryClipSize,			"secondary clip size" )
GET_WEAPON_ATTR_INT(	GetDefaultPrimaryClipSize,		"primary default clip size" )
GET_WEAPON_ATTR_INT(	GetDefaultSecondaryClipSize,	"secondary default clip size" )

GET_WEAPON_ATTR_INT(	GetKillAward,					"kill award" )
GET_WEAPON_ATTR_BOOL(	HasBurstMode,					"has burst mode" )
GET_WEAPON_ATTR_BOOL(	IsRevolver,						"is revolver" )
GET_WEAPON_ATTR_FLOAT(	GetArmorRatio,					"armor ratio" )
GET_WEAPON_ATTR_BOOL(	CannotShootUnderwater,			"cannot shoot underwater" )
GET_WEAPON_ATTR_BOOL(	DoesUnzoomAfterShot,			"unzoom after shot" )
GET_WEAPON_ATTR_BOOL(	DoesHideViewModelWhenZoomed,	"hide view model zoomed" )
GET_WEAPON_ATTR_INT(	GetZoomLevels,					"zoom levels" )
GET_WEAPON_ATTR_INT(	GetZoomFOV1,					"zoom fov 1" )
GET_WEAPON_ATTR_INT(	GetZoomFOV2,					"zoom fov 2" )
GET_WEAPON_ATTR_FLOAT(	GetZoomTime0,					"zoom time 0" )
GET_WEAPON_ATTR_FLOAT(	GetZoomTime1,					"zoom time 1" )
GET_WEAPON_ATTR_FLOAT(	GetZoomTime2,					"zoom time 2" )

GET_WEAPON_ATTR_INT(	GetPrimaryReserveAmmoMax,		"primary reserve ammo max" )
GET_WEAPON_ATTR_INT(	GetSecondaryReserveAmmoMax,		"secondary reserve ammo max" )

GET_WEAPON_ATTR_STRING( GetWrongTeamMsg,				"wrong team msg" )
GET_WEAPON_ATTR_STRING( GetSilencerModel,				"silencer model" )
GET_WEAPON_ATTR_FLOAT(	GetAddonScale,					"addon scale" )
GET_WEAPON_ATTR_FLOAT(	GetBotAudibleRange,				"bot audible range" )
GET_WEAPON_ATTR_FLOAT(	GetThrowVelocity,				"throw velocity" )

GET_WEAPON_ATTR_FLOAT(  GetInaccuracyPitchShift,		"inaccuracy pitch shift" )
GET_WEAPON_ATTR_FLOAT(  GetInaccuracyAltSoundThreshold, "inaccuracy alt sound threshold" )

GET_WEAPON_ATTR_FLOAT( GetAttackMovespeedFactor,  "attack movespeed factor" )

#if USE_WEAPON_DATA_CACHE
} // namespace OldWeaponData
#endif


//////////////////////////////////////////////////////////////////////////
// CCSWeaponData
//////////////////////////////////////////////////////////////////////////

bool CCSWeaponData::Fill()
{
	const CCStrike15ItemDefinition* pItemDef = GetItem();

	// Early out for non-weapons
	if ( !pItemDef->GetWeaponTypeString() )
		return false;

	if ( !BaseClass::Fill() )
		return false;

	// $$$REI Currently weapon ids correspond to classes from the data.  They *should* correspond
	// $$$REI to individual items (e.g. revolver vs deagle, m4a1-s vs m4a4).  However, currently
	// $$$REI knives have unique items for each different knife model, so we still need some way
	// $$$REI to access the 'base' item for a particular item.
	// $$$REI
	// $$$REI I'm hoping to end around this entirely by removing weapon id from the game, but it's
	// $$$REI currently used too heavily to make that easy.
	m_WeaponID							= WeaponIdFromString( m_szClassName );

	// Bail if we failed to match an existing weapon id.  TODO: This is a hack, need a better way to handle this
	if ( m_WeaponID == WEAPON_NONE )
		return false;

	m_WeaponType						= WeaponClassFromString( pItemDef->GetWeaponTypeString() );
	m_WeaponCategory					= Helper_GetWeaponCategoryFromType( m_WeaponType );

	m_nPrice							= ITEM_ATTR_INT( "in game price" );
	m_nKillAward						= ITEM_ATTR_INT( "kill award" );

	m_szAnimExtension					= WithDefault( pItemDef->GetPlayerAnimationExtension(), "m4" );

	FIRINGMODE_ATTR_FLOAT( m_flCycleTime, "cycletime" );
	m_flTimeToIdleAfterFire				= ITEM_ATTR_FLOAT( "time to idle" );
	m_flIdleInterval					= ITEM_ATTR_FLOAT( "idle interval" );
	m_bIsFullAuto						= ITEM_ATTR_BOOL( "is full auto" );

	m_nDamage							= ITEM_ATTR_INT( "damage" );
	m_flArmorRatio						= ITEM_ATTR_FLOAT( "armor ratio" );
	m_nNumBullets						= ITEM_ATTR_INT( "bullets" );
	m_flPenetration						= ITEM_ATTR_FLOAT( "penetration" );
	m_flFlinchVelocityModifierLarge		= ITEM_ATTR_FLOAT( "flinch velocity modifier large" );
	m_flFlinchVelocityModifierSmall		= ITEM_ATTR_FLOAT( "flinch velocity modifier small" );
	m_flRange							= ITEM_ATTR_FLOAT( "range" );
	m_flRangeModifier					= ITEM_ATTR_FLOAT( "range modifier" );

	m_flThrowVelocity					= ITEM_ATTR_FLOAT( "throw velocity" );
	m_vSmokeColor						= pItemDef->GetAssetInfo()->m_vGrenadeSmokeColor;

	m_eSilencerType						= (CSWeaponSilencerType) ITEM_ATTR_INT( "has silencer" );
	m_szSilencerModel					= WithDefault( ITEM_ATTR_STRING( "silencer model" ), "" );

	m_nCrosshairMinDistance				= ITEM_ATTR_INT( "crosshair min distance" );
	m_nCrosshairDeltaDistance			= ITEM_ATTR_INT( "crosshair delta distance" );

	FIRINGMODE_ATTR_FLOAT( m_flMaxSpeed,				"max player speed" );

	m_flAttackMovespeedFactor			= ITEM_ATTR_FLOAT( "attack movespeed factor" );


	FIRINGMODE_ATTR_FLOAT_SCALE( m_flSpread,			"spread", 0.001f );
	FIRINGMODE_ATTR_FLOAT_SCALE( m_flInaccuracyCrouch,	"inaccuracy crouch", 0.001f );
	FIRINGMODE_ATTR_FLOAT_SCALE( m_flInaccuracyStand,	"inaccuracy stand", 0.001f );
	m_flInaccuracyJumpInitial			= ITEM_ATTR_FLOAT_SCALE( "inaccuracy jump initial", 0.001f );
	FIRINGMODE_ATTR_FLOAT_SCALE( m_flInaccuracyJump,	"inaccuracy jump", 0.001f );
	FIRINGMODE_ATTR_FLOAT_SCALE( m_flInaccuracyLand,	"inaccuracy land", 0.001f );
	FIRINGMODE_ATTR_FLOAT_SCALE( m_flInaccuracyLadder,	"inaccuracy ladder", 0.001f );
	FIRINGMODE_ATTR_FLOAT_SCALE( m_flInaccuracyFire,	"inaccuracy fire", 0.001f );
	FIRINGMODE_ATTR_FLOAT_SCALE( m_flInaccuracyMove,	"inaccuracy move", 0.001f );
	m_flInaccuracyReload				= ITEM_ATTR_FLOAT_SCALE( "inaccuracy reload", 0.001f );
	// mInaccuracyAltSwitch = ...

	m_nRecoilSeed						= (int) ITEM_ATTR_FLOAT( "recoil seed" );		// Recoil seed is stored as a float attribute but always used as int
	FIRINGMODE_ATTR_FLOAT( m_flRecoilAngle,				"recoil angle" );
	FIRINGMODE_ATTR_FLOAT( m_flRecoilAngleVariance,		"recoil angle variance" );
	FIRINGMODE_ATTR_FLOAT( m_flRecoilMagnitude,			"recoil magnitude" );
	FIRINGMODE_ATTR_FLOAT( m_flRecoilMagnitudeVariance,	"recoil magnitude variance" );
	m_nSpreadSeed						= ITEM_ATTR_INT( "spread seed" );

	m_flRecoveryTimeCrouch				= ITEM_ATTR_FLOAT( "recovery time crouch" );
	m_flRecoveryTimeStand				= ITEM_ATTR_FLOAT( "recovery time stand" );
	m_flRecoveryTimeCrouchFinal			= ITEM_ATTR_FLOAT( "recovery time crouch final" );
	m_flRecoveryTimeStandFinal			= ITEM_ATTR_FLOAT( "recovery time stand final" );
	m_nRecoveryTransitionStartBullet	= ITEM_ATTR_INT( "recovery transition start bullet" );
	m_nRecoveryTransitionEndBullet		= ITEM_ATTR_INT( "recovery transition end bullet" );

	m_bUnzoomsAfterShot					= ITEM_ATTR_BOOL( "unzoom after shot" );
	m_bHideViewModelWhenZoomed			= ITEM_ATTR_BOOL( "hide view model zoomed" );
	m_nZoomLevels						= ITEM_ATTR_INT( "zoom levels" );
	m_nZoomFOV1							= ITEM_ATTR_INT( "zoom fov 1" );
	m_nZoomFOV2							= ITEM_ATTR_INT( "zoom fov 2" );
	m_flZoomTime0						= ITEM_ATTR_FLOAT( "zoom time 0" );
	m_flZoomTime1						= ITEM_ATTR_FLOAT( "zoom time 1" );
	m_flZoomTime2						= ITEM_ATTR_FLOAT( "zoom time 2" );

	m_szAddonLocation					= WithDefault( pItemDef->GetAddonLocation(), "" );
	m_szAddonModel						= WithDefault( pItemDef->GetPlayerHolsteredModel(), "" );
	m_flAddonScale						= ITEM_ATTR_FLOAT( "addon scale" );

	m_szEjectBrassEffectName			= WithDefault( pItemDef->GetEjectBrassEffect(), "" );
	m_szTracerEffectName				= WithDefault( pItemDef->GetTracerEffect(), "" );
	FIRINGMODE_ATTR_INT( m_nTracerFrequency, "tracer frequency" );
	m_szMuzzleFlashEffectName_1stPerson[0] = WithDefault( pItemDef->GetMuzzleFlashEffect1stPerson(), "" );
	m_szMuzzleFlashEffectName_1stPerson[1] = WithDefault( pItemDef->GetMuzzleFlashEffect1stPersonAlt(), "" );
	m_szMuzzleFlashEffectName_3rdPerson[0] = WithDefault( pItemDef->GetMuzzleFlashEffect3rdPerson(), "" );
	m_szMuzzleFlashEffectName_3rdPerson[1] = WithDefault( pItemDef->GetMuzzleFlashEffect3rdPersonAlt(), "" );
	m_szHeatEffectName					= WithDefault( pItemDef->GetHeatEffect(), "" );
	m_flHeatPerShot						= ITEM_ATTR_FLOAT( "heat per shot" );

	m_szZoomInSound						= WithDefault( pItemDef->GetZoomInSound(), "" );
	m_szZoomOutSound					= WithDefault( pItemDef->GetZoomOutSound(), "" );
	m_flInaccuracyPitchShfit			= ITEM_ATTR_FLOAT( "inaccuracy pitch shift" );
	m_flInaccuracyAltSoundThreshold		= ITEM_ATTR_FLOAT( "inaccuracy alt sound threshold" );
	m_flBotAudibleRange					= ITEM_ATTR_FLOAT( "bot audible range" );

	m_PaintData							= pItemDef->GetPaintData();

	m_nUsedByTeam						= pItemDef->GetUsedByTeam();
	m_szWrongTeamMsg					= WithDefault( ITEM_ATTR_STRING( "wrong team msg" ), "");
	m_bHasBurstMode						= ITEM_ATTR_BOOL( "has burst mode" );
	m_bIsRevolver						= ITEM_ATTR_BOOL( "is revolver" );
	m_bCannotShootUnderwater			= ITEM_ATTR_BOOL( "cannot shoot underwater" );

	return true;
}


WeaponRecoilData::WeaponRecoilData()
{
	m_mapRecoilTables.SetLessFunc( DefLessFunc( item_definition_index_t ) );
}

WeaponRecoilData::~WeaponRecoilData()
{
	m_mapRecoilTables.PurgeAndDeleteElements();
}

static inline float AttrValueAsFloat( attrib_value_t val )
{
	float flValue;
	Q_memcpy( &flValue, &val, sizeof( float ) );
	return flValue;
}

void WeaponRecoilData::GenerateRecoilTable( RecoilData *data )
{
	const int iSuppressionShots = weapon_recoil_suppression_shots.GetInt();
	const float fBaseSuppressionFactor = weapon_recoil_suppression_factor.GetFloat();
	const float fRecoilVariance = weapon_recoil_variance.GetFloat();
	CUniformRandomStream recoilRandom;

	if ( !data )
		return;

	const CEconItemDefinition *pEconItemDefinition = GetItemSchema()->GetItemDefinition( data->iItemDefIndex );
	Assert( pEconItemDefinition );

	if ( !pEconItemDefinition )
		return;

	// Walk the attributes to determine all things that we need
	int iSeed = 0;
	bool bHasAttrSeed = false;
	bool bFullAuto = false;
	bool bHasAttrFullAuto = false;
	float flRecoilAngle[2] = {};
	bool bHasAttrRecoilAngle[2] = {};
	float flRecoilAngleVariance[2] = {};
	bool bHasAttrRecoilAngleVariance[2] = {};
	float flRecoilMagnitude[2] = {};
	bool bHasAttrRecoilMagnitude[2] = {};
	float flRecoilMagnitudeVariance[2] = {};
	bool bHasAttrRecoilMagnitudeVariance[2] = {};

	static const CSchemaAttributeDefHandle attrRecoilSeed					( "recoil seed" );
	static const CSchemaAttributeDefHandle attrRecoilAngle					( "recoil angle" );
	static const CSchemaAttributeDefHandle attrRecoilAngleAlt				( "recoil angle alt" );
	static const CSchemaAttributeDefHandle attrRecoilAngleVariance			( "recoil angle variance" );
	static const CSchemaAttributeDefHandle attrRecoilAngleVarianceAlt		( "recoil angle variance alt" );
	static const CSchemaAttributeDefHandle attrRecoilMagnitude				( "recoil magnitude" );
	static const CSchemaAttributeDefHandle attrRecoilMagnitudeAlt			( "recoil magnitude alt" );
	static const CSchemaAttributeDefHandle attrRecoilMagnitudeVariance		( "recoil magnitude variance" );
	static const CSchemaAttributeDefHandle attrRecoilMagnitudeVarianceAlt	( "recoil magnitude variance alt" );
	static const CSchemaAttributeDefHandle attrFullAuto						( "is full auto" );

	const CUtlVector< static_attrib_t > &arrAttributes = pEconItemDefinition->GetStaticAttributes();
	for ( int j = 0; j < arrAttributes.Count(); ++ j )
	{
		attrib_definition_index_t attrIndex = arrAttributes[j].iDefIndex;
		if ( attrRecoilSeed && attrIndex == attrRecoilSeed->GetDefinitionIndex() )
		{
			Assert( !bHasAttrSeed );
			bHasAttrSeed = true;
			iSeed = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilAngle && attrIndex == attrRecoilAngle->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilAngle[0] );
			bHasAttrRecoilAngle[0] = true;
			flRecoilAngle[0] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilAngleAlt && attrIndex == attrRecoilAngleAlt->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilAngle[1] );
			bHasAttrRecoilAngle[1] = true;
			flRecoilAngle[1] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilAngleVariance && attrIndex == attrRecoilAngleVariance->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilAngleVariance[0] );
			bHasAttrRecoilAngleVariance[0] = true;
			flRecoilAngleVariance[0] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilAngleVarianceAlt && attrIndex == attrRecoilAngleVarianceAlt->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilAngleVariance[1] );
			bHasAttrRecoilAngleVariance[1] = true;
			flRecoilAngleVariance[1] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilMagnitude && attrIndex == attrRecoilMagnitude->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilMagnitude[0] );
			bHasAttrRecoilMagnitude[0] = true;
			flRecoilMagnitude[0] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilMagnitudeAlt && attrIndex == attrRecoilMagnitudeAlt->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilMagnitude[1] );
			bHasAttrRecoilMagnitude[1] = true;
			flRecoilMagnitude[1] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilMagnitudeVariance && attrIndex == attrRecoilMagnitudeVariance->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilMagnitudeVariance[0] );
			bHasAttrRecoilMagnitudeVariance[0] = true;
			flRecoilMagnitudeVariance[0] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrRecoilMagnitudeVarianceAlt && attrIndex == attrRecoilMagnitudeVarianceAlt->GetDefinitionIndex() )
		{
			Assert( !bHasAttrRecoilMagnitudeVariance[1] );
			bHasAttrRecoilMagnitudeVariance[1] = true;
			flRecoilMagnitudeVariance[1] = arrAttributes[j].m_value.asFloat;
		}
		else if ( attrFullAuto && attrIndex == attrFullAuto->GetDefinitionIndex() )
		{
			Assert( !bHasAttrFullAuto );
			bHasAttrFullAuto = true;
			bFullAuto = ( arrAttributes[j].m_value.asUint32 != 0.0f );
		}
	}

	for ( int iMode = 0; iMode < 2; ++iMode )
	{
		Assert( bHasAttrSeed && bHasAttrFullAuto &&
			bHasAttrRecoilAngle[iMode] && bHasAttrRecoilAngleVariance[iMode] &&
			bHasAttrRecoilMagnitude[iMode] && bHasAttrRecoilMagnitudeVariance[iMode] );

		recoilRandom.SetSeed( iSeed );

		float fAngle = 0.0f;
		float fMagnitude = 0.0f;

		for ( int j = 0; j < ARRAYSIZE( data->recoilTable[iMode] ); ++j )
		{
			float fAngleNew = flRecoilAngle[iMode] + recoilRandom.RandomFloat(- flRecoilAngleVariance[iMode], + flRecoilAngleVariance[iMode] );
			float fMagnitudeNew = flRecoilMagnitude[iMode] + recoilRandom.RandomFloat(- flRecoilMagnitudeVariance[iMode], + flRecoilMagnitudeVariance[iMode] );

			if ( bFullAuto && ( j > 0 ) )
			{
				fAngle = Lerp( fRecoilVariance, fAngle, fAngleNew );
				fMagnitude = Lerp( fRecoilVariance, fMagnitude, fMagnitudeNew );
			}
			else
			{
				fAngle = fAngleNew;
				fMagnitude = fMagnitudeNew;
			}

			if ( bFullAuto && ( j < iSuppressionShots ) )
			{
				float fSuppressionFactor = Lerp( (float)j / (float)iSuppressionShots, fBaseSuppressionFactor, 1.0f );
				fMagnitude *= fSuppressionFactor;
			}

			data->recoilTable[iMode][j].fAngle = fAngle;
			data->recoilTable[iMode][j].fMagnitude = fMagnitude;
		}
	}
}

void WeaponRecoilData::GenerateSpreadTable( RecoilData *data )
{
	CUniformRandomStream spreadRandom;

	if ( !data )
		return;

	const CEconItemDefinition *pEconItemDefinition = GetItemSchema()->GetItemDefinition( data->iItemDefIndex );
	Assert( pEconItemDefinition );

	if ( !pEconItemDefinition )
		return;

	// Walk the attributes to determine all things that we need
	int iSeed = 0;
	bool bHasAttrSeed = false;
	int iBullets = 0;
	bool bHasAttrBullets = false;

	static const CSchemaAttributeDefHandle attrSpreadSeed( "spread seed" );
	static const CSchemaAttributeDefHandle attrBullets( "bullets" );

	const CUtlVector< static_attrib_t > &arrAttributes = pEconItemDefinition->GetStaticAttributes();
	for ( int j = 0; j < arrAttributes.Count(); ++j )
	{
		attrib_definition_index_t attrIndex = arrAttributes[j].iDefIndex;
		if ( attrSpreadSeed && attrIndex == attrSpreadSeed->GetDefinitionIndex() )
		{
			Assert( !bHasAttrSeed );
			bHasAttrSeed = true;
			iSeed = arrAttributes[j].m_value.asUint32;
		}
		else if ( attrBullets && attrIndex == attrBullets->GetDefinitionIndex() )
		{
			Assert( !bHasAttrBullets );
			bHasAttrBullets = true;
			iBullets = arrAttributes[j].m_value.asUint32;
		}
	}

	Assert( bHasAttrSeed && bHasAttrBullets );

	// Guns that fire a single bullet still use old spread system.
	data->bRandomSpread = ( iBullets <= 1 );
	if ( data->bRandomSpread )
		return;

	Assert( iBullets <= ARRAYSIZE( data->spreadTable ) );
	if ( iBullets > ARRAYSIZE( data->spreadTable ) )
	{
		iBullets = ARRAYSIZE( data->spreadTable );
	}

	AssertMsg2( iSeed != 0, "Required attribute \"spread seed\" not specified on item %d ('%s')", data->iItemDefIndex, pEconItemDefinition->GetDefinitionName() );

	spreadRandom.SetSeed( iSeed );

	float flInvBullets = 1.0f / MAX( iBullets, 1 );

	int idxBullet = 0;
	for ( int j = 0; j < ARRAYSIZE( data->spreadTable ); ++j, ++idxBullet )
	{
		if ( idxBullet >= iBullets )
			idxBullet = 0;

		float fAngle = spreadRandom.RandomFloat( 0.0f, 2.0f * M_PI );

		// place 1 pellet in each spread circle.
		float flMinSpread = ( idxBullet + 0 ) * flInvBullets;
		float flMaxSpread = ( idxBullet + 1 ) * flInvBullets;
		float fMagnitude = spreadRandom.RandomFloat( flMinSpread, flMaxSpread );
		fMagnitude = Clamp( fMagnitude, 0.0f, 1.0f );

		data->spreadTable[j].fAngle = fAngle;
		data->spreadTable[j].fMagnitude = fMagnitude;
	}
}


void WeaponRecoilData::GetRecoilOffsets( CWeaponCSBase *pWeapon, int iMode, int iIndex, float& fAngle, float &fMagnitude )
{
	// Recoil offset tables are indexed by a weapon's definition index.
	// Look for the existing table, otherwise generate it.

	item_definition_index_t iDefIndex = pWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionIndex();

	RecoilData *wepData = NULL;
	CUtlMap< item_definition_index_t, RecoilData* >::IndexType_t iMapLocation = m_mapRecoilTables.Find( iDefIndex );
	if ( iMapLocation == m_mapRecoilTables.InvalidIndex() )
	{
		Assert( !"Generating recoil table too late" ); // failed to find recoil table, need to re-generate!
		iMapLocation = GenerateRecoilPatternForItemDefinition( iDefIndex );
	}

	wepData = m_mapRecoilTables.Element( iMapLocation );
	Assert( wepData );
	Assert( wepData->iItemDefIndex == iDefIndex );

	iIndex = iIndex % ARRAYSIZE( wepData->recoilTable[iMode] );
	fAngle = wepData->recoilTable[iMode][iIndex].fAngle;
	fMagnitude = wepData->recoilTable[iMode][iIndex].fMagnitude;
}

void WeaponRecoilData::GetSpreadOffsets( item_definition_index_t iDefIndex, int iMode, int iIndex, float& fAngle, float& fMagnitude )
{
	// Look for the existing table, otherwise generate it.
	RecoilData *wepData = NULL;
	CUtlMap< item_definition_index_t, RecoilData* >::IndexType_t iMapLocation = m_mapRecoilTables.Find( iDefIndex );
	if ( iMapLocation == m_mapRecoilTables.InvalidIndex() )
	{
		Assert( !"Generating recoil table too late" ); // failed to find recoil table, need to re-generate!
		iMapLocation = GenerateRecoilPatternForItemDefinition( iDefIndex );
	}

	wepData = m_mapRecoilTables.Element( iMapLocation );
	Assert( wepData );
	Assert( wepData->iItemDefIndex == iDefIndex );

	if ( wepData->bRandomSpread || iIndex >= ARRAYSIZE( wepData->spreadTable ) )
	{
		fAngle = RandomFloat( 0.0f, 2.0f * M_PI );
		fMagnitude = RandomFloat( 0.0f, 1.0f );
	}
	else
	{
		fAngle = wepData->spreadTable[iIndex].fAngle;
		fMagnitude = wepData->spreadTable[iIndex].fMagnitude;
	}
}

WeaponRecoilData::tRecoilTableMapIndex WeaponRecoilData::GenerateRecoilPatternForItemDefinition( item_definition_index_t idx )
{
	tRecoilTableMapIndex iMapLocation = m_mapRecoilTables.Find( idx );
	RecoilData* wepData;
	if ( iMapLocation == m_mapRecoilTables.InvalidIndex() )
	{
		wepData = new RecoilData;
	}
	else
	{
		wepData = m_mapRecoilTables.Element( iMapLocation );
	}

	wepData->iItemDefIndex = idx;
	iMapLocation = m_mapRecoilTables.InsertOrReplace( idx, wepData );
	GenerateRecoilTable( wepData );
	GenerateSpreadTable( wepData );

	return iMapLocation;
}

const WeaponRecoilData::tRecoilTableMap& WeaponRecoilData::GetRecoilTables()
{
	return m_mapRecoilTables;
}

WeaponRecoilData g_WeaponRecoilData;

void GenerateWeaponRecoilPatternForItemDefinition( item_definition_index_t idx )
{
	const WeaponRecoilData::tRecoilTableMap& recoilTableMap = g_WeaponRecoilData.GetRecoilTables();
	if ( recoilTableMap.Find( idx ) == recoilTableMap.InvalidIndex() )
		g_WeaponRecoilData.GenerateRecoilPatternForItemDefinition( idx );
}


const CCSWeaponInfo* GetWeaponInfoFromItem( const CEconItemDefinition* pItemDef )
{
	if ( !pItemDef )
		return nullptr;

#if USE_WEAPON_DATA_CACHE
	return assert_cast<const CCSWeaponInfo*>( g_pWeaponSystem->GetWeaponData( pItemDef->GetDefinitionIndex() ) );
#else
	// $$$REI TODO: Do we need to determine that this item is / is not something with weaponinfo data?
	//
	// The old code looked up the item's classname in the weapon database, but that
	// no longer exists.  We could try looking up a particular attribute on the item that should only
	// be set for valid weapons.

	// We no longer store any data in this object.  Just give a dummy result
	// This downcast is safe because Helper_GetNullWeaponInfo returns a CCSWeaponInfo internally
	return static_cast< const CCSWeaponInfo* >( Helper_GetNullWeaponInfo() );
#endif
}

const CCSWeaponInfo* GetWeaponInfoFromItem( const CEconItemView* pWeaponView )
{
	if ( !pWeaponView )
		return nullptr;

	return GetWeaponInfoFromItem( pWeaponView->GetStaticData() );
}

CSWeaponID GetWeaponIDFromItem( const CEconItemDefinition* pWeaponDef )
{
	if ( !pWeaponDef )
		return WEAPON_NONE;

	return WeaponIdFromString( pWeaponDef->GetItemClass() );
}

CSWeaponID GetWeaponIDFromItem( const CEconItemView* pWeaponView )
{
	if ( !pWeaponView )
		return WEAPON_NONE;

	return GetWeaponIDFromItem( pWeaponView->GetStaticData() );
}


const CEconItemView* GetGenericWeaponView( CSWeaponID weaponId )
{
	const CEconItemDefinition* pItemDef = WeaponIDToDefinition( weaponId );
	if ( !pItemDef )
		return nullptr;

	if ( !CSInventoryManager() )
		return nullptr;

	item_definition_index_t nItemDef = pItemDef->GetDefinitionIndex();
	return CSInventoryManager()->GetReferenceEconItem( nItemDef, 0 );
}

int Helper_GetWeaponAccuracy( const CCSWeaponInfo* pWeaponInfo, const CEconItemView * pItem )
{
	float flRecoilMag = 0;
	float flRecoilMagVar = 0;
	float flRecoilAng = 0;
	float flRecoilAngVar = 0;
	int nType = pWeaponInfo->GetWeaponType( pItem );
	if ( nType == WEAPONTYPE_SNIPER_RIFLE )
	{
		flRecoilMag = pWeaponInfo->GetRecoilMagnitude( pItem, Secondary_Mode );
		flRecoilMagVar = pWeaponInfo->GetRecoilMagnitudeVariance( pItem, Secondary_Mode );
		flRecoilAng = pWeaponInfo->GetRecoilAngle( pItem, Secondary_Mode );
		flRecoilAngVar = pWeaponInfo->GetRecoilAngleVariance( pItem, Secondary_Mode );
	}
	else
	{
		flRecoilMag = pWeaponInfo->GetRecoilMagnitude( pItem );
		flRecoilMagVar = pWeaponInfo->GetRecoilMagnitudeVariance( pItem );
		flRecoilAng = pWeaponInfo->GetRecoilAngle( pItem );
		flRecoilAngVar = pWeaponInfo->GetRecoilAngleVariance( pItem );
	}



	float flMagScaler = 1 + MIN( 1, ( flRecoilMagVar / 30.0 ) );
	float flAngScaler = 1 + MIN( 1, ( flRecoilAngVar / 180.0 ) );

	float flValue = ( flRecoilMag*flMagScaler + flRecoilAng*flAngScaler ) - 5;
	int nHandling = MAX( 1, ( 1.0f - ( flValue / 80.0f ) ) * 100 );

	return nHandling;
}


float GetEffectiveRangeRawValue( const CEconItemView *pItem )
{
	//const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoForPosition( nPos );
	float flInaccuracy = GetWeaponInaccuracyValue( pItem );
	float flSpread = GetWeaponSpreadValue( pItem );

	const float kAccurateRadius = 0.5f * 12; // 12 inch dinner plate

	float fFinalInaccuracy = flInaccuracy + flSpread;

	// Calculate effective range:              
	//                                   ----| -
	//                              -----    | ^
	//                         -----         | |
	//                    -----              | accurateRadius
	//               -----                   | |   
	//          ----|                        | |    vecUp * (accurateradius / inaccuracy)
	//     -----    | vecUp * inaccuracy     | v          * inaccuracy
	//  ------------|------------------------| -    = vecUp * accurateRadius
	//     vecDirShooting
	//  |<--- 1 --->|
	//  |<-------- effective range --------->| =  accurateradius / inaccuracy
	float flEffectiveRange = fFinalInaccuracy > 0.00001f ? ( kAccurateRadius / fFinalInaccuracy ) : 1000000.0f;

	const float kMetersPerInch = 0.0254f;
	return ( flEffectiveRange * kMetersPerInch ); // buy menu displays effective range in meters
}

float GetWeaponInaccuracyValue( const CEconItemView *pItem )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromItem( pItem );

	int nType = pWeaponInfo->GetWeaponType( pItem );
	if ( nType == WEAPONTYPE_SNIPER_RIFLE )
	{
		return pWeaponInfo->GetInaccuracyStand( pItem, Secondary_Mode );
	}
	else
	{
		return pWeaponInfo->GetInaccuracyStand( pItem );
	}
}

float GetWeaponSpreadValue( const CEconItemView *pItem )
{
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromItem( pItem );

	int nType = pWeaponInfo->GetWeaponType( pItem );
	if ( nType == WEAPONTYPE_SNIPER_RIFLE )
	{
		return pWeaponInfo->GetSpread( pItem, Secondary_Mode );
	}
	else
	{
		return pWeaponInfo->GetSpread( pItem );
	}
}

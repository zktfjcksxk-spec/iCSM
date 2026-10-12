//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon data file parsing, shared by game & client dlls.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <keyvalues.h>
#include <tier0/mem.h>
#include "filesystem.h"
#include "utldict.h"
#include "ammodef.h"
#include "util_shared.h"
#include "weapon_parse.h"
#include "weapon_cache.h"
#include "econ_item_view.h"
#include "rumble_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The sound categories found in the weapon classname.txt files
// This needs to match the WeaponSound_t enum in weapon_parse.h
#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL)
const char *pWeaponSoundCategories[ NUM_SHOOT_SOUND_TYPES ] = 
{
	"empty",
	"single_shot",
	"single_shot_accurate",
	"single_shot_npc",
	"double_shot",
	"double_shot_npc",
	"burst",
	"reload",
	"reload_npc",
	"melee_miss",
	"melee_hit",
	"melee_hit_world",
	"special1",
	"special2",
	"special3",
	"taunt",
	"nearlyempty",
	"fastreload"
};
#else
extern const char *pWeaponSoundCategories[ NUM_SHOOT_SOUND_TYPES ];
#endif

int GetWeaponSoundFromString( const char *pszString )
{
	for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
	{
		if ( !Q_stricmp(pszString,pWeaponSoundCategories[i]) )
			return (WeaponSound_t)i;
	}
	return -1;
}


//
// TODO: Move to a more appropriate location
//
KeyValues* ReadEncryptedKVFile( IFileSystem *pFilesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile )
{
	Assert( strchr( szFilenameWithoutExtension, '.' ) == NULL );
	char szFullName[512];

	const char *pSearchPath = "GAME";

	// Open the weapon data file, and abort if we can't
	KeyValues *pKV = new KeyValues( "WeaponDatafile" );
	pKV->UsesEscapeSequences( true );

	Q_snprintf(szFullName,sizeof(szFullName), "%s.txt", szFilenameWithoutExtension);

	if ( bForceReadEncryptedFile || !pKV->LoadFromFile( pFilesystem, szFullName, pSearchPath ) ) // try to load the normal .txt file first
	{
		if ( pICEKey )
		{
			Q_snprintf(szFullName,sizeof(szFullName), "%s.ctx", szFilenameWithoutExtension); // fall back to the .ctx file

			FileHandle_t f = pFilesystem->Open( szFullName, "rb", pSearchPath );

			if (!f)
			{
				pKV->deleteThis();
				return NULL;
			}
			// load file into a null-terminated buffer
			int fileSize = pFilesystem->Size(f);
			char *buffer = (char*)MemAllocScratch(fileSize + 1);
		
			Assert(buffer);
		
			pFilesystem->Read(buffer, fileSize, f); // read into local buffer
			buffer[fileSize] = 0; // null terminate file as EOF
			pFilesystem->Close( f );	// close file after reading

			UTIL_DecodeICE( (unsigned char*)buffer, fileSize, pICEKey );

			bool retOK = pKV->LoadFromBuffer( szFullName, buffer, pFilesystem );

			MemFreeScratch();

			if ( !retOK )
			{
				pKV->deleteThis();
				return NULL;
			}
		}
		else
		{
			pKV->deleteThis();
			return NULL;
		}
	}

	return pKV;
}

#if USE_WEAPON_DATA_CACHE
OldWeaponData::FileWeaponInfo_t::FileWeaponInfo_t()
#else
FileWeaponInfo_t::FileWeaponInfo_t()
#endif
{
}

//-----------------------------------------------------------------------------
// FileWeaponInfo_t implementation.
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
extern ConVar hud_fastswitch;
#endif

#if USE_WEAPON_DATA_CACHE
namespace OldWeaponData {
#endif
int /*gear_slot_t*/ FileWeaponInfo_t::GetGearSlot( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		return pWepView->GetStaticData()->GetWeaponSlot();
	}

	Assert( false );
	return GEAR_SLOT_INVALID;
}

int FileWeaponInfo_t::GetGearSlotPosition( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		return pWepView->GetStaticData()->GetWeaponSlotPosition();
	}

	Assert( false );
	return -1;
}

int FileWeaponInfo_t::GetLoadoutPosition( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		if ( iTeam < 0 )
			return pWepView->GetStaticData()->GetDefaultLoadoutSlot();
		else
			return pWepView->GetStaticData()->GetLoadoutSlot( iTeam );
	}

	Assert( false );
	return LOADOUT_POSITION_INVALID;
}

// see rumble_shared.h for the details on the return type
int FileWeaponInfo_t::GetRumbleEffect( const CEconItemView* pWepView ) const
{
	static const CSchemaAttributeDefHandle attr( "rumble effect" );
	uint32 unAttrVal;
	if ( pWepView && pWepView->IsValid() && attr && pWepView->FindAttribute( attr, &unAttrVal ) )
	{
		return ( int32 )unAttrVal;
	}

	Assert( pWepView && pWepView->IsValid() );
	return RUMBLE_INVALID;
}

const char* FileWeaponInfo_t::GetWorldModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char* pchItemModel = pWepView->GetItemDefinition()->GetWorldDisplayModel();
		if ( pchItemModel )
			return pchItemModel;
	}

	Assert( false );
	return "";
}

const char* FileWeaponInfo_t::GetViewModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char* pchItemModel = pWepView->GetItemDefinition()->GetBasePlayerDisplayModel();
		if ( pchItemModel )
			return pchItemModel;
	}

	Assert( false );
	return "";
}

const char* FileWeaponInfo_t::GetWorldDroppedModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldDroppedModel = pWepView->GetItemDefinition()->GetWorldDroppedModel();
		if ( pchWorldDroppedModel )
			return pchWorldDroppedModel;
	}

	Assert( false );
	return "";
}

const char* FileWeaponInfo_t::GetShootSound( const CEconItemView* pWepView, int iSoundIndex ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pszSound = pWepView->GetStaticData()->GetWeaponReplacementSound( ( WeaponSound_t )iSoundIndex );
		if ( pszSound )
			return pszSound;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}

const char* FileWeaponInfo_t::GetPrimaryAmmo( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		if ( const char *pszString = pWepView->GetStaticData()->GetPrimaryAmmo() )
			return pszString;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}


const char* FileWeaponInfo_t::GetSecondaryAmmo( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		if ( const char *pszString = pWepView->GetStaticData()->GetSecondaryAmmo() )
			return pszString;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}


const char* FileWeaponInfo_t::GetPrintName( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		if ( const char *pszString = pWepView->GetStaticData()->GetItemBaseName() )
			return pszString;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}

const char* FileWeaponInfo_t::GetClassName( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		if ( const char *pszString = pWepView->GetStaticData()->GetItemClass() )
			return pszString;
	}

	Assert( pWepView && pWepView->IsValid() );
	return "";
}

int FileWeaponInfo_t::GetPrimaryAmmoType( const CEconItemView* pWepView ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		if ( const char *pszString = pWepView->GetStaticData()->GetPrimaryAmmo() )
			return GetAmmoDef()->Index( pszString );
	}

	Assert( pWepView && pWepView->IsValid() );
	return 0;
}


bool FileWeaponInfo_t::HasFlag( const CEconItemView* pWepView, int iFlag ) const
{
	static CSchemaAttributeDefHandle attrSelectOnEmpty		("itemflag select on empty");
	static CSchemaAttributeDefHandle attrNoAutoReload		("itemflag no auto reload");
	static CSchemaAttributeDefHandle attrNoAutoSwitchEmpty	("itemflag no auto switch empty");
	static CSchemaAttributeDefHandle attrLimitInWorld		("itemflag limit in world");
	static CSchemaAttributeDefHandle attrExhaustible		("itemflag exhaustible");
	static CSchemaAttributeDefHandle attrDoHitLocationDmg	("itemflag do hit location dmg");
	static CSchemaAttributeDefHandle attrNoAmmoPickups		("itemflag no ammo pickups");
	static CSchemaAttributeDefHandle attrNoItemPickup		("itemflag no item pickup");

	// default limit in world to true, everything else false
	bool defaultValue = false;
	const CSchemaAttributeDefHandle* pAttr = nullptr;
	switch(iFlag)
	{
	case ITEM_FLAG_SELECTONEMPTY:		pAttr = &attrSelectOnEmpty;								break;
	case ITEM_FLAG_NOAUTORELOAD:		pAttr = &attrNoAutoReload;								break;
	case ITEM_FLAG_NOAUTOSWITCHEMPTY:	pAttr = &attrNoAutoSwitchEmpty;							break;
	case ITEM_FLAG_LIMITINWORLD:		pAttr = &attrLimitInWorld;		defaultValue = true;	break;
	case ITEM_FLAG_EXHAUSTIBLE:			pAttr = &attrExhaustible;								break;
	case ITEM_FLAG_DOHITLOCATIONDMG:	pAttr = &attrDoHitLocationDmg;							break;
	case ITEM_FLAG_NOAMMOPICKUPS:		pAttr = &attrNoAmmoPickups;								break;
	case ITEM_FLAG_NOITEMPICKUP:		pAttr = &attrNoItemPickup;								break;
	}
	AssertMsg1( pAttr, "Invalid flag 0x%08x passed to Weapon::HasFlag", iFlag  );
	AssertMsg1( !pAttr || *pAttr, "Undefined attribute %s", pAttr->GetName() );

	uint32 unAttrValue;
	if ( pWepView && pWepView->IsValid() && pAttr && *pAttr && pWepView->FindAttribute( *pAttr, &unAttrValue ) )
	{
		return unAttrValue != 0;
	}

	return defaultValue;
}

bool FileWeaponInfo_t::AllowsFlipping(const CEconItemView* pWepView) const
{
	static CSchemaAttributeDefHandle attr( "allow hand flipping" );

	uint32 unAttrValue;
	if ( pWepView && pWepView->IsValid() && attr && pWepView->FindAttribute( attr, &unAttrValue ) )
	{
		return unAttrValue != 0;
	}

	Assert( false );
	return false;
}

bool FileWeaponInfo_t::ModelIsRightHanded( const CEconItemView* pWepView ) const
{
	static CSchemaAttributeDefHandle attr( "model right handed" );

	uint32 unAttrValue;
	if ( pWepView && pWepView->IsValid() && attr && pWepView->FindAttribute( attr, &unAttrValue ) )
	{
		return unAttrValue != 0;
	}

	Assert( false );
	return false;
}

bool FileWeaponInfo_t::IsMeleeWeapon( const CEconItemView* pWepView )const
{
	static CSchemaAttributeDefHandle attr( "is melee weapon" );

	uint32 unAttrValue;
	if ( pWepView && pWepView->IsValid() && attr && pWepView->FindAttribute( attr, &unAttrValue ) )
	{
		return unAttrValue != 0;
	}

	Assert( false );
	return false;
}

int FileWeaponInfo_t::GetWeight( const CEconItemView* pWepView ) const
{
	static CSchemaAttributeDefHandle attr( "weapon weight" );

	uint32 unAttrValue;
	if ( pWepView && pWepView->IsValid() && attr && pWepView->FindAttribute( attr, &unAttrValue ) )
	{
		return (int32)unAttrValue;
	}

	Assert( false );
	return false;
}

bool FileWeaponInfo_t::AllowsAutoSwitchFrom( const CEconItemView* pWepView ) const
{
	// Always true in CSGO
	return true;
}

bool FileWeaponInfo_t::AllowsAutoSwitchTo( const CEconItemView* pWepView ) const
{
	// Always true in CSGO
	return true;
}

#if USE_WEAPON_DATA_CACHE
} // namespace OldWeaponData
#endif


bool CWeaponData::Fill()
{
	if ( !BaseClass::Fill() )
		return false;

	const GameItemDefinition_t* pItemDef = GetItem();

	m_nPrimaryClipSize			= ITEM_ATTR_INT( "primary clip size" );
	m_nSecondaryClipSize		= ITEM_ATTR_INT( "secondary clip size" );
	m_nDefaultPrimaryClipSize	= ITEM_ATTR_INT( "primary default clip size" );
	m_nDefaultSecondaryClipSize	= ITEM_ATTR_INT( "secondary default clip size" );

	m_nPrimaryReserveAmmoMax	= ITEM_ATTR_INT( "primary reserve ammo max" );
	m_nSecondaryReserveAmmoMax	= ITEM_ATTR_INT( "secondary reserve ammo max" );

	m_szWorldModel				= WithDefault( pItemDef->GetWorldDisplayModel(), "" );
	m_szViewModel				= WithDefault( pItemDef->GetBasePlayerDisplayModel(), "" );
	m_szWorldDroppedModel		= WithDefault( pItemDef->GetWorldDroppedModel(), "" );

	for ( int iSound = 0; iSound < NUM_SHOOT_SOUND_TYPES; ++iSound )
	{
		m_aszShootSound[iSound]	= WithDefault( pItemDef->GetWeaponReplacementSound( (WeaponSound_t)iSound ), "" );
	}

	m_szPrimaryAmmo				= WithDefault( pItemDef->GetPrimaryAmmo(), "" );
	m_szSecondaryAmmo			= WithDefault( pItemDef->GetSecondaryAmmo(), "" );
	m_szPrintName				= WithDefault( pItemDef->GetItemBaseName(), "" );
	m_szClassName				= WithDefault( pItemDef->GetItemClass(), "" );

	m_bAllowsFlipping			= ITEM_ATTR_BOOL( "allow hand flipping" );
	m_bModelIsRightHanded		= ITEM_ATTR_BOOL( "model right handed" );
	m_bIsMeleeWeapon			= ITEM_ATTR_BOOL( "is melee weapon" );
	m_bAllowsAutoSwitchFrom		= true;		// always true in CSGO
	m_bAllowsAutoSwitchTo		= true;		// always true in CSGO

	// flags
	m_ItemFlags = 0;
	if ( ITEM_ATTR_BOOL( "itemflag select on empty" ) )
		m_ItemFlags |= ITEM_FLAG_SELECTONEMPTY;
	if ( ITEM_ATTR_BOOL( "itemflag no auto reload" ) )
		m_ItemFlags |= ITEM_FLAG_NOAUTORELOAD;
	if ( ITEM_ATTR_BOOL( "itemflag no auto switch empty" ) )
		m_ItemFlags |= ITEM_FLAG_NOAUTOSWITCHEMPTY;
	if ( ITEM_ATTR_BOOL_DEF( "itemflag limit in world", true ) )
		m_ItemFlags |= ITEM_FLAG_LIMITINWORLD;
	if ( ITEM_ATTR_BOOL( "itemflag exhaustible" ) )
		m_ItemFlags |= ITEM_FLAG_EXHAUSTIBLE;
	if ( ITEM_ATTR_BOOL( "itemflag do hit location dmg" ) )
		m_ItemFlags |= ITEM_FLAG_DOHITLOCATIONDMG;
	if ( ITEM_ATTR_BOOL( "itemflag no ammo pickups" ) )
		m_ItemFlags |= ITEM_FLAG_NOAMMOPICKUPS;
	if ( ITEM_ATTR_BOOL( "itemflag no item pickup" ) )
		m_ItemFlags |= ITEM_FLAG_NOITEMPICKUP;

	m_nWeight					= ITEM_ATTR_INT( "weapon weight" );

	m_nPrimaryAmmoType			= 0;
	if ( const char* pszPrimaryAmmoType = pItemDef->GetPrimaryAmmo() )
		m_nPrimaryAmmoType		= GetAmmoDef()->Index( pszPrimaryAmmoType );

	m_GearSlot					= pItemDef->GetWeaponSlot();
	m_GearSlotPosition			= pItemDef->GetWeaponSlotPosition();
	m_DefaultLoadoutPosition	= pItemDef->GetDefaultLoadoutSlot();
	
	for ( int iTeam = 0; iTeam < TEAM_MAXCOUNT; ++iTeam )
		m_TeamLoadoutPosition[iTeam] = ( loadout_positions_t )pItemDef->GetLoadoutSlot( iTeam );

	m_nRumbleEffect				= ITEM_ATTR_INT_DEF( "rumble effect", -1 );

	return true;
}

int CWeaponData::GetLoadoutPosition( int nTeam ) const
{
	COMPILE_TIME_ASSERT( kTeamCount == TEAM_MAXCOUNT );

	if ( nTeam < 0 )
		return m_DefaultLoadoutPosition;

	if ( nTeam < TEAM_MAXCOUNT )
		return m_TeamLoadoutPosition[nTeam];

	return LOADOUT_POSITION_INVALID;
}



//-----------------------------------------------------------------------------
// CItemDataCache
//-----------------------------------------------------------------------------

CItemDataCache::CItemDataCache( const GameItemDefinition_t* pItemDef )
	// Our linux compiler doesn't allow delegating constructors yet.
	// : CItemDataCache( pItemDef ? pItemDef->GetDefinitionIndex() : INVALID_ITEM_DEF_INDEX )
	: m_pName( nullptr )
	, m_nItemDefIndex( pItemDef ? pItemDef->GetDefinitionIndex() : INVALID_ITEM_DEF_INDEX )
	, m_bValid( false )
	, m_unSchemaGeneration( ( uint32 )( -1 ) ) // fill on 1st Update()
	, m_pItemDef( nullptr )
{}

const char* CItemDataCache::GetAttrString( const CEconItemAttributeDefinition* pAttr, const char* defaultValue ) const
{
	Assert( pAttr );

	const char* result;
	if ( !pAttr || !::FindAttribute_UnsafeBitwiseCast<CAttribute_String>( m_pItemDef, pAttr, &result ) )
		return defaultValue;
	return result;
}

int CItemDataCache::GetAttrInt( const CEconItemAttributeDefinition* pAttr, int defaultValue ) const
{
	Assert( pAttr );

	uint32 result;
	if ( !pAttr || !::FindAttribute( m_pItemDef, pAttr, &result ) )
		return defaultValue;
	return result;
}

float CItemDataCache::GetAttrFloat( const CEconItemAttributeDefinition* pAttr, float scale, float defaultValue ) const
{
	Assert( pAttr );

	float result;
	if ( !pAttr || !::FindAttribute( m_pItemDef, pAttr, &result ) )
		return defaultValue;
	return result * scale;
}

bool CItemDataCache::Update()
{
	uint32 unSchemaGeneration = GEconItemSchema().GetResetCount();
	if ( m_unSchemaGeneration != unSchemaGeneration )
	{
		m_unSchemaGeneration = unSchemaGeneration;
		m_pItemDef = ( GameItemDefinition_t* )GEconItemSchema().GetItemDefinition( m_nItemDefIndex, false );
		if ( m_pItemDef )
			m_bValid = Fill();
	}

	return m_bValid;
}

bool CItemDataCache::Fill()
{
	m_pName = GetItem()->GetDefinitionName();
	return true;
}

bool CItemDataCache::VerifyItem( const CEconItemView* pItemView ) const
{
	if ( !pItemView ) return false;
	if ( !pItemView->IsValid() ) return false;
	return ( pItemView->GetStaticData()->GetDefinitionIndex() == m_nItemDefIndex );
}


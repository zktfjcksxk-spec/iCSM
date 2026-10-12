//========= Copyright © 2017, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon data file parsing, shared by game & client dlls.
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_CACHE_H
#define WEAPON_CACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_parse.h"
#include "networkvar.h" // DECLARE_CLASS stuff
#include "game_item_schema.h"


//-----------------------------------------------------------------------------
// Purpose: Helper class for accessing weapon stats in item data.
// Games can subclass this to provide additional functions.
//-----------------------------------------------------------------------------
#if USE_WEAPON_DATA_CACHE
typedef class CWeaponData FileWeaponInfo_t;
namespace OldWeaponData {
#endif

	class FileWeaponInfo_t
	{
	public:

		FileWeaponInfo_t();
		virtual ~FileWeaponInfo_t() {}

	public:
		// This is kind of a hack, these are all *always* overridden by CCSWeaponInfo
		// because that's where the attribute macro machinery lives, even though the
		// data is actually on this object and some code calls these functions.
		virtual int		GetPrimaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const { Assert( false ); return 0; }
		virtual int		GetSecondaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const { Assert( false ); return 0; }
		virtual int		GetDefaultPrimaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const { Assert( false ); return 0; }
		virtual int		GetDefaultSecondaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const { Assert( false ); return 0; }
		virtual int		GetPrimaryReserveAmmoMax( const CEconItemView* pWepView, int nAlt = 0 ) const { Assert( false ); return 0; }
		virtual int		GetSecondaryReserveAmmoMax( const CEconItemView* pWepView, int nAlt = 0 ) const { Assert( false ); return 0; }

		const char* GetWorldModel( const CEconItemView* pWepView, int iTeam = 0 ) const;
		const char* GetViewModel( const CEconItemView* pWepView, int iTeam = 0 ) const;
		const char* GetWorldDroppedModel( const CEconItemView* pWepView, int iTeam = 0 ) const;
		const char* GetShootSound( const CEconItemView* pWepView, int iSoundType ) const;
		const char* GetPrimaryAmmo( const CEconItemView* pWepView ) const;
		const char* GetSecondaryAmmo( const CEconItemView* pWepView ) const;
		const char* GetPrintName( const CEconItemView* pWepView ) const;
		const char* GetClassName( const CEconItemView* pWepView ) const;

		bool AllowsFlipping( const CEconItemView* pWepView ) const;
		bool ModelIsRightHanded( const CEconItemView* pWepView ) const;
		bool HasFlag( const CEconItemView* pWepView, int nFlag ) const;
		bool IsMeleeWeapon( const CEconItemView* pWepView ) const;
		int GetWeight( const CEconItemView* pWepView ) const;
		bool AllowsAutoSwitchTo( const CEconItemView* pWepView ) const;
		bool AllowsAutoSwitchFrom( const CEconItemView* pWepView ) const;

		int GetPrimaryAmmoType( const CEconItemView* pWepView ) const;

		int /*gear_slot_t*/ GetGearSlot( const CEconItemView* pWepView ) const;
		int /*gear_slotposition_t*/ GetGearSlotPosition( const CEconItemView* pWepView ) const;
		int /*loadout_positions_t*/ GetLoadoutPosition( const CEconItemView* pWepView, int iTeam = -1 ) const;
		int GetRumbleEffect( const CEconItemView* pWepView ) const;
	};

#if USE_WEAPON_DATA_CACHE
} // namespace OldWeaponData
#endif

#if USE_WEAPON_DATA_CACHE && TEST_WEAPON_DATA_CACHE
extern const OldWeaponData::FileWeaponInfo_t& GetOldWeaponInfo( const class CWeaponData* );
#endif


#if TEST_WEAPON_DATA_CACHE
inline bool WeaponAttr_IsSame( const char* s1, const char* s2 )
{
	if ( s1 == nullptr ) return s2 == nullptr;
	if ( s2 == nullptr ) return false;

	return !V_strcmp( s1, s2 );
}
template <typename T>
inline bool WeaponAttr_IsSame( T s1, T s2 )
{
	return s1 == s2;
}
#else
#define WeaponAttr_IsSame(s1,s2) (true)
#endif

#define WEAPON_ACCESSOR( MemberType, FunctionName, mMemberName )									\
	MemberType FunctionName( const CEconItemView* pWepView ) const {								\
		Assert( VerifyItem(pWepView) );																\
		Assert(WeaponAttr_IsSame(GetOldWeaponInfo(this).FunctionName(pWepView), mMemberName));		\
		return mMemberName;																			\
	}

#define WEAPON_ACCESSOR_ARRAY( MemberType, FunctionName, mMemberName )											\
	MemberType FunctionName( const CEconItemView* pWepView, int nElt = 0 ) const {								\
		Assert( VerifyItem(pWepView) );																			\
		Assert(WeaponAttr_IsSame(GetOldWeaponInfo(this).FunctionName(pWepView, nElt), (mMemberName)[nElt]));	\
		return (mMemberName)[nElt];																				\
	}

//
// TODO: move to new file
//
class CCStrike15ItemDefinition;
typedef CCStrike15ItemDefinition GameItemDefinition_t;
typedef uint16 item_definition_index_t;
class CEconItemAttributeDefinition;
class CEconItemView;
class CItemDataCache
{
	DECLARE_CLASS_NOBASE( CItemDataCache );

public:
	explicit CItemDataCache( item_definition_index_t nItemDefIndex )
		: m_pName( nullptr )
		, m_nItemDefIndex( nItemDefIndex )
		, m_bValid( false )
		, m_unSchemaGeneration( ( uint32 )( -1 ) ) // fill on 1st Update()
		, m_pItemDef( nullptr )
	{}

	explicit CItemDataCache( const GameItemDefinition_t* pItemDef );

	bool Update();
	bool VerifyItem( const CEconItemView* ) const;

protected:
	const GameItemDefinition_t* GetItem() { return m_pItemDef; }

	int GetAttrInt( const CEconItemAttributeDefinition* pAttr, int defaultValue = 0 ) const;
	float GetAttrFloat( const CEconItemAttributeDefinition* pAttr, float scale = 1.0f, float defaultValue = 0.0f ) const;
	const char* GetAttrString( const CEconItemAttributeDefinition* pAttr, const char* defaultValue = "" ) const;

	bool GetAttrBool( const CEconItemAttributeDefinition* pAttr, bool defaultValue = false ) const
	{
		return GetAttrInt( pAttr, defaultValue ) != 0;
	}

	// Return false for items of the wrong type
	virtual bool Fill();

private:
	const char* m_pName;			// For debugging
	const item_definition_index_t m_nItemDefIndex;
	bool m_bValid;
	uint32 m_unSchemaGeneration;
	const GameItemDefinition_t* m_pItemDef;
};

//
// Weapon data
//

class CWeaponData : public CItemDataCache
{
public:
	DECLARE_CLASS( CWeaponData, CItemDataCache );

	static const int kTeamCount = 4; // must = TEAM_MAXCOUNT

	explicit CWeaponData( item_definition_index_t itemDefIndex ) : CItemDataCache( itemDefIndex ) {}

	// Data from cache
	int m_nPrimaryClipSize;
	int m_nSecondaryClipSize;
	int m_nDefaultPrimaryClipSize;
	int m_nDefaultSecondaryClipSize;
	int m_nPrimaryReserveAmmoMax;
	int m_nSecondaryReserveAmmoMax;

	const char* m_szWorldModel;
	const char* m_szViewModel;
	const char* m_szWorldDroppedModel;
	const char* m_aszShootSound[NUM_SHOOT_SOUND_TYPES];

	const char* m_szPrimaryAmmo;
	const char* m_szSecondaryAmmo;
	const char* m_szPrintName;
	const char* m_szClassName;

	bool m_bAllowsFlipping;
	bool m_bModelIsRightHanded;
	bool m_bIsMeleeWeapon;
	bool m_bAllowsAutoSwitchFrom;
	bool m_bAllowsAutoSwitchTo;

	uint32 m_ItemFlags;			// ITEM_FLAG_*

	int m_nWeight;
	int m_nPrimaryAmmoType;
	int /*gear_slot_t*/ m_GearSlot;
	int /*gear_slotposition_t*/ m_GearSlotPosition;
	int /*loadout_positions_t*/ m_DefaultLoadoutPosition;
	int /*loadout_positions_t*/ m_TeamLoadoutPosition[kTeamCount];
	int m_nRumbleEffect;

	// Helper accessors
	bool HasFlag( uint32 nFlag ) const { return ( m_ItemFlags & nFlag ) == nFlag; }
	int /*loadout_positions_t*/ GetLoadoutPosition( int nTeam = -1 ) const;

public: // Temporary accessors for migration
	// NOTE: New code should probably not use any of the code in this section.
	// Instead, just access the member variables above directly, or use the helpers declared there that
	// do not take a CEconItemView.
	//
	// Client code can only get access to a const CWeaponData, and the simplest method is to treat that
	// as a constant view of the member variables it holds.

	bool HasFlag( const CEconItemView* pItemView, uint32 nFlag ) const { VerifyItem( pItemView ); return HasFlag( nFlag ); }
	int /*loadout_positions_t*/ GetLoadoutPosition( const CEconItemView* pItemView, int nTeam = -1 ) const { VerifyItem( pItemView ); return GetLoadoutPosition( nTeam ); }

	WEAPON_ACCESSOR( int, GetPrimaryClipSize, m_nPrimaryClipSize );
	WEAPON_ACCESSOR( int, GetSecondaryClipSize, m_nSecondaryClipSize );
	WEAPON_ACCESSOR( int, GetDefaultPrimaryClipSize, m_nDefaultPrimaryClipSize );
	WEAPON_ACCESSOR( int, GetDefaultSecondaryClipSize, m_nDefaultSecondaryClipSize );
	WEAPON_ACCESSOR( int, GetPrimaryReserveAmmoMax, m_nPrimaryReserveAmmoMax );
	WEAPON_ACCESSOR( int, GetSecondaryReserveAmmoMax, m_nSecondaryReserveAmmoMax );
	WEAPON_ACCESSOR( const char*, GetWorldModel, m_szWorldModel );
	WEAPON_ACCESSOR( const char*, GetViewModel, m_szViewModel );
	WEAPON_ACCESSOR( const char*, GetWorldDroppedModel, m_szWorldDroppedModel );
	WEAPON_ACCESSOR_ARRAY( const char*, GetShootSound, m_aszShootSound );
	WEAPON_ACCESSOR( const char*, GetPrimaryAmmo, m_szPrimaryAmmo );
	WEAPON_ACCESSOR( const char*, GetSecondaryAmmo, m_szSecondaryAmmo );
	WEAPON_ACCESSOR( const char*, GetPrintName, m_szPrintName );
	WEAPON_ACCESSOR( const char*, GetClassName, m_szClassName );
	WEAPON_ACCESSOR( bool, AllowsFlipping, m_bAllowsFlipping );
	WEAPON_ACCESSOR( bool, ModelIsRightHanded, m_bModelIsRightHanded );
	WEAPON_ACCESSOR( bool, IsMeleeWeapon, m_bIsMeleeWeapon );
	WEAPON_ACCESSOR( int, GetWeight, m_nWeight );
	WEAPON_ACCESSOR( bool, AllowsAutoSwitchTo, m_bAllowsAutoSwitchTo );
	WEAPON_ACCESSOR( bool, AllowsAutoSwitchFrom, m_bAllowsAutoSwitchFrom );
	WEAPON_ACCESSOR( int, GetPrimaryAmmoType, m_nPrimaryAmmoType );
	WEAPON_ACCESSOR( int, GetGearSlot, m_GearSlot );
	WEAPON_ACCESSOR( int, GetGearSlotPosition, m_GearSlotPosition );
	WEAPON_ACCESSOR( int, GetRumbleEffect, m_nRumbleEffect );

protected:
	virtual bool Fill() OVERRIDE;
};

// 
// Read a possibly-encrypted KeyValues file in. 
// If pICEKey is NULL, then it appends .txt to the filename and loads it as an unencrypted file.
// If pICEKey is non-NULL, then it appends .ctx to the filename and loads it as an encrypted file.
//
// (This should be moved into a more appropriate place).
//
KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile = false );

// Each game provides an implementation of IWeaponSystem and stores it in the global variable g_pWeaponSystem.
class IWeaponSystem
{
public:
	// Called during level init.  Not currently needed, all existing weapons use the regular PRECACHE_REGISTER system.
	virtual void PrecacheWeaponClasses() = 0;

	// Called when game mode/type changes to reload any data that might be affected by convars or gamemode
	virtual void RefreshForGame() = 0;

	// Get the weapon stats for a particular item
	virtual const CWeaponData* GetWeaponData( uint32 itemDefIndex ) = 0;

protected:
	~IWeaponSystem() {} // don't allow delete from this object
};

// Must be defined by game-specific weapon code
extern IWeaponSystem* g_pWeaponSystem;

//
// TODO: move to new file
//

// Convert statements into an expression using a C++11 lambda
// We use this to allow embedding of statically allocated variables into an expression context
#define STATEMENT_EXPRESSION( exprType, ... ) ( ([&]() -> exprType { __VA_ARGS__ })() )

#define ITEM_ATTRIBUTE( name )					STATEMENT_EXPRESSION( const CEconItemAttributeDefinition*, static const CSchemaAttributeDefHandle sAttrHandle((name)); return sAttrHandle; )

#define ITEM_ATTR_INT_DEF( name, def )			( GetAttrInt( ITEM_ATTRIBUTE(name), (def) ) )
#define ITEM_ATTR_FLOAT_DEF( name, scale, def )	( GetAttrFloat( ITEM_ATTRIBUTE(name), (scale), (def) ) )
#define ITEM_ATTR_STRING_DEF( name, def )		( GetAttrString( ITEM_ATTRIBUTE(name), (def) ) )
#define ITEM_ATTR_BOOL_DEF( name, def )			( GetAttrBool( ITEM_ATTRIBUTE(name), (def) ) )

#define ITEM_ATTR_INT( name )					ITEM_ATTR_INT_DEF( name, 0 )
#define ITEM_ATTR_FLOAT_SCALE( name, scale )	ITEM_ATTR_FLOAT_DEF( name, scale, 0.0f )
#define ITEM_ATTR_FLOAT( name )					ITEM_ATTR_FLOAT_SCALE( name, 1.0f )
#define ITEM_ATTR_STRING( name )				ITEM_ATTR_STRING_DEF( name, nullptr )
#define ITEM_ATTR_BOOL( name )					ITEM_ATTR_BOOL_DEF( name, false )

template <typename T>
T WithDefault( T foundValue, T defaultValue, T nullValue = 0 )
{
	return ( foundValue == nullValue ) ? defaultValue : foundValue;
}

#endif // WEAPON_CACHE_H

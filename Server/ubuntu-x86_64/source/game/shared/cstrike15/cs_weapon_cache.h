//========= Copyright © 2017, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CS_WEAPON_CACHE_H
#define CS_WEAPON_CACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "cs_weapon_parse.h"
#include "weapon_cache.h"
#include "econ_item_constants.h"

#ifdef CLIENT_DLL
#define CWeaponCSBase C_WeaponCSBase
#endif
class CWeaponCSBase;
struct WeaponPaintableMaterial_t;


class WeaponRecoilData
{
private:
	struct RecoilOffset
	{
		float	fAngle;
		float	fMagnitude;
	};

	struct RecoilData
	{
		item_definition_index_t		iItemDefIndex;
		RecoilOffset				recoilTable[2][64];
		RecoilOffset				spreadTable[64];
		bool						bRandomSpread;			// single bullet guns still use spread generated randomly with each bullet
	};

public:
	typedef CUtlMap< item_definition_index_t, RecoilData* > tRecoilTableMap;
	typedef tRecoilTableMap::IndexType_t tRecoilTableMapIndex;

	WeaponRecoilData();
	~WeaponRecoilData();

	void GetRecoilOffsets( CWeaponCSBase *pWeapon, int iMode, int iIndex, float& fAngle, float &fMagnitude );
	void GetSpreadOffsets( item_definition_index_t iWeaponDefIndex, int iMode, int iIndex, float& fAngle, float &fMagnitude );

	tRecoilTableMapIndex GenerateRecoilPatternForItemDefinition( item_definition_index_t idx );
	const tRecoilTableMap& GetRecoilTables();

private:
	tRecoilTableMap m_mapRecoilTables;
	void GenerateRecoilTable( RecoilData *data );
	void GenerateSpreadTable( RecoilData *data );
};

#if USE_WEAPON_DATA_CACHE
namespace OldWeaponData {
#endif

	//--------------------------------------------------------------------------------------------------------
	class CCSWeaponInfo : public FileWeaponInfo_t
	{
	public:
		DECLARE_CLASS_GAMEROOT( CCSWeaponInfo, FileWeaponInfo_t );

		CCSWeaponInfo();

		const char* GetZoomInSound( const CEconItemView* pWepView, int iTeam = 0 ) const;
		const char* GetZoomOutSound( const CEconItemView* pWepView, int iTeam = 0 ) const;
		const Vector& GetSmokeColor( const CEconItemView* pWepView ) const;
		CSWeaponID GetWeaponID( const CEconItemView* pWepView ) const;

		int		GetWeaponPrice( const CEconItemView* pWepView, int nAlt = 0 ) const;
		bool	IsFullAuto( const CEconItemView* pWepView, int nAlt = 0 ) const;
		bool	HasSilencer( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetBullets( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetCycleTime( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetHeatPerShot( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRecoveryTimeCrouch( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRecoveryTimeStand( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRecoveryTimeCrouchFinal( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRecoveryTimeStandFinal( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetRecoveryTransitionStartBullet( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetRecoveryTransitionEndBullet( const CEconItemView* pWepView, int nAlt = 0 ) const;

		int		GetRecoilSeed( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetFlinchVelocityModifierLarge( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetFlinchVelocityModifierSmall( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetTimeToIdleAfterFire( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetIdleInterval( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRange( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRangeModifier( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetDamage( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetPenetration( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetCrosshairDeltaDistance( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetCrosshairMinDistance( const CEconItemView* pWepView, int nAlt = 0 ) const;
		//float	GetInaccuracyAltSwitch			( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyPitchShift( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyAltSoundThreshold( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetMaxSpeed( const CEconItemView* pWepView, int nAlt = 0 ) const;

		float	GetSpread( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyCrouch( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyStand( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float   GetInaccuracyJumpInitial( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyJump( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyLand( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyLadder( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyFire( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetInaccuracyMove( const CEconItemView* pWepView, int nAlt = 0 ) const;

		float	GetInaccuracyReload( const CEconItemView* pWepView, int nAlt = 0 ) const;

		float	GetRecoilAngle( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRecoilAngleVariance( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRecoilMagnitude( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetRecoilMagnitudeVariance( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetTracerFrequency( const CEconItemView* pWepView, int nAlt = 0 ) const;

		int		GetPrimaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const OVERRIDE;
		int		GetSecondaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const OVERRIDE;
		int		GetDefaultPrimaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const OVERRIDE;
		int		GetDefaultSecondaryClipSize( const CEconItemView* pWepView, int nAlt = 0 ) const OVERRIDE;
		int		GetPrimaryReserveAmmoMax( const CEconItemView* pWepView, int nAlt = 0 ) const OVERRIDE;
		int		GetSecondaryReserveAmmoMax( const CEconItemView* pWepView, int nAlt = 0 ) const OVERRIDE;

		int		GetKillAward( const CEconItemView* pWepView, int nAlt = 0 ) const;
		bool	HasBurstMode( const CEconItemView* pWepView, int nAlt = 0 ) const;
		bool	IsRevolver( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetArmorRatio( const CEconItemView* pWepView, int nAlt = 0 ) const;
		bool	CannotShootUnderwater( const CEconItemView* pWepView, int nAlt = 0 ) const;
		bool	DoesUnzoomAfterShot( const CEconItemView* pWepView, int nAlt = 0 ) const;
		bool	DoesHideViewModelWhenZoomed( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetZoomLevels( const CEconItemView* pWepView, int nAlt = 0 ) const;


		int		GetZoomFOV1( const CEconItemView* pWepView, int nAlt = 0 ) const;
		int		GetZoomFOV2( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetZoomTime0( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetZoomTime1( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float	GetZoomTime2( const CEconItemView* pWepView, int nAlt = 0 ) const;

		float   GetAttackMovespeedFactor( const CEconItemView* pWepView = NULL, int nAlt = 0 ) const;

		CSWeaponType GetWeaponType( const CEconItemView* pWepView ) const;
		CSWeaponCategory GetWeaponCategory( const CEconItemView* pWepView ) const;
		const char* GetAddonLocation( const CEconItemView* pWepView ) const;
		const char* GetEjectBrassEffectName( const CEconItemView* pWepView ) const;
		const char* GetTracerEffectName( const CEconItemView* pWepView ) const;
		const char* GetMuzzleFlashEffectName_1stPerson( const CEconItemView* pWepView ) const;
		const char* GetMuzzleFlashEffectName_1stPersonAlt( const CEconItemView* pWepView ) const;
		const char* GetMuzzleFlashEffectName_3rdPerson( const CEconItemView* pWepView ) const;
		const char* GetMuzzleFlashEffectName_3rdPersonAlt( const CEconItemView* pWepView ) const;
		const char* GetHeatEffectName( const CEconItemView* pWepView ) const;
		const char* GetAnimExtension( const CEconItemView* pWepView ) const;
		int			GetUsedByTeam( const CEconItemView* pWepView ) const;

		float		GetBotAudibleRange( const CEconItemView* pWepView, int nAlt = 0 ) const;
		const char* GetWrongTeamMsg( const CEconItemView* pWepView, int nAlt = 0 ) const;
		const char* GetSilencerModel( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float		GetAddonScale( const CEconItemView* pWepView, int nAlt = 0 ) const;
		float		GetThrowVelocity( const CEconItemView* pWepView, int nAlt = 0 ) const;

		const char* GetAddonModel( const CEconItemView* pWepView ) const;
		const CUtlVector< WeaponPaintableMaterial_t >* GetPaintData( const CEconItemView* pWepView ) const;
	};

#if USE_WEAPON_DATA_CACHE
} // namespace OldWeaponData
#endif

#if USE_WEAPON_DATA_CACHE && TEST_WEAPON_DATA_CACHE
extern const OldWeaponData::CCSWeaponInfo& GetOldWeaponInfo( const class CCSWeaponData* ); // for testing
#endif


#define SECONDARY_FIRE_ATTR(name) ( name " alt" )

#define FIRINGMODE_ATTR_FLOAT_SCALE( var, name, scale )	do { var[0] = ITEM_ATTR_FLOAT_SCALE( name, scale ); var[1] = ITEM_ATTR_FLOAT_SCALE( name " alt", scale ); } while(0)
#define FIRINGMODE_ATTR_INT( var, name )				do { var[0] = ITEM_ATTR_INT( name ); var[1] = ITEM_ATTR_INT( name " alt" ); } while(0)
#define FIRINGMODE_ATTR_BOOL( var, name )				do { var[0] = ITEM_ATTR_BOOL( name ); var[1] = ITEM_ATTR_BOOL( name " alt" ); } while(0)
#define FIRINGMODE_ATTR_STRING( var, name )				do { var[0] = ITEM_ATTR_STRING( name ); var[1] = ITEM_ATTR_STRING( name " alt" ); } while(0)
#define FIRINGMODE_ATTR_FLOAT( var, name )				FIRINGMODE_ATTR_FLOAT_SCALE( var, name, 1.0f )

#define WEAPON_ACCESSOR_ALT( MemberType, FunctionName, mMemberName )													\
	MemberType FunctionName( const CEconItemView* pWepView, int nElt = 0 ) const {										\
		COMPILE_TIME_ASSERT( V_ARRAYSIZE(mMemberName) == 2 );															\
		Assert( VerifyItem(pWepView) );																					\
		Assert(WeaponAttr_IsSame(GetOldWeaponInfo(this).FunctionName(pWepView, nElt), (mMemberName)[nElt ? 1 : 0]));	\
		return (mMemberName)[nElt ? 1 : 0];																				\
	}

class CCSWeaponData : public CWeaponData
{
public:
	DECLARE_CLASS( CCSWeaponData, CWeaponData );

	explicit CCSWeaponData( item_definition_index_t itemDefIndex ) : CWeaponData( itemDefIndex ) {}

	CSWeaponID m_WeaponID; // TODO REMOVE
	CSWeaponType m_WeaponType;
	CSWeaponCategory m_WeaponCategory;

	// Money
	int m_nPrice;
	int m_nKillAward;

	// Animation data
	const char* m_szAnimExtension; // TODO: Investigate if this is used

	// Firing rate
	float m_flCycleTime[2];
	float m_flTimeToIdleAfterFire;
	float m_flIdleInterval;
	bool m_bIsFullAuto;

	// Damage parameters
	int m_nDamage;
	float m_flArmorRatio;
	int m_nNumBullets;		// >1 for shotguns
	float m_flPenetration;
	float m_flFlinchVelocityModifierLarge;
	float m_flFlinchVelocityModifierSmall;
	float m_flRange;
	float m_flRangeModifier;

	// Grenade data
	float m_flThrowVelocity;
	Vector m_vSmokeColor;

	// silencer
	CSWeaponSilencerType m_eSilencerType;
	const char* m_szSilencerModel;

	// Crosshair
	int m_nCrosshairMinDistance;
	int m_nCrosshairDeltaDistance;

	// Running speed
	float m_flMaxSpeed[2];

	float m_flAttackMovespeedFactor;


	// Inaccuracy parameters
	float m_flSpread[2];
	float m_flInaccuracyCrouch[2];
	float m_flInaccuracyStand[2];
	float m_flInaccuracyJumpInitial;
	float m_flInaccuracyJump[2];
	float m_flInaccuracyLand[2];
	float m_flInaccuracyLadder[2];
	float m_flInaccuracyFire[2];
	float m_flInaccuracyMove[2];
	float m_flInaccuracyReload;
	//float mInaccuracyAltSwitch; // Not shipping this

	// Spray pattern parameters
	// (Also affected by recovery rate)
	int m_nRecoilSeed;
	float m_flRecoilAngle[2];
	float m_flRecoilAngleVariance[2];
	float m_flRecoilMagnitude[2];
	float m_flRecoilMagnitudeVariance[2];
	int m_nSpreadSeed;

	// Inaccuracy recovery parameters
	float m_flRecoveryTimeCrouch;
	float m_flRecoveryTimeStand;
	float m_flRecoveryTimeCrouchFinal;
	float m_flRecoveryTimeStandFinal;
	int m_nRecoveryTransitionStartBullet;
	int m_nRecoveryTransitionEndBullet;

	// Scope information
	bool m_bUnzoomsAfterShot;
	bool m_bHideViewModelWhenZoomed;
	int m_nZoomLevels;
	int m_nZoomFOV1;
	int m_nZoomFOV2;
	float m_flZoomTime0;
	float m_flZoomTime1;
	float m_flZoomTime2;

	// Holstered visuals
	const char* m_szAddonLocation;
	const char* m_szAddonModel;
	float m_flAddonScale;

	// Firing effects
	const char* m_szEjectBrassEffectName;
	const char* m_szTracerEffectName;
	int m_nTracerFrequency[2];
	const char* m_szMuzzleFlashEffectName_1stPerson[2];
	const char* m_szMuzzleFlashEffectName_3rdPerson[2];
	const char* m_szHeatEffectName;
	float m_flHeatPerShot;

	// Sounds
	const char* m_szZoomInSound;
	const char* m_szZoomOutSound;
	float m_flInaccuracyPitchShfit;
	float m_flInaccuracyAltSoundThreshold;
	float m_flBotAudibleRange;

	// Custom paint data
	const CUtlVector< WeaponPaintableMaterial_t >* m_PaintData;

	// Misc
	int m_nUsedByTeam; // TEAM_CT or TEAM_TERRORIST or TEAM_UNASSIGNED if both
	const char* m_szWrongTeamMsg;	// Seems to always be "".  $$$REI TODO REMOVE
	bool m_bHasBurstMode;
	bool m_bIsRevolver;		// TODO: Seems like there should be a better way to do this
	bool m_bCannotShootUnderwater;

public: //Temporary accessors for migration
	// NOTE: New code should probably not use any of the code in this section.
	// Instead, just access the member variables above directly, or use the helpers declared there that
	// do not take a CEconItemView.
	//
	// Client code can only get access to a const CCSWeaponData, and the simplest method is to treat that
	// as a constant view of the member variables it holds.

	WEAPON_ACCESSOR( const char*, GetZoomInSound, m_szZoomInSound );
	WEAPON_ACCESSOR( const char*, GetZoomOutSound, m_szZoomOutSound );
	WEAPON_ACCESSOR( const Vector&, GetSmokeColor, m_vSmokeColor );
	WEAPON_ACCESSOR( CSWeaponID, GetWeaponID, m_WeaponID );
	WEAPON_ACCESSOR( int, GetWeaponPrice, m_nPrice );
	WEAPON_ACCESSOR( bool, IsFullAuto, m_bIsFullAuto );
	WEAPON_ACCESSOR( CSWeaponSilencerType, GetSilencerType, m_eSilencerType );
	WEAPON_ACCESSOR( int, GetBullets, m_nNumBullets );
	WEAPON_ACCESSOR_ALT( float, GetCycleTime, m_flCycleTime );
	WEAPON_ACCESSOR( float, GetHeatPerShot, m_flHeatPerShot );
	WEAPON_ACCESSOR( float, GetRecoveryTimeCrouch, m_flRecoveryTimeCrouch );
	WEAPON_ACCESSOR( float, GetRecoveryTimeStand, m_flRecoveryTimeStand );
	WEAPON_ACCESSOR( float, GetRecoveryTimeCrouchFinal, m_flRecoveryTimeCrouchFinal );
	WEAPON_ACCESSOR( float, GetRecoveryTimeStandFinal, m_flRecoveryTimeStandFinal );
	WEAPON_ACCESSOR( int, GetRecoveryTransitionStartBullet, m_nRecoveryTransitionStartBullet );
	WEAPON_ACCESSOR( int, GetRecoveryTransitionEndBullet, m_nRecoveryTransitionEndBullet );
	WEAPON_ACCESSOR( int, GetRecoilSeed, m_nRecoilSeed );
	WEAPON_ACCESSOR( int, GetSpreadSeed, m_nSpreadSeed );
	WEAPON_ACCESSOR( float, GetFlinchVelocityModifierLarge, m_flFlinchVelocityModifierLarge );
	WEAPON_ACCESSOR( float, GetFlinchVelocityModifierSmall, m_flFlinchVelocityModifierSmall );
	WEAPON_ACCESSOR( float, GetTimeToIdleAfterFire, m_flTimeToIdleAfterFire );
	WEAPON_ACCESSOR( float, GetIdleInterval, m_flIdleInterval );
	WEAPON_ACCESSOR( float, GetRange, m_flRange );
	WEAPON_ACCESSOR( float, GetRangeModifier, m_flRangeModifier );
	WEAPON_ACCESSOR( int, GetDamage, m_nDamage );
	WEAPON_ACCESSOR( float, GetPenetration, m_flPenetration );
	WEAPON_ACCESSOR( int, GetCrosshairDeltaDistance, m_nCrosshairDeltaDistance );
	WEAPON_ACCESSOR( int, GetCrosshairMinDistance, m_nCrosshairMinDistance );
	WEAPON_ACCESSOR( float, GetInaccuracyPitchShift, m_flInaccuracyPitchShfit );
	WEAPON_ACCESSOR( float, GetInaccuracyAltSoundThreshold, m_flInaccuracyAltSoundThreshold );
	WEAPON_ACCESSOR_ALT( float, GetMaxSpeed, m_flMaxSpeed );
	WEAPON_ACCESSOR_ALT( float, GetSpread, m_flSpread );
	WEAPON_ACCESSOR_ALT( float, GetInaccuracyCrouch, m_flInaccuracyCrouch );
	WEAPON_ACCESSOR_ALT( float, GetInaccuracyStand, m_flInaccuracyStand );
	WEAPON_ACCESSOR( float, GetInaccuracyJumpInitial, m_flInaccuracyJumpInitial );
	WEAPON_ACCESSOR_ALT( float, GetInaccuracyJump, m_flInaccuracyJump );
	WEAPON_ACCESSOR_ALT( float, GetInaccuracyLand, m_flInaccuracyLand );
	WEAPON_ACCESSOR_ALT( float, GetInaccuracyLadder, m_flInaccuracyLadder );
	WEAPON_ACCESSOR_ALT( float, GetInaccuracyFire, m_flInaccuracyFire );
	WEAPON_ACCESSOR_ALT( float, GetInaccuracyMove, m_flInaccuracyMove );
	WEAPON_ACCESSOR( float, GetInaccuracyReload, m_flInaccuracyReload );
	WEAPON_ACCESSOR_ALT( float, GetRecoilAngle, m_flRecoilAngle );
	WEAPON_ACCESSOR_ALT( float, GetRecoilAngleVariance, m_flRecoilAngleVariance );
	WEAPON_ACCESSOR_ALT( float, GetRecoilMagnitude, m_flRecoilMagnitude );
	WEAPON_ACCESSOR_ALT( float, GetRecoilMagnitudeVariance, m_flRecoilMagnitudeVariance );
	WEAPON_ACCESSOR_ALT( int, GetTracerFrequency, m_nTracerFrequency );
	WEAPON_ACCESSOR( int, GetKillAward, m_nKillAward );
	WEAPON_ACCESSOR( bool, HasBurstMode, m_bHasBurstMode );
	WEAPON_ACCESSOR( bool, IsRevolver, m_bIsRevolver );
	WEAPON_ACCESSOR( float, GetArmorRatio, m_flArmorRatio );
	WEAPON_ACCESSOR( bool, CannotShootUnderwater, m_bCannotShootUnderwater );
	WEAPON_ACCESSOR( bool, DoesUnzoomAfterShot, m_bUnzoomsAfterShot );
	WEAPON_ACCESSOR( bool, DoesHideViewModelWhenZoomed, m_bHideViewModelWhenZoomed );
	WEAPON_ACCESSOR( int, GetZoomLevels, m_nZoomLevels );
	WEAPON_ACCESSOR( int, GetZoomFOV1, m_nZoomFOV1 );
	WEAPON_ACCESSOR( int, GetZoomFOV2, m_nZoomFOV2 );
	WEAPON_ACCESSOR( float, GetZoomTime0, m_flZoomTime0 );
	WEAPON_ACCESSOR( float, GetZoomTime1, m_flZoomTime1 );
	WEAPON_ACCESSOR( float, GetZoomTime2, m_flZoomTime2 );

	WEAPON_ACCESSOR( float, GetAttackMovespeedFactor, m_flAttackMovespeedFactor );


	WEAPON_ACCESSOR( CSWeaponType, GetWeaponType, m_WeaponType );
	WEAPON_ACCESSOR( CSWeaponCategory, GetWeaponCategory, m_WeaponCategory );
	WEAPON_ACCESSOR( const char*, GetAddonLocation, m_szAddonLocation );
	WEAPON_ACCESSOR( const char*, GetEjectBrassEffectName, m_szEjectBrassEffectName );
	WEAPON_ACCESSOR( const char*, GetTracerEffectName, m_szTracerEffectName );
	WEAPON_ACCESSOR( const char*, GetMuzzleFlashEffectName_1stPerson, m_szMuzzleFlashEffectName_1stPerson[0] );
	WEAPON_ACCESSOR( const char*, GetMuzzleFlashEffectName_1stPersonAlt, m_szMuzzleFlashEffectName_1stPerson[1] );
	WEAPON_ACCESSOR( const char*, GetMuzzleFlashEffectName_3rdPerson, m_szMuzzleFlashEffectName_3rdPerson[0] );
	WEAPON_ACCESSOR( const char*, GetMuzzleFlashEffectName_3rdPersonAlt, m_szMuzzleFlashEffectName_3rdPerson[1] );
	WEAPON_ACCESSOR( const char*, GetHeatEffectName, m_szHeatEffectName );
	WEAPON_ACCESSOR( const char*, GetAnimExtension, m_szAnimExtension );
	WEAPON_ACCESSOR( int, GetUsedByTeam, m_nUsedByTeam );
	WEAPON_ACCESSOR( float, GetBotAudibleRange, m_flBotAudibleRange );
	WEAPON_ACCESSOR( const char*, GetWrongTeamMsg, m_szWrongTeamMsg );
	WEAPON_ACCESSOR( const char*, GetSilencerModel, m_szSilencerModel );
	WEAPON_ACCESSOR( float, GetAddonScale, m_flAddonScale );
	WEAPON_ACCESSOR( float, GetThrowVelocity, m_flThrowVelocity );
	WEAPON_ACCESSOR( const char*, GetAddonModel, m_szAddonModel );
	WEAPON_ACCESSOR( const CUtlVector< WeaponPaintableMaterial_t >*, GetPaintData, m_PaintData );

	bool HasSilencer() const { return m_eSilencerType != WEAPONSILENCER_NONE; }
	bool HasDetachableSilencer() const { return m_eSilencerType == WEAPONSILENCER_DETACHABLE; }

protected:
	virtual bool Fill() OVERRIDE;
};




#endif // CS_WEAPON_CACHE_H

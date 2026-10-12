//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "weapon_tablet.h"
#include "dangerzone_controller.h"
#include "drone.h"
#include "br_items.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
	#include "c_props.h"

	#include "c_cs_playerresource.h"
	#include "c_plantedc4.h"
	#include "materialsystem/imaterialvar.h"
	#include "weapon_c4.h"
	#include "mathlib/camera.h"
	#include "shaderapi/ishaderapi.h"

	#include "c_vguiscreen.h"
	#include "ienginevgui.h"
	#include "VGuiMatSurface/IMatSystemSurface.h"
#else
	#include "cs_player.h"
	#include "props.h"
	#include "world.h"
	#include "econ/econ_entity_creation.h"
	#include "func_hostage_rescue.h"
	#include "decoy_projectile.h"
#endif

#if defined(CLIENT_DLL) && defined(PANORAMA_ENABLE)
	#include "panorama/csgo_survival_buymenu.h"
	#include "panorama/hud/csgo_hudsurvivalzonemoneyprogress.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

DEVELOPMENT_ONLY_CONVAR( tablet_compass_x, 0.9 );
DEVELOPMENT_ONLY_CONVAR( tablet_compass_y, 0.235 );
DEVELOPMENT_ONLY_CONVAR( tablet_bomb_timer_x, 0.75 );
DEVELOPMENT_ONLY_CONVAR( tablet_bomb_icon_x, 0.835 );

struct staticTabletNotificationData_t
{
	Color m_fgColor;
	Color m_bgColor;
	float m_flDurationToLinger;
	int m_nSpecialInsertionRule;
	const char* m_pszLocalizationToken;
	const char* m_pszAdditionalString;
};

static const float k_flNotificationTransitionDuration = 1.0f;

static staticTabletNotificationData_t s_pszTabletNotificationLocalizationTokens[] = {
	{ Color( 0,0,0,255 ), Color( 255,255,255,100 ),	0.0f,	TABLET_STRINGBUILDRULE_NONE,			"" },											// TABLET_NOTIFICATION_UNSET
	{ Color( 0,0,0,255 ), Color( 221,199,40,100 ),	3.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_UpgradeHighRes" },			// TABLET_NOTIFICATION_UPGRADE_HIGHRES
	{ Color( 0,0,0,255 ), Color( 187,52,50,100 ),	3.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_UpgradeZone" },			// TABLET_NOTIFICATION_UPGRADE_ZONEINTEL
	{ Color( 0,0,0,255 ), Color( 69,165,203,100 ),	3.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_UpgradeDrone" },			// TABLET_NOTIFICATION_UPGRADE_DRONEINTEL
	{ Color( 0,0,0,255 ), Color( 238,168,0,100 ),	5.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_HostageTransit" },			// TABLET_NOTIFICATION_HOSTAGE_IN_TRANSIT
	{ Color( 0,0,0,255 ), Color( 109,233,238,100 ),	4.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_IncomingDelivery" },		// TABLET_NOTIFICATION_INCOMING_DELIVERY
	{ Color( 0,0,0,255 ), Color( 0,255,0,100 ),		4.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_AccessBuymenu" },			// TABLET_NOTIFICATION_PRESS_B_TO_BUY
	{ Color( 0,0,0,255 ), Color( 255,0,0,100 ),		3.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_ReturnToSafeArea" },		// TABLET_NOTIFICATION_RETURN_TO_PLAY_AREA
	{ Color( 0,0,0,255 ), Color( 200,200,200,100 ),	3.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_SatelliteSignalLost" },	// TABLET_NOTIFICATION_SIGNAL_LOST
	{ Color( 0,0,0,255 ), Color( 243,215,24,100 ),	12.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_HighlightedSectors" },		// TABLET_NOTIFICATION_HIGHLIGHTED_SECTORS_CONTAIN_PLAYERS
	{ Color( 0,0,0,255 ), Color( 0,200,0,100 ),		4.0f,	TABLET_STRINGBUILDRULE_CASHBUNDLEX,		"#TabletNotification_ExplorationPayment", "sv_dz_exploration_payment_amount" },		// TABLET_NOTIFICATION_EXPLORATION_PAYMENT
	{ Color( 0,0,0,255 ), Color( 238,168,0,100 ),	15.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_BombPlanted" },			// TABLET_NOTIFICATION_BOMB_PLANTED_ON_TARGET
	{ Color( 0,0,0,255 ), Color( 238,168,0,100 ),	8.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_BombDetonated" },			// TABLET_NOTIFICATION_BOMB_DETONATED_ON_TARGET
	{ Color( 0,0,0,255 ), Color( 109,233,238,100 ),	4.0f,	TABLET_STRINGBUILDRULE_LASTPURCHASE,	"#TabletNotification_PurchaseConfirmation" },	// TABLET_NOTIFICATION_PURCHASE_CONFIRMATION
	{ Color( 0,0,0,255 ), Color( 0,200,0,100 ),		5.0f,	TABLET_STRINGBUILDRULE_CASHBUNDLEX,		"#TabletNotification_BombWaveMoney", "sv_dz_zone_bombdrop_money_reward" },			// TABLET_NOTIFICATION_BOMBWAVE_MONEY
	{ Color( 195,75,60,255 ), Color( 55,55,55,100 ),30.0f,	TABLET_STRINGBUILDRULE_NONE,			"#TabletNotification_ParadropsAllowed" },		// TABLET_NOTIFICATION_PARADROPS_ALLOWED
};
COMPILE_TIME_ASSERT( ARRAYSIZE( s_pszTabletNotificationLocalizationTokens ) == TABLET_NOTIFICATION_COUNT );

const staticTabletNotificationData_t* GetTabletNotificationData( int nIdx )
{
	if ( nIdx < 0 || nIdx >= ARRAYSIZE( s_pszTabletNotificationLocalizationTokens ) )
	{
		Assert( false );
		return NULL;
	}
	return &s_pszTabletNotificationLocalizationTokens[nIdx];
}

#ifndef CLIENT_DLL
void UTIL_PushGlobalTabletNotification( tablet_notification_t nNotificationId )
{
	FOR_EACH_VEC( ITablet::AutoList(), i )
	{
		CTablet *pTablet = static_cast<CTablet *>(ITablet::AutoList()[i]->GetEntity());
		pTablet->PushTabletNotification( nNotificationId );
	}
}
#endif

#ifdef CLIENT_DLL
Vector2D g_TabletScreenSpace_UL;
//Vector2D g_TabletScreenSpace_LL;
Vector2D g_TabletScreenSpace_LR;

#define NUM_HIGHRES_DIRECTION 6


class CTabletTextPanel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CTabletTextPanel, CVGuiScreenPanel );

public:
	CTabletTextPanel( vgui::Panel *parent, const char *panelName );
	~CTabletTextPanel() {};
	virtual void Paint() OVERRIDE;
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void SetParams( const wchar_t *pszText, Color fgCol, Color bgCol );

private:
	vgui::HFont	m_hTextFont;
	const wchar_t *m_pszText;
	bool m_bLocalize;
	Color m_fgCol;
	Color m_bgCol;
};

DECLARE_VGUI_SCREEN_FACTORY( CTabletTextPanel, "tablet_text_panel" );

CTabletTextPanel::CTabletTextPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CTabletTextPanel", vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/tablet_text_panel.res", "Scheme" ) )
{
	SetSize( 10, 10 ); // Quiet "parent not sized yet" spew
	SetPaintBackgroundEnabled( false );
}

void CTabletTextPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	if ( pScheme )
		m_hTextFont = pScheme->GetFont( "TabletFont", true );
}

void CTabletTextPanel::SetParams( const wchar_t *pszText, Color fgCol, Color bgCol )
{
	m_pszText = pszText;
	m_fgCol = fgCol;
	m_bgCol = bgCol;
}

void CTabletTextPanel::Paint()
{
	if ( m_bgCol.a() > 0 )
	{
		int nWide, nTall;
		vgui::surface()->GetTextSize( m_hTextFont, m_pszText, nWide, nTall );
		vgui::surface()->DrawSetColor( m_bgCol );
		vgui::surface()->DrawFilledRect( 0, 0, nWide + 2, nTall );
	}

	vgui::surface()->DrawSetTextColor( m_fgCol );
	vgui::surface()->DrawSetTextFont( m_hTextFont );
	vgui::surface()->DrawSetTextPos( 0, 0 );
	vgui::surface()->DrawPrintText( m_pszText, wcslen( m_pszText ) );
}


#endif

static const float k_flTabletBootDuration = 0.82f;

COMPILE_TIME_ASSERT( TABLET_HEX_COUNT == WORLD_HEX_NUM );

DEVELOPMENT_ONLY_CONVAR( dev_tablet_zone_prediction_always, 0 );
DEVELOPMENT_ONLY_CONVAR( dev_tablet_zone_prediction_show_circle, 0 );

ConVar tablet_c4_dist_min( "tablet_c4_dist_min", "400", FCVAR_RELEASE | FCVAR_REPLICATED );
ConVar tablet_c4_dist_max( "tablet_c4_dist_max", "3000", FCVAR_RELEASE | FCVAR_REPLICATED );

IMPLEMENT_AUTO_LIST( ITablet )

IMPLEMENT_NETWORKCLASS_ALIASED( Tablet, DT_WeaponTablet )

BEGIN_NETWORK_TABLE( CTablet, DT_WeaponTablet )

#if !defined( CLIENT_DLL )
SendPropArray( SendPropFloat( SENDINFO_ARRAY( m_flUpgradeExpirationTime ), -1, SPROP_NOSCALE ), m_flUpgradeExpirationTime ),
SendPropArray( SendPropInt( SENDINFO_ARRAY( m_vecLocalHexFlags ), -1, SPROP_UNSIGNED ), m_vecLocalHexFlags ),
SendPropInt( SENDINFO( m_nContractKillGridIndex ) ),
SendPropInt( SENDINFO( m_nContractKillGridHighResIndex ) ),
SendPropBool( SENDINFO( m_bTabletReceptionIsBlocked ) ),
SendPropFloat( SENDINFO( m_flScanProgress ) ),
SendPropFloat( SENDINFO( m_flBootTime ) ),
SendPropFloat( SENDINFO( m_flShowMapTime ) ),
SendPropArray( SendPropInt( SENDINFO_ARRAY( m_vecNotificationIds ), -1, SPROP_UNSIGNED ), m_vecNotificationIds ),
SendPropArray( SendPropFloat( SENDINFO_ARRAY( m_vecNotificationTimestamps ), -1, SPROP_UNSIGNED ), m_vecNotificationTimestamps ),
SendPropArray( SendPropVector( SENDINFO_ARRAY( m_vecPlayerPositionHistory ), -1, SPROP_NOSCALE ), m_vecPlayerPositionHistory ),
SendPropInt( SENDINFO( m_nLastPurchaseIndex ) ),
#else
RecvPropArray( RecvPropFloat( RECVINFO( m_flUpgradeExpirationTime[0] ) ), m_flUpgradeExpirationTime ),
RecvPropArray( RecvPropInt( RECVINFO( m_vecLocalHexFlags[0] ) ), m_vecLocalHexFlags ),
RecvPropInt( RECVINFO( m_nContractKillGridIndex ) ),
RecvPropInt( RECVINFO( m_nContractKillGridHighResIndex ) ),
RecvPropBool( RECVINFO( m_bTabletReceptionIsBlocked ) ),
RecvPropFloat( RECVINFO( m_flScanProgress ) ),
RecvPropFloat( RECVINFO( m_flBootTime ) ),
RecvPropFloat( RECVINFO( m_flShowMapTime ) ),
RecvPropArray( RecvPropInt( RECVINFO( m_vecNotificationIds[0] ) ), m_vecNotificationIds ),
RecvPropArray( RecvPropFloat( RECVINFO( m_vecNotificationTimestamps[0] ) ), m_vecNotificationTimestamps ),
RecvPropArray( RecvPropVector( RECVINFO( m_vecPlayerPositionHistory[0] ) ), m_vecPlayerPositionHistory ),
RecvPropInt( RECVINFO( m_nLastPurchaseIndex ) ),
#endif

END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTablet )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_tablet, Tablet );

#ifdef CLIENT_DLL
PRECACHE_REGISTER( weapon_tablet )
#endif

ConVar sv_tablet_show_path_to_nearest_resq( "sv_tablet_show_path_to_nearest_resq", "0", FCVAR_RELEASE | FCVAR_REPLICATED );

#ifndef CLIENT_DLL
ConVar sv_dz_exploration_payment_amount( "sv_dz_exploration_payment_amount", "2", FCVAR_REPLICATED | FCVAR_RELEASE, "Number of cash bundles to award for exploring a new sector" );
#endif

#ifdef CLIENT_DLL
void CTablet::Precache( void )
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
		return;

	#define REGISTER_TABLET_MATERIAL( _n ) PrecacheMaterial( "models/weapons/v_models/tablet/" #_n );
	#include "weapon_tablet_materials.inc"
	#undef REGISTER_TABLET_MATERIAL

	for ( int i = 0; i < TABLETMAT_COUNT; i++ )
	{
		IMaterial *pTabletRadarMat = GetTabletMaterial( (tabletmaterialindex_t)i );
		if ( !pTabletRadarMat )
		{
			Assert( false );
			continue;
		}
	
		bool bFound = false;
		IMaterialVar* pVarThatNeedsLevelName = pTabletRadarMat->FindVar( "$levelnamereplacevar", &bFound, false );
		if ( !bFound )
			continue;

		IMaterialVar* pMapNameVar = pTabletRadarMat->FindVar( pVarThatNeedsLevelName->GetStringValue(), &bFound, true );
		if ( !bFound )
		{
			Assert( false );
			continue;
		}

		if ( V_stristr( pMapNameVar->GetStringValue(), "%levelname%" ) )
		{
			char szTemp[MAX_PATH];
			V_StrSubst( pMapNameVar->GetStringValue(), "%levelname%", engine->GetLevelNameShort(), szTemp, sizeof( szTemp ), false );
			pMapNameVar->SetStringValue( szTemp );
			pTabletRadarMat->RefreshPreservingMaterialVars();
		}
	}
}
#endif

CTablet::CTablet()
{
	m_nLastPurchaseIndex = -1;

#ifdef CLIENT_DLL
	for ( int i = 0; i < ARRAYSIZE( m_vecTabletMaterials ); i++ )
	{
		m_vecTabletMaterials[i] = NULL;
	}
	m_flNoiseFadeAlpha = 0;
	m_flPrevScanProgress = 0;
	m_nRenderTargetRes = 1024;
	V_memset( &m_vecLastHexPlayerOccupancyChange, 0, sizeof( m_vecLastHexPlayerOccupancyChange ) );
#endif

#ifndef CLIENT_DLL
	ClearContractKill();
	m_bTabletReceptionIsBlocked = false;
	m_flNextCheckForIncomingDronesTime = 0;
	m_flScanProgress = 0;
	m_flBootTime = 0;
	m_bPendingBuyMenu = false;
	m_flShowMapTime = 0;

	ResetLocalHexes();

	for ( int i = 0; i < TABLET_NOTIFICATION_ARRAYSIZE; i++ )
	{
		m_vecNotificationIds.Set( i, TABLET_NOTIFICATION_UNSET );
		m_vecNotificationTimestamps.Set( i, 0 );
	}

	for ( int i = 0; i < TABLET_PLAYER_POSITION_HISTORY_COUNT; i++ )
	{
		m_vecPlayerPositionHistory.Set( i, vec3_origin );
	}

	for ( int i = 0; i < TABLET_UPGRADE_COUNT; i++ )
	{
		m_flUpgradeExpirationTime.Set( i, 0 );
	}

	m_vecLastPlayerPosition.Invalidate();
#endif

	m_flLastClosePoseParamVal = 0.0f;
}

CTablet::~CTablet()
{
#ifdef CLIENT_DLL
	m_PanelWrapper.Deactivate();
#endif
}

void CTablet::Spawn()
{
	BaseClass::Spawn();

#ifndef CLIENT_DLL
	m_flLastPlayerOccupiedGridUpdate = 0;
	m_flLastTabletBlockedTime = 0;

	AssertMsg( IHostageRescueZone::AutoList().Count() > 1, "Map has fewer than 2 hostage rescue zones!" );
#endif

#ifdef CLIENT_DLL
	VGuiScreenInitData_t initData( this );
	m_PanelWrapper.Activate( "tablet_text_panel", NULL, 0, &initData );
#endif
}

float CTablet::GetTabletHexSize()
{
	float flScaler = 1.166666666666667;
	float flHexSize = CSGameRules()->GetSurvivalRules()->GetTabletHexSize();

	return flHexSize * flScaler;
	//return (bHighRes ? 3500.0f : 4687.5f);
}

bool CTablet::Deploy()
{
	bool bDeploy = BaseClass::Deploy();

#ifndef CLIENT_DLL // FIXME: deploy cannot be relied upon to run on the client, if the server forcibly changes a player's weapon.

	SendWeaponAnim( ACT_VM_DRAW );

	MDLCACHE_CRITICAL_SECTION();

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration() * 0.7f; // fixme: primary attack for tablet just means mostly deployed... the tablet doesn't 'fire'.... maybe it should?
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration() * 0.7f;
	SetWeaponIdleTime( gpGlobals->curtime + 2 );

	if ( GetPlayerOwner() )
	{
		CBaseViewModel *vm = GetViewModelPtr();
		if ( vm )
			vm->SetBodygroup( 2, 0 );
	}

	UpdatePlayerOccupiedGrid();

	CheckForIncomingDrones();

	UpdateTabletNotifications();

	SetTabletSkinState( TABLET_SKIN_STATE_RADAR );

	if ( CSGameRules()->IsWarmupPeriod() )
	{
		PushTabletNotification( TABLET_NOTIFICATION_HIGHLIGHTED_SECTORS_CONTAIN_PLAYERS );
	}

#endif

	return bDeploy;
}

void CTablet::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( gpGlobals->curtime > m_flNextPrimaryAttack )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;
		SetWeaponIdleTime( gpGlobals->curtime + 20 );

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}
}

#ifdef CLIENT_DLL
ConVar cl_tablet_mapmode( "cl_tablet_mapmode", "1", FCVAR_RELEASE | FCVAR_ARCHIVE );
#endif

void CTablet::SecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( gpGlobals->curtime > m_flNextSecondaryAttack )
	{
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.25f;
		SetWeaponIdleTime( gpGlobals->curtime + 20 );
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}
	else
	{
		return;
	}
}

#ifndef CLIENT_DLL

void CTablet::UpdateTabletExplorationPayout( void )
{
	if ( CSGameRules()->IsWarmupPeriod() )
		return;

	CCSPlayer *pOwnerPlayer = ToCSPlayer( GetPlayerOwner() );

	if ( !pOwnerPlayer )
		return;

	if ( gpGlobals->curtime < m_ExplorationPaymentRecord.m_flNextExplorationTime ) // pay on time based intervals
		return;

	if ( pOwnerPlayer->GetParent() ) // in spawn helicopter
		return;

	if ( pOwnerPlayer->m_bIsSpawnRappelling ) // on the rappel rope
		return;

	m_ExplorationPaymentRecord.m_flNextExplorationTime = gpGlobals->curtime + 0.25f;

	int nCurrentSectorIndex = CSGameRules()->GetSurvivalRules()->FindGridCenterIndexClosestToWorldPos( pOwnerPlayer->GetAbsOrigin() );
	if ( nCurrentSectorIndex < 0 )
		return;

	int nLimit = GetThinkLimitToExploreSector();

	if ( m_ExplorationPaymentRecord.m_nThinksInSector[nCurrentSectorIndex] < nLimit )
	{
		// still exploring
		m_ExplorationPaymentRecord.m_nThinksInSector[nCurrentSectorIndex]++; // increment
		m_flScanProgress = (float)m_ExplorationPaymentRecord.m_nThinksInSector[nCurrentSectorIndex] / (float)nLimit;
		m_flScanProgress = clamp( m_flScanProgress.Get(), 0.0f, 0.99f );
	}
	else if ( m_ExplorationPaymentRecord.m_nThinksInSector[nCurrentSectorIndex] == nLimit )
	{
		m_ExplorationPaymentRecord.m_nThinksInSector[nCurrentSectorIndex]++; // increment

		// pay
		extern ConVar contributionscore_cash_bundle, sv_dz_cash_bundle_size;
		int reward = sv_dz_exploration_payment_amount.GetInt() * sv_dz_cash_bundle_size.GetInt();
		//ClientPrint( pOwnerPlayer, HUD_PRINTTALK, "#SFUI_SurvivalSectorPayment", CFmtStr( "%d", reward ) );
		PushTabletNotification( TABLET_NOTIFICATION_EXPLORATION_PAYMENT );
		pOwnerPlayer->AddAccount( reward, true, CFmtStr( "DZ Exploration Payment from tablet entindex[%d] section[%d]", entindex(), nCurrentSectorIndex ) );
		pOwnerPlayer->AddContributionScore( sv_dz_exploration_payment_amount.GetInt() * contributionscore_cash_bundle.GetInt() );

		m_flScanProgress = 1.0f;

		pOwnerPlayer->m_nHexesExplored++;
	}
	else if ( m_flScanProgress != 1.0f )
	{
		// already paid
		m_flScanProgress = 1.0f; 
	}

}

void CTablet::UpdatePlayerOccupiedGrid( void )
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
		return;

	CCSPlayer *pOwnerPlayer = ToCSPlayer( GetPlayerOwner() );

	if ( !pOwnerPlayer )
		return;

	if ( m_flLastPlayerOccupiedGridUpdate + 2.0f > gpGlobals->curtime )
	{
		return; // too soon for another update
	}

	if ( m_flLastTabletBlockedTime + 0.1f > gpGlobals->curtime )
	{
		return; // too soon to be unblocked
	}

	CDangerZoneController *pZone = GetDangerZoneController();
	if ( pZone && !pZone->IsWithinPlayArea( pOwnerPlayer->GetAbsOrigin() ) )
	{
		PushTabletNotification( TABLET_NOTIFICATION_RETURN_TO_PLAY_AREA );
	}

	// always reset hex flags
	ResetLocalHexes();

	if ( m_bTabletReceptionIsBlocked )
	{
		// this player is outside the zone. mess their tablet up!
		
		m_bTabletReceptionIsBlocked = false; // reset

		ClearContractKill();
		
		return;
	}

	m_flLastPlayerOccupiedGridUpdate = gpGlobals->curtime;

	// init sectors to be either empty or fog-o-war'd
	for ( int i = 0; i < m_vecLocalHexFlags.Count(); i++ )
	{
		bool bExplored = m_ExplorationPaymentRecord.m_nThinksInSector[i] >= GetThinkLimitToExploreSector();
		if ( bExplored )
		{
			SetHexFlag( i, HEX_FL_EXPLORED );
		}
	}

	auto lambdaSetPlayerHexFlags = [&]( const Vector& vecPosition, int *pHighResIndex = NULL ) -> int
	{
		int nHighResLocation = -1;
		int nClosestIndex = CSGameRules()->GetSurvivalRules()->FindGridCenterIndexClosestToWorldPos( vecPosition, &nHighResLocation );
		if ( nClosestIndex >= 0 )
		{
			if ( HasTabletUpgrade( TABLET_UPGRADE_HIGHRES ) )
			{
				SetHexFlag( nClosestIndex, 1 << nHighResLocation );
			}
			else
			{
				SetHexFlag( nClosestIndex, HEX_FL_OCCUPIED_0 );
			}
		}

		if ( pHighResIndex )
		{
			*pHighResIndex = nHighResLocation;
		}

		return nClosestIndex;
	};

	// decoy grenades also light up the grid
	FOR_EACH_VEC( IDecoyProjectile::AutoList(), nDecoy )
	{
		CDecoyProjectile *pDecoy = static_cast<CDecoyProjectile*>( IDecoyProjectile::AutoList()[nDecoy]->GetEntity() );
		lambdaSetPlayerHexFlags( pDecoy->GetAbsOrigin(), NULL );
	}

	// clear contract kill
	ClearContractKill();

	CCSPlayer *pOwnerPlayerContractKillTarget = ToCSPlayer( pOwnerPlayer->m_hSurvivalAssassinationTarget.Get() );
	if ( !pOwnerPlayerContractKillTarget || !pOwnerPlayerContractKillTarget->IsAlive() )
	{
		// does it make sense to validate the contract kill target here?
		pOwnerPlayer->m_hSurvivalAssassinationTarget = nullptr;
		pOwnerPlayerContractKillTarget = NULL;
	}

	for ( int iPlayer = 1; iPlayer <= MAX_PLAYERS; ++iPlayer )
	{
		CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		// is anyone carrying a hostage?
		if ( pPlayer->m_hCarriedHostage )
		{
			PushTabletNotification( TABLET_NOTIFICATION_HOSTAGE_IN_TRANSIT );
		}

		int nHighResIndex;
		int nClosestIndex = lambdaSetPlayerHexFlags( pPlayer->GetAbsOrigin(), &nHighResIndex );
		if ( pOwnerPlayerContractKillTarget && pOwnerPlayerContractKillTarget == pPlayer )
		{
			m_nContractKillGridIndex = nClosestIndex;
			m_nContractKillGridHighResIndex = nHighResIndex;
		}		
	}

	// jam hexes with jammers
	FOR_EACH_VEC_BACK( IPhysPropRadarJammer::AutoList(), nRadarJammer )
	{
		CBaseEntity *pJammer = IPhysPropRadarJammer::AutoList()[nRadarJammer]->GetEntity();

		bool bTabletOwnsThisJammer = pJammer->GetOwnerEntity() == this;

		CUtlVector<int> vecIndices;
		CSGameRules()->GetSurvivalRules()->FindSortedGridCenterIndicesClosestToPos( pJammer->GetAbsOrigin(), vecIndices );

		for ( int i = 0; i < MIN( 3, vecIndices.Count() ); i++ )
		{
			SetHexFlag( vecIndices[i], bTabletOwnsThisJammer ? HEX_FL_JAMMED_BY_OWNER : HEX_FL_JAMMED );
		}
	}

}

int CTablet::GetThinkLimitToExploreSector( void )
{
	return 50;
}
#endif // !CLIENT_DLL

float CTablet::GetRunBobScale() const
{
	return ((GetTabletSkinState() == TABLET_SKIN_STATE_BUYMENU) ? 0.0f : 0.3f);
}

bool CTablet::ShouldBuyMenuBeOpen( void )
{
	return (GetTabletSkinState() == TABLET_SKIN_STATE_BUYMENU);
}

CBaseViewModel *CTablet::GetViewModelPtr( void ) const
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		return pPlayer->GetViewModel( m_nViewModelIndex );
	}
	return NULL;
}

bool CTablet::HasTabletUpgrade( tablet_upgrade_type_t nUpgrade )
{
	if ( dev_tablet_zone_prediction_always.GetBool() && nUpgrade == TABLET_UPGRADE_ZONEINTEL )
	{
		return true;
	}

	float flExpirationTime = m_flUpgradeExpirationTime.Get( (int)nUpgrade );

	if ( flExpirationTime == 0 )
		return false;

	if ( flExpirationTime < 0 )
		return true;

	return (gpGlobals->curtime < flExpirationTime);
}

void CTablet::UpdatePoseParameter( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetActiveCSWeapon() != this )
		return;

	CBaseViewModel *vm = GetViewModelPtr();
	if ( !vm )
		return;

	MDLCACHE_CRITICAL_SECTION();

	{
		static int nClosePoseParamIndex = vm->LookupPoseParameter( "closer" );
		Assert( nClosePoseParamIndex != -1 );
		
		if ( GetTabletSkinState() == TABLET_SKIN_STATE_BUYMENU )
		{
			m_flLastClosePoseParamVal = Approach( 1.0f, m_flLastClosePoseParamVal, gpGlobals->frametime * 3.0f );
		}
		else if ( pPlayer->IsHoldingLookAtWeapon() )
		{
			m_flLastClosePoseParamVal = Approach( 0.0f, m_flLastClosePoseParamVal, gpGlobals->frametime * 4.0f );
		}
		else
		{
			float flVel = pPlayer->GetAbsVelocity().AsVector2D().Length();
			float flTarget = RemapValClamped( flVel, 75.0f, 110.0f, 1.0f, 0.0f );
			flTarget = Gain( flTarget, 0.7f );
			float flSpeed = RemapValClamped( flVel, 75.0f, 110.0f, 2.0f, 0.7f );
			m_flLastClosePoseParamVal = Approach( flTarget, m_flLastClosePoseParamVal, gpGlobals->frametime * flSpeed );
		}
		vm->SetPoseParameter( nClosePoseParamIndex, m_flLastClosePoseParamVal );
	}

}

#ifdef CLIENT_DLL

void CTablet::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_flPrevScanProgress = m_flScanProgress;
}

void CTablet::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

#if defined(CLIENT_DLL) && defined(PANORAMA_ENABLE)
	CCSPlayer *pPlayer = GetPlayerOwner();
	// If this tablet belongs to the local player or the current observer target, send scan progress to the hud
	C_CSPlayer* pHudPlayer = GetHudPlayer();
	if ( pHudPlayer && pPlayer && ( pPlayer == pHudPlayer ) )
	{
		panorama::DispatchEvent( SurvivalZoneExplorationProgress(), nullptr, m_flScanProgress );
		m_flPrevScanProgress = m_flScanProgress;
	}
#endif

}

void CTablet::WeaponPreRender( void )
{
	BaseClass::WeaponPreRender();

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetActiveCSWeapon() != this )
		return;

	CBaseViewModel *vm = GetViewModelPtr();
	if ( !vm )
		return;

	MDLCACHE_CRITICAL_SECTION();

	static int nScreenAttachIdxUL = vm->LookupAttachment( "screen_ul" );
	//static int nScreenAttachIdxLL = vm->LookupAttachment( "screen_ll" );
	static int nScreenAttachIdxLR = vm->LookupAttachment( "screen_lr" );
	if ( nScreenAttachIdxUL >= 0 && nScreenAttachIdxLR >= 0 )
	{
		Vector vecWorldUL;
		vm->GetAttachment( nScreenAttachIdxUL, vecWorldUL );
		Vector vecScreenUL;
		ScreenTransform( vecWorldUL, vecScreenUL );
		g_TabletScreenSpace_UL.x = vecScreenUL.x;
		g_TabletScreenSpace_UL.y = vecScreenUL.y;

		//Vector vecWorldLL;
		//vm->GetAttachment( nScreenAttachIdxLL, vecWorldLL );
		//Vector vecScreenLL;
		//ScreenTransform( vecWorldLL, vecScreenLL );
		//g_TabletScreenSpace_LL.x = vecScreenLL.x;
		//g_TabletScreenSpace_LL.y = vecScreenLL.y;

		Vector vecWorldLR;
		vm->GetAttachment( nScreenAttachIdxLR, vecWorldLR );
		Vector vecScreenLR;
		ScreenTransform( vecWorldLR, vecScreenLR );
		g_TabletScreenSpace_LR.x = vecScreenLR.x;
		g_TabletScreenSpace_LR.y = vecScreenLR.y;
	}
	else
	{
		Assert( false );
	}

	UpdatePoseParameter();
	
#ifdef PANORAMA_ENABLE
	panorama::DispatchEvent( SurvivalBuyMenuUpdate(), (panorama::IUIPanelClient*)nullptr );
#endif

}

#endif

void CTablet::ItemBusyFrame()
{
	if ( m_flBootTime == 0 )
	{
		m_flBootTime = gpGlobals->curtime;

		// tablet boots instantly outside of warmup
		if ( CSGameRules() && !CSGameRules()->IsWarmupPeriod() )
		{
			m_flBootTime = gpGlobals->curtime - k_flTabletBootDuration;
		}
	}

	BaseClass::ItemBusyFrame();
}

void CTablet::UpdateShieldState()
{
#ifndef CLIENT_DLL
	UpdatePlayerOccupiedGrid();

	CheckForIncomingDrones();

	UpdateTabletNotifications();

	if ( m_bPendingBuyMenu && gpGlobals->curtime - m_flBootTime > k_flTabletBootDuration )
	{
		m_bPendingBuyMenu = false;
		SetTabletSkinState( TABLET_SKIN_STATE_BUYMENU );
	}

	// close the buymenu when a player is frozen at the end of warmup (the panorama buymenu doesn't sort well with the warmup-end screen transitions)
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer && pPlayer->IsAlive() && (pPlayer->GetFlags() & FL_FROZEN) && GetTabletSkinState() == TABLET_SKIN_STATE_BUYMENU )
	{
		SetTabletSkinState( TABLET_SKIN_STATE_RADAR );
	}

#endif
}

void CTablet::WeaponIdle()
{
#ifdef CLIENT_DLL
	// hack - slam the weapon world model skin here
	CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
	if ( pWeaponWorldModel )
	{
		if ( m_nSkin % 3 == 1 )
		{
			pWeaponWorldModel->m_nSkin = m_nSkin - 1;
		}
		else
		{
			pWeaponWorldModel->m_nSkin = m_nSkin;
		}
	}
#endif

	if ( m_flTimeWeaponIdle > gpGlobals->curtime )
		return;

	if ( IsViewModelSequenceFinished() )
		SendWeaponAnim( ACT_VM_IDLE );

	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
}

#ifndef CLIENT_DLL

void CTablet::UpdatePlayerPositionHistory( void )
{
	// note: the player position history is stored as 2d coords in vectors as X, Y, timestamp.
	// the position history is not in temporal order, to prevent churning the network vars.
	// maybe this is a needless optimization?

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;
	
	Vector2D vecPlayerPos = pPlayer->GetAbsOrigin().AsVector2D();

	if ( !m_vecLastPlayerPosition.IsValid() || m_vecLastPlayerPosition.DistTo( vecPlayerPos ) > 600.0f )
	{
		// we are far enough from the last known position to create a new log entry.
		// find the oldest log entry to replace, or better yet an unused one.
		
		int nNewEntryIdx = -1;
		float flEarliestTimestamp = gpGlobals->curtime;

		for ( int i = 0; i < TABLET_PLAYER_POSITION_HISTORY_COUNT; i++ )
		{
			Vector vecEntry = m_vecPlayerPositionHistory.Get( i );
			if ( vecEntry.z < flEarliestTimestamp )
			{
				flEarliestTimestamp = vecEntry.z;
				nNewEntryIdx = i;
			}
		}

		if ( nNewEntryIdx >= 0 && nNewEntryIdx < TABLET_PLAYER_POSITION_HISTORY_COUNT )
		{
			m_vecLastPlayerPosition = vecPlayerPos;
			m_vecPlayerPositionHistory.Set( nNewEntryIdx, Vector( vecPlayerPos.x, vecPlayerPos.y, gpGlobals->curtime ) );
		}
		else
		{
			Assert( false );
		}
	}
}

void CTablet::CheckForIncomingDrones( void )
{
	if ( m_flNextCheckForIncomingDronesTime > gpGlobals->curtime )
		return;
	m_flNextCheckForIncomingDronesTime = gpGlobals->curtime + 0.5f;

	FOR_EACH_VEC_BACK( IDrone::AutoList(), nDrone )
	{
		CDrone *pDrone = static_cast<CDrone*>(IDrone::AutoList()[nDrone]);
		if ( pDrone && pDrone->GetMoveToEnt() == this )
		{
			PushTabletNotification( TABLET_NOTIFICATION_INCOMING_DELIVERY );
			break;
		}
	}
}

void CTablet::SetTabletUpgrade( tablet_upgrade_type_t nUpgrade, bool bDoAnim )
{
	//AssertMsg( !HasTabletUpgrade( nUpgrade ), "Already has this upgrade!" );

	m_flUpgradeExpirationTime.Set( (int)nUpgrade, -1.0f );

	//reboot the tablet
	m_flBootTime = gpGlobals->curtime + (bDoAnim ? 1.3f : 0.0f);

	ResetLocalHexes(); // let the hexes repopulate

	// we use ACT_VM_RECOIL1 + upgrade offset for equipping upgrade anim
	if ( bDoAnim )
	{
		SendWeaponAnim( ACT_VM_RECOIL1 + nUpgrade );
		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
	}

	// set dropped highlight color using the most recently applied upgrade. FIXME: how to color outline when multiple upgrades are applied?
	CBaseAnimating *pEntAnim = GetBaseAnimating();
	if ( pEntAnim )
	{
		if ( nUpgrade == TABLET_UPGRADE_HIGHRES )
		{
			pEntAnim->SetHighlightColor( 255, 230, 25 ); // yellow
			PushTabletNotification( TABLET_NOTIFICATION_UPGRADE_HIGHRES );
		}
		else if ( nUpgrade == TABLET_UPGRADE_DRONEINTEL )
		{
			pEntAnim->SetHighlightColor( 60, 215, 255 ); // blue
			PushTabletNotification( TABLET_NOTIFICATION_UPGRADE_DRONEINTEL );
		}
		else if ( nUpgrade == TABLET_UPGRADE_ZONEINTEL )
		{
			pEntAnim->SetHighlightColor( 245, 72, 67 ); // reddish
			PushTabletNotification( TABLET_NOTIFICATION_UPGRADE_ZONEINTEL );
		}
		else
		{
			AssertMsg( false, "No highlight color defined for this upgrade type!" );
		}
	}
}

void CTablet::SetTabletSkinState( tablet_skin_state_t nNewState )
{
	int nSkin = 0 + (int)nNewState;

	CBaseViewModel *vm = GetViewModelPtr();
	if ( vm )
	{
		vm->m_nSkin = nSkin;
	}

	CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
	if ( pWeaponWorldModel )
	{
		pWeaponWorldModel->m_nSkin = nSkin; // hopefully the world model skin indices are the same
	}

	m_nSkin = nSkin;

	if ( nNewState == TABLET_SKIN_STATE_RADAR )
	{
		if ( m_flShowMapTime == 0 )
		{
			// this is the first time the map is shown for this tablet. Show helpful hint.
			PushTabletNotification( TABLET_NOTIFICATION_HIGHLIGHTED_SECTORS_CONTAIN_PLAYERS, k_flTabletBootDuration );
		}

		m_flShowMapTime = gpGlobals->curtime;
	}
}

void CTablet::TabletClientCommand( const char * szCmd )
{
	if ( !GetPlayerOwner() || !GetPlayerOwner()->IsAlive() )
		return;
	
	CBaseViewModel *vm = GetViewModelPtr();
	if ( !vm )
		return;

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();

	if ( !V_strcmp( szCmd, "tabletbuy_close" ) )
	{
		SetTabletSkinState( TABLET_SKIN_STATE_RADAR );
	}
	else if ( !V_strcmp( szCmd, "tabletbuy_open" ) )
	{
		if ( gpGlobals->curtime - m_flBootTime < k_flTabletBootDuration )
		{
			// don't interrupt boot up
			m_bPendingBuyMenu = true;
			return;
		}

		SetTabletSkinState( TABLET_SKIN_STATE_BUYMENU );
	}
	else if ( GetTabletSkinState() == TABLET_SKIN_STATE_BUYMENU )
	{
		if ( pBRrules && V_stristr( szCmd, "tabletbuy_buy_" ) )
		{
			pBRrules->TabletPurchase( GetPlayerOwner(), szCmd );
		}
	}
}

void CTablet::ResetLocalHexes()
{
	for ( int i=0; i<m_vecLocalHexFlags.Count(); ++i )
	{
		m_vecLocalHexFlags.Set( i, 0 );
	}
}

void CTablet::SetHexFlag( int nHex, int hexFlag )
{
	m_vecLocalHexFlags.Set( nHex, m_vecLocalHexFlags.Get( nHex ) | hexFlag );
}

void CTablet::ClearContractKill()
{
	m_nContractKillGridIndex = -1;
	m_nContractKillGridHighResIndex = -1;
}

void CTablet::UpdateTabletNotifications( void )
{
	// re-push upgrade notifications
	for ( int i = 0; i < TABLET_UPGRADE_COUNT; i++ )
	{
		tablet_upgrade_type_t nUpgrade = (tablet_upgrade_type_t)i;
		if ( HasTabletUpgrade( nUpgrade ) )
		{
			if ( nUpgrade == TABLET_UPGRADE_HIGHRES )
			{
				PushTabletNotification( TABLET_NOTIFICATION_UPGRADE_HIGHRES );
			}
			else if ( nUpgrade == TABLET_UPGRADE_DRONEINTEL )
			{
				PushTabletNotification( TABLET_NOTIFICATION_UPGRADE_DRONEINTEL );
			}
			else if ( nUpgrade == TABLET_UPGRADE_ZONEINTEL )
			{
				PushTabletNotification( TABLET_NOTIFICATION_UPGRADE_ZONEINTEL );
			}
		}
	}

	// clear notifications that have expired
	for ( int i = 0; i < TABLET_NOTIFICATION_ARRAYSIZE; i++ )
	{
		tablet_notification_t nTemp = (tablet_notification_t)m_vecNotificationIds.Get( i );
		if ( nTemp != TABLET_NOTIFICATION_UNSET )
		{
			const staticTabletNotificationData_t *pNoteData = GetTabletNotificationData( (int)nTemp );
			if ( pNoteData && pNoteData->m_flDurationToLinger == 0.0f )
			{
				continue; // this notification never dismisses
			}
			else if ( !pNoteData || gpGlobals->curtime > m_vecNotificationTimestamps.Get( i ) + pNoteData->m_flDurationToLinger + k_flNotificationTransitionDuration )
			{
				m_vecNotificationIds.Set( i, TABLET_NOTIFICATION_UNSET );
				m_vecNotificationTimestamps.Set( i, 0 );
			}
		}
	}

	// bubble up notifications into empty slots. This only does one bubble pass because this code will run again pretty soon and bubble over and over
	for ( int i = 0; i < TABLET_NOTIFICATION_ARRAYSIZE - 1; i++ )
	{
		tablet_notification_t nA = (tablet_notification_t)m_vecNotificationIds.Get( i );
		tablet_notification_t nB = (tablet_notification_t)m_vecNotificationIds.Get( i + 1 );

		if ( nA == TABLET_NOTIFICATION_UNSET && nB != TABLET_NOTIFICATION_UNSET )
		{
			m_vecNotificationIds.Set( i, nB );
			m_vecNotificationTimestamps.Set( i, m_vecNotificationTimestamps.Get( i + 1 ) );
			m_vecNotificationIds.Set( i+1, TABLET_NOTIFICATION_UNSET );
			m_vecNotificationTimestamps.Set( i + 1, 0 );
		}
	}
}

void CTablet::PushTabletNotification( tablet_notification_t nNotificationId, float flAddTimeWhenNew /* = 0.0f */ )
{
	// need to push the notification as well as clear any duplicates
	bool bAdded = false;

	// find free or existing index
	for ( int i = 0; i < TABLET_NOTIFICATION_ARRAYSIZE; i++ )
	{
		tablet_notification_t nTemp = (tablet_notification_t)m_vecNotificationIds.Get( i );
		if ( !bAdded )
		{
			if ( nTemp == TABLET_NOTIFICATION_UNSET || nTemp == nNotificationId )
			{
				m_vecNotificationIds.Set( i, (int)nNotificationId );
				bAdded = true;

				if ( nTemp == TABLET_NOTIFICATION_UNSET )
				{
					// set start time to now
					m_vecNotificationTimestamps.Set( i, gpGlobals->curtime + flAddTimeWhenNew );
				}
				else
				{
					// we don't want to completely reset the timestamp on an existing notification.
					// If we do that, it'll reset its intro animation. Instead we want to reset it to
					// a reasonable time AFTER that, but only if it's younger than the trans duration.
					if ( gpGlobals->curtime - m_vecNotificationTimestamps.Get( i ) > k_flNotificationTransitionDuration )
					{
						m_vecNotificationTimestamps.Set( i, gpGlobals->curtime - k_flNotificationTransitionDuration - 0.1f );
					}
				}
			}
		}
		else if ( nTemp == nNotificationId )
		{
			// why is there a duplicate notification? This might be 100% fine, and this assert unnecessary.
			Assert( false );
			m_vecNotificationIds.Set( i, TABLET_NOTIFICATION_UNSET );
		}
	}
}

#endif

tablet_skin_state_t CTablet::GetTabletSkinState( void ) const
{
	// assume the skin state of the viewmodel is authoritative

	if ( GetPlayerOwner() )
	{
		MDLCACHE_CRITICAL_SECTION();

		CBaseViewModel *vm = GetViewModelPtr();
		if ( vm )
			return (tablet_skin_state_t)(vm->GetSkin() % 3);
	}

	// dropped tablets are blank
	return TABLET_SKIN_STATE_BLANK;
}

bool CTablet::HexHasFlag( int nHex, int hexFlag )
{
	return (m_vecLocalHexFlags.Get( nHex ) & hexFlag) != 0;
}

#ifdef CLIENT_DLL
float CTablet::GetTimeOfLastHexChange( int nHex, int nSubRegion )
{
	return m_vecLastHexPlayerOccupancyChange[nHex][nSubRegion];
}

void CTablet::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// compare client copy of hex flags to updated hex flags in order to keep time of last hex change up to date
	for ( int i = 0; i < TABLET_HEX_COUNT; i++ )
	{
		int nHexFlags = m_vecLocalHexFlags.Get( i );
		if ( m_vecLocalHexFlagsClientCopy[i] != nHexFlags )
		{
			// what's different?
			for ( int nHighResSubSector = 0; nHighResSubSector < 6; nHighResSubSector++ )
			{
				int nHexFlagForSubSector = 1 << nHighResSubSector;

				bool bClientHexCopyHasFlag = (m_vecLocalHexFlagsClientCopy[i] & nHexFlagForSubSector) != 0;

				if ( HexHasFlag( i, nHexFlagForSubSector ) != bClientHexCopyHasFlag )
				{
					m_vecLastHexPlayerOccupancyChange[i][nHighResSubSector] = gpGlobals->curtime;
				}
			}
			m_vecLocalHexFlagsClientCopy[i] = nHexFlags;
		}
	}
}


int CTablet::KeyInput( int down, ButtonCode_t keynum, const char * pszCurrentBinding )
{
	if ( !GetPlayerOwner() )
		return 1;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( engine->IsHLTV() || ( pLocalPlayer && pLocalPlayer->IsSpectator() ) )
		return 1;
	
	bool bMenuOpen = GetTabletSkinState() == TABLET_SKIN_STATE_BUYMENU;

	if ( keynum == KEY_ESCAPE )
	{
		if ( bMenuOpen )
		{
			engine->ServerCmd( "tabletbuy_close" );
			return 0;
		}
		else if ( down )
		{
			engine->ExecuteClientCmd( "lastinv" );
			return 0;
		}
	}
	else if ( pszCurrentBinding && !V_strcmp( pszCurrentBinding, "buymenu" ) )
	{
		if ( down )
		{
			if ( !bMenuOpen )
			{
				engine->ServerCmd( "tabletbuy_open" );
			}
			else
			{
				engine->ServerCmd( "tabletbuy_close" );
			}
		}
		return 0;
	}

	return 1;
}

IMaterial *CTablet::GetTabletMaterial( tabletmaterialindex_t idx )
{
	IMaterial *pMat = m_vecTabletMaterials[(int)idx];

	if ( pMat != NULL && !pMat->IsErrorMaterial() )
	{
		return pMat;
	}

	#define REGISTER_TABLET_MATERIAL( _n ) if ( idx == _n ) { pMat = materials->FindMaterial( "models/weapons/v_models/tablet/" #_n , TEXTURE_GROUP_OTHER, false ); Assert(pMat); }
	#include "weapon_tablet_materials.inc"
	#undef REGISTER_TABLET_MATERIAL

	if ( pMat != NULL && !pMat->IsErrorMaterial() )
	{
		m_vecTabletMaterials[(int)idx] = pMat;
		return pMat;
	}

	return NULL;
}

#ifdef DEBUG
ConVar cl_tablet_camera_fly( "cl_tablet_camera_fly", "0" );
#endif

DEVELOPMENT_ONLY_CONVAR( dev_tablet_debug_bomb_wave, 0 );

void CTablet::RenderTablet( void )
{
	// offsets inside grid cell for each high-res sub-cell for placing high-res icons
	static Vector s_vecHighResOffset[NUM_HIGHRES_DIRECTION] =
	{
		Vector( 1.f,		0.f,		0.f ),
		Vector( 0.5f,		0.866f,		0.f ),
		Vector( -0.5f,		0.866f,		0.f ),
		Vector( -1.f,		0.f,		0.f ),
		Vector( -0.5f,		-0.866f,	0.f ),
		Vector( 0.5f,		-0.866f,	0.f ),
	};

	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )
		return;

	C_CSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	ITexture *pRtOutput = materials->FindTexture( "_rt_Tablet", TEXTURE_GROUP_RENDER_TARGET );
	if ( !pRtOutput )
	{
		Assert( false );
		return;
	}
	m_nRenderTargetRes = pRtOutput->GetActualWidth();

	float flTimeSinceBoot = gpGlobals->curtime - m_flBootTime;

	float flDeployLerp = 1.0f;
	CBaseViewModel *vm = GetViewModelPtr();
	if ( vm && vm->GetSequence() == 0 ) // deploy
	{
		flDeployLerp = Gain( vm->GetCycle(), 0.7f );
	}

	m_nDrawElementCount = 0;

	if ( m_bTabletReceptionIsBlocked )
	{
		m_flNoiseFadeAlpha = Approach( 1, m_flNoiseFadeAlpha, gpGlobals->frametime * 2.0f );
	}
	else
	{
		m_flNoiseFadeAlpha = Approach( 0, m_flNoiseFadeAlpha, gpGlobals->frametime * 4.0f );
	}

	IClientRenderable *pTabletRenderable = pPlayer->GetViewModel() ? pPlayer->GetViewModel()->GetClientRenderable() : NULL;

	Vector vecPlayerPos = pPlayer->GetAbsOrigin();

	CMatRenderContextPtr pRenderContext( materials );

	m_WorkingColor.SetColor( 255, 255, 255, 255 );

	Camera_t m_Camera;
	m_Camera.Init( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 1.0f, 100000.0f, 45.0f, 1.0f );

	Vector vecCamLookAt = vec3_origin;
	Vector vecCamPos = Vector( 0, -20000, 24000 );

	int nTabletMode = cl_tablet_mapmode.GetInt() % 3;
	if ( nTabletMode != 2 )
	{
		VectorRotate( vecCamPos, QAngle( 0, pPlayer->EyeAngles()[YAW] - 90.0f, 0 ), vecCamPos );
	}

	if ( nTabletMode == 0 )
	{
		vecCamLookAt.z += 3000;
		vecCamLookAt.x += vecPlayerPos.x;
		vecCamLookAt.y += vecPlayerPos.y;
		vecCamPos.x += vecPlayerPos.x;
		vecCamPos.y += vecPlayerPos.y;
		vecCamPos *= 0.85f;
	}
	else if ( nTabletMode == 1 )
	{
		vecCamLookAt.z -= 3000;
	}
	else if ( nTabletMode == 2 )
	{
		vecCamLookAt.z -= 2000;
		vecCamPos *= 0.85f;
	}

	if ( flDeployLerp < 1.0f )
	{
		Vector vecEyePos = pPlayer->EyePosition();
		Vector vecEyeForward;
		pPlayer->EyeVectors( &vecEyeForward );
		vecCamPos = Lerp( flDeployLerp, vecEyePos, vecCamPos );
		vecCamLookAt = Lerp( flDeployLerp, vecEyePos + vecEyeForward * 2000, vecCamLookAt );
	}

	if ( flTimeSinceBoot < 1.5f * k_flTabletBootDuration )
	{
		float flBootCameraDist = RemapValClamped( flTimeSinceBoot, 1.0f * k_flTabletBootDuration, 1.5f * k_flTabletBootDuration, 3.0f, 1.0f );
		vecCamPos *= flBootCameraDist;
	}

	QAngle angCamAng;
	VectorAngles( (vecCamLookAt - vecCamPos).Normalized(), angCamAng );

#ifdef DEBUG
	if ( cl_tablet_camera_fly.GetBool() )
	{
		vecCamPos = pPlayer->EyePosition();
		angCamAng = pPlayer->EyeAngles();
	}
#endif

	if ( m_flNoiseFadeAlpha < 0.1f )
	{
		m_vecLastCameraPos = vecCamPos;
		m_angLastCameraAng = angCamAng;
	}
	else
	{
		m_vecLastCameraPos = Lerp( gpGlobals->frametime, m_vecLastCameraPos, vecCamPos );
		Vector vecTemp;
		AngleVectors( m_angLastCameraAng, &vecTemp );
		vecTemp = Lerp( gpGlobals->frametime, vecTemp, (vecCamLookAt - vecCamPos).Normalized() );
		VectorAngles( vecTemp, m_angLastCameraAng );
	}
	m_Camera.InitViewParameters( m_vecLastCameraPos, m_angLastCameraAng );
	
	pRenderContext->PushRenderTargetAndViewport( pRtOutput, 0, 0, m_nRenderTargetRes, m_nRenderTargetRes );
	
	VMatrix view, projection;
	ComputeViewMatrix( &view, m_Camera );
	ComputeProjectionMatrix( &projection, m_Camera, m_nRenderTargetRes, m_nRenderTargetRes );
	
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( view );
	
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( projection );
	
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->ClearBuffers( true, true, true );

	pRenderContext->OverrideDepthEnable( true, false, false );
	
	static const float sMapDim = 20480.0f;

	// create disabled stencil state
	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;

	m_WorkingColor.SetColor( 255, 255, 255, 255 );
	pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_map_bg ), 0, 0, 1, 1, 0, 0, 1, 1, pTabletRenderable );

	m_WorkingColor.SetColor( 100, 100, 100, 255 );
	RenderMapQuad( GetTabletMaterial( tablet_radar ), 0, 0, sMapDim, sMapDim, -200, 0, 0, 1, 1, pTabletRenderable );

	ShaderStencilState_t stencilStateDrawMapQuad;
	stencilStateDrawMapQuad.m_bEnable = true;
	stencilStateDrawMapQuad.m_nReferenceValue = 1;
	stencilStateDrawMapQuad.m_nTestMask = 0xFF;
	stencilStateDrawMapQuad.m_nWriteMask = 0xFF;
	stencilStateDrawMapQuad.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
	stencilStateDrawMapQuad.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	stencilStateDrawMapQuad.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilStateDrawMapQuad.m_ZFailOp = SHADER_STENCILOP_KEEP;
	pRenderContext->SetStencilState( stencilStateDrawMapQuad );

	m_WorkingColor.SetColor( 255, 255, 255, 255 );
	RenderMapQuad( GetTabletMaterial( tablet_radar ), 0, 0, sMapDim, sMapDim, 0, 0, 0, 1, 1, pTabletRenderable );

	m_WorkingColor.SetColor( 0, 0, 0, 255 );
	RenderMapQuad( GetTabletMaterial( tablet_radar_buildings ), 0, 0, sMapDim, sMapDim, 0, 0, 0, 1, 1, pTabletRenderable );

	m_WorkingColor.SetColor( 255, 255, 255, 255 );
	RenderMapQuad( GetTabletMaterial( tablet_radar_buildings ), 0, 0, sMapDim, sMapDim, 120, 0, 0, 1, 1, pTabletRenderable );

	ShaderStencilState_t stencilStateMaskToMapQuad;
	stencilStateMaskToMapQuad.m_bEnable = true;
	stencilStateMaskToMapQuad.m_nWriteMask = 0; // We're not changing stencil
	stencilStateMaskToMapQuad.m_nTestMask = 0xFF;
	stencilStateMaskToMapQuad.m_nReferenceValue = 1;
	stencilStateMaskToMapQuad.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
	stencilStateMaskToMapQuad.m_PassOp = SHADER_STENCILOP_KEEP;
	stencilStateMaskToMapQuad.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilStateMaskToMapQuad.m_ZFailOp = SHADER_STENCILOP_KEEP;
	pRenderContext->SetStencilState( stencilStateMaskToMapQuad );

	float flHexSize = GetTabletHexSize();

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( !pBRrules )
		return;

	m_WorkingColor.SetColor( 255, 255, 255, 255 );

	// figure out paradrop crate locations
	COMPILE_TIME_ASSERT( NUM_HIGHRES_DIRECTION <= 8 ); // bits in byte
	uint8 nParadropCells[TABLET_HEX_COUNT];
	memset( nParadropCells, 0, sizeof( nParadropCells ) );
	const auto& vecParadropCrates = ITabletRenderedLootCrate::AutoList();
	FOR_EACH_VEC( vecParadropCrates, iCrate )
	{
		C_PhysPropLootCrate* pLootCrate = static_cast< C_PhysPropLootCrate* >( vecParadropCrates[iCrate] );

		if ( !pLootCrate || !pLootCrate->m_bRenderInTablet )
			continue;

		int nHighresPos = 0;
		int nTabletCell = pBRrules->FindGridCenterIndexClosestToWorldPos( pLootCrate->GetAbsOrigin(), &nHighresPos );
		if ( nTabletCell < 0 || nTabletCell >= TABLET_HEX_COUNT )
			continue;

		// clamp for safety
		if ( nHighresPos < 0 || nHighresPos >= NUM_HIGHRES_DIRECTION )
			nHighresPos = 0;

		nParadropCells[nTabletCell] |= ( 1 << nHighresPos );
	}

	// draw the map grid squares
	if ( m_flNoiseFadeAlpha == 0 )
	{
		pRenderContext->SetStencilState( stencilStateMaskToMapQuad );

		bool bHighRes = HasTabletUpgrade( TABLET_UPGRADE_HIGHRES );

		for ( int nIdx = 0; nIdx < TABLET_HEX_COUNT; nIdx++ )
		{
			const Vector2D& vPos = pBRrules->GetHexCenter( nIdx );
			RenderMapQuad( GetTabletMaterial( bHighRes ? tablet_hr_grid_border : tablet_grid_border ), vPos.x, vPos.y, flHexSize, flHexSize, 0, 0, 0, 1, 1, pTabletRenderable );
		}

		pRenderContext->SetStencilState( stencilStateDisable );

		for ( int nIdx = 0; nIdx < TABLET_HEX_COUNT; nIdx++ )
		{
			const Vector2D& vPos = pBRrules->GetHexCenter( nIdx );

			if ( HexHasFlag( nIdx, HEX_FL_JAMMED ) && !HexHasFlag( nIdx, HEX_FL_JAMMED_BY_OWNER ) )
			{
				//RenderMapQuad( GetTabletMaterial( tablet_grid_jammed ), vPos.x, vPos.y, flHexSize, flHexSize, 0, 0, 0, 1, 1, pTabletRenderable );
				continue;
			}

			if ( bHighRes )
			{
				Vector vecTemp = Vector( vPos.x, vPos.y, 0 );

				int nAngle = -30;
				for ( int nHighResSubSector = 0; nHighResSubSector < NUM_HIGHRES_DIRECTION; nHighResSubSector++ )
				{
					float flLastChange = GetTimeOfLastHexChange( nIdx, nHighResSubSector );
					bool bRecent = gpGlobals->curtime - flLastChange < 0.3f;
					bool bDraw = bRecent || HexHasFlag( nIdx, 1 << nHighResSubSector );

					if ( bDraw )
					{
						m_WorkingColor.SetColor( 255, 255, 255, 255 );
						if ( bRecent )
							m_WorkingColor.SetColor( 255, 255, 255, RemapValClamped( sin( gpGlobals->curtime * 60.0f ), -1.0f, 1.0f, 0.2f, 1.0f ) * 255 );

						Vector vecDir;
						AngleVectors( QAngle( 0, nAngle, 0 ), &vecDir );
						RenderMapQuadDir( GetTabletMaterial( tablet_hr_grid_occupied ), vecTemp, Vector( 0, 0, 1 ), vecDir, flHexSize, flHexSize, 0, 0, 1, 1, pTabletRenderable );
						m_WorkingColor.SetColor( 255, 255, 255, 255 );
					}
					nAngle += 60.0f;
				}
			}
			else
			{
				float flLastChange = GetTimeOfLastHexChange( nIdx, 0 );
				bool bRecent = gpGlobals->curtime - flLastChange < 0.3f;
				bool bDraw = bRecent || HexHasFlag( nIdx, HEX_FL_OCCUPIED_0 );

				if ( bDraw )
				{
					m_WorkingColor.SetColor( 255, 255, 255, 255 );
					if ( bRecent )
						m_WorkingColor.SetColor( 255, 255, 255, RemapValClamped( sin( gpGlobals->curtime * 60.0f ), -1.0f, 1.0f, 0.2f, 1.0f ) * 255 );

					RenderMapQuad( GetTabletMaterial( tablet_grid_occupied ), vPos.x, vPos.y, flHexSize, flHexSize, 0, 0, 0, 1, 1, pTabletRenderable );
					m_WorkingColor.SetColor( 255, 255, 255, 255 );
				}
			}

			if ( m_nContractKillGridIndex == nIdx )
			{
				float flNormalSize = 3200.f;
				float flOffsetScale = 0.f;
				if ( bHighRes )
				{
					flNormalSize = 2000.f;
					flOffsetScale = 0.3f * flHexSize;
				}
				RenderMapBillboard( GetTabletMaterial( tablet_grid_contractkill ), Vector( vPos.x, vPos.y, 0 ) + flOffsetScale * s_vecHighResOffset[ m_nContractKillGridHighResIndex ], flNormalSize, flNormalSize, 0, 0, 1, 1, pTabletRenderable );
			}

			if ( nParadropCells[nIdx] != 0 )
			{
				if ( bHighRes )
				{
					uint8 nHighresBits = nParadropCells[nIdx];
					for ( int iDir = 0; iDir < NUM_HIGHRES_DIRECTION; ++iDir )
					{
						if ( nHighresBits & ( 1 << iDir ) )
						{
							const float kOffsetScale = 0.3f * flHexSize;
							const float kSize = 2000.0f;
							RenderMapBillboard( GetTabletMaterial( tablet_grid_paradrop ), Vector( vPos.x, vPos.y, 0 ) + kOffsetScale * s_vecHighResOffset[iDir], kSize, kSize, 0, 0, 1, 1, pTabletRenderable );
						}
					}
				}
				else
				{
					const float kSize = 3200.0f;
					RenderMapBillboard( GetTabletMaterial( tablet_grid_paradrop ), Vector( vPos.x, vPos.y, 0 ), kSize, kSize, 0, 0, 1, 1, pTabletRenderable );
				}
			}

			if ( HexHasFlag( nIdx, HEX_FL_JAMMED_BY_OWNER ) )
			{
				RenderMapQuad( GetTabletMaterial( tablet_grid_jammed ), vPos.x, vPos.y, flHexSize, flHexSize, 0, 0, 0, 1, 1, pTabletRenderable );
			}

		}

		m_WorkingColor.SetColor( 255, 255, 255, 255 );
		//RenderMapQuad( GetTabletMaterial( tablet_radar_buildings ), 0, 0, sMapDim, sMapDim, 120, 0, 0, 1, 1, pTabletRenderable );

		RenderPlayerPositionHistory( pTabletRenderable );

		////show money icons in unexplored hexes
		//for ( int nIdx = 0; nIdx < GetNumHexes(); nIdx++ )
		//{
		//	if ( !HexHasFlag( nIdx, HEX_FL_EXPLORED, HEX_PL_FULL ) )
		//	{
		//		const Vector2D& vPos = pBRrules->GetHexCenter( nIdx );
		//		float flDist = vPos.DistTo( vecPlayerPos.AsVector2D() );
		//		if ( flDist < 6000.0f )
		//		{
		//			IMaterial *pMat = GetTabletMaterial( tablet_scan_map );
		//			IMaterialVar* pVar = pMat->FindVar( "$alpha", NULL );
		//			pVar->SetFloatValue( RemapValClamped( flDist, 3000.0f, 6000.0f, 1.0f, 0.0f ) );
		//
		//			RenderMapBillboard( pMat, Vector( vPos.x, vPos.y, 0 ), 2700, 2700, 0, 0, 1, 1, pTabletRenderable );
		//		}
		//	}
		//}

		if ( HasTabletUpgrade( TABLET_UPGRADE_ZONEINTEL ) )
		{
			IMaterial *pMat = GetTabletMaterial( tablet_zone_overlay_prediction );
			IMaterialVar* pVar = pMat->FindVar( "$c0_y", NULL );
			pVar->SetFloatValue( RemapValClamped( fmod( gpGlobals->curtime, 4.0f ), 0.0f, 4.0f, -1.0f, 2.0f ) );
			RenderMapQuad( pMat, 0, 0, 20480.0f, 20480.0f, 0, 0, 0, 1, 1, pTabletRenderable );
		}
		else
		{
			IMaterial *pMat = GetTabletMaterial( tablet_zone_overlay );
			IMaterialVar* pVar = pMat->FindVar( "$c0_y", NULL );
			pVar->SetFloatValue( RemapValClamped( fmod( gpGlobals->curtime, 4.0f ), 0.0f, 4.0f, -1.0f, 2.0f ) );
			RenderMapQuad( pMat, 0, 0, 20480.0f, 20480.0f, 0, 0, 0, 1, 1, pTabletRenderable );
		}

		for ( int nIdx = 0; nIdx < TABLET_HEX_COUNT; nIdx++ )
		{
			const Vector2D& vPos = pBRrules->GetHexCenter( nIdx );
			if ( HexHasFlag( nIdx, HEX_FL_JAMMED ) && !HexHasFlag( nIdx, HEX_FL_JAMMED_BY_OWNER ) )
			{
				RenderMapQuad( GetTabletMaterial( tablet_grid_jammed ), vPos.x, vPos.y, flHexSize, flHexSize, 0, 0, 0, 1, 1, pTabletRenderable );
			}
		}

	}
	//else
	//{
	//	m_WorkingColor.SetColor( 255, 255, 255, 255 );
	//	RenderMapQuad( GetTabletMaterial( tablet_radar_buildings ), 0, 0, sMapDim, sMapDim, 120, 0, 0, 1, 1, pTabletRenderable );
	//}

	pRenderContext->SetStencilState( stencilStateMaskToMapQuad );

	RenderMapQuad( GetTabletMaterial( tablet_pulse ), 0, 0, sMapDim, sMapDim, 0, 0, 0, 1, 1, pTabletRenderable );
	

	pRenderContext->SetStencilState( stencilStateDisable );

	auto lambdaPulse = [&]() -> float
	{
		return 0.5 * ( 1.f + sin( 2.f * M_PI * gpGlobals->curtime ) );
	};

	CDangerZoneController *pZoneController = GetDangerZoneController();

	// will replace by drawing the hexes as red when they are no go zones
	if ( m_flNoiseFadeAlpha == 0 && pZoneController && pZoneController->IsMasterDangerZoneEnabled() )
	{
		// draw bomb initial explosion icons
		{
			int nDangerZone = pZoneController->GetDangerZoneCount();
			for ( int iZone = 0; iZone < nDangerZone; ++iZone )
			{
				CDangerZone *pZone = pZoneController->GetDangerZone( iZone );
				float flRadius = pZone->GetDangerZoneRadius();
				if ( flRadius > 0 && flRadius < 50.0f )
				{
					Vector vecPos = pZone->GetDangerZoneOrigin();
					IMaterial *pMat = GetTabletMaterial( tablet_alert );
					if ( pMat )
					{
						IMaterialVar* pVar = pMat->FindVar( "$frame", NULL );
						if ( pVar )
						{
							pVar->SetIntValue( (int)RemapValClamped( flRadius, 0.0f, 50.0f, 0.0f, 11.99f ) );
						}
						//float flSize = RemapValClamped( flRadius, 0.0f, 50.0f, 1200.0f, 2500.0f );
						RenderMapQuad( GetTabletMaterial( tablet_alert ), vecPos.x, vecPos.y, 2000, 2000, 0, 0, 0, 1, 1, pTabletRenderable );
					}
				}
			}
		}
	}

	FOR_EACH_VEC_BACK( g_PlantedC4s, nC4 )
	{
		C_PlantedC4* pC4 = g_PlantedC4s[nC4];
		if ( pC4 )
		{
			Vector vecC4Pos = pC4->GetAbsOrigin();

			if ( pC4->GetMoveParent() ) // hack - this means on a safe
			{
				RenderMapQuad( GetTabletMaterial( tablet_c4_safe ), vecC4Pos.x, vecC4Pos.y, 4000, 4000, 0, 0, 0, 1, 1, pTabletRenderable );
			}
			else
			{
				float flC4dist = pPlayer->GetAbsOrigin().DistTo( pC4->GetAbsOrigin() );
				if ( flC4dist < tablet_c4_dist_max.GetFloat() )
				{
					float flAlpha = RemapValClamped( flC4dist, tablet_c4_dist_min.GetFloat(), tablet_c4_dist_max.GetFloat(), 1.0f, 0.0f );
					if ( flAlpha )
					{
						IMaterial *pMat = GetTabletMaterial( tablet_c4 );
						if ( pMat )
						{
							IMaterialVar* pVar = pMat->FindVar( "$alpha", NULL );
							if ( pVar )
							{
								pVar->SetFloatValue( flAlpha );
							}
						}

						RenderMapQuad( GetTabletMaterial( tablet_c4 ), vecC4Pos.x, vecC4Pos.y, 4000, 4000, 0, 0, 0, 1, 1, pTabletRenderable );
					}
				}
			}
		}
	}

	// draw hostage rescue zone info
	C_CS_PlayerResource *pCSRerouce = GetCSResources();
	if ( m_flNoiseFadeAlpha == 0 && pCSRerouce )
	{
		IMaterial *pMatRescueZone = GetTabletMaterial( tablet_rescue );
		IMaterialVar* pAlphaVar = pMatRescueZone->FindVar( "$color", NULL );
		const float flDisabledAlpha = 0.3f;

		auto lambdaDrawRescueZones = [&]( float flAlpha )
		{
			if ( pAlphaVar )
			{
				pAlphaVar->SetFloatValue( flAlpha );
			}

			for ( int i=0; i<MAX_HOSTAGE_RESCUES; ++i )
			{
				Vector vecRescuePos = pCSRerouce->GetHostageRescuePosition( i );
				if ( vecRescuePos != vec3_origin )
				{
					RenderMapBillboard( pMatRescueZone, Vector( vecRescuePos.x, vecRescuePos.y, 0 ), 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
				}
			}
		};

		if ( pPlayer && pPlayer->m_hCarriedHostage )
		{
			if ( sv_tablet_show_path_to_nearest_resq.GetBool() )
			{
				float flMinDistSqr = FLT_MAX;
				int iClosestRescue = -1;
				bool bInZoneOnly = false;
				for ( int i=0; i<MAX_HOSTAGE_RESCUES; ++i )
				{
					Vector vecRescuePos = pCSRerouce->GetHostageRescuePosition( i );
					if ( vecRescuePos == vec3_origin )
						continue;

					bool bInZone = pZoneController->IsWithinPlayArea( vecRescuePos );
					if ( bInZoneOnly && !bInZone )
						continue;

					float flDistSqr = pPlayer->GetAbsOrigin().DistToSqr( vecRescuePos );
					if ( flDistSqr < flMinDistSqr || bInZoneOnly != bInZone )
					{
						flMinDistSqr = flDistSqr;
						iClosestRescue = i;
						bInZoneOnly |= bInZone;
					}
				}

				if ( iClosestRescue != -1 )
				{
					int nNumDots = sqrtf( flMinDistSqr ) / 600.0f;
					if ( nNumDots > 1 )
					{
						Vector vecClosestRescue = pCSRerouce->GetHostageRescuePosition( iClosestRescue );
						for ( int i = 1; i < nNumDots; i++ )
						{
							float flLerp = (float)i / (float)nNumDots;
							Vector vecDotPos = Lerp( flLerp, pPlayer->GetAbsOrigin(), vecClosestRescue );

							RenderMapQuad( GetTabletMaterial( tablet_hostage_line ), vecDotPos.x, vecDotPos.y, 2000, 2000, 0, 0, 0, 1, 1, pTabletRenderable );
						}
					}
				}
			}

			float flAlpha = flDisabledAlpha + 1.5f * lambdaPulse();
			lambdaDrawRescueZones( flAlpha );
		}
		else
		{
			lambdaDrawRescueZones( flDisabledAlpha );
		}
	}

	bool bHasDroneUpgrade = HasTabletUpgrade( TABLET_UPGRADE_DRONEINTEL );
	FOR_EACH_VEC_BACK( IDrone::AutoList(), nDrone )
	{
		CDrone *pDrone = static_cast< CDrone* >( IDrone::AutoList()[nDrone] );
		if ( pDrone && !pDrone->IsDormant() && ( pDrone->GetMoveToEnt() == this || bHasDroneUpgrade ) )
		{
			const Vector& vecDronePos = pDrone->GetAbsOrigin();
			QAngle angDroneAng = pDrone->GetAbsAngles();

			angDroneAng[YAW] -= fmod( angDroneAng[YAW], 5.0f );

			Vector vecDroneDir;
			AngleVectors( angDroneAng, &vecDroneDir );
			vecDroneDir.z = 0;

			bool bEnemyDrone = pDrone->GetMoveToEnt() != this;
			
			if ( bHasDroneUpgrade )
			{
				RenderDroneTrail( GetTabletMaterial( tablet_drone_line ), pDrone, 0, 900, 900, 0, 0, 1, 1, pTabletRenderable );
			}

			if ( !bEnemyDrone )
			{
				m_WorkingColor.SetColor( 0, 0, 0, 200 );
				RenderMapQuadDir( GetTabletMaterial( tablet_drone ), Vector( vecDronePos.x, vecDronePos.y, 0 ), Vector( 0, 0, 1 ), vecDroneDir, 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
				m_WorkingColor.SetColor( 0, 216, 255, 200 );
				RenderMapQuadDir( GetTabletMaterial( tablet_drone ), Vector( vecDronePos.x, vecDronePos.y, 400 ), Vector( 0, 0, 1 ), vecDroneDir, 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
			}
			else
			{
				m_WorkingColor.SetColor( 0, 0, 0, 200 );
				RenderMapQuadDir( GetTabletMaterial( tablet_drone ), Vector( vecDronePos.x, vecDronePos.y, 0 ), Vector( 0, 0, 1 ), vecDroneDir, 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
				m_WorkingColor.SetColor( 150, 150, 150, 200 );
				RenderMapQuadDir( GetTabletMaterial( tablet_drone ), Vector( vecDronePos.x, vecDronePos.y, 400 ), Vector( 0, 0, 1 ), vecDroneDir, 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
			}
		}
	}

	if ( m_flNoiseFadeAlpha <= 0 )
	{
		if ( pBRrules && pBRrules->IsPlayingTeamMode() )
		{
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				C_CSPlayer *pCSOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
				if ( !pCSOtherPlayer || pPlayer == pCSOtherPlayer )
					continue;

				if ( pCSOtherPlayer->m_nSurvivalTeam == pPlayer->m_nSurvivalTeam )
				{
					if ( pCSOtherPlayer->IsAlive() )
					{
						Vector vecTeammatePos = pCSOtherPlayer->GetAbsOrigin();

						Vector vecTeammateEyeDir;
						AngleVectors( pCSOtherPlayer->EyeAngles(), &vecTeammateEyeDir );
						vecTeammateEyeDir.z = 0;
						m_WorkingColor.SetColor( 0, 0, 0, 200 );
						RenderMapQuadDir( GetTabletMaterial( tablet_player ), Vector( vecTeammatePos.x, vecTeammatePos.y, -100 ), Vector( 0, 0, 1 ), vecTeammateEyeDir, 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
						m_WorkingColor.SetColor( 16, 200, 225, 200 );
						RenderMapQuadDir( GetTabletMaterial( tablet_player ), Vector( vecTeammatePos.x, vecTeammatePos.y, 200 ), Vector( 0, 0, 1 ), vecTeammateEyeDir, 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
					}
					else if ( pCSOtherPlayer->m_hRagdoll.Get() )
					{
						C_CSRagdoll *pRagdoll = (C_CSRagdoll*)pCSOtherPlayer->m_hRagdoll.Get();
						if ( pRagdoll->IsDormant() )
							continue;

						MDLCACHE_CRITICAL_SECTION();
						Vector vecRagdollOrigin = pRagdoll->GetBone( 0 ).GetOrigin();

						RenderMapBillboard( GetTabletMaterial( tablet_ragdoll ), Vector( vecRagdollOrigin.x, vecRagdollOrigin.y, 0 ), 2000, 2000, 0, 0, 1, 1, pTabletRenderable );
					}
				}
			}
		}

		Vector vecPlayerEyeDir;
		AngleVectors( pPlayer->EyeAngles(), &vecPlayerEyeDir );
		vecPlayerEyeDir.z = 0;

		float flTimeSinceMapOpened = (gpGlobals->curtime - m_flShowMapTime);
		float flPlayerIconPulse = 255.0f;
		if ( flTimeSinceMapOpened < 1.0f )
		{
			flPlayerIconPulse = RemapValClamped( sin( gpGlobals->curtime * 40.0f ), -1.0f, 1.0f, 0.2f, 1.0f ) * 255;
		}

		m_WorkingColor.SetColor( 0, 0, 0, 200 );
		RenderMapQuadDir( GetTabletMaterial( tablet_player ), Vector( vecPlayerPos.x, vecPlayerPos.y, -100 ), Vector( 0, 0, 1 ), vecPlayerEyeDir, 1800.0f, 1800.0f, 0, 0, 1, 1, pTabletRenderable );
		m_WorkingColor.SetColor( flPlayerIconPulse, flPlayerIconPulse, flPlayerIconPulse, 200 );
		RenderMapQuadDir( GetTabletMaterial( tablet_player ), Vector( vecPlayerPos.x, vecPlayerPos.y, 200 ), Vector( 0, 0, 1 ), vecPlayerEyeDir, 1800.0f, 1800.0f, 0, 0, 1, 1, pTabletRenderable );

	}

	m_WorkingColor.SetColor( 255, 255, 255, 255 );
	float flCompassRotation = (360.0f - m_angLastCameraAng[YAW]);
	flCompassRotation -= fmodf( flCompassRotation, 3.0f );
	RenderRotatedScreenQuad( GetTabletMaterial( tablet_mini_compass ), tablet_compass_x.GetFloat(), tablet_compass_y.GetFloat(), 0.15f, 0.15f, 0, 0, 1, 1, pTabletRenderable, flCompassRotation );

	pRenderContext->PopRenderTargetAndViewport();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();


	// 2d elements

	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetRenderTarget( pRtOutput );
	pRenderContext->Viewport( 0, 0, m_nRenderTargetRes, m_nRenderTargetRes );

	if ( flTimeSinceBoot > k_flTabletBootDuration )
	{
		RenderNotifications();

		// incoming bomb countdown
		if ( m_flNoiseFadeAlpha == 0 && pZoneController && pZoneController->IsMasterDangerZoneEnabled() )
		{
			float flTimeUntil = pZoneController->GetTimeUntilNextWave();
			if ( flTimeUntil > 0 )
			{
				int nMins = flTimeUntil / 60;
				int nSecs = fmodf( flTimeUntil, 60.0f );

				wchar_t szWideBuff[8];
				g_pVGuiLocalize->ConvertANSIToUnicode( CFmtStr( nSecs >= 10 ? " %i:%i " : " %i:0%i ", nMins, nSecs ).Access(), szWideBuff, sizeof( szWideBuff ) );

				RenderText( szWideBuff, tablet_bomb_timer_x.GetFloat(), 0.415f, 1.2, Color( 0, 0, 0, 255 ), Color( 0, 0, 0, 0 ) ); // dropshadow
				RenderText( szWideBuff, tablet_bomb_timer_x.GetFloat(), 0.42f, 1.2, Color( 190, 23, 23, 255 ), Color( 0, 0, 0, 0 ) ); // red text

				pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_bomb_icon ),
					tablet_bomb_icon_x.GetFloat(), 0.289f,
					0.04f, 0.04f,
					0, 0, 1, 1, pTabletRenderable );
			}
		}
	}

	if ( m_flNoiseFadeAlpha > 0 )
	{
		IMaterial *pMat = GetTabletMaterial( tablet_noise );
		if ( pMat )
		{
			IMaterialVar* pVar = pMat->FindVar( "$alpha", NULL );
			if ( pVar )
			{
				pVar->SetFloatValue( m_flNoiseFadeAlpha );
			}
		}

		pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_noise ),
			0, 0,
			1, 1,
			0, 0, 1, 1, pTabletRenderable );

		if ( fmod( gpGlobals->curtime, 1.0f ) < 0.9f )
		{
			pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_blocked ),
				0.35f, 0.35f,
				0.3f, 0.3f,
				0, 0, 1, 1, pTabletRenderable );
		}

	}

	if ( flTimeSinceBoot < k_flTabletBootDuration )
	{
		pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_boot ),
			0, 0,
			1, 1,
			0, 0, 1, 1, pTabletRenderable );

		float flLogoSpin = RemapValClamped( sin( flTimeSinceBoot * 20.0f ), -1.0f, 1.0f, 0.0f, 1.0f );
		flLogoSpin -= fmod( flLogoSpin, 0.08f );

		pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_boot_logo ),
			0.68f + (flLogoSpin * 0.1f) , 0.2f,
			0.2f * (1.0f - flLogoSpin), 0.2f,
			0, 0, 1, 1, pTabletRenderable );

		float flBootReveal = RemapValClamped( flTimeSinceBoot, -0.1f, k_flTabletBootDuration, 0.0f, 1.0f );
		flBootReveal -= fmod( flBootReveal, 0.015625f );

		pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_boot ),
			0, flBootReveal,
			1, 1,
			0, 0, 0, 0, pTabletRenderable );

		float flBootCover = RemapValClamped( flTimeSinceBoot, 0.6f, k_flTabletBootDuration, 0.0f, 1.0f );
		flBootCover -= fmod( flBootCover, 0.015625f );
		flBootCover -= 1.0f;

		pRenderContext->DrawScreenSpaceRectangle( GetTabletMaterial( tablet_boot ),
			0, flBootCover,
			1, 1,
			0, 0, 0, 0, pTabletRenderable );
	}

	pRenderContext->PopRenderTargetAndViewport();

	pRenderContext->OverrideDepthEnable( false, true );

}

#include "weapon_tablet_buymenu.inc"
void CTablet::RenderNotifications( void )
{
	float flYpos = 0.62f;
	for ( int i = 0; i < TABLET_NOTIFICATION_ARRAYSIZE; i++ )
	{
		tablet_notification_t nTemp = (tablet_notification_t)m_vecNotificationIds.Get( i );
		if ( nTemp != TABLET_NOTIFICATION_UNSET )
		{
			const staticTabletNotificationData_t *pNoteData = GetTabletNotificationData( (int)nTemp );
			if ( pNoteData )
			{
				float flNotificationTimeStamp = m_vecNotificationTimestamps.Get( i );

				float flLerpIn = RemapValClamped( gpGlobals->curtime, flNotificationTimeStamp, flNotificationTimeStamp + k_flNotificationTransitionDuration, 0.0f, 1.0f );
				float flLerpOut = RemapValClamped( gpGlobals->curtime, flNotificationTimeStamp + pNoteData->m_flDurationToLinger, flNotificationTimeStamp + pNoteData->m_flDurationToLinger + k_flNotificationTransitionDuration, 0.0f, 1.0f );

				if ( flLerpIn < 1.0f )
					flLerpIn = Bias( flLerpIn, 0.95f );

				if ( flLerpOut > 0.0f )
					flLerpOut = Bias( flLerpOut, 0.1f );

				float flXPos = Lerp( flLerpIn, 1.0f, -1.0f ); // slide in from the right

				if ( pNoteData->m_flDurationToLinger == 0.0f )
					flLerpOut = 0.0f;

				flXPos -= Lerp( flLerpOut, 0.0f, 1.0f ); // slide out to the left

				// is this slow? maybe these strings should be cached
				{
					wchar_t szWideBuff[64];
					
					// different notifications use different string construction rules
					if ( pNoteData->m_nSpecialInsertionRule == TABLET_STRINGBUILDRULE_NONE ) // generic case
					{
						g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( "#TabletNotification_Spacer" ), 1, g_pVGuiLocalize->Find( pNoteData->m_pszLocalizationToken ) );
						RenderText( szWideBuff, flXPos, flYpos, 1.0, pNoteData->m_fgColor, pNoteData->m_bgColor );
					}
					else if ( pNoteData->m_nSpecialInsertionRule == TABLET_STRINGBUILDRULE_CASHBUNDLEX )
					{
						/*INTENTIONALLY NOT STATIC! Lookup every time we need it!*/ ConVarRef cvrefNumStacks( pNoteData->m_pszAdditionalString );
						static ConVarRef sv_dz_cash_bundle_size( "sv_dz_cash_bundle_size" );
						int numDollars = sv_dz_cash_bundle_size.GetInt() * cvrefNumStacks.GetInt();
						wchar_t szWideBuffDollars[ 64 ];
						V_swprintf_safe( szWideBuffDollars, L"%d", numDollars );
						wchar_t szWideBuffTemp[ 64 ];
						g_pVGuiLocalize->ConstructString( szWideBuffTemp, sizeof( szWideBuffTemp ), g_pVGuiLocalize->Find( pNoteData->m_pszLocalizationToken ), 1, szWideBuffDollars );
						g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( "#TabletNotification_Spacer" ), 1, szWideBuffTemp );
					}
					else if ( pNoteData->m_nSpecialInsertionRule == TABLET_STRINGBUILDRULE_LASTPURCHASE ) // insert the last purchase token
					{
						// Find the purchase entry
						const TabletBuyMenuEntry *pBuyMenuEntry = NULL;
						for ( int jj = 0; jj < ARRAYSIZE( s_TabletBuyMenu ); jj++ )
						{
							if ( s_TabletBuyMenu[ jj ].nUniqueIndex == m_nLastPurchaseIndex.Get() )
							{
								pBuyMenuEntry = &s_TabletBuyMenu[ jj ];
								break;
							}
						}
						if ( pBuyMenuEntry )
						{
							wchar_t szWideBuffTemp[64];
							g_pVGuiLocalize->ConstructString( szWideBuffTemp, sizeof( szWideBuffTemp ), g_pVGuiLocalize->Find( pNoteData->m_pszLocalizationToken ), 1, g_pVGuiLocalize->Find( pBuyMenuEntry->szLocalizedToken ) );
							g_pVGuiLocalize->ConstructString( szWideBuff, sizeof( szWideBuff ), g_pVGuiLocalize->Find( "#TabletNotification_Spacer" ), 1, szWideBuffTemp );
						}
						else
						{
							Assert( false );
						}
					}

					RenderText( szWideBuff, flXPos, flYpos, 1.0, pNoteData->m_fgColor, pNoteData->m_bgColor );
				}

				flYpos -= 0.065f;
			}
		}
	}

}

void CTablet::RenderText( const wchar_t *pszText, float x, float y, float flSize, Color fgCol, Color bgCol )
{
	vgui::Panel *pPanel = m_PanelWrapper.GetPanel();
	if ( pPanel )
	{
		CTabletTextPanel *pTabletText = static_cast<CTabletTextPanel*>(pPanel);
		pTabletText->SetParams( pszText, fgCol, bgCol );

		VMatrix temp;
		temp.Identity();
		temp.PostTranslate( Vector( x, y, 0 ) );

		float flSizeScalar = 1024.0f / (float)m_nRenderTargetRes;

		g_pMatSystemSurface->DrawPanelIn3DSpace( pPanel->GetVPanel(), temp, 640, 640, flSize * flSizeScalar, flSize * flSizeScalar );
	}
}

void CTablet::RenderPlayerPositionHistory( IClientRenderable *pRenderable )
{
	// build a sorted list of logged positions
	CUtlVector<Vector> vecTemp;
	for ( int i = 0; i < TABLET_PLAYER_POSITION_HISTORY_COUNT; i++ )
	{
		Vector vecEntry = m_vecPlayerPositionHistory.Get( i );
		if ( vecEntry.z > 0 )
		{
			vecTemp.AddToTail( vecEntry );
		}
	}

	if ( vecTemp.Count() < 2 )
		return;

	// we need to sort because the networked list is optimized to minimize network delta. todo: cache?
	// the sort here on the client is cheap, but does run every frame
	vecTemp.Sort( []( const Vector *lhs, const Vector *rhs ) {
		float diff = lhs->z - rhs->z;
		if ( diff < 0.0f )
			return -1;
		else if ( diff > 0.0f )
			return 1;
		else
			return 0;
	} );

	FOR_EACH_VEC( vecTemp, i )
	{
		vecTemp[i].z = 0;
		Vector vecFrom = vecTemp[i];
		Vector vecTo = vecTemp[i];

		if ( vecTemp.IsValidIndex( i - 1 ) )
		{
			vecFrom = vecTemp[i - 1];
			vecFrom.z = 0;
		}
		if ( vecTemp.IsValidIndex( i + 1 ) )
		{
			vecTo = vecTemp[i + 1];
			vecTo.z = 0;
		}
		else
		{
			continue; // don't draw current
		}

		Vector vecDir = (vecTo - vecFrom).Normalized();
		
		float flAlpha = (float)i / (float)vecTemp.Count();
		flAlpha = RemapValClamped( flAlpha, 0.0f, 0.33f, 10.0f, 90.0f );

		m_WorkingColor.SetColor( 0, 0, 0, flAlpha );
		RenderMapQuadDir( GetTabletMaterial( tablet_player_path ), vecTemp[i] - Vector(0,0,100), Vector( 0, 0, 1 ), vecDir, 600, 600, 0, 0, 1, 1, pRenderable );

		m_WorkingColor.SetColor( 255, 255, 255, flAlpha );
		RenderMapQuadDir( GetTabletMaterial( tablet_player_path ), vecTemp[i], Vector( 0, 0, 1 ), vecDir, 600, 600, 0, 0, 1, 1, pRenderable );
	}
}

void CTablet::RenderRotatedScreenQuad( IMaterial *pMaterial, float x, float y, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable, float flRotation /* = 0.0f */ )
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	x = RemapVal( x, 0.0f, 1.0f, -1.0f, 1.0f );
	y = RemapVal( y, 0.0f, 1.0f, 1.0f, -1.0f );

	pRenderContext->Translate( x, y, 0 );
	pRenderContext->Rotate( flRotation, 0, 0, 1 );

	pRenderContext->Bind( pMaterial, pRenderable );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( true );

	w *= 0.5f;
	h *= 0.5f;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	
	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s0, t1 );
	meshBuilder.Position3f( -w, -h, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s0, t0 );
	meshBuilder.Position3f( -w, h, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s1, t0 );
	meshBuilder.Position3f( w, h, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s1, t1 );
	meshBuilder.Position3f( w, -h, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	m_nDrawElementCount++;
}

void CTablet::RenderMapQuad( IMaterial *pMaterial, float x, float y, float w, float h, float z, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial, pRenderable );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float flHalfWidth = w / 2.0f;
	float flHalfHeight = h / 2.0f;

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s0, t1 );
	meshBuilder.Position3f( x - flHalfWidth, y - flHalfHeight, z );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s0, t0 );
	meshBuilder.Position3f( x - flHalfWidth, y + flHalfHeight, z );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s1, t0 );
	meshBuilder.Position3f( x + flHalfWidth, y + flHalfHeight, z );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s1, t1 );
	meshBuilder.Position3f( x + flHalfWidth, y - flHalfHeight, z );
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();

	m_nDrawElementCount++;
}

void CTablet::RenderMapQuadDir( IMaterial *pMaterial, Vector vecCenter, Vector vecNormal, Vector vecUp, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial, pRenderable );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	Vector vecSide = CrossProduct( vecNormal, vecUp );

	vecSide = vecSide.Normalized() * (w / 2.0f);
	vecUp = vecUp.Normalized() * (h / 2.0f);

	Vector vecP0 = vecCenter - vecSide + vecUp;
	Vector vecP1 = vecCenter - vecSide - vecUp;
	Vector vecP2 = vecCenter + vecSide - vecUp;
	Vector vecP3 = vecCenter + vecSide + vecUp;

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s1, t0 );
	meshBuilder.Position3fv( vecP0.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s1, t1 );
	meshBuilder.Position3fv( vecP1.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s0, t1 );
	meshBuilder.Position3fv( vecP2.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
	meshBuilder.TexCoord2f( 0, s0, t0 );
	meshBuilder.Position3fv( vecP3.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	m_nDrawElementCount++;
}

void CTablet::RenderMapBillboard( IMaterial *pMaterial, Vector vecCenter, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable )
{
	Vector vecNormal = (m_vecLastCameraPos - vecCenter).Normalized();
	Vector vecSide = CrossProduct( Vector( 0, 0, 1 ), vecNormal );
	Vector vecUp = CrossProduct( vecNormal, vecSide );
	RenderMapQuadDir( pMaterial, vecCenter, vecNormal.Normalized(), vecUp.Normalized(), w, h, s0, t0, s1, t1, pRenderable );
}

void CTablet::RenderDroneTrail( IMaterial *pMaterial, CBaseEntity *pDroneEnt, float z, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable )
{
	if ( !pDroneEnt )
		return;

	CDrone *pDrone = static_cast< CDrone* >(pDroneEnt);
	if ( !pDrone->m_vecClientSideTrailPositions.Count() )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial, pRenderable );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, pDrone->m_vecClientSideTrailPositions.Count() );

	float flHalfWidth = w / 2.0f;
	float flHalfHeight = h / 2.0f;

	bool bEnemyDrone = pDrone->GetMoveToEnt() != this;

	FOR_EACH_VEC( pDrone->m_vecClientSideTrailPositions, nTrail )
	{
		const Vector& vecTrailPos = pDrone->m_vecClientSideTrailPositions[nTrail];

		float flAlpha1 = RemapValClamped( (float)nTrail, 0.0f, (float)pDrone->m_vecClientSideTrailPositions.Count() * 0.5f, 0.0f, 1.0f );
		float flAlpha2 = RemapValClamped( gpGlobals->curtime - vecTrailPos.z, 0.0f, 30.0f, 1.0f, 0.0f );

		int nPtAgeToAlpha = (int)(flAlpha1 * flAlpha2 * 255.0f);
		if ( !bEnemyDrone )
		{
			m_WorkingColor.SetColor( 0, 216, 255, nPtAgeToAlpha );
		}
		else
		{
			m_WorkingColor.SetColor( 180, 180, 180, nPtAgeToAlpha );
		}

		float x = vecTrailPos.x;
		float y = vecTrailPos.y;
		
		meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
		meshBuilder.TexCoord2f( 0, s0, t1 );
		meshBuilder.Position3f( x - flHalfWidth, y - flHalfHeight, z );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
		meshBuilder.TexCoord2f( 0, s0, t0 );
		meshBuilder.Position3f( x - flHalfWidth, y + flHalfHeight, z );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
		meshBuilder.TexCoord2f( 0, s1, t0 );
		meshBuilder.Position3f( x + flHalfWidth, y + flHalfHeight, z );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color4ub( m_WorkingColor.r(), m_WorkingColor.g(), m_WorkingColor.b(), m_WorkingColor.a() );
		meshBuilder.TexCoord2f( 0, s1, t1 );
		meshBuilder.Position3f( x + flHalfWidth, y - flHalfHeight, z );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}

#endif


#ifndef CLIENT_DLL
IMPLEMENT_AUTO_LIST( ITabletBlocker );

LINK_ENTITY_TO_CLASS( func_tablet_blocker, CTabletBlocker );

BEGIN_DATADESC( CTabletBlocker )
DEFINE_FUNCTION( CTabletBlockerShim::Touch ),
END_DATADESC()

void CTabletBlocker::Spawn()
{
	InitTrigger();
	SetTouch( &CTabletBlocker::TabletBlockerTouch );
}

void CTabletBlocker::TabletBlockerTouch( CBaseEntity* pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

	CCSPlayer *pCSPlayer = ToCSPlayer( pOther );
	if ( pCSPlayer )
	{
		CBaseCombatWeapon* pTabletWep = pCSPlayer->Weapon_OwnsThisType( "weapon_tablet" );
		if ( pTabletWep )
		{
			CTablet *pTablet = static_cast<CTablet*>(pTabletWep);
			pTablet->BlockTabletReception();
			pTablet->PushTabletNotification( TABLET_NOTIFICATION_SIGNAL_LOST );
		}
	}
}
#endif

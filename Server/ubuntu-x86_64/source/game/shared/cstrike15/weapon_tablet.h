//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TABLET_H
#define WEAPON_TABLET_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_csbase.h"
#ifndef CLIENT_DLL
#include "triggers.h"
#endif

#ifdef CLIENT_DLL
#include "panelmetaclassmgr.h"
#endif

#if defined( CLIENT_DLL )
	#define CTablet C_Tablet
#endif

#ifdef CLIENT_DLL
#define REGISTER_TABLET_MATERIAL( _n ) _n,
enum tabletmaterialindex_t
{
	#include "weapon_tablet_materials.inc"
	TABLETMAT_COUNT,
};
#undef REGISTER_TABLET_MATERIAL
#endif

#define TABLET_HEX_COUNT 42

#define TABLET_NOTIFICATION_ARRAYSIZE 8

#define TABLET_PLAYER_POSITION_HISTORY_COUNT 24

enum tablet_skin_state_t
{
	TABLET_SKIN_STATE_BLANK = 0,
	TABLET_SKIN_STATE_RADAR,
	TABLET_SKIN_STATE_BUYMENU
};

enum tablet_upgrade_type_t
{
	TABLET_UPGRADE_HIGHRES = 0,
	TABLET_UPGRADE_DRONEINTEL,
	TABLET_UPGRADE_ZONEINTEL,
	TABLET_UPGRADE_COUNT,
};

enum tablet_notification_stringbuildingrule_t
{
	TABLET_STRINGBUILDRULE_NONE = 0,
	TABLET_STRINGBUILDRULE_LASTPURCHASE,
	TABLET_STRINGBUILDRULE_CASHBUNDLEX,
	// timer/countdown behaviors to go here?
};

enum LocalHexFlag_t
{
	HEX_FL_OCCUPIED_0 = (1 << 0),
	HEX_FL_HIRES_OCCUPIED_1 = (1 << 1),
	HEX_FL_HIRES_OCCUPIED_2 = (1 << 2),
	HEX_FL_HIRES_OCCUPIED_3 = (1 << 3),
	HEX_FL_HIRES_OCCUPIED_4 = (1 << 4),
	HEX_FL_HIRES_OCCUPIED_5 = (1 << 5),
	HEX_FL_EXPLORED = (1 << 6),
	HEX_FL_JAMMED = (1 << 7),
	HEX_FL_JAMMED_BY_OWNER = (1 << 8),
};

enum tablet_notification_t
{
	TABLET_NOTIFICATION_UNSET = 0,
	TABLET_NOTIFICATION_UPGRADE_HIGHRES,
	TABLET_NOTIFICATION_UPGRADE_ZONEINTEL,
	TABLET_NOTIFICATION_UPGRADE_DRONEINTEL,
	TABLET_NOTIFICATION_HOSTAGE_IN_TRANSIT,
	TABLET_NOTIFICATION_INCOMING_DELIVERY,
	TABLET_NOTIFICATION_PRESS_B_TO_BUY,
	TABLET_NOTIFICATION_RETURN_TO_PLAY_AREA,
	TABLET_NOTIFICATION_SIGNAL_LOST,
	TABLET_NOTIFICATION_HIGHLIGHTED_SECTORS_CONTAIN_PLAYERS,
	TABLET_NOTIFICATION_EXPLORATION_PAYMENT,
	TABLET_NOTIFICATION_BOMB_PLANTED_ON_TARGET,
	TABLET_NOTIFICATION_BOMB_DETONATED_ON_TARGET,
	TABLET_NOTIFICATION_PURCHASE_CONFIRMATION,
	TABLET_NOTIFICATION_BOMBWAVE_MONEY,
	TABLET_NOTIFICATION_PARADROPS_ALLOWED,
	TABLET_NOTIFICATION_COUNT,
};

DECLARE_AUTO_LIST( ITablet )
class CTablet : public CWeaponCSBase, public ITablet
{
public:
	DECLARE_CLASS( CTablet, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	IMPLEMENT_AUTO_LIST_GET();
	
	CTablet();
	CTablet( const CTablet & ) {}
	~CTablet();

	virtual bool HasPrimaryAmmo() { return false; }
	virtual bool CanBeSelected() { return true; }
	void Spawn();
	void PrimaryAttack();
	void SecondaryAttack();
	virtual bool Deploy();
	void WeaponIdle();
	void UpdateShieldState();
	virtual void ItemBusyFrame();

	virtual CSWeaponType GetWeaponType( void ) const { return WEAPONTYPE_TABLET; }

	virtual float GetRunBobScale() const;
	bool ShouldBuyMenuBeOpen( void );

	CBaseViewModel *GetViewModelPtr( void ) const;

	bool HasTabletUpgrade( tablet_upgrade_type_t nUpgrade );

	void UpdatePoseParameter( void );

#ifndef CLIENT_DLL
	void UpdatePlayerPositionHistory( void );

	int GetThinkLimitToExploreSector( void );

	void UpdateTabletExplorationPayout( void );

	void TabletClientCommand( const char* szCmd );
	bool m_bPendingBuyMenu;

	void SetTabletUpgrade( tablet_upgrade_type_t nUpgrade, bool bDoAnim );
	void SetTabletSkinState( tablet_skin_state_t nNewState );

	void BlockTabletReception() { m_bTabletReceptionIsBlocked = true; m_flLastTabletBlockedTime = gpGlobals->curtime; }

	void PushTabletNotification( tablet_notification_t nNotificationId, float flAddTimeWhenNew = 0.0f );
	void UpdateTabletNotifications( void );

	void SetLastPurchaseIndex( int nIdx ) { m_nLastPurchaseIndex = nIdx; }
#endif

	float GetTabletHexSize();

#ifdef CLIENT_DLL
	virtual void OnPreDataChanged( DataUpdateType_t updateType ) OVERRIDE;
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual void Precache( void ) OVERRIDE;
	virtual int KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	void RenderTablet( void );
	void RenderMapQuad( IMaterial *pMaterial, float x, float y, float w, float h, float z, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable );
	void RenderMapQuadDir( IMaterial *pMaterial, Vector vecCenter, Vector vecNormal, Vector vecUp, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable );
	void RenderMapBillboard( IMaterial *pMaterial, Vector vecCenter, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable );
	void RenderRotatedScreenQuad( IMaterial *pMaterial, float x, float y, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable, float flRotation = 0.0f );
	void RenderDroneTrail( IMaterial *pMaterial, CBaseEntity *pDroneEnt, float z, float w, float h, float s0, float t0, float s1, float t1, IClientRenderable *pRenderable );
	void RenderText( const wchar_t *pszText, float x, float y, float flSize, Color fgCol, Color bgCol );
	void RenderNotifications( void );
	void RenderPlayerPositionHistory( IClientRenderable *pRenderable );

	virtual void WeaponPreRender( void ) OVERRIDE;

	CPanelWrapper m_PanelWrapper;

#endif // CLIENT_DLL

private:

	tablet_skin_state_t GetTabletSkinState( void ) const;
	
	bool HexHasFlag( int nHex, int hexFlag );

	CNetworkArray( float, m_flUpgradeExpirationTime, TABLET_UPGRADE_COUNT );
	CNetworkArray( int, m_vecLocalHexFlags, TABLET_HEX_COUNT );
	CNetworkVar( int, m_nContractKillGridIndex );
	CNetworkVar( int, m_nContractKillGridHighResIndex );
	CNetworkVar( bool, m_bTabletReceptionIsBlocked );
	CNetworkVar( float, m_flScanProgress );
	CNetworkVar( float, m_flBootTime );
	CNetworkVar( float, m_flShowMapTime );
	CNetworkArray( int, m_vecNotificationIds, TABLET_NOTIFICATION_ARRAYSIZE );
	CNetworkArray( float, m_vecNotificationTimestamps, TABLET_NOTIFICATION_ARRAYSIZE );
	CNetworkArray( Vector, m_vecPlayerPositionHistory, TABLET_PLAYER_POSITION_HISTORY_COUNT );
	CNetworkVar( int, m_nLastPurchaseIndex );

#ifdef CLIENT_DLL
	float GetTimeOfLastHexChange( int nHex, int nSubRegion );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	int m_vecLocalHexFlagsClientCopy[TABLET_HEX_COUNT];
	float m_vecLastHexPlayerOccupancyChange[TABLET_HEX_COUNT][6];
#endif

#ifndef CLIENT_DLL

	Vector2D m_vecLastPlayerPosition;

	void UpdatePlayerOccupiedGrid( void );
	void CheckForIncomingDrones( void );
	void ResetLocalHexes();
	void SetHexFlag( int nHex, int hexFlag );
	void ClearContractKill();

	float m_flNextCheckForIncomingDronesTime;

	float m_flLastPlayerOccupiedGridUpdate;
	float m_flLastTabletBlockedTime;
	
	struct playerExplorationPaymentSectorRecord_t
	{
		float m_flNextExplorationTime;
		int m_nThinksInSector[TABLET_HEX_COUNT];
		playerExplorationPaymentSectorRecord_t()
		{
			m_flNextExplorationTime = gpGlobals->curtime;
			for ( int i = 0; i < TABLET_HEX_COUNT; i++ )
			{
				m_nThinksInSector[i] = 0;
			}
		}
	} m_ExplorationPaymentRecord;

#else

	IMaterial *GetTabletMaterial( tabletmaterialindex_t idx );
	IMaterial *m_vecTabletMaterials[TABLETMAT_COUNT];

	float m_flNoiseFadeAlpha;
	
	Color m_WorkingColor;
	Vector m_vecLastCameraPos;
	QAngle m_angLastCameraAng;
	int m_nDrawElementCount;
	float m_flPrevScanProgress;

	int m_nRenderTargetRes;

#endif

	float m_flLastClosePoseParamVal;
};

#ifndef CLIENT_DLL
void UTIL_PushGlobalTabletNotification( tablet_notification_t nNotificationId );
#endif

#ifndef CLIENT_DLL
class CTabletBlockerShim : public CBaseTrigger
{
public:
	void Touch( CBaseEntity *pOther ) { return TabletBlockerTouch( pOther ); }

	virtual void TabletBlockerTouch( CBaseEntity* pOther ) = 0;
};

DECLARE_AUTO_LIST( ITabletBlocker );
class CTabletBlocker : public CTabletBlockerShim, public ITabletBlocker
{
public:
	DECLARE_CLASS( CTabletBlocker, CBaseTrigger );
	DECLARE_DATADESC();

	IMPLEMENT_AUTO_LIST_GET();

	void Spawn();

	void TabletBlockerTouch( CBaseEntity* pOther );
};
#endif


#endif // WEAPON_TABLET_H

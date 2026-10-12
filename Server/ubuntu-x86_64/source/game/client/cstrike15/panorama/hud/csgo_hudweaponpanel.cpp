//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
// SF Differences:
//		* Missing additive blending on "AmmoTextClip"
//		* Kill icons currently png -> change to vtf ?
//		* defuse / c4 icons currently png -> change to svg
//		* bombzone icon - no animation 
//		* bombzone / c4 / defuse icons - no additive blending
//		* kill icons - slightly different animation
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudweaponpanel.h"
#include "csgo_hud.h"
#include "clientmode_csnormal.h" // CSGOFrameUpdate

#include "c_cs_player.h"
#include "panorama/uievents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_HudWeaponPanel, CSGOHudWeaponPanel );

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;
extern ConVar cl_hud_background_alpha;
extern ConVar cl_hud_healthammo_style;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudWeaponPanel::CCSGO_HudWeaponPanel( panorama::CPanel2D *pParent, const char *pchID )
:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudWeaponPanel", this ),
	m_nNextFreeShellIndex( 0 )
{
	SetHiddenBits( HIDEHUD_WEAPONSELECTION );

	RequireLoadLayout( "file://{resources}/layout/hud/hudweaponpanel.xml" );

	m_pDefuseIconPanel = RequireChildInLayoutFile( "DefuseIcon" );

	m_pAmmoContentPanel = RequireChildInLayoutFile( "AmmoContent" );
	m_pAmmoTextClipPanel = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "AmmoTextClip" ) );
	m_pAmmoTextTotalPanel = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "AmmoTextTotal" ) );
	m_pAmmoBurstIconsPanel = RequireChildInLayoutFile( "AmmoBurstIcons" );
	m_pWeaponPanelBottomBG = RequireChildInLayoutFile( "WeaponPanelBottomBG" );

	// $$$REI Replace with loc strings and dialog vars
	m_pKillCountIconsPanel = RequireChildInLayoutFile( "KillCountIcons" );
	m_pKillCountTextPanel = RequireChildInLayoutFile( "KillCountText" );
	m_pKillCountPanel = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "KillCount" ) );
	m_pKillCountTextIconPanel = RequireChildInLayoutFile( "KillCountTextIcon" );
	m_pKillEaterContentPanel = RequireChildInLayoutFile( "KillEaterContent" );
	m_pKillEaterCountPanel = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "KillEaterCount" ) );

	m_pAmmoAnimPanel = RequireChildInLayoutFile( "AmmoAnim" );
	m_pAmmoAnimBulletsPanel = RequireChildInLayoutFile( "AmmoAnimBullets" );
	m_pAmmoAnimShellsPanel = RequireChildInLayoutFile( "AmmoAnimShells" );
	m_pAmmoAnimGrenadesPanel = RequireChildInLayoutFile( "AmmoAnimGrenades" );

	ResetData();

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudWeaponPanel::~CCSGO_HudWeaponPanel()
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::LevelInit()
{
	ShowPanel( false );
	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::LevelShutdown()
{	
	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: Called once per frame for visible elements before general key processing
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::Think()
{
	Update();
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame for visible elements before general key processing
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::Update()
{
	/////////////////////////////////// 
	//	Data used to update the weapon panel UI
	///////////////////////////////////

	int nEntityIndex = -1;

	bool bCarryingDefuse = false;

	int nRoundKills = 0;
	int nRoundKillsHeadshots = 0;
	int nRequiredKills = 0;
	int nKillEaterAltScore = 0;

	int	nCurrentClip = 0;
	// we need the map clip to calculate the percentage of ammo left 
	// in the clip so the hud knows when to warn you
	int	nMaxClip = 0;
	int	nTotalAmmo = 0;

	bool bInTRBombMode = false;
	int nCurrTRPoints = -1;

	///////////////////////////////////
	// Collect data
	///////////////////////////////////

	if ( CSGameRules()->IsPlayingGunGame() && CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		bInTRBombMode = true;
	}

	C_CSPlayer *pPlayer = ToCSPlayer( GetLocalOrObservedPlayer() );
	C_WeaponCSBase *pWeapon = NULL;

	if ( pPlayer )
	{
		nEntityIndex = pPlayer->entindex();

		if ( bInTRBombMode )
		{
			nCurrTRPoints = pPlayer->GetNumGunGameTRKillPoints();
		}

		// Check if the player if carrying the bomb, defuser or is in the bomb zone

		if ( CSGameRules()->IsBombDefuseMap() || CSGameRules()->IsHostageRescueMap() )
		{
			bCarryingDefuse = pPlayer->HasDefuser();
		}

		// Update round kills

		nRoundKills = pPlayer->GetNumRoundKills();
		nRoundKillsHeadshots = pPlayer->GetNumRoundKillsHeadshots();
		if ( pPlayer->IsControllingBot() )
		{
			C_CSPlayer *controlledPlayerScorer = ToCSPlayer( UTIL_PlayerByIndex( pPlayer->GetControlledBotIndex() ) );
			if ( controlledPlayerScorer )
			{
				nRoundKills = controlledPlayerScorer->GetNumRoundKills();
				nRoundKillsHeadshots = controlledPlayerScorer->GetNumRoundKillsHeadshots();
			}
		}
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			int nCurIndex = pPlayer->GetPlayerGunGameWeaponIndex();
			nRequiredKills = CSGameRules()->GetGunGameNumKillsRequiredForWeapon( nCurIndex, pPlayer->GetTeamNumber() );
		}

		pWeapon = ( C_WeaponCSBase* )pPlayer->GetActiveWeapon();
		if ( pWeapon )
		{
			// KILLEATER

			CEconItemView *pItem = pWeapon->GetEconItemView();

			// Get the supported killeater types on this weapon
			CUtlSortVector<uint32> killEaterTypes;
			pItem->GetKillEaterTypes( killEaterTypes );
			// Get the kill eater value of the highest-numbered killeater type
			// TODO: Instead, we should find out which killeater value is being displayed and use that
			if ( killEaterTypes.Count() > 0 )
			{
				// TODO: The numerically last killeater type wins?
				nKillEaterAltScore = pItem->GetKillEaterValueByType( killEaterTypes[killEaterTypes.Count() - 1] );
			}

			// hide the counter if the weapon owner is not the weapon holder.
			CSteamID pKillerSteamID;

			if ( ( !pPlayer->GetSteamID( &pKillerSteamID ) ) ||
				!pKillerSteamID.IsValid() ||
				( pKillerSteamID.GetAccountID() != pItem->GetAccountID() ) )
			{
				nKillEaterAltScore = 0;
			}

			nKillEaterAltScore = clamp( nKillEaterAltScore, 0, 999999 );

			// AMMO

			// determine what to display for ammo: "clip/total", "total" or nothing (for knife, c4, etc)
			if ( !pWeapon->UsesPrimaryAmmo() )
			{
				nCurrentClip = -1;
				nMaxClip = -1;
				nTotalAmmo = -1;
			}
			else
			{
				nCurrentClip = pWeapon->Clip1();
				nMaxClip = pWeapon->GetMaxClip1();
				if ( nCurrentClip < 0 )
				{
					// we don't use clip ammo, just use the total ammo count
					nCurrentClip = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
					nTotalAmmo = -1;
				}
				else
				{
					// we use clip ammo, so the second ammo is the total ammo
					nTotalAmmo = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
				}
			}
		}
	}

	// burst mode
	bool bShowBurstBurst = ( pWeapon && pWeapon->WeaponHasBurst() && pWeapon->IsInBurstMode() );
	bool bShowBurstSingle = ( pWeapon && pWeapon->WeaponHasBurst() && !pWeapon->IsInBurstMode() );

	m_pWeaponPanelBottomBG->SetOpacitySimple( cl_hud_background_alpha.GetFloat() );

	/////////////////////////////////// 
	// Update weapon panel UI
	///////////////////////////////////

	UpdateIconsUI( bCarryingDefuse );
	UpdateAmmoUI( pPlayer, pWeapon, nCurrentClip, nMaxClip, nTotalAmmo, bShowBurstSingle, bShowBurstBurst );
	UpdateKillsUI( nRoundKills, nRoundKillsHeadshots, nRequiredKills, nKillEaterAltScore );

	///////////////////////////////////
	// Save data to be used next frame
	///////////////////////////////////

	m_hObservedPlayer = pPlayer;
	m_hObservedWeapon = pWeapon;

	m_nPrevCurrentClip = nCurrentClip;
	m_nPrevTotalAmmo = nTotalAmmo;

	m_nPrevRoundKills = nRoundKills;
	m_nPrevRoundKillsHeadshot = nRoundKillsHeadshots;
	m_nPrevKillEaterAltScore = nKillEaterAltScore;

	SetHasClass( "WeaponPanel--Simple", cl_hud_healthammo_style.GetInt() == 1 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::ResetData()
{
	m_hObservedPlayer = nullptr;
	m_hObservedWeapon = nullptr;
	
	m_nPrevCurrentClip = -1;
	m_nPrevTotalAmmo = -1;

	m_nPrevRoundKills = -1;
	m_nPrevRoundKillsHeadshot = -1;
	m_nPrevKillEaterAltScore = -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::UpdateIconsUI( bool bCarryingDefuse )
{
	static const panorama::CPanoramaSymbol k_symIconShow( "WeaponPanelTop__Icon--Show" );
	m_pDefuseIconPanel->SetHasClass( k_symIconShow, bCarryingDefuse );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::UpdateAmmoUI( C_CSPlayer* pPlayer, C_WeaponCSBase* pWeapon, int nCurrentClip, int nMaxClip, int nTotalAmmo, bool bShowBurstSingle, bool bShowBurstBurst )
{
	static const panorama::CPanoramaSymbol k_symAmmoHidden( "Ammo--Hidden" );
	static const panorama::CPanoramaSymbol k_symAmmoBurstBurst( "Ammo__Burst--Burst" );
	static const panorama::CPanoramaSymbol k_symAmmoBurstSingle( "Ammo__Burst--Single" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimBulletsHidden( "AmmoAnim__Bullets--Hidden" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimBulletIconAnim( "AmmoAnim__BulletIcon--Anim" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimBulletIconRed( "AmmoAnim__BulletIcon--Red" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimBulletIconHidden( "AmmoAnim__BulletIcon--Hidden" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimGrenadesHidden( "AmmoAnim__Grenades--Hidden" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimGrenadeIconHidden( "AmmoAnim_GrenadeIcon--Hidden" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimShellIconAnim( "AmmoAnim__ShellIcon--Anim" );
	static const panorama::CPanoramaSymbol k_symAmmoAnimShellIconRed( "AmmoAnim__ShellIcon--Red" );
	
	if ( nCurrentClip !=  m_nPrevCurrentClip )
	{
		m_pAmmoTextClipPanel->SetText( CNumStr( nCurrentClip ).String() );
	}
	if ( nTotalAmmo != m_nPrevTotalAmmo )
	{
		m_pAmmoTextTotalPanel->SetText( CFmtStr( "/ %d", nTotalAmmo ).String() );
	}

	bool bHideAmmo = ( nTotalAmmo < 0 );
	m_pAmmoContentPanel->SetHasClass( k_symAmmoHidden, bHideAmmo );
	m_pWeaponPanelBottomBG->SetVisible( !bHideAmmo );

	m_pAmmoBurstIconsPanel->SetHasClass( k_symAmmoBurstBurst, bShowBurstBurst );
	m_pAmmoBurstIconsPanel->SetHasClass( k_symAmmoBurstSingle, bShowBurstSingle );

	// Bullets animation

	CSWeaponType eWeaponType = pWeapon ? pWeapon->GetWeaponType() : WEAPONTYPE_UNKNOWN;
	const bool bShowBulletsPanel = IsGunWeapon( eWeaponType );
	m_pAmmoAnimBulletsPanel->SetHasClass( k_symAmmoAnimBulletsHidden, !bShowBulletsPanel );
	if ( bShowBulletsPanel )
	{		
		const float flAmmoPercent = (float)nCurrentClip / (float)nMaxClip;
		
		// Notify the HUD when we fire - detected by not changing weapon/observed player, and our ammo count decreases
		if ( !m_hObservedPlayer.ChangedFrom( pPlayer ) && !m_hObservedWeapon.ChangedFrom( pWeapon ) && ( m_nPrevCurrentClip > 0 ) && ( nCurrentClip < m_nPrevCurrentClip ) )
		{
			// Play "weapon fired" animation

			for ( int nIcon = 0; nIcon < m_pAmmoAnimBulletsPanel->GetChildCount(); ++nIcon )
			{
				panorama::CPanel2D *pIconPanel = m_pAmmoAnimBulletsPanel->GetChild( nIcon );
				pIconPanel->TriggerClass( k_symAmmoAnimBulletIconAnim );

				pIconPanel->SetHasClass( k_symAmmoAnimBulletIconRed, ( flAmmoPercent <= 0.2f ) );
				pIconPanel->SetHasClass( k_symAmmoAnimBulletIconHidden, ( nCurrentClip <= nIcon ) );
			}

			// Play shell animation

			panorama::CPanel2D *pShellIconPanel = m_pAmmoAnimShellsPanel->GetChild( m_nNextFreeShellIndex );
			pShellIconPanel->TriggerClass( k_symAmmoAnimShellIconAnim );
			pShellIconPanel->SetHasClass( k_symAmmoAnimShellIconRed, ( flAmmoPercent <= 0.2f ) );

			m_nNextFreeShellIndex = ( m_nNextFreeShellIndex + 1 ) % m_pAmmoAnimShellsPanel->GetChildCount();
		}
		else if ( nCurrentClip != m_nPrevCurrentClip )
		{
			// Just display bullets - no animation

			for ( int nIcon = 0; nIcon < m_pAmmoAnimBulletsPanel->GetChildCount(); ++nIcon )
			{
				panorama::CPanel2D *pIconPanel = m_pAmmoAnimBulletsPanel->GetChild( nIcon );
				pIconPanel->RemoveClass( k_symAmmoAnimBulletIconAnim );

				pIconPanel->SetHasClass( k_symAmmoAnimBulletIconRed, ( flAmmoPercent <= 0.2f ) );
				pIconPanel->SetHasClass( k_symAmmoAnimBulletIconHidden, ( nCurrentClip <= nIcon ) );
			}
		}


	}

	// Grenades 
	const bool bShowGrenadesPanel = eWeaponType == WEAPONTYPE_GRENADE || eWeaponType == WEAPONTYPE_STACKABLEITEM;
	m_pAmmoAnimGrenadesPanel->SetHasClass( k_symAmmoAnimGrenadesHidden, !bShowGrenadesPanel );
	if ( bShowGrenadesPanel )
	{
		Assert( pWeapon );

		static const CSchemaItemDefHandle weapon_healthshot( "weapon_healthshot" );
		bool bIsHealthShot = ( weapon_healthshot && pWeapon->GetItemDefinition() == weapon_healthshot );

		if ( m_hObservedWeapon.ChangedFrom( pWeapon ) )
		{
			// Change grenade icons
			const char *szWeaponName = pWeapon->GetDefinitionName();
			if ( szWeaponName )
			{
				if ( IsWeaponClassname( szWeaponName ) )
					szWeaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;

				CFmtStr iconPath( "file://{images}/icons/equipment/%s.svg", szWeaponName );
				if ( bIsHealthShot )
					iconPath = "file://{images}/hud/weaponpanel/healthshotammo.png";

				for ( int nIcon = 0; nIcon < m_pAmmoAnimGrenadesPanel->GetChildCount(); ++nIcon )
				{
					panorama::CImagePanel *pIconPanel = panorama::panel_cast< panorama::CImagePanel * >( m_pAmmoAnimGrenadesPanel->GetChild( nIcon ) );
					pIconPanel->SetImageJS( iconPath.String() );
				}
			}
		}

		if ( ( nCurrentClip != m_nPrevCurrentClip ) || m_hObservedWeapon.ChangedFrom( pWeapon ) )
		{
			for ( int nIcon = 0; nIcon < m_pAmmoAnimGrenadesPanel->GetChildCount(); ++nIcon )
			{
				panorama::CPanel2D *pIconPanel = m_pAmmoAnimGrenadesPanel->GetChild( nIcon );
				pIconPanel->SetHasClass( k_symAmmoAnimGrenadeIconHidden, nIcon >= nCurrentClip );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::UpdateKillsUI( int nRoundkills, int nRoundKillsHeadshots, int nRequiredKills, int nKillEaterAltScore )
{
	static const panorama::CPanoramaSymbol k_symKillsCountHidden( "Kills__Count--Hidden" );
	static const panorama::CPanoramaSymbol k_symKillIconHidden( "Kills__Icon--Hidden" );
	static const panorama::CPanoramaSymbol k_symKillIconHeadshot( "Kills__Icon--Headshot" );
	static const panorama::CPanoramaSymbol k_symKillIconOutline( "Kills__Icon--Outline" );
	static const panorama::CPanoramaSymbol k_symKillIconAnimate( "Kills__Icon--Animate" );
	static const panorama::CPanoramaSymbol k_symKillKillEaterHidden( "Kills__KillEater--Hidden" );
	
	// Update kill icons
	
	if ( ( nRoundkills != m_nPrevRoundKills ) || ( nRoundKillsHeadshots != m_nPrevRoundKillsHeadshot ) )
	{
		// Index of the icon to animate in case of a new kill
		//		* headshot - animate first icon
		//		* normal kill - animate last icon
		int nIconToAnimate = -1;
		if ( ( nRoundkills > 0 ) && ( nRoundkills > m_nPrevRoundKills ) )
		{
			if ( nRoundKillsHeadshots > m_nPrevRoundKillsHeadshot )
			{
				nIconToAnimate = 0;
			}
			else
			{
				nIconToAnimate = nRoundkills - 1;
			}
		}
		
		const int nMaxIcons = m_pKillCountIconsPanel->GetChildCount();
		const bool bIconMode = ( nRoundkills <= nMaxIcons );
		if ( bIconMode )
		{
			// Icon mode

			for ( int nIcon = 0; nIcon < nMaxIcons; ++nIcon )
			{
				panorama::CPanel2D *pIconPanel = m_pKillCountIconsPanel->GetChild( nIcon );

				pIconPanel->SetHasClass( k_symKillIconHeadshot, ( nIcon < nRoundKillsHeadshots ) );
				pIconPanel->SetHasClass( k_symKillIconHidden, ( nIcon >= nRoundkills ) && ( nIcon >= nRequiredKills ) );
				pIconPanel->SetHasClass( k_symKillIconOutline, ( nIcon >= nRoundkills ) && ( nIcon < nRequiredKills ) );

				if ( nIcon == nIconToAnimate )
				{
					pIconPanel->TriggerClass( k_symKillIconAnimate );
				}
				else
				{
					pIconPanel->RemoveClass( k_symKillIconAnimate );
				}
			}
		}
		else
		{
			// Text mode

			m_pKillCountPanel->SetText( CFmtStr( "x%d", nRoundkills ).String() );
			m_pKillCountTextIconPanel->TriggerClass( k_symKillIconAnimate );
		}

		m_pKillCountIconsPanel->SetHasClass( k_symKillsCountHidden, !bIconMode );
		m_pKillCountTextPanel->SetHasClass( k_symKillsCountHidden, bIconMode );
	}

	// Update StatTrak

	if ( m_nPrevKillEaterAltScore != nKillEaterAltScore )
	{
		m_pKillEaterCountPanel->SetText( CNumStr( nKillEaterAltScore ).String() );
	}
	m_pKillEaterContentPanel->SetHasClass( k_symKillKillEaterHidden, ( nKillEaterAltScore == 0 ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudWeaponPanel::ShouldDraw( void )
{
	C_CSPlayer *pPlayer = GetHudPlayer();
	if ( !pPlayer || pPlayer->IsPlayerGhost() )
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		ShowPanel( bActive );
	}

	CHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWeaponPanel::ShowPanel( bool bShow )
{
	static panorama::CPanoramaSymbol k_symWeaponPanelHidden( "WeaponPanel--Hidden" );

	SetHasClass( k_symWeaponPanelHidden, !bShow );

	ResetData();
}


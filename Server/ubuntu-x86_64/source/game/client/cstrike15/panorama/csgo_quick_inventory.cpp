//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_quick_inventory.h"
#include "IGameUIFuncs.h"
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientsteamcontext.h"
#include "panorama/iuisoundsystem.h"
#include "inputsystem/iinputsystem.h"

#include "weapon_csbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_QuickInventory, CSGOQuickInventory );

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;

bool g_bQuickInventoryOpen = false;

void QuickInventoryOpen( void )
{
	engine->ClientCmd( "savedwep_set" );
	g_bQuickInventoryOpen = true;
}

void QuickInventoryClose( void )
{
	g_bQuickInventoryOpen = false;
	engine->ClientCmd( "savedwep_lastinv" );
}

#if DEVELOPMENT_ONLY
static ConCommand quickinventoryopen( "+quickinv", QuickInventoryOpen );
static ConCommand quickinventoryclose( "-quickinv", QuickInventoryClose );
#endif

ConVar cl_quickinventory_deadzone_size( "cl_quickinventory_deadzone_size", "0.05", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE );
ConVar cl_quickinventory_lastinv( "cl_quickinventory_lastinv", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE );

struct quickInvOrdering_t
{
	CSWeaponType m_wepType;
	float m_flInitialAngleWidth;
	bool m_bAllowResize;
	bool m_bPlaceAfterGap;
};

static quickInvOrdering_t s_quickInvOrdering[] = // this is the radial order of items that the quick select menu uses when sorting
{
	// primary
	{ WEAPONTYPE_SUBMACHINEGUN,		60.0f,		false,		false },
	{ WEAPONTYPE_RIFLE,				60.0f,		false,		false },
	{ WEAPONTYPE_SHOTGUN,			60.0f,		false,		false },
	{ WEAPONTYPE_SNIPER_RIFLE,		60.0f,		false,		false },
	{ WEAPONTYPE_MACHINEGUN,		60.0f,		false,		false },

	// pistol
	{ WEAPONTYPE_PISTOL,			60.0f,		false,		false },

	// grenade/explosives
	{ WEAPONTYPE_GRENADE,			60.0f,		true,		false },
	{ WEAPONTYPE_C4,				60.0f,		true,		false },
	{ WEAPONTYPE_BREACHCHARGE,		60.0f,		true,		false },

	// tools/equipment
	{ WEAPONTYPE_EQUIPMENT,			60.0f,		true,		true },
	{ WEAPONTYPE_STACKABLEITEM,		60.0f,		true,		true },
	{ WEAPONTYPE_TABLET,			60.0f,		true,		true },

	// melee/taser
	{ WEAPONTYPE_FISTS,				60.0f,		true,		true },
	{ WEAPONTYPE_MELEE,				60.0f,		true,		true },
	{ WEAPONTYPE_KNIFE,				60.0f,		true,		true },
	{ WEAPONTYPE_TASER,				60.0f,		true,		true },

	// everything else (error case)
	{ WEAPONTYPE_UNKNOWN,			60.0f,		true,		true },
};

CCSGO_QuickInventory::CCSGO_QuickInventory( panorama::CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_QuickInventory", this )
	, panorama::CPanel2D( pParent, pchID )
	, m_Capture( this, "QuickInventory", k_EGameInputCaptureMouse, false )
{
	RequireLoadLayout( "file://{resources}/layout/quickinventory.xml" );

	SetHitTestEnabled( true );
	SetHitTestChildrenEnabled( false );

	// we start out invisible; visible means we have input captured
	SetVisible( false );

	m_bDoLastInv = true;
	m_nLastKnownInvHash = 0;
	m_hSelectedPanel.Clear();
}

CCSGO_QuickInventory::~CCSGO_QuickInventory()
{
	DestroySegments();
}

bool CCSGO_QuickInventory::ShouldDraw( void )
{
	if ( !cl_drawhud.GetBool() || cl_draw_only_deathnotices.GetBool() || !CPanoramaHudElement::ShouldDraw() || g_bQuickInventoryOpen == false )
	{
		return false;
	}

	C_BasePlayer* pLocalPlayer = CHudElement::GetLocalOrObservedPlayer( 0 );
	if ( !pLocalPlayer || !pLocalPlayer->IsAlive() )
	{
		return false;
	}

	return true;
}

void CCSGO_QuickInventory::DestroySegments( void )
{
	FOR_EACH_VEC( m_vecQuickInvSegments, i )
	{
		quickInvSegment& segment = m_vecQuickInvSegments[i];
		if ( CPanel2D* pPanel = segment.m_pPanel.Get() )
		{
			delete pPanel;
			segment.m_pPanel = nullptr;
		}
	}
	m_vecQuickInvSegments.RemoveAll();
}

const quickInvSegment *CCSGO_QuickInventory::GetQuickInvSegmentFromMouseCoord( float flMouseX, float flMouseY )
{
	Vector2D vecCenter( GetActualLayoutWidth() / 2, GetActualLayoutHeight() / 2 );
	Vector2D vecMouse( flMouseX - vecCenter.x, flMouseY - vecCenter.y );

	if ( vecMouse.Length() < GetActualLayoutHeight() * cl_quickinventory_deadzone_size.GetFloat() )
		return NULL;

	float flMouseAng = RAD2DEG( atan2f( -vecMouse.x, vecMouse.y ) ) + 180.0f;

	FOR_EACH_VEC( m_vecQuickInvSegments, i )
	{
		float flLocalAng = flMouseAng;

		float flStart = m_vecQuickInvSegments[i].m_flAngleStart;
		float flWidth = MIN( m_vecQuickInvSegments[i].m_flAngleClockwiseWidth, 360.0f );
		float flEnd = flStart + flWidth;

		if ( flEnd > 360.0f )
		{
			float flAdjust = flEnd - 360.0f;

			flEnd -= flAdjust;
			flStart -= flAdjust;
			flLocalAng -= flAdjust;

			if ( flLocalAng < 0 )
				flLocalAng += 360.0f;
		}
		else if ( flStart < 0.0f )
		{
			float flAdjust = abs( flStart );

			flEnd += flAdjust;
			flStart += flAdjust;
			flLocalAng = fmodf( flLocalAng + flAdjust, 360.0f );
		}

		if ( flLocalAng >= flStart && flLocalAng < flEnd )
		{
			return &m_vecQuickInvSegments[i];
			break;
		}
	}

	return NULL;

}

void CCSGO_QuickInventory::QuickSelectWeapon( C_BaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return;

	// validate player

	C_BasePlayer* pLocalPlayer = CHudElement::GetLocalOrObservedPlayer( 0 );
	if ( !pLocalPlayer )
		return;

	if ( pWeapon->GetOwner() != pLocalPlayer || pWeapon == pLocalPlayer->GetActiveWeapon() )
		return;

	input->MakeWeaponSelection( pWeapon );
}

bool CCSGO_QuickInventory::OnCapturedMouseMove( IUIPanel *pPanel, float flMouseX, float flMouseY )
{
	CPanel2D* pPrevSelection = m_hSelectedPanel.Get();
	m_hSelectedPanel.Clear();

	if ( pPanel == UIPanel() )
	{
		const quickInvSegment *pInvSegment = GetQuickInvSegmentFromMouseCoord( flMouseX, flMouseY );

		if ( pInvSegment )
		{
			m_hSelectedPanel = pInvSegment->m_pPanel;
			QuickSelectWeapon( pInvSegment->m_hAssociatedWeapon.Get() );

			m_bDoLastInv = false;
		}
	}

	if ( pPrevSelection != m_hSelectedPanel.Get() )
	{
		UpdateHoverPanel( pPrevSelection, m_hSelectedPanel.Get() );
	}

	return false;
}

void CCSGO_QuickInventory::UpdateHoverPanel( CPanel2D *pPrev, CPanel2D *pCurSelection )
{
	static CPanoramaSymbol k_symQuickInventoryHover( "QuickInventorySegment--hover" );
	static CPanoramaSymbol k_symPropertyOnMouseOver( "onmouseover" );
	static CPanoramaSymbol k_symPropertyOnMouseOut( "onmouseout" );
	if ( pPrev )
	{
		pPrev->RemoveClass( k_symQuickInventoryHover );
		pPrev->DispatchPanelEvent( k_symPropertyOnMouseOut );
	}
	if ( pCurSelection )
	{
		pCurSelection->AddClass( k_symQuickInventoryHover );
		pCurSelection->DispatchPanelEvent( k_symPropertyOnMouseOver );
	}

	if ( m_rolloverSound.Length() > 0 )
	{
		UIEngine()->UISoundSystem()->PlaySound( m_rolloverSound.Get(), nullptr, panorama::k_ESoundType_Effects, 1.0f, 0.5f, 0.0f );
	}
}

bool CCSGO_QuickInventory::OnCapturedMouseButtonUp( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code )
{
	if ( code.m_MouseCode == MouseCode::MOUSE_LEFT )
	{	
		QuickInventoryClose();

		if ( m_clickSound.Length() > 0 )
		{
			UIEngine()->UISoundSystem()->PlaySound( m_clickSound.Get(), nullptr, panorama::k_ESoundType_Effects, 1.0f, 0.5f, 0.0f );
		}

	}
	return false;
}

bool CCSGO_QuickInventory::BSetProperties( const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties )
{
	static CPanoramaSymbol k_symSoundClick( "sound_click" );
	static CPanoramaSymbol k_symSoundRollover( "sound_rollover" );


	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( prop.m_symName == k_symSoundClick )
		{
			m_clickSound = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_symSoundRollover )
		{
			m_rolloverSound = prop.m_pchValue;
		}
	}

	return true;
}

bool CCSGO_QuickInventory::UpdateQuickInventoryRadial( void )
{
	C_BasePlayer* pLocalPlayer = CHudElement::GetLocalOrObservedPlayer( 0 );
	if ( !pLocalPlayer )
		return false;

	DestroySegments();

	CUtlVector<CWeaponCSBase*> vecWeps;
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CWeaponCSBase* pWeapon = (CWeaponCSBase*)pLocalPlayer->GetWeapon( i );
		if ( pWeapon )
		{
			vecWeps.AddToTail( pWeapon );
		}
	}

	if ( vecWeps.Count() < 2 )
		return false;

	// sort the weapons and allocate radial size

	struct QuickInvWepEntry_t
	{
		CWeaponCSBase* m_Wep;
		float m_flAngleWidth;
		int m_nQuickInvOrderIdx;
	};

	// use s_quickInvOrdering to sort the weapons into the desired order
	CUtlVector<QuickInvWepEntry_t> vecWepsSorted;
	for ( int i = 0; i < ARRAYSIZE( s_quickInvOrdering ); i++ )
	{
		FOR_EACH_VEC_BACK( vecWeps, n )
		{
			Assert( s_quickInvOrdering[i].m_wepType != WEAPONTYPE_UNKNOWN ); // we encountered a weapon type we don't know how to sort!

			CSWeaponType wepType = vecWeps[n]->GetWeaponType();
			if ( wepType == s_quickInvOrdering[i].m_wepType || s_quickInvOrdering[i].m_wepType == WEAPONTYPE_UNKNOWN )
			{	
				QuickInvWepEntry_t *pEntry = vecWepsSorted.AddToTailGetPtr();
				pEntry->m_Wep = vecWeps[n];
				pEntry->m_flAngleWidth = s_quickInvOrdering[i].m_flInitialAngleWidth;
				pEntry->m_nQuickInvOrderIdx = i;
				vecWeps.Remove( n );
			}
		}
	}

	if ( vecWepsSorted.Count() < 2 )
		return false;

	float flWorkingStartAngle = 360.0f - (vecWepsSorted.Head().m_flAngleWidth * 0.5f); // center the first segment

	// resize resizable segments
	float flUsedSpace = 0.0f;
	{
		float flSpaceToFitResizables = 360.0f;
		float flSpaceResizablesConsume = 0.0f;
		FOR_EACH_VEC( vecWepsSorted, i )
		{
			const quickInvOrdering_t *pInvOrdering = &s_quickInvOrdering[vecWepsSorted[i].m_nQuickInvOrderIdx];
			if ( !pInvOrdering->m_bAllowResize )
			{
				flSpaceToFitResizables -= vecWepsSorted[i].m_flAngleWidth;
			}
			else
			{
				flSpaceResizablesConsume += vecWepsSorted[i].m_flAngleWidth;
			}
		}
		if ( flSpaceResizablesConsume > 0 && flSpaceToFitResizables > 0 )
		{
			float flResizeRatio = flSpaceToFitResizables / flSpaceResizablesConsume;
			FOR_EACH_VEC( vecWepsSorted, i )
			{
				const quickInvOrdering_t *pInvOrdering = &s_quickInvOrdering[vecWepsSorted[i].m_nQuickInvOrderIdx];
				if ( pInvOrdering->m_bAllowResize )
				{
					vecWepsSorted[i].m_flAngleWidth = MIN( pInvOrdering->m_flInitialAngleWidth, vecWepsSorted[i].m_flAngleWidth * flResizeRatio );
				}

				flUsedSpace += vecWepsSorted[i].m_flAngleWidth;
			}
		}
	}
	flUsedSpace = clamp( flUsedSpace, 0.0f, 360.0f );

	// build radial menu itself
	
	bool bBeforeGap = true;
	FOR_EACH_VEC( vecWepsSorted, i )
	{
		CWeaponCSBase *pWeapon = vecWepsSorted[i].m_Wep;

		CPanel2D* pNewSegmentPanel = new CPanel2D( this, nullptr );
		pNewSegmentPanel->RequireLoadLayoutSnippet( "QuickInventorySegment" );

		pNewSegmentPanel->SetAttribute( "data-panel-id", i );

		quickInvSegment segment;
		segment.m_pPanel = pNewSegmentPanel;

		// consume gap
		const quickInvOrdering_t *pInvOrdering = &s_quickInvOrdering[vecWepsSorted[i].m_nQuickInvOrderIdx];
		if ( bBeforeGap && pInvOrdering->m_bPlaceAfterGap )
		{
			flWorkingStartAngle += (360.0f - flUsedSpace);
			bBeforeGap = false;
		}

		// position angular segment
		segment.m_flAngleStart = flWorkingStartAngle;
		segment.m_flAngleClockwiseWidth = vecWepsSorted[i].m_flAngleWidth;
		
		// assign weapon
		segment.m_hAssociatedWeapon = pWeapon;

		CUILength center( 50.0f, CUILength::k_EUILengthPercent );
		float flAngleSeparator = 0.8f;
		segment.m_pPanel->AccessStyleDirty()->SetRadialClip( true, center, center, segment.m_flAngleStart + segment.m_flAngleClockwiseWidth - flAngleSeparator, 360.0f - segment.m_flAngleClockwiseWidth + flAngleSeparator + flAngleSeparator );

		// position panel inside angular segment
		float flRotateTau = DEG2RAD( segment.m_flAngleStart + (segment.m_flAngleClockwiseWidth * 0.5f) );
		Vector2D vecSegmentCentroid = Vector2D( sinf( flRotateTau ), -cosf( flRotateTau ) );
		vecSegmentCentroid.x = 50.0f + vecSegmentCentroid.x * 33.0f;
		vecSegmentCentroid.y = 50.0f + vecSegmentCentroid.y * 33.0f;

		CPanel2D* pDataPanel = pNewSegmentPanel->FindChildTraverse( "segment-data" );
		pDataPanel->SetPosition(
			CUILength( vecSegmentCentroid.x, CUILength::k_EUILengthPercent ),
			CUILength( vecSegmentCentroid.y, CUILength::k_EUILengthPercent ),
			CUILength( 0, CUILength::k_EUILengthLength ) );

		// fill in data
		const CEconItemView* pItem = pWeapon->GetEconItemView();
		if ( pItem )
		{
			// set icon
			{
				const char* szWeaponIconName = pItem->GetStaticData()->GetDefinitionName();
				const char* skip_ = strchr( szWeaponIconName, '_' );
				if ( skip_ )
					szWeaponIconName = skip_ + 1;

				panorama::CImagePanel* pIcon = panorama::panel_cast<panorama::CImagePanel*>(pNewSegmentPanel->FindChildTraverse( "item-image" ));
				pIcon->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", szWeaponIconName ) );
			}
			
			// set name
			{
				char weaponNameUtf8[512];
				V_UnicodeToUTF8( pItem->GetItemName(), weaponNameUtf8, sizeof( weaponNameUtf8 ) );
				pNewSegmentPanel->SetDialogVariable( "WeaponIcon--name", weaponNameUtf8 );
			}
		}

		m_vecQuickInvSegments.AddToTail( segment );

		// advance to next segment
		flWorkingStartAngle = fmodf( flWorkingStartAngle + vecWepsSorted[i].m_flAngleWidth, 360.0f );
	}

	return true;
}

int CCSGO_QuickInventory::HashLocalPlayerWeapons( void )
{
	C_BasePlayer* pLocalPlayer = CHudElement::GetLocalOrObservedPlayer( 0 );
	if ( !pLocalPlayer )
		return 0;

	// FIXME: use changed/dirty flag on player

	int nWeps = 0;
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CWeaponCSBase* pWeapon = (CWeaponCSBase*)pLocalPlayer->GetWeapon( i );
		if ( pWeapon )
		{
			nWeps += (int)pWeapon->GetWeaponType();
		}
	}

	return nWeps;
}

void CCSGO_QuickInventory::Think( void )
{
	bool bShouldDraw = ShouldDraw();

	if ( BIsVisible() && !bShouldDraw )
	{
		// we are visible, and shouldn't be. Hide!

		m_Capture.Disable();
		UIInputEngine()->ReleaseInputCapture( this );

		if ( m_bDoLastInv && cl_quickinventory_lastinv.GetBool() )
		{
			engine->ExecuteClientCmd( "lastinv" );
		}

		SetVisible( false );
		return;
	}

	if ( !BIsVisible() && bShouldDraw )
	{
		// we are NOT visible, but we should be. Show!

		if ( UpdateQuickInventoryRadial() )
		{
			m_Capture.Enable();
			UIInputEngine()->SetInputCapture( this );

			m_bDoLastInv = true;

			m_nLastKnownInvHash = HashLocalPlayerWeapons();

			SetVisible( true );
		}

		return;
	}

	if ( BIsVisible() )
	{
		// check if we need to update the radial
		int nLocalPlayerWepHash = HashLocalPlayerWeapons();
		if ( nLocalPlayerWepHash != m_nLastKnownInvHash )
		{
			UpdateQuickInventoryRadial();
			m_nLastKnownInvHash = nLocalPlayerWepHash;
		}
	}
}

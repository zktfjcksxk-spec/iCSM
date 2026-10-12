//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include "ienginevgui.h"
#include "gameui_interface.h"
#include "basepanel.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/RadioButton.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/MessageBox.h"
#include "matsys_controls/colorpickerpanel.h"
#include "matsys_controls/vtfpreviewpanel.h"
#include "filesystem.h"
#include "keyvalues.h"
#include "engineinterface.h"
#include "gameui_interface.h"
#include "vstdlib/random.h"

#include "weapon_tablet.h"

#include "tablet_buymenu.h"

#include "c_cs_player.h"

CTabletBuymenu *g_pTabletBuyMenu = NULL;

using namespace vgui;

#include "weapon_tablet_buymenu.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CTabletBuymenu::CTabletBuymenu(vgui::Panel *parent) : BaseClass(parent, "TabletBuyMenu")
{
	SetDeleteSelfOnClose(true);
	
	C_CSPlayer *pCSPlayer = GetLocalTabletCSPlayer();

	if ( !pCSPlayer )
	{
		Close();
		return;
	}
	
	SetSizeable( false );

	LoadControlSettings("Resource/tablet_buymenu.res");
	
	int nButtonY = 125;

	int nMoney = 0;
	if ( pCSPlayer )
	{
		nMoney = pCSPlayer->GetAccount();
	}

	vgui::Label *pLblAvailableFunds = dynamic_cast<vgui::Label *>(FindChildByName( "lbl_availablefundsnumber" ));
	if ( pLblAvailableFunds )
	{
		char szMoney[64];
		V_sprintf_safe( szMoney, "$%i", nMoney );
		pLblAvailableFunds->SetText( szMoney );
	}

	//CTablet* pTablet = dynamic_cast<CTablet*>(pCSPlayer->Weapon_OwnsThisType( "weapon_tablet" ));

	for ( int i = 0; i < ARRAYSIZE( s_TabletBuyMenu ); i++ )
	{
		const TabletBuyMenuEntry *pBuyMenuEntry = &s_TabletBuyMenu[i];

		bool bButtonEnabled = nMoney >= pBuyMenuEntry->nPrice;

		vgui::Label *pLabel = new vgui::Label( this, "label", CFmtStr( "$%d", pBuyMenuEntry->nPrice ) );
		pLabel->SetContentAlignment( vgui::Label::a_east );
		pLabel->SetPos( 80, nButtonY );
		pLabel->SetSize( 255, 24 );

		vgui::Label *pLabelNumber = new vgui::Label( this, "number", pBuyMenuEntry->szHotKeyNumber );
		pLabelNumber->SetContentAlignment( vgui::Label::a_west );
		pLabelNumber->SetPos( 50, nButtonY );
		pLabelNumber->SetSize( 255, 24 );

		vgui::Button *pButton = new vgui::Button( this, "button", pBuyMenuEntry->szLocalizedToken );
		pButton->SetPos( 80, nButtonY );
		pButton->SetSize( 200, 24 );
		pButton->SetCommand( pBuyMenuEntry->szSpawnRuleGroupName );
		pButton->SetEnabled( bButtonEnabled );

		nButtonY += 30;
	}

	MoveToCenterOfScreen();

	vgui::VPANEL parentPanel = enginevgui->GetPanel( PANEL_INGAMESCREENS );
	if ( parentPanel )
		SetParent( parentPanel );

	SetVisible( true );
	Repaint();
	MoveToFront();

}

CTabletBuymenu::~CTabletBuymenu()
{
}

C_CSPlayer *CTabletBuymenu::GetLocalTabletCSPlayer( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	
	if ( !pLocalPlayer )
		return NULL;

	CBaseCombatWeapon* pCurrentWep = pLocalPlayer->GetActiveWeapon();
	CBaseCombatWeapon* pTablet = pLocalPlayer->Weapon_OwnsThisType( "weapon_tablet" );

	if ( pCurrentWep && pCurrentWep == pTablet )
	{
		return pLocalPlayer;
	}

	return NULL;
}

void CTabletBuymenu::OnClose( void )
{
	if ( g_pTabletBuyMenu )
	{
		engine->ServerCmd( "tabletbuy_close" );
	}

	BaseClass::OnClose();

	g_pTabletBuyMenu = NULL;
}

void CTabletBuymenu::OnCommand( char const *cmd )
{
	if ( !GetLocalTabletCSPlayer() )
	{
		Close();
		return;
	}

	for ( int i = 0; i < ARRAYSIZE( s_TabletBuyMenu ); i++ )
	{
		if ( !Q_stricmp( cmd, s_TabletBuyMenu[i].szSpawnRuleGroupName ) )
		{
			engine->ServerCmd( CFmtStr( "tabletbuy_buy_%s", s_TabletBuyMenu[i].szSpawnRuleGroupName ) );
			Close();
			return;
		}
	}

	if ( !Q_stricmp( cmd, "OnClose" ) )
	{
		Close();
	}
}

void CTabletBuymenu::OnMessage( const KeyValues *pParams, vgui::VPANEL fromPanel )
{
	if ( !GetLocalTabletCSPlayer() )
	{
		Close();
		return;
	}
	else if ( !Q_strcmp( "CloseFrameButtonPressed", pParams->GetName() ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnMessage( pParams, fromPanel );
	}
}

void CTabletBuymenu::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_ESCAPE || V_stristr( engine->Key_BindingForKey( code ), "buymenu" ) )
	{
		Close();
		return;
	}

	auto lambdaCheckBuyHotKey = [&]( int iKey ) -> bool
	{
		if ( iKey >= 0 && iKey < ARRAYSIZE( s_TabletBuyMenu ) )
		{
			engine->ServerCmd( CFmtStr( "tabletbuy_buy_%s", s_TabletBuyMenu[ iKey ].szSpawnRuleGroupName ) );

			return true;
		}

		return false;
	};

	// check if code pressed is buy hot key
	if ( lambdaCheckBuyHotKey( code - KEY_1 ) || lambdaCheckBuyHotKey( code - KEY_PAD_1 ) )
	{
		Close();
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}

CON_COMMAND_F( tabletbuy_open, "", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( g_pTabletBuyMenu )
		return;

	g_pTabletBuyMenu = new CTabletBuymenu( NULL );

	engine->ServerCmd( "tabletbuy_open" );
}

CON_COMMAND_F( tabletbuy_close, "", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( !g_pTabletBuyMenu )
		return;

	g_pTabletBuyMenu->Close();

	engine->ServerCmd( "tabletbuy_close" );
}

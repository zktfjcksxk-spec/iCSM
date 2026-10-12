//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Display selection of radio messages to broadcast
//
// SF differences :
//		* 
//
//=============================================================================//

#include "cbase.h"
#include "keyvalues.h"
#include "filesystem.h"

#include "c_cs_player.h"
#include "csgo_hudradio.h"

#include "panorama/iuiengine.h"
#include "panorama/controls/label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_HudRadio, CSGOHudRadio );

extern ConVar cl_drawhud;

KeyValues *CCSGO_HudRadio::m_pKVRadioPanel = nullptr;

static const float s_flCmdDefaultTimeout = 5.f;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRadio::CCSGO_HudRadio( panorama::CPanel2D *pParent, const char *pchID )
:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudRadio", this )
{
	static panorama::CPanoramaSymbol k_symRadioGroup( "RadioGroup" );
	static panorama::CPanoramaSymbol k_symRadioHeadingBackground( "RadioHeadingBackground" );
	static panorama::CPanoramaSymbol k_symRadioTextHeading( "RadioTextHeading" );
	static panorama::CPanoramaSymbol k_symRadioTextOptions( "RadioTextOptions" );
	static panorama::CPanoramaSymbol k_symRadioTextOptionsExit( "RadioTextOptionsExit" );


	SetHiddenBits( HIDEHUD_PLAYERDEAD );
	
	RequireLoadLayout( "file://{resources}/layout/hud/hudradio.xml" );

	m_pRadioPanel = RequireChildInLayoutFile( "RadioPanel" );
	m_pRadioPanelBG = RequireChildInLayoutFile( "RadioPanelBG" );

	m_bVisible = false;

	m_activeGroup = -1;
	m_CommandPanelTimer.Invalidate();

	//
	// Parse RadioPanel commands
	//
	m_pKVRadioPanel = new KeyValues( "RadioPanel" );

	if ( !m_pKVRadioPanel->LoadFromFile( g_pFullFileSystem, "resource/ui/RadioPanel.txt" ) )
	{
		AssertMsg( false, "failed to load radio messages!" );
		m_pKVRadioPanel->deleteThis();
		m_pKVRadioPanel = nullptr;
	}

	if ( m_pKVRadioPanel )
	{
		KeyValues *kvKey;

		for ( KeyValues *kvSection = m_pKVRadioPanel->GetFirstTrueSubKey(); kvSection; kvSection = kvSection->GetNextTrueSubKey() )
		{
			for ( KeyValues *kvGroup = kvSection->GetFirstTrueSubKey(); kvGroup; kvGroup = kvGroup->GetNextTrueSubKey() )
			{
				sRadioCommandList *pRadioCommandList = m_RadioCommandList.AddToTailGetPtr();

				pRadioCommandList->m_hotKey = -1;
				pRadioCommandList->m_title = nullptr;
				pRadioCommandList->m_flTimeout = s_flCmdDefaultTimeout;

				kvKey = kvGroup->FindKey( "hotkey" );
				if ( kvKey )
				{
					pRadioCommandList->m_hotKey = kvKey->GetInt();
				}

				kvKey = kvGroup->FindKey( "title" );
				if ( kvKey )
				{
					pRadioCommandList->m_title = kvKey->GetString();
				}

				kvKey = kvGroup->FindKey( "timeout" );
				if ( kvKey )
				{
					pRadioCommandList->m_flTimeout = kvKey->GetFloat();
				}

				char szGroupLabel[ 64 ];
				V_snprintf( szGroupLabel, 64, "%d%s", pRadioCommandList->m_hotKey, pRadioCommandList->m_title );

				panorama::CPanel2D *pCommandGroup = new panorama::CPanel2D( m_pRadioPanel, szGroupLabel );
				pCommandGroup->AddClass( k_symRadioGroup );
				pCommandGroup->SetVisible( false );

				panorama::CPanel2D *pGroupHeadingBackground = new panorama::CPanel2D( pCommandGroup, "" );
				pGroupHeadingBackground->AddClass( k_symRadioHeadingBackground );

				panorama::CLabel *pGroupHeading = new panorama::CLabel( pGroupHeadingBackground, pRadioCommandList->m_title );
				pGroupHeading->SetText( pRadioCommandList->m_title );
				pGroupHeading->AddClass( k_symRadioTextHeading );

				KeyValues *kvCommands = kvGroup->FindKey( "Commands" );

				if ( !kvCommands )
					continue; // no commands

				for ( KeyValues *kvCommand = kvCommands->GetFirstTrueSubKey(); kvCommand; kvCommand = kvCommand->GetNextTrueSubKey() )
				{
					sRadioCommand *pCommand = pRadioCommandList->m_RadioCommands.AddToTailGetPtr();
					pCommand->m_hotKey = -1;

					kvKey = kvCommand->FindKey( "hotkey" );
					if ( kvKey )
					{
						pCommand->m_hotKey = kvKey->GetInt();
					}

					kvKey = kvCommand->FindKey( "label" );
					if ( kvKey )
					{
						pCommand->m_label = kvKey->GetString();
					}

					kvKey = kvCommand->FindKey( "cmd" );
					if ( kvKey )
					{
						pCommand->m_cmd = kvKey->GetString();
					}

					char szNumber[ 4 ];
					V_snprintf( szNumber, 4, "%d. ", pCommand->m_hotKey );

					panorama::CPanel2D *pGroupLine = new panorama::CPanel2D( pCommandGroup, "" );
					pGroupLine->AddClass( k_symRadioTextOptions );

					panorama::CLabel *pGroupLineNum = new panorama::CLabel( pGroupLine, "" );
					pGroupLineNum->SetText( szNumber );

					panorama::CLabel *pGroupLineCommand = new panorama::CLabel( pGroupLine, "" );
					pGroupLineCommand->SetText( pCommand->m_label );
				}

				panorama::CLabel *pGroupLineExitText = new panorama::CLabel( pCommandGroup, "" );
				pGroupLineExitText->SetText( "#SFUI_Radio_Exit" );
				pGroupLineExitText->AddClass( k_symRadioTextOptions );			
				pGroupLineExitText->AddClass( k_symRadioTextOptionsExit );
			}
		}
	}

	// Make sure radio panel has its own input hierarchy and therefore will not lose focus
	// when pushing another input context (peer panels such as scoreboard, chat)
	SetTopOfInputContext( true );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	// Hide panel initially
	ShowPanel( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRadio::~CCSGO_HudRadio()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame before general key processing
//-----------------------------------------------------------------------------
void CCSGO_HudRadio::ProcessInput()
{
	// When timer elapses, fade out command panel
	if ( m_CommandPanelTimer.HasStarted() && m_CommandPanelTimer.IsElapsed() )
	{
		m_CommandPanelTimer.Invalidate();
		m_activeGroup = -1;
	}
	ShowPanel( m_CommandPanelTimer.HasStarted() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadio::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		ShowPanel( bActive );
	}

	CPanoramaHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: Return true if this hud element should be visible in the current hud state
//-----------------------------------------------------------------------------
bool CCSGO_HudRadio::ShouldDraw()
{
	return cl_drawhud.GetBool() && CPanoramaHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadio::ShowPanel( bool bShow )
{
	static panorama::CPanoramaSymbol k_symShowHudRadio( "ShowHudRadio" );

	if ( m_bVisible != bShow )
	{
		m_bVisible = bShow;

		SetHasClass( k_symShowHudRadio, m_bVisible );

		if ( m_bVisible )
		{
			// radio panel has its own input context (cf SetTopOfInputContext( true ) in constructor)
			// Therefore calling SetFocus will switch input context. Make sure to remove it from the stack
			// when hiding the radio panel
			SetFocus();
		}
		else
		{
			// Removing radio panel from the input context stack
			GetParentWindow()->UIWindowInput()->RemoveInputContext( this->UIPanel() );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadio::ShowRadioGroup( int nSetID )
{
	int nNewActiveGroup = nSetID;

	if ( nNewActiveGroup == m_activeGroup )
	{
		// Hide CCSGO_HudRadio panel
		ShowPanel( false );
		m_activeGroup = -1;
		m_CommandPanelTimer.Invalidate();
	}
	else if ( nNewActiveGroup >= 0 )
	{
		// Show CCSGO_HudRadio panel
		ShowPanel( true );

		// Hide all groups
		for ( int i=0; i<m_pRadioPanel->GetChildCount(); ++i )
		{
			m_pRadioPanel->GetChild( i )->SetVisible( false );
		}

		// Show the new group
		m_activeGroup = nNewActiveGroup;

		if ( m_activeGroup < m_pRadioPanel->GetChildCount() )
			m_pRadioPanel->GetChild( m_activeGroup )->SetVisible( true );

		// make sure no child of m_pRadioPanel get randomly deleted because this panel assumes the size is the same as command list
		Assert( m_RadioCommandList.Count() == m_pRadioPanel->GetChildCount() );

		float flTimeout = m_RadioCommandList.IsValidIndex( m_activeGroup ) ? m_RadioCommandList[ m_activeGroup ].m_flTimeout : s_flCmdDefaultTimeout;
		m_CommandPanelTimer.Start( flTimeout );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRadio::OnKeyDown( const panorama::KeyData_t &unichar )
{
	if ( m_activeGroup == -1 )
		return false;

	panorama::KeyCode kcode = unichar.m_KeyCode;

	if ( ( kcode == panorama::KEY_ESCAPE ) ||
		 ( kcode == panorama::KEY_0 ) ||
		 ( kcode == panorama::KEY_PAD_0 ) )
	{
		// Hide CCSGO_HudRadio panel
		ShowPanel( false );
		m_activeGroup = -1;
		m_CommandPanelTimer.Invalidate();

		return true;
	}

	int nCommandNumber = -1;

	if ( kcode > panorama::KEY_PAD_0 )
	{
		nCommandNumber = kcode - panorama::KEY_PAD_1;
	}
	else 
	{
		nCommandNumber = kcode - panorama::KEY_1;
	}

	if ( m_RadioCommandList.IsValidIndex( m_activeGroup ) &&
		 m_RadioCommandList[ m_activeGroup ].m_RadioCommands.IsValidIndex( nCommandNumber ) )
	{
		// exec radio command
		engine->ClientCmd( m_RadioCommandList[ m_activeGroup ].m_RadioCommands[ nCommandNumber ].m_cmd );

		// Hide CCSGO_HudRadio panel
		ShowPanel( false );
		m_activeGroup = -1;
		m_CommandPanelTimer.Invalidate();

		return true;
	}

	return false;
}
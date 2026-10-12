//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "clientui_dialog_panel.h"
#include "customgameeventmanager.h"
#include "inputsystem/iinputstacksystem.h"
#include "clientui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
REGISTER_PANEL2D_FACTORY( CClientUIDialogPanel, ClientUIDialogPanel );

DEFINE_PANORAMA_EVENT( ClientUI_CloseDialog );
DEFINE_PANORAMA_EVENT( ClientUI_FireOutput );
DEFINE_PANORAMA_EVENT( ClientUI_FireOutputStr );


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CClientUIDialogPanel::CClientUIDialogPanel( panorama::CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
{
	RegisterEventHandler( ClientUI_CloseDialog(), this, &CClientUIDialogPanel::EventClientUI_CloseDialog );
	RegisterEventHandler( ClientUI_FireOutput(), this, &CClientUIDialogPanel::EventClientUI_FireOutput );
	RegisterEventHandler( ClientUI_FireOutputStr(), this, &CClientUIDialogPanel::EventClientUI_FireOutputStr );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CClientUIDialogPanel::~CClientUIDialogPanel()
{
	EndDialog();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CClientUIDialogPanel::BeginDialog( IClientUIDialogListener *pListener, bool bIsUIPopup )
{
	Assert( !m_bActive );
	if ( m_bActive )
		return;

	m_pListener = pListener;
	m_bActive = true;
	m_bIsUIPopup = bIsUIPopup;
	if ( bIsUIPopup )
	{
		g_pClientUI->ToggleClientUIPopup( true );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CClientUIDialogPanel::EndDialog()
{
	if ( !m_bActive )
		return;

	m_bActive = false;
	if ( m_bIsUIPopup )
	{
		g_pClientUI->ToggleClientUIPopup( false );
	}
	if ( m_pListener )
	{
		m_pListener->OnDialogEnded();
		m_pListener = nullptr;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CClientUIDialogPanel::EventClientUI_CloseDialog()
{
	EndDialog();
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CClientUIDialogPanel::EventClientUI_FireOutput( const char *pOutput )
{
	if ( !m_bActive || !m_pListener )
		return true;

	m_pListener->OnDialogFireOutput( pOutput, "" );
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CClientUIDialogPanel::EventClientUI_FireOutputStr( const char *pOutput, const char *pStr )
{
	if ( !m_bActive || !m_pListener )
		return true;

	m_pListener->OnDialogFireOutput( pOutput, pStr );
	return true;
}


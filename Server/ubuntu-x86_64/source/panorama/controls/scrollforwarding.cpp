//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Panels for forwarding scroll input to other panels
//
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/scrollforwarding.h"
#include "panorama/controls/html.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CVerticalScrollForwardingPanel, VerticalScrollForwarding );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVerticalScrollForwardingPanel::CVerticalScrollForwardingPanel( panorama::CPanel2D *pParent, const char *pchPanelID ) : CPanel2D( pParent, pchPanelID )
{
	SetAcceptsInput( true );
}


//-----------------------------------------------------------------------------
// Purpose: Parse property from XML
//-----------------------------------------------------------------------------
bool CVerticalScrollForwardingPanel::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	if ( symName == "target" )
	{
		m_strTargetID = pchValue;
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Looks up and returns target panel
//-----------------------------------------------------------------------------
panorama::CPanel2D *CVerticalScrollForwardingPanel::GetTarget()
{
	CPanel2D *pPanel = m_ptrTarget.Get();
	if ( pPanel )
		return pPanel;

	if ( m_strTargetID.IsEmpty() )
		return NULL;

	// try to find child{
	pPanel = FindChildInLayoutFile( m_strTargetID );
	if ( pPanel )
	{
		m_ptrTarget = pPanel;
		pPanel->EnableAnalogStickScrolling( true );
	}

	return pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to convert to HTML control
//-----------------------------------------------------------------------------
panorama::CHTML *ToHTML( panorama::CPanel2D *pPanel )
{
	if ( pPanel && pPanel->GetPanelType() == panorama::CHTML::GetPanelSymbol() )
		return (panorama::CHTML*)pPanel;

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Forwards filtered input to target
//-----------------------------------------------------------------------------
bool CVerticalScrollForwardingPanel::OnGamePadAnalog( const GamePadData_t &code )
{
	CPanel2D *pTarget = GetTarget();
	panorama::CHTML *pHTML = ToHTML( pTarget );

	if ( pHTML && (code.m_GamePadCode == XK_STICK1_ANALOG || code.m_GamePadCode == STEAM_LEFTSTICK_ANALOG) )
	{
		pHTML->ProcessAnalogScroll( 0.0f, code.m_fValueY, UIInputEngine()->GetDeadZoneValue( XK_STICK1_ANALOG ), false );
			
		return true;
	}
	
	// generic panel
	if ( pTarget )
	{
		// panel2d scrolls on right stick. Instead of adding another piece of data to panel2d, swapping here as we want left stick scrolling. Can add param or update later
		// if necessary
		if ( code.m_GamePadCode == XK_STICK2_ANALOG )
			return true;

		if ( code.m_GamePadCode == XK_STICK1_ANALOG || code.m_GamePadCode == STEAM_LEFTSTICK_ANALOG )
		{
			GamePadData_t copy = code;
			copy.m_GamePadCode = XK_STICK2_ANALOG;
			pTarget->OnGamePadAnalog( copy );
			return true;
		}

		// unhandled
	}
	
	return BaseClass::OnGamePadAnalog( code );
}


//-----------------------------------------------------------------------------
// Purpose: Forwards filtered input to target
//-----------------------------------------------------------------------------
bool CVerticalScrollForwardingPanel::OnKeyDown( const KeyData_t &code )
{
	// check if we handle this key
	switch ( code.m_KeyCode )
	{
	case KEY_UP:
	case KEY_DOWN:
	case KEY_PAGEUP:
	case KEY_PAGEDOWN:
	case KEY_SPACE:
	case KEY_HOME:
	case KEY_END:
		break;			// handled below

	default:
		return BaseClass::OnKeyDown( code );
	}

	// need to forward
	CPanel2D *pTarget = GetTarget();
	if ( !pTarget )
		return BaseClass::OnKeyDown( code );

	panorama::CHTML *pHTML = ToHTML( pTarget );
	if ( pHTML )
		return pHTML->OnKeyDown( code );

	if ( code.m_KeyCode == KEY_PAGEUP )
		return pTarget->OnPageUp();

	if ( code.m_KeyCode == KEY_PAGEDOWN || code.m_KeyCode == KEY_SPACE )
		return pTarget->OnPageDown();

	if ( code.m_KeyCode == KEY_HOME )
	{
		pTarget->ScrollToTop();
		return true;
	}

	if ( code.m_KeyCode == KEY_END )
	{
		pTarget->ScrollToBottom();
		return true;
	}

	if ( code.m_KeyCode == KEY_UP )
		return pTarget->OnScrollUp();

	if ( code.m_KeyCode == KEY_DOWN )
		return pTarget->OnScrollDown();

	// shouldn't get here.. early out above
	return BaseClass::OnKeyDown( code );
}


//-----------------------------------------------------------------------------
// Purpose: forward selected input to the HTML panel
//-----------------------------------------------------------------------------
bool CVerticalScrollForwardingPanel::OnMouseWheel( const MouseData_t &code )
{
	CPanel2D *pTarget = GetTarget();
	if ( !pTarget )
		return BaseClass::OnMouseWheel( code );

	panorama::CHTML *pHTML = ToHTML( pTarget );
	if ( pHTML )
		return pHTML->OnMouseWheel( code );
	
	return pTarget->OnMouseWheel( code );
}

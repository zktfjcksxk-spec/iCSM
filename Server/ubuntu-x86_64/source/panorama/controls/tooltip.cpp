//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/tooltip.h"
#include "panorama/controls/label.h"
#include "contextui.h"
#include "uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D( CTooltip, Tooltip );
REGISTER_PANEL2D( CTextTooltip, TextTooltip );

DEFINE_PANORAMA_EVENT( TooltipVisible );
DEFINE_PANORAMA_EVENT( TooltipHidden );

static const float k_nMouseTooltipVerticalOffset = 20.0f;
static const float k_nMouseTooltipHorizontalOffset = 20.0f;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTooltip::CTooltip( IUIWindow *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	Init();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTooltip::CTooltip( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	Init();
}


//-----------------------------------------------------------------------------
// Purpose: init members
//-----------------------------------------------------------------------------
void CTooltip::Init()
{
	m_bReposition = false;
	m_ePrevPosition = k_EContextUIPositionUnset;

	SetTooltipVisible( false );

	RegisterForUnhandledEvent( TooltipVisible(), this, &CTooltip::EventBaseTooltipVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTooltip::~CTooltip()
{
	UnregisterForUnhandledEvent( TooltipVisible(), this, &CTooltip::EventBaseTooltipVisible );
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//-----------------------------------------------------------------------------
void CTooltip::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	// PANORAMA_USE_S1WRAPPER: Always update the position on layout traverse
	// Fixed issue on friendlist tooltips - tooltip wouldn't always show at the right position
	// Note: need to check the perf implications.
#ifndef PANORAMA_USE_S1WRAPPER
	if ( m_bReposition )
#endif
	{
		UpdatePosition();
		m_bReposition = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the panel that the tooltip should be relative to
//-----------------------------------------------------------------------------
void CTooltip::SetTooltipTarget( const CPanelPtr< IUIPanel > &panelPtr )
{
	m_pTooltipTarget = panelPtr;
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether the tooltip is currently visible
//-----------------------------------------------------------------------------
void CTooltip::SetTooltipVisible( bool bVisible )
{
	static const CPanoramaSymbol symTooltipVisible( "TooltipVisible" );

	bool bBecomingVisible = bVisible && !IsTooltipVisible();
	bool bBecomingHidden = !bVisible && IsTooltipVisible();

	SetHasClass( symTooltipVisible, bVisible );

	if ( bBecomingVisible )
	{
		DispatchEvent( TooltipVisible(), this );
	}
	else if ( bBecomingHidden )
	{
		DispatchEvent( TooltipHidden(), this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Ask whether tooltip is currently visible
//-----------------------------------------------------------------------------
bool CTooltip::IsTooltipVisible() const
{
	static const CPanoramaSymbol symTooltipVisible( "TooltipVisible" );
	return BHasClass( symTooltipVisible );
}

//-----------------------------------------------------------------------------
// Purpose: Sets position of panel relative to the mouse
//-----------------------------------------------------------------------------
void CTooltip::UpdatePosition()
{
	CPanel2D *pTargetPanel = NULL;
	if ( m_pTooltipTarget.Get() )
	{
		pTargetPanel = assert_cast< CPanel2D * > ( m_pTooltipTarget->ClientPtr() );
	}

	CContextUI::SLayoutTarget layoutTarget;
	if ( GetAttribute( k_szHasTooltipTargetPosition, 0 ) != 0 )
	{
		layoutTarget.flTargetX = GetAttribute( k_szTooltipTargetX, 0.0f );
		layoutTarget.flTargetY = GetAttribute( k_szTooltipTargetY, 0.0f );
		layoutTarget.flTargetWidth = GetAttribute( k_szTooltipTargetWidth, 0.0f );
		layoutTarget.flTargetHeight = GetAttribute( k_szTooltipTargetHeight, 0.0f );

		layoutTarget.bTargetVisible = true;
	}
	else
	{
		CContextUI::SetupTargetForPanel( layoutTarget, pTargetPanel, GetParentWindow() );
	}

	// Use the target panel's style information to determine how to position the tooltip relative
	// to it if possible. Fall back to the container's positioning information if necessary.
	CContextUI::SLayoutPosition layoutPosition;
	IUIPanelStyle *pStyle = pTargetPanel ? pTargetPanel->AccessStyle() : AccessStyle();
	pStyle->GetTooltipPositions( layoutPosition.ePositions );
	pStyle->GetTooltipBodyPosition( layoutPosition.lenHorizontalBodyPosition, layoutPosition.lenVerticalBodyPosition );
	pStyle->GetTooltipArrowPosition( layoutPosition.lenHorizontalArrowPosition, layoutPosition.lenVerticalArrowPosition );

#ifdef PANORAMA_USE_S1WRAPPER
	// Make sure to try the tooltip position from the previous frame first (instead of the first tooltip position
	// given by the target panel's style information) in order to avoid layout traversal every frame in case the tooltip can't fit at the 
	// first position given by the target panel's style information.
	//
	// Examples: if the default positions (style) are TooltipArrowLeft / TooltipArrowRight and if TooltipArrowRight
	// was selected the previous frame
	//		1 - First AddClass TooltipArrowLeft -> Invalidate style (leading to layout traversal the next frame) -> tooltip doesn't fit try next position
	//		2 - Try TooltipArrowRight -> Invalidate style -> tooltip fits
	//
	// The code below will ensure to try TooltipArrowRight (from the previous frame) first and therefore avoid 
	// any style invalidation leading to layout traversal the next frame.

	if ( ( m_ePrevPosition != k_EContextUIPositionUnset ) && ( layoutPosition.ePositions[0] != m_ePrevPosition) )
	{
		// Put position from previous frame in the first slot
		EContextUIPosition eFirstPosition = layoutPosition.ePositions[0];
		for ( EContextUIPosition &ePosition : layoutPosition.ePositions )
		{
			if ( ePosition == m_ePrevPosition )
			{
				ePosition = eFirstPosition;
			}
		}
		layoutPosition.ePositions[0] = m_ePrevPosition;
	}
#endif	// PANORAMA_USE_S1WRAPPER

	m_ePrevPosition = CContextUI::LayoutContextUI( this, layoutTarget, layoutPosition );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a tooltip visible event is received
//-----------------------------------------------------------------------------
bool CTooltip::EventBaseTooltipVisible( const CPanelPtr< IUIPanel > &pPanel )
{
	CPanel2D *pOther = ToPanel2D( pPanel.Get() );
	if ( this != pOther )
		SetTooltipVisible( false );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a tooltip visible event is received
//-----------------------------------------------------------------------------
void CTooltip::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "GetTooltipTarget", PANORAMA_DELEGATE( &CTooltip::GetTooltipTarget ) );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextTooltip::CTextTooltip( IUIWindow *pParent, const char *pchName, bool bInit /* = true */ ) : CTooltip( pParent, pchName )
{
	if ( bInit )
		Init();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextTooltip::CTextTooltip( CPanel2D *pParent, const char *pchName,  bool bInit /* = true */ ) : CTooltip( pParent, pchName )
{
	if (bInit)
		Init();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextTooltip::~CTextTooltip()
{
}

//-----------------------------------------------------------------------------
// Purpose: init members
//-----------------------------------------------------------------------------
void CTextTooltip::Init()
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/tooltip.xml" ) );
	m_pText = ( CLabel* )FindChildInLayoutFile( "Text" );
}

//-----------------------------------------------------------------------------
// Purpose: Sets text shown in tooltip
//-----------------------------------------------------------------------------
void CTextTooltip::SetText( const char *pchText, CLabel::ETextType eTextType )
{
	m_pText->SetText( pchText, eTextType );
}

CPanel2D *CTextTooltip::Clone()
{
	if ( !IsClonable() )
	{
		AssertMsg( false, "Panel can't be cloned (panel type or child not clonable)" );
		return NULL;
	}

	CTextTooltip *pPanel = NULL;
	if ( GetParent() )
	{
		pPanel = new CTextTooltip( GetParent(), NULL, false );
	}
	else
	{
		pPanel = new CTextTooltip( GetParentWindow(), NULL, false );
	}

	InitClonedPanel( pPanel );

	return pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Adds class specific data to clone a panel
//-----------------------------------------------------------------------------
void CTextTooltip::InitClonedPanel( CPanel2D *pClone )
{
	BaseClass::InitClonedPanel( pClone );

	m_pText = ( CLabel* )FindChildInLayoutFile( "Text" );
}


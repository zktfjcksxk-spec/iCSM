//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/uievents.h"
#include "panorama/controls/progressbar.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CProgressBar, ProgressBar );

//-----------------------------------------------------------------------------
// Purpose: ctor; sets up default ranges
//-----------------------------------------------------------------------------
CProgressBar::CProgressBar( panorama::CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_flMin( 0.0f )
	, m_flMax( 1.0f )
	, m_flCur( 0.0f )
{
	typedef CFmtStrN<64> CFmtStr64;
	m_ppanelLeft = new CPanel2D( this, CFmtStr64( "%s_Left", pchID ).String() );
	m_ppanelLeft->AddClass( "ProgressBarLeft" );
	m_ppanelRight = new CPanel2D( this, CFmtStr64( "%s_Right", pchID ).String() );
	m_ppanelRight->AddClass( "ProgressBarRight" );
}


//-----------------------------------------------------------------------------
// Purpose: dtor
//-----------------------------------------------------------------------------
CProgressBar::~CProgressBar()
{
}


//-----------------------------------------------------------------------------
// Purpose: layout - sets sizes of child panels
//-----------------------------------------------------------------------------
void CProgressBar::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	// Value, normalized between 0.0 and 1.0
	float flValueNormalized = ( m_flCur - m_flMin ) / ( m_flMax - m_flMin );

	// Account for borders/padding
	float flInsetLeft, flInsetTop, flInsetRight, flInsetBottom;
	AccessStyle()->GetContentInset( flFinalWidth, flFinalHeight, false, flInsetLeft, flInsetTop, flInsetRight, flInsetBottom );
	float flContentWidth = ( flFinalWidth - ( flInsetLeft + flInsetRight ) ) / GetActualUIScaleX();

	CUILength widthLeft( flValueNormalized * flContentWidth, CUILength::k_EUILengthLength );
	CUILength widthRight( ( 1.0f - flValueNormalized ) * flContentWidth, CUILength::k_EUILengthLength );
	CUILength height( 100.0f, CUILength::k_EUILengthPercent );

	// set "left" panel size
	m_ppanelLeft->SetSize( widthLeft, height );

	// set "right" panel size/placement
	m_ppanelRight->SetSize( widthRight, height );
 	m_ppanelRight->SetPosition( widthLeft, CUILength( 0, CUILength::k_EUILengthLength ), CUILength( 0, CUILength::k_EUILengthLength ) );

	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
}


//-----------------------------------------------------------------------------
// Purpose: set property from xml
//-----------------------------------------------------------------------------
bool CProgressBar::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symMin( "min" );
	static const CPanoramaSymbol k_symMax( "max" );
	static const CPanoramaSymbol k_symValue( "value" );

	if ( symName == k_symMin )
	{
		SetMin( V_atof( pchValue ) );
		return true;
	}
	else if ( symName == k_symMax )
	{
		SetMax( V_atof( pchValue ) );
		return true;
	}
	else if ( symName == k_symValue )
	{
		SetValue( V_atof( pchValue ) );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CProgressBar::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "value", PANORAMA_DELEGATE( &CProgressBar::GetValue ), PANORAMA_DELEGATE( &CProgressBar::SetValue ) );
	RegisterJSAccessor( "min", PANORAMA_DELEGATE( &CProgressBar::GetMin ), PANORAMA_DELEGATE( &CProgressBar::SetMin ) );
	RegisterJSAccessor( "max", PANORAMA_DELEGATE( &CProgressBar::GetMax ), PANORAMA_DELEGATE( &CProgressBar::SetMax ) );
}
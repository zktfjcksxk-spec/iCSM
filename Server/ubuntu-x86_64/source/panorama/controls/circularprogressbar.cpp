//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/uievents.h"
#include "panorama/controls/circularprogressbar.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCircularProgressBar, CircularProgressBar );

//-----------------------------------------------------------------------------
// Purpose: ctor; sets up default ranges
//-----------------------------------------------------------------------------
CCircularProgressBar::CCircularProgressBar( panorama::CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_flMin( 0.0f )
	, m_flMax( 1.0f )
	, m_flCur( 0.0f )
{
	typedef CFmtStrN<64> CFmtStr64;
	m_ppanelBG = new CPanel2D( this, CFmtStr64( "%s_BG", pchID ).String() );
	m_ppanelBG->AddClass( "CircularProgressBarBG" );
	m_ppanelFG = new CPanel2D( this, CFmtStr64( "%s_FG", pchID ).String() );
	m_ppanelFG->AddClass( "CircularProgressBarFG" );

	UpdateRadialClip();
}


//-----------------------------------------------------------------------------
// Purpose: dtor
//-----------------------------------------------------------------------------
CCircularProgressBar::~CCircularProgressBar()
{
}


//-----------------------------------------------------------------------------
// Purpose: set property from xml
//-----------------------------------------------------------------------------
bool CCircularProgressBar::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
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
void CCircularProgressBar::SetupJavascriptObjectTemplate()
{
	RegisterJSAccessor( "value", PANORAMA_DELEGATE( &CCircularProgressBar::GetValue ), PANORAMA_DELEGATE( &CCircularProgressBar::SetValue ) );
	RegisterJSAccessor( "min", PANORAMA_DELEGATE( &CCircularProgressBar::GetMin ), PANORAMA_DELEGATE( &CCircularProgressBar::SetMin ) );
	RegisterJSAccessor( "max", PANORAMA_DELEGATE( &CCircularProgressBar::GetMax ), PANORAMA_DELEGATE( &CCircularProgressBar::SetMax ) );
}


//-----------------------------------------------------------------------------
void CCircularProgressBar::UpdateRadialClip()
{
	// width of our range
	float dflRange = m_flMax - m_flMin;

	// absolute delta vs m_flMin
	float dflValue = m_flCur - m_flMin;

	// normalize delta to [ 0.0, 360.0 ]
	dflValue *= 360.0f / dflRange;

	CUILength center( 50.0f, CUILength::k_EUILengthPercent );
	m_ppanelFG->AccessStyleDirty()->SetRadialClip( true, center, center, 0.0f, dflValue );
}
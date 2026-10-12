//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "panorama/layout/uilength.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

static CUILength g_lenZero( 0.0f, CUILength::k_EUILengthLength );
static CUILength g_len100Percent( 100.0f, CUILength::k_EUILengthPercent );

//-----------------------------------------------------------------------------
// Purpose: Helper for doing lerp on ui length values, converting % and such
//-----------------------------------------------------------------------------
CUILength panorama::LerpUILength( float flProgress, CUILength start, CUILength end, float flPixelSize )
{
	// If neither are set, just leave alone
	if ( !start.IsSet() && !end.IsSet() )
		return end;

	// If only one was unset, initialize to zero length
	if ( !start.IsSet() )
		start.SetLength( 0.0f );

	if ( !end.IsSet() )
		end.SetLength( 0.0f );

	// If "fit-children" is involved, then we snap rather than interpolating
	if ( start.IsFitChildren()  || end.IsFitChildren() )
	{
		return end;
	}
	else if ( start.IsFillParentFlow() || end.IsFillParentFlow() )
	{
		// If "fill-parent-flow" is involved, then we can interpolate only if both are of that type, 
		// otherwise we snap
		if ( !start.IsFillParentFlow() || !end.IsFillParentFlow() )
		{
			return end;
		}
	}
	else
	{
		// If we get here, then both are percent or pixels, we can convert units to enable interpolating
		if ( start.IsPercent() )
			end.ConvertToPercent( flPixelSize );
		else if ( start.IsLength() )
			end.ConvertToLength( flPixelSize );
	}

	Assert( start.GetType() == end.GetType() && 
		( start.GetType() == CUILength::k_EUILengthLength || 
		  start.GetType() == CUILength::k_EUILengthPercent || 
		  start.GetType() == CUILength::k_EUILengthWidthPercentage || 
		  start.GetType() == CUILength::k_EUILengthHeightPercentage || 
		  start.GetType() == CUILength::k_EUILengthFillParentFlow ) );

	float flOutValue = Lerp( flProgress, start.GetValue(), end.GetValue() );

	CUILength out;
	out.Set( flOutValue, start.GetType() );
	return out;
}

//-----------------------------------------------------------------------------
// Purpose: Helper for a zero UI length
//-----------------------------------------------------------------------------
/*static*/ const CUILength &CUILength::ZeroLength()
{
	return g_lenZero;
}

//-----------------------------------------------------------------------------
// Purpose: Helper for a 100% UI length
//-----------------------------------------------------------------------------
/*static*/ const CUILength &CUILength::OneHundredPercent()
{
	return g_len100Percent;
}

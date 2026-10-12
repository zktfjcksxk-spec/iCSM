//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "panorama/layout/fillbrush.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: Interpolate from brush collection towards target collection
//-----------------------------------------------------------------------------
void CFillBrushCollection::Interpolate( float flActualWidth, float flActualHeight, const CFillBrushCollection &target, float flProgress )
{
	if( flProgress < 0.000001f )
		return;

	if ( flProgress >= 1.0f )
	{
		*this = target;
		return;
	}

	// Need a copy we can modify
	CFillBrushCollection targetCopy;
	targetCopy = target;

	// Normalize to the same number of brushes per collection
	CFillBrush transparent;
	while ( targetCopy.GetBrushCount() > GetBrushCount() )
		AddFillBrush( transparent, 1.0 );

	while ( GetBrushCount() > targetCopy.GetBrushCount() )
		targetCopy.AddFillBrush( transparent, 1.0 );

	// Interpolate each brush now
	BrushVec_t &vecThis = AccessBrushes();
	BrushVec_t &vecTarget = targetCopy.AccessBrushes();

	CopyableBrushVec_t vecOutput;
	vecOutput.EnsureCapacity( vecThis.Count() );

	for ( uint32 i=0; i < GetBrushCount(); ++i )
	{
		FillBrush_t &out = vecOutput[vecOutput.AddToTail()];

		out.m_Opacity = Lerp( flProgress, vecThis[i].m_Opacity, vecTarget[i].m_Opacity );
		if ( !vecThis[i].m_Brush.Interpolate( flActualWidth, flActualHeight, vecTarget[i].m_Brush, flProgress ) )
		{
			// If the brushes couldn't do a smart interpolate, then just interpolate alpha and cross fade
			out.m_Opacity = vecThis[i].m_Opacity * (1.0f - flProgress );
			out.m_Brush = vecThis[i].m_Brush;

			FillBrush_t &outTwo = vecOutput[vecOutput.AddToTail()];
			outTwo.m_Opacity = vecTarget[i].m_Opacity * (flProgress );
			outTwo.m_Brush = vecTarget[i].m_Brush;
		}
		else
		{
			out.m_Brush = vecThis[i].m_Brush;
		}
	}

	m_vecFillBrushes = vecOutput;
}


//-----------------------------------------------------------------------------
// Purpose: Equality check
//-----------------------------------------------------------------------------
bool CFillBrushCollection::operator==( const CFillBrushCollection &rhs ) const
{
	if ( m_vecFillBrushes.Count() != rhs.m_vecFillBrushes.Count() )
		return false;

	for ( int i = 0; i < m_vecFillBrushes.Count(); ++i )
	{
		if ( m_vecFillBrushes[i] != rhs.m_vecFillBrushes[i] )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Convert fill to a linear gradient which is equivalent in appearance
//-----------------------------------------------------------------------------
void CFillBrush::ConvertToLinearGradient()
{
	if ( m_eType == k_EStrokeTypeLinearGradient )
		return;

	// Can only convert fill colors
	Assert( m_eType == k_EStrokeTypeFillColor );

	CUILength zero( 0, CUILength::k_EUILengthLength );
	CGradientColorStop fillColorStop( 0.0, m_FillColor );
	CUtlVector< CGradientColorStop > vecColorStops;
	vecColorStops.EnsureCapacity( 2 );
	vecColorStops.AddToTail( fillColorStop );
	vecColorStops.AddToTail( fillColorStop );

	SetToLinearGradient( zero, zero, zero, zero, vecColorStops );
}


//-----------------------------------------------------------------------------
// Purpose: Convert fill to a radial gradient which is equivalent in appearance
//-----------------------------------------------------------------------------
void CFillBrush::ConvertToRadialGradient()
{
	if ( m_eType == k_EStrokeTypeRadialGradient )
		return;

	// Can only convert fill colors
	Assert( m_eType == k_EStrokeTypeFillColor );

	CUILength zero( 0, CUILength::k_EUILengthLength );
	CGradientColorStop fillColorStop( 0.0, m_FillColor );
	CUtlVector< CGradientColorStop > vecColorStops;
	vecColorStops.EnsureCapacity( 2 );
	vecColorStops.AddToTail( fillColorStop );
	vecColorStops.AddToTail( fillColorStop );

	SetToRadialGradient( zero, zero, zero, zero, zero, zero, vecColorStops );
}


//-----------------------------------------------------------------------------
// Purpose: Normalize the stop count for the gradient stop vector to what is desired
//-----------------------------------------------------------------------------
void CFillBrush::NormalizeStopCount( CUtlVector<CGradientColorStop> &vec, int nStopsNeeded, Color defaultColor )
{
	AssertMsg( nStopsNeeded >= vec.Count(), "Can't decrease number of stops!" );
	if ( nStopsNeeded > vec.Count() )
	{
		int stopsLeft = nStopsNeeded - vec.Count() ;

		CGradientColorStop stop;
		stop.SetPosition( 0.0 );
		stop.SetColor( defaultColor );
		
		int iLastMatchedStop = -1;
		while( stopsLeft-- )
		{
			++iLastMatchedStop;
			if ( iLastMatchedStop < vec.Count()-1 )
				stop = vec[ iLastMatchedStop ];

			vec.AddToTail( stop );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate from brush towards target
//-----------------------------------------------------------------------------
bool CFillBrush::Interpolate( float flActualWidth, float flActualHeight, CFillBrush &target, float flProgress )
{
	if ( flProgress < 0.000001f )
		return true;

	if ( flProgress >= 1.0f )
	{
		*this = target;
		return true;
	}

	if ( GetType() == k_EStrokeTypeFillColor && target.GetType() == k_EStrokeTypeFillColor )
	{
		// Simple color interpolation
		m_FillColor.SetColor(
				Lerp( flProgress, m_FillColor.r(), target.m_FillColor.r() ),
				Lerp( flProgress, m_FillColor.g(), target.m_FillColor.g() ),
				Lerp( flProgress, m_FillColor.b(), target.m_FillColor.b() ),
				Lerp( flProgress, m_FillColor.a(), target.m_FillColor.a() )
			);

		return true;
	}
	else if ( GetType() == k_EStrokeTypeParticleSystem && target.GetType() == k_EStrokeTypeParticleSystem )
	{
		// Any particle system can interpolate to any other
		m_pParticleSystem->Interpolate( flProgress, *target.m_pParticleSystem );
		return true;
	}
	else if ( ( GetType() == k_EStrokeTypeLinearGradient || GetType() == k_EStrokeTypeFillColor ) && ( target.GetType() == k_EStrokeTypeLinearGradient || target.GetType() == k_EStrokeTypeFillColor ) )
	{
		ConvertToLinearGradient();
		target.ConvertToLinearGradient();

		int nStopsRequired = MAX( AccessStopColors().Count(), target.AccessStopColors().Count() );
		NormalizeStopCount( m_pLinearGradient->AccessMutableStopColors(), nStopsRequired, m_FillColor );
		target.NormalizeStopCount( target.m_pLinearGradient->AccessMutableStopColors(), nStopsRequired, target.m_FillColor );


		CUILength startXA, startXB, startYA, startYB;
		CUILength endXA, endXB, endYA, endYB;

		m_pLinearGradient->GetStartPoint( startXA, startYA );
		m_pLinearGradient->GetEndPoint( endXA, endYA );

		target.m_pLinearGradient->GetStartPoint( startXB, startYB );
		target.m_pLinearGradient->GetEndPoint( endXB, endYB );

		startXA = LerpUILength( flProgress, startXA, startXB, flActualWidth );
		endXA = LerpUILength( flProgress, endXA, endXB, flActualWidth );

		startYA = LerpUILength( flProgress, startYA, startYB, flActualHeight );
		endYA = LerpUILength( flProgress, endYA, endYB, flActualHeight );

		m_pLinearGradient->SetControlPoints( startXA, startYA, endXA, endYA );

		CUtlVector<CGradientColorStop> &vecThis = m_pLinearGradient->AccessMutableStopColors();
		CUtlVector<CGradientColorStop> &vecTarget = target.m_pLinearGradient->AccessMutableStopColors();

		FOR_EACH_VEC( vecThis, i )
		{
			vecThis[i].SetPosition( Lerp( flProgress, vecThis[i].GetPosition(), vecTarget[i].GetPosition() ) );
			
			Color a = vecThis[i].GetColor();
			Color b = vecTarget[i].GetColor();
			Color c = Color( Lerp( flProgress, a.r(), b.r() ),
				Lerp( flProgress, a.g(), b.g() ),
				Lerp( flProgress, a.b(), b.b() ),
				Lerp( flProgress, a.a(), b.a() ) );
			vecThis[i].SetColor( c );
		}

		return true;
	}
	else if ( ( GetType() == k_EStrokeTypeRadialGradient || GetType() == k_EStrokeTypeFillColor ) && ( target.GetType() == k_EStrokeTypeRadialGradient || target.GetType() == k_EStrokeTypeFillColor ) )
	{
		ConvertToRadialGradient();
		target.ConvertToRadialGradient();

		int nStopsRequired = MAX( AccessStopColors().Count(), target.AccessStopColors().Count() );
		NormalizeStopCount( m_pRadialGradient->AccessMutableStopColors(), nStopsRequired, m_FillColor );
		target.NormalizeStopCount( target.m_pRadialGradient->AccessMutableStopColors(), nStopsRequired, target.m_FillColor );


		CUILength xA, xB, yA, yB;

		m_pRadialGradient->GetCenterPoint( xA, yA );
		target.m_pRadialGradient->GetCenterPoint( xB, yB );

		xA = LerpUILength( flProgress, xA, xB, flActualWidth );
		yA = LerpUILength( flProgress, yA, yB, flActualHeight );

		m_pRadialGradient->SetCenterPoint( xA, yA );

		m_pRadialGradient->GetOffsetDistance( xA, yA );
		target.m_pRadialGradient->GetOffsetDistance( xB, yB );

		xA = LerpUILength( flProgress, xA, xB, flActualWidth );
		yA = LerpUILength( flProgress, yA, yB, flActualHeight );

		m_pRadialGradient->SetOffsetDistance( xA, yA );

		m_pRadialGradient->GetRadii( xA, yA );
		target.m_pRadialGradient->GetRadii( xB, yB );

		xA = LerpUILength( flProgress, xA, xB, flActualWidth );
		yA = LerpUILength( flProgress, yA, yB, flActualHeight );

		m_pRadialGradient->SetRadii( xA, yA );


		CUtlVector<CGradientColorStop> &vecThis = m_pRadialGradient->AccessMutableStopColors();
		CUtlVector<CGradientColorStop> &vecTarget = target.m_pRadialGradient->AccessMutableStopColors();

		FOR_EACH_VEC( vecThis, i )
		{
			vecThis[i].SetPosition( Lerp( flProgress, vecThis[i].GetPosition(), vecTarget[i].GetPosition() ) );

			Color a = vecThis[i].GetColor();
			Color b = vecTarget[i].GetColor();
			Color c = Color( Lerp( flProgress, a.r(), b.r() ),
				Lerp( flProgress, a.g(), b.g() ),
				Lerp( flProgress, a.b(), b.b() ),
				Lerp( flProgress, a.a(), b.a() ) );
			vecThis[i].SetColor( c );
		}

		return true;
	}
	else
	{
		// We can't do an interpolation of the brushes, higher level collection should use both the source and target
		// and perform a cross fade.  
		return false;
	}
}

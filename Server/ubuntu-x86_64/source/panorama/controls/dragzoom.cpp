//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/dragzoom.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDragZoom, DragZoom )

//-----------------------------------------------------------------------------
// Use for a panel where your content is larger than your viewport. The DragZoom panel is your parent panel
// and the content panel is accessed by calling "GetContentPanel"
// bStartScaled: If you want the content panel scaled to fit inside the parent window
// nNumMouseWheelTicks: Number of mousewheel ticks it takes to go from completely zoomed out to completely zoomed in
//-----------------------------------------------------------------------------
CDragZoom::CDragZoom( panorama::CPanel2D *pParent, const char *pchID, bool bStartScaled, int nNumMouseWheelTicks )
	: CPanel2D( pParent, pchID )
	, m_bIsScaled( bStartScaled )
	, m_unNumMouseWheelTicks( nNumMouseWheelTicks )
{
	m_bFirstLoad = true;

	// Set default position and scale values
	m_flLastPositionY = 0.0f;
	m_flLastPositionX = 0.0f;
	m_flLastScale = 1.0f;
	
	// Set default mouse values
	m_bIsMouseDown = false;
	m_flLastMouseX = 0.0f;
	m_flLastMouseY = 0.0f;
	m_nMouseDelta = 0;

	m_flExtraZoomOut = 1.0f;
	m_flLayoutWidth = 0.0f;
	m_flLayoutHeight = 0.0f;
	m_flContentWidth = 0.0f;
	m_flContentHeight = 0.0f;
	m_flMinScale = 1.0f;

	// Set the zoom position to the 
	if ( m_bIsScaled )
	{
		m_unZoomPosition = 0;
	}
	else
	{
		m_unZoomPosition = m_unNumMouseWheelTicks;
	}

	// Set so parent panel accepts input
	SetAcceptsInput( true );
	SetMouseTracking( true );

	RegisterEventHandler( PanelLoaded(), this, &CDragZoom::OnPanelLoaded );

	// Create content panel and set HitTest to false so only the parent grabs the clicks
	m_pSubPanel = new CPanel2D( this, "DragZoomContentPanel" );
	m_pSubPanel->SetHitTestEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDragZoom::~CDragZoom( )
{
}

//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------

bool CDragZoom::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	static CPanoramaSymbol symExtraZoom( "extrazoomout" );
	static CPanoramaSymbol symStartZoomed( "startzoomed" );
	static CPanoramaSymbol symMouseWheelTicks( "mousewheeltickcount" );

	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( prop.m_symName == symExtraZoom )
		{
			m_flExtraZoomOut = V_atof( prop.m_pchValue );
			continue;
		}
		else if ( prop.m_symName == symStartZoomed )
		{
			m_bIsScaled = ( atoi( prop.m_pchValue ) == 0 );
			continue;
		}
		else if ( prop.m_symName == symMouseWheelTicks )
		{
			m_unNumMouseWheelTicks = atoi( prop.m_pchValue );
			if ( m_unNumMouseWheelTicks <= 0 )
			{
				m_unNumMouseWheelTicks = 1;
			}
			continue;
		}
	}

	return BaseClass::BSetProperties( vecProperties );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the horizontal position based on a percentage. Expects value sent in to already be a decimal (0.8 = 80%)
//-----------------------------------------------------------------------------
void CDragZoom::SetHorizontalPositionPercentage( float flHorizontalPercentage )
{
	SetHorizontalPosition( m_flContentWidth * flHorizontalPercentage );
	//float flRequestedPosition = ( m_flContentWidth / 2 ) - ( m_flContentWidth * flHorizontalPercentage );
	//float flNewPosition = flRequestedPosition - m_flLastPositionX;
	//ApplyUpdatedTransform( flNewPosition, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the vertical position based on a percentage. Expects value sent in to already be a decimal (0.8 = 80%)
//-----------------------------------------------------------------------------
void CDragZoom::SetVerticalPositionPercentage( float flVerticalPercentage )
{
	SetVerticalPosition( m_flContentHeight * flVerticalPercentage );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the horizontal position based on the total content width.
// Example: If your content width is 1000 and you set the position to 500, it would horizontally center your content in the parent panel
//-----------------------------------------------------------------------------
void CDragZoom::SetHorizontalPosition( float flRequestedPositionX )
{
	flRequestedPositionX = ( m_flContentWidth / 2 ) - flRequestedPositionX;
	float flNewPositionX = flRequestedPositionX - m_flLastPositionX;
	ApplyUpdatedTransform( flNewPositionX, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the vertical position based on the total content width.
// Example: If your content height is 1000 and you set the position to 500, it would vertically center your content in the parent panel
//-----------------------------------------------------------------------------
void CDragZoom::SetVerticalPosition( float flRequestedPositionY )
{
	flRequestedPositionY = ( m_flContentHeight / 2 ) - flRequestedPositionY;
	float flNewPositionY = flRequestedPositionY - m_flLastPositionY;
	ApplyUpdatedTransform( 0, flNewPositionY );
}

//-----------------------------------------------------------------------------
// Purpose: Use to access content panel
//-----------------------------------------------------------------------------
CPanel2D *CDragZoom::GetContentPanel( )
{
	return m_pSubPanel;
}

bool CDragZoom::CheckContentPanelSize()
{
	UpdateContentLayoutValues();
	// If our content panel isn't larger than our parent panel, do nothing
	return ( m_flLayoutWidth >= ( m_flContentWidth * m_flLastScale ) && m_flLayoutHeight >= ( m_flContentHeight * m_flLastScale ) );
}

//-----------------------------------------------------------------------------
// Purpose: Handle cursor when enabled
//-----------------------------------------------------------------------------
EMouseCursors CDragZoom::GetMouseCursor()
{
	if ( m_bIsMouseDown )
		return eMouseCursor_Hand_Closed;
	else
		return eMouseCursor_Hand;
}

void CDragZoom::OnMouseMove( float flMouseX, float flMouseY )
{
	if ( m_flLastMouseX == 0.0 )
	{
		m_flLastMouseX = flMouseX;
	}

	if ( m_flLastMouseY == 0.0 )
	{
		m_flLastMouseY = flMouseY;
	}

	float flMouseDeltaX = flMouseX - m_flLastMouseX;
	m_flLastMouseX = flMouseX;

	float flMouseDeltaY = flMouseY - m_flLastMouseY;
	m_flLastMouseY = flMouseY;

	if ( CheckContentPanelSize() )
		return;

	if ( m_bIsMouseDown )
	{
		EnableFastTransform( true );
		ApplyUpdatedTransform( ( flMouseDeltaX / m_pSubPanel->GetActualUIScaleX() ), ( flMouseDeltaY / m_pSubPanel->GetActualUIScaleY()) );
	}

	return;
	
}

bool CDragZoom::OnMouseButtonDown( const panorama::MouseData_t &code )
{
	CTopLevelWindow *pWindow = (CTopLevelWindow*)GetParentWindow();
	if ( pWindow )
		pWindow->SetMouseCursor( eMouseCursor_Hand_Closed );
	m_bIsMouseDown = true;
	return BaseClass::OnMouseButtonDown( code );
}

bool CDragZoom::OnMouseButtonUp( const panorama::MouseData_t &code )
{
	m_bIsMouseDown = false;
	EnableFastTransform( false );
	return BaseClass::OnMouseButtonUp( code );
}

void CDragZoom::ToggleFullZoomInternal( float flMouseX, float flMouseY )
{
	m_bIsScaled = !m_bIsScaled;
	UpdateContentLayoutValues();
	EnableFastTransform( false );

	float flTransformX = 0.0f;
	float flTransformY = 0.0f;

	if ( m_bIsScaled )
	{
		m_unZoomPosition = 0;
		m_flLastScale = m_flMinScale;
		flTransformX = 0.0f;
		flTransformY = 0.0f;
		m_flLastPositionX = 0.0f;
		m_flLastPositionY = 0.0f;
	}
	else
	{
		flTransformX = ( ( m_flLayoutWidth / 2.0f ) - flMouseX ) / ( m_flLayoutWidth / m_flContentWidth );
		flTransformY = ( ( m_flLayoutHeight / 2.0f ) - flMouseY ) / ( m_flLayoutHeight / m_flContentHeight );

		m_unZoomPosition = m_unNumMouseWheelTicks;
		m_flLastScale = 1.0f;
	}

	OnScaleChanged( m_flLastScale );
	ApplyUpdatedTransform( flTransformX, flTransformY );
}

void CDragZoom::ToggleFullZoom( float flNormalizedCenterX, float flNormalizedCenterY )
{
	float flPixelX = ( flNormalizedCenterX - 0.5f ) * m_flLayoutWidth;
	float flPixelY = ( flNormalizedCenterY - 0.5f ) * m_flLayoutHeight;
	ToggleFullZoomInternal( flPixelX, flPixelY );
}

bool CDragZoom::OnMouseButtonDoubleClick( const panorama::MouseData_t &code )
{
	ToggleFullZoomInternal( m_flLastMouseX, m_flLastMouseY );
	return BaseClass::OnMouseButtonDoubleClick( code );
}

bool CDragZoom::OnMouseWheel( const panorama::MouseData_t &code )
{
	float flTransformX = 0.0f;
	float flTransformY = 0.0f;

	if ( m_pSubPanel )
	{
		UpdateContentLayoutValues();
		EnableFastTransform( false );

		m_nMouseDelta = code.m_Delta;
		
		float flScale;
		float flMaxScale = 1.0f;

		float flMouseDelta = (flMaxScale - m_flMinScale) / m_unNumMouseWheelTicks;
		if ( m_nMouseDelta > 0 )
		{
			flScale = m_flLastScale + flMouseDelta;
		}
		else
		{
			flScale = m_flLastScale - flMouseDelta;
		}

		if ( flScale >= 0.0f )
		{
			m_bIsScaled = true;
			if ( ((m_flContentHeight * flScale) < m_flLayoutHeight) && ((m_flContentWidth * flScale) < m_flLayoutWidth) )
			{
				flScale = m_flMinScale;
			}
			if ( flScale >= 1.0f )
			{
				flScale = 1.0f;
				m_bIsScaled = false;
			}

			if ( flScale == m_flLastScale )
				return true;

			if ( m_nMouseDelta > 0.0 )
			{
				// Get the Mouse position relative to the parent panel and scale based on the percent change (MouseDelta)
				flTransformX = -((m_flLastMouseX - (m_flLayoutWidth / 2.0f)) * flMouseDelta) * 2.0f;
				flTransformY = -((m_flLastMouseY - (m_flLayoutHeight / 2.0f)) * flMouseDelta) * 2.0f;

				m_unZoomPosition++;
			}
			else
			{
				//Cast Zoom Position to a float for calculation purposes
				float flZoomPosition = m_unZoomPosition;

				// Calculate the transform based on the number of ticks left to get to a 0 zoom position
				flTransformX = -(m_flLastPositionX / flZoomPosition);
				flTransformY = -(m_flLastPositionY / flZoomPosition);

				m_unZoomPosition--;
			}

			m_flLastScale = flScale;
			OnScaleChanged( flScale );
		}

		ApplyUpdatedTransform( flTransformX, flTransformY );
	}
	return BaseClass::OnMouseWheel( code );
}

void CDragZoom::ClampTransformValues( float *pflTransformX, float *pflTransformY )
{
	// Get the borders of our new panel and use those values for clamping
	float flOverflowX = (((m_flContentWidth * m_flLastScale) - m_flLayoutWidth) / 2.0f) / m_pSubPanel->GetActualUIScaleX();
	float flMaxTransX = flOverflowX - m_flLastPositionX;
	float flMinTransX = flOverflowX + m_flLastPositionX;

	float flOverflowY = (((m_flContentHeight * m_flLastScale) - m_flLayoutHeight) / 2.0f) / m_pSubPanel->GetActualUIScaleY();
	float flMaxTransY = flOverflowY - m_flLastPositionY;
	float flMinTransY = flOverflowY + m_flLastPositionY;

	// Clamp values so our content panel doesn't move beyond our parent panels borders
	if ( m_flContentWidth * m_flLastScale > m_flLayoutWidth )
	{
		*pflTransformX = Clamp( *pflTransformX, -flMinTransX, flMaxTransX );
	}
	else
	{
		*pflTransformX = 0.0f;
		m_flLastPositionX = 0.0f;
	}

	if ( m_flContentHeight * m_flLastScale > m_flLayoutHeight )
	{
		*pflTransformY = Clamp( *pflTransformY, -flMinTransY, flMaxTransY );
	}
	else
	{
		*pflTransformY = 0.0f;
		m_flLastPositionY = 0.0f;
	}

}

void CDragZoom::ApplyUpdatedTransform( float flTransformX, float flTransformY )
{
	CUtlVector< CTransform3D * > vecTransform;
	ClampTransformValues( &flTransformX, &flTransformY );
	m_flLastPositionX += flTransformX;
	m_flLastPositionY += flTransformY;

	vecTransform.AddToTail( new CTransformScale3D( m_flLastScale, m_flLastScale, 1.0f ) );
	vecTransform.AddToTail( new CTransformTranslate3D( m_flLastPositionX, m_flLastPositionY, 0.0f ) );
	m_pSubPanel->AccessStyleDirty()->SetTransform3D( vecTransform );
}

void CDragZoom::EnableFastTransform( bool bIsFast )
{
	if ( bIsFast )
	{
		m_pSubPanel->AddClass( "DragZoomFastTransition" );
		m_pSubPanel->RemoveClass( "DragZoomSlowTransition" );
	}
	else
	{
		m_pSubPanel->AddClass( "DragZoomSlowTransition" );
		m_pSubPanel->RemoveClass( "DragZoomFastTransition" );
	}
}

void CDragZoom::UpdateContentLayoutValues()
{
	m_flLayoutHeight = GetActualLayoutHeight();
	m_flLayoutWidth = GetActualLayoutWidth();
	m_flContentHeight = m_pSubPanel->GetContentHeight() * m_flExtraZoomOut;
	m_flContentWidth = m_pSubPanel->GetContentWidth() * m_flExtraZoomOut;
	m_flMinScale = MIN( (m_flLayoutHeight / m_flContentHeight), (m_flLayoutWidth / m_flContentWidth) );
}

void CDragZoom::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	UpdateContentLayoutValues();

	if ( m_bFirstLoad )
	{
		EnableFastTransform( false );

		float flTransformX = 0.0f;
		float flTransformY = 0.0f;

		if ( m_bIsScaled )
		{
			m_unZoomPosition = 0;
			m_flLastScale = m_flMinScale;
			flTransformX = 0.0f;
			flTransformY = 0.0f;
			m_flLastPositionX = 0.0f;
			m_flLastPositionY = 0.0f;
		}
		else
		{
			// Set scale to 1.0 and zoom in on your mouse cursor
			flTransformX = 0.0f;
			flTransformY = 0.0f;

			m_unZoomPosition = m_unNumMouseWheelTicks;
			m_flLastScale = 1.0f;
		}
		OnScaleChanged( m_flLastScale );

		ApplyUpdatedTransform( flTransformX, flTransformY );
		m_bFirstLoad = false;
	}
}

bool CDragZoom::OnPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr )
{
	CUtlVector<CPanel2D *> vecChildrenToReparent;

	for ( int iChild = 0; iChild < GetChildCount(); iChild++ )
	{
		if ( GetChild( iChild ) != m_pSubPanel )
		{
			vecChildrenToReparent.AddToTail( GetChild( iChild ) );
		}
	}

	for ( CPanel2D *pChildToReparent : vecChildrenToReparent )
	{
		pChildToReparent->SetParent( m_pSubPanel );
	}

	return true;
}
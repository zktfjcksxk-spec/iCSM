//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/slider.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;


REGISTER_PANEL2D_FACTORY( CSlider, Slider );
REGISTER_PANEL2D_FACTORY( CSlottedSlider, SlottedSlider );
REGISTER_PANEL2D_FACTORY( CSpinner, Spinner );

DEFINE_PANORAMA_EVENT( SliderValueChanged );
DEFINE_PANORAMA_EVENT( SlottedSliderValueChanged );
DEFINE_PANORAMA_EVENT( SliderFocusChanged );
DEFINE_PANORAMA_EVENT( SpinnerValueChanged );


const float s_flMouseDownLoiterTime = 0.25f;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSlider::CSlider( panorama::CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
	m_flCur = 0.0f;
	m_flMin = 0.0f;
	m_flMax = 1.0f;
	m_flDefault = 0.5f;
	m_flIncrement = 0.1f;
	m_bDraggingThumb = false;
	m_flMouseDownValueOffset = 0.0f;
	m_flLastMouseX = 0.0f;
	m_flLastMouseY = 0.0f;
	m_eDirection = k_EDirectionVertical;
	m_bRequiresSelection = false;
	m_bShowDefault = false;
	m_bMouseDown = false;
	m_flMouseDownTime = 0.0f;

	// Allocate in code rather than XML because we don't want to hardcode a
	// particular CSS stylesheet.
	m_pTrack = new CPanel2D( this, "SliderTrack" );
	m_pProgress = new CPanel2D( this, "SliderTrackProgress" );
	m_pThumb = new CPanel2D( this, "SliderThumb" );
	m_pDefaultTick = new CPanel2D( this, "SliderDefault" );

	SetAcceptsFocus( true );
	SetMouseTracking( true );

	// register for events
	if( !UIEngine()->BHaveEventHandlersRegisteredForType( CSlider::GetPanelSymbol() ) )
	{
		RegisterEventHandler( Activated(), this, &CSlider::EventActivated );
		RegisterEventHandler( Cancelled(), this, &CSlider::EventCancelled );
		RegisterEventHandler( StyleFlagsChanged(), this, &CSlider::EventStyleFlagsChanged );
		RegisterEventHandler( ResetToDefaultValue(), this, &CSlider::EventResetToDefault );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSlider::~CSlider()
{

}


//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CSlider::SetupJavascriptObjectTemplate()
{
	RegisterJSAccessor( "value", PANORAMA_DELEGATE( &CSlider::GetValue ), PANORAMA_DELEGATE( &CSlider::SetValue ) );
	RegisterJSAccessor( "min", PANORAMA_DELEGATE( &CSlider::GetMin ), PANORAMA_DELEGATE( &CSlider::SetMin ) );
	RegisterJSAccessor( "max", PANORAMA_DELEGATE( &CSlider::GetMax ), PANORAMA_DELEGATE( &CSlider::SetMax ) );
	RegisterJSAccessor( "increment", PANORAMA_DELEGATE( &CSlider::GetIncrement ), PANORAMA_DELEGATE( &CSlider::SetIncrement ) );
	RegisterJSAccessor( "default", PANORAMA_DELEGATE( &CSlider::GetDefaultValue ), PANORAMA_DELEGATE( &CSlider::SetDefaultValue ) );
	RegisterJSAccessorReadOnly( "mousedown", PANORAMA_DELEGATE( &CSlider::IsMouseDown ) );


	RegisterJSMethod( "SetDirection", PANORAMA_DELEGATE( &CSlider::SetDirection ) );
	RegisterJSMethod( "SetShowDefaultValue", PANORAMA_DELEGATE( &CSlider::SetShowDefaultValue ) );
	RegisterJSMethod( "SetRequiresSelection", PANORAMA_DELEGATE( &CSlider::SetRequiresSelection ) );
	RegisterJSMethod( "SetValueNoEvents", PANORAMA_DELEGATE( &CSlider::SetValueNoEvents ) );
}


//-----------------------------------------------------------------------------
// Purpose: toggle selected state on activation
//-----------------------------------------------------------------------------
bool CSlider::OnActivate( panorama::EPanelEventSource_t eSource )
{
	if ( eSource == k_ePanelEventSourceMouse )
	{
		if ( !IsSelected() )
		{
			AddStyleFlag(panorama::k_EStyleFlagSelected);
			m_flLast = m_flCur;
		}
	} 
	else
	{
		// toggle selection
		if ( IsSelected() )
		{
			RemoveStyleFlag(panorama::k_EStyleFlagSelected);
		}
		else
		{
			AddStyleFlag(panorama::k_EStyleFlagSelected);
			m_flLast = m_flCur;
		}
	}

	DispatchEvent( SliderFocusChanged(), this, (GetStyleFlags() & panorama::k_EStyleFlagSelected) != 0 );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: exit selected state on cancel
//-----------------------------------------------------------------------------
bool CSlider::OnCancel( panorama::EPanelEventSource_t eSource )
{
	// toggle selection
	if ( IsSelected() )
	{
		// If we cancelled, restore our original value.
		SetValue( m_flLast );
		RemoveStyleFlag( panorama::k_EStyleFlagSelected );

		DispatchEvent( SliderFocusChanged(), this, false );

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: exit selected state when focus is lost
//-----------------------------------------------------------------------------
void CSlider::OnStyleFlagsChanged( )
{
	// if we lose focus, we need to deselect as well
	if ( !( GetStyleFlags() & panorama::k_EStyleFlagFocus ) )
	{
		RemoveStyleFlag( panorama::k_EStyleFlagSelected );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set our value to the default value
//-----------------------------------------------------------------------------
void CSlider::OnResetToDefaultValue( )
{
	SetValue( m_flDefault );
}


//-----------------------------------------------------------------------------
// Purpose: we've been activated; toggle edit mode
//-----------------------------------------------------------------------------
bool CSlider::EventActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource )
{
	if ( ToPanel2D( pPanel.Get() ) != this )
		return false;

	return OnActivate( eSource );
}


//-----------------------------------------------------------------------------
// Purpose: we've been cancelled; if we're in edit mode, go out
//-----------------------------------------------------------------------------
bool CSlider::EventCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource )
{
	if ( ToPanel2D( pPanel.Get() ) != this )
		return false;

	return OnCancel( eSource );
}


//-----------------------------------------------------------------------------
// Purpose: our style flags have changed; perhaps leave selected/edit mode if we lost focus
//-----------------------------------------------------------------------------
bool CSlider::EventStyleFlagsChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( ToPanel2D( pPanel.Get() ) == this )
		OnStyleFlagsChanged( );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Reset our value to default if our panel matches
//-----------------------------------------------------------------------------
bool CSlider::EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( ToPanel2D( pPanel.Get() ) == this )
		OnResetToDefaultValue();

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets current value.. makes sure it doesn't exceed bounds and dispatches event
//-----------------------------------------------------------------------------
void CSlider::SetValue( float flValue )
{
	m_flCur = clamp( flValue, m_flMin, m_flMax );
	DispatchEvent( SliderValueChanged(), this, m_flCur );
	static const CPanoramaSymbol k_symOnValueChanged( "onvaluechanged" );
	DispatchPanelEvent( k_symOnValueChanged );

	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Sets current value.. makes sure it doesn't exceed bounds and does *not* dispatches event
//-----------------------------------------------------------------------------
void CSlider::SetValueNoEvents( float flValue )
{
	m_flCur = clamp( flValue, m_flMin, m_flMax );

	InvalidateSizeAndPosition();
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the value for a given mouse position
//-----------------------------------------------------------------------------
float CSlider::CalculateValueFromMousePosition( float x, float y )
{
	float flPercent = 0;
	if ( m_eDirection == k_EDirectionVertical )
	{
		float flTrackTop = m_pTrack->GetActualYOffset();
		float flAdjustedY = y - flTrackTop;
		flPercent = 1.0f - flAdjustedY / ( m_pTrack->GetActualLayoutHeight() - m_pThumb->GetActualRenderHeight() );
	}
	else
	{
		float flTrackLeft = m_pTrack->GetActualXOffset();
		float flAdjustedX = x - flTrackLeft;
		flPercent = flAdjustedX / ( m_pTrack->GetActualLayoutWidth() - m_pThumb->GetActualRenderWidth() );
	}

	return ( ( m_flMax - m_flMin ) * flPercent ) + m_flMin;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the current value from mouse coords
//-----------------------------------------------------------------------------
void CSlider::SetValueFromMouse( float x, float y )
{
	float flValue = CalculateValueFromMousePosition( x, y );
	flValue += m_flMouseDownValueOffset;
	SetValue( flValue );
}


//-----------------------------------------------------------------------------
// Purpose: layout - sets sizes of child panels
//-----------------------------------------------------------------------------
void CSlider::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	
	// normalize to 0.0-1.0
	float flPercent = m_flCur - m_flMin;
	flPercent *= 1.0f / (m_flMax - m_flMin);

	m_pDefaultTick->SetVisible( m_bShowDefault && fabs( m_flCur - GetDefaultValue() ) > ( m_flIncrement / 2.0f ) );

	if ( m_eDirection == k_EDirectionVertical )
	{
		float flThumbHeight = m_pThumb->GetActualRenderHeight();

		float flTrackHeight = m_pTrack->GetActualLayoutHeight() - flThumbHeight;
		float flVerticalStart = m_pTrack->GetActualYOffset();

		// adjust thumb to correct position
		float flThumbCenter = flVerticalStart + (flTrackHeight * (1.0f - flPercent));
		m_pThumb->LayoutTraverse( m_pThumb->GetActualXOffset(), flThumbCenter, m_pThumb->GetActualLayoutWidth(), m_pThumb->GetActualRenderHeight() );

		float flDefaultCenter = flVerticalStart + ( ( m_pTrack->GetActualLayoutHeight() - m_pDefaultTick->GetActualRenderHeight() ) * ( 1.0f - GetDefaultValue() ) );
		m_pDefaultTick->LayoutTraverse( m_pDefaultTick->GetActualXOffset(), flDefaultCenter, m_pDefaultTick->GetActualLayoutWidth(), m_pDefaultTick->GetActualRenderHeight() );

		// adjust progress. it should start at center of thumb and go to bottom of track
		float flProgressHeight = flTrackHeight * flPercent;
		m_pProgress->LayoutTraverse( m_pProgress->GetActualXOffset(), flThumbCenter+flThumbHeight, m_pProgress->GetActualLayoutWidth(), flProgressHeight );
	}
	else
	{
		float flThumbWidth = m_pThumb->GetActualRenderWidth();

		float flTrackWidth = m_pTrack->GetActualLayoutWidth() - flThumbWidth;
		float flHorizontalStart = m_pTrack->GetActualXOffset();

		// adjust thumb to correct position
		float flThumbCenter = flHorizontalStart + (flTrackWidth * flPercent);
		m_pThumb->LayoutTraverse( flThumbCenter, m_pThumb->GetActualYOffset(), m_pThumb->GetActualLayoutWidth(), m_pThumb->GetActualRenderHeight() );

		float flDefaultCenter = flHorizontalStart + ( ( m_pTrack->GetActualLayoutWidth() - m_pDefaultTick->GetActualRenderWidth() ) * GetDefaultValue() );
		m_pDefaultTick->LayoutTraverse( flDefaultCenter, m_pDefaultTick->GetActualYOffset(), m_pDefaultTick->GetActualLayoutWidth(), m_pDefaultTick->GetActualRenderHeight() );

		// adjust progress. it should start at center of thumb and go to bottom of track
		m_pProgress->LayoutTraverse( m_pProgress->GetActualXOffset(), m_pProgress->GetActualYOffset(), flThumbCenter, m_pProgress->GetActualLayoutHeight() );
	}	
}

bool CSlider::AllowInteraction( void )
{
	return ( !m_bRequiresSelection || ( m_bRequiresSelection && GetStyleFlags() & panorama::k_EStyleFlagSelected ) );
}

//-----------------------------------------------------------------------------
// Purpose: Called when mouse button goes down
//-----------------------------------------------------------------------------
bool CSlider::OnMouseButtonDown( const MouseData_t &code )
{
	if ( code.m_MouseCode != MOUSE_LEFT )
		return false;

	m_bMouseDown = true;
	m_flMouseDownTime = UIEngine()->GetCurrentFrameTime();

	if ( !m_bDraggingThumb && ToPanel2D( GetParentWindow()->UIWindowInput()->GetMouseHover() ) == m_pThumb )
	{
		m_bDraggingThumb = true;
		// if over thumb, drag using an offset from where the value currently is
		m_flMouseDownValueOffset = GetValue() - CalculateValueFromMousePosition( m_flLastMouseX, m_flLastMouseY );
	}
	else
	{
		Vector2D vecPositions[ 2 ] = 
		{
			{ 0.0f, 0.0f },
			{ 0.5f * m_pThumb->GetActualRenderWidth(), 0.5f * m_pThumb->GetActualRenderHeight() }
		};
		m_pThumb->GetPointsWithinAncestor( this, vecPositions, vecPositions, ARRAYSIZE( vecPositions ) );
		m_flMouseDownValueOffset = CalculateValueFromMousePosition( vecPositions[ 0 ].x, vecPositions[ 0 ].y ) - CalculateValueFromMousePosition( vecPositions[ 1 ].x, vecPositions[ 1 ].y );
	}

	SetValueFromMouse( m_flLastMouseX, m_flLastMouseY );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse button is released
//-----------------------------------------------------------------------------
bool CSlider::OnMouseButtonUp( const MouseData_t &code )
{
	if ( code.m_MouseCode == MOUSE_LEFT )
	{
		m_bDraggingThumb = false;
		m_bMouseDown = false;
		m_flMouseDownTime = 0.0f;
		m_flMouseDownValueOffset = 0.0f;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse moves
//-----------------------------------------------------------------------------
void CSlider::OnMouseMove( float flMouseX, float flMouseY )
{
	m_flLastMouseX = flMouseX;
	m_flLastMouseY = flMouseY;

	CPanel2D *pHoverPanel = ToPanel2D( GetParentWindow()->UIWindowInput()->GetMouseHover() );
	// if over thumb, no movement yet, as we consider that not actually having dragged until you move outside thumb bounds.
	// this avoids jitter on clicking thumb.
	if ( pHoverPanel == m_pThumb && (UIEngine()->GetCurrentFrameTime() - m_flMouseDownTime) < s_flMouseDownLoiterTime )
	{
		return;
	}

	if ( !m_bDraggingThumb )
	{
		if ( m_bMouseDown && pHoverPanel ) // if they are hovering us then 
		{
			if ( (UIEngine()->GetCurrentFrameTime() - m_flMouseDownTime) > s_flMouseDownLoiterTime ) // don't start the drag right after the click, give a little time for the up
			{
				while ( pHoverPanel && pHoverPanel->GetParent() != this )
				{
					pHoverPanel = pHoverPanel->GetParent();
				}

				if ( pHoverPanel ) // mouse is down and hovering us, turn this into a drag
				{
					SetValueFromMouse( flMouseX, flMouseY );
					m_bDraggingThumb = true;
				}
			}
		}
		return;
	}

	SetValueFromMouse( flMouseX, flMouseY );
}


//-----------------------------------------------------------------------------
// Purpose: Increases current value
//-----------------------------------------------------------------------------
bool CSlider::OnMoveUp( int nRepeats )
{
	if ( AllowInteraction() && m_eDirection == k_EDirectionVertical )
		SetValue( m_flCur + m_flIncrement );
	else
		BaseClass::OnMoveUp( nRepeats );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Increases current value
//-----------------------------------------------------------------------------
bool CSlider::OnMoveRight( int nRepeats )
{
	if ( AllowInteraction() && m_eDirection == k_EDirectionHorizontal )
		SetValue( m_flCur + m_flIncrement );
	else
		BaseClass::OnMoveRight( nRepeats );

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: Decreases current value
//-----------------------------------------------------------------------------
bool CSlider::OnMoveDown( int nRepeats )
{
	if ( AllowInteraction() && m_eDirection == k_EDirectionVertical )
		SetValue( m_flCur - m_flIncrement );
	else
		BaseClass::OnMoveDown( nRepeats );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Decreases current value
//-----------------------------------------------------------------------------
bool CSlider::OnMoveLeft( int nRepeats )
{
	if ( AllowInteraction() && m_eDirection == k_EDirectionHorizontal )
		SetValue( m_flCur - m_flIncrement );
	else
		BaseClass::OnMoveLeft( nRepeats );

	return true;
}

CSlottedSlider::CSlottedSlider( CPanel2D *pParent, const char *pchID ) :CSlider( pParent, pchID )
{
	m_nNumNotches = 0;
	m_nCurNotch = 0;
}

CSlottedSlider::~CSlottedSlider()
{}

bool CSlottedSlider::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symNotches( "notches" );
	if ( symName == symNotches )
	{
		int nVal = atoi( pchValue );
		m_nNumNotches = nVal;
		SetIncrement( 1.0f / ( float )( nVal - 1 ) );
		// create the visual guide posts
		for ( int i = 0; i < nVal; ++i )
		{
			CPanel2D *pNotch = new CPanel2D( this, "" );
			pNotch->AddClass( "SliderNotch" );
			m_pNotches.AddToTail( pNotch );
		}
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );

}

void CSlottedSlider::SetValue( float flPercent )
{
	AssertMsg( m_nNumNotches > 0, "No notches for the slotted slider to use" );
	const float flSlotIdx = ( float )( m_nNumNotches - 1 );
	float flValue = ( float )( flSlotIdx )* flPercent;
	m_nCurNotch = Clamp( RoundFloatToInt( flValue ), 0, m_nNumNotches - 1 );
	float flResult = ( float )( m_nCurNotch ) / ( float )( flSlotIdx );
	DispatchEvent( SlottedSliderValueChanged( ), this, m_nCurNotch );
	BaseClass::SetValue( flResult );
}

void CSlottedSlider::SetValue( int nValue )
{
	AssertMsg( nValue < m_nNumNotches && nValue >= 0, "Not enough notches for set value/value is negative" );
	nValue = Clamp( nValue, 0, m_nNumNotches - 1 );
	float flPercent = ( float )( nValue ) / ( float )( m_nNumNotches - 1 );
	m_nCurNotch = nValue;
	DispatchEvent( SlottedSliderValueChanged(), this, m_nCurNotch );
	BaseClass::SetValue( flPercent );
}

void CSlottedSlider::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	
	if ( GetDirection() == k_EDirectionVertical )
	{
		//TODO
	}
	else
	{
		float flThumbWidth = m_pThumb->GetActualRenderWidth();

		float flTrackWidth = m_pTrack->GetActualLayoutWidth() - flThumbWidth;
		float flHorizontalStart = m_pTrack->GetActualXOffset();

		for ( int i = 0; i < m_nNumNotches; ++i )
		{
			float flNotchWidth = m_pNotches[i]->GetActualRenderWidth();
			float flNotchPercent = ( float )( i ) / ( float )( m_nNumNotches-1 );
			float flButtonCenter = flHorizontalStart + ( flTrackWidth * flNotchPercent );
			float flDelta = flThumbWidth - flNotchWidth;
			float flActualCenter = flButtonCenter + flDelta/2.0f;
			m_pNotches[i]->LayoutTraverse( flActualCenter, m_pTrack->GetActualYOffset( ), m_pNotches[i]->GetActualLayoutWidth( ), m_pNotches[i]->GetActualRenderHeight( ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set direction
//-----------------------------------------------------------------------------
void CSlider::SetDirection( ESliderDirection eValue )
{
	if ( m_eDirection != eValue )
	{
		m_eDirection = eValue;
		SetHasClass( "HorizontalSlider", m_eDirection == k_EDirectionHorizontal );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------
bool CSlider::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symDirection( "direction" );

	if ( symName == symDirection )
	{
		if ( V_stricmp( pchValue, "vertical" ) == 0 )
		{
			SetDirection( k_EDirectionVertical );
			return true;
		}
		else if ( V_stricmp( pchValue, "horizontal" ) == 0 )
		{
			SetDirection( k_EDirectionHorizontal );
			return true;
		}
	}

	return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------
bool CSlider::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	static const CPanoramaSymbol k_symOnValueChanged( "onvaluechanged" );

	if ( symProperty == k_symOnValueChanged )
		return true;

	return BaseClass::BIsClientPanelEvent( symProperty );
}


CSpinner::CSpinner( panorama::CPanel2D *pParent, const char *pchID ) :CPanel2D( pParent, pchID )
{
	SetAcceptsFocus( true );
	SetMouseTracking( true );

	m_bMouseDown = false;
	m_flMouseDownTime = 0.0f;
	m_flMouseDownY = 0.0f;
	m_flMin = 0.0f;
	m_flMax = 1.0f;
	m_nThrow = 300;
	m_flCur = 0;
	m_flLast = m_flMax;
	m_bMouseIsSpinning = false;

	// register for events
	RegisterEventHandler( Activated(), this, &CSpinner::EventActivated );

}

CSpinner::~CSpinner()
{

}

void CSpinner::SetupJavascriptObjectTemplate()
{
	RegisterJSAccessor( "value", PANORAMA_DELEGATE( &CSpinner::GetValue ), PANORAMA_DELEGATE( &CSpinner::SetValue ) );
	RegisterJSAccessor( "spinlock", PANORAMA_DELEGATE( &CSpinner::GetSpinlock ), PANORAMA_DELEGATE( &CSpinner::SetSpinlock ) );

	BaseClass::SetupJavascriptObjectTemplate();
}

//-----------------------------------------------------------------------------
// Purpose: Called when mouse button goes down
//-----------------------------------------------------------------------------
bool CSpinner::OnMouseButtonDown( const MouseData_t &code )
{

	if ( m_bSpinlock )
		return false;

	if ( code.m_MouseCode != MOUSE_LEFT )
		return false;

	m_bMouseDown = true;
	m_flMouseDownTime = UIEngine()->GetCurrentFrameTime();
	m_bMouseIsSpinning = false;


	if ( ToPanel2D( GetParentWindow()->UIWindowInput()->GetMouseHover() ) )
	{
		m_flMouseDownY =  m_flLastMouseY;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Called when mouse button is released
//-----------------------------------------------------------------------------
bool CSpinner::OnMouseButtonUp( const MouseData_t &code )
{
	if ( m_bSpinlock )
		return false;

	if ( code.m_MouseCode == MOUSE_LEFT )
	{
		m_bMouseDown = false;
		m_flMouseDownTime = 0.0f;
		m_flMouseDownY = 0.0f;

		m_flLast = m_flCur;
	}

	return false;

}

//-----------------------------------------------------------------------------
// Purpose: Called when mouse moves
//-----------------------------------------------------------------------------
void CSpinner::OnMouseMove( float flMouseX, float flMouseY )
{
	if ( m_bSpinlock )
		return;

	m_flLastMouseX = flMouseX;
	m_flLastMouseY = flMouseY;

	CPanel2D *pHoverPanel = ToPanel2D( GetParentWindow()->UIWindowInput()->GetMouseHover() );
	// if over thumb, no movement yet, as we consider that not actually having dragged until you move outside thumb bounds.
	// this avoids jitter on clicking thumb.
	if ( pHoverPanel == this && ( UIEngine()->GetCurrentFrameTime() - m_flMouseDownTime ) < s_flMouseDownLoiterTime )
	{
		return;
	}

// 	if ( m_bMouseDown && pHoverPanel ) // if they are hovering us then 
// 	{
// 		if ( ( UIEngine()->GetCurrentFrameTime() - m_flMouseDownTime ) > s_flMouseDownLoiterTime ) // don't start the drag right after the click, give a little time for the up
// 		{
// 			while ( pHoverPanel && pHoverPanel != this )
// 			{
// 				pHoverPanel = pHoverPanel->GetParent();
// 			}
// 
// 			if ( pHoverPanel ) // mouse is down and hovering us, turn this into a drag
// 			{
// 				m_bDragging = true;
// 			}
// 		}
// 	}
	

#define MOUSE_MOVE_THRESHOLD 10

	if ( !m_bMouseIsSpinning && m_bMouseDown && ( abs( m_flMouseDownY - flMouseY ) > MOUSE_MOVE_THRESHOLD ) )
	{
		m_bMouseIsSpinning = true;
	}

	if ( m_bMouseDown && m_bMouseIsSpinning )
		SetValueFromMouse( flMouseX, flMouseY );


}

void CSpinner::SetValueFromMouse( float x, float y )
{
	float flValue = CalculateValueFromMousePosition( x, y );
 	flValue += m_flLast;
	SetValue( flValue );
}


//-----------------------------------------------------------------------------
// Purpose: Sets current value.. makes sure it doesn't exceed bounds and dispatches event
//-----------------------------------------------------------------------------
void CSpinner::SetValue( float flValue )
{
	m_flCur = clamp( flValue, m_flMin, m_flMax );

	DispatchEvent( SpinnerValueChanged(), this, m_flCur );
	static const CPanoramaSymbol k_symOnValueChanged( "onvaluechanged" );
	DispatchPanelEvent( k_symOnValueChanged );
}



//-----------------------------------------------------------------------------
// Purpose: Calculate the value for a given mouse position
//-----------------------------------------------------------------------------
float CSpinner::CalculateValueFromMousePosition( float x, float y )
{
	return ( ( ( m_flMax - m_flMin ) * ( ( m_flMouseDownY - m_flLastMouseY ) / m_nThrow ) ) + m_flMin );
}

//-----------------------------------------------------------------------------
// Purpose: Activation event
//-----------------------------------------------------------------------------
bool CSpinner::EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	if ( ToPanel2D( pPanel.Get() ) != this )
		return false;

	return OnActivate( eSource );
}


//-----------------------------------------------------------------------------
// Purpose: toggle selected state on activation
//-----------------------------------------------------------------------------
bool CSpinner::OnActivate( panorama::EPanelEventSource_t eSource )
{
	return m_bMouseIsSpinning;
}

//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------
bool CSpinner::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symMin( "min" );
	static CPanoramaSymbol symMax( "max" );
	static CPanoramaSymbol symThrow( "throw" );

	if ( symName == symMin )
	{
		m_flMin = V_atof( pchValue );
		return true;
	}
	else if ( symName == symMax )
	{
		m_flMax = V_atof( pchValue );
		return true;
	}
	else if ( symName == symThrow )
	{
		m_nThrow = V_atoi( pchValue );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

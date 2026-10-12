//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/uievents.h"
#include "panorama/controls/vumeter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CVUMeter, VUMeter )

//////////////////////////////////////////////////////////////////////////
//
// volume bars control for volume/mic levels
//


DEFINE_PANORAMA_EVENT( VUMeterBarsChanged );

//-----------------------------------------------------------------------------
// Purpose: ctor
//-----------------------------------------------------------------------------
CVUMeter::CVUMeter( panorama::CPanel2D *pParent, const char *pchID ) : panorama::CPanel2D( pParent, pchID )
{
	m_numBars = 10;
	m_numActive = 0;
	m_bWritable = false;
	m_symBarPanelType = panorama::CPanel2D::GetPanelSymbol();
	RegisterEventHandler( panorama::Activated(), this, &CVUMeter::EventActivated );
	RegisterEventHandler( panorama::Cancelled(), this, &CVUMeter::EventCancelled );
	RegisterEventHandler( panorama::StyleFlagsChanged(), this, &CVUMeter::EventStyleFlagsChanged );
}


//-----------------------------------------------------------------------------
// Purpose: dtor
//-----------------------------------------------------------------------------
CVUMeter::~CVUMeter()
{

}


//-----------------------------------------------------------------------------
// Purpose: Apply a property from a layout file
//-----------------------------------------------------------------------------
bool CVUMeter::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	const static CPanoramaSymbol symNumBars( "numbars" );
	const static CPanoramaSymbol symNumBarsActive( "numbarsactive" );
	const static CPanoramaSymbol symBarPanelType( "barpaneltype" );
	const static CPanoramaSymbol symBarPanelAddClass( "barpaneladdclass" );
	const static CPanoramaSymbol symBarPanelActiveClass( "barpanelactiveclass" );
	const static CPanoramaSymbol symWritable( "writable" );

	if ( symNumBars == symName )
	{
		m_numBars = V_atoi( pchValue );
		Assert( m_numBars > 0 );
		return true;
	}
	else if ( symBarPanelType == symName )
	{
		m_symBarPanelType = pchValue;
		return true;
	}
	else if ( symBarPanelAddClass == symName )
	{
		m_symBarPanelAddClass = pchValue;
		return true;
	}
	else if ( symBarPanelActiveClass == symName )
	{
		m_symBarPanelActiveClass = pchValue;
		return true;
	}
	else if ( symNumBarsActive == symName )
	{
		m_numActive = V_atoi( pchValue );
		return true;
	}
	else if ( symWritable == symName )
	{
		bool bWritable = false;
		if ( !panorama::CSSHelpers::BParseTrueFalse( pchValue, &bWritable) )
			bWritable = false;

		SetWritable( bWritable );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: We have initialized our data from the layout file; commit changes
// to the document tree
//-----------------------------------------------------------------------------
void CVUMeter::OnInitializedFromLayout()
{
	m_arrBars.PurgeAndDeleteElements();
	m_arrBars.EnsureCapacity( m_numBars );
	for ( int k = 0; k < m_numBars; ++ k )
	{
		// bugbug jmccaskey - why is this using the factory and not just newing the panel?  Fix?
		panorama::CPanel2D *pChild = (panorama::CPanel2D *)(UIEngine()->CreatePanelClient( m_symBarPanelType, CFmtStr32( "Bar%d", k+1 ).String(), UIPanel() ));
		m_arrBars.AddToTail( pChild );
		
		if ( m_symBarPanelAddClass.IsValid() )
			pChild->AddClasses( m_symBarPanelAddClass.String() );
		if ( k < m_numActive )
			pChild->AddClasses( m_symBarPanelActiveClass.String() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: set the number of active bars in the VU meter
//-----------------------------------------------------------------------------
void CVUMeter::SetNumActiveBars( int numActive )
{
	if ( m_numActive == numActive )
		return;

	for ( int k = MIN( m_numActive, numActive ); k < MAX( m_numActive, numActive ); ++ k )
	{
		if ( k < numActive )
			m_arrBars[k]->AddClasses( m_symBarPanelActiveClass.String() );
		else
			m_arrBars[k]->RemoveClasses( m_symBarPanelActiveClass.String() );
	}

	m_numActive = numActive;
}


//-----------------------------------------------------------------------------
// Purpose: set whether the VU meter is read/write or read only
//-----------------------------------------------------------------------------
void CVUMeter::SetWritable( bool bWritable )
{
	if ( m_bWritable == bWritable )
		return;

	m_bWritable = bWritable;

	SetAcceptsFocus( bWritable );
}


//-----------------------------------------------------------------------------
// Purpose: toggle selected state on activation
//-----------------------------------------------------------------------------
bool CVUMeter::OnActivate( panorama::EPanelEventSource_t eSource )
{
	if ( eSource == k_ePanelEventSourceMouse )
	{
		if ( !IsSelected() )
		{
			AddStyleFlag(panorama::k_EStyleFlagSelected);
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
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: exit selected state on cancel
//-----------------------------------------------------------------------------
bool CVUMeter::OnCancel( panorama::EPanelEventSource_t eSource )
{
	// toggle selection
	if ( IsSelected() )
	{
		RemoveStyleFlag( panorama::k_EStyleFlagSelected );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: exit selected state when focus is lost
//-----------------------------------------------------------------------------
void CVUMeter::OnStyleFlagsChanged( )
{
	// if we lose focus, we need to deselect as well
	if ( !( GetStyleFlags() & panorama::k_EStyleFlagFocus ) )
	{
		RemoveStyleFlag( panorama::k_EStyleFlagSelected );
	}
}


//-----------------------------------------------------------------------------
// Purpose: we've been activated; toggle edit mode
//-----------------------------------------------------------------------------
bool CVUMeter::EventActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource )
{
	if ( ToPanel2D( pPanel.Get() ) != this )
		return false;

	return OnActivate( eSource );
}


//-----------------------------------------------------------------------------
// Purpose: we've been cancelled; if we're in edit mode, go out
//-----------------------------------------------------------------------------
bool CVUMeter::EventCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource )
{
	if ( ToPanel2D( pPanel.Get() ) != this )
		return false;

	return OnCancel( eSource );
}


//-----------------------------------------------------------------------------
// Purpose: our style flags have changed; perhaps leave selected/edit mode if we lost focus
//-----------------------------------------------------------------------------
bool CVUMeter::EventStyleFlagsChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( ToPanel2D( pPanel.Get() ) == this )
		OnStyleFlagsChanged( );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVUMeter::OnMoveLeft(int nRepeats)
{
	if (OnLeftRight(-(nRepeats + 1)))
		return true;
	return BaseClass::OnMoveLeft(nRepeats);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVUMeter::OnMoveRight(int nRepeats)
{
	if (OnLeftRight(nRepeats + 1))
		return true;
	return BaseClass::OnMoveRight(nRepeats);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVUMeter::OnMoveUp(int nRepeats)
{
	if (GetStyleFlags() & panorama::k_EStyleFlagSelected)
		return true;
	return BaseClass::OnMoveUp(nRepeats);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVUMeter::OnMoveDown(int nRepeats)
{
	if (GetStyleFlags() & panorama::k_EStyleFlagSelected)
		return true;
	return BaseClass::OnMoveDown(nRepeats);
}


//-----------------------------------------------------------------------------
// Purpose: we are selected and left/right was hit; update the number of active bars
//-----------------------------------------------------------------------------
bool CVUMeter::OnLeftRight( int dx )
{
	if ( GetStyleFlags() & panorama::k_EStyleFlagSelected )
	{
		int cBarsNew = GetNumActiveBars() + dx;
		cBarsNew = clamp( cBarsNew, 0, GetNumBarsTotal() );

		SetNumActiveBars( cBarsNew );
		DispatchEvent( VUMeterBarsChanged(), this, GetNumActiveBars() );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVUMeter::OnMouseButtonUp(const MouseData_t &code)
{
	if ( !IsSelected() ) 
	{
		// Only select, but do not change the value...
		AddStyleFlag(panorama::k_EStyleFlagSelected);
		return true;
	}
	
	// Assumption 1: OnMouseMove happened before...
	// Assumption 2: the children of the VUMeter represent the bars;	
	float flWidth = GetActualLayoutWidth();
	float flHeight = GetActualLayoutHeight();
	
	if ( m_flLastMouseY >= 0 && m_flLastMouseY <= flHeight )
	{
		int nNewValue = -1;
		int nChildCount = GetChildCount();
		for( int i=0; i < nChildCount; i++ )
		{
			int nCurrentX1 = GetChild( i )->GetActualXOffset();
			int nCurrentX2 = ( i < nChildCount - 1 ) ? GetChild( i+1 )->GetActualXOffset() : flWidth;

			// Click to the left of the bars should set the value to Zero
			if ( i == 0 && m_flLastMouseX >= 0.0 && m_flLastMouseX <= nCurrentX1 )
			{
				nNewValue = 0;
				break;
			}
			if ( m_flLastMouseX >= nCurrentX1 && m_flLastMouseX <= nCurrentX2 )
			{
				nNewValue = i+1;
				break;
			}
		}

		if ( nNewValue >= 0 )
		{
			SetNumActiveBars( nNewValue );
			DispatchEvent(VUMeterBarsChanged(), this, GetNumActiveBars());
		}
		return true;
	}

	return BaseClass::OnMouseButtonUp( code );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CVUMeter::OnMouseWheel(const MouseData_t &code)
{
	if ( GetStyleFlags() & panorama::k_EStyleFlagSelected )
	{
		return OnLeftRight( code.m_Delta > 0 ? +1 : -1 );
	}

	return BaseClass::OnMouseWheel( code );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVUMeter::OnMouseMove(float flMouseX, float flMouseY)
{
	m_flLastMouseX = flMouseX;
	m_flLastMouseY = flMouseY;
	return BaseClass::OnMouseMove( flMouseX, flMouseY );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CVUMeter::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	BaseClass::ValidateClientPanel( validator, pchName );

	VALIDATE_SCOPE();
	ValidateObj( m_arrBars );
}
#endif


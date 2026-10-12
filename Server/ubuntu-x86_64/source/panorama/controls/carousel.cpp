//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/carousel.h"
#include "panorama/uijsregistration.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/renderer/styleproperties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCarousel, Carousel );

static const char * k_pchCursorVisible( "CursorVisible" );

static const char k_rgchLeftChildStyle[] = "LeftOfFocus%d";
static const char k_rgchRightChildStyle[] = "RightOfFocus%d";
static const char k_rgchFocusedChildStyle[] = "Focused";
static const char k_rgchOffscreenStyle[] = "Offscreen";

static const int k_iInvalidChild = -1;

static const float k_flMaxWrapMoveRepeatInterval = 0.12f;

#ifdef DEBUG_TENFOOT_CAROUSEL
#define SPEW_CAROUSEL Msg
#else
#define SPEW_CAROUSEL( a, ... ) REFERENCE(a);
#endif

DECLARE_PANORAMA_EVENT0( UpdateFocusAndDirtyChildStyles );
DEFINE_PANORAMA_EVENT( UpdateFocusAndDirtyChildStyles );

DECLARE_PANORAMA_EVENT1( CarouselAutoScroll, uint8 );
DEFINE_PANORAMA_EVENT( CarouselAutoScroll );

DEFINE_PANORAMA_EVENT( ResetCarouselMouseWheelCounts );
DEFINE_PANORAMA_EVENT( SetCarouselSelectedChild );
DEFINE_PANORAMA_EVENT( CarouselChildrenChanged );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCarousel::CCarousel( CPanel2D *parent, const char * pchPanelID ) : CPanel2D( parent, pchPanelID )
{
	SetAcceptsFocus( true );
	m_pTitleLabel = NULL;
	m_bWrap = false;
	m_eFocusType = k_EFocusTypeLeft;
	m_eLastFocusEdge = k_EFocusEdgeLeft;
	m_iFocusLastEdge = 0;
	m_lenOffset.SetLength( 0.0f );
	m_bFlowingLayout = false;
	m_bIncludeScale2d = false;
	m_bHadFocus = false;
	m_bDelayedMovePosted = false;
	m_pDirtyChildStyles = NULL;
	m_pFocusedChild = NULL;
	m_flLastMouseWheel = 0.0f;
	m_bRegisteredForCursorChanges = false;
	m_flLastMove = 0.0f;
	m_bShuffleIntoView = false;
	m_nPanelsVisible = 0;
	m_bReactToFocusChange = true;
	m_flAutoScrollDelay = 0.0f;
	m_flAutoScrollRandomDelay = 0.0f;
	m_bAutoScrollScheduled = false;
	m_bAutoScrollEnabled = true;
	m_unAutoScrollID = 0;
	m_nChildCountHighWatermark = 0;

	static const CPanoramaSymbol k_symLeftMouseScrollRegion = "LeftMouseScrollRegion";
	static const CPanoramaSymbol k_symRightMouseScrollRegion = "RightMouseScrollRegion";

	m_pLeftMouseScrollRegion = new CMouseScrollRegion( this, "LeftMouseScrollRegion" );
	m_pLeftMouseScrollRegion->AddClass( k_symLeftMouseScrollRegion );

	m_pRightMouseScrollRegion = new CMouseScrollRegion( this, "RightMouseScrollRegion" );
	m_pRightMouseScrollRegion->AddClass( k_symRightMouseScrollRegion );

	// Carousels are all about input, need all children to get auto values
	SetTabIndex( k_flTabIndexAuto );
	SetSelectionPosition( k_flSelectionPosAuto, k_flSelectionPosAuto );

	if( !UIEngine()->BHaveEventHandlersRegisteredForType( CCarousel::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( InputFocusSet(), &CCarousel::EventInputFocusSet );
		RegisterEventHandlerOnPanelType( InputFocusLost(), &CCarousel::EventInputFocusLost );
		RegisterEventHandlerOnPanelType( ResetCarouselMouseWheelCounts(), &CCarousel::OnResetMouseWheelCounts );
		RegisterEventHandlerOnPanelType( MouseScroll(), &CCarousel::EventCarouselMouseScroll );
		RegisterEventHandlerOnPanelType( ::UpdateFocusAndDirtyChildStyles(), &CCarousel::UpdateFocusAndDirtyChildStyles );
		RegisterEventHandlerOnPanelType( CarouselAutoScroll(), &CCarousel::EventAutoScroll );
	}

	MarkFocusDirty();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCarousel::~CCarousel()
{
	UnregisterForCursorChanges();
	SAFE_DELETE( m_pDirtyChildStyles );
}


//-----------------------------------------------------------------------------
// Purpose: Once we are initialized fully from layout we probably have children, who should get styles right away
//-----------------------------------------------------------------------------
void CCarousel::OnInitializedFromLayout()
{
	DispatchEventAsync( 0.0f, ::UpdateFocusAndDirtyChildStyles(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Replacement for RemoveAndDeleteChildren.. doesn't delete label
//-----------------------------------------------------------------------------
void CCarousel::DeleteChildren()
{
	// delete all children
	for ( int i = GetChildCount() - 1; i >= 0; i-- )
	{
		CPanel2D *pChild = GetChild( i );
		if ( pChild == m_pTitleLabel )
			continue;

		delete pChild;
	}

	// clear focus child
	m_pFocusedChild = NULL;
}



//-----------------------------------------------------------------------------
// Purpose: Creates and returns the title label
//-----------------------------------------------------------------------------
CLabel *CCarousel::CreateTitleLabel()
{
	if ( !m_pTitleLabel )
	{
		m_pTitleLabel = new CLabel( this, "CarouselTitle" );

		if ( GetChildCount() > 1 )
			MoveChildBefore( m_pTitleLabel, GetChild( 0 ) );
		
		int iFocus = GetChildIndex( m_pFocusedChild.Get() );
		m_pFocusedChild = (iFocus != -1 ) ? GetChild( iFocus ) : m_pTitleLabel;		
	}

	return m_pTitleLabel;
}


//-----------------------------------------------------------------------------
// Purpose: Set title text, which is optional
//-----------------------------------------------------------------------------
void CCarousel::SetTitleText( const char *pchTitle )
{
	CreateTitleLabel()->SetText( pchTitle );
}


//-----------------------------------------------------------------------------
// Purpose: Set the title area visible or invisible
//-----------------------------------------------------------------------------
void CCarousel::SetTitleVisible( bool bVisible )
{
	CreateTitleLabel()->SetVisible( bVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Enables/Disables panel wrapping
//-----------------------------------------------------------------------------
void CCarousel::SetWrap( bool bWrap )
{
	m_bWrap = bWrap;
	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Sets focus mode on panel
//-----------------------------------------------------------------------------
void CCarousel::SetFocusType( EFocusType eType )
{
	m_eFocusType = eType;
	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Sets offset for focus panel
//-----------------------------------------------------------------------------
void CCarousel::SetOffset( CUILength len )
{
	m_lenOffset = len;
	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Sets index of child to focus
//-----------------------------------------------------------------------------
bool CCarousel::SetFocusToIndex( int iFocus )
{
	if ( iFocus < 0 || iFocus >= GetChildCount() )
		return false;

	CPanel2D *pChild = GetChild( iFocus );
	bool bResult = pChild->SetFocus();
	if ( bResult )
	{
		CarouselSelectedChildChanged();
	}
	return bResult;
}


//-----------------------------------------------------------------------------
// Purpose: Tries to set focus to the specified panel or one of its children
//-----------------------------------------------------------------------------
bool CCarousel::BSetFocusToChild( CPanel2D *pChild )
{
	if ( !pChild->BIsVisible() || !pChild->IsEnabled() )
		return false;

	bool bResult = pChild->SetFocus();
	if ( bResult )
	{
		CarouselSelectedChildChanged();
	}
	return bResult;
}


//-----------------------------------------------------------------------------
// Purpose: Handle move event
//-----------------------------------------------------------------------------
bool CCarousel::OnMoveRight( int nRepeats )
{
	if ( GetChildCount() == 0 )
		return false;	

	if ( m_bWrap && UIEngine()->GetCurrentFrameTime() - m_flLastMove < k_flMaxWrapMoveRepeatInterval  )
	{
		if ( !m_bDelayedMovePosted )
		{
			DispatchEventAsync( k_flMaxWrapMoveRepeatInterval - UIEngine()->GetCurrentFrameTime() - m_flLastMove, MoveRight(), this, nRepeats );
			m_bDelayedMovePosted = true;
		}
		return true;
	}

	m_bDelayedMovePosted = false;

	m_flLastMove = UIEngine()->GetCurrentFrameTime();

	int iFocus = GetChildIndex( m_pFocusedChild.Get() );
	int nOffset = m_bWrap ? 0 - iFocus : 0;
	int i = iFocus;
	while ( i + nOffset < GetChildCount() - 1 )
	{
		i++;
		CPanel2D *pChild = GetChild( i % GetChildCount() );
		if ( BSetFocusToChild( pChild ) )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handle move event
//-----------------------------------------------------------------------------
bool CCarousel::OnMoveLeft( int nRepeats )
{
	if ( GetChildCount() == 0 )
		return false;

	if ( m_bWrap && UIEngine()->GetCurrentFrameTime() - m_flLastMove < k_flMaxWrapMoveRepeatInterval  )
	{
		if ( !m_bDelayedMovePosted )
		{
			DispatchEventAsync( k_flMaxWrapMoveRepeatInterval - UIEngine()->GetCurrentFrameTime() - m_flLastMove, MoveLeft(), this, nRepeats );
			m_bDelayedMovePosted = true;
		}
		return true;
	}

	m_bDelayedMovePosted = false;

	m_flLastMove = UIEngine()->GetCurrentFrameTime();

	int iFocus = GetChildIndex( m_pFocusedChild.Get() );
	int cRemaining = m_bWrap ? GetChildCount() - 1 : iFocus;
	int i = GetPreviousWrapPanel( iFocus );
	while ( cRemaining > 0 )
	{
		CPanel2D *pChild = GetChild( i );
		if ( BSetFocusToChild( pChild ) )
		{
			return true;		
		}

		cRemaining--;
		i = GetPreviousWrapPanel( i );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles request to scroll from mouse region
//-----------------------------------------------------------------------------
bool CCarousel::EventCarouselMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat )
{
	CPanel2D *pPanel = ToPanel2D(ptrPanel.Get());
	if ( pPanel == m_pLeftMouseScrollRegion )
	{
		if ( m_pFocusedChild.Get() )
			DispatchEvent( MoveLeft(), m_pFocusedChild.Get(), cRepeat );
		return true;
	}
	else if ( pPanel == m_pRightMouseScrollRegion )
	{
		if ( m_pFocusedChild.Get() )
			DispatchEvent( MoveRight(), m_pFocusedChild.Get(), cRepeat );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Set panel properties
//-----------------------------------------------------------------------------
bool CCarousel::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{

	static CPanoramaSymbol symWrap( "wrap" );
	static CPanoramaSymbol symFocus( "focus" );
	static CPanoramaSymbol symOffset( "focus-offset" );
	static CPanoramaSymbol symText( "title" );
	static CPanoramaSymbol symXOffset( "x-offset" );
	static CPanoramaSymbol symYOffset( "y-offset" );
	static CPanoramaSymbol symZOffset( "z-offset" );
	static CPanoramaSymbol symFocusXOffset( "focus-x-offset" );
	static CPanoramaSymbol symFocusYOffset( "focus-y-offset" );
	static CPanoramaSymbol symFocusZOffset( "focus-z-offset" );
	static CPanoramaSymbol symIncludeScale2d( "include-scale2d" );
	static CPanoramaSymbol symShuffleIntoView( "shuffle-into-view" );
	static CPanoramaSymbol symAutoScrollDelay( "autoscroll-delay" );

	//
	// The panels-visible attribute is intended for use in carousels that have
	// large numbers of items.  The attribute value, which is an int,
	// specifies the maximum number of panels to be displayed on either side
	// of focus.  Panels outside of this range are tagged with "Offscreen"
	// style, which can be styled in css to collapse the panel to
	// save processing time and memory.
	//
	// When using this attribute, setting the panels not visible on your own
	// will yield unpredictable results, so don't do that.
	//
	static CPanoramaSymbol symPanelsVisible( "panels-visible" );

	if ( symName == symText )
	{
		if ( V_strlen( pchValue ) )
		{
			SetTitleText( pchValue );
			SetTitleVisible( true );
		}
		else
		{
			SetTitleText( "" );
			SetTitleVisible( false );
		}
		return true;
	}
	else if ( symName == symShuffleIntoView )
	{
		if ( V_stricmp( pchValue, "true" ) == 0 )
			m_bShuffleIntoView = true;
		else if ( V_stricmp( pchValue, "false" ) == 0 )
			m_bShuffleIntoView = false;
		else if ( V_atoi( pchValue ) )
			m_bShuffleIntoView = true;
		else
			return false;

		return true;
	}
	else if ( symName == symPanelsVisible )
	{
		m_nPanelsVisible = V_atoi( pchValue );
		return true;
	}
	else if ( symName == symWrap )
	{
		if ( V_stricmp( pchValue, "true" ) == 0 )
			SetWrap( true );
		else if ( V_stricmp( pchValue, "false" ) == 0 )
			SetWrap( false );
		else
			return false;

		return true;
	}
	else if ( symName == symFocus )
	{
		if ( V_stricmp( pchValue, "left" ) == 0 )
			SetFocusType( k_EFocusTypeLeft );
		else if ( V_stricmp( pchValue, "center" ) == 0 )
			SetFocusType( k_EFocusTypeCenter );
		else if ( V_stricmp( pchValue, "edge" ) == 0 )
			SetFocusType( k_EFocusTypeEdge );
		else
			return false;

		return true;
	}
	else if ( symName == symOffset )
	{
		CUILength len;
		if ( !CSSHelpers::BParseIntoUILength( &len, pchValue ) )
			return false;

		SetOffset( len );
		return true;
	}
	else if ( symName == symXOffset )
	{
		m_childOffsets.x.RemoveAll();
		return CSSHelpers::BParseCommaSepListWithScaling( &m_childOffsets.x, CSSHelpers::BParseIntoUILength, pchValue, GetActualUIScaleX() );		
	}
	else if ( symName == symYOffset )
	{
		m_childOffsets.y.RemoveAll();
		return CSSHelpers::BParseCommaSepListWithScaling( &m_childOffsets.y, CSSHelpers::BParseIntoUILength, pchValue, GetActualUIScaleY() );		
	}
	else if ( symName == symZOffset )
	{
		m_childOffsets.z.RemoveAll();
		return CSSHelpers::BParseCommaSepListWithScaling( &m_childOffsets.z, CSSHelpers::BParseIntoUILength, pchValue, GetActualUIScaleZ() );		
	}
	else if ( symName == symFocusXOffset )
	{
		m_childOffsetsFocus.x.RemoveAll();
		return CSSHelpers::BParseCommaSepListWithScaling( &m_childOffsetsFocus.x, CSSHelpers::BParseIntoUILength, pchValue, GetActualUIScaleX() );		
	}
	else if ( symName == symFocusYOffset )
	{
		m_childOffsetsFocus.y.RemoveAll();
		return CSSHelpers::BParseCommaSepListWithScaling( &m_childOffsetsFocus.y, CSSHelpers::BParseIntoUILength, pchValue, GetActualUIScaleY() );		
	}
	else if ( symName == symFocusZOffset )
	{
		m_childOffsetsFocus.z.RemoveAll();
		return CSSHelpers::BParseCommaSepListWithScaling( &m_childOffsetsFocus.z, CSSHelpers::BParseIntoUILength, pchValue, GetActualUIScaleZ() );		
	}
	else if ( symName == symIncludeScale2d )
	{
		if ( V_stricmp( pchValue, "true" ) == 0 )
			m_bIncludeScale2d = true;
		else if ( V_stricmp( pchValue, "false" ) == 0 )
			m_bIncludeScale2d = false;
		else
			return false;

		return true;
	}
	else if ( symName == symAutoScrollDelay )
	{
		return SetAutoScrollDelay( pchValue );
	}
	
	return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to unparse comma list of offsets
//-----------------------------------------------------------------------------
static void GetDebugPropertyOffsetList( CUtlVector< DebugPropertyOutput_t *> *pvecProperties, const char *pchName, const CUtlVector< panorama::CUILength > & vecOffsets, float flScaleFactor )
{
	if ( vecOffsets.Count() == 0 )
		return;

	CFmtStr1024 fmtBuffer;
	const char *pchSep = "";
	FOR_EACH_VEC( vecOffsets, i )
	{
		fmtBuffer.Append( pchSep );
		CUILength len = vecOffsets[i];
		len.ScaleLengthValue( flScaleFactor );
		CSSHelpers::AppendUILength( &fmtBuffer, len );
		pchSep = ",";
	}
	pvecProperties->AddToTail( new DebugPropertyOutput_t( pchName, fmtBuffer ) );
}


//-----------------------------------------------------------------------------
// Purpose: Format custom properties for display in debugger
//-----------------------------------------------------------------------------
void CCarousel::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	static CPanoramaSymbol symWrap( "wrap" );
	static CPanoramaSymbol symFocus( "focus" );
	static CPanoramaSymbol symOffset( "focus-offset" );
	static CPanoramaSymbol symText( "title" );
	static CPanoramaSymbol symXOffset( "x-offset" );
	static CPanoramaSymbol symYOffset( "y-offset" );
	static CPanoramaSymbol symZOffset( "z-offset" );
	static CPanoramaSymbol symFocusXOffset( "focus-x-offset" );
	static CPanoramaSymbol symFocusYOffset( "focus-y-offset" );
	static CPanoramaSymbol symFocusZOffset( "focus-z-offset" );
	static CPanoramaSymbol symIncludeScale2d( "include-scale2d" );
	static CPanoramaSymbol symShuffleIntoView( "shuffle-into-view" );
	static CPanoramaSymbol symPanelsVisible( "panels-visible" );

	BaseClass::GetDebugPropertyInfo( pvecProperties );

	if ( m_pTitleLabel && m_pTitleLabel->BIsVisible() )
	{
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symText.String(), m_pTitleLabel->PchGetText() ) );
	}
	if ( m_bShuffleIntoView )
	{
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symShuffleIntoView.String( ), "true" ) );
	}
	if ( m_bWrap )
	{
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symWrap.String( ), "true" ) );
	}
	switch ( m_eFocusType )
	{
	case k_EFocusTypeLeft:
		// default
		// pvecProperties->AddToTail( new DebugPropertyOutput_t( symFocus.String( ), "left" ) );
		break;
	case k_EFocusTypeCenter:
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symFocus.String( ), "center" ) );
		break;
	case k_EFocusTypeEdge:
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symFocus.String( ), "edge" ) );
		break;
	}
	if ( m_lenOffset.IsSet() )
	{
		CFmtStr1024 fmtBuffer;
		CSSHelpers::AppendUILength( &fmtBuffer, m_lenOffset );
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symOffset.String( ), fmtBuffer ) );
	}

	GetDebugPropertyOffsetList( pvecProperties, symXOffset.String(), m_childOffsets.x, GetActualUIScaleX() );
	GetDebugPropertyOffsetList( pvecProperties, symYOffset.String(), m_childOffsets.y, GetActualUIScaleY() );
	GetDebugPropertyOffsetList( pvecProperties, symZOffset.String(), m_childOffsets.z, GetActualUIScaleZ() );

	GetDebugPropertyOffsetList( pvecProperties, symFocusXOffset.String(), m_childOffsetsFocus.x, GetActualUIScaleX() );
	GetDebugPropertyOffsetList( pvecProperties, symFocusYOffset.String(), m_childOffsetsFocus.y, GetActualUIScaleY() );
	GetDebugPropertyOffsetList( pvecProperties, symFocusZOffset.String(), m_childOffsetsFocus.z, GetActualUIScaleZ() );

	if ( m_bIncludeScale2d )
	{
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symIncludeScale2d.String( ), "true" ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when one of our style classes changed
//-----------------------------------------------------------------------------
void CCarousel::OnStylesChanged()
{
	BaseClass::OnStylesChanged();

	// only set positions if we are in a flowing layout. When leaving a flowing layout, clear all child positions
	EFlowDirection eFlowDirection;
	AccessStyle()->GetFlowChildren( eFlowDirection );
	if ( m_bFlowingLayout && eFlowDirection != k_EFlowRight )
	{
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			GetChild( i )->ClearPropertyFromCode( CStylePropertyPosition::symbol );
		}		
	}

	m_bFlowingLayout = (eFlowDirection == k_EFlowRight);
}


//-----------------------------------------------------------------------------
// Purpose: This panel or a child just received focus
//-----------------------------------------------------------------------------
bool CCarousel::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	// Don't do anything if told not to
	if ( !m_bReactToFocusChange )
		return false;

	// can't do anything w/o children
	if ( GetChildCount() == 0 )
		return false;

	// If we received focus, set focus to our first children. 
	CPanel2D *pPanel = ToPanel2D(ptrPanel.Get());
	if ( pPanel == this && GetParentWindow()->UIWindowInput()->GetInputFocus() == UIPanel() )
	{
		if( m_pFocusedChild.Get() )
		{
			if ( BSetFocusToChild( m_pFocusedChild.Get() ) )
				return false;

			// m_iFocusedChild doesn't accept input? Try to move to next
			OnMoveRight( 0 );
			return false;
		}
		else if( SetInputFocusToFirstOrLastChildInFocusOrder(k_ENextInTabOrder, GetSelectionPositionX(), GetSelectionPositionY() ) )
			return false;
	}

	// must be on a descendant.. find our child and set focus changed	
	while ( pPanel && pPanel->GetParent() != this )
	{
		pPanel = pPanel->GetParent();
	}

	if ( pPanel && pPanel->GetParent() == this && m_pFocusedChild.Get() != pPanel )
	{
		MarkFocusDirty();
		m_pFocusedChild = pPanel;
		CarouselSelectedChildChanged();
	}

	// we need to invalidate position when gaining or losing focus, so "x-offset-focus" etc. work correctly
	if ( (BHasKeyFocus() || BHasDescendantKeyFocus()) && !m_bHadFocus )
	{
		InvalidatePosition();
		RegisterForCursorChanges();
		m_bHadFocus = true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CCarousel::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "SetSelectedChild", PANORAMA_DELEGATE( &CCarousel::SetSelectedChild ) );
	RegisterJSMethod( "GetFocusChild", PANORAMA_DELEGATE( &CCarousel::GetFocusChild ) );
	RegisterJSMethod( "GetFocusIndex", PANORAMA_DELEGATE( &CCarousel::GetFocusIndex ) );
	RegisterJSMethod( "SetAutoScrollEnabled", PANORAMA_DELEGATE( &CCarousel::SetAutoScrollEnabled ) );
}

//-----------------------------------------------------------------------------
// Purpose: sets the child that will get focus when the carousel has focus. Remembered between focus calls
//-----------------------------------------------------------------------------
void CCarousel::SetSelectedChild( CPanel2D *pPanel )
{
	if ( !pPanel || pPanel->GetParent() != this )
	{
		AssertMsg( false, "Invalid panel passed to CCarousel::SetSelectedChild" );
		return;
	}

	if( BHasKeyFocus() || BHasDescendantKeyFocus() )
	{
		// Update focus within our context, but do not SetFocus() as we don't want context to change at all if we aren't already the active one
		pPanel->UpdateFocusInContext();
	}
	else
	{
		MarkFocusDirty();
		m_pFocusedChild = pPanel;
	}

	CarouselSelectedChildChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever the selected child changed
//-----------------------------------------------------------------------------
void CCarousel::CarouselSelectedChildChanged()
{
	DispatchEvent( SetCarouselSelectedChild(), this, m_pFocusedChild );

	// If we have an auto-scroll event in flight, then we should ignore that
	// and reschedule after a new delay.
	if ( m_bAutoScrollScheduled )
	{
		m_bAutoScrollScheduled = false;
		m_unAutoScrollID++;
		CheckScheduleAutoScroll();
	}
}


//-----------------------------------------------------------------------------
// Purpose: This panel or a child just lost input focus
//-----------------------------------------------------------------------------
bool CCarousel::EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel )
{
	// we need to invalidate position when gaining or losing focus, so "x-offset-focus" etc. work
	if ( (!BHasKeyFocus() && !BHasDescendantKeyFocus()) && m_bHadFocus )
	{
		InvalidatePosition();
		UnregisterForCursorChanges();
		m_bHadFocus = false;		
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Marks carousel specific child styles as dirty to be reapplied next frame
//-----------------------------------------------------------------------------
void CCarousel::MarkFocusDirty()
{
	if ( m_pDirtyChildStyles )
		return;

	m_pDirtyChildStyles = new DirtyChildStyles_t();
	m_pDirtyChildStyles->m_iOriginalFocus = GetChildIndex( m_pFocusedChild.Get() );
	m_pDirtyChildStyles->m_vecPanels.EnsureCapacity( GetChildCount() );
	
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		m_pDirtyChildStyles->m_vecPanels.AddToTail( GetChild( i ) );
	}

	UIEngine()->CallBeforeStyleAndLayout( UIPanel() );
}


//-----------------------------------------------------------------------------
// Purpose: Updates focus pointer & fixes dirty child styles
//-----------------------------------------------------------------------------
bool CCarousel::UpdateFocusAndDirtyChildStyles()
{
	if ( !m_pDirtyChildStyles )
		return true;

	// check if focus child has changed
	int iFocusedChild = GetChildIndex( m_pFocusedChild.Get() );
	if ( iFocusedChild == -1 && GetChildCount() > 0 )
	{
		m_pFocusedChild = GetChild( 0 );
		iFocusedChild = 0;
	}

	// loop through and remove old style
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		if ( m_pDirtyChildStyles->m_iOriginalFocus != -1 )
		{
			int iOldIndex = m_pDirtyChildStyles->m_vecPanels.Find( GetChild( i ) );
			if ( iOldIndex != m_pDirtyChildStyles->m_vecPanels.InvalidIndex() )
				RemoveCarouselStyle( GetChild( i ), iOldIndex, m_pDirtyChildStyles->m_iOriginalFocus );
		}

		if ( iFocusedChild != -1 )
			AddCarouselStyle( GetChild( i ), i, iFocusedChild );
	}

	InvalidatePosition();
	SAFE_DELETE( m_pDirtyChildStyles );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Forcibly refreshes all the left/right focus styles on children
//-----------------------------------------------------------------------------
void CCarousel::ForceRefreshChildFocusStyles()
{
	int iFocusedChild = GetChildIndex( m_pFocusedChild.Get() );
	if ( iFocusedChild == -1 && GetChildCount() > 0 )
	{
		m_pFocusedChild = GetChild( 0 );
		iFocusedChild = 0;
	}

	for ( int iChild = 0; iChild < GetChildCount(); iChild++ )
	{
		CPanel2D *pChild = GetChild( iChild );
		if ( pChild )
		{
			for ( int iClass = 0; iClass < m_nChildCountHighWatermark; iClass++ )
			{
				pChild->RemoveClass( CFmtStr( k_rgchLeftChildStyle, iClass ) );
				pChild->RemoveClass( CFmtStr( k_rgchRightChildStyle, iClass ) );
				pChild->RemoveClass( k_rgchFocusedChildStyle );
			}

			AddCarouselStyle( pChild, iChild, iFocusedChild );
		}
	}

	InvalidatePosition();
}


//-----------------------------------------------------------------------------
// Purpose: Adds carousel styles to the specified child
//-----------------------------------------------------------------------------
void CCarousel::AddCarouselStyle( CPanel2D *pChild, int iChild, int iFocus )
{
	static const CPanoramaSymbol k_symPreviouslyLeft = "PreviouslyLeft";
	static const CPanoramaSymbol k_symPreviouslyRight = "PreviouslyRight";

	int nOffset = abs( iFocus - iChild );

	if( iChild < iFocus )
	{
		pChild->AddClass( CFmtStr( k_rgchLeftChildStyle, nOffset ) );
		pChild->AddClass( k_symPreviouslyLeft );
		pChild->RemoveClass( k_symPreviouslyRight );
	}
	else if( iChild > iFocus )
	{
		pChild->AddClass( CFmtStr( k_rgchRightChildStyle, nOffset ) );
		pChild->AddClass( k_symPreviouslyRight );
		pChild->RemoveClass( k_symPreviouslyLeft );
	}
	else if( iChild == iFocus )
	{
		pChild->AddClass( k_rgchFocusedChildStyle );
	}

	if ( m_nPanelsVisible != 0 )
	{
		pChild->SetHasClass( k_rgchOffscreenStyle, nOffset > m_nPanelsVisible );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes carousel styles from the specified child
//-----------------------------------------------------------------------------
void CCarousel::RemoveCarouselStyle( CPanel2D *pChild, int iChild, int iFocus )
{
	int nOffset = abs( iFocus - iChild );

	if ( iChild < iFocus )
	{
		pChild->RemoveClass( CFmtStr( k_rgchLeftChildStyle, nOffset ) );
	}
	else if ( iChild > iFocus )
	{
		pChild->RemoveClass( CFmtStr( k_rgchRightChildStyle, nOffset ) );
	}
	else if ( iChild == iFocus )
	{
		pChild->RemoveClass( k_rgchFocusedChildStyle );
	}

	if ( m_nPanelsVisible != 0 )
	{
		pChild->SetHasClass( k_rgchOffscreenStyle, nOffset > m_nPanelsVisible );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called before an operation changes our children (add, remove, move, sort, etc.)
//-----------------------------------------------------------------------------
void CCarousel::OnBeforeChildrenChanged()
{
	MarkFocusDirty();
}


//-----------------------------------------------------------------------------
// Purpose: Called after an operation changes our children (add, remove, move, sort, etc.)
//-----------------------------------------------------------------------------
void CCarousel::OnAfterChildrenChanged()
{
	// Notify any listeners that our children might have changed
	DispatchEvent( CarouselChildrenChanged(), this );

	CheckScheduleAutoScroll();

	m_nChildCountHighWatermark = Max( m_nChildCountHighWatermark, GetChildCount() );
}


//-----------------------------------------------------------------------------
// Purpose: Layout traverse
//-----------------------------------------------------------------------------
void CCarousel::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	SPEW_CAROUSEL( "0x%x:CCarousel::OnLayoutTraverse\n", this );

	// only set positions if we are in a flowing layout
	EFlowDirection eFlowDirection;
	AccessStyle()->GetFlowChildren( eFlowDirection );

	// include padding
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flFinalWidth, flFinalHeight, false, flLeft, flTop, flRight, flBottom );

	// Our total space minus padding, used to compute % based margins on children below
	const float flContainerWidth = flFinalWidth - flLeft - flRight;
	const float flContainerHeight = flFinalHeight - flTop - flBottom;

	// position each child
	CUtlVector< CPanel2D* > vecNewChildren;
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		CPanel2D *pChild = GetChild( i );
		if ( !pChild->BIsVisible() )
			continue;

		CUILength marginLeft, marginTop, marginRight, marginBottom;
		pChild->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flContainerWidth );
		marginRight.ConvertToLength( flContainerWidth );
		marginTop.ConvertToLength( flContainerHeight );
		marginBottom.ConvertToLength( flContainerHeight );

		// by default, give the child all remaining space after padding. Flow directions can override per child.
		float flChildWidth = flFinalWidth - flLeft - flRight;
		if ( eFlowDirection == k_EFlowRight )
			flChildWidth = pChild->GetDesiredLayoutWidth() + marginLeft.GetValue() + marginRight.GetValue();

		float flChildHeight = flFinalHeight - flTop - flBottom;

		// Ensure we are on pixel boundaries for edges of flowing panels, helps prevent fuzzy text and gaps between
		// anti-aliased edges, last child gets extra round off error.
		flChildWidth = RoundFloatToInt( flChildWidth );

		if ( !pChild->BHasBeenLayedOut() )
		{
			vecNewChildren.EnsureCapacity( GetChildCount() );
			vecNewChildren.AddToTail( pChild );
		}

		pChild->LayoutTraverse( flLeft, flTop, flChildWidth - marginLeft.GetValue() - marginRight.GetValue(), flChildHeight - marginTop.GetValue() - marginBottom.GetValue() );
	}

	// done with layout.. continue if we need to adjust positions (done once per invalidate.. position transitions are done on the animation thread)
	if ( GetChildCount() == 0 )
		return;

	// if we are in edge snapping mode and our width somehow got calculated to 0, don't continue. We don't want to change what edge we thought we should snap
	// the focus element to. Another layout pass should come in and fix our width.
	if ( flFinalWidth < 1.0f && m_eFocusType == k_EFocusTypeEdge )
		return;

	if ( eFlowDirection != k_EFlowRight )
	{
		Assert( eFlowDirection == k_EFlowNone );
		return;
	}

	int iFocus = GetChildIndex( m_pFocusedChild.Get() );
	if ( iFocus == -1 )
	{
		AssertMsg( false, "Need focus child" );
		return;
	}

	// calculate offset specified by carousel offset property
	float flCarouselOffset = m_lenOffset.GetValueAsLength( flContainerWidth ) * GetActualUIScaleX();

	// layout children
	float flOffset = 0.0f;	
	GetLayoutStart( iFocus, &flOffset, flLeft, flCarouselOffset, flContainerWidth, flContainerHeight );
	LayoutChildPanels( iFocus, flOffset, flLeft, flRight, flContainerWidth, flContainerHeight, vecNewChildren );
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the first child & its offset to use in the layout pass
//-----------------------------------------------------------------------------
void CCarousel::GetLayoutStart( int iFocusChild, float *pflOffset, float flLeft, float flCarouselOffset, const float flContainerWidth, const float flContainerHeight ) 
{
	*pflOffset = 0;

	if ( m_eFocusType == k_EFocusTypeLeft )
	{
		*pflOffset = flLeft + flCarouselOffset;
	}
	else if ( m_eFocusType == k_EFocusTypeCenter )
	{
		// calculate where the left edge should be from center
		CPanel2D *pFocus = GetChild( iFocusChild );
		if ( pFocus )
		{
			float flChildWidth, flChildHeight;
			GetFinalChildDimensions( &flChildWidth, &flChildHeight, pFocus, flContainerHeight );			

			float flLeftEdge = (flContainerWidth / 2.0f) - (flChildWidth / 2.0f) + flCarouselOffset;
			*pflOffset = flLeft + flLeftEdge;
		}
	}
	else if ( m_eFocusType == k_EFocusTypeEdge )
	{
		bool bBeyondLeftEdge = (m_iFocusLastEdge > iFocusChild);
		bool bBeyondRightEdge = (m_iFocusLastEdge < iFocusChild);
		if ( m_bWrap )
		{
			Assert( false );
		}

		if ( m_eLastFocusEdge == k_EFocusEdgeLeft && bBeyondLeftEdge )
		{
			// new child is before the left edge
			m_iFocusLastEdge = iFocusChild;
			m_eLastFocusEdge = k_EFocusEdgeLeft;

			*pflOffset = flLeft + flCarouselOffset;
		}
		else if ( m_eLastFocusEdge == k_EFocusEdgeRight && bBeyondRightEdge )
		{
			// new child is after right edge
			m_iFocusLastEdge = iFocusChild;
			m_eLastFocusEdge = k_EFocusEdgeRight;

			float flWidth = GetFinalChildWidth( GetChild( iFocusChild ), flContainerHeight );
			*pflOffset = flContainerWidth - flCarouselOffset -  flWidth;
		}
		else
		{
			// new child could require us to change edges.. check if fits
			float flRequiredWidth = 0.0f;
			bool bFoward = (m_eLastFocusEdge == k_EFocusEdgeRight);
			int nDistanceFromFocus = 0;
			for ( int i = iFocusChild; i != k_iInvalidChild; i = bFoward ? GetNextPanelInLayout( i ) : GetPreviousPanelInLayout( i ) )
			{
				float flChildWidth, flChildHeight;
				GetFinalChildDimensions( &flChildWidth, &flChildHeight, GetChild( i ), flContainerHeight );

				CUILength lenXOffset, lenYOffset, lenZOffset;
				GetPanelOffsets( &lenXOffset, &lenYOffset, &lenZOffset, nDistanceFromFocus++, flChildWidth, flChildHeight );

				flRequiredWidth += flChildWidth + lenXOffset.GetValue();

				// stop when we have included the focus edge
				if ( i == m_iFocusLastEdge )
					break;
			}				

			if ( flRequiredWidth + flLeft + flCarouselOffset > flContainerWidth )
			{
				// entire panel wont fit.. set as edge panel
				m_iFocusLastEdge = iFocusChild;
				m_eLastFocusEdge = ( m_eLastFocusEdge == k_EFocusEdgeLeft ) ? k_EFocusEdgeRight : k_EFocusEdgeLeft;
				flRequiredWidth = GetFinalChildWidth( GetChild( iFocusChild ), flContainerHeight );
			}

			if ( m_eLastFocusEdge == k_EFocusEdgeLeft )
			{
				float flFocusWidth = GetFinalChildWidth( GetChild( iFocusChild ), flContainerHeight );
				*pflOffset = flLeft + flCarouselOffset + flRequiredWidth - flFocusWidth;
			}
			else
			{
				*pflOffset = flContainerWidth - flRequiredWidth;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finds the next panel in layout (visible, accounts for wrapping)
//-----------------------------------------------------------------------------
int CCarousel::GetNextPanelInLayout( int iStart )
{
	if ( m_bWrap )
	{
		int i = GetNextWrapPanel( iStart );
		while ( i != iStart )
		{
			CPanel2D *pChild = GetChild( i );
			if ( pChild->BIsVisible() )
				break;

			i = GetNextWrapPanel( i );
		}

		return (i == iStart) ? k_iInvalidChild : i;
	}

	// no wrap
	int i = iStart + 1;
	while ( i < GetChildCount() )
	{
		CPanel2D *pChild = GetChild( i );
		if ( pChild->BIsVisible() || m_nPanelsVisible > 0 )
			break;

		i++;
	}

	return (i >= GetChildCount()) ? k_iInvalidChild : i;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the previous panel in layout (visible, accounts for wrapping)
//-----------------------------------------------------------------------------
int CCarousel::GetPreviousPanelInLayout( int iStart )
{
	if ( m_bWrap )
	{
		int i = GetPreviousWrapPanel( iStart );
		while ( i != iStart )
		{
			CPanel2D *pChild = GetChild( i );
			if ( pChild->BIsVisible() )
				break;

			i = GetPreviousWrapPanel( i );
		}

		return (i == iStart) ? k_iInvalidChild : i;
	}

	// no wrap
	int i = iStart - 1;
	while ( i >= 0 )
	{
		CPanel2D *pChild = GetChild( i );
		if ( pChild->BIsVisible() || m_nPanelsVisible > 0 )
			break;

		i--;
	}

	return (i < 0) ? k_iInvalidChild : i;
}


//-----------------------------------------------------------------------------
// Purpose: Sets position of child panels for layout
//-----------------------------------------------------------------------------
void CCarousel::LayoutChildPanels( int iFocusChild, float flOffset, float flLeft, float flRight, const float flContainerWidth, const float flContainerHeight, const CUtlVector< CPanel2D* > &vecNewChildren )
{
	SPEW_CAROUSEL( "***Layout info: flOffest=%f, flLeft=%f, flContainerWidth=%f, flContainerHeight=%f\n", flOffset, flLeft, flContainerWidth, flContainerHeight );
	
	// draw start panel
	float flOffsetRight = flOffset;
	DbgVerify( BPositionPanelRight( iFocusChild, 0, &flOffsetRight, flLeft, flContainerWidth, flContainerHeight, false, vecNewChildren ) );
	GetChild( iFocusChild )->AccessStyle()->SetZIndex( 0 );

	// draw any panels to the right of that panel
	int iLastRight = iFocusChild;
	int cDrawnRight = 0;
	SPEW_CAROUSEL( "***First loop\n" );
	while ( true )
	{
		int iNext = GetNextPanelInLayout( iLastRight );
		if ( iNext == k_iInvalidChild || iNext == iFocusChild )
			break;
		
		if ( !BPositionPanelRight( iNext, cDrawnRight + 1, &flOffsetRight, flLeft, flContainerWidth, flContainerHeight, m_bWrap, vecNewChildren ) )
			break;

		cDrawnRight++;
		GetChild( iNext )->AccessStyle()->SetZIndex( 0 - cDrawnRight );
		iLastRight = iNext;
	}

	// draw any panels to left of start
	float flOffsetLeft = flOffset;
	int iLastLeft = iFocusChild;
	int cDrawnLeft = 0;
	SPEW_CAROUSEL( "***Second loop\n" );
	while ( true )
	{
		int iNext = GetPreviousPanelInLayout( iLastLeft );
		if ( iNext == k_iInvalidChild || iNext == iLastRight )
			break;

		if ( !BPositionPanelLeft( iNext, cDrawnLeft + 1, &flOffsetLeft, flLeft, flContainerWidth, flContainerHeight, true, vecNewChildren ) )
			break;

		// set zindex
		cDrawnLeft++;
		GetChild( iNext )->AccessStyle()->SetZIndex( 0 - cDrawnLeft );
		iLastLeft = iNext;
	}

	SPEW_CAROUSEL( "Last left=%d, Last right=%d\n", iLastLeft, iLastRight );
	// position the remaining panels to the right	
	if ( m_bWrap )
	{
		SPEW_CAROUSEL( "***Putting remaining visible which were left, left\n" );
		// if panel was visible on left, keep it going that way
		int iNext = iLastLeft;
		while ( true )
		{
			iNext = GetPreviousPanelInLayout( iNext );
			if ( iNext == k_iInvalidChild || iNext == iLastRight )
				break;

			CPanel2D *pChild = GetChild( iNext );
			CUILength lenX, lenY, lenZ;
			pChild->AccessStyle()->GetInterpolatedPosition( lenX, lenY, lenZ, false );
			float flChildX = lenX.GetValueAsLength( flContainerWidth );
			if ( flChildX >= flLeft + flContainerWidth + flRight || flChildX + flContainerWidth <= flLeft )
				continue;

			DbgVerify( BPositionPanelLeft( iNext, cDrawnLeft + 1, &flOffsetLeft, flLeft, flContainerWidth, flContainerHeight, false, vecNewChildren ) );
			cDrawnLeft++;
			pChild->AccessStyle()->SetZIndex( 0 - cDrawnLeft );
			iLastLeft = iNext;
		}

		SPEW_CAROUSEL( "***Putting remaining right\n" );
		// place the remaining right
		iNext = iLastRight;
		while ( true )
		{
			iNext = GetNextPanelInLayout( iNext );
			if ( iNext == k_iInvalidChild || iNext == iLastLeft )
				break;		

			DbgVerify( BPositionPanelRight( iNext, cDrawnRight + 1, &flOffsetRight, flLeft, flContainerWidth, flContainerHeight, false, vecNewChildren ) );
			cDrawnRight++;
			GetChild( iNext )->AccessStyle()->SetZIndex( 0 - cDrawnRight );
			iLastRight = iNext;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper to get the next panel with wrapping
//-----------------------------------------------------------------------------
int CCarousel::GetNextWrapPanel( int i )
{
	return (i + 1) % GetChildCount();
}


//-----------------------------------------------------------------------------
// Purpose: Helper to get the previous panel with wrapping
//-----------------------------------------------------------------------------
int CCarousel::GetPreviousWrapPanel( int i )
{
	if ( i == 0 )
		return (GetChildCount() - 1);

	return (i - 1);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the position of a panel when layout out panels from left to right
// Returns: true if panel was positioned, false if we were done (ran out of room to left)
// Params: pflOffset is IN/OUT
//-----------------------------------------------------------------------------
bool CCarousel::BPositionPanelRight( int iPanel, int nDistanceFromFocus, float *pflOffset, float flLeft, float flContainerWidth, float flContainerHeight, bool bCheckFits, const CUtlVector< CPanel2D* > &vecNewChildren )
{
	CPanel2D *pChild = GetChild( iPanel );
	if ( !pChild )
		return true;

	bool bVisible = pChild->BIsVisible();
	if ( m_nPanelsVisible != 0 )
	{
		if ( nDistanceFromFocus < m_nPanelsVisible )
		{
			pChild->RemoveClass( k_rgchOffscreenStyle );
		}
		else
		{
			pChild->AddClass( k_rgchOffscreenStyle );
			bVisible = false;
		}
	}

	if ( !bVisible )
		return true;

	// get current position
	CUILength lenX, lenY, lenZ;
	pChild->AccessStyle()->GetInterpolatedPosition( lenX, lenY, lenZ, true );

	float flChildWidth, flChildHeight;
	GetFinalChildDimensions( &flChildWidth, &flChildHeight, pChild, flContainerHeight );
	
	CUILength lenXOffset, lenYOffset, lenZOffset;
	GetPanelOffsets( &lenXOffset, &lenYOffset, &lenZOffset, nDistanceFromFocus, flChildWidth, flChildHeight );

	float flOffset = *pflOffset - lenXOffset.GetValue(); // lenXOffset already a length, not percentage

	// if wrapping, we need stop once all previous & currently visible panels have been drawn
	if ( bCheckFits && m_bWrap && flOffset >= flContainerWidth )
	{
		SPEW_CAROUSEL( "****BPositionRight: Panel %d didn't fit\n", iPanel );
		return false;
	}

	// if previously offscreen left, we need to move to where it would be off screen right w/o transition (so the panel does not
	// slide across the screen)
	if ( m_bWrap && lenX.GetValueAsLength( flContainerWidth ) + pChild->GetActualLayoutWidth() <= flLeft )
	{
		int iPrev = GetPreviousPanelInLayout( iPanel );
		CPanel2D *pPrevChild = GetChild( iPrev );

		CUILength lenImmediateX, lenImmediateY, lenImmediateZ;
		pPrevChild->AccessStyle()->GetInterpolatedPosition( lenImmediateX, lenImmediateY, lenImmediateZ, false );

		float flPrevWidth, flPrevHeight;
		GetFinalChildDimensions( &flPrevWidth, &flPrevHeight, pPrevChild, flContainerHeight );

		CUILength lenPrevXOffset, lenPrevYOffset, lenPrevZOffset;
		GetPanelOffsets( &lenPrevXOffset, &lenPrevYOffset, &lenPrevZOffset, nDistanceFromFocus + 1, flPrevWidth, flPrevHeight );

		float flImmediateX = lenImmediateX.GetValueAsLength( flContainerWidth ) + flPrevWidth - lenPrevXOffset.GetValue();
		CUILength lenXOffsetNow( flImmediateX, CUILength::k_EUILengthLength );
		CUILength lenYOffsetNow = lenYOffset;

		lenXOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
		lenYOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleY() );
		lenZOffset.ScaleLengthValue( 1.0f / GetActualUIScaleZ() );

		pChild->SetPositionWithoutTransition( lenXOffsetNow, lenYOffsetNow, lenZOffset );
		SPEW_CAROUSEL( "****BPositionRight: Switching sides.. setting position w/o transition on %d\n", iPanel );
	}	

	// only need to update if our new position doesn't match set
	if ( lenX.GetValueAsLength( flOffset ) != flOffset || lenY != lenYOffset || lenZ != lenZOffset )
	{
		CUILength lenXOffsetNow( flOffset, CUILength::k_EUILengthLength );
		CUILength lenYOffsetNow = lenYOffset;
		lenXOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
		lenYOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleY() );
		lenZOffset.ScaleLengthValue( 1.0f / GetActualUIScaleZ() );

		if ( !m_bShuffleIntoView && vecNewChildren.HasElement( pChild ) )
			pChild->SetPositionWithoutTransition( lenXOffsetNow, lenYOffsetNow, lenZOffset );
		else
			pChild->SetPosition( lenXOffsetNow, lenYOffsetNow, lenZOffset );
	}

	SPEW_CAROUSEL( "***BPositionPanelRight - %d to %f\n", iPanel, flOffset );

	// measure final
	*pflOffset = flOffset + flChildWidth;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Paint the panel
//-----------------------------------------------------------------------------
void CCarousel::OnUIScaleFactorChanged( const Vector &vOldScaleFactor, const Vector &vNewScaleFactor )
{
	Vector vScaleFactorChange = vNewScaleFactor / vOldScaleFactor;

	FOR_EACH_VEC( m_childOffsets.x, i )
	{
		m_childOffsets.x[ i ].ScaleLengthValue( vScaleFactorChange.x );
	}

	FOR_EACH_VEC( m_childOffsets.y, i )
	{
		m_childOffsets.y[ i ].ScaleLengthValue( vScaleFactorChange.y );
	}

	FOR_EACH_VEC( m_childOffsets.z, i )
	{
		m_childOffsets.z[i].ScaleLengthValue( vScaleFactorChange.z );
	}

	FOR_EACH_VEC( m_childOffsetsFocus.x, i )
	{
		m_childOffsetsFocus.x[ i ].ScaleLengthValue( vScaleFactorChange.x );
	}

	FOR_EACH_VEC( m_childOffsetsFocus.y, i )
	{
		m_childOffsetsFocus.y[ i ].ScaleLengthValue( vScaleFactorChange.y );
	}

	FOR_EACH_VEC( m_childOffsetsFocus.z, i )
	{
		m_childOffsetsFocus.z[i].ScaleLengthValue( vScaleFactorChange.z );
	}

	BaseClass::OnUIScaleFactorChanged( vOldScaleFactor, vNewScaleFactor );
}


//-----------------------------------------------------------------------------
// Purpose: handle mouse wheel
//-----------------------------------------------------------------------------
bool CCarousel::OnResetMouseWheelCounts()
{
	if ( UIEngine()->GetCurrentFrameTime() - m_flLastMouseWheel > 0.4f )
	{
		m_flLastMouseWheel = 0;
		m_unMouseWheelCount = 0;
	}
	else
	{
		DispatchEventAsync( 0.401f - UIEngine()->GetCurrentFrameTime() - m_flLastMouseWheel , ResetCarouselMouseWheelCounts(), this );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: handle mouse wheel
//-----------------------------------------------------------------------------
bool CCarousel::OnMouseWheel( const MouseData_t &code )
{
	float flDelay = 0.0f;
	int iCount = abs( code.m_Delta );

	if ( !(GetStyleFlags() & (k_EStyleFlagDescendantFocused|k_EStyleFlagFocus)) )
		return false;

	if ( UIEngine()->GetCurrentFrameTime() - m_flLastMouseWheel < 0.01f )
		return true;

	// Windows normally sends two or three lines of scroll per mouse wheel movement, but we want to 
	// allow the user to easily scroll one grid item at a time, hence this logic to ignore the configured
	// line interval until we've seen multiple separate scroll events quickly.  We also want to add some level
	// of acceleration so we double the lines if this is a fast repeat.
	if ( UIEngine()->GetCurrentFrameTime() - m_flLastMouseWheel > 0.28f || m_unMouseWheelCount < 4 )
		iCount = 1;

	int iFocusChild = GetChildIndex( m_pFocusedChild.Get() );
	if ( code.m_Delta < 0 )
		iCount = clamp( iCount, 1, MAX( GetChildCount() - iFocusChild - 1, 1 ) );
	else
		iCount = clamp( iCount, 1, MAX( iFocusChild, 1 ) );


	for( int i=0; i < iCount; ++i )
	{
		if ( code.m_Delta < 0 )
			DispatchEventAsync( flDelay, panorama::MoveRight(), m_pFocusedChild.Get(), code.m_RepeatCount );
		else
			DispatchEventAsync( flDelay, panorama::MoveLeft(), m_pFocusedChild.Get(), code.m_RepeatCount );

		if ( i < 2 )
			flDelay += 0.12f;
	}

	if ( m_unMouseWheelCount == 0 )
		DispatchEventAsync( 0.401f, ResetCarouselMouseWheelCounts(), this );

	++m_unMouseWheelCount;
	m_flLastMouseWheel = UIEngine()->GetCurrentFrameTime();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the position of a panel when layout out panels from right to left
// Returns: true if panel was positioned, false if we were done (ran out of room to left)
// Params: pflOffset is IN/OUT
//-----------------------------------------------------------------------------
bool CCarousel::BPositionPanelLeft( int iPanel, int nDistanceFromFocus, float *pflOffset, float flLeft, float flContainerWidth, float flContainerHeight, bool bCheckFits, const CUtlVector< CPanel2D* > &vecNewChildren )
{
	CPanel2D *pChild = GetChild( iPanel );
	if ( !pChild )
		return true;

	bool bVisible = pChild->BIsVisible();
	if ( m_nPanelsVisible != 0 )
	{
		if ( nDistanceFromFocus < m_nPanelsVisible )
		{
			pChild->RemoveClass( k_rgchOffscreenStyle );
		}
		else
		{
			pChild->AddClass( k_rgchOffscreenStyle );
			bVisible = false;
		}
	}

	if ( !bVisible )
		return true;

	float flChildWidth, flChildHeight;
	GetFinalChildDimensions( &flChildWidth, &flChildHeight, pChild, flContainerHeight );

	CUILength lenXOffset, lenYOffset, lenZOffset;
	GetPanelOffsets( &lenXOffset, &lenYOffset, &lenZOffset, nDistanceFromFocus, flChildWidth, flChildHeight );

	float flOffest = *pflOffset - flChildWidth + lenXOffset.GetValue();

	// if wrapping, we need stop once all previous & currently visible panels have been drawn
	if ( bCheckFits && m_bWrap && (flOffest + flChildWidth) <= flLeft )
	{		
		SPEW_CAROUSEL( "****BPositionLeft: Panel %d didn't fit\n", iPanel );
		return false;
	}

	// position
	CUILength lenX, lenY, lenZ;
	pChild->AccessStyle()->GetInterpolatedPosition( lenX, lenY, lenZ, true );

	// if previously off screen right, we need to move to where it would be off screen left w/o transition (so the panel does not
	// slide across the screen)
	if ( m_bWrap && lenX.GetValueAsLength( flContainerWidth ) >= flContainerWidth )
	{
		int iNext = GetNextPanelInLayout( iPanel );
		CPanel2D *pNextChild = GetChild( iNext );

		CUILength lenImmediateX, lenImmediateY, lenImmediateZ;
		pNextChild->AccessStyle()->GetInterpolatedPosition( lenImmediateX, lenImmediateY, lenImmediateZ, false );

		float flNextWidth, flNextHeight;
		GetFinalChildDimensions( &flNextWidth, &flNextHeight, pNextChild, flContainerHeight );

		CUILength lenNextXOffset, lenNextYOffset, lenNextZOffset;
		GetPanelOffsets( &lenNextXOffset, &lenNextYOffset, &lenNextZOffset, nDistanceFromFocus + 1, flNextWidth, flNextHeight );

		float flImmediateX = lenImmediateX.GetValueAsLength( flContainerWidth ) - flNextWidth + lenNextXOffset.GetValue();

		CUILength lenXOffsetNow( flImmediateX, CUILength::k_EUILengthLength );
		CUILength lenYOffsetNow = lenImmediateY;

		lenXOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
		lenYOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleY() );
		lenImmediateZ.ScaleLengthValue( 1.0f / GetActualUIScaleZ() );

		pChild->SetPositionWithoutTransition( lenXOffsetNow, lenYOffsetNow, lenImmediateZ );
		SPEW_CAROUSEL( "****BPositionLeft: Switching sides.. setting position w/o transition on %d\n", iPanel );
	}

	// only need to update if our new position doesn't match set
	if ( lenX.GetValueAsLength( flOffest ) != flOffest || lenY != lenYOffset || lenZ != lenZOffset )
	{
		CUILength lenXOffsetNow( flOffest, CUILength::k_EUILengthLength );
		CUILength lenYOffsetNow = lenYOffset;
		lenXOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
		lenYOffsetNow.ScaleLengthValue( 1.0f / GetActualUIScaleY() );
		lenZOffset.ScaleLengthValue( 1.0f / GetActualUIScaleZ() );

		if ( !m_bShuffleIntoView && vecNewChildren.HasElement( pChild ) )
			pChild->SetPositionWithoutTransition( lenXOffsetNow, lenYOffsetNow, lenZOffset );
		else
			pChild->SetPosition( lenXOffsetNow, lenYOffsetNow, lenZOffset );
	}

	SPEW_CAROUSEL( "***BPositionPanelLeft - %d to %f\n", iPanel, flOffest );

	*pflOffset = flOffest;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the final child layout width, including margins
//-----------------------------------------------------------------------------
float CCarousel::GetFinalChildWidth( CPanel2D *pChild, float flContainerHeight )
{
	float flWidth, flHeight;
	GetFinalChildDimensions( &flWidth, &flHeight, pChild, flContainerHeight );

	return flWidth;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the final child layout width and height, including margins
//-----------------------------------------------------------------------------
void CCarousel::GetFinalChildDimensions( float *pflWidth, float *pflHeight, CPanel2D *pChild, float flContainerHeight )
{
	*pflWidth = 0.0f;
	*pflHeight = 0.0f;
	if ( !pChild->BIsVisible() )
		return;

	// measure final
	if( pChild->IsSizeValid() && pChild->IsChildSizeValid() )
	{
		*pflWidth = pChild->GetDesiredLayoutWidth();
		*pflHeight = pChild->GetDesiredLayoutHeight();
	}
	else
	{
		pChild->DesiredLayoutSizeTraverse( pflWidth, pflHeight, k_flMaxWidthOrHeight, flContainerHeight, true );	
	}

	// make sure to include child margins
	CUILength marginLeft, marginRight, marginTop, marginBottom;
	pChild->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
	if ( marginLeft.IsLength() )
		*pflWidth = *pflWidth + marginLeft.GetValue();
	if ( marginRight.IsLength() )
		*pflWidth = *pflWidth + marginRight.GetValue();
	
	*pflHeight = *pflHeight + marginTop.GetValueAsLength( flContainerHeight );
	*pflHeight = *pflHeight + marginBottom.GetValueAsLength( flContainerHeight );

	return;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the index distance between two panels, includes wrapping
//-----------------------------------------------------------------------------
int CCarousel::CalcIndexDistanceBetweenPanels( int iLHS, int iRHS )
{
	int nDistance = abs( iLHS - iRHS );
	if ( m_bWrap )
	{
		if ( iLHS < iRHS )
			nDistance = MIN( nDistance, iLHS + GetChildCount() - iRHS );
		else
			nDistance = MIN( nDistance, iRHS + GetChildCount() - iLHS );
	}

	return nDistance;
};


//-----------------------------------------------------------------------------
// Purpose: Calculates the index distance from focused panel, then returns the configured X, Y, and Z offsets for that panel
//-----------------------------------------------------------------------------
void CCarousel::GetPanelOffsets( CUILength *plenX, CUILength *plenY, CUILength *plenZ, int nDistanceFromFocus, float flWidth, float flHeight )
{
	bool bFocus;
	if ( m_ptrPanelFocusOffset.Get() != NULL )
	{
		bFocus = m_ptrPanelFocusOffset->BHasKeyFocus() || m_ptrPanelFocusOffset->BHasDescendantKeyFocus();
	}
	else
	{
		bFocus = (BHasKeyFocus() || BHasDescendantKeyFocus());
	}

	*plenX = GetPanelOffset( nDistanceFromFocus, bFocus, m_childOffsets.x, m_childOffsetsFocus.x );
	*plenY = GetPanelOffset( nDistanceFromFocus, bFocus, m_childOffsets.y, m_childOffsetsFocus.y );
	*plenZ = GetPanelOffset( nDistanceFromFocus, bFocus, m_childOffsets.z, m_childOffsetsFocus.z );

	plenX->ConvertToLength( flWidth );
	plenY->ConvertToLength( flHeight );

	if ( !plenZ->IsLength() )
	{
		AssertMsg( false, "Don't know how to handle Z" );
		plenZ->Set( 0.0f, CUILength::k_EUILengthLength );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves specific offset from vector
//-----------------------------------------------------------------------------
CUILength CCarousel::GetPanelOffset( int nDistanceFromFocus, bool bUseFocus, const CUtlVector< CUILength > &vecOffsets, const CUtlVector< CUILength > &vecFocusOffsets )
{
	const CUtlVector< CUILength > *pvec = &vecOffsets;

	// only use focus offsets when provided
	if ( bUseFocus && vecFocusOffsets.Count() > 0 )
		pvec = &vecFocusOffsets;

	CUILength len( 0, CUILength::k_EUILengthLength );
	if ( nDistanceFromFocus > 0 && pvec->Count() > 0 )
		len = (nDistanceFromFocus > pvec->Count()) ? pvec->Element( pvec->Count() - 1 ) : pvec->Element( nDistanceFromFocus - 1 );

	return len;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event for window cursor shown
//-----------------------------------------------------------------------------
bool CCarousel::EventWindowCursorShown( IUIWindow *pWindow )
{
	if ( pWindow == GetParentWindow() )
		AddClass( k_pchCursorVisible );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event for window cursor hidden
//-----------------------------------------------------------------------------
bool CCarousel::EventWindowCursorHidden( IUIWindow *pWindow )
{
	if ( pWindow == GetParentWindow() )
		RemoveClass( k_pchCursorVisible );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Registers for window cursor changes and sets appropriate style
//-----------------------------------------------------------------------------
void CCarousel::RegisterForCursorChanges()
{
	if ( m_bRegisteredForCursorChanges )
		return;

	m_bRegisteredForCursorChanges = true;
	RegisterForUnhandledEvent( WindowCursorShown(), this, &CCarousel::EventWindowCursorShown );
	RegisterForUnhandledEvent( WindowCursorHidden(), this, &CCarousel::EventWindowCursorHidden );

	if ( GetParentWindow()->BCursorVisible() )
		AddClass( k_pchCursorVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Unregisters for window cursor changes and sets appropriate style
//-----------------------------------------------------------------------------
void CCarousel::UnregisterForCursorChanges()
{
	if ( !m_bRegisteredForCursorChanges )
		return;

	m_bRegisteredForCursorChanges = false;
	UnregisterForUnhandledEvent( WindowCursorShown(), this, &CCarousel::EventWindowCursorShown );
	UnregisterForUnhandledEvent( WindowCursorHidden(), this, &CCarousel::EventWindowCursorHidden );

	RemoveClass( k_pchCursorVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Event handler to autoscroll after a delay
//-----------------------------------------------------------------------------
bool CCarousel::EventAutoScroll( uint8 unAutoScrollID )
{
	// If the IDs don't match, we should ignore this scroll
	if ( m_unAutoScrollID != unAutoScrollID )
		return true;

	m_bAutoScrollScheduled = false;

	// Might have disabled autoscroll before receiving this event
	if ( !IsAutoScrollEnabled() )
		return true;

	// Don't automatically scroll while we're hovered over
	if ( !BHasHoverStyle() )
	{
		int nCurrentIndex = GetFocusIndex();
		if ( nCurrentIndex < 0 )
		{
			nCurrentIndex = 0;
		}

		int nChildCount = GetChildCount();
		for ( int i = 1; i < nChildCount; ++i )
		{
			int nNextIndex = ( nCurrentIndex + i ) % nChildCount;
			CPanel2D *pChild = GetChild( nNextIndex );
			if ( pChild && pChild->IsEnabled() )
			{
				SetSelectedChild( pChild );
				break;
			}
		}
	}

	CheckScheduleAutoScroll();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set the delay for the next autoscroll
//-----------------------------------------------------------------------------
void CCarousel::SetAutoScrollDelay( float flAutoScrollDelay, float flAutoScrollRandomDelay /* = 0.0f */ )
{
	if ( m_flAutoScrollDelay == flAutoScrollDelay && m_flAutoScrollRandomDelay == flAutoScrollRandomDelay )
		return;

	m_flAutoScrollDelay = flAutoScrollDelay;
	m_flAutoScrollRandomDelay = flAutoScrollRandomDelay;

	CheckScheduleAutoScroll();
}


//-----------------------------------------------------------------------------
// Purpose: Parse a string containing the auto scroll delay and optionally the random delay and set it
//-----------------------------------------------------------------------------
bool CCarousel::SetAutoScrollDelay( const char *pchValue )
{
	CUtlVector< double > vecScrollDelay;
	if ( !CSSHelpers::BParseCommaSepList( &vecScrollDelay, CSSHelpers::BParseTime, pchValue ) )
		return false;

	if ( vecScrollDelay.IsEmpty() || vecScrollDelay.Count() > 2 )
		return false;

	float flDelay = ( float )vecScrollDelay[ 0 ];
	float flRandomDelay = vecScrollDelay.Count() > 1 ? ( float )vecScrollDelay[ 1 ] : 0.0f;

	SetAutoScrollDelay( flDelay, flRandomDelay );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if we should be autoscrolling
//-----------------------------------------------------------------------------
bool CCarousel::IsAutoScrollEnabled() const
{
	return m_bAutoScrollEnabled && ( m_flAutoScrollDelay > 0.0f || m_flAutoScrollRandomDelay > 0.0f ) && GetChildCount() > 1;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should be autoscrolling
//-----------------------------------------------------------------------------
void CCarousel::SetAutoScrollEnabled( bool bEnabled )
{
	if( m_bAutoScrollEnabled != bEnabled )
	{
		m_bAutoScrollEnabled = bEnabled;
		CheckScheduleAutoScroll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if we should schedule a new autoscroll event
//-----------------------------------------------------------------------------
void CCarousel::CheckScheduleAutoScroll()
{
	if ( m_bAutoScrollScheduled )
		return;

	if ( !IsAutoScrollEnabled() )
		return;

	m_bAutoScrollScheduled = true;

	float flDelay = 0.0f;
	flDelay += m_flAutoScrollDelay > 0.0f ? m_flAutoScrollDelay : 0.0f;
	flDelay += m_flAutoScrollRandomDelay > 0.0f ? WeakRandomFloat( 0.0f, m_flAutoScrollRandomDelay ) : 0.0f;
	DispatchEventAsync( flDelay, CarouselAutoScroll(), this, ++m_unAutoScrollID );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CCarousel::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	BaseClass::ValidateClientPanel( validator, pchName );

	VALIDATE_SCOPE();
	ValidateObj( m_childOffsets.x );
	ValidateObj( m_childOffsets.y );
	ValidateObj( m_childOffsets.z );
	ValidateObj( m_childOffsetsFocus.x );
	ValidateObj( m_childOffsetsFocus.y );
	ValidateObj( m_childOffsetsFocus.z );
	
	if ( m_pDirtyChildStyles )
	{
		validator.ClaimMemory( m_pDirtyChildStyles );
		ValidateObj( m_pDirtyChildStyles->m_vecPanels );		
	}
}
#endif


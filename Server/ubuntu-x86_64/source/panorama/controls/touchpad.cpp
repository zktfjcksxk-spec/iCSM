//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Control to present buttons using a steam controller's touchpad
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/touchpad.h"


using namespace panorama;

DEFINE_PANORAMA_EVENT( TouchKeyStyleChanged );
DEFINE_PANORAMA_EVENT( TouchKeyClicked );

//-----------------------------------------------------------------------------
//	Purpose:
//-----------------------------------------------------------------------------
bool CTouchPad::Initialize( CPanel2D *pParent,
												 const char *pointerID,
												 const char *padID,
												 const char *pszTouchPadActiveClass,
												 IUIEngine::EHapticFeedbackPosition eHapticsPosition,
												 bool bFingerOnPad )
{
	V_memset( &m_renderPointerState, 0, sizeof( m_renderPointerState ) );
	
	m_pszTouchPadActiveClass = pszTouchPadActiveClass;
	m_pPointerPanel = pParent->FindChildInLayoutFile( pointerID );
	m_pPadPanel = pParent->FindChildInLayoutFile( padID );
	m_pHoverKey = NULL;
	m_pLastHoverKey = nullptr;
	
	if ( !m_pPointerPanel || !m_pPadPanel )
		return false;
	
	m_pPadPanel->UIPanel()->FindChildrenWithClassTraverse( "TouchKey", m_vecTouchKeys );
	
	// Put keys in back to front order, to match our hit detection with visual occlusion
	m_vecTouchKeys.Reverse();
	
	m_pParent = pParent;

	VMatrix matParentTransform = m_pParent->AccessStyle()->GetTransform3DMatrix();
	
	Assert( matParentTransform.m[0][0] == matParentTransform.m[1][1] );
	
	m_flScaleFactor = matParentTransform.m[0][0];
	
	m_eHapticsPosition = eHapticsPosition;
	
	if ( bFingerOnPad )
	{
		OnTouch();
	}
	else
	{
		OnRelease();
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTouchPad::OnRelease( void )
{
	m_pPointerPanel->RemoveClass( m_pszTouchPadActiveClass );
	m_bFingerOnPad = false;
	m_pHoverKey = nullptr;
	
	if ( m_pLastHoverKey.Get() )
	{
		DispatchEvent( TouchKeyStyleChanged(), m_pParent, m_pLastHoverKey, "TouchKeyHover", false );
		m_pLastHoverKey = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static bool BIsSelfAndParentsVisible( IUIPanel *pPanel )
{
	for ( IUIPanel *pCurrent = pPanel; pCurrent; pCurrent = pCurrent->GetParent() )
	{
		if ( !pCurrent->BIsVisible() )
			return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Non-linear remapping of touchapd input to position on the keyboard
// WARNING- this code is duplicated in //vr/steamvr/main/src/vrcompositor/distort.cpp
// Any changes to this algorithm MUST be copied there.
//-----------------------------------------------------------------------------
static float RemapPhysicalPadPositionToKeyboardPosition( float f, bool bApplyOffset )
{
	Assert( f >= -1.0f );
	Assert( f <= 1.0f );
	
	static float s_fCenterOffset = -0.1f;
	static float s_fTouchPadScaleFactor = 1.25f;
	static float s_fHalfwayTransitionPoint = 0.60f;
	
	// f is -1.0 to 1.0. We treat it as 0.0 to 1.0, do the math to translate it to a different value in the
	// same space, and then put the sign bit back.
	bool bWasNegative = (f < 0.0f);
	f = fabsf( f );
	
	// Scale up our input value pretty dramatically. In general users don't go anywhere near the edge of the
	// trackpad on purpose, and don't like it when they do. This has the effect of making it so that we throw
	// out roughly the outer third of the trackpad, clamping it all to "the edge".
	f *= s_fTouchPadScaleFactor;

	// We intentionally don't clamp to [0.0, 1.0] here. For regions near the center of the touchpad the scaling
	// math below will still work fine. For regions near the edge of the touchpad we'll blow past our 1.0 range
	// but we don't really care because we'll clamp at the end.
	//
	// We don't clamp here because if we *do* blow past the edge of our range and we're applying an offset, the
	// difference between clamp-then-offset and offset-then-clamp means that we would lose access to the entire
	// bottom say 10% of the visual range on the keyboard and that feels weird for some people.
		
	// If we're in the (scaled) middle two thirds of the touchpad, we bump up the effect each unit of movement
	// has (or: we increase sensitivity). The center of the keyboard areas has a pretty dense set of keys, but
	// users are comfortable making fine movements there so we lend them a hand.
	if ( f < s_fHalfwayTransitionPoint )
	{
		// Remap [0, 2/3] to [0, 1/2].
		f *= (0.5f / s_fHalfwayTransitionPoint);
	}
	// If we're on the (scaled!) outer edge of the touchpad, we assume that users are going to be making bigger
	// motions. We remap the last outer third to the full 50% of controller range.
	else
	{
		// Remap [2/3, 1] to [1/2, 1].
		f = 0.5f + ((f - s_fHalfwayTransitionPoint) * (0.5f / (1.0f - s_fHalfwayTransitionPoint)));
	}
	
	// Put back the sign bit.
	f *= (bWasNegative ? -1.0f : 1.0f);
	
	// If we want to move the center of the pad to not be the center of the keyboard region we're in, we can
	// optionally enforce an percentage offset.
	if ( bApplyOffset )
	{
		f += s_fCenterOffset;
	}

	// Bring our final range back into line so that calling code always has a valid range to work with.
	f = clamp( f, -1.0f, 1.0f );
	
	return f;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTouchPad::UpdatePointerState( bool bSteamPadHardwarePointersEnabled )
{
	// It's possible that we get called initially when we're completely invisible during transition in. This means
	// that when we try to get our layout width, position, etc. we're going to get back garbage. Because we're completely
	// invisible, we don't care what data we put in the fields because we're not going to render the cursor anyway
	// (bVisible false).
	m_renderPointerState.bVisible = bSteamPadHardwarePointersEnabled && BIsSelfAndParentsVisible( m_pPadPanel->UIPanel() );
	
	if ( m_renderPointerState.bVisible )
	{
		m_pParent->AccessStyle()->GetOpacity( m_renderPointerState.flOpacity );
		
		m_renderPointerState.flRadius = m_pPadPanel->GetActualRenderWidth() / 2.0f;
		
		m_pPadPanel->GetPositionWithinWindow( &m_renderPointerState.vecCenter.x,
											  &m_renderPointerState.vecCenter.y );
		
		m_renderPointerState.flRadius *= m_flScaleFactor;
		
		m_renderPointerState.vecCenter.x += m_renderPointerState.flRadius;
		m_renderPointerState.vecCenter.y += m_renderPointerState.flRadius;
		
		m_renderPointerState.iControllerID = UIEngine()->UIInputEngine()->GetLastSteamControllerActiveIndex();
		
		m_renderPointerState.funcPreRenderCalculatePadOffset = &RemapPhysicalPadPositionToKeyboardPosition;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTouchPad::OnButtonDown( void )
{
	if ( !m_bFingerOnPad )
		return;
	
	if ( m_pLastHoverKey.Get() )
	{
		float fTouchKeyClickedActiveTime = V_atof( m_pParent->GetLayoutFileDefine( "touchkeyclickedactivetime" ) );
		DispatchEvent( TouchKeyClicked(), m_pParent, m_pLastHoverKey.Get(), this );
		
		if ( m_pLastHoverKey.Get() )
		{
			DispatchEvent( TouchKeyStyleChanged(), m_pParent, m_pLastHoverKey, "TouchKeyClicked", true );
			DispatchEventAsync( fTouchKeyClickedActiveTime, TouchKeyStyleChanged(), m_pParent, m_pLastHoverKey, "TouchKeyClicked", false );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTouchPad::OnTouch( void )
{
	m_pPointerPanel->AddClass( m_pszTouchPadActiveClass );
	m_bFingerOnPad = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTouchPad::OverlapsTouchKey( float pointerX, float pointerY, float fParentPadX, float fParentPadY, IUIPanel *pTouchKey, EKeyOverlapTestBehavior eOverlapTest )
{
	CPanel2D *pTouchKeyPanel = ToPanel2D( pTouchKey );
	if ( !BIsSelfAndParentsVisible( pTouchKeyPanel->UIPanel() ) )
		return false;
	
	// Could be faster by just iterating to the parent pad, could create a panel method
	// that does the same thing but has a parent panel target instead of going all the way to the window
	float flPosX, flPosY;
	pTouchKeyPanel->GetPositionWithinWindow( &flPosX, &flPosY );
	
	flPosX = flPosX - fParentPadX;
	flPosY = flPosY - fParentPadY;
	
	// GetPositionWithinWindow now evaluates scale transforms, so to determine
	// our untransformed offset we need to undo that now. This will be incorrect
	// if anyone but our immediate parent has a scale transform
	flPosX /= m_flScaleFactor;
	flPosY /= m_flScaleFactor;
	
	// These attributes bypass a dimension check, allowing us to create "endless" keys for sides of the pads
	// if we want. We always extend the bottom area (spacebar and done button) and the top area (suggestions, if
	// visible) to infinity.
	const bool bLeftQuadrant = (eOverlapTest == kOverlapTest_ExtendLeftRightQuadrantsToInfinity) && pTouchKeyPanel->BHasClass("LeftQuadrant");
	const bool bRightQuadrant = (eOverlapTest == kOverlapTest_ExtendLeftRightQuadrantsToInfinity) && pTouchKeyPanel->BHasClass("RightQuadrant");
	const bool bTopQuadrant = pTouchKeyPanel->BHasClass("TopQuadrant");
	const bool bBottomQuadrant = pTouchKeyPanel->BHasClass("BottomQuadrant");
	
	// Original comment:
	//		It makes some users uncomfortable to have the cursor right between two keys and see it flickering
	//		back and forth, especially when they are in the process of submitting a character. Adding a deadzone
	//		(requiring that the cursor be at least N pixels inside the key) doesn't seem to actually affect
	//		accuracy for anyone, but does make some folks more comfortable.
	//
	// More event:
	//		We now no longer update the hovered key when the cursor is over a deadzone. If the user types a
	//		character when in this deadzone we'll type the last character they hovered over fully. This helps
	//		a ton with users moving the cursor position as a part of their natural thumb motion when pushing
	//		the pad or the trigger. If this number gets too high, it gets perceivable as lag.
	static float s_fDefaultDeadZone = 6.0f;
	
	const float fDeadZone = (eOverlapTest == kOverlapTest_OverlapActualPositionIgnoringDeadzone ? 0.0f : s_fDefaultDeadZone);
	
	if (( bLeftQuadrant || pointerX > (flPosX + fDeadZone) ) &&
		( bRightQuadrant || pointerX < (flPosX - fDeadZone + pTouchKey->GetActualRenderWidth()) ) &&
		( bTopQuadrant || pointerY > (flPosY + fDeadZone) ) &&
		( bBottomQuadrant || pointerY < (flPosY - fDeadZone + pTouchKey->GetActualRenderHeight()) ) )
	{
		// If we hit a key directly and we're only looking for neighbors (looking for typos), ignore.
		return eOverlapTest != kOverlapTest_OnlyTestNeighbors;
	}
	
	if ( eOverlapTest == kOverlapTest_OnlyTestNeighbors )
	{
		static const float k_flTouchKeyNeighborAddedRadius = 18.0f;
		
		// looking for typos, enlarge search area
		flPosX -= k_flTouchKeyNeighborAddedRadius;
		flPosY -= k_flTouchKeyNeighborAddedRadius;
		
		float enlargedWidth = pTouchKey->GetActualRenderWidth() + k_flTouchKeyNeighborAddedRadius * 2;
		float enlargedHeight = pTouchKey->GetActualRenderHeight() + k_flTouchKeyNeighborAddedRadius * 2;
		
		if (( pointerX > flPosX ) &&
			( pointerX < flPosX + enlargedWidth ) &&
			( pointerY > flPosY ) &&
			( pointerY < flPosY + enlargedHeight ) )
		{
			return true;
		}
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTouchPad::OnMove( float touchX, float touchY )
{
	if ( !m_bFingerOnPad )
		return false;
	
	float flPadRadius = m_pPadPanel->GetActualRenderWidth() / 2.0f;
	
	float flX = RemapPhysicalPadPositionToKeyboardPosition( touchX / 1000.0f, false ) * flPadRadius;
	float flY = RemapPhysicalPadPositionToKeyboardPosition( -touchY / 1000.0f, true ) * flPadRadius;
	
	// update "software" pointer position
	CUtlVector<CTransform3D *> vecTransforms;
	vecTransforms.AddToTail( new CTransformTranslate3D( flX * m_pParent->GetActualUIScaleX(), flY * m_pParent->GetActualUIScaleY(), 0.0f ) );
	m_pPointerPanel->SetTransform3D( vecTransforms );
	
	flX += flPadRadius;
	flY += flPadRadius;
	
	float flParentX, flParentY;
	m_pPadPanel->GetPositionWithinWindow( &flParentX, &flParentY );
	
	for ( auto pTouchKey : m_vecTouchKeys )
	{
		if ( !OverlapsTouchKey( flX, flY, flParentX, flParentY, pTouchKey, kOverlapTest_ExtendLeftRightQuadrantsToInfinity ) )
			continue;
		
		CPanel2D *pHitKey = ToPanel2D( pTouchKey );
		
		// Hovered state changes?
		if ( m_pHoverKey.Get() != pHitKey )
		{
			// It's important that we do the unset of the style before the set because it's possible that we transition from
			// "hovering over Q" to "not hovering anything" to "hovering over Q". If we do the order backwards we'll wind up
			// with Q being hovered internally but the styles not representing that.
			
			// ...from hovered to not?
			if ( m_pLastHoverKey.Get() )
			{
				DispatchEvent( TouchKeyStyleChanged(), m_pParent, m_pLastHoverKey, "TouchKeyHover", false );
			}
			
			// ...from unhovered to hovered?
			DispatchEvent( TouchKeyStyleChanged(), m_pParent, pHitKey, "TouchKeyHover", true );

			UIEngine()->PulseActiveControllerHaptic( m_eHapticsPosition, IUIEngine::k_EHapticFeedbackStrength_Low );
			
			m_pHoverKey = pHitKey;				// we're now hovering over this key
			m_pLastHoverKey = m_pHoverKey;		// ditto
			m_hoverX = flX;						// with this position (used for neighbor testing)
			m_hoverY = flY;
		}
		
		// We found someone. Regardless of whether our state changed, we're done now.
		return true;
	}
	
	// Leave the hovered style on our m_pLastHoverKey, even though we aren't technically hovering over it
	// anymore. This is the key that will be input if the user pushes the button.
	m_pHoverKey = nullptr;
	
	return true;
}



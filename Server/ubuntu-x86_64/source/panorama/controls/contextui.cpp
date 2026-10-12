//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "contextui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

/*static*/ void CContextUI::SetupTargetForPanel( SLayoutTarget &layoutTarget, CPanel2D *pPanel, IUIWindow *pContainerWindow )
{
	float flWindowWidth = pContainerWindow ? pContainerWindow->GetSurfaceWidth() : 0.0f;
	float flWindowHeight = pContainerWindow ? pContainerWindow->GetSurfaceHeight() : 0.0f;

	if ( pPanel )
	{
		if ( !pPanel->GetContextUIBounds( &layoutTarget.flTargetX, &layoutTarget.flTargetY, &layoutTarget.flTargetWidth, &layoutTarget.flTargetHeight ) )
		{
			float flRight, flBottom;
			pPanel->GetBoundsWithinAncestor( nullptr, &layoutTarget.flTargetX, &layoutTarget.flTargetY, &flRight, &flBottom );
			layoutTarget.flTargetWidth = flRight - layoutTarget.flTargetX;
			layoutTarget.flTargetHeight = flBottom - layoutTarget.flTargetY;
		}

		layoutTarget.bTargetVisible = true;
	}
	else if ( pContainerWindow && pContainerWindow->BCursorVisible() )
	{
		// Create a box around the current mouse position
		float x, y;
		pContainerWindow->UIWindowInput()->GetSurfaceMousePosition( x, y );

		layoutTarget.flTargetX = x - k_nMouseContextUIVerticalOffset;
		layoutTarget.flTargetY = y - k_nMouseContextUIHorizontalOffset;
		layoutTarget.flTargetWidth = 2 * k_nMouseContextUIVerticalOffset;
		layoutTarget.flTargetHeight = 2 * k_nMouseContextUIHorizontalOffset;

		layoutTarget.bTargetVisible = true;
	}
	else
	{
		// Must be using the game controller.  Just aim a little above the middle of the window
		layoutTarget.flTargetX = flWindowWidth * 0.5f;
		layoutTarget.flTargetY = flWindowHeight * 0.3f;
		layoutTarget.flTargetWidth = 0.0f;
		layoutTarget.flTargetHeight = 0.0f;

		layoutTarget.bTargetVisible = false;
	}
}


/*static*/ EContextUIPosition CContextUI::LayoutContextUI( CPanel2D *pContainer, const SLayoutTarget &layoutTarget, const SLayoutPosition &layoutPosition )
{
	// calculate available space
	IUIWindow *pWindow = pContainer->GetParentWindow();

	float flWindowWidth, flWindowHeight;
	flWindowWidth = pWindow->GetSurfaceWidth();
	flWindowHeight = pWindow->GetSurfaceHeight();

	// Load the components of the tooltip.  These might be NULL
	CPanel2D *pLeftArrowPanel = pContainer->FindChildTraverse( "LeftArrow" );
	CPanel2D *pRightArrowPanel = pContainer->FindChildTraverse( "RightArrow" );
	CPanel2D *pTopArrowPanel = pContainer->FindChildTraverse( "TopArrow" );
	CPanel2D *pBottomArrowPanel = pContainer->FindChildTraverse( "BottomArrow" );
	
	// Panel containing all of the contents of the context ui (not the arrows around it)
	CPanel2D *pContentsPanel = pContainer->FindChildTraverse( "Contents" );

	// Panel inside of Contents that should be used for measurments/sizing of where to position the
	// context UI. Usually NULL, but if you want to visually have context ui with things hanging off
	// the side (e.g. DOTA's Profile Card), then you need to use this to get the arrows positioned correctly
	CPanel2D *pContentsBodyPanel = pContainer->FindChildTraverse( "ContentsBody" );

	for ( const EContextUIPosition &ePosition : layoutPosition.ePositions )
	{
		static const CPanoramaSymbol k_symLeftArrowVisible( "LeftArrowVisible" );
		static const CPanoramaSymbol k_symRightArrowVisible( "RightArrowVisible" );
		static const CPanoramaSymbol k_symTopArrowVisible( "TopArrowVisible" );
		static const CPanoramaSymbol k_symBottomArrowVisible( "BottomArrowVisible" );

		// Set the class associated with which arrow should be visible
		pContainer->SetHasClass( k_symLeftArrowVisible, layoutTarget.bTargetVisible && pLeftArrowPanel && ePosition == k_EContextUIPositionRight );
		pContainer->SetHasClass( k_symRightArrowVisible, layoutTarget.bTargetVisible && pRightArrowPanel && ePosition == k_EContextUIPositionLeft );
		pContainer->SetHasClass( k_symTopArrowVisible, layoutTarget.bTargetVisible && pTopArrowPanel && ePosition == k_EContextUIPositionBottom );
		pContainer->SetHasClass( k_symBottomArrowVisible, layoutTarget.bTargetVisible && pBottomArrowPanel && ePosition == k_EContextUIPositionTop );

		// Now measure the container size
		float flContainerWidth, flContainerHeight;
		pContainer->DesiredLayoutSizeTraverse( &flContainerWidth, &flContainerHeight, flWindowWidth, flWindowHeight, true );

		// Get the margins of the container
		float flContainerMarginLeft, flContainerMarginTop, flContainerMarginRight, flContainerMarginBottom;
		pContainer->AccessStyle()->GetMargin( flContainerWidth, flContainerHeight, flContainerMarginLeft, flContainerMarginTop, flContainerMarginRight, flContainerMarginBottom );
		float flContainerTotalWidth = flContainerWidth + flContainerMarginLeft + flContainerMarginRight;
		float flContainerTotalHeight = flContainerHeight + flContainerMarginTop + flContainerMarginBottom;

		// Get the margins of the body of the container
		float flBodyWidth, flBodyHeight;
		float flBodyMarginLeft, flBodyMarginTop, flBodyMarginRight, flBodyMarginBottom;
		if ( pContentsPanel )
		{
			// Force a layout traverse to position all the contents. Use this so that we can query actual sizes with just GetActualLayoutWidth/Height
			pContentsPanel->LayoutTraverse( 0.0f, 0.0f, pContentsPanel->GetDesiredLayoutWidth(), pContentsPanel->GetDesiredLayoutHeight() );

			CPanel2D *pBodyPanel = pContentsBodyPanel ? pContentsBodyPanel : pContentsPanel;
			flBodyWidth = pBodyPanel->GetActualLayoutWidth();
			flBodyHeight = pBodyPanel->GetActualLayoutHeight();
			pBodyPanel->AccessStyle()->GetMargin( flBodyWidth, flBodyHeight, flBodyMarginLeft, flBodyMarginTop, flBodyMarginRight, flBodyMarginBottom );
		}
		else
		{
			// If we don't have a contents panel, the the full panel is the contents
			flBodyWidth = flContainerWidth;
			flBodyHeight = flContainerHeight;
			flBodyMarginLeft = flContainerMarginLeft;
			flBodyMarginTop = flContainerMarginTop;
			flBodyMarginRight = flContainerMarginRight;
			flBodyMarginBottom = flContainerMarginBottom;
		}

		// Now figure out where to position the arrow of the tooltip if necessary
		CPanel2D *pArrowPanel = NULL;
		switch ( ePosition )
		{
			case k_EContextUIPositionRight:	 pArrowPanel = pLeftArrowPanel;   break;
			case k_EContextUIPositionLeft:   pArrowPanel = pRightArrowPanel;  break;
			case k_EContextUIPositionTop:    pArrowPanel = pBottomArrowPanel; break;
			case k_EContextUIPositionBottom: pArrowPanel = pTopArrowPanel;    break;
		}

		float flArrowX = 0.0f;
		float flArrowY = 0.0f;
		float flArrowTotalWidth = 0.0f;
		float flArrowTotalHeight = 0.0f;
		if ( pArrowPanel )
		{
			// Measure the arrow's size and margins
			float flArrowWidth, flArrowHeight;
			float flArrowMarginLeft, flArrowMarginTop, flArrowMarginRight, flArrowMarginBottom;
			pArrowPanel->DesiredLayoutSizeTraverse( &flArrowWidth, &flArrowHeight, flWindowWidth, flWindowHeight, false );
			pArrowPanel->AccessStyle()->GetMargin( flArrowWidth, flArrowHeight, flArrowMarginLeft, flArrowMarginTop, flArrowMarginRight, flArrowMarginBottom );

			// Get the arrow's origin point.  We use that as the point that we align relative to the target
			const char *pszArrowTarget = pArrowPanel->GetAttribute( "arroworigin", "" );
			CUILength lenArrowOriginX, lenArrowOriginY;
			if ( !CSSHelpers::BParseIntoTwoUILengths( &lenArrowOriginX, &lenArrowOriginY, pszArrowTarget, &pszArrowTarget ) )
			{
				lenArrowOriginX.SetPercent( 50.0f );
				lenArrowOriginY.SetPercent( 50.0f );
			}

			if ( ePosition == k_EContextUIPositionLeft || ePosition == k_EContextUIPositionRight )
			{
				// Point the arrow at the right spot on the target
				float flArrowOriginY = lenArrowOriginY.GetValueAsLength( flArrowHeight ) + flArrowMarginTop;
				float flArrowTargetY = layoutTarget.flTargetY + layoutPosition.lenVerticalArrowPosition.GetValueAsLength( layoutTarget.flTargetHeight );
				flArrowY = flArrowTargetY - flArrowOriginY;
				flArrowTotalHeight = flArrowHeight + flArrowMarginTop + flArrowMarginBottom;
			}
			else
			{
				float flArrowOriginX = lenArrowOriginX.GetValueAsLength( flArrowWidth ) + flArrowMarginLeft;
				float flArrowTargetX = layoutTarget.flTargetX + layoutPosition.lenHorizontalArrowPosition.GetValueAsLength( layoutTarget.flTargetWidth );
				flArrowX = flArrowTargetX - flArrowOriginX;
				flArrowTotalWidth = flArrowWidth + flArrowMarginLeft + flArrowMarginRight;
			}
		}

		// Now figure out where to position the body of the tooltip
		float flBodyMinX, flBodyMaxX;
		float flBodyMinY, flBodyMaxY;
		switch ( ePosition )
		{
			case k_EContextUIPositionRight:
				flBodyMinX = flBodyMaxX = layoutTarget.flTargetX + layoutTarget.flTargetWidth;
				flBodyMinY = layoutTarget.flTargetY - flBodyMarginTop;
				flBodyMaxY = layoutTarget.flTargetY + layoutTarget.flTargetHeight - ( flBodyHeight + flBodyMarginTop );
				break;

			case k_EContextUIPositionLeft:
				flBodyMinX = flBodyMaxX = layoutTarget.flTargetX - flContainerTotalWidth;
				flBodyMinY = layoutTarget.flTargetY - flBodyMarginTop;
				flBodyMaxY = layoutTarget.flTargetY + layoutTarget.flTargetHeight - ( flBodyHeight + flBodyMarginTop );
				break;

			case k_EContextUIPositionTop:
				flBodyMinX = layoutTarget.flTargetX - flBodyMarginLeft;
				flBodyMaxX = layoutTarget.flTargetX + layoutTarget.flTargetWidth - ( flBodyWidth + flBodyMarginLeft );
				flBodyMinY = flBodyMaxY = layoutTarget.flTargetY - flContainerTotalHeight;
				break;

			case k_EContextUIPositionBottom:
				flBodyMinX = layoutTarget.flTargetX - flBodyMarginLeft;
				flBodyMaxX = layoutTarget.flTargetX + layoutTarget.flTargetWidth - ( flBodyWidth + flBodyMarginLeft );
				flBodyMinY = flBodyMaxY = layoutTarget.flTargetY + layoutTarget.flTargetHeight;
				break;

			default:
				Assert( false );
				continue;
		}

		// Make sure we leave enough room for the full size of the arrow including its margins
		if ( flArrowTotalWidth > 0.0f )
		{
			flBodyMinX = Min( flBodyMinX, flArrowX );
			flBodyMaxX = Max( flBodyMaxX, flArrowX + flArrowTotalWidth - ( flBodyWidth + flBodyMarginLeft ) );
		}
		if ( flArrowTotalHeight > 0.0f )
		{
			flBodyMinY = Min( flBodyMinY, flArrowY );
			flBodyMaxY = Max( flBodyMaxY, flArrowY + flArrowTotalHeight - ( flBodyHeight + flBodyMarginTop ) );
		}

		// Calculate how much room is available
		float flHorizontalAvailable = -1.0f;
		float flVerticalAvailable = -1.0f;
		switch ( ePosition )
		{
			case k_EContextUIPositionRight:		flHorizontalAvailable = flWindowWidth - flBodyMinX;		break;
			case k_EContextUIPositionLeft:		flHorizontalAvailable = layoutTarget.flTargetX;			break;
			case k_EContextUIPositionTop:		flVerticalAvailable = layoutTarget.flTargetY;			break;
			case k_EContextUIPositionBottom:	flVerticalAvailable = flWindowHeight - flBodyMinY;		break;
		}

		// Does the container fit in the available area? If not, try a different side
		if ( flHorizontalAvailable > 0.0f && flContainerTotalWidth > flHorizontalAvailable )
			continue;
		if ( flVerticalAvailable > 0.0f && flContainerTotalHeight > flVerticalAvailable )
			continue;

		// Make a copy of the body position, because we might modify it below
		CUILength lenHorizontalBodyPosition = layoutPosition.lenHorizontalBodyPosition;
		CUILength lenVerticalBodyPosition = layoutPosition.lenVerticalBodyPosition;

		// If the tooltip size is smaller than the target size, then we need to reverse
		// some values to keep the alignment
		if ( flBodyMinX > flBodyMaxX )
		{
			std::swap( flBodyMinX, flBodyMaxX );
			if ( lenHorizontalBodyPosition.IsPercent() )
			{
				lenHorizontalBodyPosition.SetPercent( 100.0f - lenHorizontalBodyPosition.GetValue() );
			}
		}
		if ( flBodyMinY > flBodyMaxY )
		{
			std::swap( flBodyMinY, flBodyMaxY );
			if ( lenVerticalBodyPosition.IsPercent() )
			{
				lenVerticalBodyPosition.SetPercent( 100.0f - lenVerticalBodyPosition.GetValue() );
			}
		}

		// Use the body position to adjust where the tooltip body sits relative to the target
		float flBodyX = flBodyMinX + lenHorizontalBodyPosition.GetValueAsLength( flBodyMaxX - flBodyMinX );
		float flBodyY = flBodyMinY + lenVerticalBodyPosition.GetValueAsLength( flBodyMaxY - flBodyMinY );

		// Make sure it fits entirely within the window
		flBodyX = Clamp( flBodyX, 0.0f, flWindowWidth - flContainerTotalWidth );
		flBodyY = Clamp( flBodyY, 0.0f, flWindowHeight - flContainerTotalHeight );

		// Now position the full container, including the arrow if necessary
		float flContainerX, flContainerY;
		if ( pArrowPanel )
		{
			if ( ePosition == k_EContextUIPositionLeft || ePosition == k_EContextUIPositionRight )
			{
				// REI TEST
				flArrowY = Clamp( flArrowY, flBodyY + flBodyMarginTop, flBodyY + flBodyMarginTop + flBodyHeight - flArrowTotalHeight );

				// The arrow is contained in the container, so make its position relative to that
				flContainerY = Min( flBodyY, flArrowY );
				flArrowY -= flContainerY;
				flContainerX = flBodyX;
			}
			else
			{
				// REI TEST
				flArrowX = Clamp( flArrowX, flBodyX + flBodyMarginLeft, flBodyX + flBodyMarginLeft + flBodyWidth - flArrowTotalWidth );

				flContainerX = Min( flBodyX, flArrowX );
				flArrowX -= flContainerX;
				flContainerY = flBodyY;
			}

			// The body is contained within the tooltip, so adjust its coordinates
			flBodyX -= flContainerX;
			flBodyY -= flContainerY;

			SetContextUIPanelPosition( pContentsPanel, flBodyX, flBodyY );
			SetContextUIPanelPosition( pArrowPanel, flArrowX, flArrowY );
		}
		else
		{
			flContainerX = flBodyX;
			flContainerY = flBodyY;

			SetContextUIPanelPosition( pContentsPanel, 0.0f, 0.0f );
		}

		SetContextUIPanelPosition( pContainer, flContainerX, flContainerY );
		return ePosition;
	}

	// Can't fit this context ui anywhere? Just throw it in the top left corner. Tooltip or target is probably just way too big.
	CUILength lenZero( 0, CUILength::k_EUILengthLength );
	pContainer->SetPosition( lenZero, lenZero, lenZero );

	return k_EContextUIPositionUnset;
}


/*static*/ void CContextUI::SetContextUIPanelPosition( CPanel2D *pPanel, float x, float y )
{
	if ( !pPanel )
		return;

	// Always place things on pixel boundaries or else we get fuzzy text
	float xPosition = RoundFloatToInt( x / pPanel->GetActualUIScaleX() );
	float yPosition = RoundFloatToInt( y / pPanel->GetActualUIScaleY() );

	// Use a transform rather than SetPosition, because flow layout already uses SetPosition
	CUtlVector< CTransform3D* > vecTransforms;
	vecTransforms.AddToTail( new CTransformTranslate3D( xPosition, yPosition, 0.0f ) );
	pPanel->SetTransform3D( vecTransforms );
}

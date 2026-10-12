//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CONTEXTUI_H
#define CONTEXTUI_H

#ifdef _WIN32
#pragma once
#endif

namespace panorama 
{

const float k_nMouseContextUIVerticalOffset = 20.0f;
const float k_nMouseContextUIHorizontalOffset = 20.0f;

// Helper class for positioning panels contextually next to a target. Used for tooltips, context menus, and dropdowns
class CContextUI
{
public:

	// A target that you want to position context UI relative to. Usually filled in with SetupTargetForPanel
	struct SLayoutTarget
	{
		// Target's bounding rectangle
		float flTargetX;
		float flTargetY;
		float flTargetWidth;
		float flTargetHeight;

		// Whether the target is visible on the screen. Controls whether we show an arrow to it or not
		bool bTargetVisible;
	};

	// How to position this context UI relative to a target
	struct SLayoutPosition
	{
		// order to check sides
		EContextUIPosition ePositions[ 4 ];

		// body relative to target
		CUILength lenHorizontalBodyPosition;
		CUILength lenVerticalBodyPosition; 

		// arrow relative to target
		CUILength lenHorizontalArrowPosition;
		CUILength lenVerticalArrowPosition;
	};

	// Given a panel, fill out the values for the given layoutTarget so that it points at the panel. If pPanel is null,
	// will try to position either relative to the cursor, or near the center of the window
	static void SetupTargetForPanel( SLayoutTarget &layoutTarget, CPanel2D *pPanel, IUIWindow *pContainerWindow );

	// Position the pContainer panel so that it is relative to the layoutTarget
	// Returns the context position selected 
	static EContextUIPosition LayoutContextUI( CPanel2D *pContainer, const SLayoutTarget &layoutTarget, const SLayoutPosition &layoutPosition );

private:
	static void SetContextUIPanelPosition( CPanel2D *pPanel, float x, float y );
};


} // namespace panorama

#endif // CONTEXTUI_H
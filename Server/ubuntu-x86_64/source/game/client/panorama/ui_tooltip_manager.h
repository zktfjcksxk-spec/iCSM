//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

namespace panorama
{
	class CTooltip;
}

DECLARE_PANEL_EVENT1( UIShowTextTooltip, const char * );
DECLARE_PANEL_EVENT2( UIShowTextTooltipStyled, const char *, const char * );
DECLARE_PANEL_EVENT0( UIHideTextTooltip );

DECLARE_PANEL_EVENT2( UIShowTitleTextTooltip, const char *, const char * );
DECLARE_PANEL_EVENT3( UIShowTitleTextTooltipStyled, const char *, const char *, const char * );
DECLARE_PANEL_EVENT0( UIHideTitleTextTooltip );

DECLARE_PANEL_EVENT3( UIShowTitleImageTextTooltip, const char *, const char *, const char * );
DECLARE_PANEL_EVENT4( UIShowTitleImageTextTooltipStyled, const char *, const char *, const char *, const char * );
DECLARE_PANEL_EVENT0( UIHideTitleImageTextTooltip );

DECLARE_PANEL_EVENT2( UIShowCustomLayoutTooltip, const char *, const char * );
DECLARE_PANEL_EVENT3( UIShowCustomLayoutTooltipStyled, const char *, const char *, const char * );
DECLARE_PANEL_EVENT3( UIShowCustomLayoutParametersTooltip, const char *, const char *, const char * );
DECLARE_PANEL_EVENT4( UIShowCustomLayoutParametersTooltipStyled, const char *, const char *, const char *, const char * );
DECLARE_PANEL_EVENT1( UIHideCustomLayoutTooltip, const char * );

//-----------------------------------------------------------------------------
// Purpose: Container for styled tooltips
//-----------------------------------------------------------------------------
class CUI_TooltipManager: public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_TooltipManager, panorama::CPanel2D );

public:
	CUI_TooltipManager( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_TooltipManager();

	static CUI_TooltipManager *GetWindowTooltipManager( panorama::IUIWindow *pWindow );
	static CUI_TooltipManager *GetPanelTooltipManager( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr );

	// Methods for creating/showing/hiding tooltips generically. These can be used directly if you have a one-off
	// tooltip that you don't want to include in CDOTA_UI_TooltipManager itself.
	template < class T > T* GetTooltip( const char *pszID );		// Returns null if not found
	template < class T > T* EnsureTooltip( const char *pszID );		// Creates the tooltip if not found
	bool ShowTooltip( const char *pszID, const panorama::CPanelPtr< panorama::IUIPanel >& targetPanelPtr );
	bool ShowTooltipAtPos( const char *pszID, const panorama::CPanelPtr< panorama::IUIPanel >& targetPanelPtr, float flX, float flY );
	bool HideTooltip( const char *pszID );

	// A tooltip with just text
	bool ShowTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszText );
	bool ShowTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszText, const char *pszClass );
	bool HideTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr );

	// A tooltip with a title and text
	bool ShowTitleTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszText );
	bool ShowTitleTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszText, const char *pszClass );
	bool HideTitleTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr );

	// A tooltip with a title, image, and text
	bool ShowTitleImageTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszImagePath, const char *pszText );
	bool ShowTitleImageTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszImagePath, const char *pszText, const char *pszClass );
	bool HideTitleImageTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr );

	// A tooltip with a custom XML layout
	bool ShowCustomLayoutParametersTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters );
	bool ShowCustomLayoutParametersTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters, const char *pszClass );
	bool ShowCustomLayoutTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml );
	bool ShowCustomLayoutTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml, const char *pszClass );
	bool HideCustomLayoutTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID );

protected:
	bool ShouldHandleTooltipEvent( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr );
};


template < class T >
T* CUI_TooltipManager::GetTooltip( const char *pszID )
{
	static_assert( std::is_base_of< panorama::CTooltip, T >::value, "Trying to get a tooltip that's not derived from CTooltip" );
	return assert_cast<T*>( FindChild( pszID ) );
}

template < class T >
T* CUI_TooltipManager::EnsureTooltip( const char *pszID )
{
	static_assert( std::is_base_of< panorama::CTooltip, T >::value, "Trying to create a tooltip that's not derived from CTooltip" );

	AssertMsg( pszID != NULL && pszID[0] != '\0', "Tooltips need a unique ID so that they can be shown/hidden properly" );

	T* pTooltip = assert_cast<T*>( FindChild( pszID ) );
	if ( !pTooltip )
	{
		pTooltip = new T( this, pszID );
	}

	return pTooltip;
}
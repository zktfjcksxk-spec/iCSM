//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// UI Page
//
// A "Page" represents a single CPanel2D that takes over the main area of the dashboard.
// To create a new page, do the following:
//   1) Create a new class and inherit from CUI_Page
//   2) Create xml/css files and use REGISTER_PANEL2D_FACTORY as normal.
//   3) Use DECLARE_UI_PAGE_EVENT# and DEFINE_UI_PAGE_EVENT# macros to create
//      events that will automatically allocate your class and switch the
//      dashboard to show it. Note that you can have multiple events that point
//      to the same class if you have multiple ways of entering the page.
//   4) Implement SetupPage functions that correspond to the parameters of the
//      UI_PAGE_EVENT# macros.
//
//   If you have two different page events with the same parameters that you
//   want to call different setup functions, use the DECLARE_UI_PAGE_EVENT_SETUP#
//   versions of the macros.
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "ui_page_types.h"

class CUI_PageManager;

class CUI_Page : public panorama::CPanel2D
{
public:
	CUI_Page( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_Page() {}

	// Subclasses should implement versions of SetupPage as required by the DB_PAGE_EVENT macros
	// EPageSetupResult SetupPage( ... )

	// Override to get notifications of navigating to/from this page
	virtual void OnNavigatingAway( NavigationChangeReason_t reason ) {}
	virtual void OnNavigatedAway( NavigationChangeReason_t reason ) {}
	virtual void OnNavigatingTo( NavigationChangeReason_t reason ) {}
	virtual void OnNavigatedTo( NavigationChangeReason_t reason ) {}

	// Return which primary tab this page is considered part of
	virtual const char *GetPrimaryTab();

	// Override to do custom backward navigation
	virtual bool BOverrideNavigateBack() { return false; }
	virtual bool BCanNavigateBack() { Assert( BOverrideNavigateBack() ); return false; }
	virtual void NavigateBack() { Assert( BOverrideNavigateBack() ); }

	// Override to do custom forward navigation
	virtual bool BOverrideNavigateForward() { return false; }
	virtual bool BCanNavigateForward() { Assert( BOverrideNavigateForward() ); return false; }
	virtual void NavigateForward() { Assert( BOverrideNavigateForward() ); }

	// Should you suppress navigation events fired right now. Used in rare cases where
	// you want a complicated page transition to complete.
	virtual bool BSuppressNavigation() { return false; }

	// Is this page the currently active page
	bool IsPageActive() const;

	// Represents whether the page is active and the parent window is visible to know that this page
	// is visible to the user right now.
	bool IsUserViewingPage() const { return IsPageActive() && GetParentWindow()->BIsVisible(); }

	// The page navigate ID is a token you can use to call CUI_PageManager APIs asynchronously. If your
	// current page navigate ID is different than the system's, then that means that the user has
	// navigated to a new page and any of your async calls should no longer affect it.
	void SetPageNavigateID( PageNavigateID_t unPageNavigateID ) { m_unPageNavigateID = unPageNavigateID; }
	PageNavigateID_t GetPageNavigateID() const { return m_unPageNavigateID; }

protected:
	virtual bool BIsClientPanelEvent( panorama::CPanoramaSymbol symProperty ) OVERRIDE;

private:
	PageNavigateID_t m_unPageNavigateID;
};

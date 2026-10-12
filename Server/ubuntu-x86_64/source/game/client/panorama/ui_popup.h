//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/uiinputcapture.h"

class CUI_PopupManager;

namespace panorama
{
	class IUIEvent;
}

// Fire these events from your popups to dismiss the popup. The parameter is a
// subsequent event to fire once the popup is dismissed.
DECLARE_PANEL_EVENT1( UIPopupButtonClicked, const char * );
DECLARE_PANEL_EVENT1( UIPopupButtonClickedEvent, panorama::IUIEvent * );

enum EPopupBackgroundStyle
{
	k_EPopupBackgroundStyle_None,
	k_EPopupBackgroundStyle_Dim,			// Dim background, clicking on the background will NOT close the popup
	k_EPopupBackgroundStyle_Dim_Dismiss,	// Dim background. clicking on the background will close the popup
	k_EPopupBackgroundStyle_Blur,			// Blurred background, clicking on the background will NOT close the popup
	k_EPopupBackgroundStyle_Blur_Dismiss,	// Blurred background, clicking on the background will close the popup
};

// If you're creating a new style of popup, inherit from this class
class CUI_Popup : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_Popup, panorama::CPanel2D );

public:
	CUI_Popup( panorama::CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent );
	virtual ~CUI_Popup();

	void SetManager( CUI_PopupManager *pManager ) { m_pManager = pManager; }
	CUI_PopupManager* GetManager() { return m_pManager; }

	virtual void OnShow();
	virtual void OnClose();

	void ShowPopup();
	void ShowPopupAsync( float flDelay );
	panorama::IUIEvent* YldShowPopup( );
	bool IsPopupVisible();

	void SetPopupBackgroundStyle( EPopupBackgroundStyle eStyle );
	EPopupBackgroundStyle GetPopupBackgroundStyle() const { return m_eBackgroundStyle; }

	void CloseAndDeleteAsync( float flDelay = 0.0f );

	virtual void HandlePopupButtonClicked( const char *pchEventText );
	void HandlePopupButtonClicked( panorama::IUIEvent *pEvent );

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue );

	virtual void OnLayoutReloaded() OVERRIDE;

	panorama::CPanel2D *GetEventParent() { return m_pEventParent.Get(); }

protected:
	bool WasCreatedByJob() const;

protected:
	panorama::CPanelPtr< panorama::CPanel2D > m_pEventParent;
	panorama::IUIEvent* m_pResultEvent;
	bool m_bIsCreatedByJob;
	bool m_bReceivedPopupButtonClicked;

	// These methods are intended only to be called by CUI_PopupManager for popups in jobs
private:
	// Used for popups invoked by jobs.
	panorama::IUIEvent* GetResult() const;
	void SetIsCreatedByJob( bool bIsCreatedByJob );
	void CleanupPopupInJob();
	bool WasPopupButtonClicked() const;

private:
	bool OnPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char *pchEventText );
	bool OnPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, panorama::IUIEvent *pEvent );

	CUI_PopupManager *m_pManager;
	EPopupBackgroundStyle m_eBackgroundStyle;

	panorama::CGameInputCapture m_Capture;

	friend class CUI_PopupManager;
};


inline panorama::IUIEvent* CUI_Popup::GetResult() const
{
	return m_pResultEvent;
}

inline bool CUI_Popup::WasCreatedByJob() const
{
	return m_bIsCreatedByJob;
}

inline bool CUI_Popup::WasPopupButtonClicked() const
{
	return m_bReceivedPopupButtonClicked;
}

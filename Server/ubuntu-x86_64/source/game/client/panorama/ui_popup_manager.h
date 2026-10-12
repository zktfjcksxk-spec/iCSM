//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/controls/button.h"
#include "popups/ui_popup_generic_enums.h"
/*#include "game/shared/econ/econ_item_constants.h"*/

class CUI_Popup;
class CUI_Popup_Generic;

DECLARE_PANORAMA_EVENT1( SetPopupBackgroundBlur, bool );
DECLARE_PANEL_EVENT2( UIShowCustomLayoutPopup, const char *, const char * );
DECLARE_PANEL_EVENT3( UIShowCustomLayoutPopupParameters, const char *, const char *, const char * );
DECLARE_PANEL_EVENT1( UIPopupManagerVisibilityChanged, bool );

class CUI_PopupManager : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_PopupManager, panorama::CPanel2D );

public:
	CUI_PopupManager( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_PopupManager();

	static CUI_PopupManager *GetWindowPopupManager( panorama::IUIWindow *pWindow );
	static CUI_PopupManager *GetPanelPopupManager( panorama::CPanel2D *pPanel );

	void ShowPopup( CUI_Popup *pPopup );
	void ShowPopupAsync( CUI_Popup *pPopup, float flDelay );

	// This version is designed to be used from within a job.
	// It returns a result code which the job can switch on to do work
	panorama::IUIEvent* YldShowPopup( CUI_Popup *pPopup );

	bool IsPopupVisible( CUI_Popup *pPopup );
	bool IsAnyPopupVisible();
    void CloseIfVisible( CUI_Popup *pPopup, bool bImmediate = false );
	void CloseAllVisiblePopups();

	CUI_Popup_Generic *CreateGeneric( const char *pszTitle, const char *pszMessage, const char *pszOption1 = nullptr, const char *pszEvent1 = nullptr );
	void ShowGeneric( const char *pszTitle, const char *pszMessage );

	// This version is designed to be used from within a job
	// It returns a result code which the job can switch on to do work
	GenericPopupResult_t YldShowGeneric( const char *pszTitle, const char *pszMessage );

	// Some helper methods designed to be used from within jobs
#if defined( CSTRIKE15 )
	bool BYldPromptOKCancel( const char* pszHeader, const char* pszBody, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
	void YldPromptOK( const char* pszHeader, const char* pszBody, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
#else
	bool BYldPromptOKCancel( const char* pszHeader, const char* pszBody, const EconItemIDs_t *pIcon = nullptr, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
	void YldPromptOK( const char* pszHeader, const char* pszBody, const EconItemIDs_t *pIcon = nullptr, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
#endif
	bool BYldPromptOKCancelWithHeroIcon( const char* pszHeader, const char* pszBody, int nHeroID );
	void YldPromptOKWithHeroIcon( const char* pszHeader, const char* pszBody, int nHeroID );
#if defined( CSTRIKE15 )
	bool BYldPromptDangerousOKCancel( const char* pszHeader, const char* pszBody, const char *pConfirmationCode, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
#else
	bool BYldPromptDangerousOKCancel( const char* pszHeader, const char* pszBody, const char *pConfirmationCode, const EconItemIDs_t *pIcon = nullptr, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
#endif

	void UpdatePopupBackgrounds();

	virtual void OnAfterChildrenChanged() OVERRIDE;

	// Dev
	void ForceClosePopups( void );
	void OnPopupLayoutReloaded( CUI_Popup *pPopup );

	virtual bool BIsClientPanelEvent( panorama::CPanoramaSymbol symProperty );

protected:
	virtual void CloseAndDeletePopup( CUI_Popup *pPopup, float flDelay );

private:
	bool EventBackgroundClicked();
	bool EventShowPopupAsync( const char *pszPopupID );
	bool EventShowCustomLayoutPopup( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, const char *pszPopupID, const char *pszLayoutFile );
	bool EventShowCustomLayoutPopupParameters( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, const char *pszPopupID, const char *pszLayoutFile, const char *pszParameters );

	// Localization parent, used by CPopupDialogVariableScope
	virtual panorama::IUIPanel *GetLocalizationParent() const OVERRIDE;
	void SetLocalizationParent( panorama::CPanel2D *pPanel );

	panorama::CPanel2D *m_pDimBackgroundPanel;
	panorama::CButton *m_pBlurBackgroundButton;

	panorama::CPanelPtr< panorama::CPanel2D > m_hLocalizationParent;

	bool m_bHandlingChildrenChanged;

	friend class CUI_Popup;
	friend class CPopupDialogVariableScope;
};


// Helper used to set dialog variables on all popups created within the scope of CPopupDialogVariableScope
class CPopupDialogVariableScope
{
public:
	CPopupDialogVariableScope( CUI_PopupManager *pManager );
	~CPopupDialogVariableScope();

	void SetDialogVariable( const char *pchKey, const char *pchValue );
	void SetDialogVariable( const char *pchKey, int iVal );
	void SetDialogVariable( const char *pchKey, uint64 iVal );
	void SetDialogVariable( const char *pchKey, time_t timeVal );
	void SetDialogVariable( const char *pchKey, CCurrencyAmount amount );
	void SetDialogVariable( const char *varName, const CUtlString &value );
	void SetDialogVariableLocString( const char *varName, const char *pchValue );

private:
	panorama::CPanelPtr< panorama::CPanel2D > m_hTempPanel;
	panorama::CPanelPtr< panorama::CPanel2D > m_hPrevLocalizationParent;
	panorama::CPanelPtr< CUI_PopupManager > m_hPopupManager;
};

CUI_PopupManager *GetTopmostPopupManager();

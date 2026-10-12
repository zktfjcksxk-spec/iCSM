//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "../ui_popup.h"
#include "panorama/popups/ui_popup_generic_enums.h"

namespace panorama
{
	class CLabel;
	class CButton;
}

struct EconItemIDs_t;

struct PopupChoiceParam_t
{
	const char *pszLabel;
	const char *pEvent;			// event string
};

struct PopupChoiceParamEvent_t
{
	const char *pszLabel;
	panorama::IUIEvent *pEvent;	// panorama event
};

class CUI_Popup_Generic : public CUI_Popup
{
	DECLARE_PANEL2D( CUI_Popup_Generic, CUI_Popup );

public:
	CUI_Popup_Generic( panorama::CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent );

	virtual void LoadLayout( void );

	bool SetButtonEvent( int nButtonIndex, panorama::IUIEvent *pEvent );

	void SetDisplayParameters( const char *pszTitle, const char *pszMessage, const CUtlVector< PopupChoiceParam_t > &vecOptions );
	void SetDisplayParameters( const char *pszTitle, const char *pszMessage, const CUtlVector< PopupChoiceParamEvent_t > &vecOptions );

	// Convenience functions that just funnel into SetDisplayParameters
	void SetDisplayOneOption( const char *pszTitle, const char *pszMessage, const char *pszOption1, const char *pszEvent1 );
	void SetDisplayTwoOptions( const char *pszTitle, const char *pszMessage, const char *pszOption1, const char *pszEvent1, const char *pszOption2, const char *pszEvent2 );
	void SetDisplayOk( const char *pszTitle, const char *pszMessage, const char *pszOKEvent );
	void SetDisplayCancel( const char *pszTitle, const char *pszMessage, const char *pszCancelEvent );
	void SetDisplayYesNo( const char *pszTitle, const char *pszMessage, const char *pszYesEvent, const char *pszNoEvent );
	void SetDisplayOkCancel( const char *pszTitle, const char *pszMessage, const char *pszOKEvent, const char *pszCancelEvent );
	void SetDisplayYesNoCancel( const char *pszTitle, const char *pszMessage, const char *pszYesEvent, const char *pszNoEvent, const char *pszCancelEvent );

	// This is used for an intermediate window for when you're waiting for a response from the GC or something
	void SetDisplayNoOption( const char *pszTitle, const char *pszMessage );

    void SetSpinnerVisible( bool bVisible );
	void SetHeroIconVisible( int nHeroID );
#if !defined( CSTRIKE15 )
	void SetEconItemIconVisible( const EconItemIDs_t &ids, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
#endif
	void SetConfirmationCodeVisible( const char *pConfirmationCode );
	void SetProgressBarVisible( bool bVisible, float flMin = 0.0, float flMax = 1.0 );
	void SetProgressBarLevel( float flLevel );
    
	void SetDismissAndCancelEvent( const char *pszEvent );
	void SetDismissAndCancelEvent( panorama::IUIEvent *pEvent );

	// Show the popup from withi a job, get the button clicked back
	GenericPopupResult_t YldShowGenericPopup();

private:

	template <typename T>
	void SetDisplayParametersInternal( const char *pszTitle, const char *pszMessage, const CUtlVector< T > &vecOptions );

	// Used by SetDisplayParametersInternal to set the button activate event
	void SetButtonEventInternal( panorama::CButton *pButton, int nButtonIndex, const char *pszEvent );
	void SetButtonEventInternal( panorama::CButton *pButton, int nButtonIndex, panorama::IUIEvent *pEvent );

	bool OnConfirmationTextChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	// Never call this!! Use YldShowGenericPopup instead
	panorama::IUIEvent* YldShowPopup();

	bool OnGenericPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, GenericPopupResult_t result, const char *pchEventText );
	bool OnGenericPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, GenericPopupResult_t result, panorama::IUIEvent *pEvent );
	bool OnGenericConfirmFinished();

	GenericPopupResult_t m_nResult;
	CUtlString m_confirmationCode;
	bool m_bLayoutLoaded;
};

// Utility which will show a 'processing' dialog for the scope of the class
class CDOTA_UI_ProcessingPopupScope
{
public:
	CDOTA_UI_ProcessingPopupScope( const char *pszTitle, const char *pszMessage, bool bImmediateDismissal = true );
	~CDOTA_UI_ProcessingPopupScope();

	void Release();
private:
	panorama::CPanelPtr< CUI_Popup_Generic > m_hPopup;
	bool m_bImmediateDismissal;
};

// CUI_Popup_Generic_Command runs con commands based on the user's response (instead of firing Panorama events)
// Can be used in place of the Scaleform CCommandMsgBox
class CUI_Popup_Generic_Command : public CUI_Popup_Generic
{
public:
	CUI_Popup_Generic_Command( panorama::CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent );
	virtual void HandlePopupButtonClicked( const char *pchEventText );

	// This function always returns GENERIC_POPUP_RESULT_CANCELLED_BY_SYSTEM
	// These popups are not designed to be run from within a job
	GenericPopupResult_t YldShowGenericPopup();
};
//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "../ui_popup.h"

struct EconItemIDs_t;

namespace panorama
{
	class CPanel2D;
	class CTextEntry;
}

enum PopupGenericTextEntryMode_t
{
	MODE_SINGLE_LINE = 0,
	MODE_MULTI_LINE
};

class CDOTA_UI_Popup_Generic_TextEntry : public CUI_Popup
{
	DECLARE_PANEL2D( CDOTA_UI_Popup_Generic_TextEntry, CUI_Popup );

public:
	CDOTA_UI_Popup_Generic_TextEntry( panorama::CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent );

	void Init( PopupGenericTextEntryMode_t mode, const char *pszTitle, const char *pszMessage, const char *pszInitialTextEntryTextUTF8, const char *pszOption1 = nullptr, const char *pszEvent1 = nullptr, const char *pszOption2 = nullptr, const char *pszEvent2 = nullptr );
	void InitOkCancel( PopupGenericTextEntryMode_t mode, const char *pszTitle, const char *pszMessage, const char *pszInitialTextEntryTextUTF8, const char *pszEvent1 = nullptr, const char *pszEvent2 = nullptr );
	void SetHeroIconVisible( int nHeroID );
#if !defined( CSTRIKE15 )
	void SetEconItemIconVisible( const EconItemIDs_t &ids, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
#endif
	void SetMaxCharacters( int nMaxCount );

	// Show the popup from within a job, get the button clicked back, as well as the text entered.
	bool BYldShowPopup( CUtlString &textEntered );

private:
	struct PopupChoiceParam_t
	{
		const char *m_pszLabel;
		const char *m_pszEvent;
	};

	void Init( PopupGenericTextEntryMode_t mode, const char *pszTitle, const char *pszMessage, const char *pszInitialTextEntryTextUTF8, int nCount, const PopupChoiceParam_t *pParams );

	// Never call this!! Use BYldShowPopup insted
	panorama::IUIEvent* YldShowPopup();

	bool OnGenericTextEntryPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, int nResult, const char *pchEventText );
	bool OnGenericPopupTextEntryFinished();
	void SetDismissAndCancelEvent( const char *pszEvent );

	panorama::CPanel2D *m_pButtonContainer;
	panorama::CTextEntry *m_pTextEntry;

	int m_nResult;
};

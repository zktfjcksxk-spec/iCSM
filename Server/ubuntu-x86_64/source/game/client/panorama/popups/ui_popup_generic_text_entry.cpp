//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_popup_generic_text_entry.h"
#include "panorama/controls/label.h"
#include "panorama/controls/button.h"
#include "panorama/ui_popup_manager.h"
#if !defined( CSTRIKE15 )
#include "panorama/ui_econ_item_image.h"
#endif
#include "panorama/controls/textentry.h"

#ifdef DOTA_DLL
#include "panorama/dota_ui_tooltip_manager.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CDOTA_UI_Popup_Generic_TextEntry, PopupGenericTextEntry )

DECLARE_PANEL_EVENT2( GenericTextEntryPopupButtonClicked, int, const char * );
DEFINE_PANORAMA_EVENT( GenericTextEntryPopupButtonClicked )

DECLARE_PANORAMA_EVENT0( GenericPopupTextEntryFinished );
DEFINE_PANORAMA_EVENT( GenericPopupTextEntryFinished )

using namespace panorama;

CDOTA_UI_Popup_Generic_TextEntry::CDOTA_UI_Popup_Generic_TextEntry( CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent )
	: CUI_Popup( pParent, pchID, pEventParent )
	, m_nResult( -1 )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/popups/popup_generic_text_entry.xml" ) );

	m_pButtonContainer = FindChildInLayoutFile( "ButtonContainer" );
	m_pTextEntry = panel_cast< CTextEntry * >( FindChildInLayoutFile( "TextEntry" ) );

	RegisterEventHandler( GenericTextEntryPopupButtonClicked(), this, &CDOTA_UI_Popup_Generic_TextEntry::OnGenericTextEntryPopupButtonClicked );
	RegisterEventHandler( GenericPopupTextEntryFinished(), this, &CDOTA_UI_Popup_Generic_TextEntry::OnGenericPopupTextEntryFinished );
}

void CDOTA_UI_Popup_Generic_TextEntry::Init( PopupGenericTextEntryMode_t mode, const char *pszTitle, const char *pszMessage, const char *pszInitialTextEntryTextUTF8, int nCount, const PopupChoiceParam_t *pParams )
{
	SetDialogVariableLocString( "generic_popup_title", pszTitle ? pszTitle : "" );
	SetDialogVariableLocString( "generic_popup_body", pszMessage ? pszMessage : "" );
	
	bool bMultiline = ( mode == MODE_MULTI_LINE );
	SetHasClass( "Multiline", bMultiline );
	m_pTextEntry->SetMultiline( bMultiline );
	m_pTextEntry->SetText( pszInitialTextEntryTextUTF8 );
	m_pTextEntry->SetFocus();

	m_pButtonContainer->RemoveAndDeleteChildren();

	for ( int i = 0; i < nCount; ++i )
	{
		if ( !pParams[i].m_pszLabel )
			continue;

		const char *pszID = CFmtStr( "Button%d", i );
		CButton *pButton = new CButton( m_pButtonContainer, pszID );
		pButton->AddClass( "PopupButton" );
		CLabel *pLabel = new CLabel( pButton, NULL );
		pLabel->SetText( pParams[ i ].m_pszLabel );
		pButton->SetOnActivateEvent( GenericTextEntryPopupButtonClicked::MakeEvent( this, i, pParams[i].m_pszEvent ? pParams[i].m_pszEvent : "" ) );
	}
}

void CDOTA_UI_Popup_Generic_TextEntry::Init( PopupGenericTextEntryMode_t mode, const char *pszTitle, const char *pszMessage, const char *pszInitialTextEntryTextUTF8, const char *pszOption1, const char *pszEvent1, const char *pszOption2, const char *pszEvent2 )
{
	PopupChoiceParam_t params[] =
	{
		{ pszOption1, pszEvent1 },
		{ pszOption2, pszEvent2 },
	};

	Init( mode, pszTitle, pszMessage, pszInitialTextEntryTextUTF8, V_ARRAYSIZE( params ), params );
}

void CDOTA_UI_Popup_Generic_TextEntry::SetDismissAndCancelEvent( const char *pszEvent )
{
	SetOnCancelEvent( UIPopupButtonClicked::MakeEvent( this, pszEvent ) );
}

void CDOTA_UI_Popup_Generic_TextEntry::InitOkCancel( PopupGenericTextEntryMode_t mode, const char *pszTitle, const char *pszMessage, const char *pszInitialTextEntryTextUTF8, const char *pszEvent1, const char *pszEvent2 )
{
	PopupChoiceParam_t params[] =
	{
		{ "#OK", pszEvent1 },
		{ "#Cancel", pszEvent2 },
	};

	Init( mode, pszTitle, pszMessage, pszInitialTextEntryTextUTF8, V_ARRAYSIZE( params ), params );
	SetDismissAndCancelEvent( pszEvent2 );
}

#if !defined( CSTRIKE15 )
void CDOTA_UI_Popup_Generic_TextEntry::SetEconItemIconVisible( const EconItemIDs_t &ids, style_index_t nStyleIndex )
{
	CUI_EconItemImage *pImage = panel_cast< CUI_EconItemImage* >( FindChildInLayoutFile( "EconItemImage" ) );
	SetHasClass( "EconItemIconEnabled", true );
	pImage->SetItem( ids, nStyleIndex );

//	pImage->SetOnMouseOverEvent( ShowEconItemIdTooltip::MakeEvent( pImage, ids.m_nItemID, ids.m_nItemDefIndex, INVALID_STYLE_INDEX, -1 ) );
//	pImage->SetOnMouseOutEvent( HideEconItemTooltip::MakeEvent( pImage ) );
}
#endif

void CDOTA_UI_Popup_Generic_TextEntry::SetHeroIconVisible( int nHeroID )
{
#ifdef DOTA_DLL
	bool bHasHero = ( nHeroID >= 0 ) && ( nHeroID != PLAYER_LOADOUT_HERO_ID );
	RemoveClass( "HeroIconEnabled" );
	if ( bHasHero )
	{
		const char *pszHeroName = DOTAGameManager()->GetHeroUnitNameByID( nHeroID );
		if ( pszHeroName && pszHeroName[0] )
		{
			CImagePanel *pImage = panel_cast< CImagePanel* >( FindChildInLayoutFile( "HeroImage" ) );
			CFmtStr imageName( "file://{images}/heroes/%s.png", pszHeroName );
			pImage->SetImage( imageName.Get() );
			AddClass( "HeroIconEnabled" );
		}
	}
#endif
}

void CDOTA_UI_Popup_Generic_TextEntry::SetMaxCharacters( int nMaxCount )
{
	m_pTextEntry->SetMaxChars( nMaxCount );
}

bool CDOTA_UI_Popup_Generic_TextEntry::OnGenericPopupTextEntryFinished()
{
	if ( m_pButtonContainer->GetChildCount() > 0 )
	{
		CButton *pButton = panel_cast< CButton* >( m_pButtonContainer->GetChild( 0 ) );
		if ( pButton->IsEnabled() )
		{
			pButton->DispatchPanelEvent( "onactivate" );
		}
	}
	return true;
}

bool CDOTA_UI_Popup_Generic_TextEntry::OnGenericTextEntryPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, int nResult, const char *pchEventText )
{
	m_nResult = nResult;
	HandlePopupButtonClicked( pchEventText );
	return true;
}

bool CDOTA_UI_Popup_Generic_TextEntry::BYldShowPopup( CUtlString &textEntered )
{
	// Have to cache off the popup since it may have get deleted during YldShowPopup.
	panorama::CPanelPtr< CUI_Popup > hPopup = this;

	assert_cast< CUI_PopupManager* >( GetParent() )->YldShowPopup( this );

	CUI_Popup *pThis = hPopup.Get();
	if ( !pThis )
		return false;

	bool bOk = ( m_nResult == 0 );
	if ( bOk )
	{
		textEntered = m_pTextEntry->PchGetText();
	}
	return bOk;
}

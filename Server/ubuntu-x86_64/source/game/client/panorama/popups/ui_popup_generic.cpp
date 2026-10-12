//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_popup_generic.h"
#include "panorama/controls/label.h"
#include "panorama/controls/button.h"
#include "panorama/controls/progressbar.h"
#include "panorama/ui_popup_manager.h"
#if !defined( CSTRIKE15 )
#include "panorama/ui_econ_item_image.h"
#endif
#include "panorama/controls/textentry.h"
//#include "panorama/dota_ui_tooltip_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Popup_Generic, PopupGeneric )

DECLARE_PANEL_EVENT2( GenericPopupButtonClicked, GenericPopupResult_t, const char * );
DECLARE_PANEL_EVENT2( GenericPopupButtonClickedEvent, GenericPopupResult_t, panorama::IUIEvent * );
DEFINE_PANORAMA_EVENT( GenericPopupButtonClicked )
DEFINE_PANORAMA_EVENT( GenericPopupButtonClickedEvent )

DECLARE_PANORAMA_EVENT0( GenericConfirmFinished );
DEFINE_PANORAMA_EVENT( GenericConfirmFinished )

using namespace panorama;

#define POPUP_GENERIC_LAYOUT

CUI_Popup_Generic::CUI_Popup_Generic( CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent )
	: CUI_Popup( pParent, pchID, pEventParent )
	, m_bLayoutLoaded( false )
	, m_nResult( GENERIC_POPUP_RESULT_NOT_RECEIVED )
{
	RegisterEventHandler( GenericPopupButtonClicked(), this, &CUI_Popup_Generic::OnGenericPopupButtonClicked );
	RegisterEventHandler( GenericPopupButtonClickedEvent(), this, &CUI_Popup_Generic::OnGenericPopupButtonClicked );
	RegisterEventHandler( GenericConfirmFinished(), this, &CUI_Popup_Generic::OnGenericConfirmFinished );
}

void CUI_Popup_Generic::LoadLayout( void )
{
	if ( !m_bLayoutLoaded )
	{
		DbgVerify( BLoadLayout( "file://{resources}/layout/popups/popup_generic.xml" ) );
		RegisterEventHandler( TextEntryChanged(), this, &CUI_Popup_Generic::OnConfirmationTextChanged );
		m_bLayoutLoaded = true;
	}
}

bool CUI_Popup_Generic::SetButtonEvent( int nButtonIndex, panorama::IUIEvent *pEvent )
{
	if ( !pEvent )
		return false;

	const char *pszID = CFmtStr( "Button%d", nButtonIndex );

	CButton *pButton = panel_cast<CButton *>( FindChildInLayoutFile( pszID ) );
	if ( pButton )
	{
		pButton->SetOnActivateEvent( pEvent );
		return true;
	}

	return false;
}

void CUI_Popup_Generic::SetButtonEventInternal( panorama::CButton *pButton, int nButtonIndex, const char *pszEvent )
{
	if ( !pButton )
		return;

	if ( pszEvent && *pszEvent )
	{
		pButton->SetOnActivateEvent( GenericPopupButtonClicked::MakeEvent( this, (GenericPopupResult_t)nButtonIndex, pszEvent ) );
	}
	else
	{
		pButton->SetOnActivateEvent( GenericPopupButtonClicked::MakeEvent( this, (GenericPopupResult_t)nButtonIndex, "" ) );
	}
}

void CUI_Popup_Generic::SetButtonEventInternal( panorama::CButton *pButton, int nButtonIndex, panorama::IUIEvent *pEvent )
{
	if ( !pButton )
		return;

	
	pButton->SetOnActivateEvent( GenericPopupButtonClickedEvent::MakeEvent( this, (GenericPopupResult_t)nButtonIndex, pEvent ) );
}

template <typename T>
void CUI_Popup_Generic::SetDisplayParametersInternal( const char *pszTitle, const char *pszMessage, const CUtlVector< T > &vecOptions )
{
	LoadLayout();

	CLabel *pTitleLabel = panel_cast<CLabel *>( FindChildInLayoutFile( "TitleLabel" ) );
	CLabel *pMessageLabel = panel_cast<CLabel *>( FindChildInLayoutFile( "MessageLabel" ) );
	CPanel2D *pButtonContainer = FindChildInLayoutFile( "ButtonContainer" );

	static const CPanoramaSymbol k_symNoTitle( "NoTitle" );
	static const CPanoramaSymbol k_symNoMessage( "NoMessage" );

	SetHasClass( k_symNoTitle, V_isempty( pszTitle ) );
	SetHasClass( k_symNoMessage, V_isempty( pszMessage ) );

	pTitleLabel->SetText( pszTitle );
	pMessageLabel->SetText( pszMessage );

	int nMaxOptionCount = Max( vecOptions.Count(), pButtonContainer->GetChildCount() );
	for ( int i = 0; i < nMaxOptionCount; ++i )
	{
		CButton *pButton = NULL;
		CLabel *pLabel = NULL;
		if ( i >= pButtonContainer->GetChildCount() )
		{
			const char *pszID = CFmtStr( "Button%d", i );
			pButton = new CButton( pButtonContainer, pszID );
			pButton->AddClass( "PopupButton" );
			pLabel = new CLabel( pButton, NULL );
		}
		else
		{
			pButton = panel_cast< CButton * > ( pButtonContainer->GetChild( i ) );
			pLabel = panel_cast< CLabel * >( pButton->GetFirstChild() );
		}

		if ( i >= vecOptions.Count() )
		{
			pButton->SetVisible( false );
		}
		else
		{
			pButton->SetVisible( true );
			pLabel->SetText( vecOptions[ i ].pszLabel );

			SetButtonEventInternal( pButton, i, vecOptions[i].pEvent );
		}

		// Disable the 'OK' button if there's a confirmation code
		if ( ( i == 0 ) && !m_confirmationCode.IsEmpty() )
		{
			pButton->SetEnabled( false );
		}

		pButton->SetTabIndex( i );
		pButton->SetSelectionPosition( k_flSelectionPosAuto, k_flSelectionPosAuto );
	}
}

void CUI_Popup_Generic::SetDisplayParameters( const char *pszTitle, const char *pszMessage, const CUtlVector< PopupChoiceParam_t > &vecOptions )
{
	SetDisplayParametersInternal( pszTitle, pszMessage, vecOptions );
}

void CUI_Popup_Generic::SetDisplayParameters( const char *pszTitle, const char *pszMessage, const CUtlVector< PopupChoiceParamEvent_t > &vecOptions )
{
	SetDisplayParametersInternal( pszTitle, pszMessage, vecOptions );
}

void CUI_Popup_Generic::SetDisplayNoOption( const char *pszTitle, const char *pszMessage )
{
	CUtlVector< PopupChoiceParam_t > vecOptions;
	SetDisplayParameters( pszTitle, pszMessage, vecOptions );
}

void CUI_Popup_Generic::SetDisplayOneOption( const char *pszTitle, const char *pszMessage, const char *pszOption1, const char *pszEvent1 )
{
	CUtlVector< PopupChoiceParam_t > vecOptions;
	vecOptions.AddToTail( { pszOption1, pszEvent1 } );
	SetDisplayParameters( pszTitle, pszMessage, vecOptions );
	SetDismissAndCancelEvent( pszEvent1 );
}

void CUI_Popup_Generic::SetDisplayTwoOptions( const char *pszTitle, const char *pszMessage, const char *pszOption1, const char *pszEvent1, const char *pszOption2, const char *pszEvent2 )
{
	CUtlVector< PopupChoiceParam_t > vecOptions;
	vecOptions.AddToTail( { pszOption1, pszEvent1 } );
	vecOptions.AddToTail( { pszOption2, pszEvent2 } );
	SetDisplayParameters( pszTitle, pszMessage, vecOptions );
}

void CUI_Popup_Generic::SetDisplayOk( const char *pszTitle, const char *pszMessage, const char *pszOKEvent )
{
	SetDisplayOneOption( pszTitle, pszMessage, "#OK", pszOKEvent );
}

void CUI_Popup_Generic::SetDisplayCancel( const char *pszTitle, const char *pszMessage, const char *pszCancelEvent )
{
	SetDisplayOneOption( pszTitle, pszMessage, "#Cancel", pszCancelEvent );
}

void CUI_Popup_Generic::SetDisplayYesNo( const char *pszTitle, const char *pszMessage, const char *pszYesEvent, const char *pszNoEvent )
{
	SetDisplayTwoOptions( pszTitle, pszMessage, "#UI_Yes", pszYesEvent, "#UI_No", pszNoEvent );
}

void CUI_Popup_Generic::SetDisplayOkCancel( const char *pszTitle, const char *pszMessage, const char *pszOKEvent, const char *pszCancelEvent )
{
	SetDisplayTwoOptions( pszTitle, pszMessage, "#OK", pszOKEvent, "#Cancel", pszCancelEvent );
	SetDismissAndCancelEvent( pszCancelEvent );
}

void CUI_Popup_Generic::SetDisplayYesNoCancel( const char *pszTitle, const char *pszMessage, const char *pszYesEvent, const char *pszNoEvent, const char *pszCancelEvent )
{
	CUtlVector< PopupChoiceParam_t > vecOptions;
	vecOptions.AddToTail( { "#UI_Yes", pszYesEvent } );
	vecOptions.AddToTail( { "#UI_No", pszNoEvent } );
	vecOptions.AddToTail( { "#Cancel", pszCancelEvent } );
	SetDisplayParameters( pszTitle, pszMessage, vecOptions );
	SetDismissAndCancelEvent( pszCancelEvent );
}

void CUI_Popup_Generic::SetSpinnerVisible( bool bVisible )
{
	LoadLayout();

	CPanel2D *pSpinner = FindChildInLayoutFile( "Spinner" );
    pSpinner->SetHasClass( "SpinnerVisible", bVisible );
}

#if !defined( CSTRIKE15 )
void CUI_Popup_Generic::SetEconItemIconVisible( const EconItemIDs_t &ids, style_index_t nStyleIndex )
{
	LoadLayout();

	CUI_EconItemImage *pImage = panel_cast< CUI_EconItemImage* >( FindChildInLayoutFile( "EconItemImage" ) );
	SetHasClass( "EconItemIconEnabled", true );
	pImage->SetItem( ids, nStyleIndex );
//	pImage->SetOnMouseOverEvent( ShowEconItemIdTooltip::MakeEvent( pImage, ids.m_nItemID, ids.m_nItemDefIndex, INVALID_STYLE_INDEX, -1 ) );
//	pImage->SetOnMouseOutEvent( HideEconItemTooltip::MakeEvent( pImage ) );
}
#endif

void CUI_Popup_Generic::SetHeroIconVisible( int nHeroID )
{
	// Only thing here not immediately generalizable. DOTA should probably implement a CUI_Popup_Generic for this functionality.
#ifdef DOTA_DLL
	LoadLayout();

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

void CUI_Popup_Generic::SetConfirmationCodeVisible( const char *pConfirmationCode )
{
	LoadLayout();

	CLocStringSafePointer pString = UILocalize()->PchFindToken( UIPanel(), pConfirmationCode, k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None, k_eStringEscapeStyle_None );
	if ( pString && !pString->IsEmpty() )
	{
		m_confirmationCode = pString->String();
	}
	else
	{
		m_confirmationCode = pConfirmationCode;
	}

	SetHasClass( "ConfirmationCodeEnabled", !m_confirmationCode.IsEmpty() );

	// Cause notifications to be called when typing in the text box
	CTextEntry *pTextEntry = panel_cast< CTextEntry * >( FindChildInLayoutFile( "ConfirmationText" ) );
	pTextEntry->RaiseChangeEvents( true );

	SetDialogVariable( "confirmation_code", m_confirmationCode.Get() );
	CPanel2D *pButtonContainer = FindChildInLayoutFile( "ButtonContainer" );
	if ( pButtonContainer->GetChildCount() > 0 )
	{
		CButton *pButton = panel_cast< CButton* >( pButtonContainer->GetChild( 0 ) );
		pButton->SetEnabled( m_confirmationCode.IsEmpty() );
	}
}

bool CUI_Popup_Generic::OnConfirmationTextChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( m_confirmationCode.IsEmpty() )
		return true;

	CTextEntry *pTextEntry = panel_cast< CTextEntry * >( ToPanel2D( pPanel.Get() ) );
	bool bMatches = !V_stricmp( m_confirmationCode.Get(), pTextEntry->PchGetText() );

	CPanel2D *pButtonContainer = FindChildInLayoutFile( "ButtonContainer" );
	if ( pButtonContainer->GetChildCount() > 0 )
	{
		CButton *pButton = panel_cast< CButton* >( pButtonContainer->GetChild( 0 ) );
		pButton->SetEnabled( bMatches );
	}

	return true;
}

bool CUI_Popup_Generic::OnGenericConfirmFinished()
{
	CPanel2D *pButtonContainer = FindChildInLayoutFile( "ButtonContainer" );
	if ( pButtonContainer->GetChildCount() > 0 )
	{
		CButton *pButton = panel_cast< CButton* >( pButtonContainer->GetChild( 0 ) );
		if ( pButton->IsEnabled() )
		{
			pButton->DispatchPanelEvent( "onactivate" );
		}
	}
	return true;
}

void CUI_Popup_Generic::SetProgressBarVisible( bool bVisible, float flMin, float flMax )
{
	LoadLayout();

	panorama::CProgressBar *pProgressBar = panel_cast<CProgressBar*>( FindChildInLayoutFile( "ProgressBar" ) );
	if ( pProgressBar )
	{
		pProgressBar->SetHasClass( "ProgressBarVisible", bVisible );
		pProgressBar->SetMin( flMin );
		pProgressBar->SetMax( flMax );
	}
}

void CUI_Popup_Generic::SetProgressBarLevel( float flLevel )
{
	panorama::CProgressBar *pProgressBar = panel_cast<CProgressBar*>( FindChildInLayoutFile( "ProgressBar" ) );
	if ( pProgressBar )
	{
		pProgressBar->SetValue( flLevel );
	}
}

void CUI_Popup_Generic::SetDismissAndCancelEvent( const char *pszEvent )
{
	SetOnCancelEvent( UIPopupButtonClicked::MakeEvent( this, pszEvent ) );
}

void CUI_Popup_Generic::SetDismissAndCancelEvent( panorama::IUIEvent *pEvent )
{
	SetOnCancelEvent( UIPopupButtonClickedEvent::MakeEvent( this, pEvent ) );
}

bool CUI_Popup_Generic::OnGenericPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, GenericPopupResult_t result, const char *pchEventText )
{
	m_nResult = result;
	HandlePopupButtonClicked( pchEventText );
	return true;
}

bool CUI_Popup_Generic::OnGenericPopupButtonClicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, GenericPopupResult_t result, panorama::IUIEvent *pEvent )
{
	m_nResult = result;
	HandlePopupButtonClicked( pEvent ? pEvent->Copy() : nullptr );
	return true;
}

GenericPopupResult_t CUI_Popup_Generic::YldShowGenericPopup()
{
	// Have to cache off the popup since it may have get deleted during YldShowPopup.
	panorama::CPanelPtr< CUI_Popup > hPopup = this;

	assert_cast< CUI_PopupManager* >( GetParent() )->YldShowPopup( this );

	CUI_Popup *pThis = hPopup.Get();
	if ( !pThis )
		return GENERIC_POPUP_RESULT_CANCELLED_BY_SYSTEM;

	return m_nResult;
}


// Utility which will show a 'processing' dialog for the scope of the class
CDOTA_UI_ProcessingPopupScope::CDOTA_UI_ProcessingPopupScope( const char *pszTitle, const char *pszMessage, bool bImmediateDismissal )
{
	CUI_Popup_Generic *pPopup = new CUI_Popup_Generic( GetTopmostPopupManager(), nullptr, nullptr );
	pPopup->SetDisplayNoOption( pszTitle, pszMessage );
	pPopup->SetSpinnerVisible( true );
	GetTopmostPopupManager()->ShowPopup( pPopup );
	m_hPopup = pPopup;
	m_bImmediateDismissal = bImmediateDismissal;
}

CDOTA_UI_ProcessingPopupScope::~CDOTA_UI_ProcessingPopupScope()
{
	Release();
}

void CDOTA_UI_ProcessingPopupScope::Release()
{
	CUI_Popup_Generic *pPopup = m_hPopup.Get();
	if ( pPopup )
	{
		GetTopmostPopupManager()->CloseIfVisible( pPopup, m_bImmediateDismissal );
		m_hPopup = nullptr;
	}
}

CUI_Popup_Generic_Command::CUI_Popup_Generic_Command( CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent )
	: CUI_Popup_Generic( pParent, pchID, pEventParent )
{
}

void CUI_Popup_Generic_Command::HandlePopupButtonClicked( const char *pchCommandText )
{
	// Send the con command if necessary
	if ( pchCommandText )
	{
		engine->ClientCmd_Unrestricted( pchCommandText );
	}
	CloseAndDeleteAsync( 0.5f );
}

GenericPopupResult_t CUI_Popup_Generic_Command::YldShowGenericPopup()
{
	// These popups are not designed to be run from within a job
	return GENERIC_POPUP_RESULT_CANCELLED_BY_SYSTEM;
}


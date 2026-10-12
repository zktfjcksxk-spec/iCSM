//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_toast_manager.h"
#include "panorama/uievents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_ToastManager, ToastManager )

DECLARE_PANORAMA_EVENT0( ToastManagerCheckShowNewToasts );

DEFINE_PANORAMA_EVENT( ToastManagerCheckShowNewToasts );
DEFINE_PANORAMA_EVENT( DismissToast );
DEFINE_PANORAMA_EVENT( ToastShown );
DEFINE_PANORAMA_EVENT( ToastHidden );

using namespace panorama;

const float k_flToastPollTime = 0.1f;
const float k_flToastDelayTime = 0.3f;
const float k_flDefaultToastDuration = 5.0f;
const int k_nDefaultMaxToasts = 3;

CGlobalPanoramaSymbol k_symToastPanel( "ToastPanel" );
CGlobalPanoramaSymbol k_symToastVisible( "ToastVisible" );
const char *k_pszToastShowTime = "toast_show_time";
const char *k_pszToastExitDurationDefine = "toastExitDuration";

CUI_ToastManager::CUI_ToastManager( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_flToastDuration( k_flDefaultToastDuration )
	, m_nMaxToastsVisible( k_nDefaultMaxToasts )
	, m_flLastToastShowTime( 0.0f )
{
	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CUI_ToastManager::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ToastManagerCheckShowNewToasts(), &CUI_ToastManager::EventCheckShowNewToasts );
		RegisterEventHandlerOnPanelType( ReadyForDisplay(), &CUI_ToastManager::EventReadyForDisplay );
		RegisterEventHandlerOnPanelType( DismissToast(), &CUI_ToastManager::EventDismissToast );
	}

	RegisterForReadyEvents( true );
}

CUI_ToastManager::~CUI_ToastManager()
{
}

bool CUI_ToastManager::BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symToastDuration( "toastduration" );
	static const CPanoramaSymbol k_symMaxToastsVisible( "maxtoastsvisible" );

	if ( symName == k_symToastDuration )
	{
		double dSeconds = 0.0f;
		if ( !CSSHelpers::BParseTime( &dSeconds, pchValue ) )
			return false;

		SetToastDuration( ( float )dSeconds );
		return true;
	}
	else if ( symName == k_symMaxToastsVisible )
	{
		int nMaxToastsVisible = V_atoi( pchValue );
		if ( nMaxToastsVisible <= 0 )
			return false;

		SetMaxToastsVisible( nMaxToastsVisible );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

void CUI_ToastManager::QueueToast( panorama::CPanel2D *pToast )
{
	pToast->AddClass( k_symToastPanel );
	pToast->SetParent( this );

	CheckShowNewToasts();
}

void CUI_ToastManager::SetToastDuration( float flDuration )
{
	m_flToastDuration = flDuration;
}

void CUI_ToastManager::SetMaxToastsVisible( int nMaxToastsVisible )
{
	m_nMaxToastsVisible = nMaxToastsVisible;
}

bool CUI_ToastManager::IsToastVisible( panorama::CPanel2D *pToast ) const
{
	if ( !pToast )
		return false;

	if ( pToast->GetParent() != this )
		return false;

	return pToast->BHasClass( k_symToastVisible );
}

void CUI_ToastManager::CheckShowNewToasts()
{
	int nVisibleCount = 0;
	int nChildCount = GetChildCount();
	for ( int i = 0; i < m_nMaxToastsVisible && i < nChildCount; ++i )
	{
		CPanel2D *pChild = GetChild( i );
		if ( !pChild )
		{
			Assert( false );
			continue;
		}

		if ( UIEngine()->BIsPanelWaitingAsyncDelete( pChild->UIPanel() ) )
			continue;

		// If a toast is visible, see if we should delete it
		if ( pChild->BHasClass( k_symToastVisible ) )
		{
			float flToastShowTime = pChild->GetAttribute( k_pszToastShowTime, 0.0f );
			if ( Plat_FloatTime() > flToastShowTime + m_flToastDuration && !pChild->BHasHoverStyle() )
			{
				AsyncDeleteToast( pChild );
			}
			else
			{
				nVisibleCount++;
			}

			continue;
		}

		// If we're ready for display and we've waited a little bit after just showing a different toast,
		// then show the next toast
		if ( BReadyForDisplay() )
		{
			float flShowTime = ( float )Plat_FloatTime();
			if ( flShowTime > m_flLastToastShowTime + k_flToastDelayTime )
			{
				pChild->SetAttribute( k_pszToastShowTime, flShowTime );
				pChild->AddClass( k_symToastVisible );
				m_flLastToastShowTime = flShowTime;

				UIEngine()->DispatchEvent( ToastShown::MakeEvent( pChild ) );
				break;
			}
		}
	}

	// If we still have toasts that might need to be displayed and we're either ready for display
	// or we have displayed toasts, then keep polling
	if ( nChildCount > 0 && ( BReadyForDisplay() || nVisibleCount > 0 ) )
	{
		DispatchEventAsync( k_flToastPollTime, ToastManagerCheckShowNewToasts(), this );
	}
}

void CUI_ToastManager::AsyncDeleteToast( panorama::CPanel2D *pToast )
{
	float flExitDuration = pToast->GetLayoutFileDefineFloat( k_pszToastExitDurationDefine, 0.0f );
	pToast->RemoveClass( k_symToastVisible );
	pToast->SetAttribute( k_pszToastShowTime, -1.0f );
	pToast->DeleteAsync( flExitDuration );

	UIEngine()->DispatchEvent( ToastHidden::MakeEvent( pToast ) );
}

void CUI_ToastManager::RemoveToast( panorama::CPanel2D *pToast )
{
	if ( !pToast || pToast->GetParent() != this )
		return;

	// If it's visible, delete it with an animation immediately
	if ( pToast->BHasClass( k_symToastVisible ) )
	{
		AsyncDeleteToast( pToast );
		return;
	}

	// If it's already about to be deleted, then there's nothing more to do
	if ( UIEngine()->BIsPanelWaitingAsyncDelete( pToast->UIPanel() ) )
		return;

	// If it's pending becoming visible, then we can just kill it directly
	delete pToast;
}

void CUI_ToastManager::RemoveAllToasts()
{
	for ( int i = GetChildCount() - 1; i >= 0; --i )
	{
		RemoveToast( GetChild( i ) );
	}
}

bool CUI_ToastManager::EventCheckShowNewToasts()
{
	CheckShowNewToasts();
	return true;
}

bool CUI_ToastManager::EventDismissToast( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr )
{
	CPanel2D *pToast = ToPanel2D( panelPtr.Get() );
	if ( !pToast || pToast->GetParent() != this )
		return false;

	RemoveToast( pToast );
	return true;
}

bool CUI_ToastManager::EventReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr )
{
	CheckShowNewToasts();
	return true;
}

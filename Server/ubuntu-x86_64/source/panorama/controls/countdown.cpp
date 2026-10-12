//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/countdown.h"
#include "uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCountdown, Countdown );

DECLARE_PANORAMA_EVENT1( UpdateCountdownInternal, int );
DEFINE_PANORAMA_EVENT( UpdateCountdownInternal );

CCountdown::CCountdown( CPanel2D *parent, const char * pchPanelID )
	: CPanel2D( parent, pchPanelID )
	, m_tStartTime( 0 )
	, m_tEndTime( 0 )
	, m_flUpdateInterval( 1.0f )
	, m_strTimeDialogVariable( "countdown_time" )
	, m_nUpdateCount( 0 )
	, m_bUpdateEnabled( true )
{
	// Register for events
	RegisterForReadyEvents( true );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CCountdown::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ReadyForDisplay(), &CCountdown::EventReadyForDisplay );
		RegisterEventHandlerOnPanelType( UpdateCountdownInternal(), &CCountdown::EventUpdateCountdown );
	}
}

CCountdown::~CCountdown()
{
}

bool CCountdown::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symStartTime( "start-time" );
	static const CPanoramaSymbol k_symEndTime( "end-time" );
	static const CPanoramaSymbol k_symUpdateInterval( "update-interval" );
	static const CPanoramaSymbol k_symTimeDialogVariable( "time-dialog-variable" );

	if ( symName == k_symStartTime )
	{
		SetStartTime( ( time_t )V_atoi64( pchValue ) );
		return true;
	}
	else if ( symName == k_symEndTime )
	{
		SetEndTime( ( time_t )V_atoui64( pchValue ) );
		return true;
	}
	else if ( symName == k_symUpdateInterval )
	{
		SetUpdateInterval( V_atof( pchValue ) );
		return true;
	}
	else if ( symName == k_symTimeDialogVariable )
	{
		SetTimeDialogVariable( pchValue );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}


void CCountdown::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "startTime", PANORAMA_DELEGATE( &CCountdown::GetStartTime ), PANORAMA_DELEGATE( &CCountdown::SetStartTime ) );
	RegisterJSAccessor( "endTime", PANORAMA_DELEGATE( &CCountdown::GetEndTime ), PANORAMA_DELEGATE( &CCountdown::SetEndTime ) );
	RegisterJSAccessor( "updateInterval", PANORAMA_DELEGATE( &CCountdown::GetUpdateInterval ), PANORAMA_DELEGATE( &CCountdown::SetUpdateInterval ) );
	RegisterJSAccessor( "timeDialogVariable", PANORAMA_DELEGATE( &CCountdown::GetTimeDialogVariable ), PANORAMA_DELEGATE( &CCountdown::SetTimeDialogVariable ) );
}

bool CCountdown::EventReadyForDisplay( const CPanelPtr< IUIPanel > &panelPtr )
{
	UpdateCountdown();
	return true;
}

void CCountdown::UpdateCountdown()
{
	static const CPanoramaSymbol k_symCountdownState( "countdown_state" );
	static const CPanoramaSymbol k_symCountdownBeforeStart( "CountdownBeforeStart" );
	static const CPanoramaSymbol k_symCountdownActive( "CountdownActive" );
	static const CPanoramaSymbol k_symCountdownAfterEnd( "CountdownAfterEnd" );

	if( m_bUpdateEnabled )
	{
		time_t tTimeNow = time( NULL );
		if( tTimeNow < m_tStartTime )
		{
			SwitchClass( k_symCountdownState, k_symCountdownBeforeStart );
			SetDialogVariable( m_strTimeDialogVariable, m_tStartTime - tTimeNow );
		}
		else if( tTimeNow < m_tEndTime )
		{
			SwitchClass( k_symCountdownState, k_symCountdownActive );
			SetDialogVariable( m_strTimeDialogVariable, m_tEndTime - tTimeNow );
		}
		else
		{
			SwitchClass( k_symCountdownState, k_symCountdownAfterEnd );
			SetDialogVariable( m_strTimeDialogVariable, tTimeNow - m_tEndTime );
		}

		if( BReadyForDisplay() )
		{
			DispatchEventAsync( m_flUpdateInterval, UpdateCountdownInternal(), this, ++m_nUpdateCount );
		}
	}
}

bool CCountdown::EventUpdateCountdown( int nUpdateCount )
{
	if ( m_nUpdateCount != nUpdateCount )
		return true;

	UpdateCountdown();
	return true;
}


void CCountdown::SetStartTime( time_t tStartTime )
{
	if ( m_tStartTime == tStartTime )
		return;

	m_tStartTime = tStartTime;

	UpdateCountdown();
}

void CCountdown::SetEndTime( time_t tEndTime )
{
	if ( m_tEndTime == tEndTime )
		return;
	
	m_tEndTime = tEndTime;

	UpdateCountdown();
}

void CCountdown::SetUpdateInterval( float flUpdateInterval )
{
	if ( m_flUpdateInterval == flUpdateInterval )
		return;

	m_flUpdateInterval = flUpdateInterval;

	UpdateCountdown();
}

void CCountdown::SetUpdateEnabled( bool bEnabled )
{
	if( m_bUpdateEnabled != bEnabled )
	{
		m_bUpdateEnabled = bEnabled;
		if( bEnabled )
		{
			UpdateCountdown();
		}
	}
}

void CCountdown::SetTimeDialogVariable( const char *pszTimeDialogVariable )
{
	if ( m_strTimeDialogVariable == pszTimeDialogVariable )
		return;

	m_strTimeDialogVariable = pszTimeDialogVariable;

	UpdateCountdown();
}

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for party chat
//
//=============================================================================//

#include "cbase.h"
#include "csgo_timers.h"
#include "panorama/uijsregistration.h"
#include "csgo_panorama_script_bindings.h"
#include "clientmode_csnormal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_CountdownTimer, CountdownTimer );

using namespace panorama;

CCSGO_CountdownTimer::CCSGO_CountdownTimer( panorama::CPanel2D *pParent, const char* pchID )
	: CPanel2D( pParent, pchID ),
	m_bOutputMilliseconds( false ),
	m_flTime( 0.0 ),
	m_flExpireTime( -1.0 ),
	m_bHasRegisteredFrameUpdates( false ),
	m_ClockType( kClockType_Realtime )
{
	OnTimeChanged();
}

CCSGO_CountdownTimer::~CCSGO_CountdownTimer()
{
	if ( m_bHasRegisteredFrameUpdates )
	{
		UnregisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_CountdownTimer::Update );
		m_bHasRegisteredFrameUpdates = false;
	}
}

bool CCSGO_CountdownTimer::Update( void )
{
	UpdateTime();
	OnTimeChanged();
	return false;
}

void CCSGO_CountdownTimer::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "timeleft", PANORAMA_DELEGATE( &CCSGO_CountdownTimer::GetTime ), PANORAMA_DELEGATE( &CCSGO_CountdownTimer::SetTimeJS ) );
	RegisterJSAccessor( "clocktype", PANORAMA_DELEGATE( &CCSGO_CountdownTimer::GetClockTypeJS ), PANORAMA_DELEGATE( &CCSGO_CountdownTimer::SetClockTypeJS ) );
}

bool CCSGO_CountdownTimer::BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol symTimeLeft = "timeleft";
	static const CPanoramaSymbol symWantsMillisecondResolution = "output_milliseconds";
	static const CPanoramaSymbol symClock = "clock";

	if ( symName == symTimeLeft )
	{
		double dFrameTime = 0.0;
		if ( !CSSHelpers::BParseTime( &dFrameTime, pchValue ) )
			return false;

		SetTime( m_ClockType, float( dFrameTime ) );
		return true;
	}
	else if ( symName == symWantsMillisecondResolution )
	{
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &m_bOutputMilliseconds ) )
			return false;

		return true;
	}
	else if ( symName == symClock )
	{
		return SetClockType( pchValue );
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

void CCSGO_CountdownTimer::SetTime( EClockType clockType, float flSeconds )
{
	if ( flSeconds < 0.0f )
		flSeconds = 0.0f;

	m_ClockType = clockType;
	UpdateTime();
	m_flExpireTime = m_flTime + flSeconds;
	OnTimeChanged();
}

void CCSGO_CountdownTimer::SetTimeJS( float flSeconds )
{
	SetTime( m_ClockType, flSeconds );
}

void CCSGO_CountdownTimer::SetClockType( EClockType clockType )
{
	if ( clockType == m_ClockType )
		return;

	// If we are currently running, we need to re-calculate the expiration time against the new clock
	if ( m_flExpireTime >= 0.0 )
	{
		UpdateTime();
		double flRemainingTime = m_flExpireTime - m_flTime;
		m_ClockType = clockType;
		UpdateTime();
		m_flExpireTime = m_flTime + flRemainingTime;
		OnTimeChanged();	// may need to start/stop the per-frame event
		return;
	}

	m_ClockType = clockType;
	UpdateTime();
}

bool CCSGO_CountdownTimer::SetClockType( const char* szClockType )
{
	// Try to parse as integer
	char* res;
	int32 clockTypeEnum = strtol( szClockType, &res, 10 );
	if ( res != nullptr && res != szClockType )
	{
		// successful integer parse
		// fail if out of range
		if ( clockTypeEnum < 0 || clockTypeEnum >= kClockTypeCount )
			return false;

		SetClockType( ( EClockType )clockTypeEnum );
		return true;
	}

	// Check string names
	EClockType clockType = kClockTypeCount;

	if ( !V_stricmp( szClockType, "none" ) )
		clockType = kClockType_None;
	if ( !V_stricmp( szClockType, "realtime" ) )
		clockType = kClockType_Realtime;
	else if ( !V_stricmp( szClockType, "wall" ) )
		clockType = kClockType_Wall;
	else if ( !V_stricmp( szClockType, "game" ) )
		clockType = kClockType_Game;
	else if ( !V_stricmp( szClockType, "game-tick" ) )
		clockType = kClockType_Game_Tick;
	else if ( !V_stricmp( szClockType, "game-server" ) )
		clockType = kClockType_Game_Tick;

	if ( clockType == kClockTypeCount )
		return false;

	SetClockType( clockType );
	return true;
}

void CCSGO_CountdownTimer::SetClockTypeJS( const char* szClockType )
{
	SetClockType( szClockType ); // ignore return value
}

const char* CCSGO_CountdownTimer::GetClockTypeJS()
{
	switch ( m_ClockType )
	{
	case kClockType_None:
		return "none";
	case kClockType_Realtime:
		return "realtime";
	case kClockType_Wall:
		return "wall";
	case kClockType_Game:
		return "game";
	case kClockType_Game_Tick:
		return "game-tick";
	case kClockType_Game_Server:
		return "game-server";
	}

	return "undefined";
}

void CCSGO_CountdownTimer::OnTimeChanged()
{
	bool bShouldHandleFrameUpdates = false;

	if ( m_flExpireTime <= m_flTime )
	{
		// We're done counting down, zero the dialog vars
		m_flExpireTime = -1.0;

		SetDialogVariable( "duration", ( time_t )0 );
		if ( m_bOutputMilliseconds )
			SetDialogVariable( "milliseconds", "000" );
	}
	else
	{
		uint32 timeLeftMs = uint32( ( m_flExpireTime - m_flTime ) * 1000.0 );
		time_t totalSecs = timeLeftMs / 1000;

		SetDialogVariable( "duration", totalSecs );
		if ( m_bOutputMilliseconds )
			SetDialogVariable( "milliseconds", CFmtStr( "%03lu", ( timeLeftMs % 1000 ) ).String() );

		// If we're running and getting clock types from any internal source, update on every frame
		bShouldHandleFrameUpdates = ( m_ClockType != kClockType_None );
	}

	// Set/Clear the event handler for updating the clock every frame if needed.
	if ( bShouldHandleFrameUpdates && !m_bHasRegisteredFrameUpdates )
	{
		RegisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_CountdownTimer::Update );
		m_bHasRegisteredFrameUpdates = true;
	}
	else if ( !bShouldHandleFrameUpdates && m_bHasRegisteredFrameUpdates )
	{
		UnregisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_CountdownTimer::Update );
		m_bHasRegisteredFrameUpdates = false;
	}
}

void CCSGO_CountdownTimer::UpdateTime()
{
	switch ( m_ClockType )
	{
	case kClockType_None:
		// not updated based on time
		m_flTime = 0.0;
		break;

	case kClockType_Realtime:
		// updated once per frame
		m_flTime = gpGlobals->realtime;
		break;

	case kClockType_Wall:
		// updated instantaneously when this function is called; different timers may have different values
		m_flTime = Plat_FloatTime();
		break;

	case kClockType_Game:
		// updated once per frame, based on current game time including interpolation
		m_flTime = gpGlobals->curtime;
		break;

	case kClockType_Game_Tick:
		// close to curtime, but only updated on tick changes
		m_flTime = gpGlobals->tickcount * gpGlobals->interval_per_tick;
		break;

	case kClockType_Game_Server:
		// close to curtime, but only updated when we receive a new packet from server
		m_flTime = engine->GetServerTick() * gpGlobals->interval_per_tick;
		break;

	default:
		m_flTime = 0.0;
		break;
	}

	// We assume time is always positive;
	// to fix this we would need more logic around whether m_flExpireTime is set or not
	Assert( m_flTime >= 0.0 );
}

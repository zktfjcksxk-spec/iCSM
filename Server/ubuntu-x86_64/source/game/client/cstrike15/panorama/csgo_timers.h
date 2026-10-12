//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Panels which update time related dialog vars for display
//
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"

class CCSGO_CountdownTimer : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_CountdownTimer, panorama::CPanel2D );
public:
	enum EClockType {
		kClockType_None,		// Clock is only updated from an external source via SetTime()
		kClockType_Realtime,	// Realtime but consistent across a render frame (gpGlobals->realtime) (default)
		kClockType_Wall,		// Absolute real time; also used for panorama animations. (Plat_FloatTime())
		kClockType_Game,		// For timers related to game events (gpGlobals->curtime)
								// Note that during UI update this is higher resolution than the server tick amount
		kClockType_Game_Tick,	// Game timer that only updates on tick boundaries
		kClockType_Game_Server,	// Game timer that matches the most recently received server tick (to avoid things like bomb defuse timer hitting 0 due to network latency)

		kClockTypeCount
	};

	CCSGO_CountdownTimer( panorama::CPanel2D *pParent, const char* pchID );
	virtual ~CCSGO_CountdownTimer();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	// Get the number of seconds until the clock expires
	float GetTime( void );

	// Get the clock type
	EClockType GetClockType();
	const char* GetClockTypeJS();

	// Expire any current time
	void Clear();

	// Set an exact expiration time
	void SetExpirationTime( EClockType clockType, double flTime );

	// Set the amount of remaining time until the clock expires
	void SetTime( EClockType clockType, float flSeconds ); 
	void SetTimeJS( float flSeconds );

	// Set the clock type
	void SetClockType( EClockType clockType );
	bool SetClockType( const char* clockType );
	void SetClockTypeJS( const char* clockType );

protected:
	void UpdateTime(); // sets m_flTime based on m_ClockType
	void OnTimeChanged();
	bool Update( void );

	double m_flTime;
	double m_flExpireTime;

	bool m_bOutputMilliseconds;
	bool m_bHasRegisteredFrameUpdates;
	EClockType m_ClockType;
};

inline float CCSGO_CountdownTimer::GetTime()
{
	if ( m_flExpireTime >= 0.0 )
		return float( m_flExpireTime - m_flTime );

	return 0.0f;
}

inline CCSGO_CountdownTimer::EClockType CCSGO_CountdownTimer::GetClockType()
{
	return m_ClockType;
}



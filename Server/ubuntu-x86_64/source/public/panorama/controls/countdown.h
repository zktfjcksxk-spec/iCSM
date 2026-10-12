//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Control that counts down to a particular timestamp
//-----------------------------------------------------------------------------
class CCountdown : public CPanel2D
{
	DECLARE_PANEL2D( CCountdown, CPanel2D );

public:
	CCountdown( CPanel2D *parent, const char * pchPanelID );
	virtual ~CCountdown();

	void SetStartTime( time_t tStartTime );
	time_t GetStartTime() const { return m_tStartTime; }

	void SetEndTime( time_t tEndTime );
	time_t GetEndTime() const { return m_tEndTime; }

	void SetUpdateInterval( float flUpdateInterval );
	float GetUpdateInterval() const { return m_flUpdateInterval; }
	void SetUpdateEnabled( bool bEnabled );

	void SetTimeDialogVariable( const char *pszTimeDialogVariable );
	const char *GetTimeDialogVariable() const { return m_strTimeDialogVariable.Get(); }

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	bool EventReadyForDisplay( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventUpdateCountdown( int nUpdateCount );

	void UpdateCountdown();

	time_t m_tStartTime;
	time_t m_tEndTime;
	float m_flUpdateInterval;
	CUtlString m_strTimeDialogVariable;

	int m_nUpdateCount;
	bool m_bUpdateEnabled;
};


} // namespace panorama

#endif // COUNTDOWN_H

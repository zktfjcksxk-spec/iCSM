//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//


#include "stdafx.h"
#include "panorama/uischeduleddel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// constructor, destructor
#ifdef SCHEDULED_FUNC_HAS_NAME
CUIScheduledDel::CUIScheduledDel( CUtlDelegate< void() > del, const char *pchName )
#else
CUIScheduledDel::CUIScheduledDel( CUtlDelegate< void() > del )
#endif
{
	m_del = del;
#ifdef SCHEDULED_FUNC_HAS_NAME
	m_sName = pchName;
#endif
	m_iScheduledIndex = -1;
	m_flNextScheduled = 0.0f;
}


CUIScheduledDel::~CUIScheduledDel()
{
	Cancel();
}

void CUIScheduledDel::Schedule( double flSecondsToRunIn )
{
	if( m_iScheduledIndex != -1 )
	{
		Cancel();
	}

	double flSecondsToRunInFixed = Max( flSecondsToRunIn, 0.0001 );
	m_flNextScheduled = UIEngine()->GetCurrentFrameTime() + flSecondsToRunInFixed;
#ifdef SCHEDULED_FUNC_HAS_NAME
	m_iScheduledIndex = UIEngine()->RegisterScheduledDelegate( m_flNextScheduled, m_del, m_sName );
#else
	m_iScheduledIndex = UIEngine()->RegisterScheduledDelegate( m_flNextScheduled, m_del, "NoName" );
#endif
}

void CUIScheduledDel::Cancel()
{
	if( m_iScheduledIndex != -1 && m_flNextScheduled > UIEngine()->GetLastScheduledDelegateRunTime() )
	{
		UIEngine()->CancelScheduledDelegate( m_iScheduledIndex );
	}
	m_iScheduledIndex = -1;
}

bool CUIScheduledDel::BScheduled()
{
	return (m_iScheduledIndex != -1 && m_flNextScheduled > UIEngine()->GetLastScheduledDelegateRunTime());
}

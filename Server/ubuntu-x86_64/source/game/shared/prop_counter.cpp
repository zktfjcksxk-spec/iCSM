//====== Copyright © Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//==================================================================

#include "cbase.h"
#include "prop_counter.h"
#include "cs_gamerules.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"
// --------------------------------------------------------------

#ifdef CLIENT_DLL

IMPLEMENT_CLIENTCLASS_DT( C_PropCounter, DT_PropCounter, CPropCounter )
RecvPropFloat( RECVINFO( m_flDisplayValue ) ),
END_RECV_TABLE()

#define PROP_COUNTER_LERP_DURATION 2.0f

float C_PropCounter::GetDisplayValue( void )
{
	float flTimeSinceLastValueChance = clamp( abs( gpGlobals->curtime - m_flTimeOfLastValueChange ), 0.0f, PROP_COUNTER_LERP_DURATION );

	return RemapValClamped( flTimeSinceLastValueChance, 0.0f, PROP_COUNTER_LERP_DURATION, m_flPreviousValue, m_flDisplayValue );
}

void C_PropCounter::OnPreDataChanged( DataUpdateType_t updateType )
{
	m_flDisplayValueLocal = m_flDisplayValue;
	BaseClass::OnPreDataChanged( updateType );
}

void C_PropCounter::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flPreviousValue = m_flDisplayValue;
		return;
	}

	if ( m_flDisplayValueLocal != m_flDisplayValue )
	{
		m_flPreviousValue = m_flDisplayValueLocal;
		m_flDisplayValueLocal = m_flDisplayValue;
		m_flTimeOfLastValueChange = gpGlobals->curtime;
	}
}

#else

LINK_ENTITY_TO_CLASS( prop_counter, CPropCounter );

BEGIN_DATADESC( CPropCounter )
DEFINE_KEYFIELD( m_nInitialValue, FIELD_INTEGER, "initialValue" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPropCounter, DT_PropCounter )
SendPropFloat( SENDINFO( m_flDisplayValue ) ),
END_SEND_TABLE()

#define PROP_COUNTER_MODEL "models/props_survival/counter/counter_a.mdl"

CPropCounter::CPropCounter()
{
}

void CPropCounter::Precache()
{
	PrecacheModel( PROP_COUNTER_MODEL );

	SetModelName( MAKE_STRING( PROP_COUNTER_MODEL ) );

	BaseClass::Precache();
}

void CPropCounter::Spawn()
{
	BaseClass::Spawn();
	Precache();

	m_flDisplayValue = (float)m_nInitialValue;

	SetModel( PROP_COUNTER_MODEL );
}

#endif

//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "point_worldtext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(point_worldtext, CPointWorldText);

BEGIN_DATADESC( CPointWorldText )
	DEFINE_FIELD( m_szText, FIELD_STRING ),
	DEFINE_KEYFIELD( m_flTextSize,	FIELD_FLOAT, "textsize" ),
	DEFINE_KEYFIELD( m_textColor, FIELD_COLOR32, "color" ), 
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPointWorldText, DT_PointWorldText )
	SendPropString(SENDINFO(m_szText)),
	SendPropInt(SENDINFO(m_textColor), 32, SPROP_UNSIGNED, SendProxy_Color32ToInt32 ),
	SendPropFloat(SENDINFO(m_flTextSize)),
END_SEND_TABLE()

CPointWorldText::CPointWorldText() : 
	CBaseEntity()
{
	V_memset( m_szText.GetForModify(), 0, sizeof( m_szText ) );
	color32 tmp = { 255, 255, 255, 255 };
	m_textColor = tmp;
	m_flTextSize = 10.0f;
}

CPointWorldText::~CPointWorldText()
{
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CPointWorldText::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


bool CPointWorldText::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "message" ) )
	{
		V_strncpy( m_szText.GetForModify(), szValue, sizeof(m_szText) );
		return true;
	}
	return BaseClass::KeyValue( szKeyName, szValue );
}

void CPointWorldText::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	BaseClass::Spawn();
}

//========= Copyright Valve Corporation, All rights reserved. ============//

#pragma once

//------------------------------------------------------------------------------
class CPointWorldText: public CBaseEntity
{
public:
	DECLARE_CLASS( CPointWorldText, CBaseEntity );

	CPointWorldText();
	virtual ~CPointWorldText();

	virtual void Spawn( void ) OVERRIDE;

	virtual bool KeyValue( const char *szKeyName, const char *szValue ) OVERRIDE;
	virtual int  UpdateTransmitState() OVERRIDE;

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	CNetworkString( m_szText, MAX_PATH );
	CNetworkVar( float, m_flTextSize );
	CNetworkColor32( m_textColor );
};

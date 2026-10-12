//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "functionproxy.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Returns the player health (from 0 to 1)
//-----------------------------------------------------------------------------
class CProxyHealth : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
	bool m_bUseThreshold;
	CFloatInput	m_Threshold;
};

bool CProxyHealth::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	m_Threshold.Init( pMaterial, pKeyValues, "threshold", 0 );
	m_bUseThreshold = m_Threshold.GetFloat() > 0;

	return true;
}

void CProxyHealth::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	if ( !pEntity )
		return;

	Assert( m_pResult );

	if ( m_bUseThreshold )
	{
		if ( pEntity->HealthFraction() * m_Factor.GetFloat() > m_Threshold.GetFloat() )
		{
			SetFloatResult( 0.0f );
		}
		else
		{
			SetFloatResult( 1.0f );
		}
		return;
	}

	SetFloatResult( pEntity->HealthFraction() * m_Factor.GetFloat() );
}

EXPOSE_MATERIAL_PROXY( CProxyHealth, Health );



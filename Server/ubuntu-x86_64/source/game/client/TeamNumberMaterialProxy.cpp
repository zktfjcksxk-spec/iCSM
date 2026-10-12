//========= Copyright © 2017, Valve Corporation, All rights reserved. ============//
//
// Purpose: A material proxy that outputs the team number of the attached entity
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "functionproxy.h"
#include "imaterialproxydict.h"

class CTeamNumberMaterialProxy : public CResultProxy
{
public:
	virtual void OnBind( void *pC_BaseEntity ) OVERRIDE;
};

void CTeamNumberMaterialProxy::OnBind( void *pC_BaseEntity )
{
	Assert( pC_BaseEntity );
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	int teamNumber = pEntity->GetTeamNumber();

	SetFloatResult( (float)teamNumber );
}

EXPOSE_MATERIAL_PROXY( CTeamNumberMaterialProxy, GetTeamNumber );


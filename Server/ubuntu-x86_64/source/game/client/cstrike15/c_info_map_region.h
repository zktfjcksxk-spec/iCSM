//============ Copyright © Valve Corporation, All rights reserved. ============//
//
// Point entity with a radius to name regions of a map 
//
//=============================================================================//
#pragma once

#include "c_baseentity.h"

class C_InfoMapRegion : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_InfoMapRegion, CBaseEntity );
	DECLARE_CLIENTCLASS();

	C_InfoMapRegion();
	virtual ~C_InfoMapRegion();

	enum { k_eMaxLocTokenLen = 128 };
	float m_flRadius;
	char m_szLocToken[ k_eMaxLocTokenLen ];
	C_InfoMapRegion	*m_pNext;
};

C_InfoMapRegion* GetRegionNameList();
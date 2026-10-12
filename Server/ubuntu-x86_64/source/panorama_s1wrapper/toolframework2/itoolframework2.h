#ifndef INCLUDED_ITOOLFRAMEWORK2_H
#define INCLUDED_ITOOLFRAMEWORK2_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "assetsystem/iassetsystem.h"

typedef int EngineToolID_t;

#define ENGINE_TOOL_INVALID 0xFF

abstract_class IToolFramework2
{
public:
	virtual void OpenInPrimaryTool( const CUtlVector<IAsset *> &assetFiles, EngineToolID_t nOverrideEngineToolID = ENGINE_TOOL_INVALID ) = 0;
};

extern IToolFramework2 *g_pToolFramework2;


#endif // INCLUDED_ITOOLFRAMEWORK2_H
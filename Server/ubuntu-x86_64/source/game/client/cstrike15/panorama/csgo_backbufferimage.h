//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_BACKBUFFERIMAGE_H_
#define CSGO_BACKBUFFERIMAGE_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/controls/source2/renderpanel.h"


class CCSGO_BackBufferImageRenderer : public panorama::CRenderThreadCallback
{
public:

	CCSGO_BackBufferImageRenderer();
	virtual ~CCSGO_BackBufferImageRenderer();

	virtual void RenderThreadCallback( Vector4D *pScissorRect, float x0, float y0, float x1, float y1, bool bEnableSSAA ) OVERRIDE;

private:

	CMaterialReference  m_ScreenSpaceMaterial;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGO_BackbufferImagePanel : public panorama::CRenderPanel
{
	DECLARE_PANEL2D( CCSGO_BackbufferImagePanel, panorama::CRenderPanel );

public:

	CCSGO_BackbufferImagePanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_BackbufferImagePanel();

private:

	CRefPtr< CCSGO_BackBufferImageRenderer > m_pRenderer;
};

#endif	// CSGO_BACKBUFFERIMAGE_H_
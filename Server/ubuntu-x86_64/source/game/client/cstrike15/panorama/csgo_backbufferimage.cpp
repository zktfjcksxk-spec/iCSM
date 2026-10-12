//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "csgo_backbufferimage.h"

#include "view_scene.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_BackbufferImagePanel, CSGOBackbufferImagePanel )


//-----------------------------------------------------------------------------
//
//	CCSGO_BackbufferImagePanel Methods
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CCSGO_BackbufferImagePanel::CCSGO_BackbufferImagePanel( panorama::CPanel2D *pParent, const char *pchID )
:
	CRenderPanel( pParent, pchID ),
	m_pRenderer( new CCSGO_BackBufferImageRenderer() )
{
	SetRenderThreadCallback( m_pRenderer );
}


//-----------------------------------------------------------------------------
CCSGO_BackbufferImagePanel::~CCSGO_BackbufferImagePanel()
{}


//-----------------------------------------------------------------------------
//
//	CCSGO_BackBufferImageRenderer Methods
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CCSGO_BackBufferImageRenderer::CCSGO_BackBufferImageRenderer()
{
	KeyValues *pVMTKeyValues = new KeyValues( "screenspace_general" );
	pVMTKeyValues->SetString( "$PIXSHADER", "unlitgeneric_ps20" );
	pVMTKeyValues->SetString( "$basetexture", "_rt_FullFrameFB" );
	pVMTKeyValues->SetInt( "$COPYALPHA", 1 );
	m_ScreenSpaceMaterial.Init( "PanoramaBackBufferScreenSpace", TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_ScreenSpaceMaterial->Refresh();
}


//-----------------------------------------------------------------------------
CCSGO_BackBufferImageRenderer::~CCSGO_BackBufferImageRenderer()
{
	m_ScreenSpaceMaterial.Shutdown( true );
}


//-----------------------------------------------------------------------------
void CCSGO_BackBufferImageRenderer::RenderThreadCallback( Vector4D *pScissorRect, float x0, float y0, float x1, float y1, bool bEnableSSAA )
{
	CMatRenderContextPtr pRenderContext( materials );

	int nLeft = RoundFloatToInt( x0 );
	int nRight = RoundFloatToInt( x1 );
	nLeft = Max( 0, nLeft );
	nRight = Max( nLeft, nRight );

	int nTop = RoundFloatToInt( y0 );
	int nBottom = RoundFloatToInt( y1 );
	nTop = Max( 0, nTop );
	nBottom = Max( nTop, nBottom );

	int nRenderWidth = nRight - nLeft;
	int nRenderHeight = nBottom - nTop;
	
	//
	// Copy back buffer to "_rt_FullFrameFB"
	//

	ITexture *pSaveRenderTarget = pRenderContext->GetRenderTarget();
	pRenderContext->SetRenderTarget( NULL );	// set to the back buffer

	Rect_t actualRect;
	UpdateScreenEffectTexture( 0, nLeft, nTop, nRenderWidth, nRenderHeight, false, &actualRect );
	ITexture *pRtFullFrame = GetFullFrameFrameBufferTexture(0);

	pRenderContext->SetRenderTarget( pSaveRenderTarget );	
	

	//
	// Draw "_rt_FullFrameFB" to the active render target using screenspace_general shader
	//

	pRenderContext->Viewport( nLeft, nTop, nRenderWidth, nRenderHeight );
	pRenderContext->PushScissorRect( pScissorRect->x, pScissorRect->y, pScissorRect->x + pScissorRect->z, pScissorRect->y + pScissorRect->w );

	// Ensure alpha is set to 0xff - screenspace_general is not writing to alpha
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->ClearBuffers( true, false );

	pRenderContext->DrawScreenSpaceRectangle( m_ScreenSpaceMaterial, nLeft, nTop, nRenderWidth, nRenderHeight,
		actualRect.x, actualRect.y, actualRect.x + actualRect.width - 1, actualRect.y + actualRect.height - 1,
		pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight() );

	pRenderContext->PopScissorRect();
}
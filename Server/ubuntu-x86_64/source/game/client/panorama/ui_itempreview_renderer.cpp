//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"

#include "panorama/ui_itempreview_renderer.h"
#include "animation.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "imaterialproxydict.h"
#include "materialsystem/ivisualsdataprocessor.h"
#include "materialsystem/icompositetexture.h"

#include "matsys_controls/matsyscontrols.h"
#include "istudiorender.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier2/renderutils.h"

#include "materialsystem/imesh.h"

#include "shaderapi/ishaderapi.h"
#include "view_shared.h"
#include "ivrenderview.h"
#include "game/client/irendercaptureconfiguration.h"
#include "viewpostprocess.h"
#include "bone_setup.h"
#include "renderparm.h"

#include "particles_ez.h"
#include "IGameUIFuncs.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static ConVar panorama_3dpanel_ssaa( "panorama_3dpanel_ssaa", "1", FCVAR_DEVELOPMENTONLY, "0-off, 1-use ssaa if possible, will add (or fallback to) fxaa if scratch rt not large enough for 1.25x supersmapling in both dimensions" );
static ConVar panorama_3dpanel_fxaa( "panorama_3dpanel_fxaa", "0", FCVAR_DEVELOPMENTONLY, "0-off, 1-always use fxaa (not just as a fallback if scratch rt not large enough for SSAA)" );
static ConVar panorama_3dpanel_ssaa_scale( "panorama_3dpanel_ssaa_scale", "2.0", FCVAR_DEVELOPMENTONLY, "Scaling of panel size for ssaa" );
static ConVar panorama_3dpanel_ssaa_min_scale( "panorama_3dpanel_ssaa_min_scale", "1.5", FCVAR_DEVELOPMENTONLY, "min scaling of panel size for ssaa to be effective, otherwise resorts to fxaa" );

static ConVar panorama_3dpanel_fxaa_subpixel_Q( "panorama_3dpanel_fxaa_subpixel_Q", "0.75", 0, "Effects sub-pixel AA quality and inversely sharpness (only used on FXAA Quality): (0.0 - off), (1.0 - upper limit, softer), default = 0.75" );
static ConVar panorama_3dpanel_fxaa_edge_threshold_Q( "panorama_3dpanel_fxaa_edge_threshold_Q", ".166", 0, "The minimum amount of local contrast required to apply algorithm: (0.063 - overkill, slower), (0.125 - high quality), (0.166 - default), (0.250 - low quality), (0.333 - too little, faster)" );
static ConVar panorama_3dpanel_fxaa_edge_threshold_min_Q( "panorama_3dpanel_fxaa_edge_threshold_min_Q", "0.0", 0, "Trims the algorithm from processing darks: (0.0312 - visible limit, slower), (0.0625 - high quality, faster), (0.0833 - upper limit, the start of visible unfiltered edges). Special note: when using FXAA_GREEN_AS_LUMA, likely want to set this to zero" );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#define MAX_PANEL_PARTICLE_SYSTEMS 8
#define ITEMPREVIEW_SS_RT_NAME		"_rt_FullFrameFB2"
#define ITEMPREVIEW_SS_DEPTH_NAME	"_rt_FullFrameDepth"

#define FLASHLIGHT_MDL "models/editor/spot.mdl"
#define DIRLIGHT_MDL "models/tools/bullet_hit_marker.mdl"
#define AXIS_MDL "models/tools/translate_widget.mdl"
#define PIVOT_MDL "models/editor/axis_helper.mdl"
#define CAMERA_MDL "models/editor/camera.mdl"
#define FLOOR_MDL "models/tools/green_plane/green_plane.mdl"


//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

static float UI_GetAutoPlayTime( void )
{
	static int g_prevTicks = 0;
	static float g_time = 0.0f;

	int ticks = Plat_MSTime();

	// limit delta so that float time doesn't overflow
	if ( g_prevTicks == 0 )
	{
		g_prevTicks = ticks;
	}

	g_time += (ticks - g_prevTicks) / 1000.0f;
	g_prevTicks = ticks;

	return g_time;
}



//-----------------------------------------------------------------------------
// test callback
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::RenderThreadCallback( Vector4D *pScissorAttribute, float x0, float y0, float x1, float y1, bool bEnableAA )
{
	VPROF_BUDGET_THREAD( "CUI_ItemPreviewRenderer::RenderThreadCallback", VPROF_BUDGETGROUP_TENFOOT );

	//Msg("CUI_ItemPreviewRenderer::RenderThreadCallback\n");

 	if ( !IsRenderingEnabled() )
 		return;

	CMatRenderContextPtr pRenderContext( materials );

	if ( IsRenderingInventoryItem() )
	{
		// FIXME - horrible workaround for inventory panels
		// issue is icon is rendered at 512x512, 
		// but only the top 512x384 part is saved as png - the 3d window clips the weapon if rendered at the same or similar aspect
		// force a square aspect here, assuming that the post ssaa 'resolve' will copy into the correct part of the panel surface.
		y1 = x1;
	}

	if ( GetRenderingWithFlashlightConfiguration() )
	{
		// flashlight shadow
		RenderCapture();
	}

	if ( ( !g_pMaterialSystemHardwareConfig->IsAAEnabled() ) && ( bEnableAA == false ) )
		bEnableAA = true;

	if ( gameuifuncs->IsPanoramaInECOMode() )
		bEnableAA = false;

	SetRenderWithAA( bEnableAA && ( panorama_3dpanel_ssaa.GetBool() || panorama_3dpanel_fxaa.GetBool() ) );

	Begin3DPaint( (int)x0, (int)y0, int(x1), int(y1), pScissorAttribute );

	OnPaint3D();

	End3DPaint( true );

	SetNeedsRedraw( !m_bAnimationPaused );
}

CUI_ItemPreviewRenderer::CUI_ItemPreviewRenderer()
{
	m_bEnableRendering = true;

	m_pShadowFloorMaterial = NULL;

	m_bWireFrame = false;
	m_bLockView = false;

	// ensure anims/particles don't advance time until the panel is visible (1st render callback made).
	m_bTimeStarted = false;

	m_bAnimationPaused = false;
	m_bNeedsRedraw = true;

	m_pCameraLookAtItem = NULL;

	m_iRotateMDLIndex = -1;

	m_iItemAttachment = -1;
	m_iCameraAttachment = -1;
	m_iRenderCaptureCameraAttachment = -1;
	Q_memset( m_iDirectionalLightAttachments, ~0, sizeof( m_iDirectionalLightAttachments ) );  

	m_flAutoPlayTimeBase = UI_GetAutoPlayTime();

	m_bDrawDebugHelpers = true;
	m_bDrawShadowFloor = false;
	m_bDrawWorldPivot = false;
	m_bDrawFlashlightPivot = false;
	m_bDrawPresetCameras = false;
	m_nDrawDebugDirectionalLightIdx = -1;

	m_bCameraManipulateEnabled = false;
	m_bCameraOrientOverrideEnabled = false;
	m_bCameraPositionOverrideEnabled = false;
	m_vecCameraOrientOverride.Init();
	m_vecCameraPositionOverride.Init();

	m_bHasLightProbe = false;
	m_bSetupRenderStateDelayed = false;
	m_bRenderWithAA = false;
	m_bInRender3dForRenderCapture = false;

	m_bInventoryItem = false;

	m_pvRenderingWithFlashlightConfiguration = NULL;

	m_ClearColor.SetColor( 76, 88, 68, 255 );
	m_GridColor.SetColor( 255, 255, 255, 255 );

	SetIdentityMatrix( m_CameraPivot );
	m_CameraPivotAngles = vec3_angle;

	m_FlashlightPivotPos = vec3_origin;

	InitDefaultLights();

	m_vecCameraOffset.Init( 100.0f, 0.0f, 100.0f );

	//m_Camera.Init( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 3.0f, 16384.0f * 1.73205080757f, 30.0f, 1.0f );
	//m_RenderCaptureCamera.Init( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 3.0f, 16384.0f * 1.73205080757f, 30.0f, 1.0f );

	m_Camera.Init( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 1.0f, 16384.0f * 1.73205080757f, 30.0f, 1.0f );
	m_RenderCaptureCamera.Init( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 3.0f, 16384.0f * 1.73205080757f, 30.0f, 1.0f );

	UpdateCameraTransform();

	m_aCameraPresets.Purge();
	ResetCameraPreset();

	m_bIn3DPaintMode = false;

	SetCameraManipulateAllowed( false );

	m_SceneMergedMDL.SetParentRenderer( this );

	InitParticleSystems();

	// Super sampling
	m_FullScreenBufferMaterial.Shutdown();
	m_FullScreenBuffer.Shutdown();

	// Set up a material for final SSAA 'resolve' to panorama panel RT
	char pTemp[ 512 ];
	KeyValues *pVMTKeyValuesSSAA = new KeyValues( "PanoramaSSAAResolve" );
	pVMTKeyValuesSSAA->SetString( "$basetexture", ITEMPREVIEW_SS_RT_NAME );
	Q_snprintf( pTemp, sizeof( pTemp ), "UI_ITEMPREVIEW_FullScreen_%s", ITEMPREVIEW_SS_RT_NAME );
	m_FullScreenBufferMaterial.Init( pTemp, TEXTURE_GROUP_OTHER, pVMTKeyValuesSSAA );

	m_FullScreenBufferMaterial->RefreshPreservingMaterialVars();

	// FXAA
	m_FXAAMaterial.Shutdown();

	// Set up material for FXAA 'resolve' to panorama panel RT, if SSAA is enabled at the same time, use this material to perform both
	KeyValues *pVMTKeyValuesFXAA = new KeyValues( "engine_post" );
	pVMTKeyValuesFXAA->SetString( "$basetexture", "_rt_SmallFB0" ); // for bloom, not used here
	pVMTKeyValuesFXAA->SetString( "$fbtexture", ITEMPREVIEW_SS_RT_NAME );
	pVMTKeyValuesFXAA->SetString( "$BloomEnable", "0" );
	pVMTKeyValuesFXAA->SetString( "$AllowVignette", "0" );
	pVMTKeyValuesFXAA->SetString( "$VignetteEnable", "0" );
	pVMTKeyValuesFXAA->SetString( "$BlurredVignetteEnable", "0" );
	pVMTKeyValuesFXAA->SetString( "$AllowNoise", "0" );
	pVMTKeyValuesFXAA->SetString( "$NoiseEnable", "0" );
	pVMTKeyValuesFXAA->SetString( "$VomitEnable", "0" );
	pVMTKeyValuesFXAA->SetString( "$Fade", "0" );
	pVMTKeyValuesFXAA->SetString( "$DesaturateEnable", "0" );
	pVMTKeyValuesFXAA->SetString( "$AllowLocalContrast", "0" );
	pVMTKeyValuesFXAA->SetString( "$LocalContrastEnable", "0" );
	pVMTKeyValuesFXAA->SetString( "$AAEnable", "1" );
	pVMTKeyValuesFXAA->SetString( "$additive", "0" );
	pVMTKeyValuesFXAA->SetString( "$ignorez", "1" );
	pVMTKeyValuesFXAA->SetString( "$toolmode", "0" );
	pVMTKeyValuesFXAA->SetString( "$panorama", "1" );
	Q_snprintf( pTemp, sizeof( pTemp ), "UI_ITEMPREVIEW_fxaa" );
	m_FXAAMaterial.Init( pTemp, TEXTURE_GROUP_OTHER, pVMTKeyValuesFXAA );
	m_FXAAMaterial->RefreshPreservingMaterialVars();

	m_bInAARendering = false;
	m_bForceFXAA = false;

	// initialise debug mdls
	InitDebugMDLs();
}

CUI_ItemPreviewRenderer::~CUI_ItemPreviewRenderer()
{
	m_SceneMergedMDL.m_aMergedItemData.Purge();

	m_aSceneParticleSystems.Purge();

	DestroyLights();

	m_FullScreenBufferMaterial.Shutdown();
	m_FullScreenBuffer.Shutdown();

	m_FXAAMaterial.Shutdown();
}

void CUI_ItemPreviewRenderer::EnableRenderingWithFlashlight( void *pvConfiguration )
{
	m_pvRenderingWithFlashlightConfiguration = pvConfiguration;
}

void CUI_ItemPreviewRenderer::RenderCapture()
{
	if ( !GetRenderingWithFlashlightConfiguration() )
		return;

	CRenderCaptureConfigurationState *pCfg = reinterpret_cast<CRenderCaptureConfigurationState *>(GetRenderingWithFlashlightConfiguration());

	// camera
	Vector vecPos;
	QAngle angDir;

	if ( IsRenderCaptureCameraPositionOverrideEnabled() )
	{
		vecPos = *GetRenderCaptureCameraPositionOverride( );

		if ( IsRenderCaptureCameraOrientOverrideEnabled() )
		{
			angDir = *GetRenderCaptureCameraOrientOverride();
		}

		SetRenderCaptureCameraPositionAndAngles( vecPos, angDir );
	}

	m_nRenderWidth = pCfg->m_pFlashlightDepthTexture->GetActualWidth();
	m_nRenderHeight = pCfg->m_pFlashlightDepthTexture->GetActualHeight();
	
	m_flClipLeft = -1.0f;
	m_flClipRight = 1.0f;
	m_flClipBottom = -1.0f;
	m_flClipTop = 1.0f;

	if ( !m_bSetupRenderStateDelayed )
		SetupRenderState( m_nRenderWidth, m_nRenderHeight );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CUI_ItemPreviewRenderer::RenderCapture" );

	CViewSetup view2d;
	view2d.x = 0;
	view2d.y = 0;
	view2d.width = m_nRenderWidth;
	view2d.height = m_nRenderHeight;
	pCfg->m_pIVRenderView->Push3DView( pRenderContext, view2d, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH, pCfg->m_pDummyColorBufferTexture, NULL, pCfg->m_pFlashlightDepthTexture );

	pRenderContext->PushScissorRect( 0, 0, m_nRenderWidth, m_nRenderHeight );

	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
	pRenderContext->ClearBuffers( true, true, false );

	m_bInRender3dForRenderCapture = true;
	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );
	g_pStudioRender->ForcedMaterialOverride( NULL, OVERRIDE_DEPTH_WRITE );

	// set depth bias factors 
	//	pRenderContext->SetShadowDepthBiasFactors( pCfg->m_renderFlashlightState.m_flShadowSlopeScaleDepthBias, pCfg->m_renderFlashlightState.m_flShadowDepthBias );
	extern ConVar panorama_3dpanel_shadowslopescaledepthbias;
	extern ConVar panorama_3dpanel_shadowdepthbias;
	// taking bias factors from convars for now instead of out of flashlight state, otherwise we have to re-create the panel or reload the manifest to tweak and see the change
	pRenderContext->SetShadowDepthBiasFactors( panorama_3dpanel_shadowslopescaledepthbias.GetFloat(), panorama_3dpanel_shadowdepthbias.GetFloat() );

	// Don't draw the 3D scene w/ stencil
	ShaderStencilState_t state;
	pRenderContext->SetStencilState( state );

	OnPaint3D();

	g_pStudioRender->ForcedMaterialOverride( NULL );
	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
	m_bInRender3dForRenderCapture = false;

	pRenderContext->PopScissorRect();

	pCfg->m_pIVRenderView->PopView( pRenderContext, NULL );
	pRenderContext->Flush();
}

//-----------------------------------------------------------------------------
// FIXME: This should be moved into studiorender
//-----------------------------------------------------------------------------
static ConVar	r_showenvcubemap( "r_showenvcubemap", "0", FCVAR_CHEAT );
static ConVar	r_eyegloss( "r_eyegloss", "1", FCVAR_ARCHIVE ); // wet eyes
static ConVar	r_eyemove( "r_eyemove", "1", FCVAR_ARCHIVE ); // look around
static ConVar	r_eyeshift_x( "r_eyeshift_x", "0", FCVAR_ARCHIVE ); // eye X position
static ConVar	r_eyeshift_y( "r_eyeshift_y", "0", FCVAR_ARCHIVE ); // eye Y position
static ConVar	r_eyeshift_z( "r_eyeshift_z", "0", FCVAR_ARCHIVE ); // eye Z position
static ConVar	r_eyesize( "r_eyesize", "0", FCVAR_ARCHIVE ); // adjustment to iris textures
static ConVar	mat_softwareskin( "mat_softwareskin", "0", FCVAR_CHEAT );
static ConVar	r_nohw( "r_nohw", "0", FCVAR_CHEAT );
static ConVar	r_nosw( "r_nosw", "0", FCVAR_CHEAT );
static ConVar	r_teeth( "r_teeth", "1" );
static ConVar	r_drawentities( "r_drawentities", "1", FCVAR_CHEAT );
static ConVar	r_flex( "r_flex", "1" );
static ConVar	r_eyes( "r_eyes", "1" );
static ConVar	r_skin( "r_skin", "0", FCVAR_CHEAT );
static ConVar	r_maxmodeldecal( "r_maxmodeldecal", "50" );
static ConVar	r_modelwireframedecal( "r_modelwireframedecal", "0", FCVAR_CHEAT );
static ConVar	mat_normals( "mat_normals", "0", FCVAR_CHEAT );
static ConVar	r_eyeglintlodpixels( "r_eyeglintlodpixels", "0" );
static ConVar	r_rootlod( "r_rootlod", "0" );

static StudioRenderConfig_t s_StudioRenderConfig;

void CUI_ItemPreviewRenderer::UpdateStudioRenderConfig()
{
	memset( &s_StudioRenderConfig, 0, sizeof( s_StudioRenderConfig ) );

	s_StudioRenderConfig.bEyeMove = !!r_eyemove.GetInt();
	s_StudioRenderConfig.fEyeShiftX = r_eyeshift_x.GetFloat();
	s_StudioRenderConfig.fEyeShiftY = r_eyeshift_y.GetFloat();
	s_StudioRenderConfig.fEyeShiftZ = r_eyeshift_z.GetFloat();
	s_StudioRenderConfig.fEyeSize = r_eyesize.GetFloat();
	if ( mat_softwareskin.GetInt() || m_bWireFrame )
	{
		s_StudioRenderConfig.bSoftwareSkin = true;
	}
	else
	{
		s_StudioRenderConfig.bSoftwareSkin = false;
	}
	s_StudioRenderConfig.bNoHardware = !!r_nohw.GetInt();
	s_StudioRenderConfig.bNoSoftware = !!r_nosw.GetInt();
	s_StudioRenderConfig.bTeeth = !!r_teeth.GetInt();
	s_StudioRenderConfig.drawEntities = r_drawentities.GetInt();
	s_StudioRenderConfig.bFlex = !!r_flex.GetInt();
	s_StudioRenderConfig.bEyes = !!r_eyes.GetInt();
	s_StudioRenderConfig.bWireframe = m_bWireFrame;
	s_StudioRenderConfig.bDrawNormals = mat_normals.GetBool();
	s_StudioRenderConfig.skin = r_skin.GetInt();
	s_StudioRenderConfig.maxDecalsPerModel = r_maxmodeldecal.GetInt();
	s_StudioRenderConfig.bWireframeDecals = r_modelwireframedecal.GetInt() != 0;

	s_StudioRenderConfig.fullbright = false;
	s_StudioRenderConfig.bSoftwareLighting = false;

	s_StudioRenderConfig.bShowEnvCubemapOnly = r_showenvcubemap.GetBool();
	s_StudioRenderConfig.fEyeGlintPixelWidthLODThreshold = r_eyeglintlodpixels.GetFloat();

	g_pStudioRender->UpdateConfig( s_StudioRenderConfig );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::BeginAARendering()
{
	m_bInAARendering = false;

	if ( !m_bRenderWithAA ) // TODO - remov, should this always be true now?
		return;

	// TODO = only perform super sampling if we are not rendering into the main RT (which will likely have MSAA anyway)

	// For super sampling use the off-screen render target the material system allocates
	// NOTE: We have to grab it here, as opposed to during init,
	// because the mode hasn't been set by now.
	if ( !m_FullScreenBuffer )
	{
		m_FullScreenBuffer.Init( materials->FindTexture( ITEMPREVIEW_SS_RT_NAME, "render targets" ) );
	}

	m_bForceFXAA = false;

	// see if we have enough extra resolution to make super sampling worthwhile (from 1.25x up to 2x)
	if ( panorama_3dpanel_ssaa.GetBool() )
	{
		int nMaxSSWidth = m_FullScreenBuffer->GetActualWidth();
		int nMaxSSHeight = m_FullScreenBuffer->GetActualHeight();

		// is SSAA worthwhile for this panel size?
		int nMinSSWidth = (int)( (float)m_nRenderWidth * panorama_3dpanel_ssaa_min_scale.GetFloat() );
		int nMinSSHeight = (int)( (float)m_nRenderHeight * panorama_3dpanel_ssaa_min_scale.GetFloat() );

		if ( ( nMinSSWidth > nMaxSSWidth ) || ( nMinSSHeight > nMaxSSHeight ) )
		{
			// add fxaa if RT not large enough for SSAA
			m_bForceFXAA = true;
		}
		else
		{
			// clamp ssaa resolution
			m_nRenderWidth = MIN( panorama_3dpanel_ssaa_scale.GetFloat() * m_nRenderWidth, nMaxSSWidth );
			m_nRenderHeight = MIN( panorama_3dpanel_ssaa_scale.GetFloat() * m_nRenderHeight, nMaxSSHeight );
		}
	}

	CMatRenderContextPtr pRenderContext( materials );

	if ( IsRenderingInventoryItem() )
	{
		// see hack regarding inventory icon rendering, fix up aspect here
		float flAspect = (float)( m_nRenderWidth ) / (float)( m_nRenderHeight );
		SetCameraAspect( flAspect );
	}

	pRenderContext->PushRenderTargetAndViewport( m_FullScreenBuffer,
												 0, 0, m_nRenderWidth, m_nRenderHeight );
	pRenderContext->PushScissorRect( 0, 0, m_nRenderWidth, m_nRenderHeight );

	pRenderContext->ClearBuffers( true, true, true );

	m_bInAARendering = true;
}

void CUI_ItemPreviewRenderer::EndAARendering()
{
	if ( !m_bRenderWithAA )
		return;

	if ( !m_bInAARendering )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Flush();

	// reset for rendering SS RT back into panel RT
	pRenderContext->PopRenderTargetAndViewport();
	pRenderContext->PopScissorRect();

	int nSSWidth = m_FullScreenBuffer->GetActualWidth();
	int nSSHeight = m_FullScreenBuffer->GetActualHeight();

	float fSSU = (float)( m_nRenderWidth - 1 );
	float fSSV = (float)( m_nRenderHeight - 1 );

	m_nRenderWidth = m_n3DRight - m_n3DLeft;
	m_nRenderHeight = m_n3DBottom - m_n3DTop;

	pRenderContext->PushScissorRect( m_vecScissorAttribute.x, m_vecScissorAttribute.y, m_vecScissorAttribute.x + m_vecScissorAttribute.z, m_vecScissorAttribute.y + m_vecScissorAttribute.w );
	pRenderContext->Viewport( m_n3DLeft, m_n3DTop,
							  m_nRenderWidth, m_nRenderHeight );

	if ( IsRenderingInventoryItem() )
	{
		// see hack regarding inventory icon rendering, set appropriate tex coords for 'resolve' here
		fSSV *= m_vecScissorAttribute.w / m_vecScissorAttribute.z;
	}

	if ( panorama_3dpanel_fxaa.GetBool() || m_bForceFXAA )
	{
		bool bFoundVar = false;
		IMaterialVar *pMaterialParam_FXAAValuesQ = m_FXAAMaterial->FindVar( "$FXAAInternalQ", &bFoundVar, false );
		IMaterialVar *pMaterialParam_FXAAValues1 = m_FXAAMaterial->FindVar( "$AAInternal1", &bFoundVar, false );
		IMaterialVar *pMaterialParam_FXAAValues2 = m_FXAAMaterial->FindVar( "$AAInternal2", &bFoundVar, false );


		float vFXAAValues[ 4 ];
		vFXAAValues[ 0 ] = panorama_3dpanel_fxaa_subpixel_Q.GetFloat();
		vFXAAValues[ 1 ] = 0.0f; // unused
		vFXAAValues[ 2 ] = panorama_3dpanel_fxaa_edge_threshold_Q.GetFloat();
		vFXAAValues[ 3 ] = panorama_3dpanel_fxaa_edge_threshold_min_Q.GetFloat(); // unused?

		if ( pMaterialParam_FXAAValuesQ )
			pMaterialParam_FXAAValuesQ->SetVecValue( vFXAAValues, 4 );

		float vFXAAValues1[ 4 ] = { 1.0f, 0.0f, 0.0f, 0.0f };
		if ( pMaterialParam_FXAAValues1 )
			pMaterialParam_FXAAValues1->SetVecValue( vFXAAValues1, 4 );

		float vFXAAValues2[ 4 ] = { 0.0f, 0.0f, (float)m_nRenderWidth / (float)nSSWidth, (float)m_nRenderHeight / (float)nSSHeight };
		if ( pMaterialParam_FXAAValues2 )
			pMaterialParam_FXAAValues2->SetVecValue( vFXAAValues2, 4 );

		pRenderContext->EnableColorCorrection( false );

		pRenderContext->DrawScreenSpaceRectangle( m_FXAAMaterial,
												  0, 0, m_nRenderWidth, m_nRenderHeight,
												  0.0f, 0.0f, fSSU, fSSV,
												  m_nRenderWidth, m_nRenderHeight, NULL );
	}
	else
	{
		pRenderContext->DrawScreenSpaceRectangle( m_FullScreenBufferMaterial,
												  0, 0, m_nRenderWidth, m_nRenderHeight,
												  0.0f, 0.0f, fSSU, fSSV,
												  nSSWidth, nSSHeight, NULL );
	}

	m_bInAARendering = false;
}

//-----------------------------------------------------------------------------
// Begins, ends 3D painting from within a panel paint() method
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::Begin3DPaint( int iLeft, int iTop, int iRight, int iBottom, Vector4D *pScissorAttribute )
{
	Assert( iRight > iLeft );
	Assert( iBottom > iTop );

	// clamp the viewport rectangle to itself while also making sure it is positive
	// e.g. We don't want to change the max coordinate so if nLeft is < 0 we reduce width instead of increasing nRight (i.e. keeping width constant)
	m_n3DLeft = iLeft;
	m_n3DRight = iRight;
	m_n3DLeft = Max( 0, m_n3DLeft );
	m_n3DRight = Max( m_n3DLeft, m_n3DRight );

	m_n3DTop = iTop;
	m_n3DBottom = iBottom;
	m_n3DTop = Max( 0, m_n3DTop );
	m_n3DBottom = Max( m_n3DTop, m_n3DBottom );

	m_nRenderWidth = m_n3DRight - m_n3DLeft;
	m_nRenderHeight = m_n3DBottom - m_n3DTop;

	// clip space extents of panel
 	m_flClipLeft   = -1.0f;
 	m_flClipRight  = 1.0f;
 	m_flClipBottom = -1.0f;
 	m_flClipTop	   = 1.0f;

	if ( m_n3DLeft == 0 )
	{
		float flRatio = -(float)iLeft / (float)( iRight - iLeft );
		m_flClipLeft = m_flClipLeft + ( flRatio *  ( m_flClipRight - m_flClipLeft ) );
	}

	if ( m_n3DTop == 0 )
	{
		float flRatio = -(float)iTop / (float)( iBottom - iTop );
		m_flClipTop = m_flClipBottom + ( ( 1.0-flRatio ) * ( m_flClipTop - m_flClipBottom ) );
	}

	m_flAspect = (float)( iRight - iLeft ) / (float)( iBottom - iTop );
	SetCameraAspect( m_flAspect );

	Assert( !m_bIn3DPaintMode );
	m_bIn3DPaintMode = true;

	CMatRenderContextPtr pRenderContext( materials );

	BeginAARendering();

	if ( !m_bInAARendering )
	{
		pRenderContext->PushScissorRect( pScissorAttribute->x, pScissorAttribute->y, pScissorAttribute->x + pScissorAttribute->z, pScissorAttribute->y + pScissorAttribute->w );
		pRenderContext->Viewport( m_n3DLeft, m_n3DTop,
								  m_nRenderWidth, m_nRenderHeight );
	}
	else
	{
		m_vecScissorAttribute = *pScissorAttribute;
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	// Don't draw the 3D scene w/ stencil
	ShaderStencilState_t state;
	state.m_bEnable = false;
	pRenderContext->SetStencilState( state );

	if ( !m_bSetupRenderStateDelayed )
	{
		SetupRenderState( m_nRenderWidth, m_nRenderHeight );
	}

	if ( HasLightProbe() )
	{
		IMaterial *pMaterial = (g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE) ?
		m_LightProbeBackground : m_LightProbeHDRBackground;

		RenderBox( m_Camera.m_origin, vec3_angle, Vector( -100, -100, -100 ), Vector( 100, 100, 100 ),
				   Color( 255, 255, 255, 255 ), pMaterial, true );
	}
}

void CUI_ItemPreviewRenderer::End3DPaint( bool bIgnoreAlphaWhenCompositing )
{
	// Can't use this feature when drawing into the 3D world
 	Assert( m_bIn3DPaintMode );
	m_bIn3DPaintMode = false;

	CMatRenderContextPtr pRenderContext( materials );

	EndAARendering();
	
	pRenderContext->PopScissorRect();
}


void CUI_ItemPreviewRenderer::OnPaint3D()
{
	// can safely start progressing time now that panel is rendering
	m_bTimeStarted = true;

	// FIXME: Move this call into DrawModel in StudioRender
	StudioRenderConfig_t oldStudioRenderConfig;
	g_pStudioRender->GetCurrentConfig( oldStudioRenderConfig );

	UpdateStudioRenderConfig();

	CMatRenderContextPtr pRenderContext( materials );

	// We want the models to use their natural alpha, not depth in alpha
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, 0 );

	if ( IsPaint3dForRenderCapture() )
	{
		// We are rendering into flashlight depth texture
		pRenderContext->SetFlashlightMode( false ); // disable shadows since we should be using DEPTH_WRITE material
	}
	else if ( GetRenderingWithFlashlightConfiguration() )
	{
		// Setup shadow state that we configured
		g_pStudioRender->ClearAllShadows();
		// NOTE: flashlight shadow is added post bone setup
	}
	else
	{
		// flashlights can't work in the model panel under queued mode (the state isn't ready yet, so causes a crash)
		pRenderContext->SetFlashlightMode( false );
	}

	ITexture *pMyCube = materials->FindTexture( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP, true );

	if ( HasLightProbe() )
	{
		pMyCube = GetLightProbeCubemap( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE );
	}
	pRenderContext->BindLocalCubemap( pMyCube );

	// draw scene
	{
		MDLCACHE_CRITICAL_SECTION();

		// draw merged mdl scene
		DrawMainScene();

		// draw debug mode floor?
		DrawShadowFloor();

		// draw pivot helper?
		DrawPivot();

		// draw preset cameras
		DrawPresetCameras();

		// draw dirn light helper?
		DrawDirectionalLight();

		// particles
		DrawParticleSystems();
	}

	if ( IsPaint3dForRenderCapture() )
	{
		// We are finished rendering into flashlight buffer
	}
	else if ( GetRenderingWithFlashlightConfiguration() )
	{
		// Clear all shadow state that we configured and used now
		g_pStudioRender->ClearAllShadows();
	}

	pRenderContext->Flush();
	g_pStudioRender->UpdateConfig( oldStudioRenderConfig );
}

//-----------------------------------------------------------------------------
// called when we're ticked...
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::OnTick()
{
	// time
	float flCurrentTime = UI_GetAutoPlayTime();
	float flMdlTime = flCurrentTime - m_flAutoPlayTimeBase;
	m_flAutoPlayTimeBase = flCurrentTime;

	m_SceneMergedMDL.UpdateTime( flMdlTime, m_bAnimationPaused || !m_bTimeStarted );
}

//-----------------------------------------------------------------------------
// 'Create'/'Set' lights
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::ClearDirectionalLights()
{
	m_InitialLightingState.m_nLocalLightCount = 0;
	m_CurrentLightingState.m_nLocalLightCount = 0;
}

void CUI_ItemPreviewRenderer::InitDefaultLights()
{
	memset( &m_InitialLightingState, 0, sizeof( MaterialLightingState_t ) );
	for ( int i = 0; i < 6; ++i )
	{
		m_InitialLightingState.m_vecAmbientCube[i].Init( 0.4f, 0.4f, 0.4f );
	}

	//SetIdentityMatrix( m_LightToWorld[0] );
	m_InitialLightingState.m_pLocalLightDesc[0].m_Type = MATERIAL_LIGHT_DIRECTIONAL;
	m_InitialLightingState.m_pLocalLightDesc[0].m_Color.Init( 1.0f, 1.0f, 1.0f );
	m_InitialLightingState.m_pLocalLightDesc[0].m_Direction.Init( 0.0f, 0.0f, -1.0f );
	m_InitialLightingState.m_pLocalLightDesc[0].m_Range = 0.0;
	m_InitialLightingState.m_pLocalLightDesc[0].m_Attenuation0 = 1.0;
	m_InitialLightingState.m_pLocalLightDesc[0].m_Attenuation1 = 0;
	m_InitialLightingState.m_pLocalLightDesc[0].m_Attenuation2 = 0;
	m_InitialLightingState.m_pLocalLightDesc[0].RecalculateDerivedValues();
	m_InitialLightingState.m_nLocalLightCount = 0; // <-- important to be zero otherwise flashlight 'light' will double up
}

void CUI_ItemPreviewRenderer::AddDirectionalLight( const Vector& color, const Vector& direction )
{
	if ( m_InitialLightingState.m_nLocalLightCount < MATERIAL_MAX_LIGHT_COUNT )
	{
		int idx = m_InitialLightingState.m_nLocalLightCount;

		//SetIdentityMatrix( m_LightToWorld[ idx ] );
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Type = MATERIAL_LIGHT_DIRECTIONAL;
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Color = color;
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Direction.Init( direction.x, direction.y, direction.z );
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Direction.Normalized();
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Range = 0.0;
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Attenuation0 = 1.0;
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Attenuation1 = 0;
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Attenuation2 = 0;
		m_InitialLightingState.m_pLocalLightDesc[ idx ].RecalculateDerivedValues();

		m_InitialLightingState.m_nLocalLightCount++;
	}
}

void CUI_ItemPreviewRenderer::SetDirectionalLight(int idx, const Vector& color, const Vector& direction)
{
	if (idx >= 0 && idx < m_InitialLightingState.m_nLocalLightCount)
	{
		// Update the existing light
		//SetIdentityMatrix(m_LightToWorld[idx]);
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Type = MATERIAL_LIGHT_DIRECTIONAL;
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Color = color;
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Direction.Init(direction.x, direction.y, direction.z);
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Direction.Normalized();
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Range = 0.0;
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Attenuation0 = 1.0;
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Attenuation1 = 0;
		m_InitialLightingState.m_pLocalLightDesc[idx].m_Attenuation2 = 0;
		m_InitialLightingState.m_pLocalLightDesc[idx].RecalculateDerivedValues();
	}
	else
	{
		AddDirectionalLight(color, direction);
	}
}

void CUI_ItemPreviewRenderer::SetDirectionalLightDir( int idx, const Vector& direction )
{
	if ( idx >= 0 && idx < m_InitialLightingState.m_nLocalLightCount )
	{
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Direction.Init( direction.x, direction.y, direction.z );
	
	}
}

void CUI_ItemPreviewRenderer::SetDirectionalLightCol( int idx, const Vector& color )
{
	if ( idx >= 0 && idx < m_InitialLightingState.m_nLocalLightCount )
	{
		m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Color = color;
	}
}

void CUI_ItemPreviewRenderer::SetLightAmbient( const Vector& ambient )
{
	for ( int i = 0; i < 6; ++i )
	{
		m_InitialLightingState.m_vecAmbientCube[ i ].Init( ambient.x, ambient.y, ambient.z );
	}
}

void CUI_ItemPreviewRenderer::SetInitialFlashlightState()
{
	m_InitialFlashlightPositionOverride = m_vecRenderCaptureCameraPositionOverride;
	m_InitialFlashlightOrientOverride   = m_vecRenderCaptureCameraOrientOverride;
}

void CUI_ItemPreviewRenderer::DestroyLights()
{
	m_InitialLightingState.m_nLocalLightCount = 0;
	m_CurrentLightingState.m_nLocalLightCount = 0;
}

void CUI_ItemPreviewRenderer::SetCurrentFromInitialLightingState()
{
	m_CurrentLightingState = m_InitialLightingState;

	// flashlight/render capture
	m_vecRenderCaptureCameraPositionOverride = m_InitialFlashlightPositionOverride;
	m_vecRenderCaptureCameraOrientOverride = m_InitialFlashlightOrientOverride;
}

//-----------------------------------------------------------------------------
// 'Get' light data
//-----------------------------------------------------------------------------

void CUI_ItemPreviewRenderer::GetDirectionalLight_InitialCol( int idx, Vector &col )
{
	if ( idx >= 0 && idx < m_InitialLightingState.m_nLocalLightCount )
	{
		col = m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Color;  
	}
	else
	{
		col = vec3_origin;
	}
}

void CUI_ItemPreviewRenderer::GetDirectionalLight_CurrentCol( int idx, Vector &col )
{
	if ( idx >= 0 && idx < m_CurrentLightingState.m_nLocalLightCount )
	{
		col = m_CurrentLightingState.m_pLocalLightDesc[ idx ].m_Color;  // initial state ignores pulsing and editing modes
	}
}

void CUI_ItemPreviewRenderer::GetDirectionalLight_InitialDir( int idx, Vector &dir )
{
	if ( idx >= 0 && idx < m_InitialLightingState.m_nLocalLightCount )
	{
		dir = m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Direction;		// current actual direction, not initial state
	}
}

void CUI_ItemPreviewRenderer::GetDirectionalLight_CurrentDir( int idx, Vector &dir )
{
	if ( idx >= 0 && idx < m_CurrentLightingState.m_nLocalLightCount )
	{
		dir = m_CurrentLightingState.m_pLocalLightDesc[ idx ].m_Direction;		// current actual direction, not initial state
	}
}

void CUI_ItemPreviewRenderer::GetLightAmbient( Vector &ambient )
{
	// repeated for all entries anyway
	ambient = m_InitialLightingState.m_vecAmbientCube[ 0 ];
}

void CUI_ItemPreviewRenderer::GetInitialFlashlightState( Vector &pos, QAngle &orient, Vector &col )
{
	pos = m_InitialFlashlightPositionOverride;
	orient = m_InitialFlashlightOrientOverride;

	CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( GetRenderingWithFlashlightConfiguration() );

	if ( pFlashlightInfo )
	{
		col.x = pFlashlightInfo->m_renderFlashlightState.m_Color[ 0 ];
		col.y = pFlashlightInfo->m_renderFlashlightState.m_Color[ 1 ];
		col.z = pFlashlightInfo->m_renderFlashlightState.m_Color[ 2 ];

		col *= pFlashlightInfo->m_renderFlashlightState.m_fBrightnessScale;
	}
	else
	{
		col = vec3_origin;
	}
}

int CUI_ItemPreviewRenderer::GetLocalLightCount()
{
	return m_InitialLightingState.m_nLocalLightCount;
}

//-----------------------------------------------------------------------------
// Animating lights
//-----------------------------------------------------------------------------

void CUI_ItemPreviewRenderer::AnimateAmbientLightAmount( float flAmount )
{
	for ( int i = 0; i < 6; ++i )
	{
		m_CurrentLightingState.m_vecAmbientCube[ i ] = flAmount * m_InitialLightingState.m_vecAmbientCube[ i ];
	}
}

void CUI_ItemPreviewRenderer::AnimateDirectionalLightAmount( int idx, float flAmount )
{
	DbgAssert( m_InitialLightingState.m_nLocalLightCount == m_CurrentLightingState.m_nLocalLightCount );
	DbgAssert( idx >= 0 && idx < m_CurrentLightingState.m_nLocalLightCount );

	m_CurrentLightingState.m_pLocalLightDesc[ idx ].m_Color = flAmount * m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Color;
}

void CUI_ItemPreviewRenderer::AnimateAllDirectionalLightAmounts( float flAmount )
{
	DbgAssert( m_InitialLightingState.m_nLocalLightCount == m_CurrentLightingState.m_nLocalLightCount );

	for ( int idx = 0; idx < m_InitialLightingState.m_nLocalLightCount; idx++ )
	{
		m_CurrentLightingState.m_pLocalLightDesc[ idx ].m_Color = flAmount * m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Color;
	}
}

void CUI_ItemPreviewRenderer::RotateDirectionalLight( int idx, const QAngle angles )
{
	DbgAssert( m_InitialLightingState.m_nLocalLightCount == m_CurrentLightingState.m_nLocalLightCount );
	DbgAssert( idx >= 0 && idx < m_CurrentLightingState.m_nLocalLightCount );

	matrix3x4_t mat;

	AngleMatrix( angles, mat );

	m_CurrentLightingState.m_pLocalLightDesc[ idx ].m_Direction = mat.TransformVector( m_InitialLightingState.m_pLocalLightDesc[ idx ].m_Direction );
}

void CUI_ItemPreviewRenderer::RotateFlashlight( const QAngle angles )
{
	matrix3x4_t rotMat, origMat, dstMat;

	AngleMatrix( m_InitialFlashlightOrientOverride, m_InitialFlashlightPositionOverride, origMat );

	AngleMatrix( angles, rotMat );

	ConcatTransforms( rotMat, origMat, dstMat );

	Vector newPos;
	QAngle newAng;

	MatrixAngles( dstMat, newAng, newPos );

	SetRenderCaptureCameraPositionOverride( newPos );
	SetRenderCaptureCameraOrientOverride( newAng );
}

//-----------------------------------------------------------------------------
// Sets the background color
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetBackgroundColor( int r, int g, int b )
{
	m_ClearColor.SetColor( r, g, b, 255 );
}

void CUI_ItemPreviewRenderer::SetBackgroundColor( const Color& c )
{
	m_ClearColor = c;
}

const Color& CUI_ItemPreviewRenderer::GetBackgroundColor() const
{
	return m_ClearColor;
}

void CUI_ItemPreviewRenderer::SetGridColor( int r, int g, int b )
{
	m_GridColor.SetColor( r, g, b, 255 );
}

//-----------------------------------------------------------------------------
// Light probe
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetLightProbe( CDmxElement *pLightProbe )
{
	m_LightProbeBackground.Shutdown();
	m_LightProbeHDRBackground.Shutdown();
	m_LightProbeCubemap.Shutdown();
	m_LightProbeHDRCubemap.Shutdown();
	DestroyLights();

	m_bHasLightProbe = (pLightProbe != NULL);
	if ( !m_bHasLightProbe )
	{
		InitDefaultLights();
		return;
	}

	const char *pCubemap = pLightProbe->GetValueString( "cubemap" );
	m_LightProbeCubemap.Init( pCubemap, TEXTURE_GROUP_OTHER );

	const char *pCubemapHDR = pLightProbe->HasAttribute( "cubemapHdr" ) ? pLightProbe->GetValueString( "cubemapHdr" ) : pCubemap;
	m_LightProbeHDRCubemap.Init( pCubemapHDR, TEXTURE_GROUP_OTHER );

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetString( "$envmap", pCubemap );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	m_LightProbeBackground.Init( "SPWP_LightProbeBackground", pVMTKeyValues );
	m_LightProbeBackground->Refresh();

	pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetString( "$envmap", pCubemapHDR );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	m_LightProbeHDRBackground.Init( "SPWP_LightProbeBackground_HDR", pVMTKeyValues );
	m_LightProbeHDRBackground->Refresh();

	const CUtlVector< Vector >& ambientCube = pLightProbe->GetArray<Vector>( "ambientCube" );
	if ( ambientCube.Count() == 6 )
	{
		for ( int i = 0; i < 6; ++i )
		{
			m_InitialLightingState.m_vecAmbientCube[i].Init( ambientCube[i].x, ambientCube[i].y, ambientCube[i].z );
		}
	}

	const CUtlVector< CDmxElement* >& localLights = pLightProbe->GetArray< CDmxElement* >( "localLights" );
	int nLightCount = localLights.Count();
	for ( int i = 0; i < nLightCount; ++i )
	{
		if ( m_InitialLightingState.m_nLocalLightCount == MATERIAL_MAX_LIGHT_COUNT )
			break;

		LightDesc_t *pDesc = &m_InitialLightingState.m_pLocalLightDesc[ m_InitialLightingState.m_nLocalLightCount ];
		CDmxElement *pLocalLight = localLights[i];
		const char *pType = pLocalLight->GetValueString( "name" );
		const Vector& vecColor = pLocalLight->GetValue<Vector>( "color" );

		if ( !Q_stricmp( pType, "directional" ) )
		{
			pDesc->InitDirectional( pLocalLight->GetValue<Vector>( "direction" ), vecColor );
			++m_InitialLightingState.m_nLocalLightCount;
			continue;
		}

		if ( !Q_stricmp( pType, "point" ) )
		{
			const Vector& vecAtten = pLocalLight->GetValue<Vector>( "attenuation" );
			pDesc->InitPoint( pLocalLight->GetValue<Vector>( "origin" ), vecColor );
			pDesc->m_Attenuation0 = vecAtten.x;
			pDesc->m_Attenuation1 = vecAtten.y;
			pDesc->m_Attenuation2 = vecAtten.z;
			pDesc->m_Range = pLocalLight->GetValue<float>( "maxDistance" );
			pDesc->RecalculateDerivedValues();
			++m_InitialLightingState.m_nLocalLightCount;
			continue;
		}

		if ( !Q_stricmp( pType, "spot" ) )
		{
			const Vector& vecAtten = pLocalLight->GetValue<Vector>( "attenuation" );
			pDesc->InitSpot( pLocalLight->GetValue<Vector>( "origin" ), vecColor, vec3_origin,
							 0.5f * RAD2DEG( pLocalLight->GetValue<float>( "theta" ) ),
							 0.5f * RAD2DEG( pLocalLight->GetValue<float>( "phi" ) ) );

			pDesc->m_Direction = pLocalLight->GetValue<Vector>( "direction" );
			pDesc->m_Attenuation0 = vecAtten.x;
			pDesc->m_Attenuation1 = vecAtten.y;
			pDesc->m_Attenuation2 = vecAtten.z;
			pDesc->m_Range = pLocalLight->GetValue<float>( "maxDistance" );
			pDesc->m_Falloff = pLocalLight->GetValue<float>( "exponent" );
			pDesc->RecalculateDerivedValues();
			++m_InitialLightingState.m_nLocalLightCount;
			continue;
		}
	}
}

bool CUI_ItemPreviewRenderer::HasLightProbe() const
{
	return m_bHasLightProbe;
}

ITexture *CUI_ItemPreviewRenderer::GetLightProbeCubemap( bool bHDR )
{
	if ( !m_bHasLightProbe )
		return NULL;

	return bHDR ? m_LightProbeHDRCubemap : m_LightProbeCubemap;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CUI_ItemPreviewRenderer::GetCameraFOV( void )
{
	return m_Camera.m_flFOVX;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetCameraFOV( float flFOV )
{
	m_Camera.m_flFOVX = flFOV;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetCameraAspect( float flAspect )
{
	m_Camera.m_flAspect = flAspect;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CUI_ItemPreviewRenderer::GetCameraAspect( void )
{
	return m_Camera.m_flAspect;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetCameraOffset( const Vector &vecOffset )
{
	m_vecCameraOffset = vecOffset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::GetCameraOffset( Vector &vecOffset )
{
	vecOffset = m_vecCameraOffset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetCameraPositionAndAngles( const Vector &vecPos, const QAngle &angDir )
{
	m_Camera.m_origin = vecPos;
	m_Camera.m_angles = angDir;
}

void CUI_ItemPreviewRenderer::SetCameraPosition( const Vector &vecPos )
{
	m_Camera.m_origin = vecPos;
}

void CUI_ItemPreviewRenderer::SetCameraAngles( const QAngle &angDir )
{
	m_Camera.m_angles = angDir;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::GetCameraPositionAndAngles( Vector &vecPos, QAngle &angDir )
{
	vecPos = m_Camera.m_origin;
	angDir = m_Camera.m_angles;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetRenderCaptureCameraFOV( float flFOV )
{
	m_RenderCaptureCamera.m_flFOVX = flFOV;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetRenderCaptureCameraPositionAndAngles( const Vector &vecPos, const QAngle &angDir )
{
	m_RenderCaptureCamera.m_origin = vecPos;
	m_RenderCaptureCamera.m_angles = angDir;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::GetRenderCaptureCameraPositionAndAngles( Vector &vecPos, QAngle &angDir )
{
	vecPos = m_RenderCaptureCamera.m_origin;
	angDir = m_RenderCaptureCamera.m_angles;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::ResetCameraPivot( void )
{
	SetIdentityMatrix( m_CameraPivot );
	m_CameraPivotAngles = vec3_angle;

	m_FlashlightPivotPos = vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::GetCameraPivotPositionAndAngles( Vector &vecPos, QAngle &angDir )
{
	vecPos = m_CameraPivot.GetColumn( ORIGIN );
	angDir = m_CameraPivotAngles;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::GetCameraPivotPosition( Vector &vecPos )
{
	vecPos = m_CameraPivot.GetColumn( ORIGIN );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetCameraPivotPosition( Vector &vecPos )
{
	m_CameraPivot.SetColumn( vecPos, ORIGIN );

	m_FlashlightPivotPos = vecPos;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::GetFlashlightPivotPosition( Vector &vecPos )
{
	vecPos = m_FlashlightPivotPos;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetFlashlightPivotPosition( Vector &vecPos )
{
	m_FlashlightPivotPos = vecPos;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::ResetView( void )
{
	SetIdentityMatrix( m_CameraPivot );
	m_CameraPivotAngles = vec3_angle;
	m_vecCameraOffset = vec3_origin;
	UpdateCameraTransform();
}

//-----------------------------------------------------------------------------
// Sets the camera to look at the the thing we're spinning around
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::LookAt( float flDistance )
{
	m_vecCameraOffset.x = -flDistance;
	UpdateCameraTransform();
}


void CUI_ItemPreviewRenderer::LookAt( const Vector &vecCenter, float flRadius )
{
	MatrixSetColumn( vecCenter, 3, m_CameraPivot );
	LookAt( flRadius );
}


void CUI_ItemPreviewRenderer::SetCameraIsManipulating( bool bEnabled )
{
	m_bCameraManipulateEnabled = bEnabled;
}

void CUI_ItemPreviewRenderer::SetCameraManipulateAllowed( bool bEnabled )
{
	m_bCameraManipulateAllowed = bEnabled;
}

void CUI_ItemPreviewRenderer::UpdateCameraManipulateTransform( float flAltitude, float flAzimuth, float flDistance, float flLookAtOffsetX, float flLookAtOffsetY, bool bForce )
{
	if ( !m_bCameraManipulateAllowed && !bForce )
		return;

	QAngle angles( RAD2DEG( flAltitude ), RAD2DEG( flAzimuth ), 0.0f );
	AngleMatrix( angles, vec3_origin, m_CameraPivot );
	m_CameraPivotAngles = angles;

	Vector center = vec3_origin;
	center.z += flLookAtOffsetY;
	center.x += flLookAtOffsetX;
	LookAt( center, flDistance );
}

void CUI_ItemPreviewRenderer::UpdateCameraManipulateTransform( float flAltitude, float flAzimuth, float flDistance, Vector vLookAtOffset, bool bForce )
{
	if ( !m_bCameraManipulateAllowed && !bForce )
		return;

	QAngle angles( RAD2DEG( flAltitude ), RAD2DEG( flAzimuth ), 0.0f );
	AngleMatrix( angles, vec3_origin, m_CameraPivot );
	m_CameraPivotAngles = angles;

	LookAt( vLookAtOffset, flDistance );

	UpdateCameraPreset0();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::UpdateCameraPreset( int idx, const Vector &vecPos, const Vector &vecPivot, const QAngle &angDir, const float &fov )
{
	if ( idx >= 0 && idx < m_aCameraPresets.Count() )
	{
		// Update the existing preset
		m_aCameraPresets[ idx ].m_pos = vecPos;
		m_aCameraPresets[ idx ].m_pivot = vecPivot;
		m_aCameraPresets[ idx ].m_ang = angDir;
		m_aCameraPresets[ idx ].m_fov = fov;
	}
	else
	{	
		sCameraPreset *pCameraPreset = m_aCameraPresets.AddToTailGetPtr();

		if ( pCameraPreset )
		{
			pCameraPreset->m_pos = vecPos;
			pCameraPreset->m_pivot = vecPivot;
			pCameraPreset->m_ang = angDir;
			pCameraPreset->m_fov = fov;
		}
	}
}

void CUI_ItemPreviewRenderer::UpdateCameraPreset0()
{
	sCameraPreset *pCameraPreset = NULL;

	if ( m_aCameraPresets.Count() )
	{
		pCameraPreset = &m_aCameraPresets[ 0 ];
	}
	else
	{
		pCameraPreset = m_aCameraPresets.AddToTailGetPtr();
	}

	if ( !pCameraPreset )
		return;

	// Update the existing preset
	GetCameraPivotPosition( pCameraPreset->m_pivot );
	GetCameraPositionAndAngles( pCameraPreset->m_pos, pCameraPreset->m_ang );
	pCameraPreset->m_fov = (float)GetCameraFOV();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
sCameraPreset *CUI_ItemPreviewRenderer::SetCameraPreset( int nPreset, bool bBlend )
{
	if ( m_aCameraPresets.Count() == 0 )
		return NULL;

	if ( ( nPreset < 0 ) || ( nPreset >= m_aCameraPresets.Count() ) )
		return NULL;

	if ( bBlend )
	{
		m_nLastCameraPreset = m_nCurrCameraPreset;
	}
	else
	{
		m_nLastCameraPreset = nPreset;
	}
	
	m_nCurrCameraPreset = nPreset;

	return &m_aCameraPresets[ m_nCurrCameraPreset ];
}

sCameraPreset *CUI_ItemPreviewRenderer::NextCameraPreset()
{
	if ( m_aCameraPresets.Count() == 0 )
		return NULL;

	m_nLastCameraPreset = m_nCurrCameraPreset;
	m_nCurrCameraPreset = ( m_nCurrCameraPreset + 1 ) % m_aCameraPresets.Count();

	return &m_aCameraPresets[ m_nCurrCameraPreset ];
}

sCameraPreset *CUI_ItemPreviewRenderer::PrevCameraPreset()
{
	if ( m_aCameraPresets.Count() == 0 )
		return NULL;

	m_nLastCameraPreset = m_nCurrCameraPreset;

	if ( m_nCurrCameraPreset == 0 )
		m_nCurrCameraPreset = m_aCameraPresets.Count();

	m_nCurrCameraPreset--;

	return &m_aCameraPresets[ m_nCurrCameraPreset ];
}

sCameraPreset *CUI_ItemPreviewRenderer::GetCameraPreset()
{
	if ( m_aCameraPresets.Count() == 0 )
		return NULL;

	if ( ( m_nCurrCameraPreset < 0 ) || ( m_nCurrCameraPreset >= m_aCameraPresets.Count() ) )
		return NULL;

	return &m_aCameraPresets[ m_nCurrCameraPreset ];
}

sCameraPreset *CUI_ItemPreviewRenderer::GetLastCameraPreset()
{
	if ( m_aCameraPresets.Count() == 0 )
		return NULL;

	if ( ( m_nLastCameraPreset < 0 ) || ( m_nLastCameraPreset >= m_aCameraPresets.Count() ) )
		return NULL;

	return &m_aCameraPresets[ m_nLastCameraPreset ];
}

void CUI_ItemPreviewRenderer::ResetCameraPreset()
{
	m_nCurrCameraPreset = 0;
	m_nLastCameraPreset = 0;
}

void CUI_ItemPreviewRenderer::BlendCameraPreset( Vector &blendedPos, Vector &blendedPivot, QAngle &blendedAng, float &blendedFOV, float s )
{
	sCameraPreset *pLastCameraPreset = GetLastCameraPreset();
	sCameraPreset *pCurrCameraPreset = GetCameraPreset();

	if ( pCurrCameraPreset )
	{
		if ( s >= 1.0f )
		{
			m_nLastCameraPreset = -1;
			pLastCameraPreset = NULL;
		}

		if ( pLastCameraPreset && pLastCameraPreset != pCurrCameraPreset )
		{
			Vector vD;

			// blend pivot
			blendedPivot = ( ( 1.0f - s ) * pLastCameraPreset->m_pivot ) + ( s * pCurrCameraPreset->m_pivot );

			// blend distance
			vD = pLastCameraPreset->m_pos - pLastCameraPreset->m_pivot;
			float d0 = vD.Length();
			vD = pCurrCameraPreset->m_pos - pCurrCameraPreset->m_pivot;
			float d1 = vD.Length();

			float d = ( ( 1.0f - s ) * d0 ) + ( s * d1 );
			blendedAng = ( ( 1.0f - s ) * pLastCameraPreset->m_ang ) + ( s * pCurrCameraPreset->m_ang );

			matrix3x4_t mat;
			AngleMatrix( blendedAng, vec3_origin, mat );

			vD = d * mat.GetForward();
			blendedPos = blendedPivot - vD;

			// blended FOV
			blendedFOV = ( ( 1.0f - s ) * pLastCameraPreset->m_fov ) + ( s * pCurrCameraPreset->m_fov );
		}
		else
		{
			blendedPos   = pCurrCameraPreset->m_pos;
			blendedPivot = pCurrCameraPreset->m_pivot;
			blendedAng   = pCurrCameraPreset->m_ang;
			blendedFOV   = pCurrCameraPreset->m_fov;
		}
	}
	else
	{
		// fallback
		//GetCameraPivotPosition( blendedPivot );
		GetCameraPositionAndAngles( blendedPos, blendedAng );
	}
}


//-----------------------------------------------------------------------------
// Sets up render state in the material system for rendering
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::SetupRenderState( int nDisplayWidth, int nDisplayHeight )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	VMatrix view, projection;

	if ( IsPaint3dForRenderCapture() )
	{
		ComputeViewMatrix( &view, m_RenderCaptureCamera );
		ComputeProjectionMatrix( &projection, m_RenderCaptureCamera, nDisplayWidth, nDisplayHeight );
	}
	else
	{
		ComputeViewMatrix( &view, m_Camera );
		ComputeProjectionMatrix( &projection, m_Camera.m_flZNear, m_Camera.m_flZFar, m_Camera.m_flFOVX, m_Camera.m_flAspect,
								 m_flClipLeft, m_flClipBottom, m_flClipRight, m_flClipTop);
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadMatrix( view );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadMatrix( projection );

	pRenderContext->SetLightingState( m_CurrentLightingState );

	// FIXME: Remove this! This should automatically happen in DrawModel
	// in studiorender.
	if ( !g_pStudioRender )
		return;

	VMatrix worldToCamera;
	MatrixInverseTR( view, worldToCamera );
	Vector vecOrigin, vecRight, vecUp, vecForward;
	MatrixGetColumn( worldToCamera, 0, &vecRight );
	MatrixGetColumn( worldToCamera, 1, &vecUp );
	MatrixGetColumn( worldToCamera, 2, &vecForward );
	MatrixGetColumn( worldToCamera, 3, &vecOrigin );
	g_pStudioRender->SetViewState( vecOrigin, vecRight, vecUp, vecForward );
}


//-----------------------------------------------------------------------------
// Compute the camera world position
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::UpdateCameraTransform()
{
	// Set up the render state for the camera + light
	matrix3x4_t offset, worldToCamera;
	SetIdentityMatrix( offset );
	MatrixSetColumn( m_vecCameraOffset, 3, offset );
	ConcatTransforms( m_CameraPivot, offset, worldToCamera );
	MatrixAngles( worldToCamera, m_Camera.m_angles, m_Camera.m_origin );
}

void CUI_ItemPreviewRenderer::ComputeCameraTransform( matrix3x4_t *pWorldToCamera )
{
	AngleMatrix( m_Camera.m_angles, m_Camera.m_origin, *pWorldToCamera );
}


//-----------------------------------------------------------------------------
// MDL helpers
//-----------------------------------------------------------------------------
int CUI_ItemPreviewRenderer::GetAttachmentIndex( CUI_SceneItem *pItem, const char *pszAttachment )
{
	MDLCACHE_CRITICAL_SECTION();

	// Check to see if we have a valid model to look at.
	if ( pItem->GetRootMDL()->GetMDL() == MDLHANDLE_INVALID )
		return -1;

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( pItem->GetRootMDL()->GetMDL() ), g_pMDLCache );
	
	return Studio_FindAttachment( &studioHdr, pszAttachment );
}

void CUI_ItemPreviewRenderer::SetDirectionalLightAttachment( CUI_SceneItem *pItem, int idx, const char *pszAttachment )
{
	MDLCACHE_CRITICAL_SECTION();

	if ( !pszAttachment || !pItem )
		return;

	// Check to see if we have a valid model to look at.
	if ( pItem->GetRootMDL()->GetMDL() == MDLHANDLE_INVALID )
		return;
	if ( idx < 0 || idx >= MATERIAL_MAX_LIGHT_COUNT )
		return;

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( pItem->GetRootMDL()->GetMDL() ), g_pMDLCache );

	m_iDirectionalLightAttachments[idx] = Studio_FindAttachment( &studioHdr, pszAttachment );
}

void CUI_ItemPreviewRenderer::SetLookAtCamera( CUI_SceneItem *pItem )
{
	m_pCameraLookAtItem = pItem;
}

void CUI_ItemPreviewRenderer::SetCameraAttachment( CUI_SceneItem *pItem, const char *pszAttachment )
{
	MDLCACHE_CRITICAL_SECTION();

	// Check to see if we have a valid model to look at.
	if ( pItem->GetRootMDL()->GetMDL() == MDLHANDLE_INVALID )
		return;

	SetLookAtCamera( NULL );

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( pItem->GetRootMDL()->GetMDL() ), g_pMDLCache );

	m_iCameraAttachment = Studio_FindAttachment( &studioHdr, pszAttachment );

	if ( m_iCameraAttachment == -1 )
	{
		SetLookAtCamera( pItem );
	}
}

void CUI_ItemPreviewRenderer::SetRenderCaptureCameraAttachment( CUI_SceneItem *pItem, const char *pszAttachment )
{
	MDLCACHE_CRITICAL_SECTION();

	// Check to see if we have a valid model to look at.
	if ( pItem->GetRootMDL()->GetMDL() == MDLHANDLE_INVALID )
		return;

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( pItem->GetRootMDL()->GetMDL() ), g_pMDLCache );

	m_iRenderCaptureCameraAttachment = Studio_FindAttachment( &studioHdr, pszAttachment );
}

void CUI_ItemPreviewRenderer::GetSceneBounds( Vector &bbMin, Vector &bbMax )
{
	// floor pos - just low enough to sit under scene bounds
	MDLHandle_t hSceneMDL = m_SceneMergedMDL.GetMainMDLHandle ();
	GetMDLBoundingBox ( &bbMin, &bbMax, hSceneMDL, 0 );
}

void CUI_ItemPreviewRenderer::GetSceneBounds( Vector &center, float &radius )
{
	Vector bbMin, bbMax;

	// floor pos - just low enough to sit under scene bounds
	MDLHandle_t hSceneMDL = m_SceneMergedMDL.GetMainMDLHandle ();
	GetMDLBoundingBox ( &bbMin, &bbMax, hSceneMDL, 0 );

	center = 0.5f * ( bbMin + bbMax );

	Vector r = bbMax - center;

	radius = r.Length();
}

void CUI_ItemPreviewRenderer::DrawMainScene()
{
	if ( m_SceneMergedMDL.GetMainCMDL() )
	{
		m_SceneMergedMDL.Draw();
	}
	else
	{
		// temp workaround to get empty/null scene working
		m_SceneMergedMDL.OnPostSetUpBonesPreDraw();
	}
}

void CUI_ItemPreviewRenderer::DrawShadowFloor()
{
	if ( IsPaint3dForRenderCapture() )
		return;

	if ( !m_bDrawShadowFloor )
		return;

	CMatRenderContextPtr pRenderContext( materials );
	matrix3x4_t mat;
	QAngle ang;
	Vector pos;

	if ( m_bDrawDebugHelpers )
	{
		Vector bbMin, bbMax;

		// floor pos - just low enough to sit under scene bounds
		MDLHandle_t hSceneMDL = m_SceneMergedMDL.GetMainMDLHandle();
		GetMDLBoundingBox( &bbMin, &bbMax, hSceneMDL, 0 );

		pos = 0.5f * ( bbMin + bbMax );
		pos.z -= 0.5f * ( bbMax.z - bbMin.z );

		if ( !m_pShadowFloorMaterial )
		{
			m_pShadowFloorMaterial = materials->FindMaterial( "models/weapons/pedestals/panorama_floor", TEXTURE_GROUP_MODEL );
			//		m_pShadowFloorMaterial = materials->FindMaterial( "materials/concrete/concretefloor01a", TEXTURE_GROUP_MODEL );
		}

		mat.SetToIdentity();
		g_pStudioRender->ForcedMaterialOverride( m_pShadowFloorMaterial );
		mat.SetOrigin( pos );
		m_DebugMDL_Floor.Draw( mat );
		g_pStudioRender->ForcedMaterialOverride( NULL );

		mat.SetToIdentity();
		m_DebugMDL_Axis.Draw( mat );
	}

	GetRenderCaptureCameraPositionAndAngles( pos, ang );
	AngleMatrix( ang, pos, mat );
	m_DebugMDL_Flashlight.Draw( mat );

}

void CUI_ItemPreviewRenderer::DrawPivot ()
{
	if ( IsPaint3dForRenderCapture () )
		return;

	if ( !( m_bDrawWorldPivot || m_bDrawFlashlightPivot ) )
		return;

	matrix3x4_t mat;
	Vector pos;

	mat.SetToIdentity();

	if ( m_bDrawWorldPivot )
	{
		GetCameraPivotPosition ( pos );
		mat.SetOrigin ( pos );
		m_DebugMDL_Pivot.Draw ( mat );
	}

	if ( m_bDrawFlashlightPivot )
	{
		GetFlashlightPivotPosition ( pos );
		mat.SetOrigin ( pos );
		m_DebugMDL_Pivot.Draw ( mat );
	}

}

void CUI_ItemPreviewRenderer::DrawPresetCameras()
{
	if ( IsPaint3dForRenderCapture() )
		return;

	if ( !m_bDrawPresetCameras )
		return;

// 	matrix3x4_t mat;
// 	Vector pos;
// 
// 	mat.SetToIdentity();
// 	GetCameraPivotPosition( pos );
// 	mat.SetOrigin( pos );
// 
// 	m_DebugMDL_Camera.Draw( mat );
}

void CUI_ItemPreviewRenderer::DrawDirectionalLight()
{
	static float sArrowOffsetDist = 7.0f;

	if ( IsPaint3dForRenderCapture() )
		return;

	if ( ( m_nDrawDebugDirectionalLightIdx < 0 ) || ( m_nDrawDebugDirectionalLightIdx >= m_InitialLightingState.m_nLocalLightCount ) )
		return;

	matrix3x4_t mat;
	Vector pos;

	mat.SetToIdentity();
	
	//mat.SetOrigin( pos );
	Vector dir, pivot, col;
	
	GetCameraPivotPosition( pivot );

	GetDirectionalLight_CurrentDir( m_nDrawDebugDirectionalLightIdx, dir );
	GetDirectionalLight_InitialCol( m_nDrawDebugDirectionalLightIdx, col );
	VectorMatrix( -dir, mat );
	mat.SetOrigin( pivot - ( dir * sArrowOffsetDist ) );

	m_DebugMDL_DirLight.m_Color = Color( (int)( col.x * 255.0f ), (int)( col.y * 255.0f ), (int)( col.z * 255.0f ), 255 );
	m_DebugMDL_DirLight.Draw( mat );

	// don't draw axes?
	if ( !m_bDrawDebugHelpers )
		return;

	mat.SetToIdentity();
	m_DebugMDL_Axis.Draw( mat );
}

void CUI_ItemPreviewRenderer::InitDebugMDLs()
{
	MDLHandle_t hMDL;
	
	hMDL = g_pMDLCache->FindMDL( FLASHLIGHT_MDL );
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}
	m_DebugMDL_Flashlight.SetMDL( hMDL );
	if ( hMDL != MDLHANDLE_INVALID )
		g_pMDLCache->Release( hMDL );

	hMDL = g_pMDLCache->FindMDL( DIRLIGHT_MDL );
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}
	m_DebugMDL_DirLight.SetMDL( hMDL );
	if ( hMDL != MDLHANDLE_INVALID )
		g_pMDLCache->Release( hMDL );

	hMDL = g_pMDLCache->FindMDL( AXIS_MDL );
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}
	m_DebugMDL_Axis.SetMDL( hMDL );
	if ( hMDL != MDLHANDLE_INVALID )
		g_pMDLCache->Release( hMDL );

	hMDL = g_pMDLCache->FindMDL( PIVOT_MDL );
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}
	m_DebugMDL_Pivot.SetMDL( hMDL );
	if ( hMDL != MDLHANDLE_INVALID )
		g_pMDLCache->Release ( hMDL );

	hMDL = g_pMDLCache->FindMDL( FLOOR_MDL );
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}
	m_DebugMDL_Floor.SetMDL( hMDL );
	if ( hMDL != MDLHANDLE_INVALID )
		g_pMDLCache->Release( hMDL );
}

//-----------------------------------------------------------------------------
// Particles
//-----------------------------------------------------------------------------
void CUI_ItemPreviewRenderer::InitParticleSystems()
{
	m_vecParticleSystemOffset = vec3_origin;
	m_angParticleSystemOffset = vec3_angle;

	m_aSceneParticleSystems.Purge();
}

void CUI_ItemPreviewRenderer::UpdateParticleSystems( float flTime )
{
	// update all running systems
	for ( int nIndex = 0; nIndex < m_aSceneParticleSystems.Count(); nIndex++ )
	{
		m_aSceneParticleSystems[ nIndex ].Update( flTime );
	}
}

void CUI_ItemPreviewRenderer::StopParticleSystems()
{
	// Stop all running systems
	for ( int nIndex = 0; nIndex < m_aSceneParticleSystems.Count(); nIndex++ )
	{
		m_aSceneParticleSystems[ nIndex ].Stop();
	}
}

void CUI_ItemPreviewRenderer::SetParticleSystemOffsetPosition( float flX, float flY, float flZ )
{
	m_vecParticleSystemOffset.Init( flX, flY, flZ );
}

void CUI_ItemPreviewRenderer::SetParticleSystemOffsetAngles( float flX, float flY, float flZ )
{
	m_angParticleSystemOffset.Init( flX, flY, flZ );
}

void CUI_ItemPreviewRenderer::AddParticleSystem( const char *szParticleSystem, const char *szAttachToBone, bool bRepeat )
{
	int nIndex;

	// look for system with free particle collection before allocating a new one
	for ( nIndex = 0; nIndex < m_aSceneParticleSystems.Count(); nIndex++ )
	{
		CUI_SceneParticleSystem *pParticleSystem = &m_aSceneParticleSystems[ nIndex ];

		if ( pParticleSystem->m_pParticleCollection == NULL )
			break;
	}

	if ( !m_aSceneParticleSystems.IsValidIndex( nIndex ) && m_aSceneParticleSystems.Count() <= MAX_PANEL_PARTICLE_SYSTEMS )
	{
		nIndex = m_aSceneParticleSystems.AddToTail();
	}

	if ( !m_aSceneParticleSystems.IsValidIndex( nIndex ) )
		return;

	m_aSceneParticleSystems[ nIndex ].Init( szParticleSystem, szAttachToBone, m_SceneMergedMDL.GetRootMDL()->GetMDL(), bRepeat, m_vecParticleSystemOffset, m_angParticleSystemOffset );
}


void CUI_ItemPreviewRenderer::DrawParticleSystems()
{
	if ( IsPaint3dForRenderCapture() )
		return;

	for ( int nIndex = 0; nIndex < m_aSceneParticleSystems.Count(); nIndex++ )
	{
		m_aSceneParticleSystems[ nIndex ].Draw();
	}

}

//-----------------------------------------------------------------------------
// CUI_SceneItem
//-----------------------------------------------------------------------------

CUI_SceneItem::CUI_SceneItem()
{
	m_ItemToWorld.SetToIdentity();

	m_pParentRenderer = NULL;

	m_hMainMDL = MDLHANDLE_INVALID;
	m_sMainEconItemName = "";
	m_iMainMDLIndex = -1;

	m_pSprayTex = nullptr;

	m_Angle.Init();
}

CUI_SceneItem::~CUI_SceneItem()
{
	m_aMergeMDLs.Purge();
}

void CUI_SceneItem::Init()
{
	m_pSprayTex = nullptr;

	m_ItemToWorld.SetToIdentity();
}

void CUI_SceneItem::InitMDL( const char *szMDLName, const char *pEconItemName, CEconItemView *pEconItemView, IClientRenderable *pProxyData, CompositeTextureSize_t compTextureSize )
{
	// TODO - do we need to set an initial sequence?
	// If we do this on the current ui character model, that sequence (0) may not be what we want
	// so assume the setup code will initialise this appropriately instead of the scene item init
	//SetSequence( 0, true );

	m_hMainMDL = szMDLName ? g_pMDLCache->FindMDL( szMDLName ) : MDLHANDLE_INVALID;

	bool bGeneratedCustomMat = false;
	if ( pEconItemView && ( pEconItemView->GetCustomPaintKitIndex() != 0 ) && ( pEconItemView->GetCustomMaterialCount() == 0 ) )
	{
		pEconItemView->UpdateGeneratedMaterial( false, m_hMainMDL, compTextureSize );
		bGeneratedCustomMat = true;
	}

	SetMDL( m_hMainMDL, pEconItemView, pProxyData );
	
	if ( m_hMainMDL != MDLHANDLE_INVALID )
		g_pMDLCache->Release( m_hMainMDL );

	m_sMainEconItemName = pEconItemName;

	// SetMDL above will increase ref count of custom materials, by duplicating them to the CMDL
	// safe to clear them from the econ item now
	if ( bGeneratedCustomMat )
		pEconItemView->ClearCustomMaterials();
}


int CUI_SceneItem::AddMergeMDL( const char *szMDLName, const char *pEconItemName, CEconItemView *pEconItemView, IClientRenderable *pProxyData, bool bRequestBonemergeTakeover, CompositeTextureSize_t compTextureSize )
{
	if ( !szMDLName )
		return -1;

	if ( g_pMDLCache == NULL )
		return -1;

	// don't add same mdl more than once?
	MDLHandle_t hMDL = szMDLName ? g_pMDLCache->FindMDL( szMDLName ) : MDLHANDLE_INVALID;
	if ( ( hMDL == MDLHANDLE_INVALID ) )
		return -1;

	if ( GetMergeMDLIndex ( hMDL ) != -1 )
	{
		g_pMDLCache->Release( hMDL );	// FindMDL does addref
		return -1;
	}

	bool bGeneratedCustomMat = false;
	if ( pEconItemView && ( pEconItemView->GetCustomPaintKitIndex() != 0 ) && ( pEconItemView->GetCustomMaterialCount() == 0 ) )
	{
		pEconItemView->UpdateGeneratedMaterial( false, hMDL, compTextureSize );
		bGeneratedCustomMat = true;
	}

	SetMergeMDL( hMDL, pEconItemView, pProxyData, bRequestBonemergeTakeover );
	g_pMDLCache->Release( hMDL );	// FindMDL does addref, safe to release after SetMergeMDL which also does addref

	if ( bGeneratedCustomMat )
		pEconItemView->ClearCustomMaterials();

	int iIndex = m_aMergedItemData.AddToTail();

	if ( !m_aMergedItemData.IsValidIndex ( iIndex ) )
		return -1;

	m_aMergedItemData[ iIndex ].m_hMDL = hMDL;
	m_aMergedItemData[ iIndex ].m_sMDLName.Set( szMDLName );
	m_aMergedItemData[ iIndex ].m_sEconItemName = pEconItemName;

	return iIndex;
}

bool CUI_SceneItem::MergeEconItem( int &nMergeIndex, const char *szEconItemName, CEconItemView *pEconItemView, char const *szReason /* = NULL */ )
{
	nMergeIndex = -1;

	if ( !pEconItemView )
		return false;

	if ( !pEconItemView->IsValid() )
		return false;

	const char *pszItem = pEconItemView->GetWorldDisplayModel();
	if ( !pszItem )
		return false;

	if ( szReason && !V_strcmp( szReason, "equip_player" ) )
	{	// Mac-10 has a special model that doesn't have a jiggly strap (avoids bugs where jiggly strap doesn't hang down)
		if ( !V_stricmp( pszItem, "models/weapons/w_smg_mac10.mdl" ) )
			pszItem = "models/weapons/w_smg_mac10_nogrip.mdl";
	}

	int nSkinIndex = 0;
	if ( V_strstr( pszItem, "glove" ) )
	{
		MDLCACHE_CRITICAL_SECTION();

		// turn off default gloves if we're merging in new ones
		// first assumption is that the 0th merged mdl is the character (root model being the scaffold), we want to use bodyparts for the character mdl
		MDLHandle_t hMDL = m_aMergedItemData[ 0 ].m_hMDL;
		studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( hMDL );
		CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

		// second assumption is that the hand bodygroup contains 'gloves' in the name!
 		int iBodyGroup = FindBodygroupByName( &studioHdr, "gloves" );
 		if ( iBodyGroup > -1 )
 		{
 			::SetBodygroup( &studioHdr, GetMergeMDL( hMDL )->m_nBody, iBodyGroup, 1 );
 		}

		// due to mismatch between viewmodel and worldmodel material list for gloves, make sure we re-generate 
		pEconItemView->ClearCustomMaterials();

		// get the hand skin index
		const PlayerViewmodelArmConfig *pArmConfig = GetPlayerViewmodelArmConfigForPlayerModel( pStudioHdr->pszName() );
		nSkinIndex = atoi( pArmConfig->szSkintoneIndex );
	}

	nMergeIndex = AddMergeMDL( pszItem, szEconItemName, pEconItemView, NULL, false );

	if ( nSkinIndex != 0 )
	{
		// set the hand skin index on the merged mdl
		MDLHandle_t hMDL = m_aMergedItemData[ nMergeIndex ].m_hMDL;
		CMDL *pCMDL = GetMergeMDL( hMDL );
		pCMDL->m_nSkin = nSkinIndex;
	}

	if ( pEconItemView->GetCustomMaterialCount() > 0 && m_aMergedItemData.IsValidIndex( nMergeIndex ) )
	{
		MDLHandle_t hMDL = m_aMergedItemData[ nMergeIndex ].m_hMDL;
		CMDL *pCMDL = GetMergeMDL( hMDL );
		pEconItemView->DuplicateCustomMaterialsToOther( pCMDL );
	}

	return true;
}

// for merged mdls
void CUI_SceneItem::SetMainMDL( int iMainMDLIndex )
{
	if ( iMainMDLIndex == -1 )
		return;

	m_iMainMDLIndex = iMainMDLIndex;

	m_hMainMDL = m_aMergedItemData[ iMainMDLIndex ].m_hMDL;
	m_sMainEconItemName = m_aMergedItemData[ iMainMDLIndex ].m_sEconItemName;
}

MDLHandle_t CUI_SceneItem::GetMainMDLHandle()
{
	if ( m_iMainMDLIndex == -1 )
		return m_hMainMDL;
 
 	return m_aMergedItemData[ m_iMainMDLIndex ].m_hMDL;
}

CMDL *CUI_SceneItem::GetMainCMDL()
{
	MDLHandle_t hMDL = GetMainMDLHandle();
	CMDL *pCMDL;

	if ( m_iMainMDLIndex == -1 )
	{
		pCMDL = ( hMDL != MDLHANDLE_INVALID ) ? GetMDL() : NULL;
	}
	else
	{
		pCMDL = ( hMDL != MDLHANDLE_INVALID ) ? GetMergeMDL( hMDL ) : NULL;
	}

	return pCMDL;
}

CMDL *CUI_SceneItem::GetShadowFloorCMDL( const char *pMDLName )
{
	MDLHandle_t hMDL = pMDLName ? g_pMDLCache->FindMDL( pMDLName ) : MDLHANDLE_INVALID;

	if ( hMDL == MDLHANDLE_INVALID )
		return NULL;

	return GetMergeMDL( hMDL );
}

const char *CUI_SceneItem::GetMainEconItemName()
{
	if ( m_iMainMDLIndex == -1 )
		return m_sMainEconItemName.Get();

	return m_aMergedItemData[ m_iMainMDLIndex ].m_sEconItemName.Get();
}

void CUI_SceneItem::SetMDLSkinIndex( int nNewSkinIndex )
{
	MDLCACHE_CRITICAL_SECTION();

	CMDL *pMDL = &m_RootMDL.m_MDL;

	if ( !pMDL )
		return;

	pMDL->m_nSkin = nNewSkinIndex;
}

CMDL *CUI_SceneItem::GetRootMDL()
{
	return &m_RootMDL.m_MDL;
}

CMDL *CUI_SceneItem::GetMDLItemConfig()
{
	if ( m_iMainMDLIndex == -1 )
	{
		return GetRootMDL();
	}
	else
	{
		return GetMergeMDL( GetMainMDLHandle() );
	}
}

void CUI_SceneItem::SetAnglesAndPosition( MDLHandle_t hMDL, const QAngle &ang, const Vector &pos )
{
	if ( m_iMainMDLIndex == -1 )
	{
		matrix3x4_t mat;

		mat = ang.ToMatrix();
		
		// set position to be rotated lookat pos since model not guaranteed to have origin where you would like it
		// (i.e. we're looking at lookat pos, so let's rotate object around that point. TODO - try other methods)
		
		if ( m_pParentRenderer )
		{
			Vector vLookAt;
			QAngle angPivot;
			
			m_pParentRenderer->GetCameraPivotPositionAndAngles( vLookAt, angPivot );

			Vector vRotatedLookAt;
			VectorRotate( vLookAt, ang, vRotatedLookAt );

			Vector shiftedPos = pos + ( vLookAt - vRotatedLookAt );

			mat.SetColumn( shiftedPos, ORIGIN );
		}
		else
		{
			// shouldn't get here
			mat.SetColumn( pos, ORIGIN );
		}

		SetRootMDLTransform( mat );
	}
	else
	{
		CMDL *pMDL = GetMergeMDL( hMDL );

		if ( pMDL )
		{
			matrix3x4_t localTransform;

			localTransform = ang.ToMatrix();
			localTransform.SetColumn( pos, ORIGIN );

			SetLocalTransform( hMDL, localTransform );
		}
	}
}

void CUI_SceneItem::UpdateTime( float flMdlTime, bool bPaused )
{
	if ( bPaused )
		return;

	m_RootMDL.m_MDL.AdjustTime( flMdlTime );

	for ( int k = 0; k < m_aMergeMDLs.Count(); ++k )
	{
		if ( m_aMergeMDLs[k].m_MDL.GetMDL() != MDLHANDLE_INVALID )
		{
			m_aMergeMDLs[k].m_MDL.AdjustTime( flMdlTime );
		}
	}
}

float CUI_SceneItem::GetTime()
{
	return m_RootMDL.m_MDL.m_flTime;
}

bool CUI_SceneItem::AttemptToSyncWithItemFromOtherPanel( CUI_SceneItem *pSrc )
{
	// TODO
	return false;
}

void CUI_SceneItem::SetParentRenderer( CUI_ItemPreviewRenderer *pSceneRenderer )
{
	m_pParentRenderer = pSceneRenderer;
}

void CUI_SceneItem::OnModelDrawPassStart( int iPass, CStudioHdr *pStudioHdr, int &nFlags )
{
	if ( !m_pParentRenderer )
		return;

	if ( m_pParentRenderer->IsPaint3dForRenderCapture() )
		nFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	else if ( m_pParentRenderer->GetRenderingWithFlashlightConfiguration() )
		nFlags &= ~STUDIORENDER_DRAW_NO_SHADOWS;
}

void CUI_SceneItem::OnModelDrawPassFinished( int iPass, CStudioHdr *pStudioHdr, int &nFlags )
{
}

ConVar panorama_camera_rotate_radius_scale( "panorama_camera_rotate_radius_scale", "1.0" );

void CUI_SceneItem::OnPostSetUpBonesPreDraw()
{
	VPROF_BUDGET_THREAD( "CUI_SceneItem::OnPostSetUpBonesPreDraw", VPROF_BUDGETGROUP_TENFOOT );

	if ( !m_pParentRenderer )
		return;

	Vector vecPositionFlashlight = vec3_origin;
	QAngle anglesFlashlight = vec3_angle;

	Vector vecPositionCamera = vec3_origin;
	QAngle anglesCamera = vec3_angle;

	// override camera - do this if camera needs to be attached to a bone that's just been setup prior to this call
	// otherwise leave camera as is

	if ( m_pParentRenderer->IsPaint3dForRenderCapture() )
	{
		if ( m_pParentRenderer->GetRenderCaptureAttachment() >= 0 )
		{
			matrix3x4_t camera;

			if ( GetAttachment( m_pParentRenderer->GetRenderCaptureAttachment() + 1, camera ) )
			{
				Vector vecPosition;
				QAngle angles;

				MatrixPosition( camera, vecPosition );
				MatrixAngles( camera, angles );

				vecPositionFlashlight = vecPosition;
				anglesFlashlight = angles;

				if ( m_pParentRenderer->IsRenderCaptureCameraPositionOverrideEnabled() )
				{
					vecPositionFlashlight += *m_pParentRenderer->GetRenderCaptureCameraPositionOverride();
				}

				if ( m_pParentRenderer->IsRenderCaptureCameraOrientOverrideEnabled() )
				{
					anglesFlashlight += *m_pParentRenderer->GetRenderCaptureCameraOrientOverride();
				}

				m_pParentRenderer->SetRenderCaptureCameraPositionAndAngles( vecPositionFlashlight, anglesFlashlight );
			}
		}
	}
	else
	{
		if ( m_pParentRenderer->IsRenderingInventoryItem() )
		{
			vecPositionCamera = *m_pParentRenderer->GetInventoryCameraPosition();
			anglesCamera = *m_pParentRenderer->GetInventoryCameraOrient();

			m_pParentRenderer->SetCameraPositionAndAngles( vecPositionCamera, anglesCamera );
		}
		else if ( m_pParentRenderer->GetCameraAttachment() >= 0 )
		{
			matrix3x4_t camera;
			if ( GetAttachment( m_pParentRenderer->GetCameraAttachment() + 1, camera ) )
			{
				Vector vecPosition;
				QAngle angles;

				MatrixPosition( camera, vecPosition );
				MatrixAngles( camera, angles );

				vecPositionCamera = vecPosition;
				anglesCamera = angles;

				if ( m_pParentRenderer->IsCameraPositionOverrideEnabled() )
				{
					vecPositionCamera += *m_pParentRenderer->GetCameraPositionOverride();
				}

				if ( m_pParentRenderer->IsCameraOrientOverrideEnabled() )
				{
					anglesCamera += *m_pParentRenderer->GetCameraOrientOverride();
				}

				m_pParentRenderer->SetCameraPositionAndAngles( vecPositionCamera, anglesCamera );
			}
		}

		// update particle system control points
 		for ( int nParticleSysIndex = 0; nParticleSysIndex < m_pParentRenderer->m_aSceneParticleSystems.Count(); nParticleSysIndex++ )
 		{
			CUI_SceneParticleSystem *pSystem = &m_pParentRenderer->m_aSceneParticleSystems[ nParticleSysIndex ];

			if ( pSystem->m_nAttachIndex >= 0 )
			{
				matrix3x4_t matCP;
				if ( GetAttachment( pSystem->m_nAttachIndex, matCP ) )
				{
					Vector vecPosition;
					QAngle angles;

					MatrixPosition( matCP, vecPosition );
					MatrixAngles( matCP, angles );

					pSystem->SetControlPoint( vecPosition, angles );
				}
			}
 		}
	}

	m_pParentRenderer->GetRenderCaptureCameraPositionAndAngles( vecPositionFlashlight, anglesFlashlight );

	if ( m_pParentRenderer->GetRenderingWithFlashlightConfiguration() != NULL )
	{
		CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>( m_pParentRenderer->GetRenderingWithFlashlightConfiguration() );

		//
		// Update flashlight position and orientation
		//
		{
			pFlashlightInfo->m_renderFlashlightState.m_vecLightOrigin = vecPositionFlashlight;
			AngleQuaternion( anglesFlashlight, pFlashlightInfo->m_renderFlashlightState.m_quatOrientation );
		}

		//
		// Build world to shadow matrix, then perspective projection and concatenate
		//
		{
			VMatrix matWorldToShadowView, matPerspective;
			matrix3x4_t matOrientation;
			QuaternionMatrix( pFlashlightInfo->m_renderFlashlightState.m_quatOrientation, matOrientation );		// Convert quat to matrix3x4
			PositionMatrix( vec3_origin, matOrientation );				// Zero out translation elements

			VMatrix matBasis( matOrientation );							// Convert matrix3x4 to VMatrix

			Vector vForward, vLeft, vUp;
			matBasis.GetBasisVectors( vForward, vLeft, vUp );
			matBasis.SetForward( vLeft );								// Bizarre vector flip inherited from earlier code, WTF?
			matBasis.SetLeft( vUp );
			matBasis.SetUp( vForward );
			matWorldToShadowView = matBasis.Transpose();					// Transpose

			Vector translation;
			Vector3DMultiply( matWorldToShadowView, pFlashlightInfo->m_renderFlashlightState.m_vecLightOrigin, translation );

			translation *= -1.0f;
			matWorldToShadowView.SetTranslation( translation );

			// The the bottom row.
			matWorldToShadowView[ 3 ][ 0 ] = matWorldToShadowView[ 3 ][ 1 ] = matWorldToShadowView[ 3 ][ 2 ] = 0.0f;
			matWorldToShadowView[ 3 ][ 3 ] = 1.0f;

			MatrixBuildPerspective( matPerspective, pFlashlightInfo->m_renderFlashlightState.m_fHorizontalFOVDegrees,
				pFlashlightInfo->m_renderFlashlightState.m_fVerticalFOVDegrees,
				pFlashlightInfo->m_renderFlashlightState.m_NearZ, pFlashlightInfo->m_renderFlashlightState.m_FarZ );

			MatrixMultiply( matPerspective, matWorldToShadowView, pFlashlightInfo->m_renderMatrixWorldToShadow );
		}
	}

	// set light positions
	MaterialLightingState_t *pLightingState = m_pParentRenderer->GetCurrentLightingState();

	for ( int j = 0; j < (int)MIN( pLightingState->m_nLocalLightCount, MATERIAL_MAX_LIGHT_COUNT ); ++j )
	{
		if ( m_pParentRenderer->GetDirectionalLightAttachment( j ) >= 0 )
		{
			matrix3x4_t camera;
			if ( GetAttachment( m_pParentRenderer->GetDirectionalLightAttachment( j ) + 1, camera ) )
			{
				Vector vecPosition;
				QAngle angles;

				MatrixPosition( camera, vecPosition );
				MatrixAngles( camera, angles );

				Vector vecForward;
				AngleVectors( angles, &vecForward );
				pLightingState->m_pLocalLightDesc[ j ].m_Direction = vecForward;
				pLightingState->m_pLocalLightDesc[ j ].RecalculateDerivedValues();
			}
		}
	}

	if ( m_pParentRenderer->IsPaint3dForRenderCapture() )
	{
		CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>(m_pParentRenderer->GetRenderingWithFlashlightConfiguration());

		Camera_t &cameraSettings = m_pParentRenderer->GetRenderCaptureCameraSettings();

		cameraSettings.m_flZNear = pFlashlightInfo->m_renderFlashlightState.m_NearZ;
		cameraSettings.m_flZFar = pFlashlightInfo->m_renderFlashlightState.m_FarZ;

		// Configure view with the updated camera settings and restore Z planes
		m_pParentRenderer->SetupRenderStateDelayed();	
	}
	else
	{
		m_pParentRenderer->SetupRenderStateDelayed();

		if ( m_pParentRenderer->GetRenderingWithFlashlightConfiguration() )
		{
			// Add the shadow we rendered in previous pass to our model
			CRenderCaptureConfigurationState *pFlashlightInfo = reinterpret_cast<CRenderCaptureConfigurationState *>(m_pParentRenderer->GetRenderingWithFlashlightConfiguration());
			g_pStudioRender->AddShadow( NULL, NULL, &pFlashlightInfo->m_renderFlashlightState, &pFlashlightInfo->m_renderMatrixWorldToShadow, pFlashlightInfo->m_pFlashlightDepthTexture );
		}
	}
}

int CUI_SceneItem::FindSequenceFromActivity( CStudioHdr *pStudioHdr, const char *pszActivity )
{
	if ( !pStudioHdr || !pszActivity )
		return -1;

	for ( int iSeq = 0; iSeq < pStudioHdr->GetNumSeq(); ++iSeq )
	{
		mstudioseqdesc_t &seqDesc = pStudioHdr->pSeqdesc( iSeq );
		if ( !stricmp( seqDesc.pszActivityName(), pszActivity ) )
		{
			return iSeq;
		}
	}

	return -1;
}

void CUI_SceneItem::SetAnim( const char *pszName, bool bUseSequencePlaybackFPS )
{
	MDLCACHE_CRITICAL_SECTION();

	if ( V_isempty( pszName ) )
		return;

	// Get the studio header of the root model.
	studiohdr_t *pStudioHdr = m_RootMDL.m_MDL.GetStudioHdr();
	if ( !pStudioHdr )
		return;

	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	int iSequence = ACT_INVALID;

	iSequence = FindSequenceFromActivity( &studioHdr, pszName );

	if ( iSequence == ACT_INVALID )
	{
		iSequence = LookupSequence( &studioHdr, pszName );
	}

	if ( iSequence != ACT_INVALID )
	{
		//DevMsg("CMDL SetSeq: %s, %d\n", pszName, iSequence );
		SetSequence( iSequence, bUseSequencePlaybackFPS );
	}
}

void CUI_SceneItem::AddAnimFollowLoop( const char *pszName, bool bUseSequencePlaybackFPS )
{
	MDLCACHE_CRITICAL_SECTION();

	if ( V_isempty( pszName ) )
		return;

	// Get the studio header of the root model.
	studiohdr_t *pStudioHdr = m_RootMDL.m_MDL.GetStudioHdr();
	if ( !pStudioHdr )
		return;

	CStudioHdr studioHdr( pStudioHdr, g_pMDLCache );

	int iSequence = ACT_INVALID;

	iSequence = FindSequenceFromActivity( &studioHdr, pszName );

	if ( iSequence == ACT_INVALID )
	{
		iSequence = LookupSequence( &studioHdr, pszName );
	}

	if ( iSequence != ACT_INVALID )
	{
		//DevMsg( "CMDL AddSequenceFollowLoop: %s, %d\n", pszName, iSequence );
		AddSequenceFollowLoop( iSequence, bUseSequencePlaybackFPS );
	}
}

void CUI_SceneItem::ClearAnimFollowLoop()
{
	ClearSequenceFollowLoop();
}

void CUI_SceneItem::SetRootMDLTransform(const matrix3x4_t &mat)
{
	m_ItemToWorld = mat;

	m_RootMDL.m_MDLToWorld = mat;
}

const matrix3x4_t *CUI_SceneItem::GetRootMDLTransform()
{
	// identity for now 
	return &m_ItemToWorld;
}

float *CUI_SceneItem::GetPoseParameters()
{ 
	return m_PoseParameters; 
}

int CUI_SceneItem::GetNumSequenceLayers()
{
	return m_aSequenceLayers.Count();
}

void CUI_SceneItem::RemoveAllSequenceLayers()
{
	m_aSequenceLayers.RemoveAll();
}

void CUI_SceneItem::RemoveSequenceLayer( int nIndex )
{
	if ( m_aSequenceLayers.IsValidIndex( nIndex ) )
	{
		m_aSequenceLayers.Remove( nIndex );
	}
}

MDLSquenceLayer_t *CUI_SceneItem::GetSequenceLayer( int nIndex )
{
	MDLSquenceLayer_t *pLayer = NULL;

	if ( m_aSequenceLayers.IsValidIndex( nIndex ) )
	{
		pLayer = &m_aSequenceLayers[ nIndex ];
	}

	return pLayer;
}

MDLSquenceLayer_t *CUI_SceneItem::GetNextEmptySequenceLayer()
{ 
	int nIndex = m_aSequenceLayers.AddToTail();

	return GetSequenceLayer( nIndex );
}

float CUI_SceneItem::GetSequenceEndTime( CStudioHdr *pStudioHdr, int nIndex )
{
	MDLSquenceLayer_t *pLayer = GetSequenceLayer( nIndex );
	float flEndTime = 0.0f;

	if ( pLayer )
	{
		int nFrameCount = MAX( 1, Studio_MaxFrame( pStudioHdr, pLayer->m_nSequenceIndex, GetPoseParameters() ) );
		float flPlaybackRate = Studio_FPS( pStudioHdr, pLayer->m_nSequenceIndex, GetPoseParameters() );

		float flDuration = ( flPlaybackRate > 0.0f ) ? float( nFrameCount ) / flPlaybackRate : float( nFrameCount );

		if ( pLayer->m_bLoop )
		{
			float flAdjustedTime = GetRootMDL()->m_flTime - pLayer->m_flStartTime;
			float flCycleTime = fmod( flAdjustedTime, flDuration );
			flEndTime = ( GetRootMDL()->m_flTime - flCycleTime ) + flDuration;
		}
		else
		{
			flEndTime = pLayer->m_flStartTime + flDuration;
		}
	}

	return flEndTime;
}

//-----------------------------------------------------------------------------
// CUI_SceneParticleSystem
//-----------------------------------------------------------------------------

CUI_SceneParticleSystem::CUI_SceneParticleSystem()
{
	m_pParticleCollection = NULL;
	m_flLastTime = FLT_MAX;
	m_bRepeat = false;
}

CUI_SceneParticleSystem::~CUI_SceneParticleSystem()
{
	if ( m_pParticleCollection )
		delete m_pParticleCollection;
}

void CUI_SceneParticleSystem::ResetParticleCollection()
{
	if ( m_pParticleCollection )
		delete m_pParticleCollection;

	m_pParticleCollection = NULL;
	m_flLastTime = FLT_MAX;
	m_bRepeat = false;
}

void CUI_SceneParticleSystem::Init( const char *szParticleSystem, const char *szAttachment, MDLHandle_t hAttachMDL, bool bRepeat, const Vector &vecSystemOffset, const QAngle &angSystemOffset )
{
	Assert( m_pParticleCollection == NULL );

	if ( g_pParticleSystemMgr->IsParticleSystemDefined( szParticleSystem ) )
	{
		//g_pParticleSystemMgr->ShouldLoadSheets( true );
		//CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pParticleSystem->m_Name );
		//pDef->m_bAlwaysPrecache = true;

		int nIndex = g_pParticleSystemMgr->GetParticleSystemIndex( szParticleSystem );
		g_pParticleSystemMgr->PrecacheParticleSystem( nIndex, szParticleSystem );

		m_pParticleCollection = g_pParticleSystemMgr->CreateParticleCollection( szParticleSystem );

		//bone
		m_vecOffset = vecSystemOffset;
		m_angOffset = angSystemOffset;
		m_vecCP = vec3_origin;
		m_angCP = vec3_angle;

		m_Name = szParticleSystem;
		m_bRepeat = bRepeat;
		m_nAttachIndex = -1;

		// check for attachment in scene
		if ( hAttachMDL != MDLHANDLE_INVALID )
		{
			MDLCACHE_CRITICAL_SECTION();

			CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( hAttachMDL ), g_pMDLCache );

			m_nAttachIndex = Studio_FindAttachment( &studioHdr, szAttachment );
		}
	}
	else
	{
		m_vecOffset = vec3_origin;
		m_angOffset = vec3_angle;
		m_vecCP = vec3_origin;
		m_angCP = vec3_angle;
		m_nAttachIndex = -1;
		m_pParticleCollection = NULL;
	}

	m_flLastTime = FLT_MAX;

}

void CUI_SceneParticleSystem::Update( float flTime )
{
	CParticleCollection *pParticleCollection = m_pParticleCollection;

	if ( !pParticleCollection )
		return;

	if ( m_flLastTime == FLT_MAX )
	{
		m_flLastTime = flTime;
	}

	float flDt = flTime - m_flLastTime;
	m_flLastTime = flTime;

	for ( int i = 0; i < MAX_PARTICLE_CONTROL_POINTS; ++i )
	{
		if ( !pParticleCollection->ReadsControlPoint( i ) )//|| m_pControlPointValue[ i ] == vec3_invalid )
			continue;

		//m_pParticleCollection->SetControlPoint( i, m_pControlPointValue[ i ] );

		if ( m_nAttachIndex == -1 )
		{
			pParticleCollection->SetControlPoint( i, m_vecOffset );
			pParticleCollection->SetControlPointOrientation( i, Vector( 1, 0, 0 ), Vector( 0, -1, 0 ), Vector( 0, 0, 1 ) );
			pParticleCollection->SetControlPointParent( i, i );
		}
		else
		{
			pParticleCollection->SetControlPoint( i, m_vecCP + m_vecOffset ); // TODO - m_vecOffset should be in bone space, testing for now
			pParticleCollection->SetControlPointOrientation( i, Vector( 1, 0, 0 ), Vector( 0, -1, 0 ), Vector( 0, 0, 1 ) );
			pParticleCollection->SetControlPointParent( i, i );
		}
	}

	bool bIsInvalid = !pParticleCollection->IsFullyValid();
	bool bIsFinished = pParticleCollection->IsFinished();

	if ( ( bIsFinished && m_bRepeat ) || bIsInvalid )
	{
		delete m_pParticleCollection;
		m_pParticleCollection = NULL;

		if ( m_Name.Length() )
		{
			CParticleCollection *pNewParticleCollection = g_pParticleSystemMgr->CreateParticleCollection( m_Name );
			m_pParticleCollection = pNewParticleCollection;
		}

		m_flLastTime = FLT_MAX;
	}
	else if ( !bIsFinished )
	{
		pParticleCollection->Simulate( flDt );
	}
	else
	{
		ResetParticleCollection();
	}
}

void CUI_SceneParticleSystem::Draw()
{
	static const Vector4D vecDiffuseModulation( 1.0f, 1.0f, 1.0f, 1.0f );

	if ( m_pParticleCollection )
	{
		CMatRenderContextPtr pRenderContext( materials );

		//MDLCACHE_CRITICAL_SECTION();

		m_pParticleCollection->Render( 0, pRenderContext, vecDiffuseModulation );
	}
}

void CUI_SceneParticleSystem::Stop()
{
	if ( !m_pParticleCollection )
		return;

	m_pParticleCollection->StopEmission( false, false, false, true );
}

void CUI_SceneParticleSystem::SetControlPoint( const Vector &vecCP, const QAngle &angCP )
{
	m_vecCP = vecCP;
	m_angCP = angCP;
}

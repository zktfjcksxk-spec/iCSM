//====== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. =====//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "BaseVSShader.h"
#include "flashlight_shadow_decal_dx9_helper.h"

#include "flashlight_shadow_decal_vs20.inc"
#include "flashlight_shadow_decal_ps20b.inc"

//#include "shaderapifast.h"
#include "shaderlib/commandbuilder.h"
#include "convar.h"

#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//static ConVar mat_force_vertexfog( "mat_force_vertexfog", "0", FCVAR_DEVELOPMENTONLY );

ConVar mat_flashlight_shadow_decal_indirectcolour_amount( "mat_flashlight_shadow_decal_indirectcolour_amount", "0.1", FCVAR_DEVELOPMENTONLY );
ConVar mat_flashlight_shadow_decal_max_alpha( "mat_flashlight_shadow_decal_max_alpha", "0.2", FCVAR_DEVELOPMENTONLY );


//-----------------------------------------------------------------------------
// Initialize shader parameters
//-----------------------------------------------------------------------------
void InitParamsFlashlightShadowDecal_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, FlashlightShadowDecal_DX9_Vars_t &info )
{	
	InitIntParam( info.m_nFlashlightNoLambert, params, 0 );
	
	// FLASHLIGHTFIXME: Do ShaderAPI::BindFlashlightTexture
// 	if ( info.m_nFlashlightTexture != -1 )
// 	{
// 		params[ FLASHLIGHTTEXTURE ]->SetStringValue( GetFlashlightTextureFilename() );
// 	}

	InitIntParam( info.m_nLinearWrite, params, 0 );
}


//-----------------------------------------------------------------------------
// Initialize shader
//-----------------------------------------------------------------------------

void InitFlashlightShadowDecal_DX9( CBaseVSShader *pShader, IMaterialVar** params, FlashlightShadowDecal_DX9_Vars_t &info )
{
// 	if ( info.m_nFlashlightTexture != -1 )
// 	{
// 		pShader->LoadTexture( info.m_nFlashlightTexture, TEXTUREFLAGS_SRGB );
// 	}

}

class CFlashlightShadowDecal_DX9_Context : public CBasePerMaterialContextData
{
public:
	CCommandBufferBuilder< CFixedCommandStorageBuffer< 800 > > m_SemiStaticCmdsOut;
};

//-----------------------------------------------------------------------------
// Draws the shader
//-----------------------------------------------------------------------------
void DrawFlashlightShadowDecal_DX9( CBaseVSShader *pShader, IMaterialVar** params,
								    IShaderDynamicAPI * pShaderAPI,
									IShaderShadow* pShaderShadow,
									FlashlightShadowDecal_DX9_Vars_t &info,
									VertexCompressionType_t vertexCompression,
									CBasePerMaterialContextData **pContextDataPtr ) 

{
	CFlashlightShadowDecal_DX9_Context *pContextData = reinterpret_cast< CFlashlightShadowDecal_DX9_Context *> ( *pContextDataPtr );

	bool bFlashlightNoLambert = false;
	if ( ( info.m_nFlashlightNoLambert != -1 ) && params[ info.m_nFlashlightNoLambert ]->GetIntValue() )
	{
		bFlashlightNoLambert = true;
	}

	bool bSRGBWrite = true;
	if ( ( info.m_nLinearWrite != -1 ) && ( params[ info.m_nLinearWrite ]->GetIntValue() == 1 ) )
	{
		bSRGBWrite = false;
	}

	if ( pShader->IsSnapshotting() || (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) )
	{
		if  ( pShader->IsSnapshotting() )
		{
			ShadowFilterMode_t nShadowFilterMode = SHADOWFILTERMODE_DEFAULT;
			if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				nShadowFilterMode = g_pHardwareConfig->GetShadowFilterMode( false, true );	// Based upon vendor and device dependent formats
			}

			pShader->SetBlendingShadowState( BT_BLEND );

			pShaderShadow->EnableBlending( true );
			pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			pShaderShadow->EnableAlphaTest( false );

			pShaderShadow->EnableBlendingSeparateAlpha( true );
			pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			pShaderShadow->BlendOpSeparateAlpha( SHADER_BLEND_OP_ADD );

			unsigned int flags = VERTEX_POSITION;
			flags |= VERTEX_NORMAL;

			int userDataSize = 0;

			pShaderShadow->EnableTexture( SHADER_SAMPLER8, true );	// Depth texture
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER8, false );
			pShaderShadow->EnableTexture( SHADER_SAMPLER6, true );	// Noise map
			pShaderShadow->EnableTexture( SHADER_SAMPLER7, true );	// Flashlight cookie
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER7, true );

			pShaderShadow->EnableSRGBWrite( true );
		
			// texcoord0 : base texcoord
			int pTexCoordDim[3] = { 2, 2, 3 };
			int nTexCoordCount = 1;
		
			// This shader supports compressed vertices, so OR in that flag:
			flags |= VERTEX_FORMAT_COMPRESSED;

			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, pTexCoordDim, userDataSize );

			DECLARE_STATIC_VERTEX_SHADER( flashlight_shadow_decal_vs20 );
			SET_STATIC_VERTEX_SHADER( flashlight_shadow_decal_vs20 );

			DECLARE_STATIC_PIXEL_SHADER( flashlight_shadow_decal_ps20b );
			SET_STATIC_PIXEL_SHADER_COMBO( FLASHLIGHTDEPTHFILTERMODE, nShadowFilterMode );
			SET_STATIC_PIXEL_SHADER( flashlight_shadow_decal_ps20b );

			pShader->DefaultFog();

			pShaderShadow->EnableAlphaWrites( true );
		}

		if ( pShaderAPI && ( (! pContextData ) || ( pContextData->m_bMaterialVarsChanged ) ) )
		{
			if ( !pContextData )								// make sure allocated
			{
				pContextData = new CFlashlightShadowDecal_DX9_Context;
				*pContextDataPtr = pContextData;
			}
			pContextData->m_bMaterialVarsChanged = false;
			pContextData->m_SemiStaticCmdsOut.Reset();
			pContextData->m_SemiStaticCmdsOut.SetPixelShaderFogParams( 21 );

			// store eye pos in shader constant 20
			float flEyeW = pShader->TextureIsTranslucent( BASETEXTURE, true ) ? 1.0f : 0.0f;
			pContextData->m_SemiStaticCmdsOut.StoreEyePosInPixelShaderConstant( 20, flEyeW );

			pContextData->m_SemiStaticCmdsOut.SetVertexShaderFlashlightState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6 );

			CBCmdSetPixelShaderFlashlightState_t state;
			state.m_LightSampler = SHADER_SAMPLER7;
			state.m_DepthSampler = SHADER_SAMPLER8;
			state.m_ShadowNoiseSampler = SHADER_SAMPLER6;
			state.m_nColorConstant = 28;
			state.m_nAttenConstant = 22;
			state.m_nOriginConstant = 23;
			state.m_nDepthTweakConstant = 2;
			state.m_nScreenScaleConstant = 31;
			state.m_nWorldToTextureConstant = 24;
			state.m_bFlashlightNoLambert = bFlashlightNoLambert;
			state.m_bSinglePassFlashlight = true;

			pContextData->m_SemiStaticCmdsOut.SetPixelShaderFlashlightState( state );
			pContextData->m_SemiStaticCmdsOut.End();
		}
	}
	if ( pShaderAPI )
	{
		CCommandBufferBuilder< CFixedCommandStorageBuffer< 1000 > > DynamicCmdsOut;
		DynamicCmdsOut.Call( pContextData->m_SemiStaticCmdsOut.Base() );

		bool bFlashlightShadows = true;
		bool bUberLight;
		ShaderApiFast( pShaderAPI )->GetFlashlightShaderInfo( &bFlashlightShadows, &bUberLight );

		LightState_t lightState = {0, false, false};
		ShaderApiFast( pShaderAPI )->GetDX9LightState( &lightState );

		float vConst[4] = { lightState.m_bStaticLight ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
		ShaderApiFast( pShaderAPI )->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, vConst );

		float flColorTweakConsts[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };
		flColorTweakConsts[ 0 ] = mat_flashlight_shadow_decal_indirectcolour_amount.GetFloat();
		flColorTweakConsts[ 1 ] = mat_flashlight_shadow_decal_max_alpha.GetFloat();
		DynamicCmdsOut.SetPixelShaderConstant( 3, flColorTweakConsts, 1 );

		DECLARE_DYNAMIC_VERTEX_SHADER( flashlight_shadow_decal_vs20 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( DYNAMIC_LIGHT, lightState.HasDynamicLight() );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( NUM_LIGHTS, lightState.m_nNumLights );
		SET_DYNAMIC_VERTEX_SHADER_CMD( DynamicCmdsOut, flashlight_shadow_decal_vs20 );

		DECLARE_DYNAMIC_PIXEL_SHADER( flashlight_shadow_decal_ps20b );
		SET_DYNAMIC_PIXEL_SHADER_CMD( DynamicCmdsOut, flashlight_shadow_decal_ps20b );

		bool bUnusedTexCoords[3] = { false, true, true };
		ShaderApiFast( pShaderAPI )->MarkUnusedVertexFields( 0, 3, bUnusedTexCoords );

		DynamicCmdsOut.End();
		ShaderApiFast( pShaderAPI )->ExecuteCommandBuffer( DynamicCmdsOut.Base() );
	}
	pShader->Draw();
}

//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "BaseVSShader.h"
#include <string.h>
#include "const.h"

#include "cpp_shader_constant_register_map.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#include "cs_grass_vs30.inc"
#include "cs_grass_ps20.inc"
#include "cs_grass_ps20b.inc"
#include "cs_grass_ps30.inc"

ConVar cl_proximity_grass( "cl_proximity_grass", "1" );

ConVar cl_grass_mip_bias( "cl_grass_mip_bias", "-0.5", FCVAR_RELEASE | FCVAR_ARCHIVE, "", true, -1.0f, true, 1.0f );

BEGIN_VS_SHADER( Grass, "Help for Grass" )
			  
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( WORLDSPACESCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( WORLDSPACETINT, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( WORLDSPACETYPE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( WORLDSPACEBURN, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( MINIMUMSPRITESIZE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( WORLDSPACEZONE, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( MASKS, SHADER_PARAM_TYPE_TEXTURE, "", "" )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}
	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_NO_DEBUG_OVERRIDE );
		SET_FLAGS( MATERIAL_VAR_VERTEXCOLOR );
		SET_FLAGS( MATERIAL_VAR_VERTEXALPHA );

		if ( !(params[WORLDSPACESCALE]->IsDefined()) ) { params[WORLDSPACESCALE]->SetFloatValue( 1.0f ); }
		if ( !(params[MINIMUMSPRITESIZE]->IsDefined()) ) { params[MINIMUMSPRITESIZE]->SetFloatValue( 0.0f ); }
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
		LoadTexture( MASKS );

		if ( params[WORLDSPACETINT]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			LoadTexture( WORLDSPACETINT, TEXTUREFLAGS_SRGB );
		}

		if ( params[WORLDSPACETYPE]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			LoadTexture( WORLDSPACETYPE );
		}

		if ( params[WORLDSPACEBURN]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			LoadTexture( WORLDSPACEBURN );
		}

		if ( params[WORLDSPACEZONE]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			LoadTexture( WORLDSPACEZONE );
		}
	}

#define SHADER_USE_VERTEX_COLOR		1
#define SHADER_USE_CONSTANT_COLOR	2

	void SetCSGrassCommonShadowState( unsigned int shaderFlags, IMaterialVar **params )
	{
		IShaderShadow *pShaderShadow = s_pShaderShadow;
		pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
		pShaderShadow->EnableSRGBRead( SHADER_SAMPLER4, true );

		pShaderShadow->EnableTexture( SHADER_SAMPLER5, true );

		bool bWorldSpaceTintTexture = params[WORLDSPACETINT]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() && !UsingEditor( params );
		if ( bWorldSpaceTintTexture )
		{
			pShaderShadow->EnableVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER0, true );
		}

		bool bWorldSpaceTypeTexture = params[WORLDSPACETYPE]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() && !UsingEditor( params );
		if ( bWorldSpaceTypeTexture )
		{
			pShaderShadow->EnableVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER1, true );
		}

		bool bWorldSpaceBurnTexture = params[WORLDSPACEBURN]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() && !UsingEditor( params );
		if ( bWorldSpaceBurnTexture )
		{
			pShaderShadow->EnableVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, true );
		}

		bool bWorldSpaceZoneTexture = params[WORLDSPACEZONE]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() && !UsingEditor( params );
		if ( bWorldSpaceZoneTexture )
		{
			pShaderShadow->EnableVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER3, true );
		}

		bool bCSMEnabled;
		int	nCSMQualityComboValue = 0;
		bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() ) ? true : false;

        bCSMEnabled = g_pHardwareConfig->SupportsCascadedShadowMapping() && !bSFM;
        if ( bCSMEnabled )
        {
            nCSMQualityComboValue = g_pHardwareConfig->GetCSMShaderMode( materials->GetCurrentConfigForVideoCard().GetCSMQualityMode() );
        }

		unsigned int flags = VERTEX_POSITION;

		//if( shaderFlags & SHADER_USE_VERTEX_COLOR )
		{
			flags |= VERTEX_COLOR;
		}

		int numTexCoords = 1;
		static int s_TexCoordDims[] = { 4 };
		pShaderShadow->VertexShaderVertexFormat( flags, numTexCoords, s_TexCoordDims, 0 );

		pShaderShadow->EnableAlphaTest( true );
		//pShaderShadow->EnableAlphaWrites( false );
		pShaderShadow->EnableDepthWrites( true );
		pShaderShadow->EnableCulling( false ); // grass quad winding order can go either way
		pShaderShadow->EnableSRGBWrite( true );

		DefaultFog();

		DECLARE_STATIC_VERTEX_SHADER( cs_grass_vs30 );
		SET_STATIC_VERTEX_SHADER_COMBO( WORLDSPACETINTTEXTURE, bWorldSpaceTintTexture );
		SET_STATIC_VERTEX_SHADER_COMBO( WORLDSPACETYPETEXTURE, bWorldSpaceTypeTexture );
		SET_STATIC_VERTEX_SHADER_COMBO( WORLDSPACEBURNTEXTURE, bWorldSpaceBurnTexture );
		SET_STATIC_VERTEX_SHADER_COMBO( WORLDSPACEZONETEXTURE, bWorldSpaceZoneTexture );
		SET_STATIC_VERTEX_SHADER( cs_grass_vs30 );

		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_STATIC_PIXEL_SHADER( cs_grass_ps30 );
			SET_STATIC_PIXEL_SHADER_COMBO( CASCADED_SHADOW_MAPPING, bCSMEnabled );
			SET_STATIC_PIXEL_SHADER_COMBO( CSM_MODE, nCSMQualityComboValue);
			SET_STATIC_PIXEL_SHADER_COMBO( WORLDSPACETINTTEXTURE, bWorldSpaceTintTexture );
			SET_STATIC_PIXEL_SHADER_COMBO( WORLDSPACETYPETEXTURE, bWorldSpaceTypeTexture );
			SET_STATIC_PIXEL_SHADER( cs_grass_ps30 );
		}
		else if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			DECLARE_STATIC_PIXEL_SHADER( cs_grass_ps20b );
			SET_STATIC_PIXEL_SHADER( cs_grass_ps20b );
		}
		else
		{
			DECLARE_STATIC_PIXEL_SHADER( cs_grass_ps20 );
			SET_STATIC_PIXEL_SHADER( cs_grass_ps20 );
		}

	}

	void SetCSGrassCommonDynamicState( unsigned int shaderFlags, IMaterialVar **params )
	{
		IShaderDynamicAPI *pShaderAPI = s_pShaderAPI;

		BindTexture( SHADER_SAMPLER4, TEXTURE_BINDFLAGS_SRGBREAD, BASETEXTURE, FRAME );
		BindTexture( SHADER_SAMPLER5, TEXTURE_BINDFLAGS_NONE, MASKS, FRAME );

		bool bWorldSpaceTintTexture = params[WORLDSPACETINT]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0();
		if ( bWorldSpaceTintTexture )
		{
			BindVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER0, WORLDSPACETINT );
		}

		bool bWorldSpaceTypeTexture = params[WORLDSPACETYPE]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0();
		if ( bWorldSpaceTypeTexture )
		{
			BindVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER1, WORLDSPACETYPE );
		}

		bool bWorldSpaceBurnTexture = params[WORLDSPACEBURN]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() && !UsingEditor( params );
		if ( bWorldSpaceBurnTexture )
		{
			BindVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER2, WORLDSPACEBURN );
		}

		bool bWorldSpaceZoneTexture = params[WORLDSPACEZONE]->IsDefined() && g_pHardwareConfig->SupportsPixelShaders_3_0() && !UsingEditor( params );
		if ( bWorldSpaceZoneTexture )
		{
			BindVertexTexture( SHADER_VERTEXTEXTURE_SAMPLER3, WORLDSPACEZONE );
		}

		//LoadModelViewMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
		//LoadProjectionMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3 );

		float flTime = pShaderAPI->CurrentTime();//sin( pShaderAPI->CurrentTime() ) + sin( pShaderAPI->CurrentTime() * 2 );

		float flConst0[4] = { flTime, params[WORLDSPACESCALE]->GetFloatValueFast(), 0, params[MINIMUMSPRITESIZE]->GetFloatValueFast() };
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, flConst0, 4 );

		CLIENT_RENDER_STATE_BUFFER_GET( pShaderAPI, CLIENT_RENDER_STATE_BUFFER_WORLD_2D_MINS_MAXS, Vector4D, pvecWorldMinMaxBuffer, numWorldMinMaxBufferEntries );
		if ( pvecWorldMinMaxBuffer && numWorldMinMaxBufferEntries )
		{
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, pvecWorldMinMaxBuffer[0].Base(), 4 );
		}
		else
		{
			Assert( false );
			float flConst1[4] = { 0, 0, 0, 0 };
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, flConst1, 4 );
		}
		
		bool bCullGrassOusideDangerRadius = false;
		CLIENT_RENDER_STATE_BUFFER_GET( pShaderAPI, CLIENT_RENDER_STATE_BUFFER_DANGERZONE_INFO, Vector4D, pvecDangerZoneBuffer, numDangerZoneBufferEntries );
		if ( pvecDangerZoneBuffer && numDangerZoneBufferEntries && pvecDangerZoneBuffer[0].w > 0 )
		{
			bCullGrassOusideDangerRadius = true;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, pvecDangerZoneBuffer[0].Base(), 4 );
		}

		int nNumNearbyPlayers = 0;

		if ( cl_proximity_grass.GetBool() )
		{
			CLIENT_RENDER_STATE_BUFFER_GET( pShaderAPI, CLIENT_RENDER_STATE_BUFFER_PROXIMITY_PLAYERS, Vector4D, pvecProximityBuffer, numProximityBufferEntries );
			if ( pvecProximityBuffer && numProximityBufferEntries )
			{
				nNumNearbyPlayers = numProximityBufferEntries;

				if ( nNumNearbyPlayers > 5 )
				{
					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_13, pvecProximityBuffer[5].Base(), 4 );
				}
				if ( nNumNearbyPlayers > 4 )
				{
					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_12, pvecProximityBuffer[4].Base(), 4 );
				}
				if ( nNumNearbyPlayers > 3 )
				{
					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, pvecProximityBuffer[3].Base(), 4 );
				}
				if ( nNumNearbyPlayers > 2 )
				{
					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, pvecProximityBuffer[2].Base(), 4 );
				}
				if ( nNumNearbyPlayers > 1 )
				{
					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, pvecProximityBuffer[1].Base(), 4 );
				}
				if ( nNumNearbyPlayers > 0 )
				{
					pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, pvecProximityBuffer[0].Base(), 4 );
				}
			}
		}

		DECLARE_DYNAMIC_VERTEX_SHADER( cs_grass_vs30 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( DZONE, bCullGrassOusideDangerRadius );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( NEARBYPLAYERS, nNumNearbyPlayers );
		SET_DYNAMIC_VERTEX_SHADER( cs_grass_vs30 );



		bool bCSMEnabled;
		bool bSFM = ( ToolsEnabled() && IsPlatformWindowsPC() && g_pHardwareConfig->SupportsPixelShaders_3_0() ) ? true : false;

        bCSMEnabled = g_pHardwareConfig->SupportsCascadedShadowMapping() && pShaderAPI->IsCascadedShadowMapping() && !bSFM;

		if ( bCSMEnabled )
		{
			ITexture *pDepthTextureAtlas = NULL;
			const CascadedShadowMappingState_t &cascadeState = pShaderAPI->GetCascadedShadowMappingState( &pDepthTextureAtlas );

			if ( pDepthTextureAtlas )
			{
				BindTexture( SHADER_SAMPLER15, TEXTURE_BINDFLAGS_SHADOWDEPTH, pDepthTextureAtlas, 0 );
				pShaderAPI->SetPixelShaderConstant( 64, &cascadeState.m_vLightColor.x, CASCADED_SHADOW_MAPPING_CONSTANT_BUFFER_SIZE );
			}
		}

		if ( g_pHardwareConfig->SupportsPixelShaders_3_0() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( cs_grass_ps30 );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( DYN_CSM_ENABLED, bCSMEnabled ? 1 : 0 );
			SET_DYNAMIC_PIXEL_SHADER( cs_grass_ps30 );
		}
		else if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( cs_grass_ps20b );
			SET_DYNAMIC_PIXEL_SHADER( cs_grass_ps20b );
		}
		else
		{
			DECLARE_DYNAMIC_PIXEL_SHADER( cs_grass_ps20 );
			SET_DYNAMIC_PIXEL_SHADER( cs_grass_ps20 );
		}

		pShaderAPI->SetPixelShaderFogParams( PSREG_FOG_PARAMS );

		float vEyePos_SpecExponent[4];
		pShaderAPI->GetWorldSpaceCameraPosition( vEyePos_SpecExponent );

		vEyePos_SpecExponent[3] = cl_grass_mip_bias.GetFloat();
		pShaderAPI->SetPixelShaderConstant( PSREG_EYEPOS_SPEC_EXPONENT, vEyePos_SpecExponent, 1 );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			SetCSGrassCommonShadowState( 0, params );
		}
		DYNAMIC_STATE
		{
			SetCSGrassCommonDynamicState( 0, params );
		}
		
		Draw();
	}
END_SHADER

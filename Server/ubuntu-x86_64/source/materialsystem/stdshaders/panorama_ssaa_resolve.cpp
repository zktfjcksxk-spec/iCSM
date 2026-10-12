//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Simple 'resolve' shader when performing SSAA rendering of 3d panels in panorama 
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "convar.h"

#include "unlitgeneric_ps30.inc"
#include "unlitgeneric_vs30.inc"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER_FLAGS( PanoramaSSAAResolve, "Help for PanoramaSSAAResolve", SHADER_NOT_EDITABLE )
			  
	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE, 0 );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, false );

			pShaderShadow->EnableAlphaTest( true );
			pShaderShadow->EnableAlphaWrites( true );

			pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GREATER, 1 / 255 );


			// TODO - EnableAlphaBlending with SrcA/InvSrcA if resolving a panorama panel that uses the scene RT and has no MSAA
			// otherwise alpha not resolved correctly to scene RT, general case for SSAA is on a panel using the panel/layer RT and thus 
			// resolving to that, where alpha blending should be disabled
			// if ( SceneRT )
			//    EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			// else
			DisableAlphaBlending();

			pShaderShadow->EnableDepthTest( true );
			pShaderShadow->EnableBlendingSeparateAlpha( false );

			unsigned int flags = VERTEX_POSITION;
			int nTexCoordCount = 1;
			int userDataSize = 0;
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, NULL, userDataSize );

			DECLARE_STATIC_VERTEX_SHADER( unlitgeneric_vs30 );
			SET_STATIC_VERTEX_SHADER_COMBO( VERTEXCOLOR, 0 );
			SET_STATIC_VERTEX_SHADER( unlitgeneric_vs30 );

			DECLARE_STATIC_PIXEL_SHADER( unlitgeneric_ps30 );
			SET_STATIC_PIXEL_SHADER( unlitgeneric_ps30 );
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_NONE, BASETEXTURE );

			float color[ 4 ] = { 1.0f, 1.0f, 1.0f, 1.0f };
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_MODULATION_COLOR, color );

			SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, BASETEXTURETRANSFORM );

			DECLARE_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs30 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, 0 );
			SET_DYNAMIC_VERTEX_SHADER_COMBO( TESSELLATION, 0 );
			SET_DYNAMIC_VERTEX_SHADER( unlitgeneric_vs30 );

			DECLARE_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps30 );
			SET_DYNAMIC_PIXEL_SHADER( unlitgeneric_ps30 );
			
		}
		Draw();
	}
END_SHADER


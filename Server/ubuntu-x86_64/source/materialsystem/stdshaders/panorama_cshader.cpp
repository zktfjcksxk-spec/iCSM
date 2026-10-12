//========= Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ============//


#include "BaseVSShader.h"
#include "shaderlib/cshader.h"

#include "panorama_vs30.inc"
#include "panorama_ps30.inc"


#include "panorama/s1wrapperRenderAttributes.h"


// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"



DEFINE_FALLBACK_SHADER( panorama, panorama_dx9 )
BEGIN_VS_SHADER( panorama_dx9, "Help for panorama" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( BLENDSTATE, SHADER_PARAM_TYPE_INTEGER, "0", "" )
		SHADER_PARAM( RENDERATTR, SHADER_PARAM_TYPE_INTEGER, "0", "" )
#ifdef PLATFORM_64BITS
	SHADER_PARAM( RENDERATTR_HIGH, SHADER_PARAM_TYPE_INTEGER, "0", "" )
#endif
END_SHADER_PARAMS

	SHADER_INIT
	{
// 		if ( params[BASETEXTURE]->IsDefined() )
// 		{
// 			LoadTexture( BASETEXTURE );
// 		}
	}
	
	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_ALWAYS, 0 );
			pShaderShadow->EnableAlphaTest( false );

			pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_ALWAYS );
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableDepthTest( false );

			pShaderShadow->EnableSRGBWrite( true );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			pShaderShadow->EnableAlphaToCoverage( false );
			pShaderShadow->EnableBlending( true );
			pShaderShadow->EnableBlendingSeparateAlpha( true );
			pShaderShadow->EnableColorWrites( true );
			pShaderShadow->EnableAlphaWrites( true );
			pShaderShadow->BlendOp( SHADER_BLEND_OP_ADD );

			int blendState = params[ BLENDSTATE ]->GetIntValue();

			switch ( blendState )
			{

			case BLENDSTATE_ALPHA:
				pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

				// Premultiplied Alpha Blend
			case BLENDSTATE_PREMULT_ALPHA:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

				// Alpha Only Blend
			case BLENDSTATE_ONLY_ALPHA:
				pShaderShadow->BlendFunc( SHADER_BLEND_ZERO, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ZERO, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			case BLENDSTATE_MIX_MULTIPLY:
				pShaderShadow->BlendFunc( SHADER_BLEND_DST_COLOR, SHADER_BLEND_ZERO );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );

				break;

			case BLENDSTATE_MIX_SCREEN:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE_MINUS_DST_COLOR, SHADER_BLEND_ONE );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			case BLENDSTATE_MIX_ADDITIVE:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

 			case BLENDSTATE_MIX_ADDITIVESRGB:
 				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
 				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
 				break;

			case BLENDSTATE_MIX_OPAQUE:
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ZERO );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			default:
				DevWarning( "Invalid blend mode (%d) using pan_dx 0\n", blendState );
				pShaderShadow->BlendFunc( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				pShaderShadow->BlendFuncSeparateAlpha( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				break;

			}

			// Set stream format (note that this shader supports compression)
			unsigned int flags = VERTEX_POSITION;
			int nTexCoordCount = 3;
			int userDataSize = 0;
			static int s_TexCoordSize[] = { 4, 4, 4, 4, 4, 4, 4, 4 };
			pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, s_TexCoordSize, userDataSize );

			DECLARE_STATIC_VERTEX_SHADER( panorama_vs30 );
			SET_STATIC_VERTEX_SHADER( panorama_vs30 );

//			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
//			{
// 				DECLARE_STATIC_PIXEL_SHADER( debugtextureview_ps20b );
// 				SET_STATIC_PIXEL_SHADER_COMBO( SHOWALPHA, params[SHOWALPHA]->GetIntValue() != 0 );
// 				SET_STATIC_PIXEL_SHADER( debugtextureview_ps20b );
//			}
//			else
//			{
			DECLARE_STATIC_PIXEL_SHADER( panorama_ps30 );
//			SET_STATIC_PIXEL_SHADER_COMBO( SHOWALPHA, params[SHOWALPHA]->GetIntValue() != 0 );
			SET_STATIC_PIXEL_SHADER( panorama_ps30 );

			//			}
		}

		DYNAMIC_STATE
		{
#ifdef PLATFORM_64BITS
			CRenderAttributes* pAttr = (CRenderAttributes*)( ( uint64( params[ RENDERATTR_HIGH ]->GetIntValue() ) << 32 ) | ( uint64( params[ RENDERATTR ]->GetIntValue() ) & 0xffffffff ) );
#else
			CRenderAttributes* pAttr = (CRenderAttributes*)params[ RENDERATTR ]->GetIntValue();
#endif

			// Vertex Shader - no longer require VS consts

			// Pixel Shader
			
			Vector4D centerWeight, BlurSigma, BlurMultiplyVec, ParticleSharpness, uvclamp;
			Vector4D sample1, sample2, sample3, sample4, sample5, sample6, sample7, sample8;

			ITexture *pTexture = nullptr;
			pAttr->GetValue( &pTexture, ATTR_Texture0 );
			BindTexture( SHADER_SAMPLER0, TEXTURE_BINDFLAGS_SRGBREAD, pTexture, 0 );

			pAttr->GetValue( &centerWeight, ATTR_centerWeight);
			pAttr->GetValue( &sample1, ATTR_sample1);
			pAttr->GetValue( &sample2, ATTR_sample2);
			pAttr->GetValue( &sample3, ATTR_sample3);
			pAttr->GetValue( &sample4, ATTR_sample4);
			pAttr->GetValue( &sample5, ATTR_sample5);
			pAttr->GetValue( &sample6, ATTR_sample6);
			pAttr->GetValue( &sample7, ATTR_sample7);
			pAttr->GetValue( &sample8, ATTR_sample8);
			pAttr->GetValue( &BlurMultiplyVec, ATTR_BlurMultiplyVec);
			pAttr->GetValue( &BlurSigma, ATTR_BlurSigma);
			pAttr->GetValue( &ParticleSharpness, ATTR_ParticleSharpness);
			pAttr->GetValue( &uvclamp, ATTR_UVClamp );

			pShaderAPI->SetPixelShaderConstant( 0, centerWeight.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 1, sample1.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 2, sample2.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 3, sample3.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 4, sample4.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 5, sample5.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 6, sample6.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 7, sample7.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 8, sample8.Base(), 1 );

			pShaderAPI->SetPixelShaderConstant( 9, BlurMultiplyVec.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 10, BlurSigma.Base(), 1 );
			pShaderAPI->SetPixelShaderConstant( 11, ParticleSharpness.Base(), 1 );

			pShaderAPI->SetPixelShaderConstant( 12, uvclamp.Base(), 1 );

			DECLARE_DYNAMIC_VERTEX_SHADER( panorama_vs30 );
//			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
			SET_DYNAMIC_VERTEX_SHADER( panorama_vs30 );

// 			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
// 			{
//				// Mac ?
// 			}
// 			else
// 			{

			DECLARE_DYNAMIC_PIXEL_SHADER( panorama_ps30 );

			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_TEX2DFASTBLUR, pAttr->GetValue( ATTR_D_TEX2DFASTBLUR ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_TEX2DBLUR, pAttr->GetValue( ATTR_D_TEX2DBLUR ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_TEX2DPARTICLE, pAttr->GetValue( ATTR_D_TEX2DPARTICLE ) );
			SET_DYNAMIC_PIXEL_SHADER_COMBO( D_TEX2DDOWNSAMPLE, pAttr->GetValue( ATTR_D_TEX2DDOWNSAMPLE ) );

			SET_DYNAMIC_PIXEL_SHADER( panorama_ps30 );
//			}
		}
		Draw();
	}
END_SHADER

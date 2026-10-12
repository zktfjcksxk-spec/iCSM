//========= Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "shaderlib/cshader.h"

#include "panoramafancy_vs30.inc"
#include "panoramafancy_ps30.inc"

#include "panorama/s1wrapperRenderAttributes.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DEFINE_FALLBACK_SHADER( panoramafancy, panoramafancy_dx9 )
BEGIN_VS_SHADER( panoramafancy_dx9, "Help for panorama" )
BEGIN_SHADER_PARAMS
SHADER_PARAM( BLENDSTATE, SHADER_PARAM_TYPE_INTEGER, "0", "" )
SHADER_PARAM( RENDERATTR, SHADER_PARAM_TYPE_INTEGER, "0", "" )
#ifdef PLATFORM_64BITS
SHADER_PARAM( RENDERATTR_HIGH, SHADER_PARAM_TYPE_INTEGER, "0", "" )
#endif
END_SHADER_PARAMS

SHADER_INIT
{
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
		pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
		pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );

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
		int nTexCoordCount = 5;
		int userDataSize = 0;
		static int s_TexCoordSize[] = { 4,4,4,4,4,4,4,4 };

		pShaderShadow->VertexShaderVertexFormat( flags, nTexCoordCount, s_TexCoordSize, userDataSize );

		DECLARE_STATIC_VERTEX_SHADER( panoramafancy_vs30 );
		SET_STATIC_VERTEX_SHADER( panoramafancy_vs30 );

		DECLARE_STATIC_PIXEL_SHADER( panoramafancy_ps30 );
		//			SET_STATIC_PIXEL_SHADER_COMBO( SHOWALPHA, params[SHOWALPHA]->GetIntValue() != 0 );
		SET_STATIC_PIXEL_SHADER( panoramafancy_ps30 );

	}

	DYNAMIC_STATE
	{
#ifdef PLATFORM_64BITS
		CRenderAttributes* pAttr = (CRenderAttributes*)( ( uint64( params[ RENDERATTR_HIGH ]->GetIntValue() ) << 32 ) | ( uint64( params[ RENDERATTR ]->GetIntValue() ) & 0xffffffff ) );
#else
		CRenderAttributes* pAttr = (CRenderAttributes*)params[RENDERATTR]->GetIntValue();
#endif

		// Vertex Shader - no longer require VS consts

		// Pixel Shader

		ITexture* pTexture;

		int texType = pAttr->GetValue( ATTR_D_TEXTURETYPE );

		TextureBindFlags_t flags[ 4 ] = { TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BINDFLAGS_SRGBREAD };

		if ( texType )
		{
			if ( texType == 3 )
			{
				flags[0] = TEXTURE_BINDFLAGS_NONE;
				flags[1] = TEXTURE_BINDFLAGS_NONE;
				flags[2] = TEXTURE_BINDFLAGS_NONE;
			}

			pAttr->GetValue( &pTexture, ATTR_Texture0 );
			if ( texType == 4 )
			{
				if ( pTexture->GetFlags() & TEXTUREFLAGS_YCOCG )
				{
					// YCoCg, reset bind flag
					flags[0] = TEXTURE_BINDFLAGS_NONE;
				}
				else
				{
					// texture flagged as having alpha bits, then reset type to RGBA (1)
					texType = 1;
				}
			}
			BindTexture( SHADER_SAMPLER0, flags[0], pTexture, 0 );

			pAttr->GetValue( &pTexture, ATTR_Texture1 );
			BindTexture( SHADER_SAMPLER1, flags[ 1 ], pTexture, 0 );

			pAttr->GetValue( &pTexture, ATTR_Texture2 );
			BindTexture( SHADER_SAMPLER2, flags[ 2 ], pTexture, 0 );
		}
		
		if ( pAttr->GetValue( ATTR_D_GRADIENT_COMPLEX ) )
		{
			pAttr->GetValue( &pTexture, ATTR_Texture3 );
			BindTexture( SHADER_SAMPLER3, flags[ 3 ], pTexture, 0 );
		}

		Vector4D vTopCornerRad, vBtmCornerRad;
		pAttr->GetValue( &vTopCornerRad, ATTR_TopCornerRad );
		pAttr->GetValue( &vBtmCornerRad, ATTR_BtmCornerRad );

		vTopCornerRad += Vector4D( 0.5, 0.5, 0.5, 0.5 );
		vBtmCornerRad += Vector4D( 0.5, 0.5, 0.5, 0.5 );

		Vector4D vWd = pAttr->GetValue( ATTR_BorderWd ) + Vector4D( 0.5, 0.5, 0.5, 0.5 );

		Vector4D vGradientRadialOffset, vOpacityMaskOpacity, vH, vS, vB, vC;
		pAttr->GetValue( &vGradientRadialOffset, ATTR_Gradientradialoffset );
		pAttr->GetValue( &vOpacityMaskOpacity, ATTR_OpacityMaskOpacity );
		pAttr->GetValue( &vH, ATTR_HueShift );
		pAttr->GetValue( &vS, ATTR_Saturation );
		pAttr->GetValue( &vB, ATTR_Brightness );
		pAttr->GetValue( &vC, ATTR_Contrast );

		Vector4D vRadialClipInfo;
		if ( pAttr->GetValue( ATTR_D_USERADIALCLIP ) )
		{
			Vector4D vRadialClipTmp;
			pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipCenterX );	// currently not used
			vRadialClipInfo.x = vRadialClipTmp.x;
			pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipCenterY ); // currently not used
			vRadialClipInfo.y = vRadialClipTmp.y;
			pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipStartAngle );
			vRadialClipInfo.z = vRadialClipTmp.z;
			pAttr->GetValue( &vRadialClipTmp, ATTR_RadialClipSectorAngle );
			vRadialClipInfo.w = vRadialClipTmp.w;
		}

		// packing PS consts
		vGradientRadialOffset.z = vOpacityMaskOpacity.x;
		vGradientRadialOffset.w = 0.0f; // unused
		vH.y = vS.x;
		vH.z = vB.x;
		vH.w = vC.x;

 		pShaderAPI->SetPixelShaderConstant( 0, vTopCornerRad.Base(), 1 );
 		pShaderAPI->SetPixelShaderConstant( 1, vBtmCornerRad.Base(), 1 );
 		pShaderAPI->SetPixelShaderConstant( 2, vWd.Base(), 1 );
		pShaderAPI->SetPixelShaderConstant( 3, pAttr->GetValue( ATTR_Bordercolor).Base(), 1 );
		pShaderAPI->SetPixelShaderConstant( 4, vGradientRadialOffset.Base(), 1 );  // w currently not used
		pShaderAPI->SetPixelShaderConstant( 5, vH.Base(), 1 );
		pShaderAPI->SetPixelShaderConstant( 6, vRadialClipInfo.Base(), 1 ); // xy currently not used but reserved for radial clip center

		// Combos

		DECLARE_DYNAMIC_VERTEX_SHADER( panoramafancy_vs30 );
		//			SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (int)vertexCompression );
		SET_DYNAMIC_VERTEX_SHADER( panoramafancy_vs30 );

		DECLARE_DYNAMIC_PIXEL_SHADER( panoramafancy_ps30 );

		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_TEXTURETYPE,			texType );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_PREMULTIPLY_ALPHA,	pAttr->GetValue( ATTR_D_PREMULTIPLY_ALPHA ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USERADIALGRADIENT,	pAttr->GetValue( ATTR_D_USERADIALGRADIENT ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USEOUTERCORNER, pAttr->GetValue( ATTR_D_USEOUTERCORNER ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USEINNERCORNER, pAttr->GetValue( ATTR_D_USEINNERCORNER ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_COLORCORRECTION,		pAttr->GetValue( ATTR_D_COLORCORRECTION ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USEOPACITYMASK,		pAttr->GetValue( ATTR_D_USEOPACITYMASK ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_GRADIENT_TWOSTOP,		pAttr->GetValue( ATTR_D_GRADIENT_TWOSTOP ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_GRADIENT_COMPLEX,		pAttr->GetValue( ATTR_D_GRADIENT_COMPLEX ) );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( D_USERADIALCLIP, pAttr->GetValue( ATTR_D_USERADIALCLIP ) );


		SET_DYNAMIC_PIXEL_SHADER( panoramafancy_ps30 );
	}
	Draw();
}
END_SHADER

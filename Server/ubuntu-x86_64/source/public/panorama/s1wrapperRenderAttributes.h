#ifndef INCLUDED_S1WRAPPERRENDERATTRIBUTES_H
#define INCLUDED_S1WRAPPERRENDERATTRIBUTES_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// Render attrubutes
//--------------------------------------------------------------------------------------------------

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "filesystem/iasyncfilesystem.h"
#include "materialsystem/itexture.h"
#include "mathlib/vector4d.h"
#include "mathlib/vmatrix.h"

#ifndef PANORAMA_USE_S1WRAPPER
enum RsBlendStateHandle_t
{
	BLENDSTATE_ALPHA = 1,
	BLENDSTATE_PREMULT_ALPHA,
	BLENDSTATE_ONLY_ALPHA,
	BLENDSTATE_MIX_SCREEN,
	BLENDSTATE_MIX_MULTIPLY,
	BLENDSTATE_MIX_ADDITIVE,
	BLENDSTATE_MIX_ADDITIVESRGB,
	BLENDSTATE_MIX_OPAQUE,
	BLENDSTATE_MAX

};

#endif

// Harcoded render attributes to match panormama shaders
// Float, Vector2D and Vector 4D are all stored together.

enum RenderAttrFloat_t
{
	ATTR_FLOAT_MIN = 0,
	ATTR_BlurSigma = ATTR_FLOAT_MIN,
	ATTR_Brightness,
	ATTR_Contrast,
	ATTR_HueShift,
	ATTR_MotionBlurVelocityScale,
	ATTR_OpacityMaskOpacity,
	ATTR_ParticleSharpness,
	ATTR_RadialClipBlendStepX,
	ATTR_RadialClipBlendStepY,
	ATTR_RadialClipCenterX,
	ATTR_RadialClipCenterY,
	ATTR_RadialClipSectorAngle,
	ATTR_RadialClipStartAngle,
	ATTR_Saturation,
	ATTR_ViewportHeight,
	ATTR_ViewportWidth,
	ATTR_centerWeight,
	ATTR_FLOAT_MAX
};

enum RenderAttrVector2D_t
{
	ATTR_VECTOR2D_MIN = ATTR_FLOAT_MAX,
	ATTR_BlurMultiplyVec = ATTR_VECTOR2D_MIN,
	ATTR_UVClamp,
	ATTR_VECTOR2D_MAX
};

enum RenderAttrVector4D_t
{
	ATTR_VECTOR4D_MIN = ATTR_VECTOR2D_MAX,
	ATTR_TopLeftWdHt = ATTR_VECTOR4D_MIN,
	ATTR_TopCornerRad,
	ATTR_BtmCornerRad,
	ATTR_BorderWd,
	ATTR_Bordercolor,
	ATTR_Gradientradialoffset,
	ATTR_Panorama_ScissorRect,
	ATTR_sample0, 
	ATTR_sample1, 
	ATTR_sample2, 
	ATTR_sample3, 
	ATTR_sample4, 
	ATTR_sample5, 
	ATTR_sample6, 
	ATTR_sample7, 
	ATTR_sample8,
	ATTR_VECTOR4D_MAX
};

enum RenderAttrVMatrix_t
{
	ATTR_VMATRIX_MIN = ATTR_VECTOR4D_MAX,
	ATTR_MatTransform,
	ATTR_VMATRIX_MAX
};

enum RenderAttrInt_t
{
	ATTR_INT_MIN = ATTR_VMATRIX_MAX,
	ATTR_D_COLORCORRECTION,
	ATTR_D_ENABLEMOTIONBLUR,
	ATTR_D_GRADIENT_COMPLEX,
	ATTR_D_GRADIENT_TWOSTOP,
	ATTR_D_GRADIENTTYPE,
	ATTR_D_PREMULTIPLY_ALPHA,
	ATTR_D_TEX2DBLUR,
	ATTR_D_TEX2DFASTBLUR,
	ATTR_D_TEX2DPARTICLE,
	ATTR_D_TEX2DDOWNSAMPLE,
	ATTR_D_TEXTURETYPE,
	ATTR_D_TEXTURETYPE_NONE,
	ATTR_D_USEINNERCORNER,
	ATTR_D_USEOPACITYMASK,
	ATTR_D_USEOUTERCORNER,
	ATTR_D_USERADIALCLIP,
	ATTR_D_USERADIALGRADIENT,
	ATTR_D_FAST_BLUR_SAMPLES,
	ATTR_INT_MAX
};


enum RenderAttrTexture_t
{
	ATTR_TEXTURE_MIN = ATTR_INT_MAX,
	ATTR_Texture0,
	ATTR_Texture1,
	ATTR_Texture2,
	ATTR_Texture3,
	ATTR_Panorama_OutputColor,
	ATTR_Panorama_OutputIntermediate,
	ATTR_Panorama_OutputDepth,
	ATTR_TEXTURE_MAX
};

class CRenderAttributes
{
public:

	//--------------------------------------------------------------------------------------------------
	// SetValues
	//--------------------------------------------------------------------------------------------------

	//-----------------------------------------------
	// Float - Uses the Vector4D attribute array
	//-----------------------------------------------
	FORCEINLINE void SetFloatValue( RenderAttrFloat_t nAttr, float flValue )
	{
		if ( ( nAttr >= ATTR_FLOAT_MIN ) && ( nAttr < ATTR_FLOAT_MAX ) )
		{
			m_vec4Attrs[ nAttr ] = Vector4D( flValue, flValue, flValue, flValue );
		}
	}

	//-----------------------------------------------
	// Vector2D - Uses the Vector4D attribute list
	//-----------------------------------------------
	FORCEINLINE void SetVector2DValue( RenderAttrVector2D_t nAttr, Vector2D vValue )
	{
		if ( ( nAttr >= ATTR_VECTOR2D_MIN ) && ( nAttr < ATTR_VECTOR2D_MAX ) )
		{
			m_vec4Attrs[ nAttr ] = Vector4D( vValue.x, vValue.y, 0.0f, 0.0f );
		}
	}

	//-----------------------------------------------
	// Vector4D
	//-----------------------------------------------
	FORCEINLINE void SetVector4DValue( RenderAttrVector4D_t nAttr, Vector4D vValue )
	{
		if ( ( nAttr >= ATTR_VECTOR4D_MIN ) && ( nAttr < ATTR_VECTOR4D_MAX ) )
		{
			m_vec4Attrs[ nAttr ] = vValue;
		}
	}

	//-----------------------------------------------
	// VMatrix
	//-----------------------------------------------
	FORCEINLINE void SetVMatrixValue( RenderAttrVMatrix_t nAttr, VMatrix vValue )
	{
		if ( ( nAttr >= ATTR_VMATRIX_MIN ) && ( nAttr < ATTR_VMATRIX_MAX ) )
		{
			m_matrix4x4Attrs[ nAttr - ATTR_VMATRIX_MIN ] = vValue;
		}
	}

	//-----------------------------------------------
	// Int
	//-----------------------------------------------
	FORCEINLINE void SetIntValue( RenderAttrInt_t nAttr, int nValue )
	{
		if ( ( nAttr >= ATTR_INT_MIN ) && ( nAttr < ATTR_INT_MAX ) )
		{
			m_intAttrs[nAttr - ATTR_INT_MIN ] = nValue;
		}
	}

	//-----------------------------------------------
	// Textures
	//-----------------------------------------------
#ifdef PANORAMA_USE_S1WRAPPER
	void SetTextureValue( RenderAttrTexture_t nAttr, HRenderTexture txtr );
#endif

	//-----------------------------------------------
	// Blur
	//-----------------------------------------------
	FORCEINLINE void AddBlurRect( const Vector4D &vRect, const VMatrix &vMat )
	{
		m_blurAttrs.AddToTail( { vRect, vMat } );
	}

	//--------------------------------------------------------------------------------------------------
	// Get Values
	//--------------------------------------------------------------------------------------------------

	//-----------------------------------------------
	// Vector4D
	//-----------------------------------------------

	FORCEINLINE void GetValue( Vector4D* vValue, RenderAttrFloat_t nAttr )
	{
		if ( ( nAttr >= ATTR_FLOAT_MIN ) && ( nAttr < ATTR_FLOAT_MAX ) )
		{
			*vValue = m_vec4Attrs[ nAttr ];
		}
	}

	FORCEINLINE void GetValue( Vector4D* vValue, RenderAttrVector2D_t nAttr )
	{
		if ( ( nAttr >= ATTR_VECTOR2D_MIN ) && ( nAttr < ATTR_VECTOR2D_MAX ) )
		{
			*vValue = m_vec4Attrs[ nAttr ];
		}
	}

	FORCEINLINE void GetValue( Vector4D* vValue, RenderAttrVector4D_t nAttr )
	{
		if ( ( nAttr >= ATTR_VECTOR4D_MIN ) && ( nAttr < ATTR_VECTOR4D_MAX ) )
		{
			*vValue = m_vec4Attrs[ nAttr ];
		}
	}

	FORCEINLINE Vector4D GetValue( RenderAttrVector4D_t nAttr )
	{
		if ( ( nAttr >= ATTR_VECTOR4D_MIN ) && ( nAttr < ATTR_VECTOR4D_MAX ) )
		{
			return m_vec4Attrs[ nAttr ];
		}
		else
		{
			return Vector4D( 0, 0, 0, 0 );
		}
	}

	FORCEINLINE Vector4D GetValue( RenderAttrVector2D_t nAttr )
	{
		if ( ( nAttr >= ATTR_VECTOR2D_MIN ) && ( nAttr < ATTR_VECTOR2D_MAX ) )
		{
			return m_vec4Attrs[ nAttr ];
		}
		else
		{
			return Vector4D( 0, 0, 0, 0 );
		}
	}

	FORCEINLINE Vector4D GetValue( RenderAttrFloat_t nAttr )
	{
		if ( ( nAttr >= ATTR_FLOAT_MIN ) && ( nAttr < ATTR_FLOAT_MAX ) )
		{
			return m_vec4Attrs[ nAttr ];
		}
		else
		{
			return Vector4D( 0, 0, 0, 0 );
		}
	}



	//-----------------------------------------------
	// VMatrix
	//-----------------------------------------------
	FORCEINLINE void GetValue( VMatrix *vValue, RenderAttrVMatrix_t nAttr )
	{
		if ( ( nAttr >= ATTR_VMATRIX_MIN ) && ( nAttr < ATTR_VMATRIX_MAX ) )
		{
			*vValue = m_matrix4x4Attrs[ nAttr - ATTR_VMATRIX_MIN ];
		}
	}

	//-----------------------------------------------
	// Int
	//-----------------------------------------------
	FORCEINLINE bool GetValue( int* pInt, RenderAttrInt_t nAttr )
	{
		if ( ( nAttr >= ATTR_INT_MIN ) && ( nAttr < ATTR_INT_MAX ) )
		{
			*pInt = m_intAttrs[ nAttr - ATTR_INT_MIN ];
		}

		return ( *pInt != 0 );
	}

	FORCEINLINE int GetValue( RenderAttrInt_t nAttr )
	{
		if ( ( nAttr >= ATTR_INT_MIN ) && ( nAttr < ATTR_INT_MAX ) )
		{
			return m_intAttrs[ nAttr - ATTR_INT_MIN ];
		}
		else
		{
			return 0;
		}
	}

	//-----------------------------------------------
	// Texture
	//-----------------------------------------------
	FORCEINLINE void GetValue( ITexture** ppTex, RenderAttrTexture_t nAttr )
	{
		if ( ( nAttr >= ATTR_TEXTURE_MIN ) && ( nAttr < ATTR_TEXTURE_MAX ) )
		{
			*ppTex = m_textureAttrs[ nAttr - ATTR_TEXTURE_MIN ];
		}
	}
		
	FORCEINLINE ITexture* GetValue( RenderAttrTexture_t nAttr)
	{
		if ( ( nAttr >= ATTR_TEXTURE_MIN ) && ( nAttr < ATTR_TEXTURE_MAX ) )
		{
			return m_textureAttrs[ nAttr - ATTR_TEXTURE_MIN ];
		}
		else
		{
			return 0;
		}
	}

	//-----------------------------------------------
	// Blur
	//-----------------------------------------------
	FORCEINLINE void GetTargetBlurRect( Vector4D* vRect, VMatrix *vMatrix )
	{
		if ( m_blurAttrs.Count() )
		{
			const BlurAttributes_t &blurAttr = m_blurAttrs[0];
			if ( vRect )
			{
				*vRect = blurAttr.m_vecRect;
			}
			if ( vMatrix )
			{
				*vMatrix = blurAttr.m_matrix4x4;
			}
		}
	}

	FORCEINLINE int GetNumSourceBlurRects()
	{
		// Do not count first element of m_blurAttrs as it represents the target rect
		return Max( 0, ( m_blurAttrs.Count() - 1 ) );
	}

	FORCEINLINE void GetSourceBlurRect( Vector4D* vRect, VMatrix *vMatrix, int nIndex )
	{
		// Ignore first element of m_blurAttrs as it represents the target rect
		int nSourceRectIndex = nIndex + 1;
		if ( nSourceRectIndex < m_blurAttrs.Count() )
		{
			const BlurAttributes_t &blurAttr = m_blurAttrs[nSourceRectIndex];
			if ( vRect )
			{
				*vRect = blurAttr.m_vecRect;
			}
			if ( vMatrix )
			{
				*vMatrix = blurAttr.m_matrix4x4;
			}
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Clear
	// Integer attributes are set to 0
	//--------------------------------------------------------------------------------------------------

	void Clear( bool bFreeMem )
	{
		memset( m_vec4Attrs, 0, ARRAYSIZE( m_vec4Attrs ) * sizeof ( Vector4D ) );
		memset( m_matrix4x4Attrs, 0, ARRAYSIZE( m_matrix4x4Attrs ) * sizeof( VMatrix ) );
		memset( m_intAttrs, 0, ARRAYSIZE( m_intAttrs ) * sizeof( int ) );
		memset( m_textureAttrs, 0, ARRAYSIZE( m_textureAttrs ) * sizeof( ITexture* ) );
		m_blurAttrs.RemoveAll();
	}

	//-----------------------------------------------
	// Constructor & Destructor
	//-----------------------------------------------
	CRenderAttributes( void )
	{
		Clear( false );
	}

	~CRenderAttributes( void )
	{
	}

	Vector4D	m_vec4Attrs[ ATTR_VECTOR4D_MAX ];
	VMatrix		m_matrix4x4Attrs[ ATTR_VMATRIX_MAX - ATTR_VMATRIX_MIN ];
	int			m_intAttrs[ATTR_INT_MAX - ATTR_INT_MIN];
	ITexture*	m_textureAttrs[ ATTR_TEXTURE_MAX - ATTR_TEXTURE_MIN ];

	struct BlurAttributes_t
	{
		Vector4D m_vecRect;
		VMatrix m_matrix4x4;
	};
	// m_blurAttrs[ 0 ]   -> target rect
	// m_blurAttrs[ i>0 ] -> source rectangles
	CUtlVectorFixedGrowable< BlurAttributes_t, 8 > m_blurAttrs;
};

#endif // INCLUDED_S1WRAPPERRENDERATTRIBUTES_H
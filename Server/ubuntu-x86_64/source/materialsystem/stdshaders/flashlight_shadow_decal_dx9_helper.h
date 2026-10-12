//========= Copyright (c) 1996-2007, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FLASHLIGHT_SHADOW_DECAL_DX9_HELPER_H
#define FLASHLIGHT_SHADOW_DECAL_DX9_HELPER_H

#include <string.h>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseVSShader;
class IMaterialVar;
class IShaderDynamicAPI;
class IShaderShadow;


//-----------------------------------------------------------------------------
// Init params/ init/ draw methods
//-----------------------------------------------------------------------------
struct FlashlightShadowDecal_DX9_Vars_t
{
	FlashlightShadowDecal_DX9_Vars_t( ) { memset( this, 0xFF, sizeof( *this ) ); }

//	int m_nAlphaTestReference;
	int m_nFlashlightNoLambert;
//	int m_nFlashlightTexture;
//	int m_nFlashlightTextureFrame;

	int m_nLinearWrite;
};

void InitParamsFlashlightShadowDecal_DX9( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, FlashlightShadowDecal_DX9_Vars_t &info );
void InitFlashlightShadowDecal_DX9( CBaseVSShader *pShader, IMaterialVar** params, FlashlightShadowDecal_DX9_Vars_t &info );
void DrawFlashlightShadowDecal_DX9( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI, IShaderShadow* pShaderShadow,
									FlashlightShadowDecal_DX9_Vars_t &info, VertexCompressionType_t vertexCompression,
									CBasePerMaterialContextData **pContextDataPtr );

#endif // VERTEXLITGENERIC_DX9_HELPER_H

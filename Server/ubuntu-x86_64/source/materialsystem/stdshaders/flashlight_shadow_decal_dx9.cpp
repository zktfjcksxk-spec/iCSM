//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=====================================================================================//

#include "BaseVSShader.h"
#include "flashlight_shadow_decal_dx9_helper.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_VS_SHADER( flashlight_shadow_decal, "Help for FlashlightShadowDecal" )
	BEGIN_SHADER_PARAMS

//		SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )	
		SHADER_PARAM( FLASHLIGHTNOLAMBERT, SHADER_PARAM_TYPE_BOOL, "0", "Flashlight pass sets N.L=1.0" )

		SHADER_PARAM( LINEARWRITE, SHADER_PARAM_TYPE_INTEGER, "0", "Disables SRGB conversion of shader results." )

	END_SHADER_PARAMS

	void SetupVars( FlashlightShadowDecal_DX9_Vars_t& info )
	{
		info.m_nFlashlightNoLambert = FLASHLIGHTNOLAMBERT;
///		info.m_nLowQualityFlashlightShadows = LOWQUALITYFLASHLIGHTSHADOWS;
		info.m_nLinearWrite = LINEARWRITE;
	}

	SHADER_INIT_PARAMS()
	{
		FlashlightShadowDecal_DX9_Vars_t vars;
		SetupVars( vars );
		InitParamsFlashlightShadowDecal_DX9( this, params, pMaterialName, vars );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		FlashlightShadowDecal_DX9_Vars_t vars;
		SetupVars( vars );
		InitFlashlightShadowDecal_DX9( this, params, vars );
	}

	SHADER_DRAW
	{
		FlashlightShadowDecal_DX9_Vars_t vars;
		SetupVars( vars );
		DrawFlashlightShadowDecal_DX9( this, params, pShaderAPI, pShaderShadow, vars, vertexCompression, pContextDataPtr );
	}


END_SHADER

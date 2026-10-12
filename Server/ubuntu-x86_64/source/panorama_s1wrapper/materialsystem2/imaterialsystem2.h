#ifndef INCLUDED_IMATERIALSYSTEM2_H
#define INCLUDED_IMATERIALSYSTEM2_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// panormam material handles
//--------------------------------------------------------------------------------------------------

#define PANORAMA_MATERIAL_INVALID	0
#define PANORAMA_MATERIAL			1
#define PANORAMA_MATERIAL_FANCYQUAD 2

//---------------------------------------------------------------------------------------------------------------------------------------------------
abstract_class IMaterialLayer
{
public:
	// Need calls to get shader handle and anything else that will help the renderer bucket passes

};

#define MATERIAL_RENDERABLE_PASS_MAX 8 // For code that wants to hard-code the array size to pass into ComputeRenderablePassesForContext() below

enum MaterialShaderProgram_t
{
	MATERIAL_SHADER_PROGRAM_VS = 0,
	MATERIAL_SHADER_PROGRAM_PS,
	MATERIAL_SHADER_PROGRAM_GS,
	MATERIAL_SHADER_PROGRAM_HS,
	MATERIAL_SHADER_PROGRAM_DS,

	MATERIAL_SHADER_PROGRAM_MAX,
};

struct MaterialRenderablePass_t
{
	const IMaterialLayer *pLayer;
	//MaterialDataFromClient *data;

	// TODO: This data needs to be compacted
	int nLayerIndex;
	int nPassIndex;

	// TODO: Change this to shader handles
	uint64 staticComboIdArray[ MATERIAL_SHADER_PROGRAM_MAX ]; // Should these be a 16-bit index to a post-skipped combo or anything smaller than 64-bits?
	uint64 dynamicComboIdArray[ MATERIAL_SHADER_PROGRAM_MAX ];

	uint nFrameCount;
};


//---------------------------------------------------------------------------------------------------------------------------------------------------
abstract_class IMaterialMode
{
public:
	// This gets the max number of passes this mode will ever render
	virtual int GetTotalNumPasses() const = 0;

	// NOTE: This data cannot be used across frame boundaries! The data can be reused within a single frame but not across frames.
	// This must be called to converge on a dynamic combo before calling g_pMaterialSystem2->SetRenderStateForRenderablePass().
	// The array size must be at least GetTotalNumPasses() large, but using MATERIAL_RENDERABLE_PASS_MAX will always work.
	virtual int ComputeRenderablePassesForContext( const CRenderAttributes *pAttributes, IRenderContext *pRenderContext, MaterialRenderablePass_t *pRenderablePassArray, int nRenderablePassArraySize = MATERIAL_RENDERABLE_PASS_MAX ) = 0;
	virtual int ComputeRenderablePassesForContext( const CRenderAttributes *pAttributes, const CRenderAttributes *pAttributes2, MaterialRenderablePass_t *pRenderablePassArray, int nRenderablePassArraySize = MATERIAL_RENDERABLE_PASS_MAX ) = 0;

};

abstract_class IMaterial2
{
public:
	// Get the name of the material
	virtual const char *GetName() const = 0;
	virtual const char *GetNameWithMod() const = 0;

	// When async loading, this will let the caller know everything is loaded
	virtual bool IsLoaded() const = 0;

	//	// The number of modes is constant for all materials and is defined by the client and stored in the material system.
	//	// Returning NULL means this is an unsupported mode. A non-NULL mode can still have 0 passes depending on game state, so
	//	// NULL here means the mode isn't supported by this material and it is up to the caller to decide what to do.
	//	virtual IMaterialMode *GetMode( const CUtlStringToken &shaderMode = CUtlStringToken() ) const = 0;

	virtual IMaterialMode *GetMode() const = 0;


	// Get the per-material attribute table (read-only)
	//	virtual const CRenderAttributes &GetAttributes() const = 0;
	//
	//	// Get the vertex shader input signature expected by this material
	virtual const CVsInputSignatureVector *GetVertexShaderInputSignature() const = 0;

	// Indicates whether the material can be set up once and reused for several consecutive draw calls, or whether the 
	// material has to set up custom state for each draw call.
	//virtual bool IsBatchableInMode( const CUtlStringToken &shaderMode ) const = 0;

	// Backward compat for S1
	virtual int GetMappingWidth() const = 0;
	virtual int GetMappingHeight() const = 0;
	virtual int GetNumAnimationFrames() const = 0;

	// This call will change the material globally. Any geometry using the material will render with the new texture.
	virtual bool OverrideTextureParam( const char *pParamName, HRenderTexture hNewTex ) = 0;

	// A little helper to simplify setting up the renderpass array for a mode in one step. Returns
	// the number of passes. If there's an error, prints a warning and returns 0 passes. Calls
	// pMaterial->GetMode() and ComputeRenderablePassesForContext.  Potentially could save one
	// virtual call per draw if changed to directly access those functions.  The pass data array
	// should be declared like MaterialRenderablePass_t xxx[ MATERIAL_RENDERABLE_PASS_MAX ];
	// pAttrs may be NULL.
	//	FORCEINLINE int ComputePassData( const CUtlStringToken &mode, IRenderContext *pRenderContext,
	//									 MaterialRenderablePass_t *pPassArray, CRenderAttributes const *pAttrs = NULL  ) const;

private:

	// Allow functions like OverrideTextureParam to be called. 
	// Do not call directly; rather call use a CMaterialOverrideScopeGuard.
	//	virtual void EnterOverrideScope() = 0;
	//	virtual void LeaveOverrideScope() = 0;
	//	friend class CMaterialOverrideScopeGuard;
};


abstract_class IMaterialSystem2
{
public:
	//	// Create a material from a resource
	virtual HMaterial FindOrCreateMaterialFromResource( const char *pResourceFileName ) = 0;
	//
	//	// renderablePass is created by IMaterial2->IMaterialMode->ComputeRenderablePassesForContext()
	virtual void SetRenderStateForRenderablePass( const CRenderAttributes *pAttributes, IRenderContext *pRenderContext, RenderInputLayout_t hInputLayout, const MaterialRenderablePass_t &renderablePass, RsStencilStateOverride_t const *pOverrideStencilState = NULL, int nRequiredMipSize = INT_MAX ) const = 0;

	virtual uint32 GetCurrentMaterialGeneration() = 0;

};

#define g_pMaterialSystem2 g_pMaterialSystem3
extern IMaterialSystem2* g_pMaterialSystem2;


#endif // INCLUDED_IMATERIALSYSTEM2_H
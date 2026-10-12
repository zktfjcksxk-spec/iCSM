#ifndef INCLUDED_ISCENESYSTEM_H
#define INCLUDED_ISCENESYSTEM_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

/// A ISceneLayer represents a rendering pass. The layer parameters will decide which objects get
/// routed to it, what render mode it uses, the rendertargets and viewports, etc.
class ISceneLayer
{
public:
	virtual const RenderTargetDesc_t &GetRenderTargetDesc() const = 0;
};

// Scene System


enum SceneSystemRenderTargetSize_t
{
	SCENE_RTSIZE_128 = 0,	// currently unused
	SCENE_RTSIZE_256,	// light propagation volumes only
	SCENE_RTSIZE_512,	// also MONITOR_RTSIZE and SHADOW_RTSIZE; technically used for VSM shadows but the code looks like it has rotted
	SCENE_RTSIZE_FRAMEBUFFER,	// vgui (SCENE_RTGT_SCRATCH_TEXTURE_8888), scenesystem (SCENE_RTGT_SCRATCH_DEPTHSTENCIL_TEXTURE) for read-only depth stencil fallback. scenesystem (SCENE_RTGT_SCRATCH_TEXTURE_8888) for refract FB copy
	SCENE_RTSIZE_2K,	// also BAKEDSHADOW_SRC_SIZE. Only used for experimental scenesystem light shadow baking.

	SCENE_RTSIZE_COUNT,
	SCENE_RTSIZE_INVALID
};

/// "scratch" 256x256 render target textures
enum SceneSystemWellKnownRenderTargetID_t
{
	/// a scratch render target texture for use for monitor rendering and other temp storage
	SCENE_RTGT_SCRATCH_TEXTURE_8888,
	/// a single channel FP32 scratch render target.  URRENTLY UNUSED.
	//SCENE_RTGT_SCRATCH_TEXTURE_FP32,
	/// a 10/10/10/2 scratch render target suitable for storing normals
	SCENE_RTGT_SCRATCH_TEXTURE_1010102,
	/// a scratch fp16 rendertarget suitable for lighting accumulation. CURRENTLY UNUSED.
	//SCENE_RTGT_SCRATCH_TEXTURE_A_16161616F,
	/// and another for specular. CURRENTLY UNUSED.
	//SCENE_RTGT_SCRATCH_TEXTURE_B_16161616F,
	/// a depth texture/buffer to use (for instance, for shadows or monitors )
	SCENE_RTGT_SCRATCH_DEPTHSTENCIL_TEXTURE,
	/// a 2 channel fp16 texture for variance shadow mapping
	SCENE_RTGT_SCRATCH_TEXTURE_16F16F,
	/// another 2 channel fp16 texture for variance shadow mapping to use a a target for gaussian blur temp. CURRENTLY UNUSED.
	//SCENE_RTGT_SCRATCH_TEXTURE_16F16F_B,
	/// need a resolve target for now from the first FP32 to this FP32. CURRENTLY UNUSED.
	//SCENE_RTGT_SCRATCH_TEXTURE2_FP32,
	/// depth buffer with mip chain for Scalable Ambient Obscurance SSAO algorithm
	SCENE_RTGT_SCRATCH_SCALABLE_AMBIENT_OBSCURANCE_MIPPED_DEPTH,

	/// temp buffer for screenspace reflections
	SCENE_RTGT_SCRATCH_TEXTURE_16161616F_MIPPED,

	SCENE_RTGT_COUNT
};


class ISceneSystem
{
	//	friend class CSceneObject;

public:
	virtual HRenderTexture GetWellKnownRenderTarget( SceneSystemWellKnownRenderTargetID_t nID, SceneSystemRenderTargetSize_t nSize, int nIndex ) = 0;
};

#define g_pSceneSystem g_pSceneSystem2
extern ISceneSystem* g_pSceneSystem;

class ISceneUtils
{
public:
	// Calculates the set of sample weights and offsets to generate a gaussian blur from a sum of linear texture samples
	// returns the center weight in pSamplesWeights[0] and 0.0 in pSampleOffsets[0]
	// for decent quality, nSampleArraySize should be at least ceil( flBlurRadius / 2.0f ) + 1, but ceil( flBlurRadius / 1.5f ) + 1 is even better
	virtual int CalculateLinearWeightsForGaussianBlur( float flBlurRadius, float *pSampleWeights, float *pSampleOffsets, int nSampleArraySize, float flCutoff = 0.004f ) = 0;
};

extern ISceneUtils* g_pSceneUtils;

#endif // INCLUDED_ISCENESYSTEM_H
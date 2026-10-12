//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "s1wrapper.h"
#include "rendersystem/irenderhardwareconfig.h"
#include "rendersystem/irendercontext.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "wrap_texture.h"

#include "panorama/panoramatypes.h"

#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
// material system
//--------------------------------------------------------------------------------------------------

class CMaterialSystem2 : IMaterialSystem2
{
public:
	virtual HMaterial FindOrCreateMaterialFromResource( const char *pResourceFileName );
	virtual void SetRenderStateForRenderablePass( const CRenderAttributes *pAttributes, IRenderContext *pRenderContext, RenderInputLayout_t hInputLayout, const MaterialRenderablePass_t &renderablePass, RsStencilStateOverride_t const *pOverrideStencilState = NULL, int nRequiredMipSize = INT_MAX ) const
	{}

	virtual uint32 GetCurrentMaterialGeneration() {
		return 0;
	}

};

CMaterialSystem2 g_MaterialSystem2;
IMaterialSystem2* g_pMaterialSystem2 = (IMaterialSystem2*)&g_MaterialSystem2;

class CMaterial2 : public IMaterial2, public IMaterialMode
{
public:
	// Constructor and destructor
	CMaterial2( int hMaterial ) { m_nMaterialId = hMaterial; }
	virtual ~CMaterial2(){}

	//-----------------------//
	// Methods of IMaterial2 //
	//-----------------------//
	virtual const char *GetName() const OVERRIDE;
	virtual const char *GetNameWithMod() const OVERRIDE{ return GetName(); }
	virtual bool IsLoaded() const OVERRIDE{ return ( m_nMaterialId != PANORAMA_MATERIAL_INVALID ); }
	virtual IMaterialMode *GetMode() const OVERRIDE{ return ( IMaterialMode * )this; }

	virtual const CVsInputSignatureVector *GetVertexShaderInputSignature() const { return 0; }
	virtual int ComputeRenderablePassesForContext( const CRenderAttributes *pAttributes, const CRenderAttributes *pAttributes2, MaterialRenderablePass_t *pRenderablePassArray, int nRenderablePassArraySize = MATERIAL_RENDERABLE_PASS_MAX ) { return 0; }



	// Not called..

	virtual int GetMappingWidth() const OVERRIDE{ return 0; }
	virtual int GetMappingHeight() const OVERRIDE{ return 0; }
	virtual int GetNumAnimationFrames() const OVERRIDE{ return 0; }
	virtual bool OverrideTextureParam( const char *pParamName, HRenderTexture hTex ) OVERRIDE{ return false; }

	//-----------------------//
	// Methods of IMaterialMode //
	//-----------------------//
	virtual int GetTotalNumPasses() const OVERRIDE{ return 1; }
	virtual int ComputeRenderablePassesForContext( const CRenderAttributes *pAttributes, IRenderContext *pRenderContext, MaterialRenderablePass_t *pRenderablePassArray, int nRenderablePassArraySize = MATERIAL_RENDERABLE_PASS_MAX ) OVERRIDE;

	int m_nMaterialId;
};

const char * CMaterial2::GetName() const
{
	switch ( m_nMaterialId )
	{
	case PANORAMA_MATERIAL:
		return "Panorama Material";
	case PANORAMA_MATERIAL_FANCYQUAD:
		return "Panorama Fancyquad Material";
	}
	return "Invalid Material";
}

int CMaterial2::ComputeRenderablePassesForContext( const CRenderAttributes *pAttributes, IRenderContext *pSrc2RenderContext, MaterialRenderablePass_t *pRenderablePassArray, int nRenderablePassArraySize )
{
	// Ok, we store the material here
	((CRenderContext*)pSrc2RenderContext)->m_nPanMaterial = m_nMaterialId;
	// Plus the attributes

	( (CRenderContext*)pSrc2RenderContext )->m_pAttr = (CRenderAttributes *)pAttributes;

	return 1;
}

static ResourceData_t s_panoramaMaterialResourceData = { PANORAMA_MATERIAL, RESOURCE_TYPE_MATERIAL };
static ResourceData_t s_panoramaFancyMaterialResourceData = { PANORAMA_MATERIAL_FANCYQUAD, RESOURCE_TYPE_MATERIAL };

HMaterial CMaterialSystem2::FindOrCreateMaterialFromResource( const char *pResourceFileName )
{
	HMaterial hMaterial = RESOURCE_HANDLE_INVALID;
	if ( V_strstr( pResourceFileName, "panorama_fancyquad.vmat" ) )
	{
		hMaterial = &s_panoramaFancyMaterialResourceData;
	}
	else if ( V_strstr( pResourceFileName, "panorama.vmat" ) )
	{
		hMaterial = &s_panoramaMaterialResourceData;
	}
	return hMaterial;
}

static CMaterial2 s_panoramaMaterial( PANORAMA_MATERIAL );
static CMaterial2 s_panoramaFancyQuadMaterial( PANORAMA_MATERIAL_FANCYQUAD );

CResourceStrongHandle::operator const IMaterial2*( )
{
	if ( IsLoaded() && ( m_hResource->m_nType == RESOURCE_TYPE_MATERIAL ) )
	{
		switch ( m_hResource->m_handle )
		{
		case PANORAMA_MATERIAL:
			return &s_panoramaMaterial;
		case PANORAMA_MATERIAL_FANCYQUAD:
			return &s_panoramaFancyQuadMaterial;
		}
	}
	return nullptr;
};

//--------------------------------------------------------------------------------------------------
// Scene System
//--------------------------------------------------------------------------------------------------

class CSceneSystem : ISceneSystem
{
	virtual HRenderTexture GetWellKnownRenderTarget( SceneSystemWellKnownRenderTargetID_t nID, SceneSystemRenderTargetSize_t nSize, int nIndex )
	{		
		return S1Wrapper_FindFullFrameBuffer(nIndex);
	}

};

CSceneSystem g_SceneSystem;
ISceneSystem* g_pSceneSystem = (ISceneSystem*)&g_SceneSystem;


class CSceneUtils : ISceneUtils
{
	// Calculates the set of sample weights and offsets to generate a gaussian blur from a sum of linear texture samples
	// returns the center weight in pSamplesWeights[0] and 0.0 in pSampleOffsets[0]
	// for decent quality, nSampleArraySize should be at least ceil( flBlurRadius / 2.0f ) + 1, but ceil( flBlurRadius / 1.5f ) + 1 is even better
	int CalculateLinearWeightsForGaussianBlur( float flBlurRadius, float *pSampleWeights, float *pSampleOffsets, int nSampleArraySize, float flCutoff = 0.004f )
	{
		float flBlurSigma = 0.5f * flBlurRadius;

		int nGaussianSamples = 2 * nSampleArraySize + 1;
		float flGaussianWeightSum = 0.0f;
		float *pGaussianWeights = StackAlloc( float, nGaussianSamples );
		for ( int i = 0; i < nGaussianSamples; ++i )
		{
			float t = i / flBlurSigma;
			float flWeight = expf( -0.5f * t * t );
			pGaussianWeights[ i ] = flWeight;
			// The weights outside of the center are doubled as it's
			// expected that users will sample in both the positive
			// and negative directions.
			flGaussianWeightSum += i == 0 ? flWeight : 2.0f * flWeight;
		}

		for ( int i = 0; i < nGaussianSamples; ++i )
		{
			pGaussianWeights[ i ] /= flGaussianWeightSum;
		}

		int nNumSamples = 1;

		pSampleWeights[ 0 ] = pGaussianWeights[ 0 ];
		pSampleOffsets[ 0 ] = 0.0f; // unused
		float flSampleWeightSum = pSampleWeights[ 0 ];
		for ( int i = 1; i < nSampleArraySize; ++i )
		{
			// ig's are both an index into flGaussianWeights and also a pixel offset
			int ig1 = 2 * i - 1;
			int ig2 = 2 * i;
			pSampleWeights[ i ] = pGaussianWeights[ ig1 ] + pGaussianWeights[ ig2 ];
			pSampleOffsets[ i ] = ( ig1 * pGaussianWeights[ ig1 ] + ig2 * pGaussianWeights[ ig2 ] ) / pSampleWeights[ i ];

			if ( pSampleWeights[ i ] >= flCutoff ) // don't bother sampling unless the weight is going to have a noticeable impact
			{
				// The weights outside of the center are doubled as it's
				// expected that users will sample in both the positive
				// and negative directions.
				flSampleWeightSum += 2.0f * pSampleWeights[ i ];
				++nNumSamples;
			}
			else
			{
				break;
			}
		}

		for ( int i = 0; i < nNumSamples; ++i )
		{
			pSampleWeights[ i ] /= flSampleWeightSum;
		}

		return nNumSamples;
	}

};

CSceneUtils g_SceneUtils;
ISceneUtils* g_pSceneUtils = (ISceneUtils*)&g_SceneUtils;

//--------------------------------------------------------------------------------------------------
// Render device mgr
//--------------------------------------------------------------------------------------------------

class CRenderDeviceMgr : IRenderDeviceMgr
{
	virtual void AddDeviceEventListener( IRenderDeviceEventListener *pObject )
	{
	}
	virtual void RemoveDeviceEventListener( IRenderDeviceEventListener *pObject )
	{
	}
};

CRenderDeviceMgr g_RenderDeviceMgr;
IRenderDeviceMgr* g_pRenderDeviceMgr = (IRenderDeviceMgr*)&g_RenderDeviceMgr;

//--------------------------------------------------------------------------------------------------
// render device
//--------------------------------------------------------------------------------------------------

class CRenderDevice : IRenderDevice
{
public:
	CRenderDevice() : m_hFancyQuadGradientTexture() {};

	virtual const CTextureDesc *GetTextureDesc( HRenderTexture hTexture ) const; // runtime representation of the current streamed data
	virtual HRenderTexture FindOrCreateFileTexture( const char *pFileName, RenderSystemAssetFileLoadMode_t nLoadMode = LOADMODE_ASYNCHRONOUS );
	virtual HRenderTexture FindOrCreateTexture( const char *pResourceName, bool bIsAnonymous, const CTextureCreationDesc *pDescriptor,
												const CTextureDesc *pDataDesc = NULL, const void *pData = NULL, int nDataSize = 0, bool bIsPreTiled = false );
	virtual void AsyncSetTextureData( HRenderTexture hTexture, const CTextureDesc *pDataDesc, const void *pData, int nDataSize, bool bIsPreTiled = false,
									  int nSpecificMipLevelToSet = -1, Rect3D_t const *pSubRectToUpdate = nullptr, uint32 nTextureUpdateFlags = 0,
									  const DataRecycleDelegate_t *pDataRecycleDelegate = nullptr );
	virtual const CTextureDesc *GetOnDiskTextureDesc( HRenderTexture hTexture ) const; // runtime representation of the on-disk data
	virtual PlatWindow_t GetSwapChainPlatWindow( SwapChainHandle_t hSwapChain ) const;

	virtual HRenderTexture CreateAlphaTexture( int32 iWidth, int32 iHeight );
	virtual void DestroyAlphaTexture( HRenderTexture hTexture );
	virtual void UpdateAlphaTexture( HRenderTexture hTexture, int32 xOffset, int32 yOffset, int32 iWidth, int32 iHeight, void *pImageData );

	virtual HRenderTexture CreateTextureFromEngineRT( const char *pchEngineRTName ) OVERRIDE;
	virtual void DestroyTextureFromEngineRT( HRenderTexture hTexture ) OVERRIDE;

	virtual void PushPanelRT( HRenderTexture hRT );
	virtual void PopPanelRT();

	virtual void ExcludeTextureFromForceIntoHardware( HRenderTexture hTexture, bool bExclude );

private:
	HRenderTextureStrong m_hFancyQuadGradientTexture;
};

CRenderDevice  g_RenderDevice;
IRenderDevice* g_pRenderDevice = (IRenderDevice*)&g_RenderDevice;

const CTextureDesc *CRenderDevice::GetTextureDesc( HRenderTexture hTexture ) const // runtime representation of the current streamed dat
{
	if ( !hTexture.IsValid() )
	{
		return nullptr;
	}

	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hTexture.GetResourceHandle()->m_handle;

	return &pTexture->m_textureDesc;
}

HRenderTexture CRenderDevice::FindOrCreateFileTexture( const char *pFileName, RenderSystemAssetFileLoadMode_t nLoadMode )
{
	ResourceHandle_t found = g_pResourceSystem->FindExistingResourceByName( pFileName, RESOURCE_TYPE_NONE );
	if ( found != RESOURCE_HANDLE_INVALID ) return found;

	const char *resources[ 1 ];
	resources[ 0 ] = pFileName;

	HResourceManifest hResourceManifest = g_pResourceSystem->CreateResourceManifest( 1, resources, RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama", RESOURCE_MANIFEST_LOAD_PRIORITY_HIGH );
	if ( hResourceManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
	{
		g_pResourceSystem->ForceSynchronizationAndBlockUntilManifestLoaded( hResourceManifest );
	}
	
	g_pResourceSystem->DestroyResourceManifest( hResourceManifest );

	found = g_pResourceSystem->FindExistingResourceByName( pFileName, RESOURCE_TYPE_NONE );

	return found;
}
HRenderTexture CRenderDevice::FindOrCreateTexture( const char *pResourceName, bool bIsAnonymous, const CTextureCreationDesc *pDescriptor,
												   const CTextureDesc *pDataDesc, const void *pData, int nDataSize, bool bIsPreTiled )
{
	char anonymousName[MAX_PATH];
	uint16 nTextureUniqueId = UINT16_MAX;
	if ( bIsAnonymous )
	{
		// Make name unique for anonymous resources
		nTextureUniqueId = g_pResourceSystem->GetTextureUniqueId();
		V_sprintf_safe( anonymousName, "%d_%s", nTextureUniqueId, pResourceName );
		pResourceName = anonymousName;
	}

	// Check if we already have this texture...
	ResourceHandle_t found = g_pResourceSystem->FindExistingResourceByName( pResourceName, RESOURCE_TYPE_NONE );
	if ( found != RESOURCE_HANDLE_INVALID ) return found;

	HRenderTexture hTexture = S1Wrapper_CreateTexture( pResourceName, nTextureUniqueId, pDescriptor, pDataDesc, pData, nDataSize );
	if ( hTexture.IsValid() )
	{
		g_pResourceSystem->AddExistingResource( pResourceName, hTexture.GetResourceHandle() );
	}

	return hTexture;
}
void CRenderDevice::AsyncSetTextureData( HRenderTexture hTexture, const CTextureDesc *pDataDesc, const void *pData, int nDataSize,
										 bool bIsPreTiled, int nSpecificMipLevelToSet, Rect3D_t const *pSubRectToUpdate, uint32 nTextureUpdateFlags,
										 const DataRecycleDelegate_t *pDataRecycleDelegate )
{
	Rect3D_t nullrect;

	nullrect.x = nullrect.y = nullrect.z = 0;
	nullrect.depth = 1;

	nullrect.width = pDataDesc->m_nWidth;
	nullrect.height = pDataDesc->m_nHeight;

	if ( pSubRectToUpdate == nullptr )
	{
		pSubRectToUpdate = &nullrect;
	}

	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hTexture.GetResourceHandle()->m_handle;
	pTexture->SetTextureData( nullptr, pDataDesc->m_nImageFormat, pData, nDataSize, pSubRectToUpdate, pDataRecycleDelegate );
}


const CTextureDesc *CRenderDevice::GetOnDiskTextureDesc( HRenderTexture hTexture ) const // runtime representation of the on-disk dat
{
	if ( !hTexture.IsValid() )
	{
		return nullptr;
	}

	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hTexture.GetResourceHandle()->m_handle;

	return &pTexture->m_textureDesc;
}
PlatWindow_t CRenderDevice::GetSwapChainPlatWindow( SwapChainHandle_t hSwapChain ) const
{
	return 0;
}

HRenderTexture CRenderDevice::CreateAlphaTexture( int32 iWidth, int32 iHeight )
{
	return S1Wrapper_CreateAlphaTexture( iWidth, iHeight );
}

void CRenderDevice::DestroyAlphaTexture( HRenderTexture hTexture )
{
	S1Wrapper_DestroyTexture( hTexture );
}

void CRenderDevice::UpdateAlphaTexture( HRenderTexture hTexture, int32 xOffset, int32 yOffset, int32 iWidth, int32 iHeight, void *pImageData )
{
	S1Wrapper_UpdateAlphaTexture( hTexture, xOffset, yOffset, iWidth, iHeight, pImageData );
}

HRenderTexture CRenderDevice::CreateTextureFromEngineRT( const char *pchEngineRTName )
{
	return S1Wrapper_FindEngineRT( pchEngineRTName );
}

void CRenderDevice::DestroyTextureFromEngineRT( HRenderTexture hTexture )
{
	S1Wrapper_DestroyTexture( hTexture );
}

void CRenderDevice::PushPanelRT( HRenderTexture hRT )
{
	// This code needs to be executed on the render thread as it will be 
	// responsible for creating the src1 render target if it doesn't
	// exist (cf S1Wrapper_Texture_t::GetS1Texture())
	Assert( g_pMaterialSystem->IsRenderThreadSafe() );
	
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)(S1Wrapper_Texture_t *)hRT.GetResourceHandle()->m_handle;;
	pRenderContext->PushRenderTargetAndViewport( pTexture->GetS1Texture() );

	bool bClearColor = true;
	bool bClearDepth = true;
	bool bClearStencil = true;

	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
	pRenderContext->ClearBuffers( bClearColor, bClearDepth, bClearStencil );
}

void CRenderDevice::PopPanelRT()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->PopRenderTargetAndViewport();
}

void CRenderDevice::ExcludeTextureFromForceIntoHardware( HRenderTexture hTexture, bool bExclude )
{
	if ( !hTexture.IsValid() )
	{
		return;
	}

	S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hTexture.GetResourceHandle()->m_handle;
	pTexture->GetS1Texture()->ExcludeTextureFromForceIntoHardware( bExclude );
}

//--------------------------------------------------------------------------------------------------
// render hardware config
//--------------------------------------------------------------------------------------------------

class CRenderHardwareConfig : public IRenderHardwareConfig
{
public:
	virtual int	 GetFrameBufferColorDepth() const { return 0; }
	virtual int  GetSamplerCount() const { return 16; }
	virtual int  MaximumAnisotropicLevel() const { return 16; }	// 0 means no anisotropic filtering
	virtual int  MaxTextureWidth() const { return 4096; }
	virtual int  MaxTextureHeight() const { return 4096; }
	virtual uint64 TextureMemorySize() const { return 4096; }
	virtual bool SupportsMipmappedCubemaps() const { return true; }
	virtual bool SupportsNPO2Textures() const { return true; }
private:
};

CRenderHardwareConfig  g_RenderHardwareConfig;
IRenderHardwareConfig* g_pRenderHardwareConfig = (IRenderHardwareConfig*)&g_RenderHardwareConfig;


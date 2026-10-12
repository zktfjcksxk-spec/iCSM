//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#ifndef INCLUDED_WRAP_TEXTURE_H
#define INCLUDED_WRAP_TEXTURE_H

#include "tier0/platform.h"
#include "tier0/basetypes.h"
#include "tier0/dbg.h"
#include "s1wrapper.h"
#include "resourcesystem/resourcehandle.h"
#include "rendersystem/irenderdevice.h"
#include "bitmap/imageformat_declarations.h"


// Uncomment to get detailed logging (csv format) about:
//		* creation / destruction of S1Wrapper_Texture_t
//		* creation / destruction of src1 texture ITexture (defer to the render thread)
//#define S1WRAP_TEXTURE_DETAILED_LOG


class ITexture;
class CTextureCreationDesc;
class CPanoramaProceduralRegen;
class IVTFTexture;
template <class T, bool bTestOptimizer >
class CTSQueue;
class IThreadPool;

// Callback supplied by the resource system, stored in the s1wrapper texture and used when the texture bit are in and decoded
typedef void( *S1WrapperTextureCompletionCallback_t )( void *pContext );


struct S1Wrapper_SetTextureDataCmd_t
{
	IVTFTexture *	m_pVTFFromFile;
	void *			m_pData;			// we allocate this unless there is a m_DataRecycleDelegate which then manages it
	int				m_nDataSize;
	CUtlDelegate< void( const void * )> m_DataRecycleDelegate;	// Used by CSource2DoubleBufferedYUV420Texture
	Rect3D_t		m_subRect;
	ImageFormat		m_nFmt;

#ifdef S1WRAP_TEXTURE_DETAILED_LOG
	CUtlString m_textureName;
#endif

	void ReleaseResources();
};

// S1Wrapper_Texture_t struct
// Responsible for creating a src1 ITexture from a file or data
// Note that texture creation on src1 is not thread safe, so we defer
// the creation of the ITexture until we actually need it on the render thread.
// Each texture has it's own "updates" queue, which is serviced every time its 
// texture interface is accessed (read).

struct S1Wrapper_Texture_t : public ResourceData_t
{
public:

	// Called by resourcesystem
	static void StaticInit();
	static void StaticShutdown();

	// Construct destruct instance
	S1Wrapper_Texture_t();
	~S1Wrapper_Texture_t();

	// Create the src1 ITexture and upload data to GPU if necessary before returning the src1 texture
	// Function called on the render thread (usually when setting up draw call)
	// TODO: Investigate perf as we are currently uploading the texture data just before using it 
	// and could cause the GPU to stall. 

	// Access ITexture*. Creates the texture via matsys if it has not already been created.
	// So renderthread only
	ITexture *GetS1Texture();

	// Initialisation of S1Wrapper_texture. Does not create any matsys resources
	// Can be called from any thread
	bool InitFromFileAsync( const char *pFileName, uint16 nUniqueId, S1WrapperTextureCompletionCallback_t callback, void *pCallbackContext );
	bool InitFromData( const char *pResourceName, uint16 nUniqueId, const CTextureCreationDesc *pDescriptor, const CTextureDesc *pDataDesc, const void *pData, int nDataSize );

	// Helper fn. Called from any thread eg IO
	void FillTextureDesc( int16 width, int16 height, ImageFormat imageFormat, bool bRenderTarget = false );

	// Call this ( any thread ) to update (part) of texture
	// Adds Commands to m_pSetTextureDataCmds
	void SetTextureData( IVTFTexture* pVTF, ImageFormat fmt, const void *pData, int nDataSize, Rect3D_t const *pSubRect, const CUtlDelegate< void( const void * )> *pDataRecycleDelegate );


	// Async callbacks -- IO threads, decides what to do with VTF / PNG ie which job to run to produce the texture bits.
	void FileReadCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus );

	// Invoking the complettion callback originally supplied by the resourcesystem
	void InvokeCompletionCallback();

	// Error texture -- called from load completion callbacks IO threads ?
	void SetToErrorTexture();
	bool IsErrorTexture() const { return (m_pTexture == sm_pErrorTexture); }


private:

	// Called from the destructor, ONLY S1Wrapper_DestroyTexture() should delete s1wrapper_textures
	void Destroy();

	// Processes m_pSetTextureDataCmds cmds to update texture, or add a texture regenerator (renderthreadonly)
	void UploadTextureData();

	// Actually make matsys calls to create a texture (renderthread only)
	void CreateS1Texture();

public:

	ITexture * m_pTexture;			// S1 texture  -- should access thru GetS1Texture when rendering.
									// cannot be private because alphatexture and shared fullframe textures
									// access it and probably not on the renderthread.

	uint16 m_nUniqueId;				// Unique identifier used by the resource system

	// Data used to create the src1 ITexture. width, height and image format are stored in m_textureDesc
	// cannot be private, reached from any thread publically.

	CUtlString m_textureName;
	CTextureDesc m_textureDesc;		// S2 Texture description
	int m_nFlags;

private:
	// IS a RT with a depth buffer
	bool m_bRenderTargetWithDepth;

	// Panorama procedural textures
	CPanoramaProceduralRegen *m_pRegenerator;

	// Update commands
	CTSQueue< S1Wrapper_SetTextureDataCmd_t > *m_pSetTextureDataCmds;

	// Completion callback
	S1WrapperTextureCompletionCallback_t m_completionCallback;
	void *m_pCompletionCallbackContext;

	// Async file read data
	FSAsyncControl_t m_hFileAsyncControl;

	// Decode job started after reading file
	CJob *m_pTextureDecodeJob;

	// S1 error texture
	static ITexture *sm_pErrorTexture;

	// Threadpool for texture jobs -- These are decoding/decompressio/svg generation. Files are read by g_pFileSystem(Async)
	static IThreadPool *sm_pTextureDecodeThreadPool;
};

//--------------------------------------------------------------------------------------------------
// This is the API for panorama to use to create/destroy textures
//--------------------------------------------------------------------------------------------------

// Use these to create an s1wrapper_texture from files
HRenderTexture S1Wrapper_CreateTextureAsync( const char *pFileName, uint16 nUniqueId, S1WrapperTextureCompletionCallback_t callback, void *pCallbackContext );

// Use this to create a procedural or RT texture
HRenderTexture S1Wrapper_CreateTexture( const char *pResourceName, uint16 nUniqueId, const CTextureCreationDesc *pDescriptor, const CTextureDesc *pDataDesc, const void *pData, int nDataSize );

// Delete texture and all it's resources
// This is called by the resource system, when a strong handle is released.
// The actual call is from mainthread endframe CResourceSystem::DeleteRequestedResources()
// Apart from font ( alpha texture seperate path ) which is deleted by the font texture cache
// Probably want to check this
void S1Wrapper_DestroyTexture( HRenderTexture hTexture );

// Font special case - Using an alternative and faster way of writing image data (instead of 
// relying procedural texture / texture regenerator) by directly calling IShaderApi::TexLock / IShaderApi::TexUnlock.
// Note that the current implementation of fonts in panorama doesn't need to restore texture bits after alt-tab or device lost
HRenderTexture S1Wrapper_CreateAlphaTexture( int32 iWidth, int32 iHeight );
void S1Wrapper_UpdateAlphaTexture( HRenderTexture hTexture, int32 xOffset, int32 yOffset, int32 iWidth, int32 iHeight, void *pImageData );

HRenderTexture S1Wrapper_FindFullFrameBuffer(int nIndex);
HRenderTexture S1Wrapper_FindEngineRT( const char *pchEngineRTName );

#endif	// INCLUDED_WRAP_TEXTURE_H
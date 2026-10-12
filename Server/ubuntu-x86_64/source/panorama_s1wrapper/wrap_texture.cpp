#include "wrap_texture.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "pixelwriter.h"

#include "jpegloader.h"
#include "tgaloader.h"
#include "pngloader.h"
#include "svg/svgloader.h"

#include "vstdlib/jobthread.h"

#include "tier0/tslist.h"

#include "../panorama/source2/renderer/source2surface.h"

#include "game/shared/csgo/econ_shared_defs.h" // for csgo inventory image cache (.iic) file sizes.


// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"


#ifdef S1WRAP_TEXTURE_DETAILED_LOG
#define WrapTextureLog( name, width, height, op, size, ptr ) Msg( ">>>>>WRAPTEX, %.3f, %s, %d, %d, %s, %d, 0x%x\n", Plat_FloatTime(), name, width, height, op, size, ptr )
#else
#define WrapTextureLog(...)
#endif

bool ConvertDDS( void * pDDS, int nDDS, CUtlBuffer &bufOutput, int &width, int &height, ImageFormat &fmt );

//==================================================================================================
//
// Async callbacks
//
//==================================================================================================

static void AsyncInitFromFileCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	S1Wrapper_Texture_t *pS1WrapperTexture = ( S1Wrapper_Texture_t * )request.pContext;

	pS1WrapperTexture->FileReadCallback( request, numReadBytes, asyncStatus );
}


//==================================================================================================
//
// Base class for jobs setting up S1Wrapper_Texture_t
//
//==================================================================================================

class CCreateWrapperTextureBaseJob : public CJob
{
public:

	CCreateWrapperTextureBaseJob( S1Wrapper_Texture_t *pWrapperTexture, void *pData, int nDataSize );
	virtual ~CCreateWrapperTextureBaseJob();

protected:

	void FreeFileBuffer();

	S1Wrapper_Texture_t *m_pWrapperTexture;
	void *m_pData;
	int m_nDataSize;
};

//-----------------------------------------------------------------------------
CCreateWrapperTextureBaseJob::CCreateWrapperTextureBaseJob( S1Wrapper_Texture_t *pWrapperTexture, void *pData, int nDataSize )
:
	m_pWrapperTexture( pWrapperTexture ),
	m_pData( pData ),
	m_nDataSize( nDataSize )
{

}

//-----------------------------------------------------------------------------
CCreateWrapperTextureBaseJob::~CCreateWrapperTextureBaseJob()
{
	FreeFileBuffer();
}

//-----------------------------------------------------------------------------
void CCreateWrapperTextureBaseJob::FreeFileBuffer()
{
	if ( m_pData )
	{
		g_pFullFileSystem->FreeOptimalReadBuffer( m_pData );
		m_pData = nullptr;
	}
}


//==================================================================================================
//
// Job to set up a S1Wrapper_Texture_t from a buffer containing a VTF texture
//
//==================================================================================================

class CCreateWrapperTextureFromVTFJob : public CCreateWrapperTextureBaseJob
{
public:

	CCreateWrapperTextureFromVTFJob( S1Wrapper_Texture_t *pWrapperTexture, void *pData, int nDataSize );

	virtual JobStatus_t DoExecute();
};

//-----------------------------------------------------------------------------
CCreateWrapperTextureFromVTFJob::CCreateWrapperTextureFromVTFJob( S1Wrapper_Texture_t *pWrapperTexture, void *pData, int nDataSize )
:
	CCreateWrapperTextureBaseJob( pWrapperTexture, pData, nDataSize )
{}

//-----------------------------------------------------------------------------
JobStatus_t CCreateWrapperTextureFromVTFJob::DoExecute()
{
	VPROF_BUDGET( "CCreateVTFTextureJob::DoExecute", VPROF_BUDGETGROUP_TENFOOT );

	CUtlBuffer fileData;
	fileData.SetExternalBuffer( m_pData, m_nDataSize, m_nDataSize, CUtlBuffer::READ_ONLY );

	// Create the corresponding VTF texture
	IVTFTexture* pVTFTexture = CreateVTFTexture();
	if ( !pVTFTexture->Unserialize( fileData ) )
	{
		Warning( "Failed to deserialize VTF %s\n", m_pWrapperTexture->m_textureName.String() );

		DestroyVTFTexture( pVTFTexture );

		m_pWrapperTexture->SetToErrorTexture();
		m_pWrapperTexture->InvokeCompletionCallback();

		FreeFileBuffer();

		return JOB_OK;
	}

	// Create the procedural ITexture using the VTF texture as an input
	m_pWrapperTexture->m_nFlags = TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NODEBUGOVERRIDE | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_TRILINEAR | TEXTUREFLAGS_SKIP_INITIAL_DOWNLOAD | TEXTUREFLAGS_NOLOD;
	m_pWrapperTexture->m_nFlags |= pVTFTexture->Flags();

	// Detect YCoCg textures : dxt5 & opaque
	// Explicitly adding YCOCG flag here, as we are creating a procedural texture so all dxt5  textures will have
	// the TEXTUREFLAGS_EIGHTBITALPHA flag (cf CTexture::InitProceduralTexture)
	// TODO Add YCoCg flag in tool chain
	if ( ( pVTFTexture->Format() == IMAGE_FORMAT_DXT5 ) && !( pVTFTexture->Flags() & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) ) )
	{
		m_pWrapperTexture->m_nFlags |= TEXTUREFLAGS_YCOCG;
	}

	m_pWrapperTexture->SetTextureData( pVTFTexture, IMAGE_FORMAT_NULL, nullptr, 0, nullptr, nullptr );

	m_pWrapperTexture->FillTextureDesc( pVTFTexture->Width(), pVTFTexture->Height(), pVTFTexture->Format() );

	// Call completion callback
	m_pWrapperTexture->InvokeCompletionCallback();

	FreeFileBuffer();
	
	return JOB_OK;
}


//==================================================================================================
//
// Job to set up a S1Wrapper_Texture_t from a buffer containing a TGA / PNG / JPG ...
//
//==================================================================================================

class CCreateWrapperTextureFromCommonFormatsJob : public CCreateWrapperTextureBaseJob
{
public:

	CCreateWrapperTextureFromCommonFormatsJob( S1Wrapper_Texture_t *pWrapperTexture, void *pData, int nDataSize );

	virtual JobStatus_t DoExecute();
};

//-----------------------------------------------------------------------------
CCreateWrapperTextureFromCommonFormatsJob::CCreateWrapperTextureFromCommonFormatsJob( S1Wrapper_Texture_t *pWrapperTexture, void *pData, int nDataSize )
	:
	CCreateWrapperTextureBaseJob( pWrapperTexture, pData, nDataSize )
{}

//-----------------------------------------------------------------------------

static unsigned char pngSig[] = { 137,80,78,71,13,10,26,10 };

JobStatus_t CCreateWrapperTextureFromCommonFormatsJob::DoExecute()
{
	VPROF_BUDGET( "CCreateWrapperTextureFromCommonFormatsJob::DoExecute", VPROF_BUDGETGROUP_TENFOOT );
	
	const char *pchFilePath = m_pWrapperTexture->m_textureName.String();
	
	// Convert input data to RGBA

	CUtlBuffer rgbaBuffer;
	int nWidth = 0;
	int nHeight = 0;
	bool bImageDecoded = false;
	ImageFormat imgFormat = IMAGE_FORMAT_RGBA8888;

	if ( V_stristr( pchFilePath, ".tga" ) )
	{
		VPROF_BUDGET( "CCreateWrapperTextureFromCommonFormatsJob::DoExecute - TGA", VPROF_BUDGETGROUP_TENFOOT );
		char *pchImageBytes = NULL;
		int nBytes;
		if ( LoadTGA( m_nDataSize, (char *)m_pData, (byte **)&pchImageBytes, &nBytes, &nWidth, &nHeight ) )
		{
			rgbaBuffer.Put( pchImageBytes, nBytes );
			delete[] pchImageBytes;
			bImageDecoded = true;
		}
	}
	else if ( V_stristr( pchFilePath, ".png" ) 
			  || (V_stristr( pchFilePath, ".dds" ) && (m_nDataSize >= sizeof( pngSig )) && (!memcmp(m_pData, pngSig, sizeof( pngSig ))) ) )
	{
		VPROF_BUDGET( "CCreateWrapperTextureFromCommonFormatsJob::DoExecute - PNG", VPROF_BUDGETGROUP_TENFOOT );
		if ( ConvertPNGToRGBA( (const byte *)m_pData, m_nDataSize, rgbaBuffer, nWidth, nHeight ) )
		{
			bImageDecoded = true;
		}
	}
	else if ( V_stristr( pchFilePath, ".jpg" ) || V_stristr( pchFilePath, ".jpeg" ) )
	{
		VPROF_BUDGET( "CCreateWrapperTextureFromCommonFormatsJob::DoExecute - JPG", VPROF_BUDGETGROUP_TENFOOT );
		if ( ConvertJpegToRGBA( (const byte *)m_pData, m_nDataSize, rgbaBuffer, nWidth, nHeight ) )
		{
			bImageDecoded = true;
		}
	}
	else if( V_stristr( pchFilePath, ".svg" ) )
	{
		VPROF_BUDGET( "CCreateWrapperTextureFromCommonFormatsJob::DoExecute - SVG", VPROF_BUDGETGROUP_TENFOOT );
		if( ConvertSVGToRGBA( (const byte *)m_pData, m_nDataSize, rgbaBuffer, nWidth, nHeight ) )
		{
			bImageDecoded = true;
		}
	}
	else if( V_stristr( pchFilePath, ".iic" ) )
	{
		VPROF_BUDGET( "CCreateWrapperTextureFromCommonFormatsJob::DoExecute - IIC", VPROF_BUDGETGROUP_TENFOOT );
		// TODO: Could move these sizes into a file size, or find a way to plumb from game code... Moved to a public header for now.
		nWidth = ECON_ITEM_GENERATED_ICON_WIDTH;
		nHeight = ECON_ITEM_GENERATED_ICON_HEIGHT;
		rgbaBuffer.CopyBuffer( m_pData, nWidth * nHeight * 4 );
		imgFormat = IMAGE_FORMAT_BGRA8888;
		bImageDecoded = true;
	}
	else if ( V_stristr( pchFilePath, ".dds" ) )
	{
		ImageFormat fmt;

		if ( ConvertDDS( m_pData, m_nDataSize, rgbaBuffer, nWidth, nHeight, fmt ) )
		{
			bImageDecoded = true;
			imgFormat = fmt;
		}
	}
	else
	{
		Warning( "Unable to decode %s - unknown format\n", pchFilePath );
	}
	
	if ( !bImageDecoded || ( nWidth == 0 ) || ( nHeight == 0 ) )
	{
		Warning( "Failed to convert image data to RGBA from %s\n", pchFilePath );

		m_pWrapperTexture->SetToErrorTexture();
	}
	else
	{
		// Set up S1Wrapper_Texture_t

		m_pWrapperTexture->m_nFlags = TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NODEBUGOVERRIDE | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_TRILINEAR | TEXTUREFLAGS_NOLOD;
		m_pWrapperTexture->m_nFlags |= ( TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOLOD );
		m_pWrapperTexture->m_nFlags |= ( TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SKIP_INITIAL_DOWNLOAD );

		Rect3D_t subrect( 0, 0, 0, nWidth, nHeight, 1 );
		m_pWrapperTexture->SetTextureData( nullptr, imgFormat, rgbaBuffer.Base(), rgbaBuffer.TellPut(), &subrect, nullptr );

		m_pWrapperTexture->FillTextureDesc( nWidth, nHeight, imgFormat );
	}

	// Call completion callback
	m_pWrapperTexture->InvokeCompletionCallback();

	FreeFileBuffer();

	return JOB_OK;
}



//==================================================================================================
//
// Panorama procedural textures. Handle the download
//
//==================================================================================================

void S1Wrapper_SetTextureDataCmd_t::ReleaseResources()
{
	if ( m_pVTFFromFile )
	{
		DestroyVTFTexture( m_pVTFFromFile );
		m_pVTFFromFile = nullptr;
	}

	if ( m_pData )
	{
		if ( !m_DataRecycleDelegate.IsEmpty() )
		{
			m_DataRecycleDelegate( m_pData );
		}
		else
		{
			WrapTextureLog( m_textureName.String(), 0, 0, "S1Wrapper_SetTextureDataCmd_t free", m_nDataSize, m_pData );
			free( m_pData );
		}
		m_pData = nullptr;
	}
}

class CPanoramaProceduralRegen : public ITextureRegenerator
{
public:
	CPanoramaProceduralRegen()
	{
		m_pBackingStore = nullptr;
		m_nBackingStoreSize = 0;
	}

	~CPanoramaProceduralRegen()
	{
		DeleteTextureBits();
	}

	// Slow copy, for NPO2 textures and format differences (BGRX vs BGR)
	void CopyTextureData( IVTFTexture *pVTFDst, IVTFTexture *pVTFSrc )
	{
		if ( ( pVTFDst->Width() < pVTFSrc->Width() ) ||
			 ( pVTFDst->Height() < pVTFSrc->Height() ) ||
			 ( pVTFDst->Depth() < pVTFSrc->Depth() ) )
		{
			return;
		}

		if ( ( pVTFDst->Format() == IMAGE_FORMAT_DXT1_RUNTIME ) ||
			 ( pVTFDst->Format() == IMAGE_FORMAT_DXT5_RUNTIME ) || 
			 (pVTFDst->Format() == IMAGE_FORMAT_DXT3_RUNTIME) )
		{
			int nBlockSizeBytes = ( pVTFDst->Format() != IMAGE_FORMAT_DXT1_RUNTIME ) ? 16 : 8;

			int nNumBlocksWideSrc = pVTFSrc->Width() / 4;
			int nNumBlocksWideDst = pVTFDst->Width() / 4;

			int nNumCopyBytes = nNumBlocksWideSrc * nBlockSizeBytes;
			int nNumPaddingBytes = ( nNumBlocksWideDst - nNumBlocksWideSrc ) * nBlockSizeBytes;

			int nNumBlocksHighSrc = pVTFSrc->Height() / 4;

			int iMipLevel = 0;
			int iDepth = 0;

			unsigned char *pBitsSrc = (unsigned char *)pVTFSrc->ImageData( 0, 0, iMipLevel, 0, 0, iDepth );
			unsigned char *pBitsDst = (unsigned char *)pVTFDst->ImageData( 0, 0, iMipLevel, 0, 0, iDepth );

			for ( int h = 0; h < nNumBlocksHighSrc; h++ )
			{
				// copy numblocks wide * blockSize
				memcpy( pBitsDst, pBitsSrc, nNumCopyBytes );

				pBitsSrc += nNumCopyBytes;
				pBitsDst += nNumCopyBytes;

				// fill padding bytes
				memset( pBitsDst, 0, nNumPaddingBytes );
				pBitsDst += nNumPaddingBytes;
			}
		}
		else 
		{
			Rect3D_t subRect( 0, 0, 0, pVTFSrc->Width(), pVTFSrc->Height(), pVTFSrc->Depth() );
			char* pBits = (char*)pVTFSrc->ImageData();

			for ( int z = 0; z < pVTFSrc->Depth(); z++ )
			{
				CPixelWriter pixelWriter;

				pixelWriter.SetPixelMemory( pVTFDst->Format(),
											pVTFDst->ImageData( 0, 0, 0, 0, 0, z ),
											pVTFDst->RowSizeInBytes( 0 ) );          

				if ( pVTFDst->Format() == pVTFSrc->Format() )
				{
					pixelWriter.Seek( 0, 0 );
					CopyMemory3D( pixelWriter.GetCurrentPixel(), pBits,
								  pVTFSrc->RowSizeInBytes( 0 ), pVTFSrc->Height(), 1,
								  pVTFSrc->RowSizeInBytes( 0 ), pVTFSrc->RowSizeInBytes( 0 ) * pVTFSrc->Height(),
								  pVTFDst->RowSizeInBytes( 0 ), pVTFDst->RowSizeInBytes( 0 ) * pVTFDst->Height() );
				}
				else
				{
					CPixelWriter pixelReader;
					pixelReader.SetPixelMemory( pVTFSrc->Format(),
												pBits,		
												pVTFSrc->RowSizeInBytes( 0 ) );

					int topx = subRect.x;
					int topy = subRect.y;

					for ( int y = topy; y < topy + subRect.height; ++y )
					{
						for ( int x = topx; x < topx + subRect.width; ++x )
						{
							int r, g, b, a;
							pixelReader.Seek( x - topx, y - topy );
							pixelReader.ReadPixelNoAdvance( r, g, b, a );

							pixelWriter.Seek( x, y );
							pixelWriter.WritePixel( r, g, b, a );
						}
					}
				}
			}
		}
	}

	void UpdateSubRect( const S1Wrapper_SetTextureDataCmd_t &params, IVTFTexture *pVTFTexture )
	{
		Assert( ( pVTFTexture && ( pVTFTexture->Depth() == m_nDepth ) ) || !pVTFTexture );

		const Rect3D_t* pSubRect = &params.m_subRect;
		char* pBits = (char*)params.m_pData;

		if ( ( pSubRect->width * pSubRect->height * pSubRect->depth ) > ( m_nWidth*m_nHeight*m_nDepth ) )
		{
			Error( "Subrect > texture\n" );
		}

		for ( int z = 0; z < m_nDepth; ++z )
		{
			int nSrcBytesPerRow = params.m_nDataSize / pSubRect->height;
			int nBackingBytesPerRow;
			if ( ( m_nFmt == IMAGE_FORMAT_DXT1_RUNTIME ) || (m_nFmt == IMAGE_FORMAT_DXT5_RUNTIME) || (m_nFmt == IMAGE_FORMAT_DXT3_RUNTIME) )
			{ 
				if ( pVTFTexture )
				{
					uint32 nCopySize = MIN( pVTFTexture->ComputeTotalSize(), params.m_nDataSize );
					void* pDst = pVTFTexture->ImageData(0, 0, 0, 0, 0, z);
					memcpy( pDst, pBits, nCopySize );
				}

				uint32 nCopySize = MIN( m_nBackingStoreSize, params.m_nDataSize );
				memcpy( m_pBackingStore, pBits, nCopySize );
				
				m_params.m_pData = nullptr;
				return;

			}
			else
			{
				nBackingBytesPerRow = ImageLoader::SizeInBytes(m_nFmt) * m_nWidth;
			}
			bool bSetBackingStore = true;
			bool bSetVTFTexture = ( pVTFTexture != nullptr );

			CPixelWriter pixelWriter1, pixelWriter2;

			if ( bSetVTFTexture )
			{
				pixelWriter1.SetPixelMemory( pVTFTexture->Format(),
					pVTFTexture->ImageData( 0, 0, 0, 0, 0, z ),
					pVTFTexture->RowSizeInBytes( 0 ) );
			}

			pixelWriter2.SetPixelMemory( m_nFmt,
				m_pBackingStore,
				nBackingBytesPerRow );

			if ( bSetBackingStore && ( m_nFmt == params.m_nFmt ) )
			{
				pixelWriter2.Seek( pSubRect->x, pSubRect->y );
				CopyMemory3D( pixelWriter2.GetCurrentPixel(), pBits,
					nSrcBytesPerRow, pSubRect->height, 1,
					nSrcBytesPerRow, 0,
					nBackingBytesPerRow, 0 );
				bSetBackingStore = false;
			}

			if ( bSetVTFTexture && ( pVTFTexture->Format() == params.m_nFmt ) )
			{
				pixelWriter1.Seek( pSubRect->x, pSubRect->y );
				CopyMemory3D( pixelWriter1.GetCurrentPixel(), pBits,
					nSrcBytesPerRow, pSubRect->height, 1,
					nSrcBytesPerRow, 0,
					pVTFTexture->RowSizeInBytes( 0 ), 0 );
				bSetVTFTexture = false;
			}

			if ( bSetBackingStore || bSetVTFTexture )
			{
				CPixelWriter pixelReader;
				pixelReader.SetPixelMemory( params.m_nFmt,
					pBits,
					nSrcBytesPerRow );

				int topx = pSubRect->x;
				int topy = pSubRect->y;

				for ( int y = topy; y < topy + pSubRect->height; ++y )
				{
					for ( int x = topx; x < topx + pSubRect->width; ++x )
					{
						int r, g, b, a;						
						pixelReader.Seek( x - topx, y - topy );
						pixelReader.ReadPixelNoAdvance( r, g, b, a );

						if ( bSetVTFTexture )
						{
							pixelWriter1.Seek( x, y );
							pixelWriter1.WritePixel( r, g, b, a );
						}

						if ( bSetBackingStore )
						{
							pixelWriter2.Seek( x, y );
							pixelWriter2.WritePixel( r, g, b, a );
						}
					}
				}
				bSetBackingStore = false;
				bSetVTFTexture = false;
			}
		}

		m_params.m_pData = nullptr;
	}

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRectAll )
	{
		
		if ( m_params.m_pVTFFromFile )
		{
			if ( ( pVTFTexture->Width() * pVTFTexture->Height() ) == ( m_params.m_pVTFFromFile->Width() * m_params.m_pVTFFromFile->Height() ) )
			{
				if ( ( pVTFTexture->Format() == m_params.m_pVTFFromFile->Format() ) ||
					 ImageLoader::IsCompressed( pVTFTexture->Format() ) && ImageLoader::IsCompressed( m_params.m_pVTFFromFile->Format() ) )
				{
					for ( int z = 0; z < 1; ++z )
					{
						unsigned char* pSrc = m_params.m_pVTFFromFile->ImageData( 0, 0, 0, 0, 0, z );
						unsigned char* pDst = pVTFTexture->ImageData( 0, 0, 0, 0, 0, z );

						int nSize = MIN( pVTFTexture->ComputeTotalSize(), m_params.m_pVTFFromFile->ComputeTotalSize() );
						memcpy( pDst, pSrc, nSize );
					}
				}
				else
				{
					CopyTextureData( pVTFTexture, m_params.m_pVTFFromFile );
				}
			}
			else
			{
				// src and dst VTF are different sizes, src is a subrect of dst (dst next power of two greater to src) so memcpy won't cut it
				CopyTextureData( pVTFTexture, m_params.m_pVTFFromFile );
			}
		}
		else
		{
			// This path is for procedural textures including png loaded textures

			if ( !m_params.m_pData )
			{
				// m_pData will be zero when we are being asked to restore after, say ALT_TAB, or device lost

				if ( m_pBackingStore )
				{
					if ( m_nFmt == pVTFTexture->Format() )
					{
						memcpy( pVTFTexture->ImageData( 0, 0, 0, 0, 0, 0 ), m_pBackingStore, pVTFTexture->ComputeTotalSize() );
					}
					else
					{
						int nBackingBytesPerRow = ImageLoader::SizeInBytes( m_nFmt ) * m_nWidth;
						
						CPixelWriter pixelReader, pixelWriter;
						
						pixelReader.SetPixelMemory( m_nFmt,
													 m_pBackingStore,
													 nBackingBytesPerRow );

						pixelWriter.SetPixelMemory( pVTFTexture->Format(),
							pVTFTexture->ImageData( 0, 0, 0, 0, 0, 0 ),
							pVTFTexture->RowSizeInBytes( 0 ) );


						for ( int y = 0; y < pVTFTexture->Height(); ++y )
						{
							for ( int x = 0; x < pVTFTexture->Width(); ++x )
							{
								int r, g, b, a;
								pixelReader.Seek( x, y );
								pixelReader.ReadPixelNoAdvance( r, g, b, a );
								pixelWriter.Seek( x, y );
								pixelWriter.WritePixel( r, g, b, a );
							}
						}

					}

				}
			}
			else
			{
				// If we have data this is a normal download for a subrect or all of the texture(eg png)

				if ( !m_pBackingStore )
				{
					m_nBackingStoreSize = pVTFTexture->ComputeTotalSize();
					m_pBackingStore = (char*)malloc( m_nBackingStoreSize );
					m_nWidth = pVTFTexture->Width();
					m_nHeight = pVTFTexture->Height();
					m_nDepth = pVTFTexture->Depth();
					m_nFmt = pVTFTexture->Format();
				}

				UpdateSubRect( m_params, pVTFTexture );
			}

			m_params.m_pData = nullptr;
		}
	}

	void UpdateBackingStore( ITexture *pTexture )
	{
		if ( !m_params.m_pData )
			return;

		if ( !m_pBackingStore )
		{
			m_nWidth = pTexture->GetMappingWidth();
			m_nHeight = pTexture->GetMappingHeight();
			m_nDepth = pTexture->GetMappingDepth();
			m_nFmt = pTexture->GetImageFormat();
			m_nBackingStoreSize = ImageLoader::GetMemRequired( m_nWidth, m_nHeight, m_nDepth, 1, m_nFmt );
			m_pBackingStore = (char*)malloc( m_nBackingStoreSize );
		}

		// Only update the backing store
		UpdateSubRect( m_params, nullptr );
	}

	virtual void Release()
	{
		DeleteTextureBits();
	}

	void DeleteTextureBits()
	{
		m_params.ReleaseResources();
		
		if ( m_pBackingStore )
		{
			free( m_pBackingStore );
			m_pBackingStore = nullptr;
			m_nBackingStoreSize = 0;
		}
	}

	S1Wrapper_SetTextureDataCmd_t  m_params;

	// Backing store data
	char*		m_pBackingStore;
	int			m_nBackingStoreSize;
	int			m_nWidth;
	int			m_nHeight;
	int			m_nDepth;
	ImageFormat m_nFmt;
};


//==================================================================================================
//
//	S1Wrapper_Texture_t Methods
//
//==================================================================================================

ITexture *S1Wrapper_Texture_t::sm_pErrorTexture = nullptr;
IThreadPool *S1Wrapper_Texture_t::sm_pTextureDecodeThreadPool = nullptr;


//-----------------------------------------------------------------------------
/*static*/ void S1Wrapper_Texture_t::StaticInit()
{
	Assert( g_pMaterialSystem->IsTextureLoaded( "error" ) );
	sm_pErrorTexture = g_pMaterialSystem->FindTexture( "error", TEXTURE_GROUP_OTHER );

	// Start thread pool used to create VTF textures
	sm_pTextureDecodeThreadPool = CreateNewThreadPool();

	ThreadPoolStartParams_t startParams;
	startParams.nThreads = 1;
	startParams.nStackSize = 256 * 1024;
	sm_pTextureDecodeThreadPool->Start( startParams, "PanoramaTextureDecode" );

}

//-----------------------------------------------------------------------------
/*static*/ void S1Wrapper_Texture_t::StaticShutdown()
{
	if ( sm_pTextureDecodeThreadPool )
	{
		sm_pTextureDecodeThreadPool->Stop();
		DestroyThreadPool( sm_pTextureDecodeThreadPool );
		sm_pTextureDecodeThreadPool = nullptr;
	}
}

//-----------------------------------------------------------------------------
S1Wrapper_Texture_t::S1Wrapper_Texture_t()
:
	ResourceData_t( (intptr_t)this, RESOURCE_TYPE_TEXTURE ),
	// src1 ITexture created on the render thread
	m_pTexture( nullptr ),
	m_nUniqueId( UINT16_MAX ),
	m_pRegenerator( nullptr ),
	m_bRenderTargetWithDepth( false ),
	m_completionCallback( nullptr ),
	m_pCompletionCallbackContext( nullptr ),
	m_hFileAsyncControl( nullptr ),
	m_pTextureDecodeJob( nullptr )
{
	m_pSetTextureDataCmds = new CTSQueue< S1Wrapper_SetTextureDataCmd_t >;
}

//-----------------------------------------------------------------------------
S1Wrapper_Texture_t::~S1Wrapper_Texture_t()
{
	Destroy();
}

//-----------------------------------------------------------------------------
void S1Wrapper_Texture_t::Destroy()
{
	WrapTextureLog( m_textureName.String(), m_textureDesc.m_nWidth, m_textureDesc.m_nHeight, "Destroy", 0, this );

	if ( g_pFullFileSystem && m_hFileAsyncControl )
	{
		g_pFullFileSystem->AsyncAbort( m_hFileAsyncControl );
		g_pFullFileSystem->AsyncRelease( m_hFileAsyncControl );
	}

	if ( m_pTextureDecodeJob )
	{
		m_pTextureDecodeJob->Abort();
		m_pTextureDecodeJob->Release();
	}

	if ( m_pSetTextureDataCmds )
	{
		// destroy pending VTF or data
		S1Wrapper_SetTextureDataCmd_t cmd;
		while ( m_pSetTextureDataCmds->PopItem( &cmd ) )
		{
			cmd.ReleaseResources();
		}
		delete m_pSetTextureDataCmds;
		m_pSetTextureDataCmds = nullptr;
	}
	
	// TODO Check thread safety. When is Destroy actually getting called ???
	if ( m_pTexture && ( m_pTexture != sm_pErrorTexture ) )
	{
		if ( m_pRegenerator )
		{
			m_pTexture->SetTextureRegenerator( nullptr );	// Should call Release() on the existing regenerator
		}
		m_pTexture->DecrementReferenceCount();
		if ( m_pTexture->GetReferenceCount() == 0 )
		{
			WrapTextureLog( m_textureName.String(), m_textureDesc.m_nWidth, m_textureDesc.m_nHeight, "Destroy release src1", 0, m_pTexture );
		}
		else if ( m_pTexture->GetReferenceCount() < 0 )
		{
			WrapTextureLog( m_textureName.String(), m_textureDesc.m_nWidth, m_textureDesc.m_nHeight, "Destroy release src1(negative)", 0, m_pTexture );
		}
		m_pTexture->DeleteIfUnreferenced();
		m_pTexture = nullptr;
	}
	
	if ( m_pRegenerator )
	{
		delete m_pRegenerator;
		m_pRegenerator = nullptr;
	}

	m_textureName.Clear();

	g_pResourceSystem->MarkTextureUniqueIdUnused( m_nUniqueId );
	m_nUniqueId = UINT16_MAX;
}

//-----------------------------------------------------------------------------
// Setup S1Wrapper_Texture_t asynchronously from the given file:
//	1 - Read file asynchronously (using the IO thread pool)
//	2 - In the case of a vtf file, create IVTFTexture and unserialize file data (using our own texture decode thread pool)
//	3 - Call completion callback
// Note that in case of an error (IO, unserialize) we are loading the "error" texture
//
bool S1Wrapper_Texture_t::InitFromFileAsync( const char *pFileName, uint16 nUniqueId, S1WrapperTextureCompletionCallback_t callback, void *pCallbackContext )
{
	// src1 ITexture created on the render thread
	m_pTexture = nullptr;

	m_textureName = pFileName;
	m_nUniqueId = nUniqueId;

	m_completionCallback = callback;
	m_pCompletionCallbackContext = pCallbackContext;

	WrapTextureLog( m_textureName.String(), 0, 0, "InitFromFileAsync", 0, this );
	
	// Read file asynchronously
	FileAsyncRequest_t asyncFileRequest;
	asyncFileRequest.pszFilename = m_textureName.String();
	asyncFileRequest.pContext = this;
	asyncFileRequest.pfnCallback = &AsyncInitFromFileCallback;
	asyncFileRequest.priority = 0;
	asyncFileRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;

	g_pFullFileSystem->AsyncRead( asyncFileRequest, &m_hFileAsyncControl );

	return true;
}

//-----------------------------------------------------------------------------
void S1Wrapper_Texture_t::FileReadCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
#ifdef S1WRAP_TEXTURE_DETAILED_LOG
	if ( asyncStatus != FSASYNC_OK )
	{
		WrapTextureLog( m_textureName.String(), 0, 0, "Finish async file read - failure", 0, this );
	}
	else
	{
		WrapTextureLog( m_textureName.String(), 0, 0, "Finish async file read - success", 0, this );
	}
#endif

	if ( asyncStatus != FSASYNC_OK )
	{
		Warning( "Error reading file %s.\n", m_textureName.String() );
		SetToErrorTexture();
		InvokeCompletionCallback();
		g_pFullFileSystem->FreeOptimalReadBuffer( request.pData );
		return;
	}

	// Create IVTFTexture and unserialize file data (using our own texture decode thread pool)
	
	m_pTextureDecodeJob = nullptr;
	if ( V_stristr( m_textureName.String(), ".vtf" ) )
	{
		m_pTextureDecodeJob = new CCreateWrapperTextureFromVTFJob( this, request.pData, numReadBytes );
	}
	else
	{
		m_pTextureDecodeJob = new CCreateWrapperTextureFromCommonFormatsJob( this, request.pData, numReadBytes );
	}
	
	if ( m_pTextureDecodeJob )
	{
		m_pTextureDecodeJob->SetFlags( JF_QUEUE );

		sm_pTextureDecodeThreadPool->AddJob( m_pTextureDecodeJob );
	}
	else
	{
		SetToErrorTexture();
		InvokeCompletionCallback();
		g_pFullFileSystem->FreeOptimalReadBuffer( request.pData );
	}
}

//-----------------------------------------------------------------------------
bool S1Wrapper_Texture_t::InitFromData( const char *pResourceName, uint16 nUniqueId, const CTextureCreationDesc *pDescriptor, const CTextureDesc *pDataDesc, const void *pData, int nDataSize )
{
	m_textureName = pResourceName;
	m_nUniqueId = nUniqueId;

	WrapTextureLog( m_textureName.String(), pDescriptor->m_nWidth, pDescriptor->m_nHeight, "InitFromData", 0, this );

	m_nFlags = TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NODEBUGOVERRIDE | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_TRILINEAR;
	if ( pDescriptor->m_nFlags & TSPEC_SUGGEST_CLAMPS ) { m_nFlags |= TEXTUREFLAGS_CLAMPS; }
	if ( pDescriptor->m_nFlags & TSPEC_SUGGEST_CLAMPT ) { m_nFlags |= TEXTUREFLAGS_CLAMPT; }
	if ( pDescriptor->m_nFlags & TSPEC_SUGGEST_CLAMPU ) { m_nFlags |= TEXTUREFLAGS_CLAMPU; }
	if ( pDescriptor->m_nFlags & TSPEC_NO_LOD ) { m_nFlags |= TEXTUREFLAGS_NOLOD; }
	if ( pDescriptor->m_nFlags & TSPEC_VERTEX_TEXTURE ) { m_nFlags |= TEXTUREFLAGS_VERTEXTEXTURE; }

#if CSTRIKE_TRUNK_BUILD
	if ( ( pDescriptor->m_nFlags & TSPEC_RENDER_TARGET ) &&
		 (( pDescriptor->m_nHeight >= 4096 ) || ( pDescriptor->m_nWidth >= 4096 ) ) )
	{
		Msg( "Warning : Panorama creating a very large RT texture %d x %d\n", pDescriptor->m_nWidth, pDescriptor->m_nHeight );
		AssertMsgAlways( 0, "Warning : Panorama creating a very large texture (>= 4K)" );
	}
#endif

	if ( pDescriptor->m_nFlags & TSPEC_RENDER_TARGET )
	{
		m_nFlags |= TEXTUREFLAGS_RENDERTARGET;

		if ( pDescriptor->m_nFlags & TSPEC_RENDER_TARGET_WITHDS )
		{
			m_bRenderTargetWithDepth = true;
		}
	}
	else
	{
		m_nFlags |= ( TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SKIP_INITIAL_DOWNLOAD | TEXTUREFLAGS_NOLOD );
	}

	if ( m_nFlags & TEXTUREFLAGS_PROCEDURAL )
	{
		if ( pData )
		{
			Rect3D_t subrect( 0, 0, 0, pDataDesc->m_nWidth, pDataDesc->m_nHeight, pDataDesc->GetDepth() );
			SetTextureData( nullptr, pDataDesc->m_nImageFormat, pData, nDataSize, &subrect, nullptr );
		}
	}

	FillTextureDesc( pDescriptor->m_nWidth, pDescriptor->m_nHeight, pDescriptor->m_nImageFormat, ( m_nFlags & TEXTUREFLAGS_RENDERTARGET ) ? true : false );
	
	return true;
}

//-----------------------------------------------------------------------------
void S1Wrapper_Texture_t::SetToErrorTexture()
{
	Assert( !m_pTexture );
	m_pTexture = sm_pErrorTexture;
	FillTextureDesc( m_pTexture->GetActualWidth(), m_pTexture->GetActualHeight(), m_pTexture->GetImageFormat() );
}

//-----------------------------------------------------------------------------
void S1Wrapper_Texture_t::CreateS1Texture()
{
	if ( m_nFlags & TEXTUREFLAGS_PROCEDURAL )
	{
		ImageFormat nFmt = m_textureDesc.m_nImageFormat;
		// Translate dxt1/dxt5 to the dxt1_runtime / dxt5_runtime 
		// to avoid firing assert in CTexture::InitProceduralTexture
		// (compressed textures are not allowed for procedural textures except the runtime ones)
		switch ( m_textureDesc.m_nImageFormat )
		{
		case IMAGE_FORMAT_DXT1:
			nFmt = IMAGE_FORMAT_DXT1_RUNTIME;
			break;
		case IMAGE_FORMAT_DXT5:
			nFmt = IMAGE_FORMAT_DXT5_RUNTIME;
			break;
		case IMAGE_FORMAT_DXT3:
			nFmt = IMAGE_FORMAT_DXT3_RUNTIME;
			break;
		default:
			break;
		}

		m_pTexture = g_pMaterialSystem->CreateProceduralTexture( m_textureName.String(), TEXTURE_GROUP_OTHER,
																 m_textureDesc.m_nWidth, m_textureDesc.m_nHeight,
																 nFmt, m_nFlags );

		WrapTextureLog( m_textureName.String(), m_textureDesc.m_nWidth, m_textureDesc.m_nHeight, "Create src1 proc tex", 0, m_pTexture );
	}
	else if ( m_nFlags & TEXTUREFLAGS_RENDERTARGET )
	{
		if ( m_bRenderTargetWithDepth )
		{
			m_pTexture = g_pMaterialSystem->CreateNamedRenderTargetTextureEx( m_textureName.String(),
																			  m_textureDesc.m_nWidth, m_textureDesc.m_nHeight,
																			  RT_SIZE_OFFSCREEN, m_textureDesc.m_nImageFormat, MATERIAL_RT_DEPTH_SEPARATE,
																			  m_nFlags,
																			  0 );
		}
		else
		{
			m_pTexture = g_pMaterialSystem->CreateNamedRenderTargetTextureEx( m_textureName.String(),
																			  m_textureDesc.m_nWidth, m_textureDesc.m_nHeight,
																			  RT_SIZE_NO_CHANGE, m_textureDesc.m_nImageFormat, MATERIAL_RT_DEPTH_NONE,
																			  m_nFlags,
																			  0 );
		}

		WrapTextureLog( m_textureName.String(), m_textureDesc.m_nWidth, m_textureDesc.m_nHeight, "Create src1 RT tex", 0, m_pTexture );
	}
	
	if ( !m_pTexture )
	{
		Warning( "Failed to create texture corresponding to %s\n", m_textureName.String() );
		// destroy VTF or data
		S1Wrapper_SetTextureDataCmd_t cmd;
		while ( m_pSetTextureDataCmds->PopItem( &cmd ) )
		{
			cmd.ReleaseResources();
		}
	}
	else
	{
		m_pTexture->ExcludeTextureFromForceIntoHardware( true );
		Assert( m_pTexture->GetReferenceCount() == 1 );
	}
}

//-----------------------------------------------------------------------------
void S1Wrapper_Texture_t::SetTextureData( IVTFTexture* pVTF, ImageFormat fmt, const void *pData, int nDataSize, Rect3D_t const *pSubRect, const CUtlDelegate< void( const void * )> *pDataRecycleDelegate )
{
	// Create a SetTextureData command and add it to the list
	// The data will actually get uploaded on the render thread

	S1Wrapper_SetTextureDataCmd_t cmd;
	cmd.m_pVTFFromFile = pVTF;
	cmd.m_pData = nullptr;
	cmd.m_DataRecycleDelegate.Clear();
#ifdef S1WRAP_TEXTURE_DETAILED_LOG
	cmd.m_textureName = m_textureName;
#endif
	if ( !pVTF )
	{
		// has the caller let us take over the data so we can bypass copying it?
		if ( pDataRecycleDelegate )
		{
			cmd.m_pData = ( void * )pData;
			cmd.m_DataRecycleDelegate = *pDataRecycleDelegate;
		}
		else
		{
			cmd.m_pData = malloc( nDataSize );
			memcpy( cmd.m_pData, pData, nDataSize );
			WrapTextureLog( m_textureName.String(), m_textureDesc.m_nWidth, m_textureDesc.m_nHeight, "S1Wrapper_SetTextureDataCmd_t alloc", nDataSize, cmd.m_pData );
		}
		cmd.m_nDataSize = nDataSize;
		cmd.m_subRect = *pSubRect;
		cmd.m_nFmt = fmt;

		if ( ( cmd.m_subRect.x == 0 ) && ( cmd.m_subRect.y == 0 ) && ( cmd.m_subRect.z == 0 )
			&& ( cmd.m_subRect.width == m_textureDesc.m_nWidth ) && ( cmd.m_subRect.height == m_textureDesc.m_nHeight ) && ( cmd.m_subRect.depth == m_textureDesc.GetDepth() ) )
		{
			// Overwriting the entire texture -> delete previous SetTextureData commands
			S1Wrapper_SetTextureDataCmd_t cmdToDelete;
			while ( m_pSetTextureDataCmds->PopItem( &cmdToDelete ) )
			{
				// Free data allocated in SetTextureData
				cmdToDelete.ReleaseResources();
			}
		}
	}
	m_pSetTextureDataCmds->PushItem( cmd );
}

//-----------------------------------------------------------------------------
// Responsible for setting the texture data of the src1 ITexture
// Done on the render thread
void S1Wrapper_Texture_t::UploadTextureData()
{
	if ( !m_pSetTextureDataCmds->Count() )
	{
		// Nothing to do
		return;
	}

	if ( !m_pTexture )
	{
		return;
	}

	if ( !m_pRegenerator )
	{
		m_pRegenerator = new CPanoramaProceduralRegen;
	}

	S1Wrapper_SetTextureDataCmd_t cmd;
	while ( m_pSetTextureDataCmds->PopItem( &cmd ) )
	{
		m_pRegenerator->m_params = cmd;

		if ( !cmd.m_pVTFFromFile )
		{
			//Msg( ">>>Set Proc Regen %p for texture  %s:%p  subRect %d, %d, %dx%d\n", pRegen, pTexture->GetName(), pTexture, pSubRect->x, pSubRect->y, pSubRect->width, pSubRect->height );

			m_pTexture->SetTextureRegenerator( m_pRegenerator, false );		// "false" stops us releasing existing.

			Rect_t updateRect;
			updateRect.x = cmd.m_subRect.x;
			updateRect.y = cmd.m_subRect.y;
			updateRect.width = cmd.m_subRect.width;
			updateRect.height = cmd.m_subRect.height;

			// Force a download
			if ( materials->CanDownloadTextures() )
			{
				m_pTexture->Download( &updateRect );
			}
			else
			{
				// Device lost, just update our backing copy
				m_pRegenerator->UpdateBackingStore( m_pTexture );
			}

			// Free data allocated in SetTextureData
			cmd.ReleaseResources();
		}
		else
		{
			//Msg( ">>>Set VTF Regen %p for texture  %s:%p\n", pRegen, pTexture->GetName(), pTexture );

			m_pTexture->SetTextureRegenerator( m_pRegenerator, false );		// "false" stops us releasing existing.
			m_pTexture->Download();
		}
	}
}


//-----------------------------------------------------------------------------
void S1Wrapper_Texture_t::FillTextureDesc( int16 width, int16 height, ImageFormat imageFormat, bool bRenderTarget )
{
	bool bSupportsNPO2 = g_pMaterialSystemHardwareConfig->SupportsNPO2Textures();

	// For now we just fill in width, height and format, which is all that panorama requires.
	
	if ( bRenderTarget || bSupportsNPO2 )
	{
		m_textureDesc.m_nWidth = width;
		m_textureDesc.m_nHeight = height;
		m_textureDesc.m_nDisplayRectWidth = 0;
		m_textureDesc.m_nDisplayRectHeight = 0;
	}
	else
	{
		m_textureDesc.m_nWidth = SmallestPowerOfTwoGreaterOrEqual( width );
		m_textureDesc.m_nHeight = SmallestPowerOfTwoGreaterOrEqual( height );

		m_textureDesc.m_nDisplayRectWidth = width;
		m_textureDesc.m_nDisplayRectHeight = height;
	}

	m_textureDesc.m_nImageFormat		= imageFormat;
}

//-----------------------------------------------------------------------------
ITexture *S1Wrapper_Texture_t::GetS1Texture()
{
	// About to make a draw call using the texture therefore
	//	1 - create the src1 ITexture if necessary
	//  2 - set texture data
	// Both operation need to be done on the render thread

	if ( !m_pTexture )
	{

#if ( PANDX_DRAW )
		if ( g_bPanDx )
		{
			if ( IsPanDxInsideOwnDx() )
			{
				PanDxFlushBatch( true );
				PanDxInvalidateRTShadowState();
			}
		}
#endif

		CreateS1Texture();
	}

	UploadTextureData();
	
	return m_pTexture;
}

//-----------------------------------------------------------------------------
void S1Wrapper_Texture_t::InvokeCompletionCallback()
{
	VPROF_BUDGET( "S1Wrapper_Texture_t::InvokeCompletionCallback()", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_completionCallback )
	{
		m_completionCallback( m_pCompletionCallbackContext );
	}
}

//==================================================================================================
//
//	External Functions
//
//==================================================================================================

//-----------------------------------------------------------------------------
HRenderTexture S1Wrapper_CreateTextureAsync( const char *pFileName, uint16 nUniqueId, S1WrapperTextureCompletionCallback_t callback, void *pCallbackContext )
{
	HRenderTexture hTexture = RESOURCE_HANDLE_INVALID;

	S1Wrapper_Texture_t *pTexture = new S1Wrapper_Texture_t();
	if ( pTexture && pTexture->InitFromFileAsync( pFileName, nUniqueId, callback, pCallbackContext ) )
	{
		hTexture = pTexture;
	}
	else
	{
		// Failed to create texture, free memory
		delete pTexture;
	}

	return hTexture;
}

//-----------------------------------------------------------------------------
HRenderTexture S1Wrapper_CreateTexture( const char *pResourceName, uint16 nUniqueId, const CTextureCreationDesc *pDescriptor, const CTextureDesc *pDataDesc, const void *pData, int nDataSize )
{
	HRenderTexture hTexture = RESOURCE_HANDLE_INVALID;

	S1Wrapper_Texture_t *pTexture = new S1Wrapper_Texture_t();
	if ( pTexture && pTexture->InitFromData( pResourceName, nUniqueId, pDescriptor, pDataDesc, pData, nDataSize ) )
	{
		hTexture = pTexture;
	}
	else
	{
		// Failed to create texture, free memory
		delete pTexture;
	}

	return hTexture;
}

//-----------------------------------------------------------------------------
void S1Wrapper_DestroyTexture( HRenderTexture hTexture )
{
	if ( hTexture.IsLoaded() )
	{
		S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hTexture.GetResourceHandle()->m_handle;
		
		delete pTexture;
	}
}

//-----------------------------------------------------------------------------
HRenderTexture S1Wrapper_CreateAlphaTexture( int32 iWidth, int32 iHeight )
{
	HRenderTexture hTexture = RESOURCE_HANDLE_INVALID;

	S1Wrapper_Texture_t *pTexture = new S1Wrapper_Texture_t();
	if ( pTexture )
	{
		char textureName[MAX_PATH];
		uint16 nAlphaTextureId = g_pResourceSystem->GetTextureUniqueId();
		V_sprintf_safe( textureName, "panorama_alpha_texture_%d", nAlphaTextureId );

		int nFlags = TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NODEBUGOVERRIDE | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_TRILINEAR | TEXTUREFLAGS_NOLOD;
				
		pTexture->m_pTexture = materials->CreatePanoramaAlphaTexture( textureName, TEXTURE_GROUP_OTHER, iWidth, iHeight, nFlags );
		pTexture->m_pTexture->ExcludeTextureFromForceIntoHardware( true );
		pTexture->m_nUniqueId = nAlphaTextureId;
		pTexture->FillTextureDesc( iWidth, iHeight, IMAGE_FORMAT_A8, false );

		hTexture = pTexture;
	}

	return hTexture;
}

//-----------------------------------------------------------------------------
void S1Wrapper_UpdateAlphaTexture( HRenderTexture hTexture, int32 xOffset, int32 yOffset, int32 iWidth, int32 iHeight, void *pImageData )
{
	if ( hTexture.IsLoaded() )
	{
		S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)hTexture.GetResourceHandle()->m_handle;

		materials->UpdatePanoramaAlphaTexture( pTexture->m_pTexture, xOffset, yOffset, iWidth, iHeight, pImageData );
	}
}

//-----------------------------------------------------------------------------
HRenderTexture S1Wrapper_FindFullFrameBuffer( int nIndex )
{
	static HRenderTextureStrong s_hFullFrameTextureArray[ 2 ];
	static char* rtNameArray[2] = { "_rt_FullFrameFB", "_rt_FullFrameFB2" };

	if ( nIndex >= 2 ) return RESOURCE_HANDLE_INVALID;

	if ( !s_hFullFrameTextureArray[nIndex].IsLoaded() )
	{
		ITexture *pTexture = materials->FindTexture( rtNameArray[nIndex], TEXTURE_GROUP_RENDER_TARGET );
		if ( !pTexture ) Error( "Can't find scratch RT %s\n", rtNameArray[ nIndex ] );
		pTexture->IncrementReferenceCount();

		S1Wrapper_Texture_t *pS1WrapperTexture = new S1Wrapper_Texture_t();
		pS1WrapperTexture->m_pTexture = pTexture;
		pS1WrapperTexture->FillTextureDesc( pTexture->GetActualWidth(), pTexture->GetActualHeight(), pTexture->GetImageFormat(), true );

		s_hFullFrameTextureArray[nIndex] = pS1WrapperTexture;
	}
	else
	{
		S1Wrapper_Texture_t *pS1WrapperTexture = (S1Wrapper_Texture_t *)s_hFullFrameTextureArray[nIndex].GetResourceHandle()->m_handle;
		pS1WrapperTexture->FillTextureDesc( pS1WrapperTexture->m_pTexture->GetActualWidth(), pS1WrapperTexture->m_pTexture->GetActualHeight(), pS1WrapperTexture->m_pTexture->GetImageFormat(), true );
	}

	return s_hFullFrameTextureArray[nIndex];
}

//-----------------------------------------------------------------------------
HRenderTexture S1Wrapper_FindEngineRT( const char *pchEngineRTName )
{
	if ( !pchEngineRTName || !pchEngineRTName[0] )
		return RESOURCE_HANDLE_INVALID;

	HRenderTexture hTexture = RESOURCE_HANDLE_INVALID;

	ITexture *pTexture = materials->FindTexture( pchEngineRTName, TEXTURE_GROUP_RENDER_TARGET );
	if ( pTexture )
	{
		pTexture->IncrementReferenceCount();

		S1Wrapper_Texture_t *pS1WrapperTexture = new S1Wrapper_Texture_t();
		if ( pS1WrapperTexture )
		{
			pS1WrapperTexture->m_pTexture = pTexture;
			pS1WrapperTexture->FillTextureDesc( pTexture->GetActualWidth(), pTexture->GetActualHeight(), pTexture->GetImageFormat(), true );

			hTexture = pS1WrapperTexture;
		}
	}
	
	return hTexture;
}

//--------------------------------------------------------------------------------------------------
// CRenderAttributes::SetTextureValue
// Used by source2surface to put the s1 ITexture* into the render attributes
//--------------------------------------------------------------------------------------------------

void CRenderAttributes::SetTextureValue( RenderAttrTexture_t nAttr, HRenderTexture txtr )
{
	if ( txtr.IsValid() )
	{
		S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)txtr.GetResourceHandle()->m_handle;
		ITexture *pS1Texture = pTexture ? pTexture->GetS1Texture() : nullptr;
		m_textureAttrs[ nAttr - ATTR_TEXTURE_MIN] = pS1Texture;
	}
}

//--------------------------------------------------------------------------------------------------
// Process a DDS file
// We load the main image 2d image, supporting DXT, RGB, RGBA modes
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Code from DirectXTK...
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------
// DirectXTexDDS.cpp
//  
// DirectX Texture Library - Microsoft DirectDraw Surface (DDS) file format reader/writer
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------


#include "dds.h"

struct CPanDDSProps
{
	DDS_HEADER*		pHeader;
	DDS_HEADER_DXT10* pHeaderDX10;
	uint32          width;
	uint32          height;     // Should be 1 for 1D textures
	uint32          depth;      // Should be 1 for 1D or 2D textures
	uint32          arraySize;  // For cubemap, this is a multiple of 6
	uint32          mipLevels;
	uint32	        miscFlags;
	uint32		    miscFlags2;
	DXGI_FORMAT     dxgiFormat;
	DDS_PIXELFORMAT	ddsFormat;
	DDS_RESOURCE_DIMENSION dimension;

	bool IsCubemap() const { return (miscFlags & DDS_RESOURCE_MISC_TEXTURECUBE) != 0; }

	bool IsPMAlpha() const { return ((miscFlags2 & DDS_MISC_FLAGS2_ALPHA_MODE_MASK) == DDS_ALPHA_MODE_PREMULTIPLIED) != 0; }
	void  SetAlphaMode( DDS_ALPHA_MODE mode ) { miscFlags2 = (miscFlags2 & ~DDS_MISC_FLAGS2_ALPHA_MODE_MASK) | uint32(mode); }
	DDS_ALPHA_MODE  GetAlphaMode() const { return (DDS_ALPHA_MODE)(miscFlags2 & DDS_MISC_FLAGS2_ALPHA_MODE_MASK); }

	bool IsVolumemap() const { return (dimension == DDS_DIMENSION_TEXTURE3D); }
	// Helper for dimension
};

// Conversion flags.
// Since some legacy dx9 pixel formats do not exist in DXGI, these flags
// allow us to use the DXGI pixel formats, but we can support the obsolete 24bpp format or
// expand them as we please.
// The alternative is to have two sets of code which use different pixel format specifiers
//

enum CONVERSION_FLAGS
{
	CONV_FLAGS_NONE = 0x0,
	CONV_FLAGS_EXPAND = 0x1,      // Conversion requires expanded pixel size
	CONV_FLAGS_NOALPHA = 0x2,      // Conversion requires setting alpha to known value
	CONV_FLAGS_SWIZZLE = 0x4,      // BGR/RGB order swizzling required
	CONV_FLAGS_PAL8 = 0x8,      // Has an 8-bit palette
	CONV_FLAGS_888 = 0x10,     // Source is an 8:8:8 (24bpp) format
	CONV_FLAGS_565 = 0x20,     // Source is a 5:6:5 (16bpp) format
	CONV_FLAGS_5551 = 0x40,     // Source is a 5:5:5:1 (16bpp) format
	CONV_FLAGS_4444 = 0x80,     // Source is a 4:4:4:4 (16bpp) format
	CONV_FLAGS_44 = 0x100,    // Source is a 4:4 (8bpp) format
	CONV_FLAGS_332 = 0x200,    // Source is a 3:3:2 (8bpp) format
	CONV_FLAGS_8332 = 0x400,    // Source is a 8:3:3:2 (16bpp) format
	CONV_FLAGS_A8P8 = 0x800,    // Has an 8-bit palette with an alpha channel
	CONV_FLAGS_DX10 = 0x10000,  // Has the 'DX10' extension header
	CONV_FLAGS_PMALPHA = 0x20000,  // Contains premultiplied alpha data
	CONV_FLAGS_L8 = 0x40000,  // Source is a 8 luminance format 
	CONV_FLAGS_L16 = 0x80000,  // Source is a 16 luminance format 
	CONV_FLAGS_A8L8 = 0x100000, // Source is a 8:8 luminance format 
};

DXGI_FORMAT GetDXGIFormat(const DDS_HEADER& hdr, const DDS_PIXELFORMAT& ddpf, DWORD& convFlags);

bool CalcPitch( uint32 &rowPitch, uint32 &slicePitch, DDS_HEADER *pHeader, DXGI_FORMAT dxgiFormat, 
				DDS_PIXELFORMAT ddsFormat, uint32 width, uint32 height );

inline bool IsMultipleOf4( int value )
{
	// NOTE: This catches powers of 2 less than 4 also
	return ( value <= 2 ) || ( ( value & 0x3 ) == 0 );
}

bool ConvertDDS( void * pDDS, int nDDS, CUtlBuffer &bufOutput, int &width, int &height, ImageFormat &fmt )
{
	DDS_HEADER* pHeader = 0;
	DDS_HEADER_DXT10* pHeaderDX10 = 0;
	uint8 *pPixels = 0;

	//--------------------------------------------------------------------------------------------------
	// Check this is a DDS, and if it has a a DX10 header
	//--------------------------------------------------------------------------------------------------

	CPanDDSProps ddsProps;
	ddsProps.pHeaderDX10 = NULL;
	memset( &ddsProps, 0, sizeof( ddsProps ) );

	if ( nDDS < (sizeof( DDS_HEADER ) + sizeof( uint32_t )) )
	{
		return false;
	}

	uint32 dwMagicNumber = *static_cast<const uint32*>(pDDS);
	if ( dwMagicNumber != DDS_MAGIC )
	{
		return false;
	}

	pHeader = (DDS_HEADER*)((uint8*)pDDS + sizeof( uint32 ));
	pPixels = (uint8*)(pHeader+1);
	ddsProps.pHeader = pHeader;
	if ( pHeader->size != sizeof( DDS_HEADER )
		|| pHeader->ddspf.size != sizeof( DDS_PIXELFORMAT ) )
	{
		return false;
	}

	if ( (pHeader->ddspf.flags & DDS_FOURCC)
		&& (MAKEFOURCC( 'D', 'X', '1', '0' ) == pHeader->ddspf.fourCC) )
	{
		if ( nDDS < (sizeof( DDS_HEADER ) + sizeof( DDS_HEADER_DXT10 ) + sizeof( uint32_t )) )
		{
			return false;
		}

		pHeaderDX10 = (DDS_HEADER_DXT10*)((uint8*)pHeader + sizeof( DDS_HEADER ));
		pPixels = (uint8*)(pHeaderDX10 + 1);
	}

	//--------------------------------------------------------------------------------------------------
	// Extract properties into ddsProps 
	//--------------------------------------------------------------------------------------------------

	ddsProps.mipLevels = MAX( pHeader->mipMapCount, 1 );
	ddsProps.ddsFormat = pHeader->ddspf;

	if ( pHeaderDX10 )
	{
		ddsProps.width = pHeader->width;
		ddsProps.height = pHeader->height;
		ddsProps.depth = 1;	// overwritten for 3d
		ddsProps.dimension = (DDS_RESOURCE_DIMENSION)pHeaderDX10->resourceDimension;

		ddsProps.arraySize = pHeaderDX10->arraySize;
		if ( ddsProps.arraySize == 0 ) return false;

		ddsProps.dxgiFormat = pHeaderDX10->dxgiFormat;
		if ( !IsValid( ddsProps.dxgiFormat ) || IsPalettized( ddsProps.dxgiFormat ) ) return false;

		ddsProps.miscFlags = pHeaderDX10->miscFlag & ~DDS_RESOURCE_MISC_TEXTURECUBE;

		switch ( pHeaderDX10->resourceDimension )
		{
		case DDS_DIMENSION_TEXTURE1D:
			if ( (pHeader->flags & DDS_HEIGHT) && pHeader->height != 1 ) return false;
			ddsProps.height = 1;
			break;

		case DDS_DIMENSION_TEXTURE2D:
			if ( pHeaderDX10->miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE )
			{
				ddsProps.miscFlags |= DDS_RESOURCE_MISC_TEXTURECUBE;
				ddsProps.arraySize *= 6;
			}
			break;

		case DDS_DIMENSION_TEXTURE3D:
			if ( !(pHeader->flags & DDS_HEADER_FLAGS_VOLUME) ) return false;
			if ( ddsProps.arraySize > 1 ) return false;
			ddsProps.depth = pHeader->depth;
			break;

		default:
			return false;
		}

		ddsProps.miscFlags2 = pHeaderDX10->miscFlags2;
	}
	else
	{
		// Older DDS ( no 1D )
		ddsProps.arraySize = 1;
		ddsProps.width = pHeader->width;
		ddsProps.height = pHeader->height;

		if ( pHeader->flags & DDS_HEADER_FLAGS_VOLUME )
		{
			ddsProps.depth = pHeader->depth;
			ddsProps.dimension = DDS_DIMENSION_TEXTURE3D;
		}
		else
		{
			ddsProps.depth = 1;
			if ( pHeader->caps2 & DDS_CUBEMAP )
			{
				if ( (pHeader->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES ) return false;

				ddsProps.arraySize = 6;
				ddsProps.miscFlags |= DDS_RESOURCE_MISC_TEXTURECUBE;
			}

			ddsProps.dimension = DDS_DIMENSION_TEXTURE2D;
		}
		ddsProps.dxgiFormat = DXGI_FORMAT_UNKNOWN;
	}

	//--------------------------------------------------------------------------------------------------
	// Calc pitch, create array of image information for each distinct image
	//--------------------------------------------------------------------------------------------------

	// At this point reject anything that's not a 2D dds and add that feture if panorama needs it,

	if ( (ddsProps.mipLevels == 0)
		|| (ddsProps.arraySize == 0)
		|| (ddsProps.dimension != DDS_DIMENSION_TEXTURE2D) )
	{
		return false;
	}

	// Check that the DDS is wellformed ( images within it have known fmts/fit in the file size )

	int pixelSize = 0;
	int nImages = 0;

	for ( uint32 item = 0; item < ddsProps.arraySize; ++item )
	{
		uint32 w = ddsProps.width;
		uint32 h = ddsProps.height;

		for ( uint32 level = 0; level < ddsProps.mipLevels; ++level )
		{
			uint32 rowPitch, slicePitch;

			if ( !CalcPitch( rowPitch, slicePitch, pHeader, ddsProps.dxgiFormat, ddsProps.ddsFormat, w, h ) )
			{
				return false;
			}

			pixelSize += slicePitch;
			nImages++;


			if ( h > 1 )
				h >>= 1;

			if ( w > 1 )
				w >>= 1;
		}
	}

	if ( (nImages == 0)
		|| ((pPixels + pixelSize) > ((uint8*)pDDS + nDDS)) )
	{
		return false;
	}

	// Allocate array of image structs

	struct CDDSImage
	{
		uint32      width;
		uint32      height;
		uint32      rowPitch;
		uint32      slicePitch;
		uint8_t*    pixels;
	};

	CUtlVector< CDDSImage > images(0,nImages);

	uint8_t* pPix = pPixels;

	for ( uint32 item = 0; item < ddsProps.arraySize; ++item )
	{
		uint32 w = ddsProps.width;
		uint32 h = ddsProps.height;

		for ( uint32 level = 0; level < ddsProps.mipLevels; ++level )
		{
			int index = images.AddToTail();

			if ( index >= nImages )
			{
				return false;
			}

			uint32 rowPitch, slicePitch;
			CalcPitch( rowPitch, slicePitch, pHeader, ddsProps.dxgiFormat, ddsProps.ddsFormat, w, h );

			images[ index ].width = w;
			images[ index ].height = h;
			images[ index ].rowPitch = rowPitch;
			images[ index ].slicePitch = slicePitch;
			images[ index ].pixels = pPix;
			++index;

			pPix += slicePitch;
			if ( pPix > ((uint8*)pDDS + nDDS) )
			{
				return false;
			}

			if ( h > 1 )
				h >>= 1;

			if ( w > 1 )
				w >>= 1;
		}
	}

	if ( nImages > 0 )
	{
		// Determine if this is a format we can pass to srcengine

		DXGI_FORMAT srcFmt;

		uint32 flags = 0;

		DWORD convflags = 0;

		if (ddsProps.dxgiFormat == DXGI_FORMAT_UNKNOWN) // legacy DDS ?
		{
			srcFmt = GetDXGIFormat(*pHeader, ddsProps.ddsFormat, convflags);
		}
		else
		{
			srcFmt = ddsProps.dxgiFormat;
		}

		switch (srcFmt)
		{
		case DXGI_FORMAT_BC1_UNORM:
			fmt = IMAGE_FORMAT_DXT1_RUNTIME;
			break;
		case DXGI_FORMAT_BC2_UNORM:
			fmt = IMAGE_FORMAT_DXT3_RUNTIME;
			break;
		case DXGI_FORMAT_BC3_UNORM:
			fmt = IMAGE_FORMAT_DXT5_RUNTIME;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			if (convflags &  CONV_FLAGS_888)
			{
				fmt = IMAGE_FORMAT_RGB888;
			}
			else
			{
				fmt = IMAGE_FORMAT_RGBA8888;
			}
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			fmt = IMAGE_FORMAT_BGRA8888;
			break;
		default:
			return false;			// unsupported fmt
		}

		if (images[0].slicePitch > 0)
		{
			width = images[0].width;
			height = images[0].height;
			
			// expand 888 -> 8888
			if (fmt == IMAGE_FORMAT_RGB888)
			{
				fmt = IMAGE_FORMAT_RGBA8888;
				uint32 newSize = (images[0].slicePitch / 3) * 4;
				bufOutput.EnsureCapacity(newSize);
				bufOutput.SeekPut(CUtlBuffer::SEEK_HEAD, newSize);

				uint8 *pSrc = images[0].pixels;
				uint8 *pDst = (uint8*)bufOutput.Base();
				for (int i = 0; i < (newSize / 4); i++ )
				{
					*pDst++ = *pSrc++;
					*pDst++ = *pSrc++;
					*pDst++ = *pSrc++;
					*pDst++ = 255;
				}

			}
			else
			{
				bufOutput.EnsureCapacity(images[0].slicePitch);
				bufOutput.SeekPut(CUtlBuffer::SEEK_HEAD, images[0].slicePitch);
				memcpy(bufOutput.Base(), images[0].pixels, images[0].slicePitch);

				if ( ( fmt == IMAGE_FORMAT_DXT1_RUNTIME ) || ( fmt == IMAGE_FORMAT_DXT5_RUNTIME ) || (fmt == IMAGE_FORMAT_DXT3_RUNTIME) )
				{
					if ( !IsMultipleOf4( width ) || !IsMultipleOf4( height ) )
					{
						Warning( "Unable to load dds file - image dimensions must be multiple of 4! Current dimensions: %dx%d\n", width, height );
						width = 0;
						height = 0;
						return false;
					}
				}
			}

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

}

//-------------------------------------------------------------------------------------
// Legacy format mapping table (used for DDS files without 'DX10' extended header)
//-------------------------------------------------------------------------------------
struct LegacyDDS
{
	DXGI_FORMAT     format;
	DWORD           convFlags;
	DDS_PIXELFORMAT ddpf;
};

const LegacyDDS g_LegacyDDSMap[] =
{
{ DXGI_FORMAT_BC1_UNORM,          CONV_FLAGS_NONE,        DDSPF_DXT1 }, // D3DFMT_DXT1
{ DXGI_FORMAT_BC1_UNORM,          CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D', 'X', 'T', '1'), 0, 0, 0, 0, 0 } },

{ DXGI_FORMAT_BC3_UNORM,          CONV_FLAGS_NONE,        DDSPF_DXT5 }, // D3DFMT_DXT5
{ DXGI_FORMAT_BC3_UNORM,          CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC | DDS_ALPHAPIXELS, MAKEFOURCC('D', 'X', 'T', '5'), 0, 0, 0, 0, 0 } },

{ DXGI_FORMAT_BC2_UNORM,          CONV_FLAGS_PMALPHA,     DDSPF_DXT2 }, // D3DFMT_DXT2
{ DXGI_FORMAT_BC2_UNORM,          CONV_FLAGS_NONE,        DDSPF_DXT3 }, // D3DFMT_DXT3
{ DXGI_FORMAT_BC3_UNORM,          CONV_FLAGS_PMALPHA,     DDSPF_DXT4 }, // D3DFMT_DXT4

{ DXGI_FORMAT_BC4_UNORM,          CONV_FLAGS_NONE,        DDSPF_BC4_UNORM },
{ DXGI_FORMAT_BC4_SNORM,          CONV_FLAGS_NONE,        DDSPF_BC4_SNORM },
{ DXGI_FORMAT_BC5_UNORM,          CONV_FLAGS_NONE,        DDSPF_BC5_UNORM },
{ DXGI_FORMAT_BC5_SNORM,          CONV_FLAGS_NONE,        DDSPF_BC5_SNORM },

{ DXGI_FORMAT_BC4_UNORM,          CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '1'), 0, 0, 0, 0, 0 } },
{ DXGI_FORMAT_BC5_UNORM,          CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '2'), 0, 0, 0, 0, 0 } },

{ DXGI_FORMAT_R8G8_B8G8_UNORM,    CONV_FLAGS_NONE,        DDSPF_R8G8_B8G8 }, // D3DFMT_R8G8_B8G8
{ DXGI_FORMAT_G8R8_G8B8_UNORM,    CONV_FLAGS_NONE,        DDSPF_G8R8_G8B8 }, // D3DFMT_G8R8_G8B8

{ DXGI_FORMAT_B8G8R8A8_UNORM,     CONV_FLAGS_NONE,        DDSPF_A8R8G8B8 }, // D3DFMT_A8R8G8B8 (uses DXGI 1.1 format)
{ DXGI_FORMAT_B8G8R8X8_UNORM,     CONV_FLAGS_NONE,        DDSPF_X8R8G8B8 }, // D3DFMT_X8R8G8B8 (uses DXGI 1.1 format)
{ DXGI_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_NONE,        DDSPF_A8B8G8R8 }, // D3DFMT_A8B8G8R8
{ DXGI_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_NOALPHA,     DDSPF_X8B8G8R8 }, // D3DFMT_X8B8G8R8
{ DXGI_FORMAT_R16G16_UNORM,       CONV_FLAGS_NONE,        DDSPF_G16R16 }, // D3DFMT_G16R16

{ DXGI_FORMAT_R10G10B10A2_UNORM,  CONV_FLAGS_SWIZZLE,{ sizeof(DDS_PIXELFORMAT), DDS_RGBA,      0, 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000 } }, // D3DFMT_A2R10G10B10 (D3DX reversal issue workaround)
{ DXGI_FORMAT_R10G10B10A2_UNORM,  CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_RGBA,      0, 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000 } }, // D3DFMT_A2B10G10R10 (D3DX reversal issue workaround)

{ DXGI_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_EXPAND | CONV_FLAGS_NOALPHA | CONV_FLAGS_888,       DDSPF_R8G8B8 }, // D3DFMT_R8G8B8

{ DXGI_FORMAT_B5G6R5_UNORM,       CONV_FLAGS_565,         DDSPF_R5G6B5 }, // D3DFMT_R5G6B5
{ DXGI_FORMAT_B5G5R5A1_UNORM,     CONV_FLAGS_5551,        DDSPF_A1R5G5B5 }, // D3DFMT_A1R5G5B5
{ DXGI_FORMAT_B5G5R5A1_UNORM,     CONV_FLAGS_5551 | CONV_FLAGS_NOALPHA,{ sizeof(DDS_PIXELFORMAT), DDS_RGB,       0, 16, 0x7c00,     0x03e0,     0x001f,     0x0000 } }, // D3DFMT_X1R5G5B5

{ DXGI_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_EXPAND | CONV_FLAGS_8332,{ sizeof(DDS_PIXELFORMAT), DDS_RGBA,      0, 16, 0x00e0,     0x001c,     0x0003,     0xff00 } }, // D3DFMT_A8R3G3B2
{ DXGI_FORMAT_B5G6R5_UNORM,       CONV_FLAGS_EXPAND | CONV_FLAGS_332,{ sizeof(DDS_PIXELFORMAT), DDS_RGB,       0,  8, 0xe0,       0x1c,       0x03,       0x00 } }, // D3DFMT_R3G3B2

{ DXGI_FORMAT_R8_UNORM,           CONV_FLAGS_NONE,        DDSPF_L8 }, // D3DFMT_L8
{ DXGI_FORMAT_R16_UNORM,          CONV_FLAGS_NONE,        DDSPF_L16 }, // D3DFMT_L16
{ DXGI_FORMAT_R8G8_UNORM,         CONV_FLAGS_NONE,        DDSPF_A8L8 }, // D3DFMT_A8L8
{ DXGI_FORMAT_R8G8_UNORM,         CONV_FLAGS_NONE,        DDSPF_A8L8_ALT }, // D3DFMT_A8L8 (alternative bitcount)

{ DXGI_FORMAT_A8_UNORM,           CONV_FLAGS_NONE,        DDSPF_A8 }, // D3DFMT_A8

{ DXGI_FORMAT_R16G16B16A16_UNORM, CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,   36,  0, 0,          0,          0,          0 } }, // D3DFMT_A16B16G16R16
{ DXGI_FORMAT_R16G16B16A16_SNORM, CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  110,  0, 0,          0,          0,          0 } }, // D3DFMT_Q16W16V16U16
{ DXGI_FORMAT_R16_FLOAT,          CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  111,  0, 0,          0,          0,          0 } }, // D3DFMT_R16F
{ DXGI_FORMAT_R16G16_FLOAT,       CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  112,  0, 0,          0,          0,          0 } }, // D3DFMT_G16R16F
{ DXGI_FORMAT_R16G16B16A16_FLOAT, CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  113,  0, 0,          0,          0,          0 } }, // D3DFMT_A16B16G16R16F
{ DXGI_FORMAT_R32_FLOAT,          CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  114,  0, 0,          0,          0,          0 } }, // D3DFMT_R32F
{ DXGI_FORMAT_R32G32_FLOAT,       CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  115,  0, 0,          0,          0,          0 } }, // D3DFMT_G32R32F
{ DXGI_FORMAT_R32G32B32A32_FLOAT, CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,  116,  0, 0,          0,          0,          0 } }, // D3DFMT_A32B32G32R32F

{ DXGI_FORMAT_R32_FLOAT,          CONV_FLAGS_NONE,{ sizeof(DDS_PIXELFORMAT), DDS_RGB,       0, 32, 0xffffffff, 0x00000000, 0x00000000, 0x00000000 } }, // D3DFMT_R32F (D3DX uses FourCC 114 instead)

{ DXGI_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_EXPAND | CONV_FLAGS_PAL8 | CONV_FLAGS_A8P8,{ sizeof(DDS_PIXELFORMAT), DDS_PAL8A,     0, 16, 0,          0,          0,          0 } }, // D3DFMT_A8P8
{ DXGI_FORMAT_R8G8B8A8_UNORM,     CONV_FLAGS_EXPAND | CONV_FLAGS_PAL8,{ sizeof(DDS_PIXELFORMAT), DDS_PAL8,      0,  8, 0,          0,          0,          0 } }, // D3DFMT_P8

{ DXGI_FORMAT_B4G4R4A4_UNORM,     CONV_FLAGS_4444,        DDSPF_A4R4G4B4 }, // D3DFMT_A4R4G4B4 (uses DXGI 1.2 format)
{ DXGI_FORMAT_B4G4R4A4_UNORM,     CONV_FLAGS_NOALPHA
| CONV_FLAGS_4444,{ sizeof(DDS_PIXELFORMAT), DDS_RGB,       0, 16, 0x0f00,     0x00f0,     0x000f,     0x0000 } }, // D3DFMT_X4R4G4B4 (uses DXGI 1.2 format)
{ DXGI_FORMAT_B4G4R4A4_UNORM,     CONV_FLAGS_EXPAND
| CONV_FLAGS_44,{ sizeof(DDS_PIXELFORMAT), DDS_LUMINANCEA,0,  8, 0x0f,       0x00,       0x00,       0xf0 } }, // D3DFMT_A4L4 (uses DXGI 1.2 format)

{ DXGI_FORMAT_YUY2,               CONV_FLAGS_NONE,        DDSPF_YUY2 }, // D3DFMT_YUY2 (uses DXGI 1.2 format)
{ DXGI_FORMAT_YUY2,               CONV_FLAGS_SWIZZLE,{ sizeof(DDS_PIXELFORMAT), DDS_FOURCC,    MAKEFOURCC('U','Y','V','Y'), 0, 0, 0, 0, 0 } }, // D3DFMT_UYVY (uses DXGI 1.2 format)

{ DXGI_FORMAT_R8G8_SNORM,         CONV_FLAGS_NONE,        DDSPF_V8U8 },     // D3DFMT_V8U8
{ DXGI_FORMAT_R8G8B8A8_SNORM,     CONV_FLAGS_NONE,        DDSPF_Q8W8V8U8 }, // D3DFMT_Q8W8V8U8
{ DXGI_FORMAT_R16G16_SNORM,       CONV_FLAGS_NONE,        DDSPF_V16U16 },   // D3DFMT_V16U16
};


DXGI_FORMAT GetDXGIFormat( const DDS_HEADER& hdr, const DDS_PIXELFORMAT& ddpf, DWORD& convFlags )
{
	uint32_t ddpfFlags = ddpf.flags;
	if (hdr.reserved1[9] == MAKEFOURCC('N', 'V', 'T', 'T'))
	{
		// Clear out non-standard nVidia DDS flags
		ddpfFlags &= ~0xC0000000 /* DDPF_SRGB | DDPF_NORMAL */;
	}

	const size_t MAP_SIZE = sizeof(g_LegacyDDSMap) / sizeof(LegacyDDS);
	size_t index = 0;
	for (index = 0; index < MAP_SIZE; ++index)
	{
		const LegacyDDS* entry = &g_LegacyDDSMap[index];

		if (ddpfFlags == entry->ddpf.flags)
		{
			if (entry->ddpf.flags & DDS_FOURCC)
			{
				if (ddpf.fourCC == entry->ddpf.fourCC)
					break;
			}
			else if (entry->ddpf.flags & DDS_PAL8)
			{
				if (ddpf.RGBBitCount == entry->ddpf.RGBBitCount)
					break;
			}
			else if (entry->ddpf.flags & DDS_ALPHA)
			{
				if (ddpf.RGBBitCount == entry->ddpf.RGBBitCount
					&& ddpf.ABitMask == entry->ddpf.ABitMask)
					break;
			}
			else if (entry->ddpf.flags & DDS_LUMINANCE)
			{
				if (entry->ddpf.flags & DDS_ALPHAPIXELS)
				{
					// LUMINANCEA
					if (ddpf.RGBBitCount == entry->ddpf.RGBBitCount
						&& ddpf.RBitMask == entry->ddpf.RBitMask
						&& ddpf.ABitMask == entry->ddpf.ABitMask)
						break;
				}
				else
				{
					// LUMINANCE
					if (ddpf.RGBBitCount == entry->ddpf.RGBBitCount
						&& ddpf.RBitMask == entry->ddpf.RBitMask)
						break;
				}
			}
			else if (entry->ddpf.flags & DDS_BUMPDUDV)
			{
				if (ddpf.RGBBitCount == entry->ddpf.RGBBitCount
					&& ddpf.RBitMask == entry->ddpf.RBitMask
					&& ddpf.GBitMask == entry->ddpf.GBitMask
					&& ddpf.BBitMask == entry->ddpf.BBitMask
					&& ddpf.ABitMask == entry->ddpf.ABitMask)
					break;
			}
			else if (ddpf.RGBBitCount == entry->ddpf.RGBBitCount)
			{
				if (entry->ddpf.flags & DDS_ALPHAPIXELS)
				{
					// RGBA
					if (ddpf.RBitMask == entry->ddpf.RBitMask
						&& ddpf.GBitMask == entry->ddpf.GBitMask
						&& ddpf.BBitMask == entry->ddpf.BBitMask
						&& ddpf.ABitMask == entry->ddpf.ABitMask)
						break;
				}
				else
				{
					// RGB
					if (ddpf.RBitMask == entry->ddpf.RBitMask
						&& ddpf.GBitMask == entry->ddpf.GBitMask
						&& ddpf.BBitMask == entry->ddpf.BBitMask)
						break;
				}
			}
		}
	}

	if (index >= MAP_SIZE)
		return DXGI_FORMAT_UNKNOWN;

	DWORD cflags = g_LegacyDDSMap[index].convFlags;
	DXGI_FORMAT format = g_LegacyDDSMap[index].format;

// 	if ((hdr.reserved1[9] == MAKEFOURCC('N', 'V', 'T', 'T'))
// 		&& (ddpf.flags & 0x40000000 /* DDPF_SRGB */))
// 	{
// 		format = MakeSRGB(format);
// 	}

	convFlags = cflags;

	return format;
}



enum CP_FLAGS
{
	CP_FLAGS_NONE = 0x0,      // Normal operation
	CP_FLAGS_LEGACY_DWORD = 0x1,      // Assume pitch is DWORD aligned instead of BYTE aligned
	CP_FLAGS_PARAGRAPH = 0x2,      // Assume pitch is 16-byte aligned instead of BYTE aligned
	CP_FLAGS_YMM = 0x4,      // Assume pitch is 32-byte aligned instead of BYTE aligned
	CP_FLAGS_ZMM = 0x8,      // Assume pitch is 64-byte aligned instead of BYTE aligned
	CP_FLAGS_PAGE4K = 0x200,    // Assume pitch is 4096-byte aligned instead of BYTE aligned
	CP_FLAGS_BAD_DXTN_TAILS = 0x1000,   // BC formats with malformed mipchain blocks smaller than 4x4
	CP_FLAGS_24BPP = 0x10000,  // Override with a legacy 24 bits-per-pixel format size
	CP_FLAGS_16BPP = 0x20000,  // Override with a legacy 16 bits-per-pixel format size
	CP_FLAGS_8BPP = 0x40000,  // Override with a legacy 8 bits-per-pixel format size
};


bool CalcPitch(uint32 &rowPitch, uint32 &slicePitch, DDS_HEADER *pHeader, DXGI_FORMAT dxgiFormat, DDS_PIXELFORMAT ddsFormat, uint32 width, uint32 height )
{

	DXGI_FORMAT fmt;

	uint32 flags = 0;

	if ( dxgiFormat == DXGI_FORMAT_UNKNOWN ) // legacy DDS ?
	{
		DWORD convflags = 0;

		fmt = GetDXGIFormat( *pHeader, ddsFormat, convflags );

		if ( convflags & CONV_FLAGS_888 )
		{
			flags |= CP_FLAGS_24BPP;
		}

	}
	else
	{
		fmt = dxgiFormat;
	}

	if ( fmt == DXGI_FORMAT_UNKNOWN ) return false;

	switch ( static_cast<int>(fmt) )
	{
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		assert( IsCompressed( fmt ) );
		{
			if ( flags & CP_FLAGS_BAD_DXTN_TAILS )
			{
				uint32 nbw = width >> 2;
				uint32 nbh = height >> 2;
				rowPitch = std::max<uint32>( 1, nbw * 8 );
				slicePitch = std::max<uint32>( 1, rowPitch * nbh );
			}
			else
			{
				uint32 nbw = std::max<uint32>( 1, (width + 3) / 4 );
				uint32 nbh = std::max<uint32>( 1, (height + 3) / 4 );
				rowPitch = nbw * 8;
				slicePitch = rowPitch * nbh;
			}
		}
		break;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		assert( IsCompressed( fmt ) );
		{
			if ( flags & CP_FLAGS_BAD_DXTN_TAILS )
			{
				uint32 nbw = width >> 2;
				uint32 nbh = height >> 2;
				rowPitch = std::max<uint32>( 1, nbw * 16 );
				slicePitch = std::max<uint32>( 1, rowPitch * nbh );
			}
			else
			{
				uint32 nbw = std::max<uint32>( 1, (width + 3) / 4 );
				uint32 nbh = std::max<uint32>( 1, (height + 3) / 4 );
				rowPitch = nbw * 16;
				slicePitch = rowPitch * nbh;
			}
		}
		break;

	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_YUY2:
		assert( IsPacked( fmt ) );
		rowPitch = ((width + 1) >> 1) * 4;
		slicePitch = rowPitch * height;
		break;

	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		assert( IsPacked( fmt ) );
		rowPitch = ((width + 1) >> 1) * 8;
		slicePitch = rowPitch * height;
		break;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
		assert( IsPlanar( fmt ) );
		rowPitch = ((width + 1) >> 1) * 2;
		slicePitch = rowPitch * (height + ((height + 1) >> 1));
		break;

// 	case DXGI_FORMAT_P010:
// 	case DXGI_FORMAT_P016:
// 	case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
// 	case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
// 	case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
// 		assert( IsPlanar( fmt ) );
// 		rowPitch = ((width + 1) >> 1) * 4;
// 		slicePitch = rowPitch * (height + ((height + 1) >> 1));
// 		break;

	case DXGI_FORMAT_NV11:
		assert( IsPlanar( fmt ) );
		rowPitch = ((width + 3) >> 2) * 4;
		slicePitch = rowPitch * height * 2;
		break;

// 	case WIN10_DXGI_FORMAT_P208:
// 		assert( IsPlanar( fmt ) );
// 		rowPitch = ((width + 1) >> 1) * 2;
// 		slicePitch = rowPitch * height * 2;
// 		break;

// 	case WIN10_DXGI_FORMAT_V208:
// 		assert( IsPlanar( fmt ) );
// 		rowPitch = width;
// 		slicePitch = rowPitch * (height + (((height + 1) >> 1) * 2));
// 		break;

// 	case WIN10_DXGI_FORMAT_V408:
// 		assert( IsPlanar( fmt ) );
// 		rowPitch = width;
// 		slicePitch = rowPitch * (height + ((height >> 1) * 4));
// 		break;

	default:
		assert( IsValid( fmt ) );
		assert( !IsCompressed( fmt ) && !IsPacked( fmt ) && !IsPlanar( fmt ) );
		{

			uint32 bpp;

			if ( flags & CP_FLAGS_24BPP )
				bpp = 24;
			else if ( flags & CP_FLAGS_16BPP )
				bpp = 16;
			else if ( flags & CP_FLAGS_8BPP )
				bpp = 8;
			else
				bpp = BitsPerPixel( fmt );

			if ( flags & (CP_FLAGS_LEGACY_DWORD | CP_FLAGS_PARAGRAPH | CP_FLAGS_YMM | CP_FLAGS_ZMM | CP_FLAGS_PAGE4K) )
			{
				if ( flags & CP_FLAGS_PAGE4K )
				{
					rowPitch = ((width * bpp + 32767) / 32768) * 4096;
					slicePitch = rowPitch * height;
				}
				else if ( flags & CP_FLAGS_ZMM )
				{
					rowPitch = ((width * bpp + 511) / 512) * 64;
					slicePitch = rowPitch * height;
				}
				else if ( flags & CP_FLAGS_YMM )
				{
					rowPitch = ((width * bpp + 255) / 256) * 32;
					slicePitch = rowPitch * height;
				}
				else if ( flags & CP_FLAGS_PARAGRAPH )
				{
					rowPitch = ((width * bpp + 127) / 128) * 16;
					slicePitch = rowPitch * height;
				}
				else // DWORD alignment
				{
					// Special computation for some incorrectly created DDS files based on
					// legacy DirectDraw assumptions about pitch alignment
					rowPitch = ((width * bpp + 31) / 32) * sizeof( uint32_t );
					slicePitch = rowPitch * height;
				}
			}
			else
			{
				// Default byte alignment
				rowPitch = (width * bpp + 7) / 8;
				slicePitch = rowPitch * height;
			}
		}
		break;
	}

	return true;

}


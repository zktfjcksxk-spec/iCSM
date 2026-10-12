//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include <direct.h>
#include "cmdlib.h"
#include "tier0/icommandline.h"
#include "tier1\keyvalues.h"
#include "utlbuffer.h"
#include "zip_utils.h"
#include "stripsrvl.h"
#include "stripblocks.h"

#define INCLUDE_PANORAMA_ZIPFILE_VERSION_ONLY
#include "panorama/iuifilesystem.h"

// Usage 

void Usage( void )
{
	fprintf( stderr, "usage: \n" );
	fprintf( stderr, "\tpanzip [-dbg]\n\n");
	fprintf( stderr, " zips the scripts, styles, layout directories, cfg file\n" );
	fprintf( stderr, " run from the GAME folder, out file written to GAME/csgo/%s\n", PANORAMA_ZIPFILE_NAME );
	fprintf( stderr, " using config file panzip.cfg\n" );
	fprintf( stderr, " -dbg: writes stripped results to game/panzip_temp\n" );
}

//////////////////////////////////////////////////////////////////////////
//
// Generic directory walk
// 
//////////////////////////////////////////////////////////////////////////


class DirWalkCallback
{
public:
	virtual void ItemCallback( const char *pFoundItem, bool bIsDir ) {}
};

// Walk directory, calling callback for each file found, then recursing over each subdir. 
// Callback is called for each subdir itself as well, either before or after processing subdir contents, 
// depending on value of bDirBeforeContents. Callback paths are relative, starting from initial parent directory
static void DirWalk( const char *pDirectory, DirWalkCallback *pDirWalkCallback, bool bDirBeforeContents )
{
	char pSearchString[MAX_PATH];
	V_snprintf(pSearchString, MAX_PATH, "%s\\*", pDirectory);

	// Walk folder
	FileFindHandle_t hFind;
	const char *pFoundFile = g_pFullFileSystem->FindFirstEx(pSearchString, "GAME", &hFind);

	CUtlVector< CUtlString > subDirs;
	for(; pFoundFile; pFoundFile = g_pFullFileSystem->FindNext(hFind))
	{
		if( !V_strnicmp(pFoundFile, ".", 2) || !V_strnicmp(pFoundFile, "..", 3) )
		{
			continue;
		}

		char pChildPath[MAX_PATH];
		V_snprintf( pChildPath, MAX_PATH, "%s\\%s", pDirectory, pFoundFile );

		if( g_pFullFileSystem->FindIsDirectory(hFind) )
		{
			subDirs.AddToTail( pChildPath );
			continue;
		}

		// Call callback
		pDirWalkCallback->ItemCallback( pChildPath, false );
	}

	g_pFullFileSystem->FindClose(hFind);

	int nCount = subDirs.Count();
	for( int i = 0; i < nCount; ++i )
	{
		if ( bDirBeforeContents )
		{
			pDirWalkCallback->ItemCallback( subDirs[i], true );
		}

		DirWalk( subDirs[i], pDirWalkCallback, bDirBeforeContents );

		if ( !bDirBeforeContents )
		{
			pDirWalkCallback->ItemCallback( subDirs[i], true );
		}
	}
}

// Dir walk: delete dir tree
class DirWalkDelete : public DirWalkCallback
{
public:

	virtual void ItemCallback( const char *pFoundItem, bool bIsDir )
	{
		bool bRes = true;

		if( bIsDir )
		{
			bRes = (_rmdir(pFoundItem) == 0);
		}
		else
		{
			bRes = (_unlink(pFoundItem) == 0);
		}

		if( !bRes )
		{
			Msg( "Error: unable to delete %s\n", pFoundItem );
			exit( -1 );
		}
	}
};

// Dir walk: create copy of dir tree under panzip_temp
class DirWalkCopyHierarchy : public DirWalkCallback
{
public:
	virtual void ItemCallback(const char *pFoundItem, bool bIsDir)
	{
		if(bIsDir)
		{
			char buff[1024];
			sprintf(buff, "panzip_temp\\%s", pFoundItem);
			if ( _mkdir(buff) != 0 )
			{
				Msg("Error: unable to create directory %s\n", buff );
				exit(-1);
			}
		}
	}
};

// Dir walk: create list of Panorama resources
class DirWalkCollectResources : public DirWalkCallback
{
public:

	DirWalkCollectResources( CUtlVector< const char* > *pResourceList ) : 
		m_pResourceList( pResourceList ) {}

	virtual void ItemCallback(const char *pFoundItem, bool bIsDir)
	{
		if ( bIsDir )
		{
			return;
		}

		// Check extension 
		const char* pExtension = V_GetFileExtensionSafe(pFoundItem);
		if( V_stricmp(pExtension, "xml") && V_stricmp(pExtension, "js") && V_stricmp(pExtension, "css") && V_stricmp(pExtension, "cfg"))
		{
			return;
		}

		m_pResourceList->AddToTail( _strdup( pFoundItem ) );
	}

private:

	CUtlVector< const char* > *m_pResourceList;
};

// Forward-declare crypto function
size_t launcher_keypair_signdata( const byte *pubData, int cbData, const byte *pubPrivateKey, int cbPrivateKey, byte *pbSignatureBuffer );

// Create an empty dir hierarchy in game/panzip_temp, that matches the hierarchy in game/csgo/panorama.
// This is for the results of stripping markup and comments. Assumes exe is running in the game folder
void CreateOutputDirTree()
{
	// Create dir, fail silently if already exists
	_mkdir("panzip_temp");

	// Delete contents if already exists
	DirWalkDelete delTree;
	DirWalk("panzip_temp", &delTree, false);

	// Create empty version of game/csgo/panorama tree
	_mkdir("panzip_temp\\csgo");
	_mkdir("panzip_temp\\csgo\\panorama");

	DirWalkCopyHierarchy copyTree;
	DirWalk("csgo\\panorama", &copyTree, true);
}

int main( int argc, char **argv )
{
	Msg( "\nValve Software - panzip.exe (%s)\n", __DATE__ );

	Usage();

	// Init the filesystem with GAME as "." the cwd
	CommandLine()->CreateCmdLine( argc, argv );
	FileSystem_Init( ".", 0, FS_INIT_COMPATIBILITY_MODE ); 	
	
	bool bDbg = CommandLine()->HasParm( "-dbg" );

	char outName0[ 1024 ];
	char outName1[ 1024 ];
	V_snprintf( outName0, MAX_PATH, "csgo/%s", PANORAMA_ZIPFILE_NAME );
	Q_MakeAbsolutePath( outName1, sizeof( outName1 ), outName0 );
	Msg( "PanZip writing %s...\n", outName1 );

	if ( bDbg )
	{
		CreateOutputDirTree();
	}

	// Call the directory scanner for XML/CSS/JS/CFG files
	CUtlVector< const char* > resourceList;
	DirWalkCollectResources collectResources( &resourceList );
	DirWalk( "csgo\\panorama", &collectResources, false );
	
	KeyValues kvPanzipCfg( "PanzipCfg" );
	kvPanzipCfg.LoadFromFile( g_pFullFileSystem,"bin\\panzip.cfg","GAME");
	
	CBlockDef *pBlockDefs;
	int nNumBlockDefs;
	if ( !BlockDefsFromKeyValues( &pBlockDefs, &nNumBlockDefs, &kvPanzipCfg ) )
	{
		exit(-1);
	}

	// Create a Zip of all the files we scanned
	IZip* pZip = IZip::CreateZip();
	for ( int i = 0; i < resourceList.Count(); i++)
	{
		if ( V_strstr( resourceList[ i ], "csgo\\" ) == resourceList[ i ] )
		{
			// Strip survival mode
			char dstFileName[ _MAX_PATH ];	
			if ( bDbg )
			{
				sprintf( dstFileName, "panzip_temp\\%s", resourceList[i] );
			}
			else
			{
				sprintf( dstFileName, "stripsrvl.temp" );
			}

			if ( !StripBlocks( resourceList[i], dstFileName, pBlockDefs, nNumBlockDefs ) )
			{
				exit(-1);
			}
			const char *relativeName = resourceList[ i ] + strlen( "csgo\\" );
			pZip->AddFileToZip( relativeName, dstFileName );

			if ( !bDbg )
			{
				_unlink( dstFileName );
			}
		}

		free( (void*)resourceList.Element( i ) );
	}

	// Prepare the output buffer
	CUtlBuffer bufOutput;
	pZip->SaveToBuffer( bufOutput );

	// Header bytes
	char chHeader[4] = { 'P', 'A', 'N', PANORAMA_ZIPFILE_VERSION };
	// Also add the version byte for signature digest
	bufOutput.PutChar( PANORAMA_ZIPFILE_VERSION );

	// Compute the digest
	byte ubDigest[4096];
	const byte CertificateData[] = {
#include "../../devtools/bin/certificates/panoramapack.private.h"
	};
	size_t numDigestBytes = launcher_keypair_signdata( ( const byte * ) bufOutput.Base(), bufOutput.TellPut(), CertificateData, sizeof( CertificateData ), ubDigest );

	// Save the ZIP to the output filename
	FILE *fp = fopen( outName1, "wb" );

	if ( fp == nullptr)
	{
		Msg( "Unable to open %s for writing\n", outName1 );
		exit (-1);
	}

	fwrite( chHeader, sizeof( chHeader ), 1, fp );
	fwrite( ubDigest, numDigestBytes, 1, fp );
	fwrite( bufOutput.Base(), bufOutput.TellPut(), 1, fp );
	fclose( fp );

	Msg( "PanZip done!\n" );

	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Cryptography support code
//
//////////////////////////////////////////////////////////////////////////

#ifdef Verify
#undef Verify
#endif

#define bswap_16 __bswap_16
#define bswap_64 __bswap_64

#include "cryptlib.h"
#include "rsa.h"
#include "osrng.h"

using namespace CryptoPP;
typedef AutoSeededX917RNG<AES> CAutoSeededRNG;

// Special usage here in the launcher without linking in tier0 tslist implementation
// list of auto-seeded RNG pointers
// these are very expensive to construct, so it makes sense to cache them

size_t launcher_keypair_signdata( const byte *pubData, int cbData, const byte *pubPrivateKey, int cbPrivateKey, byte *pbSignatureBuffer )
{
	try           // handle any exceptions crypto++ may throw
	{
		StringSource stringSourcePrivateKey( pubPrivateKey, cbPrivateKey, true );
		RSASSA_PKCS1v15_SHA_Signer rsaSigner( stringSourcePrivateKey );
		CAutoSeededRNG rng;
		// ( rsaSigner.MaxSignatureLength() );
		{
			size_t len = rsaSigner.SignMessage( rng, pubData, cbData, pbSignatureBuffer );
			return len;
		}
	}
	catch ( Exception e )
	{
	}
	catch ( ... )
	{
	}
	return 0;
}

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIFILESYSTEM_H
#define IUIFILESYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#define PANORAMA_ZIPFILE_VERSION 1
#define PANORAMA_ZIPFILE_NAME "panorama/code.pbin"

// panzip tool is using minimal compile and cannot include all the V8 headers, support just the zip file version #define
#ifndef INCLUDE_PANORAMA_ZIPFILE_VERSION_ONLY

#include "panoramatypes.h"
#include "utlbuffer.h"

namespace panorama
{

typedef void( __cdecl *FileChangeCallback_t )( const char *pFullPath );

//-----------------------------------------------------------------------------
// Purpose: FileSystem support
//-----------------------------------------------------------------------------

typedef void (LoadFileIntoBufferCallback_t)( const char * pchFile, CUtlBuffer &buf, bool bSuccess );

typedef void * HLOADINTOBUFFER;

class IUIFileSystem
{
public:
	// fully load the file into the buffer object
	virtual bool LoadFileIntoBuffer( const char *pchFile, CUtlBuffer &buf, bool bText, FileChangeCallback_t fileChangeCallback = NULL, uint nPadding = 0 ) = 0;

	// Perform an async file load
	virtual HLOADINTOBUFFER LoadFileIntoBufferAsync( const char *pchFile, CUtlBuffer &buf, bool bText, CUtlDelegate< LoadFileIntoBufferCallback_t > del ) = 0;

	// Cancel an async file load
	virtual bool CancelLoadFileIntoBufferAsync( HLOADINTOBUFFER hLoad ) = 0;

	// replace this file with the contents of the buffer object
	virtual bool SaveBufferToFile( CUtlBuffer &buf, const char *pchFile ) = 0;

	// return true if this file is on disk (or in a resource file)
	virtual bool FileExists( const char *pchFile ) = 0;

	virtual bool RestoreContentFilename( const char *pchFile, CUtlString &fixedFilename ) { return false; }
	virtual bool RestoreResourceFilename( const char *pchFile, CUtlString &fixedFilename ) { return false; }

	virtual char* LoadFromPanZip( const char* fname ) = 0;

	// Run frame on main thread
	virtual void RunFrame() = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
};


}

#endif // INCLUDE_PANORAMA_ZIPFILE_VERSION_ONLY

#endif // IUIFILESYSTEM_H

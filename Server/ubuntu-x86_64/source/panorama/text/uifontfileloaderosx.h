//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIFONTFILELOADEROSX_H
#define UIFONTFILELOADEROSX_H

#include "tier0/threadtools.h"
#include "tier1/utlstring.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/utlhashmap.h"
#include "uifontfile.h"
#include <Carbon/Carbon.h>

namespace panorama
{

class CUIFontLoaderOSX
{
public:
	CUIFontLoaderOSX();
	~CUIFontLoaderOSX();

	bool RegisterDir( const char *pszDirPath );
	static CUIFontLoaderOSX &GetInstance();

private:
	bool RegisterFont( const CUtlString &strFontPath, CUIFontPackage &package, int iFont );
	const CUtlBuffer *FindFontData( const char *pszFilePathname );

	CUtlHashMap<uint32, CUtlBuffer *> m_mapFonts;
	CThreadMutex m_lock;
	CUtlVector<ATSFontContainerRef> m_vecLoadedFonts;
};

}

#endif // UIFONTFILELOADEROSX_H

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIFONTFILELOADERLINUX_H
#define UIFONTFILELOADERLINUX_H
#pragma once

#include "tier0/threadtools.h"
#include "tier1/utlstring.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/utlhashmap.h"

#define UI_FONT_NEW_FACE_OVERRIDE "UI_Font_New_Face_Override"
typedef FT_Error( *FT_NewFace_Ptr )( FT_Library, const char *, FT_Long, FT_Face * );

class CUIFontPackage;

namespace panorama
{


class CUIFontLoaderLinux
{
public:
	CUIFontLoaderLinux();
	~CUIFontLoaderLinux();
	bool Initialize();
	void Shutdown();

	bool RegisterDir( const char *pszDirPath );
	bool RegisterFile( const char *pszFontName, const char *pszFullPath );
	static CUIFontLoaderLinux &GetInstance();

    FT_Error NewFaceOverride( FT_Library library, const char *pszFilePathname, FT_Long iFace, FT_Face *pFace );

private:
	bool RegisterFont( const char *pszRegisterName, const char *pszFullPath, CUIFontPackage *pPackage, int iPackageFont );
	const CUtlBuffer *FindFontData( const char *pszFilePathname );

#ifdef SOURCE_USE_SYSTEM_PANGO
	CUtlHashMap<uint32, CUtlString> m_mapNativeFonts;
	const char *NativeFontPath( const char *pszRegisterName );
#endif
	CUtlHashMap<uint32, CUtlBuffer *> m_mapFonts;
	FT_NewFace_Ptr m_pfnNewFaceOriginal;
	CThreadMutex m_lock;
};

}

#endif // UIFONTFILELOADERLINUX_H

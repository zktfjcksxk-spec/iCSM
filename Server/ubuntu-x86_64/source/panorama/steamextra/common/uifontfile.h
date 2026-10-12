//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIFONTFILE_H
#define UIFONTFILE_H

#ifdef _WIN32
#pragma once
#endif

#include "uifontfile_format.pb.h"
#include "tier0/threadtools.h"
#include "panorama/panoramasymbol.h"

// Turn this on to enable use of the below function which can be used to create .uifont package files containing other fonts 
// for shipping.  The fonts are encrypted so the user can't directly use them as a system font.
// #define ALLOW_FONT_PACKAGE_CREATION

#ifdef ALLOW_FONT_PACKAGE_CREATION
bool BGenerateFontPackage( const char *pchFontPackageSourcePath, const char *pchFontPackageOutputFile );
#endif


class CUIFontPackage
{
public:

	CUIFontPackage( const char *pchFontPackagePath );
	~CUIFontPackage();

	const char *GetPackageFileFullPath() { return m_strPackageFile.String(); }

	int GetNextFileIndex();
	int InvalidFileIndex() { return -1; }
	bool BGetFontNameAndData( int iIndex, CUtlString &symFontName, CUtlBuffer *pBufFontData );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		validator.ClaimMemory( m_pFontPackagePB );
		ValidateObj( m_strPackageFile );
	}
#endif

private:

	CUtlString m_strPackageFile;
	int m_iCurIndex;
	CUIFontFilePackagePB *m_pFontPackagePB;
};


#endif // UIFONTFILE_H

#define _GNU_SOURCE 1
#include <dlfcn.h>

#include "override.h"

//-----------------------------------------------------------------------------
// Override of FT_New_Face from libfreetype.so
//-----------------------------------------------------------------------------

static FT_NewFace_Ptr s_pfnNewFaceOverride;
static FT_NewFace_Ptr s_pfnNewFaceOriginal;

void SetOverrideOriginal_FT_New_Face()
{
	if ( s_pfnNewFaceOriginal == NULL )
	{
		// Attempt to get the freetype module handle without relying on the soname, which
		// is version dependent.
		void *pv = dlsym( RTLD_DEFAULT, "FT_New_Memory_Face" );
		Dl_info info;
		PlatModule_t hModule = PLAT_MODULE_INVALID;
		if ( dladdr( pv, &info ) != 0 )
		{
			Plat_LoadModuleRaw( info.dli_fname, &hModule, RTLD_LAZY );
		}
		else
		{
			Plat_LoadModuleRaw( "libfreetype.so.6", &hModule, RTLD_LAZY );
		}
		s_pfnNewFaceOriginal = ( FT_NewFace_Ptr )dlsym( hModule, "FT_New_Face" );
		dlclose( hModule );
	}
}

void SetOverride_FT_New_Face( FT_NewFace_Ptr pfnNewFace )
{
	s_pfnNewFaceOverride = pfnNewFace;
}

FT_NewFace_Ptr GetOriginal_FT_New_Face()
{
	if ( s_pfnNewFaceOriginal == NULL )
		SetOverrideOriginal_FT_New_Face();
	return s_pfnNewFaceOriginal;

}

FT_Error FT_New_Face( FT_Library library, const char *pszFilePathname, FT_Long iFace, FT_Face *aface )
{
	if ( s_pfnNewFaceOverride != NULL )
		return s_pfnNewFaceOverride( library, pszFilePathname, iFace, aface );
	if ( s_pfnNewFaceOriginal == NULL )
		SetOverrideOriginal_FT_New_Face();
	if ( s_pfnNewFaceOriginal != NULL )
		return s_pfnNewFaceOriginal( library, pszFilePathname, iFace, aface );
	return FT_Err_Cannot_Open_Resource;	
}

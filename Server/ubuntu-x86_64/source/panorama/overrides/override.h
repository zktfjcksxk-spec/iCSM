#ifndef OVERRIDE_H
#define OVERRIDE_H

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#define EXPORTED __attribute__ ((visibility("default")))

typedef FT_Error ( *FT_NewFace_Ptr )( FT_Library, const char *, FT_Long, FT_Face * );
EXPORTED FT_Error FT_New_Face( FT_Library library, const char *pszFilePathname, FT_Long iFace, FT_Face *aface );
EXPORTED FT_NewFace_Ptr GetOriginal_FT_New_Face();
EXPORTED void SetOverride_FT_New_Face( FT_NewFace_Ptr pfnNewFace );

#ifdef __cplusplus
};
#endif

#endif // OVERRIDE_H

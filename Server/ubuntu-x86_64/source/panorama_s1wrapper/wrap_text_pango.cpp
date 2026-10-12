//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "s1wrapper.h"
#include <tier0/memdbgon.h>

//--------------------------------------------------------------------------------------------------
// tier3 -- does nothing in real version
//--------------------------------------------------------------------------------------------------

#ifdef PLATFORM_WINDOWS
int SDLCALL SDL_setenv( const char *name, const char *value, int overwrite )
{
	return SetEnvironmentVariable( name, value );
}
#endif




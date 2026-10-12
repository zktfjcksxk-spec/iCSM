//======== Copyright 2010, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#pragma once

#include "steam/steamtypes.h"


// relative time string construction, always relative to current time
bool ConstructRelativeDateString( char *output, int cbOutput, const char *pchLocPrefix, RTime32 timeTarget, const char *szLanguage, bool bLongDate = false );

// constructs a string for some likely-today time (so probably HH:MM)
bool ConstructRecentTimeString( char *pszOutput, int cbOutput, const char *pchLocPrefix, RTime32 timeTarget, const char *szLanguage );
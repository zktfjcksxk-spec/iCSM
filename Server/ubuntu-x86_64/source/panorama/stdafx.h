//====== Copyright 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: precompiled header for panorama project
//
//=============================================================================

#ifndef STDAFX_H
#define STDAFX_H
#ifdef _WIN32
#pragma once
#endif

// base
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( SOURCE2_PANORAMA )
#define VERSION_SAFE_STEAM_API_INTERFACES
#endif

#ifdef WIN32
	#if defined( SOURCE2_PANORAMA )
		#include "tier0/basetypes.h"
	#else
		#include <minmax.h>
	#endif
// Panorama targets Vista+, and because it's a UI framework it needs lots of the stuff from the full windows header, not winlite.h
#define _WIN32_WINNT 0x0502
#include <WinSock2.h>
#include <windows.h>

// Conflicts with some protobuf extension stuff we use
#undef GetMessage
#endif

#ifdef POSIX
#define GL_GLEXT_PROTOTYPES
#if defined( SOURCE2_PANORAMA )
#include <SDL_opengl.h>
#else
#include <SDL2/SDL_opengl.h>
#endif
#endif

#ifdef OSX
#include <ApplicationServices/ApplicationServices.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif
#endif

// low level
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#ifdef PANORAMA_USE_S1WRAPPER
#include "vstdlib/vstrtools.h"
#include "time.h"
#include "tier1/strtools.h"
#endif


#if defined( SOURCE2_PANORAMA )
	#include "tier1/strtools.h"
#else
	#include "vstdlib/strtools.h"
#endif
#include "vstdlib/random.h"
#include "filesystem.h"
#if !defined( SOURCE2_PANORAMA )
#include "misc.h"
#endif
#include "tier1/keyvalues.h"
#include "vstdlib/osversion.h"
#include "tier0/vprof.h"

// tier1
#include "tier1/convar.h"
#include "tier1/checksum_crc.h"
#include "tier1/fmtstr.h"
#include "tier1/fileio.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "tier1/utlmap.h"

//common
#include "language.h"
#include "refcount.h"

// mathlib
#include "mathlib/mathlib.h"
#include "mathlib/vmatrix.h"

// panorama public
#include "panorama/panorama.h"
#include "panorama/transformations.h"
#include "panorama/input/keycodes.h"
#include "panorama/layout/panel2dfactory.h"
#include "panorama/input/mousecursors.h"
#include "panorama/input/iuiinput.h"
#include "panorama/uievent.h"
#include "panorama/uievents.h"
#include "panorama/uitoplevelwindow.h"
#include "panorama/layout/panel2dfactory.h"
#include "panorama/data/imageloader.h"
#include "panorama/data/panoramavideoplayer.h"

// panorama internal
#include "uiengine.h"
#include "renderer/styles.h"
#include "./layout/layoutfile.h"
#include "renderer/uirenderengine.h"
#include "input/uiinput.h"
#include "uicommands.h"
#include "localization/localize.h"
#include "steam/steam_api.h"


#include "../gcsdk/steamextra/tier1/utlstringbuilder.h"

#endif // STDAFX_H

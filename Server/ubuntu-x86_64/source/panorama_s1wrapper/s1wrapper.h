#ifndef INCLUDED_S1WRAPPER_H
#define INCLUDED_S1WRAPPER_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0502
#include <windows.h>
#include <shellapi.h>
#endif
#include "time.h"

#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/vprof.h"
#include "tier1/strtools.h"
#include "tier1/keyvalues.h"
#include "tier1/convar.h"
#include "tier1/checksum_crc.h"
#include "tier1/fmtstr.h"
#include "tier1/fileio.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "tier1/utlmap.h"
#include "filesystem.h"
#include "vstdlib/random.h"
#include "vstdlib/osversion.h"
#include "vstdlib/vstrtools.h"

#include "language.h"
#include "refcount.h"

#include "mathlib/mathlib.h"
#include "mathlib/vmatrix.h"

#include "materialsystem/imaterialsystem.h"		// s1
#include "engine/IEngineSound.h"

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

// s1 wrapper
#include "resourcesystem/iresourcesystem.h"
#include "rendersystem/vertexdata.h"

#include "assetsystem/iassetsystem.h"

#include "soundsystem/isoundsystem.h"
#include "soundsystem/isoundopsystem.h"

#include "materialsystem2/imaterialsystem2.h"

#endif // INCLUDED_S1WRAPPER_H
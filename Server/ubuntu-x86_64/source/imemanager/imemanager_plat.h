//============ Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Platform-specific IME implementation header.
//
//===========================================================================//

#pragma once

#ifdef PLATFORM_WINDOWS_PC

#define _WIN32_WINNT 0x0501
// UNICODE is critical, the Imm* ansi windows versions go haywire with IME
#define UNICODE 1
#include <windows.h>

#endif

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTLAYOUTPOSIX_H
#define UITEXTLAYOUTPOSIX_H


#ifdef LINUX
#include "text/uitextlayoutpango.h"
typedef panorama::CUITextLayoutPango CUITextLayout;
#elif defined(OSX)
#include "text/uitextlayoutpango.h"
typedef panorama::CUITextLayoutPango CUITextLayout;
#elif defined(WIN32)
#include "../text/uitextlayoutpango.h"
typedef panorama::CUITextLayoutPango CUITextLayout;
#else
#error "Needs a text layout impl"
#endif

#endif // UITEXTLAYOUTPOSIX_H 


//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifdef OSX
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#include "stdafx.h"
#include "../uitextservicespango.h"

using namespace panorama;

CUITextServicesPango s_PanoramaTextServices;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CUITextServicesPango, IUITextServices, PANORAMA_TEXT_SERVICES_INTERFACE_VERSION, s_PanoramaTextServices )

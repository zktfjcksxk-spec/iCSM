//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Header file that automatically includes a bunch of really commonly
// used panorama classes
//=============================================================================//
#pragma once


#ifndef CSGO_PORT
// In Source1 the pointers_to_members pragma is used within Scaleform and VGUI headers to ensure member function pointers
// are maximum size. This affects the panorama javascript bindings (see uijsregistration.h).
// We also add the pragma here so that we always assume the larger size pointers in source 1 for consistency, regardless
// of the order of includes. 
#define MEMBER_FUNCPTRS_MAXSIZE
#pragma pointers_to_members( full_generality, virtual_inheritance )
#endif


// 7ls - Specialized versions of ParseUIEventParam not yet required for CSGO source2
#ifdef CSGO_SOURCE2_UNSUPPORTED
// A little bit tricky, but this includes the primary template for ParseUIEventParam
#include "panorama/parseuieventparam.h"

// This is the specialization
#include "econ/econ_item_constants.h"
namespace panorama
{

template <>
inline bool ParseUIEventParam< item_definition_index_t >( item_definition_index_t *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	return ParseUIEventParam< uint16 >( pOut->GetRawPtrForWrite(), pPanel, pchEvent, pchNextParam );
}

template <>
inline bool ParseUIEventParam< itemid_t >( itemid_t *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	return ParseUIEventParam< uint64 >( pOut->GetRawPtrForWrite(), pPanel, pchEvent, pchNextParam );
}

}
#endif	// CSGO_SOURCE2_UNSUPPORTED

// Now include the rest of the ParseUIEventParam template stuff, etc.
#include "panorama/uievents.h"

#include "panorama/controls/panel2d.h"
#include "panorama/controls/button.h"
#include "panorama/controls/label.h"
#include "panorama/controls/textentry.h"
#include "panorama/controls/image.h"
#include "panorama/controls/slider.h"
#include "panorama/controls/progressbar.h"
#include "panorama/controls/dropdown.h"
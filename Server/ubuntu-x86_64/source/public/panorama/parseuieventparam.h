
#ifndef PARSEUIEVENTPARAM_H
#define PARSEUIEVENTPARAM_H
#pragma once

#include "iuipanel.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Helpers to create an event from string
//-----------------------------------------------------------------------------
template < typename T >
bool ParseUIEventParam( T *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
#ifdef PANORAMA_STRICT_EVENT_TEMPLATE_USAGE
	TEMPLATE_USAGE_INVALID( T );
#else
	AssertMsg( false, "ParseUIEventParam not implemented for type" );
	// Zero-fill so that the compiler doesn't complain about use
	// of uninitialized data, even though this code path is not functional.
	memset( pOut, 0, sizeof(*pOut) );
	return false;
#endif
}


template < typename T >
bool ParseUIEventParam( T *pOut, panorama::IUIWindow *pPanel, const char *pchEvent, const char **pchNextParam )
{
#ifdef PANORAMA_STRICT_EVENT_TEMPLATE_USAGE
	TEMPLATE_USAGE_INVALID( T );
#else
	AssertMsg( false, "ParseUIEventParam not implemented for type" );
	// Zero-fill so that the compiler doesn't complain about use
	// of uninitialized data, even though this code path is not functional.
	memset( pOut, 0, sizeof( *pOut ) );
	return false;
#endif
}


}

#endif // PARSEUIEVENTPARAM_H
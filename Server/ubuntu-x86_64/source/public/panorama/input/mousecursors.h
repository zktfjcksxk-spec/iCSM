//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef MOUSECURSORS_H
#define MOUSECURSORS_H
#pragma once

namespace panorama
{

enum EMouseCursors
{
	eMouseCursor_None = 0,
	eMouseCursor_Arrow,
	eMouseCursor_IBeam,
	eMouseCursor_SizeWE,
	eMouseCursor_SizeNS,
	eMouseCursor_Hand,
	eMouseCursor_Hand_Closed,
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
	eMouseCursor_PassThrough,
#endif
	eMouseCursor_Last
};

} // namespace panorama

#endif // MOUSECURSORS_H

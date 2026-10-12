//========= Copyright (c) Valve Corporation, All rights reserved. ============
//
// Purpose: SVG vector graphics parsing and rendering
//          Supports a subset of SVG features.
//          Uses parsifal for parsing and cairo for rendering.
//
//=============================================================================

#ifndef SVGLOADER_H
#define SVGLOADER_H
#pragma once

#include "tier0/platform.h"
#include "tier1/utlbuffer.h"
#include "svghelpers.h"

// Get dimensions out of SVG header
bool GetSVGDimensions( const byte *pubSVGData, int cubSVGData, uint32 &width, uint32 &height );

// Renders an SVG file into an RGBA memory buffer
bool ConvertSVGToRGBA( const byte *pubSVGData, int cubSVGData, CUtlBuffer &bufOutput, int &width, int &height, float fScaleFactor = -1.0f, const SvgAttributeOverrides_t* pAttributeOverrides = NULL );

#endif // SVGLOADER_H



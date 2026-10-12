//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Helper functions for parsing SVG files
//          Contains some duplication of functions from panorama csshelpers.h,
//          but with some tailoring for SVG, and available outside panorama.
//=============================================================================//

#ifndef SVGHELPERS_H
#define SVGHELPERS_H
#pragma once

#include "platform.h"
#include "utlsymbol.h"

class Color;

#define SVG_FILL_ATTRIBUTE_IS_URL	(1<<(k_ESvgAttributeMax + 0))
#define SVG_STROKE_ATTRIBUTE_IS_URL (1<<(k_ESvgAttributeMax + 1))

enum ESvgAttribute
{
	k_ESvgAttributeFill,
	k_ESvgAttributeFill_opacity,
	k_ESvgAttributeStroke,
	k_ESvgAttributeStroke_width,
	k_ESvgAttributeStroke_linecap,
	k_ESvgAttributeStroke_linejoin,
	k_ESvgAttributeStroke_opacity,
	k_ESvgAttributeOpacity,
	k_ESvgAttributeFill_rule,
	k_ESvgAttributeClip_path,
	k_ESvgAttributeClip_rule,
	k_ESvgAttributeMax
};

enum ESvgStrokeLineCap
{
	k_ESvgButt,
	k_ESvgCapRound,
	k_ESvgSquare
};

enum ESvgStrokeLineJoin
{
	k_ESvgMiter,
	k_ESvgJoinRound,
	k_ESvgBevel
};

enum ESvgFillRule
{
	k_ESvgNonzero,
	k_ESvgEvenodd
};

struct SvgAttributeValue_t {
	union {
		unsigned char m_color[4];
		float m_opacity;
		float m_length;
		ESvgStrokeLineCap m_strokeLineCap;
		ESvgStrokeLineJoin m_strokeLineJoin;
		ESvgFillRule m_fillRule;
		UtlSymId_t m_id;
	};
};

struct SvgAttributeOverrides_t
{
	SvgAttributeOverrides_t() : m_nFlags( 0 ) {}
	SvgAttributeValue_t m_overrides[k_ESvgAttributeMax];
	uint32 m_nFlags; // Bitflags
};

//-----------------------------------------------------------------------------
// Purpose: Common helper functions for dealing with SVG files
//-----------------------------------------------------------------------------
namespace SVGHelpers
{
	bool BParseFloat( float* pFloat, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseInt( int32* pFloat, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseColor( Color *pColor, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseNamedColor( Color *pColor, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseNumber( float *pNumber, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseIdent( char *rgchIdent, int cubIdent, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseURL( UtlSymId_t &elementID, CUtlSymbolTable &elementIDs, const char *pchString, const char **pchAfterParse = NULL );
	bool BParsePercent( float *pPercent, const char *pchString, const char **pchAfterParse = NULL );

	const char *SkipSpaces( const char *pchString );
	const char *SkipSpacesOrComma( const char *pchString );
	bool BSkipComma( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipLeftParen( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipRightParen( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipQuote( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipSlash( const char *pchString, const char **pchAfterParse = NULL );

	bool BParseCSSToken( char *pchToken, uint cubToken, const char *pchString, const char **pchAfterParse = NULL );

	//-------------------------------------------------------------------------------------------------
	// ParsePresentationAttribute
	// Helper function to convert a string value into an SVG presentation attribute (fill, stroke etc.) 
	// For use parsing SVG files, or parsing panorama image panels (which can optionally override some SVG attributes)
	//
	// pElementIDs is a string table for storing element ID strings, 
	// only required if attribute values may be a reference to another element,
	// (e.g. for referencing clip paths and linear gradients, not required when parsing panorama image panels)
	//-------------------------------------------------------------------------------------------------
	void ParsePresentationAttribute( SvgAttributeOverrides_t &svgOverrides, ESvgAttribute attribute_type, const char* pchValue, CUtlSymbolTable* pElementIDs = NULL );
};

#endif //SVGHELPERS_H

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "svghelpers.h"
#include "color.h"
#include "strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

const char k_rgchCSSDefaultTerm[] = { ';', '{', '}', ':' };

//-------------------------------------------------------------------------------------------------
// ParsePresentationAttribute
// Helper function to convert a string value into an SVG presentation attribute (fill, stroke etc.) 
// For use parsing SVG files, or parsing panorama image panels (which can optionally override some SVG attributes)
//
// pElementIDs is a string table for storing element ID strings, 
// only required if attribute values may be a reference to another element,
// (e.g. for referencing clip paths and linear gradients, not required when parsing panorama image panels)
//-------------------------------------------------------------------------------------------------
void SVGHelpers::ParsePresentationAttribute( SvgAttributeOverrides_t &svgOverrides, ESvgAttribute attribute_type, const char* pchValue, CUtlSymbolTable* pElementIDs /*= NULL*/ )
{
	bool bSuccess = true;

	SvgAttributeValue_t* pValue = &svgOverrides.m_overrides[attribute_type];

	float fOpacity = 0.0f;
	switch( attribute_type ) {
	case k_ESvgAttributeFill_opacity:
	case k_ESvgAttributeOpacity:
	case k_ESvgAttributeStroke_opacity:
		SVGHelpers::BParseFloat( &fOpacity, pchValue );
		if( fOpacity < 0.0f ) fOpacity = 0.0f;
		if( fOpacity > 1.0f ) fOpacity = 1.0f;
		pValue->m_opacity = fOpacity;
		break;
	case k_ESvgAttributeFill:
	case k_ESvgAttributeStroke:
		if( pElementIDs && SVGHelpers::BParseURL( pValue->m_id, *pElementIDs, pchValue ) )
		{
			uint32 urlFlag = (attribute_type == k_ESvgAttributeFill) ? SVG_FILL_ATTRIBUTE_IS_URL : SVG_STROKE_ATTRIBUTE_IS_URL;
			svgOverrides.m_nFlags |= urlFlag;
		}
		else
		{
			SVGHelpers::BParseColor( (Color*)&pValue->m_color, pchValue );
		}
		break;
	case k_ESvgAttributeStroke_width:
		SVGHelpers::BParseFloat( &pValue->m_length, pchValue );
		break;
	case k_ESvgAttributeStroke_linecap:
	{
		if( V_stricmp( pchValue, "butt" ) == 0 )
		{
			pValue->m_strokeLineCap = k_ESvgButt;
		}
		else if( V_stricmp( pchValue, "round" ) == 0 )
		{
			pValue->m_strokeLineCap = k_ESvgCapRound;
		}
		else if( V_stricmp( pchValue, "square" ) == 0 )
		{
			pValue->m_strokeLineCap = k_ESvgSquare;
		}
		else
		{
			Msg( "unknown strokeLineCap value: %s\n", pchValue );
			bSuccess = false;
		}
	}
	break;
	case k_ESvgAttributeStroke_linejoin:
	{
		if( V_stricmp( pchValue, "miter" ) == 0 )
		{
			pValue->m_strokeLineJoin = k_ESvgMiter;
		}
		else if( V_stricmp( pchValue, "round" ) == 0 )
		{
			pValue->m_strokeLineJoin = k_ESvgJoinRound;
		}
		else if( V_stricmp( pchValue, "bevel" ) == 0 )
		{
			pValue->m_strokeLineJoin = k_ESvgBevel;
		}
		else
		{
			Msg( "unknown strokeLineJoin value: %s\n", pchValue );
			bSuccess = false;
		}
	}
	break;
	case k_ESvgAttributeFill_rule:
	case k_ESvgAttributeClip_rule:
	{
		if( V_stricmp( pchValue, "nonzero" ) == 0 ) {
			pValue->m_fillRule = k_ESvgNonzero;
		}
		else if( V_stricmp( pchValue, "evenodd" ) == 0 ) {
			pValue->m_fillRule = k_ESvgEvenodd;
		}
		else {
			Msg( "unknown fill-rule value: %s\n", pchValue );
			bSuccess = false;
		}
	}
	break;
	case k_ESvgAttributeClip_path:
	{
		if( pElementIDs )
		{
			if( !SVGHelpers::BParseURL( pValue->m_id, *pElementIDs, pchValue ) )
			{
				Msg( "Invalid clip-path %s, expecting url(#...)\n", pchValue );
				bSuccess = false;
			}
		}
		else
		{
			Msg( "Clip-path not supported unless element string table supplied\n" );
			bSuccess = false;
		}
	}
	break;
	default:
		Msg( "Unknown SVG attribute type\n" );
		bSuccess = false;
		break;
	}
	if( bSuccess )
	{
		svgOverrides.m_nFlags |= (1 << attribute_type);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks if character is in list of characters.
//-----------------------------------------------------------------------------
static bool BCharInArray( char chFind, const char *pchCharacters, uint cchCharacters )
{
	for( uint i = 0; i < cchCharacters; i++ )
	{
		if( pchCharacters[i] == chFind )
			return true;
	}

	return false;
}

//----------------------------------------------------------------------------
// Purpose: Reads the next CSS token
//----------------------------------------------------------------------------
bool SVGHelpers::BParseCSSToken( char *pchToken, uint cubToken, const char *pchString, const char **pchAfterParse /*= NULL*/ )
{
	pchToken[0] = '\0';

	const char* pTmp = pchString;
	pTmp = SkipSpaces( pTmp );

	// read in the token until we hit a whitespace or a control character
	uint cWritten = 0;
	bool bInSingleQuote = false;
	bool bInDoubleQuote = false;
	bool bPreviousCharEscape = false;
	while( *pTmp )
	{
		// check for quote
		char c = *pTmp;
		if( c == '\\' )
		{
			bPreviousCharEscape = !bPreviousCharEscape;

			// if we didn't receive "\\", skip
			if( bPreviousCharEscape )
			{
				++pTmp;
				continue;
			}
		}
		else if( bPreviousCharEscape )
		{
			// we currently only support escapes in quotes
			if( !bInSingleQuote && !bInDoubleQuote )
				return false;

			// only support 3 characters
			if( c != '\\' && c != '"' && c != '\'' )
				return false;

			bPreviousCharEscape = false;
			// continue.. we should output this character
		}
		else if( c == '\'' && !bInDoubleQuote && !bPreviousCharEscape )
		{
			bInSingleQuote = !bInSingleQuote;
		}
		else if( c == '"' && !bInSingleQuote && !bPreviousCharEscape )
		{
			bInDoubleQuote = !bInDoubleQuote;
		}
		else if( !bInSingleQuote && !bInDoubleQuote )
		{
			bool bIsSpace = (V_isspace( c ) != 0);
			if( bIsSpace || BCharInArray( c, k_rgchCSSDefaultTerm, V_ARRAYSIZE( k_rgchCSSDefaultTerm ) ) )
			{
				// if this is the first character.. add to buffer
				if( cWritten == 0 )
				{
					pchToken[cWritten] = c;
					cWritten++;
					++pTmp;
				}
				break;
			}
		}

		// make sure we have space for this character
		if( cubToken - 1 < cWritten )
			return false;

		// output character
		pchToken[cWritten] = c;
		cWritten++;

		// next
		++pTmp;
	}

	if( *pTmp == '\0' )
	{
		// Only ok if we are at an ok termination point
		if( bPreviousCharEscape || bInDoubleQuote || bInSingleQuote )
		{
			return false;
		}
	}

	// terminate
	pchToken[cWritten] = '\0';

	if( pchAfterParse )
	{
		*pchAfterParse = pTmp;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if a string is only made up of numeric characters and a .
//-----------------------------------------------------------------------------
static bool IsFloat( const char *pchString )
{
	// skip negative sign if set
	if ( *pchString == '-' )
		pchString++;

	bool bFoundDecimal = false;
	while ( *pchString != '\0' )
	{
		char c = *pchString;
		if ( c == '.' )
		{
			if ( bFoundDecimal)
				return false;

			bFoundDecimal = true;
			pchString++;
			continue;
		}

		if ( c < '0' || c > '9' )
			return false;

		pchString++;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to first character that is not a space
//-----------------------------------------------------------------------------
const char *SVGHelpers::SkipSpaces( const char *pchString )
{
	while ( V_isspace( *pchString ) )
		pchString++;

	return pchString;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to first character that is not a space or comma, 
//          only skips single instance of comma 
//-----------------------------------------------------------------------------
const char *SVGHelpers::SkipSpacesOrComma( const char *pchString )
{
	bool bCommaSkipped = false;
	while( V_isspace( *pchString ) || (*pchString == ',' && !bCommaSkipped) )
	{
		if( *pchString == ',' )
		{
			bCommaSkipped = true;
		}
		pchString++;
	}
	return pchString;
}

//-----------------------------------------------------------------------------
// Purpose: Parses a string into a float and updates the supplied char ptr
//          to the first character after the float.
//          Returns false if the str is not a valid floating point number,
//          (and also sets the float to zero in this case, see strtod() spec).
//-----------------------------------------------------------------------------
bool SVGHelpers::BParseFloat( float* pFloat, const char *pchString, const char **pchAfterParse /*= NULL*/ )
{
	char* pEnd;
	*pFloat = strtod( pchString, &pEnd );
	if( pchString == pEnd )
	{
		return false;
	}
	if( pchAfterParse )
	{
		*pchAfterParse = pEnd;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Parses a string into an integer and updates the supplied char ptr
//          to the first character after the integer.
//          Returns false if the str is not a valid integer,
//          (and also sets the value to zero in this case, see strtol() spec).
//-----------------------------------------------------------------------------
bool SVGHelpers::BParseInt( int32* pInt, const char *pchString, const char **pchAfterParse /*= NULL*/ )
{
	char* pEnd;
	*pInt = strtol( pchString, &pEnd, 10 );
	if( (errno == ERANGE) || (pchString == pEnd) )
	{
		return false;
	}
	if( pchAfterParse )
	{
		*pchAfterParse = pEnd;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parse a color string
// Supports: #rgb, #rgba, #rrggbb, #rrggbbaa, rgb(), rgba(), named colors
// (supports CSS4 specification which is a superset of SVG allowed color formats,
//  e.g. rgba() formats are in CSS4 and not SVG1.1,
//  this is for compatibility with panorama where SVG style overrides can be supplied
//  as extra attributes of the panorama imagepanel)
//-----------------------------------------------------------------------------
bool SVGHelpers::BParseColor( Color *pColor, const char *pchString, const char **pchAfterParse )
{

	pchString = SkipSpaces( pchString );

	if ( V_strnicmp( pchString, "none", V_strlen( "none" ) ) == 0 )
	{
		pColor->SetRawColor( 0x00000000 );
		pchString += V_strlen( "none" );

		if(  pchAfterParse )
			*pchAfterParse = pchString;

		return true;
	}

	if ( pchString[0] == '#' )
	{
		// should be #rgb, #rgba, #rrggbb or #rrggbbaa
		const char *pchRGB = &pchString[1];
		int cchInput = V_strcspn( pchRGB, " ,;)" );
		int cchFull = cchInput;

		// If it's the short form, expand it out to the long form
		char szExpanded[ 9 ];
		if ( cchInput == 3 || cchInput == 4 )
		{
			szExpanded[ 0 ] = pchRGB[ 0 ];
			szExpanded[ 1 ] = pchRGB[ 0 ];
			szExpanded[ 2 ] = pchRGB[ 1 ];
			szExpanded[ 3 ] = pchRGB[ 1 ];
			szExpanded[ 4 ] = pchRGB[ 2 ];
			szExpanded[ 5 ] = pchRGB[ 2 ];

			if ( cchInput == 4 )
			{
				szExpanded[ 6 ] = pchRGB[ 3 ];
				szExpanded[ 7 ] = pchRGB[ 3 ];
				szExpanded[ 8 ] = '\0';
			}
			else
			{
				szExpanded[ 6 ] = '\0';
			}

			pchRGB = szExpanded;
			cchFull = cchInput * 2;
		}

		if ( ( cchFull != 6 && cchFull != 8 ) || !V_isvalidhex( pchRGB, cchFull ) )
			return false;

		unsigned char rgubColor[4];
		V_hextobinary( pchRGB, cchFull, rgubColor, V_ARRAYSIZE( rgubColor ) );

		// default to opaque if alpha not passed
		if ( cchFull < 8 )
			rgubColor[3] = 0xff;

		pColor->SetColor( rgubColor[0], rgubColor[1], rgubColor[2], rgubColor[3] );

		if ( pchAfterParse )
			*pchAfterParse = pchString + 1 + cchInput;

		return true;
	}
	else if( V_strnicmp( pchString, "rgb", V_strlen( "rgb" ) ) == 0 )
	{
		const char *pchRGBA = pchString + 3;

		bool bAlpha = false;
		if( pchRGBA[0] == 'a' )
		{
			bAlpha = true;
			++pchRGBA;
		}

		if( !BSkipLeftParen( pchRGBA, &pchRGBA ) )
			return false;

		float r, g, b;
		float a = 255;
		if( !BParseNumber( &r, pchRGBA, &pchRGBA ) )
			return false;

		if( !BSkipComma( pchRGBA, &pchRGBA ) )
			return false;

		if( !BParseNumber( &g, pchRGBA, &pchRGBA ) )
			return false;

		if( !BSkipComma( pchRGBA, &pchRGBA ) )
			return false;

		if( !BParseNumber( &b, pchRGBA, &pchRGBA ) )
			return false;
	
		if( bAlpha )
		{
			if( !BSkipComma( pchRGBA, &pchRGBA ) )
				return false;

			if( !BParseNumber( &a, pchRGBA, &pchRGBA ) )
				return false;

			// Alpha is 0.0-1.0 according to CSS standards
			// https://www.w3.org/TR/css-color-4/#rgb-functions

			// return a parsing error for now so users know we've changed our parsing
			// even though the standard says to clamp
			if ( a > 1.0f )
				return false;

			a *= 255.0f;
		}

		if( !BSkipRightParen( pchRGBA, &pchRGBA ) )
			return false;

		pColor->SetColor( (int)r, (int)g, (int)b, (int)a );

		if( pchAfterParse )
			*pchAfterParse = pchRGBA;
		return true;
	}
	else if ( BParseNamedColor( pColor, pchString, pchAfterParse ) )
	{
		return true;
	}

	return false;
}

struct SNamedColor
{
	const char *pszName;
	Color color;
};

bool SVGHelpers::BParseNamedColor( Color *pColor, const char *pchString, const char **pchAfterParse /* = NULL */ )
{
	// CSS named colors from http://dev.w3.org/csswg/css-color-4/#named-colors.
	// Keep alphabetical.
	static const SNamedColor s_namedColors[] = 
	{
		{ "aliceblue", Color( 240, 248, 255, 255 ) },
		{ "antiquewhite", Color( 250, 235, 215, 255 ) },
		{ "aqua", Color( 0, 255, 255, 255 ) },
		{ "aquamarine", Color( 127, 255, 212, 255 ) },
		{ "azure", Color( 240, 255, 255, 255 ) },
		{ "beige", Color( 245, 245, 220, 255 ) },
		{ "bisque", Color( 255, 228, 196, 255 ) },
		{ "black", Color( 0, 0, 0, 255 ) },
		{ "blanchedalmond", Color( 255, 235, 205, 255 ) },
		{ "blue", Color( 0, 0, 255, 255 ) },
		{ "blueviolet", Color( 138, 43, 226, 255 ) },
		{ "brown", Color( 165, 42, 42, 255 ) },
		{ "burlywood", Color( 222, 184, 135, 255 ) },
		{ "cadetblue", Color( 95, 158, 160, 255 ) },
		{ "chartreuse", Color( 127, 255, 0, 255 ) },
		{ "chocolate", Color( 210, 105, 30, 255 ) },
		{ "coral", Color( 255, 127, 80, 255 ) },
		{ "cornflowerblue", Color( 100, 149, 237, 255 ) },
		{ "cornsilk", Color( 255, 248, 220, 255 ) },
		{ "crimson", Color( 220, 20, 60, 255 ) },
		{ "cyan", Color( 0, 255, 255, 255 ) },
		{ "darkblue", Color( 0, 0, 139, 255 ) },
		{ "darkcyan", Color( 0, 139, 139, 255 ) },
		{ "darkgoldenrod", Color( 184, 134, 11, 255 ) },
		{ "darkgray", Color( 169, 169, 169, 255 ) },
		{ "darkgreen", Color( 0, 100, 0, 255 ) },
		{ "darkgrey", Color( 169, 169, 169, 255 ) },
		{ "darkkhaki", Color( 189, 183, 107, 255 ) },
		{ "darkmagenta", Color( 139, 0, 139, 255 ) },
		{ "darkolivegreen", Color( 85, 107, 47, 255 ) },
		{ "darkorange", Color( 255, 140, 0, 255 ) },
		{ "darkorchid", Color( 153, 50, 204, 255 ) },
		{ "darkred", Color( 139, 0, 0, 255 ) },
		{ "darksalmon", Color( 233, 150, 122, 255 ) },
		{ "darkseagreen", Color( 143, 188, 143, 255 ) },
		{ "darkslateblue", Color( 72, 61, 139, 255 ) },
		{ "darkslategray", Color( 47, 79, 79, 255 ) },
		{ "darkslategrey", Color( 47, 79, 79, 255 ) },
		{ "darkturquoise", Color( 0, 206, 209, 255 ) },
		{ "darkviolet", Color( 148, 0, 211, 255 ) },
		{ "deeppink", Color( 255, 20, 147, 255 ) },
		{ "deepskyblue", Color( 0, 191, 255, 255 ) },
		{ "dimgray", Color( 105, 105, 105, 255 ) },
		{ "dimgrey", Color( 105, 105, 105, 255 ) },
		{ "dodgerblue", Color( 30, 144, 255, 255 ) },
		{ "firebrick", Color( 178, 34, 34, 255 ) },
		{ "floralwhite", Color( 255, 250, 240, 255 ) },
		{ "forestgreen", Color( 34, 139, 34, 255 ) },
		{ "fuchsia", Color( 255, 0, 255, 255 ) },
		{ "gainsboro", Color( 220, 220, 220, 255 ) },
		{ "ghostwhite", Color( 248, 248, 255, 255 ) },
		{ "gold", Color( 255, 215, 0, 255 ) },
		{ "goldenrod", Color( 218, 165, 32, 255 ) },
		{ "gray", Color( 128, 128, 128, 255 ) },
		{ "green", Color( 0, 128, 0, 255 ) },
		{ "greenyellow", Color( 173, 255, 47, 255 ) },
		{ "grey", Color( 128, 128, 128, 255 ) },
		{ "honeydew", Color( 240, 255, 240, 255 ) },
		{ "hotpink", Color( 255, 105, 180, 255 ) },
		{ "indianred", Color( 205, 92, 92, 255 ) },
		{ "indigo", Color( 75, 0, 130, 255 ) },
		{ "ivory", Color( 255, 255, 240, 255 ) },
		{ "khaki", Color( 240, 230, 140, 255 ) },
		{ "lavender", Color( 230, 230, 250, 255 ) },
		{ "lavenderblush", Color( 255, 240, 245, 255 ) },
		{ "lawngreen", Color( 124, 252, 0, 255 ) },
		{ "lemonchiffon", Color( 255, 250, 205, 255 ) },
		{ "lightblue", Color( 173, 216, 230, 255 ) },
		{ "lightcoral", Color( 240, 128, 128, 255 ) },
		{ "lightcyan", Color( 224, 255, 255, 255 ) },
		{ "lightgoldenrodyellow", Color( 250, 250, 210, 255 ) },
		{ "lightgray", Color( 211, 211, 211, 255 ) },
		{ "lightgreen", Color( 144, 238, 144, 255 ) },
		{ "lightgrey", Color( 211, 211, 211, 255 ) },
		{ "lightpink", Color( 255, 182, 193, 255 ) },
		{ "lightsalmon", Color( 255, 160, 122, 255 ) },
		{ "lightseagreen", Color( 32, 178, 170, 255 ) },
		{ "lightskyblue", Color( 135, 206, 250, 255 ) },
		{ "lightslategray", Color( 119, 136, 153, 255 ) },
		{ "lightslategrey", Color( 119, 136, 153, 255 ) },
		{ "lightsteelblue", Color( 176, 196, 222, 255 ) },
		{ "lightyellow", Color( 255, 255, 224, 255 ) },
		{ "lime", Color( 0, 255, 0 , 255 ) },
		{ "limegreen", Color( 50, 205, 50, 255 ) },
		{ "linen", Color( 250, 240, 230, 255 ) },
		{ "magenta", Color( 255, 0, 255, 255 ) },
		{ "maroon", Color( 128, 0, 0, 255 ) },
		{ "mediumaquamarine", Color( 102, 205, 170, 255 ) },
		{ "mediumblue", Color( 0, 0, 205, 255 ) },
		{ "mediumorchid", Color( 186, 85, 211, 255 ) },
		{ "mediumpurple", Color( 147, 112, 219, 255 ) },
		{ "mediumseagreen", Color( 60, 179, 113, 255 ) },
		{ "mediumslateblue", Color( 123, 104, 238, 255 ) },
		{ "mediumspringgreen", Color( 0, 250, 154, 255 ) },
		{ "mediumturquoise", Color( 72, 209, 204, 255 ) },
		{ "mediumvioletred", Color( 199, 21, 133, 255 ) },
		{ "midnightblue", Color( 25, 25, 112, 255 ) },
		{ "mintcream", Color( 245, 255, 250, 255 ) },
		{ "mistyrose", Color( 255, 228, 225, 255 ) },
		{ "moccasin", Color( 255, 228, 181, 255 ) },
		{ "navajowhite", Color( 255, 222, 173, 255 ) },
		{ "navy", Color( 0, 0, 128, 255 ) },
		{ "oldlace", Color( 253, 245, 230, 255 ) },
		{ "olive", Color( 128, 128, 0, 255 ) },
		{ "olivedrab", Color( 107, 142, 35, 255 ) },
		{ "orange", Color( 255, 165, 0, 255 ) },
		{ "orangered", Color( 255, 69, 0, 255 ) },
		{ "orchid", Color( 218, 112, 214, 255 ) },
		{ "palegoldenrod", Color( 238, 232, 170, 255 ) },
		{ "palegreen", Color( 152, 251, 152, 255 ) },
		{ "paleturquoise", Color( 175, 238, 238, 255 ) },
		{ "palevioletred", Color( 219, 112, 147, 255 ) },
		{ "papayawhip", Color( 255, 239, 213, 255 ) },
		{ "peachpuff", Color( 255, 218, 185, 255 ) },
		{ "peru", Color( 205, 133, 63, 255 ) },
		{ "pink", Color( 255, 192, 203, 255 ) },
		{ "plum", Color( 221, 160, 221, 255 ) },
		{ "powderblue", Color( 176, 224, 230, 255 ) },
		{ "purple", Color( 128, 0, 128, 255 ) },
		{ "rebeccapurple", Color( 102, 51, 153, 255 ) },
		{ "red", Color( 255, 0, 0, 255 ) },
		{ "rosybrown", Color( 188, 143, 143, 255 ) },
		{ "royalblue", Color( 65, 105, 225, 255 ) },
		{ "saddlebrown", Color( 139, 69, 19, 255 ) },
		{ "salmon", Color( 250, 128, 114, 255 ) },
		{ "sandybrown", Color( 244, 164, 96, 255 ) },
		{ "seagreen", Color( 46, 139, 87, 255 ) },
		{ "seashell", Color( 255, 245, 238, 255 ) },
		{ "sienna", Color( 160, 82, 45, 255 ) },
		{ "silver", Color( 192, 192, 192, 255 ) },
		{ "skyblue", Color( 135, 206, 235, 255 ) },
		{ "slateblue", Color( 106, 90, 205, 255 ) },
		{ "slategray", Color( 112, 128, 144, 255 ) },
		{ "slategrey", Color( 112, 128, 144, 255 ) },
		{ "snow", Color( 255, 250, 250, 255 ) },
		{ "springgreen", Color( 0, 255, 127, 255 ) },
		{ "steelblue", Color( 70, 130, 180, 255 ) },
		{ "tan", Color( 210, 180, 140, 255 ) },
		{ "teal", Color( 0, 128, 128, 255 ) },
		{ "thistle", Color( 216, 191, 216, 255 ) },
		{ "tomato", Color( 255, 99, 71, 255 ) },
		{ "transparent", Color( 0, 0, 0, 0 ) }, // Special!
		{ "turquoise", Color( 64, 224, 208, 255 ) },
		{ "violet", Color( 238, 130, 238, 255 ) },
		{ "wheat", Color( 245, 222, 179, 255 ) },
		{ "white", Color( 255, 255, 255, 255 ) },
		{ "whitesmoke", Color( 245, 245, 245, 255 ) },
		{ "yellow", Color( 255, 255, 0, 255 ) },
		{ "yellowgreen", Color( 154, 205, 50, 255 ) },
	};

	const char *pchAfterIdentifier = NULL;
	char szColor[ 64 ];
	if ( !BParseIdent( szColor, sizeof( szColor ), pchString, &pchAfterIdentifier ) )
		return false;

	// Do the search as case insensitive
	V_strlower_fast( szColor );

	// Binary search to find a match
	int iMin = 0;
	int iMax = V_ARRAYSIZE( s_namedColors ) - 1;
	while ( iMax >= iMin )
	{
		int iMid = ( iMin + iMax ) / 2;

		int nCompareResult = V_strcmp( s_namedColors[ iMid ].pszName, szColor );
		if ( nCompareResult == 0 )
		{
			*pColor = s_namedColors[ iMid ].color;
			if ( pchAfterParse )
				*pchAfterParse = pchAfterIdentifier;
			return true;
		}
		else if ( nCompareResult < 0 )
		{
			iMin = iMid + 1;
		}
		else if ( nCompareResult > 0 )
		{
			iMax = iMid - 1;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Parses a url
// Supports: url(#...)
//-----------------------------------------------------------------------------
bool SVGHelpers::BParseURL( UtlSymId_t &elementID, CUtlSymbolTable &elementIDs, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if( V_strnicmp( "url", pchString, 3 ) )
		return false;

	pchString += 3;

	if( !BSkipLeftParen( pchString, &pchString ) )
		return false;

	bool bHasQuote = BSkipQuote( pchString, &pchString );
	pchString = SkipSpaces( pchString );

	if( *pchString != '#' )
	{
		return false; // For SVG we only support references by ID within the document
	}
	++pchString;

	int cch = V_strcspn( pchString, bHasQuote ? "\"'" : " )" );
	if( bHasQuote )
	{
		if( pchString[cch] != '"' && pchString[cch] != '\'' )
			return false; // missing closing bracket
	}

	char pchBuffer[1024];
	V_strncpy( pchBuffer, pchString, cch + 1 ); // + 1 for \0
	elementID = elementIDs.AddString( pchBuffer );

	if( bHasQuote )
		cch++; // skip the end quote char

	if( !BSkipRightParen( pchString + cch, &pchString ) )
		return false;

	if( pchAfterParse )
		*pchAfterParse = pchString;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to parse some known amount of a string to a float
//-----------------------------------------------------------------------------
static bool BParseNumberInternal( double *pNumber, const char *pchString, uint32 cchNumber, uint32 cchToAdvance, const char **pchAfterParse )
{
	// pick a max length
	char rgchBuffer[32];
	if ( cchNumber > V_ARRAYSIZE( rgchBuffer ) - 1 )
		return false;

	V_strncpy( rgchBuffer, pchString, cchNumber + 1 ); // + 1 for \0
	if ( rgchBuffer[0] == '\0' )
		return false;

	if ( !IsFloat( rgchBuffer ) )
		return false;

	*pNumber = V_atof( rgchBuffer );
	if ( pchAfterParse )
		*pchAfterParse = pchString + cchToAdvance;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to parse some known amount of a string to a float
//-----------------------------------------------------------------------------
static bool BParseNumberInternal( float *pNumber, const char *pchString, uint32 cchNumber, uint32 cchToAdvance, const char **pchAfterParse )
{
	// pick a max length
	char rgchBuffer[32];
	if( cchNumber > V_ARRAYSIZE( rgchBuffer ) - 1 )
		return false;

	V_strncpy( rgchBuffer, pchString, cchNumber + 1 ); // + 1 for \0
	if( rgchBuffer[0] == '\0' )
		return false;

	if( !IsFloat( rgchBuffer ) )
		return false;

	*pNumber = V_atof( rgchBuffer );
	if( pchAfterParse )
		*pchAfterParse = pchString + cchToAdvance;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS number into a float
//-----------------------------------------------------------------------------
bool SVGHelpers::BParseNumber( float *pNumber, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	int cch = V_strcspn( pchString, " ,);" );

	return BParseNumberInternal( pNumber, pchString, cch, cch, pchAfterParse );	
}

//-----------------------------------------------------------------------------
// Purpose: Parses a CSS identifier
//-----------------------------------------------------------------------------
bool SVGHelpers::BParseIdent( char *rgchIdent, int cubIdent, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );

	int cch = V_strcspn( pchString, " ,)(;" );
	if ( cubIdent < cch + 1 )
		return false;

	V_strncpy( rgchIdent, pchString, cch + 1 ); // +1 for '\0'
	if ( pchAfterParse )
		*pchAfterParse = pchString + cch;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Parse a CSS percent value
//-----------------------------------------------------------------------------
bool SVGHelpers::BParsePercent( float *pPercent, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );

	int cch = V_strcspn( pchString, " ,);" );
	if ( cch == 0 || pchString[cch - 1] != '%' )
		return false;	

	return BParseNumberInternal( pPercent, pchString, cch - 1, cch, pchAfterParse );
}

//-----------------------------------------------------------------------------
// Purpose: Skips a ( and any white space that precedes it. Will return false if no paren was found
//-----------------------------------------------------------------------------
bool SVGHelpers::BSkipLeftParen( const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( pchString[0] != '(' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Skips a " or ' and any white space that precedes it. Will return false if no quote was found
//-----------------------------------------------------------------------------
bool SVGHelpers::BSkipQuote( const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( pchString[0] != '"' && pchString[0] != '\'' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Skips a / any white space that precedes it. Will return false if no slash was found
//-----------------------------------------------------------------------------
bool SVGHelpers::BSkipSlash( const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( pchString[0] != '/' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Skips a ) and any white space that precedes it. Will return false if no paren was found
//-----------------------------------------------------------------------------
bool SVGHelpers::BSkipRightParen( const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( pchString[0] != ')' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Skips a comma and any white space that precedes it. Will return false if no comma was found
//-----------------------------------------------------------------------------
bool SVGHelpers::BSkipComma( const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( pchString[0] != ',' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	return true;
}


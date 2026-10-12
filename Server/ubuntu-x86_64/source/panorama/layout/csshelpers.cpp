//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "color.h"
#include "panorama/layout/csshelpers.h"
#ifdef SOURCE2_PANORAMA
#include "enumutils_panorama.h"
#else
#include "enumutils.h"
#endif
#include "panorama/layout/backgroundimage.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;


//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and EFontStyle
//-----------------------------------------------------------------------------
namespace panorama
{
	extern void GetAnimationCurveControlPoints( EAnimationTimingFunction eTransitionEffect, Vector2D vecPoints[4] );

ENUMSTRINGS_START( EFontWeight )
	{ k_EFontWeightUnset, "unset" },
	{ k_EFontWeightNormal, "normal" },
	{ k_EFontWeightMedium, "medium" },
	{ k_EFontWeightBold, "bold" },
	{ k_EFontWeightBlack, "black" },
	{ k_EFontWeightThin, "thin" },
	{ k_EFontWeightLight, "light" },
	{ k_EFontWeightSemiBold, "semi-bold" },
ENUMSTRINGS_REVERSE( EFontWeight, k_EFontWeightUnset )


//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and EFontStyle
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( EFontStyle )
	{ k_EFontStyleUnset, "unset" },
	{ k_EFontStyleNormal, "normal" },
	{ k_EFontStyleItalic, "bold" },
ENUMSTRINGS_REVERSE( EFontStyle, k_EFontStyleUnset )


//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and ETextAlign
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( ETextAlign )
	{ k_ETextAlignLeft, "left" },
	{ k_ETextAlignCenter, "center" },
	{ k_ETextAlignRight, "right" },
ENUMSTRINGS_REVERSE( ETextAlign, k_ETextAlignUnset )


//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and ETextDecoration
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( ETextDecoration )
	{ k_ETextDecorationNone, "none" },
	{ k_ETextDecorationUnderline, "underline" },
	{ k_ETextDecorationLineThrough, "line-through" },
ENUMSTRINGS_REVERSE( ETextDecoration, k_ETextDecorationUnset )


//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and ETextTransform
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( ETextTransform )
	{ k_ETextTransformNone, "none" },
	{ k_ETextTransformUppercase, "uppercase" },
	{ k_ETextTransformLowercase, "lowercase" },
ENUMSTRINGS_REVERSE( ETextTransform, k_ETextTransformUnset )


//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and EAnimationTimingFunction
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( EAnimationTimingFunction )
	{ k_EAnimationNone, "none" },
	{ k_EAnimationEase, "ease" },
	{ k_EAnimationEaseIn, "ease-in" },
	{ k_EAnimationEaseOut, "ease-out" },
	{ k_EAnimationEaseInOut, "ease-in-out" },
	{ k_EAnimationLinear, "linear" },
	{ k_EAnimationUnset, "unset" }
ENUMSTRINGS_REVERSE( EAnimationTimingFunction, k_EAnimationNone )


//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and EBackgroundRepeat
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( EBackgroundRepeat )
	{ k_EBackgroundRepeatUnset, "unset" },
	{ k_EBackgroundRepeatRepeat, "repeat" },
	{ k_EBackgroundRepeatSpace, "space" },
	{ k_EBackgroundRepeatRound, "round" },
	{ k_EBackgroundRepeatNo, "no-repeat" },
ENUMSTRINGS_REVERSE( EBackgroundRepeat, k_EBackgroundRepeatUnset )

} // namespace panorama


//----------------------------------------------------------------------------
// Purpose: skips white space and comments
// Returns: true/false if comment was found
//----------------------------------------------------------------------------
bool CSSHelpers::EatCSSComment( CUtlBuffer &buffer )
{
	return CSSHelpers::BReadCSSComment( buffer, NULL, 0 );
}


//----------------------------------------------------------------------------
// Purpose: skips white space and comments
//----------------------------------------------------------------------------
void CSSHelpers::EatCSSIgnorables( CUtlBuffer &buffer )
{
	while( buffer.IsValid() )
	{
		buffer.EatWhiteSpace();
		if( EatCSSComment( buffer ) )
			continue;

		break;
	}
}

//----------------------------------------------------------------------------
// Purpose: Retrieves the next character in the buffer. Does not advance buffer read position
//----------------------------------------------------------------------------
bool CSSHelpers::BPeekCSSToken( CUtlBuffer &buffer, char *pchNextChar )
{
	*pchNextChar = 0;

	// eating white spaces and remarks loop
	EatCSSIgnorables( buffer );
	if( !buffer.IsValid() )
	{
		return true;			// end of file.. nothing to parse
	}

	const char *pPeek = (const char*)buffer.PeekGet( sizeof( char ), 0 );

	// check if we hit the end of the file. Should hit a control character first, so this would be an error.
	if( !pPeek || *pPeek == '\0' )
		return false;

	*pchNextChar = *pPeek;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if character is in list of characters. Doesn't use V_strrchr as we want to handle '\0'
//-----------------------------------------------------------------------------
bool BCharInArray( char chFind, const char *pchCharacters, uint cchCharacters )
{
	for ( uint i = 0; i < cchCharacters; i++ )
	{
		if ( pchCharacters[i] == chFind )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Reads the next CSS token and makes sure it is one character and matches chExpected
//-----------------------------------------------------------------------------
bool CSSHelpers::BReadMatchingCSSToken( CUtlBuffer &buffer, char chExpected )
{
	// need space for a single character and terminator
	char rgchRead[2];
	if ( !BReadCSSToken( buffer, rgchRead, V_ARRAYSIZE( rgchRead ) ) )
		return false;

	return (rgchRead[0] == chExpected);
}


//----------------------------------------------------------------------------
// Purpose: Reads the next CSS token
// 
// Important: For simplicity, if pchStopAt contains ' ', we will stop at any anything that matches isspace()
//----------------------------------------------------------------------------
bool CSSHelpers::BReadCSSToken( CUtlBuffer &buffer, char *pchToken, uint cubToken )
{
	return CSSHelpers::BReadCSSToken( buffer, pchToken, cubToken, k_rgchCSSDefaultTerm, V_ARRAYSIZE( k_rgchCSSDefaultTerm ) );
}


//----------------------------------------------------------------------------
// Purpose: Reads the next CSS token
// 
// Important: For simplicity, if pchStopAt contains ' ', we will stop at any anything that matches isspace()
//----------------------------------------------------------------------------
bool CSSHelpers::BReadCSSToken( CUtlBuffer &buffer, char *pchToken, uint cubToken, const char *pchStopAt, uint cchStopAt )
{
	pchToken[0] = '\0';
	bool bIncludeSpaceInOutput = !BCharInArray( ' ', pchStopAt, cchStopAt );

	// eating white spaces and remarks loop
	EatCSSIgnorables( buffer );
	if( !buffer.IsValid() )
		return true;			// end of file.. nothing to parse

	// read in the token until we hit a whitespace or a control character
	uint cWritten = 0;
	bool bInSingleQuote = false;
	bool bInDoubleQuote = false;
	bool bPreviousCharEscape = false;
	while( buffer.IsValid() )
	{
		const char *pPeek = (const char*)buffer.PeekGet( sizeof( char ), 0 );

		// check if we hit the end of the file. Should hit a control character first, so this would be an error.
		if( !pPeek )
			return false;

		if( *pPeek == '\0' )
		{
			// Only ok if we are at an ok termination point and null char is in the stop at list
			if ( !bPreviousCharEscape && !bInDoubleQuote && !bInSingleQuote && BCharInArray( '\0', pchStopAt, cchStopAt ) )
				break;

			return false;
		}

		// check for quote
		char c = *pPeek;
		if( c == '\\' )
		{
			bPreviousCharEscape = !bPreviousCharEscape;

			// if we didn't receive "\\", skip
			if( bPreviousCharEscape )
			{
				buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
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
			// found end?
			// bugbug cboyd - find list of valid css description/value characters

			bool bIsSpace = (V_isspace( c ) != 0);
			if ( (bIsSpace && !bIncludeSpaceInOutput) || BCharInArray( c, pchStopAt, cchStopAt ) )
			{
				// if this is the first character.. add to buffer
				if( cWritten == 0 )
				{
					pchToken[cWritten] = c;
					cWritten++;
					buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
				}

				// if we are including spaces, trim the end of the string to make parsing easier
				if ( bIncludeSpaceInOutput )
				{
					while( cWritten > 0 && V_isspace( pchToken[cWritten - 1] ) )
						cWritten--;
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
		buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
	}

	// terminate
	pchToken[cWritten] = '\0';

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Reads a C/C++ style comment
// Returns: true/false if comment was found
// Parameters: pchBuffer == NULL || cubBuffer == 0 will skip returning the buffer
//-----------------------------------------------------------------------------
bool CSSHelpers::BReadCSSComment( CUtlBuffer &buffer, char *pchBuffer, uint cubBuffer )
{
	if( !buffer.IsText() || !buffer.IsValid() )
		return false;

	// find C style start
	const char *pPeek = (const char *)buffer.PeekGet( 2 * sizeof( char ), 0 );
	if( !pPeek || pPeek[0] != '/' || (pPeek[1] != '*' && pPeek[1] != '/') )
		return false;

	bool bCStyle = pPeek[1] == '*';
	int nStart = buffer.TellGet();

	// skip start
	buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, 2 );

	// keep skipping until we find a close. Could be anywhere
	while( buffer.IsValid() )
	{
		pPeek = (const char *)buffer.PeekGet( sizeof( char ), 0 );
		if( !pPeek )
			return false;

		// c++ comments end at return line
		if( !bCStyle && pPeek[0] == '\n' )
			break;

		if( !bCStyle || pPeek[0] != '*' )
		{
			buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
			continue;
		}

		// looking for a c style comment end
		buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
		pPeek = (const char *)buffer.PeekGet( sizeof( char ), 0 );
		if( !pPeek )
			return false;

		// Only consumed the peeked character if it's a '/' as
		// otherwise we can skip characters to consider as a
		// terminator.  For example '**/' would break because
		// we would consume the second '*' as part of looking
		// at the first '*' and then we'd miss the end of the comment.
		if( pPeek[0] == '/' )
		{
			buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
			break;
		}
	}

	// if caller doesn't want text, we are done
	if( !pchBuffer || cubBuffer == 0 )
		return true;

	// if we made it here, buffer is pointing to the first character after our comment
	int cubComment = buffer.TellGet() - nStart;
	buffer.SeekGet( CUtlBuffer::SEEK_HEAD, nStart );
	int nRead = MIN( cubComment, (int)(cubBuffer - 1) );
	buffer.Get( pchBuffer, cubComment );
	pchBuffer[nRead] = '\0';

	// trim, could have had \r\n
	V_StrTrim( pchBuffer );

	// skip remaining characters if buffer wasn't big enough
	if( cubComment > (int)cubBuffer )
		buffer.SeekGet( CUtlBuffer::SEEK_CURRENT, cubComment - cubBuffer );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if a string is only made up of numeric characters and a .
//-----------------------------------------------------------------------------
bool IsFloat( const char *pchString )
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
const char *CSSHelpers::SkipSpaces( const char *pchString )
{
	while ( V_isspace( *pchString ) )
		pchString++;

	return pchString;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a collection of fill brushes, comma delimited.
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseFillBrushCollection( CFillBrushCollection *pCollection, const char *pchString, const char **pchAfterParse, float flScalingFactor )
{
	bool bOneFound = false;
	while( 1 ) 
	{
		CFillBrush brush;
		if ( CSSHelpers::BParseFillBrush( &brush, pchString, &pchString, flScalingFactor ) )
		{
			bOneFound = true;
			pCollection->AddFillBrush( brush );

			if ( !BSkipComma( pchString, &pchString ) )
				break;
		}
		else
		{
			return false;
		}
	} 

	if ( bOneFound )
	{
		if ( pchAfterParse )
			*pchAfterParse = pchString;

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a function name like 'timing-function(' or 'base-position ('
// This skips the name, which it returns in the symbol out param, and also skips
// the opening (.  It handles  whitespace before/after the ( as well.
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseFunctionName( CPanoramaSymbol &symFunctionNameOut, const char *pchString, const char **pchAfterParse /*= NULL */ )
{
	const char *pchOpenParen = strchr( pchString, '(' );
	if ( !pchOpenParen || pchOpenParen == pchString )
		return false;
	
	size_t len = pchOpenParen - pchString;
	char rgchFuncName[512];
	V_memcpy( rgchFuncName, pchString, MIN( 512, len ) );
	rgchFuncName[ MIN( 512 - 1, len ) ] = 0;

	V_StrTrim( rgchFuncName );

	if ( V_strlen( rgchFuncName ) == 0 )
		return false;

	symFunctionNameOut = rgchFuncName;

	if ( pchAfterParse )
		*pchAfterParse = pchOpenParen + 1;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a fill brush
// Supports: #rrggbb, #rrggbbaa, 
//
//           gradient( linear, 0% 0%, 100% 100%, color-stop(0, #ffffffff), ..., color-stop(1.0 #00000000 ) ),
//
//           gradient( radial, 50% 50%, 0px 0px, 50px 30px, color-stop(0, #ffffffff), ..., color-stop(1.0 #00000000 ) ),
//
//			 particle-system( base-position( 0px, 50%, 0px ), base-position-variance( 0px, 10%, 0px ), 
//                            particle-size( 10px ), particle-size-variance( 5px ), 
//                            particles-per-second( 4 ), particles-per-second-variance( 2 ),
//                            particle-lifespan-seconds( 4s ), particle-lifespan-seconds-variance( 3s ),
//							  particle-initial-velocity( 0px, 12px, -2px ), particle-initial-velocity-variance( 0px, 5px, 1px ),
//							  gravity-acceleration( 0px, -1.9px, 0px ),
//							  particle-start-color( #0044aacc ), particle-start-color-variance( #00111122 ),
//							  particle-end-color( #0044aa88 ), particle-end-color-variance( #00111122 )
//                          )
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseFillBrush( CFillBrush *pBrush, const char *pchString, const char **pchAfterParse, float flScalingFactor )
{
	static CPanoramaSymbol k_SymParticleSystem = "particle-system";
	static CPanoramaSymbol k_SymBasePos = "base-position";
	static CPanoramaSymbol k_SymBasePosVar = "base-position-variance";
	static CPanoramaSymbol k_SymParticleSize = "particle-size";
	static CPanoramaSymbol k_SymParticleSizeVar = "particle-size-variance";
	static CPanoramaSymbol k_SymParticlesPerSecond = "particles-per-second";
	static CPanoramaSymbol k_SymParticlesPerSecondVar = "particles-per-second-variance";
	static CPanoramaSymbol k_SymParticleLifeSpan = "particle-lifespan-seconds";
	static CPanoramaSymbol k_SymParticleLifeSpanVar = "particle-lifespan-seconds-variance";
	static CPanoramaSymbol k_SymParticleInitialVel = "particle-initial-velocity";
	static CPanoramaSymbol k_SymParticleInitialVelVar = "particle-initial-velocity-variance";
	static CPanoramaSymbol k_SymGravityAccel = "gravity-acceleration";
	static CPanoramaSymbol k_SymGravityAccelParticleVar = "gravity-acceleration-particle-variance";
	static CPanoramaSymbol k_SymStartColor = "particle-start-color";
	static CPanoramaSymbol k_SymStartColorVar = "particle-start-color-variance";
	static CPanoramaSymbol k_SymEndColor = "particle-end-color";
	static CPanoramaSymbol k_SymEndColorVar = "particle-end-color-variance";
	static CPanoramaSymbol k_SymSharpness = "particle-sharpness";
	static CPanoramaSymbol k_SymSharpnessVar = "particle-sharpness-variance";
	static CPanoramaSymbol k_SymFlicker = "particle-flicker";
	static CPanoramaSymbol k_SymFlickerVar = "particle-flicker-variance";
	static CPanoramaSymbol k_SymParticleVelocityMin = "particle-velocity-min";
	static CPanoramaSymbol k_SymParticleVelocityMax = "particle-velocity-max";

	static CPanoramaSymbol k_SymGradient ="gradient";

	// First check if this is straight up color
	Color color;
	const char *pchEnd;
	if ( BParseColor( &color, pchString, &pchEnd ) )
	{
		if ( pchAfterParse )
			*pchAfterParse = pchEnd;

		pBrush->SetToFillColor( color );
		return true;
	}
	else if ( V_stricmp( "transparent", pchString ) == 0 )
	{
		pBrush->SetToFillColor( Color( 0, 0, 0, 0 ) );
		return true;
	}
	else
	{
		CPanoramaSymbol symMainFunc;
		if ( !BParseFunctionName( symMainFunc, pchString, &pchString ) )
			return false;

		// Look for gradient and type, or particle-system
		pchString = SkipSpaces( pchString );

		// Initialize to empty fill color, since we can't initialize to the actual particle system until we are sure
		// we've validaed all the data
		pBrush->SetToFillColor( Color( 0, 0, 0, 0 ) );

		if ( symMainFunc == k_SymParticleSystem )
		{
			bool bFoundSomeProperties = false;

			Vector vecBasePositon( 0.0f, 0.0f, 0.0f );
			Vector vecBasePositionVariance( 0.0f, 0.0f, 0.0f );
			float flSize = 0.0f;
			float flSizeVariance = 0.0f;
			float flParticlesPerSecond = 0.0f;
			float flParticlesPerSecondVariance = 0.0f;
			double flLifeSpanSeconds = 0.0f;
			double flLifeSpanSecondsVariance = 0.0f;
			Vector vecInitialVelocity( 0.0f, 0.0f, 0.0f );
			Vector vecInitialVelocityVariance( 0.0f, 0.0f, 0.0f );
			Vector vecVelocityMin( -999999999.0f, -999999999.0f, -999999999.0f );
			Vector vecVelocityMax( 999999999.0f, 999999999.0f, 999999999.0f );
			Vector vecGravityAcceleration( 0.0f, 0.0f, 0.0f );
			Vector vecGravityAccelerationParticleVariance( 0.0f, 0.0f, 0.0f );
			Color colorStart;
			Color colorStartVariance;
			Color colorEnd;
			Color colorEndVariance;
			float flSharpness = 1.0f;
			float flSharpnessVariance = 0.0f;
			float flFlicker = 0.0f;
			float flFlickerVariance = 0.0f;
			
			while ( 1 )
			{
				// Parse any of the individual properties in any order, should find at least one to consider this a valid system

				pchString = SkipSpaces( pchString );

				CPanoramaSymbol symParamName;
				if ( !BParseFunctionName( symParamName, pchString, &pchString ) )
				{
					break;
				}

				if ( symParamName == k_SymBasePos )
				{
					if ( !BParseLength( &(vecBasePositon.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecBasePositon.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymBasePosVar )
				{
					if ( !BParseLength( &(vecBasePositionVariance.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecBasePositionVariance.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleSize )
				{
					if ( !BParseLength( &flSize, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleSizeVar )
				{
					if ( !BParseLength( &flSizeVariance, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticlesPerSecond )
				{
					if ( !BParseNumber( &flParticlesPerSecond, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticlesPerSecondVar )
				{
					if ( !BParseNumber( &flParticlesPerSecondVariance, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleLifeSpan )
				{
					if ( !BParseTime( &flLifeSpanSeconds, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleLifeSpanVar )
				{
					if ( !BParseTime( &flLifeSpanSecondsVariance, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleInitialVel )
				{
					if ( !BParseLength( &(vecInitialVelocity.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecInitialVelocity.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleInitialVelVar )
				{
					if ( !BParseLength( &(vecInitialVelocityVariance.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecInitialVelocityVariance.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleVelocityMin )
				{
					if ( !BParseLength( &(vecVelocityMin.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecVelocityMin.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymParticleVelocityMax )
				{
					if ( !BParseLength( &(vecVelocityMax.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecVelocityMax.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymGravityAccel )
				{
					if ( !BParseLength( &(vecGravityAcceleration.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecGravityAcceleration.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymGravityAccelParticleVar )
				{
					if ( !BParseLength( &(vecGravityAccelerationParticleVariance.x), pchString, &pchString ) || !BSkipComma( pchString, &pchString) )
						return false;

					if ( !BParseLength( &(vecGravityAccelerationParticleVariance.y), pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymStartColor )
				{
					if ( !BParseColor( &colorStart, pchString, &pchString ) )
						return false;					
				}
				else if ( symParamName == k_SymStartColorVar )
				{
					if ( !BParseColor( &colorStartVariance, pchString, &pchString ) )
						return false;			
				}
				else if ( symParamName == k_SymEndColor )
				{
					if ( !BParseColor( &colorEnd, pchString, &pchString ) )
						return false;		
				}
				else if ( symParamName == k_SymEndColorVar )
				{
					if ( !BParseColor( &colorEndVariance, pchString, &pchString ) )
						return false;	
				}
				else if ( symParamName == k_SymSharpness )
				{
					if ( !BParseNumber( &flSharpness, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymSharpnessVar )
				{
					if ( !BParseNumber( &flSharpnessVariance, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymFlicker )
				{
					if ( !BParseNumber( &flFlicker, pchString, &pchString ) )
						return false;
				}
				else if ( symParamName == k_SymFlickerVar )
				{
					if ( !BParseNumber( &flFlickerVariance, pchString, &pchString ) )
						return false;
				}
				else
				{
					return false;
				}

				bFoundSomeProperties = true;

				pchString = SkipSpaces( pchString );

				if ( !BSkipRightParen( pchString, &pchString ) )
					return false;

				pchString = SkipSpaces( pchString );

				if ( !BSkipComma( pchString, &pchString ) )
					break;
				
			}

			if ( !bFoundSomeProperties )
				return false;

			if ( !BSkipRightParen( pchString, &pchString ) ) 
				return false;

			pBrush->SetToParticleSystem( vecBasePositon, vecBasePositionVariance, flSize, flSizeVariance, flParticlesPerSecond, flParticlesPerSecondVariance, 
				flLifeSpanSeconds, flLifeSpanSecondsVariance, vecInitialVelocity, vecInitialVelocityVariance, vecVelocityMin, vecVelocityMax,
				vecGravityAcceleration, vecGravityAccelerationParticleVariance, colorStart, colorStartVariance, colorEnd, colorEndVariance, flSharpness, flSharpnessVariance,
				flFlicker, flFlickerVariance );

			if ( pchAfterParse ) 
				*pchAfterParse = pchString;

			return true;
		}
		else if ( symMainFunc == k_SymGradient )
		{
			pchString = SkipSpaces( pchString );

			// Find radial or linear
			if ( V_strnicmp( pchString, "linear", V_strlen( "linear" ) ) == 0 )
			{
				pchString += V_strlen( "linear" );

				if ( !BSkipComma( pchString, &pchString ) )
					return false;

				CUILength startX, startY;
				CUILength endX, endY;
				
				if ( !BParseIntoUILength( &startX, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BParseIntoUILength( &startY, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BSkipComma( pchString, &pchString ) )
					return false;

				if ( !BParseIntoUILength( &endX, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BParseIntoUILength( &endY, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BSkipComma( pchString, &pchString ) )
					return false;

				CGradientColorStop colorStop;
				CUtlVector< CGradientColorStop > vecColorStops;
				vecColorStops.EnsureCapacity( 2 );

				while ( BParseGradientColorStop( &colorStop, pchString, &pchString, flScalingFactor ) )
				{
					vecColorStops.AddToTail( colorStop );

					// Skip comma, if there is one, ok to fail here at the end of the color-stops list
					if ( !BSkipComma( pchString, &pchString ) )
						break;
				}

				if ( vecColorStops.Count() < 2 )
					return false;

				pBrush->SetToLinearGradient( startX, startY, endX, endY, vecColorStops );
			}
			else if ( V_strnicmp( pchString, "radial", V_strlen( "radial" ) ) == 0 )
			{
				pchString += V_strlen( "radial" );

				if ( !BSkipComma( pchString, &pchString ) )
					return false;

				CUILength centerX, centerY;
				CUILength offsetX, offsetY;
				CUILength radiusX, radiusY;

				if ( !BParseIntoUILength( &centerX, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BParseIntoUILength( &centerY, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BSkipComma( pchString, &pchString ) )
					return false;

				if ( !BParseIntoUILength( &offsetX, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BParseIntoUILength( &offsetY, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BSkipComma( pchString, &pchString ) )
					return false;

				if ( !BParseIntoUILength( &radiusX, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BParseIntoUILength( &radiusY, pchString, &pchString, flScalingFactor ) )
					return false;

				if ( !BSkipComma( pchString, &pchString ) )
					return false;

				CGradientColorStop colorStop;
				CUtlVector< CGradientColorStop > vecColorStops;
				vecColorStops.EnsureCapacity( 2 );

				while ( BParseGradientColorStop( &colorStop, pchString, &pchString, flScalingFactor ) )
				{
					vecColorStops.AddToTail( colorStop );

					// Skip comma, if there is one, ok to fail here at the end of the color-stops list
					if ( !BSkipComma( pchString, &pchString ) )
						break;
				}

				if ( vecColorStops.Count() < 2 )
					return false;

				pBrush->SetToRadialGradient( centerX, centerY, offsetX, offsetY, radiusX, radiusY, vecColorStops );
			}
			else
			{
				return false;
			}

			if ( !BSkipRightParen( pchString, &pchString ) ) 
				return false;

			if ( pchAfterParse ) 
				*pchAfterParse = pchString;

			return true;
		}

		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Parses background position
// Example: left 10px top 20px
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseBackgroundPosition( CBackgroundPosition *pPosition, const char *pchString, const char **pchAfterParse )
{
	CUILength lenZeroPct;
	lenZeroPct.SetPercent( 0 );	

	// need a count of values to know how to parse this thing
	const char *pchNext = CSSHelpers::SkipSpaces( pchString );
	if ( pchNext[0] == ',' || pchNext[0] == '\0' )
		return false;

	int cValues = 1;
	while ( pchNext[0] != ',' && pchNext[0] != '\0' )
	{
		if ( pchNext[0] == ' ' )
		{
			cValues++;
			pchNext = CSSHelpers::SkipSpaces( pchNext );
			if ( pchNext[0] == ',' || pchNext[0] == '\0' )
			{
				// had spaces remaining, remove count
				cValues--;
			}
		}
		else
		{
			pchNext++;
		}
	}

	// try to parse all 4 positions
	EHorizontalAlignment eHorizontal = k_EHorizontalAlignmentUnset;
	bool bHorizontalKeyword = BParseHorizontalAlignment( &eHorizontal, pchString, &pchString );

	CUILength lenFirst;
	bool bParsedFirst = BParseIntoUILength( &lenFirst, pchString, &pchString );	


	EVerticalAlignment eVertical = k_EVerticalAlignmentUnset;
	bool bVerticalKeyword = BParseVerticalAlignment( &eVertical, pchString, &pchString );

	CUILength lenSecond;
	bool bParsedSecond = BParseIntoUILength( &lenSecond, pchString, &pchString );

	// check we parsed as many as we expected
	int cParsed = 0;
	if ( bHorizontalKeyword )
		cParsed++;
	if ( bParsedFirst )
		cParsed++;
	if ( bVerticalKeyword )
		cParsed++;
	if ( bParsedSecond )
		cParsed++;

	if ( cParsed != cValues )
		return false;

	if ( cValues == 1 )
	{
		// with 1 term, second is assumed to be center (weird yes.. from spec)
		if ( bHorizontalKeyword )
			pPosition->Set( eHorizontal, lenZeroPct, k_EVerticalAlignmentCenter, lenZeroPct );
		if ( bParsedFirst )
			pPosition->Set( k_EHorizontalAlignmentLeft, lenFirst, k_EVerticalAlignmentCenter, lenZeroPct );
		if ( bVerticalKeyword )
			pPosition->Set( k_EHorizontalAlignmentCenter, lenZeroPct, eVertical, lenZeroPct );

		Assert( !bParsedSecond );
	}
	else if ( cValues == 2 )
	{
		// first horizontal, second vertical. Must be in that order, so "bottom left" is invalid
		if ( bHorizontalKeyword )
		{
			// left 10% or left top
			if ( bParsedFirst )
				pPosition->Set( eHorizontal, lenZeroPct, k_EVerticalAlignmentTop, lenFirst );
			else
				pPosition->Set( eHorizontal, lenZeroPct, eVertical, lenZeroPct );
		}
		else if ( bParsedFirst )
		{
			// 10% top or 10% 10%
			if ( bVerticalKeyword )
				pPosition->Set( k_EHorizontalAlignmentLeft, lenFirst, eVertical, lenZeroPct );
			else
				pPosition->Set( k_EHorizontalAlignmentLeft, lenFirst, k_EVerticalAlignmentTop, lenSecond );
		}
		else
		{
			return false;
		}	
	}
	else if ( cValues == 3 )
	{
		if ( !bHorizontalKeyword )
			pPosition->Set( k_EHorizontalAlignmentLeft, lenFirst, eVertical, lenSecond );
		else if ( !bParsedFirst )
			pPosition->Set( eHorizontal, lenZeroPct, eVertical, lenSecond );
		else if ( !bVerticalKeyword )
			pPosition->Set( eHorizontal, lenFirst, k_EVerticalAlignmentTop, lenSecond );
		else if ( !bParsedSecond )
			pPosition->Set( eHorizontal, lenFirst, eVertical, lenZeroPct );
	}
	else
	{
		Assert( cValues == 4 );
		pPosition->Set( eHorizontal, lenFirst, eVertical, lenSecond );
	}

	if ( pchAfterParse )
		*pchAfterParse = pchString;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses background repeat
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseBackgroundRepeat( CBackgroundRepeat *pBackgroundRepeat, const char *pchString, const char **pchAfterParse )
{
	const char rgchRepeatX[] = "repeat-x";
	const char rgchRepeatY[] = "repeat-y";

	// parsing as ident should work
	char rgchBuffer[128];
	if ( !CSSHelpers::BParseIdent( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), pchString, &pchString ) )
		return false;

	// first param can be a couple special values
	if ( V_strcmp( rgchBuffer, rgchRepeatX ) == 0 )
	{
		pBackgroundRepeat->Set( k_EBackgroundRepeatRepeat, k_EBackgroundRepeatNo );
		if ( pchAfterParse )
			*pchAfterParse = pchString;

		return true;
	}
	else if ( V_strcmp( rgchBuffer, rgchRepeatY ) == 0 )
	{
		pBackgroundRepeat->Set( k_EBackgroundRepeatNo, k_EBackgroundRepeatRepeat );
		if ( pchAfterParse )
			*pchAfterParse = pchString;

		return true;
	}

	EBackgroundRepeat eHorizontal = EBackgroundRepeatFromName( rgchBuffer );
	if ( eHorizontal == k_EBackgroundRepeatUnset )
		return false;

	// check if only one value was specified
	pchString = CSSHelpers::SkipSpaces( pchString );
	if ( pchString[0] == '\0' || pchString[0] == ',' )
	{		
		pBackgroundRepeat->Set( eHorizontal, eHorizontal );
		if ( pchAfterParse )
			*pchAfterParse = pchString;

		return true;
	}

	// grab vertical
	if ( !CSSHelpers::BParseIdent( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), pchString, &pchString ) )
		return false;

	EBackgroundRepeat eVertical = EBackgroundRepeatFromName( rgchBuffer );
	if ( eVertical == k_EBackgroundRepeatUnset )
		return false;

	pBackgroundRepeat->Set( eHorizontal, eVertical );
	if ( pchAfterParse )
		*pchAfterParse = pchString;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a gaussian blur defintion
// Supports: gaussian( 3.0, 3.0, 1.0 ), first param is passes, second is horizontal stddev, 
// third is vertical stddev, the third param is optional and may hurt perf if > 1.0.
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseGaussianBlur( BlurType_t &blurType, float &flPasses, float &flStdDevHorizontal, float &flStdDevVertical, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );


	if ( V_strnicmp( pchString, "fastanimgaussian", V_strlen( "fastanimgaussian" ) ) == 0 )
	{
		blurType = BT_FASTANIM;
		pchString += V_strlen( "fastanim" );
	}
	else if ( V_strnicmp( pchString, "fastgaussian", V_strlen( "fastgaussian" ) ) == 0 )
	{
		blurType = BT_FAST;
		pchString += V_strlen( "fast" );
	}
	else
	{
		blurType = BT_NORMAL;
	}

	if ( V_strnicmp( pchString, "gaussian", V_strlen( "gaussian" ) ) == 0 )
	{
		pchString += V_strlen( "gaussian" );

		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;

		if ( !BParseNumber( &flStdDevHorizontal, pchString, &pchString ) && !BParseLength( &flStdDevHorizontal, pchString, &pchString ) )
			return false;

		if ( BSkipComma( pchString, &pchString ) )
		{
			if ( !BParseNumber( &flStdDevVertical, pchString, &pchString ) && !BParseLength( &flStdDevVertical, pchString, &pchString ) )
				return false;

			if ( BSkipComma( pchString, &pchString ) )
			{
				if ( !BParseNumber( &flPasses, pchString, &pchString ) )
					return false;
			}
			else
			{
				flPasses = 1.0f;
			}
		}
		else
		{
			// IF we only have one value, then hor/ver match, and passes = 1.0
			flStdDevVertical = flStdDevHorizontal;
			flPasses = 1.0f;
		}
		
		if ( !BSkipRightParen( pchString, &pchString ) )
			return false;

		return true;
	}
	else if ( V_strnicmp( pchString, "none", V_strlen( "none" ) ) == 0 )
	{
		pchString += V_strlen( "none" );
		if ( pchAfterParse ) *pchAfterParse = pchString;

		flPasses = 0.0f;
		flStdDevHorizontal = 0.0f;
		flStdDevVertical = 0.0f;
		blurType = BT_NORMAL;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a gradient color stop
// Supports: from( #color ), to( #color ), color-stop( 0.5, #ffffffff )
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseGradientColorStop( CGradientColorStop *pColorStop, const char *pchString, const char **pchAfterParse, float flScalingFactor )
{
	pchString = SkipSpaces( pchString );
	if ( V_strnicmp( pchString, "from", V_strlen( "from" ) ) == 0 )
	{
		pchString += V_strlen( "from" );

		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;

		Color c;
		if ( BParseColor( &c, pchString, &pchString ) )
		{
			pColorStop->SetPosition( 0.0f );
			pColorStop->SetColor( c.r(), c.g(), c.b(), c.a() );
			
			BSkipRightParen( pchString, &pchString );

			if ( pchAfterParse ) *pchAfterParse = pchString;

			return true;
		}
	}
	else if ( V_strnicmp( pchString, "to", V_strlen( "to" ) ) == 0 )
	{
		pchString += V_strlen( "to" );

		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;

		Color c;
		if ( BParseColor( &c, pchString, &pchString ) )
		{
			pColorStop->SetPosition( 1.0f );
			pColorStop->SetColor( c.r(), c.g(), c.b(), c.a() );

			BSkipRightParen( pchString, &pchString );

			if ( pchAfterParse ) *pchAfterParse = pchString;

			
			return true;
		}
	}
	else if ( V_strnicmp( pchString, "color-stop", V_strlen( "color-stop" ) ) == 0 )
	{
		pchString += V_strlen( "color-stop" );

		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;

		Color c;
		float flPosition;
		if ( BParseNumber( &flPosition, pchString, &pchString ) && BSkipComma( pchString, &pchString ) && BParseColor( &c, pchString, &pchString ) )
		{
			pColorStop->SetPosition( flPosition );
			pColorStop->SetColor( c.r(), c.g(), c.b(), c.a() );

			BSkipRightParen( pchString, &pchString );

			if ( pchAfterParse ) *pchAfterParse = pchString;

			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS rect string
// Supports: rect( 0px, 200px, 200px, 0px )
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseRect( CUILength *pTop, CUILength *pRight, CUILength *pBottom, CUILength *pLeft, const char *pchString, const char **pchAfterParse /*= NULL */, float flScalingFactor /* = 1.0f */ )
{
	pchString = SkipSpaces( pchString );
	if( V_strnicmp( pchString, "rect", V_strlen( "rect" ) ) == 0 )
	{
		const char *pchRect = pchString + 4;

		if( !BSkipLeftParen( pchRect, &pchRect ) )
			return false;

		if( !BParseIntoUILength( pTop, pchRect, &pchRect ) )
		{
			if ( V_strnicmp( pchRect, "auto", V_strlen( "auto"  ) ) == 0 )
				pchRect += 4;
			else
				return false;
		}

		if( !BSkipComma( pchRect, &pchRect ) )
			return false;

		if( !BParseIntoUILength( pRight, pchRect, &pchRect ) )
		{
			if( V_strnicmp( pchRect, "auto", V_strlen( "auto" ) ) == 0 )
				pchRect += 4;
			else
				return false;
		}

		if( !BSkipComma( pchRect, &pchRect ) )
			return false;

		if( !BParseIntoUILength( pBottom, pchRect, &pchRect ) )
		{
			if( V_strnicmp( pchRect, "auto", V_strlen( "auto" ) ) == 0 )
				pchRect += 4;
			else
				return false;
		}

		if( !BSkipComma( pchRect, &pchRect ) )
			return false;

		if( !BParseIntoUILength( pLeft, pchRect, &pchRect ) )
		{
			if( V_strnicmp( pchRect, "auto", V_strlen( "auto" ) ) == 0 )
				pchRect += 4;
			else
				return false;
		}

		BSkipRightParen( pchRect, &pchRect );

		if ( pchAfterParse ) *pchAfterParse = pchRect;

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS color string
// Supports: #rrggbb, #rrggbbaa
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseColor( Color *pColor, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );

	if ( BSkipName( "none", pchString, pchAfterParse ) )
	{
		pColor->SetRawColor( 0x00000000 );
		return true;
	}

	// bugbug cboyd - add rgb() support
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
	else if ( V_strnicmp( pchString, "rgb", V_strlen( "rgb" ) ) == 0 )
	{
		const char *pchRGBA = pchString + V_strlen( "rgb" );

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
	else if ( BSkipName( "hsv-transform", pchString, &pchString ) )
	{
		Color originalColor;
		float flHueRotate;
		float flSaturationScale;
		float flValueScale;

		// Parse parameters
		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;
		if ( !BParseColor( &originalColor, pchString, &pchString ) )
			return false;
		if ( !BSkipComma( pchString, &pchString ) )
			return false;
		if ( !BParseNumber( &flHueRotate, pchString, &pchString ) )
			return false;
		if ( !BSkipComma( pchString, &pchString ) )
			return false;
		if ( !BParseNumber( &flSaturationScale, pchString, &pchString ) )
			return false;
		if ( !BSkipComma( pchString, &pchString ) )
			return false;
		if ( !BParseNumber( &flValueScale, pchString, &pchString ) )
			return false;
		if ( !BSkipRightParen( pchString, &pchString ) )
			return false;

		// Validate parameters
		if ( fabsf( flHueRotate ) > 360.0f )
			return false;
		if ( flSaturationScale < 0.0f )
			return false;
		if ( flValueScale < 0.0f )
			return false;

		// Convert to linear space
		Vector rgb, hsv;
		rgb.x = SrgbGammaToLinear( ( float )originalColor.r() / 255.0f );
		rgb.y = SrgbGammaToLinear( ( float )originalColor.g() / 255.0f );
		rgb.z = SrgbGammaToLinear( ( float )originalColor.b() / 255.0f );

		// apply transform
		RGBtoHSV( rgb, hsv );

		hsv.x = fmodf( hsv.x + flHueRotate, 360.0f );
		hsv.y *= flSaturationScale;
		hsv.z *= flValueScale;

		if ( hsv.x < 0.0f )		// roate hue back to 0-360 range
			hsv.x += 360.0f;
		if ( hsv.y > 1.0f )		// can't be more than 100% saturation
			hsv.y = 1.0f;
		if ( hsv.z > 1.0f )		// can't be more than 100% value
			hsv.z = 1.0f;

		HSVtoRGB( hsv, rgb );

		// Convert back to gamma space
		rgb.x = SrgbLinearToGamma( rgb.x ) * 255.0f;
		rgb.y = SrgbLinearToGamma( rgb.y ) * 255.0f;
		rgb.z = SrgbLinearToGamma( rgb.z ) * 255.0f;

		// Success
		pColor->SetColor( ( int )rgb.x, ( int )rgb.y, ( int )rgb.z, originalColor.a() );

		if ( pchAfterParse )
			*pchAfterParse = pchString;
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

bool CSSHelpers::BParseNamedColor( Color *pColor, const char *pchString, const char **pchAfterParse /* = NULL */ )
{
	// CSS named colors from http://dev.w3.org/csswg/css-color-4/#named-colors.
	// Keep alphabetical.
	static const SNamedColor s_namedColors[] = 
	{
		{ "aliceblue", Color( 240, 248, 255 ) },
		{ "antiquewhite", Color( 250, 235, 215 ) },
		{ "aqua", Color( 0, 255, 255 ) },
		{ "aquamarine", Color( 127, 255, 212 ) },
		{ "azure", Color( 240, 255, 255 ) },
		{ "beige", Color( 245, 245, 220 ) },
		{ "bisque", Color( 255, 228, 196 ) },
		{ "black", Color( 0, 0, 0 ) },
		{ "blanchedalmond", Color( 255, 235, 205 ) },
		{ "blue", Color( 0, 0, 255 ) },
		{ "blueviolet", Color( 138, 43, 226 ) },
		{ "brown", Color( 165, 42, 42 ) },
		{ "burlywood", Color( 222, 184, 135 ) },
		{ "cadetblue", Color( 95, 158, 160 ) },
		{ "chartreuse", Color( 127, 255, 0 ) },
		{ "chocolate", Color( 210, 105, 30 ) },
		{ "coral", Color( 255, 127, 80 ) },
		{ "cornflowerblue", Color( 100, 149, 237 ) },
		{ "cornsilk", Color( 255, 248, 220 ) },
		{ "crimson", Color( 220, 20, 60 ) },
		{ "cyan", Color( 0, 255, 255 ) },
		{ "darkblue", Color( 0, 0, 139 ) },
		{ "darkcyan", Color( 0, 139, 139 ) },
		{ "darkgoldenrod", Color( 184, 134, 11 ) },
		{ "darkgray", Color( 169, 169, 169 ) },
		{ "darkgreen", Color( 0, 100, 0 ) },
		{ "darkgrey", Color( 169, 169, 169 ) },
		{ "darkkhaki", Color( 189, 183, 107 ) },
		{ "darkmagenta", Color( 139, 0, 139 ) },
		{ "darkolivegreen", Color( 85, 107, 47 ) },
		{ "darkorange", Color( 255, 140, 0 ) },
		{ "darkorchid", Color( 153, 50, 204 ) },
		{ "darkred", Color( 139, 0, 0 ) },
		{ "darksalmon", Color( 233, 150, 122 ) },
		{ "darkseagreen", Color( 143, 188, 143 ) },
		{ "darkslateblue", Color( 72, 61, 139 ) },
		{ "darkslategray", Color( 47, 79, 79 ) },
		{ "darkslategrey", Color( 47, 79, 79 ) },
		{ "darkturquoise", Color( 0, 206, 209 ) },
		{ "darkviolet", Color( 148, 0, 211 ) },
		{ "deeppink", Color( 255, 20, 147 ) },
		{ "deepskyblue", Color( 0, 191, 255 ) },
		{ "dimgray", Color( 105, 105, 105 ) },
		{ "dimgrey", Color( 105, 105, 105 ) },
		{ "dodgerblue", Color( 30, 144, 255 ) },
		{ "firebrick", Color( 178, 34, 34 ) },
		{ "floralwhite", Color( 255, 250, 240 ) },
		{ "forestgreen", Color( 34, 139, 34 ) },
		{ "fuchsia", Color( 255, 0, 255 ) },
		{ "gainsboro", Color( 220, 220, 220 ) },
		{ "ghostwhite", Color( 248, 248, 255 ) },
		{ "gold", Color( 255, 215, 0 ) },
		{ "goldenrod", Color( 218, 165, 32 ) },
		{ "gray", Color( 128, 128, 128 ) },
		{ "green", Color( 0, 128, 0 ) },
		{ "greenyellow", Color( 173, 255, 47 ) },
		{ "grey", Color( 128, 128, 128 ) },
		{ "honeydew", Color( 240, 255, 240 ) },
		{ "hotpink", Color( 255, 105, 180 ) },
		{ "indianred", Color( 205, 92, 92 ) },
		{ "indigo", Color( 75, 0, 130 ) },
		{ "ivory", Color( 255, 255, 240 ) },
		{ "khaki", Color( 240, 230, 140 ) },
		{ "lavender", Color( 230, 230, 250 ) },
		{ "lavenderblush", Color( 255, 240, 245 ) },
		{ "lawngreen", Color( 124, 252, 0 ) },
		{ "lemonchiffon", Color( 255, 250, 205 ) },
		{ "lightblue", Color( 173, 216, 230 ) },
		{ "lightcoral", Color( 240, 128, 128 ) },
		{ "lightcyan", Color( 224, 255, 255 ) },
		{ "lightgoldenrodyellow", Color( 250, 250, 210 ) },
		{ "lightgray", Color( 211, 211, 211 ) },
		{ "lightgreen", Color( 144, 238, 144 ) },
		{ "lightgrey", Color( 211, 211, 211 ) },
		{ "lightpink", Color( 255, 182, 193 ) },
		{ "lightsalmon", Color( 255, 160, 122 ) },
		{ "lightseagreen", Color( 32, 178, 170 ) },
		{ "lightskyblue", Color( 135, 206, 250 ) },
		{ "lightslategray", Color( 119, 136, 153 ) },
		{ "lightslategrey", Color( 119, 136, 153 ) },
		{ "lightsteelblue", Color( 176, 196, 222 ) },
		{ "lightyellow", Color( 255, 255, 224 ) },
		{ "lime", Color( 0, 255, 0 ) },
		{ "limegreen", Color( 50, 205, 50 ) },
		{ "linen", Color( 250, 240, 230 ) },
		{ "magenta", Color( 255, 0, 255 ) },
		{ "maroon", Color( 128, 0, 0 ) },
		{ "mediumaquamarine", Color( 102, 205, 170 ) },
		{ "mediumblue", Color( 0, 0, 205 ) },
		{ "mediumorchid", Color( 186, 85, 211 ) },
		{ "mediumpurple", Color( 147, 112, 219 ) },
		{ "mediumseagreen", Color( 60, 179, 113 ) },
		{ "mediumslateblue", Color( 123, 104, 238 ) },
		{ "mediumspringgreen", Color( 0, 250, 154 ) },
		{ "mediumturquoise", Color( 72, 209, 204 ) },
		{ "mediumvioletred", Color( 199, 21, 133 ) },
		{ "midnightblue", Color( 25, 25, 112 ) },
		{ "mintcream", Color( 245, 255, 250 ) },
		{ "mistyrose", Color( 255, 228, 225 ) },
		{ "moccasin", Color( 255, 228, 181 ) },
		{ "navajowhite", Color( 255, 222, 173 ) },
		{ "navy", Color( 0, 0, 128 ) },
		{ "oldlace", Color( 253, 245, 230 ) },
		{ "olive", Color( 128, 128, 0 ) },
		{ "olivedrab", Color( 107, 142, 35 ) },
		{ "orange", Color( 255, 165, 0 ) },
		{ "orangered", Color( 255, 69, 0 ) },
		{ "orchid", Color( 218, 112, 214 ) },
		{ "palegoldenrod", Color( 238, 232, 170 ) },
		{ "palegreen", Color( 152, 251, 152 ) },
		{ "paleturquoise", Color( 175, 238, 238 ) },
		{ "palevioletred", Color( 219, 112, 147 ) },
		{ "papayawhip", Color( 255, 239, 213 ) },
		{ "peachpuff", Color( 255, 218, 185 ) },
		{ "peru", Color( 205, 133, 63 ) },
		{ "pink", Color( 255, 192, 203 ) },
		{ "plum", Color( 221, 160, 221 ) },
		{ "powderblue", Color( 176, 224, 230 ) },
		{ "purple", Color( 128, 0, 128 ) },
		{ "rebeccapurple", Color( 102, 51, 153 ) },
		{ "red", Color( 255, 0, 0 ) },
		{ "rosybrown", Color( 188, 143, 143 ) },
		{ "royalblue", Color( 65, 105, 225 ) },
		{ "saddlebrown", Color( 139, 69, 19 ) },
		{ "salmon", Color( 250, 128, 114 ) },
		{ "sandybrown", Color( 244, 164, 96 ) },
		{ "seagreen", Color( 46, 139, 87 ) },
		{ "seashell", Color( 255, 245, 238 ) },
		{ "sienna", Color( 160, 82, 45 ) },
		{ "silver", Color( 192, 192, 192 ) },
		{ "skyblue", Color( 135, 206, 235 ) },
		{ "slateblue", Color( 106, 90, 205 ) },
		{ "slategray", Color( 112, 128, 144 ) },
		{ "slategrey", Color( 112, 128, 144 ) },
		{ "snow", Color( 255, 250, 250 ) },
		{ "springgreen", Color( 0, 255, 127 ) },
		{ "steelblue", Color( 70, 130, 180 ) },
		{ "tan", Color( 210, 180, 140 ) },
		{ "teal", Color( 0, 128, 128 ) },
		{ "thistle", Color( 216, 191, 216 ) },
		{ "tomato", Color( 255, 99, 71 ) },
		{ "transparent", Color( 0, 0, 0, 0 ) }, // Special!
		{ "turquoise", Color( 64, 224, 208 ) },
		{ "violet", Color( 238, 130, 238 ) },
		{ "wheat", Color( 245, 222, 179 ) },
		{ "white", Color( 255, 255, 255 ) },
		{ "whitesmoke", Color( 245, 245, 245 ) },
		{ "yellow", Color( 255, 255, 0 ) },
		{ "yellowgreen", Color( 154, 205, 50 ) },
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
// Purpose: Parses a quoted string
// Supports: "test", 'test'
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseQuotedString( CUtlString &sOutput, const char *pchString, const char **pchAfterParse /* = NULL */ )
{
	pchString = SkipSpaces( pchString );

	char chQuote = pchString[ 0 ];
	if ( chQuote != '\'' && chQuote != '\"' )
		return false;

	pchString++;
	const char *pchStart = pchString;

	while ( pchString[ 0 ] != chQuote && pchString[ 0 ] != '\0' && pchString[ 0 ] != '\n' )
	{
		pchString++;
	}

	if ( pchString[ 0 ] == '\0' || pchString[ 0 ] == '\n' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	sOutput.SetDirect( pchStart, pchString - pchStart );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS url
// Supports: url( ... )
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseURL( CUtlString &sPath, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( V_strnicmp( "url", pchString, 3) )
		return false;
	
	pchString += 3;

	if ( !BSkipLeftParen( pchString, &pchString ) )
		return false;

	bool bHasQuote = BSkipQuote( pchString, &pchString );
	pchString = SkipSpaces( pchString );

	int cch = V_strcspn( pchString, bHasQuote ? "\"'" : " )" );
	if ( bHasQuote  )
	{
		if ( pchString[cch] != '"' && pchString[cch] != '\'' )
			return false; // missing closing bracket
	}

	if ( V_strlen(pchString) < 7 || 
		(  V_strnicmp( "http://", pchString, 7 ) != 0 && 
			V_strnicmp( "https://", pchString, 8 ) != 0 && 
			V_strnicmp( "file://", pchString, 7 ) != 0 &&
			V_strnicmp( "s2r://", pchString, 6 ) != 0  ) )
	{
		CUtlString tmpURI;

		tmpURI.SetDirect( pchString, cch );

// 7ls
// There's a bug in the source1 version of CUtlString::SetDirect which results in the string holding an 
// invalid length value (one less than it should be). So a subsequent call to append(0) ends up removing
// the last character from the string.
// The call to append(0) isn't actually required because SetDirect null terminates the string anyway,
// so remove it for source1 - avoids having to fix the CUtlString implementation which may have unintended
// consequences if other code relies on existing behaviour.
#ifndef PANORAMA_USE_S1WRAPPER
		tmpURI.Append( "\0" );
#endif
		sPath = "file://";
		sPath += tmpURI;
	}
	else
	{
		sPath.SetDirect( pchString, cch );
#ifndef PANORAMA_USE_S1WRAPPER
		sPath.Append( "\0" );
#endif
	}

	if ( bHasQuote  )
		cch++; // skip the end quote char

	if ( !BSkipRightParen( pchString +cch, &pchString ) )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to parse some known amount of a string to a float
//-----------------------------------------------------------------------------
bool BParseNumberInternal( double *pNumber, const char *pchString, uint32 cchNumber, uint32 cchToAdvance, const char **pchAfterParse )
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
bool BParseNumberInternal( float *pNumber, const char *pchString, uint32 cchNumber, uint32 cchToAdvance, const char **pchAfterParse )
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
// Purpose: Parses a CSS length string
// Supports: <number>px
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseLength( float *pLength, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );

	int cch = V_strcspn( pchString, " ,);" );

	// 0 w/o units is a special case in CSS. Accept it as a length
	if ( cch == 1 && pchString[0] == '0' )
	{
		*pLength = 0.0f;
		if ( pchAfterParse )
			*pchAfterParse = pchString + cch;

		return true;
	}

	if ( cch < 2 || pchString[cch - 2] != 'p' || pchString[cch - 1] != 'x' )
		return false;

	return BParseNumberInternal( pLength, pchString, cch - 2, cch, pchAfterParse );
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS number into a float
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseNumber( float *pNumber, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	int cch = V_strcspn( pchString, " ,);" );

	return BParseNumberInternal( pNumber, pchString, cch, cch, pchAfterParse );	
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS number into a float
// Supports: <number>s, <number>ms
// Returns: seconds
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseTime( double *pSeconds, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );

	int cch = V_strcspn( pchString, " ,);" );
	if ( cch < 2 || pchString[cch - 1] != 's' )
		return false;

	bool bMS = (pchString[cch - 2] == 'm');
	
	if ( !BParseNumberInternal( pSeconds, pchString, cch - (bMS ? 2 : 1), cch, pchAfterParse ) )
		return false;

	if ( bMS )
		*pSeconds /= 1000.0f;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS identifier
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseIdent( char *rgchIdent, int cubIdent, const char *pchString, const char **pchAfterParse )
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
bool CSSHelpers::BParseIdentToSymbol( CPanoramaSymbol *pSymbol, const char *pchString, const char **pchAfterParse )
{
	char rgchIdent[k_nCSSPropertyNameMax];
	if ( !BParseIdent( rgchIdent, V_ARRAYSIZE( rgchIdent ), pchString, pchAfterParse ) )
		return false;

	*pSymbol = CPanoramaSymbol( rgchIdent );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parse a CSS percent value
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseIdentToStyleSymbol( CStyleSymbol *pSymbol, const char *pchString, const char **pchAfterParse )
{
	char rgchIdent[k_nCSSPropertyNameMax];
	if ( !BParseIdent( rgchIdent, V_ARRAYSIZE( rgchIdent ), pchString, pchAfterParse ) )
		return false;

	*pSymbol = CStyleSymbol( rgchIdent );
	if ( pSymbol->IsValid() )
		return true;
	else 
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: Parse a CSS percent value
//-----------------------------------------------------------------------------
bool CSSHelpers::BParsePercent( float *pPercent, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );

	int cch = V_strcspn( pchString, " ,);" );
	if ( cch == 0 || pchString[cch - 1] != '%' )
		return false;	

	return BParseNumberInternal( pPercent, pchString, cch - 1, cch, pchAfterParse );
}


//-----------------------------------------------------------------------------
// Purpose: Tries to parse value into a UILength for sizing (length, percent, fill-parent-flow( weight ), fit-children )
//----------------------------------------------------------------------------
bool CSSHelpers::BParseIntoUILengthForSizing( CUILength *pLength, const char *pchString, const char **pchAfterParse, float flScalingFactor )
{
	pchString = SkipSpaces( pchString );

	if ( BParseIntoUILength( pLength, pchString, pchAfterParse, flScalingFactor ) )
		return true;

	if ( V_strnicmp( pchString, "fill-parent-flow", V_strlen( "fill-parent-flow" ) ) == 0 )
	{
		pchString += V_strlen( "fill-parent-flow" );
		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;

		float flWeight;
		if ( BParseNumber( &flWeight, pchString, &pchString ) )
		{
			if ( BSkipRightParen( pchString, &pchString ) )
			{
				if ( pchAfterParse ) 
					*pchAfterParse = pchString;

				pLength->SetFillParentFlow( flWeight );
				return true;
			}
		}
	}
	else if ( V_strnicmp( pchString, "fit-children", V_strlen( "fit-children" ) ) == 0 )
	{
		pchString += V_strlen( "fit-children" );
		if ( pchAfterParse ) 
			*pchAfterParse = pchString;

		pLength->SetFitChildren();
		return true;
	}
	else if ( V_strnicmp( pchString, "height-percentage", V_strlen( "height-percentage" ) ) == 0 )
	{
		pchString += V_strlen( "height-percentage" );
		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;

		float flPercentage;
		if ( BParsePercent( &flPercentage, pchString, &pchString ) )
		{
			if ( BSkipRightParen( pchString, &pchString ) )
			{
				if ( pchAfterParse )
					*pchAfterParse = pchString;

				pLength->SetHeightPercentage( flPercentage );
			}
		}

		return true;
	}
	else if ( V_strnicmp( pchString, "width-percentage", V_strlen( "width-percentage" ) ) == 0 )
	{
		pchString += V_strlen( "width-percentage" );
		if ( !BSkipLeftParen( pchString, &pchString ) )
			return false;

		float flPercentage;
		if ( BParsePercent( &flPercentage, pchString, &pchString ) )
		{
			if ( BSkipRightParen( pchString, &pchString ) )
			{
				if ( pchAfterParse )
					*pchAfterParse = pchString;

				pLength->SetWidthPercentage( flPercentage );
			}
		}

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Tries to parse value into a UILength (length or percent)
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseIntoUILength( CUILength *pLength, const char *pchString, const char **pchAfterParse, float flScalingFactor )
{
	pchString = SkipSpaces( pchString );

	float flValue;
	if ( BParsePercent( &flValue, pchString, pchAfterParse ) )
	{
		pLength->SetPercent( flValue );
		return true;
	}
	else if ( BParseLength( &flValue, pchString, pchAfterParse ) )
	{
		// Don't make width/height values of 1.0 shrink, as things like 1 px horizontal rules will go invisible!  Kind of hacky,
		// but this mostly works without issue as we have so few things < 1px.
		if ( fabsf( 1.0f - fabsf( flValue ) ) < 0.1f && flScalingFactor < 1.0f  )
			pLength->SetLength( flValue );
		else
		{
			// If the original value was pixel aligned, try to keep that.  This is important so text doesn't accumulate error layers down
			// from us and thus get blurry.  Can't apply this rule to all values though as things like a shadow offset of 5.5 can be common
			// and the half pixel can be important for the shadow to balance on both sides of the layer.
			if ( fabsf( flValue - (float)RoundFloatToInt( flValue ) ) < 0.001f )
				pLength->SetLength( (float)RoundFloatToInt( flValue * flScalingFactor ) );
			else
				pLength->SetLength( flValue * flScalingFactor );
		}
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Tries to parse value into two UILengths. If only one is provided,
// then its value will be set into both.
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseIntoTwoUILengths( CUILength *pLength1, CUILength *pLength2, const char *pchString, const char **pchAfterParse, float flScalingFactor /* = 1.0f */ )
{
	if ( !BParseIntoUILength( pLength1, pchString, &pchString, flScalingFactor ) )
		return false;

	if ( !BParseIntoUILength( pLength2, pchString, &pchString, flScalingFactor ) )
	{
		*pLength2 = *pLength1;
	}

	if ( pchAfterParse )
		*pchAfterParse = pchString;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Skips a ( and any white space that precedes it. Will return false if no paren was found
//-----------------------------------------------------------------------------
bool CSSHelpers::BSkipLeftParen( const char *pchString, const char **pchAfterParse )
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
bool CSSHelpers::BSkipQuote( const char *pchString, const char **pchAfterParse )
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
bool CSSHelpers::BSkipSlash( const char *pchString, const char **pchAfterParse )
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
bool CSSHelpers::BSkipRightParen( const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( pchString[0] != ')' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Skips a particular css 'name' token, given the expected token
//
// EXAMPLES:
//
//     const char* pchAfterParse = nullptr;
//     BSkipName("good", "goodness", &pchAfterParse)
//							-> FALSE, pchAfterParse unchanged.
//     BSkipName("good", "good stuff", &pchAfterParse)
//							-> TRUE, pchAfterParse advanced to " stuff"
//-----------------------------------------------------------------------------
static bool BSkipNameInternal( const char* szExpectedIdent, bool bCaseSensitive, const char* pchString, const char** pchAfterParse )
{
	int len = V_strlen( szExpectedIdent );

	pchString = CSSHelpers::SkipSpaces( pchString );
	if ( bCaseSensitive )
	{
		if ( V_strncmp( pchString, szExpectedIdent, len ) != 0 )
			return false;
	}
	else
	{
		if ( V_strnicmp( pchString, szExpectedIdent, len ) != 0 )
			return false;
	}

	// advance past the matched string
	pchString += len;

	// Make sure we aren't followed by a legal token-character
	//
	// From https://www.w3.org/TR/CSS21/grammar.html, the lexical grammar for {name}:
	//
	//	name		{nmchar}+
	//	nmchar		[_a-z0-9-]|{nonascii}|{escape}
	//	nonascii	[\240-\377]						// decimal 160 -> 255
	//	escape		{unicode}|\\[^\r\n\f0-9a-f]		// starts with "\"
	//	unicode		\\{h}{1,6}(\r\n|[ \t\r\n\f])?	// also starts with "\"

	unsigned char c = *pchString; // next character
	if ( ( c >= 'a' && c <= 'z' )
		|| ( c >= 'A' && c <= 'Z' )
		|| ( c >= '0' && c <= '9' )
		|| ( c == '-' )
		|| ( c == '_' )
		|| ( c == '\\' ) // "escaped" or "unicode"
		|| ( c >= 160 ) // "nonascii"
		)
	{
		// name doesn't match the full token (matching "the" against "thematic" should fail)
		return false;
	}

	// found it
	if ( pchAfterParse )
		*pchAfterParse = pchString;
	return true;
}

bool CSSHelpers::BSkipName( const char* szExpectedIdent, const char* pchString, const char** pchAfterParse /* = NULL */ )
{
	return BSkipNameInternal( szExpectedIdent, false, pchString, pchAfterParse );
}

bool CSSHelpers::BSkipNameCaseSensitive( const char* szExpectedIdent, const char* pchString, const char** pchAfterParse /* = NULL */ )
{
	return BSkipNameInternal( szExpectedIdent, true, pchString, pchAfterParse );
}




//-----------------------------------------------------------------------------
// Purpose: Skips a comma and any white space that precedes it. Will return false if no comma was found
//-----------------------------------------------------------------------------
bool CSSHelpers::BSkipComma( const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( pchString[0] != ',' )
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString + 1;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses animation timing function
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseTimingFunction( EAnimationTimingFunction *peTimingFunction, CCubicBezierCurve< Vector2D > *pCubicBezier, const char *pchString, const char **pchAfterParse )
{
	// parsing as ident should work
	char rgchBuffer[128];
	if ( !CSSHelpers::BParseIdent( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), pchString, pchAfterParse ) )
		return false;

	if ( V_strcmp( rgchBuffer, "none" ) == 0 )
		*peTimingFunction = k_EAnimationNone;
	else if ( V_strcmp( rgchBuffer, "ease" ) == 0 )
		*peTimingFunction = k_EAnimationEase;
	else if ( V_strcmp( rgchBuffer, "ease-in" ) == 0 )
		*peTimingFunction = k_EAnimationEaseIn;
	else if ( V_strcmp( rgchBuffer, "ease-out" ) == 0 )
		*peTimingFunction = k_EAnimationEaseOut;
	else if ( V_strcmp( rgchBuffer, "ease-in-out" ) == 0 )
		*peTimingFunction = k_EAnimationEaseInOut;
	else if ( V_strcmp( rgchBuffer, "linear" ) == 0 )
		*peTimingFunction = k_EAnimationLinear;
	else if ( V_strncmp( rgchBuffer, "cubic-bezier", V_strlen( "cubic-bezier" ) ) == 0 )
		*peTimingFunction = k_EAnimationCustomBezier;
	else
		return false;

	if ( *peTimingFunction != k_EAnimationCustomBezier )
	{
		Vector2D vecControlPoints[4];
		panorama::GetAnimationCurveControlPoints( *peTimingFunction, vecControlPoints );
		pCubicBezier->SetControlPoints( vecControlPoints );
	}
	else
	{
		*pchAfterParse = CSSHelpers::SkipSpaces( *pchAfterParse );
		if( !CSSHelpers::BSkipLeftParen( *pchAfterParse, pchAfterParse ) )
			return false;

		*pchAfterParse = CSSHelpers::SkipSpaces( *pchAfterParse );

		int i = V_strcspn( *pchAfterParse, ")" );
		char cSaved = (*pchAfterParse)[i];
		((char*)(*pchAfterParse))[i] = 0;

		CUtlVector< float > vecValues;
		if ( !BParseCommaSepList( &vecValues, CSSHelpers::BParseNumber, *pchAfterParse ) )
		{
			((char*)(*pchAfterParse))[i] = cSaved;
			return false;
		}

		((char*)(*pchAfterParse))[i] = cSaved;
		*pchAfterParse = (*pchAfterParse)+i+1;

		if ( vecValues.Count() != 4 )
			return false;

		Vector2D vecPoints[4];
		vecPoints[0].x = 0.0f;
		vecPoints[0].y = 0.0f;
// 		vecPoints[0].z = 0.0f;
		vecPoints[1].x = vecValues[0];
		vecPoints[1].y = vecValues[1];
// 		vecPoints[1].z = 0.0f;
		vecPoints[2].x = vecValues[2];
		vecPoints[2].y = vecValues[3];
// 		vecPoints[2].z = 0.0f;
		vecPoints[3].x = 1.0f;
		vecPoints[3].y = 1.0f;
// 		vecPoints[3].z = 0.0f;

		pCubicBezier->SetControlPoints( vecPoints );
	}


	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses animation direction
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseAnimationDirectionFunction( EAnimationDirection *peAnimationDirection, const char *pchString, const char **pchAfterParse )
{
	// parsing as ident should work
	char rgchBuffer[128];
	if ( !CSSHelpers::BParseIdent( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), pchString, pchAfterParse ) )
		return false;

	if ( V_strcmp( rgchBuffer, "normal" ) == 0 )
		*peAnimationDirection = k_EAnimationDirectionNormal;
	else if ( V_strcmp( rgchBuffer, "alternate" ) == 0 )
		*peAnimationDirection = k_EAnimationDirectionAlternate;
	else if ( V_strcmp( rgchBuffer, "reverse" ) == 0 )
		*peAnimationDirection = k_EAnimationDirectionReverse;
	else if ( V_strcmp( rgchBuffer, "alternate-reverse" ) == 0 )
		*peAnimationDirection = k_EAnimationDirectionAlternateReverse;
	else
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Parses animation FillMode
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseAnimationFillModeFunction( EAnimationFillMode *peAnimationFillMode, const char *pchString, const char **pchAfterParse )
{
	// parsing as ident should work
	char rgchBuffer[ 128 ];
	if ( !CSSHelpers::BParseIdent( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), pchString, pchAfterParse ) )
		return false;

	if ( V_strcmp( rgchBuffer, "none" ) == 0 )
		*peAnimationFillMode = k_EAnimationFillModeNone;
	else if ( V_strcmp( rgchBuffer, "forwards" ) == 0 )
		*peAnimationFillMode = k_EAnimationFillModeForwards;
	else if ( V_strcmp( rgchBuffer, "backwards" ) == 0 )
		*peAnimationFillMode = k_EAnimationFillModeBackwards;
	else if ( V_strcmp( rgchBuffer, "both" ) == 0 )
		*peAnimationFillMode = k_EAnimationFillModeBoth;
	else
		return false;

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: Parses an angle
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseAngle( float *pDegrees, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );

	// currently just parses degrees
	int cch = V_strcspn( pchString, " ,);" );
	if ( cch < 3 || V_strncmp( &pchString[cch - 3], "deg", 3 )  != 0 )
		return false;

	return BParseNumberInternal( pDegrees, pchString, cch - 3, cch, pchAfterParse );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to parse comma separated lists with same value type
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseCommaSepList( CUtlVector< EAnimationTimingFunction > *pvec, CUtlVector< CCubicBezierCurve< Vector2D > > *pvec2, bool (*func)( EAnimationTimingFunction *pvec, CCubicBezierCurve< Vector2D > *pvec2, const char *, const char ** ), const char *pchString )
{
	while ( *pchString != '\0' )
	{
		EAnimationTimingFunction val;
		CCubicBezierCurve< Vector2D > val2;
		if ( !func( &val, &val2, pchString, &pchString ) )
			return false;

		pvec->AddToTail( val );
		pvec2->AddToTail( val2 );

		// done?
		if ( !CSSHelpers::BSkipComma( pchString, &pchString ) )
		{
			// no comma, should be empty string
			pchString = CSSHelpers::SkipSpaces( pchString );
			if ( pchString[0] != '\0' )
				return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses transform function
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseTransformFunction( CTransform3D **pTransform, const char *pchString, const char **pchAfterParse, float flScalingFactor )
{
	static const char k_rgchTranslate3D[] = "translate3d(";
	static const char k_rgchTranslateX[] = "translateX(";
	static const char k_rgchTranslateY[] = "translateY(";
	static const char k_rgchTranslateZ[] = "translateZ(";
	static const char k_rgchScale3D[] = "scale3d(";
	static const char k_rgchScaleX[] = "scaleX(";
	static const char k_rgchScaleY[] = "scaleY(";
	static const char k_rgchScaleZ[] = "scaleZ(";
	static const char k_rgchRotateX[] = "rotateX(";
	static const char k_rgchRotateY[] = "rotateY(";
	static const char k_rgchRotateZ[] = "rotateZ(";
	static const char k_rgchRotate3D[] = "rotate3d(";

	pchString = SkipSpaces( pchString );

	// copy function into buffer for easy parsing. looking for "func( ... )". Terminate string at ')'
	char rgchBuffer[256];
	const char *pchEnd = V_strstr( pchString, ")" );
	if ( !pchEnd || (pchEnd - pchString) > V_ARRAYSIZE( rgchBuffer ) )
		return false;

	V_strncpy( rgchBuffer, pchString, pchEnd - pchString + 1 );
	rgchBuffer[ pchEnd - pchString ] = '\0';
	pchString = rgchBuffer;

	if ( V_strnicmp( k_rgchTranslate3D, pchString, V_ARRAYSIZE( k_rgchTranslate3D ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchTranslate3D ) - 1;

		// should be "x, y, z"
		CUtlVector< CUILength > vecValues;
		if ( !BParseCommaSepListWithScaling( &vecValues, CSSHelpers::BParseIntoUILength, pchString, flScalingFactor ) || vecValues.Count() != 3 )
			return false;

		*pTransform = new CTransformTranslate3D( vecValues[0], vecValues[1], vecValues[2] );
	}	
	else if ( V_strnicmp( k_rgchTranslateX, pchString, V_ARRAYSIZE( k_rgchTranslateX ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchTranslateX ) - 1;

		CUILength length;
		if ( !BParseIntoUILength( &length, pchString, &pchString, flScalingFactor ) )
			return false;

		*pTransform = new CTransformTranslate3D( length, CUILength( 0, CUILength::k_EUILengthLength ), CUILength( 0, CUILength::k_EUILengthLength ) );
	}
	else if ( V_strnicmp( k_rgchTranslateY, pchString, V_ARRAYSIZE( k_rgchTranslateY ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchTranslateY ) - 1;

		CUILength length;
		if ( !BParseIntoUILength( &length, pchString, &pchString, flScalingFactor ) )
			return false;

		*pTransform = new CTransformTranslate3D( CUILength( 0, CUILength::k_EUILengthLength ), length, CUILength( 0, CUILength::k_EUILengthLength ) );
	}
	else if ( V_strnicmp( k_rgchTranslateZ, pchString, V_ARRAYSIZE( k_rgchTranslateZ ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchTranslateZ ) - 1;

		CUILength length;
		if ( !BParseIntoUILength( &length, pchString, &pchString, flScalingFactor ) )
			return false;

		*pTransform = new CTransformTranslate3D( CUILength( 0, CUILength::k_EUILengthLength ), CUILength( 0, CUILength::k_EUILengthLength ), length );
	}
	else if ( V_strnicmp( k_rgchScale3D, pchString, V_ARRAYSIZE( k_rgchScale3D ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchScale3D ) - 1;

		// should be "x, y, z"
		CUtlVector< float > vecValues;
		if ( !BParseCommaSepList( &vecValues, CSSHelpers::BParseNumber, pchString ) || vecValues.Count() != 3 )
			return false;

		*pTransform = new CTransformScale3D( vecValues[0], vecValues[1], vecValues[2] );
	}
	else if ( V_strnicmp( k_rgchScaleX, pchString, V_ARRAYSIZE( k_rgchScaleX ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchScaleX ) - 1;

		// should be "x"
		CUtlVector< float > vecValues;
		if ( !BParseCommaSepList( &vecValues, CSSHelpers::BParseNumber, pchString ) || vecValues.Count() != 1 )
			return false;

		*pTransform = new CTransformScale3D( vecValues[0], 1.0f, 1.0f );
	}
	else if ( V_strnicmp( k_rgchScaleY, pchString, V_ARRAYSIZE( k_rgchScaleY ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchScaleY ) - 1;

		// should be "y"
		CUtlVector< float > vecValues;
		if ( !BParseCommaSepList( &vecValues, CSSHelpers::BParseNumber, pchString ) || vecValues.Count() != 1 )
			return false;

		*pTransform = new CTransformScale3D( 1.0f, vecValues[0], 1.0f );
	}
	else if ( V_strnicmp( k_rgchScaleZ, pchString, V_ARRAYSIZE( k_rgchScaleZ ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchScaleZ ) - 1;

		// should be "z"
		CUtlVector< float > vecValues;
		if ( !BParseCommaSepList( &vecValues, CSSHelpers::BParseNumber, pchString ) || vecValues.Count() != 1 )
			return false;

		*pTransform = new CTransformScale3D( 1.0f, 1.0f, vecValues[0] );
	}
	else if ( V_strnicmp( k_rgchRotateX, pchString, V_ARRAYSIZE( k_rgchRotateX ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchRotateX ) - 1;

		float flDegrees;
		if ( !BParseAngle( &flDegrees, pchString, &pchString ) )
			return false;

		*pTransform = new CTransformRotate3D( 0, 0, flDegrees );
	}
	else if ( V_strnicmp( k_rgchRotateY, pchString, V_ARRAYSIZE( k_rgchRotateY ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchRotateY ) - 1;

		float flDegrees;
		if ( !BParseAngle( &flDegrees, pchString, &pchString ) )
			return false;

		*pTransform = new CTransformRotate3D( flDegrees, 0, 0 );
	}
	else if ( V_strnicmp( k_rgchRotateZ, pchString, V_ARRAYSIZE( k_rgchRotateZ ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchRotateZ ) - 1;

		float flDegrees;
		if ( !BParseAngle( &flDegrees, pchString, &pchString ) )
			return false;

		*pTransform = new CTransformRotate3D( 0, flDegrees, 0 );
	}
	else if ( V_strnicmp( k_rgchRotate3D, pchString, V_ARRAYSIZE( k_rgchRotate3D ) - 1 ) == 0 )
	{
		pchString += V_ARRAYSIZE( k_rgchRotate3D ) - 1;

		// should be "x, y, z, angle" where x, y, z are normalized
		float x, y, z;
		if ( !CSSHelpers::BParseNumber( &x, pchString, &pchString ) || !CSSHelpers::BSkipComma( pchString, &pchString ) )
			return false;

		if ( !CSSHelpers::BParseNumber( &y, pchString, &pchString ) || !CSSHelpers::BSkipComma( pchString, &pchString ) )
			return false;

		if ( !CSSHelpers::BParseNumber( &z, pchString, &pchString ) || !CSSHelpers::BSkipComma( pchString, &pchString ) )
			return false;

		Vector vec( x, y, z );
		vec.NormalizeInPlace();

		float flDegrees;
		if ( !BParseAngle( &flDegrees, pchString, &pchString ) )
			return false;

		*pTransform = new CTransformRotate3D( vec.y* flDegrees, vec.z * flDegrees, vec.x * flDegrees );
	}
	else
	{
		return false;
	}

	if ( pchAfterParse )
		*pchAfterParse = pchEnd + 1;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses an angle
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseBorderStyle( EBorderStyle *pStyle, const char *pchString, const char **pchAfterParse )
{
	pchString = SkipSpaces( pchString );
	if ( V_strnicmp( pchString, "none", V_strlen( "none" ) ) == 0 )
	{
		pchString += V_strlen( "none" );
		*pStyle = k_EBorderStyleNone;
		if ( pchAfterParse )
			*pchAfterParse = pchString;
		return true;
	}
	else if( V_strnicmp( pchString, "solid", V_strlen( "solid" ) ) == 0 )
	{
		pchString += V_strlen( "solid" );
		*pStyle = k_EBorderStyleSolid;
		if ( pchAfterParse )
			*pchAfterParse = pchString;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to make float output pretty
//
// WARNING: THIS RETURNS A POINTER TO A STATIC BUFFER. DO NOT HOLD RESULTS FROM A 
//			PREVIOUS CALL ACROSS THE NEXT
//-----------------------------------------------------------------------------
static const char *PretifyFloat( float flValue )
{
	static char s_rgchBuffer[32];
	V_snprintf( s_rgchBuffer, V_ARRAYSIZE( s_rgchBuffer ), "%f", flValue );

	// kill excessive trailing 0's after decimal point
	char *pch = V_strstr( s_rgchBuffer, "." );
	if ( !pch )
		return &s_rgchBuffer[0];

	while ( *pch != '\0' )
	{
		// find next 0
		while ( *pch != '\0' && *pch != '0' )
			pch++;

		char *pchStart = pch;

		// see if this is the last 0
		while ( *pch != '\0' && *pch == '0' )
			pch++;

		// if we hit the end of the string, was last 0
		if ( *pch == '\0' )
			*(++pchStart) = '\0';
	}
	

	return &s_rgchBuffer[0];
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a UI length
//-----------------------------------------------------------------------------
void CSSHelpers::AppendUILength( CFmtStr1024 *pfmtBuffer, const CUILength &length )
{
	if ( length.IsPercent() )
		AppendPercent( pfmtBuffer, length.GetValue() );
	else if ( length.IsLength() )
		CSSHelpers::AppendLength( pfmtBuffer, length.GetValue() );
	else if ( length.IsFitChildren() )
		pfmtBuffer->Append( "fit-children" );
	else if ( length.IsFillParentFlow() )
		pfmtBuffer->AppendFormat( "fill-parent-flow( %s )", PretifyFloat( length.GetValue() ) );
	else if ( length.IsHeightPercentage() )
		pfmtBuffer->AppendFormat( "height-percentage( %s )", PretifyFloat( length.GetValue() ) );
	else if ( length.IsWidthPercentage() )
		pfmtBuffer->AppendFormat( "width-percentage( %s )", PretifyFloat( length.GetValue() ) );
	else if ( !length.IsSet() )
		pfmtBuffer->AppendFormat( "none" );
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a float
//-----------------------------------------------------------------------------
void CSSHelpers::AppendFloat( CFmtStr1024 *pfmtBuffer, float flValue )
{
	pfmtBuffer->Append( PretifyFloat( flValue ) );
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a color
//-----------------------------------------------------------------------------
void CSSHelpers::AppendColor( CFmtStr1024 *pfmtBuffer, Color c )
{
	if ( c == Color( 0, 0, 0, 0 ) )
	{
		pfmtBuffer->Append( "transparent" );
	}
	else
	{
		pfmtBuffer->AppendFormat( "#%02X%02X%02X%02X", c.r(), c.g(), c.b(), c.a() );			
	}
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a transform
//-----------------------------------------------------------------------------
void CSSHelpers::AppendTransform( CFmtStr1024 *pfmtBuffer, CTransform3D *pTransform )
{
	if ( pTransform->GetType() == k_ETransform3DTranslate )
	{
		CTransformTranslate3D *pTranslate = (CTransformTranslate3D*)pTransform;

		pfmtBuffer->Append( "translate3d( " );
		AppendUILength( pfmtBuffer, pTranslate->GetX() );
		pfmtBuffer->Append( " " );
		AppendUILength( pfmtBuffer, pTranslate->GetY() );
		pfmtBuffer->Append( " " );
		AppendUILength( pfmtBuffer, pTranslate->GetZ() );
		pfmtBuffer->Append( " )" );
	}
	else if ( pTransform->GetType() == k_ETransform3DRotate )
	{
		CTransformRotate3D *pRotate = (CTransformRotate3D*)pTransform;
		QAngle angles = pRotate->GetAngles();

		pfmtBuffer->Append( "rotate3d( " );
		pfmtBuffer->AppendFormat( "%sdeg", PretifyFloat( angles.x ) );
		pfmtBuffer->AppendFormat( ", %sdeg", PretifyFloat( angles.y ) );
		pfmtBuffer->AppendFormat( ", %sdeg )", PretifyFloat( angles.z ) );
	}
	else if ( pTransform->GetType() == k_ETransform3DScale )
	{
		CTransformScale3D *pScale = (CTransformScale3D*)pTransform;

		pfmtBuffer->Append( "scale3d( " );
		pfmtBuffer->AppendFormat( "%s", PretifyFloat( pScale->GetX() ) );
		pfmtBuffer->AppendFormat( ", %s", PretifyFloat( pScale->GetY() ) );
		pfmtBuffer->AppendFormat( ", %s )", PretifyFloat( pScale->GetZ() ) );		
	}
	else
	{
		AssertMsg( false, "Unknown transform type" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of time
//-----------------------------------------------------------------------------
void CSSHelpers::AppendTime( CFmtStr1024 *pfmtBuffer, float flValue )
{
	if ( flValue > 1.0f )
		pfmtBuffer->AppendFormat( "%ss", PretifyFloat( flValue ) );
	else
		pfmtBuffer->AppendFormat( "%sms", PretifyFloat( flValue * 1000.0f ) );
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a fill brush collection
//-----------------------------------------------------------------------------
void CSSHelpers::AppendFillBrushCollection( CFmtStr1024 *pfmtBuffer, const CFillBrushCollection &collection )
{
	if ( collection.GetBrushCount() == 0 )
	{
		pfmtBuffer->Append( "none" );
		return;
	}

	const CFillBrushCollection::BrushVec_t &vecBrushes = collection.AccessBrushes();
	for ( int i = 0; i < vecBrushes.Count(); ++i )
	{
		if ( i > 0 )
			pfmtBuffer->Append( ",\n" );

		AppendFillBrush( pfmtBuffer, vecBrushes[i].m_Brush );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a fill brush
//-----------------------------------------------------------------------------
void CSSHelpers::AppendFillBrush( CFmtStr1024 *pfmtBuffer, const CFillBrush &brush )
{	
	if ( brush.GetType() == CFillBrush::k_EStrokeTypeFillColor )
	{
		AppendColor( pfmtBuffer, brush.GetFillColor() );
	}
	else if ( brush.GetType() == CFillBrush::k_EStrokeTypeParticleSystem )
	{
		pfmtBuffer->Append( "particle-system( ... )" );
	}
	else if ( brush.GetType() == CFillBrush::k_EStrokeTypeLinearGradient || brush.GetType() == CFillBrush::k_EStrokeTypeRadialGradient )
	{
		bool bLinear = (brush.GetType() == CFillBrush::k_EStrokeTypeLinearGradient);
		pfmtBuffer->Append( "gradient( " );
		pfmtBuffer->Append( bLinear ? "linear" : "radial" );
		pfmtBuffer->Append( ", " );
		
		if ( bLinear )
		{
			CUILength startX, startY, endX, endY;
			brush.GetStartAndEndPoints( startX, startY, endX, endY );

			AppendUILength( pfmtBuffer, startX );
			pfmtBuffer->Append( " " );
			AppendUILength( pfmtBuffer, startY );
			pfmtBuffer->Append( ", " );
			AppendUILength( pfmtBuffer, endX );
			pfmtBuffer->Append( " " );
			AppendUILength( pfmtBuffer, endY );
			pfmtBuffer->Append( ", " );
		}		
		else
		{
			CUILength centerX, centerY, offsetX, offsetY, radiusX, radiusY;
			brush.GetRadialGradientValues( centerX, centerY, offsetX, offsetY, radiusX, radiusY );
			AppendUILength( pfmtBuffer, centerX );
			pfmtBuffer->Append( " " );
			AppendUILength( pfmtBuffer, centerY );
			pfmtBuffer->Append( ", " );
			AppendUILength( pfmtBuffer, offsetX );
			pfmtBuffer->Append( " " );
			AppendUILength( pfmtBuffer, offsetY );
			pfmtBuffer->Append( ", " );
			AppendUILength( pfmtBuffer, radiusX );
			pfmtBuffer->Append( " " );
			AppendUILength( pfmtBuffer, radiusY );
		}

		const CUtlVector<CGradientColorStop> &stops = brush.AccessStopColors();
		FOR_EACH_VEC( stops, i )
		{
			if ( i > 0 )
				pfmtBuffer->Append( ", " );

			AppendGradientColorStop( pfmtBuffer, stops[i] );
		}

		pfmtBuffer->Append( " )" );
	}	
	else
	{
		AssertMsg( false, "Unknown brush type" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a fill brush
//-----------------------------------------------------------------------------
void CSSHelpers::AppendGradientColorStop( CFmtStr1024 *pfmtBuffer, const CGradientColorStop &stop )
{
	pfmtBuffer->AppendFormat( "color-stop( %s, ", PretifyFloat(stop.GetPosition() ) );
	AppendColor( pfmtBuffer, stop.GetColor() );
	pfmtBuffer->Append( " )" );
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a url
//-----------------------------------------------------------------------------
void CSSHelpers::AppendURL( CFmtStr1024 *pfmtBuffer, const char *pchURL )
{
	// show file:// ?
	pfmtBuffer->AppendFormat( "url( %s )", pchURL );
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of a length
//-----------------------------------------------------------------------------
void CSSHelpers::AppendLength( CFmtStr1024 *pfmtBuffer, float flValue )
{
	if ( flValue == k_flFloatAuto )
		pfmtBuffer->AppendFormat( "auto" );
	else if ( flValue == k_flFloatNotSet )
		pfmtBuffer->AppendFormat( "not set" );
	else
		pfmtBuffer->AppendFormat( "%spx", PretifyFloat( flValue ) );
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation of an angle
//-----------------------------------------------------------------------------
void CSSHelpers::AppendAngle( CFmtStr1024 *pfmtBuffer, float flDegrees )
{
	pfmtBuffer->AppendFormat( "%sdeg", PretifyFloat( flDegrees ) );
}


//-----------------------------------------------------------------------------
// Purpose: Appends the string representation the percent
//-----------------------------------------------------------------------------
void CSSHelpers::AppendPercent( CFmtStr1024 *pfmtBuffer, float flPercent )
{
	pfmtBuffer->AppendFormat( "%s%%", PretifyFloat( flPercent ) );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to parse horizontal alignment
//----------------------------------------------------------------------------- 
bool CSSHelpers::BParseHorizontalAlignment( EHorizontalAlignment *eHorizontalAlignment, const char *pchString, const char **pchAfterParse )
{
	char rgchIdent[k_nCSSPropertyNameMax];
	if ( !CSSHelpers::BParseIdent( rgchIdent, V_ARRAYSIZE( rgchIdent ), pchString, &pchString ) )
		return false;

	if ( V_stricmp( rgchIdent, "left" ) == 0 )
		*eHorizontalAlignment = k_EHorizontalAlignmentLeft;
	else if ( V_stricmp( rgchIdent, "center" ) == 0 || V_stricmp( rgchIdent, "middle" ) == 0 )
		*eHorizontalAlignment = k_EHorizontalAlignmentCenter;
	else if ( V_stricmp( rgchIdent, "right" ) == 0 )
		*eHorizontalAlignment = k_EHorizontalAlignmentRight;
	else if ( V_stricmp( rgchIdent, "center_nopixelsnap" ) == 0 )
		*eHorizontalAlignment = k_EHorizontalAlignmentCenterNoPixelSnap;
	else
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: helper to parse vertical alignment
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseVerticalAlignment( EVerticalAlignment *eVerticalAlignment, const char *pchString, const char **pchAfterParse )
{
	char rgchIdent[k_nCSSPropertyNameMax];
	if ( !CSSHelpers::BParseIdent( rgchIdent, V_ARRAYSIZE( rgchIdent ), pchString, &pchString ) )
		return false;

	if ( V_stricmp( rgchIdent, "top" ) == 0 )
		*eVerticalAlignment = k_EVerticalAlignmentTop;
	else if ( V_stricmp( rgchIdent, "center" ) == 0 || V_stricmp( rgchIdent, "middle" ) == 0 )
		*eVerticalAlignment = k_EVerticalAlignmentCenter;
	else if ( V_stricmp( rgchIdent, "bottom" ) == 0 )
		*eVerticalAlignment = k_EVerticalAlignmentBottom;
	else if ( V_stricmp( rgchIdent, "center_nopixelsnap" ) == 0 )
		*eVerticalAlignment = k_EVerticalAlignmentCenterNoPixelSnap;
	else
		return false;

	if ( pchAfterParse )
		*pchAfterParse = pchString;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to parse true/false
//			If string does not match either, returns false, else true
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseTrueFalse( const char *pchString, bool *pbValue )
{
	*pbValue = false;
	if ( V_stricmp( pchString, "true") == 0 )
		*pbValue = true;
	else if ( V_stricmp( pchString, "false") == 0 )
		*pbValue = false;
	else
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Converts enum to string
//-----------------------------------------------------------------------------
const char *panorama::PchNameFromEHorizontalAlignment( EHorizontalAlignment eHorizontalAlignment )
{
	if ( eHorizontalAlignment == k_EHorizontalAlignmentLeft )
		return "left";
	else if ( eHorizontalAlignment == k_EHorizontalAlignmentCenter )
		return "center";
	else if ( eHorizontalAlignment == k_EHorizontalAlignmentRight )
		return "right";
	else
		return "unset";
}


//-----------------------------------------------------------------------------
// Purpose: Converts enum to string
//-----------------------------------------------------------------------------
const char *panorama::PchNameFromEVerticalAlignment( EVerticalAlignment eVerticalAlignment )
{
	if ( eVerticalAlignment == k_EVerticalAlignmentTop )
		return "top";
	else if ( eVerticalAlignment == k_EVerticalAlignmentCenter )
		return "center";
	else if ( eVerticalAlignment == k_EVerticalAlignmentBottom )
		return "bottom";
	else
		return "unset";
}


//-----------------------------------------------------------------------------
// Purpose: Converts enum to string
//-----------------------------------------------------------------------------
const char *panorama::PchNameFromEContextUIPosition( EContextUIPosition ePosition )
{
	if ( ePosition == k_EContextUIPositionLeft )
		return "left";
	else if ( ePosition == k_EContextUIPositionTop )
		return "top";
	else if ( ePosition == k_EContextUIPositionRight )
		return "right";
	else if ( ePosition == k_EContextUIPositionBottom )
		return "bottom";
	else
		return "unset";
}

//-----------------------------------------------------------------------------
// Purpose: Helper to parse radial clip
//-----------------------------------------------------------------------------
bool CSSHelpers::BParseRadialClip( CUILength *pCenterX, CUILength *pCenterY, float *pStartAngle, float *pSectorAngle, const char *pchString, const char **pchAfterParse /* = NULL */ )
{
	pchString = SkipSpaces( pchString );
	if( V_strnicmp( pchString, "radial", V_strlen( "radial" ) ) == 0 )
	{
		const char *pchRadial = pchString + 6;

		if( !BSkipLeftParen( pchRadial, &pchRadial ) )
			return false;

		if( !BParseIntoUILength( pCenterX, pchRadial, &pchRadial ) )
		{
			if ( V_strnicmp( pchRadial, "auto", V_strlen( "auto"  ) ) == 0 )
				pchRadial += 4;
			else
				return false;
		}

		if( !BParseIntoUILength( pCenterY, pchRadial, &pchRadial ) )
		{
			if ( V_strnicmp( pchRadial, "auto", V_strlen( "auto"  ) ) == 0 )
				pchRadial += 4;
			else
				return false;
		}

		if( !BSkipComma( pchRadial, &pchRadial ) )
			return false;

		if( !BParseAngle( pStartAngle, pchRadial, &pchRadial ) )
			return false;

		if( !BSkipComma( pchRadial, &pchRadial ) )
			return false;

		if( !BParseAngle( pSectorAngle, pchRadial, &pchRadial ) )
			return false;

		BSkipRightParen( pchRadial, &pchRadial );

		if ( pchAfterParse ) *pchAfterParse = pchRadial;

		return true;
	}
	return false;
}

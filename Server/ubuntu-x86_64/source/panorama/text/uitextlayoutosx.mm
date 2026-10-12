//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#define DONT_DEFINE_BOOL // conflicts with Cocoa defn
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>
#import <CoreFoundation/CoreFoundation.h>
#include "stdafx.h"
#include "uitextlayoutosx.h"
#include "uifontfileloaderosx.h"
#include "uienginesdl.h"
#include "../renderer/sdlopenglsurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// Statics

CUtlSortVector< CUtlString > CUITextLayoutOSX::m_vecSortedValidFontNames( DefLessFuncCtx( CUtlString ) );

#define DIP_TO_POINT 1.0f;

extern GLuint CreateOpenGLTextureId();
const float k_flLargeFrameHeight = 100000.0f;

//-----------------------------------------------------------------------------
// Purpose: Helper to convert an EFontWeight to a DWrite font weight
//-----------------------------------------------------------------------------
float GetCoreTextFontWeight( EFontWeight eFontWeight )
{
	float dwweight = 0.0;
	switch( eFontWeight )
	{
	case k_EFontWeightMedium:
		dwweight = 0.24;
		break;
	case k_EFontWeightBold:
		dwweight = 0.5;
		break;
	case k_EFontWeightBlack:
		dwweight = 0.75;
		break;
	case k_EFontWeightLight:
		dwweight = -0.35;
		break;
	case k_EFontWeightThin:
		dwweight = -0.70;
		break;
	default:
		break;
	}

	return dwweight;
}





//-----------------------------------------------------------------------------
// Purpose: Init globals
//-----------------------------------------------------------------------------
bool CUITextLayoutOSX::BInitGlobals()
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Free globals
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::FreeGlobals()
{
}


//-----------------------------------------------------------------------------
// Purpose: Get list of valid font names, sorted
//-----------------------------------------------------------------------------
const CUtlSortVector< CUtlString > &CUITextLayoutOSX::GetSortedValidFontNames()
{
	if ( m_vecSortedValidFontNames.Count() == 0 )
	{
		CUITextLayoutOSX::BInitGlobals();
		
		//get an array of all the available font names
		CFArrayRef fontFamilies = CTFontManagerCopyAvailableFontFamilyNames();
		
		if ( fontFamilies != NULL )
		{
			//loop through the array
			for(CFIndex i = 0; i < CFArrayGetCount(fontFamilies); i++)
			{
				//get the current name
				CFStringRef fontName = (CFStringRef)CFArrayGetValueAtIndex(fontFamilies, i);
				m_vecSortedValidFontNames.Insert( CFStringGetCStringPtr( fontName, kCFStringEncodingMacRoman ) );
			}
			CFRelease( fontFamilies );
		}
	}

	return m_vecSortedValidFontNames;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUITextLayoutOSX::CUITextLayoutOSX()
{
	m_ctfFrameSetter = 0;
	m_attrString = 0;
	m_flMaxWidth = k_flFloatNotSet;
	m_flMaxHeight = k_flFloatNotSet;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUITextLayoutOSX::~CUITextLayoutOSX()
{
    if ( m_attrString )
        CFRelease( m_attrString );
	if ( m_ctfFrameSetter )
		CFRelease( m_ctfFrameSetter );
	m_ctfFrameSetter = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
CTFontRef CreateFontFromNameWeightStyle( const char *pchFontName, float flFontSize, EFontWeight weight, EFontStyle style )
{
	// for motiva OSX doesn't detect the font properly so we need to manually load by postscript name depending on the variant we want
	const char *pchPostScriptFontName = NULL;
	if ( !V_stricmp( pchFontName, "Motiva Sans" ) ) 
	{
		switch( weight )
		{
			case k_EFontWeightNormal:
			case k_EFontWeightMedium:
				if ( style == k_EFontStyleItalic  )
					pchPostScriptFontName = "MotivaSans-RegularItalic";
				else
					pchPostScriptFontName = "MotivaSans-Regular";
				break;
			case k_EFontWeightBlack:
			case k_EFontWeightBold:
				if ( style == k_EFontStyleItalic  )
					pchPostScriptFontName = "MotivaSans-BoldItalic";
				else
					pchPostScriptFontName = "MotivaSans-Bold";
				break;
			case k_EFontWeightLight:
			case k_EFontWeightThin:
				if ( style == k_EFontStyleItalic  )
					pchPostScriptFontName = "MotivaSans-LightItalic";
				else
					pchPostScriptFontName = "MotivaSans-Light";
				break;
				break;
			default:
				Assert( !"Unknown weight" );
				break;
		}		
		
	}	
	
	if ( !V_stricmp( pchFontName, "Consolas" ) ) 
	{
		pchFontName = "Menlo-Regular";
	}
	
	// CoreText performance note: Client called CTFontCreateWithName() using name "Arial" and got font with PostScript name "ArialMT".
	// For best performance, only use PostScript names when calling this API.
	if ( !V_stricmp( pchFontName, "Arial" ) )
	{
		pchFontName = "ArialMT";
	}

	double flWeight = GetCoreTextFontWeight( weight ); 	
	
	CTFontRef font = NULL;
	if ( pchPostScriptFontName )
	{
		CFMutableDictionaryRef fontAttrs = CFDictionaryCreateMutable(  kCFAllocatorDefault, 0,
																	 &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		
		if ( flWeight != 0.0f )
        {
            CFNumberRef cfnWeight = CFNumberCreate( kCFAllocatorDefault, kCFNumberFloatType, &flWeight  );
			CFDictionaryAddValue( fontAttrs, kCTFontWeightTrait, cfnWeight );
            CFRelease( cfnWeight );
        }
        CFNumberRef cfnSize = CFNumberCreate( kCFAllocatorDefault, kCFNumberFloatType, &flFontSize  );
		CFDictionaryAddValue( fontAttrs, kCTFontSizeAttribute, cfnSize );
        CFRelease( cfnSize );
		
		CFStringRef strPostscriptName  = CFStringCreateWithCString( NULL, pchPostScriptFontName, kCFStringEncodingUTF8 ); 
		CFDictionaryAddValue( fontAttrs, kCTFontNameAttribute, strPostscriptName );
		CFRelease( strPostscriptName );
		
		CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(fontAttrs);
		font =  CTFontCreateWithFontDescriptor(descriptor, flFontSize, NULL );
		
		CFRelease( descriptor );
		CFRelease( fontAttrs );
	}
	else
	{
		CFStringRef strFontName  = CFStringCreateWithCString( NULL, pchFontName, kCFStringEncodingUTF8 ); 
		font =  CTFontCreateWithName( strFontName, flFontSize, NULL );
		CFRelease( strFontName );
		
		CTFontRef fontNew = NULL;
		if ( style == k_EFontStyleItalic  )
		{
			fontNew =  CTFontCreateCopyWithSymbolicTraits( font, flFontSize, NULL, kCTFontItalicTrait, kCTFontItalicTrait );
		}
		else if ( flWeight > 0.0 )
		{
			fontNew =  CTFontCreateCopyWithSymbolicTraits( font, flFontSize, NULL, kCTFontBoldTrait, kCTFontBoldTrait );		
		}
		
		if ( fontNew )
		{
			CFRelease( font );
			font = fontNew;
		}
	}
	
	
	//	test code to extract the name of the font you loaded and compare it to what you expected
	CFStringRef sampleText = CTFontCopyName( font,kCTFontFullNameKey );
	char localBuffer[128];
	Boolean success;
	success = CFStringGetCString( sampleText, localBuffer, sizeof(localBuffer), kCFStringEncodingMacRoman);
	CFRelease( sampleText );

	return font;
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
bool CUITextLayoutOSX::BInitialize( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const IUITextServices::TextLayoutParams_t *pParams )
{
	VPROF_BUDGET( "CUITextLayoutOSX::BInitialize", VPROF_BUDGETGROUP_TENFOOT );

	Assert( m_ctfFrameSetter == 0 );
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	m_flFontSize = flFontSize * DIP_TO_POINT;// point size in 72dpi, our font size is in 96 dpi space,so scale
	m_Weight = weight;
	m_Style = style;
	m_bWrap = bWrap;
	
	CTFontRef font = CreateFontFromNameWeightStyle( pchFontName, flFontSize, weight, style );
	
	m_cchText = V_wcslen(pwchText);
	size_t cbText = m_cchText * sizeof(wchar_t);
	
	CTTextAlignment alignment = kCTLeftTextAlignment;
	switch ( align )
	{
		case k_ETextAlignLeft:
			alignment = kCTLeftTextAlignment;
			break;
		case k_ETextAlignRight:
			alignment = kCTRightTextAlignment;
			break;
		case k_ETextAlignCenter:
			alignment = kCTCenterTextAlignment;
			break;
		default:
			break;
	}
	
	CTLineBreakMode lineBreak = bWrap ? kCTLineBreakByWordWrapping : kCTLineBreakByClipping;
	CGFloat flMaximumLineHeight = flLineHeight;
	if ( flLineHeight == k_flFloatNotSet )
		flMaximumLineHeight = 0.0f;
	
	CTParagraphStyleSetting settings[] = {
		{ kCTParagraphStyleSpecifierAlignment, sizeof(alignment), &alignment },
		{ kCTParagraphStyleSpecifierLineBreakMode, sizeof(lineBreak), &lineBreak },
		{ kCTParagraphStyleSpecifierMinimumLineHeight, sizeof(flMaximumLineHeight), &flMaximumLineHeight},
		{ kCTParagraphStyleSpecifierMaximumLineHeight, sizeof(flMaximumLineHeight), &flMaximumLineHeight}
	};
	
	CTParagraphStyleRef paragraphStyle = CTParagraphStyleCreate(settings, sizeof(settings) / sizeof(settings[0]));
	
    // Create an attributed string
    CFStringRef keys[] = { kCTFontAttributeName, kCTParagraphStyleAttributeName };
    CFTypeRef values[] = { font, paragraphStyle };
    CFDictionaryRef attr = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values,
                                              sizeof(keys) / sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	CFStringRef textRef = (CFStringRef) [[[NSString alloc] initWithBytes:pwchText length:cbText encoding:NSUTF32LittleEndianStringEncoding] autorelease];

	m_attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
	CFAttributedStringReplaceString ( m_attrString, CFRangeMake(0, 0), textRef);
	CFAttributedStringSetAttributes( m_attrString, CFRangeMake(0, CFAttributedStringGetLength(m_attrString)), attr, false );
 
	float flWeight = -1.0;
    CFNumberRef cfnWeight = CFNumberCreate( kCFAllocatorDefault, kCFNumberFloatType, &flWeight );
	CFAttributedStringSetAttribute( m_attrString, CFRangeMake(0, CFAttributedStringGetLength(m_attrString)), kCTStrokeWidthAttributeName, cfnWeight );
    CFRelease( cfnWeight );
	
    CFRelease( attr );
	CFRelease( paragraphStyle );
	CFRelease( font );

	[pool release];

	m_flMaxWidth = flMaxWidth;
	m_flMaxHeight = flMaxHeight;
	m_sFontName = pchFontName;
	m_bEllipseOnTruncate = bEllipsis;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: finds the index for a render format that starts at a certain position and is a color change
//-----------------------------------------------------------------------------
int FindNextTextRangeWithColorAndInRange( const ::google::protobuf::RepeatedPtrField< CMsgRenderTextRangeFormat > &rangeFormats, int iStartIndex, int iEndIndex, int iCurrent = -1 )
{
	iCurrent++;
	for ( int i = iCurrent; i < rangeFormats.size(); i++ )
	{
		const CMsgRenderTextFormat msgFormat = rangeFormats.Get( i ).format();
		if ( iStartIndex >= rangeFormats.Get( i ).start_index()  && iEndIndex <= rangeFormats.Get( i ).end_index()
			&& msgFormat.has_fill_brush_collection() && msgFormat.fill_brush_collection().fill_brush().size() > 0 )
			return i;
	}
	
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: helper to detect Lion and above
//-----------------------------------------------------------------------------
bool CUITextLayoutOSX::BIsOSX107OrAbove()
{
	static EOSType osType = k_eOSUnknown;
	if ( osType == k_eOSUnknown )
	{
		osType = GetOSType();
	}
	return osType >= k_eMacOS107;
}


//-----------------------------------------------------------------------------
// Purpose: helper to make a CTFrame for this object
//-----------------------------------------------------------------------------
CTFrameRef CUITextLayoutOSX::CreateFrameHelper( bool bUseMaxSize, float &flFrameHeight, CGSize *pFrameSize )
{
	if ( !m_ctfFrameSetter )
    {
        // work around what seems to be an apple ref counting bug around CTFramesetterCreateWithAttributedString 
        // and MutableAttributedStrings
        CFAttributedStringRef aStringRef = CFAttributedStringCreateCopy( kCFAllocatorDefault, m_attrString );
		m_ctfFrameSetter = CTFramesetterCreateWithAttributedString( aStringRef );
        CFRelease( aStringRef );
    }
	if ( !m_ctfFrameSetter )
		return NULL;
	
	CTFrameRef frame;
	CFRange range;
	
	CGSize layoutSize = CGSizeMake( CGFLOAT_MAX, BIsOSX107OrAbove() ?  CGFLOAT_MAX: k_flLargeFrameHeight );
	if ( m_flMaxWidth != k_flFloatNotSet && m_flMaxWidth > 0.0f )
		layoutSize.width = m_flMaxWidth;

	// only set a height if we want to wrap and have a max height set
	// otherwise leave it as max so we can emulate the win32 behavior of clipping of glyphs	
	if ( bUseMaxSize && m_bWrap && BIsOSX107OrAbove() && m_flMaxHeight != k_flFloatNotSet && m_flMaxHeight > 0.0f )
		layoutSize.height = m_flMaxHeight;
	
	CGSize textSize;
	textSize = CTFramesetterSuggestFrameSizeWithConstraints( m_ctfFrameSetter, CFRangeMake(0, 0), NULL, layoutSize, &range );

	if ( bUseMaxSize )
	{
		textSize.width = m_flMaxWidth;
		if ( m_bWrap )
			textSize.height = m_flMaxHeight;
	}
	
	flFrameHeight = BIsOSX107OrAbove() ? textSize.height : k_flLargeFrameHeight;

	if ( textSize.width <= 0.0f && range.length > 0 )
	{
		if ( m_flMaxWidth != k_flFloatNotSet && m_flMaxWidth > 0.0f )
			textSize.width = m_flMaxWidth;
		else
		{
			Msg( "got bad size and no width set\n" );
			return NULL;
		}
	}
	
	if ( textSize.height == 0 && range.length == 0 )
	{
		layoutSize.height = 1000.0f;

		textSize = CTFramesetterSuggestFrameSizeWithConstraints( m_ctfFrameSetter, CFRangeMake(0, 0), NULL, layoutSize, &range );
		if ( textSize.height == 0 && range.length == 0 )
		{
			textSize.height = m_flFontSize*1.2;
			textSize.width = m_flMaxWidth;
		}

		flFrameHeight = BIsOSX107OrAbove() ? textSize.height : k_flLargeFrameHeight;
	}
	
	CGMutablePathRef path = CGPathCreateMutable();
	// CTFramesetterSuggestFrameSizeWithConstraints under reports the required height so give extra room in the bounding path so it won't just fail to draw some lines
	CGRect frameBounds = CGRectMake( 0.0f, 0.0f, ceilf( textSize.width ), (flFrameHeight + m_flFontSize/2) );
	CGPathAddRect( path, NULL, frameBounds );
	flFrameHeight = frameBounds.size.height; // update the frame height for rendering so when we flip the co-ords we get all the glyph pixels
	
	// Create the frame and draw it into the graphics context
	frame = CTFramesetterCreateFrame( m_ctfFrameSetter, CFRangeMake(0, 0), path, NULL );
	
	CGPathRelease( path );

	if ( pFrameSize )
		*pFrameSize = textSize;
	return frame;
}


//-----------------------------------------------------------------------------
// Purpose: create opacity masks for our string
//-----------------------------------------------------------------------------
bool CUITextLayoutOSX::Draw( CUtlVector<GLTextOpacityMaskDataRange_t> &drawRanges, const google::protobuf::RepeatedPtrField<CMsgRenderTextRangeFormat > &rangeFormats, COpenGLSurface *pSurface )
{
	bool bDrawText = false;
	
	float flFrameHeight = 0.0f;
	CTFrameRef frame = CreateFrameHelper( true, flFrameHeight );
	if( frame != NULL )
	{
		// truncation token is a CTLineRef itself 
		CFMutableAttributedStringRef truncationString = CFAttributedStringCreateMutableCopy( NULL, 0, m_attrString );
		CFStringRef str = CFSTR("\u2026");
		CFAttributedStringReplaceString ( truncationString, CFRangeMake(0, CFAttributedStringGetLength(truncationString) ), str );
				
		CTLineRef truncationToken = CTLineCreateWithAttributedString(truncationString); 
		CFRelease(truncationString); 
		
		
		CTLineRef truncated = NULL;

		CGPoint textPosition = CGPointMake( 0, 0 );
		CFArrayRef lines = CTFrameGetLines( frame );
		for ( int iLine=0; iLine < CFArrayGetCount( lines ); iLine++ )
		{
			CGFloat  ascent, descent, leading;
			
			CGPoint lineOrigin;
			CTFrameGetLineOrigins( frame, CFRangeMake( iLine, 1 ), &lineOrigin );
			
			CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex( lines, iLine );
			
			if ( m_bEllipseOnTruncate && iLine == CFArrayGetCount( lines ) - 1 ) // see if we need to truncate the last line
			{
				// make a range of the whole rest of the string
				CFRange rng = CFRangeMake( CTLineGetStringRange(line).location, 0);
				rng.length = CFAttributedStringGetLength(m_attrString) - rng.location;
				
				// substring with that range
				CFAttributedStringRef longString = CFAttributedStringCreateWithSubstring(NULL, m_attrString, rng);
				// line for that string
				CTLineRef longLine = CTLineCreateWithAttributedString(longString);
				CFRelease(longString );
				
				truncated = CTLineCreateTruncatedLine( longLine, m_flMaxWidth, kCTLineTruncationEnd, truncationToken); 
				// if 'truncated' is NULL, then no truncation was required to fit it 
				if ( truncated != NULL )
				{
					line = truncated;
				}
				CFRelease( longLine );
			}
			
			CFArrayRef glyphRuns = CTLineGetGlyphRuns( line );
			float flLineWidth = CTLineGetTypographicBounds( line, &ascent, &descent, &leading );

			textPosition.x = lineOrigin.x;
			textPosition.y = lineOrigin.y;
			float flLineHeight = ( ascent + descent + leading );
			for ( int iGlyph = 0; iGlyph < CFArrayGetCount( glyphRuns ); iGlyph++ )
			{
				CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex( glyphRuns, iGlyph );
				float flRunWidth = CTRunGetTypographicBounds( run, CFRangeMake( 0, 0 ), &ascent, &descent, &leading );
				if ( flRunWidth == 0 )
					continue;
				
				// make the context to draw on
				CGContextRef context = CGBitmapContextCreate( NULL, ceil( flRunWidth ), ceil( flLineHeight ), 8, ceil( flRunWidth ), NULL, kCGImageAlphaOnly );
				
				CGContextSetAlpha(context, 1.0f);
				CGContextSetTextPosition(context, 0.0f, 0.0f );
				CGContextSetRGBStrokeColor( context, 1.0, 1.0, 1.0, 1.0 );
				CGContextSetTextDrawingMode( context, kCGTextFillStroke );
				CGContextSetTextMatrix( context, CGAffineTransformIdentity );
				CGContextSetAllowsFontSubpixelPositioning( context, true );
				CGContextSetShouldSubpixelPositionFonts( context, true );
				CGContextSetShouldSubpixelQuantizeFonts( context, true );
				CGContextSetAllowsFontSubpixelQuantization( context, true );
				CGContextSetAllowsAntialiasing( context, true );
				CGContextSetShouldAntialias( context, true );
				CGContextSetShouldSmoothFonts( context, true );

				CGContextClearRect(context, CGRectMake(0, 0, ceil( flRunWidth ), ceil( flLineHeight ) ));

				// find where on the frame page it wants to draw
				CGPoint position;
				CTRunGetPositions(run, CFRangeMake( 0, 1), &position);

				// move it in by the x offset so the first glyph draws at 0.0f in x
				CGContextSetTextPosition( context, -1.0 * position.x, descent );
				CTRunDraw( run, context, CFRangeMake( 0, 0 ) );
				
				bDrawText = true;
				
				CGRect cgRectRunBounds = CTRunGetImageBounds( run, context, CFRangeMake( 0, 0 ) );
				if ( cgRectRunBounds.size.width > 0 && cgRectRunBounds.size.height > 0 )
				{
					GLTextOpacityMaskDataRange_t &data = drawRanges[ drawRanges.AddToTail() ];
					data.m_flStringOffsetX = textPosition.x;
					// CG co-ords are inverse y, so textPosition.y is offset from the bottom edge (CG is 0,0 at bottom left, we are 0,0 at top left).
					// So, our height minus where this should draw its baseline (textpos.y) minus its room above the line (ascent + leading) should get 
					// us the offset from our top left to start the draw
					data.m_flStringOffsetY = (flFrameHeight - textPosition.y - ascent - leading);
					
					CFRange runRange = CTRunGetStringRange( run );
					data.m_iColorIndex = FindNextTextRangeWithColorAndInRange( rangeFormats, runRange.location, runRange.location + runRange.length - 1  ); // check if this should be a color change
					
					CGDataProviderRef dataProvider = CGDataProviderCreateWithData( NULL, CGBitmapContextGetData( context ), CGBitmapContextGetBytesPerRow(context) * CGBitmapContextGetHeight(context), NULL );
					
					int cxBitmap = CGBitmapContextGetWidth(context);
					int cyBitmap = CGBitmapContextGetHeight(context) ;
					
					COpenGLSurface::GetTextAlphaOnlyTargetResult_t alphaTarget = pSurface->GetTextAlphaOnlyTarget( cxBitmap, cyBitmap );
					
					float flOutWidth = cxBitmap;
					float flOutHeight = cyBitmap;
					
					pSurface->SetTexture( GL_TEXTURE0_ARB, alphaTarget.m_unRenderTarget );
					
					glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
					
					static CUtlBuffer bufZeroMem;
					CThreadMutex mutexZeroMem;
					
					GLint xOffsetGL = alphaTarget.m_rectTarget.left + alphaTarget.m_iPadding;
					GLint yOffsetGL = alphaTarget.m_rectTarget.top + alphaTarget.m_iPadding;
					
					// First zero the entire region
					int minDesired = MAX( 1024*24, alphaTarget.m_rectTarget.right - alphaTarget.m_rectTarget.left );
					if ( bufZeroMem.Size() < minDesired  )
					{
						AUTO_LOCK( mutexZeroMem );
						int nOldSize = bufZeroMem.Size();
						bufZeroMem.EnsureCapacity( minDesired );
						V_memset( (byte*)bufZeroMem.Base()+nOldSize, 0, bufZeroMem.Size()-nOldSize );
					}
					
					int yStart = alphaTarget.m_rectTarget.top;
					int yEnd = alphaTarget.m_rectTarget.bottom;
					int paddedWidth = alphaTarget.m_rectTarget.right - alphaTarget.m_rectTarget.left;
					int yPerPass = minDesired / paddedWidth;
					
					glPixelStorei( GL_UNPACK_ROW_LENGTH, paddedWidth );
					while ( yStart < yEnd )
					{
						int yToDo = MIN( yPerPass, yEnd - yStart );
						glTexSubImage2D( GL_TEXTURE_2D, 0, alphaTarget.m_rectTarget.left, yStart, paddedWidth, yToDo, GL_ALPHA, GL_UNSIGNED_BYTE, bufZeroMem.Base() );
						yStart += yToDo;
					}
					
					// now draw the glyph itself
					Assert( cxBitmap <= alphaTarget.m_rectTarget.right - alphaTarget.m_iPadding - alphaTarget.m_rectTarget.left );
					glPixelStorei( GL_UNPACK_ROW_LENGTH, cxBitmap );
					glTexSubImage2D( GL_TEXTURE_2D, 0, xOffsetGL, yOffsetGL, cxBitmap, cyBitmap, GL_ALPHA, GL_UNSIGNED_BYTE, CGBitmapContextGetData( context ) );
					glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

					CFRelease( dataProvider );
					
					data.m_x0 = alphaTarget.m_rectTarget.left;
					data.m_y0 = alphaTarget.m_rectTarget.top;

					data.m_textureId = alphaTarget.m_unRenderTarget;
					data.m_x0 += alphaTarget.m_iPadding;
					data.m_y0 += alphaTarget.m_iPadding;
					data.m_x1 = data.m_x0 + flOutWidth;
					data.m_y1 = data.m_y0 + flOutHeight;
					data.m_texWidth = alphaTarget.m_texWidth;
					data.m_texHeight = alphaTarget.m_texHeight;
					
				}
				CFRelease( context );
				textPosition.x += flRunWidth;
				
			} // each run
			
			if ( truncated )
				CFRelease( truncated );
			
		} // each line 
		
		CFRelease(frame);
		if ( truncationToken )
			CFRelease( truncationToken );
	}
	
	return bDrawText;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the required size to fully draw the text layout
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::GetRequiredSize( float &flWidth, float &flHeight )
{	
	VPROF_BUDGET( "CUITextLayoutOSX::GetRequiredSize", VPROF_BUDGETGROUP_TENFOOT );
	
	float flFrameHeight = 0.0f;
	CGSize frameSize;
	CTFrameRef frame = CreateFrameHelper( false, flFrameHeight, &frameSize );
	
	float flLineHeight = 0.0f;
	float flLineWidth = 0.0f;
	if( frame != NULL )
	{
		CGPoint textPosition = CGPointMake( 0, 0 );
		CFArrayRef lines = CTFrameGetLines( frame );
		for ( int iLine=0; iLine < CFArrayGetCount( lines ); iLine++ )
		{
			CGFloat  ascent, descent, leading;
			CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex( lines, iLine );
			float flThisLineWidth = CTLineGetTypographicBounds( line, &ascent, &descent, &leading );
			flLineWidth = MAX( flThisLineWidth, flLineWidth );
			flLineHeight += ( ascent + descent + leading );
		}

		CFRelease(frame);
	}
	

	if ( !BIsOSX107OrAbove() && flLineHeight > 0.0f )
	{
		flHeight = flLineHeight;	
	}
	else
	{
		flHeight = ceilf(frameSize.height);
	}

	flWidth = ceilf(frameSize.width);

			
}


//-----------------------------------------------------------------------------
// Purpose:  Hit tests a point against the text layout
//  
// unHitRunLength returns the character index hit
// bIsTrailingHit indicates whether the hit is on the leading or trailing side of the char
// bInside is true if the hit is within the text region, when false the position closest the point is returned as
// the character hit offset
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::HitTestPoint( Vector2D point, uint32 &unFirstHitOffset, bool &bIsTrailingHit, bool &bIsInsideString )
{
	bIsTrailingHit = false;
	bIsInsideString = false;
	unFirstHitOffset = 0;

	float flFrameHeight = 0.0f;
	CGSize frameBounds;
	CTFrameRef frame = CreateFrameHelper( true, flFrameHeight, &frameBounds );
	if( frame == NULL )
		return;
	
	CFArrayRef lines = CTFrameGetLines( frame );
	CGPoint origins[CFArrayGetCount(lines)];
	
	CTFrameGetLineOrigins( frame, CFRangeMake(0, 0), origins);

	CGPoint cgpoint;
	cgpoint = CGPointMake( point.x, point.y );

	CTLineRef line = NULL;
	CGPoint lineOrigin = CGPointZero;
	for (int i= CFArrayGetCount(lines)-1; i >= 0; i--)
	{
		CGPoint origin = origins[i];
		CGPathRef path = CTFrameGetPath(frame);
		CGRect rect = CGPathGetBoundingBox(path);
		
		CGFloat y = frameBounds.height - rect.origin.y - origin.y;
        
		if ( point.y <= y )
		{
			line = (CTLineRef)CFArrayGetValueAtIndex(lines, i);
			lineOrigin = origin;
		}
	}
	
	if ( line )
	{
		bIsInsideString = true;
		
		point.x -= lineOrigin.x;
		
		CGPoint location;
		location = CGPointMake( point.x, point.y );
		CFIndex index = CTLineGetStringIndexForPosition( line, location );

		unFirstHitOffset = index;
	
		CGFloat flCharOffset = CTLineGetOffsetForStringIndex( line, index, NULL );
		if ( point.x > flCharOffset )
			bIsTrailingHit = true;
	}
	
	CFRelease(frame);

}


//-----------------------------------------------------------------------------
// Purpose: helper to determine char hit rects for a range in a string
//-----------------------------------------------------------------------------
bool CUITextLayoutOSX::CharacterCoordinatesHelper( uint32 unCharIndexStart, uint32 unCharIndexEnd, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects )
{
	float flFrameHeight = 0.0f;
	CTFrameRef frame = CreateFrameHelper( true, flFrameHeight );
	if( frame == NULL )
		return false;
		
	CFArrayRef lines = CTFrameGetLines( frame );
	uint32 unCharIndexCurrent = MIN( unCharIndexStart, unCharIndexEnd );
	unCharIndexEnd = MAX( unCharIndexStart, unCharIndexEnd );
	for (int iLine= 0; iLine < CFArrayGetCount(lines); iLine++)
	{
		if ( unCharIndexCurrent > unCharIndexEnd )
			break;
		
		CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex( lines, iLine );	
		if ( line == 0 )
			continue;
			
		CFRange charRange = CTLineGetStringRange( line );
		if ( charRange.location <= unCharIndexCurrent && charRange.location+charRange.length >= unCharIndexCurrent )
		{
			CGFloat  ascent, descent, leading;
			
			CGPoint lineOrigin;
			CTFrameGetLineOrigins( frame, CFRangeMake( iLine, 1 ), &lineOrigin );
			
			CGPathRef path = CTFrameGetPath(frame);
			CGRect frameBoundingBox = CGPathGetBoundingBox(path);
			
			uint32 unCharIndexEndThisRange = MIN( unCharIndexEnd + 1, charRange.location+charRange.length );

			float flLineWidth = CTLineGetTypographicBounds( line, &ascent, &descent, &leading );
			float flLineHeight = ( ascent + descent + leading );
			
			float flLeftY = ( frameBoundingBox.size.height - flLineHeight ) -  lineOrigin.y + descent;							
			float flRightY = flLeftY + flLineHeight;							
			float flLeftX = CTLineGetOffsetForStringIndex( line, unCharIndexCurrent, NULL );
			float flRightX = CTLineGetOffsetForStringIndex( line, unCharIndexEndThisRange, NULL );			
		
			IUITextLayout::HitTestRegionRect_t &charRegionRect = vecRangeRegionRects[ vecRangeRegionRects.AddToTail() ];
			charRegionRect.topLeft.x = flLeftX;
			charRegionRect.topLeft.y = flLeftY;
			charRegionRect.bottomRight.x = flRightX;
			charRegionRect.bottomRight.y = flRightY; 
			
			charRegionRect.unCharStart = unCharIndexCurrent; 
			charRegionRect.unCharEnd = unCharIndexEndThisRange; 
			charRegionRect.bIsText = true;
			
			unCharIndexCurrent = unCharIndexEndThisRange;
		}												
	}

    CFRelease(frame);
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Determines the layout coordinates for a given character offset, 
// coordinates are relative to top left of text layout
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::GetCharacterCoordinates( uint32 unCharIndex, IUITextLayout::HitTestRegionRect_t &charRegionRect )
{
	CUtlVector<IUITextLayout::HitTestRegionRect_t> vecHits;
	if ( CharacterCoordinatesHelper( unCharIndex, unCharIndex, vecHits ) && vecHits.Count() >= 1 )	
	{
		charRegionRect.topLeft.x = vecHits[0].topLeft.x;
		charRegionRect.topLeft.y =vecHits[0].topLeft.y;
		charRegionRect.bottomRight.x = vecHits[0].bottomRight.x;
		charRegionRect.bottomRight.y = vecHits[0].bottomRight.y;
	
		charRegionRect.unCharStart = unCharIndex;
		charRegionRect.unCharEnd = unCharIndex;
		charRegionRect.bIsText = vecHits[0].bIsText;
		charRegionRect.bIsTrimmed = vecHits[0].bIsTrimmed;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines a vector of rects enclosing a range of text, normally 
// used for getting selection highlight regions
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::GetCharacterRangeCoordinates( uint32 unCharStartIndex, uint32 unCharEndIndex, CUtlVector<IUITextLayout::HitTestRegionRect_t> &vecRangeRegionRects ) 
{
	vecRangeRegionRects.RemoveAll();

	CharacterCoordinatesHelper( unCharStartIndex, unCharEndIndex, vecRangeRegionRects );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font name for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::SetFontName( uint32 unCharStartIndex, uint32 unCharEndIndex, const char *pchFontName )
{
	m_sFontName = pchFontName;
	CTFontRef font = CreateFontFromNameWeightStyle( m_sFontName, m_flFontSize, m_Weight, m_Style );
	if ( font )
	{
		CFAttributedStringSetAttribute( m_attrString, CFRangeMake( unCharStartIndex, unCharEndIndex - unCharStartIndex + 1 ), kCTFontAttributeName, font );
		CFRelease( font );
	}
	
	if ( m_ctfFrameSetter )
		CFRelease( m_ctfFrameSetter );
	m_ctfFrameSetter = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font size for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::SetFontSize( uint32 unCharStartIndex, uint32 unCharEndIndex, float flFontSize )
{
	m_flFontSize = flFontSize * DIP_TO_POINT;
	SetFontName( unCharStartIndex, unCharEndIndex, m_sFontName );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font style for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::SetFontStyle( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontStyle eFontStyle )
{
	m_Style = eFontStyle;
	CTFontRef font = CreateFontFromNameWeightStyle( m_sFontName, m_flFontSize, m_Weight, m_Style );
	if ( font )
	{
		CFAttributedStringSetAttribute( m_attrString, CFRangeMake( unCharStartIndex, unCharEndIndex - unCharStartIndex + 1 ), kCTFontAttributeName, font );
		CFRelease( font );		
	}
	
	if ( m_ctfFrameSetter )
		CFRelease( m_ctfFrameSetter );
	m_ctfFrameSetter = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the font weight for a specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::SetFontWeight( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontWeight eFontWeight )
{
	m_Weight = eFontWeight;
	
	CTFontRef font = CreateFontFromNameWeightStyle( m_sFontName, m_flFontSize, m_Weight, m_Style );
	if ( font )
	{
		CFAttributedStringSetAttribute( m_attrString, CFRangeMake( unCharStartIndex, unCharEndIndex - unCharStartIndex + 1 ), kCTFontAttributeName, font );
		CFRelease( font );
	}
	
	if ( m_ctfFrameSetter )
		CFRelease( m_ctfFrameSetter );
	m_ctfFrameSetter = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Underlines the specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::SetUnderline( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bUnderline )
{	
	if ( m_attrString )
		CFAttributedStringSetAttribute( m_attrString, CFRangeMake( unCharStartIndex, unCharEndIndex - unCharStartIndex + 1 ), kCTUnderlineStyleAttributeName, (CFNumberRef)[NSNumber numberWithInt:bUnderline ? kCTUnderlineStyleSingle : kCTUnderlineStyleNone] );
	
	if ( m_ctfFrameSetter )
		CFRelease( m_ctfFrameSetter );
	m_ctfFrameSetter = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets strikethrough on the specified character range
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::SetStrikethrough( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bStrikethrough )
{
	if ( m_attrString )
    {
        CFNumberRef cfnStrike = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &bStrikethrough );
		CFAttributedStringSetAttribute( m_attrString, CFRangeMake( unCharStartIndex, unCharEndIndex - unCharStartIndex + 1 ), CFSTR( "DTCustomStrikeOut" ), cfnStrike );
        CFRelease( cfnStrike );
    }
	
	if ( m_ctfFrameSetter )
		CFRelease( m_ctfFrameSetter );
	m_ctfFrameSetter = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Marks a range of text needing a color. This will affect measurement
//-----------------------------------------------------------------------------
void CUITextLayoutOSX::MarkColorRangeForMeasurement( uint32 unCharStartIndex, uint32 unCharEndIndex )
{
	// Create a color and add it as an attribute to the string.
	CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
	CGFloat components[] = { 1.0 * unCharStartIndex/m_cchText, 1.0 * unCharEndIndex/m_cchText, 0.0, 1.0 };
	CGColorRef color = CGColorCreate(rgbColorSpace, components);
	CGColorSpaceRelease(rgbColorSpace);
	if ( m_attrString )
		CFAttributedStringSetAttribute( m_attrString, CFRangeMake( unCharStartIndex, unCharEndIndex - unCharStartIndex + 1 ), kCTForegroundColorAttributeName, color );
	
	CFRelease( color );
	if ( m_ctfFrameSetter )
		CFRelease( m_ctfFrameSetter );
	m_ctfFrameSetter = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Register a custom font collection
//-----------------------------------------------------------------------------
bool CUITextLayoutOSX::BLoadCustomFontCollection( const char *pchPathForCustomFonts )
{
	return CUIFontLoaderOSX::GetInstance().RegisterDir( pchPathForCustomFonts );
}


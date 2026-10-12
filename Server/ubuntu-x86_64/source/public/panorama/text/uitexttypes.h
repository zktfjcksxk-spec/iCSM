//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITEXTTYPES_H
#define UITEXTTYPES_H
#pragma once

#include "language.h"
#include "tier1/utlptrarray.h"

#define TEXT_DIMENSION_TOLERANCE 0.001f

namespace panorama
{

// Describes how to interpret raw text bytes.
enum EPanoramaTextEncoding : uint8
{
	k_EPanoramaTextEncodingNone    = 0,
	k_EPanoramaTextEncodingUTF8    = 1,
	k_EPanoramaTextEncodingUChar16 = 2,
	k_EPanoramaTextEncodingUChar32 = 3,
};

// wchar_t is UTF-16 on Windows but UTF-32 on Linux and Mac.
#ifdef _WIN32
#define k_EPanoramaTextEncodingWChar k_EPanoramaTextEncodingUChar16
#else
#define k_EPanoramaTextEncodingWChar k_EPanoramaTextEncodingUChar32
#endif

typedef void *UITextTextureHandle_t;

struct UITextRect
{
	int32 m_iLeft;
	int32 m_iTop;
	int32 m_iRight;
	int32 m_iBottom;
};

struct UITextRectFloat32
{
	float m_flLeft;
	float m_flTop;
	float m_flRight;
	float m_flBottom;
};

// A 2D region within the given texture.
struct UITextTextureRegion_t
{
	UITextTextureHandle_t m_hTexture;
	UITextRect m_rect;
	// Text layouts that do atlasing, such as Pango and OSX, add
	// padding space around regions to give a black border for safe blending.
	// This padding will be included in the result rectangle.
	int32 m_iPadding;
	float m_flTextureWidth;
	float m_flTextureHeight;
};

// Contains all info related to a span of text w/ alpha mask rendered to a texture
struct UITextOpacityMaskDataRange_t
{
	static const int k_iColorIndexUnset = -1;

	UITextTextureHandle_t m_hTexture;
	float m_flTextureWidth;
	float m_flTextureHeight;

	float m_x0;						// Run relative
	float m_y0;
	float m_x1;
	float m_y1;
	float m_flStringOffsetX;		// Start X pos relative to top left of complete rendered text
	float m_flStringOffsetY;		// Start Y pos relative to top left of complete rendered text
	int m_iColorIndex;				// Index of color from provided per-range color indices. -1 if unset
};

// Structure to keep track of cached text already drawn into an opacity mask texture
struct UITextOpacityMaskData_t
{
	UITextOpacityMaskDataRange_t *m_pRangeData;
	int m_cRangeData;
	int m_nUniqueId;
};

struct UITextFormatProperties_t
{
	static const int k_iColorIndexUnset = -1;

	uint32 m_unCharStartIndex;
	uint32 m_unCharEndIndex;
	uint32 m_unCRCFontName;
	float m_flFontSize;
	EFontWeight m_eWeight;
	EFontStyle m_eStyle;
	bool m_bUnderline;
	bool m_bStrikethrough;
	int m_iColorIndex;				// index of color into provided brushes. -1 if unset
	float m_flInlineObjectWidth;
	float m_flInlineObjectHeight;
	int m_iLetterSpacing;

	int Compare( const UITextFormatProperties_t &rhs ) const
	{
		if ( m_unCharStartIndex != rhs.m_unCharStartIndex )
			return (m_unCharStartIndex < rhs.m_unCharStartIndex) ? -1 : 1;

		if ( m_unCharEndIndex != rhs.m_unCharEndIndex )
			return (m_unCharEndIndex < rhs.m_unCharEndIndex) ? -1 : 1;

		if ( m_unCRCFontName != rhs.m_unCRCFontName )
			return (m_unCRCFontName < rhs.m_unCRCFontName) ? -1 : 1;

		if( (rhs.m_flFontSize - m_flFontSize) > TEXT_DIMENSION_TOLERANCE )
			return -1;
		else if( (m_flFontSize - rhs.m_flFontSize) > TEXT_DIMENSION_TOLERANCE )
			return 1;

		if ( m_eWeight != rhs.m_eWeight )
			return (m_eWeight < rhs.m_eWeight) ? -1 : 1;

		if ( m_eStyle != rhs.m_eStyle )
			return (m_eStyle < rhs.m_eStyle) ? -1 : 1;

		if ( m_bUnderline != rhs.m_bUnderline )
			return (m_bUnderline < rhs.m_bUnderline) ? -1 : 1;

		if ( m_bStrikethrough != rhs.m_bStrikethrough )
			return (m_bStrikethrough < rhs.m_bStrikethrough) ? -1 : 1;

		if ( m_iColorIndex != rhs.m_iColorIndex )
			return (m_iColorIndex < rhs.m_iColorIndex) ? -1 : 1;

		if( (rhs.m_flInlineObjectWidth - m_flInlineObjectWidth) > TEXT_DIMENSION_TOLERANCE )
			return -1;
		else if( (m_flInlineObjectWidth - rhs.m_flInlineObjectWidth) > TEXT_DIMENSION_TOLERANCE )
			return 1;

		if( (rhs.m_flInlineObjectHeight - m_flInlineObjectHeight) > TEXT_DIMENSION_TOLERANCE )
			return -1;
		else if( (m_flInlineObjectHeight - rhs.m_flInlineObjectHeight) > TEXT_DIMENSION_TOLERANCE )
			return 1;

		if ( m_iLetterSpacing != rhs.m_iLetterSpacing )
			return (m_iLetterSpacing < rhs.m_iLetterSpacing) ? -1 : 1;

		return 0;
	}
};

struct UITextLayoutProperties_t
{
	ELanguage m_language;

	float m_flWidth;
	float m_flHeight;
	uint32 m_unCRCString;
	byte m_rgFirstEightBytesString[8];

	float m_flLineHeight;
	ETextAlign m_Align;
	bool m_bEllipsis;
	bool m_bWrap;

	UITextFormatProperties_t m_defaultFormat;
	CUtlPtrArray< UITextFormatProperties_t > m_arrayFormats;

    UITextLayoutProperties_t& operator=( const UITextLayoutProperties_t &src )
    {
        m_language = src.m_language;
        m_flWidth = src.m_flWidth;
        m_flHeight = src.m_flHeight;
        m_unCRCString = src.m_unCRCString;
        memcpy( m_rgFirstEightBytesString, src.m_rgFirstEightBytesString, sizeof(m_rgFirstEightBytesString) );
        m_flLineHeight = src.m_flLineHeight;
        m_Align = src.m_Align;
        m_bEllipsis = src.m_bEllipsis;
        m_bWrap = src.m_bWrap;
        m_defaultFormat = src.m_defaultFormat;
        if ( src.m_arrayFormats.Count() > 0 )
        {
            m_arrayFormats.Copy( &src.m_arrayFormats[0], src.m_arrayFormats.Count() );
        }
        else
        {
            m_arrayFormats.Purge();
        }
        return *this;
    }

	bool operator <( const UITextLayoutProperties_t &l ) const
	{
		if ( m_unCRCString < l.m_unCRCString )
			return true;
		else if ( m_unCRCString > l.m_unCRCString )
			return false;

		if( ( l.m_flLineHeight - m_flLineHeight ) > TEXT_DIMENSION_TOLERANCE )
			return true;
		else if( ( m_flLineHeight - l.m_flLineHeight ) > TEXT_DIMENSION_TOLERANCE )
			return false;

		if ( m_Align < l.m_Align )
			return true;
		else if ( m_Align > l.m_Align )
			return false;

		if ( m_bEllipsis != l.m_bEllipsis )
			return !m_bEllipsis;

		if ( m_bWrap != l.m_bWrap )
			return !m_bWrap;

		if ( m_language < l.m_language )
			return true;
		else if ( m_language > l.m_language )
			return false;
			
		if( (l.m_flWidth - m_flWidth) > TEXT_DIMENSION_TOLERANCE )
			return true;
		else if( (m_flWidth - l.m_flWidth) > TEXT_DIMENSION_TOLERANCE )
			return false;
 
		if( (l.m_flHeight - m_flHeight) > TEXT_DIMENSION_TOLERANCE )
			return true;
		else if( (m_flHeight - l.m_flHeight) > TEXT_DIMENSION_TOLERANCE )
			return false;

		for ( int i = 0; i < V_ARRAYSIZE( m_rgFirstEightBytesString ); ++i )
		{
			if ( m_rgFirstEightBytesString[i] < l.m_rgFirstEightBytesString[i] )
				return true;
			else if ( m_rgFirstEightBytesString[i] > l.m_rgFirstEightBytesString[i] )
				return false;
		}

		int nCmpDefault = m_defaultFormat.Compare( l.m_defaultFormat );
		if( nCmpDefault != 0 )
			return nCmpDefault < 0;

		if ( m_arrayFormats.Count() != l.m_arrayFormats.Count() )
			return m_arrayFormats.Count() < l.m_arrayFormats.Count();

		FOR_EACH_PTR_ARRAY( m_arrayFormats, i )
		{
			int nCmp = m_arrayFormats[i].Compare( l.m_arrayFormats[i] );
			if ( nCmp == 0 )
				continue;

			return (nCmp < 0);
		}
		return false;
	}
};

// Aggregate font metrics.  A layout may use multiple fonts so
// these values may be a composite of everything in the layout
// or representative of only some font mapping situations.
struct UITextLayoutFontMetrics_t
{
	float m_flAscent;
	float m_flDescent;
	float m_flLineHeight;
	// Only valid if m_bHaveCharMetrics is true.
	float m_flMaxCharWidth;
	// Fixed or monospace font.
	// Only valid if m_bHaveCharMetrics is true.
	bool m_bFixedCharWidth;
	// Extra metrics are available, such as information in this
	// struct, ABC widths and kerning.
	bool m_bHaveCharMetrics;
};

//
// Interface that needs to be implemented by callers to provide
// storage space for text bitmap data.
//
class IUITextTextureStorage
{
public:
	virtual ~IUITextTextureStorage() {}

	virtual uint32 GetMaximumTextureWidth() = 0;
	virtual uint32 GetMaximumTextureHeight() = 0;

	virtual UITextTextureRegion_t GetTextureRegion( int32 iWidth, int32 iHeight ) = 0;

	virtual void StartUpdateFontGlyphTexture( UITextTextureHandle_t hTexture ) = 0;
	virtual void UpdateFontGlyphTexture( UITextTextureHandle_t hTexture, int xOffset, int yOffset, int width, int height, void *pSourceData ) = 0;
	virtual void EndUpdateFontGlyphTexture( UITextTextureHandle_t hTexture ) = 0;
};

//
// Interface that provides lowest-level texture creation services.
//
class IUITextTextureProvider
{
public:
	virtual ~IUITextTextureProvider() {}

	// Creates a new 8-bit alpha-only texture.
	virtual UITextTextureHandle_t AllocAlphaTexture( int32 iWidth, int32 iHeight ) = 0;
	virtual void FreeTexture( UITextTextureHandle_t hTexture ) = 0;
};

//
// Interface that provides 2D region caching services for text storage.
//
class IUITextTextureCache
{
public:
	virtual ~IUITextTextureCache() {}

	virtual UITextTextureRegion_t GetTextureRegion( int32 iWidth, int32 iHeight ) = 0;
	virtual void FreeTextureRegion( const UITextTextureRegion_t &region ) = 0;

	// Forgets all cache content but does not call FreeTexture.
	virtual void Clear() = 0;
	// Calls FreeTexture for every texture in use.
	virtual void Purge() = 0;

	virtual int GetNumTexturesInUse() const = 0;
};

//
// Interface that provides opacity mask caching services for drawn text layouts.
//
class IUITextLayoutDrawCache
{
public:
	virtual ~IUITextLayoutDrawCache() {}

	virtual void InitTextLayoutProperties( UITextLayoutProperties_t *pKey, const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, ELanguage language ){}
	virtual UITextLayoutProperties_t *AllocTextLayoutProperties( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, ELanguage language ) = 0;
	virtual void FreeTextLayoutProperties( UITextLayoutProperties_t *pProps ) = 0;

	virtual UITextOpacityMaskData_t *GetTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, UITextLayoutProperties_t *pProps, const char **pchRangeFontNames, float flUsageTime, void *pRenderContext ) = 0;
	virtual void GetTextOpacityMaskAsync( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, UITextLayoutProperties_t *pProps, const char **pchRangeFontNames, float flUsageTime ) = 0;

	// If the specified text layout properties exists, sets its timestamp and
	// moves it to the end of the LRU list
	virtual void Touch( UITextLayoutProperties_t *pProps, float flUsageTime ) = 0;

	// Only returns the oldest entry if it is older than the given
	// limit time.  A negative limit time means no limit check is made.
	virtual UITextOpacityMaskData_t *GetOldestEntry( float flLimitTime ) = 0;
	virtual void DeleteOldestEntry() = 0;

	// Helper routine for code also using a texture cache.  Finds
	// all entries older than the limit, frees their texture region
	// in the given cache and deletes them from the draw cache.
	virtual void DeleteOlderEntriesToTextureCache( float flLimitTime, IUITextTextureCache *pTextureCache ) = 0;
	
	virtual void Clear() = 0;
};

} // namespace panorama

#endif // UITEXTTYPES_H

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "textlayoutdrawcache.h"
#include "jobthread.h"

using namespace panorama;

static CThreadMutex s_drawCacheMutex;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextLayoutDrawCache::CTextLayoutDrawCache() :
	m_PoolTextLayoutProperties( 64 ),
	m_mapCacheEntries( 64 ),
	m_listCacheEntries( 64 )
{
	m_pTextServices = NULL;
	m_pTextureStorage = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextLayoutDrawCache::~CTextLayoutDrawCache()
{
	// Ranges have allocated data in them that need to be explicitly deleted.
	Clear();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the text services callback
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::SetTextServices( IUITextServices *pServices )
{
	// We don't support switching callbacks.
	Assert( m_pTextServices == NULL );

	m_pTextServices = pServices;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the texture storage callback
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::SetTextureStorage( IUITextTextureStorage *pStorage )
{
	// We don't support switching callbacks.
	Assert( m_pTextureStorage == NULL );

	m_pTextureStorage = pStorage;
}


//-----------------------------------------------------------------------------
// Purpose: Allocate a new text layout properties instance
//-----------------------------------------------------------------------------
UITextLayoutProperties_t *CTextLayoutDrawCache::AllocTextLayoutProperties( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, ELanguage language )
{
	VPROF_BUDGET_THREAD( "GetCachedTextOpacityMask - Key Creation", VPROF_BUDGETGROUP_TENFOOT );

	UITextLayoutProperties_t *pProps = m_PoolTextLayoutProperties.Alloc();
	InitTextLayoutProperties( pProps, pRawText, cbRawText, cTextChars, eTextEncoding, x0, y0, x1, y1, flLineHeight, align, bWrap, bEllipsis, language );
	return pProps;
}

//-----------------------------------------------------------------------------
// Purpose: Init a new text layout properties instance
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::InitTextLayoutProperties( UITextLayoutProperties_t *pProps, const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, ELanguage language )
{
	VPROF_BUDGET_THREAD( "GetCachedTextOpacityMask - Key Init", VPROF_BUDGETGROUP_TENFOOT );

	pProps->m_language = language;
	pProps->m_flWidth = x1 - x0;
	pProps->m_flHeight = y1 - y0;
	pProps->m_flLineHeight = flLineHeight;
	pProps->m_Align = align;
	pProps->m_bEllipsis = bEllipsis;
	pProps->m_bWrap = bWrap;

	if ( cbRawText > sizeof( pProps->m_rgFirstEightBytesString ) )
	{
		V_memcpy( pProps->m_rgFirstEightBytesString, pRawText, sizeof( pProps->m_rgFirstEightBytesString ) );
	}
	else
	{
		V_memset( pProps->m_rgFirstEightBytesString, 0, sizeof( pProps->m_rgFirstEightBytesString ) );
		V_memcpy( pProps->m_rgFirstEightBytesString, pRawText, cbRawText );
	}

	pProps->m_unCRCString = CRC32_ProcessSingleBuffer( pRawText, cbRawText );
}


//-----------------------------------------------------------------------------
// Purpose: Frees a text layout properties instance
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::FreeTextLayoutProperties( UITextLayoutProperties_t *pProps )
{
	m_PoolTextLayoutProperties.Free( pProps );
}

//-----------------------------------------------------------------------------
// Purpose: Gets cache entry if already exists in the cache
//-----------------------------------------------------------------------------
CTextLayoutDrawCache::CacheEntry_t* CTextLayoutDrawCache::LookupCacheEntry( UITextLayoutProperties_t *pProps )
{
	AUTO_LOCK( s_drawCacheMutex );

	int iMap = m_mapCacheEntries.Find( pProps );
	if( iMap != m_mapCacheEntries.InvalidIndex() )
	{
		return m_mapCacheEntries[iMap];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Adds new entry to cache
//-----------------------------------------------------------------------------
CTextLayoutDrawCache::CacheEntry_t* CTextLayoutDrawCache::AddCacheEntry( UITextLayoutProperties_t *pProps, float flUsageTime )
{
	AUTO_LOCK( s_drawCacheMutex );

	static int uniqueID = 1;

	// Make a copy of the properties for the map to hold.
	UITextLayoutProperties_t *pMapProps = m_PoolTextLayoutProperties.Alloc();
	*pMapProps = *pProps;

	// Add item to cache index
	int ilistIndex = m_listCacheEntries.AddToTail();
	CacheEntry_t* pCacheEntry = &m_listCacheEntries[ilistIndex];
	int iMap = m_mapCacheEntries.Insert( pMapProps, pCacheEntry );
    UITextOpacityMaskData_t& data = pCacheEntry->m_textOpacityMaskData;

	data.m_pRangeData = NULL;
	data.m_cRangeData = 0;
	data.m_nUniqueId = uniqueID++;
	pCacheEntry->m_iList = ilistIndex;
	pCacheEntry->m_iMap = iMap;
	pCacheEntry->m_flLastUseTime = flUsageTime;
	pCacheEntry->m_pPendingDrawJob = NULL;

	return pCacheEntry;
}

//-----------------------------------------------------------------------------
// Purpose: Updates LRU timestamp and moves it to the end of the LRU list
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::UpdateLruPosition( CacheEntry_t *pCacheEntry, float flUsageTime )
{
	AUTO_LOCK( s_drawCacheMutex );

	pCacheEntry->m_flLastUseTime = flUsageTime;
	m_listCacheEntries.LinkToTail( pCacheEntry->m_iList );
	pCacheEntry->m_iList = m_listCacheEntries.Tail();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the texture storage callback
//-----------------------------------------------------------------------------
UITextOpacityMaskData_t *CTextLayoutDrawCache::GetTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, UITextLayoutProperties_t *pProps, const char **pchRangeFontNames, float flUsageTime, void *pRenderContext )
{
	CacheEntry_t *pCacheEntry = NULL;
	bool bCacheEntryFound = false;

	{
		// Only need mutex protection round the lookup and add.
		// This runs on the render thread, and deletion from the cache only occurs on the render thread,
		// (either from a call to Clear() in BeginFrame, or to DeleteOlderEntriesToTextureCache() in EndFrame).
		// So once the entry has been added we can safely edit it without mutex protection.
		// Just need to protect against the very unlikely possibility of a call to GetTextOpacityMaskAsync() 
		// adding a duplicate entry into the cache in between the lookup and add.
		AUTO_LOCK( s_drawCacheMutex );
		pCacheEntry = LookupCacheEntry( pProps );
		if( pCacheEntry )
		{
			bCacheEntryFound = true;
		}
		else
		{
			pCacheEntry = AddCacheEntry( pProps, flUsageTime );
		}
	}


	if( bCacheEntryFound )
	{
		VPROF_BUDGET_THREAD( "CTextLayoutDrawCache::GetTextOpacityMask - hit", VPROF_BUDGETGROUP_TENFOOT );

		UpdateLruPosition( pCacheEntry, flUsageTime );

		if( pCacheEntry->m_pPendingDrawJob )
		{
			// Block until drawing job completes
			m_pTextServices->BDrawComplete( pCacheEntry->m_pPendingDrawJob, pCacheEntry->m_textOpacityMaskData, m_pTextureStorage, true );
			pCacheEntry->m_pPendingDrawJob->Release();
			pCacheEntry->m_pPendingDrawJob = NULL;
		}
	}
	else
	{
		VPROF_BUDGET_THREAD( "CTextLayoutDrawCache::GetTextOpacityMask - miss", VPROF_BUDGETGROUP_TENFOOT );

		const UITextFormatProperties_t &defaultFormat = pProps->m_defaultFormat;

		IUITextServices::TextLayoutParams_t params( pProps->m_language, pchFontName, pProps->m_defaultFormat.m_flFontSize, pProps->m_flLineHeight, pProps->m_flWidth, pProps->m_flHeight );
		params.m_weight = defaultFormat.m_eWeight;
		params.m_style = defaultFormat.m_eStyle;
		params.m_align = pProps->m_Align;
		params.m_bWrap = pProps->m_bWrap;
		params.m_bEllipsis = pProps->m_bEllipsis;
		params.m_nLetterSpacing = defaultFormat.m_iLetterSpacing;
		IUITextLayout *pTextLayout = m_pTextServices->CreateTextLayout( pRawText, cbRawText, cTextChars, eTextEncoding, &params );

		Assert( pTextLayout );
		if ( pTextLayout )
		{
			// set any special defaults
			if ( defaultFormat.m_bUnderline )
				pTextLayout->SetUnderline( 0, cTextChars - 1, true );
			if ( defaultFormat.m_bStrikethrough )
				pTextLayout->SetStrikethrough( 0, cTextChars - 1, true );

			// add in all range formats
			for ( int i = 0; (uint)i < pProps->m_arrayFormats.Count(); i++ )
			{
				const UITextFormatProperties_t &rangeFormat = pProps->m_arrayFormats[i];
				uint32 unStartIndex = rangeFormat.m_unCharStartIndex;
				uint32 unEndIndex = rangeFormat.m_unCharEndIndex;

				if ( pchRangeFontNames && pchRangeFontNames[i] )
					pTextLayout->SetFontName( unStartIndex, unEndIndex, pchRangeFontNames[i] );

				if ( rangeFormat.m_flFontSize != defaultFormat.m_flFontSize )
					pTextLayout->SetFontSize( unStartIndex, unEndIndex, rangeFormat.m_flFontSize );

				if ( rangeFormat.m_eStyle != defaultFormat.m_eStyle )
					pTextLayout->SetFontStyle( unStartIndex, unEndIndex, rangeFormat.m_eStyle );

				if ( rangeFormat.m_eWeight != defaultFormat.m_eWeight )
					pTextLayout->SetFontWeight( unStartIndex, unEndIndex, rangeFormat.m_eWeight );

				if ( rangeFormat.m_bUnderline != defaultFormat.m_bUnderline )
					pTextLayout->SetUnderline( unStartIndex, unEndIndex, rangeFormat.m_bUnderline );

				if ( rangeFormat.m_bStrikethrough != defaultFormat.m_bStrikethrough )
					pTextLayout->SetStrikethrough( unStartIndex, unEndIndex, rangeFormat.m_bStrikethrough );

				if ( rangeFormat.m_flInlineObjectWidth > 0.0f || rangeFormat.m_flInlineObjectHeight > 0.0f )
					pTextLayout->SetInlineObject( unStartIndex, rangeFormat.m_flInlineObjectWidth, rangeFormat.m_flInlineObjectHeight );
			}

			// before we make any measurements, we need to add drawing effects. Adding an effect can cause DWrite to slightly modify
			// the dimensions of a glyph run.
			AddColorDrawEffects( pTextLayout, pProps->m_arrayFormats.Base(), pProps->m_arrayFormats.Count(), cTextChars );

			// Generate text immediately
			CUtlVector< UITextOpacityMaskDataRange_t > vecTextRanges;
			if( pTextLayout->BDraw( vecTextRanges, pProps->m_arrayFormats.Base(), pProps->m_arrayFormats.Count(), m_pTextureStorage, pProps->m_flHeight, pRenderContext ) )
			{
				pCacheEntry->m_textOpacityMaskData.m_pRangeData = new UITextOpacityMaskDataRange_t[vecTextRanges.Count()];
				pCacheEntry->m_textOpacityMaskData.m_cRangeData = vecTextRanges.Count();
				V_memcpy( pCacheEntry->m_textOpacityMaskData.m_pRangeData, vecTextRanges.Base(), sizeof( UITextOpacityMaskDataRange_t ) * vecTextRanges.Count() );
			}
			m_pTextServices->FreeTextLayout( pTextLayout );
		}
	}

	return &pCacheEntry->m_textOpacityMaskData;
}

//-----------------------------------------------------------------------------
// Purpose: Kick off async job to add text region to cache if not already there.
//          Called from main thread PaintTraverse().
//          Results of the async job are picked up by the call to GetTextOpacityMask()
//          on the render thread.
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::GetTextOpacityMaskAsync( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, UITextLayoutProperties_t *pProps, const char **pchRangeFontNames, float flUsageTime )
{
	AUTO_LOCK( s_drawCacheMutex );

	CacheEntry_t *pCacheEntry = LookupCacheEntry( pProps );

	if( !pCacheEntry )
	{
		VPROF_BUDGET_THREAD( "CTextLayoutDrawCache::GetTextOpacityMaskAsync - miss", VPROF_BUDGETGROUP_TENFOOT );

		pCacheEntry = AddCacheEntry( pProps, flUsageTime );
		UITextLayoutProperties_t *pKey = m_mapCacheEntries.Key( pCacheEntry->m_iMap );

		// Kick off job to generate text bitmap
		CJob* pDrawJob = m_pTextServices->BDrawAsync( pRawText, cbRawText, cTextChars, eTextEncoding, *pKey, pchFontName, pchRangeFontNames, m_pTextureStorage );
		if( pDrawJob )
		{
			pCacheEntry->m_pPendingDrawJob = pDrawJob;
		}
		else
		{
			Warning( "Failed to create job to generate pango text ranges\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: If the specified text layout properties exists, sets its timestamp and
//			moves it to the end of the LRU list
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::Touch( UITextLayoutProperties_t *pProps, float flUsageTime )
{
	AUTO_LOCK( s_drawCacheMutex );
	int iMap = m_mapCacheEntries.Find( pProps );
	if ( iMap != m_mapCacheEntries.InvalidIndex() )
	{
		// Update LRU position, then return 
		CacheEntry_t *pCacheEntry = m_mapCacheEntries[iMap];
		pCacheEntry->m_flLastUseTime = flUsageTime;
		m_listCacheEntries.LinkToTail( pCacheEntry->m_iList );
		pCacheEntry->m_iList = m_listCacheEntries.Tail();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return the oldest LRU list entry if it is older than the limit
//-----------------------------------------------------------------------------
UITextOpacityMaskData_t *CTextLayoutDrawCache::GetOldestEntry( float flLimitTime )
{
	AUTO_LOCK( s_drawCacheMutex );
	if ( !m_listCacheEntries.Count() )
	{
		return NULL;
	}
	
	int iList = m_listCacheEntries.Head();
	CacheEntry_t *pCacheEntry = &m_listCacheEntries[iList];
	if ( flLimitTime >= 0.0f && pCacheEntry->m_flLastUseTime >= flLimitTime )
	{
		return NULL;
	}

	return &pCacheEntry->m_textOpacityMaskData;
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the oldest LRU list entry
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::DeleteOldestEntry()
{
	AUTO_LOCK( s_drawCacheMutex );
	Assert( m_listCacheEntries.Count() > 0 );

	int iList = m_listCacheEntries.Head();
	CacheEntry_t *pCacheEntry = &m_listCacheEntries[iList];
	UITextOpacityMaskData_t &data = pCacheEntry->m_textOpacityMaskData;
	if( pCacheEntry->m_pPendingDrawJob )
	{
		pCacheEntry->m_pPendingDrawJob->WaitForFinish();
		pCacheEntry->m_pPendingDrawJob->Release();
		pCacheEntry->m_pPendingDrawJob = NULL;
	}
	if( data.m_pRangeData )
	{
		delete[] data.m_pRangeData;
	}
	UITextLayoutProperties_t *pKey = m_mapCacheEntries.Key( pCacheEntry->m_iMap );
	m_mapCacheEntries.RemoveAt( pCacheEntry->m_iMap );
	m_listCacheEntries.Remove( iList );
	m_PoolTextLayoutProperties.Free( pKey );
}


//-----------------------------------------------------------------------------
// Purpose: Helper routine for code also using a texture cache.  Finds
// all entries older than the limit, frees their texture region
// in the given cache and deletes them from the draw cache.
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::DeleteOlderEntriesToTextureCache( float flLimitTime, IUITextTextureCache *pTextureCache )
{
 	int deleteCount = 0;
	for (;;)
	{
		AUTO_LOCK( s_drawCacheMutex );
		UITextOpacityMaskData_t *pOldestData = GetOldestEntry( flLimitTime );
		if ( !pOldestData )
		{
			break;
		}

		if( pOldestData->m_cRangeData > 0 )
		{
			for( int iRange = 0; iRange < pOldestData->m_cRangeData; iRange++ )
			{
				UITextOpacityMaskDataRange_t &rangeData = pOldestData->m_pRangeData[iRange];
				if( rangeData.m_hTexture )
				{
					UITextTextureRegion_t freeTarget;
					freeTarget.m_hTexture = rangeData.m_hTexture;
					freeTarget.m_rect.m_iLeft = rangeData.m_x0;
					freeTarget.m_rect.m_iRight = rangeData.m_x1;
					freeTarget.m_rect.m_iTop = rangeData.m_y0;
					freeTarget.m_rect.m_iBottom = rangeData.m_y1;
					freeTarget.m_iPadding = 0;
					freeTarget.m_flTextureWidth = rangeData.m_flTextureWidth;
					freeTarget.m_flTextureHeight = rangeData.m_flTextureHeight;

					pTextureCache->FreeTextureRegion( freeTarget );
				}
			}
		}
			
 		++deleteCount;
		DeleteOldestEntry();
	}
 	if( deleteCount )
 	{
// 		Msg( "DeleteOlderEntriesToTextureCache %d items deleted, map count %d, lru count %d, property pool count %d\n", deleteCount, m_mapTextOpacityMasks.Count(), m_listTextOpacityMaskLRU.Count(), m_PoolTextLayoutProperties.Count() );
 	}
}


//-----------------------------------------------------------------------------
// Purpose: Empties the cache
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::Clear()
{
	AUTO_LOCK( s_drawCacheMutex );
	while ( m_listCacheEntries.Count() )
	{
		DeleteOldestEntry();
	}

	m_listCacheEntries.Purge();
	m_mapCacheEntries.Purge();
	m_PoolTextLayoutProperties.Clear();
}


//-----------------------------------------------------------------------------
// Purpose: Searches the specified range formats for the index of the next format w/ a color set. Returns -1 if not found.
//-----------------------------------------------------------------------------
int FindNextTextRangeWithColor( const UITextFormatProperties_t *pRangeFormats, int cRangeFormats, int iCurrent = -1 )
{
	iCurrent++;
	for ( int i = iCurrent; i < cRangeFormats; i++ )
	{
		if ( pRangeFormats[i].m_iColorIndex != UITextFormatProperties_t::k_iColorIndexUnset )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: adds hints to the font render of color changes at this location so it breaks up the glyph run
//-----------------------------------------------------------------------------
void CTextLayoutDrawCache::AddColorDrawEffects( IUITextLayout *pTextLayout, const UITextFormatProperties_t *pRangeFormats, int cRangeFormats, int nStrLen )
{
	if ( nStrLen == 0 )
		return;

	// ASSUMES COLOR RANGES ARE IN ORDER AND NOT OVERLAPPING - THEY SHOULD BE!!!
	uint iCurrentChar = 0;
	uint iLastChar = nStrLen - 1;
	int iRangeFormat = FindNextTextRangeWithColor( pRangeFormats, cRangeFormats );
	CUtlVector< IUITextLayout::HitTestRegionRect_t > vecHitTestRegions;
	while ( iCurrentChar <= iLastChar )
	{
		uint iEndChar = iLastChar;
		int iColorIndex = UITextFormatProperties_t::k_iColorIndexUnset;
		if ( iRangeFormat != -1 )
		{
			if ( iCurrentChar == pRangeFormats[iRangeFormat].m_unCharStartIndex )
			{
				// processing overridden color range
				iEndChar = pRangeFormats[iRangeFormat].m_unCharEndIndex;
				iColorIndex = iRangeFormat;
				iRangeFormat = FindNextTextRangeWithColor( pRangeFormats, cRangeFormats, iRangeFormat );
			}
			else
			{
				// in default text color with a new range after
				iEndChar = pRangeFormats[iRangeFormat].m_unCharStartIndex - 1;
			}
		}

		pTextLayout->MarkColorRangeForMeasurement( iCurrentChar, iEndChar, iColorIndex );

		iCurrentChar = iEndChar + 1;
	}
}


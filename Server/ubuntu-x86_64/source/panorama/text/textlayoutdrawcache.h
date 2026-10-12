//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef TEXTLAYOUTDRAWCACHE_H
#define TEXTLAYOUTDRAWCACHE_H
#pragma once

namespace panorama
{

//
// Atlased-texture 2D cache for text string opacity masks.
//
class CTextLayoutDrawCache : public IUITextLayoutDrawCache
{
public:
    CTextLayoutDrawCache();
    virtual ~CTextLayoutDrawCache();

	void SetTextServices( IUITextServices* pServices );
	void SetTextureStorage( IUITextTextureStorage *pStorage );
	
	virtual UITextLayoutProperties_t *AllocTextLayoutProperties( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, ELanguage language ) OVERRIDE;
	virtual void InitTextLayoutProperties( UITextLayoutProperties_t *, const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, ELanguage language ) OVERRIDE;
	virtual void FreeTextLayoutProperties( UITextLayoutProperties_t *pProps ) OVERRIDE;

	virtual UITextOpacityMaskData_t *GetTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, UITextLayoutProperties_t *pProps, const char **pchRangeFontNames, float flUsageTime, void *pRenderContext ) OVERRIDE;
	virtual void GetTextOpacityMaskAsync( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, const char *pchFontName, UITextLayoutProperties_t *pProps, const char **pchRangeFontNames, float flUsageTime ) OVERRIDE;

	// If the specified text layout properties exists, sets its timestamp and
	// moves it to the end of the LRU list
	virtual void Touch( UITextLayoutProperties_t *pProps, float flUsageTime ) OVERRIDE;

	virtual UITextOpacityMaskData_t *GetOldestEntry( float flLimitTime ) OVERRIDE;
	virtual void DeleteOldestEntry() OVERRIDE;

	virtual void DeleteOlderEntriesToTextureCache( float flLimitTime, IUITextTextureCache *pTextureCache ) OVERRIDE;

	virtual void Clear() OVERRIDE;

	static void AddColorDrawEffects( IUITextLayout *pTextLayout, const UITextFormatProperties_t *pRangeFormats, int cRangeFormats, int nStrLen );

protected:

	IUITextServices* m_pTextServices;
	IUITextTextureStorage *m_pTextureStorage;
	CClassMemoryPoolMT< UITextLayoutProperties_t > m_PoolTextLayoutProperties;

	struct CacheEntry_t
	{
		UITextOpacityMaskData_t m_textOpacityMaskData;
		int m_iList;
		int m_iMap;
		double m_flLastUseTime;
		CJob* m_pPendingDrawJob;
	};

	// The following map and list containers are accessed from multiple threads
	// All functions using them should grab the s_drawCacheMutex.
	CUtlMap< UITextLayoutProperties_t*, CacheEntry_t*, int, CDefLessPtr< UITextLayoutProperties_t > > m_mapCacheEntries;
	CUtlBlockLinkedList< CacheEntry_t, int > m_listCacheEntries; // Blocked linked list to avoid reallocs of the cache entries 

private:
	// Gets cache entry if already exists in the cache
	CacheEntry_t* LookupCacheEntry( UITextLayoutProperties_t *pProps );

	// Adds new entry to cache
	CacheEntry_t* AddCacheEntry( UITextLayoutProperties_t *pProps, float flUsageTime );
	
	// Updates LRU timestamp and moves it to the end of the LRU list
	void UpdateLruPosition( CacheEntry_t* pCacheEntry, float flUsageTime );

};

} // namespace panorama

#endif // TEXTLAYOUTDRAWCACHE_H

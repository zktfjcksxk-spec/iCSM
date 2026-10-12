//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef TEXTTEXTURECACHE_H
#define TEXTTEXTURECACHE_H
#pragma once

namespace panorama
{

//
// Atlased-texture 2D cache for text string opacity masks.
//
class CTextTextureCache : public IUITextTextureCache
{
public:
    CTextTextureCache();
    
	void SetTextureProvider( IUITextTextureProvider *pProvider );
	
	virtual UITextTextureRegion_t GetTextureRegion( int32 iWidth, int32 iHeight ) OVERRIDE;
	virtual void FreeTextureRegion( const UITextTextureRegion_t &region ) OVERRIDE;

	// Forgets all cache content but does not call FreeTexture.
	virtual void Clear() OVERRIDE;

	// Calls FreeTexture for every texture in use.
	virtual void Purge() OVERRIDE;

	virtual int GetNumTexturesInUse() const OVERRIDE
	{
		return m_vecTexturesInUse.Count();
	}
	
protected:
	struct FreeTextMaskRegionRect_t
	{
		UITextTextureHandle_t m_hTexture;
		float m_flRenderTargetWidth;
		float m_flRenderTargetHeight;

		UITextRect m_freeRegion;

		// Sort on width then height
		bool operator <( const FreeTextMaskRegionRect_t &l ) const
		{
			uint32 width = m_freeRegion.m_iRight - m_freeRegion.m_iLeft;
			uint32 lWidth = l.m_freeRegion.m_iRight - l.m_freeRegion.m_iLeft;
			if ( width != lWidth )
			{
				if ( width < lWidth )
					return true;
				else
					return false;
			}

			uint32 height = m_freeRegion.m_iBottom - m_freeRegion.m_iTop;
			uint32 lHeight = l.m_freeRegion.m_iBottom - l.m_freeRegion.m_iTop;
			return height < lHeight;
		}
	};

	IUITextTextureProvider *m_pTextureProvider;
	CUtlRBTree< FreeTextMaskRegionRect_t, int, CDefLess< FreeTextMaskRegionRect_t > > m_treeFreeTextRegions;
	CUtlVector< UITextTextureHandle_t > m_vecTexturesInUse;

	static void PrintMaskRegion( const char *pDesc, FreeTextMaskRegionRect_t &region );
};

} // namespace panorama

#endif // TEXTTEXTURECACHE_H

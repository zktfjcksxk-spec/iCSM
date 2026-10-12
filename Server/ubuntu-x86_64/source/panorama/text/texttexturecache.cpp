//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "texttexturecache.h"

using namespace panorama;


// We leave a one-pixel border around the opacity masks so that
// we can blend without smearing in neighboring regions.
const int k_nTextRegionPadding = 1;

//-----------------------------------------------------------------------------
//Purpose: Round up size to a multiple of 32
//-----------------------------------------------------------------------------
static int32 RoundUpTextureSize( int32 size )
{
	return Max( 32, ( size + 31 ) & ~31 );
}


// For simple debugging of cache contents.
void CTextTextureCache::PrintMaskRegion( const char *pDesc, FreeTextMaskRegionRect_t &region )
{
	Msg( "*** %s %d,%d - %d,%d\n",
		 pDesc,
		 region.m_freeRegion.m_iLeft,
		 region.m_freeRegion.m_iTop,
		 region.m_freeRegion.m_iRight,
		 region.m_freeRegion.m_iBottom );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextTextureCache::CTextTextureCache()
{
	m_pTextureProvider = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the texture provider callback
//-----------------------------------------------------------------------------
void CTextTextureCache::SetTextureProvider( IUITextTextureProvider *pProvider )
{
	// We don't support switching providers.
	Assert( m_pTextureProvider == NULL );

	m_pTextureProvider = pProvider;
}


//-----------------------------------------------------------------------------
// Purpose: Creates or finds a cached text alpha texture
//-----------------------------------------------------------------------------
UITextTextureRegion_t CTextTextureCache::GetTextureRegion( int32 iUnpaddedWidth, int32 iUnpaddedHeight )
{
	Assert( m_pTextureProvider != NULL );

	// We pad out the text area to give a black border around the text
	// for clean filtering.
	int32 iWidth = iUnpaddedWidth + k_nTextRegionPadding * 2;
	int32 iHeight = iUnpaddedHeight + k_nTextRegionPadding * 2;

	// See if we can find a free spot in an existing texture that fits
	FreeTextMaskRegionRect_t search;
	search.m_freeRegion.m_iLeft = 0;
	search.m_freeRegion.m_iTop = 0;
	search.m_freeRegion.m_iRight = iWidth;
	search.m_freeRegion.m_iBottom = iHeight;

	float flTexWidth, flTexHeight;

	int iSearch = -1;
	{
		VPROF_BUDGET( "CTextTextureCache::GetTextureRegion - search free space", VPROF_BUDGETGROUP_TENFOOT );
		int nBestArea = INT_MAX;
		iSearch = m_treeFreeTextRegions.InvalidIndex();
		int iTree = m_treeFreeTextRegions.FindClosest( search, k_EGreaterThanOrEqualTo );

		while ( iTree != m_treeFreeTextRegions.InvalidIndex() )
		{
			FreeTextMaskRegionRect_t &cur = m_treeFreeTextRegions[iTree];
			int nFreeWidth = cur.m_freeRegion.m_iRight - cur.m_freeRegion.m_iLeft;
			int nFreeHeight = cur.m_freeRegion.m_iBottom - cur.m_freeRegion.m_iTop;
			if ( nFreeWidth >= iWidth && nFreeHeight >= iHeight )
			{
				int nNewArea = nFreeWidth * nFreeHeight;
				if ( nNewArea < nBestArea )
				{
					nBestArea = nFreeWidth * nFreeHeight;
					iSearch = iTree;
					// if the area we have found is within 10% of what we need then just go ahead and use it
					//   this fixes degenerate behavior drawing small strings when lots of allocs have happened
					//   previously and we iterate every page even though any will do
					if ( ( iWidth * iHeight * 1.1 ) > nBestArea )
						break;
				}
			}

			iTree = m_treeFreeTextRegions.NextInorder( iTree );
		}
	}

	const uint32 unTargetTexWidth = 1024;
	const uint32 unTargetTexHeight = 1024;

	UITextRect rectTarget = { 0 };

	UITextTextureHandle_t hTexture = NULL;
	if ( iSearch != m_treeFreeTextRegions.InvalidIndex() )
	{
		VPROF_BUDGET( "CTextTextureCache::GetTextureRegion - split free space", VPROF_BUDGETGROUP_TENFOOT );
		FreeTextMaskRegionRect_t used = m_treeFreeTextRegions[iSearch];
		hTexture = used.m_hTexture;

		// currently we always return a rect exactly matching the requested dimensions
		rectTarget.m_iLeft = used.m_freeRegion.m_iLeft;
		rectTarget.m_iTop = used.m_freeRegion.m_iTop;
		rectTarget.m_iRight = used.m_freeRegion.m_iLeft + iWidth;
		rectTarget.m_iBottom = used.m_freeRegion.m_iTop + iHeight;

		flTexWidth = used.m_flRenderTargetWidth;
		flTexHeight = used.m_flRenderTargetHeight;

		m_treeFreeTextRegions.RemoveAt( iSearch );


		float flRemainingHorizontal = used.m_freeRegion.m_iRight - used.m_freeRegion.m_iLeft - iWidth;
		if ( flRemainingHorizontal > 0.0f )
		{
			FreeTextMaskRegionRect_t rightRemaining;
			rightRemaining.m_hTexture = hTexture;
			rightRemaining.m_freeRegion.m_iLeft = rectTarget.m_iRight;
			rightRemaining.m_freeRegion.m_iRight = used.m_freeRegion.m_iRight;
			rightRemaining.m_freeRegion.m_iTop = used.m_freeRegion.m_iTop;
			rightRemaining.m_freeRegion.m_iBottom = rectTarget.m_iBottom;
			rightRemaining.m_flRenderTargetWidth = used.m_flRenderTargetWidth;
			rightRemaining.m_flRenderTargetHeight = used.m_flRenderTargetHeight;
			m_treeFreeTextRegions.Insert( rightRemaining );

			//Msg( "Right remaining split: %d_%u-%u_%u-%u\n", unRenderTarget, rightRemaining.m_freeRegion.m_iLeft, rightRemaining.m_freeRegion.m_iTop, rightRemaining.m_freeRegion.m_iRight, rightRemaining.m_freeRegion.m_iBottom );
		}

		float flRemainingVertical = used.m_freeRegion.m_iBottom - used.m_freeRegion.m_iTop - iHeight;
		if ( flRemainingVertical > 0.0f )
		{
			FreeTextMaskRegionRect_t bottomRemaining;
			bottomRemaining.m_hTexture = hTexture;
			bottomRemaining.m_freeRegion.m_iLeft = used.m_freeRegion.m_iLeft;
			bottomRemaining.m_freeRegion.m_iRight = used.m_freeRegion.m_iRight;
			bottomRemaining.m_freeRegion.m_iTop = rectTarget.m_iBottom;
			bottomRemaining.m_freeRegion.m_iBottom = used.m_freeRegion.m_iBottom;
			bottomRemaining.m_flRenderTargetWidth = used.m_flRenderTargetWidth;
			bottomRemaining.m_flRenderTargetHeight = used.m_flRenderTargetHeight;
			m_treeFreeTextRegions.Insert( bottomRemaining );

			//Msg( "Bottom remaining split: %d_%u-%u_%u-%u\n", unRenderTarget, bottomRemaining.m_freeRegion.m_iLeft, bottomRemaining.m_freeRegion.m_iTop, bottomRemaining.m_freeRegion.m_iRight, bottomRemaining.m_freeRegion.m_iBottom );
		}

	}
	else
	{
		VPROF_BUDGET( "CTextTextureCache::GetTextureRegion - allocate new space", VPROF_BUDGETGROUP_TENFOOT );

		int32 iPageWidth = MAX( unTargetTexWidth, iWidth );
		int32 iPageHeight = MAX( unTargetTexHeight, iHeight );

		iPageWidth = RoundUpTextureSize( iPageWidth );
		iPageHeight = RoundUpTextureSize( iPageHeight );

		hTexture = m_pTextureProvider->AllocAlphaTexture( iPageWidth, iPageHeight );
		if ( !hTexture )
		{
			AssertMsg( false, "Failed creating texture for panorama text" );
		}

		if ( iPageWidth - iWidth > 0 )
		{
			FreeTextMaskRegionRect_t rightRemaining;
			rightRemaining.m_hTexture = hTexture;
			rightRemaining.m_freeRegion.m_iLeft = iWidth;
			rightRemaining.m_freeRegion.m_iRight = iPageWidth;
			rightRemaining.m_freeRegion.m_iTop = 0;
			rightRemaining.m_freeRegion.m_iBottom = iHeight;
			rightRemaining.m_flRenderTargetWidth = iPageWidth;
			rightRemaining.m_flRenderTargetHeight = iPageHeight;
			m_treeFreeTextRegions.Insert( rightRemaining );

			//Msg( "Right (new) remaining split: %d_%u-%u_%u-%u\n", unRenderTarget, rightRemaining.m_freeRegion.m_iLeft, rightRemaining.m_freeRegion.m_iTop, rightRemaining.m_freeRegion.m_iRight, rightRemaining.m_freeRegion.m_iBottom );
		}

		if ( iPageHeight - iHeight > 0 )
		{
			FreeTextMaskRegionRect_t bottomRemaining;
			bottomRemaining.m_hTexture = hTexture;
			bottomRemaining.m_freeRegion.m_iLeft = 0;
			bottomRemaining.m_freeRegion.m_iRight = iPageWidth;
			bottomRemaining.m_freeRegion.m_iTop = iHeight;
			bottomRemaining.m_freeRegion.m_iBottom = iPageHeight;
			bottomRemaining.m_flRenderTargetWidth = iPageWidth;
			bottomRemaining.m_flRenderTargetHeight = iPageHeight;
			m_treeFreeTextRegions.Insert( bottomRemaining );

			//Msg( "Right (new) remaining split: %d_%u-%u_%u-%u\n", unRenderTarget, bottomRemaining.m_freeRegion.m_iLeft, bottomRemaining.m_freeRegion.m_iTop, bottomRemaining.m_freeRegion.m_iRight, bottomRemaining.m_freeRegion.m_iBottom );
		}

		// currently we always return a rect exactly matching the requested dimensions
		rectTarget.m_iLeft = 0;
		rectTarget.m_iTop = 0;
		rectTarget.m_iRight = iWidth;
		rectTarget.m_iBottom = iHeight;
		flTexWidth = iPageWidth;
		flTexHeight = iPageHeight;

		m_vecTexturesInUse.AddToTail( hTexture );
	}

	UITextTextureRegion_t result;
	result.m_hTexture = hTexture;
	result.m_rect = rectTarget;
	result.m_iPadding = k_nTextRegionPadding;
	result.m_flTextureWidth = flTexWidth;
	result.m_flTextureHeight = flTexHeight;

	return result;
}


void CTextTextureCache::FreeTextureRegion( const UITextTextureRegion_t &region )
{
	Assert( m_pTextureProvider != NULL );

	FreeTextMaskRegionRect_t freeRect;
	freeRect.m_hTexture = region.m_hTexture;
	freeRect.m_flRenderTargetWidth = region.m_flTextureWidth;
	freeRect.m_flRenderTargetHeight = region.m_flTextureHeight;
	freeRect.m_freeRegion.m_iLeft = region.m_rect.m_iLeft - k_nTextRegionPadding;
	freeRect.m_freeRegion.m_iRight = region.m_rect.m_iRight + k_nTextRegionPadding;
	freeRect.m_freeRegion.m_iTop = region.m_rect.m_iTop - k_nTextRegionPadding;
	freeRect.m_freeRegion.m_iBottom = region.m_rect.m_iBottom + k_nTextRegionPadding;
	m_treeFreeTextRegions.Insert( freeRect );
}


void CTextTextureCache::Clear()
{
	m_treeFreeTextRegions.Purge();
	m_vecTexturesInUse.Purge();
}


void CTextTextureCache::Purge()
{
	Assert( m_pTextureProvider != NULL );

	FOR_EACH_VEC( m_vecTexturesInUse, i )
	{
		m_pTextureProvider->FreeTexture( m_vecTexturesInUse[i] );
	}

	Clear();
}

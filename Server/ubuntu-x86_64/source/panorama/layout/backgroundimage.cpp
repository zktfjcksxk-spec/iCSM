//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "panorama/layout/backgroundimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBackgroundPosition::CBackgroundPosition()
{
	m_eHorizontalAlignment = k_EHorizontalAlignmentUnset;
	m_eVerticalAlignment = k_EVerticalAlignmentUnset;
}


//-----------------------------------------------------------------------------
// Purpose: Sets data
//-----------------------------------------------------------------------------
void CBackgroundPosition::Set( EHorizontalAlignment eHorizontal, const CUILength &horizontal, EVerticalAlignment eVertical, const CUILength &vertical )
{
	m_eHorizontalAlignment = (eHorizontal == k_EHorizontalAlignmentUnset && horizontal.IsSet()) ? k_EHorizontalAlignmentLeft : eHorizontal;
	if ( !horizontal.IsSet() && eHorizontal != k_EHorizontalAlignmentUnset )
		m_horizontal.SetPercent( 0 );
	else
		m_horizontal = horizontal;

	m_eVerticalAlignment = (eVertical == k_EVerticalAlignmentUnset && vertical.IsSet()) ? k_EVerticalAlignmentTop : eVertical;
	if ( !vertical.IsSet() && eVertical != k_EVerticalAlignmentUnset )
		m_vertical.SetPercent( 0 );
	else
		m_vertical = vertical;

	// if only one value is set, default the other to center
	if ( IsVerticalSet() && !IsHorizontalSet() )
	{
		m_eHorizontalAlignment = k_EHorizontalAlignmentCenter;
		m_horizontal.SetPercent( 0 );
	}	

	if ( IsHorizontalSet() && !IsVerticalSet() )
	{
		m_eVerticalAlignment = k_EVerticalAlignmentCenter;
		m_vertical.SetPercent( 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets default values
//-----------------------------------------------------------------------------
void CBackgroundPosition::ResolveDefaultValues()
{
	if ( IsSet() )
		return;

	// no params set, default to 0% 0%
	m_eHorizontalAlignment = k_EHorizontalAlignmentLeft;
	m_horizontal.SetPercent( 0 );
	m_eVerticalAlignment = k_EVerticalAlignmentTop;
	m_vertical.SetPercent( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Outputs values to string
//-----------------------------------------------------------------------------
void CBackgroundPosition::ToString( CFmtStr1024 *pfmtBuffer )
{
	int nStartLen = pfmtBuffer->Length();
	if ( m_eHorizontalAlignment != k_EHorizontalAlignmentUnset )
		pfmtBuffer->Append( PchNameFromEHorizontalAlignment( m_eHorizontalAlignment ) );

	if ( m_horizontal.IsSet() )
	{
		if ( pfmtBuffer->Length() != nStartLen )
			pfmtBuffer->Append( " " );

		CSSHelpers::AppendUILength( pfmtBuffer, m_horizontal );
	}

	if ( m_eVerticalAlignment != k_EVerticalAlignmentUnset )
	{
		if ( pfmtBuffer->Length() != nStartLen )
			pfmtBuffer->Append( " " );

		pfmtBuffer->Append( PchNameFromEVerticalAlignment( m_eVerticalAlignment ) );
	}

	if ( m_vertical.IsSet() )
	{
		if ( pfmtBuffer->Length() != nStartLen )
			pfmtBuffer->Append( " " );

		CSSHelpers::AppendUILength( pfmtBuffer, m_vertical );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBackgroundImageLayer::CBackgroundImageLayer()
{
	m_pImage = NULL;
	m_eImagePath = k_EImagePathUnset;
	m_eBackgroundSizeConstant = k_EBackgroundSizeConstantNone;
	m_flOpacity = 1.0f;
	m_bTemporaryLayer = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBackgroundImageLayer::~CBackgroundImageLayer()
{
	SAFE_RELEASE( m_pImage );
	m_pVideoPlayer = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets default values
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::ResolveDefaultValues()
{
	if ( m_eImagePath == k_EImagePathUnset )
		m_eImagePath = k_EImagePathNone;

	m_position.ResolveDefaultValues();

	if ( !m_width.IsSet() )
		m_width.SetLength( k_flFloatAuto );

	if ( !m_height.IsSet() )
		m_height.SetLength( k_flFloatAuto );

	m_repeat.ResolveDefaultValues();	
}


//-----------------------------------------------------------------------------
// Purpose: called when we are ready to apply any scaling factor to the values
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::ApplyUIScaleFactor( float flScaleFactorX, float flScaleFactorY )
{
	m_width.ScaleLengthValue( flScaleFactorX );
	m_height.ScaleLengthValue( flScaleFactorY );
	m_position.ScaleLengthValues( flScaleFactorX, flScaleFactorY );
}


//-----------------------------------------------------------------------------
// Purpose: Called when applied to a panel
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::OnAppliedToPanel( IUIPanel *pPanel )
{
	if ( m_eImagePath != k_EImagePathSet || m_sURLPath.IsEmpty() )
		return;

	// currently don't unregister for events when background property goes away
	pPanel->RegisterForReadyEvents( true );

	if ( !pPanel->BReadyForDisplay() )
		return;

	ReloadImage( pPanel );	
}


//-----------------------------------------------------------------------------
// Purpose: Merges our set styles to target if unset
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::MergeTo( CBackgroundImageLayer *pTarget )
{
	if ( pTarget->m_eImagePath == k_EImagePathUnset )		
	{
		pTarget->m_sURLPath = m_sURLPath;
		pTarget->m_eImagePath = m_eImagePath;
	}

	if ( !pTarget->m_position.IsSet() )
		pTarget->m_position = m_position;

	if ( !pTarget->m_width.IsSet() && !pTarget->m_height.IsSet() )
	{
		pTarget->m_width = m_width;
		pTarget->m_height = m_height;
		pTarget->m_eBackgroundSizeConstant = m_eBackgroundSizeConstant;
	}

	if ( !pTarget->m_repeat.IsSet() )
		pTarget->m_repeat = m_repeat;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates final dimensions of this layer based on panel size. Does not include repeat
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::CalculateFinalDimensions( float *pflWidth, float *pflHeight, float flPanelWidth, float flPanelHeight, float flScaleFactorX, float flScaleFactorY )
{
	if ( !GetImage() && !GetMovie() )
	{
		*pflWidth = 0.0f;
		*pflHeight = 0.0f;
		return;
	}

	// auto means we need to preserve aspect ratio
	bool bWidthAuto = ( GetWidth().IsLength() && GetWidth().GetValue() == k_flFloatAuto );
	bool bHeightAuto = ( GetHeight().IsLength() && GetHeight().GetValue() == k_flFloatAuto );

	uint32 unTextureWidth = 0;
	uint32 unTextureHeight = 0;
	if ( GetImage() )
	{
		unTextureWidth = GetImage()->GetWidth();
		unTextureHeight = GetImage()->GetHeight();
	}
	else
	{
		GetMovie()->GetTextureSize( unTextureWidth, unTextureHeight );
	}

	float flImageWidth = flScaleFactorX * unTextureWidth;
	float flImageHeight = flScaleFactorY * unTextureHeight;

	EBackgroundSizeConstant backgroundSize = GetBackgroundSizeConstant();
	if ( backgroundSize == k_EBackgroundSizeConstantClipThenCover )
	{
		backgroundSize = k_EBackgroundSizeConstantNone;
		if ( ( flPanelWidth > flImageWidth ) || ( flPanelHeight > flImageHeight ) )
		{
			backgroundSize = k_EBackgroundSizeConstantCover;
		}
	}

	// calculate non-auto first
	float flWidth = 0.0f;
	if ( !bWidthAuto && backgroundSize == k_EBackgroundSizeConstantNone )
		flWidth = GetWidth().GetValueAsLength( flPanelWidth );

	float flHeight = 0.0f;
	if ( !bHeightAuto && backgroundSize == k_EBackgroundSizeConstantNone )
		flHeight = GetHeight().GetValueAsLength( flPanelHeight );

	// handle constants and autos. If both are auto, final size is just image dimensions
	if ( backgroundSize == k_EBackgroundSizeConstantContain )
	{
		float flImageAspectRatio = flImageWidth / flImageHeight;
		float flPanelAspectRatio = flPanelWidth / flPanelHeight;

		// scale so that width and height fit within container
		if ( flImageAspectRatio > flPanelAspectRatio )
		{
			// image is wider than panel, clamp to width
			flWidth = flPanelWidth;
			flHeight = flPanelWidth / flImageAspectRatio;
		}
		else
		{
			// image is taller than panel, clamp to height
			flHeight = flPanelHeight;
			flWidth = flPanelHeight * flImageAspectRatio;
		}
	}
	else if ( backgroundSize == k_EBackgroundSizeConstantCover )
	{
		float flImageAspectRatio = flImageWidth / flImageHeight;
		float flPanelAspectRatio = flPanelWidth / flPanelHeight;

		// scale so that the panel is fully covered
		if ( flImageAspectRatio > flPanelAspectRatio )
		{
			// image is wider than panel, clamp to height and clip the width
			flHeight = flPanelHeight;
			flWidth = flPanelHeight * flImageAspectRatio;
		}
		else
		{
			// image is taller than panel, clamp to width and clip the height
			flWidth = flPanelWidth;
			flHeight = flPanelWidth / flImageAspectRatio;
		}
	}
	else if ( bWidthAuto && bHeightAuto )
	{
		flWidth = flImageWidth;
		flHeight = flImageHeight;
	}
	else if ( bWidthAuto )
	{
		float flRatio = flImageWidth / flImageHeight;
		flWidth = flHeight  * flRatio;
	}
	else if ( bHeightAuto )
	{
		float flRatio = flImageWidth / flImageHeight;
		flHeight = flWidth * flRatio;
	}

	// adjust for rounding if necessary
	if ( m_repeat.GetHorizontal() == k_EBackgroundRepeatRound )
		flWidth = flPanelWidth / RoundInt( flPanelWidth / flWidth );

	if ( m_repeat.GetVertical() == k_EBackgroundRepeatRound )
		flHeight = flPanelHeight / RoundInt( flPanelHeight / flHeight );

	*pflWidth = flWidth;
	*pflHeight = flHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the final position (top left corner) of the layer to start drawing
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::CalculateFinalPosition( float *px, float *py, float flWidthPanel, float flHeightPanel, float flWidthImage, float flHeightImage )
{
	Assert( m_position.GetHorizontalAlignment() != k_EHorizontalAlignmentUnset );
	Assert( m_position.GetVeriticalAlignment() != k_EVerticalAlignmentUnset );
	Assert( m_position.GetHorizontalLength().IsSet() );
	Assert( m_position.GetVerticalLength().IsSet() );

	// if percentage, pixel at percentage into image must match percentage into background. For example, 50% 50% means the image is centered in the panel
	// if pixels, offset from location
	// offsets are inward if positive, outward if negative

	// calculate horizontal
	float flHorizontalPercent = 0.0f;
	if ( m_position.GetHorizontalAlignment() == k_EHorizontalAlignmentLeft )
	{
		flHorizontalPercent = 0.0f;
	}
	else if ( m_position.GetHorizontalAlignment() == k_EHorizontalAlignmentCenter )
	{
		flHorizontalPercent = 0.5;
	}
	else
	{
		Assert( m_position.GetHorizontalAlignment() == k_EHorizontalAlignmentRight );
		flHorizontalPercent = 1.0;
	}

	float flOffset = m_position.GetHorizontalLength().GetValueAsLength( flWidthPanel );
	if ( m_position.GetHorizontalAlignment() == k_EHorizontalAlignmentRight )
		flOffset = 0 - flOffset;

	*px = (flHorizontalPercent * flWidthPanel) + flOffset;
	if ( m_position.GetHorizontalLength().IsPercent() )
	{
		// adjust so image location matches panel, see percentage explanation above
		float flOffsetImage = m_position.GetHorizontalLength().GetValueAsLength( flWidthImage );
		if ( m_position.GetHorizontalAlignment() == k_EHorizontalAlignmentRight )
			flOffsetImage = 0 - flOffsetImage;

		*px = *px - ((flHorizontalPercent * flWidthImage) + flOffsetImage);
	}

	// calculate vertical
	float flVerticalPercent = 0.0f;
	if ( m_position.GetVeriticalAlignment() == k_EVerticalAlignmentTop )
	{
		flVerticalPercent = 0.0f;
	}
	else if ( m_position.GetVeriticalAlignment() == k_EVerticalAlignmentCenter )
	{
		flVerticalPercent = 0.5;
	}
	else
	{
		Assert( m_position.GetVeriticalAlignment() == k_EVerticalAlignmentBottom );
		flVerticalPercent = 1.0;
	}

	flOffset = m_position.GetVerticalLength().GetValueAsLength( flHeightPanel );
	if ( m_position.GetVeriticalAlignment() == k_EVerticalAlignmentBottom )
		flOffset = 0 - flOffset;

	*py = (flVerticalPercent * flHeightPanel) + flOffset;
	if ( m_position.GetVerticalLength().IsPercent() )
	{
		// adjust so image location matches panel, see percentage explanation above
		float flOffsetImage = m_position.GetVerticalLength().GetValueAsLength( flHeightImage );
		if ( m_position.GetVeriticalAlignment() == k_EVerticalAlignmentBottom )
			flOffsetImage = 0 - flOffsetImage;

		*py = *py - ((flVerticalPercent * flHeightImage) + flOffsetImage);
	}

	// couple exceptions to the above. Space and round mean we ignore position, so just set to 0 in that direction
	if ( m_repeat.GetHorizontal() == k_EBackgroundRepeatSpace || m_repeat.GetHorizontal() == k_EBackgroundRepeatRound )
		*px = 0.0f;

	if ( m_repeat.GetVertical() == k_EBackgroundRepeatSpace || m_repeat.GetVertical() == k_EBackgroundRepeatRound )
		*py = 0.0f;

	// for repeat (not round or space, those start a 0), we may only draw part of the first image if the specified picture position is not
	// 0,0.See the following image: http://www.w3.org/TR/css3-background/bg-repeat
	if ( m_repeat.GetHorizontal() == k_EBackgroundRepeatRepeat && *px != 0.0f )
	{
		// calculate number of whole repeated images to the left of start, then adjust for 1 more (will get negative position which is correct)
		int cRepeats = RoundInt( *px / flWidthImage ) + 1;
		*px = *px - (cRepeats * flWidthImage );
	}

	if ( m_repeat.GetVertical() == k_EBackgroundRepeatRepeat && *py != 0.0f )
	{
		// calculate number of whole repeated images above start, then adjust for 1 more (will get negative position which is correct)
		int cRepeats = RoundInt( *py / flHeightImage ) + 1;
		*py = *py - (cRepeats * flHeightImage );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the final spacing for a background image when repeated
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::CalculateFinalSpacing( float *px, float *py, float flWidthPanel, float flHeightPanel, float flWidthImage, float flHeightImage )
{
	// when spacing allows for two or more images, first and last image need to line up with panel edges (if 3 panels, only 2 gaps!)

	*px = 0.0f;
	if ( m_repeat.GetHorizontal() == k_EBackgroundRepeatSpace )
	{
		int CWholeRepeats = floor( flWidthPanel / flWidthImage );
		float flRemaining = flWidthPanel - ( flWidthImage * CWholeRepeats );
		*px = flRemaining / MAX( CWholeRepeats - 1, 1 );
	}

	*py = 0.0f;
	if ( m_repeat.GetVertical() == k_EBackgroundRepeatSpace )
	{
		int CWholeRepeats = floor( flHeightPanel / flHeightImage );
		float flRemaining = flHeightPanel - ( flHeightImage * CWholeRepeats );
		*py = flRemaining / MAX( CWholeRepeats - 1, 1 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Outputs values in background shorthand notation
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::ToString( CFmtStr1024 *pfmtBuffer )
{
	if ( m_eImagePath == k_EImagePathUnset )
	{
		pfmtBuffer->Append( "unset" );
		return;
	}

	if ( m_eImagePath == k_EImagePathNone )
		pfmtBuffer->Append( "none" );
	else
		CSSHelpers::AppendURL( pfmtBuffer, m_sURLPath.String() );
	
	pfmtBuffer->Append( " " );
	m_position.ToString( pfmtBuffer );
	pfmtBuffer->Append( " / " );
	if ( m_eBackgroundSizeConstant == k_EBackgroundSizeConstantContain )
	{
		pfmtBuffer->Append( "contain" );
	}
	else
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_width );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_height );
	}	

	pfmtBuffer->AppendFormat( " %s %s", PchNameFromEBackgroundRepeat( m_repeat.GetHorizontal() ), PchNameFromEBackgroundRepeat( m_repeat.GetVertical() ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets values from other layer
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::Set( const CBackgroundImageLayer &rhs )
{
	m_sURLPath = rhs.m_sURLPath;
	m_eImagePath = rhs.m_eImagePath;
	m_position = rhs.m_position;
	m_eBackgroundSizeConstant = rhs.m_eBackgroundSizeConstant;
	m_width = rhs.m_width;
	m_height = rhs.m_height;
	m_repeat = rhs.m_repeat;

	m_pImage = NULL;
	m_pVideoPlayer = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Unloads an image (used for ready/unready)
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::UnloadImage()
{
	SAFE_RELEASE( m_pImage );
	m_pVideoPlayer = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Reloads an image (used for ready/unready)
//-----------------------------------------------------------------------------
void CBackgroundImageLayer::ReloadImage( IUIPanel *pPanel )
{
	if ( !pPanel || m_sURLPath.IsEmpty() )
		return;

	CFileResource file( m_sURLPath );
	EResourceImageType eType = DetermineResourceType( file );
	if ( eType == k_EResourceImageTypeImage || eType == k_EResourceImageTypeUnknown )
	{
		m_pImage = pPanel->UIImageManager()->LoadImageFromURL( pPanel, NULL, m_sURLPath, false, k_EImageFormatB8G8R8A8_PreMultiplied );
	}
	else if ( eType == k_EResourceImageTypeMovie )
	{
#if !defined (SOURCE2_PANORAMA )
		m_pVideoPlayer.SetNoRef( new CPanoramaVideoPlayer( pPanel->UIRenderEngine() ) );
#else
		CPanoramaVideoPlayer *pPlayer = new CPanoramaVideoPlayer( pPanel->UIRenderDevice() );
		m_pVideoPlayer = pPlayer;
		pPlayer->Release();
#endif
		if ( !m_pVideoPlayer->BLoad( m_sURLPath.String() ) )
		{
			m_pVideoPlayer = NULL;
		}
		else
		{
			m_pVideoPlayer->SetRepeat( true );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the opacity that this layer should be given the progress through an animation/transition
//-----------------------------------------------------------------------------
float CBackgroundImageLayer::GetInterpolatedOpacity( float flProgress ) const
{
	// If we're a temporary layer, we're animating from 1.0f -> 0.0f.  Otherwise, 0.0f -> 1.0f.
	return m_bTemporaryLayer ? ( 1.0f - flProgress ) : flProgress;
}


//-----------------------------------------------------------------------------
// Purpose: Validate
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CBackgroundImageLayer::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_sURLPath );

	// m_pImage should be validate by managers
	ValidatePtrIfNeeded( m_pVideoPlayer.Get() );
}
#endif

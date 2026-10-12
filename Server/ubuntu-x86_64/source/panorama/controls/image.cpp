//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/image.h"
#include "panorama/uijsregistration.h"
#include "panorama/data/iimagesource.h"
#ifdef SOURCE2_PANORAMA
#include "enumutils_panorama.h"
#else
#include "enumutils.h"
#endif // else SOURCE2_PANORAMA

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

DEFINE_PANORAMA_EVENT( SetImageSource );
DEFINE_PANORAMA_EVENT( ClearImageSource );
REGISTER_PANEL2D_FACTORY( CImagePanel, Image );


namespace panorama
{
	ENUMSTRINGS_START( EImageScaling )
	{ panorama::EImageScaling::k_EImageScalingNone, "none" },
	{ panorama::EImageScaling::k_EImageScalingStretchBoth, "stretch" },
	{ panorama::EImageScaling::k_EImageScalingStretchX, "stretchx" },
	{ panorama::EImageScaling::k_EImageScalingStretchY, "stretchy" },
	{ panorama::EImageScaling::k_EImageScalingStretchBothToFitPreserveAspectRatio, "stretch-to-fit-preserve-aspect" },
	{ panorama::EImageScaling::k_EImageScalingStretchXToFitPreserveAspectRatio, "stretch-to-fit-x-preserve-aspect" },
	{ panorama::EImageScaling::k_EImageScalingStretchYToFitPreserveAspectRatio, "stretch-to-fit-y-preserve-aspect" },
	{ panorama::EImageScaling::k_EImageScalingStretchBothToCoverPreserveAspectRatio, "stretch-to-cover-preserve-aspect" },
	ENUMSTRINGS_REVERSE( EImageScaling, k_EImageScalingNone )

} // namespace

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CImagePanel::CImagePanel( CPanel2D *parent, const char * pchPanelID ) : CPanel2D( parent, pchPanelID, ePanelFlags_DontFireOnLoad )
{
	m_pImage = NULL;
	m_eScaling = k_EImageScalingStretchBoth;
	m_eHorAlignment = k_EImageHorizontalAlignmentCenter;
	m_eVerAlignment = k_EImageVerticalAlignmentCenter;
	m_eOrientation = K_EImageOrientation_0;
	m_bAnimate = true;
	m_flPrevAnimateWidth = 0.0f;
	m_flPrevAnimateHeight = 0.0f;
	m_nVisibleSliceX = 0;
	m_nVisibleSliceY = 0;
	m_nVisibleSliceWidth = 0;
	m_nVisibleSliceHeight = 0;
	m_bImageUnloaded = false;
	m_pvecSourceSet = NULL;
	m_nLoadedSourceWidth = 0;
	m_flLastLayoutWidth = 0.0f;
	m_pImageLoading = NULL;

	// by default, want to listen for ready events
	RegisterForReadyEvents( true );

	RegisterEventHandler( ImageLoaded(), this, &CImagePanel::OnImageLoaded );
	RegisterEventHandler( SetImageSource(), this, &CImagePanel::OnSetImageSource );
	RegisterEventHandler( ClearImageSource(), this, &CImagePanel::OnClearImageSource );
	RegisterEventHandler( ReadyForDisplay(), this, &CImagePanel::OnReadyForDisplay );
	RegisterEventHandler( UnreadyForDisplay(), this, &CImagePanel::OnUnreadyForDisplay );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CImagePanel::~CImagePanel()
{
	Clear( true );
}


//-----------------------------------------------------------------------------
// Purpose: Set how the panel scales/stretches it's image contents
//-----------------------------------------------------------------------------
void CImagePanel::SetScaling( EImageScaling eScale ) 
{ 
	m_eScaling = eScale; 
	SetRepaint( k_EPanelRepaintFull ); 
}


//-----------------------------------------------------------------------------
// Purpose: Set how the panel scales/stretches it's image contents
//-----------------------------------------------------------------------------
void CImagePanel::SetScaling( CPanoramaSymbol symScale )
{
	m_eScaling = EImageScalingFromName( symScale.String() );
}


//-----------------------------------------------------------------------------
// Purpose: Set alignment for the image
//-----------------------------------------------------------------------------
void CImagePanel::SetAlignment( EImageHorizontalAlignment horAlign, EImageVerticalAlignment verAlign )
{
	m_eHorAlignment = horAlign;
	m_eVerAlignment = verAlign;
	SetRepaint( k_EPanelRepaintFull ); 
}


//-----------------------------------------------------------------------------
// Purpose: Tell the image to draw only a specific slice rather than the whole
//          image. A width or height of 0 will be interpreted as invalid, and
//          the entire image will be used.
//-----------------------------------------------------------------------------
void CImagePanel::SetVisibleImageSlice( int nX, int nY, int nWidth, int nHeight )
{
	if ( nX == m_nVisibleSliceX && nY == m_nVisibleSliceY &&
		nWidth == m_nVisibleSliceWidth && nHeight == m_nVisibleSliceHeight )
	{
		return;
	}

	int nPreviousSliceWidth = m_nVisibleSliceWidth;
	int nPreviousSliceHeight = m_nVisibleSliceHeight;

	m_nVisibleSliceX = nX;
	m_nVisibleSliceY = nY;
	m_nVisibleSliceWidth = nWidth;
	m_nVisibleSliceHeight = nHeight;

	if ( nPreviousSliceWidth != nWidth || nPreviousSliceHeight != nHeight )
		InvalidateSizeAndPosition();
	else
		SetRepaint( k_EPanelRepaintFull );
}

//-----------------------------------------------------------------------------
// Purpose: Get debug properties to show in debugger
//-----------------------------------------------------------------------------
void CImagePanel::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	BaseClass::GetDebugPropertyInfo( pvecProperties );

	// just output which is currently being used for srcset.. could show entire list after making better debug panel for properties
	DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t();
	pProperty->m_strName = m_pvecSourceSet ? "srcset_current" : "src";
	if ( m_pImage && m_pImage->BIsValid() )
	{
		pProperty->m_strValue = m_strSource.String();
	}
	else
	{
		pProperty->m_strValue = "none";
	}
	pvecProperties->AddToTail( pProperty );	

	// defaultsrc
	pProperty = new DebugPropertyOutput_t();
	pProperty->m_strName = "defaultsrc";
	if ( m_pImage && m_pImage->BIsValid()  )
	{
		pProperty->m_strValue = m_strSourceDefault.String();
	}
	else
	{
		pProperty->m_strValue = "none";
	}
	pvecProperties->AddToTail( pProperty );	
}


//-----------------------------------------------------------------------------
// Purpose: Parses a provided source set from string
//-----------------------------------------------------------------------------
bool CImagePanel::BParseSourceSet( CUtlVector< Source_t > *pvecSet, const char *pchString )
{
	// currently only support width selectors
	// example: [<url> <pixel width>w],[<url> <pixel width>w],...
	while ( pchString[0] != '\0' )
	{
		// find end of url. can't contain spaces so quick search
		pchString = CSSHelpers::SkipSpaces( pchString );
		const char *pchEnd = V_strstr( pchString, " " );
		if ( !pchEnd )
			return false;

		CUtlString strURL;
		strURL.SetDirect( pchString, pchEnd - pchString );
		pchString = pchEnd;

		// next should be <number>w
		pchString = CSSHelpers::SkipSpaces( pchString );
		pchEnd = pchString;
		int nWidth = 0;
		while ( pchEnd[0] >= '0' && pchEnd[0] <= '9' )
		{
			nWidth = (nWidth * 10) + (pchEnd[0] - '0');
			pchEnd++;
		}

		// must be at w and found something
		if ( pchEnd[0] != 'w' || pchEnd == pchString )
			return false;

		// store
		Source_t &source = pvecSet->Element( pvecSet->AddToTail() );
		source.m_strSource.Swap( strURL );
		source.m_nWidth = nWidth;

		// skip w
		pchString = CSSHelpers::SkipSpaces( pchEnd + 1 );
		if ( pchString[0] == '\0' )
			break;

		// must be comma
		if ( pchString[0] != ',' )
			return false;

		pchString++;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Applies properties set from layout file
//-----------------------------------------------------------------------------
bool CImagePanel::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	// parse out source and default source, then load
	const char *pchDefault = NULL;
	const char *pchSource = NULL;
	const char *pchEngineSource = NULL;

	static const CPanoramaSymbol symScaling = "scaling";
	static const CPanoramaSymbol symHorAlign = "horizontalalign";
	static const CPanoramaSymbol symVerAlign = "verticalalign";
	static const CPanoramaSymbol k_symSource( "src" );
	static const CPanoramaSymbol k_symDefaultSource( "defaultsrc" );
	static const CPanoramaSymbol k_symEngineRTSource( "enginertsrc" );
	static const CPanoramaSymbol k_symAnimate = "animate";
	static const CPanoramaSymbol k_symSourceSet = "srcset";
	static const CPanoramaSymbol k_symOrientation = "orientation";

	// texturewidth and textureheight can be used to override the size of vector graphics
	// Default value of -1 indicates texture width/height to be determined from source file
	static const CPanoramaSymbol k_symTextureWidth( "texturewidth" ); 
	static const CPanoramaSymbol k_symTextureHeight( "textureheight" );

	// SVG Presentation Attributes
	static const CPanoramaSymbol k_symSvgFill = "svgfill";
	static const CPanoramaSymbol k_symSvgFillOpacity = "svgfillopacity";
	static const CPanoramaSymbol k_symSvgStroke = "svgstroke";
	static const CPanoramaSymbol k_symSvgStrokeWidth( "svgstrokewidth" );
	static const CPanoramaSymbol k_symSvgStrokeLinecap( "svgstrokelinecap" );
	static const CPanoramaSymbol k_symSvgStrokeLinejoin( "svgstrokelinejoin" );
	static const CPanoramaSymbol k_symSvgStrokeOpacity = "svgstrokeopacity";
	static const CPanoramaSymbol k_symSvgOpacity = "svgopacity";
	static const CPanoramaSymbol k_symSvgFillRule = "svgfillrule";

	bool bSuccess = true;

	SvgAttributeOverrides_t& svgAttributeOverrides = m_reloadParams.m_svgAttributeOverrides;

	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( prop.m_symName == k_symSource )
		{
			if ( prop.m_pchValue && prop.m_pchValue[0] != '\0' )
				pchSource = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_symEngineRTSource )
		{
			if ( prop.m_pchValue && prop.m_pchValue[0] != '\0' )
				pchEngineSource = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_symDefaultSource )
		{
			if ( prop.m_pchValue && prop.m_pchValue[0] != '\0' )
			{
				pchDefault = prop.m_pchValue;
				BSetProperty( prop.m_symName, prop.m_pchValue );
			}
		}
		else if ( prop.m_symName == symScaling )
		{
			SetScaling( prop.m_pchValue );
		}
		else if ( prop.m_symName == symHorAlign )
		{
			if ( !V_stricmp( prop.m_pchValue, "center" ) )
			{
				m_eHorAlignment = k_EImageHorizontalAlignmentCenter;
			}
			else if ( !V_stricmp( prop.m_pchValue, "left" ) )
			{
				m_eHorAlignment = k_EImageHorizontalAlignmentLeft;
			}
			else if ( !V_stricmp( prop.m_pchValue, "right" ) )
			{
				m_eHorAlignment = k_EImageHorizontalAlignmentRight;
			}
		}
		else if ( prop.m_symName == symVerAlign )
		{
			if ( !V_stricmp( prop.m_pchValue, "center" ) )
			{
				m_eVerAlignment = k_EImageVerticalAlignmentCenter;
			}
			else if ( !V_stricmp( prop.m_pchValue, "top" ) )
			{
				m_eVerAlignment = k_EImageVerticalAlignmentTop;
			}
			else if ( !V_stricmp( prop.m_pchValue, "bottom" ) )
			{
				m_eVerAlignment = k_EImageVerticalAlignmentBottom;
			}
		}
		else if ( prop.m_symName == k_symAnimate )
		{
			if ( !V_stricmp( prop.m_pchValue, "false" ) )
			{
				m_bAnimate = false;
			}
		}
		else if ( prop.m_symName == k_symSourceSet )
		{
			if ( m_pvecSourceSet )
				m_pvecSourceSet->RemoveAll();
			else
				m_pvecSourceSet = new CUtlVector< Source_t >();

			if ( !BParseSourceSet( m_pvecSourceSet, prop.m_pchValue ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symOrientation )
		{
			if ( !V_strcmp( prop.m_pchValue, "0" ) )
			{
				m_eOrientation = K_EImageOrientation_0;
			}
			else if ( !V_strcmp( prop.m_pchValue, "90" ) )
			{
				m_eOrientation = K_EImageOrientation_90;
			}
			else if ( !V_strcmp( prop.m_pchValue, "180" ) )
			{
				m_eOrientation = K_EImageOrientation_180;
			}
			else if ( !V_strcmp( prop.m_pchValue, "270" ) )
			{
				m_eOrientation = K_EImageOrientation_270;
			}
			else
			{
				Warning( "Image Panel - Unknown orientation %s. Valid values are '0', '90', '180' or '270'" );
			}
		}
		else if( prop.m_symName == k_symTextureWidth )
		{
			m_reloadParams.m_nResizeWidth = V_atoi( prop.m_pchValue );
		}
		else if( prop.m_symName == k_symTextureHeight )
		{
			m_reloadParams.m_nResizeHeight = V_atoi( prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgFill)
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeFill, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgFillOpacity )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeFill_opacity, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgStroke )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeStroke, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgStrokeWidth )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeStroke_width, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgStrokeLinecap )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeStroke_linecap, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgStrokeLinejoin )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeStroke_linejoin, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgStrokeOpacity )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeStroke_opacity, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgOpacity )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeOpacity, prop.m_pchValue );
		}
		else if( prop.m_symName == k_symSvgFillRule )
		{
			SVGHelpers::ParsePresentationAttribute( svgAttributeOverrides, k_ESvgAttributeFill_rule, prop.m_pchValue );
		}

		else
		{
			if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
				bSuccess = false;
		}
	}

	// if a source set is provided, we will just use that so we can discard pchSource
	if ( m_pvecSourceSet )
	{
		m_strSource.Clear();
		m_strSourceDefault = pchDefault;
	}
	else if ( pchEngineSource )
	{
		if ( pchSource || pchDefault )
		{
			Warning( 
				"Image panel (id=%s) - 'enginesrc' set, ignoring 'src' (%s) and 'defaultsrc' (%s)\n",
				( GetID() ? GetID() : "unknown" ),
				( pchSource ? pchSource : "not set" ),
				( pchDefault ? pchDefault : "not set" ) );
		}
		SetImageFromEngineRT( pchEngineSource );
	}
	else if ( pchSource || pchDefault )
	{
		SetScaleFactor( pchSource );
		SetImage( pchSource, pchDefault, false, m_reloadParams );
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Sets image from string
//-----------------------------------------------------------------------------
bool CImagePanel::BRequiresContentClipLayer()
{
	// We need to require a clip layer to ensure various scaling options don't let us draw outside our bounds
	// if we have no composition layer
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set an image from tga/png/jpg data which is already in memory, but which still needs image decode
//-----------------------------------------------------------------------------
void CImagePanel::SetImageFromFileBuffer( const CUtlBuffer& bufFile, const UIImageLoadParams_t &loadParams )
{
	IImageSource *pImageSource = UIImageManager()->LoadImageFileFromMemory( UIPanel(), NULL, bufFile, loadParams  );
	SetImageInternal( pImageSource );
	SAFE_RELEASE( pImageSource );

	m_reloadParams = loadParams;

	m_strSource = "(memory)";
	m_strSourceDefault.Clear();

	m_bImageUnloaded = false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets image from a buffer
//-----------------------------------------------------------------------------
void CImagePanel::SetImage( const CUtlBuffer& bufRGBA, int nWide, int nTall, const char* pchDefaultImage /* = NULL */, EImageFormat eImageFormatIn /* = k_EImageFormatR8G8B8A8 */, const UIImageLoadParams_t &loadParams )
{
	VPROF_BUDGET( "CImagePanel::SetImage", VPROF_BUDGETGROUP_TENFOOT );

	IImageSource *pImageSource = UIImageManager()->LoadImageFromMemory( UIPanel(), pchDefaultImage, bufRGBA, nWide, nTall, eImageFormatIn, loadParams );
	SetImageInternal( pImageSource );
	SAFE_RELEASE( pImageSource );

	m_reloadParams = loadParams;

	m_strSource = "(memory)";

	if ( pchDefaultImage )
		m_strSourceDefault = pchDefaultImage;
	else
		m_strSourceDefault.Clear();

	m_bImageUnloaded = false;
}

//-----------------------------------------------------------------------------
// Purpose: Sets image from string
//-----------------------------------------------------------------------------
void CImagePanel::SetImage( const char *pchImage, const char *pchDefaultImage, bool bPrioritizeLoad, const UIImageLoadParams_t &loadParams )
{
	IImageSource *pImageSource = UIImageManager()->LoadImageFromURL( UIPanel(), pchDefaultImage, pchImage, bPrioritizeLoad, k_EImageFormatB8G8R8A8_PreMultiplied, loadParams );
	SetImageInternal( pImageSource );
	SAFE_RELEASE( pImageSource );

	m_reloadParams = loadParams;

	if ( pchImage )
		m_strSource = pchImage;
	else
		m_strSource.Clear();

	if ( pchDefaultImage )
		m_strSourceDefault = pchDefaultImage;
	else
		m_strSourceDefault.Clear();

	m_bImageUnloaded = false;
}

//-----------------------------------------------------------------------------
// Purpose: Sets image from IImageSource
//-----------------------------------------------------------------------------
void CImagePanel::SetImage( IImageSource *pImage )
{
	UIImageLoadParams_t loadParams;
	SetImageInternal( pImage );

	m_reloadParams = loadParams;

	m_strSource = "(IImageSource)";

	m_strSourceDefault.Clear();

	m_bImageUnloaded = false;

	DispatchEventAsync( ImageLoaded(), this, pImage );
}

//-----------------------------------------------------------------------------
// Purpose: Sets image from an engine render target name
//-----------------------------------------------------------------------------
void CImagePanel::SetImageFromEngineRT( const char *pchEngineRTName )
{
	UIImageLoadParams_t loadParams;
	IImageSource *pImageSource = UIImageManager()->LoadImageFromEngineRT( UIPanel(), pchEngineRTName, loadParams );
	SetImageInternal( pImageSource );
	SAFE_RELEASE( pImageSource );

	m_reloadParams = loadParams;

	m_strSource = "(memory)";
	m_strSourceDefault.Clear();
	m_bImageUnloaded = false;
}


//-----------------------------------------------------------------------------
// Purpose: Just a wrapper so we don't expose all the extra params to JS for now
//-----------------------------------------------------------------------------
void CImagePanel::SetImageJS( const char *pchImageURL )
{
	if( pchImageURL && V_strlen( pchImageURL ) )
	{
		// Update scale factor in case resolution has changed since this panel was created, or if image type has changed from or to svg
		SetScaleFactor( pchImageURL );

		SetImage( pchImageURL, NULL, false, m_reloadParams );
	}
	else
		Clear();
}


//-----------------------------------------------------------------------------
// Purpose: Clones panel
//-----------------------------------------------------------------------------
CPanel2D * CImagePanel::Clone()
{
	if ( !IsClonable() )
	{
		AssertMsg( false, "Panel can't be cloned (child type not clonable)" );
		return NULL;
	}

	CImagePanel *pImage = new CImagePanel( GetParent(), NULL );
	InitClonedPanel( pImage );

	return pImage;
}


//-----------------------------------------------------------------------------
// Purpose: Adds class specific data to clone a panel
//-----------------------------------------------------------------------------
void CImagePanel::InitClonedPanel( CPanel2D *pClone )
{
	BaseClass::InitClonedPanel( pClone );
	CImagePanel *pTarget = assert_cast< CImagePanel *>( pClone );

	pTarget->m_eScaling = m_eScaling;
	pTarget->m_eHorAlignment = m_eHorAlignment;
	pTarget->m_eVerAlignment = m_eVerAlignment;
	pTarget->m_bAnimate = m_bAnimate;
	pTarget->m_bImageUnloaded = m_bImageUnloaded;
	pTarget->SetImageInternal( m_pImage );
	pTarget->m_strSource = m_strSource;
	pTarget->m_strSourceDefault = m_strSourceDefault;
	pTarget->m_reloadParams = m_reloadParams;
}


//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CImagePanel::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "SetImage", PANORAMA_DELEGATE( &CImagePanel::SetImageJS ) );
	RegisterJSMethod( "SetScaling", PANORAMA_DELEGATE_RESOLVE( &CImagePanel::SetScaling, CPanoramaSymbol ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets image as triggered by event
//-----------------------------------------------------------------------------
bool CImagePanel::OnSetImageSource( const CPanelPtr<IUIPanel> &pPanel, const char *pchImageSource )
{
	Assert( pPanel.Get() == UIPanel() );

	if ( !m_pvecSourceSet )
		SetImage( pchImageSource, NULL );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Clears image as triggered by event
//-----------------------------------------------------------------------------
bool CImagePanel::OnClearImageSource( const CPanelPtr<IUIPanel> &pPanel )
{
	Assert( pPanel.Get() == UIPanel() );

	Clear();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets image from string
//-----------------------------------------------------------------------------
void CImagePanel::SetImageInternal( IImageSource *pImage )
{
	m_flPrevAnimateWidth = 0.0f;
	m_flPrevAnimateHeight = 0.0f;

	InvalidateSizeAndPosition();
	SAFE_RELEASE( m_pImage );

	if ( pImage )
		pImage->AddRef();
	m_pImage = pImage;

	m_bImageUnloaded = false;
}


//-----------------------------------------------------------------------------
// Purpose: Clears image data
//-----------------------------------------------------------------------------
void CImagePanel::Clear( bool bDestroying )
{
	m_flPrevAnimateWidth = 0.0f;
	m_flPrevAnimateHeight = 0.0f;
	m_strSource.Clear();
	m_strSourceDefault.Clear();

	if ( !bDestroying )
	{
		InvalidateSizeAndPosition();
	}

	SAFE_RELEASE( m_pImage );
	SAFE_RELEASE( m_pImageLoading );
	m_bImageUnloaded = false;
	SAFE_DELETE( m_pvecSourceSet );
}


//-----------------------------------------------------------------------------
// Purpose: fire the panel load event when our image loads
//-----------------------------------------------------------------------------
bool CImagePanel::OnImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage)
{
	// before handling image load code below, see if this is a load from a srcset and swap so shared load code will execute
	if ( pImage == m_pImageLoading )
	{
		SetImageInternal( pImage );
		SAFE_RELEASE( m_pImageLoading );
	}

	if ( pImage == m_pImage )
	{
		InvalidateSizeAndPosition();
		BaseClass::FirePanelLoadedEvent();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Loads image if necessary
//-----------------------------------------------------------------------------
bool CImagePanel::OnReadyForDisplay( const CPanelPtr< IUIPanel > &pPanel )
{
	ReloadImages();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Unloads image if necessary
//-----------------------------------------------------------------------------
bool CImagePanel::OnUnreadyForDisplay( const CPanelPtr< IUIPanel > &pPanel )
{
	UnloadImages();
	return true;
}



//-----------------------------------------------------------------------------
// Purpose: override to change how this panel is measured
//-----------------------------------------------------------------------------
void CImagePanel::OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	BaseClass::OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );

	// when padding is specified as a percentage, CSS uses the max width to calculate top and bottom. We have kept that pattern here.
	if ( m_pImage )
	{
		float flImageWidth = 0.0f;
		float flImageHeight = 0.0f;
		if ( m_nVisibleSliceWidth > 0 && m_nVisibleSliceHeight > 0 )
		{
			flImageWidth = MIN( m_nVisibleSliceWidth, m_pImage->GetWidth() );
			flImageHeight = MIN( m_nVisibleSliceHeight, m_pImage->GetHeight() );
		}
		else
		{
			flImageWidth = m_pImage->GetWidth();
			flImageHeight = m_pImage->GetHeight();
		}

		if ( m_eOrientation == K_EImageOrientation_90 || m_eOrientation == K_EImageOrientation_270 )
		{
			// Swap image width / height
			float flTemp = flImageWidth;
			flImageWidth = flImageHeight;
			flImageHeight = flTemp;
		}

		if ( m_reloadParams.m_fScaleFactor > 0.0f )
		{
			// image has already been scaled, so we need to apply only the difference in scales
			flImageWidth *= GetActualUIScaleX() / GetParentWindow()->GetWindowScaleFactor();
			flImageHeight *= GetActualUIScaleY() / GetParentWindow()->GetWindowScaleFactor();
		}
		else
		{
			flImageWidth *= GetActualUIScaleX();
			flImageHeight *= GetActualUIScaleY();
		}

		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( flImageWidth, flImageHeight, bFinalDimensions, flLeft, flTop, flRight, flBottom );

		*pflContentWidth = MAX( flImageWidth + flLeft + flRight, *pflContentWidth );
		*pflContentHeight = MAX( flImageHeight + flTop + flBottom, *pflContentHeight );

		// very specific fix for allowing scale in x/y, setting a css length in that direction and leaving other direction fit-children
		// Really need to override DesiredLayoutSizeTraverse() and handle all crazy mixes of scaling aspect ratio and CSS, picking which conflict to take
		if ( m_eScaling == k_EImageScalingStretchXToFitPreserveAspectRatio && *pflContentWidth > 0.0f )
		{
			CUILength styleWidth, styleHeight;
			AccessStyle()->GetInterpolatedWidth( styleWidth, bFinalDimensions );
			AccessStyle()->GetInterpolatedHeight( styleHeight, bFinalDimensions );
			if ( styleWidth.IsLength() && styleHeight.IsFitChildren() && styleWidth.GetValue() != *pflContentWidth )
			{
				float flScaleX = styleWidth.GetValue() / *pflContentWidth;
				*pflContentWidth = styleWidth.GetValue();
				*pflContentHeight = RoundFloatToInt( *pflContentHeight * flScaleX );
			}
		}
		else if ( m_eScaling == k_EImageScalingStretchYToFitPreserveAspectRatio && *pflContentHeight > 0.0f )
		{
			CUILength styleWidth, styleHeight;
			AccessStyle()->GetInterpolatedWidth( styleWidth, bFinalDimensions );
			AccessStyle()->GetInterpolatedHeight( styleHeight, bFinalDimensions );
			if ( styleHeight.IsLength() && styleWidth.IsFitChildren() && styleHeight.GetValue() != *pflContentHeight )
			{
				float flScaleY = styleHeight.GetValue() / *pflContentHeight;
				*pflContentHeight = styleHeight.GetValue();
				*pflContentWidth *= RoundFloatToInt( *pflContentWidth * flScaleY );
			}
		}


		// If we are trying to preserve aspect, then notice max width/height and scale our content size to fit within, so we don't end up clipping
		// one of them but requesting a much larger out of aspect opposing dimension
		if ( m_eScaling == k_EImageScalingStretchBothToFitPreserveAspectRatio || m_eScaling == k_EImageScalingStretchBothToCoverPreserveAspectRatio 
			|| m_eScaling == k_EImageScalingStretchXToFitPreserveAspectRatio || m_eScaling == k_EImageScalingStretchYToFitPreserveAspectRatio )
		{
			if ( *pflContentWidth > flMaxWidth )
			{
				float flScale = *pflContentWidth / flMaxWidth;
				*pflContentWidth = flMaxWidth;
				*pflContentHeight /= flScale;
			}

			if ( *pflContentHeight > flMaxHeight )
			{
				float flScale = *pflContentHeight / flMaxHeight;
				*pflContentHeight = flMaxHeight;
				*pflContentWidth /= flScale;
			}
		}
	}

	AssertMsg( IsFinite( *pflContentWidth ), "Invalid content width calculated" );
	AssertMsg( IsFinite( *pflContentHeight ), "Invalid content height calculated" );
}


//-----------------------------------------------------------------------------
// Purpose: Used to get panel width and height to select the appropriate srcset image
//-----------------------------------------------------------------------------
void CImagePanel::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	if ( !m_pvecSourceSet || m_pvecSourceSet->Count() == 0 )
		return;

	// include padding
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flFinalWidth, flFinalHeight, false, flLeft, flTop, flRight, flBottom );
	const float flContainerWidth = flFinalWidth - flLeft - flRight;
	
	if ( flContainerWidth == m_flLastLayoutWidth )
		return;

	m_flLastLayoutWidth = flContainerWidth;
	LoadImageFromSourceSet( flContainerWidth );
}


//-----------------------------------------------------------------------------
// Purpose: Load best image from sourceset
//-----------------------------------------------------------------------------
void CImagePanel::LoadImageFromSourceSet( float flWidth )
{
	// pick source that is one size larger than previous. Source list might not be sorted
	Source_t *pBest = &m_pvecSourceSet->Element( 0 );
	FOR_EACH_VEC( *m_pvecSourceSet, i )
	{
		if ( i == 0 )
			continue;

		Source_t &source = m_pvecSourceSet->Element( i );
		if ( source.m_nWidth >= flWidth )
		{
			// larger. Use if smaller than previous best or if previous best was too small
			if ( pBest->m_nWidth < flWidth || pBest->m_nWidth > source.m_nWidth )
				pBest = &source;
		}
		else
		{
			// too small. Only keep if larger than best
			if ( source.m_nWidth > pBest->m_nWidth )
				pBest = &source;
		}
	}

	// see if it changed from what we have previously loaded
	if ( pBest->m_nWidth == m_nLoadedSourceWidth )
		return;

	// load new image on the side and swap in so user doesn't see blank image while loading
	m_nLoadedSourceWidth = pBest->m_nWidth;
	SAFE_RELEASE( m_pImageLoading );

	// first load, use default image if available
	const char *pchDefault = m_pImage ? m_strSourceDefault.String() : NULL;
	m_pImageLoading = UIImageManager()->LoadImageFromURL( UIPanel(), pchDefault, pBest->m_strSource, false, k_EImageFormatB8G8R8A8_PreMultiplied, -1, -1, true );
}


//-----------------------------------------------------------------------------
// Purpose: Unload image for a while
//-----------------------------------------------------------------------------
void CImagePanel::UnloadImages()
{
	if ( m_pImage && m_strSource.Length() > 0 && V_stricmp( m_strSource.String(), "(memory)" ) != 0 )
	{
		// Validate we only reload when we have valid URLs...
		Assert( V_stristr( m_strSource.String(), "://" ) != NULL );

		m_bImageUnloaded = true;
		SAFE_RELEASE( m_pImage );
	}

	SAFE_RELEASE( m_pImageLoading );
}


//-----------------------------------------------------------------------------
// Purpose: Set scale factor for svgs based on parent window
//-----------------------------------------------------------------------------
void CImagePanel::SetScaleFactor( const char *pchImageURL )
{
	IUIWindow *pParentWindow = GetParentWindow();

	if( pchImageURL && pParentWindow )
	{
		const char* pchExtension = V_GetFileExtension( pchImageURL );

		// assume no extension might be reloaded as vector graphics
		if( !pchExtension || V_strnicmp( pchExtension, "svg", 3 ) == 0 )
		{
			// Scale vector graphics according to screen resolution
			m_reloadParams.m_fScaleFactor = pParentWindow->GetWindowScaleFactor();
		}
		else
		{
			m_reloadParams.m_fScaleFactor = -1.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update scale factor based on parent window
//-----------------------------------------------------------------------------
void CImagePanel::UpdateScaleFactor()
{
	bool bImageScaledOnLoad = m_reloadParams.m_fScaleFactor > 0.0f;
	IUIWindow *pParentWindow = GetParentWindow();
	if( bImageScaledOnLoad && pParentWindow )
	{
		m_reloadParams.m_fScaleFactor = pParentWindow->GetWindowScaleFactor();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reload actual image if needed
//-----------------------------------------------------------------------------
void CImagePanel::ReloadImages()
{
	if ( m_bImageUnloaded )
	{
		if ( m_pvecSourceSet )
		{
			// just invalidate size and position so srcset code in layout path runs
			InvalidateSizeAndPosition();
			m_flLastLayoutWidth = 0;
			m_nLoadedSourceWidth = 0;
		}
		else
		{
			UpdateScaleFactor(); // Update scale factor in case resolution has changed since this panel was created
			SetImage( m_strSource.String(), m_strSourceDefault.String(), false, m_reloadParams );
		}
		
		m_bImageUnloaded = false;
	}
}


struct ImagePanelUV_t
{
	Vector2D m_topLeft;
	Vector2D m_bottomRight;
};

static const ImagePanelUV_t s_orientationUVs[] = 
{
	{ {0.0f, 0.0f}, {1.0f, 1.0f} },		// K_EImageOrientation_0
	{ {0.0f, 1.0f}, {1.0f, 0.0f} },		// K_EImageOrientation_90
	{ {1.0f, 1.0f}, {0.0f, 0.0f} },		// K_EImageOrientation_180
	{ {1.0f, 0.0f}, {0.0f, 1.0f} },		// K_EImageOrientation_270
};

//-----------------------------------------------------------------------------
// Purpose: snag the image to use
//-----------------------------------------------------------------------------
void CImagePanel::Paint()
{
	VPROF_BUDGET_DETAILED( "CImagePanel::Paint", VPROF_BUDGETGROUP_TENFOOT );

	// If the image is unloaded we shouldn't be painting... but if we are we better reload
	if ( BReadyForDisplay() && m_bImageUnloaded )
		ReloadImages();

	BaseClass::Paint();
	if ( m_pImage )
	{
		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( GetActualLayoutWidth(), GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

		float flWidthAvail = GetActualLayoutWidth() - flLeft - flRight;
		float flHeightAvail = GetActualLayoutHeight() - flTop - flBottom;

		float flImageWidth = 0.0f;
		float flImageHeight = 0.0f;
		if ( m_nVisibleSliceWidth > 0 && m_nVisibleSliceHeight > 0 )
		{
			flImageWidth = MIN( m_nVisibleSliceWidth, m_pImage->GetWidth() );
			flImageHeight = MIN( m_nVisibleSliceHeight, m_pImage->GetHeight() );
		}
		else
		{
			flImageWidth = m_pImage->GetWidth();
			flImageHeight = m_pImage->GetHeight();
		}

		if ( m_eOrientation == K_EImageOrientation_90 || m_eOrientation == K_EImageOrientation_270 )
		{
			// Swap image width / height
			float flTemp = flImageWidth;
			flImageWidth = flImageHeight;
			flImageHeight = flTemp;
		}

		// Zero sized images don't paint
		if ( flImageWidth < 0.01f && flImageHeight < 0.01f )
			return;

		if ( m_reloadParams.m_fScaleFactor > 0.0f )
		{
			// image has already been scaled, so we need to apply only the difference in scales
			flImageWidth *= GetActualUIScaleX() / GetParentWindow()->GetWindowScaleFactor();
			flImageHeight *= GetActualUIScaleY() / GetParentWindow()->GetWindowScaleFactor();
		}
		else
		{
			flImageWidth *= GetActualUIScaleX();
			flImageHeight *= GetActualUIScaleY();
		}

		switch( m_eScaling )
		{
		case k_EImageScalingNone:
			// Don't change image width at all based on avail
			break;
		case k_EImageScalingStretchBoth:
			// Make image match avail
			flImageWidth = flWidthAvail;
			flImageHeight = flHeightAvail;
			break;
		case k_EImageScalingStretchX:
			// Make width match, but leave height alone
			flImageWidth = flWidthAvail;
			break;
		case k_EImageScalingStretchY:
			// Make height match, but leave width alone
			flImageHeight = flHeightAvail;
			break;
		case k_EImageScalingStretchBothToFitPreserveAspectRatio:
			// Stretch both, but preserve aspect ratio
			{
				float flScaleX = flImageWidth > 0.0f ? flWidthAvail / flImageWidth : 1.0f;
				float flScaleY = flImageHeight > 0.0f ? flHeightAvail / flImageHeight : 1.0f;
				float flScaleVal = MIN( flScaleX, flScaleY );

				flImageWidth *= flScaleVal;
				flImageHeight *= flScaleVal;
			}
			break;
		case k_EImageScalingStretchXToFitPreserveAspectRatio:
			// Stretch both, but preserve aspect ratio
			{
				float flScaleX = flImageWidth > 0.0f ? flWidthAvail / flImageWidth : 1.0f;
				
				flImageWidth *= flScaleX;
				flImageHeight *= flScaleX;
			}
			break;
		case k_EImageScalingStretchYToFitPreserveAspectRatio:
			// Stretch both, but preserve aspect ratio
			{
				float flScaleY = flImageHeight > 0.0f ? flHeightAvail / flImageHeight : 1.0f;

				flImageWidth *= flScaleY;
				flImageHeight *= flScaleY;
			}
			break;
		case k_EImageScalingStretchBothToCoverPreserveAspectRatio:
			// Stretch both, but preserve aspect ratio
			{
				float flScaleX = flImageWidth > 0.0f ? flWidthAvail / flImageWidth : 1.0f;
				float flScaleY = flImageHeight > 0.0f ? flHeightAvail / flImageHeight : 1.0f;
				float flScaleVal = MAX( flScaleX, flScaleY );

				flImageWidth *= flScaleVal;
				flImageHeight *= flScaleVal;
			}
			break;
		}

		ImagePanelUV_t panelUVs = s_orientationUVs[m_eOrientation];
		// If image is bigger than panel, need to adjust uv coords
		if ( flImageWidth > flWidthAvail )
		{
			float *pU0 = &panelUVs.m_topLeft.x;
			float *pU1 = &panelUVs.m_bottomRight.x;
			if ( m_eOrientation == K_EImageOrientation_90 || m_eOrientation == K_EImageOrientation_270 )
			{
				pU0 = &panelUVs.m_topLeft.y;
				pU1 = &panelUVs.m_bottomRight.y;
			}

			float uAxis = *pU1 - *pU0;
			if ( m_eHorAlignment == k_EImageHorizontalAlignmentCenter )
			{
				float flOffsetU = uAxis * ( ( ( flImageWidth - flWidthAvail ) / 2.0 ) / flImageWidth );
				*pU0 += flOffsetU;
				*pU1 -= flOffsetU;
			}
			else if ( m_eHorAlignment == k_EImageHorizontalAlignmentRight )
			{
				*pU0 = *pU1 - ( uAxis * ( flWidthAvail / flImageWidth ) );
			}
			else
			{
				// same as align left
				*pU1 = *pU0 + ( uAxis * ( flWidthAvail / flImageWidth ) );
			}

			flImageWidth = flWidthAvail;
		}

		if ( flImageHeight > flHeightAvail )
		{
			float *pV0 = &panelUVs.m_topLeft.y;
			float *pV1 = &panelUVs.m_bottomRight.y;
			if ( m_eOrientation == K_EImageOrientation_90 || m_eOrientation == K_EImageOrientation_270 )
			{
				pV0 = &panelUVs.m_topLeft.x;
				pV1 = &panelUVs.m_bottomRight.x;
			}

			float vAxis = *pV1 - *pV0;
			if ( m_eVerAlignment == k_EImageVerticalAlignmentCenter )
			{
				float flOffsetV = vAxis * ( ( ( flImageHeight - flHeightAvail ) / 2.0 ) / flImageHeight );
				*pV0 += flOffsetV;
				*pV1 -= flOffsetV;
			}
			else if ( m_eVerAlignment == k_EImageVerticalAlignmentBottom )
			{
				*pV0 = *pV1 - ( vAxis * ( flHeightAvail / flImageHeight ) );
			}
			else
			{
				// same as align top
				*pV1 = *pV0 + ( vAxis * ( flHeightAvail / flImageHeight ) );
			}

			flImageHeight = flHeightAvail;
		}

		float x0, y0, x1, y1;
		float xOffset = 0.0f;
		float yOffset = 0.0f;

		switch( m_eHorAlignment )
		{
			case k_EImageHorizontalAlignmentCenter:
				xOffset = (flWidthAvail - flImageWidth)/2.0f;
				break;
			case k_EImageHorizontalAlignmentLeft:
				xOffset = 0.0f;
				break;
			case k_EImageHorizontalAlignmentRight:
				xOffset = flWidthAvail - flImageWidth;
				break;
		}

		switch( m_eVerAlignment )
		{
		case k_EImageVerticalAlignmentCenter:
			yOffset = (flHeightAvail - flImageHeight)/2.0f;
			break;
		case k_EImageVerticalAlignmentTop:
			yOffset = 0.0f;
			break;
		case k_EImageVerticalAlignmentBottom:
			yOffset = flHeightAvail - flImageHeight;
			break;
		}

		x0 = xOffset;
		x1 = xOffset + flImageWidth;
		y0 = yOffset;
		y1 = yOffset + flImageHeight;


		if ( m_nVisibleSliceWidth > 0 && m_nVisibleSliceHeight > 0 )
		{
			float uSliceMin = RemapValClamped( m_nVisibleSliceX, 0, m_pImage->GetWidth(), 0.0f, 1.0f );
			float vSliceMin = RemapValClamped( m_nVisibleSliceY, 0, m_pImage->GetHeight(), 0.0f, 1.0f );
			float uSliceMax = RemapValClamped( m_nVisibleSliceX + m_nVisibleSliceWidth, 0, m_pImage->GetWidth(), 0.0f, 1.0f );
			float vSliceMax = RemapValClamped( m_nVisibleSliceY + m_nVisibleSliceHeight, 0, m_pImage->GetHeight(), 0.0f, 1.0f );

			panelUVs.m_topLeft.x = RemapValClamped( panelUVs.m_topLeft.x, 0.0f, 1.0f, uSliceMin, uSliceMax );
			panelUVs.m_bottomRight.x = RemapValClamped( panelUVs.m_bottomRight.x, 0.0f, 1.0f, uSliceMin, uSliceMax );
			panelUVs.m_topLeft.y = RemapValClamped( panelUVs.m_topLeft.y, 0.0f, 1.0f, vSliceMin, vSliceMax );
			panelUVs.m_bottomRight.y = RemapValClamped( panelUVs.m_bottomRight.y, 0.0f, 1.0f, vSliceMin, vSliceMax );
		}

		AccessRenderEngine()->DrawTexturedRect( m_pImage->GetTexture(), AccessStyle()->GetTexturesSampleMode(), flLeft + x0, flTop + y0,
											flLeft+x1, flTop+y1,
											panelUVs.m_topLeft.x, panelUVs.m_topLeft.y, panelUVs.m_bottomRight.x, panelUVs.m_bottomRight.y/*u0, v0, u1, v1*/ );

		if ( m_pImage->BIsAnimating() )
		{
			if ( flImageWidth != m_flPrevAnimateWidth || flImageHeight != m_flPrevAnimateHeight )
			{
				m_flPrevAnimateWidth = flImageWidth;
				m_flPrevAnimateHeight = flImageHeight;
				InvalidateSizeAndPosition();
			}
			
			SetRepaint( k_EPanelRepaintFull );
		}
	}
}

const char* CImagePanel::GetImageURL()
{
	return m_strSource;
}



#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CImagePanel::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	BaseClass::ValidateClientPanel( validator, pchName );
	VALIDATE_SCOPE();

	// This will be needed for non-shared directly loaded form memory RGBA images that the
	// image loader itself has no reason to hold a reference too, hence the IfNeeded.
	//ValidatePtrIfNeeded( m_pImage );

	ValidateObj( m_strSource );
	ValidateObj( m_strSourceDefault );

	if ( m_pvecSourceSet )
	{
		ValidatePtr( m_pvecSourceSet );
		FOR_EACH_VEC( *m_pvecSourceSet, i )
		{
			ValidateObj( m_pvecSourceSet->Element( i ).m_strSource );
		}
	}
}
#endif

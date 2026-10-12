//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "../layout/stylepropertyfactory.h"
#include "panoramatypes.h"
#include "iuisoundsystem.h"
#ifdef SOURCE2_PANORAMA
#include "enumutils_panorama.h"
#else
#include "enumutils.h"
#endif
#include "tier1/mempool.h"

using namespace panorama;

// This allows parents of transformed children to not have layers just for transform origin... perspective impacting transforms still need a parent layer until further work
#if !defined( SOURCE2_PANORAMA )
ConVar panorama::s_convarPanoramaTransformParentsNoLayerIfNoPerspective( "@panorama_transform_parents_no_layer_if_no_perspective", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
#endif

//-----------------------------------------------------------------------------
// Purpose: Helper to convert between string and EStyleFlags
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( EStyleFlags )
{ k_EStyleFlagNone, "" },
{ k_EStyleFlagHover, "hover" },
{ k_EStyleFlagFocus, "focus" },
{ k_EStyleFlagActive, "active" },
{ k_EStyleFlagDisabled, "disabled" },
{ k_EStyleFlagEnabled, "enabled" },
{ k_EStyleFlagInspect, "inspect" },
{ k_EStyleFlagSelected, "selected" },
{ k_EStyleFlagDescendantFocused, "descendantfocus" },
{ k_EStyleFlagParentDisabled, "parentdisabled" },
{ k_EStyleFlagLayoutLoading, "layoutloading" },
{ k_EStyleFlagLayoutLoadFailed, "layoutfailed" },
{ k_EStyleFlagActivationDisabled, "activationdisabled" },
ENUMSTRINGS_REVERSE( EStyleFlags, k_EStyleFlagNone )

//-----------------------------------------------------------------------------
// Purpose: Appends style flags (EStyleFlags) to a format string
//-----------------------------------------------------------------------------
void AppendStyleFlagsToString( CFmtStr1024 *pfmt, uint unStyleFlags )
{
	for( int i = 0; i < 32; i++ )
	{
		uint unFlag = 1 << i;
		if( unFlag & unStyleFlags )
			pfmt->AppendFormat( ":%s", PchNameFromEStyleFlags( unFlag ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Hash function for CStyleSymbol
//-----------------------------------------------------------------------------
uint32 panorama::HashItem( const CStyleSymbol &item )
{
	return HashInt( item.GetID() );
}


//-----------------------------------------------------------------------------
// Purpose: Updates scale factors on animation keyframe style properties
//-----------------------------------------------------------------------------
void CActiveAnimation::ApplyUIScaleFactor( const Vector &vScaleFactor, const Vector &vParentScaleFactor )
{
	FOR_EACH_MAP_FAST( m_mapFrameData, i )
	{
		VecPropertyFrameData_t *vecFrame = m_mapFrameData.Element( i );
		FOR_EACH_VEC( (*vecFrame), j )
		{
			(*vecFrame)[j].m_pStyleProperty->ApplyUIScaleFactor( vScaleFactor, vParentScaleFactor );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CActiveAnimation::CActiveAnimation( float flAnimationStart, const AnimationProperty_t &animationProperty )
{
	m_flAnimationStartTime = flAnimationStart;
	m_animationData = animationProperty;
	m_mapFrameData.EnsureCapacity( 5 );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CActiveAnimation::~CActiveAnimation()
{
	FOR_EACH_MAP_FAST( m_mapFrameData, iMap )
	{
		VecPropertyFrameData_t *pvecFrames = m_mapFrameData[iMap];
		FOR_EACH_VEC( *pvecFrames, iVec )
		{
			CStylePropertyFactory::FreeStyleProperty( pvecFrames->Element( iVec ).m_pStyleProperty );
		}
		delete pvecFrames;
	}
	m_mapFrameData.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Resets animation
//-----------------------------------------------------------------------------
void CActiveAnimation::Reset()
{
	m_flAnimationStartTime = UIEngine()->GetCurrentFrameTime();
}


//-----------------------------------------------------------------------------
// Purpose: Adds animation frame data
//-----------------------------------------------------------------------------
void CActiveAnimation::AddFrameData( float flPercent, EAnimationTimingFunction eTimingFunction, const CCubicBezierCurve<Vector2D> &cubicBezier, CStyleProperty *pProperty, const Vector &vScaleFactor, const Vector &vParentScaleFactor )
{
	// copy the property
	CStyleProperty *pNewProperty = CStylePropertyFactory::CreateStyleProperty( pProperty->GetPropertySymbol() );
	pProperty->MergeTo( pNewProperty );
	pNewProperty->ApplyUIScaleFactor( vScaleFactor, vParentScaleFactor );

	// insert into our map
	short iMap = m_mapFrameData.Find( pProperty->GetPropertySymbol() );
	if( iMap == m_mapFrameData.InvalidIndex() )
		iMap = m_mapFrameData.Insert( pProperty->GetPropertySymbol(), new VecPropertyFrameData_t() );

	VecPropertyFrameData_t *pvecFrameData = m_mapFrameData[iMap];
	PropertyFrameData_t &frameData = pvecFrameData->Element( pvecFrameData->AddToTail() );
	frameData.m_flPercent = flPercent;
	frameData.m_eTimingFunction = eTimingFunction;
	frameData.m_CubicBezier = cubicBezier;
	frameData.m_pStyleProperty = pNewProperty;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if animation includes style property
//-----------------------------------------------------------------------------
bool CActiveAnimation::BHasFrameDataForProperty( CStyleSymbol symStyleProperty )
{
	int iMap = m_mapFrameData.Find( symStyleProperty );
	if( iMap == m_mapFrameData.InvalidIndex() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Finds property frame data in this animation
//-----------------------------------------------------------------------------
const CActiveAnimation::VecPropertyFrameData_t *CActiveAnimation::GetFrameData( CStyleSymbol symProperty )
{
	int iMap = m_mapFrameData.Find( symProperty );
	if( iMap == m_mapFrameData.InvalidIndex() )
		return NULL;

	return m_mapFrameData.Element( iMap );
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the end time for an animation
//			Will return FLT_MAX if animation never ends
//-----------------------------------------------------------------------------
float CActiveAnimation::CalculateAnimationEndTime() const
{
	if( m_animationData.m_flIteration == k_flFloatInfiniteIteration )
		return FLT_MAX;

	return m_flAnimationStartTime + m_animationData.m_flDelay + (m_animationData.m_flDuration * m_animationData.m_flIteration);
}


//-----------------------------------------------------------------------------
// Purpose: Checks if properties in animation affect the panel's size & position
//-----------------------------------------------------------------------------
void CActiveAnimation::GetAffectedPanelLayoutFlags( CPanelStyle *pPanelStyle, bool *pbAffectsSize, bool *pbAffectsPosition )
{
	*pbAffectsSize = false;
	*pbAffectsPosition = false;

	FOR_EACH_MAP_FAST( m_mapFrameData, i )
	{
		VecPropertyFrameData_t *pData = m_mapFrameData[i];
		Assert( pData && pData->Count() > 0 && pData->Element( 0 ).m_pStyleProperty );
		CStyleProperty *pProperty = pData->Element( 0 ).m_pStyleProperty;

		// bugbug jmccaskey - should pass all the invididual frames to check against each other here
		// so we don't invalidate if something like border is animating it's color but not widths...

		if( pProperty->BInvalidatesSizeAndPosition( NULL ) )
		{
			*pbAffectsSize = true;
			*pbAffectsPosition = true;
			return;
		}

		if( pProperty->BInvalidatesPosition( NULL ) )
			*pbAffectsPosition = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if properties in animation affect only composition for painting, 
// or child/interior painting as well
//-----------------------------------------------------------------------------
bool CActiveAnimation::BAffectsCompositionOnly()
{
	FOR_EACH_MAP_FAST( m_mapFrameData, i )
	{
		VecPropertyFrameData_t *pData = m_mapFrameData[i];
		Assert( pData && pData->Count() > 0 && pData->Element( 0 ).m_pStyleProperty );
		CStyleProperty *pProperty = pData->Element( 0 ).m_pStyleProperty;

		if( !pProperty->BAffectsCompositionOnly() )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if properties in animation affect the panel's size & position
//-----------------------------------------------------------------------------
bool CActiveAnimation::BAffectsPanelLayoutFlags( CPanelStyle *pPanelStyle )
{
	bool bAffectsSize;
	bool bAffectsPosition;
	GetAffectedPanelLayoutFlags( pPanelStyle, &bAffectsSize, &bAffectsPosition );

	return (bAffectsSize || bAffectsPosition);
}

//-----------------------------------------------------------------------------
// Purpose: Changes direction of the animation and continues playing.
//-----------------------------------------------------------------------------
void CActiveAnimation::Reverse( void )
{
	if ( m_animationData.m_eAnimationDirection == k_EAnimationDirectionReverse )
	{
		m_animationData.m_eAnimationDirection = k_EAnimationDirectionNormal;
	}
	else if ( m_animationData.m_eAnimationDirection == k_EAnimationDirectionNormal )
	{
		m_animationData.m_eAnimationDirection = k_EAnimationDirectionReverse;
	}
	else
	{
		return;
	}

	double flCurrentTime = UIEngine()->GetCurrentFrameTime();
	double flTimeExpired = flCurrentTime - m_flAnimationStartTime;
	double flNewFrameRatio = 1.f - ( flTimeExpired / m_animationData.m_flDuration );
	m_flAnimationStartTime = flCurrentTime - m_animationData.m_flDuration * flNewFrameRatio;
}

//-----------------------------------------------------------------------------
// Purpose: Validate
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CActiveAnimation::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_mapFrameData );
	FOR_EACH_MAP_FAST( m_mapFrameData, iMap )
	{
		VecPropertyFrameData_t *pvecFrames = m_mapFrameData[iMap];
		// Don't validate style property ptr itself, it's allocated from a pool that self validates.  Do let it validate its own members.
		FOR_EACH_VEC( *pvecFrames, iVec )
		{
			pvecFrames->Element( iVec ).m_pStyleProperty->Validate( validator, "CStyleProperty" );
		}

		ValidatePtr( pvecFrames );
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Helper for getting actual size of parent, which will expand up to 
// window size if top level
//-----------------------------------------------------------------------------
void panorama::GetParentSizeAvailable( IUIPanel *pPanel, float &flParentWidth, float &flParentHeight, float &flParentPerspective )
{
	IUIWindow *pWindow = pPanel->GetParentWindow();
	flParentWidth = pWindow->GetSurfaceWidth();
	flParentHeight = pWindow->GetSurfaceHeight();
	flParentPerspective = 1000.0f * pPanel->GetActualUIScaleZ();

	IUIPanel *pParent = pPanel->GetParent();
	if( pParent )
	{
		flParentWidth = pParent->GetActualRenderWidth();
		flParentHeight = pParent->GetActualRenderHeight();
		pParent->AccessIUIStyle()->GetPerspective( flParentPerspective );

		float flLeft, flTop, flRight, flBottom;
		pParent->AccessIUIStyle()->GetContentInset( flParentWidth, flParentHeight, false, flLeft, flTop, flRight, flBottom );

		flParentWidth -= flLeft - flRight;
		flParentHeight -= flTop - flBottom;
	}
}


//-----------------------------------------------------------------------------
// Assign control points for animation curve
//-----------------------------------------------------------------------------
void panorama::GetAnimationCurveControlPoints( EAnimationTimingFunction eTransitionEffect, Vector2D vecPoints[4] )
{
	switch( eTransitionEffect )
	{
	case k_EAnimationLinear:
	case k_EAnimationNone:
	case k_EAnimationUnset:
		vecPoints[0].x = 0.0f;
		vecPoints[0].y = 0.0f;
		vecPoints[1].x = 0.25f;
		vecPoints[1].y = 0.25f;
		vecPoints[2].x = 0.75f;
		vecPoints[2].y = 0.75f;
		vecPoints[3].x = 1.0f;
		vecPoints[3].y = 1.0f;
		break;
	case k_EAnimationEase:
		vecPoints[0].x = 0.0f;
		vecPoints[0].y = 0.0f;
		vecPoints[1].x = 0.25f;
		vecPoints[1].y = 0.1f;
		vecPoints[2].x = 0.25f;
		vecPoints[2].y = 1.0f;
		vecPoints[3].x = 1.0f;
		vecPoints[3].y = 1.0f;
		break;
	case k_EAnimationEaseIn:
		vecPoints[0].x = 0.0f;
		vecPoints[0].y = 0.0f;
		vecPoints[1].x = 0.42f;
		vecPoints[1].y = 0.0f;
		vecPoints[2].x = 1.0f;
		vecPoints[2].y = 1.0f;
		vecPoints[3].x = 1.0f;
		vecPoints[3].y = 1.0f;
		break;
	case k_EAnimationEaseOut:
		vecPoints[0].x = 0.0f;
		vecPoints[0].y = 0.0f;
		vecPoints[1].x = 0.0f;
		vecPoints[1].y = 0.0f;
		vecPoints[2].x = 0.58f;
		vecPoints[2].y = 1.0f;
		vecPoints[3].x = 1.0f;
		vecPoints[3].y = 1.0f;
		break;
	case k_EAnimationEaseInOut:
		vecPoints[0].x = 0.0f;
		vecPoints[0].y = 0.0f;
		vecPoints[1].x = 0.42f;
		vecPoints[1].y = 0.0f;
		vecPoints[2].x = 0.58f;
		vecPoints[2].y = 1.0f;
		vecPoints[3].x = 1.0f;
		vecPoints[3].y = 1.0f;
		break;
	default:
		Assert( !"curve parameters are not defined" );
		break;
	}
}

namespace panorama
{

//
// Declare the most frequently used properties early so
// that they have low symbol IDs for indexing small caches.
//

DECLARE_STYLE_PROPERTY( CStylePropertyPosition, position );
DECLARE_STYLE_PROPERTY( CStylePropertyBackgroundImage, background-image );
DECLARE_STYLE_PROPERTY( CStylePropertyOpacity, opacity );
DECLARE_STYLE_PROPERTY( CStylePropertyBackgroundColor, background-color );
DECLARE_STYLE_PROPERTY( CStylePropertyBorder, border );
DECLARE_STYLE_PROPERTY( CStylePropertyOverflow, overflow );
DECLARE_STYLE_PROPERTY_INHERIT( CStylePropertyForegroundColor, color );
DECLARE_STYLE_PROPERTY( CStylePropertyPadding, padding );
DECLARE_STYLE_PROPERTY( CStylePropertyFont, font );
DECLARE_STYLE_PROPERTY( CStylePropertyWashColor, wash-color );
DECLARE_STYLE_PROPERTY( CStylePropertyBoxShadow, box-shadow );
DECLARE_STYLE_PROPERTY( CStylePropertyTextLetterSpacing, letter-spacing );
DECLARE_STYLE_PROPERTY( CStylePropertyTransform3D, transform );
DECLARE_STYLE_PROPERTY( CStylePropertyTextShadow, text-shadow );
DECLARE_STYLE_PROPERTY( CStylePropertyImageShadow, img-shadow );
DECLARE_STYLE_PROPERTY( CStylePropertyScale2DCentered, pre-transform-scale2d );
DECLARE_STYLE_PROPERTY( CStylePropertyTextAlign, text-align );
DECLARE_STYLE_PROPERTY( CStylePropertyZIndex, z-index );
DECLARE_STYLE_PROPERTY( CStylePropertyWhiteSpace, white-space );
DECLARE_STYLE_PROPERTY( CStylePropertyOpacityMask, opacity-mask );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyTextShadow, text-shadow-fast, symbolFast );

//
// Now declare the remaining properties.
//

DECLARE_STYLE_PROPERTY( CStylePropertyBackgroundImgOpacity, background-img-opacity );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyPosition, x, symbolX );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyPosition, y, symbolY );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyPosition, z, symbolZ );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyOpacityMask, opacity-mask-scroll-up, symbolScrollUp );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyOpacityMask, opacity-mask-scroll-down, symbolScrollDown );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyOpacityMask, opacity-mask-scroll-up-down, symbolScrollUpDown );
DECLARE_STYLE_PROPERTY( CStylePropertyHueShift, hue-rotation );
DECLARE_STYLE_PROPERTY( CStylePropertySaturation, saturation );
DECLARE_STYLE_PROPERTY( CStylePropertyBrightness, brightness );
DECLARE_STYLE_PROPERTY( CStylePropertyContrast, contrast );
DECLARE_STYLE_PROPERTY( CStylePropertyBlur, blur );

DECLARE_STYLE_PROPERTY( CStylePropertyRotate2DCentered, pre-transform-rotate2d );

// can be inherited, just not through standard mechanism. Call CPanelStyle::GetFontStyle()
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyFont, font-family, fontFamily );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyFont, font-size, fontSize );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyFont, font-style, fontStyle );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyFont, font-weight, fontWeight );

// text-decoration is not inherited by default in CSS
DECLARE_STYLE_PROPERTY( CStylePropertyTextDecoration, text-decoration );
DECLARE_STYLE_PROPERTY( CStylePropertyTextTransform, text-transform );

DECLARE_STYLE_PROPERTY( CStylePropertyTextOverflow, text-overflow );

DECLARE_STYLE_PROPERTY( CStylePropertyMixBlendMode, -s2-mix-blend-mode );
DECLARE_STYLE_PROPERTY( CStylePropertyTextureSampleMode, texture-sampling );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-top, symTop );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-right, symRight );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-bottom, symBottom );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-left, symLeft );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-style, symStyle )
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-top-style, symTopStyle );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-right-style, symRightStyle );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-bottom-style, symBottomStyle );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-left-style, symLeftStyle );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-width, symWidth );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-top-width, symTopWidth );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-right-width, symRightWidth );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-bottom-width, symBottomWidth );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-left-width, symLeftWidth );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-color, symColor );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-top-color, symTopColor );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-right-color, symRightColor );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-bottom-color, symBottomColor );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorder, border-left-color, symLeftColor );

DECLARE_STYLE_PROPERTY( CStylePropertyBorderRadius, border-radius );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorderRadius, border-top-right-radius, topRightRadius );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorderRadius, border-bottom-right-radius, bottomRightRadius );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorderRadius, border-bottom-left-radius, bottomLeftRadius );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBorderRadius, border-top-left-radius, topLeftRadius );

DECLARE_STYLE_PROPERTY( CStylePropertyClip, clip );

DECLARE_STYLE_PROPERTY_INHERIT( CStylePropertyLineHeight, line-height );

DECLARE_STYLE_PROPERTY( CStylePropertyPerspective, perspective );
DECLARE_STYLE_PROPERTY( CStylePropertyPerspectiveOrigin, perspective-origin );
DECLARE_STYLE_PROPERTY( CStylePropertyTransformOrigin, transform-origin );
DECLARE_STYLE_PROPERTY( CStylePropertyWidth, width );
DECLARE_STYLE_PROPERTY( CStylePropertyHeight, height );
DECLARE_STYLE_PROPERTY( CStylePropertyVisible, visibility );
DECLARE_STYLE_PROPERTY( CStylePropertyFlowChildren, flow-children );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBackgroundImage, background-size, backgroundSize );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBackgroundImage, background-position, backgroundPosition );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyBackgroundImage, background-repeat, backgroundRepeat );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyPadding, padding-left, symbolLeft );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyPadding, padding-top, symbolTop );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyPadding, padding-bottom, symbolBottom );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyPadding, padding-right, symbolRight );

DECLARE_STYLE_PROPERTY( CStylePropertyMargin, margin );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyMargin, margin-left, symbolLeft );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyMargin, margin-top, symbolTop );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyMargin, margin-bottom, symbolBottom );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyMargin, margin-right, symbolRight );

DECLARE_STYLE_PROPERTY( CStylePropertyTransitionProperties, transition );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyTransitionProperties, transition-property, symbolProperty );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyTransitionProperties, transition-duration, symbolDuration );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyTransitionProperties, transition-timing-function, symbolTiming );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyTransitionProperties, transition-delay, symbolDelay );

DECLARE_STYLE_PROPERTY( CStylePropertyAnimationProperties, animation );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAnimationProperties, animation-name, symbolName );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAnimationProperties, animation-duration, symbolDuration );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAnimationProperties, animation-timing-function, symbolTiming );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAnimationProperties, animation-iteration-count, symbolIteration );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAnimationProperties, animation-direction, symbolDirection );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAnimationProperties, animation-delay, symbolDelay );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAnimationProperties, animation-fill-mode, symbolFillMode );

DECLARE_STYLE_PROPERTY( CStylePropertyAlign, align );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAlign, horizontal-align, symbolHorizontal );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyAlign, vertical-align, symbolVertical );

DECLARE_STYLE_PROPERTY( CStylePropertyMinWidth, min-width );
DECLARE_STYLE_PROPERTY( CStylePropertyMinHeight, min-height );
DECLARE_STYLE_PROPERTY( CStylePropertyMaxWidth, max-width );
DECLARE_STYLE_PROPERTY( CStylePropertyMaxHeight, max-height );

DECLARE_STYLE_PROPERTY( CStylePropertyTooltipPosition, tooltip-position );
DECLARE_STYLE_PROPERTY( CStylePropertyTooltipBodyPosition, tooltip-body-position );
DECLARE_STYLE_PROPERTY( CStylePropertyTooltipArrowPosition, tooltip-arrow-position );

DECLARE_STYLE_PROPERTY( CStylePropertyContextMenuPosition, context-menu-position );
DECLARE_STYLE_PROPERTY( CStylePropertyContextMenuBodyPosition, context-menu-body-position );
DECLARE_STYLE_PROPERTY( CStylePropertyContextMenuArrowPosition, context-menu-arrow-position );

DECLARE_STYLE_PROPERTY( CStylePropertyEntranceSound, sound );
DECLARE_STYLE_PROPERTY( CStylePropertyExitSound, sound-out );
DECLARE_STYLE_PROPERTY( CStylePropertyTransitionSound, sound-trans );

DECLARE_STYLE_PROPERTY( CStylePropertyUIScale, ui-scale );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyUIScale, ui-scale-x, symbolX );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyUIScale, ui-scale-y, symbolY );
DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyUIScale, ui-scale-z, symbolZ );

DECLARE_STYLE_PROPERTY_ALIAS( CStylePropertyWashColor, wash-color-fast, symbolFast );

}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CStylePropertyBackgroundImage::CStylePropertyBackgroundImage() : CStyleProperty( CStylePropertyBackgroundImage::symbol )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CStylePropertyBackgroundImage::~CStylePropertyBackgroundImage()
{
	Clear();
}


//-----------------------------------------------------------------------------
// Purpose: Removes all layers
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::Clear()
{
	FOR_EACH_VEC( m_vecLayers, i )
	{
		delete m_vecLayers[i];
	}
	m_vecLayers.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Adds a layer
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::AddLayer()
{
	m_vecLayers.AddToTail( new CBackgroundImageLayer() );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a layer if necessary. Should only request 1 past end if adding
//-----------------------------------------------------------------------------
CBackgroundImageLayer *CStylePropertyBackgroundImage::GetOrAddLayer( int i )
{
	Assert( m_vecLayers.Count() >= i );
	if( m_vecLayers.Count() < i + 1 )
		AddLayer();

	return m_vecLayers[i];
}


//-----------------------------------------------------------------------------
// Purpose: Removes unset layers at end of layer list
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::RemoveUnsetLayers()
{
	FOR_EACH_VEC_BACK( m_vecLayers, i )
	{
		CBackgroundImageLayer *pLayer = m_vecLayers[i];
		if( pLayer->IsCompletelyUnset() )
		{
			m_vecLayers.Remove( i );
			delete pLayer;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Merges data to the target, this will only overwrite unset properties, and shouldn't clobber already set ones.
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::MergeTo( CStyleProperty *pTarget ) const
{
	if ( !BMergeToCommon( pTarget ) )
		return;

	CStylePropertyBackgroundImage *p = (CStylePropertyBackgroundImage *)pTarget;

	// make sure the target has at least as many layers as we do
	p->m_vecLayers.EnsureCapacity( m_vecLayers.Count() );
	for( int i = p->m_vecLayers.Count(); i < m_vecLayers.Count(); i++ )
		p->AddLayer();

	// use the first layer in the target 
	FOR_EACH_VEC( m_vecLayers, i )
	{
		m_vecLayers[i]->MergeTo( p->m_vecLayers[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Interpolation func for animation of this property
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if ( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyBackgroundImage::Interpolate" );
		return;
	}

	const CStylePropertyBackgroundImage *pTarget = ( const CStylePropertyBackgroundImage * )&target;

	CUtlVector< CBackgroundImageLayer * > vecLayers;

	FOR_EACH_VEC( pTarget->m_vecLayers, i )
	{
		CBackgroundImageLayer *pTargetLayer = pTarget->m_vecLayers[ i ];

		bool bFoundExistingLayer = false;
		FOR_EACH_VEC( m_vecLayers, j )
		{
			CBackgroundImageLayer *pExistingLayer = m_vecLayers[ j ];

			if ( *pExistingLayer == *pTargetLayer )
			{
				// todo(ericl): BUG - if you toggle back and forth between two images too quickly without letting the transition finish,
				// the opacity gets screwed up.  I think the logic here of looking at the target layer's interpolated opacity is incorrect
				// in some cases, but I'm checking in for now since it works in common cases. Can fix later.
				float flOpacity = pTargetLayer->GetInterpolatedOpacity( flProgress );
				pExistingLayer->SetTemporaryLayer( pTargetLayer->IsTemporaryLayer() );
				pExistingLayer->SetOpacity( flOpacity );
				m_vecLayers.Remove( j );
				vecLayers.AddToTail( pExistingLayer );
				bFoundExistingLayer = true;
				break;
			}
		}

		if ( !bFoundExistingLayer )
		{
			CBackgroundImageLayer *pLayer = new CBackgroundImageLayer();
			pLayer->Set( *pTargetLayer );
			pLayer->SetOpacity( pTargetLayer->GetInterpolatedOpacity( flProgress ) );
			vecLayers.AddToTail( pLayer );
		}
	}

	// Append any remaining to the front
	FOR_EACH_VEC_BACK( m_vecLayers, i )
	{
		CBackgroundImageLayer *pExistingLayer = m_vecLayers[ i ];
		float flOpacity = pExistingLayer->GetInterpolatedOpacity( flProgress );
		pExistingLayer->SetTemporaryLayer( true );
		pExistingLayer->SetOpacity( flOpacity );
		vecLayers.AddToHead( pExistingLayer );
	}

	// Now swap in the new vector
	m_vecLayers.RemoveAll();
	m_vecLayers.Swap( vecLayers );
}


//-----------------------------------------------------------------------------
// Purpose: Called when starting to transition to this property
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::OnStartingTransition( CStyleProperty *pPreviousStyleProperty )
{
	if ( !pPreviousStyleProperty )
		return;

	if ( pPreviousStyleProperty->GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyBackgroundImage::OnStartingTransition" );
		return;
	}

	const CStylePropertyBackgroundImage *pPreviousBackgroundImageProperty = ( const CStylePropertyBackgroundImage * )pPreviousStyleProperty;

	CUtlVector< CBackgroundImageLayer * > vecLayers;

	// 
	FOR_EACH_VEC( pPreviousBackgroundImageProperty->m_vecLayers, i )
	{
		CBackgroundImageLayer *pPreviousLayer = pPreviousBackgroundImageProperty->m_vecLayers[ i ];

		bool bFoundExistingLayer = false;
		FOR_EACH_VEC( m_vecLayers, j )
		{
			CBackgroundImageLayer *pExistingLayer = m_vecLayers[ j ];

			if ( *pExistingLayer == *pPreviousLayer )
			{
				vecLayers.AddToTail( pExistingLayer );
				m_vecLayers.Remove( j );
				bFoundExistingLayer = true;
				break;
			}
		}

		if ( !bFoundExistingLayer )
		{
			CBackgroundImageLayer *pLayer = new CBackgroundImageLayer();
			pLayer->Set( *pPreviousLayer );
			pLayer->SetTemporaryLayer( true );
			pLayer->SetOpacity( 0.0f );
			vecLayers.AddToTail( pLayer );
		}
	}

	// Append any remaining to the back
	vecLayers.AddVectorToTail( m_vecLayers );
	m_vecLayers.RemoveAll();
	m_vecLayers.Swap( vecLayers );
}


//-----------------------------------------------------------------------------
// Purpose: Called when finished transitioning with this property
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::OnFinishedTransition()
{
	// Transitions are all done, so we can delete any temporary layers and set opacity for our normal layers to 1.0;
	FOR_EACH_VEC_BACK( m_vecLayers, i )
	{
		CBackgroundImageLayer *pLayer = m_vecLayers[ i ];

		if ( pLayer->IsTemporaryLayer() )
		{
			delete pLayer;
			m_vecLayers.Remove( i );
		}
		else
		{
			pLayer->SetOpacity( 1.0f );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Parses string and sets value
//-----------------------------------------------------------------------------
bool CStylePropertyBackgroundImage::BSetFromString( CStyleSymbol symParsedName, const char *pchString )
{
	if( symParsedName == symbol )
	{
		int iPath = 0;
		pchString = CSSHelpers::SkipSpaces( pchString );
		while( pchString[0] != '\0' )
		{
			if( V_stricmp( "none", pchString ) == 0 )
			{
				GetOrAddLayer( iPath )->SetPathToNone();
				pchString += 4;
			}
			else
			{
				CUtlString sURLPath;
				if( !CSSHelpers::BParseURL( sURLPath, pchString, &pchString ) || !sURLPath.IsValid() )
					return false;

				GetOrAddLayer( iPath )->SetPath( sURLPath.String() );
			}

			CSSHelpers::BSkipComma( pchString, &pchString );
			pchString = CSSHelpers::SkipSpaces( pchString );
			iPath++;
		}

		// mark additional layers as unset
		for( int i = iPath; i < m_vecLayers.Count(); i++ )
			m_vecLayers[i]->SetPathUnset();
	}
	else if( symParsedName == backgroundPosition )
	{
		CUtlVector< CBackgroundPosition > vec;
		if( !CSSHelpers::BParseCommaSepList( &vec, CSSHelpers::BParseBackgroundPosition, pchString ) || vec.Count() == 0 )
			return false;

		FOR_EACH_VEC( vec, i )
		{
			GetOrAddLayer( i )->SetPosition( vec[i] );
		}

		// mark additional layers as unset
		CBackgroundPosition unset;
		for( int i = vec.Count(); i < m_vecLayers.Count(); i++ )
			m_vecLayers[i]->SetPosition( unset );
	}
	else if( symParsedName == backgroundSize )
	{
		const char k_rgchAuto[] = "auto";
		const char k_rgchContain[] = "contain";
		const char k_rgchCover[] = "cover";
		const char k_rgchClipThenCover[] = "clip_then_cover";

		int iPath = 0;
		pchString = CSSHelpers::SkipSpaces( pchString );
		while( pchString[0] != '\0' )
		{
			CUILength width;
			EBackgroundSizeConstant eConstant = k_EBackgroundSizeConstantNone;

			// check for auto
			if( V_strnicmp( pchString, k_rgchAuto, V_ARRAYSIZE( k_rgchAuto ) - 1 ) == 0 )
			{
				width.SetLength( k_flFloatAuto );
				pchString += V_ARRAYSIZE( k_rgchAuto ) - 1;
			}
			else if( V_strnicmp( pchString, k_rgchContain, V_ARRAYSIZE( k_rgchContain ) - 1 ) == 0 )
			{
				eConstant = k_EBackgroundSizeConstantContain;
				pchString += V_ARRAYSIZE( k_rgchContain ) - 1;
			}
			else if( V_strnicmp( pchString, k_rgchCover, V_ARRAYSIZE( k_rgchCover ) - 1 ) == 0 )
			{
				eConstant = k_EBackgroundSizeConstantCover;
				pchString += V_ARRAYSIZE( k_rgchCover ) - 1;
			}
			else if ( V_strnicmp( pchString, k_rgchClipThenCover, V_ARRAYSIZE( k_rgchClipThenCover ) - 1 ) == 0 )
			{
				eConstant = k_EBackgroundSizeConstantClipThenCover;
				pchString += V_ARRAYSIZE( k_rgchClipThenCover ) - 1;
			}
			else
			{
				// look for lengths
				if( !CSSHelpers::BParseIntoUILength( &width, pchString, &pchString ) )
					return false;
			}

			// check for end of this section
			pchString = CSSHelpers::SkipSpaces( pchString );
			if( pchString[0] == '\0' || pchString[0] == ',' )
			{
				if( eConstant != k_EBackgroundSizeConstantNone )
					GetOrAddLayer( iPath )->SetBackgroundSize( eConstant );
				else
					GetOrAddLayer( iPath )->SetBackgroundSize( width, width );

				CSSHelpers::BSkipComma( pchString, &pchString );
				pchString = CSSHelpers::SkipSpaces( pchString );
				iPath++;
				continue;
			}

			// constants only have 1 param
			if( eConstant != k_EBackgroundSizeConstantNone )
				return false;

			// should be another param
			CUILength height;
			if( V_strnicmp( pchString, k_rgchAuto, V_ARRAYSIZE( k_rgchAuto ) - 1 ) == 0 )
			{
				height.SetLength( k_flFloatAuto );
				pchString += V_ARRAYSIZE( k_rgchAuto ) - 1;
			}
			else
			{
				// look for lengths			
				if( !CSSHelpers::BParseIntoUILength( &height, pchString, &pchString ) )
					return false;
			}

			GetOrAddLayer( iPath )->SetBackgroundSize( width, height );

			CSSHelpers::BSkipComma( pchString, &pchString );
			pchString = CSSHelpers::SkipSpaces( pchString );
			iPath++;
		}

		// mark additional layers as unset
		CUILength unset;
		for( int i = iPath; i < m_vecLayers.Count(); i++ )
			m_vecLayers[i]->SetBackgroundSize( unset, unset );
	}
	else if( symParsedName == backgroundRepeat )
	{
		CUtlVector< CBackgroundRepeat > vec;
		if( !CSSHelpers::BParseCommaSepList( &vec, CSSHelpers::BParseBackgroundRepeat, pchString ) || vec.Count() == 0 )
			return false;

		FOR_EACH_VEC( vec, i )
		{
			GetOrAddLayer( i )->SetRepeat( vec[i] );
		}

		// mark additional layers as unset
		CBackgroundRepeat unset;
		for( int i = vec.Count(); i < m_vecLayers.Count(); i++ )
			m_vecLayers[i]->SetRepeat( unset );
	}
	else
	{
		// unkown symbol
		return false;
	}

	RemoveUnsetLayers();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Gets string representation of property
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::ToString( CFmtStr1024 *pfmtBuffer ) const
{
	FOR_EACH_VEC( m_vecLayers, i )
	{
		if( i > 0 )
			pfmtBuffer->Append( ", " );

		m_vecLayers[i]->ToString( pfmtBuffer );
	}
}


//-----------------------------------------------------------------------------
// Purpose: When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
//			by looking at lower weight styles
//-----------------------------------------------------------------------------
bool CStylePropertyBackgroundImage::BFullySet() const
{
	// just need to check the first layer to make sure all values are set
	if( m_vecLayers.Count() < 1 )
		return false;

	return m_vecLayers[0]->IsSet();
}


//-----------------------------------------------------------------------------
// Purpose: called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::ResolveDefaultValues()
{
	RemoveUnsetLayers();

	// if we have no background paths, clear all and return
	bool bFoundPath = false;
	FOR_EACH_VEC( m_vecLayers, i )
	{
		const char *pchPath = m_vecLayers[i]->GetPath();
		if( pchPath[0] != '\0' )
		{
			bFoundPath = true;
			break;
		}
	}

	if( !bFoundPath )
	{
		Clear();
		return;
	}

	// need to track how many values we have seen per property
	int cPath = 0;
	int cSize = 0;
	int cPosition = 0;
	int cRepeat = 0;
	FOR_EACH_VEC( m_vecLayers, i )
	{
		CBackgroundImageLayer *pLayer = m_vecLayers[i];
		if( pLayer->IsPathSet() )
			cPath++;

		if( pLayer->GetHeight().IsSet() && pLayer->GetWidth().IsSet() )
			cSize++;

		if( pLayer->GetPosition().IsSet() )
			cPosition++;

		if( pLayer->GetRepeat().IsSet() )
			cRepeat++;
	}


	// set defaults. If a layer's property is unset, but at least one previous layer has the property set, need to repeat set properties
	// ex:
	//	background-image: url( file1 ), url( file2), url( file3 ), url(file4 )
	//	background-repeat: repeat, space
	//
	// repeat should turn into:
	//	background-repeat: repeat, space, repeat, space

	FOR_EACH_VEC( m_vecLayers, i )
	{
		CBackgroundImageLayer *pLayer = m_vecLayers[i];
		if( !pLayer->IsPathSet() )
			pLayer->SetPathToNone();

		// size
		if( !pLayer->GetHeight().IsSet() || !pLayer->GetWidth().IsSet() )
		{
			if( cSize > 0 )
			{
				CBackgroundImageLayer *pFrom = m_vecLayers[i % cSize];
				if( pFrom->GetBackgroundSizeConstant() != k_EBackgroundSizeConstantNone )
					m_vecLayers[i]->SetBackgroundSize( pFrom->GetBackgroundSizeConstant() );
				else
					m_vecLayers[i]->SetBackgroundSize( pFrom->GetWidth(), pFrom->GetHeight() );
			}
			else
			{
				CUILength lenAuto;
				lenAuto.SetLength( k_flFloatAuto );
				m_vecLayers[i]->SetBackgroundSize( lenAuto, lenAuto );
			}
		}

		// position
		if( !pLayer->GetPosition().IsSet() )
		{
			if( cPosition > 0 )
			{
				CBackgroundImageLayer *pFrom = m_vecLayers[i % cPosition];
				m_vecLayers[i]->SetPosition( pFrom->GetPosition() );
			}
			else
			{
				CBackgroundPosition pos;
				pos.ResolveDefaultValues();
				m_vecLayers[i]->SetPosition( pos );
			}
		}

		// repeat
		if( !pLayer->GetRepeat().IsSet() )
		{
			if( cRepeat > 0 )
			{
				CBackgroundImageLayer *pFrom = m_vecLayers[i % cRepeat];
				m_vecLayers[i]->SetRepeat( pFrom->GetRepeat() );
			}
			else
			{
				CBackgroundRepeat repeat;
				repeat.ResolveDefaultValues();
				m_vecLayers[i]->SetRepeat( repeat );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when we are ready to apply any scaling factor to the values
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::ApplyUIScaleFactor( const Vector &vScaleFactor, const Vector &vParentScaleFactor )
{
	FOR_EACH_VEC( m_vecLayers, i )
	{
		m_vecLayers[i]->ApplyUIScaleFactor( vScaleFactor.x, vScaleFactor.y );
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when applied to a panel after comparing with set values
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::OnAppliedToPanel( IUIPanel *pPanel )
{
	FOR_EACH_VEC( m_vecLayers, i )
	{
		m_vecLayers[i]->OnAppliedToPanel( pPanel );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns description text for each property
//-----------------------------------------------------------------------------
const char *CStylePropertyBackgroundImage::GetDescription( CStyleSymbol symProperty )
{
	if( symProperty == CStylePropertyBackgroundImage::symbol )
	{
		return	"Comma separated list of images or movies to draw in the background. Can specify \"none\" to not draw a background layer. Combined with background-position, background-size and background-repeat values.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"background-image: url(\"file://{images}/default.tga\"), url( \"file://{movies}/Background1080p.webm\" );"
			"</pre>";
	}
	else if( symProperty == CStylePropertyBackgroundImage::backgroundSize )
	{
		return	"Sets the horizontal and vertical dimensions used to draw the background image. Can be set in pixels, percent, \"contains\" to size down to panel dimensions or \"auto\" preserves the image aspect ratio. By default, set to \"auto\" which preveres the image's original size.<br><br>"
			"Multiple background layers can be specified in a comma separated list, which are then combined with background-image, background-position, and background-repeat values.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"background-size: auto; // same as \"auto auto\" (default) \n"
			"background-size: 100% 100%; // image fills the panel\n"
			"background-size: 50% 75%; // image fills 50% of the panel's width, and 75% of the panel's height\n"
			"background-size: 300px 200px; // image is drawn 300px wide, 200px tall"
			"</pre>";
	}
	else if( symProperty == CStylePropertyBackgroundImage::backgroundPosition )
	{
		return	"Controls the horizontal and vertical placement of the background image, with the format: &lt;left|center|right&gt; &lt;horizontal length&gt; &lt;top|center|bottom&gt; &lt;vertical length&gt;<br><br>"
			"If length is a percent, the specified location within the image is positioned over that same specified position in the background. If the length is pixels, the top left corner is placed relative to the provided alignment keywords (left, bottom, etc.). See examples for more details.<br><br>"
			"If 1 value is specified, the other value is assumed to be center. If 2 values are specified, the first value must be for horizontal placement and the second for vertical.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"// aligns the top left corner of the image with the top left corner of the panel (default)\n"
			"background-position: 0% 0%;\n\n"
			"// centers the image within the background (same as \"center center\")\n"
			"background-position: center;\n\n"
			"// aligns the bottom right corner of the image with the bottom right corner of the panel (same as \"100% 100%\")\n"
			"background-position: right bottom;\n\n"
			"// the top left corner of the image is placed 10px to the right of, 40px below the top left corner of the panel\n"
			"background-position: left 10px top 40px;"
			"</pre>";
	}
	else if( symProperty == CStylePropertyBackgroundImage::backgroundRepeat )
	{
		return	"Controls if the background should be repeated in the horizontal and vertical directions.<br><br>"
			"Possible values per direction:<br>"
			"\"repeat\" - (default) Repeated in the specified direction until it fills the panel<br>"
			"\"space\" - Repeated as many times as required to fill the panel w/o being clipped. Space is added between images to to align first and last image with panel edges.<br>"
			"\"round\" - Repeated as many times as required to fill the panel w/o being clipped. The image is resized to align first and last image with panel edges.<br>"
			"\"no-repeat\" - Not repeated<br><br>"
			"Possible single values:<br>"
			"\"repeat-x\" - equals \"repeat no-repeat\"<br>"
			"\"repeat-y\" - equals \"no-repeat repeat\"<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"background-repeat: repeat; // equals \"repeat repeat\" (default)\n"
			"background-repeat: repeat space; // repeats horizontally, spaces vertically \n"
			"background-repeat: no-repeat round; // 1 column of images, scaled to fit evenly"
			"</pre>";
	}

	return CStyleProperty::GetDescription( symProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Check equality
//-----------------------------------------------------------------------------
bool CStylePropertyBackgroundImage::operator==(const CStyleProperty &other) const
{
	if( GetPropertySymbol() != other.GetPropertySymbol() )
		return false;

	const CStylePropertyBackgroundImage &rhs = (const CStylePropertyBackgroundImage &)other;

	// Check if all layers match, skipping temporary layers
	int iSelf = 0;
	int iOther = 0;
	while ( iSelf < m_vecLayers.Count() || iOther < rhs.m_vecLayers.Count() )
	{
		CBackgroundImageLayer *pSelfLayer = iSelf < m_vecLayers.Count() ? m_vecLayers[ iSelf ] : nullptr;
		if ( pSelfLayer && pSelfLayer->IsTemporaryLayer() )
		{
			iSelf++;
			continue;
		}

		CBackgroundImageLayer *pOtherLayer = iOther < rhs.m_vecLayers.Count() ? rhs.m_vecLayers[ iOther ] : nullptr;
		if ( pOtherLayer && pOtherLayer->IsTemporaryLayer() )
		{
			iOther++;
			continue;
		}

		if ( !pSelfLayer || !pOtherLayer )
			return false;

		if ( *pSelfLayer != *pOtherLayer )
			return false;

		iSelf++;
		iOther++;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if one of the layers is an active movie
//-----------------------------------------------------------------------------
bool CStylePropertyBackgroundImage::BHasActiveMovie()
{
	FOR_EACH_VEC( m_vecLayers, i )
	{
		CVideoPlayerPtr pPlayer = m_vecLayers[i]->GetMovie();
		if( pPlayer.IsValid() && pPlayer->GetPlaybackState() != k_EVideoPlayerPlaybackStateStop )
			return true;
		else
		{
			IImageSource *pImage = m_vecLayers[i]->GetImage();
			if( pImage && pImage->BIsAnimating() )
				return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables background movie playback
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::EnableBackgroundMovies( bool bEnable )
{
	FOR_EACH_VEC( m_vecLayers, i )
	{
		CVideoPlayerPtr pPlayer = m_vecLayers[i]->GetMovie();
		if( !pPlayer )
			continue;

		EVideoPlayerPlaybackState ePlaybackState = pPlayer->GetPlaybackState();
		if( !bEnable && ePlaybackState != k_EVideoPlayerPlaybackStateStop )
			pPlayer->Stop();

		if( bEnable && ePlaybackState == k_EVideoPlayerPlaybackStateStop )
			pPlayer->Play();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if one of the layers is an active movie
//-----------------------------------------------------------------------------
void CStylePropertyBackgroundImage::Set( const CUtlVector< CBackgroundImageLayer * > &vecLayers )
{
	Clear();

	m_vecLayers.EnsureCapacity( vecLayers.Count() );
	FOR_EACH_VEC( vecLayers, i )
	{
		CBackgroundImageLayer *pLayer = new CBackgroundImageLayer();
		pLayer->Set( *vecLayers[i] );

		m_vecLayers.AddToTail( pLayer );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CStylePropertyBackgroundImage::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	CStyleProperty::Validate( validator, pchName );

	ValidateObj( m_vecLayers );
	FOR_EACH_VEC( m_vecLayers, i )
	{
		ValidatePtr( m_vecLayers[i] );
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Resolve default values
//-----------------------------------------------------------------------------
void CStylePropertyOpacityMask::OnAppliedToPanel( IUIPanel *pPanel )
{
	IUIImageManager *pImageManager = pPanel->UIImageManager();
	if ( !pImageManager )
	{
		return;
	}
		
	if( m_sURL.IsValid() && !m_pImage )
		m_pImage = pImageManager->LoadImageFromURL( pPanel, NULL, m_sURL, true, k_EImageFormatA8 );

	if( m_sURLUp.IsValid() && !m_pImageUp )
		m_pImageUp = pImageManager->LoadImageFromURL( pPanel, NULL, m_sURLUp, true, k_EImageFormatA8 );

	if( m_sURLDown.IsValid() && !m_pImageDown )
		m_pImageDown = pImageManager->LoadImageFromURL( pPanel, NULL, m_sURLDown, true, k_EImageFormatA8 );

	if( m_sURLUpDown.IsValid() && !m_pImageUpDown )
		m_pImageUpDown = pImageManager->LoadImageFromURL( pPanel, NULL, m_sURLUpDown, true, k_EImageFormatA8 );
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CStylePropertyOpacityMask::Validate( CValidator &validator, const tchar *pchName )
{
	CStyleProperty::Validate( validator, pchName );

	VALIDATE_SCOPE();
	ValidateObj( m_sURL );
	ValidateObj( m_sURLUp );
	ValidateObj( m_sURLDown );
	ValidateObj( m_sURLUpDown );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Interpolation func for animation of this property
//-----------------------------------------------------------------------------
void CStylePropertyTransform3D::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyTransform3D::Interpolate" );
		return;
	}

	const CStylePropertyTransform3D *pTarget = (const CStylePropertyTransform3D *)&target;

	float flWidth = 0.0f;
	float flHeight = 0.0f;
	IUIPanel *pParent = pPanel->GetParent();
	if( pParent )
	{
		flWidth = pParent->GetActualRenderWidth();
		flHeight = pParent->GetActualRenderHeight();
	}

	VMatrix matCurrent = GetTransformMatrix( flWidth, flHeight );
	VMatrix matTransform = pTarget->GetTransformMatrix( flWidth, flHeight );

	m_vecTransforms.PurgeAndDeleteElements();
	m_Matrix = InterpolateTransformMatrix( matCurrent, matTransform, flProgress );

	m_bDirty = false;
	m_bInterpolated = true;
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for box shadows
//-----------------------------------------------------------------------------
void CStylePropertyBoxShadow::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyBoxShadow::Interpolate" );
		return;
	}

	CStylePropertyBoxShadow *p = (CStylePropertyBoxShadow *)&target;
	if( p->m_bInset != m_bInset || p->m_bFill != m_bFill )
	{
		// Don't support transition between inset/outset for now, just snap, same for fill vs no fill
		p->MergeTo( this );
	}
	else
	{
		// Interpolate distances and color
		m_HorizontalOffset = LerpUILength( flProgress, m_HorizontalOffset, p->m_HorizontalOffset, pPanel->GetActualRenderWidth() );
		m_VerticalOffset = LerpUILength( flProgress, m_VerticalOffset, p->m_VerticalOffset, pPanel->GetActualRenderHeight() );
		m_SpreadDistance = LerpUILength( flProgress, m_SpreadDistance, p->m_SpreadDistance, (pPanel->GetActualRenderHeight() + pPanel->GetActualRenderWidth()) / 2.0f );
		m_BlurRadius = LerpUILength( flProgress, m_BlurRadius, p->m_BlurRadius, (pPanel->GetActualRenderHeight() + pPanel->GetActualRenderWidth()) / 2.0f );

		float r = Lerp( flProgress, (float)m_ShadowColor.r(), (float)p->m_ShadowColor.r() );
		float g = Lerp( flProgress, (float)m_ShadowColor.g(), (float)p->m_ShadowColor.g() );
		float b = Lerp( flProgress, (float)m_ShadowColor.b(), (float)p->m_ShadowColor.b() );
		float a = Lerp( flProgress, (float)m_ShadowColor.a(), (float)p->m_ShadowColor.a() );
		m_ShadowColor.SetColor( RoundFloatToInt( r ), RoundFloatToInt( g ), RoundFloatToInt( b ), RoundFloatToInt( a ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for text shadows
//-----------------------------------------------------------------------------
void CStylePropertyTextShadow::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if ( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyTextShadow::Interpolate" );
		return;
	}

	CStylePropertyTextShadow *p = (CStylePropertyTextShadow *)&target;
	
	// Interpolate distances and color
	m_HorizontalOffset = LerpUILength( flProgress, m_HorizontalOffset, p->m_HorizontalOffset, pPanel->GetActualRenderWidth() );
	m_VerticalOffset = LerpUILength( flProgress, m_VerticalOffset, p->m_VerticalOffset, pPanel->GetActualRenderHeight() );
	m_BlurRadius = LerpUILength( flProgress, m_BlurRadius, p->m_BlurRadius, (pPanel->GetActualRenderHeight() + pPanel->GetActualRenderWidth()) / 2.0f );
	m_flStrength = Lerp( flProgress, m_flStrength, p->m_flStrength );

	float r = Lerp( flProgress, (float)m_ShadowColor.r(), (float)p->m_ShadowColor.r() );
	float g = Lerp( flProgress, (float)m_ShadowColor.g(), (float)p->m_ShadowColor.g() );
	float b = Lerp( flProgress, (float)m_ShadowColor.b(), (float)p->m_ShadowColor.b() );
	float a = Lerp( flProgress, (float)m_ShadowColor.a(), (float)p->m_ShadowColor.a() );
	m_ShadowColor.SetColor( RoundFloatToInt( r ), RoundFloatToInt( g ), RoundFloatToInt( b ), RoundFloatToInt( a ) );
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for image shadows
//-----------------------------------------------------------------------------
void CStylePropertyImageShadow::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if ( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyImageShadow::Interpolate" );
		return;
	}

	CStylePropertyImageShadow *p = ( CStylePropertyImageShadow * )&target;

	// Interpolate distances and color
	m_HorizontalOffset = LerpUILength( flProgress, m_HorizontalOffset, p->m_HorizontalOffset, pPanel->GetActualRenderWidth() );
	m_VerticalOffset = LerpUILength( flProgress, m_VerticalOffset, p->m_VerticalOffset, pPanel->GetActualRenderHeight() );
	m_BlurRadius = LerpUILength( flProgress, m_BlurRadius, p->m_BlurRadius, ( pPanel->GetActualRenderHeight() + pPanel->GetActualRenderWidth() ) / 2.0f );
	m_flStrength = Lerp( flProgress, m_flStrength, p->m_flStrength );

	float r = Lerp( flProgress, ( float )m_ShadowColor.r(), ( float )p->m_ShadowColor.r() );
	float g = Lerp( flProgress, ( float )m_ShadowColor.g(), ( float )p->m_ShadowColor.g() );
	float b = Lerp( flProgress, ( float )m_ShadowColor.b(), ( float )p->m_ShadowColor.b() );
	float a = Lerp( flProgress, ( float )m_ShadowColor.a(), ( float )p->m_ShadowColor.a() );
	m_ShadowColor.SetColor( RoundFloatToInt( r ), RoundFloatToInt( g ), RoundFloatToInt( b ), RoundFloatToInt( a ) );
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for clipping
//-----------------------------------------------------------------------------
void CStylePropertyClip::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyClip::Interpolate" );
		return;
	}

	CStylePropertyClip *p = (CStylePropertyClip *)&target;

	// Interpolate rect clip
	m_Left = LerpUILength( flProgress, m_Left, p->m_Left, pPanel->GetActualRenderWidth() );
	m_Top = LerpUILength( flProgress, m_Top, p->m_Top, pPanel->GetActualRenderHeight() );
	m_Right = LerpUILength( flProgress, m_Right, p->m_Right, pPanel->GetActualRenderWidth() );
	m_Bottom = LerpUILength( flProgress, m_Bottom, p->m_Bottom, pPanel->GetActualRenderHeight() );

	// Interpolate radial clip
	m_RadialCenterX = LerpUILength( flProgress, m_RadialCenterX, p->m_RadialCenterX, pPanel->GetActualRenderWidth() );
	m_RadialCenterY = LerpUILength( flProgress, m_RadialCenterY, p->m_RadialCenterY, pPanel->GetActualRenderHeight() );
	m_flRadialStartAngle = Lerp( flProgress, m_flRadialStartAngle, p->m_flRadialStartAngle );
	m_flRadialSectorAngle = Lerp( flProgress, m_flRadialSectorAngle, p->m_flRadialSectorAngle );
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for borders
//-----------------------------------------------------------------------------
void CStylePropertyBorder::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	CStylePropertyBorder *p = (CStylePropertyBorder *)&target;
	for( int i = 0; i < 4; ++i )
	{
		CUILength width = m_rgBorderWidth[i];
		CUILength targetWidth = p->m_rgBorderWidth[i];

		if( m_rgBorderStyle[i] == k_EBorderStyleNone )
			width.SetLength( 0.0f );

		if( p->m_rgBorderStyle[i] == k_EBorderStyleNone )
			targetWidth.SetLength( 0.0f );

		// Set style to solid if they don't match, but we'll fade out the width towards none
		if( m_rgBorderStyle[i] != p->m_rgBorderStyle[i] )
		{
			if( flProgress >= 1.0f )
			{
				m_rgBorderStyle[i] = p->m_rgBorderStyle[i];
			}
			m_rgBorderStyle[i] = k_EBorderStyleSolid;
		}

		float r = Lerp( flProgress, (float)m_rgBorderColor[i].r(), (float)p->m_rgBorderColor[i].r() );
		float g = Lerp( flProgress, (float)m_rgBorderColor[i].g(), (float)p->m_rgBorderColor[i].g() );
		float b = Lerp( flProgress, (float)m_rgBorderColor[i].b(), (float)p->m_rgBorderColor[i].b() );
		float a = Lerp( flProgress, (float)m_rgBorderColor[i].a(), (float)p->m_rgBorderColor[i].a() );

		m_rgBorderColor[i].SetColor( RoundFloatToInt( r ), RoundFloatToInt( g ), RoundFloatToInt( b ), RoundFloatToInt( a ) );
		m_rgBorderWidth[i] = LerpUILength( flProgress, width, targetWidth, i % 2 == 0 ? pPanel->GetActualRenderHeight() : pPanel->GetActualRenderWidth() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for position
//-----------------------------------------------------------------------------
void CStylePropertyPosition::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyPosition::Interpolate" );
		return;
	}

	const CStylePropertyPosition *p = (const CStylePropertyPosition *)&target;

	float flParentWidth, flParentHeight, flParentPerspective;
	GetParentSizeAvailable( pPanel, flParentWidth, flParentHeight, flParentPerspective );

	x = LerpUILength( flProgress, x, p->x, flParentWidth );
	y = LerpUILength( flProgress, y, p->y, flParentHeight );
	z = LerpUILength( flProgress, z, p->z, flParentPerspective );
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for perspective origin
//-----------------------------------------------------------------------------
void CStylePropertyPerspectiveOrigin::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyPerspectiveOrigin::Interpolate" );
		return;
	}

	const CStylePropertyPerspectiveOrigin *p = (const CStylePropertyPerspectiveOrigin *)&target;

	if( m_bInvert == p->m_bInvert )
	{
		x = LerpUILength( flProgress, x, p->x, pPanel->GetActualRenderWidth() );
		y = LerpUILength( flProgress, y, p->y, pPanel->GetActualRenderHeight() );
	}
	else
	{
		// snap in the invert not matching case, can't interpolate then, unless one side is 50/50 where invert does nothing
		CUILength myx = x;
		CUILength myy = y;
		myx.ConvertToPercent( pPanel->GetActualRenderWidth() );
		myy.ConvertToPercent( pPanel->GetActualRenderHeight() );

		if( myx.GetValue() > 49.99f && myx.GetValue() < 50.01f && myy.GetValue() > 49.99f && myy.GetValue() < 50.01f )
		{
			m_bInvert = p->m_bInvert;
			x = LerpUILength( flProgress, x, p->x, pPanel->GetActualRenderWidth() );
			y = LerpUILength( flProgress, y, p->y, pPanel->GetActualRenderHeight() );
		}
		else
		{
			CUILength tx = p->x;
			CUILength ty = p->y;
			tx.ConvertToPercent( pPanel->GetActualRenderWidth() );
			ty.ConvertToPercent( pPanel->GetActualRenderHeight() );

			if( tx.GetValue() > 49.99f && tx.GetValue() < 50.01f && ty.GetValue() > 49.99f && ty.GetValue() < 50.01f )
			{
				// Keep our invert value then, as the targets does nothing
				x = LerpUILength( flProgress, x, p->x, pPanel->GetActualRenderWidth() );
				y = LerpUILength( flProgress, y, p->y, pPanel->GetActualRenderHeight() );
			}
			else
			{
				// Just jump to final target, can't interpolate incompatible inverts that matter on both
				x = p->x;
				y = p->y;
				m_bInvert = p->m_bInvert;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Interpolate for transform origin
//-----------------------------------------------------------------------------
void CStylePropertyTransformOrigin::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyPerspectiveOrigin::Interpolate" );
		return;
	}

	const CStylePropertyTransformOrigin *p = (const CStylePropertyTransformOrigin *)&target;

	float flParentWidth = pPanel->GetActualRenderWidth();
	float flParentHeight = pPanel->GetActualRenderHeight();
	if( pPanel->GetParent() )
	{
		flParentWidth = pPanel->GetParent()->GetActualRenderWidth();
		flParentHeight = pPanel->GetParent()->GetActualLayoutHeight();
	}

	if( p->m_bParentRelative != m_bParentRelative )
	{
		// If one is parent relative and the other isn't then we must convert to lengths and interpolate since percentages can't be mixed
		// between the two
		CUILength myx = x;
		CUILength myy = y;

		myx.ConvertToLength( m_bParentRelative ? pPanel->GetActualRenderWidth() : flParentWidth );
		myy.ConvertToLength( m_bParentRelative ? pPanel->GetActualRenderHeight() : flParentHeight );

		CUILength targetx = p->x;
		CUILength targety = p->y;

		targetx.ConvertToLength( p->m_bParentRelative ? pPanel->GetActualRenderWidth() : flParentWidth );
		targety.ConvertToLength( p->m_bParentRelative ? pPanel->GetActualRenderHeight() : flParentHeight );

		x = LerpUILength( flProgress, myx, targetx, pPanel->GetActualRenderWidth() );
		y = LerpUILength( flProgress, myy, targety, pPanel->GetActualRenderHeight() );
	}
	else
	{
		x = LerpUILength( flProgress, x, p->x, m_bParentRelative ? pPanel->GetActualRenderWidth() : flParentWidth );
		y = LerpUILength( flProgress, y, p->y, m_bParentRelative ? pPanel->GetActualRenderHeight() : flParentWidth );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Interpolation func for animation of this property
//-----------------------------------------------------------------------------
void CStylePropertyFillColor::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	if( target.GetPropertySymbol() != GetPropertySymbol() )
	{
		AssertMsg( false, "Mismatched types to CStylePropertyFillColor::Interpolate" );
		return;
	}

	const CStylePropertyFillColor *p = (const CStylePropertyFillColor *)&target;
	m_FillBrushCollection.Interpolate( pPanel->GetActualRenderWidth(), pPanel->GetActualRenderHeight(), p->m_FillBrushCollection, flProgress );
}


//-----------------------------------------------------------------------------
// Purpose: Interpolation func for animation of this property
//-----------------------------------------------------------------------------
void CStylePropertyBorderRadius::Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
{
	const CStylePropertyBorderRadius *p = (const CStylePropertyBorderRadius *)&target;

	for( int i = 0; i < k_ECornerMax; ++i )
	{
		m_rgCornerRaddi[i].m_HorizontalRadii = LerpUILength( flProgress, m_rgCornerRaddi[i].m_HorizontalRadii, p->m_rgCornerRaddi[i].m_HorizontalRadii, pPanel->GetActualRenderWidth() );
		m_rgCornerRaddi[i].m_VerticalRadii = LerpUILength( flProgress, m_rgCornerRaddi[i].m_VerticalRadii, p->m_rgCornerRaddi[i].m_VerticalRadii, pPanel->GetActualRenderHeight() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Interpolation func for animation of this property
//-----------------------------------------------------------------------------

void CStylePropertySound::PlaySoundOnPanel(IUIPanel *pPanel)
{
	if(!pPanel)
		return;

	IUIWindow *pWindow = pPanel->GetParentWindow();
	if(!pWindow || !pWindow->BIsVisible())
		return;

	for(const CUtlString &strSoundName : m_vecSoundNames)
	{
		UISoundSystem()->PlaySound(strSoundName, pPanel, k_ESoundType_Effects);
	}
}


void CStylePropertySound::PlaySoundOnPanel( IUIPanel *pPanel, CStyleProperty* pPrev )
{
	if ( !pPanel || !pPrev )
		return;

	IUIWindow *pWindow = pPanel->GetParentWindow();
	if ( !pWindow || !pWindow->BIsVisible() )
		return;

	for ( const CUtlString &strSoundName : m_vecSoundNames )
	{
		if ( strSoundName != "null" && strSoundName != "" )
		{
			UISoundSystem()->PlaySound( strSoundName, pPanel, k_ESoundType_Effects );
		}
	}
}

void CStylePropertySound::ResolveDefaultValues()
{
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Validate
//-----------------------------------------------------------------------------
void CStyleProperty::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPanelIdentifiers::CPanelIdentifiers()
{
	m_unStyleFlags = 0;
	m_psymClasses = NULL;
	m_csymClasses = 0;
	m_pchID = "";
	m_pPanel = NULL;
	m_bTreatPanelAsParent = false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPanelIdentifiers::CPanelIdentifiers( IUIPanel *pPanel )
{
	m_bTreatPanelAsParent = false;
	m_symPanelType = pPanel->ClientPtr()->GetPanelType();
	m_unStyleFlags = pPanel->GetStyleFlags();
	m_psymClasses = pPanel->GetClasses().Base();
	m_csymClasses = pPanel->GetClasses().Count();
	m_pchID = pPanel->GetID();
	m_pPanel = pPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Deep copy animation keyframes
//-----------------------------------------------------------------------------
VecKeyFrames_t *CStyleAnimation::CopyKeyframes() const
{
	const VecKeyFrames_t& keyFrames = GetFrames();
	VecKeyFrames_t* pDstKeyframes = new VecKeyFrames_t;

	int nNumKeyframes = keyFrames.Count();
	for ( int i = 0; i < nNumKeyframes; i++ )
	{
		pDstKeyframes->Insert( keyFrames[i]->Copy() );
	}

	return pDstKeyframes;
}

//-----------------------------------------------------------------------------
// Purpose: Deep copy a keyframe
//-----------------------------------------------------------------------------

CStyleKeyFrame *CStyleKeyFrame::Copy() const
{
	StylePropertyHash_t *pDstKeyframeProperties = new StylePropertyHash_t;
	CStyleKeyFrame *pDstKeyframe = new CStyleKeyFrame( GetPercent(), 
		GetTimingFunction(), GetCubicBezier(), pDstKeyframeProperties );
		
	StylePropertyHash_t *pSrcProperties = GetProperties();

	int nNumProperties = pSrcProperties->Count();
	for ( int i = 0; i < nNumProperties; i++ )
	{
		CStyleProperty *pNewProperty = CStylePropertyFactory::CopyStyleProperty( *( pSrcProperties->Element(i) ) );
		pDstKeyframeProperties->Insert( pNewProperty->GetPropertySymbol(), pNewProperty );
	}

	return pDstKeyframe;
}
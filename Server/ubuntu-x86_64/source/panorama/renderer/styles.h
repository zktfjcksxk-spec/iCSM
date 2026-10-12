//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef STYLES_H
#define STYLES_H
#pragma once

#include <float.h>
#include "color.h"
#include "utlstring.h"
#include "utlvector.h"
#include "utlsymbol.h"
#include "panorama/transformations.h"
#include "panorama/renderer/rendercommands.h"
#include "panoramatypes.h"
#if !defined( SOURCE2_PANORAMA )
#include "steamcommon.h"
#endif
#include "panorama/iuiengine.h"
#include "panorama/layout/backgroundimage.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/layout/fillbrush.h"
#include "../layout/stylepropertyfactory.h"
#include "panorama/data/iimagesource.h"
#include "panorama/text/iuitextlayout.h"
#include "panorama/uifileresource.h"
#include "panorama/iuiengine.h"
#include "panorama/layout/stylesymbol.h"
#include "panorama/renderer/styleproperties.h"
#include "iuipanelstyle.h"
#include "panorama/uischeduleddel.h"

namespace panorama
{

class CStyleFileSet;
class CStyleProperty;
class CPanel2D;
class CDebugPanelStyle;
class CDebugPanelComputed;
class CLayoutFile;

//-----------------------------------------------------------------------------
// Purpose: Function to convert linear progress to bezier curve based progress for timing funcs
//-----------------------------------------------------------------------------
float GetProgressForTimingFunction( const CCubicBezierCurve<Vector2D> &cubicBezier, float flPctComplete /* 0.0 -> 1.0 */ );
float GetTimeProgress( const CCubicBezierCurve<Vector2D> &cubicBezier, double flStart, double flCurrent, float flDelay, float flDuration );


class CPanelStyle;


//-----------------------------------------------------------------------------
// Purpose: Fake property for searching tree of styles
//-----------------------------------------------------------------------------
class CStylePropertySearch : public CStyleProperty
{
public:

	CStylePropertySearch( CStyleSymbol hSymbol ) : CStyleProperty( hSymbol ) 
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		AssertMsg( false, "Copy shouldn't be called on CStylePropertySearch" );
	}
	
	virtual bool BFullySet() const { return true; }
	virtual void ResolveDefaultValues() {}

	virtual bool BCanTransition() { return false; }
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		AssertMsg( false, "Interpolate shouldn't be called on CStylePropertySearch" );
	}

	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString ) { return false; }
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const { pfmtBuffer->Append( "SEARCH!!!!" ); }

	// Comparison function
	virtual bool operator==( const CStyleProperty &other ) const { return (GetPropertySymbol() != other.GetPropertySymbol()); }

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }	
};

//-----------------------------------------------------------------------------
// Purpose: Less func class
//-----------------------------------------------------------------------------
class CStylePropertyInTransitionLess
{
public:
	CStylePropertyInTransitionLess() {}
	CStylePropertyInTransitionLess( int i ) {}
	inline bool operator()( PropertyInTransition_t * const &lhs, PropertyInTransition_t * const &rhs ) const { return ( lhs->m_pStyleProperty->GetPropertySymbol() < rhs->m_pStyleProperty->GetPropertySymbol() ); }
	inline bool operator!() const { return false; }
};



//-----------------------------------------------------------------------------
// Purpose: Represents styling information for a panel
//-----------------------------------------------------------------------------
typedef CUtlHashMap< CStyleSymbol, uint8, CDefEquals< CStyleSymbol > > SymbolHash_t;
class CPanelStyle /*FINAL*/ : public IUIPanelStyle
{
public:
	CPanelStyle( IUIPanel *pPanel );
	~CPanelStyle();

	IUIPanel *AccessPanel() { return m_pPanel; };

	virtual void Clear( bool bIncludeClearingElementStyles = true ) OVERRIDE;

	virtual void UpdateUIScaleFactor( const Vector &vOldScaleFactor, const Vector &vNewScaleFactor, const Vector &vOldParentScaleFactor, const Vector &vNewParentScaleFactor ) OVERRIDE;

	// Checks if the property has any style data at all, base, transition, or animation.
	// Useful to early out any work related to the stype when painting.
	virtual bool BHasAnyStyleDataForProperty( CStyleSymbol hSymbolProperty ) OVERRIDE;

	virtual void GetPosition( CUILength &x, CUILength &y, CUILength &z, bool bIncludeUIScaleFactor = true ) OVERRIDE;
	virtual void GetInterpolatedPosition( CUILength &x, CUILength &y, CUILength &z, bool bFinal, bool bIncludeUIScaleFactor = true ) OVERRIDE;
	virtual void SetPosition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor = false ) OVERRIDE;
	virtual void SetPositionWithoutTransition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor = false ) OVERRIDE;
	void GetPositionRenderData( PanelPositionWithTransition_t &positionData, CRenderCommandList &commandList );

	virtual void GetPerspectiveOrigin( CUILength &x, CUILength &y, bool &bInvert ) OVERRIDE;
	virtual void SetPerspectiveOrigin( CUILength &x, CUILength &y, bool &bInvert ) OVERRIDE;
	void GetTransformationPerspectiveOriginData( TransformPerspectiveOriginWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetTransformOrigin( CUILength &x, CUILength &y, bool &bParentLayerRelative ) OVERRIDE;
	virtual void SetTransformOrigin( CUILength &x, CUILength &y, bool bParentLayerRelative ) OVERRIDE;
	void GetTransformationOriginData( TransformOriginWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetPerspective( float &perspective ) OVERRIDE;
	virtual void SetPerspective( float perspective ) OVERRIDE;
	void GetTransformationPerspectiveData( TransformPerspectiveWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetZIndex( float &zindex ) OVERRIDE;
	virtual void SetZIndex( float zIndex ) OVERRIDE;

	virtual void GetOverflow( EOverflowValue &eHorizontal, EOverflowValue &eVertical ) OVERRIDE;
	virtual void SetOverflow( const EOverflowValue eHorizontal, const EOverflowValue eVertical ) OVERRIDE;

	virtual void SetTransform3D( const CUtlVector<CTransform3D *> &vecTransforms ) OVERRIDE;
	virtual void SetTransform3DWithoutTransition( const CUtlVector<CTransform3D *> &vecTransforms ) OVERRIDE;
	// Simplified version of SetTransform3D that will call a simplified version of SetProperty
	// ie bypassing updating the corresponding panel - it is the responsibility of the calling
	// code to update the corresponding UIPanel (such as calling InvalidateSizeAndPosition, AfterStyleApplied, ...).
	// Also assuming no animation / transition set for transform (via CSS or code)
	// Returns true if style has been updated with the new value, false otherwise.
	virtual bool SetTransform3DSimple( const CUtlVector<CTransform3D *> &vecTransforms ) OVERRIDE;
	virtual VMatrix GetTransform3DMatrix();
	virtual bool BTransformIsIdentityRegardlessOfParentSize();
	void GetTransformationData( TransformMatrixWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetTransforms( CUtlVector<CTransform3D*>& inTransforms ) OVERRIDE;

	virtual void GetOpacity( float &opacity ) OVERRIDE;
	virtual void SetOpacity( float opacity ) OVERRIDE;
	// Simplified version of SetOpacity that will call a simplified version of SetProperty
	// ie bypassing updating the corresponding panel - it is the responsibility of the calling
	// code to update the corresponding UIPanel (such as calling InvalidateSizeAndPosition, AfterStyleApplied, ...).
	// Also assuming no animation / transition set for transform (via CSS or code)
	// Returns true if style has been updated with the new value, false otherwise.
	virtual bool SetOpacitySimple( float opacity ) OVERRIDE;
	void GetOpacityData( OpacityWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetBackgroundImgOpacity( float &opacity ) OVERRIDE;
	virtual void SetBackgroundImgOpacity( float opacity ) OVERRIDE;
	void GetBackgroundImgOpacityData( BackgroundImgOpacityWithTransition_t &data, CRenderCommandList &commandList );

	virtual void SetScale2DCentered( float flX, float flY ) OVERRIDE;
	virtual void GetInterpolatedScale2DCentered( float &flX, float &flY ) OVERRIDE;
	virtual void GetScale2DCentered( float &flX, float &flY ) OVERRIDE;
	void GetScale2DCenteredData( Scale2DWithTransition_t &data, CRenderCommandList &commandList );

	virtual void SetRotate2DCentered( float flDegrees ) OVERRIDE;
	virtual void GetRotate2DCentered( float &flDegrees ) OVERRIDE;
	void GetRotate2DCenteredData( Rotate2DWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetHueShift( float &flHueShift ) OVERRIDE;
	virtual void SetHueShift( float flHueShift ) OVERRIDE;
	void GetHueShiftData( HueShiftWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetSaturation( float &flSaturation ) OVERRIDE;
	virtual void SetSaturation( float flSaturation ) OVERRIDE;
	void GetSaturationData( SaturationWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetBrightness( float &flBrightness ) OVERRIDE;
	virtual void SetBrightness( float flBrightness ) OVERRIDE;
	void GetBrightnessData( BrightnessWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetContrast( float &flContrast ) OVERRIDE;
	virtual void SetContrast( float flContrast ) OVERRIDE;
	void GetContrastData( ContrastWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetGaussianBlur( BlurType_t &blurType, float &passes, float &stddevhor, float &stddevver ) OVERRIDE;
	virtual void SetGaussianBlur( BlurType_t blurType, float passes, float stddevhor, float stddevver ) OVERRIDE;
	void GetGaussianBlurData( GaussianBlurWithTransition_t &data, CRenderCommandList &commandList );

	void GetBorderData( BorderWithTransition_t &data, CRenderCommandList &commandList );
	void GetBorderRadiusData( BorderRadiusWithTransition_t &data, CRenderCommandList &commandList );
	void GetBoxShadowData( BoxShadowWithTransition_t &data, CRenderCommandList &commandList );
	void GetTextShadowData( TextShadowWithTransition_t &data, CRenderCommandList &commandList );
	void GetImageShadowData( ImageShadowWithTransition_t &data, CRenderCommandList &commandList );

	void GetClipData( ClipWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetOpacityMaskImage( IImageSource *& pImage, float *pflOpacityMaskOpacity ) OVERRIDE;
	void GetOpacityMaskData( OpacityMaskWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetWashColor( Color &c ) OVERRIDE;
	virtual void SetWashColor( const char *pchColor ) OVERRIDE;
	virtual void SetSimpleWashColor( const Color &c, bool bFast = false ) OVERRIDE;
	void GetWashColorData( WashColorWithTransition_t &data, CRenderCommandList &commandList );

	EMixBlendMode GetMixBlendMode() OVERRIDE;
	ETextureSampleMode GetTexturesSampleMode() OVERRIDE;

	virtual void SetBackgroundColor( const char *pchColor ) OVERRIDE;
	virtual void SetSimpleBackgroundColor( const Color &c ) OVERRIDE;
	virtual bool GetSimpleBackgroundColor( Color &c ) OVERRIDE;

	virtual void SetForegroundColor( const char *pchColor ) OVERRIDE;
	virtual void SetSimpleForegroundColor( const Color &c ) OVERRIDE;
	virtual bool GetSimpleForegroundColor( Color &c ) OVERRIDE;

	virtual void SetBorderColor( const Color &c ) OVERRIDE;

	virtual void SetFontStyle( const char *pchFontFamily, float flSize, EFontStyle style, EFontWeight weight ) OVERRIDE;
	virtual void GetFontStyle( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight ) OVERRIDE;
	virtual void GetFontStyleNoDefaults( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight ) OVERRIDE;
	virtual void GetForegroundFillBrushCollectionData( FillBrushCollectionWithTransition_t &data, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight ) OVERRIDE;
	virtual void GetLineHeight( float &flLineHeight ) OVERRIDE;
	virtual void GetTextAlign( ETextAlign &align ) OVERRIDE;
	virtual void GetTextDecoration( ETextDecoration &decoration ) OVERRIDE;
	virtual void GetTextTransform( ETextTransform &transform ) OVERRIDE;
	virtual void GetTextLetterSpacing( int &spacing ) OVERRIDE;

	void GetForegroundFillBrushCollection( CFillBrushCollection &c );

	virtual void SetTransitionProperties( const CUtlVector< TransitionProperty_t > &vecTransitionProperties ) OVERRIDE; 	
	
	virtual bool BHasPossibleBackgroundColor() OVERRIDE;
	bool BHasConstantOpaqueBackground();
	const CFillBrushCollection *GetBackgroundFillBrushCollection();
	void SetBackgroundFillBrushCollection( CFillBrushCollection &c );
	void GetBackgroundFillBrushCollectionData( FillBrushCollectionWithTransition_t &data, CRenderCommandList &commandList );

	virtual void GetWidth( CUILength &width ) OVERRIDE;
	virtual void SetWidth( CUILength width ) OVERRIDE;
	virtual void SetWidthWithoutTransition( CUILength width ) OVERRIDE;
	virtual void GetHeight( CUILength &height ) OVERRIDE;
	virtual void SetHeight( CUILength height ) OVERRIDE;
	virtual void SetHeightWithoutTransition( CUILength width ) OVERRIDE;
	virtual void GetMinWidth( CUILength &minWidth ) OVERRIDE;
	virtual void SetMinWidth( CUILength minWidth ) OVERRIDE;
	virtual void GetMinHeight( CUILength &minHeight ) OVERRIDE;
	virtual void SetMinHeight( CUILength minHeight ) OVERRIDE;
	virtual void GetMaxWidth( CUILength &maxWidth ) OVERRIDE;
	virtual void SetMaxWidth( CUILength maxWidth ) OVERRIDE;
	virtual void GetMaxHeight( CUILength &maxHeight ) OVERRIDE;
	virtual void SetMaxHeight( CUILength maxHeight ) OVERRIDE;
	virtual void GetInterpolatedWidth( CUILength &width, bool bFinal ) OVERRIDE;
	virtual void GetInterpolatedHeight( CUILength &height, bool bFinal ) OVERRIDE;
	virtual void GetInterpolatedMaxWidth( CUILength &width, bool bFinal ) OVERRIDE;
	virtual void GetInterpolatedMaxHeight( CUILength &height, bool bFinal ) OVERRIDE;

	virtual void SetUIScale( const Vector &vUIScale ) OVERRIDE;
	virtual Vector GetUIScale() OVERRIDE;
	virtual Vector GetInterpolatedUIScale( bool bFinal ) OVERRIDE;

	virtual void GetVisibility( bool &bVisible ) OVERRIDE;
	virtual void SetVisibility( bool bVisible ) OVERRIDE;

	virtual void GetFlowChildren( EFlowDirection &eFlowDirection ) OVERRIDE;
	virtual void SetFlowChildren( EFlowDirection eFlowDirection ) OVERRIDE;

	virtual void GetInterpolatedBorderWidth( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom, bool bFinal ) OVERRIDE;

	virtual void GetWhitespaceWrap( bool &bWrap ) OVERRIDE;
	virtual void GetTextOverflow( ETextOverflow &eTextOverflow ) OVERRIDE;

	// Content inset is padding+border-width
	virtual void GetContentInset( float flBoxWidth, float flBoxHeight, bool bFinalDimensions, float &left, float &top, float &right, float &bottom ) OVERRIDE;
	virtual bool BHasContentInsetTransition() OVERRIDE;

	// You shouldn't need this outside layout code normally you want GetContentInset instead!
	virtual void GetPadding( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom ) OVERRIDE;

	virtual void GetMargin( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom ) OVERRIDE;
	virtual void GetMargin( float flBoxWidth, float flBoxHeight, float &left, float &top, float &right, float &bottom ) OVERRIDE;
	virtual void SetMargin( CUILength &left, CUILength &top, CUILength &right, CUILength &bottom ) OVERRIDE;

	virtual CUtlVector< CBackgroundImageLayer * > *GetBackgroundImages() OVERRIDE;
	virtual void SetBackgroundImages( const CUtlVector< CBackgroundImageLayer * > &vecLayers ) OVERRIDE;
	OpacityWithTransition_t *GetBackgroundImageLayerOpacityData( CBackgroundImageLayer *pLayer, CRenderCommandList &commandList );

	virtual void GetAlignment( EHorizontalAlignment &eHorizontalAlignment, EVerticalAlignment &eVerticalAlignment ) OVERRIDE;

	virtual void SetTooltipPositions( const EContextUIPosition( &eTooltipPositions )[ 4 ] ) OVERRIDE;
	virtual void GetTooltipPositions( EContextUIPosition( &eTooltipPositions )[ 4 ] ) OVERRIDE;
	virtual void SetTooltipBodyPosition( const CUILength &horizontalPosition, const CUILength &verticalPosition ) OVERRIDE;
	virtual void GetTooltipBodyPosition( CUILength &horizontalPosition, CUILength &verticalPosition ) OVERRIDE;
	virtual void SetTooltipArrowPosition( const CUILength &horizontalPosition, const CUILength &verticalPosition ) OVERRIDE;
	virtual void GetTooltipArrowPosition( CUILength &horizontalPosition, CUILength &verticalPosition ) OVERRIDE;

	virtual void SetContextMenuPositions( const EContextUIPosition( &eContextMenuPositions )[ 4 ] ) OVERRIDE;
	virtual void GetContextMenuPositions( EContextUIPosition( &eContextMenuPositions )[ 4 ] ) OVERRIDE;
	virtual void SetContextMenuBodyPosition( const CUILength &horizontalPosition, const CUILength &verticalPosition ) OVERRIDE;
	virtual void GetContextMenuBodyPosition( CUILength &horizontalPosition, CUILength &verticalPosition ) OVERRIDE;
	virtual void SetContextMenuArrowPosition( const CUILength &horizontalPosition, const CUILength &verticalPosition ) OVERRIDE;
	virtual void GetContextMenuArrowPosition( CUILength &horizontalPosition, CUILength &verticalPosition ) OVERRIDE;

	virtual void SetRadialClip( bool bRadialClip, const CUILength &x, const CUILength &y, float flStartAngle, float flSectorAngle ) OVERRIDE;
	virtual void GetRadialClip( bool &bRadialClip, CUILength &x, CUILength &y, float &flStartAngle, float &flSectorAngle ) OVERRIDE;

	// Get animation names active on the panel
	virtual void GetAnimationNames( CUtlVector< CPanoramaSymbol > *pvecAnimations ) OVERRIDE;

	// Reset animations
	virtual void ResetAnimations() OVERRIDE;

	// End any non-looping animations
	virtual void SkipAnimations() OVERRIDE;

	// Get actual parent sizes, which we need to convert some % units to px
	virtual float GetParentActualRenderWidth() OVERRIDE;
	virtual float GetParentActualRenderHeight() OVERRIDE;

	// Checks if the property has any data for transition
	bool BHasTransition( CStyleSymbol hSymbolProperty );
	virtual bool BHasAnyTransition() OVERRIDE;

	// Checks if the property has any data for transition or animation
	bool BHasTransitionOrAnimation( CStyleSymbol hSymbolProperty );

	// Checks if this panel has any active transitions or animations
	virtual bool BHasAnyTransitionOrAnimation( bool bExcludeStylesImpactingOnlyCompositing ) OVERRIDE;

	bool BHasAnimatingBackground();
	void EnableBackgroundMovies( bool bEnabled );

	// Checks if the panel is completely transparent, and has no current animation/transition of opacity.  
	// If this is true it means we don't need to draw the panel.
	virtual bool BIsTransparentWithNoOpacityTransition() OVERRIDE;

	// properties set on element style (set from code)
	virtual bool BPropertySetFromElement( CStyleSymbol symProperty ) const OVERRIDE;
	virtual void ClearPropertySetFromElement( CStyleSymbol symProperty ) OVERRIDE;

	CStyleProperty *GetPropertyFromElementStyle( CStyleSymbol symProperty ) const;

	bool BHasElementStyles() { return m_pVecElementProperties != NULL; }

	// Creates animated property values when an animation property is set
	void SetAnimationProperties( CStylePropertyAnimationProperties *pAnimationProperties );
	void SetSingleAnimation( AnimationProperty_t animation, VecKeyFrames_t &vecKeyframes );

	// Get animation control curve points
	virtual void GetAnimationCurveControlPoints( EAnimationTimingFunction eTransitionEffect, Vector2D vecPoints[4] ) OVERRIDE;

	// Does the style have transforms set that impact 3d values (z values) 
	bool BHasNon2DTransforms();

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif


	// For internal use only set property object directly, as element style
	void SetProperty( CStyleProperty *pProperty, bool bAllowTransition = true );
	void MergeProperty( CStyleProperty *pProperty, bool bAllowTransition = true );

	virtual void FindPropertyInfo( CStyleSymbol hSymbol, CStyleProperty **ppProperty, PropertyInTransition_t **ppTransitionData, CUtlVector< CActiveAnimation * > *pvecAnimations ) OVERRIDE;

	virtual TransitionProperty_t * FindTransitionData( CStyleSymbol hSymbol );

	// used by debugger
	virtual const CUtlVector<StyleEntry_t> &PropertiesSetFromElement() const OVERRIDE { return m_pVecElementProperties ? *m_pVecElementProperties : s_EmptyPropertyVec; }
	virtual const CStyleProperty *GetPropertyNoInherit( CStyleSymbol symProperty ) OVERRIDE;

	// Updates the property immediately, removing active transition.  Will interpolate to current time, will not
	// force updating all the way to end of transition.
	virtual CStyleProperty *CompletePropertyTransitionNow( CStyleSymbol hSymbol, bool bDeleteTargetProperty );

	virtual void GetActiveAnimations( CUtlVector<CActiveAnimation *> *pvecAnimations ) OVERRIDE;
	virtual CActiveAnimation* FindActiveAnimation( CPanoramaSymbol animName ) OVERRIDE;

	//template < typename T > void GetRenderData( T &data, CRenderCommandList &commandList );
	void GetRenderData( PanelPositionWithTransition_t &data, CRenderCommandList &commandList ) { GetPositionRenderData( data, commandList ); }
	void GetRenderData( ClipWithTransition_t &data, CRenderCommandList &commandList ) { GetClipData( data, commandList ); }
	void GetRenderData( TransformMatrixWithTransition_t &data, CRenderCommandList &commandList ) { GetTransformationData( data, commandList ); }
	void GetRenderData( TransformOriginWithTransition_t &data, CRenderCommandList &commandList ) { GetTransformationOriginData( data, commandList ); }
	void GetRenderData( TransformPerspectiveWithTransition_t &data, CRenderCommandList &commandList ) { GetTransformationPerspectiveData( data, commandList ); }
	void GetRenderData( TransformPerspectiveOriginWithTransition_t &data, CRenderCommandList &commandList ) { GetTransformationPerspectiveOriginData( data, commandList ); }
	void GetRenderData( OpacityWithTransition_t &data, CRenderCommandList &commandList ) { GetOpacityData( data, commandList ); }
	void GetRenderData( Scale2DWithTransition_t &data, CRenderCommandList &commandList ) { GetScale2DCenteredData( data, commandList ); }
	void GetRenderData( Rotate2DWithTransition_t &data, CRenderCommandList &commandList ) { GetRotate2DCenteredData( data, commandList ); }
	void GetRenderData( WashColorWithTransition_t &data, CRenderCommandList &commandList ) { GetWashColorData( data, commandList ); }
	void GetRenderData( HueShiftWithTransition_t &data, CRenderCommandList &commandList ) { GetHueShiftData( data, commandList ); }
	void GetRenderData( SaturationWithTransition_t &data, CRenderCommandList &commandList ) { GetSaturationData( data, commandList ); }
	void GetRenderData( BrightnessWithTransition_t &data, CRenderCommandList &commandList ) { GetBrightnessData( data, commandList ); }
	void GetRenderData( ContrastWithTransition_t &data, CRenderCommandList &commandList ) { GetContrastData( data, commandList ); }
	void GetRenderData( GaussianBlurWithTransition_t &data, CRenderCommandList &commandList ) { GetGaussianBlurData( data, commandList ); }
	void GetRenderData( BorderRadiusWithTransition_t &data, CRenderCommandList &commandList ) { GetBorderRadiusData( data, commandList ); }
	void GetRenderData( BorderWithTransition_t &data, CRenderCommandList &commandList ) { GetBorderData( data, commandList ); }
	void GetRenderData( BoxShadowWithTransition_t &data, CRenderCommandList &commandList ) { GetBoxShadowData( data, commandList ); }
	void GetRenderData( OpacityMaskWithTransition_t &data, CRenderCommandList &commandList ) { GetOpacityMaskData( data, commandList ); }
	void GetRenderData( TextShadowWithTransition_t &data, CRenderCommandList &commandList ) { GetTextShadowData( data, commandList ); }
	void GetRenderData( ImageShadowWithTransition_t &data, CRenderCommandList &commandList ) { GetImageShadowData( data, commandList ); }
	void GetRenderData( BackgroundImgOpacityWithTransition_t &data, CRenderCommandList &commandList ) { GetBackgroundImgOpacityData( data, commandList ); }

protected:
	friend class CStyleFileSet;
	friend class CStylePropertyTransform3D;

	// Setter on arbitrary base property, protected because public interface should use nice setters/getters above
	bool SetPropertyFromStyle( CStyleProperty *pProperty );
	// Simplified version called from CStyleFileSet::ApplyMatchedStylesToPanelStyle
	// for CPanelStyle with no transition data. (only valid if CPanelStyle has no transition properties and 
	// CStyleFileSet::ApplyMatchedStylesToPanelStyle will not apply any transition properties
	bool SetPropertyFromStyleSimple( CStyleProperty *pProperty );

	bool RemoveProperty( CStyleSymbol symProperty );
	void BuildListOfExistingPropsNotInTree( CStyleProperty **rgToCheck, bool *rgPropertiesToRemove );

	float GetScaledTransitionTime( float flTime );

private:

	bool BPropertyBitFlagSet( CStyleSymbol hSymbol );
	void ClearHasProperty( CStyleSymbol hSymbol, int iIndex );
	void SetHasProperty( CStyleSymbol hSymbol, int iIndex );

	// Helper function used by SetPropertyFromStyle / SetPropertyFromStyleSimple
	void SetPropertyFromStyleHelper( CStyleProperty *pProperty, CStyleProperty *pPriorStyleProperty, bool bDeletePrior );

	// Helper for filling render commands
	// You must also declare a SetRenderData() to convert a property to the render command data type (base, transition, and data in animation frame's type)
	// - Your SetRenderData() must handle receiving a NULL property pointer, which will be the default base value
	template < class PROPERTY_TYPE, class DATA_TYPE > void FillRenderData( DATA_TYPE &data, CRenderCommandList &commandList, void *pContext = NULL );

	void SetRenderData( RenderMatrix4x4_t &data, CStylePropertyTransform3D *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyHueShift *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertySaturation *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyBrightness *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyContrast *pProperty, void *pContext );
	void SetRenderData( GaussianValues_t &data, CStylePropertyBlur *pProperty, void *pContext );
	void SetRenderData( RenderPoint_t &data, CStylePropertyPerspectiveOrigin *pProperty, void *pContext );
	void SetRenderData( TransformOriginData_t &data, CStylePropertyTransformOrigin *pProperty, void *pContext );
	void SetRenderData( RenderPoint2D_t &data, CStylePropertyScale2DCentered *pProperty, void *pContext );
	void SetRenderData( WashColor_t &data, CStylePropertyWashColor *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyOpacity *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyPerspective *pProperty, void *pContext );
	void SetRenderData( RadiusData_t &data, CStylePropertyBorderRadius *pProperty, void *pContext );
	void SetRenderData( BorderData_t &data, CStylePropertyBorder *pProperty, void *pContext );
	void SetRenderData( BoxShadowData_t &data, CStylePropertyBoxShadow *pProperty, void *pContext );
	void SetRenderData( TextShadowData_t &data, CStylePropertyTextShadow *pProperty, void *pContext );
	void SetRenderData( ImageShadowData_t &data, CStylePropertyImageShadow *pProperty, void *pContext );
	void SetRenderData( FillBrushCollection_t &data, CStylePropertyForegroundColor *pProperty, void *pContext );
	void SetRenderData( FillBrushCollection_t &data, CStylePropertyBackgroundColor *pProperty, void *pContext );
	void SetRenderData( RenderPoint_t &data, CStylePropertyPosition *pProperty, void *pContext );
	void SetRenderData( OpacityMask_t &data, CStylePropertyOpacityMask *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyRotate2DCentered *pProperty, void *pContext );
	void SetRenderData( ClipData_t &data, CStylePropertyClip *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyBackgroundImage *pProperty, void *pContext );
	void SetRenderData( float &data, CStylePropertyBackgroundImgOpacity *pProperty, void *pContext );

	// Internal data lookup helpers
	CStyleProperty * FindProperty( CStyleSymbol hSymbol, int *pIndex = NULL );
	CStyleProperty * FindPropertyInTransition( CStyleSymbol hSymbol, short *pIndex = NULL );

	// Helpers for sharing some code for fill brush msg population/creation
	void AddBrushesToRenderData( CFillBrushCollection::BrushVec_t &vecBrushes, FillBrushCollection_t &collection, CRenderCommandList &commandList, float flWidth, float flHeight );
	void PopulateLinearGradientRenderData( CFillBrush *pBrush, LinearGradient_t &gradient, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight );
	void PopulateRadialGradientRenderData( CFillBrush *pBrush, RadialGradient_t &gradient, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight );
	void PopulateParticleSystemRenderData( CFillBrush *pBrush, ParticleSystem_t &particleSystem, CRenderCommandList &commandList, float flRenderWidth, float flRenderHeight, uint64 ulParentPanelHandle, int iBrushIndex );

	// Get current interpolated for transition/animation value of a property
	CStyleProperty *GetInterpolatedProperty( CStyleSymbol hSymbol, bool bFinal );
	
	void CleanupTransitionsAndAnimations();
	void SetPanelLayoutFlagsForTransitionAnimation();
	void ScheduleTransitionCleanup( double flTime );

	// Helper to fill in transition & animation data in render commands
	void FillInTransitionData( TransitionData_t *pData, PropertyInTransition_t *pTransition );
	void FillInAnimationData( BaseAnimationData_t *pData, const CActiveAnimation *pAnimation );
	void FillInAnimationFrameData( BaseAnimationFrameData_t *pFrameData, const CActiveAnimation::PropertyFrameData_t &frameDataToCopy );

	int GetActiveAnimation( CPanoramaSymbol symAnimation );

	void UpdateFontStyleNoInherit( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight );
	void OnNewTransitionPropertySet( CStylePropertyTransitionProperties *pProperty, bool bTransitionPositionPreviouslySet );

	Vector GetActualUIScale();
	float GetActualUIScaleX();
	float GetActualUIScaleY();
	float GetActualUIScaleZ();

	Vector GetParentActualUIScale();

	// Helper functions for adding animations

	void AddNewActiveAnimationToVec( CUtlVector<CActiveAnimation*> &vec, AnimationProperty_t &animation, const VecKeyFrames_t &vecKeyframes );
	void SetActiveAnimations( CUtlVector<CActiveAnimation*> &vecAnimations );	// Resets vecAnimations

	IUIPanel *m_pPanel;		// pointer to the panel we are applied
	uint64 m_rgPropsPresent[2];
	// For properties with symbol IDs that fit in this array
	// we keep the direct index of the property so that FindProperty
	// doesn't have to do a search.  We've arranged so that
	// common properties register early and have low symbol
	// IDs to make this as effective as possible.
	// The size of this is totally arbitrary, but the number
	// of frequently referenced properties is not that small
	// so this cache misses quite a bit if it gets down below
	// 8 bytes.  Right now it's 12 bytes because alignment
	// in conjunction with m_flScaleFactor gives us 4 bytes
	// for free and then 8 bytes to keep the next vector 8-byte aligned.
	uint8 m_rgDirectPropIndex[12];

	CUtlVector< StyleEntry_t > m_vecProperties;
	CUtlRBTree< PropertyInTransition_t *, uint8, CStylePropertyInTransitionLess > m_treePropertiesInTransition;
	CUtlVector< CActiveAnimation * > *m_pvecActiveAnimations;

	static CUtlVector< StyleEntry_t > s_EmptyPropertyVec;
	CUtlVector< StyleEntry_t > *m_pVecElementProperties;		// properties set from code or the debugger
	double m_flLastTransitionCleanup;
	double m_flNextTransitionCleanup;
	CUIScheduledDel m_ScheduledTransitionCleanup;
};

// CJSKeyframesObject
// A class that represents an editable copy of keyframes from a style file set, part of CSSKeyframesRule functionality
class CJSKeyframesObject : public IUIJSObject
{
public:

	CJSKeyframesObject( CSmartPtr< CLayoutFile> pLayoutFile, const char *pchKeyframesName );
	~CJSKeyframesObject();

	int FindClosestKeyframe( float flPercent );
	bool IsKeyframeValid( int nKeyframeIndex );
	
	int InsertCopyOfKeyframe( float flDstPercent, int nSrcKeyframeIndex );
	void DeleteKeyframe( int nKeyframeIndex );

	void SetKeyframeProperty( int nKeyframeIndex, const char *pszStyleProperty );
	VecKeyFrames_t *GetKeyframes() { return m_pKeyframes; }

private:

	VecKeyFrames_t *m_pKeyframes = nullptr;

	virtual const char *GetJSTypeName() { return "Keyframes"; }
	void SetupJavaScriptObjectTemplate();
};

} // namespace panorama

#endif //STYLES_H

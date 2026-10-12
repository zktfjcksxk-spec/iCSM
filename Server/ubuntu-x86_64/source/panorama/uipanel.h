//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CUIPANEL_H
#define CUIPANEL_H
#pragma once

#include "utlstring.h"
#include "panoramatypes.h"
#include "iuipanel.h"
#include "iuipanelclient.h"
#include "utlvector.h"
#include "layout/stylefiletypes.h"
#include "uiengine.h"
#include "layout/layoutfile.h"
#include "renderer/styles.h"

namespace panorama
{

class CLayoutFile;
class IUIWindow;


struct PanelEvent_t
{
	enum EEventType
	{
		k_EEventType_None,
		k_EEventType_UIEventArray,
		k_EEventType_JSScript,
		k_EEventType_JSFunction,
	};

	PanelEvent_t()
	{
		eType = k_EEventType_None;
	}

	EEventType eType;
	union PanelEventData_t
	{
		PanelEventData_t() { V_memset( this, 0, sizeof( *this ) ); }

		VecUIEvents_t *pVecIUIEvent;
		v8::Persistent<v8::Script> *pJSScript;
		v8::Persistent<v8::Function> *pJSFunction;
	} data;

	// only set for java scripts
	CPanelPtr< IUIPanel > pJSContext;
};


//-----------------------------------------------------------------------------
// Purpose: Struct containing event info used when parsing layout file
//-----------------------------------------------------------------------------
struct PanelEventsToParse_t
{
	panorama::IUIPanel *m_pPanel;
	panorama::CPanoramaSymbol m_symProperty;
	const char *m_pchEvent;
};


//-----------------------------------------------------------------------------
// Purpose: Class to manage sounds for scrolling which helps crossfade fast scroll sound
//-----------------------------------------------------------------------------
class CFastScrollSoundManager
{
public:
	CFastScrollSoundManager();
	~CFastScrollSoundManager();

	void Play( int nRepeats, float flPan );

private:

	bool OnTimeoutFastScrollSound();

	int m_nLastRepeats;
	double m_flLastFrameTime;
	HAUDIOSAMPLE m_hFastScrollSound;
};

typedef CUtlMap< CPanoramaSymbol, IUIPanel *, int, CDefLess< CPanoramaSymbol > > MapParentsByType_t;
typedef CUtlMap< const char *, IUIPanel *, int, CDefStringLess > MapParentsByID_t;
typedef CUtlMap< CPanoramaSymbol, IUIPanel *, int, CDefLess< CPanoramaSymbol > > MapParentsByClass_t;

//-----------------------------------------------------------------------------
// Purpose: Basic panel interface exposing operations used inside of panorama, rather
// than operations that are part of building/laying out controls in the panorama_client module
//-----------------------------------------------------------------------------
class CUIPanel /*FINAL*/ : public panorama::IUIPanel
{
public:

	CUIPanel();

	virtual ~CUIPanel();

	// Initialize panel
	virtual void Initialize( IUIWindow *window, IUIPanel *parent, const char *pchID, uint32 ePanelFlags ) OVERRIDE;

	// Initialize a cloned panel from ourself
	virtual void InitClonedPanel( IUIPanel *pClone ) OVERRIDE;

	// Do registration of event handlers on Panel2D panel type
	virtual void RegisterEventHandlersOnPanel2DType( CPanoramaSymbol symPanelType ) OVERRIDE;

	// Shutdown panel, should only happen right before actual deletion
	virtual void Shutdown() OVERRIDE;

	// Fire panel loaded event and mark us as now loaded, images and other panels may call this late
	virtual void FirePanelLoadedEvent() OVERRIDE;

	// Sets client type pointer value
	virtual void SetClientPtr( panorama::IUIPanelClient *pPtr ) OVERRIDE{ m_pClientPtr = pPtr; }

	// Gets client pointer
	virtual panorama::IUIPanelClient *ClientPtr() const OVERRIDE{ return m_pClientPtr; }

	virtual void SetID( const char * pchID ) OVERRIDE { m_strID = pchID; }
	virtual const char *GetID() const OVERRIDE { return m_strID.String(); }
	virtual bool BHasID() const OVERRIDE { return !m_strID.IsEmpty(); }

	// Get panel type, which really calls back into client panel object
	virtual CPanoramaSymbol GetPanelType() const OVERRIDE { return m_pClientPtr->GetPanelType(); }

	// sets & loads the layout file for this panel
	virtual bool BLoadLayout( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) OVERRIDE;

	// Considers a layout load failure a fatal error.
	virtual void RequireLoadLayout( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) OVERRIDE;

	// sets & loads the layout for this panel
	virtual bool BLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting = false, bool bPartialLayout = false ) OVERRIDE;

	// Considers a layout load failure a fatal error.
	virtual void RequireLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting = false, bool bPartialLayout = false ) OVERRIDE;

	// Loads a snippet from the panel's current layout file
	virtual bool BLoadLayoutSnippet( const char *pchSnippetName ) OVERRIDE;

	// Loads a snippet and considers failure a fatal error
	virtual void RequireLoadLayoutSnippet( const char *pchSnippetName ) OVERRIDE;

	// Returns true if a snippet is available by the given name
	virtual bool BHasLayoutSnippet( const char *pchSnippetName ) OVERRIDE;

	// sets loads the layout file for this panel, asynchronously supporting remote http:// paths
	virtual void LoadLayoutAsync( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) OVERRIDE;

	// loads the layout file for this panel, asynchronously supporting remote http:// paths in css within
	virtual void LoadLayoutFromStringAsync( const char *pchXMLString, bool bOverrideExisting, bool bPartialLayout = false ) OVERRIDE;

	// creates & appends child panels from a string. String XML should only include XML of children, not this panel as a wrapper
	virtual bool BCreateChildren( const char *pchXML ) OVERRIDE;

	// unload the layout file for this panel, destroying all children
	virtual void UnloadLayout( void ) OVERRIDE;

	// Check if the panel has loaded layout
	virtual bool IsLoaded() const OVERRIDE { return m_bLoaded; }

	virtual void SetParent( panorama::IUIPanel *pParent ) OVERRIDE;
	virtual panorama::IUIPanel *GetParent() const OVERRIDE { return m_pParent; }

	virtual IUIWindow *GetParentWindow() const OVERRIDE { return (IUIWindow*)m_pWindow; }

	virtual void SetVisible( bool bVisible ) OVERRIDE;
	virtual bool BIsVisible() const OVERRIDE;

	virtual bool BIsTransparent() const OVERRIDE;

	// searches only immediate children
	virtual IUIPanel *FindChild( const char *pchID ) OVERRIDE;

	// Considers a failure to find a child a fatal error.
	virtual IUIPanel *RequireChild( const char *pchID ) OVERRIDE;

	// searches all children even outside layout file scope
	virtual IUIPanel *FindChildTraverse( const char *pchID ) OVERRIDE;

	// Considers a failure to find a child a fatal error.
	virtual IUIPanel *RequireChildTraverse( const char *pchID ) OVERRIDE;

	// searches any children created from our layout file
	virtual IUIPanel *FindChildInLayoutFile( const char *pchID ) OVERRIDE;

	// Considers a failure to find a child a fatal error.
	virtual IUIPanel *RequireChildInLayoutFile( const char *pchID ) OVERRIDE;

	// searches any panel created from our layout file (so parents or children!)
	virtual IUIPanel *FindPanelInLayoutFile( const char *pchID ) OVERRIDE;

	// Considers a failure to find a child a fatal error.
	virtual IUIPanel *RequirePanelInLayoutFile( const char *pchID ) OVERRIDE;

	// Check if this panel is a descendant of the passed panel
	virtual bool IsDescendantOf( const IUIPanel *pPanel ) const OVERRIDE;

	// Remove and delete all children from panel
	virtual void RemoveAndDeleteChildren() OVERRIDE;

	// Remove and delete all children matching type
	virtual void RemoveAndDeleteChildrenOfType( CPanoramaSymbol symPanelType ) OVERRIDE;

	// Get child count of specific type
	virtual uint32 GetChildCountOfType( CPanoramaSymbol symPanelType ) OVERRIDE;

	// Child access
	virtual int GetChildCount() const OVERRIDE;
	virtual IUIPanel *GetChild( int i ) const OVERRIDE;
	virtual IUIPanel *GetFirstChild() const OVERRIDE;
	virtual IUIPanel *GetLastChild() const OVERRIDE;

	// Return index of child in creation/panel vector order (also default tab order)
	virtual int GetChildIndex( const IUIPanel *pChild ) const OVERRIDE;

	// Special children to be rendered in debugger
	virtual int GetHiddenChildCount() const OVERRIDE;
	virtual IUIPanel *GetHiddenChild( int i ) OVERRIDE;

	// Find ancestor with matching id
	virtual IUIPanel *FindAncestor( const char *pchID ) const OVERRIDE;

	// Find the lowest common ancestor between this panel and another panel
	virtual IUIPanel *FindLowestCommonAncestor( IUIPanel *pOther ) const OVERRIDE;

	virtual void SetRepaint( panorama::EPanelRepaint eRepaintNeeded ) OVERRIDE;
	virtual void SetRepaintOnAncestors() OVERRIDE;
	virtual void SetNeedsPaintArea( bool bNeedsPaintArea ) OVERRIDE { m_bNeedsPaintArea = bNeedsPaintArea; }

	bool BApplyLayoutFile( LayoutFilePtr_t pLayoutFile, CUtlVector< panorama::IUIPanel * > *pvecExistingPanels, bool bIsReload );
	bool BApplyLayoutSnippet( LayoutFilePtr_t pLayoutFile, const char *pchSnippetName, CUtlVector< panorama::IUIPanel * > *pvecExistingPanels );

	virtual void EnableBackgroundMovies( bool bEnabled ) OVERRIDE;

	// Measure self and children. First pass of layout
	virtual void DesiredLayoutSizeTraverse( float flMaxWidth, float flMaxHeight ) OVERRIDE;
	virtual void DesiredLayoutSizeTraverse( float *pflDesiredWidth, float *pflDesiredHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE;

	// Tell us what your content size is
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE;

	// Arrange children. Second pass of layout
	virtual void LayoutTraverse( float x, float y, float flFinalWidth, float flFinalHeight ) OVERRIDE;

	// Actual child placement, default implementation
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;

	// called by flowing and custom layout passes to set position. Will properly handle transition being applied with styles
	virtual void SetPositionFromLayoutTraverse( CUILength x, CUILength y, CUILength z ) OVERRIDE;

	// methods to invalid certain parts of layout
	virtual void InvalidateSizeAndPosition() OVERRIDE;
	virtual void InvalidatePosition() OVERRIDE;
	virtual void SetActiveSizeAndPositionTransition() OVERRIDE;
	virtual void SetActivePositionTransition() OVERRIDE;
	virtual bool IsSizeValid() OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutSizeDirty) == 0; }
	virtual bool IsPositionValid() OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutPositionDirty) == 0; }
	virtual bool IsChildSizeValid() OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutChildSizeDirty) == 0; }
	virtual bool IsChildPositionValid() OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutChildPositionDirty) == 0; }
	virtual bool IsSizeTransitioning() OVERRIDE { return (m_unPanelLayoutFlags & (k_EPanelLayoutSizeTransitionActive | k_EPanelLayoutChildSizeTransitionActive)) != 0; }
	virtual bool IsPositionTransitioning() OVERRIDE { return (m_unPanelLayoutFlags & (k_EPanelLayoutPositionTransitionActive | k_EPanelLayoutChildPositionTransitionActive)) != 0; }
	virtual bool IsChildPositionTransitioning() OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutChildPositionTransitionActive) != 0; }
	virtual bool IsChildSizeTransitioning() OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutChildSizeTransitionActive) != 0; }
	virtual void TransitionPositionApplied( bool bImmediate ) OVERRIDE;

	// A hack has been introduced to re-run the layout traverse on a panel if the opacity is changing
	// as we early out in OnContentSizeTraverse on panels with opacity 0. Ideally we only need to 
	// re-run the layout traverse if the opacity is changing from 0 to any other value. Instead of 
	// globally doing that change for all panels, the methods below have been introduced to disable
	//  the "size and position invalidation" on a panel when the opacity is changing and it is therefore the 
	// responsibility of the caller to invalidate size and position on the panel when opacity is changing 
	// from 0 to any other value.
	virtual void SetInvalidateSizeAndPositionOnOpacityChangeDisabled( bool bDisable ) OVERRIDE { m_bInvalidateSizeAndPositionOnOpacityChangeDisabled = bDisable; }
	virtual bool BInvalidateSizeAndPositionOnOpacityChangeDisabled() OVERRIDE { return m_bInvalidateSizeAndPositionOnOpacityChangeDisabled; }

	// size getters
	virtual float GetDesiredLayoutWidth() const OVERRIDE { return m_flDesiredLayoutWidth; }
	virtual float GetDesiredLayoutHeight() const OVERRIDE { return m_flDesiredLayoutHeight; }

	// Content size is what our contents actually take up, not accounting for fixed/relative
	// size set on us in styles which affect desired layout size
	virtual float GetContentWidth() const OVERRIDE { return m_flContentWidth; }
	virtual float GetContentHeight() const OVERRIDE { return m_flContentHeight; }

	// Actual size is the size given to the panel after layout, hopefully as big as its desired size.  
	// Actual size does NOT include margins (which are really in the parent).
	virtual float GetActualLayoutWidth() const OVERRIDE { return m_flActualLayoutWidth; }
	virtual float GetActualLayoutHeight() const OVERRIDE { return m_flActualLayoutHeight; }

	// Render size is the size of the content for rendering, this is either the actual layout size, or if
	// that is smaller than the content size + padding then it's the content size + padding.
	virtual float GetActualRenderWidth() OVERRIDE;
	virtual float GetActualRenderHeight() OVERRIDE;

	// Offset will include position, alignment, and margin adjustments
	virtual float GetActualXOffset() const OVERRIDE;
	virtual float GetActualYOffset() const OVERRIDE;

	// returns offset for drawing minus position
	virtual float GetRawActualXOffset() const OVERRIDE { return m_flActualXOffset; }
	virtual float GetRawActualYOffset() const OVERRIDE { return m_flActualYOffset; }

	// Returns the calculated UI scale for this panel
	virtual Vector GetActualUIScale() const OVERRIDE { return m_vActualUIScale; }
	virtual float GetActualUIScaleX() const OVERRIDE { return m_vActualUIScale.x; }
	virtual float GetActualUIScaleY() const OVERRIDE { return m_vActualUIScale.y; }
	virtual float GetActualUIScaleZ() const OVERRIDE { return m_vActualUIScale.z; }

	virtual Vector GetParentActualUIScale() const OVERRIDE;

	// Called to force a recalculation of this panel's cached UI scale and all of its descendants.
	void RecalculateUIScale( bool bFinal, const Vector *pvOldParentUIScale );

	// Offset to apply to contents for scrolling
	virtual float GetContentsXScrollOffset() const OVERRIDE { return m_pHorizontalScrollData ? m_pHorizontalScrollData->m_flOffset : 0.0f; }
	virtual float GetContentsYScrollOffset() const OVERRIDE { return m_pVerticalScrollData ? m_pVerticalScrollData->m_flOffset : 0.0f; }
	virtual float GetContentsXScrollOffsetTarget() const OVERRIDE { return m_pHorizontalScrollData ? m_pHorizontalScrollData->m_flOffsetTarget : FLT_MAX; }
	virtual float GetContentsYScrollOffsetTarget() const OVERRIDE { return m_pVerticalScrollData ? m_pVerticalScrollData->m_flOffsetTarget : FLT_MAX; }
	virtual double GetContentsXScrollTransitionStart() const OVERRIDE { return m_pHorizontalScrollData ? m_pHorizontalScrollData->m_flTransitionStart : 0.0f; }
	virtual double GetContentsYScrollTransitionStart() const OVERRIDE { return m_pVerticalScrollData ? m_pVerticalScrollData->m_flTransitionStart : 0.0f; }
	virtual double GetContentsXScrollTransitionTime() const OVERRIDE { return m_pHorizontalScrollData ? m_pHorizontalScrollData->m_flTransitionDuration : 0.0f; }
	virtual double GetContentsYScrollTransitionTime() const OVERRIDE { return m_pVerticalScrollData ? m_pVerticalScrollData->m_flTransitionDuration : 0.0f; }
	virtual EAnimationTimingFunction GetContentsXScrollTransitionTimingFunction() const OVERRIDE { return m_pHorizontalScrollData ? m_pHorizontalScrollData->m_eTimingFunction : k_EAnimationNone; }
	virtual EAnimationTimingFunction GetContentsYScrollTransitionTimingFunction() const OVERRIDE { return m_pVerticalScrollData ? m_pVerticalScrollData->m_eTimingFunction : k_EAnimationNone;; }
	virtual void GetContextXScrollTransitionControlPoints( Vector2D (&vecPoints)[4] ) const OVERRIDE;
	virtual void GetContextYScrollTransitionControlPoints( Vector2D (&vecPoints)[4] ) const OVERRIDE;
	virtual float GetInterpolatedXScrollOffset() OVERRIDE;
	virtual float GetInterpolatedYScrollOffset() OVERRIDE;
	virtual bool BScrollInProgress() OVERRIDE;
	virtual float StopHorizontalScroll();
	virtual float StopVerticalScroll();


	// Does the panel implement drag scroll?
	virtual bool BCanDragScroll() OVERRIDE;

	// Can the panel scroll further up or down?
	virtual bool BCanScrollUp() OVERRIDE;
	virtual bool BCanScrollDown() OVERRIDE;
	virtual bool BCanScrollLeft() OVERRIDE;
	virtual bool BCanScrollRight() OVERRIDE;

	// style class management
	virtual void AddClass( const char *pchName ) OVERRIDE;
	virtual void AddClass( CPanoramaSymbol symbol ) OVERRIDE{ AddClassesInternal( &symbol, 1, false ); }
	virtual void AddClasses( const char *pchName ) OVERRIDE;
	virtual void AddClasses( CPanoramaSymbol *pSymbols, uint cSymbols ) OVERRIDE{ AddClassesInternal( pSymbols, cSymbols, false ); }
	virtual void RemoveClass( const char *pchName ) OVERRIDE;
	virtual void RemoveClass( CPanoramaSymbol symName ) OVERRIDE{ RemoveClasses( &symName, 1 ); }
	virtual void RemoveClasses( const CPanoramaSymbol *pSymbols, uint cSymbols ) OVERRIDE;
	virtual void RemoveClasses( const char *pchName ) OVERRIDE;
	virtual void RemoveAllClasses() OVERRIDE;
	virtual const CUtlVector< CPanoramaSymbol > &GetClasses() const OVERRIDE{ return m_vecStyleClasses; }
	virtual bool BHasClass( const char *pchName ) OVERRIDE;
	virtual bool BHasClass( CPanoramaSymbol symName ) OVERRIDE;
	virtual bool BAscendantHasClass( const char *pchName ) OVERRIDE;
	virtual bool BAscendantHasClass( CPanoramaSymbol symName ) OVERRIDE;
	virtual void ToggleClass( const char *pchName ) OVERRIDE;
	virtual void ToggleClass( CPanoramaSymbol symName ) OVERRIDE;
	virtual void SetHasClass( const char *pchName, bool bHasClass ) OVERRIDE;
	virtual void SetHasClass( CPanoramaSymbol symName, bool bHasClass ) OVERRIDE;
	virtual void SwitchClass( const char *pchAttribute, const char *pchName ) OVERRIDE;
	virtual void SwitchClass( const char *pchAttribute, CPanoramaSymbol symName ) OVERRIDE;
	virtual void SwitchClass( CPanoramaSymbol symAttribute, const char *pchName ) OVERRIDE;
	virtual void SwitchClass( CPanoramaSymbol symAttribute, CPanoramaSymbol symName ) OVERRIDE;
	virtual void TriggerClass( const char *pchName ) OVERRIDE;
	virtual void TriggerClass( CPanoramaSymbol symName ) OVERRIDE;

	virtual bool BAcceptsInput() OVERRIDE;
	virtual void SetAcceptsInput( bool bAllowInput ) OVERRIDE;
	virtual bool BAcceptsFocus() const OVERRIDE { return (m_unInputFlags & k_EInputAcceptFocus) != 0; }
	virtual void SetAcceptsFocus( bool bAllowFocus ) OVERRIDE;

	virtual bool BAlwaysConsumeHoverClicks() OVERRIDE { return ( m_unInputFlags & k_EInputAlwaysConsumeHoverClicks ) != 0; }
	virtual void SetAlwaysConsumeHoverClicks( bool bAlwaysConsumeHoverClicks ) OVERRIDE;
	virtual void SetCanClearFocusByClicking( bool bCanClearFocusByClicking ) OVERRIDE;


	virtual bool BScrollParentToFitWhenFocused() OVERRIDE { return m_bScrollParentToFitWhenFocused; }
	virtual void SetScrollParentToFitWhenFocused( bool bScrollParentToFit ) OVERRIDE { m_bScrollParentToFitWhenFocused = bScrollParentToFit; }
	
	// true if it would normally sink input, but may not right now if disabled
	virtual bool BCanAcceptInput() OVERRIDE; 

	virtual void SetDefaultFocus( const char *pchChildID ) OVERRIDE;
	virtual const char *GetDefaultFocus() const OVERRIDE;
	virtual void SetDisableFocusOnMouseDown( bool bDisable ) OVERRIDE;
	virtual bool BFocusOnMouseDown() OVERRIDE{  return (m_unInputFlags & k_EDisableFocusOnMouseDown) == 0; }
	virtual bool BCanClearFocusByClicking() OVERRIDE { return ( m_unInputFlags & k_ECanClearFocusByClicking ) != 0; }


	// Should this panel be the top of an input hierarchy and keep track of focus within itself, not losing focus when a panel in some
	// other hierarchy changes focus?  Use this for panels that are peers like friends vs browser vs mainmenu in tenfoot
	virtual bool BTopOfInputContext() OVERRIDE { return (m_unInputFlags & k_EInputTopOfContext) != 0; }
	virtual void SetTopOfInputContext( bool bIsTopOfInputContext ) OVERRIDE
	{
		if( bIsTopOfInputContext )
			m_unInputFlags |= (uint32)k_EInputTopOfContext;
		else
			m_unInputFlags &= ~((uint32)k_EInputTopOfContext);
	}

	virtual IUIPanel *GetParentInputContext() OVERRIDE;

	// Get the default input focus child within this panel, may be null
	virtual IUIPanel *GetDefaultInputFocus() OVERRIDE;

	// Set focus to this panel, which will auto-scroll it into full view as well if parent has overflow: scroll
	virtual bool SetFocus() OVERRIDE;

	// Set the focus to this panel in it's input context, but do not make the context change if some other context currently
	// has focus
	virtual bool UpdateFocusInContext() OVERRIDE;

	// Set the focus in response to receiving hover (on panels that a parent sets childfocusonhover), this will
	// never scroll the parent.
	virtual bool SetFocusDueToHover() OVERRIDE;

	virtual void SetInputContextFocus() OVERRIDE;

	// retrieve the style flags (map to CSS psuedo-classes) for this panel
	virtual uint GetStyleFlags() const OVERRIDE { return m_unStyleFlags; }
	virtual void AddStyleFlag( EStyleFlags eStyleFlag ) OVERRIDE;
	virtual void RemoveStyleFlag( EStyleFlags eStyleFlag ) OVERRIDE;
	virtual void SetDisallowedStyleFlags( uint unDisallowedStyleFlags ) OVERRIDE;
	virtual uint GetDisallowedStyleFlags() const OVERRIDE { return m_unDisallowedStyleFlags; }
	virtual bool IsInspected() const OVERRIDE { return (m_unStyleFlags & k_EStyleFlagInspect) != 0; }
	virtual bool BHasHoverStyle() const OVERRIDE { return (m_unStyleFlags & k_EStyleFlagHover) != 0; }
	virtual void SetSelected( bool bSelected ) OVERRIDE { bSelected ? AddStyleFlag( k_EStyleFlagSelected ) : RemoveStyleFlag( k_EStyleFlagSelected ); }
	virtual bool IsSelected() const OVERRIDE { return (m_unStyleFlags & k_EStyleFlagSelected) != 0; }
	virtual bool BHasKeyFocus() const OVERRIDE { return (m_unStyleFlags & k_EStyleFlagFocus) != 0; }
	virtual bool BHasDescendantKeyFocus() const OVERRIDE { return (m_unStyleFlags & k_EStyleFlagDescendantFocused) != 0; }
	virtual bool IsLayoutLoading() const OVERRIDE { return (m_unStyleFlags & k_EStyleFlagLayoutLoading) != 0; }

	// enable/disable
	virtual void SetEnabled( bool bEnabled ) OVERRIDE;
	virtual bool IsEnabled() const OVERRIDE
	{
		if( (m_unStyleFlags & k_EStyleFlagDisabled) != 0 || (m_unStyleFlags & k_EStyleFlagParentDisabled) != 0 )
			return false;

		return true;
	}

	virtual bool IsActivationEnabled() OVERRIDE
	{
		if( (m_unStyleFlags & k_EStyleFlagDisabled) != 0 || (m_unStyleFlags & k_EStyleFlagParentDisabled) != 0 )
			return false;

		if( (m_unStyleFlags & k_EStyleFlagActivationDisabled) != 0 )
			return false;

		return true;
	}

	// Set activation disabled on this panel, input/focus still generally work, but Activate events won't be handled, useful to prevent a button
	// being clicked when out of focus, but leave it able to be focused for later activation or such
	virtual void SetActivationEnabled( bool bEnabled ) OVERRIDE;

	// Set all our immediate children enabled/disabled
	virtual void SetAllChildrenActivationEnabled( bool bEnabled ) OVERRIDE;

	// Enable/disable hit testing of this panel, you may want a parent that is never hit test that has a large region, but clicks
	// just pass through to other things behind it.  Children may still hit test.
	virtual void SetHitTestEnabled( bool bEnabled ) OVERRIDE
	{
		if( bEnabled )
			m_unInputFlags |= (uint32)k_EInputPerformHitTest;
		else
			m_unInputFlags &= ~((uint32)k_EInputPerformHitTest);
	}
	virtual bool BHitTestEnabled() const OVERRIDE { return (m_unInputFlags & k_EInputPerformHitTest) != 0; }
	virtual void SetHitTestEnabledTraverse( bool bEnabled ) OVERRIDE;

	// Enable/disable hit testing on children of this panel. Prevents recursing into children when doing hit testing,
	// thus it override children's individual hit test flags.
	virtual void SetHitTestChildrenEnabled( bool bEnabled ) OVERRIDE
	{
		if( bEnabled )
			m_unInputFlags |= (uint32)k_EInputPerformHitTestChildren;
		else
			m_unInputFlags &= ~((uint32)k_EInputPerformHitTestChildren);
	}
	virtual bool BHitTestChildrenEnabled() const OVERRIDE { return (m_unInputFlags & k_EInputPerformHitTestChildren) != 0; }

	// drag/drop
	virtual void SetDraggable( bool bEnabled ) OVERRIDE;
	virtual bool IsDraggable() const OVERRIDE { return m_bDraggable; }

	virtual void SetRememberChildFocus( bool bRememberChildFocus ) OVERRIDE;
	virtual bool GetRememberChildFocus() const OVERRIDE { return m_bRememberChildFocus; }
	virtual void ClearLastChildFocus() OVERRIDE;

	virtual void SetNeedsIntermediateTexture( bool bNeedsIntermediateTexture ) OVERRIDE { m_bNeedsIntermediateTexture = bNeedsIntermediateTexture; }
	virtual bool GetNeedsIntermediateTexture() const OVERRIDE { return m_bNeedsIntermediateTexture; }

	virtual void SetCompositionLayerTextureName( const char *pszCompositionLayerTextureName ) OVERRIDE { m_symCompositionLayerTextureName = pszCompositionLayerTextureName; }
	virtual const char *GetCompositionLayerTextureName() const OVERRIDE { return m_symInputNamespace.String(); }

	virtual void SetClipAfterTransform( bool bClipAfterTransform ) OVERRIDE { m_bClipAfterTransform = bClipAfterTransform; }
	virtual bool GetClipAfterTransform() const OVERRIDE { return m_bClipAfterTransform; }

	// the input namespace to use for this panel
	virtual const char *GetInputNamespace() const OVERRIDE { return m_symInputNamespace.String(); }

	virtual void SetInputNamespace( const char *pchNamespace ) OVERRIDE { m_symInputNamespace = pchNamespace; }

	// Check if styles are dirty for the panel
	virtual bool BStylesDirty() const OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutStylesDirty) != 0; }

	// Check if styles are possibly dirty for any of our children
	virtual bool BChildStylesDirty() OVERRIDE { return (m_unPanelLayoutFlags & k_EPanelLayoutChildStylesDirty) != 0; }

	// Parse panel event for this panel
	virtual bool BParsePanelEvent( CPanoramaSymbol symPanelEvent, const char *pchValue, IUIPanel *pJavascriptContext ) OVERRIDE;

	// Check if panel event is set on panel for event type
	virtual bool BIsPanelEventSet( CPanoramaSymbol symPanelEvent ) OVERRIDE;

	// Check if the event is a valid panel event type
	virtual bool BIsPanelEvent( CPanoramaSymbol symPanelEvent ) const OVERRIDE;

	// Dispatch the panel event if the panel has something set for it now
	virtual bool DispatchPanelEvent( CPanoramaSymbol symPanelEvent ) OVERRIDE;

	// Get the containing panel for this panels javascript context
	virtual panorama::IUIPanel *GetJavaScriptContextParent() const OVERRIDE;

	// Mark styles dirty for the panel
	virtual void MarkStylesDirty( bool bIncludeChildren ) OVERRIDE;

	// Mark child styles are dirty for the panel's parent 
	virtual void MarkChildStylesDirtyOnParents() OVERRIDE;

	virtual void SetLayoutLoadedFromParent( panorama::IUIPanel *pParent ) OVERRIDE;
	virtual void SetPanelIntoContext( panorama::IUIPanel *pParent ) OVERRIDE;

	// Returns the layout file for this panel
	virtual CPanoramaSymbol GetLayoutFile() const OVERRIDE;

	virtual CPanoramaSymbol GetLayoutFileLoadedFrom() const OVERRIDE;
	virtual int GetLayoutFileReloadCount() const OVERRIDE;

	// used to determine if sensitive JS functions should run with this panel as context (AsyncWebRequest, etc.)
	virtual const char *GetLayoutFilePathForJSCheck() const OVERRIDE;
	virtual void SetLayoutFilePathForJSCheck( const char *pchPath ) OVERRIDE;

	virtual void SetLayoutFile( CPanoramaSymbol symLayoutFile ) OVERRIDE;

	// Access panel's style information
	virtual panorama::IUIPanelStyle * AccessIUIStyle() { return (panorama::IUIPanelStyle *)AccessStyle(); }

	// Access panel's style information allowing dirty styles to be returned
	virtual panorama::IUIPanelStyle *AccessIUIStyleDirty() { return (panorama::IUIPanelStyle *)&m_style; }

	virtual void ApplyStyles( bool bTraverse ) OVERRIDE;
	virtual void AfterStylesApplied( bool bStylesChanged, EStyleRepaint eRepaint, bool bInheritablePropertiesChanged, bool bTraverse ) OVERRIDE;

	virtual void SetOnStylesChangedNeeded() OVERRIDE { Assert( BStylesDirty() ); m_bNeedOnStylesChanged = true; }

	virtual CUtlVector<IUIPanel *> const &AccessChildren() OVERRIDE { return m_vecChildren;  }

	// Accessor for appropriate image manager for panel
	virtual IUIImageManager* UIImageManager() OVERRIDE;

	// Accessor for appropriate 3d surface interface for this panel
	virtual panorama::IUIRenderEngine *UIRenderEngine() OVERRIDE;

	// Accessor for appropriate 3d surface interface for this panel
	virtual panorama::IUIRenderDevice *UIRenderDevice() OVERRIDE;

	// Paint the panel and all it's children
	virtual void PaintTraverse( PanoramaRect_t *pPaintArea, bool bUseForceBuiltPaintCmdCache ) OVERRIDE;

	// Tab index setting
	virtual void SetTabIndex( float flTabIndex ) OVERRIDE { m_flTabIndex = flTabIndex; }
	virtual float GetTabIndex() const OVERRIDE;
	virtual void SetSelectionPosition( float flXPos, float flYPos ) OVERRIDE { m_flSelectionPosX = flXPos; m_flSelectionPosY = flYPos; }
	virtual void SetSelectionPositionX( float flXPos ) OVERRIDE { m_flSelectionPosX = flXPos; }
	virtual void SetSelectionPositionY( float flYPos ) OVERRIDE { m_flSelectionPosY = flYPos; }
	virtual float GetSelectionPositionX() const OVERRIDE;
	virtual float GetSelectionPositionY() const OVERRIDE;
	virtual float GetTabIndex_Raw() const OVERRIDE { return m_flTabIndex; }
	virtual float GetSelectionPositionX_Raw() const OVERRIDE { return m_flSelectionPosX; }
	virtual float GetSelectionPositionY_Raw() const OVERRIDE { return m_flSelectionPosY; }

	virtual bool SetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float flYStart ) OVERRIDE;
	virtual bool SetInputFocusToFirstOrLastChildInFocusOrder( EFocusMoveDirection moveType, float flXStart, float flYStart ) OVERRIDE;

	virtual bool BSelectionPosVerticalBoundary() OVERRIDE { return m_bSelectionPosVerBoundary; }
	virtual bool BSelectionPosHorizontalBoundary() OVERRIDE { return m_bSelectionPosHorBoundary; }

	// controls if clicking on an unfocused panel should set focus
	virtual void SetChildFocusOnHover( bool bEnable ) OVERRIDE { m_bChildFocusOnHover = bEnable; }
	virtual bool GetChildFocusOnHover() OVERRIDE { return m_bChildFocusOnHover; }

	// controls if hovering on an unfocused panel should set focus
	virtual void SetFocusOnHover(bool bEnable) OVERRIDE{ m_bFocusOnHover = bEnable; }
	virtual bool GetFocusOnHover() OVERRIDE{ return m_bFocusOnHover; }

	// Scrolling controls
	virtual void ScrollToTop() OVERRIDE;
	virtual void ScrollToBottom() OVERRIDE;
	virtual void ScrollToLeftEdge() OVERRIDE;
	virtual void ScrollToRightEdge() OVERRIDE;
	virtual void ScrollParentToMakePanelFit( ScrollBehavior_t behavior, bool bImmediateScroll ) OVERRIDE;
	virtual void ScrollToFitRegion( float x0, float x1, float y0, float y1, ScrollBehavior_t behavior, bool bDirectParentScrollOnly = false, bool bImmediateScroll = false ) OVERRIDE;
	virtual bool BCanSeeInParentScroll() OVERRIDE;

	virtual void OnScrollPositionChanged() OVERRIDE;
	virtual void SetSendChildScrolledIntoViewEvents( bool bSendChildScrolledIntoViewEvents ) OVERRIDE;		// this must be enabled on your parent for ScrolledIntoView events to fire and IsScrolledIntoView state to be set
	virtual bool OnCheckChildrenScrolledIntoView() OVERRIDE;
	void OnCheckChildrenScrolledIntoViewRecursive( float x0, float x1, float y0, float y1 );
	virtual void FireScrolledIntoViewEvent() OVERRIDE;
	virtual void FireScrolledOutOfViewEvent() OVERRIDE;
	virtual bool IsScrolledIntoView() const OVERRIDE { return m_bScrolledIntoView; }

	// Direct child management
	virtual void SortChildren( std::function< int( IUIPanelClient *, IUIPanelClient * ) > fnCompare ) OVERRIDE;

	// child management, use with caution! normally always managed internally.
	virtual void AddChild( IUIPanel *pChild ) OVERRIDE;

	// child management, use with caution! normally always managed internally.  Returns child index we inserted at.
	virtual int AddChildSorted( bool( __cdecl *pfnLessFunc )(ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2), IUIPanel *pChild ) OVERRIDE;

	// re-sort a newly inserted child
	virtual int ReSortChild( bool( __cdecl *pfnLessFunc )( ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2 ), IUIPanel *pChild ) OVERRIDE;

	// child management, use with caution! normally always managed internally.
	virtual void RemoveChild( IUIPanel *pChild ) OVERRIDE;

	// Move child after another child
	virtual void MoveChildAfter( IUIPanel *pChildToMove, IUIPanel *pBefore ) OVERRIDE;

	// Move child before another child
	virtual void MoveChildBefore( IUIPanel *pChildToMove, IUIPanel *pAfter ) OVERRIDE;

	virtual EMouseCursors GetPanelMouseCursor() OVERRIDE;
	virtual void SetPanelMouseCursor( EMouseCursors eCursor ) OVERRIDE;

	virtual void SetMouseCanActivate( EMouseCanActivate eMouseCanActivate, const char *pchOptionalParent = NULL ) OVERRIDE;
	virtual EMouseCanActivate GetMouseCanActivate() OVERRIDE { return m_eMouseCanActivate; }
	virtual IUIPanel *FindParentForMouseCanActivate() OVERRIDE;

	virtual bool BReloadLayout( CPanoramaSymbol symPath ) OVERRIDE;

	virtual void ReloadStyleFileTraverse( CPanoramaSymbol symPath ) OVERRIDE;

	virtual bool BHasOnActivateEvent() OVERRIDE;
	virtual bool BHasOnMouseActivateEvent() OVERRIDE;

	// Called to ask us to setup object template for Javascript, you can implement this in a child class and then call
	// the base method (so all the normal panel2d stuff gets exposed), plus call the various RegisterJS helpers yourself
	// to expose additional panel type specific data/methods.
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// Low level build matching styles for debugger use
	virtual bool BBuildMatchingStyleList( CUtlVector< CascadeStyleFileInfo_t > *pvecStyles ) OVERRIDE;

	// Getter for panel attributes
	virtual int GetAttribute( const char *pchAttrName, int nDefaultValue ) const OVERRIDE;
	virtual const char *GetAttribute( const char *pchAttrName, const char * pchDefaultValue ) const OVERRIDE;
	virtual uint32 GetAttribute( const char *pchAttrName, uint32 unDefaultValue ) const OVERRIDE;
	virtual uint64 GetAttribute( const char *pchAttrName, uint64 unDefaultValue ) const OVERRIDE;
	virtual float GetAttribute( const char *pchAttrName, float flDefaultValue ) const OVERRIDE;
	virtual int GetAttribute( CPanoramaSymbol symName, int nDefaultValue ) const OVERRIDE;
	virtual const char *GetAttribute( CPanoramaSymbol symName, const char * pchDefaultValue ) const OVERRIDE;
	virtual uint32 GetAttribute( CPanoramaSymbol symName, uint32 unDefaultValue ) const OVERRIDE;
	virtual uint64 GetAttribute( CPanoramaSymbol symName, uint64 unDefaultValue ) const OVERRIDE;
	virtual float GetAttribute( CPanoramaSymbol symName, float flDefaultValue ) const OVERRIDE;

	// Setter for panel attributes
	virtual void SetAttribute( const char *pchAttrName, int nValue ) OVERRIDE;
	virtual void SetAttribute( const char *pchAttrName, const char * pchValue ) OVERRIDE;
	virtual void SetAttribute( const char *pchAttrName, uint32 unValue ) OVERRIDE;
	virtual void SetAttribute( const char *pchAttrName, uint64 unValue ) OVERRIDE;
	virtual void SetAttribute( const char *pchAttrName, float flValue ) OVERRIDE;
	virtual void SetAttribute( CPanoramaSymbol symName, int nValue ) OVERRIDE;
	virtual void SetAttribute( CPanoramaSymbol symName, const char * pchValue ) OVERRIDE;
	virtual void SetAttribute( CPanoramaSymbol symName, uint32 unValue ) OVERRIDE;
	virtual void SetAttribute( CPanoramaSymbol symName, uint64 unValue ) OVERRIDE;
	virtual void SetAttribute( CPanoramaSymbol symName, float flValue ) OVERRIDE;

	// Remove a panel attribute
	virtual void RemoveAttribute( const char *pchAttrName ) OVERRIDE;
	virtual void RemoveAttribute( CPanoramaSymbol symName ) OVERRIDE;

	// Set animation property on panel
	virtual void SetAnimation( const char *pchAnimationName, float flDuration, float flDelay, EAnimationTimingFunction eTimingFunc, EAnimationDirection eDirection, EAnimationFillMode eFillMode, float flIterations ) OVERRIDE;

	// Force an immediate update of the visibility list on our window for our current visibility
	virtual void UpdateVisibility( bool bUseDirtyStyles ) OVERRIDE;

	// Base class implementation for valid XML properties to be set
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	// Builds a string of properties and values to display in the debugger
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	// Populates a vector with all immediate children matching a class
	virtual void FindChildrenWithClass( const char *pchClass, CUtlVector<IUIPanel *> &vecMatchingChildren ) OVERRIDE;
	// Populates a vector with all immediate children matching a class
	virtual void FindChildrenWithClass( CPanoramaSymbol symClass, CUtlVector<IUIPanel *> &vecMatchingChildren ) OVERRIDE;

	// Populates a vector with all children matching a class
	virtual void FindChildrenWithClassTraverse( const char *pchClass, CUtlVector<IUIPanel *> &vecMatchingChildren ) OVERRIDE; 
	// Populates a vector with all children matching a class
	virtual void FindChildrenWithClassTraverse( CPanoramaSymbol symClass, CUtlVector<IUIPanel *> &vecMatchingChildren ) OVERRIDE;

	// Play focus change sound accounting for fast scroll volume fade effects, etc
	virtual void PlayFocusChangeSound( int nRepeats, float flPan ) OVERRIDE;

	// Clear all panel events
	virtual void ClearPanelEvents() OVERRIDE;

	// Clear specific panel event
	virtual void ClearPanelEvents( CPanoramaSymbol symPanelEvent ) OVERRIDE;

	// panel events
	virtual void SetPanelEvent( CPanoramaSymbol symPanelEvent, IUIEvent *pEvent ) OVERRIDE;

	// Should analog stick be able to scroll this panel?
	virtual bool BEnableAnalogStickScrolling() OVERRIDE{ return m_bAnalogStickScrollEnable; }
	virtual void EnableAnalogStickScrolling( bool bEnable ) OVERRIDE { m_bAnalogStickScrollEnable = bEnable; }

	// Set mouse tracking state
	virtual void SetMouseTracking( bool bState ) OVERRIDE;

	// Should only be called in the very limited cases where we are creating scrollbars within a layout pass,
	// exposed for HTML but should really only be used inside UIPanel normally
	virtual void SetInScrollbarConstruction( bool bConstructing ) OVERRIDE { s_bInScrollBarConstruction = bConstructing; }

	// Get the scrollbar for this panel if it exists
	virtual IUIScrollBar *GetVerticalScrollBar() OVERRIDE { return m_pVerticalScrollBar; }
	virtual IUIScrollBar *GetHorizontalScrollBar() OVERRIDE{ return m_pHorizontalScrollBar; }

	// Get panel events set on panel
	virtual VecUIEvents_t * GetPanelEvents( CPanoramaSymbol symEvent ) OVERRIDE;

	// Has this panel ever been laid out
	virtual bool BHasBeenLayedOut() const OVERRIDE { return m_flActualXOffset != FLT_MAX && m_flActualYOffset != FLT_MAX; }

	// Stop the animation (in animation thread) of style property until a frame update comes in from layout thread and return
	// the actual final animation/interpolation time so we can match up to it on layout thread
	virtual float StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( uint32 hSymbol );

	// Panel paint performance
	virtual bool BHasCachedCommandList() const OVERRIDE { return m_pCachedCommandList.IsValid(); }
	virtual uint32 GetCommandListBytesSize() const { return m_unPaintCommandsBytesSize; }
	virtual float GetRepaintRate() const { return m_flRepaintRequiredRate; }

	// V8 context usage
	virtual bool BUsesGlobalContext() const { return m_bUseGlobalContext; }
	virtual void SetUseGlobalContext( bool bUseGlobalContext ) { m_bUseGlobalContext = bUseGlobalContext; }

	// Control of paint cmd list cache
	virtual bool BCachePaintCmdList() const { return m_bForceBuildPaintCmdCache; }
	virtual void SetCachePaintCmdList( bool bCachePaintCmdList ) { m_bForceBuildPaintCmdCache = bCachePaintCmdList; }

	virtual CJSKeyframesObject *JSCreateCopyOfCSSKeyframes( const char *pchKeyframesName );
	virtual void JSDeleteKeyframes( CJSKeyframesObject *pKeyframes );
	virtual void JSUpdateCurrentAnimationKeyframes( CJSKeyframesObject *pKeyframes ); 

	//
	// Methods that aren't part of the interface out to client/controls code, but are public within the framework code
	//

	// Access internal panel style
	CPanelStyle *AccessStyle() const;

	// Access internal panel style
	CPanelStyle *AccessStyleDirty() { return &m_style; }

	// Get parent lookup maps for this panel to help accelerate styles matching
	void GetParentLookupMaps(
		MapParentsByType_t **pMapParentsByType,
		MapParentsByID_t **pMapParentsByID,
		MapParentsByClass_t **pMapParentsByClass );

	// Callback we get from the LayoutManager when a layout file completes loading for us asynchronously, never call this
	// if you aren't the layout manager!
	void OnGetLayoutFileAsyncComplete( LayoutFilePtr_t pLayoutFile, ELoadLayoutAsyncDetails eDetails, bool bPartialLayout );

	void ClearLayoutTransitionFlagsBubble( uint32 unFlags );

	VecEventHandlers_t *GetMutableEventHandlers() { return &m_vecEventHandlers; }
	const VecEventHandlers_t &GetEventHandlers() const { return m_vecEventHandlers; }

	static bool BInLayoutLoad() { return s_bInApplyLayoutFile; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) OVERRIDE;
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif

	LayoutFilePtr_t GetCLayoutFile() { return m_pLayoutFile; }

	// Invalidates painting and tells the panel it must repaint next frame, tells all the children too.
	void SetRepaintRecursive( EPanelRepaint eRepaintNeeded );

	// composition layer hints to improve performance
	virtual void SetRequireCompositionLayer( bool bRequireCompositionLayer ) OVERRIDE { m_bRequireCompositionLayer = bRequireCompositionLayer; }
	virtual bool BRequireCompositionLayer() const OVERRIDE { return m_bRequireCompositionLayer; }
	virtual void SetAlwaysCacheCompositionLayer( bool bAlwaysCacheCompositionLayer ) OVERRIDE { m_bAlwaysCacheCompositionLayer = bAlwaysCacheCompositionLayer; }
	virtual bool BAlwaysCacheCompositionLayer() const OVERRIDE { return m_bAlwaysCacheCompositionLayer; }
	virtual void SetForceNoCompositionLayer( bool bForceNoCompositionLayer ) OVERRIDE { m_bForceNoCompositionLayer = bForceNoCompositionLayer; }
	virtual bool BForceNoCompositionLayer() const OVERRIDE { return m_bForceNoCompositionLayer; }

	virtual const char*GetCompositionLayerRenderTargetName() OVERRIDE;

	virtual void OnStyleTransitionsCleanup() OVERRIDE;

	// ready for display
	virtual void RegisterForReadyEvents( bool bEnable ) OVERRIDE;
	virtual bool BReadyForDisplay() OVERRIDE { return m_bReadyForDisplayState; }
	virtual void SetReadyForDisplay( bool bReady ) OVERRIDE;

	// Style properties from code
	virtual void SetTransform3DSimple( const CUtlVector<CTransform3D *> &vecTransforms ) OVERRIDE;
	virtual void SetOpacitySimple( float opacity ) OVERRIDE;

	// layout invalidate
	enum EPanelLayoutFlags : uint16
	{
		k_EPanelLayoutFlagsNone = 0,

		// general panel flags
		k_EPanelLayoutPositionDirty = 1 << 0,
		k_EPanelLayoutSizeDirty = 1 << 1,
		k_EPanelLayoutChildPositionDirty = 1 << 2,
		k_EPanelLayoutChildSizeDirty = 1 << 3,

		// active animations or transitions
		k_EPanelLayoutSizeTransitionActive = 1 << 4,
		k_EPanelLayoutChildSizeTransitionActive = 1 << 5,
		k_EPanelLayoutPositionTransitionActive = 1 << 6,
		k_EPanelLayoutChildPositionTransitionActive = 1 << 7,

		// styles
		k_EPanelLayoutStylesDirty = 1 << 8,
		k_EPanelLayoutChildStylesDirty = 1 << 9,
		k_EPanelLayoutAllChildrenStylesDirty = 1 << 10,

		// skip flowing layout transition next pass (style just added "transition-property: position"... should not apply to next transition)
		k_EPanelLayoutSkipLayoutPositionTransition = 1 << 11,
	};

private:


	// Add classes to panel
	void AddClassesInternal( CPanoramaSymbol *pSymbols, uint cSymbols, bool bDelayStyleUpdate );

	// Paint debugger margin inspection for this child
	void PaintChildMarginInspection( IUIPanel *pChild );

	// Paint panel insepection
	void PaintPanelInspection();

	// Paint any background images for panel
	void PaintBackgroundImages();

	// Paint panel background
	void PaintBackground();

	// Notify this panel and all descendants that we skipped painting it
	void NotifyStoppedPaintingTraverse();

	// Apply panel description to panel
	bool BApplyPanelDescription( CPanoramaSymbol symLayoutFile, PanelDescription_t *pPanelDescription, CUtlVector< PanelEventsToParse_t > *pvecEventsToParse, CUtlVector< IUIPanel * > *pvecExistingPanels );
	bool BCreateChildrenFromDescription( CPanoramaSymbol symLayoutFile, PanelDescription_t *pPanelDescription, CUtlVector< PanelEventsToParse_t > *pvecEventsToParse, CUtlVector< IUIPanel * > *pvecExistingPanels );

	// panel events
	void SetPanelEvent( CPanoramaSymbol symPanelEvent, VecUIEvents_t *pvecEvents );
	void SetPanelEventJS( const v8::FunctionCallbackInfo<v8::Value>& args );
	void SetPanelEventInternal( CPanoramaSymbol symPanelEvent, VecUIEvents_t *pvecEvents, v8::Persistent<v8::Script> *pScript, v8::Persistent<v8::Function> *pJSFunc, IUIPanel *pJSContext );
	void ClearPanelEventJS( CPanoramaSymbol symPanelEvent );

	void ClearParentLookupMapsTraverse();
	void AddClassToChildLookupMaps( CPanoramaSymbol symClass, IUIPanel *pParent );
	void RemoveClassFromChildLookupMaps( CPanoramaSymbol symClass, IUIPanel *pParent );

	void SetLayoutFlagsOnParents( uint32 unPanelLayoutFlags );
	void SetChildLayoutFlags( uint32 unPanelLayoutFlags );
	void ClearLayoutPositionFlags();
	void ClearLayoutSizeFlags();
	void OnChildLayoutTransitionFlagsCleared( uint32 unFlags );
	void OnStylesChangedInternal();
	void VerifyWidthAndHeight();

	// These should be used very carefully and are normally managed internally.  You can't move
	// a panel to a new top level window, you can just temporarily remove and re-add it.
	void AddToTopLevelWindow();
	void RemoveFromTopLevelWindow();

	// Check if we have styling that should make us assume we need a clip layer for this panel
	bool BRequiresContentClipLayer();

	// Find first child matching ID loaded from within the same layout file as this
	IUIPanel *FindChildInLayoutFileTraverse( const char *pchID );

	void AddChildFlagsHelper( CUIPanel *pChild );
	void AddFlagToParents( EStyleFlags eStyleFlag );
	void RemoveFlagFromParents( EStyleFlags eStyleFlag );

	void DeletePanelsForReloadTraverse( CPanoramaSymbol symPath, CUtlVector< IUIPanel * > *pvecPanelsWithID );

	// callback when layout file loading completes
	void OnLoadLayoutAsyncCompleteInternal( LayoutFilePtr_t pLayoutFile, ELoadLayoutAsyncDetails eDetails, bool bPartialLayout );

	void SetLayoutFileTraverse( CPanoramaSymbol symLayoutFile );

	void AdjustPositionForAlignment( float *x, float *y, float flOurWidth, float flOurHeight, float flTotalWidth, float flTotalHeight );

	bool SetFocusInternal( bool bDueToHover, bool bChangeContextIfNeeded );

	void AddDisabledFlagToChildren();
	void RemoveDisabledFlagFromChildren();

	// for setting parent disabled style flag
	void AddParentDisabledFlag();
	void RemoveParentDisabledFlag();

	// Find first child accepting input
	IUIPanel *FindFirstChildAcceptingFocusTraverse();

	// Access render engine object
	CUIRenderEngine *AccessRenderEngine();

	// Check if we, or any of our children, accept input recursively
	bool BSelfOrChildrenAcceptFocus() const;

	bool BIsDelayedProperty( CPanoramaSymbol symProperty ) const;

	// Event handlers
	bool EventLoadLayoutFileAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile, bool bPartialLayout );
	bool EventLoadLayoutFromXMLStringAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile, bool bPartialLayout );
	bool EventLoadLayoutFromBase64XMLStringAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile, bool bPartialLayout );
	bool EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel );

	bool BHasNon2DTransforms();

	void DispatchReadyForDisplayTraverse( bool bReady );
	void IncrementAncestorReadyForDisplay( int nDelta, bool *pAncestoryReadyState = nullptr );
	bool EventReadyForDisplay( const CPanelPtr< IUIPanel > &pPanel );
	bool EventUnreadyForDisplay( const CPanelPtr< IUIPanel > &pPanel );

	void LayoutChildrenInHiding( float flFinalWidth, float flFinalHeight );
	void PaintChildrenInHiding();

	struct ScrollBarData_t;
	void UpdateScrollOffsetX();
	void UpdateScrollOffsetY();
	void UpdateScrollOffset( ScrollBarData_t *pScrollData );
	float GetInterpolatedScrollOffset( ScrollBarData_t *pScrollData );
	float StopScroll( ScrollBarData_t *pScrollData );
	ScrollBarData_t &EnsureScrollData( ScrollBarData_t *&pScrollData );
	static void GetContextScrollTransitionControlPoints( ScrollBarData_t *pScrollData, Vector2D( &vecPoints )[ 4 ] );

	typedef IUIScrollBar* (panorama::IUIPanelClient::*ScrollBarCreate_t)(float);
	void CreateOrUpdateScrollBarForLayout( IUIScrollBar **ppScrollBar, ScrollBarData_t *&pScrollData, float flContentLength, float flActualLayoutLength, EOverflowValue eOverflow, ScrollBarCreate_t pfnCreate, CPanoramaSymbol symScrollMax );
	void GetPaintArea( PanoramaRect_t *pPaintArea );
	void GetChildAreaToPaint( PanoramaRect_t *pChildRegionToPaint, const PanoramaRect_t &rectPaintArea, CUIPanel *pChild );
	bool BShouldDrawChild( PanoramaRect_t *pChildRegionToPaint, IUIPanel *pIChild, const PanoramaRect_t &rectPaintArea );

	bool BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags ) const;
	bool BHasAnyDescendantSelectorMatchingStyleFlagRecursive( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedLayoutFiles, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const;
	bool BHasAnyDescendantSelectorMatchingClasses( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses ) const;
	bool BHasAnyDescendantSelectorMatchingClassesRecursive( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedLayoutFiles, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const;

	void SetTopLevelWindow( CTopLevelWindow *pWindow );

	// Client panel ptr
	panorama::IUIPanelClient *m_pClientPtr;

	// Panel id
	CUtlString m_strID;

	// Parent
	panorama::IUIPanel *m_pParent;

	// Top level window the panel belongs to
	CTopLevelWindow *m_pWindow;

	// Vector to our children
	CUtlVector< IUIPanel * > m_vecChildren;

	// Vector to any special children we've hidden from the normal iteration list
	CUtlVector< IUIPanel * > *m_pVecChildrenInHiding;

	// determines whether we need a full repaint of all our children, or can
	// paint using a cached composition layer.
	panorama::EPanelRepaint m_eRepaint;

	PanoramaRect_t m_rectLastPaint;

	// Determine whether or not we can avoid traversing all of our children when
	// painting and instead just paint a cached version of the command list
	bool m_bForceBuildPaintCmdCache = false;	// Build paint cmd cache for this panel. Overrides heuristic in PaintTraverse
	float m_flRepaintRequiredRate;
	uint32 m_unPaintCommandsBytesSize;
	CSmartPtr< CRenderCommandList > m_pCachedCommandList;

	// Styles for the panel
	mutable CPanelStyle m_style;

	// Current index to the panel in the UISystems visible or invisible panel list
	int m_iUIPanelIndex;

	// Is the panel currently visible
	bool m_bVisible : 1;
	bool m_bApplyingStyles : 1;				// protect against reentrant calls
	bool m_bNeedOnStylesChanged : 1;

	// Are we currently deleting children, which would indicate children who destruct shouldn't notify us to remove them,
	// as we already know, and notifying causes n^2 behavior.
	bool m_bDeletingChildren : 1;

	bool m_bInvalidateSizeAndPositionOnOpacityChangeDisabled : 1;

	// Should we act as a bumper/boundary for repeats on selection movement in one/both directions?
	bool m_bSelectionPosHorBoundary : 1;
	bool m_bSelectionPosVerBoundary : 1;

	bool m_bLoadedLayoutFile : 1;					// if we loaded our own layout file
	bool m_bLayoutIncludesScripts : 1;				// if loaded own layout file, track if there was any JS included
	bool m_bAnalogStickScrollEnable : 1;
	bool m_bMouseTracking : 1;						// true if we have registered with the input system for mouse tracking
	bool m_bFocusOnHover : 1;						// panel will automatically request focus when they are hovered
	bool m_bChildFocusOnHover : 1;					// children of the panel will automatically request focus when they are hovered
	bool m_bLoaded : 1;								// loaded event has been fired
	bool m_bScrolledIntoView : 1;					// panel has been scrolled into view
	bool m_bSendChildScrolledIntoViewEvents : 1;	// if set, child panels will be sent ScrolledIntoView and ScrolledOutOfView events
	bool m_bScrollParentToFitWhenFocused : 1;		// if set, parent will be scrolled so this panel is in view when this panel is clicked
	bool m_bDraggable : 1;							// true if panel can be dragged
	bool m_bKeepScrollToBottomOnResize : 1;			// if a scroll bar is scroll to bottom and the panel size changes, restore the scrolled to bottom state
	bool m_bRememberChildFocus : 1;					// if true, default focus will track last focused child
	bool m_bNeedsIntermediateTexture : 1;			// if true, the renderer needs to allocate an intermediate texture for the render pipeline
	bool m_bClipAfterTransform : 1;					// if true, indicates that this panel should do clipping after applying the transform rather than before.
													// This prevents creation of a composition layer, but might lead to incorrect clipping.
	bool m_bHiddenChild : 1;						// Created with ePanelFlags_DontAddAsChild
	bool m_bNeedsPaintArea : 1;						// Sets whether to call Paint() or PaintArea() on the client panel
	bool m_bStoppedPaintingNotified : 1;			// Whether or not we've notified our client panel that they skipped painting

	bool m_bRequireCompositionLayer : 1;			// Content creator wants this panel to always use a composition layer
	bool m_bAlwaysCacheCompositionLayer : 1;		// Content hint that this panel's composition layer is important to cache
	bool m_bForceNoCompositionLayer : 1;			// Content creator wants this panel to never use a composition layer
	bool m_bOffscreenCompositionLayer : 1;			// Content creator wants this panel to use a composition layer however
													// this composition layer will not be drawn to the backbuffer (or other
													// render target) using Panorama. Content creator can then query for the
													// render target name and use it for some other rendering (eg model ...)

	bool m_bRegisteredForReadyEvents : 1;			// if true, dispatch ready for display events to this panel when state changes
	bool m_bReadyForDisplayState : 1;				// our current ready for display state (and what we also last passed to children)	
	bool m_bReadyForDisplaySetOnPanel : 1;			// Someone called SetReadyForDisplay() on this panel
	int m_cReadyForDisplayChildren;					// number of children who are interested in ready for display events

	EFractionalPixelPositions m_eFractionalPixelPositions;	// Giving content creator control whether to clamp or not to pixel boundaries 
	
	EMouseCursors m_ePanelMouseCursor;
	
	EMouseCanActivate m_eMouseCanActivate;	// determines if clicking on a panel should also raise the activate event

	// What is the id of the panel we should give default focus to, should be in our child 
	// hieracy somewhere
	CUtlString m_strDefaultFocus;
	
	// Pointer to the last focused child for cases where we're tracking that
	CPanelPtr< IUIPanel > m_pLastFocusedChild;

	// Tab index for the panel, which applies only within it's parent context
	float m_flTabIndex;

	// Selection position (for arrow/gamepad navigation) for the panel, which applies only within it's parent context
	float m_flSelectionPosX;
	float m_flSelectionPosY;

	// all style classes set for this panel. Ordered from oldest to newest
	CUtlVector< CPanoramaSymbol > m_vecStyleClasses;

	MapParentsByType_t *m_pMapParentsByType;
	MapParentsByID_t *m_pMapParentsByID;
	MapParentsByClass_t *m_pMapParentsByClass;

	// layout related
	LayoutFilePtr_t m_pLayoutFile;			// layout file where we get styles, etc.
	LayoutFilePtr_t m_pLayoutLoadedFrom;	// the layout file the panel was created by. Could differ from m_pLayoutFile if the panel loads its own layout file.

	// Used when finding panels by ID, as IDs are only unique within a layout file

	// Are we in the apply layout file call
	static bool s_bInApplyLayoutFile;

	// set by measure pass
	float m_flContentWidth;
	float m_flContentHeight;
	float m_flDesiredLayoutWidth;	// width we desire after applying styles to ourself which may restrict us smaller than our contents
	float m_flDesiredLayoutHeight;	// height we desire after applying styles to ourself which may restrict us smaller than our contents

	// set by layout pass
	float m_flActualXOffset;		// padding-left of parent + left-border of parent + our margin-left (does not include flowing layout; position style is set for that)
	float m_flActualYOffset;		// padding-top of parent + left-border of parent + our margin-top (does not include flowing layout; position style is set for that)
	float m_flLastAbsoluteXOffset;	// last absolute position that adjusting for alignment computed for us with parents offset added in
	float m_flLastAbsoluteYOffset;	// last absolute position that adjusting for alignment computed for us with parents offset added in

	// set by layout pass
	float m_flActualLayoutWidth;			// width to draw
	float m_flActualLayoutHeight;			// height to draw

	float m_flLastDesiredWidthFromParent;		// last width passed to our DesiredLayoutSizeTraverse
	float m_flLastDesiredHeightFromParent;		// last height passed to our DesiredLayoutSizeTraverse
	float m_flLastLayoutXFromParent;			// last x value passed to our LayoutTraverse
	float m_flLastLayoutYFromParent;			// last y value passed to our LayoutTraverse
	float m_flLastLayoutWidthFromParent;		// last width passed to our LayoutTraverse
	float m_flLastLayoutHeightFromParent;		// last height passed to our LayoutTraverse

	Vector m_vActualUIScale;		// Calculated UI scale

	// Scroll related layout values
	struct ScrollBarData_t
	{
		float m_flInitialPos;
		float m_flOffset;
		float m_flOffsetTarget;
		float m_flOverscroll;	// The number of additional pixels on either side to add when scrolling.
		double m_flTransitionStart;
		bool m_bDispatchedScrollMax;
		EAnimationTimingFunction m_eTimingFunction;
		double m_flTransitionDuration;
		float m_flControlPoints[4];
	};
	ScrollBarData_t *m_pHorizontalScrollData;
	ScrollBarData_t *m_pVerticalScrollData;
	IUIScrollBar *m_pVerticalScrollBar;
	IUIScrollBar *m_pHorizontalScrollBar;


	// style flags set on this panel
	short m_unStyleFlags;

	// style flags that are not allowed to be set on this panel
	short m_unDisallowedStyleFlags;

	// flags to track which styles that are used in animation/transform context are set, for fast
	// checking on paint
	uint32 m_unStylesPresentFlags;

	short m_unPanelLayoutFlags;

	enum EInputFlags : uint8
	{
		k_EInputAccept = 1 << 0,
		k_EInputTopOfContext = 1 << 1,
		k_EInputAcceptFocus = 1 << 2,
		k_EInputPerformHitTest = 1 << 3,
		k_EDisableFocusOnMouseDown = 1 << 4,
		k_EInputPerformHitTestChildren = 1 << 5,
		k_EInputAlwaysConsumeHoverClicks = 1 << 6,
		k_ECanClearFocusByClicking = 1 << 7,
	};

	// do we want to accept input, other input related flags
	uint8 m_unInputFlags;

	// namespace for input action events this panel is in
	CPanoramaSymbol m_symInputNamespace;

	// Name of a force composition layer texture
	CPanoramaSymbol m_symCompositionLayerTextureName;

	static CUtlVector< CascadeStyleFileInfo_t > s_vecApplyStylesTemp;

	// map of panel events set on this panel
	CUtlMap< CPanoramaSymbol, PanelEvent_t, short, CDefLess< CPanoramaSymbol > > *m_pmapPanelEvents;

	// We could theoretically make this be a list of layout events if we needed to do more.
	IUIEvent *m_pOnLayoutEvent;

	VecEventHandlers_t m_vecEventHandlers;	// event handlers registered on this panel

	CUtlMap< CPanoramaSymbol, CUtlString, int, CDefLess< CPanoramaSymbol > > *m_pMapProperties;

	static bool s_bInScrollBarConstruction;

	// override JS context
	panorama::IUIPanel *m_pJSContext;

	// if set, overrides layout file symbol for JS checks. Used when layout file was loaded from string (which have code:// paths)
	CUtlString m_strLayoutFilePathForJSCheck;

	// if set, this is the snippet loaded on the panel
	CUtlString m_strLayoutSnippet;

	// Run this panel's scripts inside the global context, instead of creating a new context
	bool m_bUseGlobalContext = false;
};

} // namespace panorama

#endif // IUIPANEL_H

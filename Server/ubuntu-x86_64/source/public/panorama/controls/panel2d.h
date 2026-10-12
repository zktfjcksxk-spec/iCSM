//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANEL2D_H
#define PANEL2D_H

#if defined( _WIN32 ) || defined( SOURCE2_PANORAMA )
#pragma once
#endif

#include "../iuiengine.h"
#include "../iuipanel.h"
#include "../iuipanelclient.h"
#include "../iuipanelstyle.h"
#include "panorama/transformations.h"
#include "panorama/input/keycodes.h"
#include "panorama/layout/panel2dfactory.h"
#include "panorama/layout/stylefiletypes.h"
#include "panorama/input/mousecursors.h"
#include "panorama/input/iuiinput.h"
#include "panorama/uieventcodes.h"
#include "panorama/uievent.h"
#include "panorama/iuiwindow.h"
#include "panorama/layout/panel2dfactory.h"
#include "color.h"
#include "utlvector.h"
#include "utlstring.h"
#include "panelptr.h"
#include "steam/steamtypes.h"
#if defined( SOURCE2_PANORAMA )
#include "currencyamount.h"
#else
#include "rtime.h"
#include "amount.h"
#endif
#include "tier1/utlmap.h"
#include "panorama/layout/stylesymbol.h"
#include <functional>

namespace panorama
{

#pragma warning(push)
// warning C4251: 'CPanel2D::symbol' : class 'CUtlSymbol' needs to have dll-interface to be used by clients of class 'CPanel2D'
#pragma warning( disable : 4251 ) 

class CLayoutFile;
class CTopLevelWindow;
struct PanelDescription_t;
class CUIRenderEngine;
class CImageResourceManager;
class CPanelStyle;
class CBackgroundImageLayer;
class CPanel2D;
class CScrollBar;
class CJSKeyframesObject;

inline CPanel2D * ToPanel2D( IUIPanel *pPanel )
{
	if( pPanel )
		return (CPanel2D*)(pPanel->ClientPtr());

	return NULL;
}

void JSCreatePanelWithCurrentContext( const v8::FunctionCallbackInfo<v8::Value>& args );
IUIPanel *GetPanelFromJSArgs( const v8::Local< v8::Value > &arg );
	
//-----------------------------------------------------------------------------
// Purpose: Struct used to perform hit tests
//-----------------------------------------------------------------------------
struct TransformContext_t
{
	float m_flPosX;
	float m_flPosY;
	float m_flPosZ;
	VMatrix m_TransformMatrix;
	float m_flWidth;
	float m_flHeight;

	float m_flPerspective;
	float m_flPerspectiveOriginX;
	float m_flPerspectiveOriginY;
};


//-----------------------------------------------------------------------------
// Purpose: Basic 2D UI panel.  These may be transformed in 3D space, but they are 
// at a base level 2D rectangular containers for other panels/paint operations.
//-----------------------------------------------------------------------------
class CPanel2D : public panorama::IUIPanelClient
{
	DECLARE_PANEL2D_NO_BASE( CPanel2D );

public:
	CPanel2D( IUIWindow *parent, const char * pchID );
	CPanel2D( CPanel2D *parent, const char * pchID );
	CPanel2D( CPanel2D *parent, const char *pchID, uint32 ePanelFlags );

	virtual ~CPanel2D();

	template <typename T> T* downcast();
	template <typename T> const T* downcast() const;

	// Check if the panel has loaded layout
	bool IsLoaded() const { return m_pIUIPanel->IsLoaded(); }

	virtual void OnDeletePanel() OVERRIDE { delete this; }

	// Access the panorama side UI panel interface for the client panel
	virtual IUIPanel *UIPanel() const OVERRIDE { return m_pIUIPanel; }

	void DeleteAsync( float flDelay = 0.0f );

	// Set the panel visible
	void SetVisible( bool bVisible ) { m_pIUIPanel->SetVisible( bVisible ); }
	bool BIsVisible() const { return m_pIUIPanel->BIsVisible(); }

	// Override paint cmd cache heuristic
	void SetForceBuildPaintCmdCache( bool bForce) { m_pIUIPanel->SetCachePaintCmdList( bForce ); }

	// Get the base position for the panel
	void GetPosition( CUILength &x, CUILength &y, CUILength &z, bool bIncludeUIScaleFactor = true );

	// Set the base position for the panel
	void SetPosition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor = false );
	void SetPositionWithoutTransition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor = false );
	void SetTransform3D( const CUtlVector<CTransform3D *> &vecTransforms );
	void SetTransform3DSimple( const CUtlVector<CTransform3D *> &vecTransforms ) { m_pIUIPanel->SetTransform3DSimple( vecTransforms ); }
	void SetOpacity( float flOpacity );
	void SetOpacitySimple( float flOpacity ) { m_pIUIPanel->SetOpacitySimple( flOpacity ); }
	bool BIsTransparent() { return m_pIUIPanel->BIsTransparent();  }
	void SetPreTransformScale2D( float flX, float flY );

	// Set the base width/height for the panel
	CUILength GetStyleWidth();
	CUILength GetStyleHeight();
	void SetSize( CUILength width, CUILength height );

	// The the bounds that should be used for placing a tooltip, returning false means "use the entire panel"
	virtual bool GetContextUIBounds( float *pflX, float *pflY, float *pflWidth, float *pflHeight ) { return false; }

	// Set the animation style for the panel
	void SetAnimation( const char *pchAnimationName, float flDuration, float flDelay, EAnimationTimingFunction eTimingFunc, EAnimationDirection eDirection, EAnimationFillMode eFillMode, float flIterations ) { m_pIUIPanel->SetAnimation( pchAnimationName, flDuration, flDelay, eTimingFunc, eDirection, eFillMode, flIterations ); }

	// Returns the layout file for this panel
	CPanoramaSymbol GetLayoutFile() const { return m_pIUIPanel->GetLayoutFile(); }

	// Returns define from layout file for this panel
	char const * GetLayoutFileDefine( char const *szDefineName );
	int GetLayoutFileDefineInt( const char *szDefineName, int defaultValue );
	float GetLayoutFileDefineFloat( const char *szDefineName, float defaultValue );

	// Style access
	panorama::IUIPanelStyle *AccessStyle() const { return m_pIUIPanel->AccessIUIStyle(); }

	// Style access
	panorama::IUIPanelStyle *AccessStyleDirty() const { return m_pIUIPanel->AccessIUIStyleDirty(); }

	void ApplyStyles( bool bTraverse ) { return m_pIUIPanel->ApplyStyles( bTraverse ); }

	// Mark styles dirty for the panel
	void MarkStylesDirty( bool bIncludeChildren ) { m_pIUIPanel->MarkStylesDirty( bIncludeChildren ); }

	void ClearPropertyFromCode( panorama::CStyleSymbol symProperty );

	// Virtual called on scale factor for panel changing
	virtual void OnUIScaleFactorChanged( const Vector &vOldScaleFactor, const Vector &vNewScaleFactor ) OVERRIDE { }

	// Paint the panel and it's children, called by the rendering layer when it's time to paint.
	void PaintTraverse() { m_pIUIPanel->PaintTraverse( NULL, false ); }

	// Paint the panel's contents
	virtual void Paint() OVERRIDE;

	// Paint the panel's contents in the given area
	virtual void PaintArea( const PanoramaRect_t &rectPaintArea ) OVERRIDE;

	// Called on the first frame that this panel skipped painting because it was not visible
	virtual void StoppedPainting() OVERRIDE;

	// Invalidates painting and tells the panel it must repaint next frame
	void SetRepaint( EPanelRepaint eRepaintNeeded );

	// sets & loads the layout file for this panel
	bool BLoadLayout( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) { return m_pIUIPanel->BLoadLayout( pchFile, bOverrideExisting, bPartialLayout ); }

	// Considers a layout load failure a fatal error.
	void RequireLoadLayout( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) { m_pIUIPanel->RequireLoadLayout( pchFile, bOverrideExisting, bPartialLayout ); }

	// sets & loads the layout for this panel
	bool BLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting = false, bool bPartialLayout = false ) { return m_pIUIPanel->BLoadLayoutFromString( pchXMLString, bOverrideExisting, bPartialLayout ); }

	// Considers a layout load failure a fatal error.
	void RequireLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting = false, bool bPartialLayout = false ) { m_pIUIPanel->RequireLoadLayoutFromString( pchXMLString, bOverrideExisting, bPartialLayout ); }

	// Loads a snippet from the panel's current layout file
	bool BLoadLayoutSnippet( const char *pchSnippetName ) { return m_pIUIPanel->BLoadLayoutSnippet( pchSnippetName ); }

	// Loads a snippet and considers failure a fatal error
	void RequireLoadLayoutSnippet( const char *pchSnippetName ) { m_pIUIPanel->RequireLoadLayoutSnippet( pchSnippetName ); }

	// Returns true if a snippet is available by the given name
	bool BHasLayoutSnippet( const char *pchSnippetName ) { return m_pIUIPanel->BHasLayoutSnippet( pchSnippetName ); }

	// sets loads the layout file for this panel, asynchronously supporting remote http:// paths
	void LoadLayoutAsync( const char *pchFile, bool bOverrideExisting = false, bool bPartialLayout = false ) { return m_pIUIPanel->LoadLayoutAsync( pchFile, bOverrideExisting, bPartialLayout ); }

	// loads the layout file for this panel, asynchronously supporting remote http:// paths in css within
	void LoadLayoutFromStringAsync( const char *pchXMLString, bool bOverrideExisting, bool bPartialLayout = false ) { return m_pIUIPanel->LoadLayoutFromStringAsync( pchXMLString, bOverrideExisting, bPartialLayout );  }

	// creates & appends child panels from a string. String XML should only include XML of children, not this panel as a wrapper
	bool BCreateChildren( const char *pchXMLSring ) { return m_pIUIPanel->BCreateChildren( pchXMLSring ); }

	// removes all children and unloads the layout
	void UnloadLayout( void ) { m_pIUIPanel->UnloadLayout(); }

	// Measure self and children. First pass of layout
	void DesiredLayoutSizeTraverse( float flMaxWidth, float flMaxHeight ) { m_pIUIPanel->DesiredLayoutSizeTraverse( flMaxWidth, flMaxHeight ); }
	void DesiredLayoutSizeTraverse( float *pflDesiredWidth, float *pflDesiredHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) { m_pIUIPanel->DesiredLayoutSizeTraverse( pflDesiredWidth, pflDesiredHeight, flMaxWidth, flMaxHeight, bFinalDimensions );  }

	// Arrange children. Second pass of layout
	void LayoutTraverse( float x, float y, float flFinalWidth, float flFinalHeight ) { m_pIUIPanel->LayoutTraverse( x, y, flFinalWidth, flFinalHeight ); }

	// methods to invalid certain parts of layout
	void InvalidateSizeAndPosition() { m_pIUIPanel->InvalidateSizeAndPosition(); }
	void InvalidatePosition() { m_pIUIPanel->InvalidatePosition(); }
	bool IsSizeValid() { return  m_pIUIPanel->IsSizeValid(); }
	bool IsPositionValid() { return m_pIUIPanel->IsPositionValid(); }
	bool IsChildSizeValid() { return m_pIUIPanel->IsChildSizeValid(); }
	bool IsChildPositionValid() { return m_pIUIPanel->IsChildPositionValid(); }
	bool IsSizeTransitioning() { return m_pIUIPanel->IsSizeTransitioning(); }
	bool IsPositionTransitioning() { return m_pIUIPanel->IsPositionTransitioning(); }
	bool IsChildPositionTransitioning() { return m_pIUIPanel->IsChildPositionTransitioning(); }
	bool IsChildSizeTransitioning() { return m_pIUIPanel->IsChildSizeTransitioning(); }

	// size getters
	float GetDesiredLayoutWidth() const { return m_pIUIPanel->GetDesiredLayoutWidth(); }
	float GetDesiredLayoutHeight() const { return m_pIUIPanel->GetDesiredLayoutHeight(); }

	// Content size is what our contents actually take up, not accounting for fixed/relative
	// size set on us in styles which affect desired layout size
	float GetContentWidth() const { return m_pIUIPanel->GetContentWidth(); }
	float GetContentHeight() const { return m_pIUIPanel->GetContentHeight(); }

	// Actual size is the size given to the panel after layout, hopefully as big as its desired size.  
	// Actual size does NOT include margins (which are really in the parent).
	float GetActualLayoutWidth() const { return m_pIUIPanel->GetActualLayoutWidth(); }
	float GetActualLayoutHeight() const { return m_pIUIPanel->GetActualLayoutHeight(); }

	// Render size is the size of the content for rendering, this is either the actual layout size, or if
	// that is smaller than the content size + padding then it's the content size + padding.
	float GetActualRenderWidth() { return m_pIUIPanel->GetActualRenderWidth(); }
	float GetActualRenderHeight() { return m_pIUIPanel->GetActualRenderHeight(); }

	// Offset will include position, alignment, and margin adjustments
	float GetActualXOffset() const { return m_pIUIPanel->GetActualXOffset(); }
	float GetActualYOffset() const { return m_pIUIPanel->GetActualYOffset(); }

	// The calculated UI scale of this panel.
	Vector GetActualUIScale() const { return m_pIUIPanel->GetActualUIScale(); }
	float GetActualUIScaleX() const { return m_pIUIPanel->GetActualUIScaleX(); }
	float GetActualUIScaleY() const { return m_pIUIPanel->GetActualUIScaleY(); }
	float GetActualUIScaleZ() const { return m_pIUIPanel->GetActualUIScaleZ(); }

	// Offset to apply to contents for scrolling
	float GetContentsYScrollOffset() const { return m_pIUIPanel->GetContentsYScrollOffset(); }
	float GetContentsXScrollOffset() const { return m_pIUIPanel->GetContentsXScrollOffset(); }
	float GetContentsYScrollOffsetTarget() const { return m_pIUIPanel->GetContentsYScrollOffsetTarget(); }
	float GetContentsXScrollOffsetTarget() const { return m_pIUIPanel->GetContentsXScrollOffsetTarget(); }
	double GetContentsXScrollTransitionStart() const { return m_pIUIPanel->GetContentsXScrollTransitionStart(); }
	double GetContentsYScrollTransitionStart() const { return m_pIUIPanel->GetContentsYScrollTransitionStart(); }
	double GetContentsXScrollTransitionTime() const { return m_pIUIPanel->GetContentsXScrollTransitionTime(); }
	double GetContentsYScrollTransitionTime() const { return m_pIUIPanel->GetContentsYScrollTransitionTime(); }
	EAnimationTimingFunction GetContentsXScrollTransitionTimingFunction() const { return m_pIUIPanel->GetContentsXScrollTransitionTimingFunction(); }
	EAnimationTimingFunction GetContentsYScrollTransitionTimingFunction() const { return m_pIUIPanel->GetContentsYScrollTransitionTimingFunction(); }
	float GetInterpolatedXScrollOffset() { return m_pIUIPanel->GetInterpolatedXScrollOffset(); }
	float GetInterpolatedYScrollOffset() { return m_pIUIPanel->GetInterpolatedYScrollOffset(); }

	// Can the panel scroll further?
	bool BCanScrollUp() { return m_pIUIPanel->BCanScrollUp();  }
	bool BCanScrollDown() { return m_pIUIPanel->BCanScrollDown();  }
	bool BCanScrollLeft() { return m_pIUIPanel->BCanScrollLeft(); }
	bool BCanScrollRight() { return m_pIUIPanel->BCanScrollRight(); }

	// style class management
	void AddClass( const char *pchName ) { m_pIUIPanel->AddClass( pchName ); }
	void AddClass( CPanoramaSymbol symbol ) { m_pIUIPanel->AddClass( symbol ); }
	void AddClasses( const char *pchName ) { m_pIUIPanel->AddClasses( pchName ); }
	void AddClasses( CPanoramaSymbol *pSymbols, uint cSymbols ) { m_pIUIPanel->AddClasses( pSymbols, cSymbols ); }
	void RemoveClass( const char *pchName ) { m_pIUIPanel->RemoveClass( pchName ); }
	void RemoveClass( CPanoramaSymbol symName ) { m_pIUIPanel->RemoveClass( symName ); }
	void RemoveClasses( const CPanoramaSymbol *pSymbols, uint cSymbols ) { m_pIUIPanel->RemoveClasses( pSymbols, cSymbols ); }
	void RemoveClasses( const char *pchName ) { m_pIUIPanel->RemoveClasses( pchName ); }
	void RemoveAllClasses() { m_pIUIPanel->RemoveAllClasses(); }

	// bugbug jmccaskey - dangerous cross dll interface signature?  Is CUtlVector the same in debug/release?
	const CUtlVector< CPanoramaSymbol > &GetClasses() const { return m_pIUIPanel->GetClasses(); }

	bool BHasClass( const char *pchName ) const { return m_pIUIPanel->BHasClass( pchName ); }
	bool BHasClass( CPanoramaSymbol symName ) const { return m_pIUIPanel->BHasClass( symName ); }
	bool BAscendantHasClass( const char *pchName ) const { return m_pIUIPanel->BAscendantHasClass( pchName ); }
	bool BAscendantHasClass( CPanoramaSymbol symName ) const { return m_pIUIPanel->BAscendantHasClass( symName ); }
	void ToggleClass( const char *pchName ) { m_pIUIPanel->ToggleClass( pchName ); }
	void ToggleClass( CPanoramaSymbol symName ) { m_pIUIPanel->ToggleClass( symName ); }
	void SetHasClass( const char *pchName, bool bHasClass ) { m_pIUIPanel->SetHasClass( pchName, bHasClass ); }
	void SetHasClass( CPanoramaSymbol symName, bool bHasClass ) { m_pIUIPanel->SetHasClass( symName, bHasClass ); }
	void SwitchClass( const char *pchAttribute, const char *pchName ) { m_pIUIPanel->SwitchClass( pchAttribute, pchName ); }
	void SwitchClass( const char *pchAttribute, CPanoramaSymbol symName ) { m_pIUIPanel->SwitchClass( pchAttribute, symName ); }
	void SwitchClass( CPanoramaSymbol symAttribute, const char *pchName ) { m_pIUIPanel->SwitchClass( symAttribute, pchName ); }
	void SwitchClass( CPanoramaSymbol symAttribute, CPanoramaSymbol symName ) { m_pIUIPanel->SwitchClass( symAttribute, symName ); }
	void TriggerClass( const char *pchName ) { m_pIUIPanel->TriggerClass( pchName ); }
	void TriggerClass( CPanoramaSymbol symName ) { m_pIUIPanel->TriggerClass( symName ); }

	const char *GetID() const { return m_pIUIPanel->GetID(); }
	bool BHasID() const { return m_pIUIPanel->GetID()[0] != '0'; }

	void SetTabIndex( float flTabIndex ) { m_pIUIPanel->SetTabIndex( flTabIndex ); }
	void SetSelectionPosition( float flXPos, float flYPos ) { m_pIUIPanel->SetSelectionPosition( flXPos, flYPos ); }
	void SetSelectionPositionX( float flXPos ) { m_pIUIPanel->SetSelectionPositionX( flXPos ); }
	void SetSelectionPositionY( float flYPos ) { m_pIUIPanel->SetSelectionPositionY( flYPos ); }

	float GetSelectionPositionX() const { return m_pIUIPanel->GetSelectionPositionX(); }
	float GetSelectionPositionY() const { return m_pIUIPanel->GetSelectionPositionY(); }
	float GetTabIndex() const { return m_pIUIPanel->GetTabIndex(); }

	//
	// Implementation of functions that panorama calls back on UI control classes
	//

	// kb/mouse management
	virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
	virtual bool OnKeyUp( const KeyData_t & code ) OVERRIDE;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
	virtual bool OnGamePadDown( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadUp( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnGamePadAnalog( const GamePadData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonDoubleClick( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonTripleClick( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseWheel( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnClick( IUIPanel *pPanel, const MouseData_t &code ) OVERRIDE;
	virtual bool OnVRTouchPad( const VRTouchEvent_t &code ) OVERRIDE{ return false; }

	// events
	bool OnScrollDirection( IUIScrollBar *pScrollBar, bool bIncreasePosition, float flDelta );
	bool OnScrollUp();
	bool OnScrollDown();
	bool OnScrollLeft();
	bool OnScrollRight();

	bool OnPageDirection( IUIScrollBar *pScrollBar, bool bIncreasePosition );
	bool OnPageUp();
	bool OnPageDown();
	bool OnPageLeft();
	bool OnPageRight();

	// Scrolling
	void ScrollVertically( float flScrollDelta, bool bImmediateMove = false );
	void ScrollHorizontally( float flScrollDelta, bool bImmediateMove = false );
	void ScrollToShowVerticalRange( float flRangeStart, float flRangeEnd );
	virtual void ScrollToXPercent( float flXPercent );
	virtual void ScrollToYPercent( float flXPercent );

	// navigation events, override these in children if you want special navigation handling,
	// shouldn't be needed often though, use tabindex/selectionpos in xml layout files!
	virtual bool OnMoveUp( int nRepeats );
	virtual bool OnMoveDown( int nRepeats );
	virtual bool OnMoveLeft( int nRepeats );
	virtual bool OnMoveRight( int nRepeats );
	virtual bool OnTabForward( int nRepeats );
	virtual bool OnTabBackward( int nRepeats );


	// child iteration
	class CChildIterator
	{
	public:
		CChildIterator( CPanel2D *pPanel, int iChildIndex ) : m_pPanel( pPanel ), m_iChildIndex( iChildIndex )
		{
			Assert( m_pPanel );
			Assert( iChildIndex >= 0 );
			Assert( iChildIndex <= m_pPanel->GetChildCount() );			// <= is intentional, supporting end() iterator
		}

		CPanel2D *operator*() { return m_pPanel->GetChild( m_iChildIndex ); }
		void operator++() { ++m_iChildIndex; }
		bool operator!=( const CChildIterator& other ) { return m_pPanel != other.m_pPanel || m_iChildIndex != other.m_iChildIndex; }

	private:
		CPanel2D *m_pPanel;
		int m_iChildIndex;
	};

	class CChildIteratorProxy
	{
	public:
		CChildIteratorProxy( CPanel2D *pPanel ) : m_pPanel( pPanel ) { Assert( pPanel ); }

		CChildIterator begin() { return CChildIterator( m_pPanel, 0 ); }
		CChildIterator end() { return CChildIterator( m_pPanel, m_pPanel->GetChildCount() ); }

	private:
		CPanel2D *m_pPanel;
	};

	CChildIteratorProxy Children() { return CChildIteratorProxy( this ); }

	int GetChildCount() const { return m_pIUIPanel->GetChildCount(); }
	CPanel2D *GetChild( int i ) const { return ToPanel2D( m_pIUIPanel->GetChild( i ) ); }
	CPanel2D *GetFirstChild() const { return ToPanel2D( m_pIUIPanel->GetFirstChild() ); }
	CPanel2D *GetLastChild() const { return ToPanel2D( m_pIUIPanel->GetLastChild() ); }
	// Return index of child in creation/panel vector order (also default tab order)
	int GetChildIndex( const CPanel2D *pChild ) const { if( !pChild ) return -1; else return m_pIUIPanel->GetChildIndex( pChild->UIPanel() ); }

	// Convenient way to iterate through children calling a lambda on each one. Return false from
	// the function break out of the loop midway through.
	bool IterateChildren( std::function< bool( CPanel2D *pChild ) > fn );
	bool IterateChildrenTraverse( std::function< bool( CPanel2D *pChild ) > fn );

	// Same as IterateChildren, but will only call the lambda on children that are exactly the
	// given type (checked by comparing the ).
	template < class T > bool IterateChildrenOfType( std::function< bool( T *pChild ) > fn );
	template < class T > bool IterateChildrenTraverseOfType( std::function< bool( T *pChild ) > fn );

	int GetHiddenChildCount() const { return m_pIUIPanel->GetHiddenChildCount(); }
	CPanel2D *GetHiddenChild( int i ) const { return ToPanel2D( m_pIUIPanel->GetHiddenChild( i ) ); }

	// Gets immediate ancestor of this panel in panel hierarchy
	CPanel2D *GetParent() const { return ToPanel2D( m_pIUIPanel->GetParent() ); }
	
	// searches only immediate children
	CPanel2D *FindChild( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindChild( pchID ) ); }

	// Considers a failure to find a child a fatal error.
	CPanel2D *RequireChild( const char *pchID ) { return ToPanel2D( m_pIUIPanel->RequireChild( pchID ) ); }

	// searches all children even outside layout file scope
	CPanel2D *FindChildTraverse( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindChildTraverse( pchID ) ); }

	// Considers a failure to find a child a fatal error.
	CPanel2D *RequireChildTraverse( const char *pchID ) { return ToPanel2D( m_pIUIPanel->RequireChildTraverse( pchID ) ); }

	// searches any children created from our layout file
	CPanel2D *FindChildInLayoutFile( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindChildInLayoutFile( pchID ) ); }

	// Considers a failure to find a child a fatal error.
	CPanel2D *RequireChildInLayoutFile( const char *pchID ) { return ToPanel2D( m_pIUIPanel->RequireChildInLayoutFile( pchID ) ); }

	// searches any panel created from our layout file (so parents or children!)
	CPanel2D *FindPanelInLayoutFile( const char *pchID ) { return ToPanel2D( m_pIUIPanel->FindPanelInLayoutFile( pchID ) ); }

	// Considers a failure to find a child a fatal error.
	CPanel2D *RequirePanelInLayoutFile( const char *pchID ) { return ToPanel2D( m_pIUIPanel->RequirePanelInLayoutFile( pchID ) ); }

	void MoveChildAfter( CPanel2D *pChildToMove, CPanel2D *pBefore ) { return m_pIUIPanel->MoveChildAfter( pChildToMove->UIPanel(), pBefore->UIPanel() ); }
	void MoveChildBefore( CPanel2D *pChildToMove, CPanel2D *pAfter ) { return m_pIUIPanel->MoveChildBefore( pChildToMove->UIPanel(), pAfter->UIPanel() ); }

	void FindChildrenWithClassTraverse( CPanoramaSymbol symClassName, /*out*/ CUtlVector<CPanel2D*> *pVecMatchingChildren );

	// window management
	IUIWindow *GetParentWindow() const { return m_pIUIPanel->GetParentWindow(); }

	// input & focus
	bool BAcceptsInput() { return m_pIUIPanel->BAcceptsInput();  }
	void SetAcceptsInput( bool bAllowInput ) { m_pIUIPanel->SetAcceptsInput( bAllowInput );  }
	bool BAcceptsFocus() const { return m_pIUIPanel->BAcceptsFocus(); }
	void SetAcceptsFocus( bool bAllowFocus ) { m_pIUIPanel->SetAcceptsFocus( bAllowFocus ); }
	bool BCanAcceptInput() { return m_pIUIPanel->BCanAcceptInput(); }
	void SetDefaultFocus( const char *pchChildID ) { m_pIUIPanel->SetDefaultFocus( pchChildID ); }
	const char *GetDefaultFocus() const { return m_pIUIPanel->GetDefaultFocus();  }
	void SetDisableFocusOnMouseDown( bool bDisable ) { m_pIUIPanel->SetDisableFocusOnMouseDown( bDisable ); }
	bool BFocusOnMouseDown() { return m_pIUIPanel->BFocusOnMouseDown(); }
	bool BCanClearFocusByClicking() { return m_pIUIPanel->BCanClearFocusByClicking(); }
	virtual bool BRequiresFocus() { return false; } // Override if your control requires taking focus in order to operate (e.g. TextEntry)

	bool BEnableAnalogStickScrolling() { return m_pIUIPanel->BEnableAnalogStickScrolling(); }
	void EnableAnalogStickScrolling( bool bEnable ) { m_pIUIPanel->EnableAnalogStickScrolling( bEnable ); }

	bool BScrollParentToFitWhenFocused() { return m_pIUIPanel->BScrollParentToFitWhenFocused(); }
	void SetScrollParentToFitWhenFocused( bool bScrollToFitParent ) { m_pIUIPanel->SetScrollParentToFitWhenFocused( bScrollToFitParent ); }

	// Should this panel be the top of an input hierarchy and keep track of focus within itself, not losing focus when a panel in some
	// other hierarchy changes focus?  Use this for panels that are peers like friends vs browser vs mainmenu in tenfoot
	bool BTopOfInputContext() {	return m_pIUIPanel->BTopOfInputContext(); }
	void SetTopOfInputContext( bool bIsTopOfInputContext ) { return m_pIUIPanel->SetTopOfInputContext( bIsTopOfInputContext ); }

	CPanel2D *GetParentInputContext() { return ToPanel2D( m_pIUIPanel->GetParentInputContext() ); }

	// Get the default input focus child within this panel, may be null
	CPanel2D *GetDefaultInputFocus() { return ToPanel2D( m_pIUIPanel->GetDefaultInputFocus() ); }

	// Override behavior for getting default input focus, callback from framework
	virtual IUIPanel *OnGetDefaultInputFocus() OVERRIDE { return NULL;  }

	// Set focus to this panel, which will auto-scroll it into full view as well if parent has overflow: scroll
	bool SetFocus() { return m_pIUIPanel->SetFocus(); }

	// Set the focus to this panel in it's input context, but do not make the context change if some other context currently
	// has focus
	bool UpdateFocusInContext() { return m_pIUIPanel->UpdateFocusInContext(); }

	// Set the focus in response to receiving hover (on panels that a parent sets childfocusonhover), this will
	// never scroll the parent.
	bool SetFocusDueToHover() { return m_pIUIPanel->SetFocusDueToHover(); }

	void SetInputContextFocus() { m_pIUIPanel->SetInputContextFocus(); }

	// retrieve the style flags (map to CSS psuedo-classes) for this panel
	uint GetStyleFlags() const { return m_pIUIPanel->GetStyleFlags(); }
	void AddStyleFlag( EStyleFlags eStyleFlag ) { m_pIUIPanel->AddStyleFlag( eStyleFlag ); }
	void RemoveStyleFlag( EStyleFlags eStyleFlag ) { m_pIUIPanel->RemoveStyleFlag( eStyleFlag ); }
	bool IsInspected() const { return m_pIUIPanel->IsInspected(); }
	bool BHasHoverStyle() const { return m_pIUIPanel->BHasHoverStyle(); }
	virtual void SetSelected( bool bSelected ) { m_pIUIPanel->SetSelected( bSelected ); }
	bool IsSelected() const { return m_pIUIPanel->IsSelected(); }
	bool BHasKeyFocus() const { return m_pIUIPanel->BHasKeyFocus(); }
	bool BHasDescendantKeyFocus() const { return m_pIUIPanel->BHasDescendantKeyFocus(); }
	bool IsLayoutLoading() const { return m_pIUIPanel->IsLayoutLoading(); }

	// enable/disable
	void SetEnabled( bool bEnabled ) { m_pIUIPanel->SetEnabled( bEnabled ); }
	bool IsEnabled() const { return m_pIUIPanel->IsEnabled(); }

	bool IsActivationEnabled() { return m_pIUIPanel->IsActivationEnabled(); }

	// Set activation disabled on this panel, input/focus still generally work, but Activate events won't be handled, useful to prevent a button
	// being clicked when out of focus, but leave it able to be focused for later activation or such
	void SetActivationEnabled( bool bEnabled ) { m_pIUIPanel->SetActivationEnabled( bEnabled ); }

	// Set all our immediate children enabled/disabled
	void SetAllChildrenActivationEnabled( bool bEnabled ) { m_pIUIPanel->SetAllChildrenActivationEnabled( bEnabled ); }

	// Enable/disable hit testing of this panel, you may want a parent that is never hit test that has a large region, but clicks
	// just pass through to other things behind it.  Children may still hit test.
	void SetHitTestEnabled( bool bEnabled ) { m_pIUIPanel->SetHitTestEnabled( bEnabled ); }
	bool BHitTestEnabled() const { return m_pIUIPanel->BHitTestEnabled(); }
	void SetHitTestEnabledTraverse( bool bEnabled ) { m_pIUIPanel->SetHitTestEnabledTraverse( bEnabled ); }

	// Enable/disable hit testing on children of this panel. Prevents recursing into children when doing hit testing,
	// thus it override children's individual hit test flags.
	void SetHitTestChildrenEnabled( bool bEnabled ) { m_pIUIPanel->SetHitTestChildrenEnabled( bEnabled ); }
	bool BHitTestChildrenEnabled() const { return m_pIUIPanel->BHitTestChildrenEnabled(); }

	void SetDraggable( bool bEnabled ) { m_pIUIPanel->SetDraggable( bEnabled ); }
	bool IsDraggable() const { return m_pIUIPanel->IsDraggable(); }

	void SetOnActivateEvent( IUIEvent *pEvent );
	void SetOnActivateEvent( const char *pchEventString );
	void SetOnFocusEvent( IUIEvent *pEvent );
	void SetOnCancelEvent( IUIEvent *pEvent );
	void SetOnContextMenuEvent( IUIEvent *pEvent );
	void SetOnLoadEvent( IUIEvent *pEvent );
	void SetOnMouseActivateEvent( IUIEvent *pEvent );
	void SetOnMouseOverEvent( IUIEvent *pEvent );
	void SetOnMouseOutEvent( IUIEvent *pEvent );
	void SetOnDblClickEvent( IUIEvent *pEvent );
	void SetOnTabForwardEvent( IUIEvent *pEvent );
	void SetOnTabBackwardEvent( IUIEvent *pEvent );
	void SetOnSelectEvent( IUIEvent *pEvent );
	void SetOnDeselectEvent( IUIEvent *pEvent );

	// bugbug jmccaskey - DELETE ME	
	// bugbug jmccaskey - both of the next two functions need to be deleted, we should
	// not expose panel event internals like this, but parental button needs some refactoring
	// first.  That refactoring must happen to work at all with javascript panel events anyway.
	VecUIEvents_t * GetPanelEvents( CPanoramaSymbol symEvent ) { return m_pIUIPanel->GetPanelEvents( symEvent ); }
	virtual void OnPanelEventSet( CPanoramaSymbol symEvent ) OVERRIDE { }

	bool BHasOnActivateEvent() { return m_pIUIPanel->BHasOnActivateEvent(); }
	bool BHasOnMouseActivateEvent() { return m_pIUIPanel->BHasOnMouseActivateEvent(); }

	void ClearOnActivateEvent();

	// Dialog variables
	void SetDialogVariable( const char *pchKey, const char *pchValue );
	void SetDialogVariable( const char *pchKey, int iVal );
	void SetDialogVariable( const char *pchKey, uint64 iVal );
	// We do NOT have a uint32 type here by design, to prevent you accidenatlly using a RTime32
	// and getting a number value. Either cast to int for a number or construct a CRTime
#if defined (SOURCE2_PANORAMA )
	void SetDialogVariable( const char *pchKey, time_t timeVal );
	void SetDialogVariable( const char *pchKey, CCurrencyAmount amount );
#else
	void SetDialogVariable( const char *pchKey, CAmount amount );
	void SetDialogVariable( const char *pchKey, CRTime timeVal );
#endif
	void SetDialogVariable( const char *varName, const CUtlString &value );
	void SetDialogVariableLocString( const char *varName, const char *pchValue );



	// Scrolling controls
	void ScrollToTop() { m_pIUIPanel->ScrollToTop(); }
	void ScrollToBottom() { m_pIUIPanel->ScrollToBottom(); }
	void ScrollToLeftEdge() { m_pIUIPanel->ScrollToLeftEdge(); }
	void ScrollToRightEdge() { m_pIUIPanel->ScrollToRightEdge(); }
	void ScrollParentToMakePanelFit( ScrollBehavior_t behavior = SCROLL_BEHAVIOR_DEFAULT, bool bImmediateScroll = false ) { m_pIUIPanel->ScrollParentToMakePanelFit( behavior, bImmediateScroll ); }
	void ScrollToFitRegion( float x0, float x1, float y0, float y1, ScrollBehavior_t behavior = SCROLL_BEHAVIOR_DEFAULT, bool bDirectParentScrollOnly = false, bool bImmediateScroll = false ) { m_pIUIPanel->ScrollToFitRegion( x0, x1, y0, y1, behavior, bDirectParentScrollOnly, bImmediateScroll ); }
	bool BCanSeeInParentScroll() { return m_pIUIPanel->BCanSeeInParentScroll(); }
	
	void OnScrollPositionChanged() { m_pIUIPanel->OnScrollPositionChanged(); }
	void SetSendChildScrolledIntoViewEvents( bool bSendChildScrolledIntoViewEvents ) { m_pIUIPanel->SetSendChildScrolledIntoViewEvents( bSendChildScrolledIntoViewEvents ); }	// this must be enabled on your parent for ScrolledIntoView events to fire and IsScrolledIntoView state to be set
	void FireScrolledIntoViewEvent() { m_pIUIPanel->FireScrolledIntoViewEvent(); }
	void FireScrolledOutOfViewEvent() { m_pIUIPanel->FireScrolledOutOfViewEvent(); }
	bool IsScrolledIntoView() const { return m_pIUIPanel->IsScrolledIntoView(); }

	bool BSelectionPosVerticalBoundary() { return m_pIUIPanel->BSelectionPosVerticalBoundary(); }
	bool BSelectionPosHorizontalBoundary() { return m_pIUIPanel->BSelectionPosHorizontalBoundary(); }

	// Tell panel to set focus to next panel in specified movement direction/type
	bool SetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float flYStart ) { return m_pIUIPanel->SetFocusToNextPanel( nRepeats, moveType, bAllowWrap, flTabIndexCurrent, flXPosCurrent, flYPosCurrent, flXStart, flYStart ); }
	virtual bool OnSetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float flYStart ) OVERRIDE{ return false; }
	bool SetInputFocusToFirstOrLastChildInFocusOrder( EFocusMoveDirection moveType, float flXStart, float flYStart ) { return m_pIUIPanel->SetInputFocusToFirstOrLastChildInFocusOrder( moveType, flXStart, flYStart ); }

	// hierarchy
	void SetParent( CPanel2D *pParent ) { m_pIUIPanel->SetParent( pParent ? pParent->UIPanel() : NULL ); }
	void RemoveAndDeleteChildren() { m_pIUIPanel->RemoveAndDeleteChildren(); }
	void RemoveAndDeleteChildrenOfType( CPanoramaSymbol symPanelType ) { m_pIUIPanel->RemoveAndDeleteChildrenOfType( symPanelType ); }
	uint32 GetChildCountOfType( CPanoramaSymbol symPanelType ) { return m_pIUIPanel->GetChildCountOfType( symPanelType ); }
	bool IsDescendantOf( const CPanel2D *pPanel ) const { return m_pIUIPanel->IsDescendantOf( pPanel ? pPanel->m_pIUIPanel : NULL ); }
	CPanel2D *FindAncestor( const char *pchID ) const { return ToPanel2D( m_pIUIPanel->FindAncestor( pchID ) );  }
	CPanel2D *FindLowestCommonAncestor( CPanel2D *pOther ) const { return ToPanel2D( m_pIUIPanel->FindLowestCommonAncestor( pOther ? pOther->UIPanel() : nullptr ) ); }

	// layout file
	CPanoramaSymbol GetLayoutFileLoadedFrom() const { return m_pIUIPanel->GetLayoutFileLoadedFrom(); }

	// Override this and return true if you know your panel will never draw outside it's bounds,
	// thus allowing an optimization to skip pushing clipping layers.
	virtual bool BRequiresContentClipLayer() OVERRIDE { return false; }

	// debug
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties );

	void SetTooltip( CPanel2D *pPanel );

	// Panel events
	bool DispatchPanelEvent( CPanoramaSymbol symPanelEvent ) { return m_pIUIPanel->DispatchPanelEvent( symPanelEvent ); }
	bool BParsePanelEvent( CPanoramaSymbol symPanelEvent, const char *pchValue, IUIPanel *pJavascriptContext ) { return m_pIUIPanel->BParsePanelEvent( symPanelEvent, pchValue, pJavascriptContext ); }
	bool BIsPanelEventSet( CPanoramaSymbol symPanelEvent ) { return m_pIUIPanel->BIsPanelEventSet( symPanelEvent ); }
	bool BIsPanelEvent( CPanoramaSymbol symProperty ) { return m_pIUIPanel->BIsPanelEvent( symProperty ); }

	// the input namespace to use for this panel
	const char *GetInputNamespace() const {	return m_pIUIPanel->GetInputNamespace(); }

	// the mouse cursor to display when hovered
	virtual EMouseCursors GetMouseCursor() OVERRIDE { return m_pIUIPanel->GetPanelMouseCursor(); }
	void SetPanelMouseCursor( EMouseCursors eCursor ) { m_pIUIPanel->SetPanelMouseCursor( eCursor ); }

	// controls if clicking on an unfocused panel should set focus
	void SetMouseCanActivate( EMouseCanActivate eMouseCanActivate, const char *pchOptionalParent = NULL ) { m_pIUIPanel->SetMouseCanActivate( eMouseCanActivate, pchOptionalParent ); }
	EMouseCanActivate GetMouseCanActivate() { return m_pIUIPanel->GetMouseCanActivate(); }
	CPanel2D *FindParentForMouseCanActivate() { return ToPanel2D( m_pIUIPanel->FindParentForMouseCanActivate() ); }

	// controls if clicking on an unfocused panel should set focus
	void SetChildFocusOnHover( bool bEnable ) { m_pIUIPanel->SetChildFocusOnHover( bEnable ); }
	bool GetChildFocusOnHover() { return m_pIUIPanel->GetChildFocusOnHover(); }

	// controls if hovering on an unfocused panel should set focus
	void SetFocusOnHover(bool bEnable) { m_pIUIPanel->SetFocusOnHover(bEnable); }
	bool GetFocusOnHover() { return m_pIUIPanel->GetFocusOnHover(); }

	// Set background images for the panel
	void SetBackgroundImages( const CUtlVector< CBackgroundImageLayer * > &vecLayers );
	CUtlVector< CBackgroundImageLayer * > *GetBackgroundImages();

	// called once after registering with UIEngine::CallBeforeStyleAndLayout()
	virtual void OnCallBeforeStyleAndLayout() OVERRIDE {}

	// clone
	virtual bool IsClonable();
	virtual CPanel2D *Clone();

	// sort children
	void SortChildren( std::function< int( IUIPanelClient *, IUIPanelClient * ) > fnCompare ) { m_pIUIPanel->SortChildren( fnCompare ); }

	// set the namespace to use for input
	void SetInputNamespace( const char *pchNamespace ) { m_pIUIPanel->SetInputNamespace( pchNamespace ); }

	// override this if you need to have the loc system consider an alternate panel hierarchy for resolving dialog variables
	virtual IUIPanel *GetLocalizationParent() const OVERRIDE { return GetParent() ? GetParent()->m_pIUIPanel : NULL; }

	// child management, use with caution! normally always managed internally.
	void AddChild( CPanel2D *pChild ) { m_pIUIPanel->AddChild( pChild->UIPanel() ); }

	// child management, use with caution! normally always managed internally.  Returns child index we inserted at.
	int AddChildSorted( bool( __cdecl *pfnLessFunc )(ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2), CPanel2D *pChild ) { return m_pIUIPanel->AddChildSorted( pfnLessFunc, pChild->UIPanel() ); }

	// re-sort a newly inserted child
	virtual int ReSortChild( bool( __cdecl *pfnLessFunc )( ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2 ), CPanel2D *pChild ) { return m_pIUIPanel->ReSortChild( pfnLessFunc, pChild->UIPanel() ); }

	// child management, use with caution! normally always managed internally.
	void RemoveChild( CPanel2D *pChild ) { m_pIUIPanel->RemoveChild( pChild->UIPanel() ); }

	// Check if styles are dirty for the panel
	bool BStylesDirty() const { return m_pIUIPanel->BStylesDirty(); }
	
	// Check if styles are possibly dirty for any of our children
	bool BChildStylesDirty() { return m_pIUIPanel->BChildStylesDirty(); }

	// Set that we need an on styles changed call when styles become non-dirty, even if there is no actual change.
	void SetOnStylesChangedNeeded() { m_pIUIPanel->SetOnStylesChangedNeeded(); }

	void ClearLastChildFocus() { m_pIUIPanel->ClearLastChildFocus(); }

	// Getter for panel attributes
	int GetAttribute( const char *pchAttrName, int nDefaultValue ) const { return m_pIUIPanel->GetAttribute( pchAttrName, nDefaultValue ); }
	const char *GetAttribute( const char *pchAttrName, const char * pchDefaultValue ) const { return m_pIUIPanel->GetAttribute( pchAttrName, pchDefaultValue ); }
	uint32 GetAttribute( const char *pchAttrName, uint32 unDefaultValue ) const { return m_pIUIPanel->GetAttribute( pchAttrName, unDefaultValue ); }
	uint64 GetAttribute( const char *pchAttrName, uint64 unDefaultValue ) const { return m_pIUIPanel->GetAttribute( pchAttrName, unDefaultValue ); }
	float GetAttribute( const char *pchAttrName, float flDefaultValue ) const { return m_pIUIPanel->GetAttribute( pchAttrName, flDefaultValue ); }

	int GetAttribute( CPanoramaSymbol symAttribute, int nDefaultValue ) const { return m_pIUIPanel->GetAttribute( symAttribute, nDefaultValue ); }
	const char *GetAttribute( CPanoramaSymbol symAttribute, const char * pchDefaultValue ) const { return m_pIUIPanel->GetAttribute( symAttribute, pchDefaultValue ); }
	uint32 GetAttribute( CPanoramaSymbol symAttribute, uint32 unDefaultValue ) const { return m_pIUIPanel->GetAttribute( symAttribute, unDefaultValue ); }
	uint64 GetAttribute( CPanoramaSymbol symAttribute, uint64 unDefaultValue ) const { return m_pIUIPanel->GetAttribute( symAttribute, unDefaultValue ); }
	float GetAttribute( CPanoramaSymbol symAttribute, float flDefaultValue ) const { return m_pIUIPanel->GetAttribute( symAttribute, flDefaultValue ); }

	// Setter for panel attributes
	void SetAttribute( const char *pchAttrName, int nValue ) { m_pIUIPanel->SetAttribute( pchAttrName, nValue ); }
	void SetAttribute( const char *pchAttrName, const char * pchValue ) { m_pIUIPanel->SetAttribute( pchAttrName, pchValue ); }
	void SetAttribute( const char *pchAttrName, uint32 unValue ) { m_pIUIPanel->SetAttribute( pchAttrName, unValue ); }
	void SetAttribute( const char *pchAttrName, uint64 unValue ) { m_pIUIPanel->SetAttribute( pchAttrName, unValue ); }
	void SetAttribute( const char *pchAttrName, float flValue ) { m_pIUIPanel->SetAttribute( pchAttrName, flValue ); }

	void SetAttribute( CPanoramaSymbol symAttribute, int nValue ) { m_pIUIPanel->SetAttribute( symAttribute, nValue ); }
	void SetAttribute( CPanoramaSymbol symAttribute, const char * pchValue ) { m_pIUIPanel->SetAttribute( symAttribute, pchValue ); }
	void SetAttribute( CPanoramaSymbol symAttribute, uint32 unValue ) { m_pIUIPanel->SetAttribute( symAttribute, unValue ); }
	void SetAttribute( CPanoramaSymbol symAttribute, uint64 unValue ) { m_pIUIPanel->SetAttribute( symAttribute, unValue ); }
	void SetAttribute( CPanoramaSymbol symAttribute, float flValue ) { m_pIUIPanel->SetAttribute( symAttribute, flValue ); }

	// Remove a panel attribute
	void RemoveAttribute( const char *pchAttrName ) { m_pIUIPanel->RemoveAttribute( pchAttrName ); }
	void RemoveAttribute( CPanoramaSymbol symAttribute ) { m_pIUIPanel->RemoveAttribute( symAttribute ); }

	// walks parents calculating the top left corner relative to window space
	void GetPositionWithinWindow( float *pflX, float *pflY );
	Vector2D GetPositionWithinWindowJS();

	// Given an array of points within the current panel's coordinate system, convert them to an ancestor's cooordinate system.
	// If the passed in panel is NULL or not an ancestor, this will end up being relative to the top level window
	void GetPointsWithinAncestor( CPanel2D *pAncestor, const Vector *pPointsIn, Vector* pPointsOut, int nPointCount );
	void GetPointsWithinAncestor( CPanel2D *pAncestor, const Vector2D *pPointsIn, Vector2D* pPointsOut, int nPointCount );

	// Get the position of the top left corner of this panel relative to an ancestor.  If the passed in panel is NULL or
	// not an ancestor, this will end up being relative to the top level window 
	virtual void GetPositionWithinAncestor( CPanel2D *pAncestor, float *pflX, float *pflY ) OVERRIDE;

	// Given an array of points within the current panel's coordinate system, convert them to another panel's coordinate system
	// If the passed in panel is NULL or not in the same top level window, this will end up being relative to the top level window
	void GetPointsRelativeToPanel( CPanel2D *pOtherPanel, const Vector *pPointsIn, Vector *pPointsOut, int nPointCount );
	void GetPointsRelativeToPanel( CPanel2D *pOtherPanel, const Vector2D *pPointsIn, Vector2D *pPointsOut, int nPointCount );

	// Get the axis-aligned bounding box of this panel relative to an ancestor. If the passed in panel is NULL or not
	// an ancestor, this will end up being relative to the top level window 
	void GetBoundsWithinAncestor( CPanel2D *pAncestor, float *pflLeft, float *pflTop, float *pflRight, float *pflBottom );

	bool BHasAnyActiveTransitions();

	// Get the nearest parent that establishes a javascript context, or return ourself if we ourselves create one
	panorama::CPanel2D *GetJavaScriptContextParent() const { return (CPanel2D*)(m_pIUIPanel->GetJavaScriptContextParent()->ClientPtr()); }

	// Called to ask us to setup object template for Javascript, you can implement this in a child class and then call
	// the base method (so all the normal panel2d stuff gets exposed), plus call the various RegisterJS helpers yourself
	// to expose additional panel type specific data/methods.
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// Call an arbitrary javascript function in the context of this panel. 
	template < typename ReturnType, typename ...Arguments >
	ReturnType CallJSFunction( const char *pchFunctionName, const Arguments&... args );

	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewVerticalScrollBar( float flInitialScrollPos ) OVERRIDE;

	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewHorizontalScrollBar( float flInitialScrollPos ) OVERRIDE;

	// Callback to hide tooltip if it's visible
	virtual void HideTooltip() OVERRIDE;

	// Has this panel ever been layed out
	virtual bool BHasBeenLayedOut() const { return m_pIUIPanel->BHasBeenLayedOut(); }

	// Allow overriding the status of scrolling for this panel
	virtual bool BCanCustomScrollUp() const OVERRIDE { return false;  }
	virtual bool BCanCustomScrollDown() const OVERRIDE{ return false; }
	virtual bool BCanCustomScrollLeft() const OVERRIDE { return false; }
	virtual bool BCanCustomScrollRight() const OVERRIDE{ return false; }

	virtual bool BCustomCanDragScroll() const OVERRIDE { return false; }
	virtual bool BCustomScrollInProgress() OVERRIDE { return false; }

	// Allow custom behavior on a layout file reload
	virtual void OnLayoutReloading() OVERRIDE {}
	virtual void OnLayoutReloaded() OVERRIDE {}

	// composition layer hints to improve performance
	void SetRequireCompositionLayer( bool bRequireCompositionLayer ) { m_pIUIPanel->SetRequireCompositionLayer( bRequireCompositionLayer ); }
	bool BRequireCompositionLayer() const { return m_pIUIPanel->BRequireCompositionLayer(); }
	void SetAlwaysCacheCompositionLayer( bool bAlwaysCacheCompositionLayer ) { m_pIUIPanel->SetAlwaysCacheCompositionLayer( bAlwaysCacheCompositionLayer ); }
	bool BAlwaysCacheCompositionLayer() const { return m_pIUIPanel->BAlwaysCacheCompositionLayer(); }
	void SetForceNoCompositionLayer( bool bForceNoCompositionLayer ) { m_pIUIPanel->SetForceNoCompositionLayer( bForceNoCompositionLayer ); }
	bool BForceNoCompositionLayer() const { return m_pIUIPanel->BForceNoCompositionLayer(); }

	// ready for display
	void RegisterForReadyEvents( bool bEnable ) { m_pIUIPanel->RegisterForReadyEvents( bEnable ); }
	bool BReadyForDisplay() { return m_pIUIPanel->BReadyForDisplay(); }
	void SetReadyForDisplay( bool bReady ) { m_pIUIPanel->SetReadyForDisplay( bReady ); }

	// CSSKeyframesRule-like support
	// CloneKeyframes looks for @keyframes of given name in panel's style file set. 
	// If found, returns copy, nullptr otherwise.
	CJSKeyframesObject *JSCreateCopyOfCSSKeyframes( const char *pchKeyframesName ) { return m_pIUIPanel->JSCreateCopyOfCSSKeyframes( pchKeyframesName ); }
	void JSDeleteKeyframes( CJSKeyframesObject *pKeyframes ) { m_pIUIPanel->JSDeleteKeyframes( pKeyframes ); }
	void JSUpdateCurrentAnimationKeyframes( CJSKeyframesObject *pKeyframes ) { m_pIUIPanel->JSUpdateCurrentAnimationKeyframes( pKeyframes ); }

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
	void Validate( CValidator &validator, const tchar *pchName );
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif

	void SetLayoutLoadedFromParent( CPanel2D *pParent ) { m_pIUIPanel->SetLayoutLoadedFromParent( pParent ? pParent->UIPanel() : NULL ); }
	void SetPanelIntoContext( CPanel2D *pPanel ) { m_pIUIPanel->SetPanelIntoContext( pPanel->UIPanel() ); }

	IUIImageManager *UIImageManager() { return m_pIUIPanel->UIImageManager(); }

	// Call a given javascript function
	static v8::Handle< v8::Value > CallPanelJSFunctionArgsCore( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv );

protected:
	friend class CPanelStyle;
	friend class CStyleFileSet;

	virtual IUIRenderEngine *AccessRenderEngine() { return m_pIUIPanel->UIRenderEngine(); }
	virtual IUIRenderDevice *AccessRenderDevice() { return m_pIUIPanel->UIRenderDevice(); }

	void FirePanelLoadedEvent() { m_pIUIPanel->FirePanelLoadedEvent();  }

	// override to change how this panel is measured
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE
	{ 
		m_pIUIPanel->OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );  
	}

	// override to add additional panel events
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;
	
	// override to change how this panel arranges its children
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE{ m_pIUIPanel->OnLayoutTraverse( flFinalWidth, flFinalHeight ); }

	virtual void OnStylesChanged() OVERRIDE { if ( m_pIUIPanel->GetParent() ) { m_pIUIPanel->GetParent()->ClientPtr()->OnChildStylesChanged(); } }
	virtual void OnVisibilityChanged() OVERRIDE {}

	virtual void OnChildStylesChanged() OVERRIDE { }

	// methods for setting properties from a layout file. Default BSetProperties calls BSetProperty on each element.
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) OVERRIDE;
	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;


	virtual void OnBeforeChildrenChanged() OVERRIDE {}
	virtual void OnAfterChildrenChanged() OVERRIDE {}
	virtual void OnRemoveChild( IUIPanel *pChild ) OVERRIDE {}

	virtual void OnInitializedFromLayout() OVERRIDE {}

	void SetLayoutFile( CPanoramaSymbol symLayoutFile ) { m_pIUIPanel->SetLayoutFile( symLayoutFile ); }
	void SetLayoutFileTraverse( CPanoramaSymbol symLayoutFile );

	void SetMouseTracking( bool bState ) { m_pIUIPanel->SetMouseTracking( bState ); }
	
	// for cloning
	bool AreChildrenClonable();
	virtual void InitClonedPanel( CPanel2D *pPanel );

	// for setting parent disabled style flag	
	//bugbug jmccaskey - these are broken... would like to fix so they are uneeded
	virtual void AddDisabledFlagToChildren() { }
	virtual void RemoveDisabledFlagFromChildren() { }

	// Pointer to basic IUIPanel interface from panorama
	IUIPanel *m_pIUIPanel;

private:
	// we by design don't have a uint32 overload of this, use this function here to enforce it
	void SetDialogVariable( const char *pchKey, uint32 nInvalid ) { Assert( !"Invalid call" ); }

	CUtlVector<IUIPanel *> const &AccessChildren() { return m_pIUIPanel->AccessChildren(); }
	CUtlVector<IUIPanel *> const &JSFindChildrenWithClassTraverse( const char *pchClass );
	void BJSLoadLayoutFromString( const v8::FunctionCallbackInfo<v8::Value> &args );

	void GetJSData( const v8::FunctionCallbackInfo< v8::Value > &args );
	
	// event handler functions, these CANNOT be virtual, if you need to override then
	// have this function call into another helper that is virtual to override behavior
	bool EventAppendChildrenFromLayoutFileAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile );
	bool EventAddStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventRemoveStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventToggleStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventSwitchStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchAttributeName, const char *pchName );
	bool EventTriggerStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventAddStyleClassToEachChild( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventRemoveStyleClassFromEachChild( const CPanelPtr< IUIPanel > &pPanel, const char *pchName );
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventPanelCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventPanelContextMenu( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventPanelLoaded( const CPanelPtr< IUIPanel > &pPanel );
	bool EventShowTooltip( const CPanelPtr< IUIPanel > &pPanel );
	bool EventScrollUp()		{ return OnScrollUp(); }
	bool EventScrollDown()		{ return OnScrollDown(); }
	bool EventScrollLeft()		{ return OnScrollLeft(); }
	bool EventScrollRight()		{ return OnScrollRight(); }
	bool EventScrollPanelUp( const CPanelPtr< IUIPanel > &pPanel )		{ return OnScrollUp(); }
	bool EventScrollPanelDown( const CPanelPtr< IUIPanel > &pPanel )	{ return OnScrollDown(); }
	bool EventScrollPanelLeft( const CPanelPtr< IUIPanel > &pPanel )	{ return OnScrollLeft(); }
	bool EventScrollPanelRight( const CPanelPtr< IUIPanel > &pPanel )	{ return OnScrollRight(); }
	bool EventPageUp()			{ return OnPageUp(); }
	bool EventPageDown()		{ return OnPageDown(); }
	bool EventPageLeft()		{ return OnPageLeft(); }
	bool EventPageRight()		{ return OnPageRight(); }
	bool EventPagePanelUp( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageUp(); }
	bool EventPagePanelDown( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageDown(); }
	bool EventPagePanelLeft( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageLeft(); }
	bool EventPagePanelRight( const CPanelPtr< IUIPanel > &pPanel )	{ return OnPageRight(); }
	bool EventScrollToTop( const CPanelPtr< IUIPanel > &pPanel );
	bool EventScrollToBottom( const CPanelPtr< IUIPanel > &pPanel );
	bool EventMoveUp( int nRepeats )			{ return OnMoveUp( nRepeats ); }
	bool EventMoveDown( int nRepeats )		{ return OnMoveDown( nRepeats ); } 
	bool EventMoveLeft( int nRepeats )		{ return OnMoveLeft( nRepeats ); }
	bool EventMoveRight( int nRepeats )		{ return OnMoveRight( nRepeats ); }
	bool EventMovePanelUp( const CPanelPtr< IUIPanel > &pPanel, int nRepeats )		{ return OnMoveUp( nRepeats ); }
	bool EventMovePanelDown( const CPanelPtr< IUIPanel > &pPanel, int nRepeats )	{ return OnMoveDown( nRepeats ); }
	bool EventMovePanelLeft( const CPanelPtr< IUIPanel > &pPanel, int nRepeats )	{ return OnMoveLeft( nRepeats ); }
	bool EventMovePanelRight( const CPanelPtr< IUIPanel > &pPanel, int nRepeats )	{ return OnMoveRight( nRepeats ); }
	bool EventTabForward( int nRepeats )		{ return OnTabForward( nRepeats ); }
	bool EventTabBackward( int nRepeats )		{ return OnTabBackward( nRepeats ); }	
	bool EventImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage );
	bool EventImageFailedLoad( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage );
	bool EventSetPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName, const char *pchPanelEventAction );
	bool EventClearPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName );
	bool EventDispatchPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName );
	bool EventIfHasClassEvent( const CPanelPtr< IUIPanel > &pPanel, const char * pchClassName, IUIEvent * pEventToFire );
	bool EventIfNotHasClassEvent( const CPanelPtr< IUIPanel > &pPanel, const char * pchClassName, IUIEvent * pEventToFire );
	bool EventIfHoverOtherEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent * pEventToFire );
	bool EventIfNotHoverOtherEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent * pEventToFire );
	bool EventIfHoverOverEventInternal( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent * pEventToFire, bool bFireIfHovered );
	bool EventCheckChildrenScrolledIntoView( const CPanelPtr< IUIPanel > &pPanel ) { return m_pIUIPanel->OnCheckChildrenScrolledIntoView(); }
	bool EventScrollPanelIntoView( const CPanelPtr< IUIPanel > &pPanel, ScrollBehavior_t behavior, bool bImmediate );
	bool EventSetPanelEnabled( const CPanelPtr< IUIPanel > &pPanel, bool bEnabled );

	bool EventDragScrollStart( const CPanelPtr< IUIPanel > &pPanel );
	bool EventDragScrollMouseMove( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, int nX, int nY );
	bool EventDragScrollEnd( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, float flVelocityX, float flVelocityY );

	void Initialize( IUIWindow *window, CPanel2D *parent, const char *pchID, uint32 ePanelFlags );
	
	// Is this a property we must create our children before applying during layout file application?
	virtual bool BIsDelayedProperty( CPanoramaSymbol symProperty ) OVERRIDE;

	panorama::IUIPanelStyle *JSAccessStyle() const { return m_pIUIPanel->AccessIUIStyle(); }
	
	void ClearPanelEventJS( CPanoramaSymbol symPanelEvent ) { m_pIUIPanel->ClearPanelEvents( symPanelEvent ); }

	void SetDefaultFocusOnMouseDownBehavior();

	// tooltip for panel. Need to keep a safe pointer as the tooltip is a top level window and will be deleted at shutdown automatically
	CPanelPtr< CPanel2D > m_pTooltip;
	
	v8::Persistent< v8::Object > *m_pJSData; // .data()

	static CUtlVector<IUIPanel *> s_vecMatchingChildren;
};

template < class T > bool CPanel2D::IterateChildrenOfType( std::function< bool( T *pChild ) > fn )
{
	return IterateChildren( [&]( CPanel2D *pChild ) -> bool {
		if ( pChild->GetPanelType() != T::GetPanelSymbol() )
			return true;
		return (bool)fn( assert_cast< T * >( pChild ) );
	} );
}

template < class T > bool CPanel2D::IterateChildrenTraverseOfType( std::function< bool( T *pChild ) > fn )
{
	return IterateChildrenTraverse( [ &]( CPanel2D *pChild ) -> bool {
		if ( pChild->GetPanelType() != T::GetPanelSymbol() )
			return true;
		return (bool)fn( assert_cast< T * >( pChild ) );
	} );
}

// Helper functions to fill in variadic template arguments for CallJSFunction
inline void FillJSArgsArray( v8::Handle< v8::Value > *pArgs );
template < typename T > inline void FillJSArgsArray( v8::Handle< v8::Value > *pArgs, const T &firstArg );
template < typename T, typename ...RemainingArguments >	inline void FillJSArgsArray( v8::Handle< v8::Value > *pArgs, const T &firstArg, const RemainingArguments&... args );

// Helper function to actually call a JS function on a panel given the arguments as an array
template < typename ReturnType > ReturnType CallPanelJSFunctionArgs( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv );

// Explicit specializations to have special behavior for a return value types
template <> void CallPanelJSFunctionArgs< void >( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv );
template <> const char *CallPanelJSFunctionArgs< const char * >( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv );
template <> CUtlString CallPanelJSFunctionArgs< CUtlString >( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv );


//-----------------------------------------------------------------------------
// Purpose: Convert the variadic template arguments into an array of v8 values
//-----------------------------------------------------------------------------
inline void FillJSArgsArray( v8::Handle< v8::Value > *pArgs )
{
}
template < typename T >
inline void FillJSArgsArray( v8::Handle< v8::Value > *pArgs, const T &firstArg )
{
	PanoramaTypeToV8Param( firstArg, pArgs );
}
template < typename T, typename ...RemainingArguments >
inline void FillJSArgsArray( v8::Handle< v8::Value > *pArgs, const T &firstArg, const RemainingArguments&... args )
{
	PanoramaTypeToV8Param( firstArg, pArgs );
	FillJSArgsArray( pArgs + 1, args... );
}

//-----------------------------------------------------------------------------
// Purpose: Call the given javascript function in the context of the given panel
//-----------------------------------------------------------------------------
template < typename ReturnType >
inline ReturnType CallPanelJSFunctionArgs( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv )
{
	v8::Handle< v8::Value > result = CPanel2D::CallPanelJSFunctionArgsCore( pPanel, pchFunctionName, argc, argv );

	ReturnType returnValue;
	V8ParamToPanoramaType( result, &returnValue );
	return returnValue;
}

//-----------------------------------------------------------------------------
// Purpose: CallPanelJSFunctionArgs template specialization for void return value
//-----------------------------------------------------------------------------
template <>
inline void CallPanelJSFunctionArgs< void >( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv )
{
	CPanel2D::CallPanelJSFunctionArgsCore( pPanel, pchFunctionName, argc, argv );
}

//-----------------------------------------------------------------------------
// Purpose: CallPanelJSFunctionArgs template specialization for const char * return value
//-----------------------------------------------------------------------------
template <>
inline const char * CallPanelJSFunctionArgs< const char * >( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv )
{
	AssertMsg( false, "Use the CUtlString version to return strings from javascript. This avoids memory leaks." );
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: CallPanelJSFunctionArgs template specialization for CUtlString return value
//-----------------------------------------------------------------------------
template <>
inline CUtlString CallPanelJSFunctionArgs< CUtlString >( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv )
{
	v8::Handle< v8::Value > result = CPanel2D::CallPanelJSFunctionArgsCore( pPanel, pchFunctionName, argc, argv );

	v8::String::Utf8Value strValue(v8::Isolate::GetCurrent(),  result );
	return CUtlString( *strValue );
}

//-----------------------------------------------------------------------------
// Purpose: Call the given javascript function on this panel
//-----------------------------------------------------------------------------
template < typename ReturnType, typename ...Arguments >
ReturnType CPanel2D::CallJSFunction( const char *pchFunctionName, const Arguments&... args )
{
	v8::Isolate *pIsolate = UIEngine()->GetV8Isolate();
	v8::Isolate::Scope isolate_scope( pIsolate );
	v8::HandleScope handle_scope( pIsolate );

	const int argc = sizeof...( Arguments );

	v8::Handle< v8::Value > argv[ argc == 0 ? 1 : argc ];
	if ( argc > 0 )
	{
		FillJSArgsArray( argv, args... );
	}

	return CallPanelJSFunctionArgs< ReturnType >( UIPanel(), pchFunctionName, argc, argv );
}

//-----------------------------------------------------------------------------
// Purpose: Fast, safe downcasting for panel types
//-----------------------------------------------------------------------------
template <typename T> T require_pointer_type( T** p );
template <typename T> T require_const_pointer_type( const T** p );

template <typename T>
FORCEINLINE const T* CPanel2D::downcast() const
{
	// fast, reduces to uint16 load from memory
	CPanoramaSymbol symTarget = T::GetPanelSymbol();

	// vf call to return pointer to static 2 pointer (8-byte) object
	const CPanel2DClassInfo* pClassInfo = &GetPanelClassInfo();

	// Walk the parent chain from our type.  This allows us to cast to a middle
	// class in a class hierarchy.  For example, if you have
	//			class GameLabel : public panorama::Label
	// then pSomeGameLabel->downcast<panorama::Label>() will work.
	//
	// The common case is that the cast is to an exact match for the target
	// panel.  That is the fastest path in this code, and immediately exits
	// successfully in the first iteration of the loop.
	for ( ; pClassInfo != nullptr; pClassInfo = pClassInfo->m_pParentClassInfo )
	{
		// We could optimize this somewhat by keeping a copy of the symbol
		// in pClassInfo instead of a pointer to it.  This would require
		// updating panorama panel-type initialization to initialize the
		// new classinfo data as well.
		if ( symTarget == *pClassInfo->m_pSymbol )
			return static_cast< const T* >( this );
	}

	// Not found, fail
	return nullptr;
}

template <typename T>
FORCEINLINE T* CPanel2D::downcast()
{
	const CPanel2D * constThis = this;
	return const_cast< T* >( constThis->downcast<T>() );
}


template <typename T>
FORCEINLINE T panel_cast( CPanel2D* pPanel )
// requires T is a pointer type
{
	if ( !pPanel )
		return nullptr;

	T result;
	result = pPanel->downcast< decltype( require_pointer_type( &result ) ) >();
	return result;
}

template <typename T>
FORCEINLINE T panel_cast( const CPanel2D* pPanel )
// requires T is a const pointer type
{
	if ( !pPanel )
		return nullptr;

	T result;
	result = pPanel->downcast< decltype( require_const_pointer_type( &result ) ) >();
	return result;
}

template <typename T>
FORCEINLINE T panel_cast( CPanel2D* pPanel, bool bRequireDowncast )
// requires T is a pointer type
{
	if ( !pPanel )
		return nullptr;

	T result;
	result = pPanel->downcast< decltype( require_pointer_type( &result ) ) >();
	Assert( !bRequireDowncast || result != nullptr );

	return result;
}

template <typename T>
FORCEINLINE const T panel_cast( const CPanel2D* pPanel, bool bRequireDowncast )
// requires T is a const pointer type
{
	if ( !pPanel )
		return nullptr;

	T result;
	result = pPanel->downcast< decltype( require_const_pointer_type( &result ) ) >();
	Assert( !bRequireDowncast || result != nullptr );

	return result;
}

#pragma warning(pop)


} // namespace panorama

#endif // PANEL2D_H

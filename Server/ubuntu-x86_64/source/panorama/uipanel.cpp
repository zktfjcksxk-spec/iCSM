#include "tier0/icommandline.h"
//=========== Copyright Valve Corporation, All rights reserved. ===============// 
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uipanel.h"
#include "uijsregistration.h"
#include "layout/layoutfile.h"
#if defined( SOURCE2_PANORAMA )
#include "rendersystem/irenderhardwareconfig.h"
#include "tier1/base64.h"
#include "appframework/iapplication.h"
#endif
#include "iuisoundsystem.h"

#include "ctx_debug.h"

#if !defined( SOURCE2_PANORAMA )
#define FCVAR_ARCHIVE 0
#endif

using namespace panorama;

extern EStyleFlags EStyleFlagsFromName( const char *pchName );

namespace panorama
{
	bool CUIPanel::s_bInApplyLayoutFile = false;
	CUtlVector< CascadeStyleFileInfo_t > CUIPanel::s_vecApplyStylesTemp( 0, 32 );
	bool CUIPanel::s_bInScrollBarConstruction = false;

	// convars
	ConVar g_ConVarReloadAnimations( "@panorama_reload_animations", "1" );
	ConVar g_ConVarOverlayOpacity( "@panorama_debug_overlay_opacity", "0.8", FCVAR_ARCHIVE );
	ConVar g_ConVarCacheCommandListRepaintThreshold( "@panorama_cache_command_list_repaint_threshold", "0.25" );
	ConVar g_ConVarCacheCommandListSizeThreshold( "@panorama_cache_command_list_size_threshold", "2048" );

	DECLARE_PANORAMA_EVENT0( TimeoutFastScrollSound );
	DEFINE_PANORAMA_EVENT( TimeoutFastScrollSound );

	// base set of panel events on uipanel
	const CPanoramaSymbol k_symPropertyClass( "class" );
	const CPanoramaSymbol k_symPropertyOnLoad( "onload" );
	const CPanoramaSymbol k_symPropertyOnActivate( "onactivate" );
	const CPanoramaSymbol k_symPropertyOnMouseActivate( "onmouseactivate" );
	const CPanoramaSymbol k_symPropertyOnContextMenu( "oncontextmenu" );
	const CPanoramaSymbol k_symPropertyOnFocus( "onfocus" );
	const CPanoramaSymbol k_symPropertyOnDescendantFocus( "ondescendantfocus" );
	const CPanoramaSymbol k_symPropertyOnBlur( "onblur" );
	const CPanoramaSymbol k_symPropertyOnDescendantBlur( "ondescendantblur" );
	const CPanoramaSymbol k_symPropertyOnCancel( "oncancel" );
	const CPanoramaSymbol k_symPropertyOnMouseOver( "onmouseover" );
	const CPanoramaSymbol k_symPropertyOnMouseOut( "onmouseout" );
	const CPanoramaSymbol k_symPropertyOnDblClick( "ondblclick" );
	const CPanoramaSymbol k_symNavigateUpEvent( "onmoveup" );
	const CPanoramaSymbol k_symNavigateDownEvent( "onmovedown" );
	const CPanoramaSymbol k_symNavigateLeftEvent( "onmoveleft" );
	const CPanoramaSymbol k_symNavigateRightEvent( "onmoveright" );
	const CPanoramaSymbol k_symNavigateTabEvent( "ontabforward" );
	const CPanoramaSymbol k_symNavigateTabbackEvent( "ontabbackward" );
	const CPanoramaSymbol k_symPropertyOnSelect( "onselect" );
	const CPanoramaSymbol k_symPropertyOnDeselect( "ondeselect" );
	const CPanoramaSymbol k_symPropertyOnScrolledToBottom( "onscrolledtobottom" );
	const CPanoramaSymbol k_symPropertyOnScrolledToRightEdge( "onscrolledtorightedge" );
	const CPanoramaSymbol k_symOnPanelEvent( "onpanelevent" );
}

static ConVar s_convarPanoramaStyleFlagForceInvalidate( "@panorama_style_flag_force_invalidate", "0", FCVAR_DEVELOPMENTONLY, "Force style invalidation of the entire panel subtree when adding / removing style flags." );
static ConVar s_convarPanoramaClassesForceInvalidate( "@panorama_classes_force_invalidate", "0", FCVAR_DEVELOPMENTONLY, "Force style invalidation of the entire panel subtree when adding / removing classes." );


#ifndef RETAIL
#define REPAINT_WATCH 1
#else
#define REPAINT_WATCH 0
#endif
#if REPAINT_WATCH
ConVar g_ConVarRepaintWatchId( "@panorama_repaint_watch_id", "" );

static bool IsRepaintWatchMatch( const char *pchID )
{
	const char *pchWatch = g_ConVarRepaintWatchId.GetString();
	if ( pchWatch[0] && V_stricmp( pchWatch, pchID ) == 0 )
	{
		return true;
	}
	return false;
}

static void ReportRepaintWatchMatch( const char *pchOrigin, CUIPanel *pPanel, EPanelRepaint eRepaint )
{
	if ( !IsRepaintWatchMatch( pPanel->GetID() ) )
	{
		return;
	}

	const char *pchRepaintStr;
	switch( eRepaint )
	{
	case k_EPanelRepaintFull:
		pchRepaintStr = "full";
		break;
	case k_EPanelRepaintComposition:
		pchRepaintStr = "composition";
		break;
	case k_EPanelRepaintNone:
		pchRepaintStr = "none";
		break;
	default:
		pchRepaintStr = "<unknown>";
		break;
	}
	
	Msg( "%s repaint %s for '%s' (%s)\n", pchOrigin, pchRepaintStr, pPanel->GetID(), pPanel->ClientPtr()->GetPanelType().String() );
	DebuggerBreakIfDebugging();
}
#else
#define IsRepaintWatchMatch( pchID ) false
#define ReportRepaintWatchMatch( pchOrigin, pPanel, eRepaint )
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFastScrollSoundManager::CFastScrollSoundManager()
{
	m_flLastFrameTime = 0.0f;
	m_hFastScrollSound = NULL;
	m_nLastRepeats = 0;

	RegisterForUnhandledEvent( TimeoutFastScrollSound(), this, &CFastScrollSoundManager::OnTimeoutFastScrollSound );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFastScrollSoundManager::~CFastScrollSoundManager()
{
	UnregisterForUnhandledEvent( TimeoutFastScrollSound(), this, &CFastScrollSoundManager::OnTimeoutFastScrollSound );
}


//-----------------------------------------------------------------------------
// Purpose: Timeout fast scroll sound if it's playing
//-----------------------------------------------------------------------------
bool CFastScrollSoundManager::OnTimeoutFastScrollSound()
{
	if( UIEngine()->GetCurrentFrameTime() - m_flLastFrameTime > 0.25f )
	{
		UISoundSystem()->FadeOutAndStopSoundSample( m_hFastScrollSound, 0.4f );
		m_hFastScrollSound = NULL;
		m_flLastFrameTime = 0.0f;
		m_nLastRepeats = 0;
	}
	else if( m_hFastScrollSound )
	{
		// Keep checking for scrolling to stop
		DispatchEventAsync( 0.1f, TimeoutFastScrollSound(), (IUIPanel*)NULL );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Play scroll/focus change sound and or fast scroll sound
//-----------------------------------------------------------------------------
void CFastScrollSoundManager::Play( int nRepeats, float flPan )
{
	if( nRepeats < m_nLastRepeats && m_hFastScrollSound )
	{
		UISoundSystem()->FadeOutAndStopSoundSample( m_hFastScrollSound, 0.2f );
		m_hFastScrollSound = NULL;
		m_flLastFrameTime = 0.0f;
	}

	float flVolume = Lerp( clamp( nRepeats / 10.0f, 0.0f, 1.0f ), 0.85f, 0.0f );

	if( flVolume > 0.0f )
	{
		UISoundSystem()->PlaySound( "focus_change", (IUIPanel*)NULL, k_ESoundType_Effects, flVolume, flPan );
	}

	UIEngine()->PulseActiveControllerHaptic( UIEngine()->GetHapticFeedbackPositionForInteraction(), nRepeats>0?IUIEngine::k_EHapticFeedbackStrength_VeryLow:IUIEngine::k_EHapticFeedbackStrength_Low );

	if( nRepeats > 3 && !m_hFastScrollSound )
	{
		m_hFastScrollSound = UISoundSystem()->PlaySound( "focus_change_fastscroll", (IUIPanel*)NULL, k_ESoundType_Effects, 0.0f, flPan, 0.0f );
		if( !m_hFastScrollSound )
		{
			m_flLastFrameTime = 0.0f;
		}
		else
		{
			UISoundSystem()->VolumeRampSoundSample( m_hFastScrollSound, 0.0f, 0.0f );
			UISoundSystem()->SetSoundSampleVolumePan( m_hFastScrollSound, UISoundSystem()->GetSoundVolume( k_ESoundType_Effects ), flPan );
			UISoundSystem()->VolumeRampSoundSample( m_hFastScrollSound, 0.85f, 0.4f );

			m_flLastFrameTime = UIEngine()->GetCurrentFrameTime();
			DispatchEventAsync( 0.251f, TimeoutFastScrollSound(), (IUIPanel*)NULL );
		}
	}

	m_nLastRepeats = nRepeats;

}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIPanel::CUIPanel()
	: m_style( this )
{
	COMPILE_TIME_ASSERT( sizeof( m_unStyleFlags ) >= sizeof( EStyleFlags ) );
	COMPILE_TIME_ASSERT( sizeof( m_unDisallowedStyleFlags ) >= sizeof( EStyleFlags ) );
	COMPILE_TIME_ASSERT( sizeof( m_unStylesPresentFlags ) >= sizeof( EStylePresentFlags ) );
	COMPILE_TIME_ASSERT( sizeof( m_unPanelLayoutFlags ) >= sizeof( EPanelLayoutFlags ) );
	COMPILE_TIME_ASSERT( sizeof( m_unInputFlags ) >= sizeof( EInputFlags ) );

	m_pOnLayoutEvent = nullptr;
	m_pParent = NULL;
	m_pVecChildrenInHiding = NULL;
	m_unPanelLayoutFlags = k_EPanelLayoutSkipLayoutPositionTransition;
	m_pMapParentsByID = NULL;
	m_pMapParentsByType = NULL;
	m_pMapParentsByClass = NULL;
	m_bApplyingStyles = false;
	m_bNeedOnStylesChanged = false;
	m_bDeletingChildren = false;
	m_pMapProperties = NULL;
	m_pmapPanelEvents = NULL;
	m_bLoaded = false;
	m_bScrolledIntoView = false;
	m_bSendChildScrolledIntoViewEvents = false;
	m_bVisible = true;
	m_iUIPanelIndex = -1;

	m_flTabIndex = k_flTabIndexInvalid;
	m_flSelectionPosX = k_flSelectionPosInvalid;
	m_flSelectionPosY = k_flSelectionPosInvalid;
	m_bSelectionPosHorBoundary = false;
	m_bSelectionPosVerBoundary = false;
	m_bAnalogStickScrollEnable = false;
	m_bKeepScrollToBottomOnResize = false;
	m_flContentWidth = 0.0f;
	m_flContentHeight = 0.0f;
	m_flDesiredLayoutWidth = 0.0f;
	m_flDesiredLayoutHeight = 0.0f;
	m_flLastDesiredWidthFromParent = 0.0f;
	m_flLastDesiredHeightFromParent = 0.0f;
	m_flLastLayoutXFromParent = 0.0f;
	m_flLastLayoutYFromParent = 0.0f;
	m_flLastLayoutWidthFromParent = 0.0f;
	m_flLastLayoutHeightFromParent = 0.0f;
	m_flActualLayoutWidth = 0.0f;
	m_flActualLayoutHeight = 0.0f;
	m_flActualXOffset = FLT_MAX;
	m_flActualYOffset = FLT_MAX;
	m_flLastAbsoluteXOffset = FLT_MAX;
	m_flLastAbsoluteYOffset = FLT_MAX;
	m_vActualUIScale = Vector( -1.0f, -1.0f, -1.0f );
	m_pVerticalScrollBar = nullptr;
	m_pHorizontalScrollBar = nullptr;
	m_pVerticalScrollData = nullptr;
	m_pHorizontalScrollData = nullptr;
	m_unInputFlags = k_EInputPerformHitTest | k_EInputPerformHitTestChildren | k_EInputAlwaysConsumeHoverClicks | k_ECanClearFocusByClicking;
	m_unStyleFlags = k_EStyleFlagNone;
	m_unDisallowedStyleFlags = k_EStyleFlagNone;
	m_unStylesPresentFlags = 0;
	m_unPanelLayoutFlags = 0;
	m_bInvalidateSizeAndPositionOnOpacityChangeDisabled = false;
	m_bLoadedLayoutFile = false;
	m_bLayoutIncludesScripts = false;
	m_bMouseTracking = false;
	m_ePanelMouseCursor = eMouseCursor_Arrow;
	m_eMouseCanActivate = k_EMouseCanActivateUnfocused;
	m_bFocusOnHover = false;
	m_bChildFocusOnHover = false;
	m_bDraggable = false;
	m_bRememberChildFocus = false;
	m_bNeedsIntermediateTexture = false;
	m_bClipAfterTransform = false;
	m_bHiddenChild = false;
	m_bNeedsPaintArea = false;
	m_bStoppedPaintingNotified = false;
	m_bRequireCompositionLayer = false;
	m_bAlwaysCacheCompositionLayer = false;
	m_bForceNoCompositionLayer = false;
	m_bOffscreenCompositionLayer = false;
	m_eFractionalPixelPositions = k_EFractionalPixelPositionsDefault;
	m_pJSContext = NULL;
	m_bScrollParentToFitWhenFocused = true;
	m_bRegisteredForReadyEvents = false;
	m_bReadyForDisplayState = true;
	m_bReadyForDisplaySetOnPanel = true;
	m_cReadyForDisplayChildren = 0;
	m_flRepaintRequiredRate = 1.0f;
	m_unPaintCommandsBytesSize = 0;
	m_rectLastPaint = { 0 };
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIPanel::~CUIPanel()
{
	VPROF_BUDGET( "CUIPanel::CUIPanel", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_pJSContext )
	{
		UIEngineInternal()->RemovePanelForV8Context( this, m_pJSContext );
		m_pJSContext = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Register static panel2d handlers
//-----------------------------------------------------------------------------
void CUIPanel::RegisterEventHandlersOnPanel2DType( CPanoramaSymbol symPanelType )
{
	panorama::UIEngine()->RegisterPanelTypeEventHandler( LoadLayoutFileAsync::symbol, symPanelType, UtlMakeDelegate( (CUIPanel*)NULL, &CUIPanel::EventLoadLayoutFileAsync ).GetAbstractDelegate(), true );
	panorama::UIEngine()->RegisterPanelTypeEventHandler( LoadLayoutFromXMLStringAsync::symbol, symPanelType, UtlMakeDelegate( (CUIPanel*)NULL, &CUIPanel::EventLoadLayoutFromXMLStringAsync ).GetAbstractDelegate(), true );
	panorama::UIEngine()->RegisterPanelTypeEventHandler( LoadLayoutFromBase64XMLStringAsync::symbol, symPanelType, UtlMakeDelegate( (CUIPanel*)NULL, &CUIPanel::EventLoadLayoutFromBase64XMLStringAsync ).GetAbstractDelegate(), true );
	panorama::UIEngine()->RegisterPanelTypeEventHandler( ReadyForDisplay::symbol, symPanelType, UtlMakeDelegate( (CUIPanel*)NULL, &CUIPanel::EventReadyForDisplay ).GetAbstractDelegate(), true );
	panorama::UIEngine()->RegisterPanelTypeEventHandler( UnreadyForDisplay::symbol, symPanelType, UtlMakeDelegate( (CUIPanel*)NULL, &CUIPanel::EventUnreadyForDisplay ).GetAbstractDelegate(), true );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize panel
//-----------------------------------------------------------------------------
void CUIPanel::Initialize( IUIWindow *window, IUIPanel *parent, const char *pchID, uint32 ePanelFlags )
{
	VPROF_BUDGET( "CUIPanel::Initialize", VPROF_BUDGETGROUP_TENFOOT );

	m_pParent = parent;
	m_pWindow = (CTopLevelWindow*)(window);
	if( !m_pWindow )
	{
		CUIPanel *pParent = (CUIPanel*)GetParent();
		Assert( pParent );
		while( pParent )
		{
			if( pParent->m_pWindow )
			{
				m_pWindow = pParent->m_pWindow;
				break;
			}
			pParent = (CUIPanel*)(pParent->m_pParent);
		}
	}
	Assert( m_pWindow );

	SetRepaint( k_EPanelRepaintFull );

	CUIPanel *pUIParent = (CUIPanel *)m_pParent;

	// If we had a parent with auto values for tabindex/selectionpos, we inherit that
	if( pUIParent )
	{
		if( pUIParent->m_flTabIndex == k_flTabIndexAuto )
			m_flTabIndex = k_flTabIndexAuto;
		if( pUIParent->m_flSelectionPosX == k_flSelectionPosAuto )
			m_flSelectionPosX = k_flSelectionPosAuto;
		if( pUIParent->m_flSelectionPosY == k_flSelectionPosAuto )
			m_flSelectionPosY = k_flSelectionPosAuto;
	}

	// NOTE:  You can't add a panel to a window during layout traverse.  If that happens the panel will be in an indeterminate state for the paint pass
	// for it's first frame.  If you are trying to add a panel in response to a change in layout you must dispatch an async event and do it next frame, or
	// something like that.  

	// One exception... scrollbars, which specially lay them selves out correctly during the pass.  Other "non children" panels should do this too... hopefully.
	if ( !s_bInScrollBarConstruction )
		AssertMsg( !m_pWindow->BIsWindowInLayoutPass(), "You are creating a panel in the middle of a layout pass.  Find comments next to this assert for more info." );


	if( m_pParent && (pUIParent->m_unStyleFlags & k_EStyleFlagParentDisabled || pUIParent->m_unStyleFlags & k_EStyleFlagDisabled) )
		m_unStyleFlags = k_EStyleFlagParentDisabled;
	else
		m_unStyleFlags = k_EStyleFlagNone;

	// Call PanelCreated() here so that CUIEngine::GetPanelHandle() works from this point forward
	SetID( pchID );

	// Add to top level visible/invisible list only if we are not a child panel
	if( !pUIParent )
		AddToTopLevelWindow();

	if( pUIParent )
	{
		// if we have a parent, take its layout file by default. If our derived class loads a layout file, it will update this value.		
		m_pLayoutFile = pUIParent->GetCLayoutFile();
		Assert( m_pLayoutFile.Get() );

		// DontAddAsChild is used for scrollbars, mouse scroll regions, etc. Styles will apply to child but they are special cased in paint and layout path and GetChild() wont return them.
		m_bHiddenChild = (ePanelFlags & ePanelFlags_DontAddAsChild);
		if ( !m_bHiddenChild )
		{
			pUIParent->AddChild( this );
		}
		else
		{
			if( !pUIParent->m_pVecChildrenInHiding )
			{
				pUIParent->m_pVecChildrenInHiding = new CUtlVector< IUIPanel * >();
			}

			pUIParent->m_pVecChildrenInHiding->AddToTail( this );
		}
	}

	MarkStylesDirty( false );

	if( !(ePanelFlags&ePanelFlags_DontFireOnLoad) )
		FirePanelLoadedEvent();

	InvalidateSizeAndPosition();
	RecalculateUIScale( false, nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize a cloned panel from ourself
//-----------------------------------------------------------------------------
void CUIPanel::InitClonedPanel( IUIPanel *pIClone )
{
	// interesting members that we do not copy:
	// - m_bVisible
	// - m_pStyle
	// - m_unPanelLayoutFlags
	// - m_bApplyingStyles
	// - m_iUIPanelIndex
	// - m_flTabIndex
	// - m_flSelectionPosX
	// - m_flSelectionPosY
	// - m_unStylesPresentFlags
	// - m_pParent
	// - m_pWindow
	// - m_bLoaded
	// - m_bMouseTracking 

	CUIPanel *pClone = (CUIPanel *)pIClone;

	pClone->m_strID = m_strID;
	pClone->m_strDefaultFocus = m_strDefaultFocus;

	pClone->m_pLayoutFile = m_pLayoutFile;
	pClone->m_pLayoutLoadedFrom = m_pLayoutLoadedFrom;
	pClone->m_bLoadedLayoutFile = m_bLoadedLayoutFile;
	pClone->m_bLayoutIncludesScripts = m_bLayoutIncludesScripts;
	pClone->m_strLayoutSnippet = m_strLayoutSnippet;

	// don't apply styles.. will most likely get reparented. Prevents transitions too.
	pClone->m_vecStyleClasses.CopyArray( m_vecStyleClasses.Base(), m_vecStyleClasses.Count() );

	UILocalize()->CloneDialogVariables( this, pClone );

	// for style flags, only copy disabled and selected
	pClone->m_unStyleFlags = (m_unStyleFlags & (k_EStyleFlagSelected | k_EStyleFlagDisabled));
	pClone->m_unInputFlags = m_unInputFlags;
	pClone->m_symInputNamespace = m_symInputNamespace;
	pClone->m_bDraggable = true;

	if( m_pmapPanelEvents )
	{
		pClone->m_pmapPanelEvents = new CUtlMap < CPanoramaSymbol, PanelEvent_t, short, CDefLess< CPanoramaSymbol > >;
		pClone->m_pmapPanelEvents->EnsureCapacity( m_pmapPanelEvents->Count() );
		FOR_EACH_MAP_FAST( *m_pmapPanelEvents, iMap )
		{

			PanelEvent_t eventsToCopy, copyOfEvents;
			eventsToCopy = m_pmapPanelEvents->Element( iMap );

			if( eventsToCopy.eType == PanelEvent_t::k_EEventType_UIEventArray && eventsToCopy.data.pVecIUIEvent )
			{
				copyOfEvents.eType = PanelEvent_t::k_EEventType_UIEventArray;
				copyOfEvents.data.pVecIUIEvent = new VecUIEvents_t();
				copyOfEvents.data.pVecIUIEvent->EnsureCapacity( eventsToCopy.data.pVecIUIEvent->Count() );
				FOR_EACH_VEC( *(eventsToCopy.data.pVecIUIEvent), iVec )
				{
					copyOfEvents.data.pVecIUIEvent->AddToTail( eventsToCopy.data.pVecIUIEvent->Element( iVec )->Copy() );
				}
			}
			else if( eventsToCopy.eType == PanelEvent_t::k_EEventType_JSScript && eventsToCopy.data.pJSScript )
			{
				copyOfEvents.data.pJSScript = new v8::Persistent<v8::Script>();

				copyOfEvents.eType = PanelEvent_t::k_EEventType_JSScript;
				copyOfEvents.data.pJSScript->Reset( UIEngine()->GetV8Isolate(), *eventsToCopy.data.pJSScript );
				copyOfEvents.pJSContext = eventsToCopy.pJSContext;
			}
			else if ( eventsToCopy.eType == PanelEvent_t::k_EEventType_JSFunction && eventsToCopy.data.pJSFunction )
			{
				copyOfEvents.data.pJSFunction = new v8::Persistent<v8::Function>();

				copyOfEvents.eType = PanelEvent_t::k_EEventType_JSFunction;
				copyOfEvents.data.pJSFunction->Reset( UIEngine()->GetV8Isolate(), *eventsToCopy.data.pJSFunction );
			}

			copyOfEvents.pJSContext = eventsToCopy.pJSContext;

			pClone->m_pmapPanelEvents->Insert( m_pmapPanelEvents->Key( iMap ), copyOfEvents );
		}
	}
	else
	{
		pClone->m_pmapPanelEvents = NULL;
	}

	if ( m_pMapProperties )
	{
		pClone->m_pMapProperties = new CUtlMap < CPanoramaSymbol, CUtlString, int, CDefLess< CPanoramaSymbol > >( );
		pClone->m_pMapProperties->EnsureCapacity( m_pMapProperties->Count( ) );
		FOR_EACH_MAP_FAST( *m_pMapProperties, iMap )
		{

			 pClone->m_pMapProperties->InsertOrReplace( m_pMapProperties->Key( iMap ), m_pMapProperties->Element( iMap ) );
		}
	}
	else
	{
		pClone->m_pMapProperties = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Shutdown panel, should only happen right before actual deletion
//-----------------------------------------------------------------------------
void CUIPanel::Shutdown()
{
	VPROF_BUDGET( "CUIPanel::Shutdown", VPROF_BUDGETGROUP_TENFOOT );

	// we want to make sure we aren't hovering (so we don't get things like stray tooltips):
	RemoveStyleFlag( k_EStyleFlagHover );

	AccessRenderEngine()->OnPanelDeleted( this );

	// if required parent focus previously, remove old registration
	if( m_eMouseCanActivate == k_EMouseCanActivateIfParentFocused )
		UIEngine()->UnregisterMouseCanActivateParent( this );

	{
		VPROF_BUDGET( "CUIPanel::Shutdown - events", VPROF_BUDGETGROUP_TENFOOT );
		UIEngine()->UnregisterEventHandlersForPanel( this );
	}

	bool bHadFocus = BHasKeyFocus();

	// Cache off tabindex/selectionpos if we had focus
	float flTabIndex = 0.0f;
	float flSelectionPosX = 0.0f;
	float flSelectionPosY = 0.0f;
	if( bHadFocus )
	{
		flTabIndex = GetTabIndex();
		flSelectionPosX = GetSelectionPositionX();
		flSelectionPosY = GetSelectionPositionY();
	}

	// delete children before removing ourselves from parent, as some things during destruction (like clearing descendant focus flag) need the parent pointer
	RemoveAndDeleteChildren();

	// clean up any panels in javascript context... basically treat them as children
	if ( !m_pJSContext && (m_bLoadedLayoutFile || GetParent() == NULL) )
		UIEngineInternal()->DeleteAssociatedPanelsForV8Context( this );		

	// delete children in hiding now too... 
	if( m_pVecChildrenInHiding )
	{
		m_bDeletingChildren = true;

		FOR_EACH_VEC_BACK( (*m_pVecChildrenInHiding), i )
		{
			IUIPanel *pChild = m_pVecChildrenInHiding->Element(i);
			pChild->ClientPtr()->OnDeletePanel();
		}

		m_bDeletingChildren = false;

		delete m_pVecChildrenInHiding;
		m_pVecChildrenInHiding = NULL;
	}

	// track old parent
	IUIPanel *pOldParent = m_pParent;

	// If we had focus, then tell parent to set to previous in tab order, don't want to leave nothing focused
	if( bHadFocus && pOldParent )
	{
		// Update focus, but do not switch contexts since this is not in response to direct input!
		IUIPanel *pLastContext = GetParentWindow()->UIWindowInput()->GetInputFocusContext();

		// Try to set previous, but without wrap, if we would have had to wrap we are probably the first in a list of panels, and we should set next rather
		// than previous, so for instance removing the first item in a list doesn't jump you all the way to the bottom of the list
		if( !pOldParent->SetFocusToNextPanel( 0, k_EPrevInTabOrder, false, flTabIndex, flSelectionPosX, flSelectionPosY, flSelectionPosX, flSelectionPosY ) )
			pOldParent->SetFocusToNextPanel( 0, k_ENextInTabOrder, true, flTabIndex, flSelectionPosX, flSelectionPosY, flSelectionPosX, flSelectionPosY );

		GetParentWindow()->UIWindowInput()->SetInputFocusContext( pLastContext );
	}

	// Remove from old parent
	if( m_pParent )
	{
		// Don't tell the parent if it's the one actively deleting us, as it will already remove, and we'll cause
		// potentially N^2 badness if we tell it as well.
		if( !((CUIPanel*)m_pParent)->m_bDeletingChildren )
			m_pParent->RemoveChild( this );
	}
	else
		RemoveFromTopLevelWindow();

	Assert( m_iUIPanelIndex == -1 );
	Assert( m_cReadyForDisplayChildren == 0 );

	// If we still have focus (shouldn't, but we could if we were the only child allowing input in the tree), 
	// then try again now that we are removed,  something really random will get focus, but at least someone 
	// will have it and we won't orphan focus
	bHadFocus = BHasKeyFocus();
	if( bHadFocus && pOldParent )
	{
		// Update focus, but do not switch contexts since this is not in response to direct input!
		IUIPanel *pLastContext = GetParentWindow()->UIWindowInput()->GetInputFocusContext();
		pOldParent->SetFocusToNextPanel( 0, k_EPrevInTabOrder, true, flTabIndex, flSelectionPosX, flSelectionPosY, flSelectionPosX, flSelectionPosY );
		GetParentWindow()->UIWindowInput()->SetInputFocusContext( pLastContext );
	}

	m_pParent = NULL;
	ClearPanelEvents();

	if( m_bMouseTracking )
		GetParentWindow()->UIWindowInput()->RemoveMouseTrackingPanel( this );

	// Delete any properties
	m_style.Clear();
	FOR_EACH_VEC( m_vecEventHandlers, i )
	{
		if( m_vecEventHandlers[i].pjsHandler )
		{
			m_vecEventHandlers[i].pjsHandler->Reset();
			delete  m_vecEventHandlers[i].pjsHandler;
		}
	}

	SAFE_DELETE( m_pMapProperties );
	SAFE_DELETE( m_pMapParentsByID );
	SAFE_DELETE( m_pMapParentsByType );
	SAFE_DELETE( m_pMapParentsByClass );

	{
		VPROF_BUDGET( "CUIPanel::Shutdown - notify", VPROF_BUDGETGROUP_TENFOOT );
		GetParentWindow()->UIWindowInput()->PanelDeleted( this, pOldParent );
	}
}


//-----------------------------------------------------------------------------
// Purpose: send the panel is loaded event through the system, virtual so you can change behavior
//			in derived classes
//-----------------------------------------------------------------------------
void CUIPanel::FirePanelLoadedEvent()
{
	m_bLoaded = true;
	DispatchEventAsync( PanelLoaded(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Loads a layout file for this panel
//-----------------------------------------------------------------------------
bool CUIPanel::BLoadLayout( const char *pchFile, bool bOverrideExisting, bool bPartialLayout )
{
	if ( CommandLine()->CheckParm( "-m5diagnostics" ) ) fprintf( stderr, "M5 layout load: %s, panel %s, already loaded %d\n", pchFile, GetID(), m_bLoadedLayoutFile );
	VPROF_BUDGET( "CUIPanel::BLoadLayout", VPROF_BUDGETGROUP_TENFOOT );

	V8_CtxDbgMsg( "BLoadLayout: Panel %x (%s), loading layout file %s\n", this, GetID(), pchFile );

	// only load one layout file
 	if( m_bLoadedLayoutFile && !bOverrideExisting )
	{
		AssertMsg( false, "Already loaded a layout file for this panel" );
		return false;
	}

	m_bLoadedLayoutFile = false;
	m_bLayoutIncludesScripts = false;
	m_strLayoutSnippet.Clear();
	RemoveAndDeleteChildren();
	UIEngineInternal()->DeleteScriptContext( this );

	LayoutFilePtr_t pLayoutFile = LayoutManager().GetCLayoutFile( pchFile, bPartialLayout );
	if( !pLayoutFile )
	{
		AddStyleFlag( k_EStyleFlagLayoutLoadFailed );
		fprintf( stderr, "Panorama: couldn't get layout file %s\n", pchFile );
		AssertMsg1( false, "Couldn't get layout file %s", pchFile );
		return false;
	}

	LayoutFilePtr_t pPriorLayoutFile = m_pLayoutFile;

	if( !bPartialLayout )
	{
		m_bLoadedLayoutFile = true;
		m_pLayoutFile = pLayoutFile;
	}

	if( !BApplyLayoutFile( pLayoutFile, NULL, false ) )
	{
		m_bLoadedLayoutFile = false;
		m_bLayoutIncludesScripts = false;
		m_pLayoutFile = pPriorLayoutFile;
		MarkStylesDirty( true );

		AddStyleFlag( k_EStyleFlagLayoutLoadFailed );
		fprintf( stderr, "Panorama: couldn't apply layout file %s\n", pchFile );
		AssertMsg1( false, "Couldn't apply layout file %s", pchFile );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Considers a layout load failure a fatal error.
//-----------------------------------------------------------------------------
void CUIPanel::RequireLoadLayout( const char *pchFile, bool bOverrideExisting, bool bPartialLayout )
{
	if ( !BLoadLayout( pchFile, bOverrideExisting, bPartialLayout ) )
	{
#if ( defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER) )
		// Let the filesystem know that something is wrong with the local content.
		if ( g_pFullFileSystem )
		{
			g_pFullFileSystem->MarkContentCorrupt( false );
		}
#endif
		
		RequiredCallFailed( "Unable to load layout file '%s'.  This may indicate a problem with your local install files and validating your install through the Steam client may resolve the issue.\n", pchFile );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads a layout xml string for this panel
//-----------------------------------------------------------------------------
bool CUIPanel::BLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting, bool bPartialLayout )
{
	// only load one layout file
	if( m_bLoadedLayoutFile && !bOverrideExisting )
	{
		AssertMsg( false, "Already loaded a layout file for this panel" );
		return false;
	}

	m_bLoadedLayoutFile = false;
	m_bLayoutIncludesScripts = false;
	m_strLayoutSnippet.Clear();
	RemoveAndDeleteChildren();
	UIEngineInternal()->DeleteScriptContext( this );

	LayoutFilePtr_t pLayoutFile = LayoutManager().GetLayoutFileFromString( pchXMLString, bPartialLayout );
	if( !pLayoutFile )
	{
		AddStyleFlag( k_EStyleFlagLayoutLoadFailed );
		AssertMsg( false, "Couldn't get layout file for string" );
		return false;
	}

	LayoutFilePtr_t pPriorLayoutFile = m_pLayoutFile;

	if( !bPartialLayout )
	{
		m_bLoadedLayoutFile = true;
		m_pLayoutFile = pLayoutFile;
	}

	if( !BApplyLayoutFile( pLayoutFile, NULL, false ) )
	{
		m_bLoadedLayoutFile = false;
		m_bLayoutIncludesScripts = false;
		m_pLayoutFile = pPriorLayoutFile;
		MarkStylesDirty( true );

		AddStyleFlag( k_EStyleFlagLayoutLoadFailed );
		AssertMsg( false, "Couldn't apply layout file from string" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Considers a layout load failure a fatal error.
//-----------------------------------------------------------------------------
void CUIPanel::RequireLoadLayoutFromString( const char *pchXMLString, bool bOverrideExisting, bool bPartialLayout )
{
	if ( !BLoadLayoutFromString( pchXMLString, bOverrideExisting, bPartialLayout ) )
	{
		RequiredCallFailed( "Unable to load layout from string\n" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads a snippet from the panel's current layout file.
//-----------------------------------------------------------------------------
bool CUIPanel::BLoadLayoutSnippet( const char *pchSnippetName )
{
	LayoutFilePtr_t pLayoutFile = GetCLayoutFile();
	if ( !pLayoutFile )
	{
		AssertMsg( false, "Can't load a snippet if no layout file is loaded." );
		return false;
	}

	if ( !BApplyLayoutSnippet( pLayoutFile, pchSnippetName, NULL ) )
	{
		AssertMsg2( false, "Unable to load snippet '%s' from layout file '%s'", pchSnippetName, pLayoutFile->GetLayoutFileSymbol().String() );
		return false;
	}

	m_strLayoutSnippet = pchSnippetName;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Loads a snippet and considers failure a fatal error.
//-----------------------------------------------------------------------------
void CUIPanel::RequireLoadLayoutSnippet( const char *pchSnippetName )
{
	if ( !BLoadLayoutSnippet( pchSnippetName ) )
	{
		RequiredCallFailed( "Unable to load snippet %s\n", pchSnippetName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if a snippet is available by the given name
//-----------------------------------------------------------------------------
bool CUIPanel::BHasLayoutSnippet( const char *pchSnippetName )
{
	if ( V_isempty( pchSnippetName ) )
		return false;

	LayoutFilePtr_t pLayoutFile = GetCLayoutFile();
	if ( !pLayoutFile )
		return false;

	return pLayoutFile->GetSnippet( pchSnippetName ) != nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: loads the layout file for this panel, asynchronously supporting remote http:// paths for css within
//-----------------------------------------------------------------------------
void CUIPanel::LoadLayoutFromStringAsync( const char *pchXMLString, bool bOverrideExisting, bool bPartialLayout )
{
	// only load one layout file
	if( (m_bLoadedLayoutFile || (GetStyleFlags() & k_EStyleFlagLayoutLoading)) && !bOverrideExisting )
	{
		AssertMsg( false, "Already loaded (or loading) a layout file for this panel" );
		OnLoadLayoutAsyncCompleteInternal( NULL, k_ELoadLayoutAsyncDetailsNone, bPartialLayout );
		return;
	}

	// We'll get called back in OnGetLayoutFileAsyncComplete() after this
	AddStyleFlag( k_EStyleFlagLayoutLoading );
	LayoutManager().GetLayoutFileFromStringAsync( pchXMLString, this, bPartialLayout );
}


//-----------------------------------------------------------------------------
// Purpose: sets  loads the layout file for this panel, asynchronously supporting remote http:// paths
//-----------------------------------------------------------------------------
void CUIPanel::LoadLayoutAsync( const char *pchFile, bool bOverrideExisting, bool bPartial )
{
	// only load one layout file
	if( (m_bLoadedLayoutFile || (GetStyleFlags() & k_EStyleFlagLayoutLoading)) && !bOverrideExisting )
	{
		AssertMsg( false, "Already loaded (or loading) a layout file for this panel" );
		OnLoadLayoutAsyncCompleteInternal( NULL, k_ELoadLayoutAsyncDetailsNone, bPartial );
		return;
	}

	// We'll get called back in OnGetLayoutFileAsyncComplete() after this
	AddStyleFlag( k_EStyleFlagLayoutLoading );
	LayoutManager().GetLayoutFileAsync( pchFile, this, bPartial );
}

//-----------------------------------------------------------------------------
// Purpose: Creates & appends child panels from a string. String XML should only include XML of children, not this panel as a wrapper
//-----------------------------------------------------------------------------
bool CUIPanel::BCreateChildren( const char *pchXML )
{
	PanelDescription_t *pPanelDescription = NULL;
	if ( !LayoutManager().BParsePartialLayout( &pPanelDescription, pchXML ) )
		return false;

	CUtlVector< PanelEventsToParse_t > vecPanelEventsToParse;
	if ( !BCreateChildrenFromDescription( m_pLayoutFile->GetLayoutFileSymbol(), pPanelDescription, &vecPanelEventsToParse, NULL ) )
		return false;

	// now that we have created all children in this layout file, parse panel events. Delaying allows for us to specify IDs in the panel events
	// for panels before they are defined in the layout file
	FOR_EACH_VEC( vecPanelEventsToParse, i )
	{
		PanelEventsToParse_t &panelEvent = vecPanelEventsToParse[i];
		if ( !panelEvent.m_pPanel->BParsePanelEvent( panelEvent.m_symProperty, panelEvent.m_pchEvent, GetJavaScriptContextParent() ) )
		{
			AssertMsg2( false, "**** Failed to parse panel event when creating children from string, event=%s: %s\n", panelEvent.m_symProperty.String(), panelEvent.m_pchEvent );
			return false;
		}
	}

	SAFE_DELETE( pPanelDescription );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: unload the layout file for this panel, destroying all children
//-----------------------------------------------------------------------------
void CUIPanel::UnloadLayout( void )
{
	if ( !m_bLoadedLayoutFile )
		return;
	RemoveAndDeleteChildren();
	m_bLoadedLayoutFile = false;
	m_bLayoutIncludesScripts = false;
}


//-----------------------------------------------------------------------------
// Purpose: Play focus change sound accounting for fast scroll volume fade effects, etc
//-----------------------------------------------------------------------------
void CUIPanel::PlayFocusChangeSound( int nRepeats, float flPan )
{
	((CTopLevelWindow *)GetParentWindow())->AccessFastScrollSoundMgr()->Play( nRepeats, 0.8f );
}


//-----------------------------------------------------------------------------
// Purpose: Invalidates painting and tells the panel it must repaint next frame
//-----------------------------------------------------------------------------
void CUIPanel::SetRepaint( EPanelRepaint eRepaintNeeded )
{
	VPROF_BUDGET_DETAILED( "CUIPanel::SetRepaint()", VPROF_BUDGETGROUP_TENFOOT );

	AssertMsg( eRepaintNeeded != k_EPanelRepaintNone, "Shouldn't set repaint to none, we'll do that automatically when ready internally" );

	// If we already flagged as needing full repaint, don't allow changing lower
	if( m_eRepaint == k_EPanelRepaintFull )
		return;

	// Same if we are already to repaint composition and we are trying to flag none, shouldn't happen
	if( m_eRepaint == k_EPanelRepaintComposition && eRepaintNeeded == k_EPanelRepaintNone )
		return;

	// Msg( "Repaint of %s %s type %u\n", GetID(), ClientPtr()->GetPanelType().String(), eRepaintNeeded );
	m_eRepaint = eRepaintNeeded;

	ReportRepaintWatchMatch( "SetRepaint", this, eRepaintNeeded );

	SetRepaintOnAncestors();
}


//-----------------------------------------------------------------------------
// Purpose: Set repaint state on all ancestors if the current panel is flagged
// 			as needing a full/composition repaint.
//-----------------------------------------------------------------------------
void CUIPanel::SetRepaintOnAncestors()
{
	if( m_eRepaint != k_EPanelRepaintNone )
	{
		CUIPanel *pChild = this;
		CUIPanel *pParent = (CUIPanel*)GetParent();
		while( pParent )
		{
			if( pParent->m_eRepaint == k_EPanelRepaintFull )
				break;

			// Don't keep going up the hierarchy if a child isn't going to draw currently, this call will happen
			// again when the child needs to draw again, so we'll do the work then.
			//
			// However, and THIS IS IMPORTANT, we must always tell our own parent to repaint if we ourselves are invisible
			// because we may have just been set to invisible/zero opacity and our parent may have us cached inside their
			// composition layer.  That layer must get updated.
			PanoramaRect_t rectChildPaint;
			PanoramaRect_t rectPaint;
			pParent->GetPaintArea( &rectPaint );
			if ( pChild == this || pParent->BShouldDrawChild( &rectChildPaint, pChild, rectPaint ) )
			{
				// Msg( "Repaint of parent %s %s type %u\n", pParent->GetID(), pParent->ClientPtr()->GetPanelType().String(), eRepaintNeeded );
				pParent->m_eRepaint = k_EPanelRepaintFull;
				ReportRepaintWatchMatch( "SetRepaint up parent chain", pParent, pParent->m_eRepaint );
			}
			else
				break;

			pChild = pParent;
			pParent = (CUIPanel*)pParent->GetParent();
		}
	}
}

void CUIPanel::SetTopLevelWindow( CTopLevelWindow *pWindow )
{
	m_pWindow = pWindow;
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel* pChild = (CUIPanel*)m_vecChildren[ i ];
		pChild->SetTopLevelWindow( pWindow );
	}
	if ( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( *m_pVecChildrenInHiding, i )
		{
			CUIPanel *pChild = ( CUIPanel* )( *m_pVecChildrenInHiding )[ i ];
			pChild->SetTopLevelWindow( pWindow );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets panel's parent and adds to parent's children list
//-----------------------------------------------------------------------------
void CUIPanel::SetParent( IUIPanel *pParent )
{
	if( pParent == m_pParent )
		return;

	// changing parents will change our layout file so also clear context
	if ( m_pJSContext )
	{
		UIEngineInternal()->RemovePanelForV8Context( this, m_pJSContext );
		m_pJSContext = NULL;
	}

	if( m_pParent )
		m_pParent->RemoveChild( this );
	else
		RemoveFromTopLevelWindow();

	ClearParentLookupMapsTraverse();

	m_pParent = pParent;
	if( pParent )
	{
		pParent->AddChild( this );
		SetTopLevelWindow( ( CTopLevelWindow* )pParent->GetParentWindow() );

		if( !m_bLoadedLayoutFile )
			SetLayoutFileTraverse( pParent->GetLayoutFile() );
	}
	else
	{
		AddToTopLevelWindow();
	}

	//
	// Setting these so BHasBeenLayedOut returns false after parent change
	//
	m_flActualXOffset = m_flActualYOffset = FLT_MAX;
	InvalidateSizeAndPosition();
	MarkStylesDirty( true );
	RecalculateUIScale( false, nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: Sets layout file for this panel and all children recursively
//-----------------------------------------------------------------------------
void CUIPanel::SetLayoutFileTraverse( CPanoramaSymbol symLayoutFile )
{
	SetLayoutFile( symLayoutFile );
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if( !pChild->m_bLoadedLayoutFile )
			pChild->SetLayoutFileTraverse( symLayoutFile );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets layout file for this panel
//-----------------------------------------------------------------------------
void CUIPanel::SetLayoutFile( CPanoramaSymbol symLayoutFile )
{
	Assert( !m_bLoadedLayoutFile );
	m_pLayoutFile = UIEngineInternal()->UILayoutManagerInternal()->GetCLayoutFile( symLayoutFile );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUIPanel::SetLayoutLoadedFromParent( panorama::IUIPanel *pParent )
{
	CUIPanel *pCParent = (CUIPanel*)pParent;
	Assert( pCParent != NULL );
	m_pLayoutLoadedFrom = pCParent->m_pLayoutFile;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the layout file and js context for a panel to that of the specified panel. For JS and CSS lookup, panel will act like it is
//			in that panel's layout file
//-----------------------------------------------------------------------------
void CUIPanel::SetPanelIntoContext( panorama::IUIPanel *pParent )
{
	Assert( ((CUIPanel*)pParent)->m_bLoadedLayoutFile );
	SetLayoutFile( pParent->GetLayoutFile() );
	SetLayoutLoadedFromParent( pParent );
	m_pJSContext = pParent;
	UIEngineInternal()->AddPanelForV8Context( this, pParent );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CPanoramaSymbol CUIPanel::GetLayoutFile() const
{
	return ( m_pLayoutFile.Get() ? m_pLayoutFile->GetLayoutFileSymbol() : CPanoramaSymbol() );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CPanoramaSymbol CUIPanel::GetLayoutFileLoadedFrom() const
{
	return ( m_pLayoutLoadedFrom.Get() ? m_pLayoutLoadedFrom->GetLayoutFileSymbol() : CPanoramaSymbol() );
}


//-----------------------------------------------------------------------------
// Purpose: Returns path override for JS permission check
//-----------------------------------------------------------------------------
const char *CUIPanel::GetLayoutFilePathForJSCheck() const
{
	if ( !m_strLayoutFilePathForJSCheck.IsEmpty() )
		return m_strLayoutFilePathForJSCheck.String();

	return GetLayoutFile().String();
}


//-----------------------------------------------------------------------------
// Purpose: Sets path override for JS permission check
//-----------------------------------------------------------------------------
void CUIPanel::SetLayoutFilePathForJSCheck( const char *pchPath )
{
	m_strLayoutFilePathForJSCheck = pchPath;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CUIPanel::GetLayoutFileReloadCount() const
{
	return ( m_pLayoutFile.Get() ? m_pLayoutFile->GetReloadCount() : -1 );
}


//-----------------------------------------------------------------------------
// Purpose: Check if panel coords are going to be drawn, including scroll region transitions
//-----------------------------------------------------------------------------
void CUIPanel::GetChildAreaToPaint( PanoramaRect_t *pChildRegionToPaint, const PanoramaRect_t &rectPaintArea, CUIPanel *pChild )
{
	CUILength x, y, z;
	pChild->AccessStyle()->GetInterpolatedPosition( x, y, z, false );
	float flX = x.GetValueAsLength( m_flActualLayoutWidth ) + pChild->GetRawActualXOffset();
	float flY = y.GetValueAsLength( m_flActualLayoutHeight ) + pChild->GetRawActualYOffset();

	// if child has any transforms beyond x/ytranslate, just paint the whole thing and never clip it
	float flXTranslate = 0.0f;
	float flYTranslate = 0.0f;
	bool bTransformSet = false;
	if ( pChild->m_unStylesPresentFlags & k_EStylePresentTransformMatrix )
	{
		if ( !pChild->AccessStyle()->BHasTransition( CStylePropertyTransform3D::symbol ) )
		{
			VMatrix matrix = pChild->AccessStyle()->GetTransform3DMatrix();
			if ( IsVMatrix2DTranslateOnly( matrix ) )
			{
				flXTranslate += matrix[0][3];
				flYTranslate += matrix[1][3];
			}
			else
			{
				bTransformSet = true;
			}
		}
		else
		{
			bTransformSet = true;
		}
	}

	// Check other transform like things that should make us paint the whole child
	if ( !bTransformSet )
		bTransformSet = (pChild->m_unStylesPresentFlags & (k_EStylePresentScale2DCentered | k_EStylePresentRotate2DCentered)) != 0;

	float flChildLayoutWidth = pChild->GetActualLayoutWidth();
	float flChildLayoutHeight = pChild->GetActualLayoutHeight();

	if ( bTransformSet || z.GetValueAsLength( 1000 ) != 0.0f )
	{
		pChildRegionToPaint->flX = 0.0f;
		pChildRegionToPaint->flY = 0.0f;
		pChildRegionToPaint->flWidth = flChildLayoutWidth;
		pChildRegionToPaint->flHeight = flChildLayoutHeight;
		return;
	}

	// set x,y in child coords
	pChildRegionToPaint->flX = Clamp( rectPaintArea.flX - (flX+flXTranslate), 0.0f, flChildLayoutWidth );
	pChildRegionToPaint->flY = Clamp( rectPaintArea.flY - (flY+flYTranslate), 0.0f, flChildLayoutHeight );

	// calculate rendered width
	float flDrawStart = Max( flX + flXTranslate, rectPaintArea.flX );
	float flDrawEnd = Min( flX + flXTranslate + flChildLayoutWidth, rectPaintArea.flX + rectPaintArea.flWidth );
	pChildRegionToPaint->flWidth = Max( flDrawEnd - flDrawStart, 0.0f );

	// calculate rendered height
	flDrawStart = Max( flY + flYTranslate, rectPaintArea.flY );
	flDrawEnd = Min( flY + flYTranslate + flChildLayoutHeight, rectPaintArea.flY + rectPaintArea.flHeight );
	pChildRegionToPaint->flHeight = Max( flDrawEnd - flDrawStart, 0.0f );

	// Prevent clipping to parent if overflow noclip on parent
	EOverflowValue eHorizontal, eVertical;
	AccessStyle()->GetOverflow( eHorizontal, eVertical );
	if ( eHorizontal == k_EOverflowNoClip )
	{
		pChildRegionToPaint->flX = 0.0f;
		pChildRegionToPaint->flWidth = flChildLayoutWidth;
	}

	if ( eVertical == k_EOverflowNoClip )
	{
		pChildRegionToPaint->flY = 0.0f;
		pChildRegionToPaint->flHeight = flChildLayoutHeight;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns paintable content area
//-----------------------------------------------------------------------------
void CUIPanel::GetPaintArea( PanoramaRect_t *pPaintArea )
{
	UpdateScrollOffsetX();
	UpdateScrollOffsetY();

	// include scrolling transitions when painting. Returned area will be area covered from start to end of scroll

	// horizontal
	float flDrawMin = 0.0f;
	float flDrawMax = 0.0f;
	if ( m_pHorizontalScrollData )
	{
		flDrawMin -= m_pHorizontalScrollData->m_flOffset;
		flDrawMax -= m_pHorizontalScrollData->m_flOffset;
		if ( m_pHorizontalScrollData->m_flOffsetTarget != FLT_MAX )
		{
			flDrawMin = Min( flDrawMin, 0 - m_pHorizontalScrollData->m_flOffsetTarget );
			flDrawMax = Max( flDrawMax, 0 - m_pHorizontalScrollData->m_flOffsetTarget );
		}
	}

	pPaintArea->flX = flDrawMin;
	pPaintArea->flWidth = (flDrawMax - flDrawMin) + m_flActualLayoutWidth;

	// vertical
	flDrawMin = 0.0f;
	flDrawMax = 0.0f;

	if ( m_pVerticalScrollData )
	{
		flDrawMin = 0 - m_pVerticalScrollData->m_flOffset;
		flDrawMax = 0 - m_pVerticalScrollData->m_flOffset;
		if ( m_pVerticalScrollData->m_flOffsetTarget != FLT_MAX )
		{
			flDrawMin = Min( flDrawMin, 0 - m_pVerticalScrollData->m_flOffsetTarget );
			flDrawMax = Max( flDrawMax, 0 - m_pVerticalScrollData->m_flOffsetTarget );
		}
	}

	pPaintArea->flY = flDrawMin;
	pPaintArea->flHeight = (flDrawMax - flDrawMin) + m_flActualLayoutHeight;
}



//-----------------------------------------------------------------------------
// Purpose: Should the child panel currently get drawn?
//-----------------------------------------------------------------------------
bool CUIPanel::BShouldDrawChild( PanoramaRect_t *pChildRegionToPaint, IUIPanel *pIChild, const PanoramaRect_t &rectPaintArea )
{
	*pChildRegionToPaint = { 0 };

	// Can't apply styles yet, assume we should draw for now
	if( s_bInApplyLayoutFile )
		return false;

	CUIPanel *pChild = (CUIPanel*)pIChild;

	// Skip traversing to draw children who are totally invisible or totally outside the bounds of our draw region
	if( !pChild->BIsVisible() )
	{
		pChild->EnableBackgroundMovies( false );
		return false;
	}

	// totally transparent is as good as not drawing/visible
	CPanelStyle *pStyle = ((CUIPanel*)pChild)->AccessStyle();
	if( pStyle->BIsTransparentWithNoOpacityTransition() )
	{
		pChild->EnableBackgroundMovies( false );
		return false;
	}
	
	// need to determine how much of child we will draw
	GetChildAreaToPaint( pChildRegionToPaint, rectPaintArea, pChild );
	bool bChildVisible = (pChildRegionToPaint->flWidth > 0.0001f && pChildRegionToPaint->flHeight > 0.0001f );
	if ( !bChildVisible )
	{
		//Msg( "Child %s is outside parent\n", pChild->m_strID.String() );

		// Fully outside parent view... no need to render, this is our fast lazy path, animation thread will
		// do much more complete clipping including transforms/interpolated values, so don't worry about
		// making this check a lot better as we won't really render stuff that passes wrongly.
		pChild->EnableBackgroundMovies( false );
		return false;
	}

	pChild->EnableBackgroundMovies( true );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Stop the animation (in animation thread) of style property until a frame update comes in 
// from layout thread and return the actual final animation/interpolation time so we can match up to 
// it on layout thread
//-----------------------------------------------------------------------------
float CUIPanel::StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( uint32 hSymbol )
{
	CPanelPtr< IUIPanel > ptr( this );
	return AccessRenderEngine()->StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( ptr.GetHandleAsUInt64(), hSymbol );
}

//-----------------------------------------------------------------------------
// Purpose: Creates copy of named keyframes, for modification by JS
//-----------------------------------------------------------------------------
CJSKeyframesObject *CUIPanel::JSCreateCopyOfCSSKeyframes( const char *pchKeyframesName )
{
	CJSKeyframesObject *pObj = new CJSKeyframesObject( GetCLayoutFile(), pchKeyframesName );
	return pObj;
}

//-----------------------------------------------------------------------------
// Purpose: Delete keyframes object
//-----------------------------------------------------------------------------
void CUIPanel::JSDeleteKeyframes( CJSKeyframesObject *pKeyframes )
{
	delete pKeyframes;
}

//-----------------------------------------------------------------------------
// Purpose: Updates current animation keyframes with those in CJSKeyframesObject
// and restarts the animation
//-----------------------------------------------------------------------------
void CUIPanel::JSUpdateCurrentAnimationKeyframes( CJSKeyframesObject *pKeyframes )
{
	VecKeyFrames_t *pNewKeyframes = pKeyframes->GetKeyframes();
	if ( pNewKeyframes == nullptr )
	{
		return;
	}

	CPanelStyle *pPanelStyle = (CPanelStyle*)AccessIUIStyle();

	CStyleProperty *pProperty;
	pPanelStyle->FindPropertyInfo( CStylePropertyAnimationProperties::symbol, &pProperty, nullptr, nullptr );
	CStylePropertyAnimationProperties *pStyleAnimProp = (CStylePropertyAnimationProperties*)pProperty;

	if ( pStyleAnimProp && pStyleAnimProp->m_vecAnimationProperties.Count() )
	{
		pPanelStyle->SetSingleAnimation( pStyleAnimProp->m_vecAnimationProperties[0], *(pKeyframes->GetKeyframes()) );
		AfterStylesApplied( true, k_EStyleRepaintFull, true, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates controls and sets
//-----------------------------------------------------------------------------
bool CUIPanel::BApplyLayoutFile( LayoutFilePtr_t pLayoutFile, CUtlVector< IUIPanel * > *pvecExistingPanels, bool bIsReload )
{
	VPROF_BUDGET( "CUIPanel::BApplyLayoutFile", VPROF_BUDGETGROUP_STEAMUI );
	PanelDescription_t *pPanelDescription = pLayoutFile->GetPanelDescription();
	Assert( pPanelDescription );
	if( !pPanelDescription )
		return false;

	CUIPanel::s_bInApplyLayoutFile = true;

	// doesn't ensure that root of layout file matches this panel type

	// apply our settings and create children if necessary
	CUtlVector< PanelEventsToParse_t > vecPanelEventsToParse;
	if( !BApplyPanelDescription( pLayoutFile->GetLayoutFileSymbol(), pPanelDescription, &vecPanelEventsToParse, pvecExistingPanels ) )
	{
		CUIPanel::s_bInApplyLayoutFile = false;
		return false;
	}

	// Ok to apply styles at this point as all children are created, JS or events in onload may cause that to happen
	CUIPanel::s_bInApplyLayoutFile = false;

	// Now that children are created execute any javascript included, first reset any existing JS context
	UIEngineInternal()->DeleteScriptContext( this );
	const CUtlVector< JSInclude_t > &vecScripts = pLayoutFile->GetLayoutFileScriptIncludes();
	if ( vecScripts.Count() > 0 )
	{
		m_bLayoutIncludesScripts = true;
	}
	FOR_EACH_VEC( vecScripts, i )
	{
		UIEngine()->RunScript( this, vecScripts[i].GetScriptContents(), vecScripts[i].strFilename.String(), vecScripts[i].nLine, vecScripts[i].nCol, false, bIsReload );
	}

	// now that we have created all children in this layout file, parse panel events. Delaying allows for us to specify IDs in the panel events
	// for panels before they are defined in the layout file
	FOR_EACH_VEC( vecPanelEventsToParse, i )
	{
		PanelEventsToParse_t &panelEvent = vecPanelEventsToParse[i];
		if ( !panelEvent.m_pPanel->BParsePanelEvent( panelEvent.m_symProperty, panelEvent.m_pchEvent, GetJavaScriptContextParent() ) )
		{
			const char *pchFullFile = pLayoutFile->GetLayoutFileSymbol().String();
			const char *pchFile = V_UnqualifiedFileName( pchFullFile );
			if( !pchFile )
				pchFile = pchFullFile;

			AssertMsg3( false, "**** Failed to parse panel event in file=%s, event=%s: %s\n", pchFile, panelEvent.m_symProperty.String(), panelEvent.m_pchEvent );
			return false;
		}
		else
		{
			m_bLayoutIncludesScripts = true;
		}
	}


	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses and sets a panel event from a layout file
//-----------------------------------------------------------------------------
bool CUIPanel::BApplyLayoutSnippet( LayoutFilePtr_t pLayoutFile, const char *pchSnippetName, CUtlVector< panorama::IUIPanel * > *pvecExistingPanels )
{
	VPROF_BUDGET( "CUIPanel::BApplyLayoutSnippet", VPROF_BUDGETGROUP_STEAMUI );
	PanelDescription_t *pPanelDescription = pLayoutFile->GetSnippet( pchSnippetName );
	Assert( pPanelDescription );
	if ( !pPanelDescription )
		return false;

	CUIPanel::s_bInApplyLayoutFile = true;

	// doesn't ensure that root of layout file matches this panel type

	// apply our settings and create children if necessary
	CUtlVector< PanelEventsToParse_t > vecPanelEventsToParse;
	if ( !BApplyPanelDescription( pLayoutFile->GetLayoutFileSymbol(), pPanelDescription, &vecPanelEventsToParse, pvecExistingPanels ) )
	{
		CUIPanel::s_bInApplyLayoutFile = false;
		return false;
	}

	// Ok to apply styles at this point as all children are created, JS or events in onload may cause that to happen
	CUIPanel::s_bInApplyLayoutFile = false;

	// now that we have created all children in this layout file, parse panel events. Delaying allows for us to specify IDs in the panel events
	// for panels before they are defined in the layout file
	FOR_EACH_VEC( vecPanelEventsToParse, i )
	{
		PanelEventsToParse_t &panelEvent = vecPanelEventsToParse[ i ];
		if ( !panelEvent.m_pPanel->BParsePanelEvent( panelEvent.m_symProperty, panelEvent.m_pchEvent, GetJavaScriptContextParent() ) )
		{
			const char *pchFullFile = pLayoutFile->GetLayoutFileSymbol().String();
			const char *pchFile = V_UnqualifiedFileName( pchFullFile );
			if ( !pchFile )
				pchFile = pchFullFile;

			AssertMsg4( false, "**** Failed to parse panel event in file=%s, snippet=%s, event=%s: %s\n", pchFile, pchSnippetName, panelEvent.m_symProperty.String(), panelEvent.m_pchEvent );
			return false;
		}
		else
		{ 
			m_bLayoutIncludesScripts = true;
		}
	}


	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses and sets a panel event from a layout file
//-----------------------------------------------------------------------------
bool CUIPanel::BParsePanelEvent( CPanoramaSymbol symProperty, const char *pchValue, IUIPanel *pJavascriptContext )
{
	if( !BIsPanelEvent( symProperty ) )
	{
		AssertMsg1( false, "%s is not a valid panel event", symProperty.String() );
		return false;
	}

	V8_CtxDbgMsg( "BParsePanelEvent: Panel %x, Context panel %x, SymProperty = <%s> Event = <%s>\n", this, pJavascriptContext, symProperty.String(), pchValue );

	VecUIEvents_t *pvecEvents = new VecUIEvents_t();
	const char *pchCur = pchValue;
	while( pchCur && pchCur[0] != '\0' )
	{
		const char *pchEnd = NULL;
		IUIEvent *pEvent = UIEngine()->CreateEventFromString( this, pchCur, &pchEnd );
		if( !pEvent || !pchEnd )
		{
			pvecEvents->PurgeAndDeleteElements();
			delete pvecEvents;

			v8::Persistent<v8::Script> *pScript = UIEngineInternal()->CompileScript( pJavascriptContext, pchValue, CFmtStr1024( "%s#%s - %s", m_pClientPtr->GetPanelType().String(), GetID() ? GetID() : "(undefined)", symProperty.String() ).String() );
			if( !pScript->IsEmpty() )
			{
				SetPanelEventInternal( symProperty, NULL, pScript, NULL, pJavascriptContext );
				return true;
			}

			delete pScript;
			return false;
		}

		pchEnd = CSSHelpers::SkipSpaces( pchEnd );
		if( pchEnd[0] == ';' )
			++pchEnd;
		pchEnd = CSSHelpers::SkipSpaces( pchEnd );

		pvecEvents->AddToTail( pEvent );
		pchCur = pchEnd;
	}


	CUIPanel::SetPanelEvent( symProperty, pvecEvents );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Stores events to be dispatched when a specific panel event occurs
//-----------------------------------------------------------------------------
void CUIPanel::SetPanelEventInternal( CPanoramaSymbol symPanelEvent, VecUIEvents_t *pvecEvents, 
	v8::Persistent<v8::Script> *pScript, v8::Persistent<v8::Function> *pJSFunc, IUIPanel *pJSContext )
{
	// if onactivate or onfocus, automatically turn on accepting input and focus
	if ( pvecEvents || pScript || pJSFunc )
	{
		if ( symPanelEvent == k_symPropertyOnActivate || symPanelEvent == k_symPropertyOnFocus )
		{
			if ( !BAcceptsFocus() && k_flTabIndexInvalid == m_flTabIndex && m_flSelectionPosX == k_flSelectionPosInvalid )
			{
				// This is bad, in that kb/gamepad nave is broken... but dota doesn't care.  These will spew like crazy and never get fixed there.
#if !defined( SOURCE2_PANORAMA )
				Msg( "Adding activate event to a panel(%s) without a tab or selection index, the gamepad and keyboard can't get here.\n", m_strID.String() );
#endif
			}

			SetAcceptsFocus( true );
		}

		// if on onmouseactivate, automatically turn on accepting input, but not focus
		if ( symPanelEvent == k_symPropertyOnMouseActivate )
		{
			SetAcceptsInput( true );
		}
	}

	// delete any existing events
	ClearPanelEvents( symPanelEvent );

	PanelEvent_t ev;
	if ( pScript )
	{
		ev.eType = PanelEvent_t::k_EEventType_JSScript;
		ev.data.pJSScript = pScript;
		ev.pJSContext = pJSContext;
	}
	else if ( pJSFunc )
	{
		ev.eType = PanelEvent_t::k_EEventType_JSFunction;
		ev.data.pJSFunction = pJSFunc;
		ev.pJSContext = pJSContext;
	}
	else if ( pvecEvents )
	{
		ev.eType = PanelEvent_t::k_EEventType_UIEventArray;
		ev.data.pVecIUIEvent = pvecEvents;
	}

	if ( ev.eType != PanelEvent_t::k_EEventType_None )
	{
		if ( !m_pmapPanelEvents )
			m_pmapPanelEvents = new CUtlMap < CPanoramaSymbol, PanelEvent_t, short, CDefLess< CPanoramaSymbol > > ;

		m_pmapPanelEvents->Insert( symPanelEvent, ev );
	}

	// bugbug jmccaskey - DELETE ME	
	// Let anyone who's interested know that the event changed.
	ClientPtr()->OnPanelEventSet( symPanelEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Return the panel's list of events for the given event type
//-----------------------------------------------------------------------------
VecUIEvents_t * CUIPanel::GetPanelEvents( CPanoramaSymbol symEvent )
{
	if( !m_pmapPanelEvents )
		return NULL;

	int iMap = m_pmapPanelEvents->Find( symEvent );
	if( iMap == m_pmapPanelEvents->InvalidIndex() )
	{
		return NULL;
	}

	if( m_pmapPanelEvents->Element( iMap ).eType != PanelEvent_t::k_EEventType_UIEventArray )
		return NULL;

	return m_pmapPanelEvents->Element( iMap ).data.pVecIUIEvent;
}



//-----------------------------------------------------------------------------
// Purpose: Removes a panel event if set
//-----------------------------------------------------------------------------
void CUIPanel::ClearPanelEvents( CPanoramaSymbol symPanelEvent )
{
	if( m_pmapPanelEvents )
	{
		short iMap = m_pmapPanelEvents->Find( symPanelEvent );
		if( iMap != m_pmapPanelEvents->InvalidIndex() )
		{
			PanelEvent_t &panelEvent = m_pmapPanelEvents->Element( iMap );
			if( panelEvent.eType == PanelEvent_t::k_EEventType_UIEventArray && panelEvent.data.pVecIUIEvent )
			{
				panelEvent.data.pVecIUIEvent->PurgeAndDeleteElements();
				delete panelEvent.data.pVecIUIEvent;
			}
			else if( panelEvent.eType == PanelEvent_t::k_EEventType_JSScript && panelEvent.data.pJSScript )
			{
				panelEvent.data.pJSScript->Reset();
				delete panelEvent.data.pJSScript;
			}
			else if ( panelEvent.eType == PanelEvent_t::k_EEventType_JSFunction && panelEvent.data.pJSFunction )
			{
				panelEvent.data.pJSFunction->Reset();
				delete panelEvent.data.pJSFunction;
			}

			m_pmapPanelEvents->RemoveAt( iMap );
		}
		if( m_pmapPanelEvents->Count() == 0 )
			SAFE_DELETE( m_pmapPanelEvents );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes all panel events
//-----------------------------------------------------------------------------
void CUIPanel::ClearPanelEvents()
{
	VPROF_BUDGET( "ClearPanelEvents", VPROF_BUDGETGROUP_TENFOOT );
	if( m_pmapPanelEvents )
	{
		FOR_EACH_MAP_FAST( *m_pmapPanelEvents, i )
		{
			PanelEvent_t &panelEvent = m_pmapPanelEvents->Element( i );
			if( panelEvent.eType == PanelEvent_t::k_EEventType_UIEventArray && panelEvent.data.pVecIUIEvent )
			{
				panelEvent.data.pVecIUIEvent->PurgeAndDeleteElements();
				delete panelEvent.data.pVecIUIEvent;
			}
			else if( panelEvent.eType == PanelEvent_t::k_EEventType_JSScript && panelEvent.data.pJSScript )
			{
				panelEvent.data.pJSScript->Reset();
				delete panelEvent.data.pJSScript;
			}
			else if ( panelEvent.eType == PanelEvent_t::k_EEventType_JSFunction && panelEvent.data.pJSFunction )
			{
				panelEvent.data.pJSFunction->Reset();
				delete panelEvent.data.pJSFunction;
			}
		}

		SAFE_DELETE( m_pmapPanelEvents );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks is a panel event has been set on this panel or not
//-----------------------------------------------------------------------------
bool CUIPanel::BIsPanelEventSet( CPanoramaSymbol symPanelEvent )
{
	if( !m_pmapPanelEvents )
		return false;

	short iMap = m_pmapPanelEvents->Find( symPanelEvent );
	if( iMap == m_pmapPanelEvents->InvalidIndex() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Dispatches all events for the specified panel event
// Returns: true if panel event was registered & events have been dispatched
//-----------------------------------------------------------------------------
bool CUIPanel::DispatchPanelEvent( CPanoramaSymbol symPanelEvent )
{
	if( !m_pmapPanelEvents )
		return false;

	short iMap = m_pmapPanelEvents->Find( symPanelEvent );
	if( iMap == m_pmapPanelEvents->InvalidIndex() )
		return false;

	// If this is a focus related event and it's being triggered while we are still in the process of
	// changing focus, then queue it and the input layer will fire it after it finishes traversing parents
	// updating focus state and we are in a consistent state.  That's important since a handler of one of these
	// events could in turn try to update focus again.
	if( symPanelEvent == k_symPropertyOnFocus || symPanelEvent == k_symPropertyOnDescendantFocus
		|| symPanelEvent == k_symPropertyOnBlur || symPanelEvent == k_symPropertyOnDescendantBlur )
	{
		if( GetParentWindow()->UIWindowInput()->BInSetInputFocusTraverse() )
		{
			GetParentWindow()->UIWindowInput()->QueuePanelFocusEvent( this, symPanelEvent );
			return true;
		}
	}


	// Local copy because while we are dispatching it's possible that in response to an event someone goes and modifies
	// our vector itself, for instance a button that sometimes dispatches Add or Remove events, then on handling updates
	// the buttons activate event.
	PanelEvent_t panelEvent = m_pmapPanelEvents->Element( iMap );
	if( panelEvent.eType == PanelEvent_t::k_EEventType_UIEventArray && panelEvent.data.pVecIUIEvent )
	{
		VecUIEvents_t vecLocal( 0, panelEvent.data.pVecIUIEvent->Count() );
		FOR_EACH_VEC( *(panelEvent.data.pVecIUIEvent), i )
		{
			vecLocal.AddToTail( panelEvent.data.pVecIUIEvent->Element( i )->Copy() );
		}

		FOR_EACH_VEC( vecLocal, i )
		{
			// We cannot dispatch the event async, or things will happen like a right click calls a 
			// ContextMenu() event, then another right click does immediately after and we double handle.
			// We need the actual event to occur synchronously here so state changes and we don't handle 
			// the second incorrectly.
			//
			// This has the side effect that if consuming code wants to delete a panel in response to an activation 
			// or such you could get in trouble.  Use DeleteAsync on the panel in that case in the event handler.
			UIEngine()->DispatchEvent( vecLocal[i] );
		}
	}
	else if( panelEvent.eType == PanelEvent_t::k_EEventType_JSScript && panelEvent.data.pJSScript )
	{
		IUIPanel *pPanel = panelEvent.pJSContext.Get();
		if ( !pPanel )
		{
			V8_CtxDbgAssert ( "DispatchPanelEvent: Null context was unexpected" );
			pPanel = GetJavaScriptContextParent();
		}

		UIEngineInternal()->RunScript( pPanel, panelEvent.data.pJSScript, false );
	}
	else if( panelEvent.eType == PanelEvent_t::k_EEventType_JSFunction && panelEvent.data.pJSFunction )
	{
		IUIPanel *pPanel = panelEvent.pJSContext.Get();
		
		if ( !pPanel )
		{
			V8_CtxDbgAssert ( "Null context was unexpected ");
		}

		UIEngineInternal()->RunFunction( pPanel, panelEvent.data.pJSFunction, 0, nullptr, false );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to parse class symbols out of space separated string
//-----------------------------------------------------------------------------
void ParseClassSymbols( CUtlVector< CPanoramaSymbol > *pvecClassSymbols, const char *pch )
{
	// can be space separated
	CUtlVector< char*, CUtlMemory< char* > > vecClassNames;
	V_SplitString( pch, " ", vecClassNames );
	pvecClassSymbols->EnsureCapacity( vecClassNames.Count() );
	FOR_EACH_VEC( vecClassNames, i )
	{
		pvecClassSymbols->AddToTail( CPanoramaSymbol( vecClassNames[i] ) );
	}
	vecClassNames.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: Should the property be applied only after all children are created when loading layout
//-----------------------------------------------------------------------------
bool CUIPanel::BIsDelayedProperty( CPanoramaSymbol symProperty ) const
{
	if( symProperty == k_symOnPanelEvent )
		return true;

	return ClientPtr()->BIsDelayedProperty( symProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the symbol matches a valid panel event
//-----------------------------------------------------------------------------
bool CUIPanel::BIsPanelEvent( CPanoramaSymbol symProperty ) const
{
	if( symProperty == k_symPropertyOnLoad )
		return true;
	else if( symProperty == k_symPropertyOnActivate )
		return true;
	else if( symProperty == k_symPropertyOnMouseActivate )
		return true;
	else if( symProperty == k_symPropertyOnContextMenu )
		return true;
	else if( symProperty == k_symPropertyOnFocus )
		return true;
	else if( symProperty == k_symPropertyOnDescendantFocus )
		return true;
	else if( symProperty == k_symPropertyOnBlur )
		return true;
	else if( symProperty == k_symPropertyOnDescendantBlur )
		return true;
	else if( symProperty == k_symPropertyOnCancel )
		return true;
	else if ( symProperty == k_symPropertyOnMouseOver )
		return true;
	else if ( symProperty == k_symPropertyOnMouseOut )
		return true;
	else if ( symProperty == k_symPropertyOnDblClick )
		return true;
	else if( symProperty == k_symNavigateUpEvent )
		return true;
	else if( symProperty == k_symNavigateDownEvent )
		return true;
	else if( symProperty == k_symNavigateLeftEvent )
		return true;
	else if( symProperty == k_symNavigateRightEvent )
		return true;
	else if( symProperty == k_symNavigateTabEvent )
		return true;
	else if( symProperty == k_symNavigateTabbackEvent )
		return true;
	else if( symProperty == k_symPropertyOnSelect )
		return true;
	else if( symProperty == k_symPropertyOnDeselect )
		return true;
	else if( symProperty == k_symPropertyOnScrolledToBottom )
		return true;
	else if( symProperty == k_symPropertyOnScrolledToRightEdge )
		return true;

	return m_pClientPtr->BIsClientPanelEvent( symProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------
bool CUIPanel::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	VPROF_BUDGET( "CUIPanel::BSetProperty", VPROF_BUDGETGROUP_TENFOOT );

	static CPanoramaSymbol k_symTab( "tabindex" );
	static CPanoramaSymbol k_symPos( "selectionpos" );
	static CPanoramaSymbol k_symSelectionPosBoundary( "selectionposboundary" );
	static CPanoramaSymbol k_symDefaultFocus( "defaultfocus" );
	static CPanoramaSymbol k_symDisabled( "disabled" );
	static CPanoramaSymbol k_symMouseCanActivate( "mousecanactivate" );
	static CPanoramaSymbol k_symFocusOnHover( "focusonhover" );
	static CPanoramaSymbol k_symChildFocusOnHover( "childfocusonhover" );
	static CPanoramaSymbol k_symHitTestEnabled( "hittest" );
	static CPanoramaSymbol k_symHitTestChildrenEnabled( "hittestchildren" );
	static CPanoramaSymbol k_symAnalogStickScroll( "analogstickscroll" );
	static CPanoramaSymbol k_symStyle( "style" );
	static CPanoramaSymbol k_symAcceptsInput( "acceptsinput" );
	static CPanoramaSymbol k_symAcceptsFocus( "acceptsfocus" );
	static CPanoramaSymbol k_symDisableFocusOnMouseDown( "disablefocusonmousedown" );
	static CPanoramaSymbol k_symScrollParentToFitWhenFocused( "scrollparenttofitwhenfocused" );
	static CPanoramaSymbol k_symInputNamespace( "inputnamespace" );
	static CPanoramaSymbol k_symOverscrollX( "overscroll-x" );
	static CPanoramaSymbol k_symOverscrollY( "overscroll-y" );
	static CPanoramaSymbol k_symSendChildScrolledIntoViewEvents( "sendchildscrolledintoviewevents" );
	static CPanoramaSymbol k_symDraggable( "draggable" );
	static CPanoramaSymbol k_symKeepScrollToBottomOnResize( "keepscrolltobottom" );
	static CPanoramaSymbol k_symRememberChildFocus( "rememberchildfocus" );
	static CPanoramaSymbol k_symClipAfterTransform( "clipaftertransform" );
	static CPanoramaSymbol k_symReadyForDisplay( "readyfordisplay" );
	static CPanoramaSymbol k_symRegisterForReadyEvents( "registerforreadyevents" );
	static CPanoramaSymbol k_symRequireCompositionLayer( "require-composition-layer" );
	static CPanoramaSymbol k_symAlwaysCacheCompositionLayer( "always-cache-composition-layer" );
	static CPanoramaSymbol k_symForceNoCompositionLayer( "force-no-composition-layer" );
	static CPanoramaSymbol ksymOffscreenCompositionLayer( "offscreen-composition-layer" );
	static CPanoramaSymbol k_symDisallowedStyleFlags( "disallowedstyleflags" );
	static CPanoramaSymbol k_symMouseTracking( "mousetracking" );
	static CPanoramaSymbol k_symUseGlobalContext( "useglobalcontext" );
	static CPanoramaSymbol k_symCachePaintCmdList( "cachepaintcmdlist" );
	static CPanoramaSymbol k_symClampFractionalPixelPositions( "clampfractionalpixelpositions" );

	const char k_rgchUnfocused[] = "unfocused";
	const char k_rgchIfFocused[] = "iffocused";
	const char k_rgchIfParentFocused[] = "ifparentfocused";
	const char k_rgchIfAnyParentFocused[] = "ifanyparentfocused";

	if( symName == k_symStyle )
	{
		size_t strLen = V_strlen( pchValue );
		if( strLen > 0 )
		{
			CUtlBuffer buf( pchValue, ( int )strLen, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
			buf.SeekPut( CUtlBuffer::SEEK_HEAD, ( int )strLen );

			StylePropertyHash_t styleProperties;
			if ( m_pLayoutFile->BParseStyleTag( buf, &styleProperties ) )
			{
				FOR_EACH_HASHMAP( styleProperties, i )
				{
					m_style.SetProperty( styleProperties.Element( i ) );
				}
				return true;
			}
		}
		else
		{
			// empty style="" tag, that's ok
			return true;
		}
	}
	else if( symName == k_symTab )
	{
		float flTabIndex = k_flTabIndexInvalid;
		if( V_strcmp( pchValue, "auto" ) == 0 )
		{
			m_flTabIndex = k_flTabIndexAuto;
			return true;
		}
		else if( V_strcmp( pchValue, "none" ) == 0 )
		{
			m_flTabIndex = k_flTabIndexInvalid;
			return true;
		}
		else if( CSSHelpers::BParseNumber( &flTabIndex, pchValue ) )
		{
			SetTabIndex( flTabIndex );
			return true;
		}
		else
		{
			Msg( "**** Invalid tabindex value in layout file\n" );
		}
	}
	else if( symName == k_symSelectionPosBoundary )
	{
		m_bSelectionPosVerBoundary = false;
		m_bSelectionPosHorBoundary = false;
		if( V_strcmp( pchValue, "both" ) == 0 )
		{
			m_bSelectionPosVerBoundary = true;
			m_bSelectionPosHorBoundary = true;
			return true;
		}
		else if( V_strcmp( pchValue, "horizontal" ) == 0 )
		{
			m_bSelectionPosHorBoundary = true;
			return true;
		}
		else if( V_strcmp( pchValue, "vertical" ) == 0 )
		{
			m_bSelectionPosVerBoundary = true;
			return true;
		}
	}
	else if( symName == k_symAnalogStickScroll )
	{
		if( V_strcmp( pchValue, "false" ) == 0 )
		{
			m_bAnalogStickScrollEnable = false;
			return true;
		}
		else if( V_strcmp( pchValue, "true" ) == 0 )
		{
			m_bAnalogStickScrollEnable = true;
			return true;
		}
	}
	else if( symName == k_symPos )
	{
		float posX = k_flSelectionPosInvalid;
		float posY = k_flSelectionPosInvalid;

		if( V_strcmp( pchValue, "auto" ) == 0 )
		{
			m_flSelectionPosX = k_flSelectionPosAuto;
			m_flSelectionPosY = k_flSelectionPosAuto;
			return true;
		}
		else if( V_strcmp( pchValue, "none" ) == 0 )
		{
			m_flSelectionPosX = k_flSelectionPosInvalid;
			m_flSelectionPosY = k_flSelectionPosInvalid;
			return true;
		}
		else if( CSSHelpers::BParseNumber( &posX, pchValue, &pchValue )
			&& CSSHelpers::BSkipComma( pchValue, &pchValue )
			&& CSSHelpers::BParseNumber( &posY, pchValue, &pchValue ) )
		{
			m_flSelectionPosX = posX;
			m_flSelectionPosY = posY;
			return true;
		}
		else
		{
			Msg( "**** Invalid selectionpos value in layout file\n" );
		}
	}
	else if( symName == k_symAcceptsInput )
	{
		SetAcceptsInput( V_atoi( pchValue ) != 0 || V_stricmp( pchValue, "true" ) == 0 );
		return true;
	}
	else if ( symName == k_symAcceptsFocus )
	{
		SetAcceptsFocus( V_atoi( pchValue ) != 0 || V_stricmp( pchValue, "true" ) == 0 );
		return true;
	}
	else if( symName == k_symDefaultFocus )
	{
		SetAcceptsInput( true );
		m_strDefaultFocus = pchValue;
		return true;
	}
	else if ( symName == k_symDisableFocusOnMouseDown )
	{
		SetDisableFocusOnMouseDown( V_atoi( pchValue ) != 0 || V_stricmp( pchValue, "true" ) == 0 );
		return true;
	}
	else if( symName == k_symScrollParentToFitWhenFocused )
	{
		SetScrollParentToFitWhenFocused( V_atoi( pchValue ) != 0 || V_stricmp( pchValue, "true" ) == 0 );
	}
	else if( symName == k_symDisabled )
	{
		SetEnabled( V_atoi( pchValue ) == 0 && V_stricmp( pchValue, "true" ) != 0 );
		return true;
	}
	else if( symName == k_symMouseCanActivate )
	{
		if( V_stricmp( pchValue, k_rgchUnfocused ) == 0 )
		{
			SetMouseCanActivate( k_EMouseCanActivateUnfocused );
			return true;
		}
		else if( V_stricmp( pchValue, k_rgchIfFocused ) == 0 )
		{
			SetMouseCanActivate( k_EMouseCanActivateIfFocused );
			return true;
		}
		else if( V_stricmp( pchValue, k_rgchIfAnyParentFocused ) == 0 )
		{
			SetMouseCanActivate( k_EMouseCanActivateIfAnyParentFocused );
			return true;
		}
		else if( V_strnicmp( pchValue, k_rgchIfParentFocused, V_ARRAYSIZE( k_rgchIfParentFocused ) - 1 ) == 0 )
		{
			const char *pchParse = pchValue += V_ARRAYSIZE( k_rgchIfParentFocused ) - 1;
			if( !CSSHelpers::BSkipLeftParen( pchParse, &pchParse ) )
			{
				Msg( "**** Failed to parse panel property %s: %s\n", symName.String(), pchValue );
				return false;
			}

			char rgchBuffer[128];
			if( !CSSHelpers::BParseIdent( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), pchParse, &pchParse ) )
			{
				Msg( "**** Failed to parse panel property %s: %s\n", symName.String(), pchValue );
				return false;
			}

			if( !CSSHelpers::BSkipRightParen( pchParse, &pchParse ) )
			{
				Msg( "**** Failed to parse panel property %s: %s\n", symName.String(), pchValue );
				return false;
			}

			SetMouseCanActivate( k_EMouseCanActivateIfParentFocused, rgchBuffer );
			return true;
		}
	}
	else if (symName == k_symFocusOnHover)
	{
		bool bFocusOnHover = false;
		if (CSSHelpers::BParseTrueFalse(pchValue, &bFocusOnHover))
		{
			m_bFocusOnHover = bFocusOnHover;
			return true;
		}
	}
	else if( symName == k_symChildFocusOnHover )
	{
		bool bFocusOnHover = false;
		if( CSSHelpers::BParseTrueFalse( pchValue, &bFocusOnHover ) )
		{
			m_bChildFocusOnHover = bFocusOnHover;
			return true;
		}
	}
	else if( symName == k_symHitTestEnabled )
	{
		if( V_stricmp( pchValue, "true" ) == 0 || V_stricmp( pchValue, "1" ) == 0 )
		{
			SetHitTestEnabled( true );
			return true;
		}
		else if( V_stricmp( pchValue, "false" ) == 0 || V_stricmp( pchValue, "0" ) == 0 )
		{
			SetHitTestEnabled( false );
			return true;
		}
		return false;
	}
	else if( symName == k_symHitTestChildrenEnabled )
	{
		if( V_stricmp( pchValue, "true" ) == 0 || V_stricmp( pchValue, "1" ) == 0 )
		{
			SetHitTestChildrenEnabled( true );
			return true;
		}
		else if( V_stricmp( pchValue, "false" ) == 0 || V_stricmp( pchValue, "0" ) == 0 )
		{
			SetHitTestChildrenEnabled( false );
			return true;
		}
		return false;
	}
	else if( symName == k_symInputNamespace )
	{
		SetInputNamespace( pchValue );
		return true;
	}
	else if( symName == k_symOverscrollX )
	{
		if( CSSHelpers::BParseNumber( &EnsureScrollData( m_pHorizontalScrollData ).m_flOverscroll, pchValue ) )
		{
			return true;
		}
		else
		{
			Msg( "**** Invalid overscroll-x value in layout file\n" );
		}
	}
	else if( symName == k_symOverscrollY )
	{
		if( CSSHelpers::BParseNumber( &EnsureScrollData( m_pVerticalScrollData ).m_flOverscroll, pchValue ) )
		{
			return true;
		}
		else
		{
			Msg( "**** Invalid overscroll-y value in layout file\n" );
		}
	}
	else if ( symName == k_symSendChildScrolledIntoViewEvents )
	{
		SetSendChildScrolledIntoViewEvents( V_atoi( pchValue ) != 0 || V_stricmp( pchValue, "true" ) == 0 );
		return true;
	}
	else if ( symName == k_symDraggable )
	{
		bool bDraggable = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bDraggable ) )
			return false;

		SetDraggable( bDraggable );
		return true;
	}
	else if ( symName == k_symKeepScrollToBottomOnResize )
	{
		m_bKeepScrollToBottomOnResize = true;
		return true;
	}
	else if ( symName == k_symRememberChildFocus )
	{
		bool bRememberChildFocus = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bRememberChildFocus ) )
			return false;
		SetRememberChildFocus( bRememberChildFocus );
		return true;
	}
	else if ( symName == k_symClipAfterTransform )
	{
		bool bValue = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
			return false;

		SetClipAfterTransform( bValue );
		return true;
	}
	else if ( symName == k_symReadyForDisplay )
	{
		bool bValue = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
			return false;

		SetReadyForDisplay( bValue );
		return true;
	}
	else if ( symName == k_symRegisterForReadyEvents )
	{
		bool bValue = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
			return false;

		RegisterForReadyEvents( bValue );
		return true;
	}
	else if ( symName == k_symMouseTracking )
	{
		bool bValue = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
			return false;

		SetMouseTracking( bValue );
		return true;
	}

	else if ( symName == k_symRequireCompositionLayer )
	{
		bool bValue = false;
		if ( CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
		{
			m_bRequireCompositionLayer = bValue;
			return true;
		}
	}
	else if ( symName == k_symAlwaysCacheCompositionLayer )
	{
		bool bValue = false;
		if ( CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
		{
			m_bAlwaysCacheCompositionLayer = bValue;
			return true;
		}
	}
	else if ( symName == k_symForceNoCompositionLayer )
	{
		bool bValue = false;
		if ( CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
		{
			m_bForceNoCompositionLayer = bValue;
			return true;
		}
	}
	else if ( symName == ksymOffscreenCompositionLayer )
	{
		bool bValue = false;
		if ( CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
		{
			m_bOffscreenCompositionLayer = bValue;
			return true;
		}
	}
	else if ( symName == k_symDisallowedStyleFlags )
	{
		bool bSuccess = true;
		uint unStyleFlags = 0;

		CUtlStringList vecStrings;
		vecStrings.SplitString( pchValue, "," );
		for ( const char *pszFlagString : vecStrings )
		{
			CUtlString strFlagString( pszFlagString );
			strFlagString.Trim();

			EStyleFlags eFlag = EStyleFlagsFromName( strFlagString.Get() );
			if ( eFlag == k_EStyleFlagNone )
			{
				bSuccess = false;
				break;
			}

			unStyleFlags |= eFlag;
		}

		SetDisallowedStyleFlags( unStyleFlags );
		return true;
	}
	else if ( symName == k_symUseGlobalContext )
	{
		bool bValue = false;
		if ( CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
		{
			if ( !CommandLine()->CheckParm( "-panorama_nosinglecontext" ) )
			{
				SetUseGlobalContext( bValue );
			}
			else
			{
				Msg( "**** Ignoring panel property %s: %s. Always false.\n", symName.String(), pchValue );
			}
			return true;
		}
	}
	else if ( symName == k_symCachePaintCmdList )
	{
		bool bValue = false;
		if ( CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
		{
			SetCachePaintCmdList( bValue );
			return true;
		}
	}
	else if ( symName == k_symClampFractionalPixelPositions )
	{
		bool bValue = false;
		if ( CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
		{
			m_eFractionalPixelPositions = bValue ? k_EFractionalPixelPositionsClamp : k_EFractionalPixelPositionsNoClamp;
			return true;
		}
	}
	else
	{
		if( !m_pMapProperties )
		{
			m_pMapProperties = new CUtlMap< CPanoramaSymbol, CUtlString, int, CDefLess< CPanoramaSymbol > >();
		}
		//Msg( "Adding param: %s=\"%s\"\n", symName.String(), pchValue );
		m_pMapProperties->Insert( symName, pchValue );
		return true;
	}

	Msg( "**** Failed to parse panel property %s: %s\n", symName.String(), pchValue );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Removes a style class from this panel
//-----------------------------------------------------------------------------
bool CUIPanel::BApplyPanelDescription( CPanoramaSymbol symLayoutFile, PanelDescription_t *pPanelDescription, CUtlVector< PanelEventsToParse_t > *pvecEventsToParse, CUtlVector< IUIPanel * > *pvecExistingPanels )
{
	VPROF_BUDGET( "CUIPanel::BApplyPanelDescription", VPROF_BUDGETGROUP_TENFOOT );

	// apply properties
	CUtlVector< ParsedPanelProperty_t > vecPanelProperties;
	CUtlVector< ParsedPanelProperty_t > vecDelayedProperties;
	vecDelayedProperties.EnsureCapacity( pPanelDescription->m_mapProperties.Count() );
	vecPanelProperties.EnsureCapacity( pPanelDescription->m_mapProperties.Count() );
	FOR_EACH_MAP_FAST( pPanelDescription->m_mapProperties, iVec )
	{
		CPanoramaSymbol symProperty = pPanelDescription->m_mapProperties.Key( iVec );
		const char *pchValue = pPanelDescription->m_mapProperties.Element( iVec );

		// handle common properties
		if( symProperty == k_symPropertyClass )
		{
			CUtlVector< CPanoramaSymbol > vecClassSymbols;
			ParseClassSymbols( &vecClassSymbols, pchValue );

			// delay updating style. We will do so after we are done applying properties
			AddClassesInternal( vecClassSymbols.Base(), vecClassSymbols.Count(), true );
			continue;
		}

		// check if this is a known event (onload, onclick, etc.)
		if( BIsPanelEvent( symProperty ) )
		{
			PanelEventsToParse_t &panelEvent = pvecEventsToParse->Element( pvecEventsToParse->AddToTail() );
			panelEvent.m_pPanel = this;
			panelEvent.m_symProperty = symProperty;
			panelEvent.m_pchEvent = pchValue;

			continue;
		}

		if( !BIsDelayedProperty( symProperty ) )
		{
			ParsedPanelProperty_t &prop = vecPanelProperties[vecPanelProperties.AddToTail()];
			prop.m_symName = symProperty;
			prop.m_pchValue = pchValue;
		}
		else
		{
			ParsedPanelProperty_t &prop = vecDelayedProperties[vecDelayedProperties.AddToTail()];
			prop.m_symName = symProperty;
			prop.m_pchValue = pchValue;
		}
	}

	// can now apply all properties at once
	ClientPtr()->BSetProperties( vecPanelProperties );
	vecPanelProperties.Purge();

	// done with properties. Update styles
	MarkStylesDirty( false );

	if ( !BCreateChildrenFromDescription( symLayoutFile, pPanelDescription, pvecEventsToParse, pvecExistingPanels ) )
		return false;

	ClientPtr()->BSetProperties( vecDelayedProperties );

	ClientPtr()->OnInitializedFromLayout();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Creates child panels from PanelDescription
//-----------------------------------------------------------------------------
bool CUIPanel::BCreateChildrenFromDescription( CPanoramaSymbol symLayoutFile, PanelDescription_t *pPanelDescription, CUtlVector< PanelEventsToParse_t > *pvecEventsToParse, CUtlVector< IUIPanel * > *pvecExistingPanels )
{
	VPROF_BUDGET( "CUIPanel::BCreateChildrenFromDescription", VPROF_BUDGETGROUP_TENFOOT );

	// create children
	FOR_EACH_VEC( pPanelDescription->m_vecChildren, iVec )
	{
		PanelDescription_t *pChildDescription = pPanelDescription->m_vecChildren.Element( iVec );

		const char *pchChildID = pChildDescription->m_strID.String();
		IUIPanel *pChild = FindChild( pchChildID );
		if ( !pChild && pchChildID[0] != '\0' && pvecExistingPanels )
		{
			FOR_EACH_VEC( *pvecExistingPanels, iExisting )
			{
				IUIPanel *pExisting = pvecExistingPanels->Element( iExisting );
				if ( V_strcmp( pExisting->GetID(), pchChildID ) == 0 && pChildDescription->m_symType == pExisting->ClientPtr()->GetPanelType() )
				{
					pChild = pExisting;
					pChild->SetParent( this );
					pvecExistingPanels->FastRemove( iExisting );
					break;
				}
			}
		}

		if ( !pChild )
		{
			IUIPanelClient *pNewChild = UIEngine()->CreatePanelClient( pChildDescription->m_symType, pchChildID, this );
			if ( !pNewChild )
			{
				AssertMsg( false, "CPanel2D tried to create child and failed" );
				return false;
			}

			pChild = pNewChild->UIPanel();

			// also mark that the panel was loaded from this layout file
			pChild->SetLayoutLoadedFromParent( this );
		}

		if ( !((CUIPanel*)pChild)->BApplyPanelDescription( symLayoutFile, pChildDescription, pvecEventsToParse, pvecExistingPanels ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check if any descendant selector include the given classes (and is a 
//			match for the given panel (id, classes, type, style flag, not selector)
//
//			Note that it is not enough to check descendant selectors for the 
//			current layout files. You also need to check descendant selectors
//			for layout files loaded by children (which could be different)
//
//			Imagine the case
//
//			<Panel class="Outer">
//			  <Panel class="Middle">
//			    <Panel class="Inner">
//			    </Panel>
//			  </Panel>
//			</Panel>
//
//			where each panel come from a different layout file
//			The inner layout file could then have a selector
//			.Outer:hover .Inner { ... }
//-----------------------------------------------------------------------------
bool CUIPanel::BHasAnyDescendantSelectorMatchingClasses( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses ) const
{
	CUtlVector< CPanoramaSymbol > vecVisitedStyleFiles( 0, 8 );
	CUtlVector< CPanoramaSymbol > vecVisitedLayoutFiles( 0, 8 );

	return BHasAnyDescendantSelectorMatchingClassesRecursive( arrChangedClasses, arrOldClasses, arrNewClasses, *this, vecVisitedLayoutFiles, vecVisitedStyleFiles );
}

bool CUIPanel::BHasAnyDescendantSelectorMatchingClassesRecursive( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedLayoutFiles, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const
{
	if ( m_pLayoutFile.Get() )
	{
		// if this panel was loaded from a separate layout file, also apply styles from that panel
		LayoutFilePtr_t pLayoutFileLoadedFrom;
		if ( m_pLayoutFile.Get() != m_pLayoutLoadedFrom.Get() )
		{
			LayoutFilePtr_t pFileLoadedFrom = m_pLayoutLoadedFrom;
			if ( !pFileLoadedFrom && GetParent() )
			{
				pFileLoadedFrom = ( (CUIPanel*)GetParent() )->m_pLayoutFile;
			}

			if ( pFileLoadedFrom.Get() && pFileLoadedFrom.Get() != m_pLayoutFile.Get() )
			{
				pLayoutFileLoadedFrom = pFileLoadedFrom;
			}
		}

		// let loaded from file override layout file
		if ( pLayoutFileLoadedFrom.Get() && !vecVisitedLayoutFiles.HasElement( pLayoutFileLoadedFrom->GetLayoutFileSymbol() ) )
		{
			vecVisitedLayoutFiles.AddToTail( pLayoutFileLoadedFrom->GetLayoutFileSymbol() );
			if ( pLayoutFileLoadedFrom->BHasAnyDescendantSelectorMatchingClasses( arrChangedClasses, arrOldClasses, arrNewClasses, panel, vecVisitedStyleFiles ) )
			{
				return true;
			}
		}

		if ( !vecVisitedLayoutFiles.HasElement( m_pLayoutFile->GetLayoutFileSymbol() ) )
		{
			vecVisitedLayoutFiles.AddToTail( m_pLayoutFile->GetLayoutFileSymbol() );
			if ( m_pLayoutFile->BHasAnyDescendantSelectorMatchingClasses( arrChangedClasses, arrOldClasses, arrNewClasses, panel, vecVisitedStyleFiles ) )
			{
				return true;
			}
		}
	}

	// Recursively check children layout files

	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if ( pChild->BHasAnyDescendantSelectorMatchingClassesRecursive( arrChangedClasses, arrOldClasses, arrNewClasses, panel, vecVisitedLayoutFiles, vecVisitedStyleFiles ) )
		{
			return true;
		}
	}
	if ( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( *m_pVecChildrenInHiding, i )
		{
			CUIPanel *pChild = (CUIPanel*)( *m_pVecChildrenInHiding )[i];
			if ( pChild->BHasAnyDescendantSelectorMatchingClassesRecursive( arrChangedClasses, arrOldClasses, arrNewClasses, panel, vecVisitedLayoutFiles, vecVisitedStyleFiles ) )
			{
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Adds multiple classes to a panel.
//-----------------------------------------------------------------------------
void CUIPanel::AddClassesInternal( CPanoramaSymbol *pSymbols, uint cSymbols, bool bDelayStyleUpdate )
{
	VPROF_BUDGET( "CUIPanel::AddClassesInternal", VPROF_BUDGETGROUP_TENFOOT );
	bool bChanged = false;

	for( uint i = 0; i < cSymbols; i++ )
	{
		if( m_vecStyleClasses.HasElement( pSymbols[i] ) )
			continue;

		bChanged = true;
		break;
	}


	if( !bChanged )
		return;

	// Need to apply previous styles before adding classes which will change them, if we were dirty before.
	{
		VPROF_BUDGET( "CUIPanel::AddClassesInternal - apply old dirty styles", VPROF_BUDGETGROUP_TENFOOT );

		// only try to apply styles we have loaded a layout file. If not, probably a top level window and addclass was called before
		// the layout file was loaded.
		if ( m_pLayoutFile.Get() && BStylesDirty() && !s_bInApplyLayoutFile )
			ApplyStyles( true );
	}

	// Perf - Copy of m_vecStyleClasses vector. 
	// Might be an issue if AddClassesInternal is called frequently
	CUtlPtrArray< CPanoramaSymbol > arrOldClasses;
	arrOldClasses.Copy( m_vecStyleClasses.Base(), m_vecStyleClasses.Count() );

	for( uint i = 0; i < cSymbols; i++ )
	{
		if( m_vecStyleClasses.HasElement( pSymbols[i] ) )
			continue;

		FOR_EACH_VEC( m_vecChildren, iChild )
		{
			((CUIPanel*)m_vecChildren[iChild])->AddClassToChildLookupMaps( pSymbols[i], this );
		}

		m_vecStyleClasses.AddToTail( pSymbols[i] );
	}

	DispatchEventAsync( StyleClassesChanged(), this );

	if( bDelayStyleUpdate )
		return;

	CUtlPtrArray< CPanoramaSymbol > arrChangedClasses;
	CUtlPtrArray< CPanoramaSymbol > arrNewClasses;

	arrChangedClasses.TakeOwnership( pSymbols, cSymbols );
	arrNewClasses.TakeOwnership( m_vecStyleClasses.Base(), m_vecStyleClasses.Count() );

	// Check if children need to be invalidated in cases where a descendant selector includes the new class
	// Can end up with false positives but it is better than invalidating the style of all children
	bool bInvalidateChildren = ( s_convarPanoramaClassesForceInvalidate.GetBool() || BHasAnyDescendantSelectorMatchingClasses( arrChangedClasses, arrOldClasses, arrNewClasses ) );
	arrChangedClasses.DetatchAndClear();
	arrNewClasses.DetatchAndClear();

	// need to apply to children as they could have a descendant selector which includes this new class
	MarkStylesDirty( bInvalidateChildren );
}


//-----------------------------------------------------------------------------
// Purpose: Marks this panel's styles as dirty. Will reload later
//-----------------------------------------------------------------------------
void CUIPanel::MarkStylesDirty( bool bIncludeChildren )
{
	VPROF_BUDGET( "CUIPanel::MarkStylesDirty", VPROF_BUDGETGROUP_TENFOOT );

	if( (m_unPanelLayoutFlags & k_EPanelLayoutStylesDirty) == 0 )
	{
		// We mark ourself with both flags, because children may already be dirty, and because we early out
		// in the apply traverse we may need to traverse them again after we apply our own in case our visibility/opacity
		// or other early out conditions have changed from our apply.
		m_unPanelLayoutFlags |= (k_EPanelLayoutStylesDirty | k_EPanelLayoutChildStylesDirty);
	}

	SetLayoutFlagsOnParents( k_EPanelLayoutChildStylesDirty );

	if( bIncludeChildren && ( (m_unPanelLayoutFlags & k_EPanelLayoutAllChildrenStylesDirty) == 0 ) )
	{
		// Flag to indicate all children have already been marked. Ensure MarkStylesDirty is only called
		// once (and avoid redundant work) when walking the panel tree (cf CUIWindowInput::ChangeHoverState
		// where hover style is first removed from all panels up to the "common" parent that should have hover and hover 
		// style is then added to all panels from the "common parent" to the target)
		// Flag removed in  CUIPanel::AfterStylesApplied
		m_unPanelLayoutFlags |= (k_EPanelLayoutAllChildrenStylesDirty);

		FOR_EACH_VEC( m_vecChildren, i )
		{
			m_vecChildren[i]->MarkStylesDirty( true );
		}

		if( m_pVecChildrenInHiding )
		{
			FOR_EACH_VEC( (*m_pVecChildrenInHiding), i )
			{
				m_pVecChildrenInHiding->Element( i )->MarkStylesDirty( true );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Mark child styles are dirty for the panel's parent 
//-----------------------------------------------------------------------------
void CUIPanel::MarkChildStylesDirtyOnParents()
{
	SetLayoutFlagsOnParents( k_EPanelLayoutChildStylesDirty );
}


//-----------------------------------------------------------------------------
// Purpose: Applies styles to ourselves and optionally to all children who are dirty
//-----------------------------------------------------------------------------
void CUIPanel::ApplyStyles( bool bTraverse )
{
	VPROF_BUDGET_DETAILED( "CUIPanel::ApplyStyles", VPROF_BUDGETGROUP_TENFOOT );

	if( s_bInApplyLayoutFile )
	{
		AssertMsg( !s_bInApplyLayoutFile, "Should never apply styles while loading layout... data isn't yet fully parsed for initial apply and we may transition things wrong." );
		return;
	}

	// protect against reentrant calls
	Assert( !m_bApplyingStyles );
	m_bApplyingStyles = true;

	// only need to apply styles if really dirty
	EStyleRepaint eRepaint = k_EStyleRepaintNone;
	bool bInheritablePropertiesChanged = false;
	bool bStylesChanged = false;
	if( BStylesDirty() )
	{
		s_vecApplyStylesTemp.RemoveAll();

		// build list of styles to apply
		if( !BBuildMatchingStyleList( &s_vecApplyStylesTemp ) )
		{
			m_bApplyingStyles = false;
			return;
		}

		// apply		
		bStylesChanged = CStyleFileSet::ApplyMatchedStylesToPanelStyle( &m_style, s_vecApplyStylesTemp, eRepaint, bInheritablePropertiesChanged );

		// done applying styles		
		m_unPanelLayoutFlags &= ~(k_EPanelLayoutStylesDirty);		
	}

	// Now that we've finished applying styles, clear out the flag marking that we know that all children are dirty.
	// We also need to clear this out along our parent chain, because they might have cached that data as well.
	for ( CUIPanel *pPanel = this; pPanel && ( pPanel->m_unPanelLayoutFlags & k_EPanelLayoutAllChildrenStylesDirty ); pPanel = ( CUIPanel * )pPanel->GetParent() )
	{
		pPanel->m_unPanelLayoutFlags &= ~( k_EPanelLayoutAllChildrenStylesDirty );
	}

	m_bApplyingStyles = false;	
	AfterStylesApplied( bStylesChanged, eRepaint, bInheritablePropertiesChanged, bTraverse );
}


//-----------------------------------------------------------------------------
// Purpose: Called when style is applied directly from code through SetPosition()
//-----------------------------------------------------------------------------
void CUIPanel::AfterStylesApplied( bool bStylesChanged, EStyleRepaint eRepaint, bool bInheritablePropertiesChanged, bool bTraverse )
{
	if ( bStylesChanged || m_bNeedOnStylesChanged )
	{
		// Probably need some level of repaint if styles changed
		if ( eRepaint == k_EStyleRepaintFull )
			SetRepaint( k_EPanelRepaintFull );
		else if ( eRepaint == k_EStyleRepaintComposition )
			SetRepaint( k_EPanelRepaintComposition );

		OnStylesChangedInternal();
	}

	bool bParentVisibleAndTraverse = bTraverse && m_bVisible && !AccessStyle()->BIsTransparentWithNoOpacityTransition();
	if ( bParentVisibleAndTraverse || bInheritablePropertiesChanged )
	{		
		CStyleFileDescendantFilter::Push( *this );
		FOR_EACH_VEC( m_vecChildren, i )
		{
			CUIPanel *pChild = (CUIPanel*)(m_vecChildren[i]);

			// If we ourselves are invisible then children don't have to get up-to-date styles applied
			if ( bParentVisibleAndTraverse && (pChild->BStylesDirty() || pChild->BChildStylesDirty()) )
			{
				pChild->ApplyStyles( true );
			}

			if ( bInheritablePropertiesChanged )
			{
				// Debug spew to test if this is invalidating way too much, it shouldn't be
				//Msg( "Telling all children of %s:%s to repaint due to inherited style change\n", GetPanelType().String(), GetID() );

				if ( eRepaint == k_EStyleRepaintFull )
					pChild->SetRepaintRecursive( k_EPanelRepaintFull );
				else if ( eRepaint == k_EStyleRepaintComposition )
					pChild->SetRepaintRecursive( k_EPanelRepaintComposition );
			}
		}
		CStyleFileDescendantFilter::Pop( *this );

		if ( bTraverse )
		{
			m_unPanelLayoutFlags &= ~(k_EPanelLayoutChildStylesDirty);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:  Invalidates painting and tells the panel it must repaint next frame, 
// applies the same level of repaint to all children
//-----------------------------------------------------------------------------
void CUIPanel::SetRepaintRecursive( EPanelRepaint eRepaintNeeded )
{
	SetRepaint( eRepaintNeeded );
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel* pPanel = (CUIPanel*)(m_vecChildren[i]);
		pPanel->SetRepaintRecursive( eRepaintNeeded );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Make sure width/height are valid for our parents layout scenario
//-----------------------------------------------------------------------------
void CUIPanel::VerifyWidthAndHeight()
{
	// Make sure width/height are valid for our parents layout scenario
	CUILength width, height;
	AccessStyleDirty()->GetWidth( width );
	AccessStyleDirty()->GetHeight( height );

	if( width.IsFillParentFlow() )
	{
		bool bIsInFlow = false;
		if( m_pParent )
		{
			EFlowDirection parentFlow;
			((CUIPanel*)m_pParent)->AccessStyle()->GetFlowChildren( parentFlow );

			// k_EFlowRightWrap and k_EFlowLeftWrap don't accept fill-parent-flow
			// like their non-wrapping equivalents do.
			if ( parentFlow == k_EFlowRight || parentFlow == k_EFlowLeft )
			{
				bIsInFlow = true;
			}
		}

		if( !bIsInFlow )
			Msg( "**** Panel %s has fill-parent-flow for width, but isn't in a flowing right layout\n", m_strID.String() );
	}

	if( height.IsFillParentFlow() )
	{
		bool bIsInFlow = false;
		if( m_pParent )
		{
			EFlowDirection parentFlow;
			((CUIPanel*)m_pParent)->AccessStyle()->GetFlowChildren( parentFlow );

			// k_EFlowDownWrap and k_EFlowUpWrap don't accept fill-parent-flow
			// like their non-wrapping equivalents do.
			if ( parentFlow == k_EFlowDown || parentFlow == k_EFlowUp )
			{
				bIsInFlow = true;
			}
		}

		if( !bIsInFlow )
			Msg( "**** Panel %s has fill-parent-flow for height, but isn't in a flowing down or up layout\n", m_strID.String() );
	}

	if ( width.IsHeightPercentage() && height.IsWidthPercentage() )
	{
		Msg( "**** Panel %s has both height-percentage and width-percentage\n", m_strID.String() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when our styles have done transition cleanup and we may need to update some flags
//-----------------------------------------------------------------------------
void CUIPanel::OnStyleTransitionsCleanup()
{
	m_unStylesPresentFlags = m_unStylesPresentFlags & ~k_EStylePresentTransformMatrix;
	CPanelStyle *pStyle = AccessStyleDirty();
	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyTransform3D::symbol ) )
	{
		if( pStyle->BHasTransitionOrAnimation( CStylePropertyTransform3D::symbol ) )
			m_unStylesPresentFlags |= k_EStylePresentTransformMatrix;
		else
		{
			if( !pStyle->BTransformIsIdentityRegardlessOfParentSize() )
				m_unStylesPresentFlags |= k_EStylePresentTransformMatrix;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when one of our style classes changed
//-----------------------------------------------------------------------------
void CUIPanel::OnStylesChangedInternal()
{
	VPROF_BUDGET_DETAILED( "CUIPanel::OnStylesChangedInternal", VPROF_BUDGETGROUP_TENFOOT );

	m_bNeedOnStylesChanged = false;

	VerifyWidthAndHeight();
	UpdateVisibility( false );

	m_unStylesPresentFlags = 0;
	CPanelStyle *pStyle = AccessStyle();

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyTransform3D::symbol ) )
	{
		if( pStyle->BHasTransitionOrAnimation( CStylePropertyTransform3D::symbol ) )
			m_unStylesPresentFlags |= k_EStylePresentTransformMatrix;
		else
		{
			if( !pStyle->BTransformIsIdentityRegardlessOfParentSize() )
				m_unStylesPresentFlags |= k_EStylePresentTransformMatrix;
		}
	}

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyPerspective::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentPerspective;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyPerspectiveOrigin::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentPerspectiveOrigin;

	if ( pStyle->BHasAnyStyleDataForProperty( CStylePropertyOpacity::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentOpacity;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyScale2DCentered::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentScale2DCentered;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyRotate2DCentered::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentRotate2DCentered;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyWashColor::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentWashColor;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyMixBlendMode::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentMixBlendMode;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyHueShift::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentHueShift;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertySaturation::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentSaturation;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyBrightness::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentBrightness;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyContrast::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentContrast;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyBlur::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentBlur;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyBorderRadius::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentBorderRadius;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyBorder::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentBorder;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyOpacityMask::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentOpacityMaskImage;

	if ( pStyle->BHasAnyStyleDataForProperty( CStylePropertyBackgroundImage::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentBackgroundImage;

	if( pStyle->BHasAnyStyleDataForProperty( CStylePropertyBoxShadow::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentBoxShadow;

	if ( pStyle->BHasAnyStyleDataForProperty( CStylePropertyTextShadow::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentTextShadow;

	if ( pStyle->BHasAnyStyleDataForProperty( CStylePropertyImageShadow::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentImageShadow;

	if ( pStyle->BHasAnyStyleDataForProperty( CStylePropertyClip::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentClip;

	if( pStyle->BHasPossibleBackgroundColor() )
		m_unStylesPresentFlags |= k_EStylePresentBackgroundFillColor;

	if ( pStyle->BHasAnyStyleDataForProperty( CStylePropertyBackgroundImgOpacity::symbol ) )
		m_unStylesPresentFlags |= k_EStylePresentBackgroundImgOpacity;

	// Let the ui client code also do work now
	ClientPtr()->OnStylesChanged();

	// Must be async, because handling this will call into Build() inside debug styles, and that calls RemoveAndDeleteChildren(), 
	// which will modify the vector of children in the top level window which is likely being iterated inside
	// the LayoutAndPaintWindows() pass right now.  We may want to make that safer somehow flagging panels as invalid
	// in the vector, skipping invalid in code that traverses, and cleaning up once per frame or something.
	DispatchEventAsync( 0.0f, PanelStyleChanged(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Builds the matching style list for this panel
//-----------------------------------------------------------------------------
bool CUIPanel::BBuildMatchingStyleList( CUtlVector< CascadeStyleFileInfo_t > *pvecStyles )
{
	VPROF_BUDGET_DETAILED( "CUIPanel::BBuildMatchingStyleList", VPROF_BUDGETGROUP_TENFOOT );

	if( !m_pLayoutFile )
	{
		// This can happen on panels that are top-level, but don't load a layout file.  This should be a transient condition 
		// usually on a panel that is going to be reparented, and is currently in the invisible panels list.  It's thus
		// ok as long as we are invisible.

		if( !m_bVisible )
			return false;
		else
		{
			AssertMsg1( false, "Panel %s has no valid layout file, thus it can have no styles and is broken.", GetID() );
			return false;
		}
	}

	// if this panel was loaded from a separate layout file, also apply styles from that panel
	LayoutFilePtr_t pLayoutFileLoadedFrom;
	if ( m_pLayoutFile.Get() != m_pLayoutLoadedFrom.Get() )
	{
		LayoutFilePtr_t pFileLoadedFrom = m_pLayoutLoadedFrom;
		if( !pFileLoadedFrom && GetParent() )
		{
			pFileLoadedFrom = ((CUIPanel*)GetParent())->m_pLayoutFile;
		}

		if ( pFileLoadedFrom.Get() && pFileLoadedFrom.Get() != m_pLayoutFile.Get() )
		{
			pLayoutFileLoadedFrom = pFileLoadedFrom;
		}
	}

	// let loaded from file override layout file
	if ( pLayoutFileLoadedFrom.Get() )
		pLayoutFileLoadedFrom->BuildMatchingStyleList( *pvecStyles, this, NULL );

	m_pLayoutFile->BuildMatchingStyleList( *pvecStyles, this, pLayoutFileLoadedFrom.Get() );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if a panel is visible. Use instead of m_bVisible, to ensure styles have been updated
//-----------------------------------------------------------------------------
bool CUIPanel::BIsVisible() const
{
	// make sure we are up to date
	if( BStylesDirty() )
		((CUIPanel*)this)->ApplyStyles( false );

	return m_bVisible;
}


//-----------------------------------------------------------------------------
// Purpose: Accessor for 3d surface
//-----------------------------------------------------------------------------
panorama::IUIRenderEngine *CUIPanel::UIRenderEngine()
{ 
	return m_pWindow->GetUIRenderEngine(); 
}


//-----------------------------------------------------------------------------
// Purpose: Accessor for render device
//-----------------------------------------------------------------------------
panorama::IUIRenderDevice *CUIPanel::UIRenderDevice()
{
	return m_pWindow->UIRenderDevice();
}


//-----------------------------------------------------------------------------
// Purpose: Check if the panel accepts input
//-----------------------------------------------------------------------------
bool CUIPanel::BAcceptsInput()
{
	if( IsEnabled() && BCanAcceptInput() )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Check if the panel can accept input
//-----------------------------------------------------------------------------
bool CUIPanel::BCanAcceptInput()
{
	if( BAcceptsFocus() || (m_unInputFlags & k_EInputAccept) || m_pVerticalScrollBar || m_pHorizontalScrollBar )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Set whether or not the panel should always consume clicks if it's hovered over,
// even if it didn't respond to the click
//-----------------------------------------------------------------------------
void CUIPanel::SetAlwaysConsumeHoverClicks( bool bAlwaysConsumeHoverClicks )
{
	if ( bAlwaysConsumeHoverClicks )
	{
		m_unInputFlags |= ( uint32 )( k_EInputAlwaysConsumeHoverClicks );
	}
	else
	{
		m_unInputFlags &= ( uint32 )( ~k_EInputAlwaysConsumeHoverClicks );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set whether or not the panel should lose focus if it's been clicked away
// from to not-panorama
//-----------------------------------------------------------------------------
void CUIPanel::SetCanClearFocusByClicking( bool bCanClearFocusByClicking )
{
	if ( bCanClearFocusByClicking )
	{
		m_unInputFlags |= (uint32)( k_ECanClearFocusByClicking);
	}
	else
	{
		m_unInputFlags &= (uint32)( ~k_ECanClearFocusByClicking);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the panel visible
//-----------------------------------------------------------------------------
void CUIPanel::SetVisible( bool bVisible )
{
	AccessStyleDirty()->SetVisibility( bVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Set the panel visible
//-----------------------------------------------------------------------------
void CUIPanel::UpdateVisibility( bool bUseDirtyStyles )
{
	bool bVisibleInStyle;
	if ( !bUseDirtyStyles )
		AccessStyle()->GetVisibility( bVisibleInStyle );
	else
		AccessStyleDirty()->GetVisibility( bVisibleInStyle );

	if ( m_bVisible != bVisibleInStyle )
	{
		// first set invisible before fixing up focus
		m_bVisible = bVisibleInStyle;
		
		// move focus if set on us or children
		if( !m_pParent )
		{
			Assert( m_pWindow );
			m_iUIPanelIndex = m_pWindow->SetPanelVisible( m_iUIPanelIndex, m_bVisible );
		}
		else if ( !bVisibleInStyle && (BHasKeyFocus() || BHasDescendantKeyFocus()) )
		{
			float flX = GetSelectionPositionX();
			float flY = GetSelectionPositionY();

			// Update focus, but do not switch contexts since this is not in response to direct input!
			CUIPanel *pLastContext = (CUIPanel*)(m_pWindow->UIWindowInput()->GetInputFocusContext());			
			GetParent()->SetFocusToNextPanel( 0, k_EPrevInTabOrder, true, GetTabIndex(), flX, flY, flX, flY );
			GetParentWindow()->UIWindowInput()->SetInputFocusContext( pLastContext );
		}

		InvalidateSizeAndPosition();
		ClientPtr()->OnVisibilityChanged();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds panel to the top level window's panel lists
//-----------------------------------------------------------------------------
void CUIPanel::AddToTopLevelWindow()
{
	Assert( m_pWindow );
	m_iUIPanelIndex = m_pWindow->AddPanel( this, m_bVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Removes panel from top level window's panel lists
//-----------------------------------------------------------------------------
void CUIPanel::RemoveFromTopLevelWindow()
{
	// check if already removed from top level window
	if( m_iUIPanelIndex == -1 )
		return;

	Assert( m_pWindow );
	m_pWindow->RemovePanel( m_iUIPanelIndex, m_bVisible );
	m_iUIPanelIndex = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Enables/disbles background movie playback in styles
//-----------------------------------------------------------------------------
void CUIPanel::EnableBackgroundMovies( bool bEnabled )
{
	CPanelStyle *pStyle = AccessStyle();
	if( pStyle )
		pStyle->EnableBackgroundMovies( bEnabled );
}


//-----------------------------------------------------------------------------
// Purpose: Returns this panel's styles, reloading if dirty
//-----------------------------------------------------------------------------
CPanelStyle *CUIPanel::AccessStyle() const
{
	if( BStylesDirty() )
	{
		const_cast< CUIPanel * >( this )->ApplyStyles( false );
	}

	return &m_style;
}


//-----------------------------------------------------------------------------
// Purpose: Access the appropriate image manager for this panel
//-----------------------------------------------------------------------------
IUIImageManager* CUIPanel::UIImageManager()
{ 
	return m_pWindow->AccessImageManager(); 
}



//-----------------------------------------------------------------------------
// Purpose: Paint the panel and it's children, called by the rendering layer when it's time to paint.
//-----------------------------------------------------------------------------
bool CUIPanel::BRequiresContentClipLayer()
{
	uint32 unFlagsThatMayCauseDrawingOutside = k_EStylePresentTransformMatrix;
	if( (m_unStylesPresentFlags & unFlagsThatMayCauseDrawingOutside) != 0 )
		return true;

	// If our children are transformed, we also must clip, or they may draw outside our bounds
	FOR_EACH_VEC( m_vecChildren, i )
	{
		if( ((CUIPanel*)m_vecChildren[i])->m_unStylesPresentFlags & k_EStylePresentTransformMatrix )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:  Access render engine object
//-----------------------------------------------------------------------------
CUIRenderEngine *CUIPanel::AccessRenderEngine()
{
	return m_pWindow->GetUIRenderEngine();
}


//-----------------------------------------------------------------------------
// Purpose:  Check for transforms that impact something beyond 2d position/size
//-----------------------------------------------------------------------------
bool CUIPanel::BHasNon2DTransforms()
{
	return AccessStyleDirty()->BHasNon2DTransforms();
}


//-----------------------------------------------------------------------------
// Purpose: Paint the panel and it's children, called by the rendering layer when it's time to paint.
//-----------------------------------------------------------------------------
void CUIPanel::PaintTraverse( PanoramaRect_t *prectPaint, bool bUseForceBuiltPaintCmdCache )
{
	// See if we need to use the cache that was force-built (overriding heuristic). Assume 
	// user knows what they are doing.
	if ( bUseForceBuiltPaintCmdCache && m_bForceBuildPaintCmdCache && m_pCachedCommandList.IsValid() )
	{
		AccessRenderEngine()->DrawCachedCommandList( *m_pCachedCommandList.Get() );
		return;
	}

	// If we aren't visible, then we don't paint and none of our children do either.
	if( !BIsVisible() )
	{
		// Ok to reset this, because to become visible we are going to have to invalidate styles which will reset this
		// having it cleared helps make sure parents don't think they need to draw due to an invisible child.
		m_eRepaint = k_EPanelRepaintNone;
		ReportRepaintWatchMatch( "PaintTraverse invisible", this, m_eRepaint );
		return;
	}

	if( m_flActualLayoutWidth <= 0.0f || m_flActualLayoutHeight <= 0.0f )
	{
		// Ok to reset this, because to become visible we are going to have to invalidate styles which will reset this
		// having it cleared helps make sure parents don't think they need to draw due to an invisible child.
		m_eRepaint = k_EPanelRepaintNone;
		ReportRepaintWatchMatch( "PaintTraverse zero size", this, m_eRepaint );
		return;
	}

	CPanelStyle *pStyle = AccessStyle();
	if( pStyle->BIsTransparentWithNoOpacityTransition() )
	{
		// Ok to reset this, because to become visible we are going to have to invalidate styles which will reset this
		// having it cleared helps make sure parents don't think they need to draw due to an invisible child.
		m_eRepaint = k_EPanelRepaintNone;
		ReportRepaintWatchMatch( "PaintTraverse transparent", this, m_eRepaint );
		return;
	}

	// If we have an opacity mask, it MUST have been already loaded
	IImageSource *pOpacityMaskSrc = NULL;
	pStyle->GetOpacityMaskImage( pOpacityMaskSrc, NULL );
	if( pOpacityMaskSrc )
	{
		if( !pOpacityMaskSrc->BIsValid() )
		{
			// Need to call the real deal to recurse up as well, as parents must also keep repainting until we have our layer... costly.
			SetRepaint( k_EPanelRepaintFull );
			return;
		}
	}

	// At this point, we know we're going to paint. So if we ever stop painting in the future, we need to notify the client panel
	m_bStoppedPaintingNotified = false;

	// if we're painting a different area than we did previously force a full paint
	if ( prectPaint )
	{
		if ( m_rectLastPaint.flX != prectPaint->flX ||
				m_rectLastPaint.flY != prectPaint->flY ||
				m_rectLastPaint.flWidth != prectPaint->flWidth ||
				m_rectLastPaint.flHeight != prectPaint->flHeight )
		{
			m_eRepaint = k_EPanelRepaintFull;
			m_rectLastPaint = *prectPaint;
			ReportRepaintWatchMatch( "PaintTraverse paint rect changed", this, m_eRepaint );
		}
	}
	else
	{
		
		if ( m_rectLastPaint.flX != 0.0f ||
				m_rectLastPaint.flY != 0.0f ||
				m_rectLastPaint.flWidth != m_flActualLayoutWidth ||
				m_rectLastPaint.flHeight != m_flActualLayoutHeight )
		{
			m_eRepaint = k_EPanelRepaintFull;
			m_rectLastPaint.flX = 0.0f;
			m_rectLastPaint.flY = 0.0f;
			m_rectLastPaint.flWidth = m_flActualLayoutWidth;
			m_rectLastPaint.flHeight = m_flActualLayoutHeight;
			ReportRepaintWatchMatch( "PaintTraverse paint rect changed", this, m_eRepaint );
		}
	}

	// Figure out if we need a full paint, and other data on how to draw us.
	EPanelRepaint eNeedsFullPaint = m_eRepaint;
	m_eRepaint = k_EPanelRepaintNone;

	// Check if transitions mean we have to repaint, first if they require a full repaint, then if they
	// require only a re-composite
	if( pStyle->BHasAnyTransitionOrAnimation( true ) )
		SetRepaint( k_EPanelRepaintFull );
	else if( pStyle->BHasAnyTransitionOrAnimation( false ) )
		SetRepaint( k_EPanelRepaintComposition );

	// We're going to paint something, so update the rate at which we require repainting now
	const float k_flRepaintRequiredDecayRate = 0.95f;
	m_flRepaintRequiredRate = k_flRepaintRequiredDecayRate * m_flRepaintRequiredRate;
	if ( eNeedsFullPaint != k_EPanelRepaintNone )
	{
		m_flRepaintRequiredRate += ( 1 - k_flRepaintRequiredDecayRate );
	}

	// If we have a cached command list and we don't need a repaint, just do that and bail.
	if ( eNeedsFullPaint == k_EPanelRepaintNone && m_pCachedCommandList.IsValid() )
	{
		AccessRenderEngine()->DrawCachedCommandList( *m_pCachedCommandList.Get() );
		return;
	}
	
	// Our cached command list is stale, so get rid of it.
	m_pCachedCommandList.Reset();

	// Use a heuristic to determine whether to create a cached command list, or to instead just tack on to the current one
	bool bSaveCommandList = m_bForceBuildPaintCmdCache || ( ( m_flRepaintRequiredRate < g_ConVarCacheCommandListRepaintThreshold.GetFloat() ) && 
		( m_unPaintCommandsBytesSize >= ( uint32 )g_ConVarCacheCommandListSizeThreshold.GetInt() ) );
	if ( bSaveCommandList )
	{
		m_pCachedCommandList = &AccessRenderEngine()->PushPaintCommandList();
	}

	uint32 unCommandListBytesStart = AccessRenderEngine()->GetCurrentCommandList().GetTotalBytesAllocated();

	// build paintable area if parent didn't specify or panel is scrolling (can add code later to convert provided coords to scrolled child coords but
	// it is very rare that a scrollable panel is partially offscreen)
	PanoramaRect_t rectPaint;
	if ( !prectPaint || m_pHorizontalScrollBar || m_pVerticalScrollBar )
	{
		GetPaintArea( &rectPaint );
		prectPaint = &rectPaint;
	}

	bool bNoChildrenOutsideBounds = true;
	if ( m_pVerticalScrollBar || m_pHorizontalScrollBar || IsInspected() || BRequiresContentClipLayer() || ClientPtr()->BRequiresContentClipLayer() )
		bNoChildrenOutsideBounds = false;

	bool bChildrenHave3DTransforms = false;

	EOverflowValue eHorizontalOverflow, eVerticalOverflow;
	AccessStyle()->GetOverflow( eHorizontalOverflow, eVerticalOverflow );
	bool bNoClip = false;
	if ( eHorizontalOverflow == k_EOverflowNoClip || eVerticalOverflow == k_EOverflowNoClip )
		bNoClip = true;

	// If we aren't squishing children, then they might be outside bounds
	if ( eHorizontalOverflow != k_EOverflowSquish || eVerticalOverflow != k_EOverflowSquish )
		bNoChildrenOutsideBounds = false;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)(m_vecChildren[i]);

		// If we aren't going to traverse to a child, then don't count it as needing to paint
		PanoramaRect_t rectChildAreaToPaint;
		if ( !BShouldDrawChild( &rectChildAreaToPaint, pChild, *prectPaint ) )
			continue;

		if ( bNoChildrenOutsideBounds )
		{
			EOverflowValue eHorizontal, eVertical;
			pChild->AccessStyle()->GetOverflow( eHorizontal, eVertical );

			if ( eHorizontal == k_EOverflowScroll || eVertical == k_EOverflowScroll )
				bNoChildrenOutsideBounds = false;
		}

		if( !bChildrenHave3DTransforms && pChild->m_unStylesPresentFlags & k_EStylePresentTransformMatrix )
		{
			// If a child has a transform assume we could have children outside bounds in case we 
			// don't require a composition layer and automatically clip to it
			bNoChildrenOutsideBounds = false;
			if( pChild->BHasNon2DTransforms() )
			{
				bChildrenHave3DTransforms = true;
			}
		}

		if ( !bChildrenHave3DTransforms || bNoChildrenOutsideBounds )
		{
			CUILength x, y, z;
			pChild->AccessStyle()->GetInterpolatedPosition( x, y, z, false );

			if ( z.GetValue() != 0.0f )
			{
				bNoChildrenOutsideBounds = false;
				bChildrenHave3DTransforms = true;
			}
		}

		if ( pChild->m_unStylesPresentFlags & k_EStylePresentScale2DCentered )
		{
			float flXScale, flYScale;
			pChild->AccessStyle()->GetInterpolatedScale2DCentered( flXScale, flYScale );
			if ( flXScale > 1.0f || flYScale > 1.0f )
				bNoChildrenOutsideBounds = false;
		}

		if ( pChild->m_unStylesPresentFlags & k_EStylePresentRotate2DCentered )
			bNoChildrenOutsideBounds = false;

		// If we already set both no point continuing to work
		if ( bNoChildrenOutsideBounds == false && bChildrenHave3DTransforms == true )
			break;
	}


	//
	// 1) Setup rendering context for the panel
	//
	bool bNeedsScreenspaceQuadOutput = (IsEnabled() && BAcceptsInput() && BHitTestEnabled());
	bool bRequireCompositionLayer = m_bRequireCompositionLayer;
	bool bCacheCompositionLayer = m_bAlwaysCacheCompositionLayer;
	if ( m_bOffscreenCompositionLayer )
	{
		bRequireCompositionLayer = true;
		bCacheCompositionLayer = true;
	}
	CPanelPtr<CPanel2D> safeptr( this );
	AccessRenderEngine()->PushAnimationAndTransformContext(
		safeptr.GetHandleAsUInt64(), m_vecChildren.Count(), m_unStylesPresentFlags, m_flActualLayoutWidth, m_flActualLayoutHeight,
		pStyle, bNoChildrenOutsideBounds, bChildrenHave3DTransforms, eNeedsFullPaint, bNoClip, BHitTestEnabled(), BHitTestChildrenEnabled(), 
		m_bNeedsIntermediateTexture, m_bClipAfterTransform, bNeedsScreenspaceQuadOutput, m_symCompositionLayerTextureName.IsValid() ? m_symCompositionLayerTextureName.String() : nullptr,
		bRequireCompositionLayer, m_bForceNoCompositionLayer, bCacheCompositionLayer, m_bOffscreenCompositionLayer, m_eFractionalPixelPositions );

	//
	// 2) Paint our background, let the animation thread know when this starts/ends so it can push/sort draw ops in parent context level
	//
	PaintBackground();

	// 
	// 3) Paint our contents
	//
	if ( m_bNeedsPaintArea )
	{
		m_pClientPtr->PaintArea( *prectPaint );
	}
	else
	{
		m_pClientPtr->Paint();
	}

#if 0
	float flXSelectionPos = GetSelectionPositionX();
	float flYSelectionPos = GetSelectionPositionY();
	if ( flXSelectionPos != k_flSelectionPosInvalid || flYSelectionPos != k_flSelectionPosInvalid )
	{
		CFmtStr strSelectionPos;
		if( flXSelectionPos == k_flSelectionPosInvalid )
			strSelectionPos.Append( "none," );
		else if( flXSelectionPos == k_flSelectionPosAuto )
			strSelectionPos.Append( "auto," );
		else
			strSelectionPos.AppendFormat( "%1.2f,", flXSelectionPos );

		if( flYSelectionPos == k_flSelectionPosInvalid )
			strSelectionPos.Append( "none" );
		else if( flYSelectionPos == k_flSelectionPosAuto )
			strSelectionPos.Append( "auto" );
		else
			strSelectionPos.AppendFormat( "%1.2f", flYSelectionPos );

		AccessRenderEngine()->DrawSolidColorTextRegion( strSelectionPos.Access(), "Arial", Color( 0xd2, 0xd2, 0x40, 0xff ).GetRawColor(),
			16.0f, 16.0f, k_EFontWeightBold, k_EFontStyleNormal, k_ETextAlignLeft, k_ETextDecorationNone,
			false, false, 0, 3.0f, 3.0f, 100.0f, 20.0f );
	}
#endif

	//
	// 4) Paint our children
	//
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)(m_vecChildren[i]);

		if ( !bUseForceBuiltPaintCmdCache || !pChild->m_bForceBuildPaintCmdCache || !pChild->m_pCachedCommandList.IsValid() )
		{
			// Skip children who invisible, or totally out of bounds of the parent
			PanoramaRect_t rectChildAreaToPaint;
			if ( !BShouldDrawChild( &rectChildAreaToPaint, pChild, *prectPaint ) )
			{
				( ( CUIPanel * )pChild )->NotifyStoppedPaintingTraverse();
				continue;
			}

			// Paint inspection margins if needed
			if( pChild->IsInspected() )
			{
				PaintChildMarginInspection( pChild );
			}
			
			// Actually paint children
			pChild->PaintTraverse( &rectChildAreaToPaint, bUseForceBuiltPaintCmdCache );
		}
		else
		{
			pChild->PaintTraverse( nullptr, bUseForceBuiltPaintCmdCache );		
		}
	}

	// Push that we are painting items that should always be painted last (ie, z order > all normal children)
	if( IsInspected() || m_pVecChildrenInHiding )
	{
		AccessRenderEngine()->BeginPaintLast();

		//
		// 5) Paint scrollbars
		// 
		PaintChildrenInHiding();

		//
		// 7) Paint inspect 
		// 
		if( IsInspected() )
		{
			PaintPanelInspection();
		}

		AccessRenderEngine()->EndPaintLast();
	}

	//
	// 7) Cleanup rendering context for panel
	//
	AccessRenderEngine()->PopAnimationAndTransformContext( safeptr.GetHandleAsUInt64() );

	// Now that rendering is finished, track how many command list bytes it took
	uint32 unCommandListBytesEnd = AccessRenderEngine()->GetCurrentCommandList().GetTotalBytesAllocated();
	m_unPaintCommandsBytesSize = unCommandListBytesEnd - unCommandListBytesStart;

	// If we're caching off the command list, then we need to pop out now
	if ( bSaveCommandList )
	{
		AccessRenderEngine()->PopPaintCommandList();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Notify this panel and all descendants that we skipped painting it
//-----------------------------------------------------------------------------
void CUIPanel::NotifyStoppedPaintingTraverse()
{
	if ( m_bStoppedPaintingNotified )
		return;

	m_bStoppedPaintingNotified = true;
	m_pCachedCommandList.Reset();

	ClientPtr()->StoppedPainting();

	FOR_EACH_VEC( m_vecChildren, i )
	{
		( ( CUIPanel * )m_vecChildren[ i ] )->NotifyStoppedPaintingTraverse();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper to paint inspection overlay for an inspected childs margins
//-----------------------------------------------------------------------------
void CUIPanel::PaintChildMarginInspection( IUIPanel *pIChild )
{
	CUIPanel *pChild = (CUIPanel*)pIChild;
	CUILength left, right, top, bottom;
	pChild->AccessStyle()->GetMargin( left, top, right, bottom );
	left.ConvertToLength( GetActualRenderWidth() );
	right.ConvertToLength( GetActualRenderWidth() );
	top.ConvertToLength( GetActualRenderHeight() );
	bottom.ConvertToLength( GetActualRenderHeight() );

	if( left.GetValue() != 0.0f || right.GetValue() != 0.0f || top.GetValue() != 0.0f || bottom.GetValue() != 0.0f )
	{
		float flLeft = fabsf( left.GetValue() );
		float flRight = fabsf( right.GetValue() );
		float flTop = fabsf( top.GetValue() );
		float flBottom = fabsf( bottom.GetValue() );

		// Hack here to get child panel style offset by margins for this drawing
		pChild->m_flActualXOffset -= flLeft;
		pChild->m_flActualYOffset -= flTop;

		// We use the childs context to paint it's margins, so it matches the transforms, but we don't want
		// to push other related styles like borders/etc.
		uint32 unFlags = pChild->m_unStylesPresentFlags & (k_EStylePresentTransformMatrix | k_EStylePresentPerspective | k_EStylePresentPerspectiveOrigin);

		AccessRenderEngine()->PushAnimationAndTransformContext( k_ulInvalidPanelHandle64, 0, unFlags, pChild->m_flActualLayoutWidth + flLeft + flRight, pChild->m_flActualLayoutHeight + flBottom + flTop, pChild->AccessStyle(), true, false, k_EPanelRepaintFull, false, false, false, false, true, false, nullptr, false, false, false, false, k_EFractionalPixelPositionsDefault );

		pChild->m_flActualXOffset += flLeft;
		pChild->m_flActualYOffset += flTop;

		// Paint hover state for childs margins
		float flOverlayOpacity = Clamp( g_ConVarOverlayOpacity.GetFloat(), 0.0f, 1.0f );
		uint32 unRawColor = Color( 0xff, 0xff, 0x00, 0xff * flOverlayOpacity ).AsUint32();

		// left 
		AccessRenderEngine()->DrawSolidColorRect(
			0,
			0,
			flLeft,
			pChild->m_flActualLayoutHeight + flBottom + flTop, unRawColor, k_EAntialiasingNone );

		// top 
		AccessRenderEngine()->DrawSolidColorRect(
			flLeft,
			0,
			flLeft + pChild->GetActualRenderWidth(),
			flTop, unRawColor, k_EAntialiasingNone );

		// right
		AccessRenderEngine()->DrawSolidColorRect(
			flLeft + pChild->GetActualRenderWidth(),
			0,
			flLeft + pChild->GetActualRenderWidth() + flRight,
			pChild->m_flActualLayoutHeight + flBottom + flTop, unRawColor, k_EAntialiasingNone );

		// bottom 
		AccessRenderEngine()->DrawSolidColorRect(
			flLeft,
			flTop + pChild->GetActualRenderHeight(),
			flLeft + pChild->GetActualRenderWidth(),
			flTop + pChild->GetActualRenderHeight() + flBottom, unRawColor, k_EAntialiasingNone );

		AccessRenderEngine()->PopAnimationAndTransformContext( k_ulInvalidPanelHandle64 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper to paint inspection overlay for the panel (bounds+padding)
//-----------------------------------------------------------------------------
void CUIPanel::PaintPanelInspection()
{
	float flOverlayOpacity = Clamp( g_ConVarOverlayOpacity.GetFloat(), 0.0f, 1.0f );
	uint32 unBodyColor = Color( 0x00, 0xff, 0xff, 0xff * flOverlayOpacity ).AsUint32();
	uint32 unBorderColor = Color( 0x00, 0xff, 0x00, 0xff * flOverlayOpacity ).AsUint32();
	uint32 unPaddingColor = Color( 0xff, 0x00, 0x00, 0xff * flOverlayOpacity ).AsUint32();

	// If we are scrolled, make sure we draw to cover the full region
	EOverflowValue eOverflowHorizontal, eOverflowVertical;
	AccessStyle()->GetOverflow( eOverflowHorizontal, eOverflowVertical );
	float flWidth = m_flActualLayoutWidth;
	float flheight = m_flActualLayoutHeight;
	if ( eOverflowHorizontal == k_EOverflowScroll )
		flWidth = GetActualRenderWidth();
	if ( eOverflowVertical == k_EOverflowScroll )
		flheight = GetActualRenderHeight();

	CUILength left, top, right, bottom;
	AccessStyle()->GetPadding( left, top, right, bottom );
	left.ConvertToLength( AccessStyle()->GetParentActualRenderWidth() );
	right.ConvertToLength( AccessStyle()->GetParentActualRenderWidth() );
	top.ConvertToLength( AccessStyle()->GetParentActualRenderHeight() );
	bottom.ConvertToLength( AccessStyle()->GetParentActualRenderHeight() );

	CUILength borderleft, bordertop, borderright, borderbottom;
	AccessStyle()->GetInterpolatedBorderWidth( borderleft, bordertop, borderright, borderbottom, false );
	borderleft.ConvertToLength( AccessStyle()->GetParentActualRenderWidth() );
	borderright.ConvertToLength( AccessStyle()->GetParentActualRenderWidth() );
	bordertop.ConvertToLength( AccessStyle()->GetParentActualRenderHeight() );
	borderbottom.ConvertToLength( AccessStyle()->GetParentActualRenderHeight() );

	// body
	AccessRenderEngine()->DrawSolidColorRect( RoundFloatToInt( left.GetValue() + borderleft.GetValue() ), RoundFloatToInt( top.GetValue() + bordertop.GetValue() ),
		RoundFloatToInt( flWidth - right.GetValue() - borderright.GetValue() ), RoundFloatToInt( flheight - bottom.GetValue() - borderbottom.GetValue() ), unBodyColor, k_EAntialiasingNone );

	// border left 
	AccessRenderEngine()->DrawSolidColorRect(
		0,
		0,
		borderleft.GetValue(),
		flheight, unBorderColor, k_EAntialiasingNone );

	// border top 
	AccessRenderEngine()->DrawSolidColorRect(
		borderleft.GetValue(),
		0,
		flWidth - borderright.GetValue(),
		bordertop.GetValue(), unBorderColor, k_EAntialiasingNone );

	// border right
	AccessRenderEngine()->DrawSolidColorRect(
		flWidth - borderright.GetValue(),
		0,
		flWidth,
		flheight, unBorderColor, k_EAntialiasingNone );

	// border bottom 
	AccessRenderEngine()->DrawSolidColorRect(
		borderleft.GetValue(),
		flheight - borderbottom.GetValue(),
		flWidth - borderright.GetValue(),
		flheight, unBorderColor, k_EAntialiasingNone );

	// padding left 
	AccessRenderEngine()->DrawSolidColorRect(
		borderleft.GetValue(),
		bordertop.GetValue(),
		borderleft.GetValue() + left.GetValue(),
		flheight - borderbottom.GetValue(), unPaddingColor, k_EAntialiasingNone );

	// padding top 
	AccessRenderEngine()->DrawSolidColorRect(
		borderleft.GetValue() + left.GetValue(),
		bordertop.GetValue(),
		flWidth - right.GetValue() - borderright.GetValue(),
		bordertop.GetValue() + top.GetValue(), unPaddingColor, k_EAntialiasingNone );

	// padding right
	AccessRenderEngine()->DrawSolidColorRect(
		flWidth - right.GetValue() - borderright.GetValue(),
		bordertop.GetValue(),
		flWidth - borderright.GetValue(),
		flheight - borderbottom.GetValue(), unPaddingColor, k_EAntialiasingNone );

	// padding bottom 
	AccessRenderEngine()->DrawSolidColorRect(
		borderleft.GetValue() + left.GetValue(),
		flheight - bottom.GetValue() - borderbottom.GetValue(),
		flWidth - right.GetValue() - borderright.GetValue(),
		flheight - borderbottom.GetValue(), unPaddingColor, k_EAntialiasingNone );
}


//-----------------------------------------------------------------------------
// Purpose: Paint the panel's background
//-----------------------------------------------------------------------------
void CUIPanel::PaintBackground()
{
	VPROF_BUDGET_DETAILED( "CUIPanel::PaintBackground", VPROF_BUDGETGROUP_TENFOOT );

	if( m_unStylesPresentFlags & k_EStylePresentBackgroundFillColor || m_unStylesPresentFlags & k_EStylePresentBackgroundImage )
	{
		AccessRenderEngine()->BeginPaintBackground();

		// grab background color from our style
		if( m_unStylesPresentFlags & k_EStylePresentBackgroundFillColor )
		{
			EOverflowValue eOverflowHorizontal, eOverflowVertical;
			AccessStyle()->GetOverflow( eOverflowHorizontal, eOverflowVertical );
			float flWidth = m_flActualLayoutWidth;
			float flHeight = m_flActualLayoutHeight;
			if ( eOverflowHorizontal == k_EOverflowScroll )
				flWidth = GetActualRenderWidth();
			if ( eOverflowVertical == k_EOverflowScroll )
				flHeight = GetActualRenderHeight();

			// Paint quad, orthographic, offsets are included in context for panel already

			CPanelPtr<IUIPanel> safeptr(this);

			FillBrushCollectionWithTransition_t *pFillBrushCollection = AccessRenderEngine()->DrawFilledRect( safeptr.GetHandleAsUInt64(), 0, 0, flWidth, flHeight );
			if ( pFillBrushCollection )
			{
				AccessStyle()->GetBackgroundFillBrushCollectionData( *pFillBrushCollection, AccessRenderEngine()->GetCommandList() );
			}
		}

		if( m_unStylesPresentFlags & k_EStylePresentBackgroundImage )
			PaintBackgroundImages();

		AccessRenderEngine()->EndPaintBackground();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Paints just background images. Should be called through PaintBackground()
//-----------------------------------------------------------------------------
void CUIPanel::PaintBackgroundImages()
{
	CUtlVector< CBackgroundImageLayer * > *pvecLayers = AccessStyle()->GetBackgroundImages();
	if ( !pvecLayers )
		return;

	// paint back to front
	FOR_EACH_VEC_BACK( *pvecLayers, i )
	{
		CBackgroundImageLayer *pLayer = pvecLayers->Element( i );
		if( !pLayer->GetMovie() && !pLayer->GetImage() )
			continue;

		float flImageWidth, flImageHeight;
		pLayer->CalculateFinalDimensions( &flImageWidth, &flImageHeight, m_flActualLayoutWidth, m_flActualLayoutHeight, GetActualUIScaleX(), GetActualUIScaleY() );

		// don't try to draw an empty background.. is transparent and code below wont work
		if( flImageWidth == 0.0f || flImageHeight == 0.0f )
			continue;

		OpacityWithTransition_t *pOpacityData = AccessStyle()->GetBackgroundImageLayerOpacityData( pLayer, AccessRenderEngine()->GetCommandList() );

		float xSpace, ySpace;
		pLayer->CalculateFinalSpacing( &xSpace, &ySpace, m_flActualLayoutWidth, m_flActualLayoutHeight, flImageWidth, flImageHeight );

		float xStart, yStart;
		pLayer->CalculateFinalPosition( &xStart, &yStart, m_flActualLayoutWidth, m_flActualLayoutHeight, flImageWidth, flImageHeight );

		EOverflowValue eHorizontalOverflow, eVerticalOverflow;
		AccessStyle()->GetOverflow( eHorizontalOverflow, eVerticalOverflow );

		// if we have a background image lets render it out
		IUITexture *pTexture = nullptr;
		if ( pLayer->GetImage() )
			pTexture = pLayer->GetImage()->GetTexture();
		else if ( pLayer->GetMovie().IsValid() )
			pTexture = pLayer->GetMovie()->GetTexture();

		for( float xDraw = xStart; xDraw < m_flActualLayoutWidth; xDraw += flImageWidth + xSpace )
		{
			for( float yDraw = yStart; yDraw < m_flActualLayoutHeight; yDraw += flImageHeight + ySpace )
			{
				// Clip drawing coords to fit inside our panel
				float x0, x1, y0, y1;
				x0 = xDraw;
				x1 = xDraw + flImageWidth;
				y0 = yDraw;
				y1 = yDraw + flImageHeight;

				float u0, u1, v0, v1;
				u0 = 0.0;
				u1 = 1.0;
				v0 = 0.0;
				v1 = 1.0;

				if ( x0 < 0.0f && eHorizontalOverflow != k_EOverflowNoClip )
				{
					u0 = (0.0 - x0 ) / flImageWidth;
					x0 = 0.0;
				}

				if ( x1 > m_flActualLayoutWidth && eHorizontalOverflow != k_EOverflowNoClip )
				{
					u1 = 1.0f - ( (x1 - m_flActualLayoutWidth) / flImageWidth );
					x1 = m_flActualLayoutWidth;
				}

				if ( y0 < 0.0f && eVerticalOverflow != k_EOverflowNoClip )
				{
					v0 = (0.0 - y0) / flImageHeight;
					y0 = 0.0;
				}

				if ( y1 > m_flActualLayoutHeight && eVerticalOverflow != k_EOverflowNoClip )
				{
					v1 = 1.0f - ((y1 - m_flActualLayoutHeight) / flImageHeight);
					y1 = m_flActualLayoutHeight;
				}
				
				AccessRenderEngine()->DrawTexturedRectOpacity( pTexture, AccessStyle()->GetTexturesSampleMode(), x0, y0, x1, y1, u0, v0, u1, v1, pOpacityData );

				if( pLayer->GetRepeat().GetVertical() == k_EBackgroundRepeatNo )
					break;
			}

			if( pLayer->GetRepeat().GetHorizontal() == k_EBackgroundRepeatNo )
				break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles changing tab focus forward
//-----------------------------------------------------------------------------
bool CUIPanel::SetFocusToNextPanel( int nRepeats, EFocusMoveDirection moveType, bool bAllowWrap, float flTabIndexCurrent, float flXPosCurrent, float flYPosCurrent, float flXStart, float flYStart )
{
	VPROF_BUDGET( "CUIPanel::SetFocusToNextPanel", VPROF_BUDGETGROUP_TENFOOT );

	// Check for client control overriding first
	if( ClientPtr()->OnSetFocusToNextPanel( nRepeats, moveType, bAllowWrap, flTabIndexCurrent, flXPosCurrent, flYPosCurrent, flXStart, flYStart ) )
		return true;

	IUIPanel *pNextChildAbove = NULL;
	float flBestNextChildDistance = 0.0f;
	float flBestNextChildSecondaryDistance = 0.0f;

	IUIPanel *pFirstChildToWrapTo = NULL;
	float flBestFirstChildDistance = 0.0f;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if( pChild->BIsVisible() && pChild->IsEnabled() && !pChild->AccessStyle()->BIsTransparentWithNoOpacityTransition() )
		{
			float flDistance = 0.0f;
			float flSecondaryDistance = 0.0f;
			if( (moveType == k_ENextInTabOrder || moveType == k_EPrevInTabOrder) && flTabIndexCurrent != k_flTabIndexInvalid )
			{
				float flTabIndex = pChild->GetTabIndex();
				if( flTabIndex == k_flTabIndexInvalid )
					continue;

				flDistance = flTabIndex - flTabIndexCurrent;
				if( moveType == k_EPrevInTabOrder )
					flDistance = flDistance * -1.0f;
			}
			else if( (moveType == k_ENextByXPosition || moveType == k_EPrevByXPosition) && flXPosCurrent != k_flSelectionPosInvalid )
			{
				float flXPos = pChild->GetSelectionPositionX();
				if( flXPos == k_flSelectionPosInvalid )
					continue;

				float flYPos = pChild->GetSelectionPositionY();
				if( flYPos != k_flSelectionPosInvalid )
				{
					flSecondaryDistance = flYPos - flYPosCurrent;
				}

				flDistance = flXPos - flXPosCurrent;
				if( moveType == k_EPrevByXPosition )
					flDistance = flDistance * -1.0f;
			}
			else if( (moveType == k_ENextByYPosition || moveType == k_EPrevByYPosition) && flYPosCurrent != k_flSelectionPosInvalid )
			{
				float flYPos = pChild->GetSelectionPositionY();
				if( flYPos == k_flSelectionPosInvalid )
					continue;

				float flXPos = pChild->GetSelectionPositionX();
				if( flXPos != k_flSelectionPosInvalid )
				{
					flSecondaryDistance = flXPos - flXPosCurrent;
				}

				flDistance = flYPos - flYPosCurrent;
				if( moveType == k_EPrevByYPosition )
					flDistance = flDistance * -1.0f;
			}

			if( flDistance > 0.00001f && (pNextChildAbove == NULL || flDistance <= flBestNextChildDistance) )
			{
				bool bUpdate = true;
				if( fabs( flDistance - flBestNextChildDistance ) < 0.00001f )
				{
					if( fabs( flSecondaryDistance ) > flBestNextChildSecondaryDistance )
					{
						// It's the same (or virtually same) distance in movement axis, but farther away in opposite axis 
						bUpdate = false;
					}
				}
				if( bUpdate )
				{
					flBestNextChildSecondaryDistance = fabs( flSecondaryDistance );
					flBestNextChildDistance = flDistance;
					pNextChildAbove = pChild;
				}
			}

			if( flDistance < -0.00001f && (pFirstChildToWrapTo == NULL || flDistance <= flBestFirstChildDistance) )
			{
				bool bUpdate = true;
				if( fabs( flDistance - flBestNextChildDistance ) < 0.00001f )
				{
					if( fabs( flSecondaryDistance ) > flBestNextChildSecondaryDistance )
					{
						// It's the same (or virtually same) distance in movement axis, but farther away in opposite axis 
						bUpdate = false;
					}
				}

				if( bUpdate )
				{
					flBestFirstChildDistance = flDistance;
					pFirstChildToWrapTo = pChild;
				}
			}
		}
	}

	if( pNextChildAbove )
	{
		if( pNextChildAbove->BAcceptsFocus() )
		{
			return pNextChildAbove->UpdateFocusInContext();
		}
		else
		{
			if( pNextChildAbove->SetInputFocusToFirstOrLastChildInFocusOrder( moveType, flXStart, flYStart ) )
				return true;
		}

		// Now want to move beyond this child, so update start pos
		return SetFocusToNextPanel( nRepeats, moveType, bAllowWrap, pNextChildAbove->GetTabIndex(), pNextChildAbove->GetSelectionPositionX(), pNextChildAbove->GetSelectionPositionY(), flXStart, flYStart );
	}

	// If we are a boundary panel, check that and bail early if nRepeats > 0
	if( nRepeats > 0 )
	{
		if( (moveType == k_ENextByYPosition || moveType == k_EPrevByYPosition) && BSelectionPosVerticalBoundary() )
			return false;

		if( (moveType == k_ENextByXPosition || moveType == k_EPrevByXPosition) && BSelectionPosHorizontalBoundary() )
			return false;
	}

	// We didn't find anyone next to actually set focus on at our level, let parents try.
	CUIPanel *pParent = (CUIPanel*)GetParent();
	if( pParent && !BTopOfInputContext() )
	{
		// If we are going to move past ourself, first check for onmove events at our level that override
		CPanoramaSymbol symMoveEvent;
		bool bMoveEventExists = false;
		switch( moveType )
		{
		case k_ENextByXPosition:
			symMoveEvent = k_symNavigateRightEvent;
			bMoveEventExists = true;
			break;
		case k_EPrevByXPosition:
			symMoveEvent = k_symNavigateLeftEvent;
			bMoveEventExists = true;
			break;
		case k_ENextByYPosition:
			symMoveEvent = k_symNavigateDownEvent;
			bMoveEventExists = true;
			break;
		case k_EPrevByYPosition:
			symMoveEvent = k_symNavigateUpEvent;
			bMoveEventExists = true;
			break;
		}

		if( bMoveEventExists && BIsPanelEventSet( symMoveEvent ) )
		{
			DispatchPanelEvent( symMoveEvent );
			return true;
		}

		if( pParent->SetFocusToNextPanel( nRepeats, moveType, bAllowWrap, GetTabIndex(), GetSelectionPositionX(), GetSelectionPositionY(), flXStart, flYStart ) )
			return true;
	}

	// The parent also doesn't have anyone ahead of us in the specified order, try wrapping back around, if allowed
	if( bAllowWrap )
	{
		if( pFirstChildToWrapTo )
		{
			if( pFirstChildToWrapTo->BAcceptsFocus() )
			{
				return pFirstChildToWrapTo->SetFocus();
			}
			else if( pFirstChildToWrapTo->SetInputFocusToFirstOrLastChildInFocusOrder( moveType, flXStart, flYStart ) )
				return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets input focus to the first child in our tab order who deserves it
//-----------------------------------------------------------------------------
bool CUIPanel::SetInputFocusToFirstOrLastChildInFocusOrder( EFocusMoveDirection moveType, float flXStart, float flYStart )
{
	VPROF_BUDGET( "CUIPanel::SetInputFocusToFirstOrLastChildInFocusOrder", VPROF_BUDGETGROUP_TENFOOT );

	if ( GetRememberChildFocus() )
	{
		return SetFocus();
	}

	struct PotentialFocusChild_t
	{
		IUIPanel *pPanelToSet;
		float flBestValue;
		float flBestSecondAxisDistance;
	};

	CUtlVector<PotentialFocusChild_t> vecCandidates;
	vecCandidates.EnsureCapacity( m_vecChildren.Count() );

	FOR_EACH_VEC( m_vecChildren, i )
	{
		IUIPanel *pChild = m_vecChildren[i];
		if( pChild->BIsVisible() && pChild->IsEnabled() && !pChild->AccessIUIStyle()->BIsTransparentWithNoOpacityTransition() )
		{
			float flOrderValue = 0.0f;
			float flSecondAxisDistance = FLT_MAX;
			if( moveType == k_ENextInTabOrder || moveType == k_EPrevInTabOrder )
			{
				flOrderValue = pChild->GetTabIndex();
				if( flOrderValue == k_flTabIndexInvalid )
					continue;

				if( moveType == k_EPrevInTabOrder )
					flOrderValue *= -1.0f;
			}
			else if( moveType == k_ENextByXPosition || moveType == k_EPrevByXPosition )
			{
				flOrderValue = pChild->GetSelectionPositionX();
				if( flOrderValue == k_flSelectionPosInvalid )
					continue;

				if( moveType == k_EPrevByXPosition )
					flOrderValue *= -1.0f;

				float flY = pChild->GetSelectionPositionY();
				if( flY == k_flSelectionPosInvalid )
					continue;
				flSecondAxisDistance = fabsf( flYStart - flY );
			}
			else if( moveType == k_ENextByYPosition || moveType == k_EPrevByYPosition )
			{
				flOrderValue = pChild->GetSelectionPositionY();
				if( flOrderValue == k_flSelectionPosInvalid )
					continue;

				if( moveType == k_EPrevByYPosition )
					flOrderValue *= -1.0f;

				float flX = pChild->GetSelectionPositionX();
				if( flX == k_flSelectionPosInvalid )
					continue;
				flSecondAxisDistance = fabsf( flXStart - flX );
			}

			vecCandidates.AddToTail( PotentialFocusChild_t{ pChild, flOrderValue, flSecondAxisDistance } );
		}
	}

	if( !vecCandidates.IsEmpty() )
	{
#if defined( SOURCE2_PANORAMA )
		vecCandidates.SortPredicate(
#else
		vecCandidates.Sort(
#endif
			[] ( const PotentialFocusChild_t& a, const PotentialFocusChild_t& b )
			{
				return std::tie( a.flBestValue, a.flBestSecondAxisDistance )
					 < std::tie( b.flBestValue, b.flBestSecondAxisDistance );
			} );

		for ( const auto& candidate : vecCandidates )
		{
			if ( candidate.pPanelToSet->BAcceptsFocus() )
			{
				GetParentWindow()->UIWindowInput()->SetInputFocus( candidate.pPanelToSet, true, true );
				return true;
			}
			else if ( candidate.pPanelToSet->SetInputFocusToFirstOrLastChildInFocusOrder( moveType, flXStart, flYStart ) )
			{
				return true;
			}
		}
	}
	else
	{
		IUIPanel *pPanelToSet = GetDefaultInputFocus();
		if( pPanelToSet == this )
		{
			// we ran out of places to put focus
			return false;
		}
		else if( pPanelToSet )
		{
			if( pPanelToSet->BAcceptsFocus() && pPanelToSet->BIsVisible() )
			{
				GetParentWindow()->UIWindowInput()->SetInputFocus( pPanelToSet, true, true );
				return true;
			}
			else
			{
				if( pPanelToSet->SetInputFocusToFirstOrLastChildInFocusOrder( moveType, flXStart, flYStart ) )
					return true;
			}
		}
	}


	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Get the nearest parent that establishes a javascript context, or 
// return ourself if we ourselves create one
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::GetJavaScriptContextParent() const
{
	if ( m_pJSContext )
		return m_pJSContext;

	if( m_bLayoutIncludesScripts || GetParent() == NULL )
		return (IUIPanel *)this;

	CUIPanel *pParent = (CUIPanel*)GetParent();
	while( pParent )
	{
		if ( pParent->m_pJSContext )
			return pParent->m_pJSContext;

		if ( pParent->m_bLayoutIncludesScripts )
			return pParent;


		// If there is no further parent, just return the current.
		CUIPanel *pNewParent = (CUIPanel*)(pParent->GetParent());
		if( pNewParent == NULL )
			return pParent;

		pParent = pNewParent;
	}

	// Shouldn't reach!
	AssertMsg( false, "Found no context within our layout file - shouldn't be possible!" );
	return NULL;
}



//-----------------------------------------------------------------------------
// Purpose: Adds a style class to this panel
//-----------------------------------------------------------------------------
void CUIPanel::AddClass( const char *pchName )
{
	CPanoramaSymbol sym( pchName );
	AddClasses( &sym, 1 );
}


//-----------------------------------------------------------------------------
// Purpose: Checks if we have a given class
//-----------------------------------------------------------------------------
bool CUIPanel::BHasClass( const char *pchName )
{
	CPanoramaSymbol sym( pchName );
	return BHasClass( sym );
}


//-----------------------------------------------------------------------------
// Purpose: Checks if we have a given class
//-----------------------------------------------------------------------------
bool CUIPanel::BHasClass( CPanoramaSymbol symName )
{
	return m_vecStyleClasses.Find( symName ) != m_vecStyleClasses.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Purpose: Checks if an ascendant has a given class
//-----------------------------------------------------------------------------
bool CUIPanel::BAscendantHasClass( const char *pchName )
{
	CPanoramaSymbol sym( pchName );
	return BAscendantHasClass( sym );
}


//-----------------------------------------------------------------------------
// Purpose: Checks if an ascendant has a given class
//-----------------------------------------------------------------------------
bool CUIPanel::BAscendantHasClass( CPanoramaSymbol symName )
{
	if( BHasClass( symName ) )
		return true;
	IUIPanel *pParent = GetParent();
	if( pParent )
	{
		return pParent->BAscendantHasClass( symName );
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Toggle a class on or off
//-----------------------------------------------------------------------------
void CUIPanel::ToggleClass( const char *pchName )
{
	CPanoramaSymbol sym( pchName );
	ToggleClass( sym );
}


//-----------------------------------------------------------------------------
// Purpose: Toggle a class on or off
//-----------------------------------------------------------------------------
void CUIPanel::ToggleClass( CPanoramaSymbol symName )
{
	if( BHasClass( symName ) )
	{
		RemoveClass( symName );
	}
	else
	{
		AddClass( symName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Turn a class on or off based on bHasClass
//-----------------------------------------------------------------------------
void CUIPanel::SetHasClass( const char *pchName, bool bHasClass )
{
	CPanoramaSymbol sym( pchName );
	SetHasClass( sym, bHasClass );
}


//-----------------------------------------------------------------------------
// Purpose: Turn a class on or off based on bHasClass
//-----------------------------------------------------------------------------
void CUIPanel::SetHasClass( CPanoramaSymbol symName, bool bHasClass )
{
	if ( bHasClass )
	{
		AddClass( symName );
	}
	else
	{
		RemoveClass( symName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Switch out an existing class with another class. Stores the
// existing class in an attribute with the name pchAttribute so that it can
// remove it when a new one is switched in.
//-----------------------------------------------------------------------------
void CUIPanel::SwitchClass( const char *pchAttribute, const char *pchName )
{
	CPanoramaSymbol sym;

	if ( pchName && pchName[ 0 ] != '\0' )
	{
		sym = pchName;
	}

	SwitchClass( pchAttribute, sym );
}


//-----------------------------------------------------------------------------
// Purpose: Switch out an existing class with another class. Stores the
// existing class in an attribute with the name pchAttribute so that it can
// remove it when a new one is switched in.
//-----------------------------------------------------------------------------
void CUIPanel::SwitchClass( const char *pchAttribute, CPanoramaSymbol symName )
{
	CPanoramaSymbol symOldClass = ( UtlSymId_t )GetAttribute( pchAttribute, UTL_INVAL_SYMBOL );
	if ( symOldClass == symName )
		return;

	if ( symOldClass.IsValid() )
	{
		RemoveClass( symOldClass );
	}

	if ( symName.IsValid() )
	{
		AddClass( symName );
	}

	SetAttribute( pchAttribute, ( UtlSymId_t )symName );
}



//-----------------------------------------------------------------------------
// Purpose: Switch out an existing class with another class. Stores the
// existing class in an attribute with the name pchAttribute so that it can
// remove it when a new one is switched in.
//-----------------------------------------------------------------------------
void CUIPanel::SwitchClass( CPanoramaSymbol symAttribute, const char *pchName )
{
	CPanoramaSymbol sym;

	if ( pchName && pchName[0] != '\0' )
	{
		sym = pchName;
	}

	SwitchClass( symAttribute, sym );
}


//-----------------------------------------------------------------------------
// Purpose: Switch out an existing class with another class. Stores the
// existing class in an attribute with the name pchAttribute so that it can
// remove it when a new one is switched in.
//-----------------------------------------------------------------------------
void CUIPanel::SwitchClass( CPanoramaSymbol symAttribute, CPanoramaSymbol symName )
{
	CPanoramaSymbol symOldClass = (UtlSymId_t)GetAttribute( symAttribute, UTL_INVAL_SYMBOL );
	if ( symOldClass == symName )
		return;

	if ( symOldClass.IsValid() )
	{
		RemoveClass( symOldClass );
	}

	if ( symName.IsValid() )
	{
		AddClass( symName );
	}

	SetAttribute( symAttribute, (UtlSymId_t)symName );
}


//-----------------------------------------------------------------------------
// Purpose: Trigger any animations/sounds/etc on a class by removing it and
// then immediately adding it back.
//-----------------------------------------------------------------------------
void CUIPanel::TriggerClass( const char *pchName )
{
	CPanoramaSymbol sym;

	if ( pchName && pchName[ 0 ] != '\0' )
	{
		sym = pchName;
	}

	TriggerClass( sym );
}


//-----------------------------------------------------------------------------
// Purpose: Trigger any animations/sounds/etc on a class by removing it and
// then immediately adding it back.
//-----------------------------------------------------------------------------
void CUIPanel::TriggerClass( CPanoramaSymbol symName )
{
	RemoveClass( symName );
	AddClass( symName );
}


//-----------------------------------------------------------------------------
// Purpose: Adds multiple classes to a panel. String is space separated
//-----------------------------------------------------------------------------
void CUIPanel::AddClasses( const char *pchName )
{
	CUtlVector< CPanoramaSymbol > vecClassSymbols;
	ParseClassSymbols( &vecClassSymbols, pchName );
	AddClasses( vecClassSymbols.Base(), vecClassSymbols.Count() );
}




//-----------------------------------------------------------------------------
// Purpose: Build the map of parents by id / type for this panel and return them
//-----------------------------------------------------------------------------
void CUIPanel::GetParentLookupMaps(	MapParentsByType_t  **pMapParentsByType,
									MapParentsByID_t **pMapParentsByID,
									MapParentsByClass_t **pMapParentsByClass )
{
	bool bBuildTypeMap = false;
	if( !m_pMapParentsByType && pMapParentsByType )
	{
		m_pMapParentsByType = new MapParentsByType_t();
		bBuildTypeMap = true;
	}

	bool bBuildIDMap = false;
	if( !m_pMapParentsByID && pMapParentsByID )
	{
		m_pMapParentsByID = new MapParentsByID_t();
		bBuildIDMap = true;
	}

	bool bBuildClassMap = false;
	if( !m_pMapParentsByClass && pMapParentsByClass )
	{
		m_pMapParentsByClass = new MapParentsByClass_t();
		bBuildClassMap = true;
	}

	if( bBuildIDMap || bBuildTypeMap || bBuildClassMap )
	{
		VPROF_BUDGET_DETAILED( "CStyleFileSet::GetParentLookupMaps - build parent maps", VPROF_BUDGETGROUP_TENFOOT );

		IUIPanel *pParent = GetParent();
		while( pParent )
		{
			if( bBuildTypeMap )
				m_pMapParentsByType->InsertWithDupes( pParent->ClientPtr()->GetPanelType(), pParent );

			if( bBuildIDMap )
				m_pMapParentsByID->InsertWithDupes( pParent->GetID(), pParent );

			if( bBuildClassMap )
			{
				const CUtlVector< CPanoramaSymbol > &arrayClasses = pParent->GetClasses();
				FOR_EACH_VEC( arrayClasses, i )
				{
					m_pMapParentsByClass->InsertWithDupes( arrayClasses[i], pParent );
				}
			}

			pParent = pParent->GetParent();
		}
	}

	if( pMapParentsByType )
		*pMapParentsByType = m_pMapParentsByType;

	if( pMapParentsByID )
		*pMapParentsByID = m_pMapParentsByID;

	if( pMapParentsByClass )
		*pMapParentsByClass = m_pMapParentsByClass;
}



//-----------------------------------------------------------------------------
// Purpose: Traverse adding class to child lookup maps for parent
//-----------------------------------------------------------------------------
void CUIPanel::AddClassToChildLookupMaps( CPanoramaSymbol symClass, IUIPanel *pParent )
{
	if( m_pMapParentsByClass )
	{
		// Can't insert if others with same class exist, might get out-of-order
		if( m_pMapParentsByClass->Find( symClass ) == m_pMapParentsByClass->InvalidIndex() )
			m_pMapParentsByClass->InsertWithDupes( symClass, pParent );
		else
			SAFE_DELETE( m_pMapParentsByClass );
	}

	FOR_EACH_VEC( m_vecChildren, i )
	{
		((CUIPanel*)m_vecChildren[i])->AddClassToChildLookupMaps( symClass, pParent );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Traverse removing class from child lookup maps for parent
//-----------------------------------------------------------------------------
void CUIPanel::RemoveClassFromChildLookupMaps( CPanoramaSymbol symClass, IUIPanel *pParent )
{
	if( m_pMapParentsByClass )
	{
		int iMap = m_pMapParentsByClass->FindFirst( symClass );
		while( iMap != m_pMapParentsByClass->InvalidIndex() )
		{
			int iLocal = iMap;
			iMap = m_pMapParentsByClass->NextInorderSameKey( iMap );

			if( m_pMapParentsByClass->Element( iLocal ) == pParent )
				m_pMapParentsByClass->RemoveAt( iLocal );
		}
	}

	FOR_EACH_VEC( m_vecChildren, i )
	{
		((CUIPanel*)m_vecChildren[i])->RemoveClassFromChildLookupMaps( symClass, pParent );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finds a panel by id looking only at the panel's immediate children
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindChild( const char *pchID )
{
	if( !pchID || pchID[0] == '\0' )
		return NULL;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		// bugbug cboyd - case sensitive? Use dict?
		if( V_strcmp( m_vecChildren[i]->GetID(), pchID ) == 0 )
			return m_vecChildren[i];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Considers a failure to find a child a fatal error.
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::RequireChild( const char *pchID )
{
	IUIPanel *pChild = FindChild( pchID );
	if ( !pChild )
	{
		RequiredCallFailed( "Unable to find child '%s' in panel '%s'\n",
							pchID, GetID() );
	}
	return pChild;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a panel by id looking at all panels in this panel's hierarchy
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindChildTraverse( const char *pchID )
{
	// check if we have the specified child loaded from our layout file
	IUIPanel *pChild = FindChild( pchID );
	if( pChild )
		return pChild;

	// search all children
	FOR_EACH_VEC( m_vecChildren, i )
	{
		pChild = m_vecChildren[i];
		IUIPanel *pFound = pChild->FindChildTraverse( pchID );
		if( pFound )
			return pFound;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Considers a failure to find a child a fatal error.
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::RequireChildTraverse( const char *pchID )
{
	IUIPanel *pChild = FindChildTraverse( pchID );
	if ( !pChild )
	{
		RequiredCallFailed( "Unable to traverse to child '%s' in panel '%s'\n",
							pchID, GetID() );
	}
	return pChild;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a panel by id looking at all panels in this panel's hierarchy, 
// but must be in the same layout file.
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindChildInLayoutFile( const char *pchID )
{
	return FindChildInLayoutFileTraverse( pchID );
}


//-----------------------------------------------------------------------------
// Purpose: Finds a panel by id looking at all panels in this panel's hierarchy, 
// but must be in the same layout file.
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindChildInLayoutFileTraverse( const char *pchID )
{
	// check if we have the specified child loaded from our layout file
	CUIPanel *pChild = (CUIPanel*)FindChild( pchID );
	if( pChild )
		return pChild;

	// search all children
	FOR_EACH_VEC( m_vecChildren, i )
	{
		pChild = (CUIPanel*)m_vecChildren[i];
		if( pChild->m_bLoadedLayoutFile )
			continue;

		CUIPanel *pFound = (CUIPanel*)pChild->FindChildInLayoutFileTraverse( pchID );
		if( pFound )
			return pFound;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Considers a failure to find a child a fatal error.
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::RequireChildInLayoutFile( const char *pchID )
{
	IUIPanel *pChild = FindChildInLayoutFile( pchID );
	if ( !pChild )
	{
		RequiredCallFailed( "Unable to find child '%s' in layout file '%s'\n",
							pchID, GetLayoutFile().String() );
	}
	return pChild;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a panel by id looking at all panels in this panels layout file, NOT just children
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindPanelInLayoutFile( const char *pchID )
{
	// check if we have the specified child loaded from our layout file
	CUIPanel *pParent = this;
	while( pParent )
	{
		CUIPanel * pParentNew = (CUIPanel*)pParent->GetParent();
		if( !pParentNew )
			break;

		pParent = pParentNew;
		if( pParentNew->m_bLoadedLayoutFile ) // if this panel loaded the layout file then check just the layout file matches
		{
			break;
		}

		if( !V_stricmp( pParent->GetID(), pchID ) ) // common case is the ID is one of our parents, lets check
			break;
	}
	if( !pParent )
		return NULL;
	if( !V_stricmp( pParent->GetID(), pchID ) ) // it was a parent, just return it
		return pParent;

	CUIPanel *pFound = (CUIPanel*)pParent->FindChildInLayoutFileTraverse( pchID );

	// If we are ourselves in some layotu file, but also load our own, then also search within our loaded layout
	// in addition to the search we just did above if it found nothing
	if( !pFound && m_bLoadedLayoutFile )
	{
		return FindChildInLayoutFileTraverse( pchID );
	}

	return pFound;
}


//-----------------------------------------------------------------------------
// Purpose: Considers a failure to find a child a fatal error.
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::RequirePanelInLayoutFile( const char *pchID )
{
	IUIPanel *pPanel = FindPanelInLayoutFile( pchID );
	if ( !pPanel )
	{
		RequiredCallFailed( "Unable to find panel '%s' in layout file '%s'\n",
							pchID, GetLayoutFile().String() );
	}
	return pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Populates a vector with all immediate children matching a class
//-----------------------------------------------------------------------------
void CUIPanel::FindChildrenWithClass( const char *pchClass, CUtlVector<IUIPanel *> &vecMatchingChildren )
{
	if( !pchClass || pchClass[0] == '\0' )
		return;

	CPanoramaSymbol symClass = pchClass;
	FindChildrenWithClass( symClass, vecMatchingChildren );
}


//-----------------------------------------------------------------------------
// Purpose: Populates a vector with all immediate children matching a class
//-----------------------------------------------------------------------------
void CUIPanel::FindChildrenWithClass( CPanoramaSymbol symClass, CUtlVector<IUIPanel *> &vecMatchingChildren )
{
	FOR_EACH_VEC( m_vecChildren, i )
	{
		if ( m_vecChildren[i]->BHasClass( symClass ) )
			vecMatchingChildren.AddToTail( m_vecChildren[i] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Populates a vector with all children matching a class
//-----------------------------------------------------------------------------
void CUIPanel::FindChildrenWithClassTraverse( const char *pchClass, CUtlVector<IUIPanel *> &vecMatchingChildren )
{
	if ( !pchClass || pchClass[0] == '\0' )
		return;

	CPanoramaSymbol symClass = pchClass;
	FindChildrenWithClassTraverse( symClass, vecMatchingChildren );
}


//-----------------------------------------------------------------------------
// Purpose: Populates a vector with all children matching a class
//-----------------------------------------------------------------------------
void CUIPanel::FindChildrenWithClassTraverse( CPanoramaSymbol symClass, CUtlVector<IUIPanel *> &vecMatchingChildren )
{
	// check if we have the specified child loaded from our layout file
	FindChildrenWithClass( symClass, vecMatchingChildren );

	// search all children
	FOR_EACH_VEC( m_vecChildren, i )
	{
		( ( CUIPanel* )m_vecChildren[i] )->FindChildrenWithClassTraverse( symClass, vecMatchingChildren );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the specified panel is an ancestor of this panel
//-----------------------------------------------------------------------------
bool CUIPanel::IsDescendantOf( const IUIPanel *pPanel ) const
{
	if( !pPanel )
		return false;

	for( IUIPanel *pParent = GetParent(); pParent != NULL; pParent = pParent->GetParent() )
	{
		if( pParent == pPanel )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Stores events to be dispatched when a specific panel event occurs
//-----------------------------------------------------------------------------
void CUIPanel::SetPanelEventJS( const v8::FunctionCallbackInfo<v8::Value>& args )
{	
	if ( args.Length() != 2 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "SetPanelEvent takes 2 arguments [panel event name, function/script]" ).ToLocalChecked() );
		return;
	}

	// handle event param
	CPanoramaSymbol symEvent;
	if ( args[0]->IsString() )
	{
		v8::String::Utf8Value strEvent(v8::Isolate::GetCurrent(),  args[0] );
		const char *pchEventName = *strEvent;
		if ( pchEventName )
			symEvent = CPanoramaSymbol( pchEventName );
	}		

	if ( !symEvent.IsValid() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "SetPanelEvent's first argument must be a panel event type" ).ToLocalChecked() );
		return;
	}
	
	// Get this panel's JS Parent to compile the event handler in. Note that the js parent may not be the be the 
	// same one as the panel that owns the current context, as contexts can switch in various ways. See confluence page on
	// Javascript authoring for more on this.
	IUIPanel *pJSContext = GetJavaScriptContextParent();
	V8_CtxDbgMsg( "SetPanelEventJS, panel = %x (%s), parent is %x(%s)\n", this, GetID(), pJSContext, pJSContext->GetID() );

	// handle script/func param
	v8::Persistent<v8::Script> *pScript = NULL;
	v8::Persistent<v8::Function> *pJSFunc = NULL;
	if ( args[1]->IsFunction() )
	{
		v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast( args[1] );
		pJSFunc = new v8::Persistent<v8::Function>( args.GetIsolate(), func );
	}

	if ( args[1]->IsString() )
	{
		v8::String::Utf8Value strScript(v8::Isolate::GetCurrent(),  args[1] );
		const char *pchScript = *strScript;
		if ( pchScript )
		{
			pScript = UIEngineInternal()->CompileScript( pJSContext, pchScript, CFmtStr1024( "%s#%s - %s", m_pClientPtr->GetPanelType().String(), GetID() ? GetID() : "(undefined)", symEvent.String() ).String() );
			if ( pScript->IsEmpty() )
				pScript = NULL;			
		}
	}

	if ( !pScript && !pJSFunc )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "SetPanelEvent's second argument must be a function or JS script" ).ToLocalChecked() );
		return;
	}

	SetPanelEventInternal( symEvent, NULL, pScript, pJSFunc, pJSContext );
}

//-----------------------------------------------------------------------------
// Purpose: Clears the named panel event on the selected panel
//-----------------------------------------------------------------------------
void CUIPanel::ClearPanelEventJS( CPanoramaSymbol symPanelEvent )
{
	ClearPanelEvents( symPanelEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Set a panel event
//-----------------------------------------------------------------------------
void CUIPanel::SetPanelEvent( CPanoramaSymbol symPanelEvent, IUIEvent *pEvent )
{
	VecUIEvents_t *pvec = NULL;
	if ( pEvent )
	{
		pvec = new VecUIEvents_t();
		pvec->AddToTail( pEvent );
	}

	SetPanelEvent( symPanelEvent, pvec );
}


//-----------------------------------------------------------------------------
// Purpose: Stores events to be dispatched when a specific panel event occurs
//-----------------------------------------------------------------------------
void CUIPanel::SetPanelEvent( CPanoramaSymbol symPanelEvent, VecUIEvents_t *pvecEvents )
{
	SetPanelEventInternal( symPanelEvent, pvecEvents, NULL, NULL, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: update appropriate internal flags when adopting a new child
//-----------------------------------------------------------------------------
void CUIPanel::AddChildFlagsHelper( CUIPanel *pChild )
{
	if( m_unStyleFlags & k_EStyleFlagParentDisabled || m_unStyleFlags & k_EStyleFlagDisabled )
		pChild->m_unStyleFlags |= k_EStyleFlagParentDisabled;

	if( pChild->m_unPanelLayoutFlags & k_EPanelLayoutStylesDirty )
	{
		m_unPanelLayoutFlags |= k_EPanelLayoutChildStylesDirty;
		SetLayoutFlagsOnParents( k_EPanelLayoutChildStylesDirty );
	}

	if( pChild->m_unPanelLayoutFlags & k_EPanelLayoutPositionDirty )
	{
		m_unPanelLayoutFlags |= k_EPanelLayoutChildPositionDirty;
		SetLayoutFlagsOnParents( k_EPanelLayoutChildPositionDirty );
	}

	if( pChild->m_unPanelLayoutFlags & k_EPanelLayoutSizeDirty )
	{
		m_unPanelLayoutFlags |= k_EPanelLayoutChildSizeDirty;
		SetLayoutFlagsOnParents( k_EPanelLayoutChildSizeDirty );
	}

	if( pChild->m_unPanelLayoutFlags & k_EPanelLayoutSizeTransitionActive )
	{
		m_unPanelLayoutFlags |= k_EPanelLayoutChildSizeTransitionActive;
		SetLayoutFlagsOnParents( k_EPanelLayoutChildSizeTransitionActive );
	}

	if( pChild->m_unPanelLayoutFlags & k_EPanelLayoutPositionTransitionActive )
	{
		m_unPanelLayoutFlags |= k_EPanelLayoutChildPositionTransitionActive;
		SetLayoutFlagsOnParents( k_EPanelLayoutChildPositionTransitionActive );
	}
}


//-----------------------------------------------------------------------------
// Purpose: if true always send is mouse move events to this panel when visible even if it isn't the hovers panel
//-----------------------------------------------------------------------------
void CUIPanel::SetMouseTracking( bool bState )
{
	m_bMouseTracking = bState;
	if( bState )
		GetParentWindow()->UIWindowInput()->AddMouseTrackingPanel( this );
	else
		GetParentWindow()->UIWindowInput()->RemoveMouseTrackingPanel( this );

}


//-----------------------------------------------------------------------------
// Purpose: Add this child
//-----------------------------------------------------------------------------
void CUIPanel::AddChild( IUIPanel *pChild )
{
#ifdef _DEBUG
	if( m_vecChildren.HasElement( pChild ) )
	{
		AssertMsg( false, "Child added to parent twice" );
		return;
	}
#endif

	// Should already be a child of the panel previous to calling AddChild(), if you hit this, maybe you really wanted
	// to call SetParent() on the child rather than AddChild() on the children.  Or, more likely, you just shoudln't be
	// manipulating this stuff directly.
	Assert( pChild->GetParent() == this );

	ClientPtr()->OnBeforeChildrenChanged();

	AddChildFlagsHelper( (CUIPanel*)pChild );

	m_vecChildren.AddToTail( pChild );

	CUIPanel *pUIChild = (CUIPanel*)pChild;
	if( (pChild->BHasKeyFocus() || pChild->BHasDescendantKeyFocus()) && !pChild->BTopOfInputContext() )
	{
		pUIChild->AddFlagToParents( k_EStyleFlagDescendantFocused );
	}

	// take care of ready for display counts
	if ( pUIChild->m_cReadyForDisplayChildren > 0 || pUIChild->m_bRegisteredForReadyEvents )
	{
		int nDelta = pUIChild->m_cReadyForDisplayChildren;
		if ( pUIChild->m_bRegisteredForReadyEvents )
			nDelta++;

		IncrementAncestorReadyForDisplay( nDelta );
	}

	// also need to set the panel's initial ready state
	pUIChild->DispatchReadyForDisplayTraverse( BReadyForDisplay() );
	
	if ( m_pHorizontalScrollData )
	{
		m_pHorizontalScrollData->m_bDispatchedScrollMax = false;
	}
	if ( m_pVerticalScrollData )
	{
		m_pVerticalScrollData->m_bDispatchedScrollMax = false;
	}

	InvalidateSizeAndPosition();
	RecalculateUIScale( false, nullptr );

	ClientPtr()->OnAfterChildrenChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Adds a style flag to all parents
//-----------------------------------------------------------------------------
void CUIPanel::AddFlagToParents( EStyleFlags eStyleFlag )
{
	CUIPanel *pParent = (CUIPanel*)m_pParent;
	while( pParent )
	{
		pParent->AddStyleFlag( eStyleFlag );

		// Stop if we reach the top of an input context and the flag is related to input
		if( pParent->BTopOfInputContext() && eStyleFlag == k_EStyleFlagDescendantFocused )
			break;

		pParent = (CUIPanel*)pParent->m_pParent;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes a style flag from all parents
//-----------------------------------------------------------------------------
void CUIPanel::RemoveFlagFromParents( EStyleFlags eStyleFlag )
{
	CUIPanel *pParent = (CUIPanel*)m_pParent;
	while( pParent )
	{
		pParent->RemoveStyleFlag( eStyleFlag );

		// Stop if we reach the top of an input context and the flag is related to input
		if( pParent->BTopOfInputContext() && eStyleFlag == k_EStyleFlagDescendantFocused )
			break;

		pParent = (CUIPanel*)pParent->m_pParent;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remove a specified child
//-----------------------------------------------------------------------------
void CUIPanel::RemoveChild( IUIPanel *pIChild )
{
	CUIPanel *pChild = (CUIPanel*)pIChild;
	if ( (pChild->BHasKeyFocus() || pChild->BHasDescendantKeyFocus()) && !pChild->BTopOfInputContext() )
	{
		pChild->RemoveFlagFromParents( k_EStyleFlagDescendantFocused );
	}

	// take care of unload when visible book keeping
	if ( pChild->m_cReadyForDisplayChildren > 0 || pChild->m_bRegisteredForReadyEvents )
	{
		int nDelta = pChild->m_cReadyForDisplayChildren;
		if ( pChild->m_bRegisteredForReadyEvents )
			nDelta++;

		IncrementAncestorReadyForDisplay( -nDelta );
	}
	
	ClientPtr()->OnBeforeChildrenChanged();

	ClientPtr()->OnRemoveChild( pChild );

	if( pChild->m_unStyleFlags & k_EStyleFlagParentDisabled )
		pChild->m_unStyleFlags &= ~(k_EStyleFlagParentDisabled);

	// child order is important so can't fast remove
	m_vecChildren.FindAndRemove( pChild );

	if( m_pVecChildrenInHiding )
		m_pVecChildrenInHiding->FindAndRemove( pChild );

	InvalidateSizeAndPosition();
	RecalculateUIScale( false, nullptr );

	ClientPtr()->OnAfterChildrenChanged();
}

typedef panorama::IUIPanel * PanelPtr_t;
static bool( __cdecl *g_pfnLessFunc_t )(ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2);
bool PanelLessFunc( PanelPtr_t const &p1, PanelPtr_t const &p2 )
{
	return g_pfnLessFunc_t( p1->ClientPtr(), p2->ClientPtr() );
}

bool PanelLessFuncCtx( PanelPtr_t const &p1, PanelPtr_t const &p2, void *pCtx )
{
	return g_pfnLessFunc_t( p1->ClientPtr(), p2->ClientPtr() );
}

//-----------------------------------------------------------------------------
// Purpose: Add this child in sorted order, assumes all children are currently sorted already
//-----------------------------------------------------------------------------
int CUIPanel::AddChildSorted( bool( __cdecl *pfnLessFunc )(ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2), IUIPanel *pChild )
{
#ifdef _DEBUG
	if( m_vecChildren.HasElement( pChild ) )
	{
		AssertMsg( false, "Child added to parent twice" );
		return -1;
	}
#endif

	CUIPanel *pUIChild = ( CUIPanel * )pChild;

	ClientPtr()->OnBeforeChildrenChanged();

	AddChildFlagsHelper( pUIChild );

	g_pfnLessFunc_t = pfnLessFunc;
#if defined( SOURCE2_PANORAMA )
	int iResult = m_vecChildren.SortedInsert( pChild, PanelLessFuncCtx, NULL );
#else
	int iResult = m_vecChildren.SortedInsert( pChild, PanelLessFunc );
#endif

	if( (pChild->BHasKeyFocus() || pChild->BHasDescendantKeyFocus()) && !pChild->BTopOfInputContext() )
	{
		((CUIPanel*)pChild)->AddFlagToParents( k_EStyleFlagDescendantFocused );
	}

	InvalidateSizeAndPosition();
	pUIChild->RecalculateUIScale( false, nullptr );

	ClientPtr()->OnAfterChildrenChanged();

	return iResult;
}

//-----------------------------------------------------------------------------
// Purpose: resort a particular child, assumes all other children are currently sorted already
//-----------------------------------------------------------------------------
int CUIPanel::ReSortChild( bool( __cdecl *pfnLessFunc )( ClientPanelPtr_t const &p1, ClientPanelPtr_t const &p2 ), IUIPanel *pChild )
{
#ifdef _DEBUG
	if ( !m_vecChildren.HasElement( pChild ) )
	{
		AssertMsg( false, "pChild is not a child of this panel" );
		return -1;
	}
#endif

	ClientPtr()->OnBeforeChildrenChanged();

	g_pfnLessFunc_t = pfnLessFunc;

	m_vecChildren.FindAndRemove( pChild );

#if defined( SOURCE2_PANORAMA )
	int iResult = m_vecChildren.SortedInsert( pChild, PanelLessFuncCtx, NULL );
#else
	int iResult = m_vecChildren.SortedInsert( pChild, PanelLessFunc );
#endif

	InvalidateSizeAndPosition();

	ClientPtr()->OnAfterChildrenChanged();

	return iResult;
}


//-----------------------------------------------------------------------------
// Purpose: Sort children
//-----------------------------------------------------------------------------
void CUIPanel::SortChildren( std::function< int( IUIPanelClient *, IUIPanelClient * ) > fnCompare )
{
	ClientPtr()->OnBeforeChildrenChanged();
	m_vecChildren.SortPredicate( [=]( IUIPanel * const &p1, IUIPanel * const &p2 ) {
		// SortChildren takes a compare function, but SortPredicate takes a
		// less-function, so we need to convert.
		return ( fnCompare( p1->ClientPtr(), p2->ClientPtr() ) < 0 );
	} );
	ClientPtr()->OnAfterChildrenChanged();
	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Moves the position of a specified child to directly after another
//-----------------------------------------------------------------------------
void CUIPanel::MoveChildAfter( IUIPanel *pChildToMove, IUIPanel *pBefore )
{
	int iToMove = m_vecChildren.Find( pChildToMove );
	int iBefore = m_vecChildren.Find( pBefore );

	if( iToMove == iBefore )
		return;

	if( iToMove == m_vecChildren.InvalidIndex() || iBefore == m_vecChildren.InvalidIndex() )
	{
		AssertMsg( false, "Trying to rearrange panels which are not both direct children" );
		return;
	}

	ClientPtr()->OnBeforeChildrenChanged();
	m_vecChildren.Remove( iToMove );
	if( iToMove < iBefore )
		iBefore--;

	m_vecChildren.InsertAfter( iBefore, pChildToMove );
	InvalidateSizeAndPosition();

	ClientPtr()->OnAfterChildrenChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Moves the position of a specified child to directly before another
//-----------------------------------------------------------------------------
void CUIPanel::MoveChildBefore( IUIPanel *pChildToMove, IUIPanel *pAfter )
{
	int iToMove = m_vecChildren.Find( pChildToMove );
	int iAfter = m_vecChildren.Find( pAfter );

	if( iToMove == iAfter )
		return;

	if( iToMove == m_vecChildren.InvalidIndex() || iAfter == m_vecChildren.InvalidIndex() )
	{
		AssertMsg( false, "Trying to rearrange panels which are not both direct children" );
		return;
	}

	ClientPtr()->OnBeforeChildrenChanged();
	m_vecChildren.Remove( iToMove );
	if( iToMove < iAfter )
		iAfter--;

	m_vecChildren.InsertBefore( iAfter, pChildToMove );
	InvalidateSizeAndPosition();

	ClientPtr()->OnAfterChildrenChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Remove all children
//-----------------------------------------------------------------------------
void CUIPanel::RemoveAndDeleteChildren()
{
	VPROF_BUDGET( "CUIPanel::RemoveAndDeleteChildren", VPROF_BUDGETGROUP_TENFOOT );
	Assert( m_bDeletingChildren == false );
	if ( m_bDeletingChildren == true )
		return; // we are already deleting, bail here rather than crashing

	if ( m_vecChildren.Count() )
	{
		m_bDeletingChildren = true;

		ClientPtr()->OnBeforeChildrenChanged();
		FOR_EACH_VEC_BACK( m_vecChildren, i )
		{
			CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];

			// can use below to test unload when invisible ref counts (also then comment IncrementAncestor at end of function)		
			//if ( pChild->m_bUnloadWhenInvisible )
			//	IncrementAncestorUnloadWhenInvisible( -1 );
		
			pChild->ClientPtr()->OnDeletePanel();
		
			m_vecChildren.FastRemove( i );
		}

		m_bDeletingChildren = false;

		InvalidateSizeAndPosition();

		ClientPtr()->OnAfterChildrenChanged();
	}

	// this code doesn't call RemoveChild() (bug??). Need to update unload if invisible.
	IncrementAncestorReadyForDisplay( -m_cReadyForDisplayChildren );
}


//-----------------------------------------------------------------------------
// Purpose: Remove all children matching given panel type
//-----------------------------------------------------------------------------
uint32 CUIPanel::GetChildCountOfType( CPanoramaSymbol symPanelType )
{
	uint32 unCount = 0;
	FOR_EACH_VEC_BACK( m_vecChildren, i )
	{
		if( m_vecChildren[i]->ClientPtr()->GetPanelType() == symPanelType )
		{
			++unCount;
		}
	}

	return unCount;
}


//-----------------------------------------------------------------------------
// Purpose: Return index of child in creation/panel vector order (also default tab order)
//-----------------------------------------------------------------------------
int CUIPanel::GetChildIndex( const IUIPanel *pChild ) const
{
	FOR_EACH_VEC( m_vecChildren, i )
	{
		if( m_vecChildren[i] == pChild )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Number of special hidden child panels
//-----------------------------------------------------------------------------
int CUIPanel::GetHiddenChildCount() const
{
	if ( m_pVecChildrenInHiding )
	{
		return m_pVecChildrenInHiding->Count();
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Return special hidden child panel
//-----------------------------------------------------------------------------
IUIPanel * CUIPanel::GetHiddenChild( int i )
{
	if ( m_pVecChildrenInHiding && m_pVecChildrenInHiding->IsValidIndex( i ))
	{
		return m_pVecChildrenInHiding->Element( i );
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Remove all children matching given panel type
//-----------------------------------------------------------------------------
void CUIPanel::RemoveAndDeleteChildrenOfType( CPanoramaSymbol symPanelType )
{
	VPROF_BUDGET( "CUIPanel::RemoveAndDeleteChildrenOfType", VPROF_BUDGETGROUP_TENFOOT );
	m_bDeletingChildren = true;

	ClientPtr()->OnBeforeChildrenChanged();
	FOR_EACH_VEC_BACK( m_vecChildren, i )
	{
		if( m_vecChildren[i]->ClientPtr()->GetPanelType() == symPanelType )
		{
			IUIPanel *pChild = m_vecChildren[i];
			pChild->ClientPtr()->OnDeletePanel();

			m_vecChildren.Remove( i );
		}
	}

	m_bDeletingChildren = false;
	InvalidateSizeAndPosition();

	ClientPtr()->OnAfterChildrenChanged();
}


//-----------------------------------------------------------------------------
// Purpose: Sets flag to let parent know a style just added 'position' as a transition property and to not transition
//-----------------------------------------------------------------------------
void CUIPanel::TransitionPositionApplied( bool bImmediate )
{
	if ( !bImmediate )
		m_unPanelLayoutFlags |= k_EPanelLayoutSkipLayoutPositionTransition;
}


//-----------------------------------------------------------------------------
// Purpose: Flag panel position as invalid and sets appropriate flags on parents
//-----------------------------------------------------------------------------
void CUIPanel::InvalidatePosition()
{
	m_unPanelLayoutFlags |= k_EPanelLayoutPositionDirty;
	SetLayoutFlagsOnParents( k_EPanelLayoutChildPositionDirty );
}


//-----------------------------------------------------------------------------
// Purpose: Flag panel size & position as invalid and sets appropriate flags on parents
//-----------------------------------------------------------------------------
void CUIPanel::InvalidateSizeAndPosition()
{
	m_unPanelLayoutFlags |= (k_EPanelLayoutPositionDirty | k_EPanelLayoutSizeDirty);
	SetLayoutFlagsOnParents( k_EPanelLayoutChildPositionDirty | k_EPanelLayoutChildSizeDirty );
}


//-----------------------------------------------------------------------------
// Purpose: Clear panel position flags
//-----------------------------------------------------------------------------
void CUIPanel::ClearLayoutPositionFlags()
{
	m_unPanelLayoutFlags &= ~(k_EPanelLayoutPositionDirty | k_EPanelLayoutChildPositionDirty);
}


//-----------------------------------------------------------------------------
// Purpose: Clear panel size flags
//-----------------------------------------------------------------------------
void CUIPanel::ClearLayoutSizeFlags()
{
	m_unPanelLayoutFlags &= ~(k_EPanelLayoutSizeDirty | k_EPanelLayoutChildSizeDirty);
}


//-----------------------------------------------------------------------------
// Purpose: Set layout flags on parent if we have one
//-----------------------------------------------------------------------------
void CUIPanel::SetLayoutFlagsOnParents( uint32 unPanelLayoutFlags )
{
	CUIPanel *pParent = (CUIPanel*)GetParent();
	if( pParent )
		pParent->SetChildLayoutFlags( unPanelLayoutFlags );
}


//-----------------------------------------------------------------------------
// Purpose: Called by child to set panel layout flags
//-----------------------------------------------------------------------------
void CUIPanel::SetChildLayoutFlags( uint32 unPanelLayoutFlags )
{
	// check if already set, can stop here if so (our parents should also have these flags as we bubble the call)
	if( (m_unPanelLayoutFlags & unPanelLayoutFlags) == unPanelLayoutFlags )
		return;

	// intended to only be used on child layout flags
#ifdef _DEBUG
	Assert( (unPanelLayoutFlags & ~(k_EPanelLayoutChildPositionDirty | k_EPanelLayoutChildSizeDirty | k_EPanelLayoutChildSizeTransitionActive | k_EPanelLayoutChildPositionTransitionActive | k_EPanelLayoutChildStylesDirty)) == 0 );
#endif

	// at least 1 new flag
	m_unPanelLayoutFlags |= unPanelLayoutFlags;
	SetLayoutFlagsOnParents( unPanelLayoutFlags );
}


//-----------------------------------------------------------------------------
// Purpose: Marks panel as having an active transition that affects size and position
//-----------------------------------------------------------------------------
void CUIPanel::SetActiveSizeAndPositionTransition()
{
	m_unPanelLayoutFlags |= (k_EPanelLayoutSizeTransitionActive | k_EPanelLayoutPositionTransitionActive);
	SetLayoutFlagsOnParents( k_EPanelLayoutChildSizeTransitionActive | k_EPanelLayoutChildPositionTransitionActive );
}


//-----------------------------------------------------------------------------
// Purpose: Marks panel as having an active transition that affects position
//-----------------------------------------------------------------------------
void CUIPanel::SetActivePositionTransition()
{
	m_unPanelLayoutFlags |= k_EPanelLayoutPositionTransitionActive;
	SetLayoutFlagsOnParents( k_EPanelLayoutChildPositionTransitionActive );
}


//-----------------------------------------------------------------------------
// Purpose: Clear layout transition flags on ourselves and parents
//-----------------------------------------------------------------------------
void CUIPanel::ClearLayoutTransitionFlagsBubble( uint32 unFlags )
{
	// check if already set, can stop here if so (our parents should also have these flags as we bubble the call)
	if( (m_unPanelLayoutFlags & unFlags) == 0 )
		return;

	// clear position and size flags
	m_unPanelLayoutFlags &= ~(unFlags);

	// tell parent
	CUIPanel *pParent = (CUIPanel*)GetParent();
	if( pParent )
	{
		uint unChildFlags = 0;
		if( unFlags & k_EPanelLayoutSizeTransitionActive )
			unChildFlags |= k_EPanelLayoutChildSizeTransitionActive;

		if( unFlags & k_EPanelLayoutPositionTransitionActive )
			unChildFlags |= k_EPanelLayoutChildPositionTransitionActive;

		pParent->OnChildLayoutTransitionFlagsCleared( unChildFlags );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Child's layout animation and transition flags were cleared
//-----------------------------------------------------------------------------
void CUIPanel::OnChildLayoutTransitionFlagsCleared( uint32 unFlags )
{
	// reset flags
	m_unPanelLayoutFlags &= ~(unFlags);

	// loop through our children, setting the appropriate remaining children flags
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if( pChild->IsSizeTransitioning() )
		{
			m_unPanelLayoutFlags |= (k_EPanelLayoutChildSizeTransitionActive | k_EPanelLayoutChildPositionTransitionActive);
			break;
		}

		if( pChild->IsPositionTransitioning() )
			m_unPanelLayoutFlags |= k_EPanelLayoutChildPositionTransitionActive;
	}

	uint unChildFlags = 0;
	if( unFlags & k_EPanelLayoutChildSizeTransitionActive && !(m_unPanelLayoutFlags & k_EPanelLayoutChildSizeTransitionActive) )
		unChildFlags |= k_EPanelLayoutChildSizeTransitionActive;

	if( unFlags & k_EPanelLayoutChildPositionTransitionActive && !(m_unPanelLayoutFlags & k_EPanelLayoutChildPositionTransitionActive) )
		unChildFlags |= k_EPanelLayoutChildPositionTransitionActive;

	// tell parent
	CUIPanel *pParent = (CUIPanel*)GetParent();
	if( pParent )
		pParent->OnChildLayoutTransitionFlagsCleared( unChildFlags );
}


//-----------------------------------------------------------------------------
// Purpose: If the parent panel has scrolling in either direction, make sure it 
// scrolls to fit the contents of this panel
//-----------------------------------------------------------------------------
void CUIPanel::ScrollParentToMakePanelFit( ScrollBehavior_t behavior, bool bImmediateScroll )
{
	if ( !m_pParent )
		return;

	// if panel or parent hasn't been laid out, this will fail. Delay till next frame
	if ( !BHasBeenLayedOut() || !m_pParent->BHasBeenLayedOut() )
	{
		if ( m_pOnLayoutEvent )
		{
			delete m_pOnLayoutEvent;
		}
		m_pOnLayoutEvent = ScrollPanelIntoView::MakeEvent( ClientPtr(), behavior, bImmediateScroll );
		return;
	}

	// include padding
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetMargin( m_flActualLayoutWidth, m_flActualLayoutHeight, flLeft, flTop, flRight, flBottom );

	float flX = GetActualXOffset();
	float flY = GetActualYOffset();
	m_pParent->ScrollToFitRegion( flX - flLeft, flX + m_flActualLayoutWidth + flRight, flY - flTop, flY + m_flActualLayoutHeight + flBottom, 
		behavior, false, bImmediateScroll );
}


//-----------------------------------------------------------------------------
// Purpose: Does this panel appear in the parent's active scroll area?
//-----------------------------------------------------------------------------
bool CUIPanel::BCanSeeInParentScroll()
{
	if( m_pParent )
	{
		PanoramaRect_t rectChildAreaToPaint;		
		PanoramaRect_t rectPaint;
		CUIPanel *pParent = (CUIPanel*)m_pParent;
		pParent->GetPaintArea( &rectPaint );
		return pParent->BShouldDrawChild( &rectChildAreaToPaint, this, rectPaint );
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called by horizontal/vertical scrollbars whenever one of them is moved
//-----------------------------------------------------------------------------
void CUIPanel::OnScrollPositionChanged()
{
	if ( !m_bSendChildScrolledIntoViewEvents )
		return;

	DispatchEventAsync( CheckChildrenScrolledIntoView(), this );
}

//-----------------------------------------------------------------------------
// Purpose: Checks if any children have moved in/out of view and dispatches events to them
//-----------------------------------------------------------------------------
bool CUIPanel::OnCheckChildrenScrolledIntoView()
{
	float flScrollOffsetX = m_pHorizontalScrollBar ? m_pHorizontalScrollBar->GetScrollWindowPosition() : 0;
	float flScrollOffsetY = m_pVerticalScrollBar ? m_pVerticalScrollBar->GetScrollWindowPosition() : 0;
	float flRight = flScrollOffsetX + m_flActualLayoutWidth;
	float flBottom = flScrollOffsetY + m_flActualLayoutHeight;

	// artificially pad the "in view" area
	flScrollOffsetX -= m_flActualLayoutWidth * 0.5f;
	flScrollOffsetY -= m_flActualLayoutHeight * 0.5f;
	flRight += m_flActualLayoutWidth * 0.5f;
	flBottom += m_flActualLayoutHeight * 0.5f;

	OnCheckChildrenScrolledIntoViewRecursive( flScrollOffsetX, flRight, flScrollOffsetY, flBottom );
	
	return true;
}

void CUIPanel::OnCheckChildrenScrolledIntoViewRecursive( float x0, float x1, float y0, float y1 )
{
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)(m_vecChildren[i]);

		const float flChildX0 = pChild->GetActualXOffset();
		const float flChildX1 = flChildX0 + pChild->m_flActualLayoutWidth;
		const float flChildY0 = pChild->GetActualYOffset();
		const float flChildY1 = flChildY0 + pChild->m_flActualLayoutHeight;
		const bool bOverlap = flChildX0 <= x1 && flChildX1 >= x0
						&& flChildY0 <= y1 && flChildY1 >= y0;

		bool bChanged = false;
		if ( pChild->IsScrolledIntoView() != bOverlap)
		{
			bChanged = true;
			if ( bOverlap )
			{
				pChild->FireScrolledIntoViewEvent();
			}
			else
			{
				pChild->FireScrolledOutOfViewEvent();
			}
		}
				
		if ( pChild->m_bSendChildScrolledIntoViewEvents && ( bChanged || pChild->IsScrolledIntoView() ) )
		{
			pChild->OnCheckChildrenScrolledIntoViewRecursive( Max( x0, flChildX0 ) - flChildX0, Min( x1, flChildX1 ) - flChildX0,
				Max( y0, flChildY0 ) - flChildY0, Min( y1, flChildY1 ) - flChildY0 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: send an event that this panel has just been scrolled into view
//-----------------------------------------------------------------------------
void CUIPanel::FireScrolledIntoViewEvent()
{
	m_bScrolledIntoView = true;
	DispatchEvent( ScrolledIntoView(), this );
}

//-----------------------------------------------------------------------------
// Purpose: send an event that this panel has just been scrolled out of view
//-----------------------------------------------------------------------------
void CUIPanel::FireScrolledOutOfViewEvent()
{
	m_bScrolledIntoView = false;
	DispatchEvent( ScrolledOutOfView(), this );
}

//-----------------------------------------------------------------------------
// Purpose: notify children when they come into view for the first time
//-----------------------------------------------------------------------------
void CUIPanel::SetSendChildScrolledIntoViewEvents( bool bSendChildScrolledIntoViewEvents )
{
	m_bSendChildScrolledIntoViewEvents = bSendChildScrolledIntoViewEvents;
}

//-----------------------------------------------------------------------------
// Purpose: Prepares a panel for reload
//-----------------------------------------------------------------------------
void CUIPanel::DeletePanelsForReloadTraverse( CPanoramaSymbol symPath, CUtlVector< IUIPanel * > *pvecPanelsWithID )
{
	// delete any panels created from the specified layout file. If
	FOR_EACH_VEC_BACK( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if( pChild->GetLayoutFileLoadedFrom() != symPath )
			continue;

		if ( !pChild->m_strLayoutSnippet.IsEmpty() )
			continue;

		// first traverse child to see if any of its children have panel ids
		pChild->DeletePanelsForReloadTraverse( symPath, pvecPanelsWithID );

		// if child doesn't have an ID, should be able to delete. Reload will create if panel still exists
		if( pChild->BHasID() )
		{
			// unparent
			pChild->SetParent( NULL );
			pvecPanelsWithID->AddToTail( pChild );
		}
		else
		{
			pChild->ClientPtr()->OnDeletePanel();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: reloads a layout file for this object
//-----------------------------------------------------------------------------
bool CUIPanel::BReloadLayout( CPanoramaSymbol symPath )
{
	bool bReload = ( ( m_bLoadedLayoutFile || !m_strLayoutSnippet.IsEmpty() ) && GetLayoutFile() == symPath );
	CUtlVector< IUIPanel * > vecPanelsWithID;
	bool bHadFocus = (BHasDescendantKeyFocus() || BHasKeyFocus());

	// if our layout file, need to remove children previously created by the layout file
	if( bReload )
	{
		ClientPtr()->OnLayoutReloading();

		// find all children loaded from our layout file or created from code. Rest can be deleted as hopefully
		// no code has a pointer to those panels
		DeletePanelsForReloadTraverse( symPath, &vecPanelsWithID );

		UIEngineInternal()->DeleteScriptContext( this );

		Msg( "Panel %s is reloading layout %s\n", GetID(), symPath.String() );
	}

	if( symPath == GetLayoutFile() )
	{
		// bugbug cboyd - fix up so we dont lose values set in code
		m_style.Clear();
	}

	// pass reload request to any remaining children before we recreate children from layout file
	FOR_EACH_VEC( m_vecChildren, i )
	{
		m_vecChildren[i]->BReloadLayout( symPath );
	}

	if( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( (*m_pVecChildrenInHiding), i )
		{
			((CUIPanel*)m_pVecChildrenInHiding->Element( i ))->BReloadLayout( symPath );
		}
	}

	// if our layout file, reapply
	if( bReload )
	{
		LayoutFilePtr_t pLayoutFile = UIEngineInternal()->UILayoutManagerInternal()->GetCLayoutFile( symPath );
		if( !pLayoutFile )
		{
			// delete any panels which were not used
			FOR_EACH_VEC( vecPanelsWithID, i )
			{
				vecPanelsWithID[i]->ClientPtr()->OnDeletePanel();
			}
			vecPanelsWithID.Purge();
			return false;
		}

		bool bApplyLayoutSuccess = false;
		if ( m_strLayoutSnippet.IsEmpty() )
		{
			bApplyLayoutSuccess = BApplyLayoutFile( pLayoutFile, &vecPanelsWithID, true );
		}
		else
		{
			bApplyLayoutSuccess = BApplyLayoutSnippet( pLayoutFile, m_strLayoutSnippet.Get(), &vecPanelsWithID );
		}

		if( !bApplyLayoutSuccess )
		{
			// delete any panels which were not used
			FOR_EACH_VEC( vecPanelsWithID, i )
			{
				vecPanelsWithID[i]->ClientPtr()->OnDeletePanel();
			}
			vecPanelsWithID.Purge();
			return false;
		}
	}

	// delete any panels which were not used
	FOR_EACH_VEC( vecPanelsWithID, i )
	{
		vecPanelsWithID[i]->ClientPtr()->OnDeletePanel();
	}
	vecPanelsWithID.Purge();

	bool bStillHasFocus = (BHasDescendantKeyFocus() || BHasKeyFocus());
	if ( bReload && bHadFocus && !bStillHasFocus )
		SetFocus();

	if ( bReload )
	{
		ClientPtr()->OnLayoutReloaded();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Received an event to reload specific style file
//-----------------------------------------------------------------------------
void CUIPanel::ReloadStyleFileTraverse( CPanoramaSymbol symPath )
{
	if ( m_pLayoutFile.Get() && m_pLayoutFile->BContainsStyleFile( symPath ) )
	{
		//Msg( "Panel %s is reloading styles %s\n", m_strID.String(), symPath.String() );

		if ( g_ConVarReloadAnimations.GetBool() )
			m_style.ResetAnimations();

		// no need to apply style classes to children. They will also receive this event
		MarkStylesDirty( false );
		for ( int i = 0; i < GetChildCount(); i++ )
			GetChild( i )->MarkStylesDirty( false );
	}

	for( int i = 0; i < GetChildCount(); i++ )
		GetChild( i )->ReloadStyleFileTraverse( symPath );

	if( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( (*m_pVecChildrenInHiding), i )
		{
			((CUIPanel*)m_pVecChildrenInHiding->Element( i ))->ReloadStyleFileTraverse( symPath );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: number of child panels
//-----------------------------------------------------------------------------
int CUIPanel::GetChildCount() const
{
	return m_vecChildren.Count();
}


//-----------------------------------------------------------------------------
// Purpose: get this child
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::GetChild( int i ) const
{
	if( m_vecChildren.IsValidIndex( i ) )
		return m_vecChildren[i];
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: get first child
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::GetFirstChild() const
{
	return GetChild( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: get last child
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::GetLastChild() const
{
	return GetChild( GetChildCount() - 1 );
}



//-----------------------------------------------------------------------------
// Purpose: Checks if an event is set for on activate
//-----------------------------------------------------------------------------
bool CUIPanel::BHasOnActivateEvent()
{
	return m_pmapPanelEvents ? m_pmapPanelEvents->Find( k_symPropertyOnActivate ) != m_pmapPanelEvents->InvalidIndex() : false;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if an event is set for onmouseactivate
//-----------------------------------------------------------------------------
bool CUIPanel::BHasOnMouseActivateEvent()
{
	return m_pmapPanelEvents ? m_pmapPanelEvents->Find( k_symPropertyOnMouseActivate ) != m_pmapPanelEvents->InvalidIndex() : false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles layout file being ready for use after potentially async loading
//-----------------------------------------------------------------------------
void CUIPanel::OnGetLayoutFileAsyncComplete( LayoutFilePtr_t pLayoutFile, ELoadLayoutAsyncDetails eDetails, bool bPartialLayout )
{
	OnLoadLayoutAsyncCompleteInternal( pLayoutFile, eDetails, bPartialLayout );
}


//-----------------------------------------------------------------------------
// Purpose: Internal handler for async complete of loading layout file
//-----------------------------------------------------------------------------
void CUIPanel::OnLoadLayoutAsyncCompleteInternal( LayoutFilePtr_t pLayoutFile, ELoadLayoutAsyncDetails eDetails, bool bPartialLayout )
{
	if ( pLayoutFile.Get() )
	{
		if( !bPartialLayout )
			m_bLoadedLayoutFile = true;

		RemoveAndDeleteChildren();
		UIEngineInternal()->DeleteScriptContext( this );

		LayoutFilePtr_t pPriorLayoutFile = m_pLayoutFile;

		// Change our layout file, which changes where we get styles from, so styles are now dirty
		if( !bPartialLayout )
			m_pLayoutFile = pLayoutFile;

		MarkStylesDirty( true );

		if( !BApplyLayoutFile( pLayoutFile, NULL, false ) )
		{
			m_bLoadedLayoutFile = false;
			m_pLayoutFile = pPriorLayoutFile;
			MarkStylesDirty( true );

			AddStyleFlag( k_EStyleFlagLayoutLoadFailed );
			AssertMsg1( false, "Couldn't apply layout file %s", pLayoutFile->GetLayoutFileSymbol().String() );
		}
	}
	else
	{
		AddStyleFlag( k_EStyleFlagLayoutLoadFailed );
	}
	RemoveStyleFlag( k_EStyleFlagLayoutLoading );

	DispatchEventAsync( LoadAsyncComplete(), this, pLayoutFile != nullptr, eDetails, bPartialLayout );
}


//-----------------------------------------------------------------------------
// Purpose: Measure self and children. First pass of layout
//-----------------------------------------------------------------------------
void CUIPanel::DesiredLayoutSizeTraverse( float flMaxWidth, float flMaxHeight )
{
	DesiredLayoutSizeTraverse( NULL, NULL, flMaxWidth, flMaxHeight, false );
}


//-----------------------------------------------------------------------------
// Purpose: Measure self and children
//
// Params:
// bFinalDimensions - if true, will calculate the final dimensions after all transitions but will NOT set member variables
//-----------------------------------------------------------------------------
void CUIPanel::DesiredLayoutSizeTraverse( float *pflDesiredWidth, float *pflDesiredHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	VPROF_BUDGET_DETAILED( "CUIPanel::DesiredLayoutSizeTraverse", VPROF_BUDGETGROUP_TENFOOT );

	// if nothing is dirty, can early out
	if( !bFinalDimensions && IsSizeValid() && IsChildSizeValid() && !IsSizeTransitioning() && m_flLastDesiredWidthFromParent == flMaxWidth && m_flLastDesiredHeightFromParent == flMaxHeight )
	{
		if( pflDesiredWidth )
			*pflDesiredWidth = m_flDesiredLayoutWidth;

		if( pflDesiredHeight )
			*pflDesiredHeight = m_flDesiredLayoutHeight;

		return;
	}

	RecalculateUIScale( bFinalDimensions, nullptr );

	Assert( flMaxHeight != k_flFloatAuto );
	Assert( flMaxWidth != k_flFloatAuto );

	SetRepaint( k_EPanelRepaintFull );
	float flInnerWidth = flMaxWidth;
	float flInnerHeight = flMaxHeight;

	CUILength styleWidth, styleHeight;
	CUILength maxWidth, maxHeight;

	{
		VPROF_BUDGET_DETAILED( "CUIPanel::DesiredLayoutSizeTraverse", VPROF_BUDGETGROUP_TENFOOT );

		// subtract margin
		CUILength marginLeft, marginTop, marginRight, marginBottom;
		AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flMaxWidth );
		marginRight.ConvertToLength( flMaxWidth );
		marginTop.ConvertToLength( flMaxHeight );
		marginBottom.ConvertToLength( flMaxHeight );

		if( flMaxWidth != k_flFloatAuto )
			flInnerWidth -= marginLeft.GetValue() + marginRight.GetValue();
		if( flMaxHeight != k_flFloatAuto )
			flInnerHeight -= marginTop.GetValue() + marginBottom.GetValue();

		// need width and height from style
		AccessStyle()->GetInterpolatedWidth( styleWidth, bFinalDimensions );
		AccessStyle()->GetInterpolatedHeight( styleHeight, bFinalDimensions );

		// taking explicit style dimensions over space parent is going to give us...
		if( !styleWidth.IsFitChildren() && !styleWidth.IsFillParentFlow() && !styleWidth.IsHeightPercentage() )
			flInnerWidth = styleWidth.GetValueAsLength( flMaxWidth );
		if( !styleHeight.IsFitChildren() && !styleHeight.IsFillParentFlow() && !styleHeight.IsWidthPercentage() )
			flInnerHeight = styleHeight.GetValueAsLength( flMaxHeight );

		if ( styleWidth.IsHeightPercentage() && styleHeight.IsWidthPercentage() )
			LogLayoutParsingError( m_pLayoutFile->GetLayoutFileSymbol(), 0, CFmtStr1024( "**** DesiredLayoutSizeTraverse for panel %s failed to handle both height and width percentage.", m_strID.String() ).String() );

		if ( styleWidth.IsHeightPercentage() )
			flInnerWidth = styleWidth.GetValue() * flInnerHeight / 100.0f;
		if ( styleHeight.IsWidthPercentage() )
			flInnerHeight = styleHeight.GetValue() * flInnerWidth / 100.0f;

		AccessStyle()->GetInterpolatedMaxWidth( maxWidth, bFinalDimensions );
		AccessStyle()->GetInterpolatedMaxHeight( maxHeight, bFinalDimensions );

		if( maxWidth.IsSet() )
			flInnerWidth = MIN( flInnerWidth, maxWidth.GetValueAsLength( flInnerWidth ) );

		if( maxHeight.IsSet() )
			flInnerHeight = MIN( flInnerHeight, maxHeight.GetValueAsLength( flInnerHeight ) );
	}


	EOverflowValue eOverflowHorizontal, eOverflowVertical;
	AccessStyle()->GetOverflow( eOverflowHorizontal, eOverflowVertical );

	// 256000000.0f here is because we use FLT_MAX as magic "auto" value, and we don't intend that here, just a large value.
	float flTraverseWidth = (eOverflowHorizontal == k_EOverflowScroll || eOverflowHorizontal == k_EOverflowNoClip) ? k_flMaxWidthOrHeight : flInnerWidth;
	float flTraverseHeight = (eOverflowVertical == k_EOverflowScroll || eOverflowVertical == k_EOverflowNoClip) ? k_flMaxWidthOrHeight : flInnerHeight;
	float flContentWidth, flContentHeight;

	ClientPtr()->OnContentSizeTraverse( &flContentWidth, &flContentHeight, flTraverseWidth, flTraverseHeight, bFinalDimensions );

	float flDesiredLayoutWidth = 0.0f;
	float flDesiredLayoutHeight = 0.0f;
	{
		VPROF_BUDGET_DETAILED( "CPanel2D::DesiredLayoutSizeTraverse", VPROF_BUDGETGROUP_TENFOOT );

		if( styleWidth.IsFitChildren() )
		{
			flDesiredLayoutWidth = flContentWidth;
		}
		else if( styleWidth.IsFillParentFlow() )
		{
			flDesiredLayoutWidth = flInnerWidth;
		}
		else
		{
			flDesiredLayoutWidth = flInnerWidth;
		}

		if( styleHeight.IsFitChildren() )
		{
			flDesiredLayoutHeight = flContentHeight;
		}
		else if( styleHeight.IsFillParentFlow() )
		{
			flDesiredLayoutHeight = flInnerHeight;
		}
		else
		{
			flDesiredLayoutHeight = flInnerHeight;
		}

		// update height & width percent. Do this before min/max width; setting both could conflict. min/max wins.
		if ( styleWidth.IsHeightPercentage() )
			flDesiredLayoutWidth = styleWidth.GetValue() * flDesiredLayoutHeight / 100.0f;
		if ( styleHeight.IsWidthPercentage() )
			flDesiredLayoutHeight = styleHeight.GetValue() * flDesiredLayoutWidth / 100.0f;

		// enforce min/max width and height
		CUILength minWidth, minHeight;
		AccessStyle()->GetMinWidth( minWidth );
		AccessStyle()->GetMinHeight( minHeight );
		minWidth.ConvertToLength( flInnerWidth );
		maxWidth.ConvertToLength( flInnerWidth );
		minHeight.ConvertToLength( flInnerHeight );
		maxHeight.ConvertToLength( flInnerHeight );

		if( minWidth.IsSet() && minWidth.GetValue() > flDesiredLayoutWidth )
			flDesiredLayoutWidth = minWidth.GetValue();
		else if( maxWidth.IsSet() && maxWidth.GetValue() < flDesiredLayoutWidth )
			flDesiredLayoutWidth = maxWidth.GetValue();

		if( minHeight.IsSet() && minHeight.GetValue() > flDesiredLayoutHeight )
			flDesiredLayoutHeight = minHeight.GetValue();
		else if( maxHeight.IsSet() && maxHeight.GetValue() < flDesiredLayoutHeight )
			flDesiredLayoutHeight = maxHeight.GetValue();

		// We want to make sure our contents really fit
		flDesiredLayoutHeight = ceil( flDesiredLayoutHeight );
		flDesiredLayoutWidth = ceil( flDesiredLayoutWidth );

		// sanity check measurements
		Assert( flDesiredLayoutWidth != k_flFloatAuto );
		Assert( flDesiredLayoutHeight != k_flFloatAuto );
	}

	if( pflDesiredWidth )
		*pflDesiredWidth = flDesiredLayoutWidth;

	if( pflDesiredHeight )
		*pflDesiredHeight = flDesiredLayoutHeight;

	// only set our member variables if not calculating final dimensions
	if( !bFinalDimensions )
	{
		// sizes have been updated
		ClearLayoutSizeFlags();
		
		AssertMsg( IsFinite( flContentWidth ), "Invalid content width calculated" );
		AssertMsg( IsFinite( flContentHeight ), "Invalid content height calculated" );
				
		m_flContentWidth = flContentWidth;
		m_flContentHeight = flContentHeight;
		m_flDesiredLayoutWidth = flDesiredLayoutWidth;
		m_flDesiredLayoutHeight = flDesiredLayoutHeight;

		m_flLastDesiredWidthFromParent = flMaxWidth;
		m_flLastDesiredHeightFromParent = flMaxHeight;

		// Since we just updated our width/height need to force another layouttraverse on ourselves as well
		InvalidatePosition();
	}
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel is measured
//-----------------------------------------------------------------------------
void CUIPanel::OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	Assert( pflContentHeight && pflContentWidth );

	// need flow direction
	EFlowDirection eFlowDirection;
	AccessStyle()->GetFlowChildren( eFlowDirection );

	// include padding
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flMaxWidth, flMaxHeight, bFinalDimensions, flLeft, flTop, flRight, flBottom );

	CUILength width, height;
	AccessStyle()->GetWidth( width );
	AccessStyle()->GetHeight( height );

	flMaxWidth -= flLeft + flRight;
	flMaxHeight -= flTop + flBottom;

	// if we have children, determine their widths and heights
	float flChildWidth = 0.0f;
	float flChildHeight = 0.0f;
	float flChildWrapOffset = 0.0f;									// where current wrap line starts; y-offset for k_EFlowRightWrap, x-offset for k_EFlowDownWrap
	float flChildWrapRowSize = 0.0f;								// when wrapping, width of current row for k_EFlowRightWrap or height of current column for k_EFlowDownWrap
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if( !pChild->BIsVisible() )
			continue;

		if( eFlowDirection == k_EFlowNone && !width.IsFitChildren() && !height.IsFitChildren() && pChild->BIsTransparent() )
			continue;

		float flChildDesiredWidth, flChildDesiredHeight;
		pChild->DesiredLayoutSizeTraverse( &flChildDesiredWidth, &flChildDesiredHeight, flMaxWidth, flMaxHeight, bFinalDimensions );

		// if child's style width or height is a percentage, don't include in our width calculations as we don't want it to change our size for fit-children
		if ( width.IsFitChildren() )
		{
			CUILength childWidth;
			pChild->AccessStyle()->GetWidth( childWidth );
			if ( childWidth.IsPercent() )
				flChildDesiredWidth = 0.0f;
		}

		if ( height.IsFitChildren() )
		{
			CUILength childHeight;
			pChild->AccessStyle()->GetHeight( childHeight );
			if ( childHeight.IsPercent() )
				flChildDesiredHeight = 0.0f;
		}

		// if our layout is none, we will allow children to position themselves. Otherwise, we override their position
		CUILength xChild;
		CUILength yChild;
		CUILength zChild;
		if ( eFlowDirection == k_EFlowNone )
		{
			pChild->AccessStyle()->GetInterpolatedPosition( xChild, yChild, zChild, bFinalDimensions );
		}
		else
		{
			xChild.SetLength( 0.0f );
			yChild.SetLength( 0.0f );
			zChild.SetLength( 0.0f );
		}

		CUILength marginLeft, marginRight, marginTop, marginBottom;
		pChild->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		flChildDesiredWidth += marginLeft.GetValueAsLength( flMaxWidth ) + marginRight.GetValueAsLength( flMaxWidth );
		flChildDesiredHeight += marginTop.GetValueAsLength( flMaxHeight ) + marginBottom.GetValueAsLength( flMaxHeight );

		if ( eFlowDirection == k_EFlowRightWrap || eFlowDirection == k_EFlowLeftWrap )
		{
			// if child would exceed max width, need to wrap
			if ( flChildWrapRowSize + flChildDesiredWidth > flMaxWidth )
			{
				flChildWrapOffset = flChildHeight;
				flChildWrapRowSize = 0.0f;
			}

			flChildWrapRowSize += flChildDesiredWidth;
			flChildWidth = Max( flChildWidth, flChildWrapRowSize );
		}
		else if ( eFlowDirection == k_EFlowRight || eFlowDirection == k_EFlowLeft )
		{
			flChildWidth += flChildDesiredWidth;
		}
		else
		{
			// Handles Up/Down and None (may be changed by DownWrap/UpWrap)
			flChildWidth = MAX( flChildWidth, flChildDesiredWidth + xChild.GetValueAsLength( flMaxWidth ) + flChildWrapOffset );
		}

		if ( eFlowDirection == k_EFlowDownWrap || eFlowDirection == k_EFlowUpWrap )
		{
			// if child would exceed max height, need to wrap
			if ( flChildWrapRowSize + flChildDesiredHeight > flMaxHeight )
			{
				flChildWrapOffset = flChildWidth;
				flChildWrapRowSize = 0.0f;
				flChildWidth = Max( flChildWidth, flChildDesiredWidth + xChild.GetValueAsLength( flMaxWidth ) + flChildWrapOffset );
			}

			flChildWrapRowSize += flChildDesiredHeight;
			flChildHeight = Max( flChildHeight, flChildWrapRowSize );
		}
		else if ( eFlowDirection == k_EFlowDown || eFlowDirection == k_EFlowUp )
		{
			flChildHeight += flChildDesiredHeight;
		}
		else
		{
			// Handles right/left variants and None
			flChildHeight = MAX( flChildHeight, flChildDesiredHeight + yChild.GetValueAsLength( flMaxHeight ) + flChildWrapOffset );
		}

		AssertMsg( IsFinite( flChildWidth ), "Invalid content width calculated" );
		AssertMsg( IsFinite( flChildHeight ), "Invalid content height calculated" );
	}

	// Add padding back to content size
	AccessStyle()->GetContentInset( flChildWidth, flChildHeight, bFinalDimensions, flLeft, flTop, flRight, flBottom );

	*pflContentWidth = flChildWidth + flLeft + flRight;
	*pflContentHeight = flChildHeight + flTop + flBottom;

	AssertMsg( IsFinite( *pflContentWidth ), "Invalid content width calculated" );
	AssertMsg( IsFinite( *pflContentHeight ), "Invalid content height calculated" );
}


//-----------------------------------------------------------------------------
// Purpose: Arrange children. Second pass of layout
//-----------------------------------------------------------------------------
void CUIPanel::LayoutTraverse( float xFromParent, float yFromParent, float flFinalWidth, float flFinalHeight )
{
	VPROF_BUDGET_DETAILED( "CUIPanel::LayoutTraverse", VPROF_BUDGETGROUP_TENFOOT );


	// if nothing is dirty, can early out
	if( IsPositionValid() && IsChildPositionValid() && !IsPositionTransitioning() && xFromParent == m_flLastLayoutXFromParent &&
		yFromParent == m_flLastLayoutYFromParent && m_flLastLayoutWidthFromParent == flFinalWidth && m_flLastLayoutHeightFromParent == flFinalHeight )
	{
		return;
	}

	// sanity check measurements
	Assert( flFinalWidth != k_flFloatAuto );
	Assert( flFinalHeight != k_flFloatAuto );

	// parent could pass us a larger or smaller area than we desired.. make some adjustments. If a larger space is provided,
	// we will use that space when applying alignment properties
	EOverflowValue eOverflowHoriztonal, eOverflowVertical;
	AccessStyle()->GetOverflow( eOverflowHoriztonal, eOverflowVertical );

	float flOurWidth = m_flDesiredLayoutWidth;
	float flOurHeight = m_flDesiredLayoutHeight;

	// need width and height from style
	CUILength width;
	CUILength height;
	AccessStyle()->GetInterpolatedWidth( width, false );
	AccessStyle()->GetInterpolatedHeight( height, false );

	// need to adjust for percentage based width & height
	float flParentWidth = m_pParent ? m_pParent->GetActualLayoutWidth() : flFinalWidth;
	float flParentHeight = m_pParent ? m_pParent->GetActualLayoutHeight() : flFinalHeight;
	{
		if ( width.IsPercent() )
			flOurWidth = width.GetValueAsLength( flParentWidth );
		if ( height.IsPercent() )
			flOurHeight = height.GetValueAsLength( flParentHeight );

		CUILength maxWidth;
		CUILength maxHeight;
		AccessStyle()->GetInterpolatedMaxWidth( maxWidth, false );
		AccessStyle()->GetInterpolatedMaxHeight( maxHeight, false );

		if ( maxWidth.IsSet() && (maxWidth.IsPercent() || maxWidth.IsLength()) )
			flOurWidth = Min( flOurWidth, maxWidth.GetValueAsLength( flParentWidth ) );
		if ( maxHeight.IsSet() && (maxHeight.IsPercent() || maxHeight.IsLength()) )
			flOurHeight = Min( flOurHeight, maxHeight.GetValueAsLength( flParentHeight ) );

		// bugbug jmccaskey - Do not remove margin, because our contract is 50% for a child means
		// 50% as the childs with of the parents, not accounting for margin.  This kind of sucks though
		// since two children with 50% and equal margins flowing right will not be the same size due to
		// squishing and the margin making them not fit... but this matches all other layout behavior.
	}

	// Even if above adjusted our percent based widths we still are not allowed to grow beyond what parent
	// had believed it reserved for us
	if( eOverflowHoriztonal != k_EOverflowNoClip )
		flOurWidth = Min( flFinalWidth, flOurWidth );

	if( eOverflowVertical != k_EOverflowNoClip )
		flOurHeight = Min( flFinalHeight, flOurHeight );

	// need to adjust for height/width percents
	if ( width.IsHeightPercentage() )
		flOurWidth = RoundFloatToInt( flOurHeight * width.GetValue() / 100.0f );
	if ( height.IsWidthPercentage() )
		flOurHeight = RoundFloatToInt( flOurWidth * height.GetValue() / 100.0f );

	// include margin, needed during OnLayoutTraverse for spacing 
	CUILength marginLeft, marginTop, marginRight, marginBottom;
	AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );

	marginLeft.ConvertToLength( flParentWidth );
	marginTop.ConvertToLength( flParentHeight );
	marginRight.ConvertToLength( flParentWidth );
	marginBottom.ConvertToLength( flParentHeight );

	float x = xFromParent + marginLeft.GetValue();
	float y = yFromParent + marginTop.GetValue();

	// Adjust position for alignment
	AdjustPositionForAlignment( &x, &y, flOurWidth, flOurHeight, flFinalWidth, flFinalHeight );

#ifndef PANORAMA_USE_S1WRAPPER	
#if defined( SOURCE2_PANORAMA )

	static const CPanoramaSymbol k_symAllowOversized( "AllowOversized" );
	if ( BIsVisible()
		&& GetAttribute( k_symAllowOversized, static_cast<int>( 0 ) ) == 0
		&& ( flOurWidth > g_pRenderHardwareConfig->MaxTextureWidth() || flOurHeight > g_pRenderHardwareConfig->MaxTextureHeight() ) )
	{
		const char *pId = BHasID() ? GetID() : "<unnamed>";
		CPanoramaSymbol panelType = GetPanelType();

		const char *pParentId = "<unnamed>";
		CPanoramaSymbol parentPanelType;
		panorama::IUIPanel *pParentPanel = GetParent();
		if ( pParentPanel )
		{
			pParentId = pParentPanel->BHasID() ? pParentPanel->GetID() : "<unnamed>";
			parentPanelType = pParentPanel->GetPanelType();
		}
		AssertMsg( false, "Panel \"%s\" of type \"%s\", is requesting a desired layout of <%d, %d> which is larger than the maximum texture resolution of <%d, %d>\n" \
			"Parent panel is \"%s\" of type \"%s\"",
			pId, panelType.String(),
			(int)flOurWidth, (int)flOurHeight, g_pRenderHardwareConfig->MaxTextureWidth(), g_pRenderHardwareConfig->MaxTextureHeight(),
			pParentId, parentPanelType.String() );

		// Clamp to avoid a crash later on
		flOurWidth = Min( flOurWidth, (float)g_pRenderHardwareConfig->MaxTextureWidth() );
		flOurHeight = Min( flOurHeight, (float)g_pRenderHardwareConfig->MaxTextureHeight() );
	}
#endif
#endif

	// update our offsets (this includes parent padding & border and our margin)
	m_flActualXOffset = x;
	m_flActualYOffset = y;

	m_flActualLayoutWidth = flOurWidth;
	m_flActualLayoutHeight = flOurHeight;

	ClientPtr()->OnLayoutTraverse( flOurWidth, flOurHeight );
	LayoutChildrenInHiding( flOurWidth, flOurHeight );

	// positions have been updated
	ClearLayoutPositionFlags();
	m_flLastLayoutXFromParent = xFromParent;
	m_flLastLayoutYFromParent = yFromParent;
	m_flLastLayoutWidthFromParent = flFinalWidth;
	m_flLastLayoutHeightFromParent = flFinalHeight;

	// If we performed re-layout, then we must repaint fully
	SetRepaint( k_EPanelRepaintFull );

	if ( m_bSendChildScrolledIntoViewEvents && ( eOverflowHoriztonal == k_EOverflowScroll || eOverflowVertical == k_EOverflowScroll ) )
	{
		DispatchEventAsync( CheckChildrenScrolledIntoView(), this );
	}

	if ( m_pOnLayoutEvent )
	{
		IUIEvent *pEvent = m_pOnLayoutEvent;
		m_pOnLayoutEvent = nullptr;
		UIEngine()->DispatchEventAsync( 0.0f, pEvent );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adjusts the x & y position by this panel's alignment if necessary
//-----------------------------------------------------------------------------
void CUIPanel::AdjustPositionForAlignment( float *x, float *y, float flOurWidth, float flOurHeight, float flTotalWidth, float flTotalHeight )
{
	//VPROF_BUDGET( "CUIPanel::AdjustPositionForAlignment", VPROF_BUDGETGROUP_TENFOOT );
	// get our alignment
	EHorizontalAlignment eHorizontalAlignment;
	EVerticalAlignment eVerticalAlignment;
	AccessStyle()->GetAlignment( eHorizontalAlignment, eVerticalAlignment );


#if defined( SOURCE2_PANORAMA )
	// bugbug - came from dota card game, looks just wrong... flTotalWidth/Height should be correct, and if not the caller should fix rather
	// than this change here?  This code regresses Big Picture layout.
	// source2 bugbug - Not having this change regresses source2 so ifdef until somebody
	// figures out what the One True Way is.
	//
	// REI - this code handles the case where a parent panel has overflow:noclip and its child overflows and has alignment specified.
	// In that case, flTotalWidth/Height are set to the requested size (since the child is allowed to overflow), but we aren't properly
	// aligning to the parent.  This should only happen in the overflow:noclip case as flTotalWidth/Height should otherwise always
	// be smaller than the parent's layout size.
	// REI - See https://confluence.valvesoftware.com/display/~ryani/Panorama+notes 
	if ( m_pParent )
	{
		if ( eHorizontalAlignment == k_EHorizontalAlignmentCenter || eHorizontalAlignment == k_EHorizontalAlignmentRight )
			flTotalWidth = Min( m_pParent->GetActualLayoutWidth(), flTotalWidth );
		if ( eVerticalAlignment == k_EVerticalAlignmentCenter || eVerticalAlignment == k_EVerticalAlignmentBottom )
			flTotalHeight = Min( m_pParent->GetActualLayoutHeight(), flTotalHeight );
	}
#endif

	// adjust x if necessary
	float flDeltaWidth = flTotalWidth - flOurWidth;
	if( fabsf( flDeltaWidth ) > 0.001f )
	{
		if ( eHorizontalAlignment == k_EHorizontalAlignmentCenter )
		{
			*x = *x + ( flDeltaWidth / 2.0f );
			*x = RoundFloatToInt( *x );
		}
		else if ( eHorizontalAlignment == k_EHorizontalAlignmentRight )
		{
			*x = *x + flDeltaWidth;
			*x = RoundFloatToInt( *x );
		}
		else if ( eHorizontalAlignment == k_EHorizontalAlignmentCenterNoPixelSnap )
		{
			*x = *x + ( flDeltaWidth / 2.0f );
		}
		else
		{
			Assert( eHorizontalAlignment == k_EHorizontalAlignmentLeft );
			*x = RoundFloatToInt( *x );
		}
	}

	// adjust y if necessary
	float flDeltaHeight = flTotalHeight - flOurHeight;
	if( fabsf( flDeltaHeight ) > 0.001f )
	{
		if ( eVerticalAlignment == k_EVerticalAlignmentCenter )
		{
			*y = *y + ( flDeltaHeight / 2.0f );
			*y = RoundFloatToInt( *y + 0.001f ); // Ensure when vertically centering panels with odd number of pixels on top and bottom, we prefer to center 1px lower
			// this solves most of the text that starts with capital letter and doesn't have subscript letters to appear "more in the center", and other UI elements
			// will have uneven top/bottom anyways.
		}
		else if ( eVerticalAlignment == k_EVerticalAlignmentBottom )
		{
			*y = *y + flDeltaHeight;
			*y = RoundFloatToInt( *y );
		}
		else if ( eVerticalAlignment == k_EVerticalAlignmentCenterNoPixelSnap )
		{
			*y = *y + ( flDeltaHeight / 2.0f );
		}
		else
		{
			Assert( eVerticalAlignment == k_EVerticalAlignmentTop );
			*y = RoundFloatToInt( *y );
		}
	}

	// Try to avoid sub pixel jitter when parents/positions are resizing slightly, this is caused
	// both by parent flipping from odd to even values, and subpixel sizing of parent, so we need
	// to try to snap to our previous position rather than just rounding up/down.

	if( eHorizontalAlignment == k_EHorizontalAlignmentCenter || eVerticalAlignment == k_EVerticalAlignmentCenter )
	{
		CUIPanel *pParent = (CUIPanel*)m_pParent;
		float xOffset = 0.0f;
		float yOffset = 0.0f;
		bool bAnimatingWidth = false;
		bool bAnimatingHeight = false;
		while( pParent )
		{
			xOffset += pParent->GetActualXOffset();
			yOffset += pParent->GetActualYOffset();
			if( !bAnimatingWidth )
				bAnimatingWidth = pParent->AccessStyle()->BHasTransitionOrAnimation( CStylePropertyWidth::symbol );
			if( !bAnimatingHeight )
				bAnimatingHeight = pParent->AccessStyle()->BHasTransitionOrAnimation( CStylePropertyWidth::symbol );
			pParent = (CUIPanel*)pParent->m_pParent;
		}

		if( !bAnimatingWidth )
			bAnimatingWidth = AccessStyle()->BHasTransitionOrAnimation( CStylePropertyWidth::symbol );
		if( !bAnimatingHeight )
			bAnimatingHeight = AccessStyle()->BHasTransitionOrAnimation( CStylePropertyWidth::symbol );

		if( eHorizontalAlignment == k_EHorizontalAlignmentCenter && bAnimatingWidth && m_flLastAbsoluteXOffset != FLT_MAX )
		{
			if( fabsf( *x + xOffset - m_flLastAbsoluteXOffset ) < 1.0f )
				*x = m_flLastAbsoluteXOffset - xOffset;
		}

		if( eVerticalAlignment == k_EVerticalAlignmentCenter && bAnimatingHeight && m_flLastAbsoluteYOffset != FLT_MAX )
		{
			if( fabsf( *y + yOffset - m_flLastAbsoluteYOffset ) < 1.0f )
				*y = m_flLastAbsoluteYOffset - yOffset;
		}

		m_flLastAbsoluteXOffset = *x + xOffset;
		m_flLastAbsoluteYOffset = *y + yOffset;
	}
	else
	{
		m_flLastAbsoluteXOffset = FLT_MAX;
		m_flLastAbsoluteYOffset = FLT_MAX;
	}
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//			This default implementation provides support for flow-direction. Feel free to override
//			if you want different layout behavior for your panel.
//-----------------------------------------------------------------------------
void CUIPanel::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	// need flow direction
	EFlowDirection eFlowDirection;
	AccessStyle()->GetFlowChildren( eFlowDirection );

	// use flow direction (and possible wrapping)
	bool bHorizontalFlow = ( eFlowDirection == k_EFlowRight || eFlowDirection == k_EFlowLeft );
	bool bHorizontalFlowWrap = ( eFlowDirection == k_EFlowRightWrap || eFlowDirection == k_EFlowLeftWrap );
	bool bVerticalFlow = ( eFlowDirection == k_EFlowDown || eFlowDirection == k_EFlowUp );
	bool bVerticalFlowWrap = ( eFlowDirection == k_EFlowDownWrap || eFlowDirection == k_EFlowUpWrap );

	// include padding
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flFinalWidth, flFinalHeight, false, flLeft, flTop, flRight, flBottom );

	// Our total space minus padding, used to compute % based margins on children below
	const float flContainerWidth = flFinalWidth - flLeft - flRight;
	const float flContainerHeight = flFinalHeight - flTop - flBottom;

	// position each child	
	float x = flLeft;
	float y = flTop;
	int iLastChild = m_vecChildren.Count() - 1;

	float flSpaceAvailableForFillPanels = 0.0f;
	float flTotalFillPanelWeight = 0.0f;
	uint32 cPanelsFillingParent = 0;

	int iLastVisibleFlowedChild = -1;

	CUILength width, height;
	AccessStyle()->GetWidth( width );
	AccessStyle()->GetHeight( height );

	// If flowing (non-wrapping), we may need to squish panels with fill-parent-flow sizing type
	if ( bHorizontalFlow || bVerticalFlow )
	{
		float flSpaceNeeded = 0.0f;
		float flSpaceAvailable = 0.0f;

		if ( bHorizontalFlow )
			flSpaceAvailable = flFinalWidth - x - flRight;
		else
			flSpaceAvailable = flFinalHeight - y - flBottom;

		// First, do a pass to check how much size our children desire, if it's larger than we have available, we'll need to squish.
		FOR_EACH_VEC( m_vecChildren, i )
		{
			CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
			if ( !pChild->BIsVisible() )
				continue;

			if ( !width.IsFitChildren() && !height.IsFitChildren() && pChild->BIsTransparent() )
				continue;

			iLastVisibleFlowedChild = i;

			CUILength marginLeft, marginTop, marginRight, marginBottom;
			pChild->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );

			if ( bHorizontalFlow )
			{
				CUILength widthChild;
				pChild->AccessStyle()->GetInterpolatedWidth( widthChild, false );
				if ( widthChild.IsFillParentFlow() )
				{
					++cPanelsFillingParent;
					flTotalFillPanelWeight += widthChild.GetValue();

					// Still need to take out margins, as those are fixed size relative to parent
					flSpaceNeeded += marginLeft.GetValueAsLength( flContainerWidth ) + marginRight.GetValueAsLength( flContainerWidth );
				}
				else
					flSpaceNeeded += pChild->m_flDesiredLayoutWidth + marginLeft.GetValueAsLength( flContainerWidth ) + marginRight.GetValueAsLength( flContainerWidth );
			}
			else
			{
				CUILength heightChild;
				pChild->AccessStyle()->GetInterpolatedHeight( heightChild, false );

				if ( heightChild.IsFillParentFlow() )
				{
					++cPanelsFillingParent;
					flTotalFillPanelWeight += heightChild.GetValue();

					// Still need to take out margins, as those are fixed size relative to parent
					flSpaceNeeded += marginTop.GetValueAsLength( flContainerHeight ) + marginBottom.GetValueAsLength( flContainerHeight );
				}
				else
					flSpaceNeeded += pChild->m_flDesiredLayoutHeight + marginTop.GetValueAsLength( flContainerHeight ) + marginBottom.GetValueAsLength( flContainerHeight );
			}
		}

		flSpaceAvailableForFillPanels = MAX( 0.0f, flSpaceAvailable - flSpaceNeeded );
	}

	EOverflowValue eOverflowHorizontal, eOverflowVertical;
	AccessStyle()->GetOverflow( eOverflowHorizontal, eOverflowVertical );

	float flWrapRowMax = 0.0f;									// when wrapping, max height of element in row for k_EFLowRightWrap or max width of element in column for k_EFlowDownWrap
	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if( !pChild->BIsVisible() )
			continue;

		if( eFlowDirection == k_EFlowNone && !width.IsFitChildren() && !height.IsFitChildren() && pChild->BIsTransparent() )
			continue;

		CUILength marginLeft, marginTop, marginRight, marginBottom;
		pChild->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flContainerWidth );
		marginRight.ConvertToLength( flContainerWidth );
		marginTop.ConvertToLength( flContainerHeight );
		marginBottom.ConvertToLength( flContainerHeight );

		// Adjust for wrapping before doing basic directional flow. We only
		// need to perform the wrapping logic specific to which axis we're
		// wrapping on because we fix up the direction later by flipping the
		// contents in that direction.
		if ( bHorizontalFlowWrap )
		{
			// only wrap if there is room for another row
			float flDesiredWidth = pChild->m_flDesiredLayoutWidth + marginLeft.GetValue() + marginRight.GetValue();
			if ( x - flLeft + flDesiredWidth > flContainerWidth && ( y + flWrapRowMax < flContainerHeight || eOverflowVertical != k_EOverflowSquish ) )
			{
				x = flLeft;
				y += flWrapRowMax;
				flWrapRowMax = 0.0f;
			}
		}
		else if ( bVerticalFlowWrap )
		{
			// only wrap if there is room for another column
			float flDesiredHeight = pChild->m_flDesiredLayoutHeight + marginTop.GetValue() + marginBottom.GetValue();
			if ( y - flTop + flDesiredHeight > flContainerHeight && ( x + flWrapRowMax < flContainerWidth || eOverflowHorizontal != k_EOverflowSquish ) )
			{
				y = flTop;
				x += flWrapRowMax;
				flWrapRowMax = 0.0f;
			}
		}

		// by default, give the child all remaining space after padding. Flow directions can override per child.
		float flChildWidth = flFinalWidth - x - flRight;
		float flChildHeight = flFinalHeight - y - flBottom;

		// If we have a flow direction, we need to override their positions
		// with our calculated values. Otherwise, we allow the children to
		// position themselves.
		if ( bHorizontalFlow || bHorizontalFlowWrap )
		{
			// the last child can have all remaining horizontal space, otherwise children should keep their desired width
			if ( i != iLastChild || eOverflowHorizontal != k_EOverflowSquish )
			{
				float flDesiredWidth = pChild->m_flDesiredLayoutWidth + marginLeft.GetValue() + marginRight.GetValue();
				if ( eOverflowHorizontal == k_EOverflowSquish )
					flChildWidth = MIN( flDesiredWidth, flChildWidth );
				else
					flChildWidth = flDesiredWidth;
			}

			if ( cPanelsFillingParent > 0 && flTotalFillPanelWeight > 0.0f )
			{
				CUILength widthChild;
				pChild->AccessStyle()->GetInterpolatedWidth( widthChild, false );
				if ( widthChild.IsFillParentFlow() )
				{
					flChildWidth = flSpaceAvailableForFillPanels * ( widthChild.GetValue() / flTotalFillPanelWeight ) + marginLeft.GetValue() + marginRight.GetValue();
				}
			}

			// Ensure we are on pixel boundaries for edges of flowing panels, helps prevent fuzzy text and gaps between
			// anti-aliased edges, last child gets extra round off error.
			if ( i == iLastVisibleFlowedChild )
				flChildWidth = ceil( flChildWidth );
			else
				flChildWidth = RoundFloatToInt( flChildWidth );
		}
		else if ( bVerticalFlow || bVerticalFlowWrap )
		{
			// the last child can have all remaining vertical space, otherwise children should keep their desired height
			if ( i != iLastChild || eOverflowVertical != k_EOverflowSquish )
			{
				float flDesiredHeight = pChild->m_flDesiredLayoutHeight + marginTop.GetValue() + marginBottom.GetValue();
				if( eOverflowVertical == k_EOverflowSquish )
					flChildHeight = MIN( flDesiredHeight, flChildHeight );
				else
					flChildHeight = flDesiredHeight;
			}

			if ( cPanelsFillingParent > 0 && flTotalFillPanelWeight > 0.0f )
			{
				CUILength heightChild;
				pChild->AccessStyle()->GetInterpolatedHeight( heightChild, false );
				if( heightChild.IsFillParentFlow() )
				{
					flChildHeight = flSpaceAvailableForFillPanels * ( heightChild.GetValue() / flTotalFillPanelWeight ) + marginTop.GetValue() + marginBottom.GetValue();
				}
			}

			// Ensure we are on pixel boundaries for edges of flowing panels, helps prevent fuzzy text and gaps between
			// anti-aliased edges, last child gets extra round off error.
			if ( i == iLastVisibleFlowedChild )
				flChildHeight = ceil( flChildHeight );
			else
				flChildHeight = RoundFloatToInt( flChildHeight );
		}

		// If we're asked not to squish, then let the child fill out its desired size if it's bigger than we'd otherwise give it.
		if ( eOverflowHorizontal != k_EOverflowSquish )
		{
			CUILength styleWidth;
			pChild->AccessStyle()->GetInterpolatedWidth( styleWidth, false );

			float flDesiredChildWidth = 0.0f;

			if ( styleWidth.IsPercent() )
			{
				flDesiredChildWidth = Min( pChild->m_flDesiredLayoutWidth + marginLeft.GetValue() + marginRight.GetValue(), styleWidth.GetValueAsLength( flFinalWidth ) + marginLeft.GetValue() + marginRight.GetValue() );
			}
			else if ( styleWidth.IsLength() || styleWidth.IsFitChildren() )
			{
				flDesiredChildWidth = pChild->m_flDesiredLayoutWidth + marginLeft.GetValue() + marginRight.GetValue();
			}

			flChildWidth = Max( flChildWidth, flDesiredChildWidth );
		}
		if ( eOverflowVertical != k_EOverflowSquish )
		{		
			CUILength styleHeight;
			pChild->AccessStyle()->GetInterpolatedHeight( styleHeight, false );

			float flDesiredChildHeight = 0.0f;

			if ( styleHeight.IsPercent() )
			{
				flDesiredChildHeight = Min( pChild->m_flDesiredLayoutHeight + marginLeft.GetValue() + marginRight.GetValue(), styleHeight.GetValueAsLength( flFinalHeight ) + marginTop.GetValue() + marginBottom.GetValue() );
			}
			else if ( styleHeight.IsLength() || styleHeight.IsFitChildren() )
			{
				flDesiredChildHeight = pChild->m_flDesiredLayoutHeight + marginTop.GetValue() + marginBottom.GetValue();
			}

			flChildHeight = Max( flChildHeight, flDesiredChildHeight );
		}

		// don't pass in negative width or height values to LayoutTraverse
		float flChildAvailableWidth = Max( flChildWidth - marginLeft.GetValue() - marginRight.GetValue(), 0.0f );
		float flChildAvailableHeight = Max( flChildHeight - marginTop.GetValue() - marginBottom.GetValue(), 0.0f );
		pChild->LayoutTraverse( flLeft, flTop, flChildAvailableWidth, flChildAvailableHeight );

		// when in a flowing layout, use position to set flowing offset for x & y so it can transition (parent's padding, border are passed to LayoutTraverse)
		if ( eFlowDirection != k_EFlowNone )
		{
			// We compute the effective X and Y positions here, which is just
			// used to keep a positive-axis value throughout the rest of the
			// code but still support bottom-up or right-to-left flows.
			float flEffectiveX = x;
			float flEffectiveY = y;

			// This is the only "opposite-direction" fixup we need. We pretend
			// that everything is going in the positive direction, and then if
			// we get here and that's not true we flip the appropriate axis
			// value within the container.
			//
			// NOTE: We may use Content Width/Height here because the Container
			// Width/Height don't represent the scrollable region, just the
			// visible region, so for this to work in a scrolling environment
			// we have to offset based off of bottom of the full panel, not
			// just the visible part. But, if we're not scrolling (bigger
			// contents than our container is our clue) then we want to anchor
			// the contents on the bottom (Flow Up) or right (Flow Left).
			if ( eFlowDirection == k_EFlowLeft || eFlowDirection == k_EFlowLeftWrap )
			{
				float flEffectiveContainerWidth = 2 * flLeft + flContainerWidth;

				if ( eOverflowHorizontal != k_EOverflowSquish )
					flEffectiveContainerWidth = Max( m_flContentWidth, 2 * flLeft + flContainerWidth );

				flEffectiveX = ( flEffectiveContainerWidth - x ) - ( pChild->m_flActualLayoutWidth + marginLeft.GetValue() + marginRight.GetValue() );
			}
			if ( eFlowDirection == k_EFlowUp || eFlowDirection == k_EFlowUpWrap )
			{
				float flEffectiveContainerHeight = 2 * flTop + flContainerHeight;

				if ( eOverflowVertical != k_EOverflowSquish )
					flEffectiveContainerHeight = Max( m_flContentHeight, 2 * flTop + flContainerHeight );

				flEffectiveY = ( flEffectiveContainerHeight - y ) - ( pChild->m_flActualLayoutHeight + marginTop.GetValue() + marginBottom.GetValue() );
			}

			// Apply the scale manually rather than letting SetPositionFromLayoutTraverse do it by using ScaleLengthValue. We've already pixel-snapped
			// in the logic above, so if you double pixel snap in ScaleLengthValue you get gaps between your controls at different aspect ratios.
			CUILength flowingX( ( flEffectiveX - flLeft ) / GetActualUIScaleX(), CUILength::k_EUILengthLength );
			CUILength flowingY( ( flEffectiveY - flTop ) / GetActualUIScaleY(), CUILength::k_EUILengthLength );
			CUILength flowingZ( 0, CUILength::k_EUILengthLength );
			pChild->SetPositionFromLayoutTraverse( flowingX, flowingY, flowingZ );
		}
		else
		{
			Assert( flLeft == x );
			Assert( flTop == y );
		}

		// If the child actually used less size than it had desired then update our value now so we don't offset the next child too much when flowing
		if ( pChild->m_flActualLayoutWidth < pChild->m_flDesiredLayoutWidth )
			flChildWidth = pChild->m_flActualLayoutWidth + marginLeft.GetValue() + marginRight.GetValue();

		if ( pChild->m_flActualLayoutHeight < pChild->m_flDesiredLayoutHeight )
			flChildHeight = pChild->m_flActualLayoutHeight + marginTop.GetValue() + marginBottom.GetValue();

		// if in a flowing layout, advance child position
		if ( bHorizontalFlow )
		{
			x += flChildWidth;
		}
		else if ( bVerticalFlow )
		{
			y += flChildHeight;
		}
		else if ( bHorizontalFlowWrap )
		{
			x += flChildWidth;
			flWrapRowMax = Max( flWrapRowMax, pChild->GetActualLayoutHeight() + marginTop.GetValue() + marginBottom.GetValue() );
		}
		else if ( bVerticalFlowWrap )
		{
			y += flChildHeight;
			flWrapRowMax = Max( flWrapRowMax, pChild->GetActualLayoutWidth() + marginLeft.GetValue() + marginRight.GetValue() );
		}
	}

	// if our actual width/height is less than our desired in the flow direction, update content width/height if necessary if not squishing
	if ( bHorizontalFlowWrap && eOverflowVertical != k_EOverflowSquish )
	{
		float flLayoutHeight = y + flWrapRowMax;
		if ( flLayoutHeight > m_flContentHeight )
			m_flContentHeight = flLayoutHeight;
	}
	
	if ( bVerticalFlowWrap && eOverflowHorizontal != k_EOverflowSquish )
	{
		float flLayoutWidth = x + flWrapRowMax;
		if ( flLayoutWidth > m_flContentWidth )
			m_flContentWidth = flLayoutWidth;
	}

	// update scrollbars
	UpdateScrollOffsetX();
	UpdateScrollOffsetY();
	CreateOrUpdateScrollBarForLayout( &m_pHorizontalScrollBar, m_pHorizontalScrollData, m_flContentWidth, m_flActualLayoutWidth, eOverflowHorizontal, &panorama::IUIPanelClient::CreateNewHorizontalScrollBar, k_symPropertyOnScrolledToRightEdge );
	CreateOrUpdateScrollBarForLayout( &m_pVerticalScrollBar, m_pVerticalScrollData, m_flContentHeight, m_flActualLayoutHeight, eOverflowVertical, &panorama::IUIPanelClient::CreateNewVerticalScrollBar, k_symPropertyOnScrolledToBottom );

	// Now layout the scroll bars, since we've set both their values, we use finalHeight/finalWidth
	// rather than containerWidth/containerHeight here since scrollbars get to ignore parents padding values!
	if( m_pVerticalScrollBar )
	{
		CUILength marginLeft, marginTop, marginRight, marginBottom;
		m_pVerticalScrollBar->UIPanel()->AccessIUIStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flFinalWidth );
		marginRight.ConvertToLength( flFinalWidth );
		marginTop.ConvertToLength( flFinalHeight );
		marginBottom.ConvertToLength( flFinalHeight );

		m_pVerticalScrollBar->UIPanel()->DesiredLayoutSizeTraverse( flFinalWidth, flFinalHeight );
		m_pVerticalScrollBar->UIPanel()->LayoutTraverse( 0.0f, 0.0f, flFinalWidth - marginLeft.GetValue() - marginRight.GetValue(), flFinalHeight - marginTop.GetValue() - marginBottom.GetValue() );

		CUtlVector<CTransform3D *> vecTransforms;
		float xOffset = m_pHorizontalScrollData ? ( m_pHorizontalScrollData->m_flOffsetTarget != FLT_MAX ? m_pHorizontalScrollData->m_flOffsetTarget : m_pHorizontalScrollData->m_flOffset ) : 0.0f;
		float yOffset = m_pVerticalScrollData->m_flOffsetTarget != FLT_MAX ? m_pVerticalScrollData->m_flOffsetTarget : m_pVerticalScrollData->m_flOffset;
		vecTransforms.AddToTail( new CTransformTranslate3D( -1.0f*xOffset*(1.0f / GetActualUIScaleX()), -1.0f*yOffset*(1.0f / GetActualUIScaleY()), 0.0f ) );

		if ( m_pVerticalScrollBar->BLastMoveImmediate() )
			m_pVerticalScrollBar->UIPanel()->AccessIUIStyleDirty()->SetTransform3DWithoutTransition( vecTransforms );
		else
			m_pVerticalScrollBar->UIPanel()->AccessIUIStyleDirty()->SetTransform3D( vecTransforms );			

	}

	if( m_pHorizontalScrollBar )
	{
		CUILength marginLeft, marginTop, marginRight, marginBottom;
		m_pHorizontalScrollBar->UIPanel()->AccessIUIStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flFinalWidth );
		marginRight.ConvertToLength( flFinalWidth );
		marginTop.ConvertToLength( flFinalHeight );
		marginBottom.ConvertToLength( flFinalHeight );

		m_pHorizontalScrollBar->UIPanel()->DesiredLayoutSizeTraverse( flFinalWidth, flFinalHeight );
		m_pHorizontalScrollBar->UIPanel()->LayoutTraverse( 0.0f, 0.0f, flFinalWidth - marginLeft.GetValue() - marginRight.GetValue(), flFinalHeight - marginTop.GetValue() - marginBottom.GetValue() );

		CUtlVector<CTransform3D *> vecTransforms;
		float xOffset = m_pHorizontalScrollData->m_flOffsetTarget != FLT_MAX ? m_pHorizontalScrollData->m_flOffsetTarget : m_pHorizontalScrollData->m_flOffset;
		float yOffset = m_pVerticalScrollData ? ( m_pVerticalScrollData->m_flOffsetTarget != FLT_MAX ? m_pVerticalScrollData->m_flOffsetTarget : m_pVerticalScrollData->m_flOffset ) : 0.0f;
		vecTransforms.AddToTail( new CTransformTranslate3D( -1.0f*xOffset*(1.0f / GetActualUIScaleX()), -1.0f*yOffset*(1.0f / GetActualUIScaleY()), 0.0f ) );

		if ( m_pHorizontalScrollBar->BLastMoveImmediate() )
			m_pHorizontalScrollBar->UIPanel()->AccessIUIStyleDirty()->SetTransform3DWithoutTransition( vecTransforms );
		else
			m_pHorizontalScrollBar->UIPanel()->AccessIUIStyleDirty()->SetTransform3D( vecTransforms );
	}
}

extern ConVar s_convarTransitionTimeFactor;

//-----------------------------------------------------------------------------
// Purpose: Helper to update scrollbars for layout pass. Need to call UpdateScrollOffset*() in scroll direction before calling
//-----------------------------------------------------------------------------
void CUIPanel::CreateOrUpdateScrollBarForLayout( IUIScrollBar **ppScrollBar, ScrollBarData_t *&pScrollData, float flContentLength, float flActualLayoutLength, EOverflowValue eOverflow, ScrollBarCreate_t pfnCreate, CPanoramaSymbol symScrollMax )
{
	IUIScrollBar *pScrollBar = *ppScrollBar;

	// can early out if we don't need a scrollbar
	if ( flContentLength <= flActualLayoutLength || eOverflow != k_EOverflowScroll )
	{
		if ( pScrollData )
		{
			pScrollData->m_flOffset = 0.0f;
			pScrollData->m_flOffsetTarget = FLT_MAX;

			// no scrollbar means at end of panel
			if ( !pScrollData->m_bDispatchedScrollMax )
			{
				pScrollData->m_bDispatchedScrollMax = true;
				DispatchPanelEvent( symScrollMax );
				//Msg( "[%s] Dispatched: %s\n", GetID(), symScrollMax.String() );
			}
		}

		if ( !pScrollBar )
			return;		
		
		IUIPanel *pScrollBarParent = pScrollBar->UIPanel()->GetParent();
		pScrollBar->ClientPtr()->OnDeletePanel();
		*ppScrollBar = NULL;

		if ( pScrollBarParent )
			pScrollBarParent->OnScrollPositionChanged();

		return;
	}

	// need a scrollbar
	bool bNew = false;
	EnsureScrollData( pScrollData );
	if ( !pScrollBar )
	{
		bNew = true;
		pScrollBar = ((*ClientPtr()).*pfnCreate)( pScrollData->m_flInitialPos );
		*ppScrollBar = pScrollBar;
	}

	if ( bNew || pScrollBar->GetRangeMax() != flContentLength || pScrollBar->GetScrollWindowSize() != flActualLayoutLength )
	{
		bool bScrollToBottom = m_bKeepScrollToBottomOnResize && (pScrollBar->GetScrollWindowPosition() + pScrollBar->GetScrollWindowSize() >= pScrollBar->GetRangeMax());

		pScrollBar->SetRangeMinMax( 0.0f, flContentLength );
		pScrollBar->SetScrollWindowSize( flActualLayoutLength );
		pScrollBar->Normalize();

		if ( bScrollToBottom )
		{
			pScrollBar->SetScrollWindowPosition( pScrollBar->GetRangeMax() - pScrollBar->GetScrollWindowSize() );
		}
	}

	float flScrollTarget = -1.0 * pScrollBar->GetScrollWindowPosition();
	if ( bNew || (pScrollData->m_flOffset != flScrollTarget && pScrollData->m_flOffsetTarget != flScrollTarget) )
	{
		pScrollBar->Normalize( pScrollBar->BLastMoveImmediate() );

		if ( pScrollBar->BLastMoveImmediate() )
		{
			// No animation if the mouse is down
			pScrollData->m_flOffset = flScrollTarget;
			pScrollData->m_flOffsetTarget = FLT_MAX;
		}
		else
		{
			// if we are scrolling and our target changed, start transitioning to there
			if ( pScrollData->m_flOffsetTarget != FLT_MAX )
			{
				pScrollData->m_flOffset = GetInterpolatedScrollOffset( pScrollData );
			}

			pScrollData->m_flTransitionStart = UIEngine()->GetCurrentFrameTime();
			pScrollData->m_flOffsetTarget = flScrollTarget;
			pScrollData->m_eTimingFunction = pScrollBar->GetTransitionTimingFunction();
			pScrollData->m_flTransitionDuration = pScrollBar->GetTransitionDuration() / s_convarTransitionTimeFactor.GetFloat();
			
			if ( pScrollData->m_eTimingFunction == k_EAnimationCustomBezier )
			{
				Vector2D vecPoints[4];
				pScrollBar->GetTransitionControlPoints( vecPoints );
				pScrollData->m_flControlPoints[0] = vecPoints[1].x;
				pScrollData->m_flControlPoints[1] = vecPoints[1].y;
				pScrollData->m_flControlPoints[2] = vecPoints[2].x;
				pScrollData->m_flControlPoints[3] = vecPoints[2].y;
			}
			
			//Msg( "[%s] current=%f, target=%f\n", GetID(), pScrollData->m_flOffset, pScrollData->m_flOffsetTarget );
		}

		SetRepaint( k_EPanelRepaintFull );
		DispatchEventAsync( 0.0f, Scroll(), this );
	}

	bool bMaxScrollDistance = (pScrollBar->GetScrollWindowPosition() + pScrollBar->GetScrollWindowSize() >= pScrollBar->GetRangeMax());
	if ( !pScrollData->m_bDispatchedScrollMax && bMaxScrollDistance )
	{
		pScrollData->m_bDispatchedScrollMax = true;
		DispatchPanelEvent( k_symPropertyOnScrolledToBottom );		
		//Msg( "[%s] Dispatched: %s\n", GetID(), symScrollMax.String() );
	}
	else if ( pScrollData->m_bDispatchedScrollMax && !bMaxScrollDistance )
	{
		pScrollData->m_bDispatchedScrollMax = false;
		//Msg( "[%s] Cleared: %s\n", GetID(), symScrollMax.String() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: layout out mouse scroll regions
//-----------------------------------------------------------------------------
void CUIPanel::LayoutChildrenInHiding( float flFinalWidth, float flFinalHeight )
{
	if ( !m_pVecChildrenInHiding )
		return;

	// already took care of scrollbars, only do other panels
	FOR_EACH_VEC( *m_pVecChildrenInHiding, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_pVecChildrenInHiding->Element( i );
		if ( m_pVerticalScrollBar && m_pVerticalScrollBar->UIPanel() == pChild )
			continue;

		if ( m_pHorizontalScrollBar && m_pHorizontalScrollBar->UIPanel() == pChild )
			continue;

		CUILength marginLeft, marginTop, marginRight, marginBottom;
		pChild->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flFinalWidth );
		marginRight.ConvertToLength( flFinalWidth );
		marginTop.ConvertToLength( flFinalHeight );
		marginBottom.ConvertToLength( flFinalHeight );

		pChild->DesiredLayoutSizeTraverse( flFinalWidth, flFinalHeight );
		pChild->LayoutTraverse( 0.0f, 0.0f, flFinalWidth - marginLeft.GetValue() - marginRight.GetValue(), flFinalHeight - marginTop.GetValue() - marginBottom.GetValue() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Paints invisible children
//-----------------------------------------------------------------------------
void CUIPanel::PaintChildrenInHiding()
{
	if ( !m_pVecChildrenInHiding )
		return;

	// need to also paint scrollbars
	FOR_EACH_VEC( *m_pVecChildrenInHiding, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_pVecChildrenInHiding->Element( i );
		pChild->PaintTraverse( NULL, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called by LayoutTraverse to set position for flowing and custom layouts. Makes sure position doesn't transition 
//			on the first layout pass after applying the transition property for position
//-----------------------------------------------------------------------------
void CUIPanel::SetPositionFromLayoutTraverse( CUILength x, CUILength y, CUILength z )
{
	if ( m_unPanelLayoutFlags & k_EPanelLayoutSkipLayoutPositionTransition )
	{
		m_unPanelLayoutFlags &= ~k_EPanelLayoutSkipLayoutPositionTransition;
		AccessStyleDirty()->SetPositionWithoutTransition( x, y, z );
	}
	else
	{
		AccessStyleDirty()->SetPosition( x, y, z );
	}
}


//-----------------------------------------------------------------------------
// Purpose: If the panel has vertical scrolling, scroll all the way to the top
//-----------------------------------------------------------------------------
void CUIPanel::ScrollToTop()
{
	EnsureScrollData( m_pVerticalScrollData ).m_flInitialPos = 0.0f;
	if( m_pVerticalScrollBar )
		m_pVerticalScrollBar->SetScrollWindowPosition( 0.0f );
}


//-----------------------------------------------------------------------------
// Purpose: If the panel has vertical scrolling, scroll all the way to the bottom
//-----------------------------------------------------------------------------
void CUIPanel::ScrollToBottom()
{
	// Since we may not have already done layout, just set to max and let next layout pass
	// fix it to the correct max window value.
	EnsureScrollData( m_pVerticalScrollData ).m_flInitialPos = FLT_MAX;
	if( m_pVerticalScrollBar )
		m_pVerticalScrollBar->SetScrollWindowPosition( FLT_MAX );
}


//-----------------------------------------------------------------------------
// Purpose: If the panel has horizontal scrolling, scroll all the way to the left
//-----------------------------------------------------------------------------
void CUIPanel::ScrollToLeftEdge()
{
	EnsureScrollData( m_pHorizontalScrollData ).m_flInitialPos = 0.0f;
	if( m_pHorizontalScrollBar )
		m_pHorizontalScrollBar->SetScrollWindowPosition( 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: If the panel has horizontal scrolling, scroll all the way to the right
//-----------------------------------------------------------------------------
void CUIPanel::ScrollToRightEdge()
{
	// Since we may not have already done layout, just set to max and let next layout pass
	// fix it to the correct max window value.
	EnsureScrollData( m_pHorizontalScrollData ).m_flInitialPos = FLT_MAX;
	if( m_pHorizontalScrollBar )
		m_pHorizontalScrollBar->SetScrollWindowPosition( FLT_MAX );
}


//-----------------------------------------------------------------------------
// Purpose: Scroll to fit the specified region into view (if possible)
//-----------------------------------------------------------------------------
void CUIPanel::ScrollToFitRegion( float x0, float x1, float y0, float y1, ScrollBehavior_t behavior, bool bDirectParentScrollOnly /* = false */, bool bImmediateScroll /* = false */ )
{
	bool bHandled = false;
	
	if( m_pVerticalScrollBar )
	{
		// include overscroll. Do this here instead of outside so we don't pass overscroll to parents
		bHandled = true;
		y0 = Max( y0 - m_pVerticalScrollData->m_flOverscroll, m_pVerticalScrollBar->GetRangeMin() );
		y1 = Min( y1 + m_pVerticalScrollData->m_flOverscroll, m_pVerticalScrollBar->GetRangeMax() );
		float flPosition = -GetInterpolatedYScrollOffset();

		if ( behavior == SCROLL_BEHAVIOR_SCROLL_TO_CENTER )
		{
			float pos = ( y0 + y1 - m_pVerticalScrollBar->GetScrollWindowSize() ) * 0.5f;
			m_pVerticalScrollData->m_flInitialPos = pos;
			m_pVerticalScrollBar->SetScrollWindowPosition( pos, bImmediateScroll );
		}
		else if ( ( y0 < flPosition ) || ( behavior == SCROLL_BEHAVIOR_SCROLL_TO_TOPLEFT_EDGE ) )
		{
			m_pVerticalScrollData->m_flInitialPos = y0;
			m_pVerticalScrollBar->SetScrollWindowPosition( y0, bImmediateScroll );
		}
		else if ( ( y1 > flPosition + m_pVerticalScrollBar->GetScrollWindowSize() ) || ( behavior == SCROLL_BEHAVIOR_SCROLL_TO_BOTTOMRIGHT_EDGE ) )
		{
			float pos = flPosition + y1 - (flPosition + m_pVerticalScrollBar->GetScrollWindowSize());
			pos = MIN( pos, y0 );
			m_pVerticalScrollData->m_flInitialPos = pos;
			m_pVerticalScrollBar->SetScrollWindowPosition( pos, bImmediateScroll );
		}
	}

	if( m_pHorizontalScrollBar )
	{
		// include overscroll. Do this here instead of outside so we don't pass overscroll to parents
		bHandled = true;
		x0 = Max( x0 - m_pHorizontalScrollData->m_flOverscroll, m_pHorizontalScrollBar->GetRangeMin() );
		x1 = Min( x1 + m_pHorizontalScrollData->m_flOverscroll, m_pHorizontalScrollBar->GetRangeMax() );
		float flPosition = -GetInterpolatedXScrollOffset();

		bHandled = true;
		if ( behavior == SCROLL_BEHAVIOR_SCROLL_TO_CENTER )
		{
			float pos = ( x0 + x1 - m_pHorizontalScrollBar->GetScrollWindowSize() ) * 0.5f;
			m_pHorizontalScrollData->m_flInitialPos = pos;
			m_pHorizontalScrollBar->SetScrollWindowPosition( pos, bImmediateScroll );
		}
		else if ( ( x0 < flPosition ) || ( behavior == SCROLL_BEHAVIOR_SCROLL_TO_TOPLEFT_EDGE ) )
		{
			m_pHorizontalScrollData->m_flInitialPos = x0;
			m_pHorizontalScrollBar->SetScrollWindowPosition( x0, bImmediateScroll );
		}
		else if ( ( x1 > flPosition + m_pHorizontalScrollBar->GetScrollWindowSize() ) || ( behavior == SCROLL_BEHAVIOR_SCROLL_TO_BOTTOMRIGHT_EDGE ) )
		{
			float pos = flPosition + x1 - (flPosition + m_pHorizontalScrollBar->GetScrollWindowSize());
			pos = MIN( pos, x0 );
			m_pHorizontalScrollData->m_flInitialPos = pos;
			m_pHorizontalScrollBar->SetScrollWindowPosition( pos, bImmediateScroll );
		}
	}


	// Traverse up so any parents that scroll also scroll to make this region visible, if they can.  We stop at the first parent
	// with any scrollbars and do not support auto scrolling within nested scrolling regions.
	if( !bDirectParentScrollOnly && !bHandled && m_pParent )
	{
		float flX = GetActualXOffset();
		float flY = GetActualYOffset();
		m_pParent->ScrollToFitRegion( x0 + flX, x1 + flX, y0 + flY, y1 + flY, behavior, false, bImmediateScroll );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Render size is the size of the content for rendering, this is either the actual layout size, or if
//			that is smaller than the content size + padding then it's the content size + padding.
//-----------------------------------------------------------------------------
float CUIPanel::GetActualRenderWidth()
{
	if( m_flActualLayoutWidth > m_flContentWidth || !m_pHorizontalScrollBar )
		return m_flActualLayoutWidth;
	else
		return m_flContentWidth;
}


//-----------------------------------------------------------------------------
// Purpose: Render size is the size of the content for rendering, this is either the actual layout size, or if
//			that is smaller than the content size + padding then it's the content size + padding.
//-----------------------------------------------------------------------------
float CUIPanel::GetActualRenderHeight()
{
	if( m_flActualLayoutHeight > m_flContentHeight || !m_pVerticalScrollBar )
		return m_flActualLayoutHeight;
	else
		return m_flContentHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Returns X offset to draw panel in parent (includes padding-left for parent, border-left for parent, margin-left for this panel, flowing layout offset/position
//-----------------------------------------------------------------------------
float CUIPanel::GetActualXOffset() const
{
	// use the old offset.. don't force a style update
	CUILength x, y, z;
	bool bFinal = true;
	m_style.GetInterpolatedPosition( x, y, z, bFinal );

	float flParentWidth = m_pParent ? m_pParent->GetActualLayoutWidth() : GetParentWindow()->GetSurfaceWidth();
	float flX = m_flActualXOffset + x.GetValueAsLength( flParentWidth );
	return flX;
}


//-----------------------------------------------------------------------------
// Purpose: Returns Y offset to draw panel in parent (includes padding-top for parent, border-top for parent, margin-top for this panel, flowing layout offset/position
//-----------------------------------------------------------------------------
float CUIPanel::GetActualYOffset() const
{
	// use the old offset.. don't force a style update
	CUILength x, y, z;
	bool bFinal = true;
	m_style.GetInterpolatedPosition( x, y, z, bFinal );

	float flParentHeight = m_pParent ? m_pParent->GetActualLayoutHeight() : GetParentWindow()->GetSurfaceHeight();
	float flY = m_flActualYOffset + y.GetValueAsLength( flParentHeight );
	return flY;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to update scroll offset times
//-----------------------------------------------------------------------------
void CUIPanel::UpdateScrollOffset( ScrollBarData_t *pScrollData )
{
	if ( !pScrollData || pScrollData->m_flOffsetTarget == FLT_MAX )
		return;

	float flTimeProgress = ((UIEngine()->GetCurrentFrameTime() - pScrollData->m_flTransitionStart)) / pScrollData->m_flTransitionDuration;
	flTimeProgress = clamp( flTimeProgress, 0.0f, 1.0f );
	if ( flTimeProgress >= 0.999999f )
	{
		pScrollData->m_flOffset = pScrollData->m_flOffsetTarget;
		pScrollData->m_flOffsetTarget = FLT_MAX;
		DispatchEventAsync( 0.0f, Scroll(), this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get interpolated scroll position
//-----------------------------------------------------------------------------
float CUIPanel::GetInterpolatedScrollOffset( ScrollBarData_t *pScrollData )
{
	if ( !pScrollData )
		return 0.0f;

	if ( pScrollData->m_flOffsetTarget == FLT_MAX )
		return pScrollData->m_flOffset;

	// must still be transitioning
	float flTimeProgress = ((UIEngine()->GetCurrentFrameTime() - pScrollData->m_flTransitionStart)) / pScrollData->m_flTransitionDuration; 
	flTimeProgress = clamp( flTimeProgress, 0.0f, 1.0f );

	CCubicBezierCurve<Vector2D> bezier;
	Vector2D vec[4];
	if ( pScrollData->m_eTimingFunction != k_EAnimationCustomBezier )
	{
		panorama::GetAnimationCurveControlPoints( pScrollData->m_eTimingFunction, vec );
	}
	else
	{
		vec[0].x = vec[0].y = 0.0;
		vec[1].x = pScrollData->m_flControlPoints[0];
		vec[1].y = pScrollData->m_flControlPoints[1];
		vec[2].x = pScrollData->m_flControlPoints[2];
		vec[2].y = pScrollData->m_flControlPoints[3];
		vec[3].x = vec[3].y = 1.0;
	}
	bezier.SetControlPoints( vec );

	flTimeProgress = GetProgressForTimingFunction( bezier, flTimeProgress );

	return Lerp( flTimeProgress, pScrollData->m_flOffset, pScrollData->m_flOffsetTarget );
}


/*static*/ void CUIPanel::GetContextScrollTransitionControlPoints( ScrollBarData_t *pScrollData, Vector2D( &vecPoints )[ 4 ] )
{
	vecPoints[ 0 ].x = vecPoints[ 0 ].y = 0.0;
	vecPoints[ 1 ].x = pScrollData ? pScrollData->m_flControlPoints[ 0 ] : 0.0f;
	vecPoints[ 1 ].y = pScrollData ? pScrollData->m_flControlPoints[ 1 ] : 0.0f;
	vecPoints[ 2 ].x = pScrollData ? pScrollData->m_flControlPoints[ 2 ] : 0.0f;
	vecPoints[ 2 ].y = pScrollData ? pScrollData->m_flControlPoints[ 3 ] : 0.0f;
	vecPoints[ 3 ].x = vecPoints[ 3 ].y = 1.0;
}


//-----------------------------------------------------------------------------
// Purpose: Get bezier curve for horizontal scroll transition
//-----------------------------------------------------------------------------
void CUIPanel::GetContextXScrollTransitionControlPoints( Vector2D (&vecPoints)[4] ) const
{
	GetContextScrollTransitionControlPoints( m_pHorizontalScrollData, vecPoints );
}


//-----------------------------------------------------------------------------
// Purpose: Get bezier curve for vertical scroll transition
//-----------------------------------------------------------------------------
void CUIPanel::GetContextYScrollTransitionControlPoints( Vector2D (&vecPoints)[4] ) const
{
	GetContextScrollTransitionControlPoints( m_pVerticalScrollData, vecPoints );
}


//-----------------------------------------------------------------------------
// Purpose: Updates X scroll offset
//-----------------------------------------------------------------------------
void CUIPanel::UpdateScrollOffsetX()
{
	UpdateScrollOffset( m_pHorizontalScrollData );
}


//-----------------------------------------------------------------------------
// Purpose: Get interpolated X scroll position
//-----------------------------------------------------------------------------
float CUIPanel::GetInterpolatedXScrollOffset()
{
	UpdateScrollOffsetX();
	return GetInterpolatedScrollOffset( m_pHorizontalScrollData );	
}


//-----------------------------------------------------------------------------
// Purpose: Updates Y scroll offset
//-----------------------------------------------------------------------------
void CUIPanel::UpdateScrollOffsetY()
{
	UpdateScrollOffset( m_pVerticalScrollData );
}


//-----------------------------------------------------------------------------
// Purpose: Get interpolated Y scroll position
//-----------------------------------------------------------------------------
float CUIPanel::GetInterpolatedYScrollOffset()
{
	UpdateScrollOffsetY();
	return GetInterpolatedScrollOffset( m_pVerticalScrollData );
}


//-----------------------------------------------------------------------------
// Purpose: Return true if a scroll operation is currently in progress
//-----------------------------------------------------------------------------
bool CUIPanel::BScrollInProgress()
{
	if ( ClientPtr()->BCustomScrollInProgress() )
	{
		return true;
	}

	UpdateScrollOffsetX();
	UpdateScrollOffsetY();

	if ( ( m_pHorizontalScrollData && m_pHorizontalScrollData->m_flOffsetTarget != FLT_MAX ) ||
		( m_pVerticalScrollData && m_pVerticalScrollData->m_flOffsetTarget != FLT_MAX ) )
	{
		return true;
	}

	return false;
}

CUIPanel::ScrollBarData_t &CUIPanel::EnsureScrollData( ScrollBarData_t *&pScrollData )
{
	if ( !pScrollData )
	{
		pScrollData = new ScrollBarData_t();
		V_memset( pScrollData, 0, sizeof( ScrollBarData_t ) );
		pScrollData->m_flOffsetTarget = FLT_MAX;
	}
	return *pScrollData;
}


//-----------------------------------------------------------------------------
// Purpose: Stop a scroll in progress and return the point at which it stopped
//-----------------------------------------------------------------------------
float CUIPanel::StopScroll( ScrollBarData_t *pScrollData )
{
	if ( !pScrollData )
		return 0.0f;

	if ( pScrollData->m_flOffsetTarget == FLT_MAX )
		return -pScrollData->m_flOffset;

	float flNow = StopAnimationOfPropertyUntilFrameUpdateAndGetStopTime( STYLE_SYMBOL_SCROLL );

	// must still be transitioning
	float flTimeProgress = ((flNow - pScrollData->m_flTransitionStart)) / pScrollData->m_flTransitionDuration; 
	flTimeProgress = clamp( flTimeProgress, 0.0f, 1.0f );

	CCubicBezierCurve<Vector2D> bezier;
	Vector2D vec[4];
	if ( pScrollData->m_eTimingFunction != k_EAnimationCustomBezier )
	{
		panorama::GetAnimationCurveControlPoints( pScrollData->m_eTimingFunction, vec );
	}
	else
	{
		vec[0].x = vec[0].y = 0.0;
		vec[1].x = pScrollData->m_flControlPoints[0];
		vec[1].y = pScrollData->m_flControlPoints[1];
		vec[2].x = pScrollData->m_flControlPoints[2];
		vec[2].y = pScrollData->m_flControlPoints[3];
		vec[3].x = vec[3].y = 1.0;
	}
	bezier.SetControlPoints( vec );

	flTimeProgress = GetProgressForTimingFunction( bezier, flTimeProgress );

	float flFinal = Lerp( flTimeProgress, pScrollData->m_flOffset, pScrollData->m_flOffsetTarget );
	pScrollData->m_flOffset = flFinal;
	pScrollData->m_flOffsetTarget = FLT_MAX;
	return -flFinal;
}

float CUIPanel::StopHorizontalScroll()
{
	return StopScroll( m_pHorizontalScrollData );
}

float CUIPanel::StopVerticalScroll()
{
	return StopScroll( m_pVerticalScrollData );
}


//-----------------------------------------------------------------------------
// Purpose: Does the panel implement drag scrolling behaviors?
//-----------------------------------------------------------------------------
bool CUIPanel::BCanDragScroll()
{
	if ( ClientPtr()->BCustomCanDragScroll() )
	{
		return true;
	}

	if ( m_pVerticalScrollBar || m_pHorizontalScrollBar )
	{
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Can the panel scroll it's contents further up
//-----------------------------------------------------------------------------
bool CUIPanel::BCanScrollUp()
{
	if ( ClientPtr()->BCanCustomScrollUp() )
		return true;

	if( !m_pVerticalScrollBar )
		return false;

	if( m_pVerticalScrollBar->GetScrollWindowPosition() > 0 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Can the panel scroll it's contents further down
//-----------------------------------------------------------------------------
bool CUIPanel::BCanScrollDown()
{
	if ( ClientPtr()->BCanCustomScrollDown() )
		return true;

	if( !m_pVerticalScrollBar )
		return false;

	if( m_pVerticalScrollBar->GetScrollWindowPosition() + m_pVerticalScrollBar->GetScrollWindowSize() >= m_pVerticalScrollBar->GetRangeMax() )
		return false;

	return true;
}

bool CUIPanel::BCanScrollLeft()
{
	if ( ClientPtr()->BCanCustomScrollLeft() )
		return true;

	if ( !m_pHorizontalScrollBar )
		return false;

	if ( m_pHorizontalScrollBar->GetScrollWindowPosition() > 0 )
		return true;

	return false;
}

bool CUIPanel::BCanScrollRight()
{
	if ( ClientPtr()->BCanCustomScrollRight() )
		return true;

	if ( !m_pHorizontalScrollBar )
		return false;

	if ( m_pHorizontalScrollBar->GetScrollWindowPosition() + m_pHorizontalScrollBar->GetScrollWindowSize() >= m_pHorizontalScrollBar->GetRangeMax() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Removes a style class from this panel
//-----------------------------------------------------------------------------
void CUIPanel::RemoveClass( const char *pchName )
{
	CPanoramaSymbol sym( pchName );
	RemoveClasses( &sym, 1 );
}


//-----------------------------------------------------------------------------
// Purpose: Remove all style classes from this panel
//-----------------------------------------------------------------------------
void CUIPanel::RemoveAllClasses()
{
	CUtlVector<CPanoramaSymbol> vecClasses;
	vecClasses.AddMultipleToTail( m_vecStyleClasses.Count(), m_vecStyleClasses.Base() );
	RemoveClasses( vecClasses.Base(), vecClasses.Count() );
}


//-----------------------------------------------------------------------------
// Purpose: Remove multiple style classes from this panel
//-----------------------------------------------------------------------------
void CUIPanel::RemoveClasses( const CPanoramaSymbol *pSymbols, uint cSymbols )
{
	bool bChanged = false;
	for( uint i = 0; i < cSymbols; i++ )
	{
		int iVec = m_vecStyleClasses.Find( pSymbols[i] );
		if( iVec == m_vecStyleClasses.InvalidIndex() )
			continue;

		bChanged = true;
		break;
	}

	if( !bChanged )
		return;

	// Need to apply previous styles before removing classes which will change them, if we were dirty before.
	{
		VPROF_BUDGET( "CPanel2D::RemoveClasses - apply old dirty styles", VPROF_BUDGETGROUP_TENFOOT );
		if( BStylesDirty() )
			ApplyStyles( true );
	}

	// Perf - Copy of m_vecStyleClasses vector. 
	// Might be an issue if AddClassesInternal is called frequently
	CUtlPtrArray< CPanoramaSymbol > arrOldClasses;
	arrOldClasses.Copy( m_vecStyleClasses.Base(), m_vecStyleClasses.Count() );

	for( uint i = 0; i < cSymbols; i++ )
	{
		int iVec = m_vecStyleClasses.Find( pSymbols[i] );
		if( iVec == m_vecStyleClasses.InvalidIndex() )
			continue;

		FOR_EACH_VEC( m_vecChildren, iChild )
		{
			((CUIPanel*)m_vecChildren[iChild])->RemoveClassFromChildLookupMaps( m_vecStyleClasses[iVec], this );
		}

		m_vecStyleClasses.Remove( iVec );
	}

	DispatchEventAsync( StyleClassesChanged(), this );

	CUtlPtrArray< CPanoramaSymbol > arrChangedClasses;
	CUtlPtrArray< CPanoramaSymbol > arrNewClasses;

	arrChangedClasses.TakeOwnership( const_cast< CPanoramaSymbol * >( pSymbols ), cSymbols );
	arrNewClasses.TakeOwnership( m_vecStyleClasses.Base(), m_vecStyleClasses.Count() );


	// Check if children need to be invalidated in cases where a descendant selector includes the new class
	// Can end up with false positives but it is better than invalidating the style of all children
	bool bInvalidateChildren = ( s_convarPanoramaClassesForceInvalidate.GetBool() || BHasAnyDescendantSelectorMatchingClasses( arrChangedClasses, arrOldClasses, arrNewClasses ) );
	arrChangedClasses.DetatchAndClear();
	arrNewClasses.DetatchAndClear();

	// need to apply to children as they could have a descendant selector which included this class
	MarkStylesDirty( bInvalidateChildren );
}

//-----------------------------------------------------------------------------
// Purpose: Removes multiple classes to a panel. String is space separated
//-----------------------------------------------------------------------------
void CUIPanel::RemoveClasses( const char *pchName )
{
	CUtlVector< CPanoramaSymbol > vecClassSymbols;
	ParseClassSymbols( &vecClassSymbols, pchName );
	RemoveClasses( vecClassSymbols.Base(), vecClassSymbols.Count() );
}



//-----------------------------------------------------------------------------
// Purpose: Set that this panel accepts input
//-----------------------------------------------------------------------------
void CUIPanel::SetAcceptsInput( bool bAllowInput )
{
	if( bAllowInput )
		m_unInputFlags |= (uint32)k_EInputAccept;
	else
		m_unInputFlags &= ~((uint32)k_EInputAccept);
}


//-----------------------------------------------------------------------------
// Purpose: Set that this panel accepts input
//-----------------------------------------------------------------------------
void CUIPanel::SetDisableFocusOnMouseDown( bool bDisable )
{
	if( bDisable )
		m_unInputFlags |= (uint32)k_EDisableFocusOnMouseDown;
	else
		m_unInputFlags &= ~((uint32)k_EDisableFocusOnMouseDown);
}


//-----------------------------------------------------------------------------
// Purpose: Set that this panel accepts focus
//-----------------------------------------------------------------------------
void CUIPanel::SetAcceptsFocus( bool bAllowFocus )
{
	if( bAllowFocus )
		m_unInputFlags |= (uint32)k_EInputAcceptFocus;
	else
		m_unInputFlags &= ~((uint32)k_EInputAcceptFocus);
}


//-----------------------------------------------------------------------------
// Purpose: Set the default focus for this panel
//-----------------------------------------------------------------------------
void CUIPanel::SetDefaultFocus( const char *pchChildID )
{
	m_strDefaultFocus = pchChildID;
}


//-----------------------------------------------------------------------------
// Purpose: Return the default focus for this panel. This is not necessarily
//			the same as what you get from GetDefaultInputFocus, which drills
//			down into children.
//-----------------------------------------------------------------------------
const char * CUIPanel::GetDefaultFocus() const
{
	return m_strDefaultFocus.String();
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the panel which should be used for default focus. Can return NULL
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::GetDefaultInputFocus()
{
	// First check if client panel has overridden default behavior
	IUIPanel *pInputFocus = ClientPtr()->OnGetDefaultInputFocus();
	if( pInputFocus )
		return pInputFocus;

	// If we're remembering our child focus and we've seen one, return that, unless it's
	// in one of the myriad states where it shouldn't accept focus.
	if ( m_bRememberChildFocus && 
		m_pLastFocusedChild.Get() != NULL && 
		m_pLastFocusedChild->BAcceptsFocus() &&
		m_pLastFocusedChild->IsEnabled() &&
		m_pLastFocusedChild->BIsVisible() &&
		!m_pLastFocusedChild->BIsTransparent() )
	{
		return m_pLastFocusedChild.Get();
	}

	if( !m_strDefaultFocus.IsEmpty() )
	{
		// Always allow defaulting to immediate children, otherwise just within layout file since this is normally set by layout file
		pInputFocus = FindChild( m_strDefaultFocus.String() );
		if( !pInputFocus )
			pInputFocus = FindChildInLayoutFile( m_strDefaultFocus.String() );

		if( !pInputFocus )
		{
			Msg( "defaultfocus=%s not found within layout file for %s\n", m_strDefaultFocus.String(), m_strID.String() );
		}
		else
		{
			pInputFocus = pInputFocus->GetDefaultInputFocus();
			if( pInputFocus )
				return pInputFocus;
		}
	}

	// if we accept focus, set focus to ourselves
	if( BAcceptsFocus() )
		return this;

	// try to set to a child
	pInputFocus = FindFirstChildAcceptingFocusTraverse();
	if( pInputFocus )
		return pInputFocus;

	// failed to find an appropriate place to set focus
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find the first child accepting input in our entire hierarchy
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindFirstChildAcceptingFocusTraverse()
{
	// children should get a chance at being key focus first
	FOR_EACH_VEC( m_vecChildren, i )
	{
		IUIPanel *pChild = m_vecChildren[i];
		if( !pChild->IsEnabled() )
			continue;

		if( !pChild->BIsVisible() )
			continue;

		if ( pChild->AccessIUIStyle()->BIsTransparentWithNoOpacityTransition() )
			continue;

		IUIPanel * pFocusChild = pChild->GetDefaultInputFocus();
		if( pFocusChild )
			return pFocusChild;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Sets focus to this panel or a descendant when appropriate
//-----------------------------------------------------------------------------
bool CUIPanel::SetFocus()
{
	return SetFocusInternal( false, true );
}


//-----------------------------------------------------------------------------
// Purpose: Sets focus to this panel or a descendant when appropriate due to a panel hover,
// this won't scroll the focused panel into view if it's in a parent with overflow: scroll
// that's the difference between on hover vs normal SetFocus().
//-----------------------------------------------------------------------------
bool CUIPanel::SetFocusDueToHover()
{
	return SetFocusInternal( true, true );
}


//-----------------------------------------------------------------------------
// Purpose: Updates focus within our panels context, does not force the context
// to switch if we were not active, unlike SetFocus() in that way
//-----------------------------------------------------------------------------
bool CUIPanel::UpdateFocusInContext()
{
	return SetFocusInternal( false, false );
}


//-----------------------------------------------------------------------------
// Purpose: Internal helper for setting focus
//-----------------------------------------------------------------------------
bool CUIPanel::SetFocusInternal( bool bDueToHover, bool bChangeContextIfNeeded )
{
	VPROF_BUDGET( "CUIPanel::SetFocusInternal", VPROF_BUDGETGROUP_TENFOOT );
	Assert( IsEnabled() );
	Assert( !UIEngine()->BIsPanelWaitingAsyncDelete( this ) );

	if ( UIEngine()->BIsPanelWaitingAsyncDelete( this ) )
	{
		// don't allow focus to go to a panel that is in a queued delete
		return false;
	}

	// try to set focus on the default input panel
	CUIPanel *pFocus = (CUIPanel*)GetDefaultInputFocus();

	// if we failed to find an appropriate place to set focus.. just set to ourselves
	if( !pFocus )
		pFocus = this;

	if( pFocus->BAcceptsFocus() )
	{
		GetParentWindow()->UIWindowInput()->SetInputFocus( pFocus, !bDueToHover && BScrollParentToFitWhenFocused(), bChangeContextIfNeeded );
		return true;
	}
	else if( pFocus != this )
	{
		return pFocus->SetFocusInternal( bDueToHover, bChangeContextIfNeeded );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: return true if this panel is fully transparent and not animating out of that state
//-----------------------------------------------------------------------------
bool CUIPanel::BIsTransparent() const
{
	return ((CUIPanel*)(this))->AccessStyle()->BIsTransparentWithNoOpacityTransition();
}


//-----------------------------------------------------------------------------
// Purpose: Finds the ancestor of this panel with the specified ID
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindAncestor( const char *pchID ) const
{
	for( IUIPanel *pParent = GetParent(); pParent != NULL; pParent = pParent->GetParent() )
	{
		if( V_stricmp( pParent->GetID(), pchID ) == 0 )
			return pParent;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find the lowest common ancestor between this panel and another panel
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindLowestCommonAncestor( IUIPanel *pOther ) const
{
	if ( !pOther )
		return nullptr;

	CUtlVector< IUIPanel * > vecThisPath;
	CUtlVector< IUIPanel * > vecOtherPath;

	// This number is pretty arbitrary, but our tree depths aren't normally going to be that deep.
	// So allocate once up front to save cost in reallocating later.
	vecThisPath.EnsureCapacity( 32 );
	vecOtherPath.EnsureCapacity( 32 );

	for ( IUIPanel *pThisParent = GetParent(); pThisParent != nullptr; pThisParent = pThisParent->GetParent() )
	{
		vecThisPath.AddToTail( pThisParent );
	}
	for ( IUIPanel *pOtherParent = pOther->GetParent(); pOtherParent != nullptr; pOtherParent = pOtherParent->GetParent() )
	{
		vecOtherPath.AddToTail( pOtherParent );
	}

	// Walk backwards through both arrays until they differ
	IUIPanel *pCommonAncestor = nullptr;
	int nThisIndex = vecThisPath.Count() - 1;
	int nOtherIndex = vecOtherPath.Count() - 1;
	while ( nThisIndex >= 0 && nOtherIndex >= 0 )
	{
		if ( vecThisPath[ nThisIndex ] != vecOtherPath[ nOtherIndex ] )
			break;

		pCommonAncestor = vecThisPath[ nThisIndex ];

		nThisIndex--;
		nOtherIndex--;
	}

	return pCommonAncestor;
}


//-----------------------------------------------------------------------------
// Purpose: Restores focus to the input context we are within, without actually 
// changing the focused child.  Will set to default focus if this context has
// no prior focus.
//-----------------------------------------------------------------------------
void CUIPanel::SetInputContextFocus()
{
	if( !GetParentWindow()->UIWindowInput()->SetInputFocusContext( this ) )
		SetFocus();
}


//-----------------------------------------------------------------------------
// Purpose: Find input context for this panel
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::GetParentInputContext()
{
	if( BTopOfInputContext() )
		return this;

	IUIPanel *pParent = GetParent();
	while( pParent )
	{
		if( pParent->BTopOfInputContext() )
			return pParent;

		pParent = pParent->GetParent();
	}

	return pParent;
}


//-----------------------------------------------------------------------------
// Purpose: Check if any descendant selector include the given style flag (and 
//			is a match for the given panel (id, classes, type, style flag, not selector)
//
//			Note that it is not enough to check descendant selectors for the 
//			current layout files. You also need to check descendant selectors
//			for layout files loaded by children (which could be different)
//
//			Imagine the case
//
//			<Panel id="Outer">
//			  <Panel id="Middle">
//			    <Panel id="Inner">
//			    </Panel>
//			  </Panel>
//			</Panel>
//
//			where each panel come from a different layout file
//			The inner layout file could then have a selector
//			#Outer:hover #Inner { ... }
//-----------------------------------------------------------------------------
bool CUIPanel::BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags ) const
{
	CUtlVector< CPanoramaSymbol > vecVisitedStyleFiles( 0, 8 );
	CUtlVector< CPanoramaSymbol > vecVisitedLayoutFiles( 0, 8 );

	return BHasAnyDescendantSelectorMatchingStyleFlagRecursive( eStyleFlag, nOldPanelFlags, nNewPanelFlags, *this, vecVisitedLayoutFiles, vecVisitedStyleFiles );
}


bool CUIPanel::BHasAnyDescendantSelectorMatchingStyleFlagRecursive( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedLayoutFiles, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const
{
	if ( m_pLayoutFile.Get() )
	{
		// if this panel was loaded from a separate layout file, also apply styles from that panel
		LayoutFilePtr_t pLayoutFileLoadedFrom;
		if ( m_pLayoutFile.Get() != m_pLayoutLoadedFrom.Get() )
		{
			LayoutFilePtr_t pFileLoadedFrom = m_pLayoutLoadedFrom;
			if ( !pFileLoadedFrom && GetParent() )
			{
				pFileLoadedFrom = ( (CUIPanel*)GetParent() )->m_pLayoutFile;
			}

			if ( pFileLoadedFrom.Get() && pFileLoadedFrom.Get() != m_pLayoutFile.Get() )
			{
				pLayoutFileLoadedFrom = pFileLoadedFrom;
			}
		}

		// let loaded from file override layout file
		if ( pLayoutFileLoadedFrom.Get() && !vecVisitedLayoutFiles.HasElement( pLayoutFileLoadedFrom->GetLayoutFileSymbol() ) )
		{
			if ( pLayoutFileLoadedFrom->BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, nOldPanelFlags, nNewPanelFlags, panel, vecVisitedStyleFiles ) )
			{
				return true;
			}
			vecVisitedLayoutFiles.AddToTail( pLayoutFileLoadedFrom->GetLayoutFileSymbol() );
		}

		if ( !vecVisitedLayoutFiles.HasElement( m_pLayoutFile->GetLayoutFileSymbol() ) )
		{
			if ( m_pLayoutFile->BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, nOldPanelFlags, nNewPanelFlags, panel, vecVisitedStyleFiles ) )
			{
				return true;
			}
			vecVisitedLayoutFiles.AddToTail( m_pLayoutFile->GetLayoutFileSymbol() );
		}
	}

	// Recursively check children layout files 

	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		if ( pChild->BHasAnyDescendantSelectorMatchingStyleFlagRecursive( eStyleFlag, nOldPanelFlags, nNewPanelFlags, panel, vecVisitedLayoutFiles, vecVisitedStyleFiles ) )
		{
			return true;
		}
	}
	if ( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( *m_pVecChildrenInHiding, i )
		{
			CUIPanel *pChild = (CUIPanel*)( *m_pVecChildrenInHiding )[i];
			if ( pChild->BHasAnyDescendantSelectorMatchingStyleFlagRecursive( eStyleFlag, nOldPanelFlags, nNewPanelFlags, panel, vecVisitedLayoutFiles, vecVisitedStyleFiles ) )
			{
				return true;
			}
		}
	}

	return false;
}



//-----------------------------------------------------------------------------
// Purpose: Sets a style flag on this panel
//-----------------------------------------------------------------------------
void CUIPanel::AddStyleFlag( EStyleFlags eStyleFlag )
{
	VPROF_BUDGET( "CUIPanel::AddStyleFlag", VPROF_BUDGETGROUP_TENFOOT );

	// if this flag is already set, skip recalculating styles
	if( (m_unStyleFlags & (uint)eStyleFlag) )
		return;

	// If this flag isn't allowed on this panel, don't let it be set
	if ( eStyleFlag & m_unDisallowedStyleFlags )
		return;

	short unOldStyleFlags = m_unStyleFlags;
	m_unStyleFlags |= eStyleFlag;

	// Style flag will make us re-evaluate styles, but since inspection isn't really a style
	// we won't think we need to repaint.  Explicitly invalidate to compensate.
	if( eStyleFlag == k_EStyleFlagInspect )
		SetRepaint( k_EPanelRepaintFull );

	if( eStyleFlag == k_EStyleFlagFocus )
		DispatchPanelEvent( k_symPropertyOnFocus );
	else if( eStyleFlag == k_EStyleFlagDescendantFocused )
		DispatchPanelEvent( k_symPropertyOnDescendantFocus );
	else if ( eStyleFlag == k_EStyleFlagSelected )
		DispatchPanelEvent( k_symPropertyOnSelect );
	else if ( eStyleFlag == k_EStyleFlagHover )
		DispatchPanelEvent( k_symPropertyOnMouseOver );

	if( eStyleFlag == k_EStyleFlagDisabled )
		AddDisabledFlagToChildren();

	DispatchEventAsync( StyleFlagsChanged(), this );

	// Check if children need to be invalidated in cases where a descendant selector includes the new flag
	// Can end up with false positives but it is better than invalidating the style of all children
	bool bInvalidateChildren = ( s_convarPanoramaStyleFlagForceInvalidate.GetBool() || BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, unOldStyleFlags, m_unStyleFlags ) );

	MarkStylesDirty( bInvalidateChildren );
}


//-----------------------------------------------------------------------------
// Purpose: Sets a group of style flags that are not allowed to be set on this panel
//-----------------------------------------------------------------------------
void CUIPanel::SetDisallowedStyleFlags( uint unDisallowedStyleFlags )
{
	if ( ( uint )m_unDisallowedStyleFlags == unDisallowedStyleFlags )
		return;

	m_unDisallowedStyleFlags = unDisallowedStyleFlags;

	// Remove anything that we just disallowed
	uint32 unValue = ( m_unDisallowedStyleFlags & m_unStyleFlags );
	int nStyleFlag = 1;
	while ( unValue != 0 )
	{
		if ( ( unValue & 1 ) != 0 )
		{
			RemoveStyleFlag( ( panorama::EStyleFlags )nStyleFlag );
		}

		unValue = unValue >> 1;
		nStyleFlag = nStyleFlag << 1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds parent disabled style flag this this panel and children
//-----------------------------------------------------------------------------
void CUIPanel::AddParentDisabledFlag()
{
	if( (m_unStyleFlags & k_EStyleFlagParentDisabled) != 0 )
		return;

	m_unStyleFlags |= k_EStyleFlagParentDisabled;
	AddDisabledFlagToChildren();
}


//-----------------------------------------------------------------------------
// Purpose: Removes the parent disabled style flag this this panel and children
//-----------------------------------------------------------------------------
void CUIPanel::RemoveParentDisabledFlag()
{
	if( (m_unStyleFlags & k_EStyleFlagParentDisabled) == 0 )
		return;

	m_unStyleFlags &= ~(k_EStyleFlagParentDisabled);

	// Stop traversal to children if we ourself are still disabled for any reason.
	if ( IsEnabled() )
	{
		RemoveDisabledFlagFromChildren();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a style flag to all children and their descendants
//-----------------------------------------------------------------------------
void CUIPanel::AddDisabledFlagToChildren()
{
	int nChildren = GetChildCount();
	for( int i = 0; i < nChildren; ++i )
		((CUIPanel*)GetChild( i ))->AddParentDisabledFlag();

	if( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( (*m_pVecChildrenInHiding), i )
		{
			((CUIPanel*)m_pVecChildrenInHiding->Element( i ))->AddParentDisabledFlag();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remove a style flag from all children and their descendants
//-----------------------------------------------------------------------------
void CUIPanel::RemoveDisabledFlagFromChildren()
{
	int nChildren = GetChildCount();
	for( int i = 0; i < nChildren; ++i )
		((CUIPanel*)GetChild( i ))->RemoveParentDisabledFlag();

	if( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( (*m_pVecChildrenInHiding), i )
		{
			((CUIPanel*)m_pVecChildrenInHiding->Element( i ))->RemoveParentDisabledFlag();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes a style flag on this panel
//-----------------------------------------------------------------------------
void CUIPanel::RemoveStyleFlag( EStyleFlags eStyleFlag )
{
	// if this flag is not set, skip recalculating styles
	if( (m_unStyleFlags & (uint)eStyleFlag) == 0 )
		return;

	short unOldStyleFlags = m_unStyleFlags;
	m_unStyleFlags &= ~eStyleFlag;

	if ( eStyleFlag == k_EStyleFlagDescendantFocused )
		return;

	// Style flag will make us re-evaluate styles, but since inspection isn't really a style
	// we won't think we need to repaint.  Explicitly invalidate to compensate.
	if( eStyleFlag == k_EStyleFlagInspect )
		SetRepaint( k_EPanelRepaintFull );

	if( eStyleFlag == k_EStyleFlagFocus )
		DispatchPanelEvent( k_symPropertyOnBlur );
	else if( eStyleFlag == k_EStyleFlagDescendantFocused )
		DispatchPanelEvent( k_symPropertyOnDescendantBlur );
	else if ( eStyleFlag == k_EStyleFlagSelected )
		DispatchPanelEvent( k_symPropertyOnDeselect );
	else if ( eStyleFlag == k_EStyleFlagHover )
		DispatchPanelEvent( k_symPropertyOnMouseOut );

	if( eStyleFlag == k_EStyleFlagDisabled )
		RemoveDisabledFlagFromChildren();

	DispatchEventAsync( StyleFlagsChanged(), this );

	// Check if children need to be invalidated in cases where a descendant selector includes the new flag
	// Can end up with false positives but it is better than invalidating the style of all children
	bool bInvalidateChildren = ( s_convarPanoramaStyleFlagForceInvalidate.GetBool() || BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, unOldStyleFlags, m_unStyleFlags ) );

	MarkStylesDirty( bInvalidateChildren );

	// hide tooltip when mouse is no longer hovering over this panel
	if( (eStyleFlag & k_EStyleFlagHover) )
		ClientPtr()->HideTooltip();
}


//-----------------------------------------------------------------------------
// Purpose: Setup v8 object template for panel type
//-----------------------------------------------------------------------------
void CUIPanel::SetupJavascriptObjectTemplate()
{
	ClientPtr()->SetupJavascriptObjectTemplate();

	RegisterJSMethodRaw( "SetPanelEvent", PANORAMA_DELEGATE( &CUIPanel::SetPanelEventJS ) );
	RegisterJSAccessor( "rememberchildfocus", PANORAMA_DELEGATE( &CUIPanel::GetRememberChildFocus) , PANORAMA_DELEGATE( &CUIPanel::SetRememberChildFocus ) );
	RegisterJSAccessorReadOnly( "paneltype", PANORAMA_DELEGATE( &CUIPanel::GetPanelType ) );
}


//-----------------------------------------------------------------------------
// Purpose: Set panel enabled/disabled for input
//-----------------------------------------------------------------------------
void CUIPanel::SetEnabled( bool bEnabled )
{
	if( bEnabled )
	{
		RemoveStyleFlag( k_EStyleFlagDisabled );
	}
	else
	{
		AddStyleFlag( k_EStyleFlagDisabled );
		if( BHasKeyFocus() && GetParent() )
		{
			// Update focus, but do not switch contexts since this is not in response to direct input!
			IUIPanel *pLastContext = GetParentWindow()->UIWindowInput()->GetInputFocusContext();
			GetParent()->SetFocusToNextPanel( 0, k_EPrevInTabOrder, true, GetTabIndex(), GetSelectionPositionX(), GetSelectionPositionY(), GetSelectionPositionX(), GetSelectionPositionY() );
			GetParentWindow()->UIWindowInput()->SetInputFocusContext( pLastContext );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set all our immediate children enabled/disabled
//-----------------------------------------------------------------------------
void CUIPanel::SetActivationEnabled( bool bEnabled )
{
	if( bEnabled )
	{
		RemoveStyleFlag( k_EStyleFlagActivationDisabled );
	}
	else
	{
		AddStyleFlag( k_EStyleFlagActivationDisabled );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set all our immediate children enabled/disabled
//-----------------------------------------------------------------------------
void CUIPanel::SetAllChildrenActivationEnabled( bool bEnabled )
{
	FOR_EACH_VEC( m_vecChildren, i )
	{
		m_vecChildren[i]->SetActivationEnabled( bEnabled );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Recursively enable/disable hit testing
//-----------------------------------------------------------------------------
void CUIPanel::SetHitTestEnabledTraverse( bool bEnabled )
{
	SetHitTestEnabled( bEnabled );

	FOR_EACH_VEC( m_vecChildren, i )
	{
		m_vecChildren[ i ]->SetHitTestEnabledTraverse( bEnabled );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets if a panel is draggable
//-----------------------------------------------------------------------------
void CUIPanel::SetDraggable( bool bEnabled )
{
	if ( bEnabled )
		SetAcceptsInput( true );

	m_bDraggable = bEnabled;
}


//-----------------------------------------------------------------------------
// Purpose: Sets if a panel remembers its last focused child
//-----------------------------------------------------------------------------
void CUIPanel::SetRememberChildFocus( bool bRememberChildFocus )
{
	if ( bRememberChildFocus && !m_bRememberChildFocus )
	{
		RegisterEventHandler( InputFocusSet(), this, &CUIPanel::EventInputFocusSet );
	}
	else if ( !bRememberChildFocus && m_bRememberChildFocus )
	{
		UnregisterEventHandler( InputFocusSet(), this, &CUIPanel::EventInputFocusSet );
	}
	m_bRememberChildFocus = bRememberChildFocus;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUIPanel::ClearLastChildFocus()
{
	m_pLastFocusedChild = nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for when a child takes focus
//-----------------------------------------------------------------------------
bool CUIPanel::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	if ( m_bRememberChildFocus )
	{
		m_pLastFocusedChild = ptrPanel;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Gets tab index for panel
//-----------------------------------------------------------------------------
float CUIPanel::GetTabIndex() const
{
	if( m_flTabIndex == k_flTabIndexAuto )
	{
		if( BSelfOrChildrenAcceptFocus() )
			return m_pParent ? m_pParent->GetChildIndex( this ) : 0;
		else
			return k_flTabIndexInvalid;
	}

	return m_flTabIndex;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates selection position x & y for a panel in wrap flow
//-----------------------------------------------------------------------------
void GetWrapFlowXY( const IUIPanel *pPanel, EFlowDirection eDirection, int *px, int *py )
{
	Assert( eDirection == k_EFlowRightWrap || eDirection == k_EFlowLeftWrap || eDirection == k_EFlowDownWrap || eDirection == k_EFlowUpWrap );
	IUIPanel *pParent = pPanel->GetParent();
	if ( !pParent )
	{
		*px = 0;
		*py = 0;
		return;
	}

	int iColumn = 0;
	int iRow = 0;
	float flPrev = 0.0f;
	bool bFirst = true;
	for ( int i = 0; i < pParent->GetChildCount(); i++ )
	{
		IUIPanel *pChild = pParent->GetChild( i );
		
		if ( !pChild->BIsVisible() )
		{
			Assert( pChild != pPanel );
			continue;
		}

		CUILength x, y, z;
		pChild->AccessIUIStyleDirty()->GetPosition( x, y, z );

		// in a flow so every uilength should already be a length				
		float flCurrent = (eDirection == k_EFlowRightWrap || eDirection == k_EFlowLeftWrap) ? x.GetValue() : y.GetValue();

		// first child sets our flPrev
		if ( bFirst )
		{
			flPrev = flCurrent;
			bFirst = false;
		}
		else
		{
			if ( eDirection == k_EFlowRightWrap || eDirection == k_EFlowLeftWrap )
			{
				iColumn++;

				// wrapped?
				if ( flCurrent <= flPrev )
				{
					iColumn = 0;
					iRow++;
				}
			}
			else
			{
				iRow++;

				// wrapped?
				if ( flCurrent <= flPrev )
				{
					iRow = 0;
					iColumn++;
				}
			}

			flPrev = flCurrent;
		}

		// if we're flowing left, negate the column
		if ( eDirection == k_EFlowLeftWrap )
			iColumn *= -1;

		// if we're flowing up, negate the row
		if ( eDirection == k_EFlowUpWrap )
			iRow *= -1;

		// column and row are updated for current panel, compare now
		if ( pChild == pPanel )
		{
			*px = iColumn;
			*py = iRow;
			return;
		}
	}

	AssertMsg( false, "GetWrapFlowXY couldn't find child" );
	*px = 0;
	*py = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns flow x/y position in flow direction for a panel (must be non-wrapped flow)
//-----------------------------------------------------------------------------
float GetFlowDirectionPos( const IUIPanel *pPanel, EFlowDirection eDirection )
{
	Assert( eDirection == k_EFlowRight || eDirection == k_EFlowLeft || eDirection == k_EFlowDown || eDirection == k_EFlowUp );

	int iPosition = 0;
	IUIPanel *pParent = pPanel->GetParent();
	for ( int i = 0; i < pParent->GetChildCount(); i++ )
	{
		IUIPanel *pChild = pParent->GetChild( i );
		if ( !pChild->BIsVisible() )
		{
			Assert( pChild != pPanel );
			continue;
		}

		if ( pChild == pPanel )
		{
			// If we're operating in the opposite direction from normal, negate
			// our position value.
			if ( eDirection == k_EFlowLeft || eDirection == k_EFlowUp )
				iPosition *= -1;

			return iPosition;
		}

		iPosition++;
	}

	AssertMsg( false, "GetFlowDirectionPos couldn't find child" );
	return 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Gets tab index for panel
//-----------------------------------------------------------------------------
float CUIPanel::GetSelectionPositionX() const
{
	if ( m_flSelectionPosX == k_flSelectionPosInvalid )
		return k_flSelectionPosInvalid;

	float flParent = 0.0f;
	IUIPanel *pParent = GetParent();
	while ( pParent )
	{
		float flThisParent = pParent->GetSelectionPositionX();
		if ( flThisParent != k_flSelectionPosInvalid )
		{
			flParent += flThisParent;
			break;
		}

		pParent = pParent->GetParent();
	}

	if ( m_flSelectionPosX == k_flSelectionPosAuto )
	{
		if ( BSelfOrChildrenAcceptFocus() )
		{
			EFlowDirection parentFlowDir = k_EFlowNone;
			if ( GetParent() )
			{
				m_pParent->AccessIUIStyle()->GetFlowChildren( parentFlowDir );

				// if flowing and not visible, return invalid
				if ( parentFlowDir != k_EFlowNone && parentFlowDir != k_EFlowUnset && !BIsVisible() )
					return k_flSelectionPosInvalid;

				// In a flowing layout just index our x position by child offset when flowing right, or 
				// if flowing down consider all x positions equivalent
				if ( parentFlowDir == k_EFlowRight || parentFlowDir == k_EFlowLeft )
					return flParent + GetFlowDirectionPos( this, parentFlowDir );
				else if ( parentFlowDir == k_EFlowDown || parentFlowDir == k_EFlowUp )
					return flParent;

				if ( parentFlowDir == k_EFlowRightWrap || parentFlowDir == k_EFlowDownWrap || parentFlowDir == k_EFlowLeftWrap || parentFlowDir == k_EFlowUpWrap )
				{
					int x, y = 0;
					GetWrapFlowXY( this, parentFlowDir, &x, &y );
					return flParent + x;
				}
			}

			// m_flActualXOffset won't be valid until we have gone through at least 1 layout pass
			if ( !BHasBeenLayedOut() )
				return k_flSelectionPosInvalid;

			return flParent + GetActualXOffset();
		}
		else
			return k_flSelectionPosInvalid;
	}

	return flParent + m_flSelectionPosX;
}


//-----------------------------------------------------------------------------
// Purpose: Gets tab index for panel
//-----------------------------------------------------------------------------
float CUIPanel::GetSelectionPositionY() const
{
	if ( m_flSelectionPosY == k_flSelectionPosInvalid )
		return k_flSelectionPosInvalid;

	float flParent = 0.0f;
	IUIPanel *pParent = GetParent();
	while ( pParent )
	{
		float flThisParent = pParent->GetSelectionPositionY();
		if ( flThisParent != k_flSelectionPosInvalid )
		{
			flParent += flThisParent;
			break;
		}

		pParent = pParent->GetParent();
	}

	if ( m_flSelectionPosY == k_flSelectionPosAuto )
	{
		if ( BSelfOrChildrenAcceptFocus() )
		{
			EFlowDirection parentFlowDir = k_EFlowNone;
			if ( GetParent() )
			{
				m_pParent->AccessIUIStyle()->GetFlowChildren( parentFlowDir );

				// if flowing and not visible, return invalid
				if ( parentFlowDir != k_EFlowNone && parentFlowDir != k_EFlowUnset && !BIsVisible() )
					return k_flSelectionPosInvalid;

				// In a flowing layout just index our y position by child offset when flowing down, or 
				// if flowing right consider all y positions equivalent
				if ( parentFlowDir == k_EFlowDown || parentFlowDir == k_EFlowUp )
					return flParent + GetFlowDirectionPos( this, parentFlowDir );
				else if ( parentFlowDir == k_EFlowRight || parentFlowDir == k_EFlowLeft )
					return flParent;
				
				if ( parentFlowDir == k_EFlowRightWrap || parentFlowDir == k_EFlowDownWrap || parentFlowDir == k_EFlowLeftWrap || parentFlowDir == k_EFlowUpWrap )
				{
					int x, y = 0;
					GetWrapFlowXY( this, parentFlowDir, &x, &y );
					return flParent + y;
				}
			}

			// m_flActualYOffset won't be valid until we have gone through at least 1 layout pass
			if ( !BHasBeenLayedOut() )
				return k_flSelectionPosInvalid;

			return flParent + GetActualYOffset();
		}
		else
			return k_flSelectionPosInvalid;
	}

	return flParent + m_flSelectionPosY;
}


//-----------------------------------------------------------------------------
// Purpose: Check if we, or any of our children, accept focus recursively
//-----------------------------------------------------------------------------
bool CUIPanel::BSelfOrChildrenAcceptFocus() const
{
	if( BAcceptsFocus() )
	{
		return true;
	}

	FOR_EACH_VEC( m_vecChildren, i )
	{
		if( ((CUIPanel*)m_vecChildren[i])->BSelfOrChildrenAcceptFocus() )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Gets default mouse cursor for this panel
//-----------------------------------------------------------------------------
EMouseCursors CUIPanel::GetPanelMouseCursor()
{
	return m_ePanelMouseCursor;
}


//-----------------------------------------------------------------------------
// Purpose: Sets default mouse cursor for this panel
//-----------------------------------------------------------------------------
void CUIPanel::SetPanelMouseCursor( EMouseCursors eCursor )
{
	m_ePanelMouseCursor = eCursor;
}


//-----------------------------------------------------------------------------
// Purpose: Sets mouse can activate flag for this panel
//-----------------------------------------------------------------------------
void CUIPanel::SetMouseCanActivate( EMouseCanActivate eMouseCanActivate, const char *pchOptionalParent )
{
	if( eMouseCanActivate == k_EMouseCanActivateIfParentFocused && (!pchOptionalParent || pchOptionalParent[0] == '\0') )
	{
		AssertMsg( false, "No parent specified for mouse can activate" );
		return;
	}

	// if required parent focus previously, remove old registration
	if( m_eMouseCanActivate == k_EMouseCanActivateIfParentFocused )
		UIEngine()->UnregisterMouseCanActivateParent( this );

	if( eMouseCanActivate == k_EMouseCanActivateIfParentFocused )
		UIEngine()->RegisterMouseCanActivateParent( this, pchOptionalParent );

	m_eMouseCanActivate = eMouseCanActivate;
}


//-----------------------------------------------------------------------------
// Purpose: If MouseCanActivate is set to "if parent focused", finds required parent panel
//			Can return NULL
//-----------------------------------------------------------------------------
IUIPanel *CUIPanel::FindParentForMouseCanActivate()
{
	if( m_eMouseCanActivate != k_EMouseCanActivateIfParentFocused )
		return NULL;

	const char *pchParent = UIEngine()->GetMouseCanActivateParent( this );
	if( !pchParent )
		return NULL;

	for( IUIPanel *pParent = GetParent(); pParent != NULL; pParent = pParent->GetParent() )
	{
		if( V_stricmp( pParent->GetID(), pchParent ) == 0 )
			return pParent;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Clears cached maps of parents by id/type/classes
//-----------------------------------------------------------------------------
void CUIPanel::ClearParentLookupMapsTraverse()
{
	SAFE_DELETE( m_pMapParentsByID );
	SAFE_DELETE( m_pMapParentsByType );
	SAFE_DELETE( m_pMapParentsByClass );

	FOR_EACH_VEC( m_vecChildren, i )
	{
		((CUIPanel*)m_vecChildren[i])->ClearParentLookupMapsTraverse();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
int CUIPanel::GetAttribute( const char *pchAttrName, int nDefaultValue ) const
{
	if( !m_pMapProperties )
		return nDefaultValue;

	int iMap = m_pMapProperties->Find( pchAttrName );
	if( iMap != m_pMapProperties->InvalidIndex() )
		return V_atoi( m_pMapProperties->Element( iMap ).String() );

	return nDefaultValue;
}


//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
uint32 CUIPanel::GetAttribute( const char *pchAttrName, uint32 unDefaultValue ) const
{
	return GetAttribute( pchAttrName, ( uint64 )unDefaultValue );
}


//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
uint64 CUIPanel::GetAttribute( const char *pchAttrName, uint64 unDefaultValue ) const
{
	if ( !m_pMapProperties )
		return unDefaultValue;

	int iMap = m_pMapProperties->Find( pchAttrName );
	if ( iMap != m_pMapProperties->InvalidIndex() )
#if defined( SOURCE2_PANORAMA )
		return V_atoui64( m_pMapProperties->Element( iMap ).String() );
#else
		return V_strtoui64( m_pMapProperties->Element( iMap ).String(), NULL, 10 );
#endif

	return unDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
const char *CUIPanel::GetAttribute( const char *pchAttrName, const char * pchDefaultValue ) const
{
	if( !m_pMapProperties )
		return pchDefaultValue;

	int iMap = m_pMapProperties->Find( pchAttrName );
	if( iMap != m_pMapProperties->InvalidIndex() )
		return m_pMapProperties->Element( iMap ).String();

	return pchDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
float CUIPanel::GetAttribute( const char *pchAttrName, float flDefaultValue ) const
{
	if ( !m_pMapProperties )
		return flDefaultValue;

	int iMap = m_pMapProperties->Find( pchAttrName );
	if ( iMap != m_pMapProperties->InvalidIndex() )
		return V_atof( m_pMapProperties->Element( iMap ).String() );

	return flDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
int CUIPanel::GetAttribute( CPanoramaSymbol symbol, int nDefaultValue ) const
{
	if ( !m_pMapProperties )
		return nDefaultValue;

	int iMap = m_pMapProperties->Find( symbol );
	if ( iMap != m_pMapProperties->InvalidIndex() )
		return V_atoi( m_pMapProperties->Element( iMap ).String() );

	return nDefaultValue;
}


//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
uint32 CUIPanel::GetAttribute( CPanoramaSymbol symbol, uint32 unDefaultValue ) const
{
	return GetAttribute( symbol, (uint64)unDefaultValue );
}


//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
uint64 CUIPanel::GetAttribute( CPanoramaSymbol symbol, uint64 unDefaultValue ) const
{
	if ( !m_pMapProperties )
		return unDefaultValue;

	int iMap = m_pMapProperties->Find( symbol );
	if ( iMap != m_pMapProperties->InvalidIndex() )
#if defined( SOURCE2_PANORAMA )
		return V_atoui64( m_pMapProperties->Element( iMap ).String() );
#else
		return V_strtoui64( m_pMapProperties->Element( iMap ).String(), NULL, 10 );
#endif

	return unDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
const char *CUIPanel::GetAttribute( CPanoramaSymbol symbol, const char * pchDefaultValue ) const
{
	if ( !m_pMapProperties )
		return pchDefaultValue;

	int iMap = m_pMapProperties->Find( symbol );
	if ( iMap != m_pMapProperties->InvalidIndex() )
		return m_pMapProperties->Element( iMap ).String();

	return pchDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Getter for panel attributes
//-----------------------------------------------------------------------------
float CUIPanel::GetAttribute( CPanoramaSymbol symbol, float flDefaultValue ) const
{
	if ( !m_pMapProperties )
		return flDefaultValue;

	int iMap = m_pMapProperties->Find( symbol );
	if ( iMap != m_pMapProperties->InvalidIndex() )
		return V_atof( m_pMapProperties->Element( iMap ).String() );

	return flDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( const char *pchAttrName, int nValue )
{
	SetAttribute( pchAttrName, CNumStr( nValue ).String() );
}


//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( const char *pchAttrName, const char * pchValue )
{
	if( !m_pMapProperties )
	{
		m_pMapProperties = new CUtlMap< CPanoramaSymbol, CUtlString, int, CDefLess< CPanoramaSymbol > >();
	}

	m_pMapProperties->InsertOrReplace( pchAttrName, pchValue );
}

//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( CPanoramaSymbol symbol, int nValue )
{
	SetAttribute( symbol, CNumStr( nValue ).String() );
}


//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( CPanoramaSymbol symbol, const char * pchValue )
{
	if ( !m_pMapProperties )
	{
		m_pMapProperties = new CUtlMap< CPanoramaSymbol, CUtlString, int, CDefLess< CPanoramaSymbol > >();
	}

	m_pMapProperties->InsertOrReplace( symbol, pchValue );
}



//-----------------------------------------------------------------------------
// Purpose: Remove a panel attribute
//-----------------------------------------------------------------------------
void CUIPanel::RemoveAttribute( const char *pchAttrName )
{
	if ( !m_pMapProperties )
		return;

	m_pMapProperties->Remove( pchAttrName );
}

//-----------------------------------------------------------------------------
// Purpose: Remove a panel attribute
//-----------------------------------------------------------------------------
void CUIPanel::RemoveAttribute( CPanoramaSymbol symbol )
{
	if ( !m_pMapProperties )
		return;

	m_pMapProperties->Remove( symbol );
}

//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( const char *pchAttrName, uint32 unValue )
{
	SetAttribute( pchAttrName, CNumStr( unValue ).String() );
}


//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( const char *pchAttrName, uint64 unValue )
{
	SetAttribute( pchAttrName, CNumStr( unValue ).String() );
}


//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( const char *pchAttrName, float flValue )
{
	SetAttribute( pchAttrName, CNumStr( flValue ).String() );
}

//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( CPanoramaSymbol symbol, uint32 unValue )
{
	SetAttribute( symbol, CNumStr( unValue ).String() );
}


//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( CPanoramaSymbol symbol, uint64 unValue )
{
	SetAttribute( symbol, CNumStr( unValue ).String() );
}


//-----------------------------------------------------------------------------
// Purpose: Setter for panel attributes
//-----------------------------------------------------------------------------
void CUIPanel::SetAttribute( CPanoramaSymbol symbol, float flValue )
{
	SetAttribute( symbol, CNumStr( flValue ).String() );
}



//-----------------------------------------------------------------------------
// Purpose: Set the animation style for the panel
//-----------------------------------------------------------------------------
void CUIPanel::SetAnimation( const char *pchAnimationName, float flDuration, float flDelay, EAnimationTimingFunction eTimingFunc, EAnimationDirection eDirection, EAnimationFillMode eFillMode, float flIterations )
{
	CStylePropertyAnimationProperties *pAnimationProps = (CStylePropertyAnimationProperties*)CStylePropertyFactory::CreateStyleProperty( CStylePropertyAnimationProperties::symbol );

	CCubicBezierCurve< Vector2D > cubicBezier;
	Vector2D vec[4];

	panorama::GetAnimationCurveControlPoints( eTimingFunc, vec );
	cubicBezier.SetControlPoints( vec );

	pAnimationProps->SetAnimation( pchAnimationName, flDuration, flDelay, eTimingFunc, cubicBezier, eDirection, eFillMode, flIterations );

	AccessStyleDirty()->SetAnimationProperties( pAnimationProps );

	CStylePropertyFactory::FreeStyleProperty( pAnimationProps );
}


//-----------------------------------------------------------------------------
// Purpose:Builds a string of properties and values to display in the debugger
//-----------------------------------------------------------------------------
void CUIPanel::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	// add ID if we have one
	CUtlString strID;
	if( !m_strID.IsEmpty() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "id", GetID() );
		pvecProperties->AddToTail( pProperty );
	}

	// build list of classes
	CUtlString strClasses;
	if( m_vecStyleClasses.Count() > 0 )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t();
		pProperty->m_strName = "class";

		FOR_EACH_VEC( m_vecStyleClasses, i )
		{
			if( i == 0 )
				pProperty->m_strValue = m_vecStyleClasses[i].String();
			else
				pProperty->m_strValue.Append( CFmtStr1024( " %s", m_vecStyleClasses[i].String() ).String() );
		}

		pvecProperties->AddToTail( pProperty );
	}

	if ( m_style.BHasElementStyles() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "style", "..." );
		pvecProperties->AddToTail( pProperty );
	}

	if ( m_bClipAfterTransform )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "clipaftertransform", "true" );
		pvecProperties->AddToTail( pProperty );
	}

	// add tab index
	if( m_flTabIndex != k_flTabIndexInvalid )
	{
		DebugPropertyOutput_t *pProperty = NULL;

		if( m_flTabIndex == k_flTabIndexAuto )
		{
			pProperty = new DebugPropertyOutput_t( "tabindex", "auto" );
		}
		else
		{
			CFmtStr1024 fmt;
			CSSHelpers::AppendFloat( &fmt, GetTabIndex() );
			pProperty = new DebugPropertyOutput_t( "tabindex", fmt );
		}

		pvecProperties->AddToTail( pProperty );
	}

	// add selection pos
	if( m_flSelectionPosX != k_flSelectionPosInvalid && m_flSelectionPosY != k_flSelectionPosInvalid )
	{
		DebugPropertyOutput_t *pProperty = NULL;

		if( m_flSelectionPosX == k_flSelectionPosAuto && m_flSelectionPosY == k_flSelectionPosAuto )
		{
			pProperty = new DebugPropertyOutput_t( "selectionpos", "auto" );
		}
		else
		{
			CFmtStr1024 fmt;
			CSSHelpers::AppendFloat( &fmt, GetSelectionPositionX() );
			fmt.Append( "," );
			CSSHelpers::AppendFloat( &fmt, GetSelectionPositionY() );

			pProperty = new DebugPropertyOutput_t( "selectionpos", fmt );
		}

		pvecProperties->AddToTail( pProperty );
	}

	// add default focus
	if( !m_strDefaultFocus.IsEmpty() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "defaultfocus", m_strDefaultFocus.String() );
		pvecProperties->AddToTail( pProperty );
	}

	if (m_bFocusOnHover)
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t("focusonhover", "true");
		pvecProperties->AddToTail(pProperty);
	}

	if( m_bChildFocusOnHover )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "childfocusonhover", "true" );
		pvecProperties->AddToTail( pProperty );
	}

	if ( m_bRememberChildFocus )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "rememberchildfocus", "true" );
		pvecProperties->AddToTail( pProperty );
	}

	if( m_bSelectionPosHorBoundary && m_bSelectionPosVerBoundary )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "selectionposboundary", "both" );
		pvecProperties->AddToTail( pProperty );
	}
	else if( m_bSelectionPosHorBoundary )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "selectionposboundary", "horizontal" );
		pvecProperties->AddToTail( pProperty );
	}
	else if( m_bSelectionPosVerBoundary )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "selectionposboundary", "vertical" );
		pvecProperties->AddToTail( pProperty );
	}

	if( !BHitTestEnabled() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "hittest", "false" );
		pvecProperties->AddToTail( pProperty );
	}


	if( !BHitTestChildrenEnabled() )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "hittestchildren", "false" );
		pvecProperties->AddToTail( pProperty );
	}

	if ( m_bRequireCompositionLayer )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "require-composition-layer", "true" );
		pvecProperties->AddToTail( pProperty );
	}
	if ( m_bForceNoCompositionLayer )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "force-no-composition-layer", "true" );
		pvecProperties->AddToTail( pProperty );
	}
	if ( m_bAlwaysCacheCompositionLayer )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "always-cache-composition-layer", "true" );
		pvecProperties->AddToTail( pProperty );
	}
	if ( m_bOffscreenCompositionLayer )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "offscreen-composition-layer", "true" );
		pvecProperties->AddToTail( pProperty );
	}
	
	if ( m_eFractionalPixelPositions != k_EFractionalPixelPositionsDefault )
	{
		DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t( "clampfractionalpixelpositions", ( ( m_eFractionalPixelPositions == k_EFractionalPixelPositionsClamp ) ? "true" : "false" ) );
		pvecProperties->AddToTail( pProperty );
	}
}


//-----------------------------------------------------------------------------
// Purpose: event handler to load layout file async, overriding existing if needed
//-----------------------------------------------------------------------------
bool CUIPanel::EventLoadLayoutFileAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile, bool bPartialLayout )
{
	if( m_pWindow->BIsWindowInLayoutPass() )
		DispatchEventAsync( 0.0f, LoadLayoutFileAsync(), this, pchLayoutFile, bPartialLayout );
	else
		LoadLayoutAsync( pchLayoutFile, true, bPartialLayout );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: event handler to load children as created via a layout file
//-----------------------------------------------------------------------------
bool CUIPanel::EventLoadLayoutFromXMLStringAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile, bool bPartialLayout )
{
	LoadLayoutFromStringAsync( pchLayoutFile, true, bPartialLayout );
	return true;
}
//-----------------------------------------------------------------------------
// Purpose: event handler to load children as created via a layout file
//-----------------------------------------------------------------------------
bool CUIPanel::EventLoadLayoutFromBase64XMLStringAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile, bool bPartialLayout )
{
	uint32 unEncodedLen = V_strlen( pchLayoutFile );
	if( unEncodedLen > 0 )
	{

#if defined( SOURCE2_PANORAMA )
		uint32 unMaxLen = base64_decode_maxsize( unEncodedLen );
#else
		uint32 unMaxLen = CCrypto::Base64DecodeMaxOutput( unEncodedLen );
#endif
		if( unMaxLen > 0 )
		{
			CUtlBuffer buf;
			buf.EnsureCapacity( unMaxLen );

			uint32 unUsedLen = unMaxLen;
#if defined( SOURCE2_PANORAMA )
			unUsedLen = base64_decode(  (uint8*)buf.Base(), unMaxLen, pchLayoutFile, unEncodedLen );
			if ( unUsedLen )
#else
			if( CCrypto::Base64Decode( pchLayoutFile, unEncodedLen, (uint8*)buf.Base(), &unUsedLen ) )
#endif
			{
				buf.SeekPut( CUtlBuffer::SEEK_HEAD, unUsedLen );
#if !defined( SOURCE2_PANORAMA_FIXME )
				if( buf.TellPut() > 2 )
				{
					byte *pchBytes = (byte*)buf.Base();
					if( pchBytes[0] == 0x1f && pchBytes[1] == 0x8b )
					{
						CUtlBuffer bufToSwapIn;
						GUnzipToBuffer( (uint8*)buf.Base(), buf.TellPut(), bufToSwapIn );
						buf.Swap( bufToSwapIn );
					}
				}
#endif

				buf.PutChar( 0 );
				LoadLayoutFromStringAsync( (const char*)buf.Base(), true, bPartialLayout );
			}
		}
	}

	// True means we handled the event and it shouldn't bubble, doesn't imply successful layout load, but still must return it
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Enables ready for display events on this panel
//-----------------------------------------------------------------------------
void CUIPanel::RegisterForReadyEvents( bool bEnable )
{
	if ( m_bRegisteredForReadyEvents == bEnable )
		return;

	m_bRegisteredForReadyEvents = bEnable;
	CUIPanel *pParent = (CUIPanel*)GetParent();
	if ( pParent )
		pParent->IncrementAncestorReadyForDisplay( bEnable ? 1 : -1 );

	// If we're now interested in ready events, and we should be ready, but aren't currently,
	// then we need to fix that up. Note that we have to fire the event async, because
	// otherwise we may try to re-entrantly apply styles
	if ( m_bRegisteredForReadyEvents && m_bReadyForDisplaySetOnPanel && m_bReadyForDisplaySetOnPanel != m_bReadyForDisplayState )
	{
		m_bReadyForDisplayState = true;
		DispatchEventAsync( 0.0f, ReadyForDisplay(), this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Child ReadyForDisplay changed; update count
//-----------------------------------------------------------------------------
void CUIPanel::IncrementAncestorReadyForDisplay( int nDelta, bool *pAncestorReadyState )
{
	// Hidden children can't currently participate in ready for display
	if ( m_bHiddenChild )
		return;

	// Nothing to do
	if ( nDelta == 0 )
		return;

	// For perf reasons, panels that don't care about ready state (themselves and children) don't get traversed when setting m_bReadyForDisplayState.
	// Might need to query current ancestor state when incrementing ancestor counts
	bool bNeedAncestorState = (m_cReadyForDisplayChildren == 0 && nDelta > 0);

	m_cReadyForDisplayChildren += nDelta;
	DbgAssert( m_cReadyForDisplayChildren >= 0 );

	CUIPanel *pParent = (CUIPanel*)GetParent();
	if ( pParent )
	{
		bool bAncestorReadyState = true;
		pParent->IncrementAncestorReadyForDisplay( nDelta, bNeedAncestorState ? &bAncestorReadyState : NULL );
		if ( bNeedAncestorState )
			m_bReadyForDisplayState = bAncestorReadyState;
	}
	else
	{
		if ( bNeedAncestorState )
			m_bReadyForDisplayState = m_bReadyForDisplaySetOnPanel;
	}

	// if child needed ready state, pass down
	if ( pAncestorReadyState )
		*pAncestorReadyState = m_bReadyForDisplayState;
}


//-----------------------------------------------------------------------------
// Purpose: Sets ready for display state on this panel and all children that care about the state
//-----------------------------------------------------------------------------
void CUIPanel::SetReadyForDisplay( bool bReady )
{
	m_bReadyForDisplaySetOnPanel = bReady;
	DispatchReadyForDisplayTraverse( m_bReadyForDisplaySetOnPanel );	
}


//-----------------------------------------------------------------------------
// Purpose: Dispatches ready for display event to this panel and child panels
//-----------------------------------------------------------------------------
void CUIPanel::DispatchReadyForDisplayTraverse( bool bReady )
{
	// could come from SetReadyForDisplay() for ourselves or from parent	

	// if called from parent, they could now be ready but this panel could have been marked as not ready. Check.
	if ( !m_bReadyForDisplaySetOnPanel && bReady )
		return;

	// stop traversing if state isn't changing so we don't dispatch duplicate messages
	if ( m_bReadyForDisplayState == bReady )
		return;

	m_bReadyForDisplayState = bReady;

	// tell this panel if it cares
	if ( m_bRegisteredForReadyEvents )
	{
		if ( bReady )
			DispatchEvent( ReadyForDisplay(), this );
		else
			DispatchEvent( UnreadyForDisplay(), this );
	}

	if ( m_pCachedCommandList.IsValid() && !bReady )
	{
		// At this point we know the panel previously cached paint commands
		// and its descendant might have images (image panel or background 
		// image) (otherwise m_cReadyForDisplayChildren would be 0). 
		// Free the paint command in order to release the corresponding IUITexture
		// when the panel is not ready.
		m_pCachedCommandList.Reset();
	}

	// done?
	if ( m_cReadyForDisplayChildren <= 0 )
		return;

	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = (CUIPanel*)m_vecChildren[i];
		pChild->DispatchReadyForDisplayTraverse( bReady );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Loads images for display
//-----------------------------------------------------------------------------
bool CUIPanel::EventReadyForDisplay( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( !(m_unStylesPresentFlags & k_EStylePresentBackgroundImage) )
		return true;

	CUtlVector< CBackgroundImageLayer * > *pvecLayers = AccessStyle()->GetBackgroundImages();
	if ( !pvecLayers )
		return true;

	FOR_EACH_VEC( *pvecLayers, i )
	{
		pvecLayers->Element( i )->ReloadImage( pPanel.Get() );
	}

	// don't bubble. Not intended for parents to get spammed with ready/unready for all children.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Loads images for display
//-----------------------------------------------------------------------------
bool CUIPanel::EventUnreadyForDisplay( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( !(m_unStylesPresentFlags & k_EStylePresentBackgroundImage) )
		return true;

	CUtlVector< CBackgroundImageLayer * > *pvecLayers = AccessStyle()->GetBackgroundImages();
	if ( !pvecLayers )
		return true;

	FOR_EACH_VEC( *pvecLayers, i )
	{
		pvecLayers->Element( i )->UnloadImage();
	}

	// don't bubble. Not intended for parents to get spammed with ready/unready for all children.
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Recalculates the actual UI scale values for this panel and all descendants
//-----------------------------------------------------------------------------
void CUIPanel::RecalculateUIScale( bool bFinal, const Vector *pvOldParentUIScale )
{
	Vector vStyleUIScale = AccessStyleDirty()->GetInterpolatedUIScale( bFinal );

	Vector vParentUIScale = GetParentActualUIScale();
	Vector vOldParentUIScale = pvOldParentUIScale ? *pvOldParentUIScale : vParentUIScale;

	Vector vOldUIScale = m_vActualUIScale;
	Vector vNewUIScale = vParentUIScale * vStyleUIScale;

	if ( m_vActualUIScale == vNewUIScale && vOldParentUIScale == vParentUIScale )
		return;

	m_vActualUIScale = vNewUIScale;

	AccessStyleDirty()->UpdateUIScaleFactor( vOldUIScale, vNewUIScale, vOldParentUIScale, vParentUIScale );
	InvalidateSizeAndPosition();

	// todo(ericl): I don't think this shouldn't actually be necessary. But for some reason, without it child panels don't
	// resize correctly during ui-scale animations.
	MarkStylesDirty( false );

	ClientPtr()->OnUIScaleFactorChanged( vOldUIScale, vNewUIScale );

	FOR_EACH_VEC( m_vecChildren, i )
	{
		CUIPanel *pChild = ( CUIPanel* )m_vecChildren[ i ];
		pChild->RecalculateUIScale( bFinal, &vOldUIScale );
	}
	if ( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( *m_pVecChildrenInHiding, i )
		{
			CUIPanel *pChild = ( CUIPanel* )( *m_pVecChildrenInHiding )[ i ];
			pChild->RecalculateUIScale( bFinal, &vOldUIScale );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the calculated UI scale of this panel's parent
//-----------------------------------------------------------------------------
Vector CUIPanel::GetParentActualUIScale() const
{
	IUIPanel *pParent = GetParent();
	if ( pParent )
		return pParent->GetActualUIScale();

	IUIWindow *pParentWindow = GetParentWindow();
	if ( pParentWindow )
	{
		Vector vWindowScale;
		vWindowScale.x = pParentWindow->GetWindowScaleFactor();
		vWindowScale.y = vWindowScale.x;
		vWindowScale.z = vWindowScale.x;
		return vWindowScale;
	}

	return CStylePropertyUIScale::GetDefault();
}


//-----------------------------------------------------------------------------
// Purpose: Set all the 3d transform attributes immediately
//-----------------------------------------------------------------------------
void CUIPanel::SetTransform3DSimple( const CUtlVector<CTransform3D *> &vecTransforms )
{
	if ( AccessStyleDirty()->SetTransform3DSimple( vecTransforms ) )
	{
		SetRepaint( k_EPanelRepaintComposition );

		m_unStylesPresentFlags |= k_EStylePresentTransformMatrix;

		// Let the ui client code also do work now
		ClientPtr()->OnStylesChanged();

		// Must be async, because handling this will call into Build() inside debug styles, and that calls RemoveAndDeleteChildren(), 
		// which will modify the vector of children in the top level window which is likely being iterated inside
		// the LayoutAndPaintWindows() pass right now.  We may want to make that safer somehow flagging panels as invalid
		// in the vector, skipping invalid in code that traverses, and cleaning up once per frame or something.
		DispatchEventAsync( 0.0f, PanelStyleChanged(), this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set all the opacity attribute immediately
//-----------------------------------------------------------------------------
void CUIPanel::SetOpacitySimple( float opacity )
{
	if ( AccessStyleDirty()->SetOpacitySimple( opacity ) )
	{
		SetRepaint( k_EPanelRepaintComposition );

		// Hack: we early out OnContentSizeTraverse on panels with opacity = 0, so if opacity is changing
		// we need to re-run they layout traverse on that panel.
		if ( BInvalidateSizeAndPositionOnOpacityChangeDisabled() )
		{
			InvalidateSizeAndPosition();
		}
		
		// We early out in the "apply style traverse" if a panel is not visible or is transparent, so if the visibility or opacity 
		// is changing we need to re-run the "apply style traverse" if styles are dirty for any of the panel's children.
		// (Ideally we only need to re-run the "apply style traverse" if the panel become visible or the opacity is changing
		// from 0 to any other value)
		if ( BChildStylesDirty() )
		{
			MarkChildStylesDirtyOnParents();
		}

		m_unStylesPresentFlags |= k_EStylePresentOpacity;

		// Let the ui client code also do work now
		ClientPtr()->OnStylesChanged();

		// Must be async, because handling this will call into Build() inside debug styles, and that calls RemoveAndDeleteChildren(), 
		// which will modify the vector of children in the top level window which is likely being iterated inside
		// the LayoutAndPaintWindows() pass right now.  We may want to make that safer somehow flagging panels as invalid
		// in the vector, skipping invalid in code that traverses, and cleaning up once per frame or something.
		DispatchEventAsync( 0.0f, PanelStyleChanged(), this );
	}
}


const char*CUIPanel::GetCompositionLayerRenderTargetName()
{
	CPanelPtr<CPanel2D> safeptr( this );
	return AccessRenderEngine()->Access3DSurface()->GetCompositionLayerRenderTargetName( safeptr.GetHandleAsUInt64() );
}


#ifdef DBGFLAG_VALIDATE
void CUIPanel::Validate( CValidator &validator, const tchar *pchName )
{
	validator.ClaimMemory( ClientPtr() );
	ClientPtr()->ValidateClientPanel( validator, pchName );

	ValidateObj( m_strID );
	ValidateObj( (*m_pStyle) );
	ValidateObj( m_vecChildren );
	ValidateObj( m_strDefaultFocus );
	FOR_EACH_VEC( m_vecChildren, i )
	{
		ValidatePtr( m_vecChildren[i] );
	}
	ValidateObj( m_vecStyleClasses );

	if( m_pmapPanelEvents )
	{
		ValidatePtr( m_pmapPanelEvents );
		FOR_EACH_MAP_FAST( *m_pmapPanelEvents, i )
		{
			PanelEvent_t &e = m_pmapPanelEvents->Element( i );
			if( e.eType == PanelEvent_t::k_EEventType_UIEventArray )
			{
				VecUIEvents_t *pvec = e.data.pVecIUIEvent;
				if( pvec )
				{
					ValidatePtr( pvec );
					FOR_EACH_VEC( *pvec, j )
					{
						ValidatePtr( pvec->Element( j ) );
					}
				}
			}
			else if( e.eType == PanelEvent_t::k_EEventType_JSScript )
			{
				validator.ClaimMemory( e.data.pJSScript );
			}
			else if( e.eType == PanelEvent_t::k_EEventType_JSFunction )
			{
				validator.ClaimMemory( e.data.pJSFunction );
			}
		}
	}
	//ValidatePtr( m_pHorizontalScrollBar );
	//ValidatePtr( m_pVerticalScrollBar );

	ValidatePtr( m_pMapParentsByType );
	if ( m_pMapParentsByID )
	{
		ValidatePtr( m_pMapParentsByID );
	}
	ValidatePtr( m_pMapParentsByClass );

	ValidateObj( m_vecEventHandlers );
	FOR_EACH_VEC( m_vecEventHandlers, i )
	{
		validator.ClaimMemory( m_vecEventHandlers[i].pjsHandler );
	}

	ValidatePtr( m_pMapProperties );
	if( m_pMapProperties )
	{
		FOR_EACH_MAP_FAST( *m_pMapProperties, i )
		{
			ValidateObj( m_pMapProperties->Element( i ) );
		}
	}

	ValidatePtr( m_pVecChildrenInHiding );
	if( m_pVecChildrenInHiding )
	{
		FOR_EACH_VEC( *m_pVecChildrenInHiding, i )
		{
			ValidatePtr( m_pVecChildrenInHiding->Element( i ) );
		}
	}
}


void CUIPanel::ValidateStatics( CValidator &validator, const char *pchName )
{
	ValidateObj( s_vecApplyStylesTemp );
}
#endif

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/panel2d.h"
#include "panorama/uijsregistration.h"
#include "panorama/localization/ilocalize.h"
#include "panorama/uievents.h"
#include "panorama/renderer/styleproperties.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/controls/scrollbar.h"
#include "panorama/controls/frame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;


// symbols used in this file
namespace panorama
{
	// constants
	const float k_flScrollLineSizeInPixelsVertical = 32.0f;
	const float k_flScrollLineSizeInPixelsHorizontal = 48.0f;

	CUtlVector< IUIPanel * > CPanel2D::s_vecMatchingChildren;
}

// panel registration
REGISTER_PANEL2D_FACTORY( CPanel2D, Panel );

REGISTER_PANEL2D_FACTORY( CFramePanel, Frame ); // moved from frame.cpp to force linkage

namespace panorama
{
	class CPanel2DAppendChildHelper : public CPanel2D
	{
		DECLARE_PANEL2D( CPanel2DAppendChildHelper, CPanel2D );

	public:
		CPanel2DAppendChildHelper( CPanel2D *parent, const char * pchID, CPanel2D *pTargetToAppendTo ) : CPanel2D( parent, pchID )
		{
			m_pPanelToAppendTo = pTargetToAppendTo;
			if( !UIEngine()->BHaveEventHandlersRegisteredForType( CPanel2DAppendChildHelper::GetPanelSymbol() ) )
			{
				RegisterEventHandlerOnPanelType( LoadAsyncComplete(), &CPanel2DAppendChildHelper::EventLoadAsyncComplete );
			}
		}

		bool EventLoadAsyncComplete( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, bool bSuccess, panorama::ELoadLayoutAsyncDetails eDetails, bool bPartialLayout )
		{
			CPanel2D *pNewParent = m_pPanelToAppendTo.Get() ;
			if( bSuccess && pNewParent )
			{
				while ( GetChildCount() > 0 )
				{
					CPanel2D *pChild = GetChild( 0 );
					pChild->SetParent( pNewParent );
				}
			}

			DeleteAsync( 0.0f );

			return true;
		}

	private:

		CPanelPtr< CPanel2D > m_pPanelToAppendTo;
	};
}
REGISTER_PANEL2D( CPanel2DAppendChildHelper, Panel2DAppendChildHelper )


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPanel2D::CPanel2D( CPanel2D *parent, const char *pchID )
{
	Initialize( NULL, parent, pchID, 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor - You should normally never need this, it's for special
// case internal controls like scrollbars that need special hierachy/rendering
// rules applied to them.
//-----------------------------------------------------------------------------
CPanel2D::CPanel2D( CPanel2D *parent, const char *pchID, uint32 ePanelFlags ) 
{
	Initialize( NULL, parent, pchID, ePanelFlags );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPanel2D::CPanel2D( IUIWindow *window, const char *pchID )
{
	Initialize( window, NULL, pchID, 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CPanel2D::Initialize( IUIWindow *window, CPanel2D *parent, const char *pchID, uint32 ePanelFlags )
{
	VPROF_BUDGET( "CPanel2D::Initialize", VPROF_BUDGETGROUP_TENFOOT );

	UISoundSystem()->ServiceAudio();

	if ( window == NULL && parent )
		window = parent->GetParentWindow();

	Assert( parent != this );
	m_pIUIPanel = UIEngine()->CreatePanel( window );
	m_pIUIPanel->SetClientPtr( this );
	m_pJSData = NULL;

	if( !UIEngine()->BHaveEventHandlersRegisteredForType( CPanel2D::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( AddStyle(), &CPanel2D::EventAddStyleClass );
		RegisterEventHandlerOnPanelType( RemoveStyle(), &CPanel2D::EventRemoveStyleClass );
		RegisterEventHandlerOnPanelType( ToggleStyle(), &CPanel2D::EventToggleStyleClass );
		RegisterEventHandlerOnPanelType( SwitchStyle(), &CPanel2D::EventSwitchStyleClass );
		RegisterEventHandlerOnPanelType( TriggerStyle(), &CPanel2D::EventTriggerStyleClass );
		RegisterEventHandlerOnPanelType( AddStyleToEachChild(), &CPanel2D::EventAddStyleClassToEachChild );
		RegisterEventHandlerOnPanelType( RemoveStyleFromEachChild(), &CPanel2D::EventRemoveStyleClassFromEachChild );
		RegisterEventHandlerOnPanelType( AppendChildrenFromLayoutFileAsync(), &CPanel2D::EventAppendChildrenFromLayoutFileAsync );
		RegisterEventHandlerOnPanelType( PanelLoaded(), &CPanel2D::EventPanelLoaded );
		RegisterEventHandlerOnPanelType( Activated(), &CPanel2D::EventPanelActivated );
		RegisterEventHandlerOnPanelType( Cancelled(), &CPanel2D::EventPanelCancelled );
		RegisterEventHandlerOnPanelType( ContextMenu(), &CPanel2D::EventPanelContextMenu );
		RegisterEventHandlerOnPanelType( MoveUp(), &CPanel2D::EventMoveUp );
		RegisterEventHandlerOnPanelType( MoveDown(), &CPanel2D::EventMoveDown );
		RegisterEventHandlerOnPanelType( MoveLeft(), &CPanel2D::EventMoveLeft );
		RegisterEventHandlerOnPanelType( MoveRight(), &CPanel2D::EventMoveRight );
		RegisterEventHandlerOnPanelType( MovePanelUp(), &CPanel2D::EventMovePanelUp );
		RegisterEventHandlerOnPanelType( MovePanelDown(), &CPanel2D::EventMovePanelDown );
		RegisterEventHandlerOnPanelType( MovePanelLeft(), &CPanel2D::EventMovePanelLeft );
		RegisterEventHandlerOnPanelType( MovePanelRight(), &CPanel2D::EventMovePanelRight );
		RegisterEventHandlerOnPanelType( ScrollUp(), &CPanel2D::EventScrollUp );
		RegisterEventHandlerOnPanelType( ScrollDown(), &CPanel2D::EventScrollDown );
		RegisterEventHandlerOnPanelType( ScrollLeft(), &CPanel2D::EventScrollLeft );
		RegisterEventHandlerOnPanelType( ScrollRight(), &CPanel2D::EventScrollRight );
		RegisterEventHandlerOnPanelType( ScrollPanelUp(), &CPanel2D::EventScrollPanelUp );
		RegisterEventHandlerOnPanelType( ScrollPanelDown(), &CPanel2D::EventScrollPanelDown );
		RegisterEventHandlerOnPanelType( ScrollPanelLeft(), &CPanel2D::EventScrollPanelLeft );
		RegisterEventHandlerOnPanelType( ScrollPanelRight(), &CPanel2D::EventScrollPanelRight );
		RegisterEventHandlerOnPanelType( PageUp(), &CPanel2D::EventPageUp );
		RegisterEventHandlerOnPanelType( PageDown(), &CPanel2D::EventPageDown );
		RegisterEventHandlerOnPanelType( PageLeft(), &CPanel2D::EventPageLeft );
		RegisterEventHandlerOnPanelType( PageRight(), &CPanel2D::EventPageRight );
		RegisterEventHandlerOnPanelType( PagePanelUp(), &CPanel2D::EventPagePanelUp );
		RegisterEventHandlerOnPanelType( PagePanelDown(), &CPanel2D::EventPagePanelDown );
		RegisterEventHandlerOnPanelType( PagePanelLeft(), &CPanel2D::EventPagePanelLeft );
		RegisterEventHandlerOnPanelType( PagePanelRight(), &CPanel2D::EventPagePanelRight );
		RegisterEventHandlerOnPanelType( TabForward(), &CPanel2D::EventTabForward );
		RegisterEventHandlerOnPanelType( TabBackward(), &CPanel2D::EventTabBackward );
		RegisterEventHandlerOnPanelType( ImageLoaded(), &CPanel2D::EventImageLoaded );
		RegisterEventHandlerOnPanelType( ImageFailedLoad(), &CPanel2D::EventImageFailedLoad );
		RegisterEventHandlerOnPanelType( panorama::SetPanelEvent(), &CPanel2D::EventSetPanelEvent );
		RegisterEventHandlerOnPanelType( panorama::ClearPanelEvent(), &CPanel2D::EventClearPanelEvent );
		RegisterEventHandlerOnPanelType( panorama::DispatchPanelEvent(), &CPanel2D::EventDispatchPanelEvent );
		RegisterEventHandlerOnPanelType( panorama::IfHasClassEvent(), &CPanel2D::EventIfHasClassEvent );
		RegisterEventHandlerOnPanelType( panorama::IfNotHasClassEvent(), &CPanel2D::EventIfNotHasClassEvent );
		RegisterEventHandlerOnPanelType( panorama::IfHoverOtherEvent(), &CPanel2D::EventIfHoverOtherEvent );
		RegisterEventHandlerOnPanelType( panorama::IfNotHoverOtherEvent(), &CPanel2D::EventIfNotHoverOtherEvent );
		RegisterEventHandlerOnPanelType( panorama::ScrollToTop(), &CPanel2D::EventScrollToTop );
		RegisterEventHandlerOnPanelType( panorama::ScrollToBottom(), &CPanel2D::EventScrollToBottom );
		RegisterEventHandlerOnPanelType( panorama::CheckChildrenScrolledIntoView(), &CPanel2D::EventCheckChildrenScrolledIntoView );
		RegisterEventHandlerOnPanelType( panorama::ScrollPanelIntoView(), &CPanel2D::EventScrollPanelIntoView );
		RegisterEventHandlerOnPanelType( panorama::SetPanelEnabled(), &CPanel2D::EventSetPanelEnabled );
		RegisterEventHandlerOnPanelType( panorama::DragScrollStart(), &CPanel2D::EventDragScrollStart );
		RegisterEventHandlerOnPanelType( panorama::DragScrollMouseMove(), &CPanel2D::EventDragScrollMouseMove );
		RegisterEventHandlerOnPanelType( panorama::DragScrollEnd(), &CPanel2D::EventDragScrollEnd );

		m_pIUIPanel->RegisterEventHandlersOnPanel2DType( CPanel2D::GetPanelSymbol() );
	}

	m_pIUIPanel->Initialize( window, parent ? parent->UIPanel() : NULL, pchID, ePanelFlags );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPanel2D::~CPanel2D()
{
	VPROF_BUDGET( "CPanel2D::~CPanel2D", VPROF_BUDGETGROUP_TENFOOT );

	UnregisterForUnhandledEvents( this );

	if ( m_pJSData )
	{
		delete m_pJSData;
		m_pJSData = NULL;
	}

	IUIPanel *pOldParent = GetParent() ? GetParent()->UIPanel() : NULL;

	if( m_pTooltip.Get() )
	{
		delete m_pTooltip.Get();
		m_pTooltip = NULL;
	}

	m_pIUIPanel->Shutdown();
	UIEngine()->PanelDestroyed( m_pIUIPanel, pOldParent );

#ifndef PANORAMA_USE_S1WRAPPER
	VPROF_BUDGET( "~CPanel2D - destroy members", VPROF_BUDGETGROUP_TENFOOT );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Callback to client panel to create a scrollbar
//-----------------------------------------------------------------------------
IUIScrollBar *CPanel2D::CreateNewVerticalScrollBar( float flInitialScrollPos )
{
	UIPanel()->SetInScrollbarConstruction( true );
	CVerticalScrollBar *pScrollBar = new CVerticalScrollBar( this, "VerticalScrollBar" );
	UIPanel()->SetInScrollbarConstruction( false );

	pScrollBar->SetScrollWindowPosition( flInitialScrollPos );
	pScrollBar->SetVisible( true );

	return pScrollBar;
}


//-----------------------------------------------------------------------------
// Purpose: Callback to client panel to create a scrollbar
//-----------------------------------------------------------------------------
IUIScrollBar *CPanel2D::CreateNewHorizontalScrollBar( float flInitialScrollPos )
{
	UIPanel()->SetInScrollbarConstruction( true );
	CHorizontalScrollBar *pScrollBar = new CHorizontalScrollBar( this, "HorizontalScrollBar" );
	UIPanel()->SetInScrollbarConstruction( false );

	pScrollBar->SetScrollWindowPosition( flInitialScrollPos );
	pScrollBar->SetVisible( true );

	return pScrollBar;
}

//-----------------------------------------------------------------------------
// Purpose: float specialization for property getter
//-----------------------------------------------------------------------------
template <typename GetterDelegate>
void JSTabIndexSelectionPosGetter( v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info )
{
	CPanel2D *pPanel = GetThisPtrForJSCall<CPanel2D>( info.Holder() );
	if( !pPanel )
		return;

	float flVal = GetterDelegate::Apply( *pPanel );
	if( flVal == k_flTabIndexAuto )
	{
		v8::Handle<v8::String> prop = v8::String::NewFromUtf8( info.GetIsolate(), "auto" ).ToLocalChecked();
		info.GetReturnValue().Set( prop );
	}
	else if( flVal == k_flTabIndexInvalid )
	{
		info.GetReturnValue().Set( v8::Null( info.GetIsolate() ) );
	}
	else
	{
		v8::Handle< v8::Number > return_val = v8::Number::New( info.GetIsolate(), (double)flVal );
		info.GetReturnValue().Set( return_val );
	}
}


//-----------------------------------------------------------------------------
// Purpose: float specialization for property setter
//-----------------------------------------------------------------------------
template <typename SetterDelegate>
void JSTabIndexSelectionPosSetter( v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info )
{
	CPanel2D *pPanel = GetThisPtrForJSCall<CPanel2D>( info.Holder() );
	if( !pPanel )
		return;

	if ( !value->IsNumber() )
	{
		if ( value->IsNull() )
			SetterDelegate::Apply( *pPanel, k_flTabIndexInvalid );
		else if ( value->IsString() )
		{
			v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  value );
			const char * pchString = *str;
			if ( V_strcmp( pchString, "auto" ) == 0 )
				SetterDelegate::Apply( *pPanel, k_flTabIndexAuto );
			else
			{
				v8::String::Utf8Value prop(v8::Isolate::GetCurrent(),  property );
				info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Error trying to set %s to something other than a number, null, or auto", *prop ).String() ).ToLocalChecked() );
			}
		}
		else
		{
			v8::String::Utf8Value prop(v8::Isolate::GetCurrent(),  property );
			info.GetIsolate()->ThrowException( v8::String::NewFromUtf8( info.GetIsolate(), CFmtStr1024( "Error trying to set %s to something other than a number, null, or auto", *prop ).String() ).ToLocalChecked() );
		}
	}
	else
		SetterDelegate::Apply( *pPanel, ( float )value->NumberValue( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( 0 ) );
}


//-----------------------------------------------------------------------------
// Purpose: Register a float member to expose to JavaScript, can pass NULL for pSetFunc if this is only able to be read not written
//-----------------------------------------------------------------------------
template <typename GetterDelegate, typename SetterDelegate>
static void RegisterJSFloatTabIndexSelectionPos( const char *pchjsMemberName, GetterDelegate pGetFunc, SetterDelegate pSetFunc )
{
	COMPILE_TIME_ASSERT( GetterDelegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES != 0 );
	COMPILE_TIME_ASSERT( SetterDelegate::USE__PANORAMA_DELEGATE__MACRO_TO_CREATE_DELEGATES != 0 );
	COMPILE_TIME_ASSERT( GetterDelegate::kNumArgs == 0 );
	COMPILE_TIME_ASSERT( SetterDelegate::kNumArgs == 1 );
	COMPILE_TIME_ASSERT( (std::is_same< void, typename SetterDelegate::ReturnType>::value) );
	COMPILE_TIME_ASSERT( (std::is_same< float, typename GetterDelegate::ReturnType>::value) );
	COMPILE_TIME_ASSERT( (std::is_same< float, typename std::tuple_element<0, typename SetterDelegate::Arguments>::type>::value) );

	RegisterJSAccessorInternal( pchjsMemberName, &JSTabIndexSelectionPosGetter<GetterDelegate>, &JSTabIndexSelectionPosSetter<SetterDelegate>, NULL, k_ERegisterJSTypeFloat );
}



//-----------------------------------------------------------------------------
// Purpose: Setup v8 object template for panel type
//-----------------------------------------------------------------------------
void CPanel2D::SetupJavascriptObjectTemplate()
{
	// Special function IsValid() to let JS check if panel is stil valid, JS gets a sort of safe ptr in that
	// all the methods/accessors will just start throwing an exception if the panel has been deleted.
	RegisterJSIsValid();

	// Read/Write from JS
	RegisterJSAccessor( "visible", PANORAMA_DELEGATE( &CPanel2D::BIsVisible ), PANORAMA_DELEGATE( &CPanel2D::SetVisible ) );
	RegisterJSAccessor( "enabled", PANORAMA_DELEGATE( &CPanel2D::IsEnabled ), PANORAMA_DELEGATE( &CPanel2D::SetEnabled ) );
	RegisterJSAccessor( "checked", PANORAMA_DELEGATE( &CPanel2D::IsSelected ), PANORAMA_DELEGATE( &CPanel2D::SetSelected ) );
	RegisterJSAccessor( "defaultfocus", PANORAMA_DELEGATE( &CPanel2D::GetDefaultFocus ), PANORAMA_DELEGATE( &CPanel2D::SetDefaultFocus ) );
	RegisterJSAccessor( "inputnamespace", PANORAMA_DELEGATE( &CPanel2D::GetInputNamespace ), PANORAMA_DELEGATE( &CPanel2D::SetInputNamespace ) );
	RegisterJSAccessor( "hittest", PANORAMA_DELEGATE( &CPanel2D::BHitTestEnabled ), PANORAMA_DELEGATE( &CPanel2D::SetHitTestEnabled ) );
	RegisterJSAccessor( "hittestchildren", PANORAMA_DELEGATE( &CPanel2D::BHitTestChildrenEnabled ), PANORAMA_DELEGATE( &CPanel2D::SetHitTestChildrenEnabled ) );
	RegisterJSAccessor( "activationenabled", PANORAMA_DELEGATE( &CPanel2D::IsActivationEnabled ), PANORAMA_DELEGATE( &CPanel2D::SetActivationEnabled ) );
	RegisterJSFloatTabIndexSelectionPos( "tabindex", PANORAMA_DELEGATE( &CPanel2D::GetTabIndex ), PANORAMA_DELEGATE( &CPanel2D::SetTabIndex ) );
	RegisterJSFloatTabIndexSelectionPos( "selectionpos_x", PANORAMA_DELEGATE( &CPanel2D::GetSelectionPositionX ), PANORAMA_DELEGATE( &CPanel2D::SetSelectionPositionX ) );
	RegisterJSFloatTabIndexSelectionPos( "selectionpos_y", PANORAMA_DELEGATE( &CPanel2D::GetSelectionPositionY ), PANORAMA_DELEGATE( &CPanel2D::SetSelectionPositionY ) );

	// Read Only
	RegisterJSAccessorReadOnly( "id", PANORAMA_DELEGATE( &CPanel2D::GetID ) );
	RegisterJSAccessorReadOnly( "layoutfile", PANORAMA_DELEGATE( &CPanel2D::GetLayoutFile ) );
	RegisterJSAccessorReadOnly( "contentwidth", PANORAMA_DELEGATE( &CPanel2D::GetContentWidth ) );
	RegisterJSAccessorReadOnly( "contentheight", PANORAMA_DELEGATE( &CPanel2D::GetContentHeight ) );
	RegisterJSAccessorReadOnly( "desiredlayoutwidth", PANORAMA_DELEGATE( &CPanel2D::GetDesiredLayoutWidth ) );
	RegisterJSAccessorReadOnly( "desiredlayoutheight", PANORAMA_DELEGATE( &CPanel2D::GetDesiredLayoutHeight ) );
	RegisterJSAccessorReadOnly( "actuallayoutwidth", PANORAMA_DELEGATE( &CPanel2D::GetActualLayoutWidth ) );
	RegisterJSAccessorReadOnly( "actuallayoutheight", PANORAMA_DELEGATE( &CPanel2D::GetActualLayoutHeight ) );
	RegisterJSAccessorReadOnly( "actualxoffset", PANORAMA_DELEGATE( &CPanel2D::GetActualXOffset ) );
	RegisterJSAccessorReadOnly( "actualyoffset", PANORAMA_DELEGATE( &CPanel2D::GetActualYOffset ) );
	RegisterJSAccessorReadOnly( "scrolloffset_y", PANORAMA_DELEGATE( &CPanel2D::GetContentsYScrollOffset ) );
	RegisterJSAccessorReadOnly( "scrolloffset_x", PANORAMA_DELEGATE( &CPanel2D::GetContentsXScrollOffset ) );
	RegisterJSAccessorReadOnly( "actualuiscale_y", PANORAMA_DELEGATE( &CPanel2D::GetActualUIScaleY ) );
	RegisterJSAccessorReadOnly( "actualuiscale_x", PANORAMA_DELEGATE( &CPanel2D::GetActualUIScaleX ) );
	// Read only, but this just means you can't set .style = something.  You can do .style.color = '#ffffff' or such though.
	RegisterJSAccessorReadOnly( "style", PANORAMA_DELEGATE( &CPanel2D::JSAccessStyle ) );

	// Member functions exposed to JS
	RegisterJSMethod( "AddClass", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::AddClass, const char* ) );
	RegisterJSMethod( "RemoveClass", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::RemoveClass, const char* ) );
	RegisterJSMethod( "BHasClass", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::BHasClass, const char* ) );
	RegisterJSMethod( "SetHasClass", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetHasClass, const char*, bool ) );
	RegisterJSMethod( "ToggleClass", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::ToggleClass, const char* ) );
	RegisterJSMethod( "SwitchClass", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SwitchClass, const char*, const char* ) );
	RegisterJSMethod( "TriggerClass", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::TriggerClass, const char * ) );

	RegisterJSMethod( "ClearPanelEvent", PANORAMA_DELEGATE( &CPanel2D::ClearPanelEventJS ) );

	RegisterJSMethod( "SetDraggable", PANORAMA_DELEGATE( &CPanel2D::SetDraggable ) );
	RegisterJSMethod( "IsDraggable", PANORAMA_DELEGATE( &CPanel2D::IsDraggable ) );

	RegisterJSMethod( "GetChildCount", PANORAMA_DELEGATE( &CPanel2D::GetChildCount ) );
	RegisterJSMethod( "GetChild", PANORAMA_DELEGATE( &CPanel2D::GetChild ) );
	RegisterJSMethod( "GetChildIndex", PANORAMA_DELEGATE( &CPanel2D::GetChildIndex ) );
	RegisterJSMethod( "Children", PANORAMA_DELEGATE( &CPanel2D::AccessChildren ) );
	RegisterJSMethod( "FindChildrenWithClassTraverse", PANORAMA_DELEGATE( &CPanel2D::JSFindChildrenWithClassTraverse ) );
	RegisterJSMethod( "GetParent", PANORAMA_DELEGATE( &CPanel2D::GetParent ) );
	RegisterJSMethod( "SetParent", PANORAMA_DELEGATE( &CPanel2D::SetParent ) );
	RegisterJSMethod( "FindChild", PANORAMA_DELEGATE( &CPanel2D::FindChild ) );
	RegisterJSMethod( "FindChildTraverse", PANORAMA_DELEGATE( &CPanel2D::FindChildTraverse ) );
	RegisterJSMethod( "FindChildInLayoutFile", PANORAMA_DELEGATE( &CPanel2D::FindChildInLayoutFile ) );
	RegisterJSMethod( "RemoveAndDeleteChildren", PANORAMA_DELEGATE( &CPanel2D::RemoveAndDeleteChildren ) );
	RegisterJSMethod( "MoveChildBefore", PANORAMA_DELEGATE( &CPanel2D::MoveChildBefore ) );
	RegisterJSMethod( "MoveChildAfter", PANORAMA_DELEGATE( &CPanel2D::MoveChildAfter ) );
	RegisterJSMethod( "GetPositionWithinWindow", PANORAMA_DELEGATE( &CPanel2D::GetPositionWithinWindowJS ) );
	RegisterJSMethod( "ApplyStyles", PANORAMA_DELEGATE( &CPanel2D::ApplyStyles ) );
	RegisterJSMethod( "ClearPropertyFromCode", PANORAMA_DELEGATE( &CPanel2D::ClearPropertyFromCode ) );

	RegisterJSMethod( "DeleteAsync", PANORAMA_DELEGATE( &CPanel2D::DeleteAsync ) );

	RegisterJSMethod( "BIsTransparent", PANORAMA_DELEGATE( &CPanel2D::BIsTransparent ) );
	RegisterJSMethod( "BAcceptsInput", PANORAMA_DELEGATE( &CPanel2D::BAcceptsInput ) );
	RegisterJSMethod( "BAcceptsFocus", PANORAMA_DELEGATE( &CPanel2D::BAcceptsFocus ) );
	RegisterJSMethod( "SetFocus", PANORAMA_DELEGATE( &CPanel2D::SetFocus ) );
	RegisterJSMethod( "UpdateFocusInContext", PANORAMA_DELEGATE( &CPanel2D::UpdateFocusInContext ) );
	RegisterJSMethod( "BHasHoverStyle", PANORAMA_DELEGATE( &CPanel2D::BHasHoverStyle ) );
	RegisterJSMethod( "SetAcceptsFocus", PANORAMA_DELEGATE( &CPanel2D::SetAcceptsFocus ) );
	RegisterJSMethod( "SetDisableFocusOnMouseDown", PANORAMA_DELEGATE( &CPanel2D::SetDisableFocusOnMouseDown ) );
	RegisterJSMethod( "BHasKeyFocus", PANORAMA_DELEGATE( &CPanel2D::BHasKeyFocus ) );
	RegisterJSMethod( "SetScrollParentToFitWhenFocused", PANORAMA_DELEGATE( &CPanel2D::SetScrollParentToFitWhenFocused ) );
	RegisterJSMethod( "BScrollParentToFitWhenFocused", PANORAMA_DELEGATE( &CPanel2D::BScrollParentToFitWhenFocused ) );
	RegisterJSMethod( "IsSelected", PANORAMA_DELEGATE( &CPanel2D::IsSelected ) );
	RegisterJSMethod( "BHasDescendantKeyFocus", PANORAMA_DELEGATE( &CPanel2D::BHasDescendantKeyFocus ) );
	RegisterJSMethod( "BLoadLayout", PANORAMA_DELEGATE( &CPanel2D::BLoadLayout ) );
	RegisterJSMethodRaw( "BLoadLayoutFromString", PANORAMA_DELEGATE( &CPanel2D::BJSLoadLayoutFromString ) );
	RegisterJSMethod( "LoadLayoutFromStringAsync", PANORAMA_DELEGATE( &CPanel2D::LoadLayoutFromStringAsync ) );
	RegisterJSMethod( "LoadLayoutAsync", PANORAMA_DELEGATE( &CPanel2D::LoadLayoutAsync ) );
	RegisterJSMethod( "BLoadLayoutSnippet", PANORAMA_DELEGATE( &CPanel2D::BLoadLayoutSnippet ) );
	RegisterJSMethod( "BCreateChildren", PANORAMA_DELEGATE( &CPanel2D::BCreateChildren ) );
	RegisterJSMethod( "SetTopOfInputContext", PANORAMA_DELEGATE( &CPanel2D::SetTopOfInputContext ) );

	RegisterJSMethod( "SetDialogVariable", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetDialogVariable, const char*, const char* ) );
	RegisterJSMethod( "SetDialogVariableInt", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetDialogVariable, const char*, int ) );

#if defined( SOURCE2_PANORAMA )
	RegisterJSMethod( "SetDialogVariableTime", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetDialogVariable, const char*, time_t ) );
#else
	RegisterJSMethod( "SetDialogVariableTime", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetDialogVariable, const char*, CRTime ) );
#endif

	RegisterJSMethod( "ScrollToTop", PANORAMA_DELEGATE( &CPanel2D::ScrollToTop ) );
	RegisterJSMethod( "ScrollToBottom", PANORAMA_DELEGATE( &CPanel2D::ScrollToBottom ) );
	RegisterJSMethod( "ScrollToLeftEdge", PANORAMA_DELEGATE( &CPanel2D::ScrollToLeftEdge ) );
	RegisterJSMethod( "ScrollToRightEdge", PANORAMA_DELEGATE( &CPanel2D::ScrollToRightEdge ) );
	RegisterJSMethod( "ScrollParentToMakePanelFit", PANORAMA_DELEGATE( &CPanel2D::ScrollParentToMakePanelFit ) );
	RegisterJSMethod( "ScrollToFitRegion", PANORAMA_DELEGATE( &CPanel2D::ScrollToFitRegion ) );
	RegisterJSMethod( "BCanSeeInParentScroll", PANORAMA_DELEGATE( &CPanel2D::BCanSeeInParentScroll ) );

	// Getters/setters for panel attributes
	RegisterJSMethod( "GetAttributeInt", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::GetAttribute, const char*, int ) );
	RegisterJSMethod( "GetAttributeString", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::GetAttribute, const char*, const char* ) );
	RegisterJSMethod( "GetAttributeUInt32", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::GetAttribute, const char*, uint32 ) );
	RegisterJSMethod( "SetAttributeInt", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetAttribute, const char*, int ) );
	RegisterJSMethod( "SetAttributeString", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetAttribute, const char*, const char * ) );
	RegisterJSMethod( "SetAttributeUInt32", PANORAMA_DELEGATE_RESOLVE( &CPanel2D::SetAttribute, const char*, uint32 ) );

	RegisterJSMethod( "SetInputNamespace", PANORAMA_DELEGATE( &CPanel2D::SetInputNamespace ) );

	RegisterJSMethod( "RegisterForReadyEvents", PANORAMA_DELEGATE( &CPanel2D::RegisterForReadyEvents ) );
	RegisterJSMethod( "BReadyForDisplay", PANORAMA_DELEGATE( &CPanel2D::BReadyForDisplay ) );
	RegisterJSMethod( "SetReadyForDisplay", PANORAMA_DELEGATE( &CPanel2D::SetReadyForDisplay ) );

	RegisterJSMethod( "SetReadyForDisplay", PANORAMA_DELEGATE( &CPanel2D::SetReadyForDisplay ) );

	RegisterJSMethod( "CreateCopyOfCSSKeyframes", PANORAMA_DELEGATE( &CPanel2D::JSCreateCopyOfCSSKeyframes ) );
	RegisterJSMethod( "DeleteKeyframes", PANORAMA_DELEGATE( &CPanel2D::JSDeleteKeyframes ) );
	RegisterJSMethod( "UpdateCurrentAnimationKeyframes", PANORAMA_DELEGATE( &CPanel2D::JSUpdateCurrentAnimationKeyframes ) );
	
	RegisterJSMethodRaw( "Data", PANORAMA_DELEGATE( &CPanel2D::GetJSData ) );	
}


//-----------------------------------------------------------------------------
// Purpose: Implementation of javascript .data() call, a blank slate object
// that can be used to hang arbitrary custom data off the panel.
//-----------------------------------------------------------------------------
void CPanel2D::GetJSData( const v8::FunctionCallbackInfo< v8::Value > &args )
{
	if ( m_pJSData == NULL )
	{
		v8::Isolate *pIsolate = panorama::UIEngine()->GetV8Isolate();
		v8::Isolate::Scope isolate_scope( pIsolate );
		v8::HandleScope handle_scope( pIsolate );

		v8::Persistent<v8::Context> *pContext = UIEngine()->GetJavaScriptContextForPanel( UIPanel() );
		Assert( pContext != NULL );

		v8::Context::Scope context_scope( v8::Local<v8::Context>::New( pIsolate, *pContext ) );

		m_pJSData = new v8::Persistent< v8::Object >();
		m_pJSData->Reset( pIsolate, v8::Object::New( pIsolate ) );
	}

	args.GetReturnValue().Set( v8::Local<v8::Object>::New( args.GetIsolate(), *m_pJSData ) );
}


//-----------------------------------------------------------------------------
// Purpose: Is this a property that should be applied only after children are
// constructed during layout file application
//-----------------------------------------------------------------------------
bool CPanel2D::BIsDelayedProperty( CPanoramaSymbol symProperty )
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: event handler to append more children as created via a layout file
//-----------------------------------------------------------------------------
bool CPanel2D::EventAppendChildrenFromLayoutFileAsync( const CPanelPtr< IUIPanel > &pPanel, const char *pchLayoutFile )
{
	if( GetParentWindow()->BIsWindowInLayoutPass() )
		DispatchEventAsync( 0.0f, AppendChildrenFromLayoutFileAsync(), this, pchLayoutFile );
	else
	{
		//bugbug - make the onscrolledtobottom event asycn
		CPanel2DAppendChildHelper *pTempPanel = new CPanel2DAppendChildHelper( this, "temp", this );

		// Need to apply immediately to force into invisble list, or we'll traverse and complain about no layout file while loading...
		pTempPanel->SetVisible( false );
		pTempPanel->UIPanel()->UpdateVisibility( true );
		pTempPanel->LoadLayoutAsync( pchLayoutFile, false, true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the symbol matches a valid panel event
//-----------------------------------------------------------------------------
bool CPanel2D::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Async panel delete
//-----------------------------------------------------------------------------
void CPanel2D::DeleteAsync( float flDelay /*= 0.0f*/ )
{
	UIEngine()->SetPanelWaitingAsyncDelete( UIPanel() );
	DispatchEventAsync( flDelay, DeletePanel(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Clears a property info set from code
//-----------------------------------------------------------------------------
void CPanel2D::ClearPropertyFromCode( CStyleSymbol symProperty )
{
	if ( !AccessStyle()->BPropertySetFromElement( symProperty ) )
		return;

	AccessStyle()->ClearPropertySetFromElement( symProperty );
	MarkStylesDirty( false );
}


//-----------------------------------------------------------------------------
// Purpose: Returns this panel's style define value
//-----------------------------------------------------------------------------
char const * CPanel2D::GetLayoutFileDefine( char const *szDefineName )
{
	IUILayoutFile *pLayoutFile = UIEngine()->UILayoutManager()->GetLayoutFile( GetLayoutFile() );
	return pLayoutFile ? pLayoutFile->GetDefine( szDefineName ) : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns this panel's style define value as int, with default value if not found
//-----------------------------------------------------------------------------
int CPanel2D::GetLayoutFileDefineInt( const char *szDefineName, int defaultValue )
{
	const char *sz = GetLayoutFileDefine( szDefineName );
	if ( sz )
	{
		return V_atoi( sz );
	}
	else
	{
		return defaultValue;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns this panel's style define value as float, with default value if not found
//-----------------------------------------------------------------------------
float CPanel2D::GetLayoutFileDefineFloat( const char *szDefineName, float defaultValue )
{
	const char *sz = GetLayoutFileDefine( szDefineName );
	if ( sz )
	{
		return V_atof( sz );
	}
	else
	{
		return defaultValue;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the default behavior for whether this panel takes focus on mouse down
//-----------------------------------------------------------------------------
void CPanel2D::SetDefaultFocusOnMouseDownBehavior()
{
	IUIWindow *pWindow = GetParentWindow();
	if ( !pWindow )
		return;

	// Only change the default if the window is set to have shy focus behavior
	if ( pWindow->GetFocusBehavior() != k_EWindowFocusBehavior_Shy )
		return;

	// Control doesn't even take focus - doesn't matter then
	if ( !BAcceptsFocus() )
		return;

	// If the control absolutely requires focus to operate, then just leave it as default (e.g. TextEntry)
	if ( BRequiresFocus() )
		return;

	// If we have a selection position or tab index, then just leave the default behavior. The user has
	// some way to keyboard through this control, so let's just let them do that.
	if ( m_pIUIPanel->GetSelectionPositionX_Raw() != k_flSelectionPosInvalid ||
		 m_pIUIPanel->GetSelectionPositionY_Raw() != k_flSelectionPosInvalid ||
		 m_pIUIPanel->GetTabIndex_Raw() != k_flTabIndexInvalid )
		return;

	// Don't take focus on mouse down by default. This is really useful for Dota, because it means we won't ever take focus
	// without having explicitly set a selection position or tab index. Since Dota doesn't generally show focus indicators,
	// this avoids a bunch of confusion where focus ends up somewhere invisible.
	SetDisableFocusOnMouseDown( true );
}

	
//-----------------------------------------------------------------------------
// Purpose: Called when this panel has been loaded
//-----------------------------------------------------------------------------
bool CPanel2D::EventPanelLoaded( const CPanelPtr< IUIPanel > &pPanel )
{
	// Set the default focus behavior in the PanelLoaded event rather than the constructor
	// so that we have done all of the BSetProperty calls that might affect our selection
	// position or tab index.
	SetDefaultFocusOnMouseDownBehavior();

	static const CPanoramaSymbol k_symPropertyOnLoad( "onload" );
	if ( pPanel.Get() == m_pIUIPanel )
		DispatchPanelEvent( k_symPropertyOnLoad );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when this panel has been selected
//-----------------------------------------------------------------------------
bool CPanel2D::EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	// If we are disabled, not only don't activate, but prevent bubbling
	if ( (m_pIUIPanel->GetStyleFlags() & k_EStyleFlagActivationDisabled) )
	{
		//Activation sounds now played in javascript rather than hardcoded here, so this is commented out.

		// If focus didn't move, then we should play activation failure sound, if focused moved there wasn't an error, we just aren't yet activating
//		if ( GetParentWindow()->UIWindowInput()->GetFocusOnLastMouseDown() == GetParentWindow()->UIWindowInput()->GetInputFocus() )
//			UISoundSystem()->PlaySound( "activation_change_fail", UIPanel(), k_ESoundType_Effects );
//		else
//			UISoundSystem()->PlaySound( "focus_change", UIPanel(), k_ESoundType_Effects, 0.85f );
		return true;
	}

	bool bCanActivate = true;

	if ( eSource == k_ePanelEventSourceMouse )
	{
		switch( m_pIUIPanel->GetMouseCanActivate() )
		{
		case k_EMouseCanActivateUnfocused:
			bCanActivate = true;
			break;
		case k_EMouseCanActivateIfFocused:
			if ( GetParentWindow()->UIWindowInput()->GetFocusOnLastMouseDown() != m_pIUIPanel )
				bCanActivate = false;
			break;
		case k_EMouseCanActivateIfParentFocused:
			{
				IUIPanel *pParent = pPanel->FindParentForMouseCanActivate();
				IUIPanel *pLastFocus = GetParentWindow()->UIWindowInput()->GetFocusOnLastMouseDown();
				bCanActivate = false;
				if ( pParent && pLastFocus )
					bCanActivate = (pLastFocus == pParent || pLastFocus->IsDescendantOf( pParent ) );
			}
			break;
		case k_EMouseCanActivateIfAnyParentFocused:
			IUIPanel *pParent = pPanel->GetParent();
			IUIPanel *pLastFocus = GetParentWindow()->UIWindowInput()->GetFocusOnLastMouseDown();
			bCanActivate = false;

			while ( pParent && !bCanActivate )
			{
				if ( pParent && pLastFocus )
					bCanActivate = (pLastFocus == pParent);

				pParent = pParent->GetParent();
			}
			break;
		}
	}

	// If we can't activate, don't activate, and don't bubble either
	if  ( !bCanActivate )
	{
		//Activation sounds now played in javascript rather than hardcoded here, so this is commented out.

		// If focus didn't move, then we should play activation failure sound, if focused moved there wasn't an error, we just aren't yet activating
//		if ( GetParentWindow()->UIWindowInput()->GetFocusOnLastMouseDown() == GetParentWindow()->UIWindowInput()->GetInputFocus() )
//			UISoundSystem()->PlaySound( "activation_change_fail", UIPanel(), k_ESoundType_Effects );
//		else
//			UISoundSystem()->PlaySound( "focus_change", UIPanel(), k_ESoundType_Effects, 0.85f );
		return true;
	}

	static const CPanoramaSymbol k_symPropertyOnActivate( "onactivate" );
	static const CPanoramaSymbol k_symPropertyOnMouseActivate( "onmouseactivate" );

	if ( eSource == k_ePanelEventSourceMouse && BIsPanelEventSet( k_symPropertyOnMouseActivate ) )
	{
		if ( DispatchPanelEvent( k_symPropertyOnMouseActivate ) )
		{
			//Activation sounds now played in javascript rather than hardcoded here.
			//UISoundSystem()->PlaySound( "activation", UIPanel(), k_ESoundType_Effects );
			return true;
		}
	}

	if ( DispatchPanelEvent( k_symPropertyOnActivate ) )
	{
		UIEngine()->PulseActiveControllerHaptic(UIEngine()->GetHapticFeedbackPositionForInteraction(), IUIEngine::k_EHapticFeedbackStrength_Low );

		//Activation sounds now played in javascript rather than hardcoded here.
		//UISoundSystem()->PlaySound( "activation", UIPanel(), k_ESoundType_Effects );
		return true;
	}
	
	if ( !GetParent() )
	{
		//Activation sounds now played in javascript rather than hardcoded here, so this is commented out.

		// If focus didn't move, then we should play activation failure sound, if focused moved there wasn't an error, we just aren't yet activating
//		if ( GetParentWindow()->UIWindowInput()->GetFocusOnLastMouseDown() == GetParentWindow()->UIWindowInput()->GetInputFocus() )
//			UISoundSystem()->PlaySound( "activation_change_fail", UIPanel(), k_ESoundType_Effects );
//		else
//			UISoundSystem()->PlaySound( "focus_change", UIPanel(), k_ESoundType_Effects, 0.85f );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when this panel has been canceled (b button/esc while in focus)
//-----------------------------------------------------------------------------
bool CPanel2D::EventPanelCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	static const CPanoramaSymbol k_symPropertyOnCancel( "oncancel" );
	return DispatchPanelEvent( k_symPropertyOnCancel );
}


//-----------------------------------------------------------------------------
// Purpose: Called when this panel has been invoked for context menu
//-----------------------------------------------------------------------------
bool CPanel2D::EventPanelContextMenu( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	static const CPanoramaSymbol k_symPropertyOnContextMenu( "oncontextmenu" );
	return DispatchPanelEvent( k_symPropertyOnContextMenu );
}


//-----------------------------------------------------------------------------
// Purpose: Set the base position for the panel
//-----------------------------------------------------------------------------
void CPanel2D::GetPosition( CUILength &x, CUILength &y, CUILength &z, bool bIncludeUIScaleFactor /*= true */ )
{
	AccessStyle()->GetPosition( x, y, z, bIncludeUIScaleFactor );
}


//-----------------------------------------------------------------------------
// Purpose: Set the base position for the panel
//-----------------------------------------------------------------------------
void CPanel2D::SetPosition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor /*= false */ )
{
	AccessStyleDirty()->SetPosition( x, y, z, bPreScaledByUIScaleFactor );
}


//-----------------------------------------------------------------------------
// Purpose: Set the base position for the panel w/o allowing transitions
//-----------------------------------------------------------------------------
void CPanel2D::SetPositionWithoutTransition( CUILength x, CUILength y, CUILength z, bool bPreScaledByUIScaleFactor /*= false */ )
{
	AccessStyleDirty()->SetPositionWithoutTransition( x, y, z, bPreScaledByUIScaleFactor );
}


//-----------------------------------------------------------------------------
// Purpose: Set the transform on top of position
//-----------------------------------------------------------------------------
void CPanel2D::SetTransform3D( const CUtlVector<CTransform3D *> &vecTransforms )
{
	AccessStyleDirty()->SetTransform3D( vecTransforms );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the pre-transform-scale2d property
//-----------------------------------------------------------------------------
void CPanel2D::SetPreTransformScale2D( float flX, float flY )
{
	AccessStyleDirty()->SetScale2DCentered( flX, flY );
}


//-----------------------------------------------------------------------------
// Purpose: Set opacity of the panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOpacity( float flOpacity )
{
	AccessStyleDirty()->SetOpacity( flOpacity );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the style width for this panel
//-----------------------------------------------------------------------------
CUILength CPanel2D::GetStyleWidth()
{
	CUILength len;
	AccessStyle()->GetWidth( len );

	return len;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the style height for this panel
//-----------------------------------------------------------------------------
CUILength CPanel2D::GetStyleHeight()
{
	CUILength len;
	AccessStyle()->GetHeight( len );

	return len;
}


//-----------------------------------------------------------------------------
// Purpose: Set the base width/height for the panel
//-----------------------------------------------------------------------------
void CPanel2D::SetSize( CUILength width, CUILength height )
{
	AccessStyleDirty()->SetWidth( width );
	AccessStyleDirty()->SetHeight( height );	
}


//-----------------------------------------------------------------------------
// Purpose: Returns panel position to JS. Can't pass pointers so need to return a struct
//-----------------------------------------------------------------------------
Vector2D CPanel2D::GetPositionWithinWindowJS()
{
	Vector2D ret;
	GetPositionWithinWindow( &ret.x, &ret.y );

	return ret;
}


//-----------------------------------------------------------------------------
// Purpose: walks parents calculating the top left corner relative to window space
//-----------------------------------------------------------------------------
void CPanel2D::GetPositionWithinWindow( float *pflX, float *pflY )
{
	GetPositionWithinAncestor( NULL, pflX, pflY );
}


//-----------------------------------------------------------------------------
// Purpose: walks parents calculating the given points in the ancestor's space.
// If the passed in panel is NULL or not an ancestor, this will end up being
// relative to the window space 
//-----------------------------------------------------------------------------
void CPanel2D::GetPointsWithinAncestor( CPanel2D *pAncestor, const Vector2D *pPointsIn, Vector2D* pPointsOut, int nPointCount )
{
	if ( nPointCount == 0 )
		return;

	// Convert to 3D
	CUtlVector< Vector > vecPoints3D;
	vecPoints3D.SetCount( nPointCount );
	for ( int i = 0; i < nPointCount; ++i )
	{
		const Vector2D &vIn = pPointsIn[ i ];
		Vector &v = vecPoints3D[ i ];

		v.x = vIn.x;
		v.y = vIn.y;
		v.z = 0.0f;
	}

	GetPointsWithinAncestor( pAncestor, vecPoints3D.Base(), vecPoints3D.Base(), nPointCount );

	// Convert back to 2D
	for ( int i = 0; i < nPointCount; ++i )
	{
		const Vector &v = vecPoints3D[ i ];
		Vector2D &vOut = pPointsOut[ i ];

		vOut.x = v.x;
		vOut.y = v.y;
	}
}


//-----------------------------------------------------------------------------
// Purpose: walks parents calculating the given points in the ancestor's space.
// If the passed in panel is NULL or not an ancestor, this will end up being
// relative to the window space 
//-----------------------------------------------------------------------------
void CPanel2D::GetPointsWithinAncestor( CPanel2D *pAncestor, const Vector *pPointsIn, Vector* pPointsOut, int nPointCount )
{
	if ( nPointCount == 0 )
		return;

	// Initialize input into output
	if ( pPointsIn != pPointsOut )
	{
		for ( int i = 0; i < nPointCount; ++i )
		{
			pPointsOut[ i ] = pPointsIn[ i ];
		}
	}

	// Walk the parent chain
	for ( CPanel2D *pPanel = this; pPanel != NULL && pPanel != pAncestor; pPanel = pPanel->GetParent() )
	{
		IUIPanelStyle *pStyle = pPanel->AccessStyle();

		VMatrix matrix = pStyle->GetTransform3DMatrix();

		// Use a separate matrix to handle the pre-transform-scale2d and pre-transform-rotate2d
		float flScale2DX, flScale2DY;
		float flRotate2D;
		pStyle->GetScale2DCentered( flScale2DX, flScale2DY );
		pStyle->GetRotate2DCentered( flRotate2D );

		VMatrix matrixScaleRotate;
		bool bUseScaleRotateMatrix = false;
		if ( flScale2DX != 1.0f || flScale2DY != 1.0f || flRotate2D != 0.0f )
		{
			bUseScaleRotateMatrix = true;

			matrixScaleRotate = SetupMatrixIdentity();
			if ( flScale2DX != 1.0f || flScale2DY != 1.0f )
			{
				matrixScaleRotate = matrixScaleRotate * SetupMatrixScale( Vector( flScale2DX, flScale2DY, 1.0f ) );
			}
			if ( flRotate2D != 0.0f )
			{
				matrixScaleRotate = matrixScaleRotate * SetupMatrixAxisRot( Vector( 0.0f, 0.0f, 1.0f ), flRotate2D );
			}

			float flCenterX = pPanel->GetActualLayoutWidth() / 2.0f;
			float flCenterY = pPanel->GetActualLayoutHeight() / 2.0f;
			matrixScaleRotate = SetupMatrixTranslation( Vector( flCenterX, flCenterY, 0 ) ) * matrixScaleRotate * SetupMatrixTranslation( Vector( -flCenterX, -flCenterY, 0 ) );
		}

		// Precompute the correct matrix transform
		bool bIdentityMatrix = matrix.IsIdentity();
		if ( !bIdentityMatrix )
		{
			// todo: this doesn't handle bParentLayerRelative properly
			CUILength lenOriginX, lenOriginY;
			bool bParentLayerRelative = false;
			pStyle->GetTransformOrigin( lenOriginX, lenOriginY, bParentLayerRelative );
			float xTrans = lenOriginX.GetValueAsLength( pPanel->GetActualLayoutWidth() );
			float yTrans = lenOriginY.GetValueAsLength( pPanel->GetActualLayoutHeight() );

			matrix = SetupMatrixTranslation( Vector( xTrans, yTrans, 0 ) ) * matrix * SetupMatrixTranslation( Vector( -xTrans, -yTrans, 0 ) );
		}

		// Transform each point
		for ( int i = 0; i < nPointCount; ++i )
		{
			Vector &vOut = pPointsOut[ i ];

			if ( bUseScaleRotateMatrix )
			{
				vOut = matrixScaleRotate * vOut;
			}

			if ( !bIdentityMatrix )
			{
				vOut = matrix * vOut;
			}

			vOut.x += pPanel->GetActualXOffset();
			vOut.y += pPanel->GetActualYOffset();

			// todo(ericl): z position?

			vOut.x += pPanel->GetParent() ? pPanel->GetParent()->GetInterpolatedXScrollOffset() : 0.0f;
			vOut.y += pPanel->GetParent() ? pPanel->GetParent()->GetInterpolatedYScrollOffset() : 0.0f;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: walks parents calculating the top left corner relative to the
// ancestor's space. If the passed in panel is NULL or not an ancestor, this
// will end up being relative to the window space 
//-----------------------------------------------------------------------------
void CPanel2D::GetPositionWithinAncestor( CPanel2D *pAncestor, float *pflX, float *pflY )
{
	Vector vPosition( 0.0f, 0.0f, 0.0f );
	GetPointsWithinAncestor( pAncestor, &vPosition, &vPosition, 1 );
	*pflX = vPosition.x;
	*pflY = vPosition.y;
}


//-----------------------------------------------------------------------------
// Purpose: calculates the axis-aligned bounding box of this panel relative
// to an ancestor panel.
//-----------------------------------------------------------------------------
void CPanel2D::GetBoundsWithinAncestor( CPanel2D *pAncestor, float *pflLeft, float *pflTop, float *pflRight, float *pflBottom )
{
	Vector2D vCorners[ 4 ];
	vCorners[ 0 ].Init( 0.0f, 0.0f );
	vCorners[ 1 ].Init( GetActualLayoutWidth(), 0.0f );
	vCorners[ 2 ].Init( 0.0f, GetActualLayoutHeight() );
	vCorners[ 3 ].Init( GetActualLayoutWidth(), GetActualLayoutHeight() );

	GetPointsWithinAncestor( pAncestor, vCorners, vCorners, V_ARRAYSIZE( vCorners ) );

	*pflLeft = vCorners[ 0 ].x;
	*pflRight = vCorners[ 0 ].x;
	*pflTop = vCorners[ 0 ].y;
	*pflBottom = vCorners[ 0 ].y;

	for ( int i = 1; i < V_ARRAYSIZE( vCorners ); ++i )
	{
		*pflLeft = Min( *pflLeft, vCorners[ i ].x );
		*pflRight = Max( *pflRight, vCorners[ i ].x );
		*pflTop  = Min( *pflTop, vCorners[ i ].y );
		*pflBottom = Max( *pflBottom, vCorners[ i ].y );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the location of points on this panel relative to another panel's coordinate system
//-----------------------------------------------------------------------------
void CPanel2D::GetPointsRelativeToPanel( CPanel2D *pOtherPanel, const Vector *pPointsIn, Vector *pPointsOut, int nPointCount )
{
	// If the other panel is an ancestor of this panel (or nullptr meaning the window), then just get
	// the points within the ancestor
	if ( !pOtherPanel || IsDescendantOf( pOtherPanel ) )
	{
		GetPointsWithinAncestor( pOtherPanel, pPointsIn, pPointsOut, nPointCount );
		return;
	}

	// If the other panel is our descendant, then 
	CPanel2D *pRelativeToAncestor = pOtherPanel->IsDescendantOf( this ) ? this : FindLowestCommonAncestor( pOtherPanel );

	// Get the origin of the other panel relative to either ourself, or our common ancestor
	Vector vOtherOrigin( 0.0f, 0.0f, 0.0f );
	pOtherPanel->GetPointsWithinAncestor( pRelativeToAncestor, &vOtherOrigin, &vOtherOrigin, 1 );

	// If we're working with a common ancestor, then convert the incoming points into its coordinate system
	if ( pRelativeToAncestor != this )
	{
		GetPointsWithinAncestor( pRelativeToAncestor, pPointsIn, pPointsOut, nPointCount );
	}

	// Add the origin of the other panel relative to the ancestor
	for ( int i = 0; i < nPointCount; ++i )
	{
		pPointsOut[ i ] -= vOtherOrigin;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Given an array of points within the current panel's coordinate
// system, convert them to another panel's coordinate system. If the passed in
// panel is NULL or not in the same top level window, this will end up being
// relative to the top level window
//-----------------------------------------------------------------------------
void CPanel2D::GetPointsRelativeToPanel( CPanel2D *pOtherPanel, const Vector2D *pPointsIn, Vector2D* pPointsOut, int nPointCount )
{
	if ( nPointCount == 0 )
		return;

	// Convert to 3D
	CUtlVector< Vector > vecPoints3D;
	vecPoints3D.SetCount( nPointCount );
	for ( int i = 0; i < nPointCount; ++i )
	{
		const Vector2D &vIn = pPointsIn[ i ];
		Vector &v = vecPoints3D[ i ];

		v.x = vIn.x;
		v.y = vIn.y;
		v.z = 0.0f;
	}

	GetPointsRelativeToPanel( pOtherPanel, vecPoints3D.Base(), vecPoints3D.Base(), nPointCount );

	// Convert back to 2D
	for ( int i = 0; i < nPointCount; ++i )
	{
		const Vector &v = vecPoints3D[ i ];
		Vector2D &vOut = pPointsOut[ i ];

		vOut.x = v.x;
		vOut.y = v.y;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Paint the panel
//-----------------------------------------------------------------------------
void CPanel2D::Paint()
{
	// paint panel contents, to be overridden
}


//-----------------------------------------------------------------------------
// Purpose: Paint the panel's contents in the given area
//-----------------------------------------------------------------------------
void CPanel2D::PaintArea( const PanoramaRect_t &rectPaintArea )
{
	// paint panel contents, to be overridden
}


//-----------------------------------------------------------------------------
// Purpose: Called on the first frame that this panel skipped painting because it was not visible
//-----------------------------------------------------------------------------
void CPanel2D::StoppedPainting()
{
	// to be overridden
}


//-----------------------------------------------------------------------------
// Purpose: Called with all properties parsed from the layout file for this panel
//			Default implementation just calls BSetProperty(). Derived panels can override either
//-----------------------------------------------------------------------------
bool CPanel2D::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	bool bSuccess = true;
	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
			bSuccess = false;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------
bool CPanel2D::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	VPROF_BUDGET( "CPanel2D::BSetProperty", VPROF_BUDGETGROUP_TENFOOT );
	return UIPanel()->BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Builds a string of properties and values to display in the debugger
//-----------------------------------------------------------------------------
void CPanel2D::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	// Let base UIPanel add all it's stuff first
	UIPanel()->GetDebugPropertyInfo( pvecProperties );

	// We could add more here if we added properties directly on CPanel2D
	// ...
}


//-----------------------------------------------------------------------------
// Purpose: Received an on add class event
//-----------------------------------------------------------------------------
bool CPanel2D::EventAddStyleClassToEachChild( const CPanelPtr< IUIPanel > &pPanel, const char *pchName )
{
	CPanoramaSymbol symClass( pchName );
	for( int i = 0; i < GetChildCount(); ++i )
	{
		GetChild(i)->AddClass( symClass );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Received an on remove class event
//-----------------------------------------------------------------------------
bool CPanel2D::EventRemoveStyleClassFromEachChild( const CPanelPtr< IUIPanel > &pPanel, const char *pchName )
{
	CPanoramaSymbol symClass( pchName );
	for( int i = 0; i < GetChildCount(); ++i )
	{
		GetChild( i )->RemoveClass( symClass );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Received an on add class event
//-----------------------------------------------------------------------------
bool CPanel2D::EventAddStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName )
{
	if ( pPanel.Get() == UIPanel() )
	{
		AddClass( pchName );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Received an on remove class event
//-----------------------------------------------------------------------------
bool CPanel2D::EventRemoveStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName )
{
	if ( pPanel.Get() == UIPanel() )
	{
		RemoveClass( pchName );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Received a toggle class event
//-----------------------------------------------------------------------------
bool CPanel2D::EventToggleStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName )
{
	if ( pPanel.Get() == UIPanel() )
	{
		ToggleClass( pchName );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Received a switch class event
//-----------------------------------------------------------------------------
bool CPanel2D::EventSwitchStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchAttributeName, const char *pchName )
{
	if ( pPanel.Get() == UIPanel() )
	{
		SwitchClass( pchAttributeName, pchName );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Received a trigger class event
//-----------------------------------------------------------------------------
bool CPanel2D::EventTriggerStyleClass( const CPanelPtr< IUIPanel > &pPanel, const char *pchName )
{
	if ( pPanel.Get() == UIPanel() )
	{
		TriggerClass( pchName );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns all children matching a class
//-----------------------------------------------------------------------------
CUtlVector<IUIPanel *> const &CPanel2D::JSFindChildrenWithClassTraverse( const char *pchClass )
{
	s_vecMatchingChildren.Purge();

	m_pIUIPanel->FindChildrenWithClassTraverse( pchClass, s_vecMatchingChildren );
	return s_vecMatchingChildren;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for scrolling in a direction
//-----------------------------------------------------------------------------
bool CPanel2D::OnScrollDirection( IUIScrollBar *pScrollBar, bool bIncreasePosition, float flDelta )
{
	if ( !pScrollBar )
		return false;

	float flOffset = ( bIncreasePosition ? 1 : -1 ) * flDelta;
	pScrollBar->SetScrollWindowPosition( pScrollBar->GetScrollWindowPosition() + flOffset );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handlers for scrolling in a direction
//-----------------------------------------------------------------------------
bool CPanel2D::OnScrollUp()		{ return OnScrollDirection( UIPanel()->GetVerticalScrollBar(), false, k_flScrollLineSizeInPixelsVertical ); }
bool CPanel2D::OnScrollDown()	{ return OnScrollDirection( UIPanel()->GetVerticalScrollBar(), true, k_flScrollLineSizeInPixelsVertical ); }
bool CPanel2D::OnScrollLeft()	{ return OnScrollDirection( UIPanel()->GetHorizontalScrollBar(), false, k_flScrollLineSizeInPixelsHorizontal ); }
bool CPanel2D::OnScrollRight()	{ return OnScrollDirection( UIPanel()->GetHorizontalScrollBar(), true, k_flScrollLineSizeInPixelsHorizontal ); }


//-----------------------------------------------------------------------------
// Purpose: Event handler for scroll up
//-----------------------------------------------------------------------------
void CPanel2D::ScrollVertically( float flScrollDelta, bool bImmediateMove )
{
	IUIScrollBar *pScrollBar = UIPanel()->GetVerticalScrollBar();
	if ( pScrollBar )
	{
		pScrollBar->SetScrollWindowPosition( pScrollBar->GetScrollWindowPosition() + flScrollDelta, bImmediateMove );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for scroll up
//-----------------------------------------------------------------------------
void CPanel2D::ScrollHorizontally( float flScrollDelta, bool bImmediateMove )
{
	IUIScrollBar *pScrollBar = UIPanel()->GetHorizontalScrollBar();
	if ( pScrollBar )
	{
		pScrollBar->SetScrollWindowPosition( pScrollBar->GetScrollWindowPosition() + flScrollDelta, bImmediateMove );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Scroll to a specific percentage of the width (pass 0.0f -> 1.0f)
//-----------------------------------------------------------------------------
void CPanel2D::ScrollToXPercent( float flXPercent )
{
	IUIScrollBar *pScrollBar = UIPanel()->GetHorizontalScrollBar();
	if ( pScrollBar )
	{
		float flPos = m_pIUIPanel->GetContentWidth() * flXPercent;
		pScrollBar->SetScrollWindowPosition( clamp( flPos, 0.0f, m_pIUIPanel->GetContentWidth() - pScrollBar->GetScrollWindowSize() ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Scroll to a specific percentage of the height (pass 0.0f -> 1.0f)
//-----------------------------------------------------------------------------
void CPanel2D::ScrollToYPercent( float flYPercent )
{
	IUIScrollBar *pScrollBar = UIPanel()->GetVerticalScrollBar();
	if ( pScrollBar )
	{
		float flPos = m_pIUIPanel->GetContentHeight() * flYPercent;
		pScrollBar->SetScrollWindowPosition( clamp( flPos, 0.0f, m_pIUIPanel->GetContentHeight() - pScrollBar->GetScrollWindowSize() ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Scrolls panel into view when event is received
//-----------------------------------------------------------------------------
bool CPanel2D::EventScrollPanelIntoView( const CPanelPtr< IUIPanel > &pPanel, ScrollBehavior_t behavior, bool bImmediate )
{
	ScrollParentToMakePanelFit( behavior, bImmediate );
	
	// don't bubble
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler to set a panel enabled
//-----------------------------------------------------------------------------
bool CPanel2D::EventSetPanelEnabled( const CPanelPtr< IUIPanel > &pPanel, bool bEnabled )
{
	SetEnabled( bEnabled );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for drag scroll start.  Dispatch to the individual
//			scrollbars as appropriate.
//-----------------------------------------------------------------------------
bool CPanel2D::EventDragScrollStart( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( UIPanel()->GetHorizontalScrollBar() )
	{
		UIPanel()->GetHorizontalScrollBar()->OnDragScrollStart();
	}

	if ( UIPanel()->GetVerticalScrollBar() )
	{
		UIPanel()->GetVerticalScrollBar()->OnDragScrollStart();
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for drag scroll mouse move.  Dispatch to the individual
//			scrollbars as appropriate.
//-----------------------------------------------------------------------------
bool CPanel2D::EventDragScrollMouseMove( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, int nX, int nY )
{
	if ( UIPanel()->GetHorizontalScrollBar() )
	{
		UIPanel()->GetHorizontalScrollBar()->OnDragScrollMouseMove( nLastX, nX );
	}
	
	if ( UIPanel()->GetVerticalScrollBar() )
	{
		UIPanel()->GetVerticalScrollBar()->OnDragScrollMouseMove( nLastY, nY );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for drag scroll end.  Dispatch to the individual
//			scrollbars as appropriate.
//-----------------------------------------------------------------------------
bool CPanel2D::EventDragScrollEnd( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, float flVelocityX, float flVelocityY )
{
	if ( UIPanel()->GetHorizontalScrollBar() )
	{
		UIPanel()->GetHorizontalScrollBar()->OnDragScrollEnd( nLastX, flVelocityX );
	}
	
	if ( UIPanel()->GetVerticalScrollBar() )
	{
		UIPanel()->GetVerticalScrollBar()->OnDragScrollEnd( nLastY, flVelocityY );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for move up
//-----------------------------------------------------------------------------
bool CPanel2D::OnMoveUp( int nRepeats )
{
	static const CPanoramaSymbol k_symNavigateUpEvent( "onmoveup" );
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

	float x0 = flLeft;
	float x1 = m_pIUIPanel->GetActualLayoutWidth() + flRight;
	float y0 = flTop;
	float y1 = m_pIUIPanel->GetActualLayoutHeight() + flBottom;

	CPanel2D *pToScroll = this;
	while ( pToScroll )
	{
		IUIScrollBar *pVerticalScrollBar = pToScroll->UIPanel()->GetVerticalScrollBar();
		if ( pVerticalScrollBar )
		{
			if ( y0 < pVerticalScrollBar->GetScrollWindowPosition() && y1 > pVerticalScrollBar->GetScrollWindowPosition()
				&& pVerticalScrollBar->GetScrollWindowPosition() > 0 )
			{
				pToScroll->ScrollVertically( -100.0f );
				return true;
			}

			break;
		}
		
		x0 += pToScroll->UIPanel()->GetActualXOffset();
		x1 += pToScroll->UIPanel()->GetActualXOffset();
		y0 += pToScroll->UIPanel()->GetActualYOffset();
		y1 += pToScroll->UIPanel()->GetActualYOffset();

		pToScroll = pToScroll->GetParent();
		if ( pToScroll )
		{
			// include padding
			pToScroll->AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

			x0 = x0-flLeft;
			x1 = x1-flLeft;
			y0 = y0-flTop;
			y1 = y1-flTop;
		}
	}

	if ( BSelectionPosVerticalBoundary() && nRepeats > 0 )
	{
		UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ) );
		return true;
	}

	// Any set onmoveup overrides normal navigation behavior
	if ( BIsPanelEventSet( k_symNavigateUpEvent ) )
	{
		DispatchPanelEvent( k_symNavigateUpEvent );
		UIPanel()->PlayFocusChangeSound( nRepeats, 0.5f );
		return true;
	}

	if ( GetParent() )
	{
		float x = GetSelectionPositionX();
		float y = GetSelectionPositionY();
		if ( GetParent()->SetFocusToNextPanel( nRepeats, k_EPrevByYPosition, false, GetTabIndex(), x, y, x, y ) )
		{
			UIPanel()->PlayFocusChangeSound( nRepeats, 0.5f );
			return true;
		}		
	}
	else
	{
		if ( GetParentWindow() && !GetParentWindow()->BOnMoveEdge( k_EPrevByYPosition ) )
		{
			UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ) );
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for move down
//-----------------------------------------------------------------------------
bool CPanel2D::OnMoveDown( int nRepeats )
{
	static const CPanoramaSymbol k_symNavigateDownEvent( "onmovedown" );
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

	float x0 = flLeft;
	float x1 = m_pIUIPanel->GetActualLayoutWidth() + flRight;
	float y0 = flTop;
	float y1 = m_pIUIPanel->GetActualLayoutHeight() + flBottom;

	CPanel2D *pToScroll = this;
	while ( pToScroll )
	{
		IUIScrollBar *pVerticalScrollBar = pToScroll->UIPanel()->GetVerticalScrollBar();
		if ( pVerticalScrollBar )
		{
			//Msg( "%s y0: %1.2f ->y1: %1.2f scroll region %1.2f -> %1.2f\n", pToScroll->GetID(), y0, y1, pVerticalScrollBar->GetScrollWindowPosition(), pVerticalScrollBar->GetScrollWindowPosition() + pVerticalScrollBar->GetScrollWindowSize() );

			if ( y1 > pVerticalScrollBar->GetScrollWindowPosition() + pVerticalScrollBar->GetScrollWindowSize() && y0 <= pVerticalScrollBar->GetScrollWindowPosition()
				&& pVerticalScrollBar->GetScrollWindowPosition() + pVerticalScrollBar->GetScrollWindowSize() < pVerticalScrollBar->GetRangeMax() )
			{
				pToScroll->ScrollVertically( 100.0f );
				return true;
			}

			break;
		}

		x0 += pToScroll->UIPanel()->GetActualXOffset();
		x1 += pToScroll->UIPanel()->GetActualXOffset();
		y0 += pToScroll->UIPanel()->GetActualYOffset();
		y1 += pToScroll->UIPanel()->GetActualYOffset();
		
		pToScroll = pToScroll->GetParent();
		if ( pToScroll )
		{
			// include padding
			pToScroll->AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

			x0 = x0-flLeft;
			x1 = x1-flLeft;
			y0 = y0-flTop;
			y1 = y1-flTop;
		}
	}

	if ( BSelectionPosVerticalBoundary() && nRepeats > 0 )
	{
		UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ) );
		return true;
	}

	// Any set onmoveup overrides normal navigation behavior
	if ( BIsPanelEventSet( k_symNavigateDownEvent ) )
	{
		DispatchPanelEvent( k_symNavigateDownEvent );
		UIPanel()->PlayFocusChangeSound( nRepeats, 0.5f );
		return true;
	}

	if ( GetParent() )
	{
		float x = GetSelectionPositionX();
		float y = GetSelectionPositionY();
		if ( GetParent()->SetFocusToNextPanel( nRepeats, k_ENextByYPosition, false, GetTabIndex(), x, y, x, y ) )
		{
			UIPanel()->PlayFocusChangeSound( nRepeats, 0.5f );
			return true;
		}		
	}
	else
	{
		if ( GetParentWindow() && !GetParentWindow()->BOnMoveEdge( k_ENextByYPosition ) )
		{
			UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ) );
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for move left
//-----------------------------------------------------------------------------
bool CPanel2D::OnMoveLeft( int nRepeats )
{
	static const CPanoramaSymbol k_symNavigateLeftEvent( "onmoveleft" );

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

	float x0 = flLeft;
	float x1 = m_pIUIPanel->GetActualLayoutWidth() + flRight;
	float y0 = flTop;
	float y1 = m_pIUIPanel->GetActualLayoutHeight() + flBottom;

	CPanel2D *pToScroll = this;
	while ( pToScroll )
	{
		IUIScrollBar *pHorizontalScrollBar = pToScroll->UIPanel()->GetHorizontalScrollBar();
		if ( pHorizontalScrollBar )
		{
			if ( x0 < pHorizontalScrollBar->GetScrollWindowPosition() && x1 > pHorizontalScrollBar->GetScrollWindowPosition()
				&& pHorizontalScrollBar->GetScrollWindowPosition() > 0 )
			{
				pToScroll->ScrollHorizontally( -100.0f );
				return true;
			}

			break;
		}

		x0 += pToScroll->UIPanel()->GetActualXOffset();
		x1 += pToScroll->UIPanel()->GetActualXOffset();
		y0 += pToScroll->UIPanel()->GetActualYOffset();
		y1 += pToScroll->UIPanel()->GetActualYOffset();

		pToScroll = pToScroll->GetParent();
		if ( pToScroll )
		{
			// include padding
			pToScroll->AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

			x0 = x0-flLeft;
			x1 = x1-flLeft;
			y0 = y0-flTop;
			y1 = y1-flTop;
		}
	}

	if ( BSelectionPosHorizontalBoundary() && nRepeats > 0 )
	{
		UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ) );
		return true;
	}

	// Any set onmoveup overrides normal navigation behavior
	if ( BIsPanelEventSet( k_symNavigateLeftEvent ) )
	{
		DispatchPanelEvent( k_symNavigateLeftEvent );
		UIPanel()->PlayFocusChangeSound( nRepeats, 0.5f );
		return true;
	}

	if ( GetParent() )
	{
		float x = GetSelectionPositionX();
		float y = GetSelectionPositionY();

		// Cache off the handle; SetFocusToNextPanel can blow it away potentially
		CPanelPtr< IUIPanel > hPrevFocus = UIPanel();
		if ( GetParent()->SetFocusToNextPanel( nRepeats, k_EPrevByXPosition, false, GetTabIndex(), x, y, x, y ) )
		{
			IUIPanel *pPrevPanel = hPrevFocus.Get();
			if ( pPrevPanel )
			{
				pPrevPanel->PlayFocusChangeSound( nRepeats, 0.2f );
			}

			return true;
		}
	}
	else
	{
		if ( GetParentWindow() && !GetParentWindow()->BOnMoveEdge( k_EPrevByXPosition ) )
		{
			UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ) );
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for move right
//-----------------------------------------------------------------------------
bool CPanel2D::OnMoveRight( int nRepeats )
{
	static const CPanoramaSymbol k_symNavigateRightEvent( "onmoveright" );
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

	float x0 = flLeft;
	float x1 = m_pIUIPanel->GetActualLayoutWidth() + flRight;
	float y0 = flTop;
	float y1 = m_pIUIPanel->GetActualLayoutHeight() + flBottom;

	CPanel2D *pToScroll = this;
	while ( pToScroll )
	{
		IUIScrollBar *pHorizontalScrollBar = pToScroll->UIPanel()->GetHorizontalScrollBar();
		if ( pHorizontalScrollBar )
		{
			if ( x1 > pHorizontalScrollBar->GetScrollWindowPosition() + pHorizontalScrollBar->GetScrollWindowSize() && x0 <= pHorizontalScrollBar->GetScrollWindowPosition()
				&& pHorizontalScrollBar->GetScrollWindowPosition() + pHorizontalScrollBar->GetScrollWindowSize() < pHorizontalScrollBar->GetRangeMax() )
			{
				pToScroll->ScrollHorizontally( 100.0f );
				return true;
			}

			break;
		}

		x0 += pToScroll->UIPanel()->GetActualXOffset();
		x1 += pToScroll->UIPanel()->GetActualXOffset();
		y0 += pToScroll->UIPanel()->GetActualYOffset();
		y1 += pToScroll->UIPanel()->GetActualYOffset();

		pToScroll = pToScroll->GetParent();
		if ( pToScroll )
		{
			// include padding
			pToScroll->AccessStyle()->GetContentInset( m_pIUIPanel->GetActualLayoutWidth(), m_pIUIPanel->GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );

			x0 = x0-flLeft;
			x1 = x1-flLeft;
			y0 = y0-flTop;
			y1 = y1-flTop;
		}
	}

	if ( BSelectionPosHorizontalBoundary() && nRepeats > 0 )
	{
		UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 4.0f, 0.0f, 1.0f ), 1.0f, 0.6f ), 0.8f );
		return true;
	}

	// Any set onmoveup overrides normal navigation behavior
	if ( BIsPanelEventSet( k_symNavigateRightEvent ) )
	{
		DispatchPanelEvent( k_symNavigateRightEvent );
		UIPanel()->PlayFocusChangeSound( nRepeats, 0.5f );
		return true;
	}

	if ( GetParent() )
	{
		float x = GetSelectionPositionX();
		float y = GetSelectionPositionY();

		// Cache off the handle; SetFocusToNextPanel can blow it away potentially
		CPanelPtr< IUIPanel > hPrevFocus = UIPanel();
		if ( GetParent()->SetFocusToNextPanel( nRepeats, k_ENextByXPosition, false, GetTabIndex(), x, y, x, y ) )
		{
			IUIPanel *pPrevPanel = hPrevFocus.Get();
			if ( pPrevPanel )
			{
				pPrevPanel->PlayFocusChangeSound( nRepeats, 0.8f );
			}

			return true;
		}
	}
	else
	{
		if ( GetParentWindow() && !GetParentWindow()->BOnMoveEdge( k_ENextByXPosition ) )
		{
			UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ) );
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler tab 
//-----------------------------------------------------------------------------
bool CPanel2D::OnTabForward( int nRepeats )
{
	static const CPanoramaSymbol k_symNavigateTabEvent( "ontabforward" );
	// Any set ontabforward overrides normal navigation behavior
	if ( BIsPanelEventSet( k_symNavigateTabEvent ) )
	{
		DispatchPanelEvent( k_symNavigateTabEvent );
		UIPanel()->PlayFocusChangeSound( nRepeats, 0.8f );
		return true;
	}

	if ( GetParent() )
	{
		float x = GetSelectionPositionX();
		float y = GetSelectionPositionY();
		if ( GetParent()->SetFocusToNextPanel( nRepeats, k_ENextInTabOrder, true, GetTabIndex(), x, y, x, y ) )
		{
			UIPanel()->PlayFocusChangeSound( nRepeats, 0.8f );

			return true;
		}
	}
	else
	{
		UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ), 0.8f );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Event handler for tab back
//-----------------------------------------------------------------------------
bool CPanel2D::OnTabBackward( int nRepeats )
{
	static const CPanoramaSymbol k_symNavigateTabbackEvent( "ontabbackward" );
	// Any set ontabforward overrides normal navigation behavior
	if ( BIsPanelEventSet( k_symNavigateTabbackEvent ) )
	{
		DispatchPanelEvent( k_symNavigateTabbackEvent );
		UIPanel()->PlayFocusChangeSound( nRepeats, 0.2f );
		return true;
	}

	if ( GetParent() )
	{
		float x = GetSelectionPositionX();
		float y = GetSelectionPositionY();
		if ( GetParent()->SetFocusToNextPanel( nRepeats, k_EPrevInTabOrder, true, GetTabIndex(), x, y, x, y ) )
		{
			UIPanel()->PlayFocusChangeSound( nRepeats, 0.2f );

			return true;
		}
	}
	else
	{
		UISoundSystem()->PlaySound( "focus_change_fail", UIPanel(), k_ESoundType_Effects, Lerp( clamp( nRepeats / 6.0f, 0.0f, 1.0f ), 1.0f, 0.4f ), 0.2f );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for paging in a direction
//-----------------------------------------------------------------------------
bool CPanel2D::OnPageDirection( IUIScrollBar *pScrollBar, bool bIncreasePosition )
{
	if ( !pScrollBar )
		return false;

	float flOffset = ( bIncreasePosition ? 1 : -1 ) * pScrollBar->GetScrollWindowSize();
	pScrollBar->SetScrollWindowPosition( pScrollBar->GetScrollWindowPosition() + flOffset );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handlers for paging in a direction
//-----------------------------------------------------------------------------
bool CPanel2D::OnPageUp()		{ return OnPageDirection( UIPanel()->GetVerticalScrollBar(), false ); }
bool CPanel2D::OnPageDown()		{ return OnPageDirection( UIPanel()->GetVerticalScrollBar(), true ); }
bool CPanel2D::OnPageLeft()		{ return OnPageDirection( UIPanel()->GetHorizontalScrollBar(), false ); }
bool CPanel2D::OnPageRight()	{ return OnPageDirection( UIPanel()->GetHorizontalScrollBar(), true ); }


//-----------------------------------------------------------------------------
// Purpose: Event handler for scroll to top
//-----------------------------------------------------------------------------
bool CPanel2D::EventScrollToTop( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( UIPanel()->GetVerticalScrollBar() )
	{
		ScrollToTop();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for scroll to bottom
//-----------------------------------------------------------------------------
bool CPanel2D::EventScrollToBottom( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( UIPanel()->GetVerticalScrollBar() )
	{
		ScrollToBottom();
		return true;
	}

	return false;
}



//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get a keypress
//-----------------------------------------------------------------------------
bool CPanel2D::OnKeyDown( const KeyData_t & code )
{
	AssertMsg( BAcceptsInput(), "Unexpected key press" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get a keyup
//-----------------------------------------------------------------------------
bool CPanel2D::OnKeyUp( const KeyData_t & code )
{
	AssertMsg( BAcceptsInput(), "Unexpected key up" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get a key typed
//-----------------------------------------------------------------------------
bool CPanel2D::OnKeyTyped( const KeyData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected key typed" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnGamePadDown( const GamePadData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnGamePadUp( const GamePadData_t & code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnGamePadAnalog( const GamePadData_t &code )
{
	IUIScrollBar *pVerticalScrollBar = UIPanel()->GetVerticalScrollBar();
	IUIScrollBar *pHorizontalScrollBar = UIPanel()->GetHorizontalScrollBar();

	AssertMsg( BAcceptsInput(), "Unexpected action" );
	if ( m_pIUIPanel->BEnableAnalogStickScrolling() && ( pVerticalScrollBar || pHorizontalScrollBar ) )
	{
		if ( code.m_GamePadCode == XK_STICK2_ANALOG )
		{
			float flDeadZone = UIInputEngine()->GetDeadZoneValue( code.m_GamePadCode ) * 0.5f;

			if ( pVerticalScrollBar && fabsf( code.m_fValueY ) > flDeadZone )
			{
				float flScrollValue = pVerticalScrollBar->GetScrollWindowPosition() - code.m_fValueY * 0.05f; 
				pVerticalScrollBar->SetScrollWindowPosition( clamp( flScrollValue, pVerticalScrollBar->GetRangeMin(), pVerticalScrollBar->GetRangeMax() - pVerticalScrollBar->GetScrollWindowSize() ) );
				pVerticalScrollBar->Normalize( false );
			}

			if ( pHorizontalScrollBar && fabsf( code.m_fValueX ) > flDeadZone )
			{
				float flScrollValue = pHorizontalScrollBar->GetScrollWindowPosition() - code.m_fValueX * 0.05f; 
				pHorizontalScrollBar->SetScrollWindowPosition( clamp( flScrollValue, pHorizontalScrollBar->GetRangeMin(), pHorizontalScrollBar->GetRangeMax() - pHorizontalScrollBar->GetScrollWindowSize() ) );
				pHorizontalScrollBar->Normalize( false );
			}

		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnMouseButtonDown( const MouseData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnMouseButtonUp( const MouseData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnMouseButtonDoubleClick( const MouseData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );
	static const CPanoramaSymbol k_symPropertyOnDblClick( "ondblclick" );
	return DispatchPanelEvent( k_symPropertyOnDblClick );
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnMouseButtonTripleClick( const MouseData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnMouseWheel( const MouseData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );

	// First try scrolling in the vertical direction. If that's not allowed or
	// available, then try scrolling in the horizontal direction.
	IUIScrollBar *pScrollBar = UIPanel()->GetVerticalScrollBar();
	float flSpeed = k_flScrollLineSizeInPixelsVertical;
	if ( !pScrollBar )
	{
		EOverflowValue eHorizontalOverflow, eVerticalOverflow;
		AccessStyle()->GetOverflow( eHorizontalOverflow, eVerticalOverflow );
		if ( eVerticalOverflow != k_EOverflowScroll )
		{
			pScrollBar = UIPanel()->GetHorizontalScrollBar();
			flSpeed = k_flScrollLineSizeInPixelsHorizontal;
		}
	}

	if ( pScrollBar )
	{
		float flCurrentPosition;
		if ( UIEngine()->GetCurrentFrameTime() - pScrollBar->GetLastScrollTime() < 0.2f )
		{
			flCurrentPosition = pScrollBar->GetScrollWindowPosition();
		}
		else
		{
			flCurrentPosition = pScrollBar->GetInterpolatedScrollWindowPosition();
		}

		if ( code.m_Delta > 0 && flCurrentPosition > 0.0f )
		{
			pScrollBar->SetScrollWindowPosition( flCurrentPosition - code.m_Delta*flSpeed );
			return true;
		}
		else if ( code.m_Delta < 0 && flCurrentPosition < pScrollBar->GetRangeMax() - pScrollBar->GetScrollWindowSize() )
		{
			pScrollBar->SetScrollWindowPosition( flCurrentPosition - code.m_Delta*flSpeed );
			return true;
		}

		// Bumper time so you don't accidentally scroll past the end too easy and bubble to the parent too fast
		if ( UIEngine()->GetCurrentFrameTime() - pScrollBar->GetLastScrollTime() < 0.6f || code.m_RepeatCount != 0 )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: by default we don't need to do anything
//-----------------------------------------------------------------------------
void CPanel2D::OnMouseMove( float flMouseX, float flMouseY )
{
	if ( GetParentWindow()->UIWindowInput()->BDragInProgress() )
	{
		CPanel2D *pScrollPanel = this;
		while ( pScrollPanel != nullptr )
		{
			if ( pScrollPanel->BCanScrollDown() || pScrollPanel->BCanScrollUp()
				|| pScrollPanel->BCanScrollLeft() || pScrollPanel->BCanScrollRight() )
				break;
			pScrollPanel = pScrollPanel->GetParent();
		}
		if ( pScrollPanel != nullptr )
		{
			float flDragZoneHorizontal = static_cast<float>( pScrollPanel->GetAttribute( "DragScrollZoneHorizontal", (uint32)20 ) );
			float flDragSpeedHorizontal = static_cast<float>( pScrollPanel->GetAttribute( "DragScrollSpeedHorizontal", (uint32)50 ) );
			float flDragZoneVertical = static_cast<float>( pScrollPanel->GetAttribute( "DragScrollZoneVertical", (uint32)20 ) );
			float flDragSpeedVertical = static_cast<float>( pScrollPanel->GetAttribute( "DragScrollSpeedVertical", (uint32)50 ) );

			float flOffsetX = 0.f; float flOffsetY = 0.f;
			GetPositionWithinAncestor( pScrollPanel, &flOffsetX, &flOffsetY );
			const float flScrollMouseX = flOffsetX + flMouseX;
			const float flScrollMouseY = flOffsetY + flMouseY;	
			if ( pScrollPanel->BCanScrollLeft() || pScrollPanel->BCanScrollRight() )
			{
				const float flWidth = pScrollPanel->GetActualLayoutWidth();

				if ( flScrollMouseX < flDragZoneHorizontal && pScrollPanel->BCanScrollLeft() )
				{
					pScrollPanel->ScrollHorizontally( -flDragSpeedHorizontal );
				}
				else if ( flScrollMouseX > ( flWidth - flDragZoneHorizontal ) && pScrollPanel->BCanScrollRight() )
				{
					pScrollPanel->ScrollHorizontally( +flDragSpeedHorizontal );
				}
			}

			if ( pScrollPanel->BCanScrollUp() || pScrollPanel->BCanScrollDown() )
			{
				const float flHeight = pScrollPanel->GetActualLayoutHeight();
				if ( flScrollMouseY < flDragZoneVertical && pScrollPanel->BCanScrollUp() )
				{
					pScrollPanel->ScrollVertically( -flDragSpeedVertical );
				}
				else if ( flScrollMouseY > ( flHeight - flDragZoneVertical ) && pScrollPanel->BCanScrollDown() )
				{
					pScrollPanel->ScrollVertically( +flDragSpeedVertical );
				}
			}
		}
	}
	// You always get these, even if you don't accept input, also always consider
	// handled, since they don't mean anything to the parent.

	//Msg( "Mouse pos relative to %s: %f,%f\n", m_strID.Access() ? m_strID.Access() : "null", flMouseX, flMouseY );
}


//-----------------------------------------------------------------------------
// Purpose: by default we shouldn't get an action
//-----------------------------------------------------------------------------
bool CPanel2D::OnClick( IUIPanel *pPanel, const MouseData_t &code )
{
	AssertMsg( BAcceptsInput(), "Unexpected action" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose:  Invalidates painting and tells the panel it must repaint next frame
//-----------------------------------------------------------------------------
void CPanel2D::SetRepaint( EPanelRepaint eRepaintNeeded )
{
	m_pIUIPanel->SetRepaint( eRepaintNeeded );
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for being told to set a new panel event
//-----------------------------------------------------------------------------
bool CPanel2D::EventSetPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName, const char *pchPanelEventAction )
{
	if ( !BParsePanelEvent( pchPanelEventName, pchPanelEventAction, m_pIUIPanel->GetJavaScriptContextParent() ) )
		Msg( "Failed to parse and set new panel event for %s=\"%s\"\n", pchPanelEventName, pchPanelEventAction );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for being told to set a clear a panel event
//-----------------------------------------------------------------------------
bool CPanel2D::EventClearPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName )
{
	m_pIUIPanel->ClearPanelEvents( pchPanelEventName );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for being told to dispatch a panel event
//-----------------------------------------------------------------------------
bool CPanel2D::EventDispatchPanelEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchPanelEventName )
{
	DispatchPanelEvent( pchPanelEventName );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for being told to fire an event conditional on class presence
//-----------------------------------------------------------------------------
bool CPanel2D::EventIfHasClassEvent( const CPanelPtr< IUIPanel > &pPanel, const char * pchClassName, IUIEvent * pEventToFire )
{
	CUtlVector< CUtlString > vecClassNames;
	V_SplitString( pchClassName, " ", vecClassNames, false );

	bool bResult = false;
	FOR_EACH_VEC( vecClassNames, i )
	{
		if ( BHasClass( vecClassNames[i].String() ) )
		{
			bResult = true;
			break;
		}
	}

	if ( bResult )
		UIEngine()->DispatchEvent( pEventToFire->Copy() );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for being told to fire an event conditional on class presence
//-----------------------------------------------------------------------------
bool CPanel2D::EventIfNotHasClassEvent( const CPanelPtr< IUIPanel > &pPanel, const char * pchClassName, IUIEvent * pEventToFire )
{
	CUtlVector< CUtlString > vecClassNames;
	V_SplitString( pchClassName, " ", vecClassNames );

	bool bResult = true;
	FOR_EACH_VEC( vecClassNames, i )
	{
		if ( BHasClass( vecClassNames[i].String() ) )
		{
			bResult = false;
			break;
		}
	}

	if ( bResult )
		UIEngine()->DispatchEvent( pEventToFire->Copy() );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Internal helper event handler for being told to fire an event conditional on whether a different panel has hover state
//-----------------------------------------------------------------------------
bool CPanel2D::EventIfHoverOverEventInternal( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent * pEventToFire, bool bFireIfHovered )
{
	bool bResult = !bFireIfHovered;
	if ( pchOtherPanelID && pchOtherPanelID[ 0 ] != '\0' )
	{
		IUIWindow *pWindow = GetParentWindow();
		if ( pWindow )
		{
			IUIWindowInput *pWindowInput = pWindow->UIWindowInput();
			if ( pWindowInput )
			{
				for ( IUIPanel *pHoverPanel = pWindowInput->GetMouseHover(); pHoverPanel != NULL; pHoverPanel = pHoverPanel->GetParent() )
				{
					if ( !pHoverPanel->BHasHoverStyle() )
						break;

					const char *pchID = pHoverPanel->GetID();
					if ( !pchID )
						continue;

					if ( V_strcmp( pchOtherPanelID, pchID ) != 0 )
						continue;

					bResult = bFireIfHovered;
					break;
				}
			}
		}
	}

	if ( bResult )
		UIEngine()->DispatchEvent( pEventToFire->Copy() );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for being told to fire an event conditional on whether a different panel has hover state
//-----------------------------------------------------------------------------
bool CPanel2D::EventIfHoverOtherEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent *pEventToFire )
{
	return EventIfHoverOverEventInternal( pPanel, pchOtherPanelID, pEventToFire, true );
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for being told to fire an event conditional on whether a different panel has hover state
//-----------------------------------------------------------------------------
bool CPanel2D::EventIfNotHoverOtherEvent( const CPanelPtr< IUIPanel > &pPanel, const char *pchOtherPanelID, IUIEvent *pEventToFire )
{
	return EventIfHoverOverEventInternal( pPanel, pchOtherPanelID, pEventToFire, false );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the activate event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnActivateEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnActivate( "onactivate" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnActivate, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the activate event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnActivateEvent( const char *pchEventString )
{
	static const CPanoramaSymbol k_symPropertyOnActivate( "onactivate" );
	if ( !BParsePanelEvent( k_symPropertyOnActivate, pchEventString, m_pIUIPanel->GetJavaScriptContextParent() ) )
		return;
		
	//LogLayoutParsingError( m_symLayoutFile, 0, CFmtStr1024( "**** SetOnActivateEvent for panel %s failed to parse target event from string '%s'.", m_strID.String(), pchEventString ).String() );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the load event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnLoadEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnLoad( "onload" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnLoad, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the on mouse activate event for this panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnMouseActivateEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnMouseActivate( "onmouseactivate" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnMouseActivate, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Clears any set on activate event
//-----------------------------------------------------------------------------
void CPanel2D::ClearOnActivateEvent()
{
	static const CPanoramaSymbol k_symPropertyOnActivate( "onactivate" );
	m_pIUIPanel->ClearPanelEvents( k_symPropertyOnActivate );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the context menu event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnContextMenuEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnContextMenu( "oncontextmenu" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnContextMenu, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the focus event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnFocusEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnFocus( "onfocus" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnFocus, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the cancel event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnCancelEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnCancel( "oncancel" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnCancel, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the mouseover event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnMouseOverEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnMouseOver( "onmouseover" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnMouseOver, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the mouseout event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnMouseOutEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnMouseOut( "onmouseout" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnMouseOut, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the double click event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnDblClickEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symPropertyOnDblClick( "ondblclick" );
	m_pIUIPanel->SetPanelEvent( k_symPropertyOnDblClick, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the double click event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnTabForwardEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symNavigateTabEvent( "ontabforward" );
	m_pIUIPanel->SetPanelEvent( k_symNavigateTabEvent, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the double click event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnTabBackwardEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symNavigateTabbackEvent( "ontabbackward" );
	m_pIUIPanel->SetPanelEvent( k_symNavigateTabbackEvent, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the onselect event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnSelectEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symSelectEvent( "onselect" );
	m_pIUIPanel->SetPanelEvent( k_symSelectEvent, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the ondeselect event for a panel
//-----------------------------------------------------------------------------
void CPanel2D::SetOnDeselectEvent( IUIEvent *pEvent )
{
	static const CPanoramaSymbol k_symDeselectEvent( "ondeselect" );
	m_pIUIPanel->SetPanelEvent( k_symDeselectEvent, pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: set the value of a dialog variable and update all strings listening for this key
//-----------------------------------------------------------------------------
void CPanel2D::SetDialogVariable( const char *pchKey, const char *pchValue )
{
	VPROF_BUDGET( "CPanel2D::SetDialogVariable - char *",  VPROF_BUDGETGROUP_TENFOOT );
	UILocalize()->SetDialogVariable( UIPanel(), pchKey, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: set the value of a dialog variable and update all strings listening for this key
//-----------------------------------------------------------------------------
void CPanel2D::SetDialogVariable( const char *pchKey, int iVal )
{
	VPROF_BUDGET( "CPanel2D::SetDialogVariable - int",  VPROF_BUDGETGROUP_TENFOOT );
	UILocalize()->SetDialogVariable( UIPanel(), pchKey, iVal );
}

void CPanel2D::SetDialogVariable( const char *pchKey, uint64 uVal64 )
{
	VPROF_BUDGET( "CPanel2D::SetDialogVariable - uint64", VPROF_BUDGETGROUP_TENFOOT );
	UILocalize()->SetDialogVariable( UIPanel(), pchKey, uVal64 );
}

//-----------------------------------------------------------------------------
// Purpose: set the value of a dialog variable and update all strings listening for this key
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
void CPanel2D::SetDialogVariable( const char *pchKey, CCurrencyAmount amount )
#else
void CPanel2D::SetDialogVariable( const char *pchKey, CAmount amount )
#endif
{
	VPROF_BUDGET( "CPanel2D::SetDialogVariable - money",  VPROF_BUDGETGROUP_TENFOOT );
	UILocalize()->SetDialogVariable( UIPanel(), pchKey, amount );
}


//-----------------------------------------------------------------------------
// Purpose: set the value of a dialog variable and update all strings listening for this key
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
void CPanel2D::SetDialogVariable( const char *pchKey, time_t timeVal )
{
	VPROF_BUDGET( "CPanel2D::SetDialogVariable - time",  VPROF_BUDGETGROUP_TENFOOT );
	UILocalize()->SetDialogVariable( UIPanel(), pchKey, timeVal );
}
#else
//-----------------------------------------------------------------------------
// Purpose: set the value of a dialog variable and update all strings listening for this key
//-----------------------------------------------------------------------------
void CPanel2D::SetDialogVariable( const char *pchKey, CRTime timeVal )
{
	VPROF_BUDGET( "CPanel2D::SetDialogVariable - time",  VPROF_BUDGETGROUP_TENFOOT );
	UILocalize()->SetDialogVariable( UIPanel(), pchKey, timeVal );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: set the value of a dialog variable and update all strings listening for this key
//-----------------------------------------------------------------------------
void CPanel2D::SetDialogVariable( const char *pchKey, const CUtlString &value )
{
	SetDialogVariable( pchKey, value.String() );
}

//-----------------------------------------------------------------------------
// Purpose: set the value of a dialog variable and update all strings listening for this key
//-----------------------------------------------------------------------------
void CPanel2D::SetDialogVariableLocString( const char *pchKey, const char *pchValue )
{
	CLocStringSafePointer pString = UILocalize()->PchFindToken( UIPanel(), pchValue, k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None, k_eStringEscapeStyle_None );
	if ( pString )
	{
		SetDialogVariable( pchKey, pString->String() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the tooltip for this panel
//-----------------------------------------------------------------------------
void CPanel2D::SetTooltip( CPanel2D *pPanel )
{
	bool bAlreadyRegistered = ( m_pTooltip.BPreviouslySet() );
	if ( bAlreadyRegistered && !pPanel )
		UnregisterEventHandler( ShowTooltip(), this, &CPanel2D::EventShowTooltip );
	
	CPanel2D *pOldTooltip = m_pTooltip.Get();
	SAFE_DELETE( pOldTooltip );
	m_pTooltip = pPanel;

	if ( !bAlreadyRegistered && pPanel )
		RegisterEventHandler( ShowTooltip(), this, &CPanel2D::EventShowTooltip );		
}


//-----------------------------------------------------------------------------
// Purpose: Called when we should show a tooltip
//-----------------------------------------------------------------------------
bool CPanel2D::EventShowTooltip( const CPanelPtr< IUIPanel > &pPanel )
{
	CPanel2D *pTooltipPanel = m_pTooltip.Get();
	if ( pTooltipPanel )
	{
		CTooltip *pTooltip = dynamic_cast< CTooltip * >( pTooltipPanel );
		if ( pTooltip )
		{
			pTooltip->CalculatePosition();
			pTooltip->SetTooltipVisible( true );
		}
		else
		{
			pTooltipPanel->SetVisible( true );
		}

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Hides our tooltip
//-----------------------------------------------------------------------------
void CPanel2D::HideTooltip()
{
	CPanel2D *pTooltipPanel = m_pTooltip.Get();
	if ( pTooltipPanel )
	{
		CTooltip *pTooltip = dynamic_cast< CTooltip * >( pTooltipPanel );
		if ( pTooltip )
		{
			pTooltip->SetTooltipVisible( false );
		}
		else
		{
			pTooltipPanel->SetVisible( false );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set background images for the panel
//-----------------------------------------------------------------------------
void CPanel2D::SetBackgroundImages( const CUtlVector< CBackgroundImageLayer * > &vecLayers )
{
	AccessStyleDirty()->SetBackgroundImages( vecLayers );
}


//-----------------------------------------------------------------------------
// Purpose: Get background images for the panel
//-----------------------------------------------------------------------------
CUtlVector< CBackgroundImageLayer * > *CPanel2D::GetBackgroundImages()
{
	return AccessStyle()->GetBackgroundImages();
}


//-----------------------------------------------------------------------------
// Purpose: Handles receiving an image loaded event
//-----------------------------------------------------------------------------
bool CPanel2D::EventImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage) 
{
	// if the image is one of our backgrounds, repaint
	if( AccessStyle()->BHasAnyStyleDataForProperty( CStylePropertyBackgroundImage::symbol ) )
	{
		CUtlVector< CBackgroundImageLayer * > *pvecLayers = AccessStyle()->GetBackgroundImages();
		if ( pvecLayers )
		{
			FOR_EACH_VEC( *pvecLayers, i )
			{
				CBackgroundImageLayer *pLayer = pvecLayers->Element( i );
				if ( pLayer->GetImage() == pImage )
				{
					SetRepaint( k_EPanelRepaintFull );
					break;
				}
			}
		}

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles receiving an image failed to load event
//-----------------------------------------------------------------------------
bool CPanel2D::EventImageFailedLoad( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage )
{
	// if the image is one of our backgrounds, repaint
	if ( pPanel.Get() == UIPanel() && AccessStyle()->BHasAnyStyleDataForProperty( CStylePropertyOpacityMask::symbol ) )
	{
		IImageSource *pOpacityMaskSrc = NULL;
		AccessStyle()->GetOpacityMaskImage( pOpacityMaskSrc, NULL );
		if ( pOpacityMaskSrc )
		{
			if ( pOpacityMaskSrc == pImage )
			{
				CFmtStr errStr( "Failed to load opacity mask for panel '%s'", GetID() );
				DispatchEvent( JSConsoleOutput(), this, this, errStr.String() );
				Msg( "%s\n", errStr.String() );
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if panel has any active transitions
//-----------------------------------------------------------------------------
bool CPanel2D::BHasAnyActiveTransitions()
{
	return AccessStyle()->BHasAnyTransition();
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the panel can be cloned
//-----------------------------------------------------------------------------
bool CPanel2D::IsClonable()
{
	if ( GetPanelType() != CPanel2D::GetPanelSymbol() )
		return false;

	return AreChildrenClonable();
}


//-----------------------------------------------------------------------------
// Purpose: Checks if this panel's children can be cloned
//-----------------------------------------------------------------------------
bool CPanel2D::AreChildrenClonable()
{
	for( int i = 0; i < GetChildCount(); ++i )
	{
		if ( !GetChild(i)->IsClonable() )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Clones the panel instance
//-----------------------------------------------------------------------------
CPanel2D *CPanel2D::Clone()
{
	if ( !IsClonable() )
	{
		AssertMsg( false, "Panel can't be cloned (panel type or child not clonable)" );
		return NULL;
	}

	CPanel2D *pPanel = new CPanel2D( GetParentWindow(), NULL );
	InitClonedPanel( pPanel );

	return pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Adds class specific data to clone a panel
//-----------------------------------------------------------------------------
void CPanel2D::InitClonedPanel( CPanel2D *pClone )
{
	m_pIUIPanel->InitClonedPanel( pClone->UIPanel() );

	CPanel2D *pTooltip = m_pTooltip.Get();
	if( pTooltip )
	{
		if( !pTooltip->IsClonable() )
		{
			AssertMsg( false, "Tooltip can't be cloned, skipping" );
		}
		else
			pClone->SetTooltip( pTooltip->Clone() );
	}

	// now that we have initialized this panel, create children
	for( int i = 0; i < GetChildCount(); ++i )
	{
		if( !GetChild(i)->IsClonable() )
		{
			AssertMsg( false, "Child can't be cloned, skipping" );
			continue;
		}

		CPanel2D *pChild = GetChild( i )->Clone();
		pChild->SetParent( pClone );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find all child panels which match the given class
//-----------------------------------------------------------------------------
void CPanel2D::FindChildrenWithClassTraverse( CPanoramaSymbol symClassName, /*out*/ CUtlVector<CPanel2D*> *pVecMatchingChildren )
{
	CUtlVector<IUIPanel*> vecFound;
	m_pIUIPanel->FindChildrenWithClassTraverse( symClassName, vecFound );
	FOR_EACH_VEC( vecFound, i )
	{
		pVecMatchingChildren->AddToTail( ToPanel2D( vecFound[i] ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Javascript version of loading a layout file from string
//-----------------------------------------------------------------------------
void CPanel2D::BJSLoadLayoutFromString( const v8::FunctionCallbackInfo<v8::Value> &args )
{
	if ( args.Length() != 3 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "BLoadLayoutFromString takes 3 arguments: panel to parent created panel to" ).ToLocalChecked() );
		return;
	}

	if ( !args[0]->IsString() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "BLoadLayoutFromString's first param must be a string" ).ToLocalChecked() );
		return;
	}

	if ( !args[1]->IsBoolean() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "BLoadLayoutFromString's second param must be a bool" ).ToLocalChecked() );
		return;
	}

	if ( !args[2]->IsBoolean() )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "BLoadLayoutFromString's third param must be a bool" ).ToLocalChecked() );
		return;
	}	

	v8::String::Utf8Value layout_string(v8::Isolate::GetCurrent(),  args[0] );
	const char *pchLayout = *layout_string;
	bool bOverrideExisting = args[1]->BooleanValue( v8::Isolate::GetCurrent() );
	bool bPartialLayout = args[2]->BooleanValue( v8::Isolate::GetCurrent() );

	if ( !m_pIUIPanel->BLoadLayoutFromString( pchLayout, bOverrideExisting, bPartialLayout ) )
	{
		args.GetReturnValue().Set( false );
		return;
	}

	IUIPanel *pContext = UIEngineInternal()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	if ( pContext && pContext != m_pIUIPanel )
		m_pIUIPanel->SetLayoutFilePathForJSCheck( pContext->GetLayoutFilePathForJSCheck() );
	
	args.GetReturnValue().Set( true );
}


//-----------------------------------------------------------------------------
// Purpose: Walk through immediate children calling the given function
//          until it returns false. Returns whether it bailed out early or not.
//-----------------------------------------------------------------------------
bool CPanel2D::IterateChildren( std::function< bool( CPanel2D *pChild ) > fn )
{
	int nChildCount = GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CPanel2D *pChild = GetChild( i );
		if ( !fn( pChild ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Walk through all children in the tree calling the given function
//          until it returns false. Returns whether it bailed out early or not.
//-----------------------------------------------------------------------------
bool CPanel2D::IterateChildrenTraverse( std::function< bool( CPanel2D *pChild ) > fn )
{
	int nChildCount = GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CPanel2D *pChild = GetChild( i );
		if ( !fn( pChild ) )
			return false;

		if ( !pChild->IterateChildrenTraverse( fn ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Call a javascript function from C++ code
//-----------------------------------------------------------------------------
// gurjeets - commenting out as not used, revisit if any code actually requires it
//v8::Handle< v8::Value > CPanel2D::CallPanelJSFunctionArgsCore( IUIPanel *pPanel, const char *pchFunctionName, int argc, v8::Handle< v8::Value > *argv )
//{
//	return UIEngine()->RunFunction( pPanel, pchFunctionName, argc, argv );
//}



#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CPanel2D::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	// Just make sure we call into UIPanel, it calls us back in ValidateClientPanel, this way we
	// ensure we always do each side of the validation once
	m_pIUIPanel->Validate( validator, pchName );
}

//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CPanel2D::ValidateClientPanel( CValidator &validator, const tchar *pchName ) 
{

}


//-----------------------------------------------------------------------------
// Purpose: Static validation
//-----------------------------------------------------------------------------
void CPanel2D::ValidateStatics( CValidator &validator, const char *pchName )
{
	ValidateObj( s_vecMatchingChildren );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Creates a top level panel that inherits the current javascript & layout context. For JS and CSS lookup, panel will act like it is
//			in that panel's layout file
//-----------------------------------------------------------------------------
void panorama::JSCreatePanelWithCurrentContext( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	// NOTE! Registration of this function disabled. See panorama::RegisterGlobalJSMethods in uiengineclient.cpp

	if ( args.Length() > 1 )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "JSCreatePanelWithCurrentContext takes 1 optional argument: panel to parent created panel to" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pContext = UIEngine()->GetPanelForJavaScriptContext( *(args.GetIsolate()->GetCurrentContext()) );
	if ( !pContext )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "JSCreatePanelWithCurrentContext was not called within a context" ).ToLocalChecked() );
		return;
	}

	IUIPanel *pParent = NULL;
	if ( args.Length() == 1 )
	{
		if ( args[0]->IsObject() )
		{
			v8::Local<v8::Object> obj = args[0]->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
			if ( obj->InternalFieldCount() == 1 )
			{
				v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() )->GetInternalField( 0 ) );
				IUIPanel *pWrappedPanel = (IUIPanel*)wrap->Value();
				if ( UIEngine()->IsValidPanelPointer( pWrappedPanel ) )
					pParent = pWrappedPanel;
			}
		}

		if ( !pParent )
		{
			args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "JSCreatePanelWithCurrentContext first param must be a panel to parent to" ).ToLocalChecked() );
			return;
		}
	}

	CPanel2D *pPanel = NULL;
	if ( pParent )
		pPanel = new CPanel2D( ToPanel2D( pParent ), NULL );
	else
		pPanel = new CPanel2D( pContext->GetParentWindow(), NULL );

	if ( !pPanel )
	{
		args.GetIsolate()->ThrowException( v8::String::NewFromUtf8( args.GetIsolate(), "Internal failure creating panel in panorama" ).ToLocalChecked() );
		return;
	}

	pPanel->SetPanelIntoContext( ToPanel2D( pContext ) );

	args.GetReturnValue().Set( v8::Local<v8::Object>::New( GetV8Isolate(), *(UIEngineInternal()->CreateV8PanelInstance( pPanel->UIPanel() )) ) );
}


//-----------------------------------------------------------------------------
// Purpose: Converts argument into a panel pointer or returns null
//-----------------------------------------------------------------------------
IUIPanel *panorama::GetPanelFromJSArgs( const v8::Local< v8::Value > &arg )
{
	if ( !arg->IsObject() )
		return NULL;

	v8::Local<v8::Object> obj = arg->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
	if ( obj->InternalFieldCount() == 1 )
	{
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() )->GetInternalField( 0 ) );
		IUIPanel *pWrappedPanel = (IUIPanel*)wrap->Value();
		if ( UIEngine()->IsValidPanelPointer( pWrappedPanel ) )
			return pWrappedPanel;
	}

	return NULL;
}

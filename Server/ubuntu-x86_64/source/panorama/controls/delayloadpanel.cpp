//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/delayloadpanel.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDelayLoadPanel, DelayLoadPanel );

DECLARE_PANORAMA_EVENT0( DelayLoadPanelUpdateLoaded );
DEFINE_PANORAMA_EVENT( DelayLoadPanelUpdateLoaded );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDelayLoadPanel::CDelayLoadPanel( CPanel2D *parent, const char * pchPanelID )
	: CPanel2D( parent, pchPanelID )
	, m_eScrollIntoViewBehavior( k_EDelayLoadBehavior_None )
	, m_eScrollOutOfViewBehavior( k_EDelayLoadBehavior_None )
	, m_eClassAddedBehavior( k_EDelayLoadBehavior_None )
	, m_eClassRemovedBehavior( k_EDelayLoadBehavior_None )
	, m_bLoaded( false )
{
	// By default, just remove and delete all children
	SetUnloadFunction( []( CDelayLoadPanel *pParent ) {
		pParent->RemoveAndDeleteChildren();
	} );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CDelayLoadPanel::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ScrolledIntoView(), &CDelayLoadPanel::EventScrolledIntoView );
		RegisterEventHandlerOnPanelType( ScrolledOutOfView(), &CDelayLoadPanel::EventScrolledOutOfView );
		RegisterEventHandlerOnPanelType( DelayLoadPanelUpdateLoaded(), &CDelayLoadPanel::EventUpdateLoaded );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDelayLoadPanel::~CDelayLoadPanel()
{
}


//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CDelayLoadPanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "SetLoadFunction", PANORAMA_DELEGATE( &CDelayLoadPanel::SetLoadFunctionJS ) );
	RegisterJSMethod( "SetUnloadFunction", PANORAMA_DELEGATE( &CDelayLoadPanel::SetUnloadFunctionJS ) );
	RegisterJSMethod( "ListenForScrollIntoView", PANORAMA_DELEGATE( &CDelayLoadPanel::ListenForScrollIntoView ) );
	RegisterJSMethod( "ListenForClassAdded", PANORAMA_DELEGATE( &CDelayLoadPanel::ListenForClassAdded ) );
	RegisterJSMethod( "ListenForClassRemoved", PANORAMA_DELEGATE( &CDelayLoadPanel::ListenForClassRemoved ) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the function to be called when loading the contents
//-----------------------------------------------------------------------------
void CDelayLoadPanel::SetLoadFunction( std::function< void( CDelayLoadPanel *pParent ) > fnLoad )
{
	m_fnLoad.Reset();
	m_fnLoad.m_cppHandler = fnLoad;
}

void CDelayLoadPanel::SetLoadFunctionJS( v8::Persistent<v8::Function> *pJSLoadFunc )
{
	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngine()->GetV8Isolate(), *pJSLoadFunc );

	m_fnLoad.Reset();
	m_fnLoad.m_jsHandler.Reset( UIEngine()->GetV8Isolate(), fnLocal );
	m_fnLoad.m_pContextPanel = GetJavaScriptContextParent();
}

//-----------------------------------------------------------------------------
// Purpose: Set the function to be called when unloading the contents
//-----------------------------------------------------------------------------
void CDelayLoadPanel::SetUnloadFunction( std::function< void( CDelayLoadPanel *pParent ) > fnUnload )
{
	m_fnUnload.Reset();
	m_fnUnload.m_cppHandler = fnUnload;
}

void CDelayLoadPanel::SetUnloadFunctionJS( v8::Persistent<v8::Function> *pJSUnloadFunc )
{
	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngine()->GetV8Isolate(), *pJSUnloadFunc );

	m_fnUnload.Reset();
	m_fnUnload.m_jsHandler.Reset( UIEngine()->GetV8Isolate(), fnLocal );
	m_fnUnload.m_pContextPanel = GetJavaScriptContextParent();
}

//-----------------------------------------------------------------------------
// Purpose: Set the behavior for when this panel scrolls into view.
//-----------------------------------------------------------------------------
void CDelayLoadPanel::SetScrollIntoViewBehavior( EDelayLoadBehavior eScrollIntoViewBehavior, EDelayLoadBehavior eScrollOutOfViewBehavior )
{
	m_eScrollIntoViewBehavior = eScrollIntoViewBehavior;
	m_eScrollOutOfViewBehavior = eScrollOutOfViewBehavior;

	UpdateLoaded();
}

//-----------------------------------------------------------------------------
// Purpose: Set the behavior for when a class is added/removed
//-----------------------------------------------------------------------------
void CDelayLoadPanel::SetClassChangeBehavior( CPanoramaSymbol symClass, EDelayLoadBehavior eClassAddedBehavior, EDelayLoadBehavior eClassRemovedBehavior )
{
	m_symClassChange = symClass;
	m_eClassAddedBehavior = eClassAddedBehavior;
	m_eClassRemovedBehavior = eClassRemovedBehavior;

	UpdateLoaded();
}

//-----------------------------------------------------------------------------
// Purpose: Load/Unload the contents of this panel
//-----------------------------------------------------------------------------
void CDelayLoadPanel::SetLoaded( bool bLoaded )
{
	if ( bLoaded == m_bLoaded )
		return;

	if ( bLoaded )
	{
		m_fnLoad.Invoke( this );
	}
	else
	{
		m_fnUnload.Invoke( this );
	}

	m_bLoaded = bLoaded;
}


//-----------------------------------------------------------------------------
// Purpose: If the panel is loaded, call the load function again to force a
// reload of the contents
//-----------------------------------------------------------------------------
void CDelayLoadPanel::ForceReload()
{
	if ( !m_bLoaded )
		return;

	m_fnLoad.Invoke( this );
}


//-----------------------------------------------------------------------------
// Purpose: Given the curent state of the panel, determine whether it should be loaded or not.
//-----------------------------------------------------------------------------
void CDelayLoadPanel::UpdateLoaded()
{
	bool bShouldLoad = false;

	if ( m_eScrollIntoViewBehavior != k_EDelayLoadBehavior_None || m_eScrollOutOfViewBehavior != k_EDelayLoadBehavior_None )
	{
		bool bScrolledIntoView = IsScrolledIntoView();
		bShouldLoad = ( bScrolledIntoView && m_eScrollIntoViewBehavior == k_EDelayLoadBehavior_Load ) ||
			( !bScrolledIntoView && m_eScrollOutOfViewBehavior == k_EDelayLoadBehavior_Load );
	}

	if ( !bShouldLoad && m_symClassChange.IsValid() )
	{
		bool bHasClass = BHasClass( m_symClassChange );
		bShouldLoad = ( bHasClass && m_eClassAddedBehavior == k_EDelayLoadBehavior_Load ) ||
			( !bHasClass && m_eClassRemovedBehavior == k_EDelayLoadBehavior_Load );
	}

	SetLoaded( bShouldLoad );
}

//-----------------------------------------------------------------------------
// Purpose: Handle scrolling into view
//-----------------------------------------------------------------------------
bool CDelayLoadPanel::EventScrolledIntoView( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( m_eScrollIntoViewBehavior == k_EDelayLoadBehavior_None )
		return false;

	if ( pPanel.Get() != UIPanel() )
		return false;

	UpdateLoaded();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handle scrolling out of view
//-----------------------------------------------------------------------------
bool CDelayLoadPanel::EventScrolledOutOfView( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( m_eScrollIntoViewBehavior == k_EDelayLoadBehavior_None )
		return false;

	if ( pPanel.Get() != UIPanel() )
		return false;

	UpdateLoaded();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Listen for styles changing on this panel
//-----------------------------------------------------------------------------
void CDelayLoadPanel::OnStylesChanged()
{
	BaseClass::OnStylesChanged();

	if ( !m_symClassChange.IsValid() )
		return;

	DispatchEventAsync( 0.0f, DelayLoadPanelUpdateLoaded(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Handle async calling of UpdateLoaded
//-----------------------------------------------------------------------------
bool CDelayLoadPanel::EventUpdateLoaded()
{
	UpdateLoaded();
	return true;
}
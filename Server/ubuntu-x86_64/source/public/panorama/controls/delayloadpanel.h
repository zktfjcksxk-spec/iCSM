//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef DELAY_LOAD_PANEL_H
#define DELAY_LOAD_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include <functional>

namespace panorama
{

enum EDelayLoadBehavior
{
	k_EDelayLoadBehavior_None,
	k_EDelayLoadBehavior_Load,
	k_EDelayLoadBehavior_Unload,
};

//-----------------------------------------------------------------------------
// Purpose: Panel that loads its contents through a lambda function on demand
//-----------------------------------------------------------------------------
class CDelayLoadPanel : public CPanel2D
{
	DECLARE_PANEL2D( CDelayLoadPanel, CPanel2D );

public:
	CDelayLoadPanel( CPanel2D *parent, const char * pchPanelID );
	virtual ~CDelayLoadPanel();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// Sets the function that will be called when the panel is loaded or unloaded.
	// WARNING: be very careful about what variables you capture into a lambda. It will
	// be called back asynchronously, so you definitely don't want to capture anything
	// on the stack by reference.
	//
	// The default unload function simply does RemoveAndDeleteChildren().
	void SetLoadFunction( std::function< void( CDelayLoadPanel *pParent ) > fnLoad );
	void SetUnloadFunction( std::function< void( CDelayLoadPanel *pParent ) > fnUnload );
	void SetLoadFunctionJS( v8::Persistent<v8::Function> *pJSLoadFunc );
	void SetUnloadFunctionJS( v8::Persistent<v8::Function> *pJSUnloadFunc );

	// Manually load/unload the content
	bool IsLoaded() const { return m_bLoaded; }
	void SetLoaded( bool bLoaded );

	// If the panel is loaded, call the load function again to force a reload of the contents.
	// Note that this does not first call the unload function, so the load function needs to
	// be structured so that it can update in-place.
	void ForceReload();

	// Use this if you want the panel to automatically load/unload when it scrolls into/out of view.
	// Make sure to call SetSendChildScrolledIntoViewEvents( true ) on the parent.
	void SetScrollIntoViewBehavior( EDelayLoadBehavior eScrollIntoViewBehavior, EDelayLoadBehavior eScrollOutOfViewBehavior );
	void ListenForScrollIntoView() { SetScrollIntoViewBehavior( k_EDelayLoadBehavior_Load, k_EDelayLoadBehavior_Unload ); }

	// Use this if you want the panel to automaically load/unload when a specific class is added/removed
	void SetClassChangeBehavior( CPanoramaSymbol symClass, EDelayLoadBehavior eClassAddedBehavior, EDelayLoadBehavior eClassRemovedBehavior );
	void ListenForClassAdded( CPanoramaSymbol symClass ) { SetClassChangeBehavior( symClass, k_EDelayLoadBehavior_Load, k_EDelayLoadBehavior_Unload ); }
	void ListenForClassRemoved( CPanoramaSymbol symClass ) { SetClassChangeBehavior( symClass, k_EDelayLoadBehavior_Unload, k_EDelayLoadBehavior_Load ); }

protected:
	virtual void OnStylesChanged() OVERRIDE;

private:
	bool EventScrolledIntoView( const CPanelPtr< IUIPanel > &pPanel );
	bool EventScrolledOutOfView( const CPanelPtr< IUIPanel > &pPanel );
	bool EventUpdateLoaded();

	void UpdateLoaded();

	struct Callback_t
	{
		void Reset()
		{
			m_cppHandler = nullptr;
			m_jsHandler.Reset();
			m_pContextPanel.Clear();
		}

		void Invoke( CDelayLoadPanel *pParent )
		{
			if ( m_cppHandler )
			{
				m_cppHandler( pParent );
			}
			if ( !m_jsHandler.IsEmpty() && m_pContextPanel )
			{
				v8::Isolate::Scope isolate_scope( UIEngine()->GetV8Isolate() );
				v8::HandleScope handle_scope( UIEngine()->GetV8Isolate() );

				v8::Handle<v8::Value> arguments[1];
				PanoramaTypeToV8Param( pParent, &arguments[0] );

				UIEngine()->RunFunction( m_pContextPanel.Get(), &m_jsHandler, 1, arguments, false );
			}
		}
		
		std::function< void( CDelayLoadPanel *pParent ) > m_cppHandler;

		v8::Persistent< v8::Function > m_jsHandler;
		CPanelPtr< IUIPanel > m_pContextPanel;
	};

	Callback_t m_fnLoad;
	Callback_t m_fnUnload;

	EDelayLoadBehavior m_eScrollIntoViewBehavior;
	EDelayLoadBehavior m_eScrollOutOfViewBehavior;

	CPanoramaSymbol m_symClassChange;
	EDelayLoadBehavior m_eClassAddedBehavior;
	EDelayLoadBehavior m_eClassRemovedBehavior;

	bool m_bLoaded;
};


} // namespace panorama

#endif // DELAY_LOAD_PANEL_H

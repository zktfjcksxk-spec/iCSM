//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "panorama/iuiengine.h"
#include "panorama/localization/ilocalize.h"
#include "csgo_dialog_variable_handlers.h"
#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//-----------------------------------------------------------------------------

static CCSGODialogVariableHandlers s_CSGODialogVariableHandlers;

/*static*/ CCSGODialogVariableHandlers &CCSGODialogVariableHandlers::Get()
{
	return s_CSGODialogVariableHandlers;
}

//-----------------------------------------------------------------------------

CCSGODialogVariableHandlers::CCSGODialogVariableHandlers()
	: CAutoGameSystem("CCSGODialogVariableHandlers")
	, m_bInitialized( false )
	, m_mapKeyBindings()
	, m_ButtonRefs()
{
	ClearBindingInfo();
}

CCSGODialogVariableHandlers::~CCSGODialogVariableHandlers()
{
}

//-----------------------------------------------------------------------------

void CCSGODialogVariableHandlers::RegisterDialogVariableHandlers()
{
	UILocalize()->InstallCustomDialogVariableHandler( "csgo_key", &CCSGODialogVariableHandlers::DialogVar_csgo_key );
	UILocalize()->InstallCustomDialogVariableHandler( "csgo_bind", &CCSGODialogVariableHandlers::DialogVar_csgo_bind, &CCSGODialogVariableHandlers::ParseModifiers_csgo_bind, this, true );
}

void CCSGODialogVariableHandlers::UnregisterDialogVariableHandlers()
{
	UILocalize()->RemoveCustomDialogVariableHandler( "csgo_key" );
	UILocalize()->RemoveCustomDialogVariableHandler( "csgo_bind" );
}

//-----------------------------------------------------------------------------

bool CCSGODialogVariableHandlers::Init()
{
	if ( !CAutoGameSystem::Init() )
		return false;

	m_bInitialized = true;
	ClearBindingInfo();
	return true;
}

void CCSGODialogVariableHandlers::Shutdown()
{
	g_pKeyBindingListenerMgr->RemoveListener( this );
	ClearBindingInfo();
	m_bInitialized = false;

	CAutoGameSystem::Shutdown();
}

void CCSGODialogVariableHandlers::ClearBindingInfo()
{
	m_mapKeyBindings.Purge();
	m_DirtyBindings.Purge();

	for ( int i = 0; i < V_ARRAYSIZE( m_ButtonRefs ); ++i )
	{
		m_ButtonRefs[i].m_bInitialized = false;
		m_ButtonRefs[i].m_iDictBinds.Purge();
		m_ButtonRefs[i].m_szKeyName.Set( nullptr );
	}
}

//-----------------------------------------------------------------------------

// Generic dialog variable lookup for {g:csgo_key:*}
/*static*/ bool CCSGODialogVariableHandlers::DialogVar_csgo_key( CUtlString& /*out*/ strResult, const CUtlString &sStringValue, int nIntValue, const IUIPanel *pPanel, const char *pszKey, uint32 fModifiers, void *pUserData )
{
	const char *pchValue = sStringValue.String();
	wchar_t wszKey[128];
	V_UTF8ToUnicode( pchValue, wszKey, sizeof( wszKey ) );

	wchar_t wszKeyBinding[128];
	UTIL_ReplaceKeyBindings( wszKey, 0, wszKeyBinding, sizeof( wszKeyBinding ) );

	static char s_pchKeyBinding[128];
	V_UnicodeToUTF8( wszKeyBinding, s_pchKeyBinding, sizeof( s_pchKeyBinding ) );

	strResult = s_pchKeyBinding;
	return true;
}

// Virtual dialog variable lookup for {v:csgo_bind:bind_*}
/*static*/ bool CCSGODialogVariableHandlers::DialogVar_csgo_bind( CUtlString& strResult, const CUtlString &sStringValue, int nIntValue, const IUIPanel *pPanel, const char *pszKey, uint32 fModifiers, void *pUserData )
{
	return static_cast< CCSGODialogVariableHandlers* >( pUserData )->DialogVar_csgo_bind_impl( strResult, sStringValue, nIntValue, pPanel, pszKey, fModifiers );
}

/*static*/ uint32 CCSGODialogVariableHandlers::ParseModifiers_csgo_bind( const char *pszModifiers, void *pUserData )
{
	uint32 result = 0;

	if ( strchr( pszModifiers, 'e' ) )
	{
		result |= k_ECsgoBindVarFlag_EmptyIfUnbound;
	}

	return result;
}


bool CCSGODialogVariableHandlers::DialogVar_csgo_bind_impl( CUtlString& strResult, const CUtlString &sStringValue, int nIntValue, const IUIPanel *pPanel, const char *pszVarName, uint32 fModifiers )
{
	if ( !m_bInitialized )
		return false;

	const char* szBind = StringAfterPrefix( pszVarName, "bind_" );
	if ( !szBind || !szBind[0] )
		return false; // couldn't find binding

	if ( *szBind == '+' )
	{
		AssertMsg( false, "hot updating of this dialog variable will not work properly; remove + from bind name" );
		++szBind;
	}

	const char* szDefaultValue = szBind;
	if ( fModifiers & k_ECsgoBindVarFlag_EmptyIfUnbound )
		szDefaultValue = "";

	int iBind = m_mapKeyBindings.Find( szBind );
	if ( iBind == m_mapKeyBindings.InvalidIndex() )
	{
		// First request for this binding.  Make sure we are listening for it!
		iBind = m_mapKeyBindings.Insert( szBind, BUTTON_CODE_INVALID );
		g_pKeyBindingListenerMgr->AddListenerForBinding( this, szBind );
		UpdateSingleBindingInfo( iBind, ButtonCode_t( engine->Key_CodeForBinding( szBind ) ) );
	}

	Assert( iBind != m_mapKeyBindings.InvalidIndex() );
	if ( iBind == m_mapKeyBindings.InvalidIndex() )
	{
		strResult = szDefaultValue;
		return true;
	}

	ButtonCode_t code = m_mapKeyBindings.Element( iBind );
	if ( code == BUTTON_CODE_INVALID )
	{
		strResult = szDefaultValue;
		return true;
	}

	if ( code < 0 || code >= V_ARRAYSIZE( m_ButtonRefs ) )
	{
		Assert( false );
		strResult = szDefaultValue;
		return true;
	}

	strResult = m_ButtonRefs[code].m_szKeyName;
	return true;
}

//////////////////////////////////////////////////////////////////////////
// UpdateSingleBindingInfo.
// 
// This is basically m_mapBindings[idxBind] = code
//
// However, m_ButtonRefs has, for each button code, a list of the map
// entries that currently refer to that button.  So when one of these
// changes, it's important to keep the back-and-forth references in sync.
// We centralize changes here to maintain this invariant.
//
// In addition, this is where we lazy-init the name for a given button
// and start listening to any changes made to its bindings (in case
// the user unbinds it or re-binds it to something else later)
//
// The return value is true iff a change was made to that binding,
// which means that any existing dialog variables that refer to it are
// dirty and need to be updated.
bool CCSGODialogVariableHandlers::UpdateSingleBindingInfo( int idxBind, ButtonCode_t code )
{
	if ( code < 0 || code >= V_ARRAYSIZE( m_ButtonRefs ) )
		code = BUTTON_CODE_INVALID;
		
	ButtonCode_t* pBindingCode = &m_mapKeyBindings.Element( idxBind );
	ButtonCode_t oldCode = *pBindingCode;
	if ( oldCode == code )
		return false;

	if ( oldCode != BUTTON_CODE_INVALID )
	{
		// need to remove binding from existing key
		Assert( oldCode >= 0 && oldCode < V_ARRAYSIZE( m_ButtonRefs ) );
		m_ButtonRefs[oldCode].m_iDictBinds.FindAndFastRemove( idxBind );
	}

	*pBindingCode = code;

	if ( code != BUTTON_CODE_INVALID )
	{
		if ( !m_ButtonRefs[code].m_bInitialized )
		{
			// First access to this button.  Initialize.
			m_ButtonRefs[code].m_bInitialized = true;

			// Get name of button
			CUtlString str_tmp = g_pInputSystem->ButtonCodeToString( code );
			str_tmp.ToUpper();
			const CUtlString& str_name = str_tmp; // to call const Get()

			m_ButtonRefs[code].m_szKeyName.Set( str_name.Get() );

			// Listen to future changes to what this button is bound to
			g_pKeyBindingListenerMgr->AddListenerForCode( this, code );
		}

		// note that this element maps to this button
		m_ButtonRefs[code].m_iDictBinds.AddToTail( idxBind );
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// OnKeyBindingChanged
//
// Called by clientdll KeyBindingListenerManager when the user changes
// bindings to a key we currently are referring to with dialog variables.
//
// We use this opportunity to inform UILocalize() of any changes to
// {v:csgo_bind:*} dialog variables.
void CCSGODialogVariableHandlers::OnKeyBindingChanged( int nSplitScreenSlot, ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding, char const *pchListenedBinding )
{
	// If we were listening to some existing bindings at this code, they might have been invalidated
	if ( buttonCode >= 0 && buttonCode < V_ARRAYSIZE( m_ButtonRefs ) && pchListenedBinding == nullptr )
	{
		KeyBindingInfo_t& info = m_ButtonRefs[buttonCode];

		// Remove invalid bindings that point at this button
		FOR_EACH_VEC( info.m_iDictBinds, i )
		{
			int idxKeyBind = info.m_iDictBinds[i];
			const char* szCmd = m_mapKeyBindings.GetElementName( idxKeyBind );

			if ( !UTIL_IsBindInBinding( szCmd, pchNewBinding ) )
				m_DirtyBindings.AddToTail( idxKeyBind );
		}

		// If we have any new bindings to point at this button, we'll update them when we get the message that they were added
	}

	if ( pchListenedBinding != nullptr )
	{
		int idxKeyBind = m_mapKeyBindings.Find( pchListenedBinding );

		// somehow got a notification for a binding that we aren't listening for
		Assert( idxKeyBind != m_mapKeyBindings.InvalidIndex() );

		if ( idxKeyBind != m_mapKeyBindings.InvalidIndex() )
			m_DirtyBindings.AddToTail( idxKeyBind );
	}

	// Clean out dirty list now that we aren't mutating our internal state
	while ( m_DirtyBindings.Count() != 0 )
	{
		int idxKeyBind = m_DirtyBindings.Tail();
		m_DirtyBindings.RemoveMultipleFromTail( 1 );

		const char* szCmd = m_mapKeyBindings.GetElementName( idxKeyBind );
		ButtonCode_t newCode = ButtonCode_t( engine->Key_CodeForBinding( szCmd ) );

		if ( UpdateSingleBindingInfo( idxKeyBind, newCode ) )
		{
			// binding changed
			UILocalize()->DirtyDialogVariable( nullptr, CFmtStr( "bind_%s", szCmd ) );
		}
	}
}


//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_DIALOG_VARIABLE_HANDLERS_H
#define CSGO_DIALOG_VARIABLE_HANDLERS_H

#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
#include "utlstring.h"
#include "keybindinglistener.h"

namespace panorama
{
	class IUIPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Modifier flags for csgo_bind dialog variable
//-----------------------------------------------------------------------------
enum ECsgoBindVarFlags : uint32 {
	k_ECsgoBindVarFlag_None = 0,

	k_ECsgoBindVarFlag_EmptyIfUnbound = 0x1,
};

DEFINE_ENUM_BITWISE_OPERATORS( ECsgoBindVarFlags );

//-----------------------------------------------------------------------------
// Purpose: Supplies the handlers for csgo-specific dialog variables
//-----------------------------------------------------------------------------
class CCSGODialogVariableHandlers : public CAutoGameSystem, public IKeyBindingListener
{
public:
	CCSGODialogVariableHandlers();
	virtual ~CCSGODialogVariableHandlers();

	// CAutoGameSystem
	virtual bool Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;

	// IKeyBindingListener
	virtual void OnKeyBindingChanged( int nSplitScreenSlot, ButtonCode_t buttonCode, char const *pchKeyName, char const *pchNewBinding, char const *pchListenedBinding ) OVERRIDE;

	// gameui::CreateGameSpecificUI() interface
	static CCSGODialogVariableHandlers &Get();
	void RegisterDialogVariableHandlers();
	void UnregisterDialogVariableHandlers();
	
	// csgo_key generic dialog variable handler
	static bool DialogVar_csgo_key( CUtlString& strResult, const CUtlString &sStringValue, int nIntValue, const panorama::IUIPanel *pPanel, const char *pszKey, uint32 fModifiers, void *pUserData );

	// csgo_bind virtual dialog variable handler
	static bool DialogVar_csgo_bind( CUtlString& strResult, const CUtlString &sStringValue, int nIntValue, const panorama::IUIPanel *pPanel, const char *pszKey, uint32 fModifiers, void *pUserData );
	bool DialogVar_csgo_bind_impl( CUtlString& strResult, const CUtlString &sStringValue, int nIntValue, const panorama::IUIPanel *pPanel, const char *pszVarName, uint32 fModifiers );
	static uint32 ParseModifiers_csgo_bind( const char *pszModifiers, void *pUserData );

private:
	void ClearBindingInfo(); // resets m_mapKeyBindings and m_ButtonRefs

	bool m_bInitialized;

	CUtlDict< ButtonCode_t > m_mapKeyBindings; // given a bind, what key is it bound to?

	struct KeyBindingInfo_t
	{
		bool m_bInitialized;			// have we looked up this key name yet
		CUtlConstString m_szKeyName;	// what is this key named
		CUtlVector<int> m_iDictBinds;	// what elements of m_mapKeyBindings currently refer to this key?
	};
	KeyBindingInfo_t m_ButtonRefs[::BUTTON_CODE_LAST];

	// m_mapKeyBindings.Element(idxBind) = code, but maintain invariants
	// returns whether an actual change was made
	bool UpdateSingleBindingInfo( int idxBind, ButtonCode_t code );

	// Temporary storage used during OnKeyBindingChanged()
	CUtlVector<int> m_DirtyBindings;
};

#endif //CSGO_DIALOG_VARIABLE_HANDLERS_H
//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_PANEL_DEBUG_H_
#define CSGO_PANEL_DEBUG_H_

#pragma once

#include "panorama/uimetaprogramming.h"

//////////////////////////////////////////////////////////////////////////
// Debug tools

struct CPanelDebugArg
{
	CUtlString mElementId;
	CUtlString mElement;
	CUtlString mJSFetch;
};

class CPanelDebugHelper
{
public:
	CUtlStringBuilder mXML;
	CUtlString mTestElementId;
	bool mbInitialized;

	CPanelDebugHelper();
	void Init();

	// create Delegate using PANORAMA_DELEGATE from panorama/uimetaprogramming.h
	template <typename Delegate>
	void AddDebugMethod( const char* name, Delegate del );

	template <typename Delegate>
	void AddCheckbox( const char* name, bool defaultState, Delegate del );

	void Finish();

	// helper, usually used by AddDebugMethod
	void BuildMethodXML( const char* name, const CUtlVector<CPanelDebugArg>& args );

	// helper, usually used by AddCheckbox. only <bool> here because we expect the method being called to be a bool only
	void BuildCheckboxXML( const char* name, bool defaultState, const std::tuple<bool>& );
};

//////////////////////////////////////////////////////////////////////////
// implementation details


CPanelDebugArg CreatePanelDebugArgument( const char* elementId, int index, panorama::TypeProxy<const char *> );
CPanelDebugArg CreatePanelDebugArgument( const char* elementId, int index, panorama::TypeProxy<int> );
CPanelDebugArg CreatePanelDebugArgument( const char* elementId, int index, panorama::TypeProxy<bool> );

template <typename... Args, int... Indices> void CreatePanelDebugArguments( CUtlVector<CPanelDebugArg>* pArgs, const char* elementId, const std::tuple<Args...>&, panorama::index_tuple<Indices...> indices )
{
	// For( Arg,Index in Args,Indices... )
	//    pArgs->AddToTail( CreatePanelDebugArgument(elementId, Index, TypeProxy<Arg>() ) );
	int ignored[] = { 0, pArgs->AddToTail( CreatePanelDebugArgument( elementId, Indices, panorama::TypeProxy<Args>() ) )... };
	( void )ignored;
}

template <typename... Args> void CreatePanelDebugArguments( CUtlVector<CPanelDebugArg>* pArgs, const char* elementId, const std::tuple<Args...>& tup )
{
	// For( index in range(sizeof...(Args)) )
	//    pArgs->AddToTail( CreatePanelDebugArgument( elementId, index, TypeProxy< Args[index] >() );
	CreatePanelDebugArguments( pArgs, elementId, tup, typename panorama::make_indexes<Args...>::type() );
}

template <typename Delegate>
inline void CPanelDebugHelper::AddDebugMethod( const char* name, Delegate del )
{
	CUtlVector<CPanelDebugArg> args;
	CreatePanelDebugArguments( &args, name, typename Delegate::Arguments() );
	BuildMethodXML( name, args );
}

template <typename Delegate>
inline void CPanelDebugHelper::AddCheckbox( const char* name, bool defaultState, Delegate del )
{
	// passing argument tuple here to a function that verifies the delegate takes only a bool
	BuildCheckboxXML( name, defaultState, typename Delegate::Arguments() );
}


#endif // CSGO_PANEL_DEBUG_H_


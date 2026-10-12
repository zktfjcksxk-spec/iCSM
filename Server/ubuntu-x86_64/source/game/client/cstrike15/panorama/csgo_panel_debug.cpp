//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "csgo_panel_debug.h"

CPanelDebugArg CreatePanelDebugArgument( const char* elementId, int index, panorama::TypeProxy<const char *> )
{
	CPanelDebugArg result;
	result.mElementId.Format( "%s_arg%d", elementId, index );
	result.mElement.Format( "<TextEntry id=\"%s\" allowrawplaceholder=\"true\" placeholder=\"string\" />", result.mElementId.Get() );
	result.mJSFetch.Format( "($('#%s').text)", result.mElementId.Get() );
	return result;
}

CPanelDebugArg CreatePanelDebugArgument( const char* elementId, int index, panorama::TypeProxy<int> )
{
	CPanelDebugArg result;
	result.mElementId.Format( "%s_arg%d", elementId, index );
	result.mElement.Format( "<TextEntry id=\"%s\" allowrawplaceholder=\"true\" placeholder=\"int\" />", result.mElementId.Get() );
	result.mJSFetch.Format( "(parseInt($('#%s').text))", result.mElementId.Get() );
	return result;
}

CPanelDebugArg CreatePanelDebugArgument( const char* elementId, int index, panorama::TypeProxy<bool> )
{
	CPanelDebugArg result;
	result.mElementId.Format( "%s_arg%d", elementId, index );
	result.mElement.Format( "<ToggleButton id=\"%s\" allowrawtext=\"true\" text=\"bool\" />", result.mElementId.Get() );
	result.mJSFetch.Format( "($('#%s').checked)", result.mElementId.Get() );
	return result;
}


CPanelDebugHelper::CPanelDebugHelper()
	: mXML()
	, mTestElementId( "PanelToTest" )
	, mbInitialized( false )
{
}

void CPanelDebugHelper::Init()
{
	mXML = "<Panel class=\"debug-tester-controls-list\">\n";
	mbInitialized = true;
}

void CPanelDebugHelper::BuildMethodXML( const char* name, const CUtlVector<CPanelDebugArg>& args )
{
	mXML.AppendFormat( "  <Panel class=\"debug-tester-control-arguments\">\n" );

	// Fill in the button + activation code
	mXML.AppendFormat( "    <Button onactivate=\"$('#%s').%s( ", mTestElementId.Get(), name );
	FOR_EACH_VEC( args, i )
	{
		const CPanelDebugArg& arg = args[i];
		if ( i != 0 )
			mXML.Append( ", " );
		mXML.Append( arg.mJSFetch.Get() );
	}
	mXML.AppendFormat( ")\">\n" );

	const char* buttonName = name;
	if ( StringHasPrefix( buttonName, "debug_" ) )
		buttonName += strlen( "debug_" );

	mXML.AppendFormat( "      <Label allowrawtext=\"true\" text=\"%s\" />\n", buttonName );
	mXML.AppendFormat( "    </Button>\n" );

	// Fill in the input fields
	FOR_EACH_VEC( args, i )
	{
		mXML.AppendFormat( "    %s\n", args[i].mElement.Get() );
	}

	mXML.AppendFormat( "  </Panel>\n" );
}

void CPanelDebugHelper::BuildCheckboxXML( const char* name, bool defaultState, const std::tuple<bool>& )
{
	mXML.AppendFormat( "  <Panel class=\"debug-tester-control-arguments\">\n" );

	const char* buttonName = name;
	if ( StringHasPrefix( buttonName, "debug_" ) )
		buttonName += strlen( "debug_" );

	// Fill in the button + activation code
	mXML.AppendFormat( "    <ToggleButton id=\"checkbox_%s\"\n", name );
	mXML.AppendFormat( "                  allowrawtext=\"true\"\n", buttonName );
	mXML.AppendFormat( "                  text=\"%s\"\n", buttonName );
	mXML.AppendFormat( "                  selected=\"%s\"\n", defaultState ? "true" : "false" );
	mXML.AppendFormat( "                  onactivate=\"$('#%s').%s( $('#checkbox_%s').checked )\"\n", mTestElementId.Get(), name, name );
	mXML.AppendFormat( "    />\n" );

	mXML.AppendFormat( "  </Panel>\n" );
}

void CPanelDebugHelper::Finish()
{
	mXML.Append( "</Panel>\n" );
}

//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "console.h"
#include "panorama/controls/label.h"
#include "panorama/controls/textentry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D( CConsole, ConsolePanel )
DEFINE_PANORAMA_EVENT( PostConsoleText );
DEFINE_PANORAMA_EVENT( PostSpewText );

static SpewOutputFunc_t s_SpewOutputFuncDft = NULL;				// Default spew output function
static CConsole *s_pConsolePanel = NULL;
ConVar s_convarMaxConsoleHistory( "@max_console_history", "100" );

//-----------------------------------------------------------------------------
// Purpose:	Stub for a call into the console
//-----------------------------------------------------------------------------
static SpewRetval_t SpewFuncStub( SpewType_t spewType, const char *pMsg )
{
	if ( ThreadInMainThread() )
		DispatchEvent( PostSpewText(), s_pConsolePanel, spewType, pMsg );
	else
		DispatchEventAsync( PostSpewText(), s_pConsolePanel, spewType, pMsg );

	return s_SpewOutputFuncDft( spewType, pMsg );
}



//-----------------------------------------------------------------------------
// Purpose: helper class to let us specialize input on the console input
//-----------------------------------------------------------------------------
namespace panorama
{
class CConsoleInput : public CTextEntry
{
	DECLARE_PANEL2D( CConsoleInput, CTextEntry );
public:
	CConsoleInput( CPanel2D *parent, const char * pchPanelID );
	~CConsoleInput();

	virtual bool OnKeyDown( const KeyData_t &code );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_CommandHistory );
		FOR_EACH_VEC( m_CommandHistory, i )
		{
			ValidateObj( m_CommandHistory[i] );
		}
		BaseClass::ValidateClientPanel( validator, pchName );
	}
#endif
private:
	CUtlVector<CUtlString> m_CommandHistory;
	int m_iCurrentCommand;
};
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CConsoleInput::CConsoleInput( CPanel2D *parent, const char * pchPanelID ) : CTextEntry( parent, pchPanelID )
{
	m_iCurrentCommand = 0;

}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CConsoleInput::~CConsoleInput()
{
}


//-----------------------------------------------------------------------------
// Purpose: handle key up and down for navigating console history and esc to clear
//-----------------------------------------------------------------------------
bool CConsoleInput::OnKeyDown( const KeyData_t &code )
{
	if ( code.m_KeyCode == KEY_UP )
	{
		if ( m_CommandHistory.Count() > 0 )
		{
			m_iCurrentCommand--;
			if ( m_iCurrentCommand < 0 )
				m_iCurrentCommand = m_CommandHistory.Count() - 1;
			SetText( m_CommandHistory[m_iCurrentCommand] );
		}
		return true;
	}
	else if ( code.m_KeyCode == KEY_DOWN )
	{
		if ( m_CommandHistory.Count() > 0 )
		{
			m_iCurrentCommand++;
			if ( m_iCurrentCommand >= m_CommandHistory.Count() )
				m_iCurrentCommand = 0;

			SetText( m_CommandHistory[m_iCurrentCommand] );
		}
		return true;
	}
	else if ( code.m_KeyCode == KEY_ENTER )
	{
		m_CommandHistory.AddToTail( PchGetText() );
		m_iCurrentCommand++;
		if ( m_CommandHistory.Count() > s_convarMaxConsoleHistory.GetInt() )
		{
			m_iCurrentCommand--;
			m_CommandHistory.Remove( 0 );
		}
		return BaseClass::OnKeyDown( code );
	}
	else if ( code.m_KeyCode == KEY_ESCAPE )
	{
		SetText( "" );
		return true;
	}
	else
		return BaseClass::OnKeyDown( code );
}


REGISTER_PANEL2D_FACTORY( CConsoleInput, ConsoleInput );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CConsole::CConsole( CTopLevelWindow *pWindow, const char *pchName ) : CPanel2D( pWindow, pchName )
{
	m_nMaxChars = k_nLocalizeMaxChars;
	DbgVerify( BLoadLayout( "file://{resources}/layout/consolepanel.xml" ) );
	m_pConsoleText = (CLabel*)FindChildInLayoutFile( "ConsoleText" );
	m_pConsoleInput = (CConsoleInput*)FindChildInLayoutFile( "ConsoleInput" );
	m_pConsoleText->SetMaxChars( k_eStringTruncationStyle_Front, m_nMaxChars );

	s_SpewOutputFuncDft = GetSpewOutputFunc();
	SpewOutputFunc( SpewFuncStub );

	CUtlLinkedList<CUtlString> &history = UIEngine()->GetConsoleHistory();
	FOR_EACH_LL( history, i )
	{
		AddText( history[i] );
	}

	CPanel2D *pPanel = FindChildInLayoutFile( "ConsoleInput" );
	pPanel->SetFocus();

	RegisterEventHandler( TextEntrySubmit(), this, &CConsole::OnTextEntrySubmit );
	RegisterEventHandler( PostConsoleText(), this, &CConsole::OnPostConsoleText );
	RegisterEventHandler( PostSpewText(), this, &CConsole::OnPostSpewText );
	s_pConsolePanel = this;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CConsole::~CConsole()
{
	SpewOutputFunc( s_SpewOutputFuncDft );
}


//-----------------------------------------------------------------------------
// Purpose: Parse properties
//-----------------------------------------------------------------------------
bool CConsole::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	if ( symName == "maxchars" )
	{
		m_nMaxChars = V_atoi( pchValue );
		return true;
	}
	else
		return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: someone hit enter in the submit dialog
//-----------------------------------------------------------------------------
bool CConsole::OnTextEntrySubmit( const CPanelPtr< IUIPanel > &pPanel, const char *pchText )
{
	if ( ToPanel2D(pPanel.Get()) == m_pConsoleInput )
		OnPostConsoleText();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: someone hit enter in the submit dialog
//-----------------------------------------------------------------------------
bool CConsole::OnPostConsoleText()
{
	const char *pchCommand = m_pConsoleInput->PchGetText();
	ExecConsoleCommand( pchCommand );
	m_pConsoleInput->SetText( "" );

	// Throw input back to the input as well
	m_pConsoleInput->SetFocus();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Log a line to the console
//-----------------------------------------------------------------------------
void CConsole::AddText( const char *pchText )
{
	if ( m_pConsoleText )
	{
		m_pConsoleText->AppendText( pchText );
		m_pConsoleText->ScrollToBottom();
	}
}


//-----------------------------------------------------------------------------
// Purpose: log a message from the logging system
//-----------------------------------------------------------------------------
bool CConsole::OnPostSpewText( SpewType_t spewType, const char *pMsg )
{
	AddText( pMsg );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: called when the enter key is pressed in the input entry
//-----------------------------------------------------------------------------
void CConsole::ExecConsoleCommand( const char *pchCommand )
{
	ConParams conParams;
	conParams.Set( pchCommand );
	const char * pchCmd = conParams.PchCmd();

	// if command is empty, nothing to do
	if ( !pchCmd || !pchCmd[0] )
		return;

	// see if we can handle it as a convar
	// walk all the con commands
	const ConCommandBase *pCmd = ConCommandBase::FindCommand( pchCmd );
	if ( !pCmd )
	{
		Msg( ">>> command not found: %s\n", pchCmd );
		return;
	}
	else if ( pCmd->IsBitSet( FCVAR_NONUSER ) )
	{
		Msg( "Non-user command %s received at console; ignoring\n", pCmd->GetName() );
		return;
	}

	if ( pCmd->IsCommand() )
	{
		(( ConCommand * )pCmd )->Dispatch( NULL, conParams );
	}
	else // must be a convar
	{	
		if( conParams.CArgs() == 0 ) // no args, print the value
		{
			// get value
			Msg( "\"%s\" = \"%s\"\n", (( ConVar * )pCmd )->GetName(), (( ConVar * )pCmd )->GetString() );
		}
		else
		{
			// set value
			char rgchValue[512] = {0};

			for ( int iArg = 1; iArg <= conParams.CArgs(); iArg++ )
			{
				V_strncat( rgchValue, conParams.PchArg( iArg ), sizeof(rgchValue) );
				if ( iArg != conParams.CArgs() )
				{
					V_strncat( rgchValue, " ", sizeof(rgchValue) );
				}
			}

			(( ConVar * )pCmd )->SetValue( rgchValue );
			Msg( "\"%s\" = \"%s\"\n", (( ConVar * )pCmd )->GetName(), (( ConVar * )pCmd )->GetString() );			
		}
	}
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CConsole::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	BaseClass::ValidateClientPanel( validator, pchName );
}
#endif

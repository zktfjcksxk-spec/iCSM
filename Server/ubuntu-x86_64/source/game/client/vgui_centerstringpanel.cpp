//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <stdarg.h>
#include "vguicenterprint.h"
#include "ivrenderview.h"
#include "vgui/IVGui.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/IPanel.h"
#include "gameui_interface.h"
#include "strtools.h"
#include "hudelement.h"

#ifdef INCLUDE_SCALEFORM
#include "Scaleform/HUD/sfhudinfopanel.h"
#endif // CSTRIKE15

#if defined ( PANORAMA_ENABLE )
#include "panorama/hud/csgo_hudhinttext.h"
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static bool ForwardPriorityMessageRaw( const wchar_t *msg, CCenterPrint::EPriority ePriority = CCenterPrint::k_EPriority_High )
{
	bool bForwarded = false;

#ifdef PANORAMA_ENABLE
	if ( GameUI().IsPanoramaEnabled() )
	{
		char szBuff[ CCSGO_HudHintText::k_eMaxHintSizeBytes ];
		V_UnicodeToUTF8( msg, szBuff, sizeof( szBuff ) );
		panorama::DispatchEvent( ShowCenterPrintText(), nullptr, szBuff, ePriority );

		bForwarded = true;
	}
#endif

#ifdef INCLUDE_SCALEFORM
	CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
	if ( pElement )
	{
		( ( SFHudInfoPanel * )pElement )->SetPriorityText( msg );
	}
	bForwarded = true;
#endif

	return bForwarded;
}

static bool ForwardPriorityMessage( const char* msg, CCenterPrint::EPriority ePriority = CCenterPrint::k_EPriority_High )
{
	if ( msg && g_BannedWords.BInitialized() )
	{
		int nLen = V_strlen( msg );
		int cubDestSizeInBytes = ( nLen + 1 ) * sizeof( wchar_t );
		wchar_t * pwchBuffer = ( wchar_t * ) stackalloc( cubDestSizeInBytes );
		V_UTF8ToUnicode( msg, pwchBuffer, cubDestSizeInBytes );
		if ( g_BannedWords.CensorBannedWordsInplace( pwchBuffer ) )
			return ForwardPriorityMessageRaw( pwchBuffer, ePriority );
	}

	bool bForwarded = false;

#ifdef PANORAMA_ENABLE
	if ( GameUI().IsPanoramaEnabled() )
	{
		panorama::DispatchEvent( ShowCenterPrintText(), nullptr, msg, ePriority );

		bForwarded = true;
	}
#endif

#ifdef INCLUDE_SCALEFORM
	CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
	if ( pElement )
	{
		( ( SFHudInfoPanel * )pElement )->SetPriorityText( msg );
	}
	bForwarded = true;
#endif
	
	return bForwarded;
}

static bool ForwardPriorityMessage( const wchar_t *msg, CCenterPrint::EPriority ePriority = CCenterPrint::k_EPriority_High )
{
	if ( msg && g_BannedWords.BInitialized() )
	{
		int nLen = Q_wcslen( msg );
		int cubDestSizeInBytes = ( nLen + 1 ) * sizeof( wchar_t );
		wchar_t* pwchBuffer = ( wchar_t* )stackalloc( nLen * sizeof( wchar_t ) );
		memcpy( pwchBuffer, msg, cubDestSizeInBytes );
		if ( g_BannedWords.CensorBannedWordsInplace( pwchBuffer ) )
			return ForwardPriorityMessageRaw( pwchBuffer );
	}

	return ForwardPriorityMessageRaw( msg, ePriority );
}

static bool ForwardHintMessage( const wchar_t* msg )
{
	bool bForwarded = false;

#ifdef PANORAMA_ENABLE
	if ( GameUI().IsPanoramaEnabled() )
	{
		char szBuff[ CCSGO_HudHintText::k_eMaxHintSizeBytes ];
		V_UnicodeToUTF8( msg, szBuff, sizeof( szBuff ) );
		panorama::DispatchEvent( ShowCenterPrintText(), nullptr, szBuff, CCenterPrint::k_EPriority_Low );


		bForwarded = true;;
	}
#endif

#ifdef INCLUDE_SCALEFORM
	CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
	if ( pElement )
	{
		( ( SFHudInfoPanel * )pElement )->SetPriorityHintText( msg );
	}
	bForwarded = true;
#endif

	return bForwarded;
}

#ifdef TF_CLIENT_DLL
static ConVar		scr_centertime( "scr_centertime", "5" );
#else
ConVar		scr_centertime( "scr_centertime", "4" );
#endif

//-----------------------------------------------------------------------------
// Purpose: Implements Center String printing
//-----------------------------------------------------------------------------
class CCenterStringLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( CCenterStringLabel, vgui::Label );

public:
	explicit 			CCenterStringLabel( vgui::VPANEL parent );
	virtual				~CCenterStringLabel( void );

	// vgui::Panel
	virtual void		ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void		OnTick( void );
	virtual bool		ShouldDraw( void );

	// CVGuiCenterPrint
	virtual void		SetTextColor( int r, int g, int b, int a );
	virtual void		Print( const char *text );
	virtual void		Print( const wchar_t *text );
	virtual void		ColorPrint( int r, int g, int b, int a, const char *text );
	virtual void		ColorPrint( int r, int g, int b, int a, const wchar_t *text );
	virtual void		Clear( void );

protected:
	MESSAGE_FUNC_INT_INT( OnScreenSizeChanged, "OnScreenSizeChanged", oldwide, oldtall );

private:
	void ComputeSize( void );

	vgui::HFont			m_hFont;

	float				m_flCentertimeOff;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CCenterStringLabel::CCenterStringLabel( vgui::VPANEL parent ) : 
	BaseClass( NULL, "CCenterStringLabel", " " )
{
	SetParent( parent );
	ComputeSize();
	SetVisible( false );
	SetCursor( 0 );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetContentAlignment( vgui::Label::a_center );
	SetScheme( "ClientScheme" );

	m_hFont = 0;
	SetFgColor( Color( 255, 255, 255, 255 ) );

	SetPaintBackgroundEnabled( false );

	m_flCentertimeOff = 0.0;

	vgui::ivgui()->AddTickSignal( GetVPanel(), 100 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCenterStringLabel::~CCenterStringLabel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Updates panel to handle the new screen size
//-----------------------------------------------------------------------------
void CCenterStringLabel::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	BaseClass::OnScreenSizeChanged(iOldWide, iOldTall);
	ComputeSize();
}

//-----------------------------------------------------------------------------
// Purpose: Computes panel's desired size and position
//-----------------------------------------------------------------------------
void CCenterStringLabel::ComputeSize( void )
{
	int w, h;
	if ( GetVParent() )
	{
		vgui::ipanel()->GetSize( GetVParent(), w, h );

		int iHeight = (int)(h * 0.3);
		SetSize( w, iHeight );
		SetPos( 0, ( h * 0.35 ) - ( iHeight / 2 ) );
	}
}

void CCenterStringLabel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// Use a large font
	m_hFont = pScheme->GetFont( "Default" );
	Assert( m_hFont );
	SetFont( m_hFont );

	ComputeSize();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void CCenterStringLabel::SetTextColor( int r, int g, int b, int a )
{
	SetFgColor( Color( r, g, b, a ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCenterStringLabel::Print( const char *text )
{
	SetText( text );
	
	m_flCentertimeOff = scr_centertime.GetFloat() + gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCenterStringLabel::Print( const wchar_t *text )
{
	SetText( text );
	
	m_flCentertimeOff = scr_centertime.GetFloat() + gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCenterStringLabel::ColorPrint( int r, int g, int b, int a, const char *text )
{
	SetTextColor( r, g, b, a );
	Print( text );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCenterStringLabel::ColorPrint( int r, int g, int b, int a, const wchar_t *text )
{
	SetTextColor( r, g, b, a );
	Print( text );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCenterStringLabel::Clear( void )
{
	m_flCentertimeOff = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCenterStringLabel::OnTick( void )
{
	bool bVisible = ShouldDraw();
	if ( IsVisible() != bVisible )
	{
		SetVisible( bVisible );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
// FIXME, this has dependencies on the engine that should go away
//-----------------------------------------------------------------------------
bool CCenterStringLabel::ShouldDraw( void )
{
	// [jason] This element only exists to forward center print messages to the Scaleform InfoPanel
#if defined ( CSTRIKE15 )
	return false;
#endif

	if ( engine->IsDrawingLoadingImage() )
	{
		return false;
	}

	if ( m_flCentertimeOff <= gpGlobals->curtime )
	{
		// not time to turn off the message yet
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
CCenterPrint::CCenterPrint( void )
{
	vguiCenterString = NULL;
}

void CCenterPrint::SetTextColor( int r, int g, int b, int a )
{
	if ( vguiCenterString )
	{
		vguiCenterString->SetTextColor( r, g, b, a );
	}
}

void CCenterPrint::Print( const char *text, EPriority ePriority /*= k_EPriority_High */ )
{
	if( ForwardPriorityMessage( text, ePriority ) )
		return;

	if ( vguiCenterString )
	{
		vguiCenterString->ColorPrint( 255, 255, 255, 255, text );
	}
}

void CCenterPrint::Print( const wchar_t *text, EPriority ePriority /*= k_EPriority_High */ )
{
	if( ForwardPriorityMessage( text, ePriority ) )
		return;

	if ( vguiCenterString )
	{
		vguiCenterString->ColorPrint( 255, 255, 255, 255, text );
	}
}

void CCenterPrint::ColorPrint( int r, int g, int b, int a, const char *text )
{
	if( ForwardPriorityMessage( text ) )
		return;

	if ( vguiCenterString )
	{
		vguiCenterString->ColorPrint( r, g, b, a, text );
	}
}

void CCenterPrint::ColorPrint( int r, int g, int b, int a, const wchar_t *text )
{
	if( ForwardPriorityMessage( text ) )
		return;

	if ( vguiCenterString )
	{
		vguiCenterString->ColorPrint( r, g, b, a, text );
	}
}

void CCenterPrint::Clear( void )
{
	if ( ForwardPriorityMessage( static_cast< wchar_t* >( NULL ) ) )
		return;

	if ( vguiCenterString )
	{
		vguiCenterString->Clear();
	}
}

void CCenterPrint::PrintHint( const wchar_t* text )
{
	if ( ForwardHintMessage( text ) )
		return;

	if ( vguiCenterString )
	{
		vguiCenterString->ColorPrint( 255, 255, 255, 255, text );
	}
}

void CCenterPrint::Create( vgui::VPANEL parent )
{
	if ( vguiCenterString )
	{
		Destroy();
	}

	vguiCenterString = new CCenterStringLabel( parent );
}

void CCenterPrint::Destroy( void )
{
	if ( vguiCenterString )
	{
		vguiCenterString->SetParent( (vgui::Panel *)NULL );
		delete vguiCenterString;
		vguiCenterString = NULL;
	}
}

static CCenterPrint g_CenterString[ MAX_SPLITSCREEN_PLAYERS ];
CCenterPrint *GetCenterPrint()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_CenterString[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

//========= Copyright � 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "c_vguiscreen.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/IVGui.h>
#include "ienginevgui.h"
#include "fmtstr.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui/ISurface.h>
#include "avi/ibik.h"
#include "engine/IEngineSound.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "c_world_vgui_text.h"
#include "vgui_controls/Label.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CVguiWorldTextPanel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CVguiWorldTextPanel, CVGuiScreenPanel );

public:
	CVguiWorldTextPanel( vgui::Panel *parent, const char *panelName );
	~CVguiWorldTextPanel( void );

	virtual void ApplySchemeSettings( IScheme *pScheme );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick( void );
	virtual void Paint( void );

private:
	bool	IsActive( void );

	inline void GetPanelPos( int &xpos, int &ypos )
	{
		xpos = ( (float) ( GetWide() - m_nPlaybackWidth ) / 2 );
		ypos = ( (float) ( GetTall() - m_nPlaybackHeight ) / 2 );
	}

private:

	// BINK playback info
	CHandle<C_VGuiScreen>	m_hVGUIScreen;
	CHandle<C_WorldVguiText>	m_hScreenEntity;
	vgui::Label *m_pTextLabel;

	int				m_nTextureId;
	int				m_nPlaybackHeight;		// Playback dimensions (proper ration adjustments)
	int				m_nPlaybackWidth;
	bool			m_bBlackBackground;
	bool			m_bInitialized;

	bool			m_bLastActiveState;		// HACK: I'd rather get a real callback...

	bool bIsAlreadyVisible;
};

DECLARE_VGUI_SCREEN_FACTORY( CVguiWorldTextPanel, "world_text_panel" );

CUtlVector <CVguiWorldTextPanel *>	g_WorldTextPanels;

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CVguiWorldTextPanel::CVguiWorldTextPanel( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, "CVguiWorldTextPanel" ) 
{
	m_pTextLabel = new vgui::Label( this, "TextDisplay", "" );

	m_bBlackBackground = false;
	m_bInitialized = false;

	// Add ourselves to the global list of movie displays
	g_WorldTextPanels.AddToTail( this );

	m_bLastActiveState = IsActive();
}

//-----------------------------------------------------------------------------
// Purpose: Clean up the movie
//-----------------------------------------------------------------------------
CVguiWorldTextPanel::~CVguiWorldTextPanel( void )
{
	
	// Remove ourselves from the global list of movie displays
	g_WorldTextPanels.FindAndRemove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Setup our scheme
//-----------------------------------------------------------------------------
void CVguiWorldTextPanel::ApplySchemeSettings( IScheme *pScheme )
{
	assert( pScheme );

	BaseClass::ApplySchemeSettings(pScheme);

	int wide, tall;
	this->GetSize( wide, tall );
	int nMaxWidth = wide;

	//m_pTextLabel->GetSize( wide, tall );
	nMaxWidth = m_hScreenEntity->GetTextPanelWidth();//MIN( nMaxWidth, m_hScreenEntity->GetTextPanelWidth() );	
	m_pTextLabel->SetFont( pScheme->GetFont( m_hScreenEntity->GetFont() ) );
	m_pTextLabel->SetFgColor( m_hScreenEntity->GetColor() );
	m_pTextLabel->SetContentAlignment(Label::a_north);
	m_pTextLabel->SetCenterWrap( true );
	m_pTextLabel->SetWrap( true );
	//m_pTextLabel->SetSize( nMaxWidth, tall );
	int left = (wide - nMaxWidth) / 2;
	m_pTextLabel->SetBounds(0, 0, nMaxWidth, tall);
	m_pTextLabel->SetPos(left, 0);
	m_pTextLabel->SetBgColor( Color(0,0,0,0) );
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CVguiWorldTextPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if ( !BaseClass::Init( pKeyValues, pInitData ) )
		return false;

	// Save this for simplicity later on
	m_hVGUIScreen = dynamic_cast<C_VGuiScreen *>( GetEntity() );
	if ( m_hVGUIScreen != NULL )
	{
		// Also get the associated entity
		m_hScreenEntity = dynamic_cast<C_WorldVguiText *>(m_hVGUIScreen->GetOwnerEntity());
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to check our active state
//-----------------------------------------------------------------------------
bool CVguiWorldTextPanel::IsActive( void )
{
	bool bScreenActive = false;
	if ( m_hVGUIScreen != NULL )
	{
		bScreenActive = m_hVGUIScreen->IsActive();
	}

	return bScreenActive;
}


//-----------------------------------------------------------------------------
// Update the display string
//-----------------------------------------------------------------------------
void CVguiWorldTextPanel::OnTick()
{
	BaseClass::OnTick(); 
}

//-----------------------------------------------------------------------------
// Purpose: Update and draw the frame
//-----------------------------------------------------------------------------
void CVguiWorldTextPanel::Paint( void )
{
	// Masters must keep the video updated
	//if ( m_bSlaved == false && m_BIKHandle == BIKHANDLE_INVALID )
	//{
	//	BaseClass::Paint();
	//	return;
	//}

	// Sit in the "center"
	int xpos, ypos;
	GetPanelPos( xpos, ypos );

	// Black out the background (we could omit drawing under the video surface, but this is straight-forward)
	if ( m_bBlackBackground )
	{
		surface()->DrawSetColor(  0, 0, 0, 255 );
		surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
	}

	//wchar_t* szTranslated = g_pVGuiLocalize->Find( m_hScreenEntity->GetDisplayText() );
	if ( g_pVGuiLocalize->Find( m_hScreenEntity->GetDisplayText() ) )
	{
		const char *option = m_hScreenEntity->GetDisplayTextOption();
		if ( option && option[0] )
		{
			wchar_t wszMessage[512] = { 0 };
			g_pVGuiLocalize->ConstructString( wszMessage, sizeof( wszMessage ), g_pVGuiLocalize->Find( m_hScreenEntity->GetDisplayText() ), 1, option );
			m_pTextLabel->SetText( wszMessage );
		}
		else
		{
			m_pTextLabel->SetText( g_pVGuiLocalize->Find( m_hScreenEntity->GetDisplayText() ) );
		}
	}
	else
	{
		const char *option = m_hScreenEntity->GetDisplayTextOption();
		if ( option && option[0] )
		{
			wchar_t wszDisplayText[128];
			g_pVGuiLocalize->ConvertANSIToUnicode( m_hScreenEntity->GetDisplayText(), wszDisplayText, sizeof( wszDisplayText ) );

			wchar_t wszText[512];
			V_swprintf_safe( wszText, L"%s", wszDisplayText );

			wchar_t wszMessage[512] = { 0 };
			g_pVGuiLocalize->ConstructString( wszMessage, sizeof( wszMessage ), wszText, 1, option );
			m_pTextLabel->SetText( wszMessage );
		}
		else
		{
			m_pTextLabel->SetText( m_hScreenEntity->GetDisplayText() );
		}	
	}

	// Parent's turn
	BaseClass::Paint();
}

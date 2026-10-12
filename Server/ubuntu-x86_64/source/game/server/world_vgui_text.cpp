//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Allows movies to be played as a VGUI screen in the world
//
//=====================================================================================//

#include "cbase.h"
#include "EnvMessage.h"
#include "fmtstr.h"
#include "vguiscreen.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class CWorldVguiText : public CBaseEntity
{
public:

	DECLARE_CLASS( CWorldVguiText, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CWorldVguiText()
	{
	}

	virtual ~CWorldVguiText();

	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	virtual int  UpdateTransmitState();
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void OnRestore( void );

	void	ScreenVisible( bool bVisible );

	void	Disable( void );
	void	Enable( void );

	void	InputDisable( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );

	void	InputSetDisplayText( inputdata_t &inputdata );
	void	InputSetDisplayTextOption( inputdata_t &inputdata );

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );

private:
	CNetworkVar( bool, m_bEnabled );

	CNetworkString( m_szDisplayText, 512 );
	string_t	m_strDisplayText;

	CNetworkString( m_szDisplayTextOption, 256 );
	string_t	m_strDisplayTextOption;

	CNetworkString( m_szFont, 64 );
	string_t	m_strFont;

	CNetworkColor32( m_clrText );
	CNetworkVar( int, m_iTextPanelWidth );

	int			m_iScreenWidth;
	int			m_iScreenHeight;
	

	bool		m_bDoFullTransmit;

	CHandle<CVGuiScreen>	m_hScreen;
};

LINK_ENTITY_TO_CLASS( vgui_world_text_panel, CWorldVguiText );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CWorldVguiText )

	DEFINE_KEYFIELD( m_bEnabled, FIELD_BOOLEAN, "enabled" ),

	DEFINE_AUTO_ARRAY( m_szDisplayText, FIELD_CHARACTER ),
	DEFINE_KEYFIELD( m_strDisplayText, FIELD_STRING, "displaytext" ),
	DEFINE_AUTO_ARRAY( m_szDisplayTextOption, FIELD_CHARACTER ),
	DEFINE_KEYFIELD( m_strDisplayTextOption, FIELD_STRING, "displaytextoption" ),
	DEFINE_AUTO_ARRAY( m_szFont, FIELD_CHARACTER ),
	DEFINE_KEYFIELD( m_strFont, FIELD_STRING, "font" ),

	DEFINE_KEYFIELD( m_iScreenWidth, FIELD_INTEGER, "width" ),
	DEFINE_KEYFIELD( m_iScreenHeight, FIELD_INTEGER, "height" ),
	DEFINE_KEYFIELD( m_iTextPanelWidth, FIELD_INTEGER, "textpanelwidth" ),

	DEFINE_KEYFIELD( m_clrText, FIELD_COLOR32, "textcolor" ),

	DEFINE_FIELD( m_bDoFullTransmit, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_hScreen, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetDisplayText", InputSetDisplayText ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetDisplayTextOption", InputSetDisplayTextOption ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CWorldVguiText, DT_WorldVguiText )
	SendPropBool( SENDINFO( m_bEnabled ) ),
	SendPropString( SENDINFO(m_szDisplayText) ),
	SendPropString( SENDINFO(m_szDisplayTextOption) ),
	SendPropString( SENDINFO(m_szFont) ),
	SendPropInt( SENDINFO( m_iTextPanelWidth ) ),
	SendPropInt(	SENDINFO(m_clrText),	32, SPROP_UNSIGNED, SendProxy_Color32ToInt32 ),
END_SEND_TABLE()

CWorldVguiText::~CWorldVguiText()
{
	DestroyVGuiScreen( m_hScreen.Get() );
}

//-----------------------------------------------------------------------------
// Read in Hammer data
//-----------------------------------------------------------------------------
bool CWorldVguiText::KeyValue( const char *szKeyName, const char *szValue ) 
{
	// NOTE: Have to do these separate because they set two values instead of one
	if( FStrEq( szKeyName, "angles" ) )
	{
		Assert( GetMoveParent() == NULL );
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );

		// Because the vgui screen basis is strange (z is front, y is up, x is right)
		// we need to rotate the typical basis before applying it
		VMatrix mat, rotation, tmp;
		MatrixFromAngles( angles, mat );
		MatrixBuildRotationAboutAxis( rotation, Vector( 0, 1, 0 ), 90 );
		MatrixMultiply( mat, rotation, tmp );
		MatrixBuildRotateZ( rotation, 90 );
		MatrixMultiply( tmp, rotation, mat );
		MatrixToAngles( mat, angles );
		SetAbsAngles( angles );

		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CWorldVguiText::UpdateTransmitState()
{
	if ( m_bDoFullTransmit )
	{
		m_bDoFullTransmit = false;
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	// Force our screen to be sent too.
	m_hScreen->SetTransmit( pInfo, bAlways );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::Spawn( void )
{
	// Move the strings into a networkable form
	Q_strcpy( m_szDisplayText.GetForModify(), m_strDisplayText.ToCStr() );
	Q_strcpy( m_szDisplayTextOption.GetForModify(), m_strDisplayTextOption.ToCStr() );
	Q_strcpy( m_szFont.GetForModify(), m_strFont.ToCStr() );

	Precache();

	BaseClass::Spawn();

	//m_bEnabled = false;

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );

	m_bDoFullTransmit = true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::Precache( void )
{
	BaseClass::Precache();

	PrecacheVGuiScreen( "world_text_panel" );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::OnRestore( void )
{
	BaseClass::OnRestore();

	m_bDoFullTransmit = true;

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::ScreenVisible( bool bVisible )
{
	// Set its active state
	m_hScreen->SetActive( bVisible );

	if ( bVisible )
	{
		m_hScreen->RemoveEffects( EF_NODRAW );
	}
	else
	{
		m_hScreen->AddEffects( EF_NODRAW );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	ScreenVisible( false );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::Enable( void )
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;

	ScreenVisible( true );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::InputSetDisplayText( inputdata_t &inputdata )
{
	Q_strcpy( m_szDisplayText.GetForModify(), inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::InputSetDisplayTextOption( inputdata_t &inputdata )
{
	Q_strcpy( m_szDisplayTextOption.GetForModify(), inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "world_text_panel";
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CWorldVguiText::SpawnControlPanels()
{
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		float flWidth = m_iScreenWidth;
		float flHeight = m_iScreenHeight;

		CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, this, this, 0 );

		//Vector vForward, vRight, vUp;
		//AngleVectors( pScreen->GetAbsAngles(), &vForward, &vRight, &vUp );
		//Vector vecNewOrigin = pScreen->GetAbsOrigin() + vForward*(flWidth/2);
		//pScreen->SetAbsOrigin( vecNewOrigin );
		pScreen->ChangeTeam( GetTeamNumber() );
		pScreen->SetActualSize( flWidth, flHeight );
		pScreen->SetActive( true );
		pScreen->MakeVisibleOnlyToTeammates( false );
		pScreen->SetTransparency( true );
		m_hScreen = pScreen;

		return;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CWorldVguiText::RestoreControlPanels( void )
{
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		CVGuiScreen *pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( NULL, pScreenClassname );

		while ( ( pScreen && pScreen->GetOwnerEntity() != this ) || Q_strcmp( pScreen->GetPanelName(), pScreenName ) != 0 )
		{
			pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( pScreen, pScreenClassname );
		}

		if ( pScreen )
		{
			m_hScreen = pScreen;
			m_hScreen->SetActive( true );
		}

		return;
	}
}

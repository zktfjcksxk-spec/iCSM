//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 'RAII style' helper for game input capture
//=============================================================================//

#ifndef PANORAMA_UIINPUTCAPTURE_H
#define PANORAMA_UIINPUTCAPTURE_H

#include "IGameUIFuncs.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/panelptr.h"

namespace panorama
{
	class CGameInputCapture
	{
	public:
		explicit CGameInputCapture( CPanel2D* pPanel, const char* szDebugName, EGameInputFlags eFlags = k_EGameInputCaptureAll, bool bEnabled = false );
		~CGameInputCapture();

		void Enable();
		void Disable();
		void SetEnabled( bool );

		void SetFlags( EGameInputFlags eFlags );

		bool BEnabled() const;
		EGameInputFlags GetFlags() const;
		const char* GetName() const;
		CPanel2D* GetPanel() const;

	private:
		panorama::CPanelPtr<panorama::IUIPanel> m_pPanel;
		const char* m_szDebugName;
		EGameInputFlags m_eFlags;
		uint64 m_hInputHandle;
	};
}

inline panorama::CGameInputCapture::CGameInputCapture( CPanel2D* pPanel, const char* szDebugName, EGameInputFlags eFlags, bool bEnabled )
	: m_pPanel( pPanel ? pPanel->UIPanel() : nullptr )
	, m_szDebugName( szDebugName )
	, m_eFlags( eFlags )
	, m_hInputHandle( 0 )
{
	if ( bEnabled )
		Enable();
}

inline panorama::CGameInputCapture::~CGameInputCapture()
{
	Disable();
}

inline void panorama::CGameInputCapture::Enable()
{
	if ( m_hInputHandle )
		return;

	m_hInputHandle = gameuifuncs->PanoramaAddGameInputHandler( m_pPanel.Get(), m_eFlags, m_szDebugName );
}

inline void panorama::CGameInputCapture::Disable()
{
	if ( !m_hInputHandle )
		return;

	gameuifuncs->PanoramaReleaseGameInputHandler( m_hInputHandle );
	m_hInputHandle = 0;
}

inline void panorama::CGameInputCapture::SetEnabled( bool bEnable )
{
	if ( bEnable )
		Enable();
	else
		Disable();
}

inline void panorama::CGameInputCapture::SetFlags( EGameInputFlags eFlags )
{
	bool bUpdateHandle = false;
	if ( eFlags != m_eFlags && m_hInputHandle != 0 )
		bUpdateHandle = true;

	m_eFlags = eFlags;

	if ( bUpdateHandle )
	{
		Disable();
		Enable();
	}
}

inline bool panorama::CGameInputCapture::BEnabled() const
{
	return ( m_hInputHandle != 0 );
}

inline panorama::EGameInputFlags panorama::CGameInputCapture::GetFlags() const
{
	return m_eFlags;
}

inline const char* panorama::CGameInputCapture::GetName() const
{
	return m_szDebugName;
}

inline panorama::CPanel2D* panorama::CGameInputCapture::GetPanel() const
{
	return ToPanel2D( m_pPanel.Get() );
}


#endif


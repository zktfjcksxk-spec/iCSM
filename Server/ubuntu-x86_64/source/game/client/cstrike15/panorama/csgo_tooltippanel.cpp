//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"

#include "panorama/csgo_tooltippanel.h"
#include "panorama/ui_tooltip_manager.h"

CCSGOMixinTooltip::CCSGOMixinTooltip( panorama::CPanel2D* owner )
	: m_Owner( owner )
{
}

void CCSGOMixinTooltip::Set( const char* pchTooltip )
{
	if ( pchTooltip )
	{
		m_Owner->SetOnMouseOverEvent( UIShowTextTooltip::MakeEvent( m_Owner, pchTooltip ) );
		m_Owner->SetOnMouseOutEvent( UIHideTextTooltip::MakeEvent( nullptr ) );
	}
	else
	{
		m_Owner->SetOnMouseOverEvent( nullptr );
		m_Owner->SetOnMouseOutEvent( nullptr );
	}
}

REGISTER_PANEL2D_FACTORY( CCSGO_TooltipPanel, TooltipPanel );

CCSGO_TooltipPanel::CCSGO_TooltipPanel( panorama::CPanel2D* parent, const char* pchID )
	: BaseClass( parent, pchID )
{
}

bool CCSGO_TooltipPanel::BSetProperties( const CUtlVector<panorama::ParsedPanelProperty_t> &vecProperties )
{
	static const panorama::CPanoramaSymbol symTooltip( "tooltip" );
	static const panorama::CPanoramaSymbol symTooltipClass( "tooltip-class" );
	static const panorama::CPanoramaSymbol symTitle( "tooltip-title" );
	static const panorama::CPanoramaSymbol symImage( "image-src" );
	static const panorama::CPanoramaSymbol symSrc( "src" );
	static const panorama::CPanoramaSymbol symId( "src-id" ); // required with src
	static const panorama::CPanoramaSymbol symParameters( "src-parameters" ); // optional with src

	const char* pchTooltip = nullptr;
	const char* pchStyle = nullptr;
	const char* pchTitle = nullptr;
	const char* pchImage = nullptr;
	const char* pchSrc = nullptr;
	const char* pchId = nullptr;
	const char* pchParameters = nullptr;

	FOR_EACH_VEC( vecProperties, i )
	{
		if ( vecProperties[i].m_symName == symTooltip )
		{
			pchTooltip = vecProperties[i].m_pchValue;
		}
		else if ( vecProperties[i].m_symName == symTooltipClass )
		{
			pchStyle = vecProperties[i].m_pchValue;
		}
		else if ( vecProperties[i].m_symName == symTitle )
		{
			pchTitle = vecProperties[i].m_pchValue;
		}
		else if ( vecProperties[i].m_symName == symImage )
		{
			pchImage = vecProperties[i].m_pchValue;
		}
		else if ( vecProperties[i].m_symName == symSrc )
		{
			pchSrc = vecProperties[i].m_pchValue;
		}
		else if ( vecProperties[i].m_symName == symId )
		{
			pchId = vecProperties[i].m_pchValue;
		}
		else if ( vecProperties[i].m_symName == symParameters )
		{
			pchParameters = vecProperties[i].m_pchValue;
		}
	}

	if ( !pchTooltip && !pchSrc )
	{
		// no tooltip parameters, probably from DelayedProperties
		// (unfortunately it's hard to distinguish between 'no data' and 'multiple calls to set properties' in panorama)
		return BaseClass::BSetProperties( vecProperties );
	}

	panorama::IUIEvent* pShowTooltipEvent = nullptr;
	panorama::IUIEvent* pHideTooltipEvent = nullptr;

	if ( pchSrc && pchSrc[0] && pchId && pchId[0] )
	{
		// Custom layout tooltip
		if ( pchParameters )
		{
			if ( pchStyle )
				pShowTooltipEvent = UIShowCustomLayoutParametersTooltipStyled::MakeEvent( this, pchId, pchSrc, pchParameters, pchStyle );
			else
				pShowTooltipEvent = UIShowCustomLayoutParametersTooltip::MakeEvent( this, pchId, pchSrc, pchParameters );
		}
		else
		{
			if ( pchStyle )
				pShowTooltipEvent = UIShowCustomLayoutTooltipStyled::MakeEvent( this, pchId, pchSrc, pchStyle );
			else
				pShowTooltipEvent = UIShowCustomLayoutTooltip::MakeEvent( this, pchId, pchSrc );
		}

		pHideTooltipEvent = UIHideCustomLayoutTooltip::MakeEvent( nullptr, pchId );
	}
	else if ( pchTooltip )
	{
		// regular tooltip variant
		if ( pchImage )
		{
			if ( !pchTitle )
				pchTitle = "";

			if ( pchStyle )
				pShowTooltipEvent = UIShowTitleImageTextTooltipStyled::MakeEvent( this, pchTitle, pchImage, pchTooltip, pchStyle );
			else
				pShowTooltipEvent = UIShowTitleImageTextTooltip::MakeEvent( this, pchTitle, pchImage, pchTooltip );

			pHideTooltipEvent = UIHideTitleImageTextTooltip::MakeEvent( nullptr );
		}
		else if ( pchTitle )
		{
			if ( pchStyle )
				pShowTooltipEvent = UIShowTitleTextTooltipStyled::MakeEvent( this, pchTitle, pchTooltip, pchStyle );
			else
				pShowTooltipEvent = UIShowTitleTextTooltip::MakeEvent( this, pchTitle, pchTooltip );

			pHideTooltipEvent = UIHideTitleTextTooltip::MakeEvent( nullptr );
		}
		else if ( pchTooltip[0] )
		{
			if ( pchStyle )
				pShowTooltipEvent = UIShowTextTooltipStyled::MakeEvent( this, pchTooltip, pchStyle );
			else
				pShowTooltipEvent = UIShowTextTooltip::MakeEvent( this, pchTooltip );

			pHideTooltipEvent = UIHideTextTooltip::MakeEvent( nullptr );
		}
		// else disable tooltip
	}

	SetOnMouseOverEvent( pShowTooltipEvent );
	SetOnMouseOutEvent( pHideTooltipEvent );

	return BaseClass::BSetProperties( vecProperties );
}

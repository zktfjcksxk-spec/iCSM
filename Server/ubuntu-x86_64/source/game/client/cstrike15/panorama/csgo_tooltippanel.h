//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_MIXIN_TOOLTIP_H_
#define CSGO_MIXIN_TOOLTIP_H_

#pragma once

#include "panorama/controls/panel2d.h"

class CCSGOMixinTooltip
{
public:
	CCSGOMixinTooltip( panorama::CPanel2D* owner );
	void Set( const char* pchTooltip );

private:
	panorama::CPanel2D* m_Owner;
};

class CCSGO_TooltipPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_TooltipPanel, panorama::CPanel2D );

public:
	CCSGO_TooltipPanel( panorama::CPanel2D* parent, const char* pchID );
	bool BSetProperties( const CUtlVector<panorama::ParsedPanelProperty_t> &vecProperties ) OVERRIDE;
};

#endif // CSGO_MIXIN_TOOLTIP_H_

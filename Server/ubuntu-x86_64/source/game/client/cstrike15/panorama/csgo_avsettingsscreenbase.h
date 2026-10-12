//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#define INVALID_OPTION_VALUE -999
#define AUTO_OPTION_VALUE 9999999

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static inline void DropdownSelectOptionByIndex( CCSGO_SettingsEnumDropDown *pDropDown, int nIndex, bool bNotify = false )
{
	pDropDown->SetSelected( nIndex, bNotify );
	pDropDown->InvalidateOptions( false );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static inline int DropdownGetSelectedValue( CCSGO_SettingsEnumDropDown *pDropdown )
{
	panorama::CPanel2D *pSelectedPanel = pDropdown->GetSelected();
	Assert( pSelectedPanel );

	int nValue = INVALID_OPTION_VALUE;
	if ( pSelectedPanel )
	{
		nValue = pSelectedPanel->GetAttribute( "value", INVALID_OPTION_VALUE );
	}

	return nValue;
}
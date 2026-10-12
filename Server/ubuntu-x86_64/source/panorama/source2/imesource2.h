//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#if !defined( __IMESOURCE2_H__ )
#define __IMESOURCE2_H__
#pragma once

#include "iimemanager.h"
#include "panorama/controls/textentry.h"

// Tracks the focused text entry widget
class CIMEUITextField
{
public:
	CIMEUITextField();
	~CIMEUITextField();

	void SetFocusedTextEntry( panorama::CTextEntry *pTextEntry );
	void ClearFocusedTextEntry( panorama::CTextEntry *pTextEntry );

	panorama::CTextEntry *GetFocusedTextEntry();
	bool IsKeyFocused();

private:
	// actual panorama backing text entry object, tracked according to current focus
	panorama::CPanelPtr< panorama::CTextEntry >	m_pTextEntry;
};

#endif

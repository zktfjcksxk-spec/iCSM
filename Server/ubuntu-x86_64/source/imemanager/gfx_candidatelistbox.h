/**************************************************************************

Filename    :   GFx_CandidateListBox.h
Content     :   GFx to C++ ListBox
Created     :   10/12/2007
Authors     :   David Cook, Prasad Silva

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef _GFX_CANDIDATELISTBOX_H_
#define _GFX_CANDIDATELISTBOX_H_

class GFxIMEWin32Impl;

struct CandidateListItem
{
    CandidateListItem( wchar_t *pWideString )
    {
        m_ValueString = pWideString;
    }

    CUtlWString	m_ValueString;
};

class CandidateListBox
{
public:
	CandidateListBox( GFxIMEWin32Impl *pIMEWin32Impl );
    ~CandidateListBox();

    // Methods to communicate with the widget
    void                        UIRefreshView();

    // List data related methods
    void						AddListItem( const CandidateListItem *pListItem );
    void                        RemoveAllListItems();
    void                        RemoveList();
    
    // Selection related methods
    void                        SetSelectedItemIndex( int32 idx, bool bRefreshList = false );

private:   
	CUtlVector< const CandidateListItem * > m_ListItems;
    int32                       m_nSelectedItemIndex;	// The index of the selected item (-1 if no item is selected)

    GFxIMEWin32Impl				*m_pIMEImpl;
};

#endif


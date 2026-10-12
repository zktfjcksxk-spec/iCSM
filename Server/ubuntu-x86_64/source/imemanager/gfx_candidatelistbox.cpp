/**************************************************************************

Filename    :   GFx_CandidateListBox.cpp
Content     :   GFx to C++ ListBox
Created     :   2/23/2007
Authors     :   David Cook, Prasad Silva

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "imemanager.h"

#include "gfx_candidatelistbox.h"
#include "gfx_imewin32impl.h"
		
CandidateListBox::CandidateListBox( GFxIMEWin32Impl *pIMEImpl )
{
	m_nSelectedItemIndex = -1;
	m_pIMEImpl = pIMEImpl;
}

CandidateListBox::~CandidateListBox()
{
	m_ListItems.PurgeAndDeleteElements();
}

// ListBox View: Refresh the UI view
void CandidateListBox::UIRefreshView()
{
    // clear the list
	m_pIMEImpl->Invoke_ClearList();

    // Notify the UI of the current selection index (this may have changed)
	m_pIMEImpl->Invoke_SelectItemInList( m_nSelectedItemIndex );

    // add the visible items
    for ( int i = 0; i < m_ListItems.Count(); i++ )
	{
       	m_pIMEImpl->Invoke_AddToList( m_ListItems[i]->m_ValueString.Get() );
	}

    // display the visible items
	m_pIMEImpl->Invoke_ShowList( true );
}

// Add a new ListItem
void CandidateListBox::AddListItem( const CandidateListItem *pItem )
{
	if ( pItem )
	{
		m_ListItems.AddToTail( pItem );
	}
}

// Remove all ListItems
//
// Maintains visible count
//
void CandidateListBox::RemoveAllListItems()
{
    m_ListItems.PurgeAndDeleteElements();
}

// Set the currently selected item
//
void CandidateListBox::SetSelectedItemIndex( int32 nIndex, bool bRefreshList )
{
    m_nSelectedItemIndex = nIndex;
	if ( bRefreshList )
	{
		m_pIMEImpl->Invoke_SelectItemInList( m_nSelectedItemIndex );
		m_pIMEImpl->Invoke_ShowList( true );
	}
}

// Removes List
void CandidateListBox::RemoveList()
{
    m_ListItems.PurgeAndDeleteElements();
	m_pIMEImpl->Invoke_RemoveList();
}

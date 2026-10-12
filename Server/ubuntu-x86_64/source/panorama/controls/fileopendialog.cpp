//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/fileopendialog.h"
#include "panorama/controls/label.h"
#include "panorama/controls/button.h"
#include "panorama/controls/dropdown.h"
#include "panorama/controls/textentry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#undef GetCurrentDirectory

using namespace panorama;

REGISTER_PANEL2D( CFileOpenDialog, FileOpenDialog );
DEFINE_PANORAMA_EVENT( FileOpenDialogOpen );
DEFINE_PANORAMA_EVENT( FileOpenDialogCancel );
DEFINE_PANORAMA_EVENT( FileOpenDialogClose );
DEFINE_PANORAMA_EVENT( FileOpenDialogFolderUp );
DEFINE_PANORAMA_EVENT( FileOpenDialogSortByColumn );
DEFINE_PANORAMA_EVENT( FileOpenDialogSelectFile );
DEFINE_PANORAMA_EVENT( FileOpenDialogDoubleClickFile );
DEFINE_PANORAMA_EVENT( FileOpenDialogFullPathChanged );
DEFINE_PANORAMA_EVENT( FileOpenDialogFilterChanged );
DEFINE_PANORAMA_EVENT( FileOpenDialogFilesSelected );

//-----------------------------------------------------------------------------
// Dictionary of start dir contexts 
//-----------------------------------------------------------------------------
static CUtlDict< CUtlString, unsigned short > s_StartDirContexts;

static const int MAX_FILTER_LENGTH = 255;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFileOpenDialog::CFileOpenDialog( CPanel2D *parent, const char * pchPanelID, FileOpenDialogType_t type ) : CPanel2D( parent, pchPanelID )
{
	m_DialogType = type;
	Init();
}

CFileOpenDialog::CFileOpenDialog( panorama::IUIWindow *pParent, const char * pchPanelID, FileOpenDialogType_t type ) : CPanel2D( pParent, pchPanelID )
{
	m_DialogType = type;
	Init();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFileOpenDialog::~CFileOpenDialog()
{
}


void CFileOpenDialog::Init()
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/fileopendialog.xml" ) );

	RegisterEventHandler( FileOpenDialogOpen(), this, &CFileOpenDialog::EventOpen );
	RegisterEventHandler( FileOpenDialogCancel(), this, &CFileOpenDialog::EventCancel );
	RegisterEventHandler( FileOpenDialogClose(), this, &CFileOpenDialog::EventClose );
	RegisterEventHandler( FileOpenDialogFolderUp(), this, &CFileOpenDialog::EventFolderUp );
	RegisterEventHandler( FileOpenDialogSortByColumn(), this, &CFileOpenDialog::EventColumnSortingChanged );
	RegisterEventHandler( FileOpenDialogSelectFile(), this, &CFileOpenDialog::EventSelectFile );
	RegisterEventHandler( FileOpenDialogDoubleClickFile(), this, &CFileOpenDialog::EventDoubleClickFile );
	RegisterEventHandler( FileOpenDialogFullPathChanged(), this, &CFileOpenDialog::EventFullPathChanged );
	RegisterEventHandler( FileOpenDialogFilterChanged(), this, &CFileOpenDialog::EventFilterChanged );

	m_bFileSelected = false;

#ifdef POSIX
	V_strncpy(m_szLastPath, "/", sizeof( m_szLastPath ) );	
#else
	V_strncpy(m_szLastPath, "c:\\", sizeof( m_szLastPath ) );
#endif
	
	m_pFullPathDropDown = assert_cast< CDropDown* >( FindChildInLayoutFile( "DrivesDropDown" ) );
	m_pFileList = FindChildInLayoutFile( "FileList" );
	m_pFileNameTextEntry = assert_cast< CTextEntry* >( FindChildInLayoutFile( "FileNameTextEntry" ) );
	m_pFileTypeCombo = assert_cast< CDropDown* >( FindChildInLayoutFile( "FileTypeDropDown" ) );
	m_pOpenButton = assert_cast< CButton* >( FindChildInLayoutFile( "OpenButton" ) );
	m_pCancelButton = assert_cast< CButton* >( FindChildInLayoutFile( "CancelButton" ) );
	m_pFolderUpButton = assert_cast< CButton* >( FindChildInLayoutFile( "FolderUpButton" ) );
	m_pFileTypeLabel = assert_cast< CLabel* >( FindChildInLayoutFile( "FileTypeLabel" ) );

	m_vecColumnHeaders.AddToTail( FindChildInLayoutFile( "HeaderName" ) );
	m_vecColumnHeaders.AddToTail( FindChildInLayoutFile( "HeaderSize" ) );
	m_vecColumnHeaders.AddToTail( FindChildInLayoutFile( "HeaderType" ) );
	m_vecColumnHeaders.AddToTail( FindChildInLayoutFile( "HeaderDate" ) );

	m_nSorting = FOD_SORT_NAME;
	m_bSortingReversed = false;
	
	switch ( m_DialogType )
	{
	case FOD_OPEN:
		m_pOpenButton->SetDialogVariableLocString( "OpenText", "#FileOpenDialog_Open" );
		break;
	case FOD_SAVE:
		m_pOpenButton->SetDialogVariableLocString( "OpenText", "#FileOpenDialog_Save" );
		break;
	case FOD_SELECT_DIRECTORY:
		m_pOpenButton->SetDialogVariableLocString( "OpenText", "#FileOpenDialog_Select" );
		m_pFileTypeCombo->SetVisible( false );
		break;
	case FOD_OPEN_MULTIPLE:
		m_pOpenButton->SetDialogVariableLocString( "OpenText", "#FileOpenDialog_Open" );
		break;
	}

	m_nStartDirContext = s_StartDirContexts.InvalidIndex();

	// Set our starting path to the current directory
	char pLocalPath[255];
	g_pFullFileSystem->GetCurrentDirectory( pLocalPath , 255 );
	SetStartDirectory( pLocalPath );

	PopulateFileList();
	PopulateDriveList();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileOpenDialog::PopulateDriveList()
{
	char fullpath[MAX_PATH * 4];
	char subDirPath[MAX_PATH * 4];
	GetDirectory(fullpath, sizeof(fullpath) - MAX_PATH);
	V_strncpy(subDirPath, fullpath, sizeof( subDirPath ) );

	m_pFullPathDropDown->RemoveAllOptions();

	// populate the drive list
	CUtlVector< CUtlString > vecDrives;
#if defined( SOURCE2_PANORAMA ) && ( !defined( PANORAMA_USE_S1WRAPPER ) )
	g_pFullFileSystem->GetAvailableDrives( vecDrives );
#else
	Assert( !"Not implemented" );
#endif

	int nItems = 0;
	FOR_EACH_VEC( vecDrives, i )
	{
		CFmtStr strLabelID( "Option_%d", nItems++ );
		CLabel *pLabel = new CLabel( m_pFullPathDropDown, strLabelID.Get() );
		pLabel->SetAttribute( "Directory", vecDrives[i].Get() );
		pLabel->SetText( vecDrives[i].Get() );

		m_pFullPathDropDown->AddOption( pLabel );

		// is this our drive - add all subdirectories
		if ( !V_strnicmp( vecDrives[i].Get(), fullpath, 2 ) )
		{
			int indent = 0;
			char *pData = fullpath;
			while (*pData)
			{
				if (*pData == CORRECT_PATH_SEPARATOR )
				{
					if (indent > 0)
					{
						memset(subDirPath, ' ', indent);
						memcpy(subDirPath+indent, fullpath, pData-fullpath);
						subDirPath[indent+pData-fullpath] = 0;

						CFmtStr strSubDirLabelID( "Option_%d", nItems++ );
						CLabel *pSubDirLabel = new CLabel( m_pFullPathDropDown, strSubDirLabelID.Get() );
						pSubDirLabel->SetAttribute( "Directory", subDirPath + indent );
						pSubDirLabel->SetText( subDirPath, CLabel::k_ETextTypeUnlocalized );
						m_pFullPathDropDown->AddOption( pSubDirLabel );
					}
					indent += 2;
				}
				pData++;
			}
		}
	}

	// make the dropdown entry representing our current location selected

	char szCurrentDirectory[MAX_PATH * 3];
	V_strcpy_safe( szCurrentDirectory, m_szLastPath );
	V_StripTrailingSlash( szCurrentDirectory );

	CPanel2D *pMenu = m_pFullPathDropDown->AccessDropDownMenu();
	for ( int i = 0; i < pMenu->GetChildCount(); i++ )
	{
		CLabel *pLabel = assert_cast<CLabel*>( pMenu->GetChild( i ) );

		char szLabelDirectory[MAX_PATH * 3];
		V_strcpy_safe( szLabelDirectory, pLabel->GetAttribute( "Directory", "" ) );
		V_StripTrailingSlash( szLabelDirectory );

		if ( !V_stricmp( szLabelDirectory, szCurrentDirectory ) )
		{
			m_pFullPathDropDown->SetSelected( pLabel->GetID(), false );

			// fix up selected label to remove the indent and localization
			CLabel *pSelectedLabel = assert_cast<CLabel*>( m_pFullPathDropDown->GetSelected() );
			pSelectedLabel->SetText( szLabelDirectory, CLabel::k_ETextTypeUnlocalized );
			break;
		}
	}
}

bool CFileOpenDialog::EventFolderUp()
{
	MoveUpFolder();
	OnOpen();
	return true;
}

//-----------------------------------------------------------------------------
// Sets the start directory context (and resets the start directory in the process)
//-----------------------------------------------------------------------------
void CFileOpenDialog::SetStartDirectoryContext( const char *pStartDirContext, const char *pDefaultDir )
{
	bool bUseCurrentDirectory = true;
	if ( pStartDirContext )
	{
		m_nStartDirContext = s_StartDirContexts.Find( pStartDirContext );
		if ( m_nStartDirContext == s_StartDirContexts.InvalidIndex() )
		{
			m_nStartDirContext = s_StartDirContexts.Insert( pStartDirContext, pDefaultDir );
			bUseCurrentDirectory = ( pDefaultDir == NULL );
		}
		else
		{
			bUseCurrentDirectory = false;
		}
	}
	else
	{
		m_nStartDirContext = s_StartDirContexts.InvalidIndex();
	}

	if ( !bUseCurrentDirectory )
	{
		SetStartDirectory( s_StartDirContexts[m_nStartDirContext].Get() );
	}
	else
	{
		// Set our starting path to the current directory
		char pLocalPath[255];
		g_pFullFileSystem->GetCurrentDirectory( pLocalPath, 255 );
		SetStartDirectory( pLocalPath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the starting directory of the file search.
//-----------------------------------------------------------------------------
void CFileOpenDialog::SetStartDirectory( const char *dir )
{
	DeselectAllEntries();
	V_strcpy_safe( m_szLastPath, dir );

	// ensure it's validity
	ValidatePath();

	// Store this in the start directory list
	if ( m_nStartDirContext != s_StartDirContexts.InvalidIndex() )
	{
		char pDirBuf[MAX_PATH];
		GetDirectory( pDirBuf, sizeof(pDirBuf) );
		s_StartDirContexts[ m_nStartDirContext ] = pDirBuf;
	}

	PopulateDriveList();
}


//-----------------------------------------------------------------------------
// Purpose: Add filters for the drop down combo box
//-----------------------------------------------------------------------------
void CFileOpenDialog::AddFilter( const char *filter, const char *filterName, bool bActive, const char *pFilterInfo )
{
	CFmtStr strLabelID( "Option_%d", m_pFileTypeCombo->AccessDropDownMenu()->GetChildCount() );
	CLabel *pLabel = new CLabel( m_pFileTypeCombo, strLabelID.Get() );
	pLabel->SetText( filterName );
	pLabel->SetAttribute( "filter", filter );
	pLabel->SetAttribute( "filterinfo", pFilterInfo );

	m_pFileTypeCombo->AddOption( pLabel );
	if ( bActive )
	{
		m_pFileTypeCombo->SetSelected( strLabelID.Get(), true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets the directory this is currently in
//-----------------------------------------------------------------------------
void CFileOpenDialog::GetDirectory( char *buf, int bufSize )
{
	V_strncpy( buf, m_szLastPath, bufSize );
}


//-----------------------------------------------------------------------------
// Purpose: Get the last selected file name
//-----------------------------------------------------------------------------
void CFileOpenDialog::GetSelectedFileName( char *buf, int bufSize )
{
	V_snprintf( buf, bufSize, "%s", m_pFileNameTextEntry->PchGetText() );
}


//-----------------------------------------------------------------------------
// Purpose: Move the directory structure up
//-----------------------------------------------------------------------------
void CFileOpenDialog::MoveUpFolder()
{
	char fullpath[MAX_PATH * 4];
	GetDirectory(fullpath, sizeof(fullpath) - MAX_PATH);

	// strip it back
	char *pos = strrchr(fullpath, CORRECT_PATH_SEPARATOR );
	if (pos)
	{
		*pos = 0;

		if (!pos[1])
		{
			pos = strrchr(fullpath, CORRECT_PATH_SEPARATOR );
			if (pos)
			{
				*pos = 0;
			}
		}
	}

	// append a trailing slash
	Q_strncat(fullpath, CORRECT_PATH_SEPARATOR_S, sizeof( fullpath ), COPY_ALL_CHARACTERS );

	SetStartDirectory(fullpath);
	m_pFileNameTextEntry->SetText("");
	PopulateFileList();
}

//-----------------------------------------------------------------------------
// Purpose: Validate that the current path is valid
//-----------------------------------------------------------------------------
void CFileOpenDialog::ValidatePath()
{
	char fullpath[MAX_PATH * 4];
	V_strncpy( fullpath, m_szLastPath, sizeof( fullpath ) - MAX_PATH );
	Q_RemoveDotSlashes( fullpath );

	// when statting a directory on Windows, you want to include
	// the terminal slash exactly when you are statting a root
	// directory. PKMN.
#ifdef _WIN32
	if ( Q_strlen( fullpath ) != 3 )
	{
		Q_StripTrailingSlash( fullpath );
	}
#endif
	
	if ( g_pFullFileSystem->IsDirectory( fullpath, NULL ) )
	{
		V_AppendSlash( fullpath, sizeof( fullpath ) );
		V_strncpy(m_szLastPath, fullpath, sizeof(m_szLastPath));
	}
	else
	{
		// failed to load file, use the previously successful path
	}	

	CPanel2D *pMenu = m_pFullPathDropDown->AccessDropDownMenu();
	for ( int i = 0; i < pMenu->GetChildCount(); i++ )
	{
		CLabel *pLabel = assert_cast<CLabel*>( pMenu->GetChild( i ) );
		if ( !V_stricmp( pLabel->PchGetText(), m_szLastPath ) )
		{
			m_pFullPathDropDown->SetSelected( pLabel->GetID(), false );
			break;
		}
	}

	// TODO: VGUI used this to show paths that don't fit in the dropdown
	//CTextTooltip *pTooltip = new CTextTooltip( GetParentWindow(), NULL );
	//pTooltip->SetText( m_szLastPath );
	//pTooltip->SetTooltipTarget( m_pFullPathDropDown );
	//m_pFullPathDropDown->SetTooltip( pTooltip );
}


static void InitFileData( bool bDirectory, char const *pszFileName, const char *pchDirectoryName, FileData_t &data )
{	
	data.m_FileName = V_UnqualifiedFileName( pszFileName );
	data.m_FullPath = CFmtStr( "%s%s", pchDirectoryName, pszFileName );
	data.m_FullPath.FixSlashes();

	if ( !bDirectory )
	{
		wchar_t wszFileType[ 256 ];
		g_pFullFileSystem->GetFileTypeForFullPath( data.m_FullPath.Get(), wszFileType, sizeof( wszFileType ) );
		
		char szFileType[ 512 ];
		V_UnicodeToUTF8( wszFileType, szFileType, V_ARRAYSIZE( szFileType ) );

		data.m_FileType = szFileType;
	}

	data.m_bDirectory = bDirectory;
	data.m_nFileSize = g_pFullFileSystem->Size( data.m_FullPath.Get() );

	if ( !g_pFullFileSystem->IsFileWritable( data.m_FullPath.Get() ) )
		data.m_FileAttributes = "R";

	long fileModified = g_pFullFileSystem->GetFileTime( data.m_FullPath.Get() );
	char pszFileModified[64];
	g_pFullFileSystem->FileTimeToString( pszFileModified, sizeof( pszFileModified ), fileModified );
	data.m_LastWriteTime = pszFileModified;
	data.m_nLastWriteTime = fileModified;
}

static int FileOpenDialogSort_FileName( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	const CFileOpenDialogEntry *pEntry1 = assert_cast<const CFileOpenDialogEntry *>( pRow1 );
	const CFileOpenDialogEntry *pEntry2 = assert_cast<const CFileOpenDialogEntry *>( pRow2 );
	if ( !pEntry1 || !pEntry2 )
		return 0;

	if ( pEntry1->GetFileData()->m_bDirectory && !pEntry2->GetFileData()->m_bDirectory )
		return -1;
	if ( !pEntry1->GetFileData()->m_bDirectory && pEntry2->GetFileData()->m_bDirectory )
		return 1;

	return V_stricmp( pEntry1->GetFileData()->m_FileName.Get(), pEntry2->GetFileData()->m_FileName.Get() );
}

static int FileOpenDialogSort_FileName_Reversed( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	return -FileOpenDialogSort_FileName( pRow1, pRow2 );
}

static int FileOpenDialogSort_Size( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	const CFileOpenDialogEntry *pEntry1 = assert_cast<const CFileOpenDialogEntry *>( pRow1 );
	const CFileOpenDialogEntry *pEntry2 = assert_cast<const CFileOpenDialogEntry *>( pRow2 );
	if ( !pEntry1 || !pEntry2 )
		return 0;

	// directories at the bottom
	if ( pEntry1->GetFileData()->m_bDirectory && !pEntry2->GetFileData()->m_bDirectory )
		return 1;
	if ( !pEntry1->GetFileData()->m_bDirectory && pEntry2->GetFileData()->m_bDirectory )
		return -1;

	if ( pEntry1->GetFileData()->m_nFileSize != pEntry2->GetFileData()->m_nFileSize )
		return pEntry1->GetFileData()->m_nFileSize - pEntry2->GetFileData()->m_nFileSize;

	return V_stricmp( pEntry1->GetFileData()->m_FileName.Get(), pEntry2->GetFileData()->m_FileName.Get() );
}

static int FileOpenDialogSort_Size_Reversed( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	const CFileOpenDialogEntry *pEntry1 = assert_cast<const CFileOpenDialogEntry *>( pRow1 );
	const CFileOpenDialogEntry *pEntry2 = assert_cast<const CFileOpenDialogEntry *>( pRow2 );
	if ( !pEntry1 || !pEntry2 )
		return 0;

	// directories at the bottom
	if ( pEntry1->GetFileData()->m_bDirectory && !pEntry2->GetFileData()->m_bDirectory )
		return 1;
	if ( !pEntry1->GetFileData()->m_bDirectory && pEntry2->GetFileData()->m_bDirectory )
		return -1;

	if ( pEntry1->GetFileData()->m_nFileSize != pEntry2->GetFileData()->m_nFileSize )
		return pEntry2->GetFileData()->m_nFileSize - pEntry1->GetFileData()->m_nFileSize;

	return V_stricmp( pEntry1->GetFileData()->m_FileName.Get(), pEntry2->GetFileData()->m_FileName.Get() );
}

static int FileOpenDialogSort_Type( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	const CFileOpenDialogEntry *pEntry1 = assert_cast<const CFileOpenDialogEntry *>( pRow1 );
	const CFileOpenDialogEntry *pEntry2 = assert_cast<const CFileOpenDialogEntry *>( pRow2 );
	if ( !pEntry1 || !pEntry2 )
		return 0;

	if ( pEntry1->GetFileData()->m_bDirectory && !pEntry2->GetFileData()->m_bDirectory )
		return -1;
	if ( !pEntry1->GetFileData()->m_bDirectory && pEntry2->GetFileData()->m_bDirectory )
		return 1;

	return V_stricmp( pEntry1->GetFileData()->m_FileType.Get(), pEntry2->GetFileData()->m_FileType.Get() );
}

static int FileOpenDialogSort_Type_Reversed( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	return -FileOpenDialogSort_Type( pRow1, pRow2 );
}

static int FileOpenDialogSort_DateModified( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	const CFileOpenDialogEntry *pEntry1 = assert_cast<const CFileOpenDialogEntry *>( pRow1 );
	const CFileOpenDialogEntry *pEntry2 = assert_cast<const CFileOpenDialogEntry *>( pRow2 );
	if ( !pEntry1 || !pEntry2 )
		return 0;

	return pEntry2->GetFileData()->m_nLastWriteTime - pEntry1->GetFileData()->m_nLastWriteTime;
}

static int FileOpenDialogSort_DateModified_Reversed( const IUIPanelClient *pRow1, const IUIPanelClient *pRow2 )
{
	return -FileOpenDialogSort_DateModified( pRow1, pRow2 );
}

void CFileOpenDialog::BuildFileList()
{
	m_Files.RemoveAll();
	m_Filtered.RemoveAll();

	// get the current directory
	char currentDir[MAX_PATH * 4];
	char dir[MAX_PATH * 4];
	char filterList[MAX_FILTER_LENGTH+1];
	GetDirectory(currentDir, sizeof(currentDir));

	FileFindHandle_t findHandle;

	CPanel2D *pSelectedFileType = m_pFileTypeCombo->GetSelected();
	if ( pSelectedFileType )
	{
		V_strncpy( filterList, pSelectedFileType->GetAttribute("filter", "*"), MAX_FILTER_LENGTH );
	}
	else
	{
		// add wildcard for search
		V_strncpy( filterList, "*\0", MAX_FILTER_LENGTH );
	}

	char *filterPtr = filterList;
	
	if ( m_DialogType != FOD_SELECT_DIRECTORY )
	{
		while ((filterPtr != NULL) && (*filterPtr != 0))
		{
			// parse the next filter in the list.
			char curFilter[MAX_FILTER_LENGTH];
			curFilter[0] = 0;
			int i = 0;
			while ((filterPtr != NULL) && ((*filterPtr == ',') || (*filterPtr == ';') || (*filterPtr <= ' ')))
			{
				++filterPtr;
			}
			while ((filterPtr != NULL) && (*filterPtr != ',') && (*filterPtr != ';') && (*filterPtr > ' '))
			{
				curFilter[i++] = *(filterPtr++);
			}
			curFilter[i] = 0;
			
			if (curFilter[0] == 0)
			{
				break;
			}
			
			Q_snprintf( dir, MAX_PATH*4, "%s%s", currentDir, curFilter );
			
			// Open the directory and walk it, loading files
			const char *pszFileName = g_pFullFileSystem->FindFirst( dir, &findHandle );
			while ( pszFileName )
			{
				if ( !g_pFullFileSystem->FindIsDirectory( findHandle ) )
				{
					FileData_t &fd = m_Files[ m_Files.AddToTail() ];
					InitFileData( false, pszFileName, currentDir, fd );
				}
				
				pszFileName = g_pFullFileSystem->FindNext( findHandle );
			}
			g_pFullFileSystem->FindClose( findHandle );
		}
	}

	
	// find all the directories
	GetDirectory(currentDir, sizeof(currentDir));
	Q_snprintf( dir, MAX_PATH*4, "%s*", currentDir );
	
	const char *pszFileName = g_pFullFileSystem->FindFirst( dir, &findHandle );
	while ( pszFileName )
	{
		if ( pszFileName[0] != '.' && g_pFullFileSystem->FindIsDirectory( findHandle ) )
		{
			FileData_t &fd = m_Files[ m_Files.AddToTail() ];
			InitFileData( true, pszFileName, currentDir, fd );
		}
		
		pszFileName = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );
}

// Static method to do wildcard matching for *, ? and . characters
bool CFileOpenDialog::FileNameWildCardMatch( char const *string, char const *pattern )
{
	for (;; ++string)
	{
		char stringc=toupper(*string);
		char patternc=toupper(*pattern++);
		switch (patternc)
		{
		case 0:
			return(stringc==0);
		case '?':
			if (stringc == 0)
				return(false);
			break;
		case '*':
			if (*pattern==0)
				return(true);
			if (*pattern=='.')
			{
				if (pattern[1]=='*' && pattern[2]==0)
					return(true);
				const char *dot=strchr(string,'.');
				if (pattern[1]==0)
					return (dot==NULL || dot[1]==0);
				if (dot!=NULL)
				{
					string=dot;
					if (strpbrk(pattern,"*?")==NULL && strchr(string+1,'.')==NULL)
						return(Q_stricmp(pattern+1,string+1)==0);
				}
			}

			while (*string)
				if (FileNameWildCardMatch(string++, pattern))
					return(true);
			return(false);
		default:
			if (patternc != stringc)
			{
				if (patternc=='.' && stringc==0)
					return(FileNameWildCardMatch(string, pattern ));
				else
					return(false);
			}
			break;
		}
	}
}

bool CFileOpenDialog::PassesFilter( FileData_t *fd )
{
	// Do the substring filtering
	if ( fd->m_bDirectory )
		return true;

	// Never filter Save... dialogs
	if ( m_DialogType == FOD_SAVE )
		return true;

	if ( m_CurrentSubstringFilter.Length() <= 0 )
		return true;

	if ( Q_stristr( fd->m_FileName.Get(), m_CurrentSubstringFilter.String() ) )
		return true;

	if ( FileNameWildCardMatch( fd->m_FileName.Get(), m_CurrentSubstringFilter.String() ) )
		return true;

	return false;
}

void CFileOpenDialog::FilterFileList()
{
	m_Filtered.RemoveAll();
	for ( int i = 0; i < m_Files.Count(); ++i )
	{
		// Apply filter
		FileData_t *pFD = &m_Files[ i ];
		if ( PassesFilter( pFD ) )
		{
			m_Filtered.AddToTail( &m_Files[ i ] );
		}
	}

	// clear the current list
	m_pFileList->RemoveAndDeleteChildren();
	// clear anything that was selected since
	// we just deleted all of the entries.
	m_vecSelectedEntries.RemoveAll();

	KeyValues *kv = new KeyValues("item");

	for ( int i = 0; i < m_Filtered.Count(); ++i )
	{
		FileData_t *fd = m_Filtered[ i ];

		CFileOpenDialogEntry *pEntry = new CFileOpenDialogEntry( m_pFileList, NULL );
		pEntry->SetFileData( fd );
	}

	kv->deleteThis();
	SortEntries();
}

void CFileOpenDialog::SortEntries()
{
	FOR_EACH_VEC( m_vecColumnHeaders, i )
	{
		m_vecColumnHeaders[i]->SetHasClass( "SortedBy", i == (int) m_nSorting );
		m_vecColumnHeaders[i]->SetHasClass( "ReverseSorted", i == (int) m_nSorting && m_bSortingReversed );
	}

	switch ( m_nSorting )
	{
	case FOD_SORT_NAME:				m_pFileList->SortChildren( m_bSortingReversed ? FileOpenDialogSort_FileName_Reversed : FileOpenDialogSort_FileName ); break;
	case FOD_SORT_SIZE:				m_pFileList->SortChildren( m_bSortingReversed ? FileOpenDialogSort_Size_Reversed : FileOpenDialogSort_Size ); break;
	case FOD_SORT_TYPE:				m_pFileList->SortChildren( m_bSortingReversed ? FileOpenDialogSort_Type_Reversed : FileOpenDialogSort_Type ); break;
	case FOD_SORT_DATE_MODIFIED:	m_pFileList->SortChildren( m_bSortingReversed ? FileOpenDialogSort_DateModified_Reversed : FileOpenDialogSort_DateModified ); break;
	}
}

bool CFileOpenDialog::EventColumnSortingChanged( const CPanelPtr< IUIPanel > &pPanel, int nColumn )
{
	if ( m_nSorting == (FileOpenDialogSorting_t) nColumn )
	{
		m_bSortingReversed = !m_bSortingReversed;
	}
	else
	{
		m_nSorting = (FileOpenDialogSorting_t) nColumn;
		m_bSortingReversed = false;
	}

	SortEntries();

	return true;
}

int CFileOpenDialog::CountSubstringMatches()
{
	int nMatches = 0;

	for ( int i = 0; i < m_Files.Count(); ++i )
	{
		// Apply filter
		FileData_t *pFD = &m_Files[ i ];
		if ( PassesFilter( pFD ) )
		{
			if ( !pFD->m_bDirectory )
			{
				++nMatches;
			}
		}
	}
	return nMatches;
}

//-----------------------------------------------------------------------------
// Purpose: Fill the filelist with the names of all the files in the current directory
//-----------------------------------------------------------------------------
void CFileOpenDialog::PopulateFileList()
{
	BuildFileList();
	FilterFileList();
}


//-----------------------------------------------------------------------------
// Does the specified extension match something in the filter list?
//-----------------------------------------------------------------------------
bool CFileOpenDialog::ExtensionMatchesFilter( const char *pExt )
{
	CPanel2D *pSelectedFileType = m_pFileTypeCombo->GetSelected();
	if ( !pSelectedFileType )
		return true;

	char filterList[MAX_FILTER_LENGTH+1];
	Q_strncpy( filterList, pSelectedFileType->GetAttribute("filter", "*"), MAX_FILTER_LENGTH );

	char *filterPtr = filterList;
	while ((filterPtr != NULL) && (*filterPtr != 0))
	{
		// parse the next filter in the list.
		char curFilter[MAX_FILTER_LENGTH];
		curFilter[0] = 0;
		int i = 0;
		while ((filterPtr != NULL) && ((*filterPtr == ',') || (*filterPtr == ';') || (*filterPtr <= ' ')))
		{
			++filterPtr;
		}
		while ((filterPtr != NULL) && (*filterPtr != ',') && (*filterPtr != ';') && (*filterPtr > ' '))
		{
			curFilter[i++] = *(filterPtr++);
		}
		curFilter[i] = 0;

		if (curFilter[0] == 0)
			break;

		if ( !Q_stricmp( curFilter, "*" ) || !Q_stricmp( curFilter, "*.*" ) )
			return true;

		// FIXME: This isn't exactly right, but tough cookies;
		// it assumes the first two characters of the filter are *.
		Assert( curFilter[0] == '*' && curFilter[1] == '.' );
		if ( !Q_stricmp( &curFilter[2], pExt ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Choose the first non *.* filter in the filter list
//-----------------------------------------------------------------------------
void CFileOpenDialog::ChooseExtension( char *pExt, int nBufLen )
{
	pExt[0] = 0;

	CPanel2D *pSelectedFileType = m_pFileTypeCombo->GetSelected();
	if ( !pSelectedFileType )
		return;

	char filterList[MAX_FILTER_LENGTH+1];
	Q_strncpy( filterList, pSelectedFileType->GetAttribute("filter", "*"), MAX_FILTER_LENGTH );

	char *filterPtr = filterList;
	while ((filterPtr != NULL) && (*filterPtr != 0))
	{
		// parse the next filter in the list.
		char curFilter[MAX_FILTER_LENGTH];
		curFilter[0] = 0;
		int i = 0;
		while ((filterPtr != NULL) && ((*filterPtr == ',') || (*filterPtr == ';') || (*filterPtr <= ' ')))
		{
			++filterPtr;
		}
		while ((filterPtr != NULL) && (*filterPtr != ',') && (*filterPtr != ';') && (*filterPtr > ' '))
		{
			curFilter[i++] = *(filterPtr++);
		}
		curFilter[i] = 0;

		if (curFilter[0] == 0)
			break;

		if ( !Q_stricmp( curFilter, "*" ) || !Q_stricmp( curFilter, "*.*" ) )
			continue;

		// FIXME: This isn't exactly right, but tough cookies;
		// it assumes the first two characters of the filter are *.
		Assert( curFilter[0] == '*' && curFilter[1] == '.' );
		Q_strncpy( pExt, &curFilter[1], nBufLen );
		break;
	}
}


//-----------------------------------------------------------------------------
// Saves the file to the start dir context
//-----------------------------------------------------------------------------
void CFileOpenDialog::SaveFileToStartDirContext( const char *pFullPath )
{
	if ( m_nStartDirContext == s_StartDirContexts.InvalidIndex() )
		return;

	char pPath[MAX_PATH];
	pPath[0] = 0;
	Q_ExtractFilePath( pFullPath, pPath, sizeof(pPath) );
	s_StartDirContexts[ m_nStartDirContext ] = pPath;
}


//-----------------------------------------------------------------------------
// Posts a file selected message
//-----------------------------------------------------------------------------
void CFileOpenDialog::PostFileSelectedMessage( const char *pFileName )
{
	m_bFileSelected = true;

	UIEngine()->DispatchEvent( FileOpenDialogFilesSelected::MakeEvent( this, pFileName ) );
	DeleteAsync();
}


//-----------------------------------------------------------------------------
// Posts a multi files selected message
//-----------------------------------------------------------------------------
void CFileOpenDialog::PostMultiFileSelectedMessage()
{
	m_bFileSelected = true;
	
	CUtlString strFiles;
	FOR_EACH_VEC( m_vecSelectedEntries, i )
	{
		if ( i > 0 )
		{
			strFiles.Append( ";" );
		}
		strFiles.Append( m_vecSelectedEntries[i]->GetFileData()->m_FullPath );
	}
	UIEngine()->DispatchEvent( FileOpenDialogFilesSelected::MakeEvent( this, strFiles.Get() ) );

	DeleteAsync();
}


//-----------------------------------------------------------------------------
// Selects the current folder
//-----------------------------------------------------------------------------
void CFileOpenDialog::OnSelectFolder()
{
	ValidatePath();

	// construct a file path
	char pFileName[MAX_PATH];
	GetSelectedFileName( pFileName, sizeof( pFileName ) );

	Q_StripTrailingSlash( pFileName );

	if ( !V_strcmp(pFileName, "..") )
	{
		MoveUpFolder();

		// clear the name text
		m_pFileNameTextEntry->SetText("");
		return;
	}

	if ( !V_strcmp(pFileName, ".") )
	{
		// clear the name text
		m_pFileNameTextEntry->SetText("");
		return;
	}

	// Compute the full path
	char pFullPath[MAX_PATH * 4];
	if ( !Q_IsAbsolutePath( pFileName ) )
	{
		GetDirectory(pFullPath, sizeof(pFullPath) - MAX_PATH);
		strcat( pFullPath, pFileName );
		if ( !pFileName[0] )
		{
			Q_StripTrailingSlash( pFullPath );
		}
	}
	else
	{
		Q_strncpy( pFullPath, pFileName, sizeof(pFullPath) );
	}

	if ( g_pFullFileSystem->FileExists( pFullPath ) )
	{
		// open the file!
		SaveFileToStartDirContext( pFullPath );
		PostFileSelectedMessage( pFullPath );
		return;
	}

	PopulateDriveList();
	PopulateFileList();
}


//-----------------------------------------------------------------------------
// Purpose: Handle the open button being pressed
//			checks on what has changed and acts accordingly
//-----------------------------------------------------------------------------
void CFileOpenDialog::OnOpen()
{
	ValidatePath();

	// construct a file path
	char pFileName[MAX_PATH];
	GetSelectedFileName( pFileName, sizeof( pFileName ) );

	int nLen = Q_strlen( pFileName );
	bool bSpecifiedDirectory = ( pFileName[nLen-1] == '/' || pFileName[nLen-1] == '\\' );
	Q_StripTrailingSlash( pFileName );

	if ( !V_strcmp(pFileName, "..") )
	{
		MoveUpFolder();
		
		// clear the name text
		m_pFileNameTextEntry->SetText("");
		return;
	}

	if ( !V_strcmp(pFileName, ".") )
	{
		// clear the name text
		m_pFileNameTextEntry->SetText("");
		return;
	}
	 
	// Compute the full path
	char pFullPath[MAX_PATH * 4];
	if ( !Q_IsAbsolutePath( pFileName ) )
	{
		GetDirectory(pFullPath, sizeof(pFullPath) - MAX_PATH);
		strcat(pFullPath, pFileName);
		if ( !pFileName[0] )
		{
			Q_StripTrailingSlash( pFullPath );
		}
	}
	else
	{
		Q_strncpy( pFullPath, pFileName, sizeof(pFullPath) );
	}

	// If the name specified is a directory, then change directory
	if ( g_pFullFileSystem->IsDirectory( pFullPath, NULL ) )
	{
		// it's a directory; change to the specified directory
		if ( !bSpecifiedDirectory )
		{
			strcat( pFullPath , CORRECT_PATH_SEPARATOR_S );
		}
		SetStartDirectory( pFullPath );

		// clear the name text
		m_pFileNameTextEntry->SetText("");
		m_CurrentSubstringFilter = "";

		PopulateDriveList();
		PopulateFileList();
		return;
	}
	else if ( bSpecifiedDirectory )
	{
		PopulateDriveList();
		PopulateFileList();
		return;
	}

	// If multiple files are selected and this is a multi-open dialog, post multi open message
	if( m_DialogType == FOD_OPEN_MULTIPLE && pFileName[0] != '\0' )
	{
		PostMultiFileSelectedMessage();
		return;
	}

	m_CurrentSubstringFilter = pFileName;

	if ( m_DialogType != FOD_SAVE )
	{
		if ( m_CurrentSubstringFilter.Length() > 0 )
		{
			if ( m_DialogType == FOD_OPEN && g_pFullFileSystem->FileExists( pFullPath ) )
			{
				// open the file!
				SaveFileToStartDirContext( pFullPath );
				PostFileSelectedMessage( pFullPath );
				return;
			}

			// It's ambiguous
			int nMatches = CountSubstringMatches();
			if ( nMatches >= 2 )
			{
				// Apply filter instead
				FilterFileList();
				return;
			}
		}
	}
	
	// Append suffix of the first filter that isn't *.*
	char extension[512];
	Q_ExtractFileExtension( pFullPath, extension, sizeof(extension) );
	if ( !ExtensionMatchesFilter( extension ) )
	{
		ChooseExtension( extension, sizeof(extension) );
		Q_SetExtension( pFullPath, extension, sizeof(pFullPath) );
	}

	if ( g_pFullFileSystem->FileExists( pFullPath ) )
	{
		// open the file!
		SaveFileToStartDirContext( pFullPath );
		PostFileSelectedMessage( pFullPath );
		return;
	}

	// file not found
	if ( ( m_DialogType == FOD_SAVE ) && pFileName[0] )
	{
		// open the file!
		SaveFileToStartDirContext( pFullPath );
		PostFileSelectedMessage( pFullPath );
		return;
	}

	PopulateDriveList();
	PopulateFileList();
}

void CFileOpenDialog::DeselectAllEntries()
{
	FOR_EACH_VEC( m_vecSelectedEntries, i )
	{
		m_vecSelectedEntries[i]->SetSelected( false );
	}
	m_vecSelectedEntries.RemoveAll();
}

bool CFileOpenDialog::EventSelectFile( const CPanelPtr< IUIPanel > &pPanel, uint32 unModifiers )
{
	CFileOpenDialogEntry *pEntry = assert_cast< CFileOpenDialogEntry* >( pPanel->ClientPtr() );

	if ( m_DialogType != FOD_OPEN_MULTIPLE )
	{
		DeselectAllEntries();
		pEntry->SetSelected( true );
		m_vecSelectedEntries.AddToTail( pEntry );
		m_pFileNameTextEntry->SetText( pEntry->GetFileData()->m_FileName.Get() );
	}
	else
	{
		if ( ( unModifiers & MODIFIER_LCONTROL ) || ( unModifiers & MODIFIER_RCONTROL ) )
		{
			if ( pEntry->IsSelected() )
			{
				pEntry->SetSelected( false );
				m_vecSelectedEntries.FindAndRemove( pEntry );
			}
			else
			{
				pEntry->SetSelected( true );
				m_vecSelectedEntries.AddToTail( pEntry );
			}
		}
		else if ( ( unModifiers & MODIFIER_LSHIFT ) || ( unModifiers & MODIFIER_RSHIFT ) )
		{
			// TODO: Remember last selected panel and select all panels in between
			DbgAssert( !"Not implemented" );
		}
		else
		{
			DeselectAllEntries();
			pEntry->SetSelected( true );
			m_vecSelectedEntries.AddToTail( pEntry );
		}
		
		CUtlString m_strCompoundFileName;
		FOR_EACH_VEC( m_vecSelectedEntries, i )
		{
			if ( i > 0 )
			{
				m_strCompoundFileName.Append( ";" );
			}
			m_strCompoundFileName.Append( m_vecSelectedEntries[i]->GetFileData()->m_FileName.Get() );
		}
		m_pFileNameTextEntry->SetText( m_strCompoundFileName.Get() );
	}
	
	return true;
}

bool CFileOpenDialog::EventDoubleClickFile( const CPanelPtr< IUIPanel > &pPanel, uint32 unModifiers )
{
	OnOpen();
	return true;
}

void CFileOpenDialog::OnMatchStringSelected()
{
	char pFileName[MAX_PATH];
	GetSelectedFileName( pFileName, sizeof( pFileName ) );

	m_CurrentSubstringFilter = pFileName;

	// Redo filter
	FilterFileList();
}

bool CFileOpenDialog::EventOpen()
{
	OnOpen();
	return true;
}

bool CFileOpenDialog::EventCancel()
{
	UIEngine()->DispatchEvent( FileOpenDialogFilesSelected::MakeEvent( this, "" ) );
	DeleteAsync();
	return true;
}

bool CFileOpenDialog::EventClose()
{
	UIEngine()->DispatchEvent( FileOpenDialogFilesSelected::MakeEvent( this, "" ) );
	DeleteAsync();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle an item in the Drive combo box being selected
//-----------------------------------------------------------------------------
bool CFileOpenDialog::EventFullPathChanged()
{
	CLabel *pSelected = assert_cast<CLabel*>( m_pFullPathDropDown->GetSelected() );
	if ( !pSelected )
		return true;

	DeselectAllEntries();
	m_pFileNameTextEntry->SetText( "" );
	V_strcpy_safe( m_szLastPath, pSelected->GetAttribute( "Directory", "" ) );
	OnOpen();
	return true;
}

bool CFileOpenDialog::EventFilterChanged()
{
	PopulateFileList();
	return true;
}

//==========================================================================================

REGISTER_PANEL2D_FACTORY( CFileOpenDialogEntry, FileOpenDialogEntry );

CFileOpenDialogEntry::CFileOpenDialogEntry( CPanel2D *parent, const char * pchPanelID ) : CButton( parent, pchPanelID )
{
	AddClass( "FileRow" );

	m_bCreatedControls = false;

	RegisterEventHandler( ScrolledIntoView(), this, &CFileOpenDialogEntry::OnScrolledIntoView );
}

CFileOpenDialogEntry::~CFileOpenDialogEntry()
{
}

void CFileOpenDialogEntry::SetFileData( FileData_t *pFileData )
{
	m_FileData = *pFileData;
}

bool CFileOpenDialogEntry::OnScrolledIntoView( const CPanelPtr< IUIPanel > &panelPtr )
{
	if ( !IsScrolledIntoView() )
		return true;

	if ( !m_bCreatedControls )
	{
		// create cells

		CLabel *pName = new CLabel( this, NULL );
		if ( m_FileData.m_bDirectory )
		{
			pName->AddClasses( "Filename Cell Directory" );
		}
		else
		{
			pName->AddClasses( "Filename Cell" );
		}

		CLabel *pSize = new CLabel( this, NULL );
		pSize->AddClasses( "Filesize Cell" );

		CLabel *pType = new CLabel( this, NULL );
		pType->AddClasses( "Filetype Cell" );

		CLabel *pDate = new CLabel( this, NULL );
		pDate->AddClasses( "Date Cell" );

		// populate cells
		pName->SetText( m_FileData.m_FileName.Get(), CLabel::k_ETextTypeUnlocalized );
		pDate->SetText( m_FileData.m_LastWriteTime.Get() );

		if ( !m_FileData.m_bDirectory )
		{
			pSize->SetText( V_pretifymem( (float)m_FileData.m_nFileSize, 0, true ) );
			pType->SetText( m_FileData.m_FileType.Get() );
		}
		else
		{
			pType->SetText( "#FileOpenDialog_FileType_Folder" );
		}
		m_bCreatedControls = true;
	}
	return true;
}

bool CFileOpenDialogEntry::OnMouseButtonUp( const panorama::MouseData_t &code )
{
	IUIEvent *pEvent = FileOpenDialogSelectFile::MakeEvent( this, code.m_Modifiers );
	UIEngine()->DispatchEvent( pEvent );
	return BaseClass::OnMouseButtonUp( code );
}

bool CFileOpenDialogEntry::OnMouseButtonDoubleClick( const panorama::MouseData_t &code )
{
	IUIEvent *pEvent = FileOpenDialogDoubleClickFile::MakeEvent( this, code.m_Modifiers );
	UIEngine()->DispatchEventAsync( 0.0001f, pEvent );
	return BaseClass::OnMouseButtonDoubleClick( code );
}

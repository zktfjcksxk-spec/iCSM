//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "panorama/layout/stylefiletypes.h"
#include "stylefile.h"
#include "panorama/layout/stylesymbol.h"
#ifdef SOURCE2_PANORAMA
#include "enumutils_panorama.h"
#else
#include "enumutils.h"
#endif
#include "../uipanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

namespace panorama
{ 
extern ConVar g_cvarDeveloper;
}



#ifdef DEBUG_TENFOOT_STYLES
#define SPEW_STYLE Msg
#else
#define SPEW_STYLE( a, ... ) REFERENCE(a);
#endif

// C++ syntax for this is brutal. Hide it in a define
#define CALL_POINTER_MEMBER_FUNCTION( pObject, pMemberFunction ) ((*pObject).*(pMemberFunction))

//-----------------------------------------------------------------------------
// Purpose: Appends style flags (EStyleFlags) to a format string
//-----------------------------------------------------------------------------
extern void AppendStyleFlagsToString( CFmtStr1024 *pfmt, uint unStyleFlags );
extern EStyleFlags EStyleFlagsFromName( const char *pchName );

//-----------------------------------------------------------------------------
// Purpose: Helper to log parsing errors
//-----------------------------------------------------------------------------
void panorama::LogLayoutParsingError( CPanoramaSymbol symPath, uint32 unLineNumber, const char *pchMsg, ... )
{	
	va_list args;
	va_start( args, pchMsg );

	CFmtStr1024 fmtStr( "***** Parsing error on %s(%u): ", symPath.String(), unLineNumber );
	fmtStr.AppendFormatV( pchMsg, args );
	fmtStr.Append( "\n" );
#ifdef SOURCE2_PANORAMA
	Log_Warning( LOG_PANORAMA, "%s", fmtStr.String() );
	fprintf( stderr, "%s", fmtStr.String() );
#else
	Msg( "%s", fmtStr.String() );
#endif

	bool bShowMessageBox = panorama::g_cvarDeveloper.GetBool();

#ifdef SOURCE2_PANORAMA
	// Determine if it is an async layout request, and don't show popups if
	// it is (since we iterate too quickly on the async layouts and don't
	// want to require people to sync super frequently).
	const char *pszPath = symPath.String();
	if ( ( V_stristr( pszPath, "http:" ) == pszPath ) || ( V_stristr( pszPath, "https:" ) == pszPath ) )
		bShowMessageBox = false;
#endif

	if ( bShowMessageBox )
		UIEngine()->ShowNativeTopMostMessageBox( fmtStr, "Error parsing layout and style files", IUIEngine::k_ENativeMessageOk );

	va_end( args );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to calculate the current line number in a text buffer. Slow.
//-----------------------------------------------------------------------------
uint32 panorama::CalcBufferLine( CUtlBuffer &buffer )
{
	Assert( buffer.IsText() );
	if ( !buffer.IsValid() )
		return 0;

	// see backward counting line numbers
	int nCurrentPos = buffer.TellGet();
	uint32 unLineCount = 0;
	for ( int i = 1; i <= nCurrentPos; i++ )
	{		
		const char *pPeek = (const char *)buffer.PeekGet( 1, -i );
		if ( !pPeek )
			return 0;

		if ( *pPeek == '\n' )
			unLineCount++;
	}

	// add one as count is 0 based
	return unLineCount + 1;
}


//-----------------------------------------------------------------------------
// Purpose: Determines of the specified character is valid for a define name
//-----------------------------------------------------------------------------
inline bool BValidDefineChar( char c )
{
	return (c >= 'A' && c <= 'Z') || (c >='a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}


//-----------------------------------------------------------------------------
// Purpose: Determines of the specified string is valid for a define name
//-----------------------------------------------------------------------------
bool BValidDefineName( const char *pchName )
{
	while ( *pchName != '\0' )
	{
		if ( !BValidDefineChar( *pchName) )
			return false;

		pchName++;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: copies set to another
//-----------------------------------------------------------------------------
void CStyleFileSet::MergeTo( CStyleFileSet *pStyleFileSet ) const
{
	pStyleFileSet->Clear();
	pStyleFileSet->m_vecStyleFiles.EnsureCapacity( m_vecStyleFiles.Count() );
	
	FOR_EACH_VEC( m_vecStyleFiles, i )
	{
		pStyleFileSet->AddStyleFile( m_vecStyleFiles[ i ] );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CStyleFileSet::Clear()
{	
	m_vecStyleFiles.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: adds a style file to the end of our style file list
//-----------------------------------------------------------------------------
void CStyleFileSet::AddStyleFile( StyleFilePtr_t pStyleFile )
{
	m_vecStyleFiles.AddToTail( pStyleFile );
}


//-----------------------------------------------------------------------------
// Purpose: Looks up the most recent define 
//-----------------------------------------------------------------------------
const char *CStyleFileSet::GetDefine( const char *pchDefine ) const
{
	FOR_EACH_VEC_BACK( m_vecStyleFiles, i )
	{
		const char *pchValue = m_vecStyleFiles[i]->GetDefine( pchDefine );
		if ( pchValue )
			return pchValue;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Looks up a keyframe by name
//-----------------------------------------------------------------------------
const CStyleAnimation *CStyleFileSet::GetAnimation( CPanoramaSymbol symKeyFrame ) const
{
	FOR_EACH_VEC_BACK( m_vecStyleFiles, i )
	{
		const CStyleAnimation *pAnimation = m_vecStyleFiles[i]->GetAnimation( symKeyFrame );
		if ( pAnimation )
			return pAnimation;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Appends a string identifying the selector for debugging
//-----------------------------------------------------------------------------
void AppendDebugStyleName( CFmtStr1024 &fmt, const CStyleSelector &selector )
{
	if ( selector.GetPanelType().IsValid() )
		fmt.Append( selector.GetPanelType().String() );

	if ( selector.GetID() )
		fmt.AppendFormat( "#%s", selector.GetID() );

	const CUtlPtrArray< CPanoramaSymbol > &arrayClasses = selector.GetClasses();
	FOR_EACH_PTR_ARRAY( arrayClasses, i )
	{
		fmt.AppendFormat( ".%s", arrayClasses[i].String() );
	}

	AppendStyleFlagsToString( &fmt, selector.GetStyleFlags() );	

	const CStyleSelector *pNotSelector = selector.GetNotSelector();
	if ( pNotSelector )
	{
		fmt.Append( ":not(" );
		AppendDebugStyleName( fmt, *pNotSelector );
		fmt.Append( ")" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a string to identify the style for debugging
//-----------------------------------------------------------------------------
const char *GetDebugStyleName( const StyleFromFile_t *pStyleFromFile )
{
	static CFmtStr1024 fmt;
	fmt.Clear();

	// add parents
	FOR_EACH_PTR_ARRAY( pStyleFromFile->m_parentSelectors, i )
	{
		AppendDebugStyleName( fmt, pStyleFromFile->m_parentSelectors[i] );
		fmt.Append( " " );
	}

	// append element
	AppendDebugStyleName( fmt, pStyleFromFile->m_selector );
	
	return fmt.Access();
}


//-----------------------------------------------------------------------------
// Purpose: calculates selector specificity for style
//-----------------------------------------------------------------------------
uint CalculateIndividualSelectorSpecificity( const CStyleSelector &selector )
{
	// See CSS TR 6.4.3 for more info explanation. 4 counts turn into a 32-bit value that can be compared
	uint unRet = 0;

	// count number of id	
	if ( selector.GetID() )
		unRet |= (1 << 16);

	// count attributes. Pseudo-classes are included in this bucket (we only support 1 pseudo-class per selector)
	uint cPsuedoClasses = (selector.GetStyleFlags() != k_EStyleFlagNone ) ? 1 : 0;
	unRet |= ((selector.GetClasses().Count() + cPsuedoClasses) << 8);

	// count number of element names
	if ( selector.GetPanelType().IsValid() )
		unRet += 1;

	// Add the :not selector's specificity
	const CStyleSelector *pNotSelector = selector.GetNotSelector();
	if ( pNotSelector )
	{
		unRet += CalculateIndividualSelectorSpecificity( *pNotSelector );
	}

	return unRet;
}

//-----------------------------------------------------------------------------
// Purpose: calculates selector specificity for style
//-----------------------------------------------------------------------------
uint CalculateSelectorSpecificity( const StyleFromFile_t *pStyleFromFile )
{
	VPROF_BUDGET_DETAILED( "CalculateSelectorSpecificity", VPROF_BUDGETGROUP_TENFOOT );
	uint unRet = CalculateIndividualSelectorSpecificity( pStyleFromFile->m_selector );

	FOR_EACH_PTR_ARRAY( pStyleFromFile->m_parentSelectors, i )
	{
		unRet += CalculateIndividualSelectorSpecificity( pStyleFromFile->m_parentSelectors[i] );
	}

	return unRet;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the specified selector matches the specified panel
//			DOES NOT CHECK PARENTS or the embedded :not selector
//-----------------------------------------------------------------------------
bool BIndividualSelectorMatchesPanelCore( const CStyleSelector &selector, CPanoramaSymbol symPanelType, int nPanelFlags, const CPanoramaSymbol *psymClasses, uint csymClasses, const char *pchID )
{
	VPROF_BUDGET_DETAILED( "BIndividualSelectorMatchesPanel", VPROF_BUDGETGROUP_TENFOOT );
	// lots to check.. do quick comparisons before slow

	// make sure our panel types match
	if ( selector.GetPanelType().IsValid() && selector.GetPanelType() != symPanelType )
		return false;
	
	// make sure id matches
	if ( selector.GetID() && V_stricmp( selector.GetID(), pchID ) != 0 )
		return false;

	// check pseudo-classes match
	if ( selector.GetStyleFlags() != k_EStyleFlagNone )
	{
		// to make styling easier, if a panel has focus, we will also match styles which require descendantfocus
		if ( nPanelFlags & k_EStyleFlagFocus )
			nPanelFlags |= k_EStyleFlagDescendantFocused;

		// if you're not disabled, you're enabled
		if ( !( nPanelFlags & k_EStyleFlagDisabled ) )
			nPanelFlags |= k_EStyleFlagEnabled;

		uint unMatchingFlags = ( selector.GetStyleFlags() & nPanelFlags );
		if ( unMatchingFlags != (uint)selector.GetStyleFlags() )
			return false;
	}

	// check required classes
	if ( selector.GetClasses().Count() > 0 )
	{
		VPROF_BUDGET_DETAILED( "BIndividualSelectorMatchesPanel - classes", VPROF_BUDGETGROUP_TENFOOT );
		const CUtlPtrArray< CPanoramaSymbol > &arrayClasses = selector.GetClasses();
		FOR_EACH_PTR_ARRAY( arrayClasses, iSelector )
		{
			if ( !psymClasses )
				return false;

			bool bFound = false;
			for ( uint iPanel = 0; iPanel < csymClasses; iPanel++ )
			{
				if ( psymClasses[iPanel] == arrayClasses[iSelector] )
				{
					bFound = true;
					break;
				}
			}

			if ( !bFound )
				return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the specified selector matches the specified panel
//			DOES NOT CHECK PARENTS
//-----------------------------------------------------------------------------
bool BIndividualSelectorMatchesPanel( const CStyleSelector &selector, CPanoramaSymbol symPanelType, int nPanelFlags, const CPanoramaSymbol *psymClasses, uint csymClasses, const char *pchID )
{
	if ( !BIndividualSelectorMatchesPanelCore( selector, symPanelType, nPanelFlags, psymClasses, csymClasses, pchID ) )
		return false;

	const CStyleSelector *pNotSelector = selector.GetNotSelector();
	if ( pNotSelector )
	{
		if ( BIndividualSelectorMatchesPanelCore( *pNotSelector, symPanelType, nPanelFlags, psymClasses, csymClasses, pchID ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if the specified selector matches the specified panel. Includes checking parents
//-----------------------------------------------------------------------------
bool CStyleFileSet::BSelectorMatchesPanel( const StyleFromFile_t *pStyleFromFile, const CPanelIdentifiers &panelID )
{
	VPROF_BUDGET_DETAILED( "BSelectorMatchesPanel", VPROF_BUDGETGROUP_TENFOOT );
	if ( !BIndividualSelectorMatchesPanel( pStyleFromFile->m_selector, panelID.m_symPanelType, panelID.m_unStyleFlags, panelID.m_psymClasses, panelID.m_csymClasses, panelID.m_pchID ) )
		return false;

	// check that parent selectors match
	if ( pStyleFromFile->m_parentSelectors.Count() == 0 )
		return true;

	// If we don't have a panel, then parent selectors cannot match, fail now.
	if ( !panelID.m_pPanel )
		return false;

	MapParentsByType_t *pMapParentsByType = NULL;
	MapParentsByClass_t *pMapParentsByClass = NULL;
	MapParentsByID_t *pMapParentsByID = NULL;

	CUIPanel *pLastMatched = (CUIPanel*)(panelID.m_pPanel);
	bool bTreatLastMatchedAsParent = panelID.m_bTreatPanelAsParent;
	FOR_EACH_PTR_ARRAY_BACK( pStyleFromFile->m_parentSelectors, i )
	{
		// need to match another selector.. make sure we have a parent
		Assert( pLastMatched );
		if ( !bTreatLastMatchedAsParent && !pLastMatched->GetParent() )
			return false;

		const CStyleSelector &selector = pStyleFromFile->m_parentSelectors[i];

		// instead of looping through parents, can quickly handle required child/parent matches (.A > .B)
		if ( selector.BChildMatchesNextStyle() )
		{
			CUIPanel *pThisParent = (bTreatLastMatchedAsParent) ? pLastMatched : (CUIPanel*)pLastMatched->GetParent();
			if ( !BIndividualSelectorMatchesPanel( selector, pThisParent->ClientPtr()->GetPanelType(), pThisParent->GetStyleFlags(), pThisParent->GetClasses().Base(), pThisParent->GetClasses().Count(), pThisParent->GetID() ) )
				return false;

			pLastMatched = pThisParent;
			bTreatLastMatchedAsParent = false;
			continue;
		}

		bool bFound = false;
		bool bDoSlowWalk = true;
		// Do not build "parent lookup" maps when "descendant selector" filter (bloom filter)
		// is active. Most selector will be filtered in CStyleFile::AddMatchingStylesCore
		// if the filter is active. (May need to revisit if the false positive rate of 
		// the bloom filter is high)
		if ( !CStyleFileDescendantFilter::BIsActive() && !panelID.m_bTreatPanelAsParent )
		{
			if ( selector.GetPanelType().IsValid() )
			{
				bDoSlowWalk = false;
				pLastMatched->GetParentLookupMaps( &pMapParentsByType, NULL, NULL );
				int iMap = pMapParentsByType->FindFirst( selector.GetPanelType() );
				while ( iMap != pMapParentsByType->InvalidIndex() )
				{
					IUIPanel *pThisParent = pMapParentsByType->Element( iMap );
					if ( pLastMatched->IsDescendantOf( pThisParent )
						&& BIndividualSelectorMatchesPanel( selector, pThisParent->ClientPtr()->GetPanelType(), pThisParent->GetStyleFlags(), pThisParent->GetClasses().Base(), pThisParent->GetClasses().Count(), pThisParent->GetID() ) )
					{
						bFound = true;
						pLastMatched = (CUIPanel*)pThisParent;
						break;
					}

					iMap = pMapParentsByType->NextInorderSameKey( iMap );
				}
			}
			else if ( selector.GetID() )
			{
				pLastMatched->GetParentLookupMaps( NULL, &pMapParentsByID, NULL );
				bDoSlowWalk = false;
				int iMap = pMapParentsByID->FindFirst( selector.GetID() );
				while ( iMap != pMapParentsByID->InvalidIndex() )
				{
					IUIPanel *pThisParent = pMapParentsByID->Element( iMap );
					if ( pLastMatched->IsDescendantOf( pThisParent )
						&& BIndividualSelectorMatchesPanel( selector, pThisParent->ClientPtr()->GetPanelType(), pThisParent->GetStyleFlags(), pThisParent->GetClasses().Base(), pThisParent->GetClasses().Count(), pThisParent->GetID() ) )
					{
						bFound = true;
						pLastMatched = (CUIPanel*)pThisParent;
						break;
					}

					iMap = pMapParentsByID->NextInorderSameKey( iMap );
				}
			}
			else if ( selector.GetClasses().Count() )
			{
				bDoSlowWalk = false;

				pLastMatched->GetParentLookupMaps( NULL, NULL, &pMapParentsByClass );
				int iMap = pMapParentsByClass->FindFirst( selector.GetClasses()[0] );
				while ( iMap != pMapParentsByClass->InvalidIndex() )
				{
					IUIPanel *pThisParent = pMapParentsByClass->Element( iMap );
					if ( pLastMatched->IsDescendantOf( pThisParent )
						&& BIndividualSelectorMatchesPanel( selector, pThisParent->ClientPtr()->GetPanelType(), pThisParent->GetStyleFlags(), pThisParent->GetClasses().Base(), pThisParent->GetClasses().Count(), pThisParent->GetID() ) )
					{
						bFound = true;
						pLastMatched = (CUIPanel*)pThisParent;
						break;
					}

					iMap = pMapParentsByClass->NextInorderSameKey( iMap );
				}
			}
		}

		if ( bDoSlowWalk )
		{
			CUIPanel *pThisParent = (bTreatLastMatchedAsParent) ? pLastMatched : (CUIPanel*)pLastMatched->GetParent();
			while ( pThisParent != NULL )
			{
				if ( BIndividualSelectorMatchesPanel( pStyleFromFile->m_parentSelectors[i], pThisParent->ClientPtr()->GetPanelType(), pThisParent->GetStyleFlags(), pThisParent->GetClasses().Base(), pThisParent->GetClasses().Count(), pThisParent->GetID() ) )
				{
					pLastMatched = pThisParent;
					bFound = true;
					break;
				}

				pThisParent = (CUIPanel*)(pThisParent->GetParent());
			}
		}

		if ( !bFound )
			return false;

		// Leave pParent modified, the next selector should be matched higher in the parent hierarchy than this last one
		bTreatLastMatchedAsParent = false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Builds a list of all matching styles for the specified panel
//-----------------------------------------------------------------------------
void CStyleFileSet::BuildMatchingStyleList( CLayoutFile *pLayoutFile, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, const CPanelIdentifiers &panelID, IUILayoutFile *pIPreviousLayoutFile )
{
	VPROF_BUDGET_DETAILED( "CStyleFileSet::BuildMatchingStyleList", VPROF_BUDGETGROUP_TENFOOT );

	CLayoutFile *pPreviousLayoutFile = (CLayoutFile *)pIPreviousLayoutFile;

	// need to check each style file
	FOR_EACH_VEC( m_vecStyleFiles, iStyleFile )
	{
		StyleFilePtr_t pStyleFile = m_vecStyleFiles[iStyleFile];

		// if the previously loaded layout file contained the specified file.. skip. Don't want to do double the work for no gain.
		if ( pPreviousLayoutFile && pPreviousLayoutFile->BContainsStyleFile( pStyleFile->GetPathSymbol() ) )
			continue;

		// check for classes
		if ( panelID.m_psymClasses )
		{
			for ( uint iPanel = 0; iPanel < panelID.m_csymClasses; iPanel++ )
			{
				// Add styles that include the class we are currently searching for as the FIRST requirement. If the class we are searching for
				// is the 2nd+ required class, we will pick up those during another iteration
				pStyleFile->AddStylesMatchingClass( pLayoutFile, panelID, panelID.m_psymClasses[ iPanel ], vecStyles, iStyleFile );
			}
		}		

		// check for panel type
		if ( panelID.m_symPanelType.IsValid() )		// should always be true
		{
			// will only return panels for just our type that have no class, but could have an id requirement
			pStyleFile->AddStylesMatchingPanelTypeNoClass( pLayoutFile, panelID, panelID.m_symPanelType, vecStyles, iStyleFile );
		}

		// check for ID
		if ( panelID.m_pchID && panelID.m_pchID[0] != '\0' )
		{
			// will only return panels for just our type that have no class or id requirements, so add them all
			pStyleFile->AddStylesMatchingJustID( pLayoutFile, panelID, panelID.m_pchID, vecStyles, iStyleFile );
		}
	}

	{
		VPROF_BUDGET_DETAILED( "Sort Styles Cascade Order", VPROF_BUDGETGROUP_TENFOOT );
		vecStyles.SortPredicate( [ pIPreviousLayoutFile ]( const CascadeStyleFileInfo_t &lhs, const CascadeStyleFileInfo_t &rhs ) {

			// First sort by selector specificity
			if ( lhs.m_unSelectorSpecificity != rhs.m_unSelectorSpecificity )
				return lhs.m_unSelectorSpecificity < rhs.m_unSelectorSpecificity;

			if ( lhs.m_pLayoutFile == rhs.m_pLayoutFile )
			{
				// If the styles are within the same layout file, sort by the order in which they appeared in the file.
				return lhs.m_pStyleFromFile->m_unFileOrder < rhs.m_pStyleFromFile->m_unFileOrder;
			}
			else
			{
				// If the styles are not within the same layout file, prefer styles from the current layout file
				// instead of previous layout file
				Assert( pIPreviousLayoutFile != nullptr );
				return lhs.m_pLayoutFile == pIPreviousLayoutFile;
			}
		} );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Applies a list of styles to the provided panel style
// Returns: true if styles changed, false otherwise
//-----------------------------------------------------------------------------
bool CStyleFileSet::ApplyMatchedStylesToPanelStyle( CPanelStyle *pPanelStyle, const CUtlVector< CascadeStyleFileInfo_t > &vecStyles, EStyleRepaint &eRepaint, bool &bInheritablePropertiesChanged )
{
	VPROF_BUDGET_DETAILED( "CStyleFileSet::ApplyMatchedStylesToPanelStyle",  VPROF_BUDGETGROUP_TENFOOT );

	eRepaint = k_EStyleRepaintNone;

	// output order for debugging
#ifdef DEBUG_TENFOOT_STYLES
	FOR_EACH_VEC_BACK( vecStyles, i )
	{
		SPEW_STYLE( "* %s, %d, %d (count=%d)\n", GetDebugStyleName( vecStyles[i].m_pStyleFromFile ), vecStyles[i].m_iStyleFile, vecStyles[i].m_pStyleFromFile->m_unFileLocation, vecStyles[i].m_pStyleFromFile->m_pProperties->Count() );
	}
#endif

	// walk ordered list and build properties to apply. Can track when a property is fully set and skip if found again. Stop once all properties are complete.
//	VPROF_BUDGET( "ApplyMatchedStylesToPanelStyle - walking vecStyles",  VPROF_BUDGETGROUP_TENFOOT );

	CStyleProperty *rgStyleProperties[MAX_PANORAMA_STYLE_SYMBOLS];
	V_memset( rgStyleProperties, 0, sizeof( rgStyleProperties ) );

	const CUtlVector< StyleEntry_t > &vecFromCode = pPanelStyle->PropertiesSetFromElement();
	FOR_EACH_VEC( vecFromCode, i )
	{
		CStyleProperty *pNewProperty = CStylePropertyFactory::CopyStyleProperty( *vecFromCode[i].m_pStyleProperty );
		rgStyleProperties[pNewProperty->GetPropertySymbol().GetID()] = pNewProperty;

		SPEW_STYLE("*** Property %s on element from code/inline style, will still merge with CSS styles if not fully set\n", pNewProperty->GetPropertySymbol().String());
	}

	FOR_EACH_VEC_BACK( vecStyles, iStyle )
	{
		SPEW_STYLE( "** Applying Style %s, %d, %d (count=%d)\n", GetDebugStyleName( vecStyles[iStyle].m_pStyleFromFile ), vecStyles[iStyle].m_iStyleFile, vecStyles[iStyle].m_pStyleFromFile->m_unFileLocation, vecStyles[iStyle].m_pStyleFromFile->m_pProperties->Count() );
		StylePropertyHash_t *pProperties = vecStyles[iStyle].m_pStyleFromFile->m_pProperties;
		if ( !pProperties )
			continue;

		FOR_EACH_HASHMAP( *pProperties, iProperty )
		{
			CStyleProperty *pProperty = pProperties->Element( iProperty );

			// Look at what we've already got to set new so far
			CStyleProperty *pNewProperty = rgStyleProperties[pProperty->GetPropertySymbol().GetID()];

			// look up existing property
			if ( pNewProperty )
			{
				// check if we have already fully set this property
				if ( pNewProperty->BFullySet() )
				{
					SPEW_STYLE( "*** Skipping property %s\n", pProperty->GetPropertySymbol().String() );
					continue;
				}
				else
				{
					pProperty->MergeTo( pNewProperty );
					SPEW_STYLE( "*** Merged property %s\n", pProperty->GetPropertySymbol().String() );
				}
			}
			else
			{
				pNewProperty = CStylePropertyFactory::CopyStyleProperty( *pProperty );
				rgStyleProperties[pProperty->GetPropertySymbol().GetID()] = pNewProperty;
				SPEW_STYLE( "*** Added property %s\n", pProperty->GetPropertySymbol().String() );
			}
		}
	}

//	VPROF_BUDGET( "ApplyMatchedStylesToPanelStyle - starting apply",  VPROF_BUDGETGROUP_TENFOOT );
	// bugbug cboyd - that was expensive.. cache results?
	bool bChanged = false;

	// we need to special case when we apply transition property
	CStylePropertyTransitionProperties *pTransitionProperty = (CStylePropertyTransitionProperties*)rgStyleProperties[CStylePropertyTransitionProperties::symbol.GetID()];
	bool bFoundTransitionProperty = false;
	if ( pTransitionProperty )
	{
		rgStyleProperties[CStylePropertyTransitionProperties::symbol.GetID()] = NULL;
		bFoundTransitionProperty = true;
	}

	// if the transition property is marked as immediate, apply before touching other properties
	if ( pTransitionProperty && pTransitionProperty->m_bImmediate )
	{
		if ( pPanelStyle->SetPropertyFromStyle( pTransitionProperty ) )
		{
			bChanged = true;
		}

		pTransitionProperty = NULL;
	}

	// remove any styles which are no longer set
	bool rgPropertiesToRemove[MAX_PANORAMA_STYLE_SYMBOLS];
	V_memset( rgPropertiesToRemove, 0, sizeof( rgPropertiesToRemove ) );

	pPanelStyle->BuildListOfExistingPropsNotInTree( rgStyleProperties, rgPropertiesToRemove );
	for( int i=0; i < V_ARRAYSIZE( rgPropertiesToRemove ); ++i )
	{
		if ( rgPropertiesToRemove[i] )
		{
			// dont touch styles set from code/element or transition
			CStyleSymbol symProperty( (short)i );
			if ( symProperty == CStylePropertyTransitionProperties::symbol || pPanelStyle->BPropertySetFromElement( symProperty ) )
				continue;

			SPEW_STYLE( "*** Removing property from panel style %s\n", symProperty.String() );
			if ( pPanelStyle->RemoveProperty( symProperty ) )
			{
				eRepaint = k_EStyleRepaintFull;
				bChanged = true;
			}


			// Hack: we early out OnContentSizeTraverse on panels with opacity = 0, so if opacity is changing
			// we need to re-run they layout traverse on that panel.
			if( symProperty == CStylePropertyOpacity::symbol && pPanelStyle->AccessPanel() )
			{
				pPanelStyle->AccessPanel()->InvalidateSizeAndPosition();
			}

			if ( !bInheritablePropertiesChanged && CStylePropertyFactory::BCanInheritProperty( symProperty ) )
				bInheritablePropertiesChanged = true;
		}
	}

	// Hacky, but we must process the ui-scale property first. This is because in the labels case where there is no panel
	// to provide the ui-scale, the other properties might rely on it
	CStylePropertyUIScale *pUIScaleProperty = ( CStylePropertyUIScale * )rgStyleProperties[ CStylePropertyUIScale::symbol.GetID() ];
	if ( pUIScaleProperty )
	{
		if ( pPanelStyle->SetPropertyFromStyle( pUIScaleProperty ) )
		{
			if ( pUIScaleProperty->BAffectsCompositionOnly() && eRepaint != k_EStyleRepaintFull )
				eRepaint = k_EStyleRepaintComposition;
			else
				eRepaint = k_EStyleRepaintFull;

			if ( !bInheritablePropertiesChanged && CStylePropertyFactory::BCanInheritProperty( pUIScaleProperty->GetPropertySymbol() ) )
				bInheritablePropertiesChanged = true;

			bChanged = true;
		}
		rgStyleProperties[ CStylePropertyUIScale::symbol.GetID() ] = nullptr;
	}
	
//	VPROF_BUDGET( "ApplyMatchedStylesToPanelStyle - set property",  VPROF_BUDGETGROUP_TENFOOT );
	// for all properties that differ from value currently set, update panel style

	typedef bool ( CPanelStyle::*SetPropertyFromStyleMemFn )( CStyleProperty *pProperty );
	SetPropertyFromStyleMemFn pSetPropertyFromStyleFn = &CPanelStyle::SetPropertyFromStyle;
	// Special case if we don't have any transition properties and will not apply any transition properties
	if ( !bFoundTransitionProperty && !pPanelStyle->FindProperty( CStylePropertyTransitionProperties::symbol ) && !pPanelStyle->m_treePropertiesInTransition.Count() )
	{
		pSetPropertyFromStyleFn = &CPanelStyle::SetPropertyFromStyleSimple;
	}

	for( int i=0; i < V_ARRAYSIZE( rgStyleProperties ); ++i )
	{
		panorama::CStyleProperty *pStyleProp = rgStyleProperties[ i ];
		if ( pStyleProp && CALL_POINTER_MEMBER_FUNCTION( pPanelStyle, pSetPropertyFromStyleFn )( pStyleProp ) )
		{
			if ( pStyleProp->BAffectsCompositionOnly() && eRepaint != k_EStyleRepaintFull )
				eRepaint = k_EStyleRepaintComposition;
			else
				eRepaint = k_EStyleRepaintFull;

			if ( !bInheritablePropertiesChanged && CStylePropertyFactory::BCanInheritProperty( pStyleProp->GetPropertySymbol() ) )
				bInheritablePropertiesChanged = true;

			// Hack: we early out OnContentSizeTraverse on panels with opacity = 0, so if opacity is changing
			// we need to re-run they layout traverse on that panel.
			if( pStyleProp->GetPropertySymbol() == CStylePropertyOpacity::symbol && pPanelStyle->AccessPanel() )
			{
				pPanelStyle->AccessPanel()->InvalidateSizeAndPosition();
			}

			bChanged = true;
		}
	}

	// VPROF_BUDGET( "ApplyMatchedStylesToPanelStyle - cleanup",  VPROF_BUDGETGROUP_TENFOOT );
	// Don't need to remove/delete ptrs in rgStyleProperties, panelstyle owns them now!

	// if we no longer have a transition property set, clear. If not set to immediate, set now that all other style have been applied
	if ( !bFoundTransitionProperty )
	{
		if ( pPanelStyle->RemoveProperty( CStylePropertyTransitionProperties::symbol ) )
		{
			bChanged = true;
		}
	}
	else if ( pTransitionProperty && !pTransitionProperty->m_bImmediate )
	{
		if ( pPanelStyle->SetPropertyFromStyle( pTransitionProperty ) )
		{
			bChanged = true;
		}

		pTransitionProperty = NULL;
	}

	Assert( pTransitionProperty == NULL );

	return bChanged;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if set includes specified style file
//-----------------------------------------------------------------------------
bool CStyleFileSet::BContainsStyleFile( CPanoramaSymbol symPath ) const
{
	VPROF_BUDGET( "CStyleFileSet::BContainsStyleFile", VPROF_BUDGETGROUP_TENFOOT );

	FOR_EACH_VEC_BACK( m_vecStyleFiles, i )
	{
		if ( m_vecStyleFiles[ i ]->GetPathSymbol() == symPath )
			return true;

		if ( m_vecStyleFiles[ i ]->BImportsStyleFile( symPath ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if any of the style files loaded have a "descendant selector" 
//			with the given style flag and matching the panel (id, type, classes, ...)
//			Used to filter which panel subtree to invalidate when adding / removing style flags
//-----------------------------------------------------------------------------
bool CStyleFileSet::BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const
{
	// need to check each style file
	FOR_EACH_VEC( m_vecStyleFiles, iStyleFile )
	{
		StyleFilePtr_t pStyleFile = m_vecStyleFiles[iStyleFile];

		if ( pStyleFile->BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, nOldPanelFlags, nNewPanelFlags, panel, vecVisitedStyleFiles ) )
		{
			return true;
		}
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if any of the style files loaded have a "descendant selector" 
//			with the given classes and matching the panel (id, type, classes, ...)
//			Used to filter which panel subtree to invalidate when adding / removing style flags
//-----------------------------------------------------------------------------
bool CStyleFileSet::BHasAnyDescendantSelectorMatchingClasses( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const
{
	// need to check each style file
	FOR_EACH_VEC( m_vecStyleFiles, iStyleFile )
	{
		StyleFilePtr_t pStyleFile = m_vecStyleFiles[iStyleFile];

		if ( pStyleFile->BHasAnyDescendantSelectorMatchingClasses( arrChangedClasses, arrOldClasses, arrNewClasses, panel, vecVisitedStyleFiles ) )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CStyleFileKey::CStyleFileKey( const CStyleFileSet &styleSet, CPanoramaSymbol symPath )
{
	Init( styleSet, symPath );
}


//-----------------------------------------------------------------------------
// Purpose: Clears key info
//-----------------------------------------------------------------------------
void CStyleFileKey::Clear()
{
	m_pKey.Purge();	
}


//-----------------------------------------------------------------------------
// Purpose: Inits key with path from style file set + newest symbol
//-----------------------------------------------------------------------------
void CStyleFileKey::Init( const CStyleFileSet &styleSet, CPanoramaSymbol symPath )
{
	Clear();

	const CUtlVector< StyleFilePtr_t > &vecStyleFiles = styleSet.GetStyleFiles();
	m_pKey.Allocate( vecStyleFiles.Count() + 1 );

	FOR_EACH_VEC( vecStyleFiles, i )
	{
		m_pKey[i] = vecStyleFiles[i]->GetPathSymbol();
	}
	m_pKey[ m_pKey.Count() - 1] = symPath;
}


//-----------------------------------------------------------------------------
// Purpose: Appends to key
//-----------------------------------------------------------------------------
void CStyleFileKey::Append( CPanoramaSymbol symPath )
{
	CUtlPtrArray< CPanoramaSymbol > pArray;
	pArray.Allocate( m_pKey.Count() + 1 );
	V_memcpy( pArray.Base(), m_pKey.Base(), m_pKey.Count() * sizeof( CPanoramaSymbol ) );
	pArray[m_pKey.Count()] = symPath;

	m_pKey.Swap( pArray );
}


//-----------------------------------------------------------------------------
// Purpose: Checks if this key contains the specified style file
//-----------------------------------------------------------------------------
bool CStyleFileKey::BContainsStyleFile( CPanoramaSymbol symPath ) const
{
	FOR_EACH_PTR_ARRAY( m_pKey, i )
	{
		if ( m_pKey[i] == symPath )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: less than operator
//-----------------------------------------------------------------------------
bool CStyleFileKey::operator<( const CStyleFileKey &rhs) const
{
	int cMin = MIN( this->m_pKey.Count(), rhs.m_pKey.Count() );
	
	for ( int i = 0; i < cMin; i++ )
	{
		if ( m_pKey[i] < rhs.m_pKey[i] )
			return true;
		if ( rhs.m_pKey[i] < m_pKey[i] )
			return false;
	}

	// equal up until cMin. Key with less symbols is smaller
	return m_pKey.Count() < rhs.m_pKey.Count();
}


//-----------------------------------------------------------------------------
// Purpose: equality check
//-----------------------------------------------------------------------------
bool CStyleFileKey::operator==( const CStyleFileKey &rhs ) const
{
	if ( m_pKey.Count() != rhs.m_pKey.Count() )
		return false;

	for ( int i = m_pKey.Count() - 1; i >= 0; --i )
	{
		if ( m_pKey[i] != rhs.m_pKey[i] )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Validate
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CStyleFileKey::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_pKey );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CStyleFile::CStyleFile()
	: m_unMaxFileOrder( 0 )
{
}


//-----------------------------------------------------------------------------
// Purpose: deconstructor
//-----------------------------------------------------------------------------
CStyleFile::~CStyleFile()
{
	Clear();
}


//-----------------------------------------------------------------------------
// Purpose: clears parsed style values, not path & fileset
//-----------------------------------------------------------------------------
void CStyleFile::Clear()
{
	for ( int nStyleFlag = k_EStyleFlagMinBit; nStyleFlag < k_EStyleFlagMaxBit; ++nStyleFlag )
	{
		m_arrDescendantSelectorsMatchingStyleFlag[nStyleFlag].RemoveAll();
	}

	FOR_EACH_HASHMAP( m_mapDescendantSelectorsMatchingClass, i )
	{
		delete m_mapDescendantSelectorsMatchingClass.Element( i );
	}
	m_mapDescendantSelectorsMatchingClass.RemoveAll();
	
	m_dictDefines.Purge();

	FOR_EACH_HASHTABLE( m_classStyles, i )
	{
		delete m_classStyles.Element( i );
	}
	m_classStyles.RemoveAll();

	FOR_EACH_HASHTABLE( m_typeStyles, i )
	{
		delete m_typeStyles.Element( i );
	}
	m_typeStyles.RemoveAll();

	FOR_EACH_HASHTABLE( m_idStyles, i )
	{
		delete m_idStyles.Element( i );
	}
	m_idStyles.RemoveAll();

	FOR_EACH_MAP_FAST( m_mapAnimations, i )
	{
		delete m_mapAnimations.Element( i );
	}
	m_mapAnimations.RemoveAll();

	m_vecImportedStyleFiles.RemoveAll();

	m_unMaxFileOrder = 0;
}



//-----------------------------------------------------------------------------
// Purpose: loads a style from a file
//-----------------------------------------------------------------------------
ELoadLayoutFileResult CStyleFile::LoadFromFile( const char *pchFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder )
{
	m_symStylePath = CPanoramaSymbol( pchFile );
	return Reload( vecPrevStyleFiles, unFileOrder );
}


//-----------------------------------------------------------------------------
// Purpose: loads a style from a file
//-----------------------------------------------------------------------------
ELoadLayoutFileResult CStyleFile::Reload( const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder )
{
	// load into buffer
	CUtlBuffer buffer;
	if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( m_symStylePath.String(), buffer, true ) )
	{
		LogLayoutParsingError( m_symStylePath, 0, "Could not load file" );
		return k_ELoadLayoutFileReadFailed;
	}

	// parse
	return BReloadLoadFromBuffer( buffer, vecPrevStyleFiles, unFileOrder ) ? k_ELoadLayoutFileOK : k_ELoadLayoutFileFailed;
}


//-----------------------------------------------------------------------------
// Purpose: loads a style from a buffer
//-----------------------------------------------------------------------------
bool CStyleFile::BLoadFromBuffer( CUtlBuffer &buffer, CPanoramaSymbol pchStyleFilePath, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder )
{
	m_symStylePath = CPanoramaSymbol( pchStyleFilePath );
	return BReloadLoadFromBuffer( buffer, vecPrevStyleFiles, unFileOrder );
}


//-----------------------------------------------------------------------------
// Purpose: loads a style from a buffer
//-----------------------------------------------------------------------------
bool CStyleFile::BReloadLoadFromBuffer( CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder )
{
	VPROF_BUDGET( "CStyleFile::BLoadFromBuffer", VPROF_BUDGETGROUP_TENFOOT );
	Clear();

	m_unMaxFileOrder = unFileOrder;

	char rgchToken[2048];
	while ( buffer.IsValid() && buffer.GetBytesRemaining() > 0 )
	{
		// save the byte offset into the file where the next section starts. Used to identify a style
		CSSHelpers::EatCSSIgnorables( buffer );
		int unSectionStart = buffer.TellGet();

		// check
		char cNextChar = 0;
		if( !CSSHelpers::BPeekCSSToken( buffer, &cNextChar ) )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Failed to parse selector" );
			buffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
			return false;
		}

		// when parsing a style, we want the entire line as that is the style selector. When parsing an at-rule, we need to walk token by token
		const char *pchTerm = k_rgchCSSSelectorTerm;
		uint cchTerm = V_ARRAYSIZE( k_rgchCSSSelectorTerm );
		if ( cNextChar == '@' )
		{
			pchTerm = k_rgchCSSAtRuleNameTerm;
			cchTerm = V_ARRAYSIZE( k_rgchCSSAtRuleNameTerm );
		}

		// get selector. Selectors can contain ':' and spaces, so include
		if ( !CSSHelpers::BReadCSSToken( buffer, rgchToken, V_ARRAYSIZE( rgchToken ), pchTerm, cchTerm ) )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Failed to parse selector" );
			buffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
			return false;
		}

		// hit end of file?
		// bugbug cboyd - is there a better way to test for end of file?
		if ( !buffer.IsValid() )
		{
			// found garbage at the end of the file
			if ( rgchToken[0] != '\0' )
			{
				LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Invalid data found at end of file: %s", rgchToken );
				buffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
				return false;
			}

			break;
		}
		
		// check if css rule
		if ( rgchToken[0] == '@' )
		{
			if ( !BParseCSSAtRule( rgchToken, unSectionStart, buffer, vecPrevStyleFiles ) )
			{
				buffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
				return false;
			}
		}
		else
		{
			// should be style
			if ( !BParseCSSStyle( rgchToken, unSectionStart, buffer, vecPrevStyleFiles ) )
			{
				buffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
				return false;
			}
		}		
	}

	buffer.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	unFileOrder = m_unMaxFileOrder;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: parses a CSS at-rule
//-----------------------------------------------------------------------------
bool CStyleFile::BParseCSSAtRule( const char *pchToken, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles )
{
	// currently only support @define.. our custom rule
	if ( V_stricmp( pchToken, "@define" ) == 0 )
		return BParseCSSDefine( pchToken, buffer, vecPrevStyleFiles );
	else if ( V_stricmp( pchToken, "@keyframes" ) == 0 )
		return BParseCSSKeyframes( pchToken, unSectionStart, buffer, vecPrevStyleFiles );
	else if ( V_stricmp( pchToken, "@import" ) == 0 )
		return BParseCSSImport( pchToken, unSectionStart, buffer, vecPrevStyleFiles );

	LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Found unsupported CSS at-rule: %s", pchToken );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Parses our custom define at-rule
//-----------------------------------------------------------------------------
bool CStyleFile::BParseCSSDefine( const char *pchToken, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles )
{
	// format should be "@define <name>: <value>;"
	char rgchBuffer[1024];

	// get name.. can't be empty
	if ( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] == '\0' )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing @define" );
		return false;
	}

	// save name
	char rgchName[256];
	V_strncpy( rgchName, rgchBuffer, V_ARRAYSIZE( rgchName ) );

	if ( !BValidDefineName( rgchName ) )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "@define name is invalid (use A-Z,a-z,_-): %s", rgchName );
		return false;
	}

	// should be followed by ':'
	if( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] != ':' )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "@define missing colon" );
		return false;
	}

	// get value.. can't be empty
	if ( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ), k_rgchCSSValueTerm, V_ARRAYSIZE( k_rgchCSSValueTerm ) ) || rgchBuffer[0] == '\0' )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing @define value" );
		return false;
	}

	// every value can be a define.. check our defines up to this point in the file, and defines from previous layout files
	if ( !BReplaceDefines( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), this, vecPrevStyleFiles, 0xFFFFFFFF ) )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "An error occurred while replacing defines @define value" );
		return false;
	}

	// save define
	AddDefine( rgchName, rgchBuffer );

	// skip ;
	if( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] != ';' )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "@define missing semicolon" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Compare to CStyleKeyFrame pointers
//-----------------------------------------------------------------------------
bool panorama::StyleKeyFrameLessPtr( const CStyleKeyFramePtr &lhs, const CStyleKeyFramePtr &rhs, void *pCtx )
{
	return *lhs < *rhs;
}


//-----------------------------------------------------------------------------
// Purpose: Purge and delete a property tree
//-----------------------------------------------------------------------------
void PurgeAndDeletePropTree( StylePropertyHash_t *ptree )
{
	FOR_EACH_HASHMAP( *ptree, i )
	{
		CStylePropertyFactory::FreeStyleProperty( ptree->Element( i ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check if keyframe properties are specified correctly
//-----------------------------------------------------------------------------
bool BValidateStylesForKeyframe( StylePropertyHash_t *pProperties, CUtlString* pErrors )
{
	CStylePropertyPosition* pPosition = LookupStylePropertyInMap<CStylePropertyPosition>( pProperties );

	if ( pPosition && !pPosition->BFullySet() )
	{
		*pErrors = "Position not fully set: specify all of 'x', 'y', and 'z', or use 'position' property";
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses a CSS Keyframe at-rule
//
// Looks something like:
//		@keyframes 'diagonal-slide' {
//			0% {
//				left: 0;
//				top: 0;
//			}
//
//			50% {
//				left: 25px;
//				top: 15px;
//			}
//
//			100% {
//				left: 100px;
//				top: 100px;
//			}
//		}
//-----------------------------------------------------------------------------
bool CStyleFile::BParseCSSKeyframes( const char *pchToken, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles )
{
	// get name
	char rgchBuffer[1024];
	if( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing @keyframe name" );
		return false;		
	}

	// strip single quotes. name shouldn't be empty 
	char* tokenName = rgchBuffer;
	int cchName = V_strlen( tokenName );

	// REI: Quoting animation names are optional (improves VS CSS editing)
	if ( rgchBuffer[0] == '\'' )
	{
		// require end quote
		if ( cchName < 2 || rgchBuffer[cchName - 1] != '\'' )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Invalid @keyframe name '%s' missing end-quote\n", tokenName );
			return false;
		}

		// strip quotes
		tokenName[cchName - 1] = '\0';
		tokenName++;

		cchName -= 2;
	}

	if ( !cchName )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Eempty @keyframe name\n" );
		return false;
	}

	CPanoramaSymbol symName( tokenName );

	// parse {
	if( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] != '{' )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Missing opening brace for @keyframe" );
		return false;
	}

	// add new animation. Replace existing animation if necessary (last one wins)
	short iMap = m_mapAnimations.Find( symName );
	if ( iMap == m_mapAnimations.InvalidIndex() )
		iMap = m_mapAnimations.Insert( symName, new CStyleAnimation( symName, m_symStylePath, unSectionStart, ++m_unMaxFileOrder ) );

	CStyleAnimation *pAnimation = m_mapAnimations.Element( iMap );
	pAnimation->ClearFrames();

	// keep parsing keyframe selectors until }
	while ( true )
	{
		// get next token. Can't be empty
		if( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] == '\0' )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing style" );
			return false;
		}
		
		// end?
		if ( rgchBuffer[0] == '}' )
			break;

		// special case from and to
		float flPercent = k_flFloatNotSet;
		if ( V_stricmp( rgchBuffer, "from" ) == 0 )
		{
			flPercent = 0;
		}
		else if ( V_stricmp( rgchBuffer, "to" ) == 0 )
		{
			flPercent = 100;
		}
		else if ( !CSSHelpers::BParsePercent( &flPercent, rgchBuffer ) )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing @keyframe selector: %s", rgchBuffer );
			return false;
		}

		// now parse styles for this keyframe
		StylePropertyHash_t *pProperties = new StylePropertyHash_t();
		if ( !BParseStyleBody( buffer, pProperties, m_symStylePath, this, vecPrevStyleFiles, false ) )
		{			
			PurgeAndDeletePropTree( pProperties );
			SAFE_DELETE( pProperties );
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Failed to parse properties for keyframe: %s", rgchBuffer );
			return false;
		}

		// Validate that styles are fully set when needed
		CUtlString strParseError;
		if ( !BValidateStylesForKeyframe( pProperties, &strParseError ) )
		{
			PurgeAndDeletePropTree( pProperties );
			SAFE_DELETE( pProperties );
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Invalid properties for keyframe: %s (%s)", rgchBuffer, strParseError.Get() );
			return false;
		}

		// if we found an animation block, pull the timing function which can be overridden per frame
		short iAnimProp = pProperties->Find( CStylePropertyAnimationProperties::symbol );
		EAnimationTimingFunction eFrameTimingFunction = k_EAnimationUnset;
		CCubicBezierCurve< Vector2D > cubicBezier;

		Vector2D vec[4];
		panorama::GetAnimationCurveControlPoints( k_EAnimationUnset, vec );
		cubicBezier.SetControlPoints( vec );

		if ( iAnimProp != pProperties->InvalidIndex() )
		{
			CStylePropertyAnimationProperties *pAnimationProperty = (CStylePropertyAnimationProperties*)pProperties->Element( iAnimProp );
			if ( pAnimationProperty->m_vecAnimationProperties.Count() > 0 )
			{
				eFrameTimingFunction = pAnimationProperty->m_vecAnimationProperties[0].m_eTimingFunction;
				cubicBezier = pAnimationProperty->m_vecAnimationProperties[0].m_CubicBezier;
			}

			pProperties->RemoveAt( iAnimProp );
			CStylePropertyFactory::FreeStyleProperty( pAnimationProperty );
		}

		// make sure all properties can interpolate
		FOR_EACH_HASHMAP( *pProperties, i )
		{
			CStyleProperty *pProperty = pProperties->Element( i );
			if ( !pProperty->BCanTransition() )
			{
				LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "%s property listed in a keyframe but marked as cannot transition", pProperty->GetPropertySymbol().String() );
				PurgeAndDeletePropTree( pProperties );
				SAFE_DELETE( pProperties );
				return false;
			}
		}

		// looks good. add frame or replace if already exists (no cascading in @keyframes). CStyleKeyFrame will take ownership of the PropertyTree memory.
		pAnimation->InsertFrame( new CStyleKeyFrame( flPercent, eFrameTimingFunction, cubicBezier, pProperties ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parse a CSS @import statement
//-----------------------------------------------------------------------------
bool CStyleFile::BParseCSSImport( const char *pchToken, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles )
{
	// get file path
	char rgchBuffer[ 1024 ];
	if ( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing @import value" );
		return false;
	}

	// parse the URL
	CUtlString strURL;
	if ( !CSSHelpers::BParseURL( strURL, rgchBuffer ) )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing @import url '%s'", rgchBuffer );
		return false;
	}

	CPanoramaSymbol symPath;
	if ( !UIEngineInternal()->UILayoutManagerInternal()->BNormalizeStyleFilePath( strURL.Get(), symPath ) )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Failed to normalize @import url '%s'", strURL.Get() );
		return false;
	}

	if ( symPath == m_symStylePath )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "A style file cannot import itself: %s trying to import '%s'", m_symStylePath.String(), strURL.Get() );
		return false;
	}

	// Build up a style file set that includes all the previous files 
	CStyleFileSet styleFileSet;
	for ( const StyleFilePtr_t &pPrevStyleFile : vecPrevStyleFiles )
	{
		if ( pPrevStyleFile->GetPathSymbol() == symPath )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "%s is trying to import %s, but that file has already been included either from the layout file directly, or from a different @import.", m_symStylePath.String(), strURL.Get() );
			return false;
		}

		styleFileSet.AddStyleFile( pPrevStyleFile );
	}

	// Include ourselves so that @defines found so far take effect
	styleFileSet.AddStyleFile( this );

	// Actually load the style file, but don't cache the result in the layout manager
	uint unImportFileOrder = m_unMaxFileOrder + 1;
	StyleFilePtr_t pStyleFile = UIEngineInternal()->UILayoutManagerInternal()->GetStyleFile( strURL.Get(), styleFileSet, unImportFileOrder, false );
	if ( !pStyleFile )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Unable to @import style file '%s'", strURL.Get() );
		return false;
	}
	m_unMaxFileOrder = unImportFileOrder;

	// Add it to our list of imports
	ImportedStyleFile_t *pImportedStyleFile = &m_vecImportedStyleFiles.Element( m_vecImportedStyleFiles.AddToTail() );
	pImportedStyleFile->pStyleFile = pStyleFile;
	pImportedStyleFile->unImportedFileLocation = unSectionStart;
	
	// skip ;
	if ( !CSSHelpers::BReadCSSToken( buffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[ 0 ] != ';' )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "@import missing semicolon" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Recursively check whether this style file imports the given style
// file name.
//-----------------------------------------------------------------------------
bool CStyleFile::BImportsStyleFile( CPanoramaSymbol symPath ) const
{
	for ( const ImportedStyleFile_t import : m_vecImportedStyleFiles )
	{
		if ( import.pStyleFile->GetPathSymbol() == symPath )
			return true;

		if ( import.pStyleFile->BImportsStyleFile( symPath ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Given the start of a nested selector string, pull out the entire
// string representing that selector.  Will handle nested parentheses
//-----------------------------------------------------------------------------
bool BParseNestedSelector( const char *pchSelector, char *pchOut, int cchOut, int *piNext )
{
	*piNext = 0;
	char *pchOutCur = pchOut;
	*pchOutCur = '\0';

	// Assume they've already skipped the initial '('
	int nParendDepth = 1;

	// Look for the matching parenthesis
	while ( nParendDepth > 0 )
	{
		char c = *pchSelector++;
		if ( c == '\0' )
			return false;

		if ( cchOut == 0 )
			return false;

		*pchOutCur = c;
		pchOutCur++;
		cchOut--;

		if ( c == '(' )
		{
			nParendDepth++;
		}
		else if ( c == ')' )
		{
			nParendDepth--;
		}
	}

	// Back up one for the final parenthesis, then null terminate
	pchOutCur--;
	*pchOutCur = '\0';

	*piNext = pchOutCur - pchOut + 1;
	return true;
}


// Forward declaration since we will call this recursively
bool BParseIndividualSelector( const char *pchSelector, int cchSelector, CStyleSelector *pSelector );

//-----------------------------------------------------------------------------
// Purpose: Parses a single CSS Style Selector (no parents!)
//
// Looks something like:
//		[panel_type][#<id>][.class_name]*[:style_flags|:not(selector)]*
//-----------------------------------------------------------------------------
bool BParseIndividualSelector( const char *pchSelector, int cchSelector, CUtlString *pstrID, CPanoramaSymbol *psymPanelType, CUtlVector< CPanoramaSymbol > *pvecClasses, EStyleFlags *peStyleFlags, CStyleSelector **ppNotSelector )
{
	if ( cchSelector == 0 )
		return false;

	// copy to buffer for easy parsing
	char rgchStyleBuf[1024];
	if ( cchSelector >= V_ARRAYSIZE( rgchStyleBuf ) )
		return false;

	V_memcpy( rgchStyleBuf, pchSelector, cchSelector );
	rgchStyleBuf[cchSelector] = '\0';
	char *pchCurrent = rgchStyleBuf;

	// check for panel type
	int iCurrent = V_strcspn( pchCurrent, ".#:" );
	char cNext = '\0';
	if ( iCurrent != 0 )
	{
		// terminate
		cNext = pchCurrent[iCurrent];
		pchCurrent[iCurrent] = '\0';
		
		*psymPanelType = CPanoramaSymbol( pchCurrent );
		pchCurrent += (iCurrent + 1);
	}
	else
	{		
		cNext = *pchCurrent;
		pchCurrent++;
	}

	// parse ID if present
	if ( cNext == '#' )
	{
		// find first style class if present and terminate
		iCurrent = V_strcspn( pchCurrent, ".:" );
		cNext = pchCurrent[iCurrent];
		pchCurrent[iCurrent] = '\0';
		
		pstrID->Set( pchCurrent );
		pchCurrent += (iCurrent + 1);
	}

	// grab all classes
	while ( cNext == '.' )
	{
		iCurrent = V_strcspn( pchCurrent, ".:" );
		cNext = pchCurrent[iCurrent];
		pchCurrent[iCurrent] = '\0';

		CPanoramaSymbol symClass( pchCurrent );
		pvecClasses->AddToTail( symClass );
		pchCurrent += (iCurrent + 1);
	}

	// grab all psuedo-classes	
	int unStyleFlags = k_EStyleFlagNone;
	while ( cNext == ':' )
	{
		iCurrent = V_strcspn( pchCurrent, ":(" );
		cNext = pchCurrent[iCurrent];
		pchCurrent[iCurrent] = '\0';

		// Check for :not( selector )
		if ( cNext == '(' )
		{
			if ( V_strcmp( pchCurrent, "not" ) == 0 )
			{
				if ( *ppNotSelector )
				{
					// Don't currently support multiple :not selectors. It'd be more correct to support it that so you could do things like
					// :not(#SomeID):not(#SomeOtherID), but then we'd need a vector of not selectors which doesn't seem worth it right now
					return false;
				}

				char rgchNotSelector[ 1024 ];
				int iNext = 0;
				if ( !BParseNestedSelector( pchCurrent + iCurrent + 1, rgchNotSelector, V_ARRAYSIZE( rgchNotSelector ), &iNext ) )
					return false;

				CStyleSelector *pNotSelector = new CStyleSelector();
				if ( !BParseIndividualSelector( rgchNotSelector, V_strlen( rgchNotSelector ), pNotSelector ) )
				{
					delete pNotSelector;
					return false;
				}

				*ppNotSelector = pNotSelector;
				iCurrent += iNext + 1;
				cNext = pchCurrent[ iCurrent ];
			}
			else
			{
				return false;
			}
		}
		else
		{
			EStyleFlags eStyleFlag = EStyleFlagsFromName( pchCurrent );
			if ( eStyleFlag == k_EStyleFlagNone )
				return false;

			unStyleFlags |= ( int )eStyleFlag;
		}

		pchCurrent += ( iCurrent + 1 );
	}
	*peStyleFlags = (EStyleFlags)unStyleFlags;

	return ( cNext == '\0' );
}


//-----------------------------------------------------------------------------
// Purpose: Parses a single CSS Style Selector (no parents!)
//-----------------------------------------------------------------------------
bool BParseIndividualSelector( const char *pchSelector, int cchSelector, CStyleSelector *pSelector )
{
	CUtlString strID;
	CPanoramaSymbol symPanelType;
	CUtlVector< CPanoramaSymbol > vecClasses;
	EStyleFlags eStyleFlags = k_EStyleFlagNone;
	CStyleSelector *pNotSelector = nullptr;
	if ( !BParseIndividualSelector( pchSelector, cchSelector, &strID, &symPanelType, &vecClasses, &eStyleFlags, &pNotSelector ) )
	{
		delete pNotSelector;
		return false;
	}

	pSelector->SetPanelType( symPanelType );
	pSelector->SetStyleFlags( eStyleFlags );
	pSelector->SetNotSelector( pNotSelector );

	if ( !strID.IsEmpty() )
		pSelector->SetID( strID.String() );

	if ( vecClasses.Count() > 0 )
		pSelector->SetClasses( vecClasses.Base(), vecClasses.Count() );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Finds next character that is space or returns index of end of string (like strcspn)
//-----------------------------------------------------------------------------
int FindNextSpace( const char *pchSelector )
{
	int i = 0;
	while ( !V_isspace( pchSelector[i] ) && pchSelector[i] != '\0' )
		i++;
	
	return i;
}


//-----------------------------------------------------------------------------
// Purpose: Skips combinator and spaces
//			Combinators are >, + and ~
//-----------------------------------------------------------------------------
const char *SkipCombinatorAndSpaces( const char *pchSelector, char *pFoundCombinator )
{
	if ( pFoundCombinator )
		*pFoundCombinator = '\0';

	pchSelector = CSSHelpers::SkipSpaces( pchSelector );
	if ( pchSelector[0] == '\0' || pchSelector[1] != ' ' )
		return pchSelector;

	if ( pchSelector[0] == '>' || pchSelector[0] == '+' || pchSelector[0] == '~' )
	{
		if ( pFoundCombinator )
			*pFoundCombinator = pchSelector[0];
	}

	return CSSHelpers::SkipSpaces( ++pchSelector );
}


//-----------------------------------------------------------------------------
// Purpose: Counts the number of selectors (parents + for element) in a string
//-----------------------------------------------------------------------------
int CountDescendantSelectorComponents( const char *pchSelector )
{
	int cSelectors = 0;

	pchSelector = CSSHelpers::SkipSpaces( pchSelector );
	while ( pchSelector[0] != '\0' )
	{
		cSelectors++;
		int iSpace = FindNextSpace( pchSelector );
		pchSelector = SkipCombinatorAndSpaces( &pchSelector[iSpace], NULL );
	}

	return cSelectors;
}


//-----------------------------------------------------------------------------
// Purpose: Parses entire selector (parents + element)
//-----------------------------------------------------------------------------
bool CStyleFile::BParseSelector( const char *pchSelector, CUtlBuffer &buffer, StyleFromFile_t *pStyleFromFile )
{
	// count how many selectors we have so we can preallocate and know when we are at the last style
	int cSelectors = CountDescendantSelectorComponents( pchSelector );
	if ( cSelectors == 0 )
		return false;

	if ( cSelectors > 1 )
		pStyleFromFile->m_parentSelectors.Allocate( cSelectors - 1 );

	// pchSelector should be passed with white space trimmed. Search for the end of each selector, and add to vector
	int iIteration = 0;
	const char *pchCurrent = CSSHelpers::SkipSpaces( pchSelector );
	while ( pchCurrent[0] != '\0' )
	{
		int iSpace = FindNextSpace( pchCurrent );

		// get a pointer to the selector we are updating this iteration
		CStyleSelector *pSelector = (iIteration == cSelectors - 1) ? &pStyleFromFile->m_selector : &pStyleFromFile->m_parentSelectors[ iIteration ];				
		if ( !BParseIndividualSelector( pchCurrent, iSpace, pSelector ) )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Error parsing style selector: %s", pchSelector );
			return false;
		}

		if ( pSelector->GetPanelType().IsValid() && !UIEngine()->BRegisteredPanelType( pSelector->GetPanelType() ) )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Unknown panel type in style selector: %s", pchSelector );
		}

		char chCombinator = '\0';
		pchCurrent = SkipCombinatorAndSpaces( &pchCurrent[iSpace], &chCombinator );

		// check for child combinator
		if ( chCombinator == '>' )
		{
			pSelector->SetChildMatchesNextStyle();
		}
		else if ( chCombinator != '\0' )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Unsupported combinator in selector: %c", chCombinator );
			return false;
		}

		iIteration++;
	}	

	return true;		
}


//-----------------------------------------------------------------------------
// Purpose: Parses a group of selectors. Comma separated
//-----------------------------------------------------------------------------
bool CStyleFile::BCreateStylesForSelectors( CUtlVector< StyleFromFile_t *> *pvecStyles, const char *pchSelectors, CUtlBuffer &buffer )
{
	char rgchBuffer[256];
	const char *pchSelector = NULL;

	const char *pchCurrent = CSSHelpers::SkipSpaces( pchSelectors );
	while ( pchCurrent[0] != '\0' )
	{
		StyleFromFile_t *pStyle = new StyleFromFile_t;
		pvecStyles->AddToTail( pStyle );

		// find end of current selector
		int cch = V_strcspn( pchCurrent, "," );
		if ( cch > V_ARRAYSIZE( rgchBuffer ) )
		{
			LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Selector exceeds temp buffer size: %s", pchSelector );
			return false;
		}

		// only need to copy if not the last selector
		if ( pchCurrent[cch] != '\0' )
		{
			V_strncpy( rgchBuffer, pchCurrent, cch + 1 );
			rgchBuffer[cch] = '\0';
			pchSelector = rgchBuffer;
			pchCurrent += cch + 1;
		}
		else
		{
			pchSelector = pchCurrent;
			pchCurrent += cch;
		}

		if ( !BParseSelector( pchSelector, buffer, pStyle ) )
		{
			pvecStyles->PurgeAndDeleteElements();
			return false;
		}

		pchCurrent = CSSHelpers::SkipSpaces( pchCurrent );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: parses a CSS style
//-----------------------------------------------------------------------------
template <typename KeyT, typename KeyHashT>
static void AddStyleFromFileToHashTable( CUtlHashtable< KeyT, StyleFromFilePointer, KeyHashT > &table, typename ArgumentTypeInfo<KeyT>::Arg_t key,  StyleFromFilePointer pStyleFromFile )
{
	UtlHashHandle_t hHandle = table.Find( key );
	if ( hHandle == table.InvalidHandle() )
	{
		table.Insert( key, pStyleFromFile );
	}
	else
	{
		// A css selector can be defined twice in a file, with css selectors in the middle
		// Add the new css selector at the end of the list
		StyleFromFile_t *pInTree = table.Element( hHandle );
		while ( pInTree->m_pNext != NULL )
			pInTree = pInTree->m_pNext;

		pInTree->m_pNext = pStyleFromFile;
	}
}

bool CStyleFile::BParseCSSStyle( const char *pchSelector, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles )
{
	SPEW_STYLE( "%s - Found style %s at %u\n", m_symStylePath.String(), pchSelector, unSectionStart );
	
	// create style info
	CUtlVector< StyleFromFile_t * > vecSelectors;
	if ( !BCreateStylesForSelectors( &vecSelectors, pchSelector, buffer ) )
		return false;

	if ( vecSelectors.Count() == 0 )
	{
		LogLayoutParsingError( m_symStylePath, CalcBufferLine( buffer ), "Found no selectors: %s", pchSelector );
		return false;
	}

	// process the created styles
	FOR_EACH_VEC( vecSelectors, i )
	{
		StyleFromFile_t *pStyleFromFile = vecSelectors[i];
		pStyleFromFile->m_pProperties = new StylePropertyHash_t( 12 );
		pStyleFromFile->m_unFileLocation = unSectionStart;
		pStyleFromFile->m_unFileOrder = ++m_unMaxFileOrder;

		// Insert css selector in to one of m_classStyles, m_typeStyles, m_idStyles hash table:
		//   * m_classStyles - selectors with classes (may have type, may have id). First class used as a key
		//	 * m_typeStyles - selectors with type and no classes (may have id). Type used as a key
		//	 * m_idStyles - selectors with id and no type and no classes. Id used as a key
		// See if an entry for this style already exists in our tree. It is ok for a style file to define 
		// the same selector multiple times.. but we need to order each definition
		if ( ( pStyleFromFile->m_selector.GetClasses().Count() > 0 ) && ( pStyleFromFile->m_selector.GetClasses()[0].IsValid() ) )
		{
			CPanoramaSymbol symClass = pStyleFromFile->m_selector.GetClasses()[0];
			AddStyleFromFileToHashTable( m_classStyles, symClass, pStyleFromFile );
		}
		else if ( pStyleFromFile->m_selector.GetPanelType().IsValid() )
		{
			CPanoramaSymbol symType = pStyleFromFile->m_selector.GetPanelType();
			AddStyleFromFileToHashTable( m_typeStyles, symType, pStyleFromFile );
		}
		else if ( pStyleFromFile->m_selector.GetID() )
		{
			const char *pchId = pStyleFromFile->m_selector.GetID();
			AddStyleFromFileToHashTable( m_idStyles, pchId, pStyleFromFile );
		}
		else
		{
			Warning( "CSS selector with no class / type / id - ignored!\n" );
			Assert( 0 );
		}
	}

	// parse the style body into the first style
	StylePropertyHash_t *pProperties = vecSelectors[0]->m_pProperties;
	if ( !BParseStyleBody( buffer, pProperties, m_symStylePath, this, vecPrevStyleFiles, false ) )
		return false;

	// copy the parsed properties into the other styles in the group. While the copied properties are expensive, they are still probably cheaper
	// than even adding ref counted overhead to each StyleFromFile_t
	for( int iSelector = 1; iSelector < vecSelectors.Count(); iSelector++ )
	{
		StylePropertyHash_t *pCopyTo = vecSelectors[iSelector]->m_pProperties;
		pCopyTo->EnsureCapacity( pProperties->Count() );
		
		FOR_EACH_HASHMAP( *pProperties, iProp )
		{
			CStyleProperty *pFrom = pProperties->Element( iProp );
			CStyleProperty *pTo = CStylePropertyFactory::CreateStyleProperty( pFrom->GetPropertySymbol() );
			pFrom->MergeTo( pTo );
			pCopyTo->Insert( pTo->GetPropertySymbol(), pTo );
		}
	}

	CollectDescendantSelectors( vecSelectors );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Iterates over CSS styles and add all "descendant CSS selectors" 
//				* with a "pseudo class" (:hover, :focus, ...) to m_arrDescendantSelectorsMatchingStyleFlag
//				* with class selectors to m_mapDescendantSelectorsMatchingClass
//-----------------------------------------------------------------------------
void CStyleFile::CollectDescendantSelectors( const CUtlVector< StyleFromFile_t * > &vecStyles )
{
	FOR_EACH_VEC( vecStyles, nStyle )
	{
		StyleFromFile_t *pStyleFromFile = vecStyles[ nStyle ] ;

		FOR_EACH_PTR_ARRAY( pStyleFromFile->m_parentSelectors, nParentSelector )
		{
			CStyleSelector &parentSelector = pStyleFromFile->m_parentSelectors[ nParentSelector ];

			// Collect descendant selectors with a "pseudo class"

			uint16 eStyleFlagToCheck = parentSelector.GetStyleFlags();
			// Add "not selector" style flags of the parent selector
			if ( parentSelector.GetNotSelector() )
			{
				eStyleFlagToCheck |= ( parentSelector.GetNotSelector()->GetStyleFlags() );
			}

			for ( int nBit = k_EStyleFlagMinBit; nBit < k_EStyleFlagMaxBit; ++nBit )
			{				
				if ( ( eStyleFlagToCheck & ( 1 << nBit ) ) )
				{
					m_arrDescendantSelectorsMatchingStyleFlag[nBit].AddToTail( &parentSelector );
				}
			}

			// Collect descendant selectors that have class selectors

			const CUtlPtrArray< CPanoramaSymbol > &arrayClasses = parentSelector.GetClasses();
			FOR_EACH_PTR_ARRAY( arrayClasses, iSelector )
			{
				int iMap = m_mapDescendantSelectorsMatchingClass.Find( arrayClasses[iSelector] );
				if ( iMap == m_mapDescendantSelectorsMatchingClass.InvalidIndex() )
					iMap = m_mapDescendantSelectorsMatchingClass.Insert( arrayClasses[iSelector], new VecStyleSelectorPtr_t() );
				
				m_mapDescendantSelectorsMatchingClass.Element( iMap )->AddToTail( &parentSelector );
			}
			// Check "not selector" of the parent selector
			if ( parentSelector.GetNotSelector() )
			{
				const CUtlPtrArray< CPanoramaSymbol > &arrayClassesNotSelector = parentSelector.GetNotSelector()->GetClasses();
				FOR_EACH_PTR_ARRAY( arrayClassesNotSelector, iSelector )
				{
					int iMap = m_mapDescendantSelectorsMatchingClass.Find( arrayClassesNotSelector[iSelector] );
					if ( iMap == m_mapDescendantSelectorsMatchingClass.InvalidIndex() )
						iMap = m_mapDescendantSelectorsMatchingClass.Insert( arrayClassesNotSelector[iSelector], new VecStyleSelectorPtr_t() );

					m_mapDescendantSelectorsMatchingClass.Element( iMap )->AddToTail( &parentSelector );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the buffer points to the specified define
//-----------------------------------------------------------------------------
bool BTestForDefine( char *pchBuffer, const char *pchDefine, uint *punLength )
{
	const char *pchCurrent = pchBuffer;
	while ( *pchCurrent != '\0' && *pchDefine != '\0' )
	{
		if ( *pchCurrent != *pchDefine )
			return false;

		pchCurrent++;
		pchDefine++;
	}

	// hit end of define?
	if ( *pchDefine != '\0' )
		return false;

	// matched define. make sure the next buffer character is not a valid char define
	// looking to make sure we do not match partial strings, like matching "bgcolor" with "bgcolorwash"
	if ( *pchCurrent != '\0' && BValidDefineChar( *pchCurrent ) )
		return false;

	// match!
	*punLength = pchCurrent - pchBuffer;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Replaces all occurrences of defines in string
//			Only replaces defined values from this specific style file
//          or files that it imports.
//-----------------------------------------------------------------------------
bool CStyleFile::BReplaceDefinedValues( char *rgchBuffer, uint cubBuffer, uint unFileOrder ) const
{
	bool bSuccess = true;

	// Replace my own defines
	if ( m_dictDefines.Count() != 0 )
	{
		uint cchTotal = V_strlen( rgchBuffer );

		// walk the string, looking for defines from this file
		bool bPrevCharValidForDefine = false;
		for ( char *pchCurrent = rgchBuffer; *pchCurrent != '\0'; pchCurrent++ )
		{
			// if the current character is not valid for a define name, we can skip testing this character.
			// if the previous character was valid for a define, we need to skip this character to prevent matching partial strings, such
			// as "red" in "steampowered.com". If the previous character was in a known define, we would have already matched this current character.

			bool bSkip = bPrevCharValidForDefine;
			bPrevCharValidForDefine = BValidDefineChar( *pchCurrent );
			if ( !bPrevCharValidForDefine || bSkip )
				continue;

			FOR_EACH_DICT_FAST( m_dictDefines, i )
			{
				uint unMatchLength = 0;
				if ( !BTestForDefine( pchCurrent, m_dictDefines.GetElementName( i ), &unMatchLength ) )
					continue;

				// Skip if this define happens later in the file order than the requested location
				const CStyleDefine &define = m_dictDefines.Element( i );
				if ( define.m_unFileOrder > unFileOrder )
					continue;

				// found match.. replace
				const char *pchReplace = define.m_strValue.String();
				uint nReplaceLength = V_strlen( pchReplace );
				if ( cchTotal + nReplaceLength > cubBuffer - 1 )		// -1 for '\0'
					return false;

				// If the next character is an ampersand, then assume they're trying to concatenate. This is
				// most useful for @defines that are colors, because you can concatenate the alpha value at
				// the end. For example:
				//   @define testColor: #ff0000;
				//   .ColoredText { color: testColor&66; }
				// This is done by pretending that the match length is one longer than it actually was, so
				// we skip the ampersand.
				if ( *( pchCurrent + unMatchLength ) == '&' )
				{
					unMatchLength++;
				}

				// move existing text to its new location after replace
				uint nToMove = cchTotal - unMatchLength - ( pchCurrent - rgchBuffer );
				V_memmove( pchCurrent + nReplaceLength, pchCurrent + unMatchLength, nToMove );

				// insert replacement text
				V_memcpy( pchCurrent, pchReplace, nReplaceLength );

				// update total size and terminate
				cchTotal = ( pchCurrent - rgchBuffer ) + nReplaceLength + nToMove;
				rgchBuffer[ cchTotal ] = '\0';

				// skip replacement length, -1 because loop will increment to next
				pchCurrent += nReplaceLength - 1;
				break;
			}
		}
	}

	// Replace any imported defines
	for ( const ImportedStyleFile_t &importedStyleFile : m_vecImportedStyleFiles )
	{
		if ( !importedStyleFile.pStyleFile->BReplaceDefinedValues( rgchBuffer, cubBuffer, unFileOrder ) )
			bSuccess = false;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: When parsing, replaces all occurrences of defines in string.
//			Uses defines from the current style file and any others in stylefile set
//-----------------------------------------------------------------------------
bool panorama::BReplaceDefines( char *rgchBuffer, uint cubBuffer, const CStyleFile *pCurrentStyleFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint unFileOrder )
{
	// this function can be called with this file in vecPrevStyleFiles or without. Only check ourselves first if not at the end of vecPrevStyleFiles
	if ( vecPrevStyleFiles.Count() == 0 || vecPrevStyleFiles[vecPrevStyleFiles.Count() - 1].Get() != pCurrentStyleFile )
	{
		if ( pCurrentStyleFile && !pCurrentStyleFile->BReplaceDefinedValues( rgchBuffer, cubBuffer, unFileOrder ) )
			return false;
	}

	// walk backward so last included define trumps previous
	FOR_EACH_VEC_BACK( vecPrevStyleFiles, i )
	{
		if ( !vecPrevStyleFiles[i]->BReplaceDefinedValues( rgchBuffer, cubBuffer, unFileOrder ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Parses a CSS property
//
// Looks something like:
//			"margin: 100px;" 
// 
//-----------------------------------------------------------------------------

bool panorama::BParseStyleProperty( CUtlBuffer &buffer, StylePropertyHash_t *pProperties, CPanoramaSymbol symPathForErrors, 
	const CStyleFile *pCurrentStyleFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, bool bNoBrackets )
{
	char rgchStyleBuf[4096];

	// get next token. Can't be empty
	if( !CSSHelpers::BReadCSSToken( buffer, rgchStyleBuf, V_ARRAYSIZE( rgchStyleBuf ) ) )
	{
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Error parsing style" );
		return false;
	}

	// should be property name. Don't check if valid yet, we want to parse all of the property and skip unknown
	CStyleSymbol symParsedName( rgchStyleBuf );

	// should be followed by ':'
	if ( !CSSHelpers::BReadMatchingCSSToken( buffer, ':' ) )
	{
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Missing colon" );
		return false;
	}

	// get the value for this property. Include spaces
	// when reading w/o brackets (parsing style="color: #ffffff"), also allow \0
	const char *pchStopAt = k_rgchCSSValueTerm;
	uint cchStopAt = V_ARRAYSIZE( k_rgchCSSValueTerm );
	if ( bNoBrackets )
	{
		pchStopAt = k_rgchCSSValueTermOrEndOfString;
		cchStopAt = V_ARRAYSIZE( k_rgchCSSValueTermOrEndOfString );
	}

	if ( !CSSHelpers::BReadCSSToken( buffer, rgchStyleBuf, V_ARRAYSIZE( rgchStyleBuf ), pchStopAt, cchStopAt ) || rgchStyleBuf[0] == '\0' )
	{
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Failed to parse property value. Did you forget a terminating semicolon?" );
		return false;
	}

	// don't require a semicolon on the last css value when parsing inline
	CSSHelpers::EatCSSIgnorables( buffer );
	bool bParsingComplete = (bNoBrackets && (!buffer.IsValid() || *((const char*)buffer.PeekGet( sizeof( char ), 0 )) == '\0'));

	// skip end of line. While not 100% accurate.. just require a ; (looks like CSS lets you leave the semicolon off the last name/value pair)
	if ( !bParsingComplete && !CSSHelpers::BReadMatchingCSSToken( buffer, ';' ) )
	{
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Missing semicolon" );
		return false;
	}

	// Parsed line successfully. Remainder of this iteration should be able to continue if parsing fails

	// See if this property name is an alias, and look to see if we already have a value for the real property name
	CStyleSymbol symRealPropertyName = CStylePropertyFactory::GetPropertyNameForAlias( symParsedName );
	if ( !symRealPropertyName.IsValid() )
	{
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Invalid property name: %s", rgchStyleBuf );
		return true;
	}		

	// create if this is the first time we have seen this property
	CStyleProperty *pProperty = NULL;
	StylePropertyHash_t::IndexType_t iPropTree = pProperties->Find( symRealPropertyName );
	if ( iPropTree == pProperties->InvalidIndex() )
	{			
		pProperty = CStylePropertyFactory::CreateStyleProperty( symRealPropertyName );
		if ( !pProperty )
		{
			LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Invalid property name: %s", rgchStyleBuf );
			return true;
		}

		pProperties->Insert( pProperty->GetPropertySymbol(), pProperty );
		SPEW_STYLE( "Creating new property %s from %s\n", symRealPropertyName.String(), rgchStyleBuf );
	}
	else
	{
		// we have already seen this property in this style (which is valid). We are working or way down the style definition, and are going to merge
		// duplicate property definitions. For most properties, this will simply replace the previous definition (defining visibility twice in the same style),
		// however for properties which can be specified using multiple parameters, this will update the property (defining margin then margin-left).
		pProperty = pProperties->Element( iPropTree );
		SPEW_STYLE( "Merging property %s from %s\n", symRealPropertyName.String(), rgchStyleBuf );
	}

	Assert( pProperty );
		
	// every value can be a define.. check our defines up to this point in the file, and defines from previous layout files
	if ( !BReplaceDefines( rgchStyleBuf, V_ARRAYSIZE( rgchStyleBuf ), pCurrentStyleFile, vecPrevStyleFiles, 0xFFFFFFFF ) )
	{
		// buffer wasn't big enough. Fail here (4k+ of define??)
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "An error occurred while replacing defines in property value (property=%s)(value=%s)", symParsedName.String(), rgchStyleBuf );
		return false;
	}		

	// try and set the property values. Make sure we pass in the parsed name, so the property knows what data to expect
	if ( !pProperty->BSetFromString( symParsedName, rgchStyleBuf ) )
	{
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Failed to set property value (property=%s)(value=%s)", symParsedName.String(), rgchStyleBuf );
		return true;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Parses a CSS style body
//
// Looks something like (part between { and } ):
//		div {
//			margin: 100px;
//			background-color: red;
//		}
// 
//	Or, if it's inline.
//		"width: 100%;"
//		"width: 100%; heigth: 50%;"
//-----------------------------------------------------------------------------
bool panorama::BParseStyleBody( CUtlBuffer &buffer, StylePropertyHash_t *pProperties, CPanoramaSymbol symPathForErrors, const CStyleFile *pCurrentStyleFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, bool bNoBrackets )
{
	// parse {
	char rgchStyleBuf[4096];
	if ( !bNoBrackets && (!CSSHelpers::BReadCSSToken( buffer, rgchStyleBuf, V_ARRAYSIZE( rgchStyleBuf ) ) || rgchStyleBuf[0] != '{') )
	{
		LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Error parsing style" );
		return false;
	}

	// keep parsing until } or end of string
	for ( ;; )
	{
		// end?
		char chEnd = !bNoBrackets ? '}' : '\0';
		bool bPeekResult = CSSHelpers::BPeekCSSToken( buffer, rgchStyleBuf );
				
		if ( rgchStyleBuf[0] == chEnd )
		{
			// Eat end curly brace and quit
			if ( chEnd == '}' )
			{
				CSSHelpers::BReadCSSToken( buffer, rgchStyleBuf, V_ARRAYSIZE( rgchStyleBuf ) );
			}
			
			break;
		}
		else if ( !bPeekResult )
		{
			LogLayoutParsingError( symPathForErrors, CalcBufferLine( buffer ), "Error parsing style" );	
			return false;
		}

		if ( !BParseStyleProperty( buffer, pProperties, symPathForErrors, pCurrentStyleFile, vecPrevStyleFiles, bNoBrackets ) )
		{
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: adds a define
//-----------------------------------------------------------------------------
void CStyleFile::AddDefine( const char *pchName, const char *pchValue )
{
	m_dictDefines.Insert( pchName, CStyleDefine( pchValue, ++m_unMaxFileOrder ) );
}


//-----------------------------------------------------------------------------
// Purpose: Checks for a define in this style file
// Returns: NULL if define does not exist
//-----------------------------------------------------------------------------
const char *CStyleFile::GetDefine( const char *pchName ) const
{
	const CStyleDefine *pDefine = GetStyleDefine( pchName );
	return pDefine ? pDefine->m_strValue.String() : nullptr;
}

const CStyleDefine *CStyleFile::GetStyleDefine( const char *pchName ) const
{
	const CStyleDefine *pDefine = nullptr;

	// First search our own defines
	short iMap = m_dictDefines.Find( pchName );
	if ( iMap != m_dictDefines.InvalidIndex() )
	{
		pDefine = &m_dictDefines.Element( iMap );
	}

	// Then check if we imported a file that has the animation. Search backwards
	// since we want the last definition to win.
	FOR_EACH_VEC_BACK( m_vecImportedStyleFiles, i )
	{
		const ImportedStyleFile_t &importedFile = m_vecImportedStyleFiles[ i ];

		// If we've found a define, and it's later in the file order than the maximum of
		// the the imported file, we can break out immediately.
		if ( pDefine && pDefine->m_unFileOrder > importedFile.pStyleFile->GetMaxFileOrder() )
			break;

		const CStyleDefine *pImportDefine = importedFile.pStyleFile->GetStyleDefine( pchName );
		if ( !pImportDefine )
			continue;

		if ( !pDefine || pImportDefine->m_unFileOrder > pDefine->m_unFileOrder )
		{
			pDefine = pImportDefine;
			continue;
		}
	}

	return pDefine;
}


//-----------------------------------------------------------------------------
// Purpose: Checks for a keyframe in this style file
// Returns: NULL if define does not exist
//-----------------------------------------------------------------------------
const CStyleAnimation *CStyleFile::GetAnimation( CPanoramaSymbol symName ) const
{
	// First search our own animations
	const CStyleAnimation *pAnimation = m_mapAnimations.FindElement( symName, nullptr );

	// Then check if we imported a file that has the animation. Search backwards
	// since we want the last definition to win.
	FOR_EACH_VEC_BACK( m_vecImportedStyleFiles, i )
	{
		const ImportedStyleFile_t &importedFile = m_vecImportedStyleFiles[ i ];

		// If we've found an animation, and it's later in the file order than the maximum of
		// the the imported file, we can break out immediately.
		if ( pAnimation && pAnimation->GetFileOrder() > importedFile.pStyleFile->GetMaxFileOrder() )
			break;

		const CStyleAnimation *pImportAnimation = importedFile.pStyleFile->GetAnimation( symName );
		if ( !pImportAnimation )
			continue;

		if ( !pAnimation || pImportAnimation->GetFileOrder() > pAnimation->GetFileOrder() )
		{
			pAnimation = pImportAnimation;
			continue;
		}
	}

	return pAnimation;
}


//-----------------------------------------------------------------------------
// Purpose: Checks for a style in this style file
// Returns: NULL if define does not exist
//-----------------------------------------------------------------------------
const StyleFromFile_t *CStyleFile::GetStylesForClass( CPanoramaSymbol symStyle )
{
	return m_classStyles.Get( symStyle, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Construct the style file index representing this style file.
// Because style files themselves can import other style files, this is not a
// simple 0-based index. Instead, it is divided into 4-bit chunks, with each
// chunk representing the index of that import file within its containing
// layout file or style file.
//-----------------------------------------------------------------------------
static StyleFileIndex_t ConstructStyleFileIndex( StyleFileIndex_t iBaseFileIndex, uint iImportDepth, uint iCurrentIndex )
{
	static_assert( sizeof( StyleFileIndex_t ) >= sizeof( uint32 ), "We use 4 bits per import level, and allow 8 import levels. So the uint storing it must be 32-bits." );
	AssertMsg( iCurrentIndex < 16, "Only 16 @import files per CSS can be displayed properly in the debugger." );
	AssertMsg( iImportDepth < 8, "Only 8 levels deep of @import file nesting can be displayed properly in the debugger." );

	return iBaseFileIndex | ( iCurrentIndex << ( iImportDepth * 4 ) );
}


//-----------------------------------------------------------------------------
// Purpose: Helper function for recursively adding a set of matching styles
//-----------------------------------------------------------------------------
template <typename TValue, typename GetStylesFunction >
void CStyleFile::AddMatchingStylesCore( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, TValue value, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, StyleFileIndex_t iCurrentFileIndex, uint iImportDepth, GetStylesFunction getStyles )
{
	bool bDescendantFilterActive = CStyleFileDescendantFilter::BIsActive();

	// Add styles for this file
	const StyleFromFile_t *pStyleFromFile = CALL_POINTER_MEMBER_FUNCTION( this, getStyles )( value );
	for ( ; pStyleFromFile != nullptr; pStyleFromFile = pStyleFromFile->m_pNext )
	{
		if ( bDescendantFilterActive && !CStyleFileDescendantFilter::BMayContain( pStyleFromFile ) )
		{
			continue;
		}

		if ( CStyleFileSet::BSelectorMatchesPanel( pStyleFromFile, panelID ) )
		{
			CascadeStyleFileInfo_t &info = vecStyles[ vecStyles.AddToTail() ];
			info.m_pStyleFromFile = pStyleFromFile;
			info.m_pLayoutFile = pLayoutFile;
			info.m_iStyleFile = iCurrentFileIndex;
			info.m_unSelectorSpecificity = CalculateSelectorSpecificity( pStyleFromFile );
		}
	}

	// Recursively add styles for the imports
	for ( int nImportIndex = 0; nImportIndex < m_vecImportedStyleFiles.Count(); ++nImportIndex )
	{
		uint32 iImportFileIndex = ConstructStyleFileIndex( iCurrentFileIndex, iImportDepth + 1, nImportIndex + 1 );
		m_vecImportedStyleFiles[ nImportIndex ].pStyleFile->AddMatchingStylesCore( pLayoutFile, panelID, value, vecStyles, iImportFileIndex, iImportDepth + 1, getStyles );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Add styles matching the given class
//-----------------------------------------------------------------------------
void CStyleFile::AddStylesMatchingClass( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, CPanoramaSymbol symStyle, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, uint32 iStyleIndex )
{
	StyleFileIndex_t iFileIndex = ConstructStyleFileIndex( 0, 0, iStyleIndex );
	AddMatchingStylesCore( pLayoutFile, panelID, symStyle, vecStyles, iFileIndex, 0, &CStyleFile::GetStylesForClass );
}


//-----------------------------------------------------------------------------
// Purpose: Add styles matching the given type without considering class
//-----------------------------------------------------------------------------
void CStyleFile::AddStylesMatchingPanelTypeNoClass( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, CPanoramaSymbol symStyle, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, uint32 iStyleIndex )
{
	StyleFileIndex_t iFileIndex = ConstructStyleFileIndex( 0, 0, iStyleIndex );
	AddMatchingStylesCore( pLayoutFile, panelID, symStyle, vecStyles, iFileIndex, 0, &CStyleFile::GetStylesForPanelTypeNoClass );
}


//-----------------------------------------------------------------------------
// Purpose: Add styles matching the given id
//-----------------------------------------------------------------------------
void CStyleFile::AddStylesMatchingJustID( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, const char *pchID, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, uint32 iStyleIndex )
{
	StyleFileIndex_t iFileIndex = ConstructStyleFileIndex( 0, 0, iStyleIndex );
	AddMatchingStylesCore( pLayoutFile, panelID, pchID, vecStyles, iFileIndex, 0, &CStyleFile::GetStylesForJustID );
}


//-----------------------------------------------------------------------------
// Purpose: Checks for a style in this style file
// Returns: NULL if define does not exist
//-----------------------------------------------------------------------------
const StyleFromFile_t *CStyleFile::GetStylesForPanelTypeNoClass( CPanoramaSymbol symStyle )
{
	return m_typeStyles.Get( symStyle, NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Checks for a style in this style file
// Returns: NULL if define does not exist
//-----------------------------------------------------------------------------
const StyleFromFile_t *CStyleFile::GetStylesForJustID( const char *pchID )
{
	return m_idStyles.Get( pchID, NULL );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CStyleFileSet::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_vecStyleFiles );
	
	// don't validate the pointers in m_vecStyleFiles. They are owned by the layout manager
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CStyleFile::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_dictDefines );
	FOR_EACH_DICT_FAST( m_dictDefines, i )
	{
		ValidateObj( m_dictDefines[i] );
	}

	ValidateObj( m_mapAnimations );
	FOR_EACH_MAP_FAST( m_mapAnimations, iMap )
	{
		ValidatePtr( m_mapAnimations[iMap] );		
	}

	ValidateObj( m_classStyles );
	FOR_EACH_HASHTABLE( m_classStyles, i )
	{
		ValidatePtr( m_classStyles[i] );
	}
	ValidateObj( m_typeStyles );
	FOR_EACH_HASHTABLE( m_typeStyles, i )
	{
		ValidatePtr( m_typeStyles[i] );
	}
	ValidateObj( m_idStyles );
	FOR_EACH_HASHTABLE( m_idStyles, i )
	{
		ValidatePtr( m_idStyles[i] );
	}

}
#endif


//-----------------------------------------------------------------------------
// Purpose: Checks if style contains any "descendant selector" 
//			with the given style flag and matching the panel (id, type, classes, ...)
//			Used to filter which panel subtree to invalidate when adding / removing style flags
//-----------------------------------------------------------------------------
bool CStyleFile::BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const
{
	return BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, nOldPanelFlags, nNewPanelFlags, panel.ClientPtr()->GetPanelType(), panel.GetClasses().Base(), panel.GetClasses().Count(), panel.GetID(), vecVisitedStyleFiles );
}

bool CStyleFile::BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, CPanoramaSymbol symPanelType, const CPanoramaSymbol *psymClasses, uint csymClasses, const char *pchID, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const
{
	if ( vecVisitedStyleFiles.HasElement( GetPathSymbol() ) )
	{
		return false;
	}
	vecVisitedStyleFiles.AddToTail( GetPathSymbol() );
	
	// Check descendant selectors in this file
	for ( int nBit = k_EStyleFlagMinBit; nBit < k_EStyleFlagMaxBit; ++nBit )
	{
		if ( ( eStyleFlag & ( 1 << nBit ) ) )
		{
			const VecStyleSelectorPtr_t &vecSelectors = m_arrDescendantSelectorsMatchingStyleFlag[ nBit ];
			FOR_EACH_VEC( vecSelectors, nSelector )
			{
				// Check descendant selector may be affected by this style flag change
				bool bOldMatch = BIndividualSelectorMatchesPanel( *vecSelectors[nSelector], symPanelType, nOldPanelFlags, psymClasses, csymClasses, pchID );
				bool bNewMatch = BIndividualSelectorMatchesPanel( *vecSelectors[nSelector], symPanelType, nNewPanelFlags, psymClasses, csymClasses, pchID );

				if ( bOldMatch != bNewMatch )
				{
					return true;
				}
			}
		}
	}

	// Recursively check descendant selectors for the imports
	for ( int nImportIndex = 0; nImportIndex < m_vecImportedStyleFiles.Count(); ++nImportIndex )
	{
		if ( m_vecImportedStyleFiles[nImportIndex].pStyleFile->BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, nOldPanelFlags, nNewPanelFlags, symPanelType, psymClasses, csymClasses, pchID, vecVisitedStyleFiles ) )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Checks if style contains any "descendant selector" 
//			with the given classes and matching the panel (id, type, classes, ...)
//			Used to filter which panel subtree to invalidate when adding / removing style flags
//-----------------------------------------------------------------------------
bool CStyleFile::BHasAnyDescendantSelectorMatchingClasses( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const
{
	if ( vecVisitedStyleFiles.HasElement( GetPathSymbol() ) )
	{
		return false;
	}
	vecVisitedStyleFiles.AddToTail( GetPathSymbol() );
	
	// Check descendant selectors in this file
	FOR_EACH_PTR_ARRAY( arrChangedClasses, nClass )
	{
		int iMap = m_mapDescendantSelectorsMatchingClass.Find( arrChangedClasses[ nClass ] );
		if ( iMap != m_mapDescendantSelectorsMatchingClass.InvalidIndex() )
		{
			VecStyleSelectorPtr_t *pVecSelectors = m_mapDescendantSelectorsMatchingClass.Element( iMap );
			if ( pVecSelectors )
			{
				FOR_EACH_VEC( *pVecSelectors, nSelector )
				{
					// Check descendant selector may be affected by this class change
					// Perf - redundant work done by calling BIndividualSelectorMatchesPanel twice, different classes only
					bool bOldMatch = BIndividualSelectorMatchesPanel( 
						*( pVecSelectors->Element( nSelector ) ), 
						panel.ClientPtr()->GetPanelType(), 
						panel.GetStyleFlags(), 
						arrOldClasses.Base(), arrOldClasses.Count(), 
						panel.GetID() );
					bool bNewMatch = BIndividualSelectorMatchesPanel( 
						*( pVecSelectors->Element( nSelector ) ), 
						panel.ClientPtr()->GetPanelType(), 
						panel.GetStyleFlags(), 
						arrNewClasses.Base(), arrNewClasses.Count(), 
						panel.GetID() );

					if ( bOldMatch != bNewMatch )
					{
						return true;
					}
				}
			}
		}
	}

	// Recursively check descendant selectors for the imports
	for ( int nImportIndex = 0; nImportIndex < m_vecImportedStyleFiles.Count(); ++nImportIndex )
	{
		if ( m_vecImportedStyleFiles[nImportIndex].pStyleFile->BHasAnyDescendantSelectorMatchingClasses( arrChangedClasses, arrOldClasses, arrNewClasses, panel, vecVisitedStyleFiles ) )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//			CStyleFileDescendantFilter Implementation
//
//-----------------------------------------------------------------------------


bool CStyleFileDescendantFilter::m_bActive = false;
CStyleFileDescendantFilter::CountingBloomFilter_t CStyleFileDescendantFilter::m_filter;


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
/*static*/ void CStyleFileDescendantFilter::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		m_bActive = bActive;
		m_filter.Clear();
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
/*static*/ bool CStyleFileDescendantFilter::BIsActive()
{
	return m_bActive;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
/*static*/ void CStyleFileDescendantFilter::Push( const IUIPanel &parentPanel )
{
	m_filter.Add( parentPanel.ClientPtr()->GetPanelType() );
	for ( int nClass = 0; nClass < parentPanel.GetClasses().Count(); ++nClass )
	{
		m_filter.Add( parentPanel.GetClasses()[nClass] );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
/*static*/ void CStyleFileDescendantFilter::Pop( const IUIPanel &parentPanel )
{
	m_filter.Remove( parentPanel.ClientPtr()->GetPanelType() );
	for ( int nClass = 0; nClass < parentPanel.GetClasses().Count(); ++nClass )
	{
		m_filter.Remove( parentPanel.GetClasses()[nClass] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check whether ancestors of the panel (added via Push() / Pop())
//			might be a match for the given css selector
//-----------------------------------------------------------------------------
/*static*/ bool CStyleFileDescendantFilter::BMayContain( const StyleFromFile_t *pStyleFromFile )
{
	for ( uint nSelector = 0; nSelector < pStyleFromFile->m_parentSelectors.Count(); ++nSelector )
	{
		const CStyleSelector &selector = pStyleFromFile->m_parentSelectors[nSelector];
		
		if ( selector.GetPanelType().IsValid() && !m_filter.BMayContain( selector.GetPanelType() ) )
		{
			return false;
		}

		if ( selector.GetClasses().Count() > 0 )
		{
			const CUtlPtrArray< CPanoramaSymbol > &arrayClasses = selector.GetClasses();
			FOR_EACH_PTR_ARRAY( arrayClasses, nClass )
			{
				if ( !m_filter.BMayContain( arrayClasses[nClass] ) )
				{
					return false;
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CStyleFileDescendantFilter::CountingBloomFilter_t::Clear()
{
	memset( m_buckets, 0, V_ARRAYSIZE( m_buckets ) );
}

//-----------------------------------------------------------------------------
// Purpose: Clear the filter. Make sure filter to clear the filter before reusing it
//			as just removing all hashes doesn't guarantee to clear it in case of 
//			overflow (8 bit counter)
//-----------------------------------------------------------------------------
void CStyleFileDescendantFilter::CountingBloomFilter_t::Add( uint32 hash )
{
	uint8 &bucket1 = FirstBucket( hash );
	if ( bucket1 != UINT8_MAX )
	{
		++bucket1;
	}

	uint8 &bucket2 = SecondBucket( hash );
	if ( bucket2 != UINT8_MAX )
	{
		++bucket2;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CStyleFileDescendantFilter::CountingBloomFilter_t::Remove( uint32 hash )
{
	// In case of an overflow, the bucket sticks in the table until Clear()
	// as we don't know whether we incremented the bucket count when we added
	// the hash

	uint8 &bucket1 = FirstBucket( hash );
	if ( bucket1 != UINT8_MAX )
	{
		--bucket1;
	}

	uint8 &bucket2 = SecondBucket( hash );
	if ( bucket2 != UINT8_MAX )
	{
		--bucket2;
	}
}

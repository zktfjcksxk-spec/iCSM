//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "debugpanelstyle.h"
#include "panorama/controls/label.h"
#include "panorama/controls/textentry.h"
#include "panorama/controls/tooltip.h"
#include "debugautocomplete.h"
#include "debugger.h"
#include "panorama/controls/button.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDebugPanelStyle, DebugPanelStyle )
REGISTER_PANEL2D_FACTORY( CDebugIndividualStyle, DebugIndividualStyle )
REGISTER_PANEL2D_FACTORY( CDebugStyleAnimation, DebugStyleAnimation )
REGISTER_PANEL2D_FACTORY( CDebugInheritedStylesHeader, DebugInheritedStylesHeader )
REGISTER_PANEL2D_FACTORY( CDebugStyleSeparator, DebugStyleSeparator )
REGISTER_PANEL2D_FACTORY( CDebugStyleBlock, DebugStyleBlock )

DECLARE_PANORAMA_EVENT0( UpdateStyleInMemory );
DEFINE_PANORAMA_EVENT( UpdateStyleInMemory );
DEFINE_PANORAMA_EVENT( DebugStyleStatus );


//-----------------------------------------------------------------------------
// Purpose: Helper for loading a style file buffer
//-----------------------------------------------------------------------------
typedef CUtlMap< CPanoramaSymbol, CUtlBuffer *, int, CDefLess< CPanoramaSymbol > > MapStyleBuffers_t;
CUtlBuffer *GetStyleBuffer( MapStyleBuffers_t *pmapBuffers, IUILayoutFile *pLayoutFile, CPanoramaSymbol symFile )
{
	VPROF_BUDGET( "CDebugPanelStyle::GetStyleBuffer", VPROF_BUDGETGROUP_TENFOOT );

	int iMap = pmapBuffers->Find( symFile );
	if ( iMap == pmapBuffers->InvalidIndex() )
	{
		// load buffer
		CUtlBuffer *pBuffer = new CUtlBuffer();
		iMap = pmapBuffers->Insert( symFile, pBuffer );

		if ( !UIEngine()->UILayoutManager()->LoadStyleIntoBuffer( symFile.String(), *pBuffer ) )
		{
			delete pBuffer;
			pmapBuffers->Remove( symFile );

			AssertMsg1( false, "Failed to load buffer: %s", symFile.String() );
			return NULL;
		}

		Assert( pBuffer->IsText() );
	}

	return pmapBuffers->Element( iMap );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the specified text looks like a property
//-----------------------------------------------------------------------------
bool BLooksLikeProperty( const char *pchName )
{
	// return false if c or c++ comment
	if ( pchName[0] == 0 )
		return false;
	
	// is first character a slash?
	if ( pchName[0] != '\0' && pchName[0] != '/' )
		return true;

	// is second character a / or *
	return ( pchName[1] != '\0' && pchName[1] != '/' && pchName[1] != '*' );
}


//-----------------------------------------------------------------------------
// Purpose: Appends tabs to a string
//-----------------------------------------------------------------------------
void AppendTabs( CFmtStrMax *pfmt, uint cTabs )
{
	for ( ; cTabs != 0; cTabs-- )
		pfmt->Append( "\t" );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a string for a path of imported styles
//-----------------------------------------------------------------------------
CUtlString ConstructStylePathString( const CUtlVector< CPanoramaSymbol > &vecStylePaths )
{
	CUtlString str;

	for ( CPanoramaSymbol symPath : vecStylePaths )
	{
		if ( !str.IsEmpty() )
		{
			str.Append( " <b>@import</b> " );
		}

		str.Append( V_UnqualifiedFileName( symPath.String() ) );
	}

	return str;
}


//-----------------------------------------------------------------------------
// Purpose: Helper function to create a single-element path of imported styles
//-----------------------------------------------------------------------------
CUtlString ConstructStylePathString( CPanoramaSymbol symPath )
{
	CUtlVector< CPanoramaSymbol > vecPaths;
	vecPaths.AddToTail( symPath );

	return ConstructStylePathString( vecPaths );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugPanelStyle::CDebugPanelStyle( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	RegisterForUnhandledEvent( SetDebugTarget(), this, &CDebugPanelStyle::OnSetDebugTarget );
	RegisterForUnhandledEvent( PanelStyleChanged(), this, &CDebugPanelStyle::OnPanelStyleChanged );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugPanelStyle::~CDebugPanelStyle()
{
	UnregisterForUnhandledEvent( SetDebugTarget(), this, &CDebugPanelStyle::OnSetDebugTarget );
	UnregisterForUnhandledEvent( PanelStyleChanged(), this, &CDebugPanelStyle::OnPanelStyleChanged );
}


//-----------------------------------------------------------------------------
// Purpose: Eats all white space but return line
//-----------------------------------------------------------------------------
void EatWhiteSpaceNotReturn( CUtlBuffer *pBuffer )
{
	while ( pBuffer->IsValid() )
	{
		const char *pPeek = (const char*)pBuffer->PeekGet( sizeof(char), 0 );
		if ( !pPeek )
			return;

		if ( *pPeek == '\n' || !V_isspace( *pPeek ) )
			return;

		pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Eats all white space and one return line
//-----------------------------------------------------------------------------
void EatWhiteSpaceAndSingleReturn( CUtlBuffer *pBuffer )
{
	EatWhiteSpaceNotReturn( pBuffer );
	const char *pPeek = (const char*)pBuffer->PeekGet( sizeof(char), 0 );
	if ( pPeek && *pPeek == '\n' )
	{
		pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
		EatWhiteSpaceNotReturn( pBuffer );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Searches buffer for style at the specified line, and adds to output
//-----------------------------------------------------------------------------
bool CDebugPanelStyle::SetStyleInfo( CUtlBuffer *pBuffer, CDebugStyleBlock *pStyleBlock )
{
	char rgchBuffer[2048];
	rgchBuffer[0] = '\0';
	
	// get selector
	if ( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ), k_rgchCSSSelectorTerm, V_ARRAYSIZE( k_rgchCSSSelectorTerm ) ) )
		return false;
	
	pStyleBlock->SetSelector( rgchBuffer );

	// parse {
	if( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] != '{' )
		return false;

	// eat return line
	EatWhiteSpaceAndSingleReturn( pBuffer );

	// keep parsing until }
	while ( true )
	{
		// check for empty lines and comments
		EatWhiteSpaceNotReturn( pBuffer );
		const char *pPeek = (const char*)pBuffer->PeekGet( sizeof(char), 0 );
		if ( pPeek && *pPeek == '\n' )
		{
			// empty line
			pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
			pStyleBlock->AddEmptyLine();
			continue;
		}

		if( CSSHelpers::BReadCSSComment( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) )
		{
			pStyleBlock->AddComment( rgchBuffer );
			EatWhiteSpaceAndSingleReturn( pBuffer );
			continue;
		}

		// get next token. Can't be empty
		if( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] == '\0' )
			return false;

		// end?
		if ( rgchBuffer[0] == '}' )
			break;

		// save off name
		CUtlString strName = rgchBuffer;

		// should be followed by ':'
		if( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] != ':' )
			return false;

		// get the value for this property. Include spaces
		if ( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ), k_rgchCSSValueTerm, V_ARRAYSIZE( k_rgchCSSValueTerm ) ) || rgchBuffer[0] == '\0' )
			return false;

		pStyleBlock->AddProperty( strName.String(), rgchBuffer );
		
		// read ; or }
		if( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || (rgchBuffer[0] != ';' && rgchBuffer[0] != '}') )
			return false;

		// end?
		if ( rgchBuffer[0] == '}' )
			break;

		// eat return line
		EatWhiteSpaceAndSingleReturn( pBuffer );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Searches buffer for style at the specified line, and adds to output
//-----------------------------------------------------------------------------
bool CDebugPanelStyle::SetAnimationInfo( CUtlBuffer *pBuffer, CDebugStyleAnimation *pAnimation )
{
	uint unFileLocation = pAnimation->GetFileLocation();

	char rgchBuffer[1024];
	rgchBuffer[0] = '\0';

	// skip to the style section
	pBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, unFileLocation );

	// get selector
	if ( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ), k_rgchCSSSelectorTerm, V_ARRAYSIZE( k_rgchCSSSelectorTerm ) ) )
		return false;

	static const char k_rgchKeyFrames[] = "@keyframes ";
	if ( V_strnicmp( rgchBuffer, k_rgchKeyFrames, V_ARRAYSIZE( k_rgchKeyFrames ) - 1 ) != 0 )
		return false;

	// strip single quotes. name shouldn't be empty
	char *pchName = (rgchBuffer + V_ARRAYSIZE( k_rgchKeyFrames ) - 1);
	int cchName = V_strlen( pchName );
	if ( cchName < 3 || pchName[0] != '\'' || pchName[ cchName - 1 ] != '\'' )
		return false;

	pchName[ cchName - 1 ] = '\0';
	pAnimation->SetName( pchName + 1 );

	// parse {
	if( !CSSHelpers::BReadCSSToken( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) || rgchBuffer[0] != '{' )
		return false;

	// eat return line
	EatWhiteSpaceAndSingleReturn( pBuffer );

	// keep parsing until }
	while ( true )
	{
		/*
		// check for empty lines and comments
		EatWhiteSpaceNotReturn( pBuffer );
		const char *pPeek = (const char*)pBuffer->PeekGet( sizeof(char), 0 );
		if ( pPeek && *pPeek == '\n' )
		{
			// empty line
			pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
			pStyleBlock->AddEmptyLine();
			continue;
		}

		if ( BReadCSSComment( *pBuffer, rgchBuffer, V_ARRAYSIZE( rgchBuffer ) ) )
		{
			pStyleBlock->AddComment( rgchBuffer );
			EatWhiteSpaceAndSingleReturn( pBuffer );
			continue;
		}
		*/
		char chNext;
		if( !CSSHelpers::BPeekCSSToken( *pBuffer, &chNext ) )
			return false;

		// end?
		if ( chNext == '}' )
			break;

		CDebugStyleBlock *pBlock = pAnimation->AddFrame();
		if ( !SetStyleInfo( pBuffer, pBlock ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Builds text
//-----------------------------------------------------------------------------
void CDebugPanelStyle::Build()
{
	VPROF_BUDGET( "CDebugPanelStyle::Build", VPROF_BUDGETGROUP_TENFOOT );

	RemoveAndDeleteChildren();
	DispatchEvent( DebugStyleStatus(), this, true );

	CPanel2D *pPanel = m_pDebugPanel.Get();
	if ( !pPanel )
		return;

	AppendElementStyles();
	AppendCascadeStyles();
	AppendInheritedStyles();
	AppendAnimations();
}


//-----------------------------------------------------------------------------
// Purpose: Appends the debug target's panel element style to this panel
//-----------------------------------------------------------------------------
void CDebugPanelStyle::AppendElementStyles()
{
	VPROF_BUDGET( "CDebugPanelStyle::AppendElementStyles", VPROF_BUDGETGROUP_TENFOOT );

	CPanel2D *pPanel = m_pDebugPanel.Get();
	Assert( pPanel );
	IUIPanelStyle *pStyle = pPanel->AccessStyle();

	// skip adding section if empty
	const CUtlVector< StyleEntry_t > &properties = pStyle->PropertiesSetFromElement();
	if ( properties.Count() == 0 )
		return;

	CDebugIndividualStyle *pDebugStyle = new CDebugIndividualStyle( this, NULL );
	pDebugStyle->SetTabIndex( k_flTabIndexAuto );
	pDebugStyle->Init( UTL_INVAL_SYMBOL, CUtlVector< CPanoramaSymbol >(), 0, 0 );
	CDebugStyleBlock *pStyleBlock = pDebugStyle->GetStyleBlock();
	pStyleBlock->SetSelector( "Element Style" );

	// add elements set from code	
	FOR_EACH_VEC( properties, i )
	{
		const CStyleProperty *pProperty = properties[ i ].m_pStyleProperty;
		if ( !pProperty )
		{
			AssertMsg( false, "Property marked as set from code but not in style, how is that possible?" );
			continue;
		}

		CFmtStr1024 fmtProperty;
		pProperty->ToString( &fmtProperty );
		pStyleBlock->AddProperty( pProperty->GetPropertySymbol().String(), fmtProperty );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Appends a list of all styles that directly apply to the debug target
//-----------------------------------------------------------------------------
void CDebugPanelStyle::AppendCascadeStyles()
{
	VPROF_BUDGET( "CDebugPanelStyle::AppendCascadeStyles", VPROF_BUDGETGROUP_TENFOOT );

	CPanel2D *pPanel = m_pDebugPanel.Get();
	Assert( pPanel );
	IUIPanelStyle *pStyle = pPanel->AccessStyle();

	// get sorted list of styles that apply to the target panel
	CUtlVector< CascadeStyleFileInfo_t > vecStyles;
	pPanel->UIPanel()->BBuildMatchingStyleList( &vecStyles );
	
	// track all properties that we have seen
	StylePropertyHash_t hashProperties;
	SymbolHash_t treePropertiesUsed;

	// make sure to include properties set from code
	const CUtlVector< StyleEntry_t > &vecFromCode = pStyle->PropertiesSetFromElement();
	treePropertiesUsed.EnsureCapacity( vecFromCode.Count() );
	FOR_EACH_VEC( vecFromCode, i )
	{
		CStyleProperty *pNewProperty = CStylePropertyFactory::CreateStyleProperty( vecFromCode[i].m_StyleSymbol );
		vecFromCode[i].m_pStyleProperty->MergeTo( pNewProperty );
		hashProperties.Insert( vecFromCode[i].m_StyleSymbol, pNewProperty );

		if ( pNewProperty->BFullySet() )
		{
			treePropertiesUsed.Insert( pNewProperty->GetPropertySymbol() );
			continue;
		}
	}

	// loop backward through styles, adding each to output
	MapStyleBuffers_t mapStyleBuffers;
	FOR_EACH_VEC_BACK( vecStyles, iVec )
	{
		IUILayoutFile *pLayoutFile = vecStyles[iVec].m_pLayoutFile;

		CUtlVector< CPanoramaSymbol > vecStyleFiles;
		pLayoutFile->GetStyleFileSymbols( vecStyles[ iVec ].m_iStyleFile, vecStyleFiles );

		if ( vecStyleFiles.Count() == 0 )
			continue;

		CPanoramaSymbol symFile = vecStyleFiles.Tail();
		CUtlBuffer *pBuffer = GetStyleBuffer( &mapStyleBuffers, pLayoutFile, symFile );
		if ( !pBuffer )
			continue;
		
		CDebugIndividualStyle *pStylePanel = new CDebugIndividualStyle( this, NULL );
		pStylePanel->Init( pLayoutFile->GetLayoutFileSymbol(), vecStyleFiles, vecStyles[iVec].m_pStyleFromFile->m_unFileLocation, vecStyles[iVec].m_pStyleFromFile->m_unFileOrder );
		pStylePanel->SetTabIndex( k_flTabIndexAuto );

		// skip to the style section
		pBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, vecStyles[iVec].m_pStyleFromFile->m_unFileLocation );

		CDebugStyleBlock *pStyleBlock = pStylePanel->GetStyleBlock();
		if ( !SetStyleInfo( pBuffer, pStyleBlock ) )
		{
			AssertMsg( false, "Failed to parse style info" );
			delete pStylePanel;
			continue;
		}

		// walk backward through the newly added styles, testing each to see if it changes the styles for the panel
		CPanel2D *pPropertySection = pStyleBlock->GetPropertySection();
		for ( int i = pPropertySection->GetChildCount() - 1; i >= 0; i-- )
		{
			CPanel2D *pRow = pPropertySection->GetChild( i );
			CTextEntry *pName = (CTextEntry*)pRow->FindChild( "PropName" );
			CTextEntry *pValue = (CTextEntry*)pRow->FindChild( "PropValue" );
			CStyleSymbol symName = CStyleSymbol( pName->PchGetText() );

			if ( !symName.IsValid() )
				continue;

			// if alias has been used or property has been fully set, can mark row as not used
			if ( treePropertiesUsed.HasElement( symName ) )
			{
				pRow->AddClass( "DebugPropertyRowNotUsed" );
				continue;
			}

			// replace defines
			char rgchValue[1024];
			V_strncpy( rgchValue, pValue->PchGetText(), V_ARRAYSIZE( rgchValue ) );
			if ( !pLayoutFile->BReplaceDefines( rgchValue, V_ARRAYSIZE( rgchValue ), vecStyles[ iVec ].m_pStyleFromFile->m_unFileOrder ) )
				continue;

			// usually we could stop if the property wasn't an alias, as the short hand property usually sets all alias properties (margin sets margin-top, margin-left, etc.),
			// but opacity-mask doesn't follow that convention so continue on to see if this value modifies the property

			CStyleProperty *pNewProperty = CStylePropertyFactory::CreateStyleProperty( symName );
			if ( !pNewProperty->BSetFromString( symName, rgchValue ) )
			{
				CStylePropertyFactory::FreeStyleProperty( pNewProperty );
				continue;
			}

			// apply new property to copy of existing, then compare to see if the new property modifies the existing
			CStyleProperty *pExistingProperty = hashProperties.FindElement( pNewProperty->GetPropertySymbol(), NULL );
			if ( !pExistingProperty )
			{
				pExistingProperty = CStylePropertyFactory::CreateStyleProperty( pNewProperty->GetPropertySymbol() );
				hashProperties.Insert( pExistingProperty->GetPropertySymbol(), pExistingProperty );
			}

			// copy existing
			bool bUsed = false;
			CStyleProperty *pCopyExisting = CStylePropertyFactory::CreateStyleProperty( pExistingProperty->GetPropertySymbol() );
			pExistingProperty->MergeTo( pCopyExisting );

			// merge new and test
			pNewProperty->MergeTo( pCopyExisting );
			if ( *pExistingProperty != *pCopyExisting )
			{
				// keep modified version
				bUsed = true;
				hashProperties.InsertOrReplace( pExistingProperty->GetPropertySymbol(), pCopyExisting );
				std::swap( pExistingProperty, pCopyExisting );
			}
			else
			{
				// didn't modify existing property. Double check that the value isn't the properties default
				CStyleProperty *pDefaultCheck = CStylePropertyFactory::CreateStyleProperty( pNewProperty->GetPropertySymbol() );
				pNewProperty->MergeTo( pDefaultCheck );
				if ( *pExistingProperty == *pDefaultCheck )
				{
					// as this style property symbol wasn't in treePropertiesUsed, first time we have seen this default
					// value. Display it as used
					bUsed = true;
				}

				CStylePropertyFactory::FreeStyleProperty( pDefaultCheck );
			}

			// clean up
			CStylePropertyFactory::FreeStyleProperty( pNewProperty );
			CStylePropertyFactory::FreeStyleProperty( pCopyExisting );

			// set style
			if ( !bUsed )
				pRow->AddClass( "DebugPropertyRowNotUsed" );

			// used
			treePropertiesUsed.InsertOrReplace( symName );

			// if property is fully set, done with that too
			if ( pExistingProperty->BFullySet() )
				treePropertiesUsed.InsertOrReplace( pExistingProperty->GetPropertySymbol() );
		}
	}

	mapStyleBuffers.PurgeAndDeleteElements();

	FOR_EACH_HASHMAP( hashProperties, i )
	{
		CStylePropertyFactory::FreeStyleProperty( hashProperties[i] );
	}
	hashProperties.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Appends a list of all styles that are inherited by the debug target
//-----------------------------------------------------------------------------
void CDebugPanelStyle::AppendInheritedStyles()
{
	VPROF_BUDGET( "CDebugPanelStyle::AppendInheritedStyles", VPROF_BUDGETGROUP_TENFOOT );

	CPanel2D *pPanel = m_pDebugPanel.Get();
	Assert( pPanel );

	// currently, we are only allowing property inheritance for entire properties (ex: if margin was inheritable, couldn't inherit just margin-left)
	// the only exception is font, where color, size, weight, and family can all be separately inherited
	CUtlVector< CStyleSymbol > vecRemainingProperties;
	vecRemainingProperties = CStylePropertyFactory::GetInheritedProperties();	
	
	FontProperty_t fontFound = { 0 };

	// search from current panel through parents for each property
	for ( CPanel2D *pCurrent = pPanel; pCurrent != NULL; pCurrent = pCurrent->GetParent() )
	{
		IUIPanelStyle *pStyle = pCurrent->AccessStyle();

		CUtlVector< CStyleSymbol > vecInheritedProperties;
		FOR_EACH_VEC_BACK( vecRemainingProperties, iRemaining )
		{
			CStyleSymbol symProperty = vecRemainingProperties[iRemaining];

			const CStyleProperty *pProperty = pStyle->GetPropertyNoInherit( symProperty );
			if ( !pProperty )
				continue;

			// found
			vecRemainingProperties.Remove( iRemaining );

			//Don't display if set on the debug target.. will display in other location
			if ( pCurrent == pPanel )
				continue;			

			vecInheritedProperties.AddToTail( symProperty );
		}		

		// need to handle fonts separately
		FontProperty_t fontInherited = { 0 };
		const CStylePropertyFont *pFontProperty = (const CStylePropertyFont*)pStyle->GetPropertyNoInherit( CStylePropertyFont::symbol );
		if ( pFontProperty )
		{
			if ( !fontFound.family && !pFontProperty->m_strFontFamily.IsEmpty() )
			{
				fontFound.family = true;
				fontInherited.family = true;
			}
				
			if ( !fontFound.size && pFontProperty->m_flFontSize != k_flFloatNotSet )
			{
				fontFound.size = true;
				fontInherited.size = true;
			}
			
			if ( !fontFound.style && pFontProperty->m_eFontStyle != k_EFontStyleUnset )
			{
				fontFound.style = true;
				fontInherited.style = true;
			}
			
			if ( !fontFound.weight && pFontProperty->m_eFontWeight != k_EFontWeightUnset )
			{
				fontFound.weight = true;
				fontInherited.weight = true;
			}

			//Don't display if set on the debug target.. will display in other location
			if ( pCurrent == pPanel )
				V_memset( &fontInherited, 0, sizeof( fontInherited ) );
		}

		AppendStyleInfoForProperty( pCurrent, vecInheritedProperties, fontInherited );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finds the styles that map to 
//-----------------------------------------------------------------------------
void CDebugPanelStyle::AppendStyleInfoForProperty( CPanel2D *pPanel, const CUtlVector< CStyleSymbol > &vecProperties, FontProperty_t fontProperties )
{
	VPROF_BUDGET( "CDebugPanelStyle::AppendStyleInfoForProperty", VPROF_BUDGETGROUP_TENFOOT );

	if ( vecProperties.Count() == 0 && !fontProperties.BAnySet() )
		return;

	CUtlVector< CStyleSymbol > vecRemaining;
	vecRemaining = vecProperties;

	// add panel section
	CDebugInheritedStylesHeader *pHeader = new CDebugInheritedStylesHeader( this, NULL );
	pHeader->Init( pPanel );	

	// get sorted list of styles that apply to the target panel
	CUtlVector< CascadeStyleFileInfo_t > vecStyles;
	pPanel->UIPanel()->BBuildMatchingStyleList( &vecStyles );	

	// loop backward through styles, adding each to output
	MapStyleBuffers_t mapStyleBuffers;
	FOR_EACH_VEC_BACK( vecStyles, iStyles )
	{
		CascadeStyleFileInfo_t &styleInfo = vecStyles[iStyles];

		// only add this style if it contains a property we are still looking for
		CUtlVector< CStyleSymbol > vecStyleIncludes;
		FOR_EACH_VEC_BACK( vecRemaining, iRemaining )
		{
			CStyleSymbol symProperty = vecRemaining[ iRemaining ];
			const CStyleProperty *pProperty = styleInfo.m_pStyleFromFile->GetProperty( symProperty );
			if ( !pProperty )
				continue;

			vecStyleIncludes.AddToTail( symProperty );
			vecRemaining.Remove( iRemaining );
		}

		// and special case fonts
		FontProperty_t fontIncludes = { 0 };
		const CStylePropertyFont *pFontProperty = (CStylePropertyFont *)styleInfo.m_pStyleFromFile->GetProperty( CStylePropertyFont::symbol );
		if ( pFontProperty )
		{			
			if ( fontProperties.family && !pFontProperty->m_strFontFamily.IsEmpty() )
			{
				fontProperties.family = false;
				fontIncludes.family = true;
			}

			if ( fontProperties.size && pFontProperty->m_flFontSize != k_flFloatNotSet )
			{
				fontProperties.size = false;
				fontIncludes.size = true;
			}

			if ( fontProperties.style && pFontProperty->m_eFontStyle != k_EFontStyleUnset )
			{
				fontProperties.style = false;
				fontIncludes.style = true;
			}

			if ( fontProperties.weight && pFontProperty->m_eFontWeight != k_EFontWeightUnset )
			{
				fontProperties.weight = false;
				fontIncludes.weight = true;
			}			
		}

		// anything in this style?
		if ( vecStyleIncludes.Count() == 0 && !fontIncludes.BAnySet() )
			continue;

		IUILayoutFile *pLayoutFile = vecStyles[iStyles].m_pLayoutFile;

		CUtlVector< CPanoramaSymbol > vecStyleFiles;
		pLayoutFile->GetStyleFileSymbols( vecStyles[ iStyles ].m_iStyleFile, vecStyleFiles );

		if ( vecStyleFiles.Count() == 0 )
			continue;

		CPanoramaSymbol symFile = vecStyleFiles.Tail();
		CUtlBuffer *pBuffer = GetStyleBuffer( &mapStyleBuffers, pLayoutFile, symFile );
		if ( !pBuffer )
			continue;

		// create all style info
		CDebugIndividualStyle *pStylePanel = new CDebugIndividualStyle( this, NULL );
		pStylePanel->Init( pLayoutFile->GetLayoutFileSymbol(), vecStyleFiles, vecStyles[ iStyles ].m_pStyleFromFile->m_unFileLocation, vecStyles[ iStyles ].m_pStyleFromFile->m_unFileOrder );
		pStylePanel->SetTabIndex( k_flTabIndexAuto );

		// skip to the style section
		pBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, vecStyles[iStyles].m_pStyleFromFile->m_unFileLocation );

		CDebugStyleBlock *pStyleBlock = pStylePanel->GetStyleBlock();
		if ( !SetStyleInfo( pBuffer, pStyleBlock ) )
		{
			AssertMsg( false, "Failed to parse style info" );
			delete pStylePanel;
			continue;
		}

		// only show inherited properties
		if ( fontIncludes.BAnySet() )
			vecStyleIncludes.AddToTail( CStylePropertyFont::symbol );

		if ( fontIncludes.family )
			vecStyleIncludes.AddToTail( CStylePropertyFont::fontFamily );

		if ( fontIncludes.size )
			vecStyleIncludes.AddToTail( CStylePropertyFont::fontSize );

		if ( fontIncludes.style )
			vecStyleIncludes.AddToTail( CStylePropertyFont::fontStyle );

		if ( fontIncludes.weight )
			vecStyleIncludes.AddToTail( CStylePropertyFont::fontWeight );

		pStyleBlock->OnlyShowProperties( vecStyleIncludes );
	}
	
	mapStyleBuffers.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: Append animations
//-----------------------------------------------------------------------------
void CDebugPanelStyle::AppendAnimations()
{
	VPROF_BUDGET( "CDebugPanelStyle::AppendAnimations", VPROF_BUDGETGROUP_TENFOOT );	

	CPanel2D *pPanel = m_pDebugPanel.Get();
	Assert( pPanel );

	// need to get style from our layout file
	IUILayoutFile *pLayoutFile = UIEngine()->UILayoutManager()->GetLayoutFile( pPanel->GetLayoutFile() );
	if ( !pLayoutFile )
	{
		AssertMsg( false, "Couldn't find layout file" );
		return;
	}

	CUtlVector< CPanoramaSymbol > vecAnimationNames;
	m_pDebugPanel->AccessStyle()->GetAnimationNames( &vecAnimationNames );

	if ( vecAnimationNames.Count() == 0 )
		return;

	// add header
	CDebugStyleSeparator *pHeader = new CDebugStyleSeparator( this, NULL );
	pHeader->Init( "#Debugger_AnimationHeader" );

	MapStyleBuffers_t mapStyleBuffers;
	FOR_EACH_VEC( vecAnimationNames, i )
	{
		const CStyleAnimation *pStyleAnimation = pLayoutFile->GetAnimation( vecAnimationNames[i] );
		if ( !pStyleAnimation )
			return;

		// load file
		CUtlBuffer *pBuffer = GetStyleBuffer( &mapStyleBuffers, pLayoutFile, pStyleAnimation->GetStyleFile() );
		if ( !pBuffer )
		{
			AssertMsg( false, "Could not find style file for animation" );
			continue;
		}

		// create panel
		CDebugStyleAnimation *pAnimation = new CDebugStyleAnimation( this, NULL );
		pAnimation->Init( pLayoutFile->GetLayoutFileSymbol(), pStyleAnimation->GetStyleFile(), pStyleAnimation->GetFileLocation(), pStyleAnimation->GetFileOrder() );
		pAnimation->SetTabIndex( k_flTabIndexAuto );

		// parse
		if ( !SetAnimationInfo( pBuffer, pAnimation ) )
		{
			AssertMsg( false, "Failed to parse style info" );
			delete pAnimation;
			continue;
		}
	}

	mapStyleBuffers.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the panel we are debugging
//-----------------------------------------------------------------------------
bool CDebugPanelStyle::OnSetDebugTarget( CPanelPtr< CPanel2D > pPanel )
{
	m_pDebugPanel = pPanel;	
	Build();

	// let others handle this message
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Styles for a panel changed
//-----------------------------------------------------------------------------
bool CDebugPanelStyle::OnPanelStyleChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	VPROF_BUDGET( "CDebugPanelStyle::OnPanelStyleChanged", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_pDebugPanel == pPanel && pPanel.Get() )
	{
		// if this window or descendant has input focus, do not rebuild. User is most likely editing a style
		if ( !BHasKeyFocus() && !BHasDescendantKeyFocus() )
			Build();
		else
			DispatchEvent( DebugStyleStatus(), this, false );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugIndividualStyle::CDebugIndividualStyle( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	m_unFileLocation = 0;
	DbgVerify( BLoadLayout( "file://{resources}/layout/debugindividualstyle.xml" ) );

	m_pStyleLink = (CLabel*)FindChildInLayoutFile( "StyleLink" );
	m_pStyleBlock = (CDebugStyleBlock*)FindChildInLayoutFile( "StyleBlock" );		
	
	RegisterForUnhandledEvent( InMemoryFileUpdate(), this, &CDebugIndividualStyle::EventInMemoryFileUpdate );
	RegisterEventHandler( UpdateStyleInMemory(), this, &CDebugIndividualStyle::EventUpdateStyleInMemory );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugIndividualStyle::~CDebugIndividualStyle()
{	
	UnregisterForUnhandledEvent( InMemoryFileUpdate(), this, &CDebugIndividualStyle::EventInMemoryFileUpdate );	
}


//-----------------------------------------------------------------------------
// Purpose: Tells layout manager to update the file's style with changes
//-----------------------------------------------------------------------------
void CDebugIndividualStyle::UpdateInMemoryFile()
{
	if ( !m_symStylePath.IsValid() || m_pStyleBlock->BContainsErrors() )
		return;

	CFmtStrMax fmt;
	m_pStyleBlock->GetText( &fmt );
	UIEngine()->UILayoutManager()->UpdateStyleInMemory( IUILayoutManager::k_EUpdateStyleStyle, m_symStylePath, m_unFileLocation, fmt );
}


//-----------------------------------------------------------------------------
// Purpose: Init the style section
//-----------------------------------------------------------------------------
void CDebugIndividualStyle::Init( CPanoramaSymbol symLayoutFile, const CUtlVector< CPanoramaSymbol > &vecStylePath, uint unFileLocation, uint unFileOrder )
{
	if ( vecStylePath.Count() > 0 )
	{
		m_symStylePath = vecStylePath.Tail();
	}
	else
	{
		m_symStylePath = CPanoramaSymbol();
	}

	m_unFileLocation = unFileLocation;

	m_pStyleBlock->SetTabDepth( 0 );
	m_pStyleBlock->SetDebugInfo( symLayoutFile, m_symStylePath, unFileOrder );

	SetDialogVariable( "stylefile", ConstructStylePathString( vecStylePath ) );
	//SetDialogVariable( "styleline", (int)unFileLocation );

	if ( m_symStylePath.IsValid() )
	{
		m_pStyleLink->SetText( "#Debugger_StyleFileLink" );
		m_pStyleLink->SetTabIndex( k_flTabIndexAuto );
		m_pStyleLink->SetOnActivateEvent( OpenFileForEdit::MakeEvent( this, m_symStylePath.String(), unFileLocation ) );
	}
	else
	{
		m_pStyleLink->SetText( "#Debugger_LayoutFileLink_Code" );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: A file was updated in memory
//-----------------------------------------------------------------------------
bool CDebugIndividualStyle::EventInMemoryFileUpdate( CPanoramaSymbol symFile, uint unLocation, uint unOldSize, uint unNewSize )
{
	// update our file location if a style changed with was before our file location
	if ( m_symStylePath == symFile && unLocation < m_unFileLocation )
		m_unFileLocation += unNewSize - unOldSize;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we need to update our style
//-----------------------------------------------------------------------------
bool CDebugIndividualStyle::EventUpdateStyleInMemory()
{
	UpdateInMemoryFile();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugStyleAnimation::CDebugStyleAnimation( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	m_unFileLocation = 0;
	DbgVerify( BLoadLayout( "file://{resources}/layout/debugstyleanimation.xml" ) );

	m_pName = (CTextEntry *)FindChildInLayoutFile( "AnimationName" );
	m_pStyleLink = (CLabel*)FindChildInLayoutFile( "StyleLink" );
	m_pFrames = FindChildInLayoutFile( "FrameSection" );

	RegisterForUnhandledEvent( InMemoryFileUpdate(), this, &CDebugStyleAnimation::EventInMemoryFileUpdate );
	RegisterEventHandler( UpdateStyleInMemory(), this, &CDebugStyleAnimation::EventUpdateStyleInMemory );
	RegisterEventHandler( Activated(), this, &CDebugStyleAnimation::EventPanelActivated );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugStyleAnimation::~CDebugStyleAnimation()
{	
	UnregisterForUnhandledEvent( InMemoryFileUpdate(), this, &CDebugStyleAnimation::EventInMemoryFileUpdate );	
}


//-----------------------------------------------------------------------------
// Purpose: Tells layout manager to update the file's style with changes
//-----------------------------------------------------------------------------
void CDebugStyleAnimation::UpdateInMemoryFile()
{
	if ( !m_symStylePath.IsValid() )
		return;

	CFmtStrMax fmt;
	fmt.AppendFormat( "@keyframes '%s'\r\n{\r\n", m_pName->PchGetText() );

	// add frames
	for ( int i = 0; i < m_pFrames->GetChildCount(); i++ )
	{
		// add an empty line between frames
		if ( i > 0 )
			fmt.AppendFormat( "\t\r\n" );

		CPanel2D *pRow = m_pFrames->GetChild( i );		
		CDebugStyleBlock *pStyleBlock = (CDebugStyleBlock*)pRow->FindChild( "FrameStyleBlock" );
		if ( pStyleBlock->BContainsErrors() )
			return;

		pStyleBlock->GetText( &fmt );
		fmt.AppendFormat( "\r\n" );
	}
	fmt.AppendFormat( "}" );

	UIEngine()->UILayoutManager()->UpdateStyleInMemory( IUILayoutManager::k_EUpdateStyleKeyframes, m_symStylePath, m_unFileLocation, fmt );
}


//-----------------------------------------------------------------------------
// Purpose: Init the style section
//-----------------------------------------------------------------------------
void CDebugStyleAnimation::Init( CPanoramaSymbol symLayoutFile, CPanoramaSymbol symStylePath, uint unFileLocation, uint unFileOrder )
{
	m_symLayoutFile = symLayoutFile;
	m_symStylePath = symStylePath;
	m_unFileLocation = unFileLocation;
	m_unFileOrder = unFileOrder;
	
	SetDialogVariable( "stylefile", ConstructStylePathString( symStylePath ) );
	//SetDialogVariable( "styleline", (int)unFileLocation );

	if ( m_symStylePath.IsValid() )
	{
		m_pStyleLink->SetText( "#Debugger_StyleFileLink" );
		m_pStyleLink->SetTabIndex( k_flTabIndexAuto );
		m_pStyleLink->SetOnActivateEvent( OpenFileForEdit::MakeEvent( this, symStylePath.String(), unFileLocation ) );
	}
	else
	{
		m_pStyleLink->SetText( "#Debugger_LayoutFileLink_Code" );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: A file was updated in memory
//-----------------------------------------------------------------------------
bool CDebugStyleAnimation::EventInMemoryFileUpdate( CPanoramaSymbol symFile, uint unLocation, uint unOldSize, uint unNewSize )
{
	// update our file location if a style changed with was before our file location
	if ( m_symStylePath == symFile && unLocation < m_unFileLocation )
		m_unFileLocation += unNewSize - unOldSize;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we need to update our style
//-----------------------------------------------------------------------------
bool CDebugStyleAnimation::EventUpdateStyleInMemory()
{
	UpdateInMemoryFile();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Adds panels for a frame
//-----------------------------------------------------------------------------
CDebugStyleBlock *CDebugStyleAnimation::AddFrame()
{
	// add a panel for each row. Style class should flow to the right
	CPanel2D *pRow = new CPanel2D( m_pFrames, "FrameRow" );
	pRow->SetTabIndex( k_flTabIndexAuto );
	pRow->AddClass( "DebugFrameRow" );

	// make a padding section where we will also place the error box if necessary
	CPanel2D *pLeftMargin = new CPanel2D( pRow, "LeftMargin" );
	pLeftMargin->AddClass( "DebugFrameLeftMargin" );
	pLeftMargin->SetTabIndex( k_flTabIndexInvalid );

	CButton *pInsert = new CButton( pLeftMargin, "InsertFrameRowButton" );
	pInsert->AddClass( "DebugPropRowButton" );
	CLabel *pLabel = new CLabel( pInsert, NULL );
	pLabel->SetText( "+" );

	CButton *pDelete = new CButton( pLeftMargin, "DeleteFrameRowButton" );
	pDelete->AddClass( "DebugPropRowButton" );
	pLabel = new CLabel( pDelete, NULL );
	pLabel->SetText( "X" );

	CDebugStyleBlock *pBlock = new CDebugStyleBlock( pRow, "FrameStyleBlock" );
	pBlock->SetTabDepth( 1 );
	pBlock->SetDebugInfo( m_symLayoutFile, m_symStylePath, m_unFileOrder );

	return pBlock;
}


//-----------------------------------------------------------------------------
// Purpose: Sets animation name
//-----------------------------------------------------------------------------
void CDebugStyleAnimation::SetName( const char *pchName )
{
	m_pName->SetText( pchName );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a child panel is activated
//-----------------------------------------------------------------------------
bool CDebugStyleAnimation::EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pActivated = ToPanel2D( pPanel.Get() );
	if ( !pActivated )
		return false;

	const char *pchID = pActivated->GetID();
	if ( V_stricmp( pchID, "InsertFrameRowButton" ) == 0 )
	{
		CPanel2D *pRow = pActivated->FindAncestor( "FrameRow" );
		Assert( pRow );

		CDebugStyleBlock *pBlock = AddFrame();
		pBlock->AddEmptyLine();

		CPanel2D *pNewRow = pBlock->FindAncestor( "FrameRow" );
		Assert( pNewRow );
		
		m_pFrames->MoveChildAfter( pNewRow, pRow );
		pBlock->FocusSelector();

		return true;
	}
	else if ( V_stricmp( pchID, "DeleteFrameRowButton" ) == 0 )
	{
		CPanel2D *pRow = pActivated->FindAncestor( "FrameRow" );
		pRow->DeleteAsync();
		
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugStyleBlock::CDebugStyleBlock( CPanel2D *pParent, const char *pchName ) : CPanel2D( pParent, pchName )
{
	m_bDirty = false;
	m_unTabDepth = 0;
	DbgVerify( BLoadLayout( "file://{resources}/layout/debugstyleblock.xml" ) );

	m_pSelector = (CTextEntry*)FindChildInLayoutFile( "Selector" );
	m_pPropertySection = FindChildInLayoutFile( "PropertySection" );

	RegisterEventHandler( TextEntrySubmit(), this, &CDebugStyleBlock::EventTextEntrySubmit );
	RegisterEventHandler( TextEntryChanged(), this, &CDebugStyleBlock::EventTextEntryChanged );
	RegisterEventHandler( Activated(), this, &CDebugStyleBlock::EventPanelActivated );
	RegisterEventHandler( InputFocusLost(), this, &CDebugStyleBlock::EventFocusLost );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDebugStyleBlock::~CDebugStyleBlock()
{
	ClearPropertySection();

	CPanel2D *pPanel = m_pContextMenuName.Get();
	SAFE_DELETE( pPanel );
	pPanel = m_pContextMenuValue.Get();
	SAFE_DELETE( pPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Sets selector text
//-----------------------------------------------------------------------------
void CDebugStyleBlock::SetSelector( const char *pchSelector )
{
	m_pSelector->SetText( pchSelector );
}


//-----------------------------------------------------------------------------
// Purpose: Clears all rows
//-----------------------------------------------------------------------------
void CDebugStyleBlock::ClearPropertySection()
{
	for ( int i = 0; i < m_pPropertySection->GetChildCount(); i++ )
	{
		RemoveInputHooksFromRow( m_pPropertySection->GetChild( i ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a property to the style section
//-----------------------------------------------------------------------------
void CDebugStyleBlock::AddProperty( const char *pchName, const char *pchValue )
{
	CreateRow( pchName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Add a comment to the style section
//-----------------------------------------------------------------------------
void CDebugStyleBlock::AddComment( const char *pchComment )
{
	CreateRow( pchComment, NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Adds an empty line to the style section
//-----------------------------------------------------------------------------
void CDebugStyleBlock::AddEmptyLine()
{
	CreateRow( "", NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a property row
//-----------------------------------------------------------------------------
CPanel2D *CDebugStyleBlock::CreateRow( const char *pchName, const char *pchValue )
{
	// add a panel for each row. Style class should flow to the right
	CPanel2D *pRow = new CPanel2D( m_pPropertySection, "PropRow" );
	pRow->SetTabIndex( k_flTabIndexAuto );
	pRow->AddClass( "DebugPropertyRow" );

	// make a padding section where we will also place the error box if necessary
	CPanel2D *pLeftMargin = new CPanel2D( pRow, "LeftMargin" );
	pLeftMargin->AddClass( "DebugPropertyLeftMargin" );
	pLeftMargin->SetTabIndex( k_flTabIndexInvalid );

	CButton *pInsert = new CButton( pLeftMargin, "InsertRowButton" );
	pInsert->AddClass( "DebugPropRowButton" );
	CLabel *pLabel = new CLabel( pInsert, NULL );
	pLabel->SetText( "+" );

	CButton *pDelete = new CButton( pLeftMargin, "DeleteRowButton" );
	pDelete->AddClass( "DebugPropRowButton" );
	pLabel = new CLabel( pDelete, NULL );
	pLabel->SetText( "X" );

	CTextEntry *pName = new CTextEntry( pRow, "PropName" );
	pName->SetUndoHistoryEnabled( true );
	pName->SetTabIndex( k_flTabIndexAuto );
	pName->AddClass( "DebugPropertyName" );
	pName->SetText( pchName );
	GetParentWindow()->UIWindowInput()->HookPanelInput( pName->UIPanel(), this );

	CLabel *pColon = new CLabel( pRow, "PropColon" );
	pColon->AddClass( "DebugPropertyColon" );
	pColon->SetText( ":" );
	pColon->SetVisible( pchValue != NULL );

	CTextEntry *pValue = new CTextEntry( pRow, "PropValue" );
	pValue->SetUndoHistoryEnabled( true );
	pValue->SetTabIndex( k_flTabIndexAuto );
	pValue->AddClass( "DebugPropertyValue" );
	pValue->SetText( pchValue ? pchValue : "" );
	pValue->SetVisible( pchValue != NULL );
	GetParentWindow()->UIWindowInput()->HookPanelInput( pValue->UIPanel(), this );

	CLabel *pSemicolon = new CLabel( pRow, "PropSemicolon" );
	pSemicolon->AddClass( "DebugPropertySemicolon" );
	pSemicolon->SetText( ";" );
	pSemicolon->SetVisible( pchValue != NULL );

	pName->RaiseChangeEvents( true );
	pValue->RaiseChangeEvents( true );

	// set tooltip
	if ( BLooksLikeProperty( pchName ) )
	{
		CStyleSymbol symStyle( pchName );
		if ( symStyle.IsValid() )
		{
			CStyleProperty *pProperty = UIEngine()->UIStyleFactory()->CreateStyleProperty( symStyle );
			if ( pProperty )
			{
				SetPropertyTooltip( pRow, pProperty->GetDescription( symStyle ) );
				UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );
			}
		}
	}

	return pRow;
}


//-----------------------------------------------------------------------------
// Purpose: Shows or hides controls that are not visible when the row is a comment or empty
//-----------------------------------------------------------------------------
void CDebugStyleBlock::SetRowCommentOrEmpty( CPanel2D *pRow, bool bEnabled )
{
	Assert( pRow );

	CTextEntry *pValue = (CTextEntry*)pRow->FindChild( "PropValue" );
	Assert( pValue );

	pValue->SetVisible( bEnabled );
	pValue->SetText( "" );

	pRow->FindChild( "PropColon" )->SetVisible( bEnabled );
	pRow->FindChild( "PropSemicolon" )->SetVisible( bEnabled );
}


//-----------------------------------------------------------------------------
// Purpose: Called when the user has finished entering text into a child text entry
//-----------------------------------------------------------------------------
bool CDebugStyleBlock::TextEntryUpdated( CPanel2D *pPanel )
{
	if ( !pPanel || !m_bDirty )
		return false;

	m_bDirty = false;
	bool bName = (V_strcmp( pPanel->GetID(), "PropName" ) == 0);
	bool bValue = (V_strcmp( pPanel->GetID(), "PropValue" ) == 0);
	if ( !bName && !bValue )
		return false;

	// get the row for this property
	CPanel2D *pRow = pPanel->GetParent();
	Assert( V_strcmp( pRow->GetID(), "PropRow" ) == 0 );

	// check if the property name is valid
	CTextEntry *pName = (CTextEntry*)pRow->FindChild( "PropName" );
	const char *pchName = pName->PchGetText();
	CTextEntry *pValue = (CTextEntry*)pRow->FindChild( "PropValue" );
	const char *pchValue = pValue->PchGetText();

	// can't do much w/o a property name	
	if ( !BLooksLikeProperty( pchName ) )
	{
		if ( pchValue[0] != '\0' )
		{
			SetPropertyError( pRow, "Value set w/o name" );
		}
		else
		{
			ClearPropertyError( pRow );
			DispatchEvent( UpdateStyleInMemory(), this );
		}

		return true;
	}

	CStyleSymbol symProperty( pchName );
	CStyleProperty *pProperty = UIEngine()->UIStyleFactory()->CreateStyleProperty( symProperty );
	if ( !pProperty )
	{
		SetPropertyError( pRow, "Invalid property name" );
		return true;
	}

	// if name changed, update tooltip for the row
	if ( bName )
		SetPropertyTooltip( pRow, pProperty->GetDescription( symProperty ) );

	// if a value is set, try to parse the text
	if ( pchValue[0] == '\0' )
	{
		// no property value yet, return
		ClearPropertyError( pRow );
		UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );
		return true;
	}
	else
	{
		IUILayoutFile *pLayoutFile = UIEngine()->UILayoutManager()->GetLayoutFile( m_symLayoutFile );
		if ( !pLayoutFile )
		{
			SetPropertyError( pRow, "Could not find layout panel" );
			UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );
			return false;
		}

		// property value set
		char rgchBuffer[1024];
		V_strncpy( rgchBuffer, pchValue, V_ARRAYSIZE( rgchBuffer ) );
		if ( !pLayoutFile->BReplaceDefines( rgchBuffer, V_ARRAYSIZE( rgchBuffer ), m_unFileOrder ) )
		{
			SetPropertyError( pRow, "Could not replace defines" );
			UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );
			return false;
		}

		if ( !pProperty->BSetFromString( symProperty, rgchBuffer ) )
		{
			SetPropertyError( pRow, "Invalid property value" );
			UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );
			return true;
		}
	}

	ClearPropertyError( pRow );
	DispatchEvent( UpdateStyleInMemory(), this );
	UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when the user has finished entering text into a child text entry
//-----------------------------------------------------------------------------
bool CDebugStyleBlock::EventTextEntrySubmit( const CPanelPtr< IUIPanel > &pPanel, const char *pchText )
{
	return TextEntryUpdated( ToPanel2D(pPanel.Get()) );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a child panel or ourselves loses focus
//-----------------------------------------------------------------------------
bool CDebugStyleBlock::EventFocusLost( const CPanelPtr< IUIPanel > &pPanel )
{
	// focus could be moving to the autocomplete. Do not update.
	if ( m_pContextMenuName.Get() )
		return false;

	// focus could be moving to the autocomplete. Do not update.
	if ( m_pContextMenuValue.Get() )
		return false;

	return TextEntryUpdated( ToPanel2D( pPanel.Get() ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets property tooltip text
//-----------------------------------------------------------------------------
void CDebugStyleBlock::SetPropertyTooltip( CPanel2D *pRow, const char *pchDescription )
{	
	CTextTooltip *pTooltip = NULL;
	if ( pchDescription[0] != '\0' )
	{
		pTooltip = new CTextTooltip( GetParentWindow(), NULL );
		pTooltip->SetTooltipTarget( pRow );
		pTooltip->SetText( pchDescription, CLabel::k_ETextTypeHTML );
	}

	pRow->SetTooltip( pTooltip );
}


//-----------------------------------------------------------------------------
// Purpose: Creates UI to show an error for the specified property row
//-----------------------------------------------------------------------------
void CDebugStyleBlock::SetPropertyError( CPanel2D *pRow, const char *pchError )
{
	CPanel2D *pLeftMargin = pRow->FindChild( "LeftMargin" );
	Assert( pLeftMargin );

	CPanel2D *pErrorBox = pLeftMargin->FindChild( "ErrorBox" );
	if ( !pErrorBox )
	{
		pErrorBox = new CPanel2D( pLeftMargin, "ErrorBox" );
		pErrorBox->AddClass( "DebugPropertyErrorBox" );
	}

	CTextTooltip *pTooltip = new CTextTooltip( GetParentWindow(), NULL );
	pTooltip->SetText( pchError );
	pErrorBox->SetTooltip( pTooltip );

	if ( !m_vecRowsWithErrors.HasElement( pRow ) )
		m_vecRowsWithErrors.AddToTail( pRow );
}


//-----------------------------------------------------------------------------
// Purpose: Clears a property error if set
//-----------------------------------------------------------------------------
void CDebugStyleBlock::ClearPropertyError( CPanel2D *pRow )
{
	m_vecRowsWithErrors.FindAndFastRemove( pRow );
	CPanel2D *pErrorBox = pRow->FindChild( "LeftMargin" )->FindChild( "ErrorBox" );
	if ( !pErrorBox )
		return;

	pErrorBox->SetParent( NULL );
	delete pErrorBox;	
}


//-----------------------------------------------------------------------------
// Purpose: Called when the user has entered a key into a child text entry
//-----------------------------------------------------------------------------
bool CDebugStyleBlock::EventTextEntryChanged( const CPanelPtr< IUIPanel > &ptrPanel )
{
	CPanel2D *pPanel = ToPanel2D(ptrPanel.Get());
	if ( !pPanel || pPanel->GetPanelType() != CTextEntry::GetPanelSymbol() )
		return false;

	CTextEntry *pTextEntry = (CTextEntry*)pPanel;
	if ( V_strcmp( "PropName", pPanel->GetID() ) == 0 )
	{
		const char *pchText = pTextEntry->PchGetText();
		int cchText = V_strlen( pchText );

		// if not a comment, show all fields
		if ( cchText == 1 )
			SetRowCommentOrEmpty( pPanel->FindAncestor( "PropRow" ), pchText[0] != '/' );
		
		PopulateNameSuggestions( pTextEntry );
	}

	if ( V_strcmp( "PropValue", pPanel->GetID() ) == 0 )
	{
		CPanel2D *pRow = pPanel->FindAncestor( "PropRow" );
		if ( pRow )
		{
			CPanel2D *pName = pRow->FindChildTraverse( "PropName" );
			if ( pName )
			{
				CTextEntry *pNameEntry = (CTextEntry*)pName;
				PopulateValueSuggestions( pNameEntry, pTextEntry );
			}
		}
	}

	m_bDirty = true;

	// swallow all key typed events. No reason for them to continue to bubble
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Updates name suggestions in context menu
//-----------------------------------------------------------------------------
void CDebugStyleBlock::PopulateNameSuggestions( CTextEntry *pName )
{
	CDebugAutoComplete *pPanel = m_pContextMenuName.Get();
	if ( !pPanel )
	{
		pPanel = new CDebugAutoComplete( pName, "#NameSuggestions" );
		m_pContextMenuName = pPanel;
	}

	pPanel->PopulateNameSuggestions();
}


//-----------------------------------------------------------------------------
// Purpose: Updates value suggestions in context menu
//-----------------------------------------------------------------------------
void CDebugStyleBlock::PopulateValueSuggestions( CTextEntry *pNameEntry, CTextEntry *pTextEntry )
{
	CDebugAutoComplete *pPanel = m_pContextMenuValue.Get();
	if ( !pPanel )
	{
		pPanel = new CDebugAutoComplete( pTextEntry, "#NameSuggestions" );
		m_pContextMenuValue = pPanel;
	}

	pPanel->PopulateValueSuggestions( pNameEntry->PchGetText() );
}


//-----------------------------------------------------------------------------
// Purpose: Tells layout manager to update the file's style with changes
//-----------------------------------------------------------------------------
void CDebugStyleBlock::GetText( CFmtStrMax *pfmt )
{
	// add selector
	AppendTabs( pfmt, m_unTabDepth );
	pfmt->AppendFormat( "%s\r\n", m_pSelector->PchGetText() );
	AppendTabs( pfmt, m_unTabDepth );
	pfmt->AppendFormat( "{\r\n" );

	// add properties
	for( int i = 0; i < m_pPropertySection->GetChildCount(); i++ )
	{
		CPanel2D *pRow = m_pPropertySection->GetChild( i );
		CTextEntry *pName = (CTextEntry*)pRow->FindChild( "PropName" );
		CTextEntry *pValue = (CTextEntry*)pRow->FindChild( "PropValue" );
		Assert( pName && pValue );
		
		// empty line?
		if ( pName->PchGetText()[0] == '\0' )
		{
			pfmt->Append( "\r\n" );
			continue;
		}

		// at minimum, name is set (could be comment or property/value)
		AppendTabs( pfmt, m_unTabDepth + 1 );
		if ( pValue->PchGetText()[0] == '\0' )
			pfmt->AppendFormat( "%s\r\n", pName->PchGetText() );
		else
			pfmt->AppendFormat( "%s: %s;\r\n", pName->PchGetText(), pValue->PchGetText() );
	}

	// no need for return line
	AppendTabs( pfmt, m_unTabDepth );
	pfmt->AppendFormat( "}" );
}


//-----------------------------------------------------------------------------
// Purpose: Places input focus on this block's selector
//-----------------------------------------------------------------------------
void CDebugStyleBlock::FocusSelector()
{
	m_pSelector->SetFocus();
}


//-----------------------------------------------------------------------------
// Purpose: Hides all properties not in the provided list
//-----------------------------------------------------------------------------
void CDebugStyleBlock::OnlyShowProperties( const CUtlVector< CStyleSymbol > &vecProperties )
{
	for ( int i = 0; i < m_pPropertySection->GetChildCount(); i++ )
	{
		CPanel2D *pRow = m_pPropertySection->GetChild( i );
		CTextEntry *pName = (CTextEntry*)pRow->FindChild( "PropName" );
		
		CStyleSymbol symName ( pName->PchGetText() );
		if ( !vecProperties.HasElement( symName ) )
			pRow->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when a child panel is activated
//-----------------------------------------------------------------------------
bool CDebugStyleBlock::EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pActivated = ToPanel2D( pPanel.Get() );
	if ( !pActivated )
		return false;

	const char *pchID = pActivated->GetID();
	if ( V_stricmp( pchID, "InsertRowButton" ) == 0 )
	{
		CreateNewRowAfter( pActivated->FindAncestor( "PropRow" ) );
		return true;
	}
	else if ( V_stricmp( pchID, "DeleteRowButton" ) == 0 )
	{
		DeleteRow( pActivated->FindAncestor( "PropRow" ) );	
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a text entry we are capturing input on has a key down event
//-----------------------------------------------------------------------------
bool CDebugStyleBlock::OnCapturedKeyDown( IUIPanel *pPanel, const KeyData_t &code )
{
	Assert( ToPanel2D(pPanel)->GetPanelType() == CTextEntry::GetPanelSymbol() );
	CTextEntry *pTextEntry = (CTextEntry*)ToPanel2D( pPanel );
	
	if ( code.m_KeyCode == KEY_ENTER && IsShiftPressed( code.m_Modifiers ) && !IsAltPressed( code.m_Modifiers) && !IsControlPressed( code.m_Modifiers ) )
	{
		// forward to the textentry so it will fire the submit event
		pTextEntry->OnKeyDown( code );

		// create a new row after the current
		CreateNewRowAfter( pTextEntry->FindAncestor( "PropRow" ) );
		return true;
	}
	else if ( code.m_KeyCode == KEY_BACKSPACE && IsShiftPressed( code.m_Modifiers ) && !IsAltPressed( code.m_Modifiers) && !IsControlPressed( code.m_Modifiers ) )
	{
		// remove row
		DeleteRow( pTextEntry->FindAncestor( "PropRow" ) );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a new row after the specified row
//-----------------------------------------------------------------------------
CPanel2D *CDebugStyleBlock::CreateNewRowAfter( CPanel2D *pRow )
{
	Assert( pRow );

	// add an empty line and set focus
	CPanel2D *pNewRow = CreateRow( "", NULL );
	m_pPropertySection->MoveChildAfter( pNewRow, pRow );

	CPanel2D *pName = pNewRow->FindChild( "PropName" );
	Assert( pName );
	pName->SetFocus();
	m_bDirty = true;
	
	return pNewRow;
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the specified row
//-----------------------------------------------------------------------------
void CDebugStyleBlock::DeleteRow( CPanel2D *pRow )
{
	// set focus to the next row
	if ( m_pPropertySection->GetChildCount() > 1 )
	{
		int iChild = m_pPropertySection->GetChildIndex( pRow );
		iChild = (iChild == 0) ? iChild + 1 : iChild - 1;
		CPanel2D *pName = m_pPropertySection->GetChild( iChild )->FindChild( "PropName" );
		pName->SetFocus();
	}
	
	// delete
	Assert( pRow );
	RemoveInputHooksFromRow( pRow );	
	pRow->DeleteAsync();
	
	m_bDirty = true;

	if ( m_pPropertySection->GetChildCount() == 0 )
	{
		CPanel2D *pNewRow = CreateRow( "", NULL );
		CPanel2D *pName = pNewRow->FindChild( "PropName" );
		pName->SetFocus();
	}

	DispatchEvent( UpdateStyleInMemory(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Removes input hooks from panels in a row
//-----------------------------------------------------------------------------
void CDebugStyleBlock::RemoveInputHooksFromRow( CPanel2D *pRow )
{
	GetParentWindow()->UIWindowInput()->RemovePanelInputHook( pRow->FindChild( "PropName" )->UIPanel(), this );
	GetParentWindow()->UIWindowInput()->RemovePanelInputHook( pRow->FindChild( "PropValue" )->UIPanel(), this );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CDebugStyleBlock::ValidateClientPanel( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_vecRowsWithErrors );

	BaseClass::ValidateClientPanel( validator, pchName );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CDebugInheritedStylesHeader::CDebugInheritedStylesHeader( CPanel2D *pParent, const char *pchName ) : BaseClass( pParent, pchName )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/debuginheritedstylesheader.xml" ) );
}


//-----------------------------------------------------------------------------
// Purpose: Sets panel data
//-----------------------------------------------------------------------------
void CDebugInheritedStylesHeader::Init( CPanel2D *pPanel )
{
	char buffer[256];
	GetDebugPanelName( buffer, V_ARRAYSIZE( buffer ), pPanel ? pPanel->UIPanel() : NULL );

	CLabel *pLabel = (CLabel*)FindChild( "ParentWithProperties" );	
	pLabel->SetText( buffer );
	pLabel->SetTabIndex( k_flTabIndexAuto );
	pLabel->SetOnActivateEvent( SetDebugTarget::MakeEvent( this, pPanel ) );
}


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CDebugStyleSeparator::CDebugStyleSeparator( CPanel2D *pParent, const char *pchName ) : BaseClass( pParent, pchName )
{
}


//-----------------------------------------------------------------------------
// Purpose: Sets panel data
//-----------------------------------------------------------------------------
void CDebugStyleSeparator::Init( const char *pchText )
{
	CLabel *pLabel = new CLabel( this, pchText );
	pLabel->SetText( pchText );	
}

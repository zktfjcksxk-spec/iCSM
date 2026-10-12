//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/label.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/controls/contextmenu.h"
#include "panorama/controls/image.h"
#include "panorama/uijsregistration.h"
#include "panorama/renderer/styleproperties.h"
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
#include "vstdlib/htmltools.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// Debugging: turn on extra validation of loc strings.
#if 0
#define VALIDATE_LOC_TEXT 1
#endif

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CLabel, Label );
REGISTER_PANEL_NAME( a );
REGISTER_PANEL_NAME( b );
REGISTER_PANEL_NAME( i );
REGISTER_PANEL_NAME( em );
REGISTER_PANEL_NAME( strong );
REGISTER_PANEL_NAME( h1 );
REGISTER_PANEL_NAME( h2 );
REGISTER_PANEL_NAME( pre );

DECLARE_PANEL_EVENT0( FindLongestStringForLocVariable );
DEFINE_PANORAMA_EVENT( FindLongestStringForLocVariable );
DEFINE_PANORAMA_EVENT( CopySelectedLabelText );

const char k_szEventPrefix[] = "event:";
const char k_szJavascriptPrefix[] = "javascript:";

#if !defined( SOURCE2_PANORAMA )
static CCommandLineParam sAllLanguages( "-all_languages", "show longest loc string from any language" );
#endif

bool BFindUnlocalizedText()
{
#if defined( SOURCE2_PANORAMA )
	static bool bFind = CommandLine()->HasParm( "-find_unlocalized" );
#else
	static CCommandLineParam s_CmdLineParamFindUnlocalized( "-find_unlocalized", "" );
	static bool bFind = CommandLine()->CheckParm( s_CmdLineParamFindUnlocalized.GetHParam() ) ? true: false;
#endif
	return bFind;
}
			
// Pick a semi-arbitrary transparent color to represent "no color specified". If a user tried to do <font color="">
// for this particular color, it won't work.  However, I really doubt anyone will specify fully transparent gray instead of
// fully transparent black or white, so I think this should be safe.
const Color CLabel::TextRangeFormat_t::k_colorUnspecified( 127, 127, 127, 0 );

uint32 CLabel::s_unNextInlineImageID = 0;
uint32 CLabel::s_unNextInlinePanelID = 0;

//-----------------------------------------------------------------------------
// Purpose: Converts an ETextTransform to an EStringTransformStyle
//-----------------------------------------------------------------------------
EStringTransformStyle ETextTransformToEStringTransformStyle( ETextTransform eTextTransform )
{
	switch ( eTextTransform )
	{
	case k_ETextTransformNone:
		return k_eStringTransformStyle_None;
	case k_ETextTransformUppercase:
		return k_eStringTransformStyle_Uppercase;
	case k_ETextTransformLowercase:
		return k_eStringTransformStyle_Lowercase;
	default:
		AssertMsg( false, "Unknown ETextTransform" );
	}

	return k_eStringTransformStyle_None;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLabel::CLabel( CPanel2D *parent, const char * pchPanelID ) : CPanel2D( parent, pchPanelID )
{
	m_bMayDrawOutsideBounds = true;

	m_pCachedTextLayout = NULL;
	m_flLastTextLayoutWidth = 0.0f;
	m_flLastTextLayoutHeight = 0.0f;
	m_bContentSizeDirty = true;
	m_flMaxHeightLastContentSize = 0.0f;
	m_flMaxWidthLastContentSize = 0.0f;
	m_flLastUIScaleX = 0.0f;
	m_flLastUIScaleY = 0.0f;
	m_nMaxChars = k_nLocalizeMaxChars; // default to infinity chars
	m_eStringTruncationStyle = k_eStringTruncationStyle_None; // don't truncate by default
	m_pLastHoverRange = NULL;
	m_pMouseDownRange = NULL;
	m_pvecParsedHREFs = NULL;
	m_pvecParsedMouseOvers = NULL;
	m_pvecParsedMouseOuts = NULL;
	m_pvecParsedContextMenus = NULL;
	m_LastMousePos.x = 0.0f;
	m_LastMousePos.y = 0.0f;
	m_bLeftMouseIsDown = false;
	m_bSelectionRectDirty = false;
	m_nSelectionStartIndex = -1;
	m_nSelectionEndIndex = -1;
	m_bAllowTextSelection = true;
	m_InlineImageScalingMethod = k_EImageScalingStretchBothToFitPreserveAspectRatio;
	m_TextType = k_ETextTypePlain;
	m_flShrinkFontSize = 0.0f;
	m_flShrinkOffsetY = 0.0f;
	m_bAllowRawText = false;
	m_bHtmlStrict = false;

	if( !UIEngine()->BHaveEventHandlersRegisteredForType( CLabel::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( LocalizationChanged(), &CLabel::OnLocalizationChanged );
		RegisterEventHandlerOnPanelType( StyleFlagsChanged(), &CLabel::EventStyleFlagsChanged );
		RegisterEventHandlerOnPanelType( CopySelectedLabelText(), &CLabel::OnCopySelectedLabelText );
		RegisterEventHandlerOnPanelType( FindLongestStringForLocVariable(), &CLabel::OnFindLongestStringForLocVariable );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLabel::~CLabel()
{
	if( m_pCachedTextLayout )
	{
		UIEngine()->FreeTextLayout( m_pCachedTextLayout );
		m_pCachedTextLayout = NULL;
	}

	RemoveTextRangeFormats();
}

//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CLabel::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "text", PANORAMA_DELEGATE( &CLabel::PchGetText ), PANORAMA_DELEGATE( &CLabel::SetTextFromJS ) );
	RegisterJSAccessor( "html", PANORAMA_DELEGATE( &CLabel::BParseAsHTML ), PANORAMA_DELEGATE( &CLabel::SetParseAsHTML ) );
	RegisterJSMethod( "SetLocalizationString", PANORAMA_DELEGATE( &CLabel::SetLocalizationString ) );
	RegisterJSMethod( "SetProceduralTextThatIPromiseIsLocalizedAndEscaped", PANORAMA_DELEGATE( &CLabel::SetProceduralTextThatIPromiseIsLocalizedAndEscaped ) );
}


//-----------------------------------------------------------------------------
// Purpose: Applies properties set from layout file
//-----------------------------------------------------------------------------
bool CLabel::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	static CPanoramaSymbol k_symText( "text" );
	static CPanoramaSymbol k_symHTML( "html" );
	static CPanoramaSymbol k_symHTMLStrict( "htmlstrict" );
	static CPanoramaSymbol k_symAllowTextSelection( "allowtextselection" );
	static CPanoramaSymbol k_symScaling( "imgscaling" );
	static CPanoramaSymbol k_symAllowRawText( "allowrawtext" );

	// do not set text until we have parsed all parameters.. need to see if "html" is present.
	bool bSuccess = true;
	const char *pchText = NULL;
	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( prop.m_symName == k_symText )
		{
			pchText = vecProperties[i].m_pchValue;
		}
		else if ( prop.m_symName == k_symHTML )
		{
			bool bParseAsHTML;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bParseAsHTML ) )
			{
				bSuccess = false;
			} 
			else 
			{
				if ( bParseAsHTML )
				{
					m_TextType = k_ETextTypeHTML;
				}
			}
		}
		else if ( prop.m_symName == k_symHTMLStrict )
		{
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &m_bHtmlStrict ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symAllowTextSelection )
		{
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &m_bAllowTextSelection ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symScaling )
		{
			m_InlineImageScalingMethod = EImageScalingFromName( prop.m_pchValue );
		}
		else if ( prop.m_symName == k_symAllowRawText )
		{
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &m_bAllowRawText ) )
				bSuccess = false;
		}
		else
		{
			if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
				bSuccess = false;
		}
	}

	if ( pchText )
	{
		// text attribute supports new lines, tabs, etc. Unescape.
		char *pchConvertedValue = NULL;
		int nStrLen = V_strlen( pchText );
		if ( pchText && nStrLen )
		{
			int nOffset = 0;
			int nOutput = 0;

			CUtlCharConversion *pConv = GetCStringCharConversion();
			int nMaxConvLength = pConv->MaxConversionLength();

			while ( nOffset <= nStrLen )
			{
				char c = pchText[nOffset];
				if ( c == pConv->GetEscapeChar() )
				{
					// Allocate conversion string since we found an escape char, copy up to that point
					if ( !pchConvertedValue )
					{
						pchConvertedValue = new char[nStrLen + 1];
						V_strncpy( pchConvertedValue, pchText, nOffset + 1 );
						nOutput += nOffset;
					}

					if ( nStrLen - nOffset < nMaxConvLength + 1 )
					{
						pchConvertedValue[nOutput++] = pchText[nOffset];
						++nOffset;
					}
					else
					{
						int nLength;
						pchConvertedValue[nOutput++] = pConv->FindConversion( (const char*)pchText + nOffset + 1, &nLength );
						nOffset += nLength + 1;
					}
				}
				else
				{
					if ( pchConvertedValue )
						pchConvertedValue[nOutput++] = pchText[nOffset];
					++nOffset;
				}

			}
		}

		SetTextInternal( pchConvertedValue ? pchConvertedValue : pchText, m_TextType, true );
		if ( BFindUnlocalizedText() && pchText[0] && pchText[0] != '#' )
		{
			AddClass( "ProbablyUnlocalizedText" );
		}

		if ( pchConvertedValue )
			delete [] pchConvertedValue;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Add a text range format to end of our list
//-----------------------------------------------------------------------------
CLabel::TextRangeFormat_t &CLabel::AppendTextRangeFormat( int iStartChar, int iEndChar, uint unHTMLFormatFlags, const CUtlVector< CPanoramaSymbol > &vecClasses, int iHREF, int iMouseOver, int iMouseOut, int iContextMenu, const Color &color, const char *pszChildID, bool bChildOwner )
{
	int iNext = m_vecTextRangeFormats.AddToTail();
	TextRangeFormat_t &format = m_vecTextRangeFormats[ iNext ];
	format.m_iStartChar = iStartChar;
	format.m_iEndChar = iEndChar;
	format.m_unHTMLFormatFlags = unHTMLFormatFlags;
	format.m_iHREF = iHREF;
	format.m_iMouseOver = iMouseOver;
	format.m_iMouseOut = iMouseOut;
	format.m_iContextMenu = iContextMenu;
	format.m_vecClasses.AddVectorToTail( vecClasses );
	format.m_color = color;
	format.m_strChildID = pszChildID;
	format.m_bChildOwner = bChildOwner;

	// consistency checking
	Assert( format.m_iEndChar >= format.m_iStartChar || format.m_iEndChar == -1 );
	int ichPreviousRangeEnd = -1;
	FOR_EACH_VEC( m_vecTextRangeFormats, i )
	{
		Assert( ichPreviousRangeEnd < m_vecTextRangeFormats[ i ].m_iStartChar );
		ichPreviousRangeEnd = m_vecTextRangeFormats[ i ].m_iEndChar;
	}

	return format;
}


//-----------------------------------------------------------------------------
// Purpose: Deletes all text range formats
//-----------------------------------------------------------------------------
void CLabel::RemoveTextRangeFormats()
{
	FOR_EACH_VEC( m_vecTextRangeFormats, i )
	{
		TextRangeFormat_t &rangeFormat = m_vecTextRangeFormats[ i ];

		if ( rangeFormat.m_pStyle )
			UIEngine()->FreePanelStyle( rangeFormat.m_pStyle );

		if ( rangeFormat.m_bChildOwner )
			delete FindChild( rangeFormat.m_strChildID.Get() );
	}

	m_vecTextRangeFormats.RemoveAll();
	SAFE_DELETE( m_pvecParsedHREFs );
	SAFE_DELETE( m_pvecParsedMouseOvers );
	SAFE_DELETE( m_pvecParsedMouseOuts );
	SAFE_DELETE( m_pvecParsedContextMenus );
}


//-----------------------------------------------------------------------------
// Purpose: our loc variable or styles changed, recaculate what the longest string for this label is now
//-----------------------------------------------------------------------------
bool CLabel::OnFindLongestStringForLocVariable( const CPanelPtr< IUIPanel > &pPanel )
{				
	if ( pPanel.Get() == UIPanel() )
	{
		// If our styles are dirty there'll be an OnStylesChanged call reentered from
		// the loc system's call to ResolveStringLengthInPixels.  OnStylesChanged
		// updates m_pLocText while SetLongestStringForToken is still doing its work
		// and so often will cause crashes.  Make sure our styles are clean before
		// we enter SetLongestStringForToken.
		ApplyStyles( false );

		// find the longest string for all languages that would render for how we have this label setup right now
		UIEngine()->UILocalize()->SetLongestStringForToken( m_pLocText.Get(), this );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set the internal string
//-----------------------------------------------------------------------------
void CLabel::SetText( const char *pchValue, ETextType eTextType )
{
	SetTextInternal( pchValue, eTextType, false );
}


//-----------------------------------------------------------------------------
// Purpose: Set the internal string and interpret dialog variables as appropriate.
//          Allows dynamically generating a string with dialog variables in it.
//-----------------------------------------------------------------------------
void CLabel::SetTextWithDialogVariables( const char *pchValue, ETextType eTextType )
{
	SetTextInternal( pchValue, eTextType, true );
}


//-----------------------------------------------------------------------------
// Purpose: Called when JS assigns label.text = value
//-----------------------------------------------------------------------------
void CLabel::SetTextFromJS( const char* pchValue )
{
	if ( m_TextType == k_ETextTypeHTML )
		SetText( pchValue, k_ETextTypeHTML );
	else
		SetText( pchValue, k_ETextTypeUnlocalized );
}

//-----------------------------------------------------------------------------
// Purpose: Assign a localization string to a label
//-----------------------------------------------------------------------------
void CLabel::SetLocalizationString( const char* pchValue )
{
	if ( !pchValue || !*pchValue )
	{
		// allowed to set empty string
		SetText( "" );
		return;
	}

	if ( *pchValue != '#' )
	{
		// assigning non-localization tag
		DevAssertMsg( false, CFmtStr1024( "Label '%s': Assigning non-localization string \"%s\" via SetLocalizationString",
			BHasID() ? GetID() : "", pchValue ) );
		return;
	}

	if ( m_TextType == k_ETextTypeHTML )
		SetText( pchValue, k_ETextTypeHTML );
	else
		SetText( pchValue, k_ETextTypePlain );
}

//-----------------------------------------------------------------------------
// Purpose: Allow JS to assign raw text to a label when appropriate
//-----------------------------------------------------------------------------
void CLabel::SetProceduralTextThatIPromiseIsLocalizedAndEscaped( const char* pchValue, bool bAllowDialogVariables )
{
	bool bAllowRawText = m_bAllowRawText;
	SetAllowRawText( true );

	if ( m_TextType == k_ETextTypeHTML )
		SetTextInternal( pchValue, k_ETextTypeHTML, bAllowDialogVariables );
	else
		SetTextInternal( pchValue, k_ETextTypeUnlocalized, bAllowDialogVariables );

	SetAllowRawText( bAllowRawText );
}


//-----------------------------------------------------------------------------
// Purpose: Set the internal string
//-----------------------------------------------------------------------------
void CLabel::SetTextInternal( const char *pchValue, ETextType eTextType, bool bTrustedSource )
{
	if ( BFindUnlocalizedText() )
	{
		RemoveClass( "ProbablyUnlocalizedText" );
	}

	m_nSelectionStartIndex = -1;
	m_nSelectionEndIndex = -1;
	m_bSelectionRectDirty = true;

	if ( eTextType != k_ETextTypeNone )
	{
		m_TextType = eTextType;
	}

	RemoveTextRangeFormats();
	m_pLocText.Clear();
	m_pLocTextHTML.Clear();

	// Since this occurs at label creation time styles are often dirty, it's cheaper to be wrong on the transform here and re-apply it
	// when we get OnStylesChanged than to actually get the real transform value now.  That's especially true since we almost never
	// use the text-transform style.  I'd like to just kill it.
	ETextTransform eTransform = k_ETextTransformNone;
	if ( !BStylesDirty() )
		AccessStyle()->GetTextTransform( eTransform );
	else
		SetOnStylesChangedNeeded();

	if ( m_TextType == k_ETextTypeHTML )
	{
		// HACK: We don't have a way to distinguish loc-tags from set unlocalized/already localized HTML.  We want to make sure we don't
		// accidentally translate input that isn't actually a loc token, so we rely on loc tokens set from JS/code always starting with "#",
		// and otherwise treat the input as plain text.  This means that if we get a pre-localized text field that randomly starts with
		// '#' we might end up translating it -- if this is somehow exposed to users, they almost always abuse it.
		//
		// This is a band-aid for places like ShowTextTooltip() where we have AllowRawText set to true because it's called from too many
		// places with too many input styles at the moment.
		//
		// Would be better for the caller to tell us explicitly whether the data has been pre-localized or not.
		if ( pchValue && *pchValue == '#' )
			m_pLocTextHTML = UILocalize()->PchFindToken( UIPanel(), pchValue, m_nMaxChars, m_eStringTruncationStyle, ETextTransformToEStringTransformStyle( eTransform ), k_eStringEscapeStyle_HTML, bTrustedSource );
		else
			m_pLocTextHTML = UILocalize()->PchSetString( UIPanel(), pchValue, m_nMaxChars, m_eStringTruncationStyle, ETextTransformToEStringTransformStyle( eTransform ), k_eStringEscapeStyle_HTML, bTrustedSource, true );

		SetFromHTMLInternal( m_pLocTextHTML->String(), false, bTrustedSource );
	}
	else
	{
		if ( m_TextType == k_ETextTypeUnlocalized )
			m_pLocText = UILocalize()->PchSetString( UIPanel(), pchValue, m_nMaxChars, m_eStringTruncationStyle, ETextTransformToEStringTransformStyle( eTransform ), k_eStringEscapeStyle_None, bTrustedSource, true );
		else // m_TextType == k_ETextTypePlain
		{
			m_pLocText = UILocalize()->PchFindToken( UIPanel(), pchValue, m_nMaxChars, m_eStringTruncationStyle, ETextTransformToEStringTransformStyle( eTransform ), k_eStringEscapeStyle_None, bTrustedSource );
#if defined( SOURCE2_PANORAMA )
			if( CommandLine()->HasParm( "-all_languages" ) )
#else
			if( CommandLine()->CheckParm( sAllLanguages.GetHParam() ) )
#endif
			{
				// check what string we should display
				DispatchEventAsync( FindLongestStringForLocVariable(), this );
			}
		}
	}
	
	m_bContentSizeDirty = true;
	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: appends to the internal string
//-----------------------------------------------------------------------------
void CLabel::AppendText( const char *pchValue, ETextType eTextType )
{
	if ( !m_pLocText )
		SetText( pchValue, eTextType );
	else
	{
		// Use html if the caller said nothing about the text type, and we're html.  Or if the caller said it's html.
		bool bUseHTML = ( eTextType == k_ETextTypeNone && m_TextType == k_ETextTypeHTML ) || eTextType == k_ETextTypeHTML;

		if ( !bUseHTML )
			m_pLocText.GetMutable()->AppendText( pchValue );
		else
		{
			m_pLocTextHTML.GetMutable()->AppendText( pchValue );

			// We don't call SetFromHTMLInternal() here because OnLocalizationChanged will be fired by the localization system
			// and will pass all of m_pLocTextHTML's dialog variable replaced text into SetFromHTMLInternal()
		}
	}
	m_bContentSizeDirty = true;
	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Called if styles affecting us change
//-----------------------------------------------------------------------------
void CLabel::OnStylesChanged()
{
	m_bContentSizeDirty = true;
	BaseClass::OnStylesChanged();

	/*
	// $$$REI not sure we have to do this every time, right now we aren't fixing this often enough!!
	if ( m_pCachedTextLayout )
	{
		UIEngine()->FreeTextLayout( m_pCachedTextLayout );
		m_pCachedTextLayout = NULL;
	}
	*/

	ETextTransform eTransform;
	AccessStyle()->GetTextTransform( eTransform );

	// pass along text transform changes if necessary. Will only rebuild if style changed
	if ( m_pLocText )
	{
		EStringTransformStyle eTranformStyle = ETextTransformToEStringTransformStyle( eTransform );
		if ( eTranformStyle != m_pLocText->GetTransformStyle() )
		{
			m_pLocText = UILocalize()->ChangeTransformStyleAndRelease( m_pLocText.Extract(), eTranformStyle );
			m_bContentSizeDirty = true;
		}
	}


	if ( m_pLocTextHTML )
	{
		EStringTransformStyle eTranformStyle = ETextTransformToEStringTransformStyle( eTransform );
		if ( eTranformStyle != m_pLocTextHTML->GetTransformStyle() )
		{
			m_pLocTextHTML = UILocalize()->ChangeTransformStyleAndRelease( m_pLocTextHTML.Extract(), eTranformStyle );
			m_bContentSizeDirty = true;
		}
	}

	bool bAllLanguages = false;
#if defined( SOURCE2_PANORAMA )
	if( CommandLine()->HasParm( "-all_languages" ) )
		bAllLanguages = true;
#else
	if( CommandLine()->CheckParm( sAllLanguages.GetHParam() ) )
		bAllLanguages = true;
#endif

	if ( m_pLocText && bAllLanguages )
	{
		// check what string we should display
		DispatchEventAsync( FindLongestStringForLocVariable(), this );
	}


	UpdateTextRangeStyles();
}


//-----------------------------------------------------------------------------
// Purpose: Helper to build a list of styles for html formats
//-----------------------------------------------------------------------------
void AppendMatchingStylesForRange( CUtlVector< CascadeStyleFileInfo_t > *pvecRangeStyles, CPanel2D *pParent, IUILayoutFile *pLayoutFile, CPanoramaSymbol symPanelType, uint unStyleFlags, const CUtlVector< CPanoramaSymbol > &vecSymStyleClasses )
{
	CPanelIdentifiers panelID;
	panelID.m_symPanelType = symPanelType;
	panelID.m_pPanel = pParent->UIPanel();
	panelID.m_bTreatPanelAsParent = true;
	panelID.m_unStyleFlags = unStyleFlags;

	if ( !vecSymStyleClasses.IsEmpty() )
	{
		panelID.m_psymClasses = &vecSymStyleClasses[0];
		panelID.m_csymClasses = vecSymStyleClasses.Count();
	}
	
	CUtlVector< CascadeStyleFileInfo_t > vecStyles;
	pLayoutFile->BuildMatchingStyleList( vecStyles, panelID, NULL );

	pvecRangeStyles->AddMultipleToTail( vecStyles.Count(), vecStyles.Base() );
}


//-----------------------------------------------------------------------------
// Purpose: Updates the styles for text ranges (default set on label panel by style system)
//-----------------------------------------------------------------------------
void CLabel::UpdateTextRangeStyles()
{
	// need to get style from our layout file
	IUILayoutFile *pLayoutFile = UIEngine()->UILayoutManager()->GetLayoutFile( GetLayoutFile() );
	if ( !pLayoutFile )
	{
		AssertMsg( false, "Couldn't find layout file" );
		return;
	}

	CUtlVector< CascadeStyleFileInfo_t > vecRangeStyles;
	EStyleRepaint eRepaint;
	FOR_EACH_VEC( m_vecTextRangeFormats, iFormats )
	{
		TextRangeFormat_t &rangeFormat = m_vecTextRangeFormats[iFormats];
		if ( !rangeFormat.m_pStyle )
			rangeFormat.m_pStyle = UIEngine()->AllocPanelStyle( NULL );

		// Update UI Scale factor if necessary
		{
			Vector vUIScale = GetActualUIScale();
			Vector vPreviousUIScale = rangeFormat.m_pStyle->GetUIScale();
			if ( vUIScale != vPreviousUIScale ) 
			{
				rangeFormat.m_pStyle->SetUIScale( vUIScale );
				
				// Normally this is handled automatically during panel layout, but since there is no panel associated with this style we must call it manually
				rangeFormat.m_pStyle->UpdateUIScaleFactor( vPreviousUIScale, vUIScale, vPreviousUIScale, vUIScale );
			}
		}

		vecRangeStyles.RemoveAll();

		if ( !rangeFormat.m_vecClasses.IsEmpty() )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, CPanel2D::GetPanelSymbol(), rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );

		if ( rangeFormat.m_unHTMLFormatFlags & k_EHTMLFormatTagAnchor )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, k_symPanela, rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );
		if ( rangeFormat.m_unHTMLFormatFlags & k_EHTMLFormatTagBold )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, k_symPanelb, rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );
		if ( rangeFormat.m_unHTMLFormatFlags & k_EHTMLFormatTagItalics )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, k_symPaneli, rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );
		if ( rangeFormat.m_unHTMLFormatFlags & k_EHTMLFormatTagEmphasized )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, k_symPanelem, rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );
		if ( rangeFormat.m_unHTMLFormatFlags & k_EHTMLFormatTagHeader1 )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, k_symPanelh1, rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );
		if ( rangeFormat.m_unHTMLFormatFlags & k_EHTMLFormatTagHeader2 )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, k_symPanelh2, rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );
		if ( rangeFormat.m_unHTMLFormatFlags & k_EHTMLFormatTagPre )
			AppendMatchingStylesForRange( &vecRangeStyles, this, pLayoutFile, k_symPanelpre, rangeFormat.m_unStyleFlags, rangeFormat.m_vecClasses );

		if ( rangeFormat.m_color != TextRangeFormat_t::k_colorUnspecified )
		{
			Color currentColor;
			if ( !rangeFormat.m_pStyle->GetSimpleForegroundColor( currentColor ) || ( currentColor != rangeFormat.m_color ) )
			{
				// Need to be called before pLayoutFile->ApplyMatchedStylesToPanelStyle, otherwise the foreground color will
				// only get applied at the next call to ApplyMatchedStylesToPanelStyle
				rangeFormat.m_pStyle->SetSimpleForegroundColor( rangeFormat.m_color );
			}
		}

		bool bInheritableStylesChanged = false;
		bool bStylesChanged = pLayoutFile->ApplyMatchedStylesToPanelStyle( rangeFormat.m_pStyle, vecRangeStyles, eRepaint, bInheritableStylesChanged );

		// done applying styles		
		if ( bStylesChanged )
		{
			// Probably need some level of repaint if styles changed
			if ( eRepaint == k_EStyleRepaintFull )
				SetRepaint( k_EPanelRepaintFull );
			else if ( eRepaint == k_EStyleRepaintComposition )
				SetRepaint( k_EPanelRepaintComposition );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the count of parsed hrefs
//-----------------------------------------------------------------------------
uint32 CLabel::GetHREFCount()
{
	return m_pvecParsedHREFs ? m_pvecParsedHREFs->Count() : 0;
}


//-----------------------------------------------------------------------------
// Purpose: Called if our loc strings change
//-----------------------------------------------------------------------------
bool CLabel::OnLocalizationChanged( const CPanelPtr< IUIPanel > &pPanel, const ILocalizationString *pString, int nStartCharIndex )
{
	Assert( pPanel.Get() == UIPanel() );
	if ( m_pLocTextHTML == pString ) // if the base html loc string changed then update our derived string
		SetFromHTMLInternal( pString->String() + nStartCharIndex, nStartCharIndex != 0, true );

	m_bContentSizeDirty = true;
	InvalidateSizeAndPosition();
	return true; // don't bubble
}


//-----------------------------------------------------------------------------
// Purpose: update any styles we have cached due to html text formatting
//-----------------------------------------------------------------------------
void CLabel::OnUIScaleFactorChanged( const Vector &vOldScaleFactor, const Vector &vNewScaleFactor )
{
	BaseClass::OnUIScaleFactorChanged( vOldScaleFactor, vNewScaleFactor );
	UpdateTextRangeStyles();
}


//-----------------------------------------------------------------------------
// Purpose: given an input string return its size for this labels current layout/styles
//-----------------------------------------------------------------------------
int CLabel::ResolveStringLengthInPixels( const char *pchString )
{
#if !defined( SOURCE2_PANORAMA )
	Assert( CommandLine()->CheckParm( sAllLanguages.GetHParam() ) );
#endif
	
	float flWidth = GetActualLayoutWidth();
	float flHeight = GetActualRenderHeight();
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );
	
	// Calculate the theoretical max width of this string ignoring potential line wrapping/external
	// UI constraints, etc.
	IUITextLayout *pLayout = CreateTextLayout( k_flMaxWidthOrHeight, flHeight - flTop - flBottom, false, pchString );

	flWidth = 0.0f;
	if ( pLayout != NULL )
	{
		// now get the size we need for this string
		pLayout->GetRequiredSize( flWidth, flHeight );
		UIEngine()->FreeTextLayout( pLayout );
	}

	return ceil( flWidth );
}


//-----------------------------------------------------------------------------
// Purpose: Create text layout object
//-----------------------------------------------------------------------------
IUITextLayout *CLabel::CreateTextLayout( float flWidth, float flHeight, bool bUseChildDesiredSize, const char *pchOptStringToUse, float flOverrideFontSize )
{
	// Compute size needed for text...
	const char *pchFontFamily = NULL;
	float flFontSize;
	EFontWeight eWeight;
	EFontStyle eStyle;
	AccessStyle()->GetFontStyle( &pchFontFamily, flFontSize, eStyle, eWeight );
	bool bWrap;
	AccessStyle()->GetWhitespaceWrap( bWrap );

	ETextOverflow eTextOverflow;
	AccessStyle()->GetTextOverflow( eTextOverflow );
	bool bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;

	ETextAlign eAlign;
	AccessStyle()->GetTextAlign( eAlign );
	int nLetterSpacing;
	AccessStyle()->GetTextLetterSpacing( nLetterSpacing );

	float flLineHeight;
	AccessStyle()->GetLineHeight( flLineHeight );

	if(eTextOverflow == k_ETextOverflowNoClip )
	{
		EOverflowValue eHorizontalOverflow, eVerticalOverflow;
		AccessStyle()->GetOverflow( eHorizontalOverflow, eVerticalOverflow );
		if ( eHorizontalOverflow == k_EOverflowNoClip || eHorizontalOverflow == k_EOverflowScroll )
			flWidth = k_flMaxWidthOrHeight;
		if ( eVerticalOverflow == k_EOverflowNoClip || eVerticalOverflow == k_EOverflowScroll )
			flHeight = k_flMaxWidthOrHeight;
	}

	if ( flOverrideFontSize > 0.0f )
	{
		flFontSize = flOverrideFontSize;
	}

	const char *pchString = pchOptStringToUse;
	if ( !pchString )
	{
#if VALIDATE_LOC_TEXT
		Assert( m_pLocText.Validate() );
#endif
		pchString = m_pLocText.Get() ? m_pLocText->String() : "";
	}
	IUITextLayout *pLayout = UIEngine()->CreateTextLayout( pchString, pchFontFamily, flFontSize, flLineHeight, eWeight, eStyle, eAlign, bWrap, bEllipsis, nLetterSpacing, flWidth, flHeight );

	// add a draw effect for each text range. Having a draw effect can affect the width of characters for the specified run
	if ( pLayout != NULL )
	{
		FOR_EACH_VEC( m_vecTextRangeFormats, i )
		{
			TextRangeFormat_t &rangeFormat = m_vecTextRangeFormats[i];
			if ( !rangeFormat.m_pStyle )
				continue;

			const char *pchRangeFont = NULL;
			float flRangeSize;
			EFontStyle eRangeStyle;
			EFontWeight eRangeWeight;
			rangeFormat.m_pStyle->GetFontStyleNoDefaults( &pchRangeFont, flRangeSize, eRangeStyle, eRangeWeight );

			ETextDecoration eRangeDecoration = k_ETextDecorationUnset;
			if ( rangeFormat.m_pStyle->BHasAnyStyleDataForProperty( CStylePropertyTextDecoration::symbol ) )
				rangeFormat.m_pStyle->GetTextDecoration( eRangeDecoration );

			bool bHasColor = rangeFormat.m_pStyle->BHasAnyStyleDataForProperty( CStylePropertyForegroundColor::symbol );

			// by not setting if default values, rendered text will fall back to the style set on the entire label
			if ( pchRangeFont )
				pLayout->SetFontName( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, pchRangeFont );

			if ( flRangeSize != k_flFloatNotSet )
				pLayout->SetFontSize( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, flRangeSize );

			if ( eRangeStyle != k_EFontStyleUnset )
				pLayout->SetFontStyle( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, eRangeStyle );

			if ( eRangeWeight != k_EFontWeightUnset )
				pLayout->SetFontWeight( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, eRangeWeight );

			if ( eRangeDecoration == k_ETextDecorationNone )
			{
				pLayout->SetUnderline( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, false );
				pLayout->SetStrikethrough( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, false );
			}
			else if ( eRangeDecoration == k_ETextDecorationUnderline )
			{
				pLayout->SetUnderline( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, true );
			}
			else if ( eRangeDecoration == k_ETextDecorationLineThrough )
			{
				pLayout->SetStrikethrough( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, true );
			}

			if ( bHasColor )
				pLayout->MarkColorRangeForMeasurement( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar, -1 );

			if ( !rangeFormat.m_strChildID.IsEmpty() )
			{
				CPanel2D *pChild = FindChild( rangeFormat.m_strChildID.Get() );
				if ( pChild )
				{
					float flChildWidth = bUseChildDesiredSize ? pChild->GetDesiredLayoutWidth() : pChild->GetActualLayoutWidth();
					float flChildHeight = bUseChildDesiredSize ? pChild->GetDesiredLayoutHeight() : pChild->GetActualLayoutHeight();

					float flMarginLeft, flMarginTop, flMarginRight, flMarginBottom;
					pChild->AccessStyle()->GetMargin( flWidth, flHeight, flMarginLeft, flMarginTop, flMarginRight, flMarginBottom );

					pLayout->SetInlineObject( rangeFormat.m_iStartChar, flMarginLeft + flChildWidth + flMarginRight, flMarginTop + flChildHeight + flMarginBottom );
				}
			}
		}
	}
	return pLayout;
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel is measured
//-----------------------------------------------------------------------------
void CLabel::OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	VPROF_BUDGET_DETAILED( "CLabel::OnContentSizeTraverse", VPROF_BUDGETGROUP_TENFOOT );

	// save off the content size in case we don't re-calc this time
	float flPrevContentWidth = GetContentWidth();
	float flPrevContentHeight = GetContentHeight();

	BaseClass::OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );	

	// Final dimensions are only different if "content inset" (padding + border-width) has transition.
	// Can use the cached values otherwise
	bool bFinalDimensionsDirty = bFinalDimensions && AccessStyle()->BHasContentInsetTransition();
	
	if ( bFinalDimensionsDirty || m_bContentSizeDirty || flMaxHeight != m_flMaxHeightLastContentSize || flMaxWidth != m_flMaxWidthLastContentSize ||
		m_flLastUIScaleX != GetActualUIScaleX() || m_flLastUIScaleY != GetActualUIScaleY() )
	{
		// Clear cached layout for selection use if we had it
		if( m_pCachedTextLayout )
		{
			UIEngine()->FreeTextLayout( m_pCachedTextLayout );
			m_pCachedTextLayout = NULL;
		}

		// if not calculating final dimensions, we are updating content size
		if ( !bFinalDimensions )
		{
			m_bContentSizeDirty = false;
			m_flMaxHeightLastContentSize = flMaxHeight;
			m_flMaxWidthLastContentSize = flMaxWidth;
			m_flLastUIScaleX = GetActualUIScaleX();
			m_flLastUIScaleY = GetActualUIScaleY();
		}

		if ( !m_pLocText )
		{
			*pflContentWidth = 0.0f;
			*pflContentHeight = 0.0f;
			return;
		}

		// include padding
		{
			float flLeft, flTop, flRight, flBottom;
			AccessStyle()->GetContentInset( flMaxWidth, flMaxHeight, bFinalDimensions, flLeft, flTop, flRight, flBottom );

			flMaxWidth = flMaxWidth - flLeft - flRight;
			flMaxHeight = flMaxHeight - flTop - flBottom;
		}

		// Compute size needed for text...
		IUITextLayout *pLayout = CreateTextLayout( flMaxWidth, flMaxHeight, true );
		if ( !pLayout )
		{
			*pflContentWidth = 0.0f;
			*pflContentHeight = 0.0f;
			return;
		}

		// all effects have been set.. we can now do measurements
		float flDesiredWidth, flDesiredHeight;
		pLayout->GetRequiredSize( flDesiredWidth, flDesiredHeight );

		// Recompute actual padding, might be smaller than we first computed if we didn't need full max region for label
		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( flDesiredWidth, flDesiredHeight, bFinalDimensions, flLeft, flTop, flRight, flBottom );

		// Ceil is used here to try to make sure we have pixel aligned bounding boxes on text, which helps when we use center
		// for vertical/horizontal align.  We'll either be pixel aligned then or off by .5 and can fix by increasing parent
		// size by 1px.  If we were arbitrary subpixel width/height it is impossible to center align well without making text
		// blurry.
		*pflContentWidth = ceil( MAX( flDesiredWidth + flLeft + flRight, *pflContentWidth ) );
		*pflContentHeight = ceil( MAX( flDesiredHeight + flTop + flBottom, *pflContentHeight ) );

		if ( !m_vecTextRangeFormats.IsEmpty() )
		{
			// Is hit test enabled?
			bool bHitTestEnabled = BHitTestEnabled();
			if( bHitTestEnabled )
			{
				// check if parent disables hit test for us
				for( CPanel2D *pParent = GetParent(); pParent != NULL; pParent = pParent->GetParent() )
				{
					if( !pParent->BHitTestChildrenEnabled() )
					{
						bHitTestEnabled = false;
						break;
					}
				}
			}

			if( bHitTestEnabled )
			{
				// Re-layout the text with the actual sizes we've got.
				pLayout->Resize( *pflContentWidth, *pflContentHeight );

				// save off hit box for each format range
				CUtlVector< IUITextLayout::HitTestRegionRect_t > vecHitRects;
				FOR_EACH_VEC( m_vecTextRangeFormats, iRange )
				{
					TextRangeFormat_t &rangeFormat = m_vecTextRangeFormats[iRange];

					vecHitRects.RemoveAll();
					pLayout->GetCharacterRangeCoordinates( rangeFormat.m_iStartChar, rangeFormat.m_iEndChar + 1, vecHitRects );

					rangeFormat.m_vecBoundingBoxes.RemoveAll();
					if( vecHitRects.Count() == 0 )
						continue;

					rangeFormat.m_vecBoundingBoxes.EnsureCapacity( vecHitRects.Count() );
					FOR_EACH_VEC( vecHitRects, iHit )
					{
						TextRangeFormat_t::RangeFormatBox_t &box = rangeFormat.m_vecBoundingBoxes[rangeFormat.m_vecBoundingBoxes.AddToTail()];
						box.topLeft = vecHitRects[iHit].topLeft;
						box.bottomRight = vecHitRects[iHit].bottomRight;

						// need to offset with padding
						box.topLeft.x += flLeft;
						box.bottomRight.x += flLeft;
						box.topLeft.y += flTop;
						box.bottomRight.y += flTop;

						//Assert( box.topLeft.x < 2000 );
						//Assert( box.topLeft.y < 2000 );
						//Assert( box.bottomRight.x < 2000 );
						//Assert( box.bottomRight.y < 2000 );
					}
				}
			}
		}
		UIEngine()->FreeTextLayout( pLayout );
	}
	else
	{
		// label bounds didn't change so use last cached value
		*pflContentWidth = flPrevContentWidth;
		*pflContentHeight = flPrevContentHeight;
	}

	AssertMsg( IsFinite( *pflContentWidth ), "Invalid content width calculated" );
	AssertMsg( IsFinite( *pflContentHeight ), "Invalid content height calculated" );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a text layout according to the current layout size of this label
//-----------------------------------------------------------------------------
IUITextLayout *CLabel::CreateCurrentLayoutTextLayout()
{
	float flWidth = GetActualLayoutWidth();
	float flHeight = GetActualRenderHeight();

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

	float flLayoutWidth = flWidth - flLeft - flRight;
	float flLayoutHeight = flHeight - flTop - flBottom;

	if( !m_pCachedTextLayout || m_flLastTextLayoutWidth != flLayoutWidth || m_flLastTextLayoutHeight != flLayoutHeight )
	{
		if( m_pCachedTextLayout )
			UIEngine()->FreeTextLayout( m_pCachedTextLayout );

		m_flLastTextLayoutWidth = flLayoutWidth;
		m_flLastTextLayoutHeight = flLayoutHeight;

		float flFontSize = 0.0f;
		ETextOverflow eTextOverflow;
		AccessStyle()->GetTextOverflow( eTextOverflow );
		if ( eTextOverflow == k_ETextOverflowShrink )
		{
			flFontSize = m_flShrinkFontSize;
		}

		m_pCachedTextLayout = CreateTextLayout( flLayoutWidth, flLayoutHeight, false, nullptr, flFontSize );
	}

	return m_pCachedTextLayout;
}


//-----------------------------------------------------------------------------
// Purpose: layout traverse, layout children
//-----------------------------------------------------------------------------
void CLabel::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	IUITextLayout *pTextLayout = NULL;

	ETextOverflow eTextOverflow;
	AccessStyle()->GetTextOverflow( eTextOverflow );

	if ( eTextOverflow == k_ETextOverflowShrink && flFinalHeight > 0.0f && flFinalWidth > 0.0f )
	{
		if ( m_flShrinkFontSize != 0.0f )
		{
			m_flShrinkFontSize = 0.0f;
			m_flShrinkOffsetY = 0.0f;
			if ( m_pCachedTextLayout )
			{
				UIEngine()->FreeTextLayout( m_pCachedTextLayout );
				m_pCachedTextLayout = nullptr;
			}
		}
		pTextLayout = CreateCurrentLayoutTextLayout();

		if ( pTextLayout )
		{
			float flInsetLeft, flInsetTop, flInsetRight, flInsetBottom;
			AccessStyle()->GetContentInset( flFinalWidth, flFinalHeight, false, flInsetLeft, flInsetTop, flInsetRight, flInsetBottom );
			float flAllowedWidth = flFinalWidth - flInsetLeft - flInsetRight;
			float flAllowedHeight = flFinalHeight - flInsetTop - flInsetBottom;

			float flRequiredWidth = 0.0f;
			float flRequiredHeight = 0.0f;
			pTextLayout->GetRequiredSize( flRequiredWidth, flRequiredHeight );

			if ( flRequiredWidth > flAllowedWidth || flRequiredHeight > flAllowedHeight )
			{
				const char *pchFontFamily = nullptr;
				float flStartingFontSize = 0.0f;
				EFontStyle eFontStyle;
				EFontWeight eFontWeight;
				AccessStyle()->GetFontStyle( &pchFontFamily, flStartingFontSize, eFontStyle, eFontWeight );

#ifdef DEBUG
				int nIteration = 1;
#endif
				// Msg( "CLabel::OnLayoutTraverse: Shrinking font size for (%s). Starting at %.2f\n", GetID(), flStartingFontSize );

				const float k_flMinimumShrinkFontSize = 8.0f; // Don't ever go below 8pt font

				// If the font is already within 4px on the width, or 2px on the height, then don't continue searching
				const float k_flWidthErrorMargin = 4.0f;
				const float k_flHeightErrorMargin = 2.0f;

				const char *pchString = m_pLocText ? m_pLocText->String() : "";
				int nEndIndex = V_UnicodeLength( pchString );

				// Loop to find a good estimate of the desired font size
				float flFontSize = flStartingFontSize;
				bool bMaybeUnderestimate = false;
				IUITextLayout *pShrinkLayout = pTextLayout;

				if ( !V_isempty( pchString ) )
				{
					while ( ( flRequiredWidth > flAllowedWidth || flRequiredHeight > flAllowedHeight ) && flFontSize > k_flMinimumShrinkFontSize )
					{
						CUtlVector< IUITextLayout::HitTestRegionRect_t > vecRects;
						pShrinkLayout->GetCharacterRangeCoordinates( 0, nEndIndex, vecRects );

						// First, calculate the width/height of all of the characters if you put them up in a single line
						float flSingleLineWidth = 0.0f;
						float flSingleLineHeight = 0.0f;
						for ( const IUITextLayout::HitTestRegionRect_t &rect : vecRects )
						{
							flSingleLineWidth += rect.bottomRight.x - rect.topLeft.x;
							flSingleLineHeight = Max( flSingleLineHeight, rect.bottomRight.y - rect.topLeft.y );
						}

						float flNextFontSize = 0.0f;

						// Assuming we divide the characters evenly across multiple lines, we want to calculate
						// the number of lines whose aspect ratio most closely matches the aspect ratio of
						// the allowed area.
						float flAllowedAspectRatio = flAllowedWidth / flAllowedHeight;

						// Aspect ratio = ( width / line count ) / ( height * line count )
						// r = ( w / l ) / ( h * l )
						// r = w / ( h * l^2 )
						// h * l^2 = w / r
						// l = sqrt( w / ( r * h ) )
						float flLineCount = sqrt( flSingleLineWidth / ( flAllowedAspectRatio * flSingleLineHeight ) );

						// Round to the nearest line count
						flLineCount = roundf( flLineCount );

						// Must have at least one line
						flLineCount = Max( flLineCount, 1.0f );

						// Estimate the line height based on the allowed height and the line count.
						float flLineHeight = flAllowedHeight / flLineCount;

						// If we're only a single line, then our text might be too wide to fit on that line
						if ( flLineCount == 1.0f )
						{
							float flLineWidth = flSingleLineWidth * flLineHeight / flSingleLineHeight;
							if ( flLineWidth > flAllowedWidth )
							{
								flLineHeight *= flAllowedWidth / flLineWidth;
							}
						}

						// Convert the line height into a font size. 0.8 is a magic number that comes from
						// CUITextLayoutPango::BInitialize describing how much spacing there is between lines.
						flNextFontSize = flLineHeight * 0.8f;

						if ( flNextFontSize < k_flMinimumShrinkFontSize )
						{
							//Msg( "   Iteration %d: Estimated %.2f, but that's below the minimum of %.2f. Using %.2f\n", nIteration, flNextFontSize, k_flMinimumShrinkFontSize, k_flMinimumShrinkFontSize );
							flNextFontSize = k_flMinimumShrinkFontSize;
							bMaybeUnderestimate = true;
						}
						else if ( flNextFontSize >= flFontSize )
						{
							//Msg( "   Iteration %d: Estimated %.2f, but made no progress. Using %.2f instead\n", nIteration, flNextFontSize, flFontSize - 1 );
							flNextFontSize = Max( flFontSize - 1, k_flMinimumShrinkFontSize );
							bMaybeUnderestimate = false;
						}
						else
						{
							//Msg( "   Iteration %d: Estimated %.2f\n", nIteration, flNextFontSize );
							bMaybeUnderestimate = true;
						}

						flFontSize = floorf( flNextFontSize );

						if ( pShrinkLayout != pTextLayout )
						{
							UIEngine()->FreeTextLayout( pShrinkLayout );
							pShrinkLayout = nullptr;
						}

						pShrinkLayout = CreateTextLayout( flAllowedWidth, flAllowedHeight, false, nullptr, flFontSize );
						pShrinkLayout->GetRequiredSize( flRequiredWidth, flRequiredHeight );

#ifdef DEBUG
						++nIteration;
#endif
					}
				}

				if ( pShrinkLayout != pTextLayout )
				{
					UIEngine()->FreeTextLayout( pShrinkLayout );
					pShrinkLayout = nullptr;
				}

				// It's possible that our estimate was an underestimate, so loop up until we find the right size.
				if ( bMaybeUnderestimate )
				{
					float flIncreaseRequiredWidth = flRequiredWidth;
					float flIncreaseRequiredHeight = flRequiredHeight;

					while ( flFontSize < flStartingFontSize &&
						( ( flAllowedWidth - flIncreaseRequiredWidth ) > k_flWidthErrorMargin || ( flAllowedHeight - flIncreaseRequiredHeight ) > k_flHeightErrorMargin ) )
					{
						float flNextFontSize = flFontSize + 1.0f;

						pShrinkLayout = CreateTextLayout( flAllowedWidth, flAllowedHeight, false, nullptr, flNextFontSize );
						pShrinkLayout->GetRequiredSize( flIncreaseRequiredWidth, flIncreaseRequiredHeight );
						UIEngine()->FreeTextLayout( pShrinkLayout );
						pShrinkLayout = nullptr;

#ifdef DEBUG
						++nIteration;
#endif

						if ( flIncreaseRequiredWidth > flAllowedWidth || flIncreaseRequiredHeight > flAllowedHeight )
						{
							//Msg( "   Iteration %d: Tried to increase from %.2f to %.2f, but didn't need to.\n", nIteration - 1, flFontSize, flNextFontSize );
							break;
						}

						//Msg( "   Iteration %d: Increased from %.2f to %.2f\n", nIteration - 1, flFontSize, flNextFontSize );
						flRequiredWidth = flIncreaseRequiredWidth;
						flRequiredHeight = flIncreaseRequiredHeight;
						flFontSize = flNextFontSize;
					}
				}

				//Msg( "CLabel::OnLayoutTraverse: Font shrinking complete. %.2f to %.2f in %d iterations\n", flStartingFontSize, flFontSize, nIteration - 1 );

#ifdef DEBUG
				const int k_nMaxIterations = 5;
				AssertMsg( nIteration - 1 <= k_nMaxIterations, CFmtStr( "Font shrinking took %d iterations for panel %s. This is expensive and more than the expected max of %d.", nIteration, GetID(), k_nMaxIterations ).Get() );
#endif

				m_flShrinkFontSize = flFontSize;
				m_flShrinkOffsetY = floorf( ( flAllowedHeight - flRequiredHeight ) / 2.0f );

				if ( m_flShrinkFontSize != flStartingFontSize && m_pCachedTextLayout )
				{
					if ( pTextLayout == m_pCachedTextLayout )
					{
						pTextLayout = nullptr;
					}

					UIEngine()->FreeTextLayout( m_pCachedTextLayout );
					m_pCachedTextLayout = nullptr;
				}
			}
		}
		else
		{
			m_flShrinkFontSize = 0.0f;
			m_flShrinkOffsetY = 0.0f;
		}
	}

	FOR_EACH_VEC( m_vecTextRangeFormats, i )
	{
		if ( m_vecTextRangeFormats[ i ].m_strChildID.IsEmpty() )
			continue;

		CPanel2D *pChild = FindChild( m_vecTextRangeFormats[ i ].m_strChildID.Get() );
		if ( !pChild )
			continue;

#ifdef DEBUG
		EFlowDirection eFlowDirection;
		AccessStyle()->GetFlowChildren( eFlowDirection );
		AssertMsg( eFlowDirection == k_EFlowNone, "Can't use inline object children on a label that has flow-children set" );
#endif

		if ( !pTextLayout )
			pTextLayout = CreateCurrentLayoutTextLayout();

		if ( pTextLayout )
		{
			IUITextLayout::HitTestRegionRect_t rect;
			pTextLayout->GetCharacterCoordinates( m_vecTextRangeFormats[ i ].m_iStartChar, rect );

			CUILength lenX, lenY, lenZ;
			pChild->GetPosition( lenX, lenY, lenZ );

			lenX.SetLength( rect.topLeft.x );
			lenY.SetLength( rect.topLeft.y );

			lenX.ScaleLengthValue( 1.0f / pChild->GetActualUIScaleX() );
			lenY.ScaleLengthValue( 1.0f / pChild->GetActualUIScaleY() );

			pChild->SetPosition( lenX, lenY, lenZ );
		}
	}

	if ( GetActualLayoutHeight() < GetContentHeight() || GetActualLayoutWidth() < GetContentWidth() )
		m_bMayDrawOutsideBounds = true;
	else
		m_bMayDrawOutsideBounds = false;
}


//-----------------------------------------------------------------------------
// Purpose: Paint label contents
//-----------------------------------------------------------------------------
void CLabel::Paint()
{
	VPROF_BUDGET_DETAILED( "panorama::CLabel::Paint", VPROF_BUDGETGROUP_TENFOOT );

	BaseClass::Paint();

	if( m_bSelectionRectDirty )
	{
		m_bSelectionRectDirty = false;
		m_vecSelectionRects.RemoveAll();
		if( m_nSelectionStartIndex != -1 && m_nSelectionEndIndex != -1 )
		{
			IUITextLayout *pTextLayout = CreateCurrentLayoutTextLayout();

			if ( pTextLayout )
			{
				pTextLayout->GetCharacterRangeCoordinates( m_nSelectionStartIndex, m_nSelectionEndIndex, m_vecSelectionRects );

				float flLeftFinal, flTopFinal, flRightFinal, flBottomFinal;
				AccessStyle()->GetContentInset( GetActualLayoutWidth(), GetActualLayoutHeight(), false, flLeftFinal, flTopFinal, flRightFinal, flBottomFinal );

				// Add in padding offsets
				FOR_EACH_VEC( m_vecSelectionRects, i )
				{
					m_vecSelectionRects[i].topLeft.x += flLeftFinal;
					m_vecSelectionRects[i].topLeft.y += flTopFinal;
					m_vecSelectionRects[i].bottomRight.x += flLeftFinal;
					m_vecSelectionRects[i].bottomRight.y += flTopFinal;
				}
			}
		}
	}

	if( m_pCachedTextLayout && !m_bLeftMouseIsDown )
	{
		UIEngine()->FreeTextLayout( m_pCachedTextLayout );
		m_pCachedTextLayout = NULL;
	}

#if VALIDATE_LOC_TEXT
	Assert( m_pLocText.Validate() );
#endif

	if ( m_pLocText && !m_pLocText->IsEmpty() )
	{
		float flLeft, flTop, flRight, flBottom;
		const char * pchFontFamily = NULL;
		float flFontSize;
		EFontWeight eWeight;
		EFontStyle eStyle;
		ETextAlign eAlign;
		ETextDecoration eDecoration;
		float flLineHeight;
		ETextOverflow eTextOverflow;
		bool bWrap, bEllipsis;
		int nLetterSpacing;
		float flDrawWidth;
		float flDrawLineHeight;

		IUIPanelStyle *pStyle = AccessStyle();
		pStyle->GetContentInset( GetActualLayoutWidth(), GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );
		pStyle->GetFontStyle( &pchFontFamily, flFontSize, eStyle, eWeight );		
		pStyle->GetTextAlign( eAlign );		
		pStyle->GetTextDecoration( eDecoration );		
		pStyle->GetLineHeight( flLineHeight );		
		pStyle->GetWhitespaceWrap( bWrap );
		pStyle->GetTextOverflow( eTextOverflow );
		pStyle->GetTextLetterSpacing( nLetterSpacing );

		bEllipsis = eTextOverflow == k_ETextOverflowEllipsis;
		float flOffsetY = 0.0f;
		if ( eTextOverflow == k_ETextOverflowShrink && m_flShrinkFontSize > 0.0f )
		{
			flFontSize = m_flShrinkFontSize;
			flOffsetY = m_flShrinkOffsetY;
		}

		// when dawing a gradient, we want the gradient to repeat each line vertically, and cover the entire text body horizontally
		flDrawWidth = GetActualRenderWidth() - flRight - flLeft;
		flDrawLineHeight = flFontSize;

		DrawTextRegionRenderCommand_t *pCommand = 
			AccessRenderEngine()->DrawTextRegion( m_pLocText->String(), pchFontFamily, flFontSize, flLineHeight, eWeight, eStyle, eAlign, eDecoration, bWrap, bEllipsis,
			nLetterSpacing, flLeft, flTop + flOffsetY, GetActualRenderWidth() - flRight, GetActualRenderHeight() - flBottom );

		if ( pCommand )
		{
			CRenderCommandList &commandList = AccessRenderEngine()->GetCommandList();

			AccessStyle()->GetForegroundFillBrushCollectionData( pCommand->default_format.fill_brush_collection, commandList, flDrawWidth, flDrawLineHeight );

			// add text ranges
			VPROF_BUDGET_DETAILED( "panorama::CLabel::Paint::m_vecTextRangeFormats", VPROF_BUDGETGROUP_TENFOOT );

			if ( !m_vecTextRangeFormats.IsEmpty() )
			{
				CRenderDataListBuilder< TextRangeFormatData_t > rangeFormatsBuilder( pCommand->range_formats, &commandList );

				int iStartChar = 0;
				int iEndChar = 0;
				FOR_EACH_VEC( m_vecTextRangeFormats, i )
				{
					TextRangeFormat_t &rangeFormat = m_vecTextRangeFormats[ i ];

					Assert( rangeFormat.m_iEndChar >= iStartChar );
					Assert( rangeFormat.m_iEndChar >= iEndChar );
					Assert( rangeFormat.m_iStartChar >= iStartChar );
					Assert( rangeFormat.m_iStartChar >= iEndChar );

					iStartChar = rangeFormat.m_iStartChar;
					iEndChar = rangeFormat.m_iEndChar;

					if ( !rangeFormat.m_pStyle )
						continue;

					const char *pchRangeFont = NULL;
					float flRangeSize;
					EFontStyle eRangeStyle;
					EFontWeight eRangeWeight;
					rangeFormat.m_pStyle->GetFontStyleNoDefaults( &pchRangeFont, flRangeSize, eRangeStyle, eRangeWeight );

					TextRangeFormatData_t *pRangeFormat = rangeFormatsBuilder.AddToTail();

					pRangeFormat->start_index = rangeFormat.m_iStartChar;
					pRangeFormat->end_index = rangeFormat.m_iEndChar;

					if ( rangeFormat.m_pStyle->BHasAnyStyleDataForProperty( CStylePropertyForegroundColor::symbol ) )
					{
						rangeFormat.m_pStyle->GetForegroundFillBrushCollectionData( pRangeFormat->format.fill_brush_collection, commandList, flDrawWidth, flDrawLineHeight );
					}

					// by not setting if default values, rendered text will fall back to the style set on the entire label
					if ( pchRangeFont )
						pRangeFormat->format.font_name = commandList.CopyString( pchRangeFont );

					if ( flRangeSize != k_flFloatNotSet )
						pRangeFormat->format.font_size = flRangeSize;

					if ( eRangeStyle != k_EFontStyleUnset )
						pRangeFormat->format.font_style = eRangeStyle;

					if ( eRangeWeight != k_EFontWeightUnset )
						pRangeFormat->format.font_weight = eRangeWeight;

					if ( rangeFormat.m_pStyle->BHasAnyStyleDataForProperty( CStylePropertyTextDecoration::symbol ) )
					{
						rangeFormat.m_pStyle->GetTextDecoration( pRangeFormat->format.text_decoration );
					}

					if ( !rangeFormat.m_strChildID.IsEmpty() )
					{
						CPanel2D *pChild = FindChild( rangeFormat.m_strChildID.Get() );
						if ( pChild )
						{
							float flMarginLeft, flMarginTop, flMarginRight, flMarginBottom;
							pChild->AccessStyle()->GetMargin( GetActualLayoutWidth(), GetActualLayoutHeight(), flMarginLeft, flMarginTop, flMarginRight, flMarginBottom );

							pRangeFormat->format.inline_object = commandList.AllocType< TextInlineObject_t >();
							pRangeFormat->format.inline_object->width = flMarginLeft + pChild->GetActualLayoutWidth() + flMarginRight;
							pRangeFormat->format.inline_object->height = flMarginTop + pChild->GetActualLayoutHeight() + flMarginBottom;
						}
					}
				}
			}
			// Kick off async text generation job
			GetParentWindow()->AsyncAddTextRegionToCache( *pCommand );
		}

		// Calling GetLayoutFileDefine is mildly expensive, so only call it if we actually need to draw selection rects
		if ( !m_vecSelectionRects.IsEmpty() )
		{
			Color selectedTextBackgroundColor( 0xFF, 0xFF, 0x00, 0x9F );
			const char *pchSelectedTextBackgroundColor = GetLayoutFileDefine( "selectedTextBackgroundColor" );
			if ( pchSelectedTextBackgroundColor )
			{
				CSSHelpers::BParseColor( &selectedTextBackgroundColor, pchSelectedTextBackgroundColor );
			}

			// Draw the selection rect
			FOR_EACH_VEC( m_vecSelectionRects, i )
			{
				IUITextLayout::HitTestRegionRect_t &rect = m_vecSelectionRects[ i ];
				AccessRenderEngine()->DrawSolidColorRect( rect.topLeft.x, rect.topLeft.y + flOffsetY, rect.bottomRight.x, rect.bottomRight.y + flOffsetY, selectedTextBackgroundColor.GetRawColor(), k_EAntialiasingNone );
			}
		}

	}
}


//-----------------------------------------------------------------------------
// Purpose: Paint label contents
//-----------------------------------------------------------------------------
void CLabel::SetMaxChars( EStringTruncationStyle eTruncationStyle, uint32 nMaxChars )
{
	m_eStringTruncationStyle = eTruncationStyle;
	m_nMaxChars = nMaxChars;
}


//-----------------------------------------------------------------------------
// Purpose: Searches the rest of the tag looking for the specified text
//-----------------------------------------------------------------------------
const char *FindStrInTag( const char *pchTag, const char *pchSearchFor )
{
	int cchSearch = V_strlen( pchSearchFor );
	bool bInQuote = false;

	// stop at end of string or end of tag
	for ( const char *pchCurrent = pchTag; *pchCurrent != '\0' && (bInQuote || *pchCurrent != '>'); pchCurrent++ )
	{
		if ( *pchCurrent == '"' )
		{
			bInQuote = !bInQuote;
			continue;
		}

		if ( bInQuote )
			continue;
		
		if ( V_strnicmp( pchCurrent, pchSearchFor, cchSearch ) != 0 )
			continue;

		// found
		return pchCurrent;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Parses string from a tag, adds it to list and returns index
//-----------------------------------------------------------------------------
int CLabel::ParseStringFromTag( const char *pchTag, const char *pchString, CUtlVector<CUtlString> **ppVecStrings )
{
	const char *pchStart = FindStrInTag( pchTag, pchString );

	if ( !pchStart )
		return -1;
	
	// find end of quote
	pchStart += V_strlen( pchString );
	for ( const char *pchCurrent = pchStart; *pchCurrent != '\0'; pchCurrent++ )
	{
		// tag ended before quote?
		if ( *pchCurrent == '>' )
			return -1;

		if ( *pchCurrent != '"' )
			continue;
		
		// found
		if ( (*ppVecStrings) == NULL)
			(*ppVecStrings) = new CUtlVector< CUtlString >();

		int iInserted = (*ppVecStrings)->AddToTail();
		(*ppVecStrings)->Element( iInserted ).SetDirect( pchStart, pchCurrent - pchStart );
		return iInserted;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Parses the HREF from a tag, adds it to list and returns index
//-----------------------------------------------------------------------------
int CLabel::ParseHREFFromTag( const char *pchTag )
{
	int iIndex = ParseStringFromTag( pchTag, "href=\"", &m_pvecParsedHREFs );

	if ( iIndex != -1 )
	{
		// Ensure that we handle input if we have a href tag
		SetAcceptsInput( true );
	}

	return iIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Parses the mouseover from a tag, adds it to list and returns index
//-----------------------------------------------------------------------------
int CLabel::ParseMouseOverFromTag( const char *pchTag )
{
	return ParseStringFromTag( pchTag, "onmouseover=\"", &m_pvecParsedMouseOvers );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the mouseout from a tag, adds it to list and returns index
//-----------------------------------------------------------------------------
int CLabel::ParseMouseOutFromTag( const char *pchTag )
{
	return ParseStringFromTag( pchTag, "onmouseout=\"", &m_pvecParsedMouseOuts );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the mouseout from a tag, adds it to list and returns index
//-----------------------------------------------------------------------------
int CLabel::ParseContextMenuFromTag( const char *pchTag )
{
	return ParseStringFromTag( pchTag, "oncontextmenu=\"", &m_pvecParsedContextMenus );
}

//-----------------------------------------------------------------------------
// Purpose: Parses the value of a given attribute from a tag given a character
// to use at the delimeter for the start/end of the tag.
//-----------------------------------------------------------------------------
bool ParseAttributeFromTagDelimiter( const char *pchTag, const char *pchAttributeName, char chDelimeter, char *pchAttributeValue, int nAttributeValueSize )
{
	CFmtStr strStart( "%s=%c", pchAttributeName, chDelimeter );

	const char *pchStart = FindStrInTag( pchTag, strStart.Get() );
	if ( !pchStart )
		return false;

	pchStart += strStart.Length();

	const char *pchEnd = V_strchr( pchStart, chDelimeter );
	if ( !pchEnd || pchEnd == pchStart )
		return false;

	V_strncpy( pchAttributeValue, pchStart, MIN( pchEnd - pchStart + 1, nAttributeValueSize ) );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Parses the value of a given attribute from a tag accepting either
// single or double quotes to start/end the value
//-----------------------------------------------------------------------------
bool ParseAttributeFromTag( const char *pchTag, const char *pchAttributeName, char *pchAttributeValue, int nAttributeValueSize )
{
	return ParseAttributeFromTagDelimiter( pchTag, pchAttributeName, '"', pchAttributeValue, nAttributeValueSize ) ||
		ParseAttributeFromTagDelimiter( pchTag, pchAttributeName, '\'', pchAttributeValue, nAttributeValueSize );
}


//-----------------------------------------------------------------------------
// Purpose: Parses color attribute from start tag
//-----------------------------------------------------------------------------
bool ParseColorFromTag( const char *pchTag, Color &color )
{
	char szColor[ 64 ];
	if ( !ParseAttributeFromTag( pchTag, "color", szColor, sizeof( szColor ) ) )
		return false;

	return CSSHelpers::BParseColor( &color, szColor );
}

//-----------------------------------------------------------------------------
// Purpose: Parses class attribute from start tag
//-----------------------------------------------------------------------------
void ParseClassesFromTag( const char *pchTag, CUtlVector< CPanoramaSymbol > &vecClasses )
{
	char szClasses[ 256 ];
	if ( !ParseAttributeFromTag( pchTag, "class", szClasses, sizeof( szClasses ) ) )
		return;

	char *pszStart = szClasses;

	while ( true )
	{
		char *pszEnd = V_strchr( pszStart, ' ' );

		if ( pszEnd != NULL )
		{
			*pszEnd = '\0';
		}

		if ( pszStart[ 0 ] != '\0' )
		{
			vecClasses.AddToTail( CPanoramaSymbol( pszStart ) );
		}

		if ( pszEnd == NULL )
			break;

		pszStart = pszEnd + 1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Container for individual open tag info
//-----------------------------------------------------------------------------
struct OpenFormatTag_t
{
	CLabel::EHTMLFormatFlag m_eHTMLFormatFlag;
	CCopyableUtlVector< CPanoramaSymbol > m_vecClasses;
	Color m_color;
};


//-----------------------------------------------------------------------------
// Purpose: Returns HTML format flags that are valid from provided vector
//-----------------------------------------------------------------------------
uint CalcHTMLFormatFlags( const CUtlVector< OpenFormatTag_t > &vecOpenFormatTags )
{
	uint unFlags = 0;
	FOR_EACH_VEC( vecOpenFormatTags, i )
	{
		unFlags |= vecOpenFormatTags[i].m_eHTMLFormatFlag;
	}

	return unFlags;
}


//-----------------------------------------------------------------------------
// Purpose: Parses provided text as HTML
//-----------------------------------------------------------------------------
void CLabel::SetFromHTMLInternal( const char *pchText, bool bAppend, bool bTrusted )
{
	if ( !bAppend )
	{
		RemoveTextRangeFormats();
	}

	// don't uppercase yet.. we are going to set it again
	const char *pchRawLocText = UILocalize()->PchFindRawString( pchText );
	if ( !pchRawLocText )
		pchRawLocText = pchText; // not a loc string, just use our input

	const int k_cchMaxHTML = 64 * 1024;
	const char *k_rgszKnownTags[]			= { "a", "b", "br", "i", "img", "em", "p", "strong", "li", "span", "h1", "h2", "font", "child", "pre", "panel" };
	const char *k_rgszKnownTagsNonStrict[]	= { "a", "b", "br", "i", "img", "em", "p", "strong", "li", "span", "h1", "h2", "font", "child", "pre", "panel", "\n" };
	const char *k_rgszNoCloseTags[] = { "br", "img", "child", "panel" };
	const EHTMLFormatFlag k_rgKnownTagFormatFlags[] = { k_EHTMLFormatTagAnchor, k_EHTMLFormatTagBold, k_EHTMLFormatTagNone, k_EHTMLFormatTagItalics, k_EHTMLFormatTagNone,
		k_EHTMLFormatTagEmphasized, k_EHTMLFormatTagNone, k_EHTMLFormatTagStrong, k_EHTMLFormatTagNone, k_EHTMLFormatTagSpan, k_EHTMLFormatTagHeader1, k_EHTMLFormatTagHeader2,
		k_EHTMLFormatTagFont, k_EHTMLFormatTagNone, k_EHTMLFormatTagPre, k_EHTMLFormatTagNone };

	COMPILE_TIME_ASSERT( V_ARRAYSIZE( k_rgszKnownTags ) == V_ARRAYSIZE( k_rgKnownTagFormatFlags ) );

	CUtlBuffer bufHTML;
	if ( m_bHtmlStrict )
	{
		V_StripAndPreserveHTMLCore( &bufHTML, pchRawLocText, k_rgszKnownTags, V_ARRAYSIZE( k_rgszKnownTags ), k_rgszNoCloseTags, V_ARRAYSIZE( k_rgszNoCloseTags ), k_cchMaxHTML );
	}
	else
	{
		// non "strict mode" will preserver new line
		// Note that V_StripAndPreserveHTMLCore will not strip '\t' ('\t' handled below)
		V_StripAndPreserveHTMLCore( &bufHTML, pchRawLocText, k_rgszKnownTagsNonStrict, V_ARRAYSIZE( k_rgszKnownTagsNonStrict ), k_rgszNoCloseTags, V_ARRAYSIZE( k_rgszNoCloseTags ), k_cchMaxHTML );
	}

	CUtlStringBuilder strParsed;
	strParsed.EnsureCapacity( bufHTML.TellPut() );

	bool bStartRangeOnNewText = false;
	int iHREF = -1;
	int iMouseOver = -1;
	int iMouseOut = -1;
	int iContextMenu = -1;
	uint unRemainingFormatFlags = k_EHTMLFormatTagNone;
	CUtlVector< CPanoramaSymbol > vecCurrentClasses;
	Color currentColor = TextRangeFormat_t::k_colorUnspecified;
	
	int cAddedUniChars = 0;	// this is a code point index in full Unicode / uchar32 characters
	if ( bAppend )
	{
		cAddedUniChars = V_UnicodeLength( m_pLocText->String() );
	}

	bool bWhitespaceSinceParagraph = true;
	int nPreTagDepth = 0;
	CUtlVector< OpenFormatTag_t > vecOpenFormatTags;

	const char *pchCurrent = (char*)bufHTML.Base();
	while ( *pchCurrent != '\0' )
	{
		// this is supposed to be HTML, so skip return line and tabs unless we're within <pre>
		if ( m_bHtmlStrict && nPreTagDepth == 0 && ( *pchCurrent == '\n' || *pchCurrent == '\t' ) )
		{
			pchCurrent++;
			continue;
		}

		// don't output whitespace if we haven't seen a character this paragraph
		if ( nPreTagDepth == 0 && bWhitespaceSinceParagraph && V_isspace( *pchCurrent ) )
		{
			pchCurrent++;
			continue;
		}

		// decode to UTF8 while we go. This prevents a full buffer copy later and lets us keep our character offsets accurate
		if ( *pchCurrent == '&' )
		{
			// find end
			const char *pchEndEncoding = V_strstr( pchCurrent, ";" );
			// maximum entity length in our table is 8, plus NUL terminator; this leaves room for larger future ones
			char rgchEncoded[ 32 ];
			int cubEncoded = ( int )MIN( pchEndEncoding - pchCurrent + 2, V_ARRAYSIZE( rgchEncoded ) ); // include \0
			V_strncpy( rgchEncoded, pchCurrent, cubEncoded );

			char rgchDecoded[ 32 ];
			if ( pchEndEncoding && V_HtmlEntityDecodeToUTF8( rgchDecoded, V_ARRAYSIZE( rgchDecoded ), rgchEncoded, cubEncoded ) )
			{
				// make sure decoded to 1 character.. else failed
				if ( V_UnicodeLength( rgchDecoded ) == 1 )
				{
					// we found more text to output. If necessary, start a new text range
					if ( bStartRangeOnNewText )
					{
						bStartRangeOnNewText = false;

						// don't need to add blocks if using default text formatting
						if ( unRemainingFormatFlags != k_EHTMLFormatTagNone )
						{
							AppendTextRangeFormat( cAddedUniChars, -1, unRemainingFormatFlags, vecCurrentClasses, iHREF, iMouseOver, iMouseOut, iContextMenu, currentColor, NULL, false );
						}
					}
					
					// text.. output
					strParsed.Append( rgchDecoded );
					cAddedUniChars++;
					pchCurrent = pchEndEncoding + 1;

					if ( !V_isspace( rgchDecoded[0] ) )
						bWhitespaceSinceParagraph = false;

					continue;
				}
			}
		}

		// tag?
		if ( *pchCurrent == '<' )
		{
			// skip <
			pchCurrent++;

			// end tag?
			bool bEndTag = false;
			if ( *pchCurrent == '/' )
			{
				pchCurrent++;
				bEndTag = true;
			}

			// need tag type length
			const char *pchEndTagType = pchCurrent;
			while ( *pchEndTagType != '>' && ( *pchEndTagType != '/' || bEndTag ) && !V_isspace( *pchEndTagType ) && *pchEndTagType != '\0' )
				pchEndTagType++;

			// found end?
			if ( *pchEndTagType == '\0' )
				break;

			for ( int iTag = 0; iTag < V_ARRAYSIZE( k_rgszKnownTags ); iTag++ )
			{
				int cchCompare = V_strlen( k_rgszKnownTags[iTag] );

				// make sure tag lengths are the same
				if ( cchCompare != (pchEndTagType - pchCurrent) )
					continue;

				if ( V_strnicmp( pchCurrent, k_rgszKnownTags[iTag], cchCompare ) != 0 )
					continue;

				// found tag
				
				// handle special tags first
				if ( k_rgKnownTagFormatFlags[iTag] == k_EHTMLFormatTagNone )
				{
					if ( V_strnicmp( pchCurrent, "br", 2 ) == 0 )
					{
						strParsed.Append( "\n" );
						cAddedUniChars++;
						bWhitespaceSinceParagraph = true;
					}
					else if ( V_strnicmp( pchCurrent, "panel", 5 ) == 0 )
					{
						// if in previous tag, end old tag
						TextRangeFormat_t *pFormat = NULL;
						if ( m_vecTextRangeFormats.Count() > 0 )
							pFormat = &m_vecTextRangeFormats[ m_vecTextRangeFormats.Count() - 1 ];

						if ( !bStartRangeOnNewText && pFormat && pFormat->m_iEndChar == -1 )
							pFormat->m_iEndChar = cAddedUniChars - 1;

						char szClass[ 256 ] = "";
						ParseAttributeFromTag( pchCurrent, "class", szClass, sizeof( szClass ) );

						CFmtStr strChildID( "InlinePanel%d", s_unNextInlinePanelID++ );
						CPanel2D *pPanel = new CPanel2D( this, strChildID.Get() );

						if ( szClass[0] != '\0' )
							pPanel->AddClasses( szClass );

						// Pango will create a rectangle with the given area for each character in the string. So, insert a dummy space character to represent the image.
						strParsed.Append( " " );
						cAddedUniChars++;

						bWhitespaceSinceParagraph = false;
						AppendTextRangeFormat( cAddedUniChars - 1, cAddedUniChars - 1, unRemainingFormatFlags, vecCurrentClasses, iHREF, iMouseOver, iMouseOut, iContextMenu, currentColor, strChildID.Get(), true );

						bStartRangeOnNewText = true;
						break;
					}
					else if ( V_strnicmp( pchCurrent, "p", 1 ) == 0 )
					{
						if ( !bWhitespaceSinceParagraph || nPreTagDepth > 0 )
						{
							// for start or end tag, append 2 return lines
							strParsed.Append( "\n\n" );
							cAddedUniChars += 2;
							bWhitespaceSinceParagraph = true;
						}
					}
					else if ( V_strnicmp( pchCurrent, "li", 2 ) == 0 )
					{
						if ( !bWhitespaceSinceParagraph || nPreTagDepth > 0 )
						{
							strParsed.Append( "\n" );
							cAddedUniChars++;
							bWhitespaceSinceParagraph = true;
						}

						if ( !bEndTag )
						{
							unsigned char rgchUTF8BulletAndSpace[] = { 0xe2, 0x97, 0x8f, ' ', 0x00 };
							strParsed.Append( (char*)rgchUTF8BulletAndSpace );
							cAddedUniChars += 2;
							bWhitespaceSinceParagraph = false;
						}						
					}
					else if ( V_strnicmp( pchCurrent, "img", 3 ) == 0 )
					{
						// if in previous tag, end old tag
						TextRangeFormat_t *pFormat = NULL;
						if ( m_vecTextRangeFormats.Count() > 0 )
							pFormat = &m_vecTextRangeFormats[ m_vecTextRangeFormats.Count() - 1 ];

						if ( !bStartRangeOnNewText && pFormat && pFormat->m_iEndChar == -1 )
							pFormat->m_iEndChar = cAddedUniChars - 1;

						char szImageSrc[ 2048 ] = "";
						ParseAttributeFromTag( pchCurrent, "src", szImageSrc, sizeof( szImageSrc ) );
						V_StrTrim( szImageSrc );  // Remove leading and trailing whitespace as per HTML spec

						char szClass[ 256 ] = "";
						ParseAttributeFromTag( pchCurrent, "class", szClass, sizeof( szClass ) );

						CFmtStr strChildID( "InlineImage%d", s_unNextInlineImageID++ );
						CImagePanel *pImagePanel = new CImagePanel( this, strChildID.Get() );
						pImagePanel->AddClass( "InlineImage" );

						if ( szClass[0] != '\0' )
							pImagePanel->AddClasses( szClass );

						pImagePanel->SetScaling( m_InlineImageScalingMethod );
						pImagePanel->SetImage( szImageSrc );

						// Pango will create a rectangle with the given area for each character in the string. So, insert a dummy space character to represent the image.
						strParsed.Append( " " );
						cAddedUniChars++;

						bWhitespaceSinceParagraph = false;
						AppendTextRangeFormat( cAddedUniChars - 1, cAddedUniChars - 1, unRemainingFormatFlags, vecCurrentClasses, iHREF, iMouseOver, iMouseOut, iContextMenu, currentColor, strChildID.Get(), true );

						bStartRangeOnNewText = true;
						break;
					}
					else if ( V_strnicmp( pchCurrent, "child", 5 ) == 0 )
					{
						// if in previous tag, end old tag
						TextRangeFormat_t *pFormat = NULL;
						if ( m_vecTextRangeFormats.Count() > 0 )
							pFormat = &m_vecTextRangeFormats[ m_vecTextRangeFormats.Count() - 1 ];

						if ( !bStartRangeOnNewText && pFormat && pFormat->m_iEndChar == -1 )
							pFormat->m_iEndChar = cAddedUniChars - 1;

						char szChildID[ 256 ] = "";
						ParseAttributeFromTag( pchCurrent, "id", szChildID, sizeof( szChildID ) );

						// Pango will create a rectangle with the given area for each character in the string. So, insert a dummy space character to represent the child.
						strParsed.Append( " " );
						cAddedUniChars++;

						bWhitespaceSinceParagraph = false;
						AppendTextRangeFormat( cAddedUniChars - 1, cAddedUniChars - 1, unRemainingFormatFlags, vecCurrentClasses, iHREF, iMouseOver, iMouseOut, iContextMenu, currentColor, szChildID, false );

						bStartRangeOnNewText = true;
						break;
					}
					

					// special tags end here (may not have close tag, etc)
					break;
				}

				// handle headers
				if ( k_rgKnownTagFormatFlags[iTag] == k_EHTMLFormatTagHeader1 || k_rgKnownTagFormatFlags[iTag] == k_EHTMLFormatTagHeader2 )
				{
					if ( !bWhitespaceSinceParagraph || nPreTagDepth > 0 || bEndTag )
					{
						strParsed.Append( "\n" );
						cAddedUniChars++;
						bWhitespaceSinceParagraph = true;
					}
				}

				// handle <pre>
				if ( k_rgKnownTagFormatFlags[ iTag ] == k_EHTMLFormatTagPre )
				{
					if ( bEndTag )
					{
						nPreTagDepth--;
						if ( nPreTagDepth < 0 )
						{
							Assert( false ); // V_StripAndPreserveHTMLCore should have made sure tags were balanced
							nPreTagDepth = 0;
						}
					}
					else
					{
						nPreTagDepth++;
					}

					strParsed.Append( "\n" );
					cAddedUniChars++;
					bWhitespaceSinceParagraph = true;
				}
				
				if ( !bEndTag )
				{
					CUtlVector< CPanoramaSymbol > vecClasses;
					ParseClassesFromTag( pchCurrent, vecClasses );

					Color color = TextRangeFormat_t::k_colorUnspecified;
					ParseColorFromTag( pchCurrent, color );

					OpenFormatTag_t openTag = { k_rgKnownTagFormatFlags[ iTag ], vecClasses, color };
					vecOpenFormatTags.AddToTail( openTag );

					// if in previous tag, end old tag
					TextRangeFormat_t *pFormat = NULL;
					if ( m_vecTextRangeFormats.Count() > 0 )
						pFormat = &m_vecTextRangeFormats[ m_vecTextRangeFormats.Count() - 1 ];

					if ( !bStartRangeOnNewText && pFormat && pFormat->m_iEndChar == -1 )
						pFormat->m_iEndChar = cAddedUniChars - 1;

					bStartRangeOnNewText = true;
					unRemainingFormatFlags = CalcHTMLFormatFlags( vecOpenFormatTags );

					vecCurrentClasses.AddVectorToTail( vecClasses );

					if ( color != TextRangeFormat_t::k_colorUnspecified )
						currentColor = color;
					
					if ( k_rgKnownTagFormatFlags[iTag] == k_EHTMLFormatTagAnchor )
					{
						iHREF = ParseHREFFromTag( pchCurrent );
						iMouseOver = ParseMouseOverFromTag( pchCurrent );
						iMouseOut = ParseMouseOutFromTag( pchCurrent );
						iContextMenu = ParseContextMenuFromTag( pchCurrent );
					}
				}
				else
				{
					// end tag
					int iLastOpenTag = vecOpenFormatTags.Count() > 0 ? vecOpenFormatTags.Count() - 1 : -1;
					if ( iLastOpenTag >= 0 && vecOpenFormatTags[iLastOpenTag].m_eHTMLFormatFlag == k_rgKnownTagFormatFlags[iTag] )
					{
						// if tag applied a style or color, find previous
						int nClassesMatched = vecOpenFormatTags[ iLastOpenTag ].m_vecClasses.Count();
						bool bColorMatched = currentColor != TextRangeFormat_t::k_colorUnspecified && currentColor == vecOpenFormatTags[ iLastOpenTag ].m_color;

						vecOpenFormatTags.Remove( vecOpenFormatTags.Count() - 1 );

						Assert( nClassesMatched <= vecCurrentClasses.Count() );
						vecCurrentClasses.RemoveMultipleFromTail( nClassesMatched );

						if ( bColorMatched )
						{
							currentColor = TextRangeFormat_t::k_colorUnspecified;
							FOR_EACH_VEC_BACK( vecOpenFormatTags, i )
							{
								if ( vecOpenFormatTags[ i ].m_color == TextRangeFormat_t::k_colorUnspecified )
									continue;

								currentColor = vecOpenFormatTags[ i ].m_color;
								break;
							}
						}
					}

					if ( !bStartRangeOnNewText )
					{
						// should be in a range that we need to end
						TextRangeFormat_t *pFormat = NULL;
						if ( m_vecTextRangeFormats.Count() > 0 )
							pFormat = &m_vecTextRangeFormats[ m_vecTextRangeFormats.Count() - 1 ];

						if ( !pFormat || pFormat->m_iEndChar != -1 )
						{
							AssertMsg( false, "Mismatched tags.. should not have gotten here!" );
							break;
						}

						pFormat->m_iEndChar = cAddedUniChars - 1;
					}

					unRemainingFormatFlags = CalcHTMLFormatFlags( vecOpenFormatTags );
					bStartRangeOnNewText = true;

					if ( k_rgKnownTagFormatFlags[iTag] == k_EHTMLFormatTagAnchor )
					{
						iHREF = -1;
						iMouseOver = -1;
						iMouseOut = -1;
						iContextMenu = -1;
					}
				}

				// handled
				break;
			}

			// don't output tag.. move to end of tag
			// FUTURE optimize this - we already parsed to the end so we know where that is. no need
			// to skip over the tag name again here
			while ( *pchCurrent != '>' && *pchCurrent != '\0' )
				pchCurrent++;

			// skip if we found end
			if ( *pchCurrent == '>' )
				pchCurrent++;

			continue;
		}

		// we found more text to output. If necessary, start a new text range
		if ( bStartRangeOnNewText )
		{
			bStartRangeOnNewText = false;

			// don't need to add blocks if using default text formatting
			if ( unRemainingFormatFlags != k_EHTMLFormatTagNone )
			{
				AppendTextRangeFormat( cAddedUniChars, -1, unRemainingFormatFlags, vecCurrentClasses, iHREF, iMouseOver, iMouseOut, iContextMenu, currentColor, NULL, false );
			}
		}

		// text.. output
		const char *pchNextUTF8 = V_UnicodeAdvance( pchCurrent, 1 );
		Assert( pchNextUTF8 && pchNextUTF8 > pchCurrent );
		strParsed.Append( pchCurrent, pchNextUTF8 - pchCurrent );

		if ( !V_isspace( pchCurrent[0] ) )
			bWhitespaceSinceParagraph = false;

		cAddedUniChars++;
		pchCurrent = pchNextUTF8;
	}

	// check for errors
	if ( m_vecTextRangeFormats.Count() > 0 )
	{
		Assert( m_vecTextRangeFormats[ m_vecTextRangeFormats.Count() - 1 ].m_iEndChar >= 0 );
	}
	
	// store and transform if necessary
	if ( !bAppend )
	{
		ETextTransform eTransform = k_ETextTransformNone;
		if ( !BStylesDirty() )
			AccessStyle()->GetTextTransform( eTransform );
		else
			SetOnStylesChangedNeeded();

		m_pLocText = UILocalize()->PchSetString( UIPanel(), strParsed.String(), m_nMaxChars, m_eStringTruncationStyle, ETextTransformToEStringTransformStyle( eTransform ), k_eStringEscapeStyle_HTML, false, true );
	}
	else
	{
		m_pLocText.GetMutable()->AppendText( strParsed.String() );
	}

	// text changed. Invalidate size, position & update style ranges if necessary. Calling MarkStylesDirty() isn't enough as styles on label probably didn't change.
	m_bContentSizeDirty = true;
	InvalidateSizeAndPosition();
	MarkStylesDirty( false );
	UpdateTextRangeStyles();
}


//-----------------------------------------------------------------------------
// Purpose: Checks if mouse coords are within text range
//-----------------------------------------------------------------------------
bool CLabel::BCoordsInTextRange( const TextRangeFormat_t &rangeFormat, float flX, float flY )
{
	FOR_EACH_VEC( rangeFormat.m_vecBoundingBoxes, i )
	{
		const TextRangeFormat_t::RangeFormatBox_t &box = rangeFormat.m_vecBoundingBoxes[i];
		if ( flX < box.topLeft.x || flX > box.bottomRight.x )
			continue;

		if ( flY < box.topLeft.y || flY > box.bottomRight.y )
			continue;

		// hit
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the index of the text range that is at the specified position
//-----------------------------------------------------------------------------
int CLabel::FindTextRangeAt( float flX, float flY )
{
	FOR_EACH_VEC( m_vecTextRangeFormats, i )
	{
		if ( BCoordsInTextRange( m_vecTextRangeFormats[i], flX, flY ) )
			return i;
	}

	return m_vecTextRangeFormats.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: Deals with any anchor tag HREF/mouseover/mouseout handling that we want to do
//-----------------------------------------------------------------------------
bool CLabel::HandleAnchorTagEvent( CUtlVector< CUtlString > *pvecLinks, int iLinkIndex )
{
	if ( pvecLinks == NULL || iLinkIndex < 0 || iLinkIndex >= pvecLinks->Count() )
		return false;

	const char *pszTag = pvecLinks->Element( iLinkIndex );

	if ( pszTag == NULL )
		return false;

	const int nEventPrefixLength = V_ARRAYSIZE( k_szEventPrefix ) - 1;
	const int nJavascriptPrefixLength = V_ARRAYSIZE( k_szJavascriptPrefix ) - 1;

	if ( V_strncmp( pszTag, k_szEventPrefix, nEventPrefixLength ) == 0 )
	{
		// If the tag starts with "event:", then convert it directly to an IUIEvent if we have hover
		const char *pszEvent = pszTag + nEventPrefixLength;
		IUIEvent* pEvent = UIEngine()->CreateEventFromString( UIPanel(), pszEvent, &pszEvent );
		if ( pEvent )
		{
			UIEngine()->DispatchEvent( pEvent );
		}
		else
		{
			AssertMsg1( false, "Failed to parse \"%s\" into a valid IUIEvent", pszTag );
			return false;
		}

		return true;
	}
	else if ( V_strncmp( pszTag, k_szJavascriptPrefix, nJavascriptPrefixLength ) == 0 )
	{
		// If the tag starts with "javascript:", then run the value as javascript
		const char *pszJavascript = pszTag + nJavascriptPrefixLength;
		UIEngine()->RunScript( UIPanel(), pszJavascript, "LabelHREF", 0, 0, false, false );

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Track last mouse position within us
//-----------------------------------------------------------------------------
void CLabel::OnMouseMove( float flMouseX, float flMouseY )
{
	// If we haven't moved at least 1 entire pixel don't redo this work
	if( (int)m_LastMousePos.x == (int)flMouseX && (int)m_LastMousePos.y == (int)flMouseY )
		return;

	VPROF_BUDGET( "Clabel::OnMouseMove", VPROF_BUDGETGROUP_TENFOOT );

	m_LastMousePos.x = flMouseX;
	m_LastMousePos.y = flMouseY;

	// see if over a text range
	if ( !m_bLeftMouseIsDown )
	{
		int iRange = FindTextRangeAt( flMouseX, flMouseY );
		TextRangeFormat_t *pCurrentHoverRange = (iRange != m_vecTextRangeFormats.InvalidIndex()) ? &m_vecTextRangeFormats[iRange] : NULL;

		if ( pCurrentHoverRange == m_pLastHoverRange )
			return;

		// remove hover flag if previously set
		if ( m_pLastHoverRange )
		{
			m_pLastHoverRange->m_unStyleFlags &= ~k_EStyleFlagHover;
			HandleAnchorTagEvent( m_pvecParsedMouseOuts, m_pLastHoverRange->m_iMouseOut );
		}

		// set if hovering over a range. Only do this if the entire label has hover as well - we can still get an OnMouseMove without hover in
		// rare cases like popping up a context menu,
		if ( BHasHoverStyle() )
		{
			if ( pCurrentHoverRange )
			{
				pCurrentHoverRange->m_unStyleFlags |= k_EStyleFlagHover;
				HandleAnchorTagEvent( m_pvecParsedMouseOvers, pCurrentHoverRange->m_iMouseOver );
			}

			m_pLastHoverRange = pCurrentHoverRange;
		}
		else
		{
			m_pLastHoverRange = NULL;
		}

		MarkStylesDirty( true );
	}
	else if ( m_bAllowTextSelection )
	{
		// If we are dragging while the mouse is down, update selection state

		// Need to hit test text to determine where the click occurred
		float flWidth = GetActualLayoutWidth();
		float flHeight = GetActualRenderHeight();

		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

		IUITextLayout *pLayout = CreateCurrentLayoutTextLayout();
		if( pLayout )
		{
			float x = m_LastMousePos.x - GetInterpolatedXScrollOffset() - flLeft;
			float y = m_LastMousePos.y - GetInterpolatedYScrollOffset() - flTop;

			uint32 unOffset = 0;
			bool bTrailing = false;
			bool bInText = false;
			pLayout->HitTestPoint( Vector2D( x, y ), unOffset, bTrailing, bInText );

			if ( bTrailing && unOffset < (uint32)m_pLocText->Length() )
				++unOffset;

			if( (int)unOffset > m_nSelectionStartIndex )
			{
				if( m_nSelectionStartIndex != (int32)unOffset )
				{
					m_nSelectionEndIndex = unOffset;
					m_bSelectionRectDirty = true;
					SetRepaint( k_EPanelRepaintFull );
				}
			}
			else
			{
				if( m_nSelectionEndIndex == -1 )
				{
					m_nSelectionEndIndex = m_nSelectionStartIndex;
					m_bSelectionRectDirty = true;
					SetRepaint( k_EPanelRepaintFull );
				}

				if( m_nSelectionStartIndex != (int32)unOffset )
				{
					m_nSelectionEndIndex = unOffset;
					m_bSelectionRectDirty = true;
					SetRepaint( k_EPanelRepaintFull );
				}
			}

		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: Mouse button was pressed
//-----------------------------------------------------------------------------
void CLabel::CopySelectionToClipboard()
{
	if ( !BHasSelection() )
		return;

	CStrAutoEncodeSrc2 strAuto( m_pLocText->String() );

	int nStartIndex = m_nSelectionStartIndex;
	int nEndIndex = m_nSelectionEndIndex;
	if ( nStartIndex > nEndIndex )
	{
		std::swap( nStartIndex, nEndIndex );
	}

	uchar32 *pStr = const_cast<uchar32*>( strAuto.ToUTF32() );
	int nLen = V_UnicodeLength( pStr );
	pStr[ MIN( nEndIndex, nLen ) ] = 0;
	Assert( nStartIndex < nLen );
	if ( nStartIndex < nLen )
		pStr += nStartIndex;

	CStrAutoEncodeSrc2 strSubUTF8( pStr );

	UIEngine()->CopyToClipboard( strSubUTF8.ToUTF8(), "#UI_Paste_Selection" );
}


//-----------------------------------------------------------------------------
// Purpose: Copy selected label text
//-----------------------------------------------------------------------------
bool CLabel::OnCopySelectedLabelText( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( pPanel.Get() == UIPanel() )
	{
		CopySelectionToClipboard();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Select all text in the label
//-----------------------------------------------------------------------------
void CLabel::SelectAll()
{
	if ( !m_bAllowTextSelection )
		return;

	int nEndIndex = V_UnicodeLength( m_pLocText->String() );
	if ( m_nSelectionStartIndex == 0 && m_nSelectionEndIndex == nEndIndex )
		return;

	m_nSelectionStartIndex = 0;
	m_nSelectionEndIndex = nEndIndex;
	m_bSelectionRectDirty = true;
	
	InvalidatePosition();
}


//-----------------------------------------------------------------------------
// Purpose: Clear the current selection
//-----------------------------------------------------------------------------
void CLabel::ClearSelection()
{
	if ( m_nSelectionStartIndex == -1 && m_nSelectionEndIndex == -1 )
		return;

	m_nSelectionStartIndex = -1;
	m_nSelectionEndIndex = -1;
	m_bSelectionRectDirty = true;

	InvalidatePosition();
}


//-----------------------------------------------------------------------------
// Purpose: Mouse button was pressed
//-----------------------------------------------------------------------------
bool CLabel::OnMouseButtonDown( const MouseData_t &code )
{
	m_pMouseDownRange = m_pLastHoverRange;

	if ( code.m_MouseCode == MOUSE_LEFT )
	{
		m_bLeftMouseIsDown = true;
		m_nSelectionStartIndex = -1;
		m_nSelectionEndIndex = -1;

		if ( m_bAllowTextSelection )
		{
			// Need to hit test text to determine where the click occurred
			float flWidth = GetActualLayoutWidth();
			float flHeight = GetActualRenderHeight();

			float flLeft, flTop, flRight, flBottom;
			AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

			IUITextLayout *pLayout = CreateCurrentLayoutTextLayout();
			if ( pLayout )
			{
				float x = m_LastMousePos.x - GetInterpolatedXScrollOffset() - flLeft;
				float y = m_LastMousePos.y - GetInterpolatedYScrollOffset() - flTop;

				uint32 unOffset = 0;
				bool bTrailing = false;
				bool bInText = false;
				pLayout->HitTestPoint( Vector2D( x, y ), unOffset, bTrailing, bInText );

				if ( bTrailing && unOffset < ( uint32 )m_pLocText->Length() )
					++unOffset;

				m_nSelectionStartIndex = unOffset;
				m_bSelectionRectDirty = true;
				InvalidatePosition();
			}
		}

		// If we already have focus, then we are done, if we didn't have focus, bubble up the event
		// so baseclass/input layer will set us to focus as well.
		if ( BHasKeyFocus() )
		{
			return true;
		}
	}

	return BaseClass::OnMouseButtonDown( code );
}


//-----------------------------------------------------------------------------
// Purpose: Mouse button was released
//-----------------------------------------------------------------------------
bool CLabel::OnMouseButtonUp( const MouseData_t &code )
{
	// First check if it's a right click, and if there is some context menu we should show
	if ( code.m_MouseCode == MOUSE_RIGHT )
	{
		// Need to hit test text to determine where the click occurred
		float flWidth = GetActualLayoutWidth();
		float flHeight = GetActualRenderHeight();

		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( flWidth, flHeight, false, flLeft, flTop, flRight, flBottom );

		IUITextLayout *pLayout = CreateCurrentLayoutTextLayout();
		if ( pLayout )
		{
			float x = m_LastMousePos.x - GetInterpolatedXScrollOffset() - flLeft;
			float y = m_LastMousePos.y - GetInterpolatedYScrollOffset() - flTop;

			uint32 unOffset = 0;
			bool bTrailing = false;
			bool bInText = false;
			pLayout->HitTestPoint( Vector2D( x, y ), unOffset, bTrailing, bInText );


			if ( bTrailing && unOffset < ( uint32 )m_pLocText->Length() )
				++unOffset;

			CSimpleContextMenu *pContextMenu = NULL;
			int nSelectionStartIndex = Min( m_nSelectionStartIndex, m_nSelectionEndIndex );
			int nSelectionEndIndex = Max( m_nSelectionStartIndex, m_nSelectionEndIndex );
			if ( nSelectionStartIndex != -1 && nSelectionEndIndex != -1 && ( int )unOffset >= nSelectionStartIndex && ( int )unOffset < nSelectionEndIndex )
			{
				// Right click was over a selected range of text, show copy as an option
				if ( !pContextMenu )
					pContextMenu = new CSimpleContextMenu( GetParentWindow(), "LabelRightClickContextMenu", this );

				pContextMenu->AddMenuItem( "#UI_Label_CopySelected", "CopySelectedLabelText()" );
			}
			// Didn't right click on selected text, so see if there's a custom context menu event on the range
			else if ( m_pMouseDownRange && HandleAnchorTagEvent( m_pvecParsedContextMenus, m_pMouseDownRange->m_iContextMenu ) )
			{
				return true;
			}

			if ( m_pMouseDownRange && m_pvecParsedHREFs && m_pMouseDownRange->m_iHREF != m_pvecParsedHREFs->InvalidIndex() )
			{
				const char *pszHREF = m_pvecParsedHREFs->Element( m_pMouseDownRange->m_iHREF ).String();

				if ( V_strncmp( pszHREF, k_szEventPrefix, V_ARRAYSIZE( k_szEventPrefix ) - 1 ) != 0 &&
					V_strncmp( pszHREF, k_szJavascriptPrefix, V_ARRAYSIZE( k_szJavascriptPrefix ) - 1 ) != 0 )
				{
					// Right click was over a URL
					if ( !pContextMenu )
						pContextMenu = new CSimpleContextMenu( GetParentWindow(), "LabelRightClickContextMenu", this );

					CFmtStr strOpenLabelURL( "BrowserGoToURL( %s )", pszHREF );
					pContextMenu->AddMenuItem( "#UI_Label_OpenLinkInBrowser", strOpenLabelURL.Access() );

						CFmtStr strCopyURL( "CopyStringToClipboard( %s, #UI_Paste_URL )", pszHREF );
					pContextMenu->AddMenuItem( "#UI_Label_CopyURLToClipboard", strCopyURL.Access() );
				}
			}

			if ( pContextMenu )
			{
				pContextMenu->SetVisible( true );
				pContextMenu->SetFocus();
				return true;
			}
		}

		return BaseClass::OnMouseButtonUp( code );
	}
	else if ( code.m_MouseCode == MOUSE_LEFT )
	{
		m_bLeftMouseIsDown = false;

		if( m_pCachedTextLayout )
		{
			UIEngine()->FreeTextLayout( m_pCachedTextLayout );
			m_pCachedTextLayout = NULL;
		}

		bool bMadeSelection = m_bAllowTextSelection && m_nSelectionEndIndex != -1 && m_nSelectionStartIndex != -1;

		// If we never dragged, then one of these (start) should be set but
		// the other (end) -1, if either are still -1 normalize to not set.
		if ( m_nSelectionEndIndex == -1 || m_nSelectionStartIndex == -1 )
		{
			m_nSelectionStartIndex = -1;
			m_nSelectionEndIndex = -1;
			m_bSelectionRectDirty = true;
		}

		if ( bMadeSelection || !m_pMouseDownRange || m_pLastHoverRange != m_pMouseDownRange )
			return BaseClass::OnMouseButtonUp( code );

		if ( !m_pvecParsedHREFs || m_pMouseDownRange->m_iHREF == m_pvecParsedHREFs->InvalidIndex() )
			return BaseClass::OnMouseButtonUp( code );

		if ( HandleAnchorTagEvent( m_pvecParsedHREFs, m_pMouseDownRange->m_iHREF ) )
			return true;

		// Otherwise just redirect the URL to the browser
		DispatchEvent( BrowserGoToURL(), this, m_pvecParsedHREFs->Element( m_pMouseDownRange->m_iHREF ).String() );
		return true;
	}

	return BaseClass::OnMouseButtonUp( code );
}


//-----------------------------------------------------------------------------
// Purpose: Get the target urls contained in this label
//-----------------------------------------------------------------------------
void CLabel::GetHREFTargets( CUtlVector< CUtlString > &vecTargets )
{
	vecTargets.RemoveAll();
	if ( m_pvecParsedHREFs )
	{
		FOR_EACH_VEC( *(m_pvecParsedHREFs ), i )
		{
			const CUtlString &strHREF = m_pvecParsedHREFs->Element( i );

			if ( V_strncmp( strHREF.Get(), k_szEventPrefix, V_ARRAYSIZE( k_szEventPrefix ) - 1 ) == 0 ||
				V_strncmp( strHREF.Get(), k_szJavascriptPrefix, V_ARRAYSIZE( k_szJavascriptPrefix ) - 1 ) == 0 )
			{
				continue;
			}

			vecTargets.AddToTail( strHREF );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLabel::EventStyleFlagsChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	// if the mouse just left our bounds, clear hover on range if necessary
	if ( !BHasHoverStyle() && m_pLastHoverRange )
	{
		HandleAnchorTagEvent( m_pvecParsedMouseOuts, m_pLastHoverRange->m_iMouseOut );
		m_pLastHoverRange->m_unStyleFlags &= ~k_EStyleFlagHover;
		m_pLastHoverRange = NULL;
		MarkStylesDirty( true );
	}

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets style for a range of text
//-----------------------------------------------------------------------------
void CLabel::SetStyleForRange( int iStartIndex, int iEndIndex, CPanoramaSymbol symStyle )
{
	if ( iStartIndex < 0 || iEndIndex >= m_pLocText->Length() )
	{
		AssertMsg( false, "Invalid character indexes passed to CLabel::SetStyleForRange.. fixing" );
		iStartIndex = MAX( 0, iStartIndex );
		iEndIndex = MIN( iEndIndex, m_pLocText->Length() - 1 );
	}

	FOR_EACH_VEC( m_vecTextRangeFormats, i )
	{
		TextRangeFormat_t &format = m_vecTextRangeFormats[i];

		// this before our section?
		if ( format.m_iEndChar < iStartIndex )
			continue;

		// if we start before the other section, create a new one up to that section if we haven't done so before
		TextRangeFormat_t *pPrevRange = i > 0 ? &m_vecTextRangeFormats[i - 1] : NULL;
		if ( iStartIndex < format.m_iStartChar && (!pPrevRange || pPrevRange->m_iEndChar != format.m_iStartChar - 1) )
		{			
			TextRangeFormat_t &formatInserted = m_vecTextRangeFormats[ m_vecTextRangeFormats.InsertBefore( i ) ];
			formatInserted.m_iStartChar = iStartIndex;
			formatInserted.m_iEndChar = MIN( format.m_iStartChar - 1, iEndIndex );
			formatInserted.m_vecClasses.AddToTail( symStyle );

			// done?
			if ( formatInserted.m_iEndChar == iEndIndex )
				break;
			
			// fix current index
			i++;
		}

		// split the range if necessary
		if ( iStartIndex >= format.m_iStartChar && iStartIndex <= iEndIndex )
		{			
			// new range ends before old.. split off the end
			if ( format.m_iEndChar > iEndIndex )
			{				
				TextRangeFormat_t &formatInserted = m_vecTextRangeFormats[ m_vecTextRangeFormats.InsertAfter( i ) ];
				formatInserted.m_iStartChar = iEndIndex + 1;
				formatInserted.m_iEndChar = format.m_iEndChar;
				formatInserted.m_vecClasses.AddVectorToTail( format.m_vecClasses );
				formatInserted.m_unHTMLFormatFlags = format.m_unHTMLFormatFlags;
				formatInserted.m_unStyleFlags = format.m_unStyleFlags;

				format.m_iEndChar = iEndIndex;
				format.m_vecClasses.RemoveAll();
				format.m_vecClasses.AddToTail( symStyle );

				// dont need to increment i here
			}

			// old range starts before new.. split off start
			if ( iStartIndex != format.m_iStartChar )
			{
				TextRangeFormat_t &formatInserted = m_vecTextRangeFormats[ m_vecTextRangeFormats.InsertBefore( i ) ];
				formatInserted.m_iStartChar = format.m_iStartChar;
				formatInserted.m_iEndChar = iStartIndex - 1;
				formatInserted.m_vecClasses.AddVectorToTail( format.m_vecClasses );
				formatInserted.m_unHTMLFormatFlags = format.m_unHTMLFormatFlags;
				formatInserted.m_unStyleFlags = format.m_unStyleFlags;

				i++;
			}

			// i should now be within our range
			TextRangeFormat_t &formatInRange = m_vecTextRangeFormats[i];
			formatInRange.m_vecClasses.RemoveAll();
			formatInRange.m_vecClasses.AddToTail( symStyle );
			continue;
		}
	}

	// the above loop handles all cases except for our range extending beyond the list of defined ranges. Handle that now.
	TextRangeFormat_t *pPrevRange = (m_vecTextRangeFormats.Count() > 0) ? &m_vecTextRangeFormats[m_vecTextRangeFormats.Count() - 1] : NULL;
	if ( !pPrevRange || pPrevRange->m_iEndChar < iEndIndex )
	{
		int iNewRangeStart = iStartIndex;
		if ( pPrevRange && pPrevRange->m_iEndChar > iStartIndex )
			iNewRangeStart = pPrevRange->m_iEndChar + 1;

		TextRangeFormat_t &formatInserted = m_vecTextRangeFormats[ m_vecTextRangeFormats.AddToTail() ];
		formatInserted.m_iStartChar = iNewRangeStart;
		formatInserted.m_iEndChar = iEndIndex;
		formatInserted.m_vecClasses.AddToTail( symStyle );
	}

	MarkStylesDirty( true );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the bounds of a text range used for tooltipping, if the text range has a mouseover event
//-----------------------------------------------------------------------------
bool CLabel::GetContextUIBounds( float *pflX, float *pflY, float *pflWidth, float *pflHeight )
{
	int iTextRange = FindTextRangeAt( m_LastMousePos.x, m_LastMousePos.y );
	if ( iTextRange == m_vecTextRangeFormats.InvalidIndex() )
		return false;

	TextRangeFormat_t &rangeFormat = m_vecTextRangeFormats[iTextRange];

	if ( rangeFormat.m_vecBoundingBoxes.Count() == 0 )
		return false;

	if ( rangeFormat.m_iMouseOver == -1 )
		return false;
	
	Vector2D topLeft(0,0), bottomRight(0,0);

	FOR_EACH_VEC( rangeFormat.m_vecBoundingBoxes, i )
	{
		const TextRangeFormat_t::RangeFormatBox_t &box = rangeFormat.m_vecBoundingBoxes[i];

		if ( i == 0 )
		{
			topLeft = box.topLeft;
			bottomRight = box.bottomRight;
			continue;
		}

		topLeft.x = Min( topLeft.x, box.topLeft.x );
		topLeft.y = Min( topLeft.y, box.topLeft.y );
		bottomRight.x = Max( bottomRight.x, box.bottomRight.x );
		bottomRight.y = Max( bottomRight.y, box.bottomRight.y );
	}

	GetPositionWithinWindow( pflX, pflY );

	*pflX += topLeft.x;
	*pflY += topLeft.y;
	*pflWidth = bottomRight.x - topLeft.x;
	*pflHeight = bottomRight.y - topLeft.y;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Clones panel
//-----------------------------------------------------------------------------
CPanel2D *CLabel::Clone()
{
	if ( !IsClonable() )
	{
		AssertMsg( false, "Panel can't be cloned (panel type or child not clonable)" );
		return NULL;
	}

	CLabel *pPanel = new CLabel( GetParent(), NULL );
	InitClonedPanel( pPanel );

	return pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Adds class specific data to clone a panel
//-----------------------------------------------------------------------------
void CLabel::InitClonedPanel( CPanel2D *pClone )
{
	BaseClass::InitClonedPanel( pClone );
	CLabel *pTarget = assert_cast< CLabel* >( pClone );

	pTarget->m_bContentSizeDirty = true;
	pTarget->m_bMayDrawOutsideBounds = true;
	pTarget->m_nMaxChars = m_nMaxChars;
	pTarget->m_eStringTruncationStyle = m_eStringTruncationStyle;
	pTarget->m_TextType = m_TextType;

	pTarget->m_vecTextRangeFormats.EnsureCapacity( m_vecTextRangeFormats.Count() );
	FOR_EACH_VEC( m_vecTextRangeFormats, i )
	{
		TextRangeFormat_t &format = pTarget->m_vecTextRangeFormats[ pTarget->m_vecTextRangeFormats.AddToTail() ];
		format.CopyWithoutRecalcData( m_vecTextRangeFormats[i] );
	}

	CloneLinksVector( m_pvecParsedHREFs, &pTarget->m_pvecParsedHREFs );
	CloneLinksVector( m_pvecParsedMouseOvers, &pTarget->m_pvecParsedMouseOvers );
	CloneLinksVector( m_pvecParsedMouseOuts, &pTarget->m_pvecParsedMouseOuts );
	CloneLinksVector( m_pvecParsedContextMenus, &pTarget->m_pvecParsedContextMenus );

	m_pLastHoverRange = NULL;
	m_pMouseDownRange = NULL;

	if ( m_pLocText )
		pTarget->m_pLocText = UILocalize()->CloneString( pTarget->UIPanel(), m_pLocText.Get(), m_TextType == CLabel::k_ETextTypeUnlocalized );
	else
		pTarget->m_pLocText.Clear();
}


//-----------------------------------------------------------------------------
// Purpose: Copies a vector 
//-----------------------------------------------------------------------------
/*static*/ void CLabel::CloneLinksVector( CUtlVector< CUtlString > *pSourceLinks, CUtlVector< CUtlString > **ppDestinationLinks )
{
	*ppDestinationLinks = NULL;

	if ( pSourceLinks )
	{
		*ppDestinationLinks = new CUtlVector< CUtlString >();
		( *ppDestinationLinks )->EnsureCapacity( pSourceLinks->Count() );

		FOR_EACH_VEC( *pSourceLinks, i )
		{
			int iInserted = ( *ppDestinationLinks )->AddToTail();
			( *ppDestinationLinks )->Element( iInserted ).Set( pSourceLinks->Element( i ).String() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Copies all struct data except for data which the label can calculate (panelstyle, bounding boxes, etc)
//-----------------------------------------------------------------------------
void CLabel::TextRangeFormat_t::CopyWithoutRecalcData( const CLabel::TextRangeFormat_t &rhs )
{
	m_iStartChar = rhs.m_iStartChar;
	m_iEndChar = rhs.m_iEndChar;
	m_unHTMLFormatFlags = rhs.m_unHTMLFormatFlags;
	m_vecClasses.RemoveAll();
	m_vecClasses.AddVectorToTail( rhs.m_vecClasses );
	m_iHREF = rhs.m_iHREF;
	m_unStyleFlags = rhs.m_unStyleFlags;
	m_color = rhs.m_color;
	m_strChildID = rhs.m_strChildID;
	m_bChildOwner = rhs.m_bChildOwner;

	if( m_pStyle )
	{
		UIEngine()->FreePanelStyle( m_pStyle );
		m_pStyle = NULL;
	}
	m_vecBoundingBoxes.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Return extra properties for the debugger
//-----------------------------------------------------------------------------
void CLabel::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	BaseClass::GetDebugPropertyInfo( pvecProperties );

	if ( m_pLocText != NULL )
	{
		const char *pszString = m_pLocText->String();
		if ( pszString )
		{
			DebugPropertyOutput_t *pProperty = new DebugPropertyOutput_t();
			pProperty->m_strName = "text";
			pProperty->m_strValue = pszString;

			// We might be too long to display, so if necessary copy the string contents and do a safe UTF8 truncation.
			enum { kMaxUTF8CodePointLength = 50 };

			if ( V_UnicodeLength( pszString ) > kMaxUTF8CodePointLength )
			{
				V_UnicodeTruncate( pProperty->m_strValue.Access(), kMaxUTF8CodePointLength );
				pProperty->m_strValue.Append( "..." );
			}

			pvecProperties->AddToTail( pProperty );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle key commands on this label
//-----------------------------------------------------------------------------
bool CLabel::OnKeyDown( const KeyData_t &code )
{
	if ( m_bAllowTextSelection )
	{
		switch ( code.m_KeyCode )
		{
			case KEY_ESCAPE:
				if ( BHasSelection() )
				{
					ClearSelection();
					return true;
				}
				break;

			case KEY_A:
				if ( IsControlShortcutPressed( code.m_Modifiers ) )
				{
					if ( m_bAllowTextSelection )
					{
						SelectAll();
						return true;
					}
				}
				break;

			case KEY_C:
				if ( IsControlShortcutPressed( code.m_Modifiers ) )
				{
					if ( BHasSelection() )
					{
						CopySelectionToClipboard();
						return true;
					}
				}
				break;
		}
	}

	return BaseClass::OnKeyDown( code );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CLabel::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	BaseClass::ValidateClientPanel( validator, pchName );

	ValidateObj( m_vecSelectionRects );

	ValidateObj( m_vecTextRangeFormats );
	FOR_EACH_VEC( m_vecTextRangeFormats, i )
	{
		ValidateObj( m_vecTextRangeFormats[i] );
	}

	ValidateLinkVector( validator, m_pvecParsedHREFs );
	ValidateLinkVector( validator, m_pvecParsedMouseOvers );
	ValidateLinkVector( validator, m_pvecParsedMouseOuts );
	ValidateLinkVector( validator, m_pvecParsedContextMenus );
}


//-----------------------------------------------------------------------------
// Purpose: validate mem for a vector of links
//-----------------------------------------------------------------------------
void CLabel::ValidateLinkVector( CValidator &validator, CUtlVector< CUtlString > *pvecLinks )
{
	ValidatePtr( pvecLinks );
	if ( pvecLinks )
	{
		FOR_EACH_VEC( ( *pvecLinks ), i )
		{
			ValidateObj( pvecLinks->Element( i ) );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CLabel::TextRangeFormat_t::Validate( CValidator &validator, const tchar *pchName )
{
	if ( m_pStyle )
		m_pStyle->Validate( validator, pchName );

	ValidateObj( m_vecBoundingBoxes );
	ValidateObj( m_strChildID );
	ValidateObj( m_vecClasses );
}
#endif

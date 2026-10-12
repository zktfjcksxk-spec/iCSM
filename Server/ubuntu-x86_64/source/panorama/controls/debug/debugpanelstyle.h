//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef DEBUGPANELSTYLEPANEL_H
#define DEBUGPANELSTYLEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/iuiengine.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/label.h"
#include "../../renderer/styles.h"

DECLARE_PANORAMA_EVENT1( DebugStyleStatus, bool );			// bool is true if updated, false otherwise

namespace panorama
{

class CTextEntry;
class CDebugIndividualStyle;
class CDebugStyleAnimation;
class CDebugStyleBlock;
class CDebugAutoComplete;

//-----------------------------------------------------------------------------
// Purpose: Panel for debugging applied panel styles
//-----------------------------------------------------------------------------
class CDebugPanelStyle : public CPanel2D
{
	DECLARE_PANEL2D( CDebugPanelStyle, CPanel2D );

public:
	CDebugPanelStyle( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugPanelStyle();

	void Build();
	bool OnSetDebugTarget( CPanelPtr< CPanel2D > pPanel );
	bool OnPanelStyleChanged( const CPanelPtr< IUIPanel > &pPanel );

private:
	struct FontProperty_t
	{
		bool BAnySet() { return (family || size || style || weight); }

		bool family;
		bool size;
		bool weight;
		bool style;
	};

	bool SetStyleInfo( CUtlBuffer *pBuffer, CDebugStyleBlock *pStyleBlock );
	bool SetAnimationInfo( CUtlBuffer *pBuffer, CDebugStyleAnimation *pAnimation );
	void AppendElementStyles();
	void AppendCascadeStyles();
	void AppendInheritedStyles();
	void AppendAnimations();
	void AppendStyleInfoForProperty( CPanel2D *pPanel, const CUtlVector< CStyleSymbol > &vecProperties, FontProperty_t fontProperties );

	CPanelPtr< CPanel2D > m_pDebugPanel;
};


//-----------------------------------------------------------------------------
// Purpose: Contains properties for a style
//-----------------------------------------------------------------------------
class CDebugStyleBlock : public CPanel2D, public CDefaultInputCapture
{
	DECLARE_PANEL2D( CDebugStyleBlock, CPanel2D );

public:
	CDebugStyleBlock( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugStyleBlock();
	
	void SetTabDepth( uint unDepth ) { m_unTabDepth = unDepth; }
	void SetDebugInfo( CPanoramaSymbol symLayoutFile, CPanoramaSymbol symStylePath, uint unFileOrder ) { m_symLayoutFile = symLayoutFile; m_symStylePath = symStylePath; m_unFileOrder = unFileOrder; }
	void SetSelector( const char *pchSelector );

	void AddProperty( const char *pchName, const char *pchValue );
	void AddComment( const char *pchComment );
	void AddEmptyLine();

	void OnlyShowProperties( const CUtlVector< CStyleSymbol > &vecProperties );

	bool BContainsErrors() { return (m_vecRowsWithErrors.Count() > 0); }
	void GetText( CFmtStrMax *pfmt );
	void FocusSelector();

	CPanel2D *GetPropertySection() { return m_pPropertySection; }

	// IInputCapture
	virtual bool OnCapturedKeyDown( IUIPanel *pPanel, const KeyData_t &code );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

private:
	CPanel2D *CreateRow( const char *pchName, const char *pchValue );
	CPanel2D *CreateNewRowAfter( CPanel2D *pRow );
	void DeleteRow( CPanel2D *pRow );
	void ClearPropertySection();
	void RemoveInputHooksFromRow( CPanel2D *pRow );	
	void GetStyleInfo( CUtlString *pstr, CPanel2D *pPanel );
	bool AddStyleInfo( CUtlString *pstr, CUtlBuffer *pBuffer, uint unFileLocation );
	void PopulateNameSuggestions( CTextEntry *pName );
	void PopulateValueSuggestions( CTextEntry *pNameEntry, CTextEntry *pTextEntry );
	void SetPropertyTooltip( CPanel2D *pRow, const char *pchDescription );
	void SetPropertyError( CPanel2D *pRow, const char *pchError );
	void ClearPropertyError( CPanel2D *pRow );
	bool TextEntryUpdated( CPanel2D *pPanel );
	void SetRowCommentOrEmpty( CPanel2D *pRow, bool bEnabled );

	// events
	bool EventTextEntrySubmit( const CPanelPtr< IUIPanel > &pPanel, const char *pchText );
	bool EventTextEntryChanged( const CPanelPtr< IUIPanel > &pPanel );
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventFocusLost( const CPanelPtr< IUIPanel > &pPanel );	

	CPanoramaSymbol m_symLayoutFile;
	CPanoramaSymbol m_symStylePath;
	uint m_unFileOrder;
	uint m_unTabDepth;
	bool m_bDirty;

	CPanelPtr< CDebugAutoComplete > m_pContextMenuName;
	CPanelPtr< CDebugAutoComplete > m_pContextMenuValue;
	CUtlVector< CPanel2D * > m_vecRowsWithErrors;
	CTextEntry *m_pSelector;
	CPanel2D *m_pPropertySection;
};


//-----------------------------------------------------------------------------
// Purpose: Panel for displaying a style
//-----------------------------------------------------------------------------
class CDebugIndividualStyle : public CPanel2D
{
	DECLARE_PANEL2D( CDebugIndividualStyle, CPanel2D );	

public:
	CDebugIndividualStyle( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugIndividualStyle();

	void Init( CPanoramaSymbol symLayoutFile, const CUtlVector< CPanoramaSymbol > &vecStylePath, uint unFileLocation, uint unFileOrder );
	CDebugStyleBlock *GetStyleBlock() { return m_pStyleBlock; }

private:
	void UpdateInMemoryFile();
	bool EventInMemoryFileUpdate( CPanoramaSymbol symFile, uint unLocation, uint unOldSize, uint unNewSize );
	bool EventUpdateStyleInMemory();

	CPanoramaSymbol m_symStylePath;
	uint m_unFileLocation;

	CLabel *m_pStyleLink;
	CDebugStyleBlock *m_pStyleBlock;
};


//-----------------------------------------------------------------------------
// Purpose: Panel for displaying a style
//-----------------------------------------------------------------------------
class CDebugStyleAnimation : public CPanel2D
{
	DECLARE_PANEL2D( CDebugStyleAnimation, CPanel2D );

public:
	CDebugStyleAnimation( CPanel2D *pParent, const char *pchName );
	virtual ~CDebugStyleAnimation();

	void Init( CPanoramaSymbol symLayoutFile, CPanoramaSymbol symStylePath, uint unFileLocation, uint unFileOrder );
	CDebugStyleBlock *AddFrame();
	void SetName( const char *pchName );

	CPanoramaSymbol GetStyleFile() const { return m_symStylePath; }
	uint GetFileLocation() const { return m_unFileLocation; }

private:
	void UpdateInMemoryFile();
	bool EventInMemoryFileUpdate( CPanoramaSymbol symFile, uint unLocation, uint unOldSize, uint unNewSize );
	bool EventUpdateStyleInMemory();
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

	CPanoramaSymbol m_symLayoutFile;
	CPanoramaSymbol m_symStylePath;
	uint m_unFileOrder;
	uint m_unFileLocation;

	CLabel *m_pStyleLink;
	CTextEntry *m_pName;
	CPanel2D *m_pFrames;
};


//-----------------------------------------------------------------------------
// Purpose: Header separating properties inherited from parents
//-----------------------------------------------------------------------------
class CDebugInheritedStylesHeader : public CPanel2D
{
	DECLARE_PANEL2D( CDebugInheritedStylesHeader, CPanel2D );

public:
	CDebugInheritedStylesHeader( CPanel2D *pParent, const char *pchName );
	void Init( CPanel2D *pPanel );
};


//-----------------------------------------------------------------------------
// Purpose: Simple separator
//-----------------------------------------------------------------------------
class CDebugStyleSeparator : public CPanel2D
{
	DECLARE_PANEL2D( CDebugStyleSeparator, CPanel2D );

public:
	CDebugStyleSeparator( CPanel2D *pParent, const char *pchName );
	void Init( const char *pchText );
};

} // namespace panorama

#endif // DEBUGPANELSTYLEPANEL_H
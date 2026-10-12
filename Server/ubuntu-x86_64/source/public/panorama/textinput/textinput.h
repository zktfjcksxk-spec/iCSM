//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_H
#define PANORAMA_TEXTINPUT_H

#if defined(_WIN32) || defined(SOURCE2_PANORAMA)
#pragma once
#endif

#include "panorama/controls/panel2d.h"
#include "panorama/input/iuiinput.h"

namespace panorama
{

class ITextInputControl;
class CTextInputHandlerSettings;

// When the handler is up and sees gamepad right-stick input, it passes it through to the containing
// control, so the control can do something with it. Dispatched to ITextInputControl::GetAssociatedPanel()
DECLARE_PANEL_EVENT1( TextInputAnalogStickPassthrough, GamePadData_t );

DECLARE_PANEL_EVENT1( TextInputSent, char const * );

DECLARE_PANORAMA_EVENT1( TextInputUnhandledButtonPress, GamePadData_t );

panorama::ETextInputHandlerType_t ETextInputHandlerType_tFromName( const char *pchName );
const char *PchNameFromETextInputHandlerType_t( int eType );

//
// Input submodes for text input handlers, used also by textentry
//
enum ETextInputMode_t
{
	k_ETextInputModeNormal,
	k_ETextInputModeNormalLower,
	k_ETextInputModePassword,
	k_ETextInputModeEmail,
	k_ETextInputModeNumeric,
	k_ETextInputModeNumericPassword,
	k_ETextInputModeURL,
	k_ETextInputModeSteamCode,
	k_ETextInputModePhoneNumber,
	k_ETextInputModeSubmit,
};

static const uint k_iPositionSlots = 4;

ETextInputMode_t ETextInputMode_tFromName( const char *pchName );
const char *PchNameFromETextInputMode_t( int eMode );

//-----------------------------------------------------------------------------
// Purpose: an interface that the text input uses to feed and be fed text
//-----------------------------------------------------------------------------
class ITextInputControl
{
public:
	virtual ~ITextInputControl() {}

	virtual bool OnKeyDown( const KeyData_t &code ) = 0;
	virtual bool OnKeyUp( const KeyData_t & code ) = 0;
	virtual bool OnKeyTyped( const KeyData_t &unichar ) = 0;
	
	// return true if you own the backing store of the text and can return it immediately on request,
	// 	false otherwise (html returns false here) 
	virtual bool BSupportsImmediateTextReturn() = 0;

	virtual int32 GetCursorOffset() const = 0;
	virtual uint GetCharCount() const = 0;

	virtual const char *PchGetText() const = 0;
	virtual const uchar32 *Pch32GetText() const = 0;

	virtual void InsertCharacterAtCursor( const uchar32 &unichar ) = 0;
	virtual void InsertCharactersAtCursor( const uchar32 *pch32, size_t cch32 ) = 0;

	virtual CPanel2D *GetAssociatedPanel() = 0;

	virtual void OnTextInputHandlerOpened( class CTextInputHandler *pHandler ) = 0;

	// request string the control now contains
	virtual void RequestControlString() = 0;
};


//-----------------------------------------------------------------------------
// Purpose: any suggestion entries need to implement this class
//-----------------------------------------------------------------------------
class CSuggestionPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSuggestionPanel, panorama::CPanel2D );
public:
	CSuggestionPanel( panorama::CPanel2D *parent, const char *pchID ) : panorama::CPanel2D( parent, pchID ) {};
	virtual const char *PchGetSuggestionTitle() = 0;
};


//-----------------------------------------------------------------------------
// Purpose: The interface over a text input handler. Derives from CPanel2D
//			for convenience.
//-----------------------------------------------------------------------------
class CTextInputHandler : public panorama::CPanel2D
{
public:
	CTextInputHandler( panorama::IUIWindow *pParent, const char *pchID );
	CTextInputHandler( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CTextInputHandler();

	void OnOpenHandler();
	void CloseHandler( bool bCommitText );
	void ApplyDockingPosition( uint unCurrentPosition ); // k_iPositionSlots

	virtual ITextInputControl *GetControlInterface() = 0;
	virtual void SuggestWord( const uchar32 *pch32, int ich ) = 0;

	// If SetSuggestionPanels returns false, it is not accepting ownership of these panels - calling code must handle them
	virtual bool SetSuggestionPanels( const CUtlVector<CSuggestionPanel *>& vecPanels ) = 0;

protected:
	virtual void CloseHandlerImpl( bool bCommitText ) = 0;
};

// Factory methods
CTextInputHandler *CreateTextInputHandler( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl );
CTextInputHandler *CreateTextInputHandler( panorama::CPanel2D *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl );

} // namespace panorama

#endif // PANORAMA_TEXTINPUT_H


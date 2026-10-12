//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/textinput/textinput.h"
#include "panorama/textinput/textinput_daisywheel.h"
#include "panorama/textinput/textinput_dualtouch.h"
#include "panorama/controls/label.h"
#include "panorama/controls/textentry.h"
#include "panorama/input/gamepadcodes.h"
#include "panorama/iuiengine.h"
#include "panorama/panoramacurves.h"
#ifdef SOURCE2_PANORAMA
#include "enumutils_panorama.h"
#else
#include "enumutils.h"
#endif
#include "utlradixtrie.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

namespace panorama
{

	DEFINE_PANORAMA_EVENT( TextInputAnalogStickPassthrough );
	DEFINE_PANORAMA_EVENT( TextInputSent );
	DEFINE_PANORAMA_EVENT( TextInputUnhandledButtonPress );


	//-----------------------------------------------------------------------------
	// Purpose: Helper to convert between string and ETextInputMode_t
	//-----------------------------------------------------------------------------
	ENUMSTRINGS_START( ETextInputMode_t )
	{
		k_ETextInputModeNormal, "normal"
	},
	{ k_ETextInputModeNormalLower, "lowercase" },
	{ k_ETextInputModePassword, "password" },
	{ k_ETextInputModeEmail, "email" },
	{ k_ETextInputModeNumeric, "numeric" },
	{ k_ETextInputModeNumericPassword, "numericpassword" },
	{ k_ETextInputModeURL, "url" },
	{ k_ETextInputModeSteamCode, "steamcode" },
	{ k_ETextInputModePhoneNumber, "phonenumber" },
	ENUMSTRINGS_REVERSE( ETextInputMode_t, k_ETextInputModeNormal )


	//-----------------------------------------------------------------------------
	// Purpose: Helper to convert between string and ETextInputHandlerType_t
	//-----------------------------------------------------------------------------
	ENUMSTRINGS_START( ETextInputHandlerType_t )
		{
			k_ETextInputHandlerType_DaisyWheel, "daisywheel"
		},
		{ k_ETextInputHandlerType_DualTouch, "dualtouch" },
		ENUMSTRINGS_REVERSE( ETextInputHandlerType_t, k_ETextInputHandlerType_DaisyWheel )

//-----------------------------------------------------------------------------
// Purpose: Create a text input handler parented to a whatever
//-----------------------------------------------------------------------------
template < typename TParentType >
static CTextInputHandler *AllocateTextInputHandlerImpl( TParentType *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl )
{
#if !defined( SOURCE2_PANORAMA )
	ETextInputHandlerType_t eInputHandlerType = UIEngine()->UISettings()
											  ? UIEngine()->UISettings()->GetActiveTextInputHandlerType()
											  : k_ETextInputHandlerTypeDefault;

	//
	// Always use daisywheel/pinpad for numeric password input (PINs)
	//
	if ( settings.GetMode() == k_ETextInputModeNumericPassword )
	{
		eInputHandlerType = k_ETextInputHandlerType_DaisyWheel;
	}

	switch ( eInputHandlerType )
	{
	case k_ETextInputHandlerType_DaisyWheel:
		return new CTextInputDaisyWheel( pParent, settings, pControl );
	case k_ETextInputHandlerType_DualTouch:
		return new CTextInputDualTouch( pParent, settings, pControl );
	}
#else
	return new CTextInputDaisyWheel( pParent, settings, pControl );
#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < typename TParentType >
static CTextInputHandler *CreateTextInputHandlerImpl( TParentType *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl )
{
	CTextInputHandler *pHandler = AllocateTextInputHandlerImpl( pParent, settings, pControl );

	pControl->OnTextInputHandlerOpened( pHandler );
	DispatchEvent( TextInputHandlerStateChange(), pHandler, true );

	return pHandler;
}


//-----------------------------------------------------------------------------
// Purpose: Create a text input handler parented to a top level window
//-----------------------------------------------------------------------------
CTextInputHandler *CreateTextInputHandler( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl )
{
	return CreateTextInputHandlerImpl( pParent, settings, pControl );
}


//-----------------------------------------------------------------------------
// Purpose: Create a text input handler parented to a panel
//-----------------------------------------------------------------------------
CTextInputHandler *CreateTextInputHandler( panorama::CPanel2D *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pControl )
{
	return CreateTextInputHandlerImpl( pParent, settings, pControl );
}


} // namespace panorama

using namespace panorama;
REGISTER_PANEL2D( CSuggestionPanel, SuggestionPanel );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextInputHandler::CTextInputHandler( panorama::IUIWindow *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextInputHandler::CTextInputHandler( panorama::CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextInputHandler::~CTextInputHandler()
{
	// make sure we always fire a hide event not matter how we dismiss
	panorama::DispatchEvent( TextInputHandlerStateChange(), this, false );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextInputHandler::CloseHandler( bool bCommitText )
{
	panorama::DispatchEvent( TextEntryHideTextInputHandler(), GetControlInterface()->GetAssociatedPanel() );

	CloseHandlerImpl( bCommitText );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextInputHandler::ApplyDockingPosition( uint unCurrentPosition )
{
	unCurrentPosition = unCurrentPosition % k_iPositionSlots;

	const bool bPositions[k_iPositionSlots][2] =
	{
		{ true,  true },  // top left
		{ false, true},   // top right
		{ false, false }, // bottom right
		{ true,  false }  // bottom left
	};

	bool bDockedLeft = bPositions[unCurrentPosition][0];
	bool bDockedTop = bPositions[unCurrentPosition][1];

	if( bDockedLeft )
	{
		AddClass( "DockLeft" );
		RemoveClass( "DockRight" );
	}
	else
	{
		AddClass( "DockRight" );
		RemoveClass( "DockLeft" );
	}

	if( bDockedTop )
	{
		AddClass( "DockTop" );
		RemoveClass( "DockBottom" );
	}
	else
	{
		RemoveClass( "DockTop" );
		AddClass( "DockBottom" );
	}
}


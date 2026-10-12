//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: QWERTY keyboard text entry method for Steam controller
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_DUALTOUCH_H
#define PANORAMA_TEXTINPUT_DUALTOUCH_H

#if defined(_WIN32) || defined(SOURCE2_PANORAMA)
#pragma once
#endif

#include "panorama/textinput/textinput.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/label.h"
#include "panorama/controls/touchpad.h"
#include "panorama/input/iuiinput.h"
#include "mathlib/beziercurve.h"
#include "tier1/utlptr.h"
#include "tier1/utlstack.h"
#include "panorama/uischeduleddel.h"

namespace panorama
{
	
	// Forward declaration
	class CTextInputDualTouch;
	class CTextEntry;
	class ITextInputSuggest;
	class CLabel;
	
	//-----------------------------------------------------------------------------
	// Purpose: main dual touch UI class
	//-----------------------------------------------------------------------------
	class CTextInputDualTouch : public panorama::CTextInputHandler
	{
		DECLARE_PANEL2D( CTextInputDualTouch, panorama::CPanel2D );
		
	public:
		// Constructor
		CTextInputDualTouch( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );
		CTextInputDualTouch( panorama::CPanel2D *parent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );
		
		// Destructor
		~CTextInputDualTouch();
		
		// CTextInputHandler overrides
		virtual void CloseHandlerImpl( bool bCommitText ) OVERRIDE;
		virtual ITextInputControl *GetControlInterface() OVERRIDE;

		void SubmitTextNoClose( void );

		virtual void SuggestWord( const uchar32 *pch32, int ich ) OVERRIDE { }

		// If SetSuggestionPanels returns false, it is not accepting ownership of these panels - calling code must handle them
		virtual bool SetSuggestionPanels( const CUtlVector<CSuggestionPanel *>& vecPanels ) OVERRIDE;
		
		static void GetSupportedLanguages( CUtlVector<ELanguage> &vecLangs );

		enum EDualtouchSuggestionMode
		{
			k_EDualtouchSuggestionMode_WithTypoCorrection,
			k_EDualtouchSuggestionMode_NoTypoCorrection,
			k_EDualtouchSuggestionMode_NoSuggestions,
			k_EDualtouchSuggestionModeCount
		};

	private:
		static const int k_DualTouchRowCount = 4;
		static const int k_DualTouchColumnCount = 11;
		static const int k_SuggestionCount = 4;
		
		enum EDualTouchModifier_t
		{
			k_EDualTouchModifierNone = 0,
			k_EDualTouchModifierShift = 1,
			k_EDualTouchModifierAlt = 2,
			k_EDualTouchModifierCount = 3,
		};

		void Initialize( const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl );
		void SetMode( ETextInputMode_t mode );
		
		// CPanel2D overrides
		virtual bool OnGamePadUp( const panorama::GamePadData_t &code ) OVERRIDE;
		virtual bool OnGamePadDown( const panorama::GamePadData_t &code ) OVERRIDE;
		virtual bool OnGamePadAnalog( const panorama::GamePadData_t &code ) OVERRIDE;
		virtual bool OnKeyTyped( const KeyData_t &unichar ) OVERRIDE;
		virtual bool OnKeyDown( const KeyData_t &code ) OVERRIDE;
		virtual bool OnKeyUp( const KeyData_t &code ) OVERRIDE;
		
	private:
		bool EventTextEntryChanged( const CPanelPtr< IUIPanel > &pPanel );

		void TogglePasswordVisibility();
		void UpdateSteamPadHardwarePointers( bool bSteamPadHardwarePointersEnabled );
		void UpdateSteamPadSoftwarePointerImage( uint32 unTextureID );
		void UpdateSteamPadHardwarePointerVisibility();
		bool OnPropertyTransitionEnd( const CPanelPtr< IUIPanel > &pPanel, CStyleSymbol prop );
		bool OnTouchKeyStyleChanged( CPanelPtr<CPanel2D> pPanel, const char *pszStyle, bool bAddedStyle );
		bool OnImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage );
		bool OnPanelStyleChanged( const CPanelPtr< IUIPanel > &pPanel );
		
		bool TouchPadClicked( CTouchPad* pTouchPad );
		
		// Listen for focus lost
		bool HandleInputFocusLost( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel );
		bool OnActiveControllerTypeChanged( EActiveControllerType eActiveControllerType );
		bool EventInputFocusTopLevelChanged( CPanelPtr< IUIPanel > ptrPanel );

		bool TypeCharacters( const char *pszUTF8 );			// return whether we inserting this codepoint makes us want to flush our suggestion state, if any
		void TypeSpace() { const char *pszSpace = " "; TypeCharacters( &pszSpace[0] ); }
		
		bool SwitchLanguage( void );
		bool LoadInputConfigurationFile( ELanguage language );
		bool LoadInputConfigurationFile( char const *szConfigFile, const char *szConfigRootDir );
		bool LoadConfigurationBuffer( char const *pszIncoming );
		
		void SetModifierKeyState( EDualTouchModifier_t modifier, bool bIsButtonPressEvent );
		EDualTouchModifier_t CalculateDesiredModifierState() const;
		void ApplyCurrentModifierLayout();
		
		bool OnTouchKeyClicked( CPanel2D *pTouchKey, CTouchPad *pTouchPad );
		void OnStandardTouchKeyClicked( CPanel2D *pTouchKey, CTouchPad *pTouchPad );
		
		//	Process scheduled key repeat
		void ScheduleKeyRepeats( panorama::GamePadCode eCode );
		void CancelOutstandingRepeats() { ScheduleKeyRepeats( XK_NULL ); }
		
		void ScheduledKeyRepeatFunction();
		
		// auto-suggestion
		void ResetSuggestionState();
		void OnSuggestionSelected( int iSuggestion );
		void UpdateSuggestionWords();
		void UpdateTextPreview();
		
		void PerformBackspace( void );
		
		void CursorMove( const panorama::GamePadData_t &code );
		void DisableCursorMode( void );

		void InitEmoticons( void );
		void PopulateEmoticonsForCurrentPage( void );
		void SetEmoticonMode( bool bActive );
		void EmoticonPageLeft( void );
		void EmoticonPageRight( void );
		void OnEmoticonClicked( CPanel2D *pTouchKey );

#ifdef DBGFLAG_VALIDATE
		virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE
		{
			VALIDATE_SCOPE();
			ValidateObj( m_repeatFunction );
		}
#endif
	private:
		void ChangeTouchkeyStyle( CPanel2D *pTouchKey, const char *pchStyle, bool bAddStyle );
	
	public:
		ITextInputControl *m_pTextInputControl; // control interface for moving text input between a control and daisy wheel
		
		CPanel2D *m_pBodyContainer;
		CPanel2D *m_pBackDrop;
		
		CLabel *m_pLang;
		
		CLabel *m_pSuggestionLabels[k_SuggestionCount];

		// As we're typing characters, we're keeping track of things that we think might be typos, like you hit "h" but
		// really you were so close to "j" that maybe that's what you were aiming at. We then build up lists of these
		// potential word roots, so you could have, for example: "h" (you were right in the middle of H), then "hi", but
		// you were on the edge of the key so maybe you meant "ho", then "hin" and "hon" when you hit N, and on and on.
		// We then use all of these roots to make ask the suggestion engine for candidate words, with two caveats:
		//
		//		- we don't want to store all "word candidates" that can't lead to any words. We know that if we start
		//		  a word with "zzz" we're never going to get any valid suggestion results, no matter what letters we
		//		  put at the end. The search space grows very quickly -- three characters for twelve letters is over half
		//		  a million word roots, the overwhelming majority of which can't contribute a valid suggestion.
		//
		//		- we have to support backspacing, where we delete whatever the last letter is, but keep track of other
		//		  word roots that got us to there, even if at the point where we were there were no candidate words
		//		  remaining. For example, if we do "aardvsk", we'll throw out "aardvs" because no words start with that,
		//		  but we still want to remember it in case we backspace all the way to "aardv". Rather than delete the
		//		  end character and then collapse identical strings we store each string length as its own bucket. Doing
		//		  this lets us map backspace to "just throw out the longest bucket entirely and back up a step".
		//
		// We model this as a list of lists. The outer vector stores the candidates for a specific character length (slot
		// 0 is all the of one-letter words that have potential candidates (*), slot 3 is all the four-letter roots, etc.)
		// and the inner vector stores the candidates themselves, including typo possibilities.
		//
		// (*: we *always* store a candidate for whatever characters the user actually hit, even if it doesn't lead to
		// any words we know of. This means every first-vector entry always has at least sub-entry.)
		typedef CUtlVector<CUtlString> VecCandidateWordRoots_t;
		CUtlStack<VecCandidateWordRoots_t *> m_PossibleWordsBeingTyped;
		
		ELanguage m_language;						// currently loaded language
		
		ITextInputSuggest *m_pSuggest;				// suggestion engine
		
		CTextEntry *m_pTextPreview;
		
		ETextInputMode_t m_mode;
		EDualtouchSuggestionMode m_eSuggestionMode;
		
		// This controls the direct-rendered steampad crosshairs; we can only
		// have them up while we're not animating around, otherwise use higher-latency
		// panel crosshairs
		IImageSource *m_pSteamPadPointerImage;
		
		CTouchPad m_leftTouchPad;
		CTouchPad m_rightTouchPad;
		
		CUtlVector< CTouchPad * > m_vecTouchPads;

		CUtlVector< CSuggestionPanel * > m_vecCustomSuggestionPanels;
		
		uchar32 m_keyLayout[k_DualTouchColumnCount][k_DualTouchRowCount][k_EDualTouchModifierCount];
		EDualTouchModifier_t m_currentModifier;
		int m_iCharactersTypedSinceModifierStateChanged;
		
		// Tracking key repeats
		CCubicBezierCurve< Vector2D > m_repeatCurve;	// Curve for key repeats
		double m_repeatStartTime;					// Time when the key was initially pressed
		double m_repeatNextTime;						// Time when the key will repeat next
		panorama::GamePadCode m_repeatGamePadCode;	// Which key was pressed (low level, for key-up tracking)
		uint32 m_repeatCounter;						// How many key repeats have happened
		panorama::CUIScheduledDel m_repeatFunction;	// Scheduled function triggering key repeats
		
		bool m_bCursorMode;
		CPanelPtr<CPanel2D> m_pCursorKey;
		bool m_bUseTouchPads;

		bool m_bModifierKeysHeld[ k_EDualTouchModifierCount ];
		
		bool m_bHardwareCursorsEnabled;
		bool m_bOverlayMode;

		bool m_bShowEmoticons;	// was the keyboard opened in a way that we should show emoticons at all?
		bool m_bEmoticonMode;	// are we currently showing emoticon selection
		int m_nEmoticonPage;
		int m_nMaxEmoticonsPerPage;
		CUtlMap< int, int > m_mapEmoticonTouchKeys;
	};
	
} // namespace panorama

#endif // PANORAMA_TEXTINPUT_DUALTOUCH_H

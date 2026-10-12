//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Public header for panorama UI framework
//
//
//=============================================================================//
#ifndef PANORAMATYPES_H
#define PANORAMATYPES_H
#pragma once

#ifdef POSIX
#include <float.h>
#endif
#include "mathlib/vector.h"
#include "panorama/panoramasymbol.h"

#if _GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined( SOURCE2_PANORAMA )
#include "../thirdparty/v8/include/v8.h"
#else
#include "../external/v8/include/v8.h"
#endif
#if _GNUC
#pragma GCC diagnostic pop
#endif

#ifdef SOURCE2_PANORAMA
#include "tier1/mempool.h"
#include "tier1/checksum_sha1.h"

class ISceneView;
class IRenderContext;
class ISceneLayer;
typedef void (RenderCallbackFunction_t)( ISceneView *, IRenderContext **, ISceneLayer *, float, float, float, float );

#endif

#if !defined( SOURCE2_PANORAMA )
#define VPROF_BUDGET_THREAD VPROF_BUDGET
#else
#define VPROF_BUDGET_THREAD( name, group ) ((void)0)

const uint32 k_cubSHA1Hash = k_cubHash;

#ifdef PANORAMA_USE_S1WRAPPER

#ifndef SAFE_DELETE
#define SAFE_DELETE( x ) if ( (x) != NULL ) { delete (x); x = NULL; }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE( x ) if ( NULL != ( x ) ) { ( x )->Release(); x = NULL; }
#endif

#endif

#ifndef AssertFatalMsg2
#define AssertFatalMsg2( b, msg, p1, p2 ) AssertFatalMsg( b, msg, p1, p2 )
#define AssertFatalMsg1( b, msg, p1 ) AssertFatalMsg( b, msg, p1 )
#endif 
#define WeakRandomFloat( minval, maxval ) RandomFloat( minval, maxval )

template <class T, class P>
inline void ConstructOneArg( T* pMemory, P const& arg )
{
	HINT( pMemory != 0 );
	::new(pMemory)T( arg );
}

template <class T, class P, class P2 >
inline void ConstructTwoArg( T* pMemory, P const& arg1, P2 const &arg2 )
{
	HINT( pMemory != 0 );
	::new(pMemory)T( arg1, arg2 );
}

template <class T, class P, class P2, class P3, class P4, class P5, class P6, class P7 >
inline void ConstructSevenArg( T* pMemory, P const& arg1, P2 const &arg2, P3 const &arg3, P4 const &arg4, P5 const &arg5, P6 const &arg6, P7 const &arg7 )
{
	HINT( pMemory != 0 );
	::new(pMemory)T( arg1, arg2, arg3, arg4, arg5, arg6, arg7 );
}
#endif


namespace panorama
{

class IUIPanelClient;


enum BlurType_t
{
	BT_NORMAL = 0,
	BT_FAST = 1,
	BT_FASTANIM = 2
};


//-----------------------------------------------------------------------------
// Purpose: Text/font related types
//-----------------------------------------------------------------------------
enum EFontWeight : int8
{
	k_EFontWeightUnset = -1,
	k_EFontWeightNormal = 0,
	k_EFontWeightMedium = 1,
	k_EFontWeightBold = 2,
	k_EFontWeightBlack = 3,
	k_EFontWeightThin = 4,
	k_EFontWeightLight = 5,
	k_EFontWeightSemiBold = 6,
};

enum EFontStyle : int8
{
	k_EFontStyleUnset = -1,
	k_EFontStyleNormal = 0,
	k_EFontStyleItalic = 2,
};

enum ETextAlign : int8
{
	k_ETextAlignUnset = -1,
	k_ETextAlignLeft = 0,
	k_ETextAlignCenter = 1,
	k_ETextAlignRight = 2
};

enum ETextDecoration : int8
{
	k_ETextDecorationUnset = -1,
	k_ETextDecorationNone = 0,
	k_ETextDecorationUnderline = 1,
	k_ETextDecorationLineThrough = 2,
};

enum ETextTransform : int8
{
	k_ETextTransformUnset = -1,
	k_ETextTransformNone = 0,
	k_ETextTransformUppercase = 1,
	k_ETextTransformLowercase = 2,
};

enum ETextOverflow : int8
{
	k_ETextOverflowUnset = -1,
	k_ETextOverflowClip = 0,
	k_ETextOverflowEllipsis = 1,
	k_ETextOverflowShrink = 2,
	k_ETextOverflowNoClip = 3,
};


//-----------------------------------------------------------------------------
// Purpose: Transform related types
//-----------------------------------------------------------------------------
enum ETransform3DType : uint8
{
	k_ETransform3DRotate,
	k_ETransform3DTranslate,
	k_ETransform3DScale
};


//-----------------------------------------------------------------------------
// Purpose: Transition timing functions
//-----------------------------------------------------------------------------
enum EAnimationTimingFunction : uint8
{
	k_EAnimationNone = 0, // Indicates that there is no transition set, do not animate.  Don't change from zero!
	k_EAnimationEase,
	k_EAnimationEaseIn,
	k_EAnimationEaseOut,
	k_EAnimationEaseInOut,
	k_EAnimationLinear,
	k_EAnimationCustomBezier,

	k_EAnimationUnset,
	// if you add another, update BParseTimingFunction()
};


//-----------------------------------------------------------------------------
// Purpose: Possible animation directions
//-----------------------------------------------------------------------------
enum EAnimationDirection : uint8
{
	k_EAnimationDirectionUnset = 0,
	k_EAnimationDirectionNormal,
	k_EAnimationDirectionAlternate,
	k_EAnimationDirectionReverse,
	k_EAnimationDirectionAlternateReverse,

	// if you add another, update BParseAnimationDirectionFunction()
};


//-----------------------------------------------------------------------------
// Purpose: Possible animation fill modes
//-----------------------------------------------------------------------------
enum EAnimationFillMode : uint8
{
	k_EAnimationFillModeNone = 0,
	k_EAnimationFillModeForwards,
	k_EAnimationFillModeBackwards,
	k_EAnimationFillModeBoth,

	// if you add another, update BParseAnimationFillModeFunction()
};


//-----------------------------------------------------------------------------
// Purpose: Supported style psuedo classes
//-----------------------------------------------------------------------------
enum EStyleFlagBit : uint8
{
	k_EStyleFlagMinBit = 0,

	k_EStyleFlagHoverBit = k_EStyleFlagMinBit,
	k_EStyleFlagFocusBit,
	k_EStyleFlagActiveBit,
	k_EStyleFlagDisabledBit,
	k_EStyleFlagEnabledBit,
	k_EStyleFlagInspectBit,
	k_EStyleFlagSelectedBit,
	k_EStyleFlagDescendantFocusedBit,
	k_EStyleFlagParentDisabledBit,
	k_EStyleFlagLayoutLoadingBit,
	k_EStyleFlagLayoutLoadFailedBit,
	k_EStyleFlagActivationDisabledBit,

	k_EStyleFlagMaxBit
};

enum EStyleFlags : uint16
{
	k_EStyleFlagNone = 0,
	k_EStyleFlagHover = 1 << k_EStyleFlagHoverBit,							// mouse is over the panel
	k_EStyleFlagFocus = 1 << k_EStyleFlagFocusBit,							// panel has keyboard focus
	k_EStyleFlagActive = 1 << k_EStyleFlagActiveBit,						// panel is being actively used (mouse-down on the panel)
	k_EStyleFlagDisabled = 1 << k_EStyleFlagDisabledBit,					// panel is disabled
	k_EStyleFlagEnabled = 1 << k_EStyleFlagEnabledBit,						// panel is enabled. Not actually set on the panel itself, but can be set on a selector
	k_EStyleFlagInspect = 1 << k_EStyleFlagInspectBit,						// panel is being inspected by the debugger
	k_EStyleFlagSelected = 1 << k_EStyleFlagSelectedBit,					// panel is selected (button checked)
	k_EStyleFlagDescendantFocused = 1 << k_EStyleFlagDescendantFocusedBit,	// a descendant of the panel has keyboard focus
	k_EStyleFlagParentDisabled = 1 << k_EStyleFlagParentDisabledBit,		// a parent of this panel is disabled, thus implicitly disabling it
	k_EStyleFlagLayoutLoading = 1 << k_EStyleFlagLayoutLoadingBit,			// panel is in-progress loading it's layout file and thus may want to display specially
	k_EStyleFlagLayoutLoadFailed = 1 << k_EStyleFlagLayoutLoadFailedBit,	// panel failed to load requested layout file, and thus may want to display specially
	k_EStyleFlagActivationDisabled = 1 << k_EStyleFlagActivationDisabledBit, // panel is disabled for activation, may still be enabled for focus (normal disabled disallows all input/focus)
};


//-----------------------------------------------------------------------------
// Purpose: Supported border styles
//-----------------------------------------------------------------------------
enum EBorderStyle : int8
{
	k_EBorderStyleUnset = -1,
	k_EBorderStyleNone = 0,
	k_EBorderStyleSolid = 1,
};


//-----------------------------------------------------------------------------
// Purpose: Horizontal alignments
//-----------------------------------------------------------------------------
enum EHorizontalAlignment : uint8
{
	k_EHorizontalAlignmentUnset,
	k_EHorizontalAlignmentLeft,
	k_EHorizontalAlignmentCenter,
	k_EHorizontalAlignmentRight,
	k_EHorizontalAlignmentCenterNoPixelSnap
};


//-----------------------------------------------------------------------------
// Purpose: Vertical alignments
//-----------------------------------------------------------------------------
enum EVerticalAlignment : uint8
{
	k_EVerticalAlignmentUnset,
	k_EVerticalAlignmentTop,
	k_EVerticalAlignmentCenter,
	k_EVerticalAlignmentBottom,
	k_EVerticalAlignmentCenterNoPixelSnap
};

//-----------------------------------------------------------------------------
// Purpose: ContextUI (tooltips + context menus) position
//-----------------------------------------------------------------------------
enum EContextUIPosition : uint8
{
	k_EContextUIPositionUnset,
	k_EContextUIPositionLeft,
	k_EContextUIPositionTop,
	k_EContextUIPositionRight,
	k_EContextUIPositionBottom,
};

//-----------------------------------------------------------------------------
// Purpose: Possible flow directions
//-----------------------------------------------------------------------------
enum EBackgroundRepeat : uint8
{
	k_EBackgroundRepeatUnset,
	k_EBackgroundRepeatRepeat,
	k_EBackgroundRepeatSpace,
	k_EBackgroundRepeatRound,
	k_EBackgroundRepeatNo,
};


//-----------------------------------------------------------------------------
// Purpose: Special words for background sizes when not length or percentage
//-----------------------------------------------------------------------------
enum EBackgroundSizeConstant : uint8
{
	k_EBackgroundSizeConstantNone,
	k_EBackgroundSizeConstantContain,
	k_EBackgroundSizeConstantCover,
	k_EBackgroundSizeConstantClipThenCover
};


//-----------------------------------------------------------------------------
// Purpose: type of repaint to do for a render target/panel
//-----------------------------------------------------------------------------
enum EPanelRepaint : uint8
{
	k_EPanelRepaintFull,
	k_EPanelRepaintComposition,
	k_EPanelRepaintNone
};


//-----------------------------------------------------------------------------
// Purpose: Specifies what category a sound is; each one of these maps to a volume control in the UI
//-----------------------------------------------------------------------------
enum ESoundType : uint8
{
	k_ESoundType_Ambient,			// ambient sounds, intro movie
	k_ESoundType_Movies,			// movies in store
	k_ESoundType_Effects,			// daisywheel, navigation
	k_ESoundType_Passthrough,		// special sound type for volume-changing UI - bypasses volume controls
};


//-----------------------------------------------------------------------------
// Purpose: Text Input handler types
//-----------------------------------------------------------------------------
enum ETextInputHandlerType_t : uint8
{
	k_ETextInputHandlerType_DaisyWheel,
	k_ETextInputHandlerType_DualTouch,

	k_ETextInputHandlerTypeDefault = k_ETextInputHandlerType_DualTouch,		// if we fail at gathering the information to figure out which is the most appropriate input method, fall back to this one
};


//-----------------------------------------------------------------------------
// Purpose: Possible flow directions
//-----------------------------------------------------------------------------
enum EFlowDirection : uint8
{
	k_EFlowUnset,
	k_EFlowNone,
	k_EFlowDown,
	k_EFlowRight,
	k_EFlowUp,
	k_EFlowLeft,
	k_EFlowDownWrap,
	k_EFlowRightWrap,
	k_EFlowUpWrap,
	k_EFlowLeftWrap
};


//-----------------------------------------------------------------------------
// Purpose: Possible overflow settings
//-----------------------------------------------------------------------------
enum EOverflowValue : uint8
{
	k_EOverflowSquish,
	k_EOverflowClip,
	k_EOverflowScroll,
	k_EOverflowNoClip,
};


enum EMouseCanActivate : uint8
{
	k_EMouseCanActivateUnfocused = 0,
	k_EMouseCanActivateIfFocused,
	k_EMouseCanActivateIfParentFocused,
	k_EMouseCanActivateIfAnyParentFocused
};

// Focus navigation
enum EFocusMoveDirection : uint8
{
	k_ENextInTabOrder = ( 1 << 0 ),
	k_EPrevInTabOrder = ( 1 << 1 ),
	k_ENextByXPosition = ( 1 << 2 ),
	k_EPrevByXPosition = ( 1 << 3 ),
	k_ENextByYPosition = ( 1 << 4 ),
	k_EPrevByYPosition = ( 1 << 5 )
};


enum ELoadLayoutAsyncDetails : uint8
{
	k_ELoadLayoutAsyncDetailsNone,
	k_ELoadLayoutAsyncDetailsNotLoggedIn
};

enum EMixBlendMode : uint8
{
	k_EMixBlendModeNormal = 0,
	k_EMixBlendModeMultiply,
	k_EMixBlendModeScreen,
	k_EMixBlendModeAdditive,
	k_EMixBlendModeAdditiveSRGB,
	k_EMixBlendModeOpaque
};

enum EFractionalPixelPositions : uint8
{
	k_EFractionalPixelPositionsDefault,
	k_EFractionalPixelPositionsClamp,
	k_EFractionalPixelPositionsNoClamp,
};


enum ETextureSampleMode : uint8
{
	k_ETextureSampleModeNormal,
	k_ETextureSampleModeAlphaOnly,
};


enum ERenderCallbackFlags : uint8
{
	k_ERenderCallbackFlagsDefault       = 0x00000000,
	// The callback will take care of any clearing of the render target.
	k_ERenderCallbackFlagsManualClear   = 0x00000001,
	// The panel should always be drawn.
	k_ERenderCallbackFlagsAlwaysRepaint = 0x00000002,
	// The panel will call SetRepaint to be drawn.
	k_ERenderCallbackFlagsManualRepaint = 0x00000004,
	// Render target mode - The user provided a render target texture that
	// will be drawn to the layer's surface (as opposed to callback mode 
	// cf CSource2Surface::RequestRenderCallback)
	k_ERenderCallbackFlagsRenderTargetMode = 0x00000008,
	// Containing composition layer requires a depth buffer
	k_ERenderCallbackFlagsNeedsDepth       = 0x00000010,
};


//-----------------------------------------------------------------------------
// Purpose: Enum for the parts of layout a style invalidates when applied
//-----------------------------------------------------------------------------
enum EStyleInvalidateLayout : uint8
{
	k_EStyleInvalidateLayoutNone,
	k_EStyleInvalidateLayoutSizeAndPosition,
	k_EStyleInvalidateLayoutPosition
};


enum EStyleRepaint : uint8
{
	k_EStyleRepaintFull,
	k_EStyleRepaintComposition,
	k_EStyleRepaintNone,
};


enum EAntialiasing : uint8
{
	k_EAntialiasingNone,
	k_EAntialisingEnabled
};

enum EOverlayWindowAlignment : uint8
{
	k_EOverlayWindowAlignment_FullscreenLetterboxed = 1,
	k_EOverlayWindowAlignment_FullscreenNoLetterBox = 2,
	k_EOverlayWindowAlignment_BottomRight = 3,
};

enum EStylePresentFlags : uint32
{
	k_EStylePresentTransformMatrix = 1 << 0,
	k_EStylePresentPerspective = 1 << 1,
	k_EStylePresentPerspectiveOrigin = 1 << 2,
	k_EStylePresentOpacity = 1 << 3,
	k_EStylePresentWashColor = 1 << 4,
	k_EStylePresentHueShift = 1 << 5,
	k_EStylePresentSaturation = 1 << 6,
	k_EStylePresentBrightness = 1 << 7,
	k_EStylePresentContrast = 1 << 8,
	k_EStylePresentBlur = 1 << 9,
	k_EStylePresentBorderRadius = 1 << 10,
	k_EStylePresentOpacityMaskImage = 1 << 11,
	k_EStylePresentBackgroundImage = 1 << 12,
	k_EStylePresentBackgroundFillColor = 1 << 13,
	k_EStylePresentBorder = 1 << 14,
	k_EStylePresentBoxShadow = 1 << 15,
	k_EStylePresentScale2DCentered = 1 << 16,
	k_EStylePresentRotate2DCentered = 1 << 17,
	k_EStylePresentTextShadow = 1 << 18,
	k_EStylePresentImageShadow = 1 << 19,
	k_EStylePresentClip = 1 << 20,
	k_EStylePresentMixBlendMode = 1 << 21,
	k_EStylePresentBackgroundImgOpacity = 1 << 22,

};

// constants, these must remain matching
const float k_flTabIndexInvalid = -1.0 * FLT_MAX;
const float k_flSelectionPosInvalid = k_flTabIndexInvalid;
const float k_flTabIndexAuto = FLT_MAX;
const float k_flSelectionPosAuto = k_flTabIndexAuto;


//-----------------------------------------------------------------------------
// Purpose: bit flags used to control panel construction behavior
//-----------------------------------------------------------------------------
enum EPanelFlags : uint8
{
	ePanelFlags_DontAddAsChild = 0x1, // don't automatically add this panel as a child to its parent
	ePanelFlags_DontFireOnLoad = 0x2, // don't fire the onload event when constructed, useful for image panel that waits until its contents load
};


enum EPanelEventSource_t : uint8
{
	k_ePanelEventSourceProgram,
	k_ePanelEventSourceGamepad,
	k_ePanelEventSourceKeyboard,
	k_ePanelEventSourceMouse,

	k_ePanelEventSourceInvalid
};

enum EMakeUIEventType : byte
{
	k_eMakeUIEventType_NoArguments,
	k_eMakeUIEventType_Source,
	k_eMakeUIEventType_Repeats
};

enum EEventDocFlags : byte
{
	k_eEventDocFlagNone = 0,
	k_eEventDocFlagInternalOnly = 1 << 0,
};

class IUIPanel;
class IUIEvent;

typedef IUIEvent* (*PFN_ParseUIEvent)(panorama::IUIPanel *pTarget, const char *pchEvent, const char **pchEventEnd);
typedef IUIEvent* (*PFN_ParseUIEventJS)(panorama::CPanoramaSymbol symEvent, panorama::IUIPanel *pTarget, const CUtlVector< v8::Local< v8::Value > > &args);
typedef IUIEvent* (*PFN_MakeUIEvent0)(const panorama::IUIPanelClient *pTarget);
typedef IUIEvent* (*PFN_MakeUIEvent1Source)(const panorama::IUIPanelClient *pTarget, EPanelEventSource_t eSource);
typedef IUIEvent* (*PFN_MakeUIEvent1Repeats)(const panorama::IUIPanelClient *pTarget, int nRepeats);
typedef CUtlString (*PFN_FormatUIEventArgs)(const char *pchArgNames);
struct UIEventFactory
{
	int m_cParams;
	bool m_bPanelEvent;

	EMakeUIEventType m_eMakeUIEventType;
	union
	{
		PFN_MakeUIEvent0 m_pfnMakeUIEvent0;
		PFN_MakeUIEvent1Repeats m_pfnMakeUIEvent1Repeats;
		PFN_MakeUIEvent1Source m_pfnMakeUIEvent1Source;
	};

	PFN_ParseUIEvent m_pfnParseUIEvent;
	PFN_ParseUIEventJS m_pfnParseUIEventJS;			// parsing function for events defined in javascript
	PFN_FormatUIEventArgs m_pfnFormatUIEventArgs;

	const char *m_pchDocumentationArgs;
	const char *m_pchDocumentationDescription;
	EEventDocFlags m_eDocFlags;
};


enum EWindowFocusBehavior : uint8
{
	// By default, controls in this window will take focus whenever they're interacted with.
	k_EWindowFocusBehavior_Default,

	// By default, clicking on controls in this window will avoid taking focus unless it's necessary.
	// Useful if you only plan on supporting keyboard focus in a few specific spots and otherwise
	// don't display keyboard focus.
	k_EWindowFocusBehavior_Shy,
};


struct SteamPadPointer_t
{
	bool bVisible;
	Vector2D vecCenter;
	float flRadius;
	float flOpacity;
	uint32 nTextureID;
	int iControllerID;
	float (* funcPreRenderCalculatePadOffset)( float, bool );
};

} // namespace panorama


#if !defined( SOURCE2_PANORAMA )
template< class T, int A >
class CUtlVectorFixedGrowable : public CUtlVector< T >
{
public:
	CUtlVectorFixedGrowable() 
	{
		this->EnsureCapacity( A );
	}

	virtual ~CUtlVectorFixedGrowable() { }
};
#endif

#endif // PANORAMATYPES_H

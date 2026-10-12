/**********************************************************************

Filename    :   GFxIMEIdMap.h
Content     :   Contains declarations for classes needed for Registry Query. 
Created     :   Jun 17, 2008
Authors     :   Ankur Mohan

Notes       :   
History     :   

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**********************************************************************/

#ifndef _GFX_IMEIDMAP_H_
#define _GFX_IMEIDMAP_H_

#include "imemanager_plat.h"
#include <stdio.h>

#define MAX_KEY_LENGTH	1024

struct TF_INPUTPROCESSORPROFILE;

/*
The piece of code below is borrowed from IMEUI.cpp in DXSDK. It's used to hide some IME UI windows that 
don't follow any other method of hiding IME windows (setting lParam = 0 while processing WM_IMESETCONTEXT,
not passing ime messages to DefWndProc). The key ideas are as follows:

1) IME Windows are hidden by suppressing WM_IMENOTIFY messages when lParam = IMN_PRIVATE and wParam = some 
magic numbers that are totally undocumented.`

2) Different IME's use different magic numbers

3) Different versions of the same IME uses different magic numbers

4) The version number of the current ime is obtained in GetIMEId() by first obtaining the IMEFileName
and then using API functions to get the version number. I load the version.dll to avoid having to link with
version.lib in the player

*/
#define LANG_CHT			MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL )
#define LANG_CHS			MAKELANGID( LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED )

#define MAKEIMEVERSION( major, minor ) ( (DWORD)( ( (BYTE)( major ) << 24 ) | ( (BYTE)( minor ) << 16 ) ) )

#define IMEID_CHT_VER42		( LANG_CHT | MAKEIMEVERSION( 4, 2 ) )	// New(Phonetic/ChanJie)IME98  : 4.2.x.x // Win98
#define IMEID_CHT_VER43		( LANG_CHT | MAKEIMEVERSION( 4, 3 ) )	// New(Phonetic/ChanJie)IME98a : 4.3.x.x // Win2k
#define IMEID_CHT_VER44		( LANG_CHT | MAKEIMEVERSION( 4, 4 ) )	// New ChanJie IME98b          : 4.4.x.x // WinXP
#define IMEID_CHT_VER50		( LANG_CHT | MAKEIMEVERSION( 5, 0 ) )	// New(Phonetic/ChanJie)IME5.0 : 5.0.x.x // WinME
#define IMEID_CHT_VER51		( LANG_CHT | MAKEIMEVERSION( 5, 1 ) )	// New(Phonetic/ChanJie)IME5.1 : 5.1.x.x // IME2002(w/OfficeXP)
#define IMEID_CHT_VER52		( LANG_CHT | MAKEIMEVERSION( 5, 2 ) )	// New(Phonetic/ChanJie)IME5.2 : 5.2.x.x // IME2002a(w/WinXP)
#define IMEID_CHT_VER60		( LANG_CHT | MAKEIMEVERSION( 6, 0 ) )	// New(Phonetic/ChanJie)IME6.0 : 6.0.x.x // New IME 6.0(web download)
#define IMEID_CHT_VER_VISTA ( LANG_CHT | MAKEIMEVERSION( 7, 0 ) )	// All TSF TIP under Cicero UI-less mode: a hack to make GetImeId() return non-zero value

// We use both Layout Text and IME File fields in KeyBoard Layout Key of the registry
// to identify supported IME's. For some IME's the registry doesn't contain the IME File 
// field. This has to be thoroughly tested. Microsoft Pinyin IME for example leads to 
// different keyboardlayoutname on Chinese XP vs English XP. The registry key corresponding
// to the keyboardlayoutname on ChineseXP doesn't have a IME File field.

enum GFxIMETag
{
    GFxIME_English					= 0x10000000,

    GFxIME_Ch_Trad_Phonetic			= 0x00000001,
    GFxIME_Ch_Trad_NewPhonetic		= 0x00000002,
    GFxIME_Ch_Trad_ChangJie			= 0x00000003,
    GFxIME_Ch_Trad_NewChangJie		= 0x00000004,
    GFxIME_Ch_Trad_DaYi				= 0x00000005,
    GFxIME_Ch_Trad_Array			= 0x00000006,
	GFxIME_Ch_Trad_Quick			= 0x00000007,
	GFxIME_Ch_Trad_NewQuick			= 0x00000008,
    GFxIME_Ch_Trad					= 0x00000009,
	GFxIME_Ch_Trad_NewChangJie2010	= 0x0000000A,
	GFxIME_Ch_Trad_NewPhonetic2010	= 0x0000000B,
	GFxIME_Ch_Trad_NewQuick2010		= 0x0000000C,

    GFxIME_Jp_2002					= 0x00000100,
    GFxIME_Jp_2007					= 0x00000200,
    GFxIME_Jp_2003					= 0x00000300,
    GFxIME_Jp						= 0x00000400,
	GFxIME_Jp_2010					= 0x00000500,
    GFxIME_Jp_ATOK2008				= 0x00D00800,
	GFxIME_Jp_ATOK2009				= 0x00D00900,
	GFxIME_Jp_GOOG2010				= 0x00D00A00,

    GFxIME_Kr_2000					= 0x00001000,
    GFxIME_Kr						= 0x00002000,
	GFxIME_Kr_2003					= 0x00003000,
	GFxIME_Kr_8_6001				= 0x00004000,
	GFxIME_Kr_2007					= 0x00005000,
	GFxIME_Kr_2002					= 0x00006000,
	GFxIME_Kr_2010					= 0x00007000,

    GFxIME_Ch_Simp_QuanPin			= 0x00010000,
    GFxIME_Ch_Simp_ShuangPin		= 0x00020000,
    GFxIME_Ch_Simp_ZhengMa			= 0x00030000,
    GFxIME_Ch_Simp_MSPinyin_3_0		= 0x00040000,
    GFxIME_Ch_Simp_MSPinyin			= 0x00050000,
    GFxIME_Ch_Simp_ABC				= 0x00060000,
    GFxIME_Ch_Simp					= 0x00080000,
    GFxIME_Ch_Simp_MSPinyin_2007	= 0x00070000,
	GFxIME_Ch_Simp_MSPinyin1_2010	= 0x00090000,
	GFxIME_Ch_Simp_MSPinyin2_2010	= 0x000A0000,

    GFxIME_GooglePinyin				= 0x00110000, // Both third party and simplified.
    GFxIME_SogouPinyin				= 0x00210000,
    GFxIME_Ch_Simp_Pinyin03			= 0x00410000,
    GFxIME_Ch_Simp_QQPinyin			= 0x00810000,
    GFxIME_Ch_Simp_NianQing			= 0x00910000,
    GFxIME_Ch_Simp_WuBi86			= 0x00A10000,
    GFxIME_Ch_Simp_WuBi98			= 0x00B10000,
    GFxIME_Ch_Simp_JJ				= 0x00C10000,
	GFxIME_BaiduPinyin				= 0x00D10000,
    // Both third party and traditional.
	GFxIME_Ch_Trad_NewChewing		= 0x00D00002,
	GFxIME_Ch_Trad_WuXia			= 0x00D00001,

    GFxIME_NotSupported				= 0x01000000,
    GFxIME_NotSet					= 0x02000000,
	GFxIME_DoesntExist				= 0x03000000 // For languages that don't have IMEs.
};

enum GFxIMETagFlags
{
    GFxIME_En_Flag					= 0x10000000,
    GFxIME_Ch_Trad_Flag				= 0x0000000F,
    GFxIME_Jp_Flag					= 0x00000F00,
    GFxIME_Kr_Flag					= 0x0000F000,
    GFxIME_Ch_Simp_Flag				= 0x000F0000,
    GFxIME_Ch_ThirdParty_Flag		= 0x00F00000,
    GFxIME_NotSupported_Flag		= 0x0F000000
};

struct StringToTagDef
{
    wchar_t		*m_pString;
    GFxIMETag  Tag;
};

struct TSFProfileNode
{
	TSFProfileNode()
	{
		m_pTSFProfile = NULL;
		m_pNext = NULL;
	}

	TF_INPUTPROCESSORPROFILE	*m_pTSFProfile;
	TSFProfileNode				*m_pNext;
};

struct InputLangTable
{
    const char		*m_pItemNameOnSystem;	// The name of the entry (input language/ime) on the system
    const char		*m_pItemNameCommon;		// Common name of the entry     

    GFxIMETag		ItemTag;
    uint32			m_nUnused;
};

// This structure defines the properties of Installed Input languages and IME's.
struct InputLangProperties
{
	InputLangProperties() :
		ItemTag( GFxIME_NotSet ), 
		Id( 0 )
	{
	}

	CUtlWString		m_ItemNameOnSystem;	// The name of the entry (input language/ime) on the system
    CUtlWString		m_ItemNameCommon;	// Common name of the entry     

    GFxIMETag		ItemTag;
    intp			Id;                 // can be HKL or LANGID
};

enum UsesWhichRegistryType
{
	USES_UNKNOWN = 0,
	USES_LAYOUT_TEXT = 1,
	USES_IME_FILENAME = 2,
};

struct InputLangProps2
{
	InputLangProps2():
		ItemTag( GFxIME_NotSet ), 
		Id( 0 ),
		m_pTSFProfileNode( NULL ),
		m_UsesIMEFileNameOrLayoutText( USES_UNKNOWN )
	{
	}

	CUtlWString		m_ItemNameOnSystem;	// The name of the entry (input language/ime) on the system
	CUtlWString		m_ItemNameCommon;	// Common name of the entry     

	GFxIMETag		ItemTag;

	intp				Id;						// can be HKL         
	TSFProfileNode		*m_pTSFProfileNode;		// Setup when using TSF

	// This records IME file name or layout text registry fields was used to identify the IME.
	UsesWhichRegistryType m_UsesIMEFileNameOrLayoutText;
};

// This structure defines the properties of Installed Input languages and IME's.
struct HKLLayoutProperties
{
	HKLLayoutProperties():
		m_HKL_As32Bit( 0 ), 
		m_bIsInstalled( false )
	{
	}

    CUtlWString		m_ImeFileName;
    CUtlWString		m_LayoutName; 

    uint32			m_HKL_As32Bit;
    bool			m_bIsInstalled;
};

// This structure is used to store a mapping between locale/lang id's and input language names.
struct InputLangHKLMap
{
    GFxIMETag   IMETag;
    size_t      LangId;		// Initially set to zero
};

class CIMENamesManager;
class GFxIMEWin32Impl;

union HKL_Size_t_Union
{
	HKL     m_HKL;
	size_t  m_Val;
};

class GFxIMEManagerWin32;

class CIMENamesManager
{      
public:
    CIMENamesManager( GFxIMEManagerWin32 *pIMEManagerWin32 ); 

    virtual ~CIMENamesManager()
    {
        CleanUp();
    }

    // We try to distinguish the functions used for querying IME names from the registry against those used to support the 
    // language bar/status window functionality. This is so that the language bar functions can be easily ifdef'd out.
    virtual bool            QualifyIMENames();
    void                    CleanUp();
    int						GetNumSupportedIMEs() { return m_SupportedIMEs.Count(); }
    virtual GFxIMETag       GetIMETag();
    virtual GFxIMETag       GetInputLangTagFromIMETag( GFxIMETag imeTag );
    virtual GFxIMETag       GetLangTagFromLangId( uint32 langId );

    virtual bool            MakeKeyboardLayoutListFromRegistry( HKL *pHKLs, int nNumInstalledIMEs );

    // Called when WM_INPUTLANGCHANGE is received. This can happen if the user clicks on our language bar
    // or the windows language bar. 
    virtual void            OnInputLangChange(DWORD langId);

    // When we switch to a new input language, a default IME is automatically selected by the system.
    // this function sets the text in the IMEName field of the language bar to this default IME.
    // It also displays the status window if it needs to be displayed.
    void                    SetLastIMEName( const wchar_t *pIMEName );

    void                    HideReadingWindow();

    int                     GetIMEId();

	void					SetupForSupportedInputLanguage( LANGID langID, const wchar_t *pInputLangName );

    virtual void            ActivateIME( const wchar_t *pIMEName ) { NOTE_UNUSED( pIMEName ); };
    // Instructs the system to switch to the requested input language
    virtual void            ActivateInputLanguage( const wchar_t *pInputLangName) { NOTE_UNUSED( pInputLangName ); };
    
    // Set's the conversion mode
    virtual void            SetConversionMode(uint32 conversionParams = -1);
    // Handles user input to the status window. Instructs the system to change the conversion mode
    // of the IME under use accordingly.
    virtual void            HandleStatusWindowNotifications(const char* pcommand, const char* parg) 
                            {NOTE_UNUSED2(pcommand, parg);} ;
    
    // Called when "LangBar_OnInit" is received from the Langbar movie 
    virtual void            OnLangBarLoaded();

	int						CheckForSupportedIME( const wchar_t *pLayoutTextName, const wchar_t *pImeFileName);
	
	void					PopulateSupportedIMEs();

	bool					IsAnyIMESupported();

	CON_COMMAND_MEMBER_F( CIMENamesManager, "ime_supported_info", IME_SpewSupportedInfo_f, "Spew IME Supported info.", FCVAR_DONTRECORD );
	CON_COMMAND_MEMBER_F( CIMENamesManager, "ime_hkl_info", IME_SpewHKLInfo_f, "Spew IME HKL info.", FCVAR_DONTRECORD );

    CUtlVector<CUtlWString>	m_JapaneseIMEs;
    CUtlVector<CUtlWString>	m_KoreanIMEs;
    CUtlVector<CUtlWString>	m_ChineseSimpIMEs;
    CUtlVector<CUtlWString>	m_ChineseTradIMEs;

    GFxIMEManagerWin32*		m_pIMEManagerWin32;

    GFxIMETag					m_CurrentInputLangTag;
	GFxIMETag					m_CurrentIMETag;

    CUtlVector< InputLangProperties >	m_SupportedInputLanguages;
	CUtlVector< InputLangProps2 >		m_SupportedIMEs;

	bool	JapaneseIMEState;

protected:
    uint32                  m_fdwConversion;

    CUtlVector< HKLLayoutProperties >	m_HKLLayoutTextMap;

    int                     m_nIMEVersionId;
};

extern StringToTagDef g_StringToTagTable[];

#endif

/**************************************************************************

Filename    :   GFx_IMENamesManagerXP.cpp
Content     :   Overrides the functions used to obtain names of installed ime's and 
                activating ime's according to platform specific implementation.
Created     :   Oct 01, 2008
Authors     :   Ankur Mohan

Notes       :   First part of this file contains the function implementations for 
                querying the registry for IME names and checking which IME's are supported
                The second part contains the functions used for implementing the language bar.
History     :   

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "imemanager.h"

#include "tier0/platform_com.h"

#include "gfx_imenamesmanagerxp.h"
#include "gfx_imemanagerwin32.h"

CIMENamesManagerXP::CIMENamesManagerXP( GFxIMEManagerWin32 *pIMEManagerWin32 ) : CIMENamesManager( pIMEManagerWin32 )
{
    NOTE_UNUSED( pIMEManagerWin32 );
}
  
CIMENamesManagerXP::~CIMENamesManagerXP() 
{
	CleanUp();
}

void CIMENamesManagerXP::CleanUp()
{
    m_HKLLayoutTextMap.Purge();
	m_SupportedInputLanguages.Purge();
}

bool CIMENamesManagerXP::QualifyIMENames()
{
	m_CurrentIMETag = GFxIME_NotSupported;
	m_CurrentInputLangTag = GFxIME_NotSupported;

	// Get the quantity of HKLs
	int nNumInstalledIMEs = GetKeyboardLayoutList( 0, 0 );
	if ( !nNumInstalledIMEs )
	{
		Log_Warning( LOG_IME, "No IME's Installed!\n" );
		return false;
	}

	// Get a list of installed Keyboard layouts. This gives us a list of HKLS for IME's installed on the system.
	HKL *pHKLs = new HKL[nNumInstalledIMEs];
	GetKeyboardLayoutList( nNumInstalledIMEs, pHKLs );

	HKL_Size_t_Union HKLUnion;
	for ( int i = 0; i < nNumInstalledIMEs; i++ )
	{
		// This is needed since for 64 bit builds, the last 8 bytes are not filled out correctly. 
		HKLUnion.m_HKL = pHKLs[i];
		HKLUnion.m_Val = HKLUnion.m_Val & 0xFFFFFFFF;
		pHKLs[i] = HKLUnion.m_HKL;
	}

	// The above method doesn't give us the ime names though. Using GetKeyboardLayoutName doesn't help
	// The only way (according to Michael Kaplan's blog) is to look into the registry for the hkls
	bool bRet = CIMENamesManager::MakeKeyboardLayoutListFromRegistry( pHKLs, nNumInstalledIMEs );

	delete [] pHKLs;

	if ( !bRet ) 
	{
		// Some problem with querying the registry- abort.
		return false; 
	}

	for ( int i = 0; i < m_SupportedIMEs.Count(); i++ )
	{
		m_SupportedIMEs[i].m_UsesIMEFileNameOrLayoutText = USES_UNKNOWN;
	}

    PlatCOMFunctions_t *pCOM = Plat_RequireLoadCOM();
    
	// Create the object.
	ITfInputProcessorProfiles *pProfiles = NULL;
	HRESULT hr = pCOM->pCoCreateInstance( CLSID_TF_InputProcessorProfiles, 
		NULL, 
		CLSCTX_INPROC_SERVER, 
		IID_ITfInputProcessorProfiles, 
		(LPVOID*)&pProfiles);
	if ( !SUCCEEDED(hr) )
	{
		// Not sure if we should record an error message here..
		Log_Warning( LOG_IME, "IME Error: Failure to Initialize InputProcessorProfiles! (%d)\n", hr );
		return false;
	}

    PlatOleAutFunctions_t *pOleAut = Plat_RequireLoadOleAut();
    
	// InputProcessorProfileMgr seems not available on Windows XP. So need to use 
	// InputProcessorProfiles.
	//  hr = pProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr_GFx, (LPVOID*)&pProfilesMgr);
	//	AM (2 April 09): Note: I just discovered that there exist XP systems on which trying to obtain ITfInputProcessorProfiles can
	//	fail. If that happens, we need to have an alternative way of obtaining installed input languages. 

	if ( SUCCEEDED( hr ) && pProfiles )
	{
		LANGID *pLangIDs = NULL;
		ULONG nLanguageListCount = 0;
		hr = pProfiles->GetLanguageList( &pLangIDs, &nLanguageListCount );
		if ( SUCCEEDED( hr ) )
		{
			Log_Detailed( LOG_IME, "Discovered %d installed languages.\n", nLanguageListCount );

			for ( ULONG nLanguage = 0; nLanguage < nLanguageListCount; nLanguage++ )
			{
				wchar_t wszLangName[MAX_PATH];
				if ( 0 != GetLocaleInfoW( MAKELCID( pLangIDs[nLanguage], SORT_DEFAULT ), LOCALE_SLANGUAGE, wszLangName, V_ARRAYSIZE( wszLangName ) ) )
				{
					IEnumTfLanguageProfiles *ppEnum = NULL;
					pProfiles->EnumLanguageProfiles( pLangIDs[nLanguage], &ppEnum );

					GUID guid;
					V_memset( &guid, 0, sizeof( guid ) );

					TF_LANGUAGEPROFILE langProfile;
					V_memset( &langProfile, 0, sizeof( langProfile ) );
					langProfile.catid = guid;
					langProfile.clsid = guid; 
					langProfile.guidProfile = guid; 
					langProfile.langid = pLangIDs[nLanguage];

					// Some languages don't have IME's. For example English. Even though they might
					// have "language profiles" for example- speech to text conversion and other text 
					// services. In our SupportedIMEs list, we only have language names and IME names.
					// Hence, for languages such as English, we must call CheckForSupportedIME outside
					// of the EnumProfile loop, so that we can create an entry for it in order to switch 
					// the input language to English. 

					// Also, note that only a few ime's register themselves as language profiles as listed
					// by EnumLanguageProfiles(). For most of the others (including all of the third party ime's) 
					// we have to go through the list of hkl's in the registry as done later.

					SetupForSupportedInputLanguage( langProfile.langid, wszLangName ); 

					// This only works with registering system IME's unfortunately. 
					// Currently deciding to rely solely on registry lookup. 

					BSTR pProfileDescription = pOleAut->pSysAllocString( NULL );
					if ( ppEnum )
					{
						while ( ppEnum->Next( 1, &langProfile, NULL) != S_FALSE )
						{
							pProfiles->GetLanguageProfileDescription( langProfile.clsid, langProfile.langid, langProfile.guidProfile, &pProfileDescription );

							if ( pOleAut->pSysStringLen( pProfileDescription ) != 0 )
							{
								pOleAut->pSysFreeString( pProfileDescription );
								pProfileDescription = NULL;
							}
						}

						ppEnum->Release();
					}
				}
			}
            pCOM->pCoTaskMemFree( pLangIDs );
		}
	}
	else 
	{
		// This is the alternative way: The idea is to look at the installed hkls in the HklLayoutTextMap and 
		// mask out the higher 16 bits (that correspond to the IME/LayoutName). We match these against the known
		// values for the languages we support. The ones that match are added to the SupportedInputLanguages array.
		// Note that we need to make sure that each inputlanguage is only added once since each input language can
		// have multiple IMEs. Also, if languages other than English, Chinese (simp), Chinese (trad), Korean, Jap
		// are installed, they will not be detected using this method (while they will be using the inputprocessorprofiles
		// based method used above). Right now this doesn't seem like a big deal.

		// Use alternative method to enumerate installed input languages. 
		for ( int i = 0; i < m_HKLLayoutTextMap.Count(); i++)
		{
			if (m_HKLLayoutTextMap[i].m_bIsInstalled)
			{
				InputLangProperties langProperties;

				langProperties.ItemTag = GFxIME_NotSupported;
				wchar_t *pString = NULL;

				if ((m_HKLLayoutTextMap[i].m_HKL_As32Bit & 0x00000FFF) == 0x409)
				{
					langProperties.ItemTag = GFxIME_English;
					pString = L"English";	
				}

				if ((m_HKLLayoutTextMap[i].m_HKL_As32Bit & 0x00000FFF) == 0x404)
				{
					langProperties.ItemTag = GFxIME_Ch_Trad;
					pString = L"Chinese (Traditional)";
				}

				if ((m_HKLLayoutTextMap[i].m_HKL_As32Bit & 0x00000FFF) == 0x804)
				{
					langProperties.ItemTag = GFxIME_Ch_Simp;
					pString = L"Chinese (Simplified)";
				}

				if ((m_HKLLayoutTextMap[i].m_HKL_As32Bit & 0x00000FFF) == 0x412)
				{
					langProperties.ItemTag = GFxIME_Kr;
					pString = L"Korean";
				}

				if ((m_HKLLayoutTextMap[i].m_HKL_As32Bit & 0x00000FFF) == 0x411)
				{
					langProperties.ItemTag = GFxIME_Jp;
					pString = L"Japanese";
				}

				if (langProperties.ItemTag != GFxIME_NotSupported)
				{
					bool hasBeenAdded = false;
					// Check if this input language has already been added
					for (int j = 0; j < m_SupportedInputLanguages.Count(); j++)
					{
						if (m_SupportedInputLanguages[j].ItemTag == langProperties.ItemTag)
						{
							hasBeenAdded = true;
							break;
						}
					}
					if (hasBeenAdded) 
						continue;

					langProperties.Id = ( m_HKLLayoutTextMap[i].m_HKL_As32Bit & 0x00000FFF );
					langProperties.m_ItemNameOnSystem = pString;

					m_SupportedInputLanguages.AddToTail( langProperties );
				}
			}
		}
	}

	// Below we are going to match the imeName and LayoutName info in the HklLayoutTextMap against
	// what's stored in the SupportedIMEs array. Note that on XP this is done differently than on Vista.
	// On Vista, the system provides a list of IMe's for each input language, while on XP, we obtain the list
	// of input languages (using the two methods described above) first and then make a list of supported IMEs.
	// The two are done seperately and it's not possible for the LayoutNames (as far as I can tell) to be the same
	// so the extra check for the currentinputlanguage (check implementation of CheckForSupportedIMEs in NamesManager)
	// is not needed.

	HKL currentKbLayout = GetKeyboardLayout( 0 );

	// Now compare what we found in the system registry with the supported IME's and fill in the HKL values,
	// so we can switch to a desired language/IME from our language bar. 
	// We first check against the layoutname and if no match is found for it, we check for the IMEFilename. 
	// Also, it's important to remember that the CurrentInputLangTag must be set correctly before 
	// CheckForSupportedIME is called. 
	for (int i = 0; i < m_HKLLayoutTextMap.Count(); i++)
	{
		if ( !m_HKLLayoutTextMap[i].m_bIsInstalled )
			continue;

		Log_Detailed( LOG_IME, "Registering IME. Layout: '%s', IME filename: '%s'\n", CTempWStringToPrintableString( m_HKLLayoutTextMap[i].m_LayoutName.Get() ).Get(), CTempWStringToPrintableString( m_HKLLayoutTextMap[i].m_ImeFileName.Get() ).Get() );

		int nSupportedIMEIndex = CheckForSupportedIME( m_HKLLayoutTextMap[i].m_LayoutName.Get(), m_HKLLayoutTextMap[i].m_ImeFileName.Get() );
		if ( nSupportedIMEIndex >= 0 )
		{
			Log_Detailed( LOG_IME, LOG_COLOR_GREEN, "   Successfully Registered (Index: %d).\n", nSupportedIMEIndex );

			m_SupportedIMEs[nSupportedIMEIndex].Id = m_HKLLayoutTextMap[i].m_HKL_As32Bit;

			HKLUnion.m_Val = m_HKLLayoutTextMap[i].m_HKL_As32Bit;
			if ( HKLUnion.m_HKL == currentKbLayout )
			{
				m_CurrentIMETag = m_SupportedIMEs[nSupportedIMEIndex].ItemTag; 
			}
		}
		else
		{
			Log_Detailed( LOG_IME, LOG_COLOR_YELLOW, "   Not Supported.\n" );
		}
	}

	// Set currently active language
	LANGID activeLangId = 0;
	if ( pProfiles )
	{
		pProfiles->GetCurrentLanguage( &activeLangId );
	}

	HKLUnion.m_HKL = GetKeyboardLayout( 0 );
	HKLUnion.m_Val = HKLUnion.m_Val & 0x00000FFF;

	m_CurrentInputLangTag = GFxIME_NotSupported;

	for ( int i = 0; i < m_SupportedInputLanguages.Count(); i++ )
	{
		if ( (size_t)m_SupportedInputLanguages[i].Id == HKLUnion.m_Val )
		{
			m_CurrentInputLangTag = m_SupportedInputLanguages[i].ItemTag; 
		}
	}

	// success
	return true;
}

int CIMENamesManagerXP::CheckForSupportedIME( const wchar_t *pLayoutTextName, const wchar_t *pIMEFileName )
{
	CUtlWString nameString;
	bool bFoundMatch = false;
	int nIndex = -1;
	GFxIMETag tag = GFxIME_NotSet;

	// First match against LayoutName field, if not match is found, try ImeFileName. 
	for ( int j = 0; j < m_SupportedIMEs.Count(); j++)
	{
		if ( pLayoutTextName && !wcsicmp( pLayoutTextName, m_SupportedIMEs[j].m_ItemNameOnSystem.Get() ) && (m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText == USES_UNKNOWN) )
		{
			nameString = m_SupportedIMEs[j].m_ItemNameOnSystem.Get();
			m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText = USES_LAYOUT_TEXT;
			bFoundMatch = true;
			nIndex = j;
			break;
		}
	}

	if ( !bFoundMatch )
	{
		for (int j = 0; j < m_SupportedIMEs.Count(); j++ )
		{
			if ( pIMEFileName && !wcsicmp( pIMEFileName, m_SupportedIMEs[j].m_ItemNameOnSystem.Get() ) )
			{
				nameString = m_SupportedIMEs[j].m_ItemNameCommon.Get();
				m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText = USES_IME_FILENAME;
				bFoundMatch = true;
				nIndex = j;
				tag = m_SupportedIMEs[j].ItemTag;
				break;
			}			
		}

		for ( int j = 0; j < m_SupportedIMEs.Count(); j++ )
		{
			if ((m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText == USES_LAYOUT_TEXT ) && (m_SupportedIMEs[j].ItemTag == tag))
			{
				bFoundMatch = false;
				break;
			}
		}
	}

	if ( bFoundMatch )
	{
		GFxIMETag inputLangTag = GetInputLangTagFromIMETag( m_SupportedIMEs[nIndex].ItemTag );

		if ( inputLangTag & GFxIME_Jp_Flag )
		{ 
			m_JapaneseIMEs.AddToTail( nameString );
		}  

		if ( inputLangTag & GFxIME_Kr_Flag )
		{ 
			m_KoreanIMEs.AddToTail( nameString );
		} 

		if ( inputLangTag & GFxIME_Ch_Simp_Flag )
		{ 
			m_ChineseSimpIMEs.AddToTail( nameString );
		} 

		if ( inputLangTag & GFxIME_Ch_Trad_Flag )
		{ 
			m_ChineseTradIMEs.AddToTail( nameString );
		}

		return nIndex;
	}

	return -1;
}

// Setting the language name is done in the derived class since we use SetLocaleInfo for XP and SetLocaleInfoEx for Vista
// as recommended in the documentation. For setting the IMEname, the base class function is called.
void CIMENamesManagerXP::OnLangBarLoaded()
{
    wchar_t wszLangName[MAX_PATH];

	HKL_Size_t_Union HKLUnion;
    HKLUnion.m_HKL = GetKeyboardLayout( 0 );
    m_CurrentInputLangTag = GetLangTagFromLangId( (uint32)( 0x0000FFFF & HKLUnion.m_Val ) );
    m_pIMEManagerWin32->SetCurrentInputLanguage(m_CurrentInputLangTag);
    if ( GetLocaleInfoW( MAKELCID( HKLUnion.m_Val, SORT_DEFAULT ), LOCALE_SLANGUAGE, wszLangName, V_ARRAYSIZE( wszLangName ) ) == 0 )
    {
        DWORD err = GetLastError();
        Log_Warning( LOG_IME, "IME Error: GetLocaleInfoW() Failed. Error Initializing Input Language Name (GetLastError() = %d)\n", err );
        m_pIMEManagerWin32->BroadcastSetCurrentInputLanguage( L"Unsupported" );
    }
    else
    {
        m_pIMEManagerWin32->BroadcastSetCurrentInputLanguage( wszLangName );

        // Here we try to distinguish between a keyboard layout and an IME
        // on XP, the HKL for IME's seem to start with an 'E'..This is not always
        // true so need to be careful here. 
        // So, if the langid is not an IME, we just set the IME name to be the same
        // as the input language name.
        if (( HKLUnion.m_Val & 0xE0000000 ) == 0 )
		{
			// Set a default supported Korean IME
            m_CurrentIMETag = GFxIME_NotSupported;
			if ( m_CurrentInputLangTag == GFxIME_Kr )
			{
				for (int i = 0; i < m_KoreanIMEs.Count(); i++)
				{
					if ( m_KoreanIMEs[i].Get() )
					{
						ActivateIME( m_KoreanIMEs[i].Get() );
						break;
					}
				}
			}
			else
			{
				int numInstalledHKls = m_HKLLayoutTextMap.Count();
				for (int i = 0; i < numInstalledHKls; i++)
				{
					if ((( HKLUnion.m_Val & 0xFFFF0000 ) >> 16 ) == m_HKLLayoutTextMap[i].m_HKL_As32Bit )
					{
						CIMENamesManager::SetLastIMEName( m_HKLLayoutTextMap[i].m_LayoutName.Get() );
						return; 
					}
				}
				CIMENamesManager::SetLastIMEName( wszLangName );
			}
        }
        else
        {
            // Set IME Name in the base class.
            CIMENamesManager::OnLangBarLoaded();
        }
    }
}

// Activates a requested IME if supported
void CIMENamesManagerXP::ActivateIME( const wchar_t *pIMEName )
{
    bool bFoundMatch = false;
    
	HKL_Size_t_Union HKLUnion;
    HKLUnion.m_Val = 0;

    for ( int j = 0; j < m_SupportedIMEs.Count(); j++ )
    {
        // check if need to match against IME file name or layout name
        if ( m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText == USES_LAYOUT_TEXT )
        {
            if ( (!wcscmp( pIMEName, m_SupportedIMEs[j].m_ItemNameOnSystem.Get() ) ) && (m_SupportedIMEs[j].Id != 0)) // The second condition precludes non-supported IMEs
            {
                bFoundMatch = true;
                HKLUnion.m_Val = m_SupportedIMEs[j].Id;
                break;
            }
        }
        else if ( m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText == USES_IME_FILENAME )
        {
            if ( ( !wcscmp( pIMEName, m_SupportedIMEs[j].m_ItemNameCommon.Get() ) ) && (m_SupportedIMEs[j].Id != 0)) 
            {
                bFoundMatch = true;
                HKLUnion.m_Val = m_SupportedIMEs[j].Id;
                break;
            }
        }
    }
            
    if ( bFoundMatch )
	{
        if ( ActivateKeyboardLayout( HKLUnion.m_HKL, 0 ) == 0 )
        {
			Log_Warning( LOG_IME, "Failure to activate input language '%s'\n", CTempWStringToPrintableString( pIMEName ).Get() );
        }
	}
}

// Activates a requested input language if supported
void CIMENamesManagerXP::ActivateInputLanguage( const wchar_t *pInputLangName )
{
    // First obtain the entry in SupportedIME's corresponding to this ime
    int nIMEIndex = -1;
    for ( int j = 0; j < m_SupportedInputLanguages.Count(); j++ )
    {
        if ( ( !wcscmp( m_SupportedInputLanguages[j].m_ItemNameOnSystem.Get(), pInputLangName ) ) && (m_SupportedInputLanguages[j].Id != 0)) // The second condition precludes non-supported IMEs
        {
            nIMEIndex = j;
            break;
        }
    }

    if ( nIMEIndex == -1 )
	{
		Log_Warning( LOG_IME, "Could not find requested IME in supported IME list!\n--> You requested: %s\n", CTempWStringToPrintableString( pInputLangName ).Get() );
		return;
	}

	HKL_Size_t_Union HKLUnion;
    HKLUnion.m_Val = m_SupportedInputLanguages[nIMEIndex].Id;
    ActivateKeyboardLayout( HKLUnion.m_HKL, 0 );
}

void CIMENamesManagerXP::HandleStatusWindowNotifications( const char* pcommand, const char* parg )
{
    DWORD   fdwConversion, fdwSentence;
    HIMC    hIMC;

    hIMC = ImmGetContext( m_pIMEManagerWin32->m_hWnd );

    ImmGetConversionStatus(hIMC, &fdwConversion, &fdwSentence);

    if (hIMC == NULL) 
		return;

    if (V_strcmp(pcommand, "StatusWindow_OnShape") == 0)
    {
        if (!V_strcmp(parg, "false"))
        {
            fdwConversion = fdwConversion & (~IME_CMODE_FULLSHAPE); 
        }

        if (!V_strcmp(parg, "true"))
        {
            fdwConversion = fdwConversion | IME_CMODE_FULLSHAPE; 
        }
    }
    else if (V_strcmp(pcommand, "StatusWindow_OnInputMode") == 0)
    {
        // Pertains to Chinese IME only
        if (!V_strcmp(parg, "false"))
        {
            fdwConversion = fdwConversion & (~IME_CMODE_NATIVE); 
        }
        else if (!V_strcmp(parg, "true"))
        {
            fdwConversion = fdwConversion | IME_CMODE_NATIVE;
        }
        // Pertains to Japanese IME.
        else if (!V_strcmp(parg, "Hiragana"))
        {
            // If in direct input mode, first turn IME on. Too bad that we have 
            // to use IMM function here. Doesn't seem to be a way to turn IME on/off
            // using TSF.
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion & (~IME_CMODE_KATAKANA); 
            fdwConversion = fdwConversion | IME_CMODE_NATIVE;
        }
        else if (!V_strcmp(parg, "Full-Width Katakana"))
        {         
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion | IME_CMODE_KATAKANA | IME_CMODE_FULLSHAPE | IME_CMODE_NATIVE; 
        }
        else if (!V_strcmp(parg, "Full-Width Alphanumeric"))
        {
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion & (~IME_CMODE_NATIVE); 
            fdwConversion = fdwConversion | IME_CMODE_FULLSHAPE; 
        }
        else if (!V_strcmp(parg, "Half-Width Katakana"))
        {
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion | IME_CMODE_KATAKANA | IME_CMODE_NATIVE;
            fdwConversion = fdwConversion & (~IME_CMODE_FULLSHAPE);
        }
		else if (!V_strcmp(parg, "Half-Width Alphanumeric"))
        {
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion & (~IME_CMODE_NATIVE);
            fdwConversion = fdwConversion & (~IME_CMODE_FULLSHAPE);
        }
        else if (!V_strcmp(parg, "DirectInput"))
        {

            if (ImmGetOpenStatus(hIMC) != 0)
            {
                ImmSetOpenStatus(hIMC, false);
            }
        }
    }
    else if (V_strcmp(pcommand, "StatusWindow_OnSymbol") == 0)
    {
        if (!V_strcmp(parg, "false"))
        {
            fdwConversion = fdwConversion & (~IME_CMODE_SYMBOL); 
        }
        else if (!V_strcmp(parg, "true"))
        {
            fdwConversion = fdwConversion | IME_CMODE_SYMBOL; 
        }
    }

    ImmSetConversionStatus(hIMC, fdwConversion, fdwSentence);
    ImmReleaseContext( m_pIMEManagerWin32->m_hWnd, hIMC );
}

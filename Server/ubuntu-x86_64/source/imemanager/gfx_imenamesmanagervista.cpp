/**********************************************************************

Filename    :   GFxIMENamesManagerVista.cpp
Content     :   Overrides the functions used to obtain names of installed ime's and 
                activating ime's according to platform specific implementation.
Created     :   Oct 01, 2008
Authors     :   Ankur Mohan

Notes       :   20/Mar/09: Must use SUCCEEDED or FAILED to test for success or failure.
History     :   

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "imemanager.h"

#include "imemanager_plat.h"
#include "tier0/platform_com.h"

#include <InitGuid.h>
#include <olectl.h>
#include "gfx_imenamesmanagervista.h"
#include "gfx_imemanagerwin32.h"

extern StringToTagDef g_StringToTagTable[];

CIMENamesManagerVista::CIMENamesManagerVista( GFxIMEManagerWin32 *pIMEManager) : CIMENamesManager( pIMEManager )
{
    NOTE_UNUSED( pIMEManager );

	m_nRef = 1;	

    m_pTFThreadMgr = NULL;
    m_pTFCompartment = NULL;
	m_dwCompartmentSinkCookie = TF_INVALID_COOKIE;
	m_dwInputProcessorProfileEventSinkCookie = TF_INVALID_COOKIE;

    HRESULT hr = Plat_RequireLoadCOM()->pCoCreateInstance(
		CLSID_TF_ThreadMgr, 
        NULL, 
        CLSCTX_INPROC_SERVER, 
        IID_ITfThreadMgr, 
        (void**)&m_pTFThreadMgr );
    if ( FAILED( hr ) )
	{
		Log_Warning( LOG_IME, "Failed to CoCreateInstance( CLSID_TF_ThreadMgr ).\n");
        return;
	}

	ITfSource *pSource = NULL;
    if ( FAILED( m_pTFThreadMgr->QueryInterface( IID_ITfSource, (void **)&pSource ) ) )
    {
		Log_Warning( LOG_IME, "Failed to QueryInterface( IID_ITfSource ).\n");
		if ( m_pTFThreadMgr )
		{
			m_pTFThreadMgr->Release();
			m_pTFThreadMgr = NULL;
		}
        goto cleanUp;
    }

    if ( FAILED( pSource->AdviseSink( IID_ITfInputProcessorProfileActivationSink_GFx, (ITfInputProcessorProfileActivationSink *)this, &m_dwInputProcessorProfileEventSinkCookie ) ) )
    {
        // make sure we don't try to Unadvise _dwThreadMgrEventSinkCookie later
        m_dwInputProcessorProfileEventSinkCookie = TF_INVALID_COOKIE;
    }

    // Get the compartment manager first and then obtain the conversion mode
    // compartment so we receive only conversion mode related notifications.
    ITfCompartmentMgr *pCompartmentMgr = NULL;
    hr = m_pTFThreadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompartmentMgr );
    if ( SUCCEEDED(hr) && pCompartmentMgr )
    {
        hr = pCompartmentMgr->GetCompartment( GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION_GFX, &m_pTFCompartment );
        if ( hr != S_OK )
        {
            Log_Warning( LOG_IME, "Failed to GetCompartment( GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION_GFX ).\n");
        }
        pCompartmentMgr->Release();
    }

    if ( !InitCompartmentSink() )
    {
        Log_Warning( LOG_IME, "Failed to create compartment sink.\n" );
    }

cleanUp:
    if ( pSource )
	{
        pSource->Release();
	}
}

CIMENamesManagerVista::~CIMENamesManagerVista()
{
	CleanUp(); 
}

STDAPI CIMENamesManagerVista::QueryInterface( REFIID riid, void **ppvObj )
{
    if (ppvObj == NULL)
        return E_INVALIDARG;

    *ppvObj = NULL;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfInputProcessorProfileActivationSink_GFx))
    {
        *ppvObj = (ITfInputProcessorProfileActivationSink *)this;
    }

    else if (IsEqualIID(riid, IID_ITfCompartmentEventSink))
    {
        *ppvObj = (ITfCompartmentEventSink *)this;
    }

    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDAPI_(ULONG) CIMENamesManagerVista::AddRef()
{
    return ++m_nRef;
}

STDAPI_(ULONG) CIMENamesManagerVista::Release()
{
    int32 nRef = --m_nRef;
    return nRef;
}

void CIMENamesManagerVista::CleanUp()
{
    for ( int i = 0; i < m_SupportedIMEs.Count(); i++ )
    {
		TSFProfileNode *pTSFProfileNode = m_SupportedIMEs[i].m_pTSFProfileNode;
		while ( pTSFProfileNode )
		{
			delete pTSFProfileNode->m_pTSFProfile;
			TSFProfileNode *pTmp = pTSFProfileNode->m_pNext;
			delete pTSFProfileNode;
			pTSFProfileNode = pTmp;
		}
    }
        
	m_HKLLayoutTextMap.Purge();

	m_SupportedInputLanguages.Purge();

    UnInitCompartmentSink();
    UnInstallProfileActivationSink();

    Release();

	if ( m_nRef == 0 )
	{
		if ( m_pTFCompartment )
		{
			m_pTFCompartment->Release();
			m_pTFCompartment = NULL;
		}

		if ( m_pTFThreadMgr )
		{
			m_pTFThreadMgr->Release();
			m_pTFThreadMgr = NULL;
		}
	}
}

bool CIMENamesManagerVista::InitCompartmentSink()
{
    ITfSource *pSource = NULL;
    bool bRet = false;

    if ( !m_pTFCompartment )
		goto cleanUp;

    if ( FAILED( m_pTFCompartment->QueryInterface( IID_ITfSource, (void **)&pSource ) ) )
	{
		Log_Warning( LOG_IME, "Failed to QueryInterface( IID_ITfSource ).\n");
        goto cleanUp;
	}

    if ( FAILED( pSource->AdviseSink( IID_ITfCompartmentEventSink, (ITfCompartmentEventSink *)this, &m_dwCompartmentSinkCookie ) ) )
    {
		Log_Warning( LOG_IME, "Failed to AdviseSink( IID_ITfCompartmentEventSink ).\n");

        // make sure we don't try to Unadvise Cookie later
        m_dwCompartmentSinkCookie = TF_INVALID_COOKIE;
        goto cleanUp;
    }

    bRet = true;

cleanUp:
    if ( pSource )
	{
        pSource->Release();
	}

    return bRet;
}

void CIMENamesManagerVista::UnInitCompartmentSink()
{
    if ( m_dwCompartmentSinkCookie == TF_INVALID_COOKIE )
        return; // never Advised

	if ( m_pTFCompartment )
	{
		ITfSource *pSource = NULL;
		if ( SUCCEEDED( m_pTFCompartment->QueryInterface( IID_ITfSource, (void **)&pSource ) ) )
		{
			HRESULT hr = pSource->UnadviseSink( m_dwCompartmentSinkCookie );
			if ( hr == S_OK )
			{
				Release();
			}
			pSource->Release();
		}
	}

    m_dwCompartmentSinkCookie = TF_INVALID_COOKIE;
}

void CIMENamesManagerVista::UnInstallProfileActivationSink()
{
    if ( m_dwInputProcessorProfileEventSinkCookie == TF_INVALID_COOKIE )
        return; // never Advised
	
	if ( m_pTFThreadMgr )
	{
		ITfSource *pSource = NULL;
		if ( SUCCEEDED( m_pTFThreadMgr->QueryInterface( IID_ITfSource, (void **)&pSource ) ) )
		{
			pSource->UnadviseSink( m_dwInputProcessorProfileEventSinkCookie );
			pSource->Release();
		}
	}

    m_dwInputProcessorProfileEventSinkCookie = TF_INVALID_COOKIE;
}

HRESULT CIMENamesManagerVista::OnChange( REFGUID refguid )
{
    VARIANT var;

    if ( refguid == GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION_GFX )
    {
        if ( SUCCEEDED( m_pTFCompartment->GetValue( &var ) ) ) 
        {
            if (var.vt == VT_I4) 
            {
                m_fdwConversion = var.lVal;
                CIMENamesManager::SetConversionMode( m_fdwConversion );
            }
        }
    }

    // Return value is ignored.
    return true;
}

// Override and do nothing since all the stuff that needs to happen here takes place in OnActivated.
void CIMENamesManagerVista::OnInputLangChange( DWORD langId )
{
	NOTE_UNUSED( langId );
}

/*
Important Note: This function is called when the input language/IME is changed using either our language bar or the windows language bar.
the WM_LANG_CHANGE notification is also sent in response, which leads to OnInputLangChange being called which we'll override and ignore 
since we don't want to repeat the same steps.
*/
HRESULT CIMENamesManagerVista::OnActivated(
	DWORD dwProfileType,
    LANGID langid,
    REFCLSID rclsid,
    REFGUID catid,
    REFGUID guidProfile,
    HKL hkl,
    DWORD dwFlags)
{
    NOTE_UNUSED3(catid, hkl, dwFlags);

    PlatOleAutFunctions_t *pOleAut = Plat_RequireLoadOleAut();

    HRESULT						hr;
    ITfInputProcessorProfiles	*pProfiles;
	ITfInputProcessorProfileMgr *pprofilesMgr;
	TF_INPUTPROCESSORPROFILE	profile1;
    BSTR						currentProfileName = pOleAut->pSysAllocStringByteLen(NULL, 0);
    wchar_t						wszLangName[MAX_PATH];
    bool						imeNameSet = false; // This variable records if we were able to obtain the IME name or not.

	// Note according to the documentation:
	// hkl: Specifies the keyboard layout handle of this profile. If dwProfileType is TF_PROFILETYPE_ INPUTPROCESSOR, hkl is NULL.
	// On chinese Vista I've found that this is not always true (for Changjie, new Changjie, phonetic and new phonetic). 
	// If this member is not NULL, the profile doesn't match what we've stored in the 
	// SupportedIME's list and the IME is not activated properly. 
	// To take care of this, we set hkl to 0 if dwProfileType = TF_PROFILETYPE_ INPUTPROCESSOR
	
    hr = Plat_RequireLoadCOM()->pCoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITfInputProcessorProfiles), (LPVOID*)&pProfiles);
	if (FAILED(hr))
		return false;

	hr = pProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr_GFx, (LPVOID*)&pprofilesMgr);
	if (FAILED(hr))
		return false;
   
	// This must be done before obtaining the profile from the profilesManager. 
	if (dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR)
	{
		hkl = 0;
	}

	pprofilesMgr->GetProfile(dwProfileType, langid, rclsid, guidProfile, hkl, &profile1);
	HKL_Size_t_Union un1;
	un1.m_HKL = profile1.hkl;

	GUID guid_null = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };

	// When this happens, it means (for some Korean IMEs) that this profile is bogus and was never registered in HashIMENames, so don't do anything.
	if ( IsEqualGUID( profile1.guidProfile, guid_null ) && IsEqualGUID( profile1.catid, guid_null ) && ( ( un1.m_Val & 0xFFFFFFFF ) == 0 ) ) 
	{
		return true;
	}

//	GUID guidclsidLocal		= rclsid;
//	GUID guidProfileLocal	= guidProfile;

	if (profile1.hklSubstitute != 0)
	{
	//	hkl = profile.hklSubstitute;
	//	profile.guidProfile = 0;
	//	GUID guid_null = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
	//	guidclsidLocal		= guid_null;
	//	guidProfileLocal	= guid_null;
	}

	if ( pprofilesMgr )
	{
		pprofilesMgr->Release();
	}

    if ( SUCCEEDED(hr) && pProfiles ) 
    {
        m_CurrentInputLangTag = GetLangTagFromLangId( langid );

        // Note- must use the W version of GetLocaleInfo, since otherwise szLangName will contain bad characters for
		// languages containing non-ascii characters.
        GetLocaleInfoW( MAKELCID( langid, SORT_DEFAULT ), LOCALE_SLANGUAGE, wszLangName, V_ARRAYSIZE( wszLangName ) );

		// 1) Send notifications to AS about input language change and update text in the InputLangTab of the language bar
		//	  Note that if the new input language is not supported, the InputLangTab might say "Unknown" corresponding to the 
		//    GFxIMETag_NotSupported.

        // Note that SetCurrentInputLanguage also calls BroadcastSwitchLanguage
        m_pIMEManagerWin32->SetCurrentInputLanguage( m_CurrentInputLangTag );
		
		// If the user uses the language bar to change the input language, we should change
        // the text in the InputLangTab of our language bar in action script. 
        m_pIMEManagerWin32->BroadcastSetCurrentInputLanguage( wszLangName );

		// Following cases to consider:
		//	1- System IME (Supported)
		//	2- System IME (UnSupported)
		//	3- Third Party IME (Supported)
		//	4- Third Party IME (UnSupported)
		//	5- Keyboard Layout (Doesn't have any IME)
		
		//2) 
		// This obtains the name of the new input language and updates the IMENameTab. Note that at this point, we haven't 
		// checked if we support the new input language or not. We are just setting the text in the IMENameTab to whatever 
		// TSF tells us is the name of the input language. 
		 pProfiles->GetLanguageProfileDescription(rclsid, langid, guidProfile, &currentProfileName);
	
        if ( pOleAut->pSysStringLen(currentProfileName) == 0 )
        {
			// So we are either a keyboard layout, or third party IME. The dwProfileType member doesn't help distinguish between
			// the two since it's equal to TF_PROFILETYPE_KEYBOARDLAYOUT  for both. The way we will distinguish is to 
			// match the hkl against the registry entries and check if there is an entry corresponding to imefile or not. 
			HKL_Size_t_Union HKLUnion;
			HKLUnion.m_HKL = hkl;
			int numInstalledHKls = m_HKLLayoutTextMap.Count();
			CIMENamesManager::SetLastIMEName( L"" );

			if (HKLUnion.m_Val != 0x04120412)
			{
				bool matchFound = false;
				for (int i = 0; i < numInstalledHKls; i++)
				{
					if (((HKLUnion.m_Val & 0xFFFFFFFF) == m_HKLLayoutTextMap[i].m_HKL_As32Bit))
					{
						if ( !m_HKLLayoutTextMap[i].m_ImeFileName.IsEmpty() )
						{
							// This is a third party IME. We will check if it's supported or not in the next step
							matchFound = true;
							break;
						}
					}
				}
				if (!matchFound)
				{
					// This is a keyboard layout for sure. Return
					CIMENamesManager::SetLastIMEName( wszLangName );
					goto cleanUp;
				}
			}
        }		

		// 3) 
		// Now we'll look into our data structures and check to see if we actually support this new input language. If we 
		// do, we'll update the name to what we get from our DS and also show the status window.
        
		// Terminology: We'll use the term "Input Processor Profile" (following MSDN documentation) to refer to an system IME, 
		// keyboard layout (for example- English(US), English (UK) etc) and third party IMEs. According to my observations, the
		// following is true:
		// 1) When the new input processor profile is a system IME (such as Quan Pin, Phonetic, Korean etc- anything that comes
		// pre-installed with the operating system), dwProfileType is TF_PROFILETYPE_INPUTPROCESSOR. For this case, hkl is null.
		
		// 2) When the new input processor profile is a third-party IME or a keyboard layout, 
		// dwProfileType = TF_PROFILETYPE_KEYBOARDLAYOUT. For this case, rclsid, catid and guidProfile are null. 
		
		m_CurrentIMETag = GFxIME_NotSupported;
		imeNameSet = false;
		for (int i = 0; i < m_SupportedIMEs.Count(); i++)
		{
			TSFProfileNode* pTSFProfileNode = m_SupportedIMEs[i].m_pTSFProfileNode;
			while ( pTSFProfileNode )
			{
				TF_INPUTPROCESSORPROFILE profile2 = *pTSFProfileNode->m_pTSFProfile;

				if ( (profile2.langid == langid ) &&
					( profile2.clsid == rclsid ) &&
					( profile2.catid == catid ) &&
					( profile2.hkl == hkl ) &&
					( profile2.guidProfile == guidProfile ) &&
					( profile2.hklSubstitute == profile1.hklSubstitute ) )
				{
					m_CurrentIMETag = m_SupportedIMEs[i].ItemTag;
					m_pIMEManagerWin32->SetIMETag( m_CurrentIMETag );

					if ( m_SupportedIMEs[i].m_UsesIMEFileNameOrLayoutText == USES_IME_FILENAME )
					{
						CIMENamesManager::SetLastIMEName( m_SupportedIMEs[i].m_ItemNameCommon.Get() );
					}
					if ( m_SupportedIMEs[i].m_UsesIMEFileNameOrLayoutText == USES_LAYOUT_TEXT )
					{
						CIMENamesManager::SetLastIMEName( m_SupportedIMEs[i].m_ItemNameOnSystem.Get() );
					}

					imeNameSet = true;
					m_pIMEManagerWin32->BroadcastDisplayStatusWindow();
					break;
				}
				pTSFProfileNode = pTSFProfileNode->m_pNext;
			}

			if (imeNameSet) 
				break;
		}

		if ( !imeNameSet ) // Failed to set the imename by the previous two techniques, just set it to the language name as the fallback option
		{
			CIMENamesManager::SetLastIMEName( L"Not Supported" );
		}

		if ( pOleAut->pSysStringLen( currentProfileName ) != 0 )
			pOleAut->pSysFreeString(currentProfileName);
cleanUp:	
		if (pProfiles)
			pProfiles->Release();

		// Return value is ignored.
		return true;
	}

	return true;   
}

/*
 The basic idea here is as follows: We have our data structure called SupportedIME's which is an array of 
 InputLangProps structs. This array contains a list of IME's that we support. The entries are as follows:

 struct InputLangProps
 {
 char* ItemNameOnSystem;    // The name of the IME on the system. Usually corresponds to the "LayoutText" entry 
                            // in the "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts" key of the registry.
 char* ItemNameCommon;      // A descriptive name assigned by us to the IME. Its use is described later.     
 GFxIMETag   ItemTag;       // An enum assigned to the IME used in comparisons.
 intp       Id;            // Locale Id or pointer to Language Profile struct. It's use is described later.
 };

 We want to query the system for the IME's installed and check in the SupportedIME DS to see which ones we support. 
 For every IME we support, we set the Id field to either the HKL for that IME (on XP) or pointer to a profile structure
 (on Vista, refer to MSDN doc on TSF for more details. HKL can't be used on Vista since more than one IME's can have the 
 same HKL). This field is used when we are called upon to switch to a specific IME. 

 Obtaining the list of active IME's is relatively straighforward on XP (the GetKeyboardLayoutList function comes handy 
 and provides us with exactly what we need). This functions provides us a list of HKL's that correspond to the active
 input langugages and IME's. We now need to correspond this information with the SupportedIME DS. The HKL's can't be used
 directly since the same IME can have a different HKL on different computers. To solve this problem, we look into the registry
 and make a list of the IMEFileName and LayoutText field for every installed IME (the entries in the registry are indexed
 by a key which is the same as the HKL for the IME- for example, look at the SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts
 entry on your comp and you'll know what I mean). Now it's important to understand the difference between "installed" and 
 "active" IME's. The user doesn't have to activate all the IME's installed on this computer. He can use the language bar
 to add or remove IME's from the list of active IME's for each input language. In the GFx language bar, we only want to 
 display the IME's that are active. Therefore, we look at the intersection of the HKLs returned by GetKEyboardLayoutList
 and those in the registry to get a list of active IMEs. For each active IME, we check if the LayoutName or the IMEFileName
 matches the corresponding entry in the SupportedIMEs DS. The IMEFileName is needed since the LayoutName can be localized
 (On a Chinese computer, the IME name could be in chinese etc), but the IMEFileName is the same. Different versions of the 
 same IME can have different IMEFileNames through, so all known versions must be tested. If the LayoutText matches, we use that
 to display the IME info in our language bar. If the IMEFileName matches, we use the ItemNameCommon entry. 

 On Vista, the situation is more complicated. The system IME's (the ones that come preinstalled with Windows Vista) all have
 the same HKL, so HKL can't be used to distinguish between IME's. 
 pProfiles->GetLanguageProfileDescription(langProfile.clsid, langProfile.langid, langProfile.guidProfile, &profileName);
 is used to get the profile info (on Vista, the term profile is used to describe IME's and Keyboardlayouts) for each
 input language. For the system IME's, the hkl field of the profile structure is 0, and the name of the profile is 
 returned in profileName (see code below). For third party IME's, a unique HKL IS assigned to each IME, so we can check
 against the registry information to obtain the IMEFilename and the LayoutText name. So to reiterate, for the system IME's,
 only the profile name as returned by the GetLanguageProfileDescription can be obtained- since we can't index these IME's
 against the registry, IMEFileName can't be obtained. For the third party IME's both the LayoutText as well as IMEFileName 
 can be obtained. These two pieces of info (IMEFileName = null for system IMEs) are used in the CheckForSupportedIME to 
 check if that IME is supported by us or not. This means that if the system IME names are localized on OS in different languages,
 we'll need to have the corresponding entry in the supportedIME DS. So far this doesn't seem to be the case. Also, on Vista,
 a pointer to the profile structure is stored in the id field of the inputLangProps DS, which can be used by TSF functions
 to switch to a certain IME or inputlanguage.
*/

bool CIMENamesManagerVista::QualifyIMENames()
{ 
    m_CurrentIMETag = GFxIME_NotSupported;
    m_CurrentInputLangTag = GFxIME_NotSupported;
    
    if ( !CIMENamesManager::MakeKeyboardLayoutListFromRegistry( 0, 0 ) )
	{
		return false;
	}
    
    for ( int i = 0; i < m_SupportedIMEs.Count(); i++ ) 
	{
		m_SupportedIMEs[i].m_UsesIMEFileNameOrLayoutText = USES_UNKNOWN;
	}

	ITfInputProcessorProfiles *pProfiles = NULL;
    HRESULT hr = Plat_RequireLoadCOM()->pCoCreateInstance( CLSID_TF_InputProcessorProfiles, 
                            NULL, 
                            CLSCTX_INPROC_SERVER, 
                            IID_ITfInputProcessorProfiles, 
                            (LPVOID*)&pProfiles);
    if ( FAILED(hr) || !pProfiles )
        return false;

    PlatOleAutFunctions_t *pOleAut = Plat_RequireLoadOleAut();

	ITfInputProcessorProfileMgr *pProfilesMgr = NULL;
    hr = pProfiles->QueryInterface( IID_ITfInputProcessorProfileMgr_GFx, (LPVOID*)&pProfilesMgr );
    if ( SUCCEEDED( hr ) && pProfilesMgr )
    {
		// Get the active profile
		TF_INPUTPROCESSORPROFILE activeProfile;
		V_memset( &activeProfile, 0, sizeof( activeProfile ) );
		pProfilesMgr->GetActiveProfile( GUID_TFCAT_TIP_KEYBOARD, &activeProfile );

		// Get the active profile's language list
		LANGID *pLangIDs = NULL;
        ULONG nLanguageListCount = 0;
        hr = pProfiles->GetLanguageList( &pLangIDs, &nLanguageListCount );
        if ( SUCCEEDED( hr ) )
        {
			Log_Detailed( LOG_IME, "Discovered %d installed languages.\n", nLanguageListCount );

			// Consider each discovered language
            for ( ULONG nLanguage = 0; nLanguage < nLanguageListCount; nLanguage++ )
            {
                wchar_t wszLangName[MAX_PATH];
                if ( 0 != GetLocaleInfoW( MAKELCID( pLangIDs[nLanguage], SORT_DEFAULT ), LOCALE_SLANGUAGE, wszLangName, V_ARRAYSIZE( wszLangName ) ) )
                {
					IEnumTfInputProcessorProfiles *ppEnum = NULL;
                    pProfilesMgr->EnumProfiles( pLangIDs[nLanguage], &ppEnum );

                    GUID guid;
					V_memset( &guid, 0, sizeof( guid ) );

					TF_INPUTPROCESSORPROFILE langProfile;
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
                    SetupForSupportedInputLanguage( langProfile.langid, wszLangName ); 

                    BSTR pProfileDescription = NULL;
                    CUtlWString profileNameStr;
					CUtlWString imeNameStr;

                    if ( ppEnum )
                    {
						// Iterate all profiles for this language
                        while ( ppEnum->Next( 1, &langProfile, NULL ) == S_OK )
                        {
                            pOleAut->pSysFreeString( pProfileDescription );
							pProfileDescription = NULL;
                            profileNameStr.Clear();

                            pProfiles->GetLanguageProfileDescription( langProfile.clsid, langProfile.langid, langProfile.guidProfile, &pProfileDescription );

							Log_Detailed( LOG_IME, "Considering LANGID: 0x%8.8x, Profile Description: '%s'\n", langProfile.langid, CTempWStringToPrintableString( pProfileDescription ? (wchar_t*)pProfileDescription : L"No Profile Description" ).Get() );

							// Check if there are any special characters in the profile description such as percentages
							// for example: "@"%ProgramFiles%\Windows Journal\NBMapTip.dll",-16"
							if ( pProfileDescription )
							{
								uintp len = V_wcslen( pProfileDescription );
								bool bSpecialChar = false;
								for ( uintp nChar = 0; nChar < len; nChar++ )
								{
									if ( pProfileDescription[nChar] == '%' )
									{
										bSpecialChar = true;
										break;
									}
								}
								if ( bSpecialChar )
								{
									// not a clean profile name as expected
									Log_Detailed( LOG_IME, LOG_COLOR_YELLOW, "   Skipping due to unexpected profile characters.\n" );
									continue;
								}
							}
							
                            if ( !( ( langProfile.dwFlags & TF_IPP_FLAG_ENABLED ) || ( langProfile.dwFlags & TF_IPP_FLAG_ACTIVE ) ) )
							{
								// neither active nor enabled
								Log_Detailed( LOG_IME, LOG_COLOR_YELLOW, "   Skipping due to not enabled nor active.\n" );
                                continue;
							}

							// This variable checks if the current profile is a keyboard layout or not. 
							// Checking for TF_PROFILETYPE_KEYBOARDLAYOUT is not enough since third party IME's
							// can have dwProfileType = above flag.

							bool bIsThirdPartyIME = false;
							bool bIsKoreanIME  = false;
							if ( ( langProfile.hkl != 0 ) || ( langProfile.hklSubstitute != 0 ) )
                            {
                                // Check in the layout text table indexed by the hkl.
								// At this point, we know we are dealing with either keyboard layout or third party ime.
								// keyboard layouts can be the default keyboard layout (for example US English) in which 
								// case the corresponding hkl is what's obtained from the registry (0x04090409). User can
								// also add different keyboard layouts to an input language (for example, United Kingdom (International),
								// United Kingdom (Extended) etc using the language settings in the control panel. 
								// Changing the keyboard layout generally changes the mapping between keystrokes and the virtual
								// key code generated and doesn't involve the IME system. In our language bar, we don't care about
								// different keyboard layouts installed for a certain input language. The system doesn't 
								// seem to provide a way to obtain information about these layouts anyways. 
							
								// We distinguish between the keyboard layouts and third party IME based on the value of 
								// ImeFileName in the corresponding registry entry.
                                for ( int i = 0; i < m_HKLLayoutTextMap.Count(); i++ )
                                {
                                    HKL_Size_t_Union un1;
									HKL_Size_t_Union un2;
									un1.m_HKL = langProfile.hkl;
									un2.m_HKL = langProfile.hklSubstitute;

									if ( (un1.m_Val & 0xFFFFFFFF) == 0x04120412 )
									{
										// Korean keyboard layout is a special case, and we must create a profile for it for 
										// things to work properly
										if ( (un1.m_Val & 0x0000FFFF) == m_HKLLayoutTextMap[i].m_HKL_As32Bit )
										{
											bIsKoreanIME = true;
											profileNameStr = m_HKLLayoutTextMap[i].m_LayoutName.Get();
											break;
										}
									}
                                    else if (((un1.m_Val & 0xFFFFFFFF) == m_HKLLayoutTextMap[i].m_HKL_As32Bit) || ((un2.m_Val & 0xFFFFFFFF) == m_HKLLayoutTextMap[i].m_HKL_As32Bit))
                                    {
                                        profileNameStr = m_HKLLayoutTextMap[i].m_LayoutName.Get();
										imeNameStr = m_HKLLayoutTextMap[i].m_ImeFileName.Get();
                                        
										if ( !imeNameStr.IsEmpty() )
										{										
											bIsThirdPartyIME = true;
											if ( profileNameStr.IsEmpty() && pProfileDescription )
											{
												// In case third part IME lacks layout name in HKL.
												profileNameStr = (wchar_t*)pProfileDescription;
											}
										}
										break;
                                    }
                                }

								if ( !bIsKoreanIME && !bIsThirdPartyIME )
								{
									// no need to store this profile, this is just a keyboard layout (either a default
									// layout like english (US) or custom layouts. Consider the next profile
									if ( langProfile.dwFlags & TF_IPP_FLAG_ACTIVE )
									{
										m_CurrentIMETag = GFxIME_DoesntExist;
									}
									continue;
								}
                            }
							else
                            {
								// System IME
                                profileNameStr = (wchar_t*)pProfileDescription;
                            }

							// Check if this profile (either a system IME or third party IME) is supported or not.
							bool bIsSupported = false;
							int nSupportedIMEIndex = -1;
                            if ( !profileNameStr.IsEmpty() )
							{
								Log_Detailed( LOG_IME, "Registering IME. Profile: '%s', IME filename: '%s'\n", CTempWStringToPrintableString( profileNameStr.Get() ).Get(), CTempWStringToPrintableString( imeNameStr.Get() ).Get() );

								nSupportedIMEIndex = CIMENamesManager::CheckForSupportedIME( profileNameStr.Get(), imeNameStr.Get() ); 
								if ( nSupportedIMEIndex >= 0 )
								{
									Log_Detailed( LOG_IME, LOG_COLOR_GREEN, "   Successfully Registered (Index: %d).\n", nSupportedIMEIndex );
									bIsSupported = true;
								}
								else
								{
									Log_Detailed( LOG_IME, LOG_COLOR_YELLOW, "   Not Supported.\n" );
								}

								// Reset imeNameStr and profileNameStr
								// Sometimes the profileNameStr can have escape characters such as percentages in it.
								imeNameStr.Clear();
								profileNameStr.Clear();
							}

							if ( bIsSupported )
							{
								// We need to create a list of profiles for each IME since TSF can send us multiple profiles for each IME and 
								// when OnActivated is called, we don't know which profile is sent last. Se we need to store all the profiles and 
								// match against all the profiles stored for an IME
								// To activate an IME, we just use the first profile stored. That has worked fine so far. 
								TF_INPUTPROCESSORPROFILE *pTSFProfile = new TF_INPUTPROCESSORPROFILE();
								V_memcpy( pTSFProfile, (void*)(&langProfile), sizeof(TF_INPUTPROCESSORPROFILE) );

								TSFProfileNode *pNewTSFProfileNode = new TSFProfileNode();
								pNewTSFProfileNode->m_pTSFProfile = pTSFProfile;
								pNewTSFProfileNode->m_pNext = m_SupportedIMEs[nSupportedIMEIndex].m_pTSFProfileNode;
								m_SupportedIMEs[nSupportedIMEIndex].m_pTSFProfileNode = pNewTSFProfileNode;

								// check if this profile is active
								if ( langProfile.dwFlags & TF_IPP_FLAG_ACTIVE )
								{
									m_CurrentIMETag = m_SupportedIMEs[nSupportedIMEIndex].ItemTag;
								}
							}
                        }
                        ppEnum->Release();
                    }         

                    pOleAut->pSysFreeString( pProfileDescription );
                    pProfileDescription = NULL;
                }               
            }
        
            Plat_RequireCOM()->pCoTaskMemFree( pLangIDs );
        }
        
        // Set currently active language
		for ( int j = 0; j < m_SupportedInputLanguages.Count(); j++ )
        {
            if ( m_SupportedInputLanguages[j].Id == activeProfile.langid )
            {
                m_CurrentInputLangTag = m_SupportedInputLanguages[j].ItemTag; 
            }
        }
		
        if ( pProfiles )
		{
            pProfiles->Release();
		}

        if ( pProfilesMgr )
		{
            pProfilesMgr->Release();
		}

		// success
        return true;
    }

	// failed
    return false;
}

void CIMENamesManagerVista::SetConversionMode(uint32 fdwConversion)
{
    NOTE_UNUSED(fdwConversion);
    // do nothing
    CIMENamesManager::SetConversionMode();
}

void CIMENamesManagerVista::ActivateInputLanguage( const wchar_t *pInputLangName )
{
    // First obtain the entry in SupportedIME's corresponding to this ime
    int nIMEIndex = -1;
    for ( int j = 0; j < m_SupportedInputLanguages.Count(); j++)
    {
		// It's ok to use strcmp here since we are matching against the inputLangName is exactly what we have stored in
		// our lang bar data structure. Therefore, case insensitive comparison is not needed. 
        if ( (!wcscmp( m_SupportedInputLanguages[j].m_ItemNameOnSystem.Get(), pInputLangName )) && ( m_SupportedInputLanguages[j].Id != 0 ) ) // The second condition precludes non-supported IMEs
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
   
	ITfInputProcessorProfiles *pProfiles = NULL;
    HRESULT hr = Plat_RequireLoadCOM()->pCoCreateInstance(  CLSID_TF_InputProcessorProfiles, 
        NULL, 
        CLSCTX_INPROC_SERVER, 
        IID_ITfInputProcessorProfiles, 
        (LPVOID*)&pProfiles);

	// CoCreateInstance never called (this happens when using different threads!)
	if ( hr == 0x800401F0 )
	{
		Log_Warning( LOG_IME, "Failure in COM. CoCreateInstance from alternate thread?\n" );
	}

    if ( SUCCEEDED( hr ) && pProfiles )
    {
        hr = pProfiles->ChangeCurrentLanguage( LANGID( m_SupportedInputLanguages[nIMEIndex].Id ) );
        pProfiles->Release();
    }
}

// The idea here is that when the user clicks on an input language tab, we check in our list 
// to see if there are ime's associated with that input language. If there are, we present a list 
// of input languages to the user. The user then selects the desired input language which causes
// the fscommand "LangBar_OnIME" to be fired which calls ActivateIME function. 
// Now if there are no ime's associated with a selected input language, that input language 
// should be activated directly. So this function can be used to switch to a desired IME
// as well as an input language
void CIMENamesManagerVista::ActivateIME( const wchar_t *pImeName )
{
    // First obtain the entry in SupportedIME's corresponding to this ime
    int nIMEIndex = -1;
	for (int j = 0; j < m_SupportedIMEs.Count(); j++)
	{
		// check if need to match against ime file name or layout name
		// Matching the InputLangTag is needed because IME's of different input languages can have 
		// the same name. For example, "Microsoft IME" for Japanese and Korean IMEs.
		if ( m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText == USES_LAYOUT_TEXT )
		{
			if ( ( !wcscmp( pImeName, m_SupportedIMEs[j].m_ItemNameOnSystem.Get() ) ) && 
			( m_CurrentInputLangTag == GetInputLangTagFromIMETag( m_SupportedIMEs[j].ItemTag ) ) && 
			( m_SupportedIMEs[j].Id != 0 ) ) // The second condition precludes non-supported IMEs
			{
				nIMEIndex = j;
				break;
			}
		}
		else if ( m_SupportedIMEs[j].m_UsesIMEFileNameOrLayoutText == USES_IME_FILENAME )
		{
			if ( ( !wcscmp( pImeName, m_SupportedIMEs[j].m_ItemNameCommon.Get() ) ) && 
				( m_CurrentInputLangTag == GetInputLangTagFromIMETag( m_SupportedIMEs[j].ItemTag ) ) && 
				( m_SupportedIMEs[j].Id != 0 ) ) 
			{
				nIMEIndex = j;
				break;
			}
		}
	}
	
    if ( nIMEIndex == -1 ) 
		return;

	ITfInputProcessorProfiles *pProfiles = NULL;
    HRESULT hr = Plat_RequireLoadCOM()->pCoCreateInstance( 
		CLSID_TF_InputProcessorProfiles, 
        NULL, 
        CLSCTX_INPROC_SERVER, 
        IID_ITfInputProcessorProfiles, 
        (LPVOID*)&pProfiles );
    if ( SUCCEEDED( hr ) && pProfiles )
    {
        ITfInputProcessorProfileMgr *pProfilesMgr = NULL;
        hr = pProfiles->QueryInterface( __uuidof(ITfInputProcessorProfileMgr), (LPVOID*)&pProfilesMgr );
        if ( SUCCEEDED( hr ) && pProfilesMgr ) 
        {
			TSFProfileNode *pTSFProfileNode = m_SupportedIMEs[nIMEIndex].m_pTSFProfileNode;
			TF_INPUTPROCESSORPROFILE *pTSFProfile = pTSFProfileNode->m_pTSFProfile;
			if ( pTSFProfile )
			{
				pProfilesMgr->ActivateProfile(
					pTSFProfile->dwProfileType, 
					pTSFProfile->langid,
					pTSFProfile->clsid, 
					pTSFProfile->guidProfile,
					pTSFProfile->hkl, 
					TF_IPPMF_FORPROCESS );
			}
            
            pProfilesMgr->Release();
        }
        pProfiles->Release();
    }
}

// Setting the language name is done in the derived class since we use SetLocaleInfo for XP and SetLocaleInfoEx for Vista
// as recommended in the documentation. For setting the IMEname, the base class function is called.
void CIMENamesManagerVista::OnLangBarLoaded()
{
	HKL_Size_t_Union HKLUnion;
    HKLUnion.m_HKL = GetKeyboardLayout( 0 );
    m_CurrentInputLangTag = GetLangTagFromLangId( (uint32)( 0x0000FFFF & HKLUnion.m_Val ) );
    m_pIMEManagerWin32->SetCurrentInputLanguage( m_CurrentInputLangTag );

    // Should be using the Ex version, but it's not declared on WinXP. Can add the declaration, but will have
    // to test there is no incompatibility with Vista etc. Using the old version for now. Look at the header
    // file for a sample declaration.
	wchar_t wszLangName[MAX_PATH];
    if ( GetLocaleInfoW( MAKELCID( HKLUnion.m_Val, SORT_DEFAULT ), LOCALE_SLANGUAGE, wszLangName, V_ARRAYSIZE( wszLangName ) ) == 0 )
    {
        DWORD err = GetLastError();
        Log_Warning( LOG_IME, "IME Error: GetLocaleInfoW() Failed. Error Initializing Input Language Name (GetLastError() = %d)\n", err );
        m_pIMEManagerWin32->BroadcastSetCurrentInputLanguage( L"Unsupported" );
    }
    else
    {
        m_pIMEManagerWin32->BroadcastSetCurrentInputLanguage( wszLangName );

        // Note that the CurrentIMETag should always tell us what the current IME is. If the application is 
        // just starting off, it's set in QualifyIMENames, otherwise it's set whenever the language is changed 
        // and OnActivated is called.
        if ( m_CurrentIMETag == GFxIME_NotSupported )
        {
			CIMENamesManager::SetLastIMEName( L"Not Supported" );
        }
		if ( m_CurrentIMETag == GFxIME_DoesntExist )
		{
			CIMENamesManager::SetLastIMEName( wszLangName );
		}
        else
        {
            // Set IME Name in the base class.
            CIMENamesManager::OnLangBarLoaded();
        }
    }
}

void CIMENamesManagerVista::HandleStatusWindowNotifications( const char *pcommand, const char *parg )
{
    DWORD   fdwConversion;
    VARIANT var;
    HIMC    hIMC;

    hIMC = ImmGetContext( m_pIMEManagerWin32->m_hWnd );
    
    DWORD conv, sentence;
    ImmGetConversionStatus(hIMC, &fdwConversion, &sentence);

    if (hIMC == NULL) return;

    if ( V_strcmp( pcommand, "StatusWindow_OnShape" ) == 0)
    {
        if (!V_strcmp(parg, "false"))
        {
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_FULLSHAPE); 
        }

        if (!V_strcmp(parg, "true"))
        {
            fdwConversion = fdwConversion | TF_CONVERSIONMODE_FULLSHAPE; 
        }
    }
    else if(V_strcmp(pcommand, "StatusWindow_OnInputMode") == 0)
    {
        // Pertains to Chinese IME only
        if (!V_strcmp(parg, "false"))
        {
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_NATIVE); 
            conv = conv & (~IME_CMODE_NATIVE); 
        }

        if (!V_strcmp(parg, "true"))
        {
            fdwConversion = fdwConversion | TF_CONVERSIONMODE_NATIVE;
            conv = conv | IME_CMODE_NATIVE; 
        }

        // Pertains to Japanese IME.

        if (!V_strcmp(parg, "Hiragana"))
        {
            // If in direct input mode, first turn IME on. Too bad that we have 
            // to use IMM function here. Doesn't seem to be a way to turn IME on/off
            // using TSF.
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_KATAKANA); 
            fdwConversion = fdwConversion | TF_CONVERSIONMODE_NATIVE;
        }

        if (!V_strcmp(parg, "Full-Width Katakana"))
        {         
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion | TF_CONVERSIONMODE_KATAKANA | TF_CONVERSIONMODE_FULLSHAPE | TF_CONVERSIONMODE_NATIVE; 
        }

        if (!V_strcmp(parg, "Full-Width Alphanumeric"))
        {
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_NATIVE); 
            fdwConversion = fdwConversion | TF_CONVERSIONMODE_FULLSHAPE; 
        }

        if (!V_strcmp(parg, "Half-Width Katakana"))
        {
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion | TF_CONVERSIONMODE_KATAKANA | TF_CONVERSIONMODE_NATIVE;
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_FULLSHAPE);
        }

        if (!V_strcmp(parg, "Half-Width Alphanumeric"))
        {
            if (ImmGetOpenStatus(hIMC) == 0)
            {
                ImmSetOpenStatus(hIMC, true);
            }
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_NATIVE);
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_FULLSHAPE);
        }

        if (!V_strcmp(parg, "DirectInput"))
        {
            if (ImmGetOpenStatus(hIMC) != 0)
            {
                ImmSetOpenStatus(hIMC, false);
            }

        }
    }
    else if(V_strcmp(pcommand, "StatusWindow_OnSymbol") == 0)
    {
        if (!V_strcmp(parg, "false"))
        {
            fdwConversion = fdwConversion & (~TF_CONVERSIONMODE_SYMBOL); 
        }

        if (!V_strcmp(parg, "true"))
        {
            fdwConversion = fdwConversion | TF_CONVERSIONMODE_SYMBOL; 
        }
    }

    var.lVal = fdwConversion;
    
    ImmSetConversionStatus(hIMC, fdwConversion, sentence);
    ImmReleaseContext( m_pIMEManagerWin32->m_hWnd, hIMC );
}


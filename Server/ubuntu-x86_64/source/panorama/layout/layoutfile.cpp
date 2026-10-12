//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "layoutfile.h"
#include "fileio.h"
#if defined( SOURCE2_PANORAMA )
#include "../thirdparty/libparsifal-0.8.3/include/libparsifal/parsifal.h"
#else
#include "libparsifal-0.8.3/include/libparsifal/parsifal.h"
#endif

#if !defined( SOURCE2_PANORAMA ) 
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
#define ClientUtils SteamUtils
#define ClientHTTP SteamHTTP
#endif
#endif

#if defined( SOURCE2_PANORAMA )
#include "appframework/iapplication.h"
#endif

#include "resourcesystem/iresourcesystem.h"

#define V_isstrlower_fast V_isstrlower

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

static const int64 k_nAutoReloadFailedFileLoadDelay = 100 * k_nThousand;

#if !defined( SOURCE2_PANORAMA )
static CCommandLineParam g_DevMode( "-dev", "Developer mode" );
#endif



//-----------------------------------------------------------------------------
// Purpose: include file container
//-----------------------------------------------------------------------------
struct LayoutIncludeFile_t
{
	ELayoutIncludeFileType m_eType;
	CUtlString m_strPath;
};


//-----------------------------------------------------------------------------
// Purpose: global layout manager
//-----------------------------------------------------------------------------
CLayoutManager &panorama::LayoutManager() { return (*(CLayoutManager*)(UIEngine()->UILayoutManager())); }

//-----------------------------------------------------------------------------
// Purpose: parses xml layout files
//-----------------------------------------------------------------------------
namespace panorama
{
class CLayoutFileDependancyParser
{
public:
	CLayoutFileDependancyParser() {	}

	~CLayoutFileDependancyParser() { }
		
	bool BParseXML( CUtlBuffer &buffer )
	{
		if ( buffer.TellPut() < 1 )
			return false;

		const char *pchStart = (const char *)buffer.Base();
		const char *pchCur = (const char *)buffer.Base();

		CUtlBuffer bufTmpPath;
		CUtlBuffer bufCurTagName( 1024 );
		bool bParsingTagName = false;
		bool bInsideStyles = false;
		bool bInRoot = false;
		bool bInsideInclude = false;
		bool bInsideScripts = false;

		while ( pchCur[0] && (pchCur - pchStart) < buffer.TellPut() )
		{
			if ( V_strncmp( "<!--", pchCur, 4 ) == 0 )
			{
				pchCur += 4;
				while( V_strncmp( pchCur, "-->", 3 ) != 0 && (pchCur - pchStart) < buffer.TellPut()  )
					++pchCur;
			}


			// If we are parsing a tag name, then keep accumulating it or bail out of parsing it
			if ( bParsingTagName )
			{
				if ( pchCur[0] != '>' && pchCur[0] != '<' && !V_isspace( pchCur[0] ) )
					bufCurTagName.PutChar( pchCur[0] );
				else
				{
					// Done parsing tag name
					bufCurTagName.PutChar( 0 );
					bParsingTagName = false;

					const char *pchElement = (const char *)bufCurTagName.Base();

					if ( !bInRoot && V_stricmp( pchElement, "root" ) == 0 )
					{
						// Just push it, and track that we are in root
						bInRoot = true;
						continue;
					}
					else if ( !bInRoot )
					{
						// Invalid XML, always must start with <root>
						return false;
					}

					if ( bInRoot )
					{
						// Done parsing styles
						if ( V_stricmp( pchElement, "/styles" ) == 0 )
						{
							bInsideStyles = false;
							continue;
						}

						if ( V_stricmp( pchElement, "styles" ) == 0 )
						{
							// Should not get multiple styles elements
							if ( bInsideStyles )
								return false;

							bInsideStyles = true;
							continue;
						}

						if( V_stricmp( pchElement, "/scripts" ) == 0 )
						{
							bInsideScripts = false;
							continue;
						}

						if( V_stricmp( pchElement, "scripts" ) == 0 )
						{
							// Should not get multiple styles elements
							if( bInsideScripts )
								return false;

							bInsideScripts = true;
							continue;
						}
						
						// If we get something that isn't scripts/styles and we are inside of root then we are done with scripts/styles that MUST come first
						if( !bInsideScripts && !bInsideStyles )
							return true;

						Assert( bInsideStyles || bInsideScripts );
						if ( V_stricmp( pchElement, "include" ) == 0 )
						{
							if ( bInsideInclude )
							{
								AssertMsg( false, "Should not see an include within an include" );
								return false;
							}

							bInsideInclude = true;

							// Skip whitespace
							while ( V_isspace( pchCur[0] ) )
								++pchCur;

							// Dont with include tag
							if ( pchCur[0] == '>' )
							{
								bInsideInclude = false;
								pchCur++;
								continue;
							}
							else if ( V_strnicmp( pchCur, "src", 3 ) == 0 )
							{
								pchCur += 3;

								//skip whitepace and = 
								while ( V_isspace( pchCur[0] ) || pchCur[0] == '=' )
									++pchCur;

								// should have a " now
								if ( pchCur[0] == '"' )
								{
									++pchCur;
									CUtlBuffer bufInclude( 1024 );
									while ( pchCur[0] )
									{
										if ( pchCur[0] == '"' || pchCur[0] == '>' )
										{
											++pchCur;
											break;
										}
										else
										{
											bufInclude.PutChar( pchCur[0] );
											++pchCur;
										}
									}
									bufInclude.PutChar( 0 );

									if ( bufInclude.TellPut() > 0 )
									{
										bufTmpPath.EnsureCapacity( bufInclude.TellPut() );
										V_HtmlEntityDecodeToUTF8( (char*)bufTmpPath.Base(), bufInclude.TellPut(), (char*)bufInclude.Base(), bufInclude.TellPut() );

										LayoutIncludeFile_t includeFile;
										includeFile.m_strPath = (const char *)bufTmpPath.Base();
										includeFile.m_eType = k_ELayoutIncludeFileTypeInvalid;
										Assert( !bInsideStyles || !bInsideScripts );
										if ( bInsideStyles )
											includeFile.m_eType = k_ELayoutIncludeFileTypeCSS;
										if ( bInsideScripts )
											includeFile.m_eType = k_ELayoutIncludeFileTypeJS;

										m_vecIncludes.AddToTail( includeFile );
									}
								}
							}

							continue;
						}
					}
				}
			}

			// Closing tag of empty element?
			if ( pchCur[0] == '/' && pchCur[1] == '>' )
			{
				// Only care about tracking reaching the end of include tags
				if ( V_stricmp( (const char*)bufCurTagName.Base(), "include" ) == 0 )
					bInsideInclude = false;

				pchCur += 2;
			}

			// Start of a new tag, close any current tag
			if ( pchCur[0] == '<' )
			{
				bParsingTagName = true;
				bufCurTagName.Clear();
				++pchCur;

				// Skip any leading spaces after the < as well
				while ( V_isspace( pchCur[0] ) )
					++pchCur;

				continue;
			}


			++pchCur;
		}

		return true;
	}

	CUtlVector< LayoutIncludeFile_t > m_vecIncludes;
};
}



namespace panorama
{
//-----------------------------------------------------------------------------
// Purpose: Helper job to load a file from a URL
//-----------------------------------------------------------------------------
class CLoadLayoutFileAsync
{
public:
	CLoadLayoutFileAsync( CFileResource &resource, const char *pchXMLString, CPanoramaSymbol symPath, IUIPanel *pNotifyPanel, bool bIsFileReload, bool bIsPartialLayout ) 
		: m_HTTPRequestCompleted( this, &CLoadLayoutFileAsync::OnXMLRequestFinished ),
		m_HTTPIncludeRequestCompleted( this, &CLoadLayoutFileAsync::OnIncludeRequestFinished )
	{
		m_FileResource = resource;
		m_strXMLString = pchXMLString;
		m_symPath = symPath;
		m_hHTTPRequest = INVALID_HTTPREQUEST_HANDLE;
		m_pNotifyPanel = pNotifyPanel;
		m_bWasFileReload = bIsFileReload;
		m_bIsPartialLayout = bIsPartialLayout;
		m_bReceivedNotLoggedIn = false;
		m_bAllRemoteCSSLoaded = true;

		LayoutManager().TrackAsyncLoad( this );

		StartLoading();
	}

	~CLoadLayoutFileAsync()
	{
		FOR_EACH_MAP( m_mapHTTPIncludeRequests, i )
		{
			ClientHTTP()->ReleaseHTTPRequest( m_mapHTTPIncludeRequests.Key(i) );
			delete m_mapHTTPIncludeRequests[i];
		}

		m_mapHTTPIncludeRequests.RemoveAll();

		if ( m_hHTTPRequest != INVALID_HTTPREQUEST_HANDLE )
			ClientHTTP()->ReleaseHTTPRequest( m_hHTTPRequest );

		LayoutManager().ClearAsyncLoad( this );
	}

	void StartLoading()
	{
		if ( !UIEngine() )
		{
			delete this;
			return;
		}

		if ( !m_FileResource.BIsValid() && m_strXMLString.Length() > 0 )
		{
			m_bufLayoutFile.Put( m_strXMLString.Access(), m_strXMLString.Length() );
			OnXMLContentsReady();
		}
		else if ( m_FileResource.BIsHTTPURL() )
		{
			if ( !ClientHTTP() )
			{
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
				AssertMsg( false, "ClientHTTP() is NULL when using client interfaces." );
#endif
				// If ClientHTTP is NULL that means we're running with public ISteam interfaces and
				// Steam itself wasn't available. Just fail all requests
				HTTPRequestCompleted_t callback;
				callback.m_bRequestSuccessful = false;
				callback.m_hRequest = INVALID_HTTPREQUEST_HANDLE;
				callback.m_eStatusCode = k_EHTTPStatusCode501NotImplemented;
				OnXMLRequestFinished( &callback, false );
				return;
			}

			m_hHTTPRequest = ClientHTTP()->CreateHTTPRequest( k_EHTTPMethodGET, m_FileResource.GetReferencePath() );

			const CUtlVector<CUtlString> &vecCookies = m_FileResource.GetCookieHeadersForHTTPURL();
			if ( vecCookies.Count() )
			{
				char rgchDomain[1024];
				V_ExtractDomainFromURL( m_FileResource.GetReferencePath(), rgchDomain, V_ARRAYSIZE( rgchDomain ) );

				HTTPCookieContainerHandle hCookieContainer = UIEngineInternal()->GetCookieContainerForDomain( rgchDomain );
				FOR_EACH_VEC( vecCookies, iCookie )
				{
					ClientHTTP()->SetCookie( hCookieContainer, rgchDomain, "/", vecCookies[iCookie].String() );
				}

				ClientHTTP()->SetHTTPRequestCookieContainer( m_hHTTPRequest, hCookieContainer );
			}

			if ( m_bWasFileReload )
			{
				// This is a forced reload, so set no-cache
				ClientHTTP()->SetHTTPRequestHeaderValue( m_hHTTPRequest, "Cache-Control", "no-cache" );
			}

#if defined( SOURCE2_PANORAMA )
			ClientHTTP()->SetHTTPRequestGetOrPostParameter(  m_hHTTPRequest, "l", g_pApplication->GetLanguage( LanguageType_UI ) );
#else
			ELanguage eLang = UIEngine()->GetDisplayLanguage();
			ClientHTTP()->SetHTTPRequestGetOrPostParameter( m_hHTTPRequest, "l", GetLanguageShortName( eLang ) );
#endif

			UIEngineInternal()->AddCommonHeadersToHttpRequest( m_hHTTPRequest );

			SteamAPICall_t hSteamAPICall;
			if ( ClientHTTP()->SendHTTPRequest( m_hHTTPRequest, &hSteamAPICall ) )
			{
				// Add call handle to get callback
				m_HTTPRequestCompleted.AddCall( hSteamAPICall );
			}
			else
			{
				// Not really ready, but this handles failure too
				HTTPRequestCompleted_t callback;
				callback.m_bRequestSuccessful = false;
				callback.m_hRequest = m_hHTTPRequest;
				callback.m_eStatusCode = k_EHTTPStatusCode500InternalServerError;
				OnXMLRequestFinished( &callback, false );
			}
		}
		else if ( m_FileResource.BIsLocalPath() )
		{
			// Do local file load, via a threaded load call, don't really care about success/failure at this level
			UIEngine()->UIFileSystem()->LoadFileIntoBuffer( m_FileResource.GetReferencePath(), m_bufLayoutFile, true );
			OnXMLContentsReady();
		}
		else
		{
			AssertMsg1( false, "File type for %s is not supported in CLoadLayoutFileAsync", m_symPath.String() );
		}
	}

	void OnXMLRequestFinished( HTTPRequestCompleted_t *pParam, bool bIOFailure )
	{
		if ( bIOFailure || !pParam )
			OnXMLContentsReady();

		Assert( pParam->m_hRequest == m_hHTTPRequest );
		if ( pParam->m_bRequestSuccessful && pParam->m_eStatusCode == k_EHTTPStatusCode200OK )
		{
			uint32 unSize;
			if( ClientHTTP()->GetHTTPResponseHeaderSize( m_hHTTPRequest, "X-NotLoggedIn", &unSize ) && unSize != 0 )
				m_bReceivedNotLoggedIn = true;

			if ( ClientHTTP()->GetHTTPResponseBodySize( m_hHTTPRequest, &unSize ) )
			{
				m_bufLayoutFile.EnsureCapacity( unSize );
				if ( ClientHTTP()->GetHTTPResponseBodyData( m_hHTTPRequest, (uint8*)m_bufLayoutFile.Base(), unSize ) )
					m_bufLayoutFile.SeekPut( CUtlBuffer::SEEK_HEAD, unSize );
			}

			if ( ClientHTTP()->GetHTTPResponseHeaderSize( m_hHTTPRequest, "X-Template-File", &unSize ) && unSize > 0 )
			{
				char *pchNewMemory = new char[unSize];
				if ( ClientHTTP()->GetHTTPResponseHeaderValue( m_hHTTPRequest, "X-Template-File", (uint8*)pchNewMemory, unSize ) )
				{
#ifndef PANORAMA_USE_S1WRAPPER
					m_strLocalTemplatePath.SetPtr( pchNewMemory );
#else
					m_strLocalTemplatePath = pchNewMemory;
					delete[] pchNewMemory;
#endif
				}
				else
					delete[] pchNewMemory;
			}

			// Explicitly do not null terminate XML parser will do the right thing, and will actually barf if a null is in the buffer.
			//m_bufLayoutFile.PutChar( 0 );
		}

		if ( pParam->m_hRequest != INVALID_HTTPREQUEST_HANDLE )
		{
			ClientHTTP()->ReleaseHTTPRequest( pParam->m_hRequest );
			if ( pParam->m_hRequest == m_hHTTPRequest )
				m_hHTTPRequest = INVALID_HTTPREQUEST_HANDLE;
		}

		// Proceed to next step processing XML contents
		OnXMLContentsReady();
	}
	STEAM_CALLRESULT( CLoadLayoutFileAsync, HTTPRequestCompleted, HTTPRequestCompleted_t );

	void OnXMLContentsReady()
	{
		// If UIEngine has shutdown just stop doing further work for the job
		if ( !UIEngine() || !UIEngine()->UILayoutManager() )
		{
			delete this;
			return;
		}

		// Do quick parse of XML to find styles included, and then pre-load each of them before proceeding
		if ( m_bufLayoutFile.TellPut() > 0 )
		{
			CLayoutFileDependancyParser parser;
			if ( parser.BParseXML( m_bufLayoutFile ) )
			{
				FOR_EACH_VEC( parser.m_vecIncludes, i )
				{
					LayoutIncludeFile_t &includeFile = parser.m_vecIncludes[i];
					IncludeRequestData_t *pData = new IncludeRequestData_t;
					pData->m_eType = includeFile.m_eType;
					pData->m_pFile.Set( includeFile.m_strPath );
				
					// Only preload http based files for now
					if ( !pData->m_pFile.BIsHTTPURL() || UIEngineInternal()->UILayoutManagerInternal()->BIsIncludeBufferCached( pData->m_eType, pData->m_pFile.GetReferencePath() ) )
					{
						delete pData;
						continue;
					}

					// get file
					HTTPRequestHandle hRequest = ClientHTTP()->CreateHTTPRequest( k_EHTTPMethodGET, pData->m_pFile.GetReferencePath() );
					const CUtlVector<CUtlString> &vecCookies = pData->m_pFile.GetCookieHeadersForHTTPURL();
					if ( vecCookies.Count() )
					{
						HTTPCookieContainerHandle hCookieContainer = ClientHTTP()->CreateCookieContainer( true );

						char rgchDomain[1024];
						V_ExtractDomainFromURL( pData->m_pFile.GetReferencePath(), rgchDomain, V_ARRAYSIZE( rgchDomain ) );

						FOR_EACH_VEC( vecCookies, iCookie )
						{
							ClientHTTP()->SetCookie( hCookieContainer, rgchDomain, "/", vecCookies[iCookie].String() );
						}

						ClientHTTP()->SetHTTPRequestCookieContainer( hRequest, hCookieContainer );

						// Ok to release now, still active for the requests lifetime
						ClientHTTP()->ReleaseCookieContainer( hCookieContainer );
					}

					UIEngineInternal()->AddCommonHeadersToHttpRequest( m_hHTTPRequest );

					m_mapHTTPIncludeRequests.Insert( hRequest, pData );
					SteamAPICall_t hSteamAPICall;
					if ( ClientHTTP()->SendHTTPRequest( hRequest, &hSteamAPICall ) )
					{
						// Add call handle to get callback
						m_HTTPIncludeRequestCompleted.AddCall( hSteamAPICall );
					}
					else
					{
						m_bAllRemoteCSSLoaded = false;

						// Not really ready, but this handles failure too
						HTTPRequestCompleted_t callback;
						callback.m_bRequestSuccessful = false;
						callback.m_hRequest = m_hHTTPRequest;
						callback.m_eStatusCode = k_EHTTPStatusCode500InternalServerError;
						OnIncludeRequestFinished( &callback, false );
					}					
				}
			}
		}


		// If we have any pending http include requests we must wait on them now, otherwise proceed further immediately
		if ( m_mapHTTPIncludeRequests.Count() > 0 )
			return;
		else
			OnAllIncludesReady();
	}

	void OnIncludeRequestFinished( HTTPRequestCompleted_t *pParam, bool bIOFailure )
	{
		if ( bIOFailure || !pParam )
		{
			// Total failure... proceed without waiting on the rest as clientdll is probably
			// no longer working.
			m_bAllRemoteCSSLoaded = false;
			OnAllIncludesReady();
		}
		
		IncludeRequestData_t *pIncludeData = NULL;
		int iMap = m_mapHTTPIncludeRequests.Find( pParam->m_hRequest );
		Assert( iMap != m_mapHTTPIncludeRequests.InvalidIndex() );
		if ( iMap != m_mapHTTPIncludeRequests.InvalidIndex() )
		{
			pIncludeData = m_mapHTTPIncludeRequests[iMap];
			m_mapHTTPIncludeRequests.RemoveAt( iMap );
		}

		if ( !pParam->m_bRequestSuccessful || pParam->m_eStatusCode != k_EHTTPStatusCode200OK )
		{
			m_bAllRemoteCSSLoaded = false;
		}
		else
		{
			uint32 unSize;
			if ( ClientHTTP()->GetHTTPResponseBodySize( pParam->m_hRequest, &unSize ) )
			{
				CUtlBuffer bufInclude;
				bufInclude.EnsureCapacity( unSize );
				if ( ClientHTTP()->GetHTTPResponseBodyData( pParam->m_hRequest, (uint8*)bufInclude.Base(), unSize ) )
				{
					bufInclude.SeekPut( CUtlBuffer::SEEK_HEAD, unSize );

					if ( pIncludeData->m_eType == k_ELayoutIncludeFileTypeCSS )
						UIEngineInternal()->UILayoutManagerInternal()->OnLoadRemoteCSSFileFromBuffer( bufInclude, pIncludeData->m_pFile.GetReferencePath(), false );
					else if ( pIncludeData->m_eType == k_ELayoutIncludeFileTypeJS )
						UIEngineInternal()->UILayoutManagerInternal()->OnLoadRemoteJSFileFromBuffer( bufInclude, pIncludeData->m_pFile.GetReferencePath(), false );
				}
				else
					m_bAllRemoteCSSLoaded = false;
			}
			else
			{
				m_bAllRemoteCSSLoaded = false;
			}
		}

		SAFE_DELETE( pIncludeData );
		ClientHTTP()->ReleaseHTTPRequest( pParam->m_hRequest );

		// All all outstanding includes now loaded?
		if ( m_mapHTTPIncludeRequests.Count() == 0 )
			OnAllIncludesReady();

	}
	STEAM_CALLRESULT( CLoadLayoutFileAsync, HTTPIncludeRequestCompleted, HTTPRequestCompleted_t );

	void OnAllIncludesReady()
	{
		if( UIEngine() && UIEngineInternal()->UILayoutManagerInternal() )
		{
			ELoadLayoutAsyncDetails eDetails = m_bReceivedNotLoggedIn ? k_ELoadLayoutAsyncDetailsNotLoggedIn : k_ELoadLayoutAsyncDetailsNone;
			UIEngineInternal()->UILayoutManagerInternal()->OnLayoutFileBufferLoaded( m_bufLayoutFile, m_symPath, m_strLocalTemplatePath.String(), m_pNotifyPanel.Get(), m_bAllRemoteCSSLoaded, m_bWasFileReload, eDetails, m_bIsPartialLayout );
		}

		delete this;
	}


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_FileResource );
		ValidateObj( m_bufLayoutFile );

		ValidateObj( m_mapHTTPIncludeRequests );
		FOR_EACH_MAP_FAST( m_mapHTTPIncludeRequests, i )
		{
			validator.ClaimMemory( m_mapHTTPIncludeRequests[i] );
			ValidateObj( m_mapHTTPIncludeRequests[i]->m_pFile );
		}

		ValidateObj( m_strLocalTemplatePath );
	}
#endif

private:

	friend class CLayoutManager;
	CPanelPtr<IUIPanel> m_pNotifyPanel;
	bool m_bWasFileReload;
	bool m_bIsPartialLayout;
	bool m_bReceivedNotLoggedIn;
	bool m_bAllRemoteCSSLoaded;

	CUtlBuffer m_bufLayoutFile;
	HTTPRequestHandle m_hHTTPRequest;
	CUtlString m_strXMLString;
	CFileResource m_FileResource;
	CPanoramaSymbol m_symPath;
	CUtlString m_strLocalTemplatePath;

	struct IncludeRequestData_t
	{
		CFileResource m_pFile;
		ELayoutIncludeFileType m_eType;
	};
	CUtlMap< HTTPRequestHandle, IncludeRequestData_t *, int, CDefLess< HTTPRequestHandle > > m_mapHTTPIncludeRequests;
};

class CReloadStyleFile;
DECLARE_PANORAMA_EVENT1( RetryStyleReload, CReloadStyleFile* )
DEFINE_PANORAMA_EVENT( RetryStyleReload );


//-----------------------------------------------------------------------------
// Purpose: Helper job to reload a style from a remote URL
//-----------------------------------------------------------------------------
class CReloadStyleFile 
{
public:
	CReloadStyleFile( ELayoutIncludeFileType eFileType, CPanoramaSymbol symPath ) : m_FileResource( symPath.String() ), m_HTTPIncludeRequestCompleted( this, &CReloadStyleFile::OnIncludeRequestFinished )
	{
		m_eFileType = eFileType;
		m_symPath = symPath;
		m_unRetries = 2;

		RegisterForUnhandledEvent( RetryStyleReload(), this, &CReloadStyleFile::StartReload );

		LayoutManager().TrackAsyncReload( this );

		StartReload( this );
	}

	~CReloadStyleFile()
	{
		if ( UIEngine() )	
			UnregisterForUnhandledEvent( RetryStyleReload(), this, &CReloadStyleFile::StartReload );

		LayoutManager().ClearAsyncReload( this );
	}

	bool StartReload( CReloadStyleFile *pThis )
	{
		if ( pThis != this )
			return false;

		if ( !UIEngine() || --m_unRetries == 0 )
		{
			delete this;
			return true;
		}

		if ( m_FileResource.BIsHTTPURL() )
		{
			if ( !ClientHTTP() )
			{
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
				AssertMsg( false, "ClientHTTP() is NULL when using client interfaces." );
#endif
				// If ClientHTTP is NULL that means we're running with public ISteam interfaces and
				// Steam itself wasn't available. Just fail all requests
				HTTPRequestCompleted_t callback;
				callback.m_bRequestSuccessful = false;
				callback.m_hRequest = INVALID_HTTPREQUEST_HANDLE;
				callback.m_eStatusCode = k_EHTTPStatusCode501NotImplemented;
				OnIncludeRequestFinished( &callback, false );
				return true;
			}

			HTTPRequestHandle hRequest = ClientHTTP()->CreateHTTPRequest( k_EHTTPMethodGET, m_FileResource.GetReferencePath() );
			const CUtlVector<CUtlString> &vecCookies = m_FileResource.GetCookieHeadersForHTTPURL();
			if ( vecCookies.Count() )
			{
				HTTPCookieContainerHandle hCookieContainer = ClientHTTP()->CreateCookieContainer( true );

				char rgchDomain[1024];
				V_ExtractDomainFromURL( m_FileResource.GetReferencePath(), rgchDomain, V_ARRAYSIZE( rgchDomain ) );

				FOR_EACH_VEC( vecCookies, iCookie )
				{
					ClientHTTP()->SetCookie( hCookieContainer, rgchDomain, "/", vecCookies[iCookie].String() );
				}

				ClientHTTP()->SetHTTPRequestCookieContainer( hRequest, hCookieContainer );

				// Ok to release now, still active for the requests lifetime
				ClientHTTP()->ReleaseCookieContainer( hCookieContainer );
			}

			// This is a forced reload, so set no-cache
			ClientHTTP()->SetHTTPRequestHeaderValue( hRequest, "Cache-Control", "no-cache" );

			UIEngineInternal()->AddCommonHeadersToHttpRequest( hRequest );

			SteamAPICall_t hSteamAPICall;
			if ( ClientHTTP()->SendHTTPRequest( hRequest, &hSteamAPICall ) )
			{
				// Add call handle to get callback
				m_HTTPIncludeRequestCompleted.AddCall( hSteamAPICall );
			}
			else
			{
				// Not really ready, but this handles failure too
				HTTPRequestCompleted_t callback;
				callback.m_bRequestSuccessful = false;
				callback.m_hRequest = hRequest;
				callback.m_eStatusCode = k_EHTTPStatusCode500InternalServerError;
				OnIncludeRequestFinished( &callback, false );
			}
		}
		else
		{
			AssertMsg( false, "CReloadStyleFile only expects to be called for http paths" );
			delete this;
		}

		return true;
	}

	void OnIncludeRequestFinished( HTTPRequestCompleted_t *pParam, bool bIOFailure )
	{
		if ( bIOFailure || !pParam )
		{
			// Total failure... proceed without waiting on the rest as clientdll is probably
			// no longer working.
			delete this;
			return;
		}

		if ( !pParam->m_bRequestSuccessful || pParam->m_eStatusCode != k_EHTTPStatusCode200OK )
		{
			if ( pParam->m_hRequest != INVALID_HTTPREQUEST_HANDLE )
			{
				DispatchEventAsync( 1.0f, RetryStyleReload(), (IUIPanel*)NULL, this );

				ClientHTTP()->ReleaseHTTPRequest( pParam->m_hRequest );
			}
		}
		else
		{
			bool bDone = false;
			uint32 unSize;
			if ( ClientHTTP()->GetHTTPResponseBodySize( pParam->m_hRequest, &unSize ) )
			{
				CUtlBuffer bufInclude;
				bufInclude.EnsureCapacity( unSize );
				if ( ClientHTTP()->GetHTTPResponseBodyData( pParam->m_hRequest, (uint8*)bufInclude.Base(), unSize ) )
				{
					bufInclude.SeekPut( CUtlBuffer::SEEK_HEAD, unSize );

					if ( m_eFileType == k_ELayoutIncludeFileTypeJS )
						UIEngineInternal()->UILayoutManagerInternal()->OnLoadRemoteJSFileFromBuffer( bufInclude, m_symPath.String(), true );
					else
						UIEngineInternal()->UILayoutManagerInternal()->OnLoadRemoteCSSFileFromBuffer( bufInclude, m_symPath.String(), true );
						
					bDone = true;
				}
			}
			ClientHTTP()->ReleaseHTTPRequest( pParam->m_hRequest );
			if ( !bDone )
				DispatchEventAsync( 1.0f, RetryStyleReload(), (IUIPanel*)NULL, this );
			else
				delete this;
		}

	}
	STEAM_CALLRESULT( CReloadStyleFile, HTTPIncludeRequestCompleted, HTTPRequestCompleted_t );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_FileResource );
	}
#endif

private:

	friend class CLayoutManager;
	
	uint32 m_unRetries;
	CFileResource m_FileResource;
	CPanoramaSymbol m_symPath;
	ELayoutIncludeFileType m_eFileType;
};
}

DECLARE_PANORAMA_EVENT0( AutoReloadFailedLayoutReloads );
DEFINE_PANORAMA_EVENT( AutoReloadFailedLayoutReloads );


//-----------------------------------------------------------------------------
// Purpose: Loads from buffer
//-----------------------------------------------------------------------------
void CJSFile::LoadFromBuffer( CPanoramaSymbol symPath, const CUtlBuffer &buffer )
{
	m_Buffer.Purge();
	m_Buffer.SetBufferType( true, true );
	m_Buffer.CopyBuffer( buffer );
	m_symPath = symPath;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CJSFile::LoadFromFile( CPanoramaSymbol symPath )
{
	if ( UIEngine()->UIFileSystem()->LoadFileIntoBuffer( symPath.String(), m_Buffer, true ) )
	{
		m_symPath = symPath;
		return true;
	}
	else
	{
		m_symPath = CPanoramaSymbol();
		return false;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CJSFile::GetBuffer() const
{
	if( m_Buffer.TellPut() > 0 )
	{
		return (const char *)m_Buffer.Base();
	}
	else
	{
		return "";
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CJSFile::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_Buffer );
	ValidateObj( m_strOriginalPath );
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CLayoutManager::CLayoutManager() 
{
	RegisterForUnhandledEvent( AutoReloadFailedLayoutReloads(), this, &CLayoutManager::AutoReloadFailedFileLoads );
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CLayoutManager::~CLayoutManager()
{
	UnregisterForUnhandledEvent( AutoReloadFailedLayoutReloads(), this, &CLayoutManager::AutoReloadFailedFileLoads );

	FOR_EACH_RBTREE_FAST( m_treeInFlightAsyncLoads, i )
	{
		delete m_treeInFlightAsyncLoads[i];
	}
	m_treeInFlightAsyncLoads.RemoveAll();

	FOR_EACH_RBTREE_FAST( m_treeInFlightAsyncReloads, i )
	{
		delete m_treeInFlightAsyncReloads[i];
	}
	m_treeInFlightAsyncReloads.RemoveAll();
	
	// ref counted, just remove
	m_mapLayoutFiles.RemoveAll();

	// ref counted, just remove
	m_mapStyleFiles.RemoveAll();

	// ref counted, just remove
	m_mapJSFiles.RemoveAll();

	FOR_EACH_MAP( m_mapInMemoryFiles, i )
	{
		delete m_mapInMemoryFiles[i].m_pBuffer;
	}
	m_mapInMemoryFiles.RemoveAll();

	FOR_EACH_MAP( m_mapCachedStyleBuffers, i )
	{
		delete m_mapCachedStyleBuffers[i];
	}
	m_mapCachedStyleBuffers.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Figures out if we think we have a path locally that is the real source of a HTTP path
//-----------------------------------------------------------------------------
bool CLayoutManager::BConvertHTTPPathToLocalP4Path( const char *pchFile, CUtlString &strOut )
{
	CUtlString strHost;

	const char *rgchSites[] = { "/community/", "/store/" };

	for ( int iSite=0; iSite < V_ARRAYSIZE( rgchSites ); ++iSite )
	{
		const char *rgchProtocols[] = { "http", "https" };
		for ( int iProtocol = 0; iProtocol < V_ARRAYSIZE( rgchProtocols ); ++iProtocol )
		{
			for( int iHostOption=0; iHostOption < 4; ++iHostOption )
			{
				if ( iHostOption== 0 )
					strHost.Format( "%s://%s.valvesoftware.com%s", rgchProtocols[iProtocol], GetLocalHostname(), rgchSites[iSite] );
				else if ( iHostOption == 1 )
					strHost.Format( "%s://%s%s", rgchProtocols[iProtocol], GetLocalHostname(), rgchSites[iSite] );
				else if ( iHostOption == 2 )
					strHost.Format( "%s://127.0.0.1%s", rgchProtocols[iProtocol], rgchSites[iSite] );
				else if ( iHostOption == 3 )
					strHost.Format( "%s://localhost%s", rgchProtocols[iProtocol], rgchSites[iSite] );

				if ( V_strnicmp( pchFile, strHost.String(), V_strlen( strHost.String() ) ) == 0 )
				{
					char rgchSteamPath[2048];
					Plat_GetExecutablePath( rgchSteamPath, V_ARRAYSIZE( rgchSteamPath ) );
					V_StripFilename( rgchSteamPath );
					V_StripLastDir( rgchSteamPath, V_ARRAYSIZE( rgchSteamPath ) );

					// Path to dev steam community, success, try full path based on running exe path
					const char *pchFilePath = V_strstr( pchFile, rgchSites[iSite] );
					Assert( pchFilePath );
					if ( pchFilePath )
					{
						char *pchGetParams = (char*)V_strstr( pchFilePath, "?" );
						if ( pchGetParams )
							pchGetParams[0] = 0;

						// shared is remapped
						CFmtStr fmtShared( "%spublic/shared/", rgchSites[iSite] );
						if ( V_strnicmp( fmtShared.String(), pchFilePath, fmtShared.Length() ) == 0 )
							strOut.Format( "%sweb/shared/public/%s", rgchSteamPath, pchFilePath + fmtShared.Length() );
						else
							strOut.Format( "%sweb%s%s", rgchSteamPath, rgchSites[iSite], pchFilePath + V_strlen( rgchSites[iSite] ) );
						
						V_FixSlashes( strOut.Access(), CORRECT_PATH_SEPARATOR );

						if ( pchGetParams )
							pchGetParams[0] = '?';

						return true;
					}
				}
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a layout file from a string. Not cached
//-----------------------------------------------------------------------------
LayoutFilePtr_t CLayoutManager::GetLayoutFileFromString( const char *pchXMLString, bool bPartialLayout )
{	
	CSHA1 sha;
	sha.Update( (uint8*)pchXMLString, V_strlen( pchXMLString ) );
	sha.Final();	
		
	char rgchHashHex[256];
	sha.GetHashHex( rgchHashHex, sizeof( rgchHashHex ) );

	CUtlString strPath = "code://";
	strPath.Append( rgchHashHex );

	// return cached version
#ifdef PANORAMA_USE_S1WRAPPER
	LayoutFilePtr_t pExisting = m_mapLayoutFiles.FindElement( strPath.String(), NULL );
	if ( pExisting.Get() )
		return pExisting;
#else
	LayoutFilePtr_t *ppExisting = m_mapLayoutFiles.FindGetPtr( strPath.String() );
	if ( ppExisting )
	{
		Assert( ppExisting->Get() );
		return *ppExisting;
	}
#endif // PANORAMA_USE_S1WRAPPER

	CUtlBuffer buf( pchXMLString, V_strlen( pchXMLString ), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
	LayoutFilePtr_t pLayoutFile( kNoAddRef, new CLayoutFile() );

	if ( pLayoutFile->BLoadFromBuffer( strPath.String(), buf, bPartialLayout ) )
	{
		m_mapLayoutFiles.Insert( strPath.String(), pLayoutFile );
		return pLayoutFile;
	}

	AssertMsg( false, "Couldn't load layout file from passed string" );
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: loads or retrieves a cached layout file
//-----------------------------------------------------------------------------
LayoutFilePtr_t CLayoutManager::GetCLayoutFile( const char *pchFile, bool bPartialLayout )
{
	CFileResource fileResource( pchFile );
	CUtlString strPath = fileResource.GetReferencePath();

#if defined( SOURCE2_PANORAMA )
	// Ensure the system normalizes to the expected content version.
	CUtlString fixedFilename;
	if ( fileResource.BIsLocalPath() && UIEngine()->UIFileSystem()->RestoreContentFilename( strPath.Get(), fixedFilename ) )
	{
		strPath = fixedFilename;
	}
#endif

	CPanoramaSymbol symPath( strPath.String() );
	LayoutFilePtr_t pLayoutFile = GetCLayoutFile( symPath );
	if ( pLayoutFile.Get() )
		return pLayoutFile;

	// not found
	if ( fileResource.BIsLocalPath() )
	{
		pLayoutFile.SetNoRef( new CLayoutFile() );
		ELoadLayoutFileResult result = pLayoutFile->LoadFromFile( strPath.String(), bPartialLayout );
		if ( result != k_ELoadLayoutFileOK )
		{
			AssertMsg2( result == k_ELoadLayoutFileOK, "Couldn't read %s: %d\n", strPath.String(), result );
			return NULL;
		}

		m_mapLayoutFiles.Insert( symPath, pLayoutFile );
		return pLayoutFile;
	}
	else
	{
		AssertMsg( false, "Only local paths should come into GetLayoutFile(), did you call syncronous BLoadLayout() with a http:// path?  You must use LoadLayoutAsync then." );
		return NULL;
	}

	// unreachable
}


//-----------------------------------------------------------------------------
// Purpose: Returns raw layout file pointer. Be careful with scope!
//-----------------------------------------------------------------------------
IUILayoutFile *CLayoutManager::GetLayoutFile( const char *pchFile, bool bPartialLayout )
{
	LayoutFilePtr_t ptr = GetCLayoutFile( pchFile, bPartialLayout );
	return (IUILayoutFile *)ptr.Get();
}


//-----------------------------------------------------------------------------
// Purpose: Returns raw layout file pointer. Be careful with scope!
//-----------------------------------------------------------------------------
IUILayoutFile *CLayoutManager::GetLayoutFile( CPanoramaSymbol symPath )
{
	LayoutFilePtr_t ptr = GetCLayoutFile( symPath );
	return (IUILayoutFile *)ptr.Get();
}


//-----------------------------------------------------------------------------
// Purpose: loads or retrieves a cached layout file asynchronously
//-----------------------------------------------------------------------------
void CLayoutManager::GetLayoutFileFromStringAsync( const char *pchXMLString, IUIPanel *pNotifyPanel, bool bPartialLayout )
{
	CUtlString strPath;

	CSHA1 sha;
	sha.Update( (uint8*)pchXMLString, V_strlen( pchXMLString ) );
	sha.Final();
	strPath = "code://";

	char rgchHashHex[256];
	sha.GetHashHex( rgchHashHex, sizeof( rgchHashHex ) );
	strPath.Append( rgchHashHex );

	CFileResource resourceInvalid;
	new CLoadLayoutFileAsync( resourceInvalid, pchXMLString, strPath.String(), pNotifyPanel, false, bPartialLayout );
}

//-----------------------------------------------------------------------------
// Purpose: loads or retrieves a cached layout file asynchronously
//-----------------------------------------------------------------------------
void CLayoutManager::GetLayoutFileAsync( const char *pchFile, IUIPanel *pNotifyPanel, bool bPartialLayout )
{
	CFileResource fileResource( pchFile );
	if ( !fileResource.BIsValid() )
	{
		if ( pNotifyPanel )
			((CUIPanel*)pNotifyPanel)->OnGetLayoutFileAsyncComplete( NULL, k_ELoadLayoutAsyncDetailsNone, bPartialLayout );

		return;
	}

	CPanoramaSymbol symPath( fileResource.GetReferencePath() );

	// If local path, then check our cache and use it.  If it's a remote path we rely on transport
	// level caches and we explicitly do not use our cache as it will violate transport level caching
	// policy.
	if ( fileResource.BIsLocalPath() )
	{
		LayoutFilePtr_t pLayoutFile = GetCLayoutFile( symPath );
		if ( pLayoutFile.Get() )
		{
			if ( pNotifyPanel )
				((CUIPanel*)pNotifyPanel)->OnGetLayoutFileAsyncComplete( pLayoutFile, k_ELoadLayoutAsyncDetailsNone, bPartialLayout );

			return;
		}
	}
	new CLoadLayoutFileAsync( fileResource, NULL, symPath, pNotifyPanel, false, bPartialLayout );
}


//-----------------------------------------------------------------------------
// Purpose: check if CSS for file/http path is already cached
//-----------------------------------------------------------------------------
bool CLayoutManager::BIsIncludeBufferCached( ELayoutIncludeFileType eType, const char *pchPath )
{
	CPanoramaSymbol symPath = StripVersionGetParamFromURL( pchPath ).String();

	if ( eType == k_ELayoutIncludeFileTypeJS )
	{
#ifdef PANORAMA_USE_S1WRAPPER
		JSFilePtr_t pFile = m_mapJSFiles.FindElement( symPath, NULL );
		if ( !pFile )
			return false;

		const char *pchOriginal = pFile->GetOriginalPath();
#else
		JSFilePtr_t *ppFile = m_mapJSFiles.FindGetPtr( symPath );
		if ( !ppFile )
			return false;

		const char *pchOriginal = (*ppFile)->GetOriginalPath();
#endif	// PANORAMA_USE_S1WRAPPER 
		if ( V_isempty( pchOriginal ) )
			return true;

		return ( V_strcmp( pchPath, pchOriginal ) == 0 );
	}
	else if ( eType == k_ELayoutIncludeFileTypeCSS )
		return m_mapCachedStyleBuffers.HasElement( symPath );
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to strip v= param from URLs, since we use this as a contents hash for caching
// but want to consider URLs with differing hash values as equivalent for JS/CSS file reload
//-----------------------------------------------------------------------------
CUtlString CLayoutManager::StripVersionGetParamFromURL( const char *pchURL ) const
{
	// Is this a http/https url?
	if( V_strncmp( pchURL, "http://", 7 ) != 0 && V_strncmp( pchURL, "https://", 8 ) != 0 )
		return pchURL;

	char *pszQuestionMark = V_strrchr( (char*)pchURL, '?' );
	if( !pszQuestionMark )
		return pchURL;

	*pszQuestionMark = '\0';

	char *pchRemaining = pszQuestionMark + 1;

	CUtlStringBuilder strOut;
	strOut.Append( pchURL );

	CUtlVector< CUtlString > vecGetParams;
	V_SplitString( pchRemaining, "&", vecGetParams );

	bool bFirst = true;
	FOR_EACH_VEC( vecGetParams, i )
	{
		// Skip v= param which is version/hash/etag stuff and we want to consider files with differing values
		// to still be the same resource.  Note that this is correct for Valve/Steam CSS/JS but maybe not every
		// non Valve thing, so this could be wrong someday if we use panorama more broadly... Sorry.
		if( V_strncmp( vecGetParams[i].String(), "v=", 2 ) == 0 )
			continue;

		if( bFirst )
		{
			strOut.Append( "?" );
			bFirst = false;
		}
		else
			strOut.Append( "&" );

		strOut.Append( vecGetParams[i].String() );
	}

	// Restore original string since we said it was const as input
	*pszQuestionMark = '?';

	return strOut.String();
}


//-----------------------------------------------------------------------------
// Purpose: Inserts into local -> http map, removing key/value dups
//-----------------------------------------------------------------------------
void CLayoutManager::InsertLocalToHTTP( const CUtlString &filePath, CPanoramaSymbol symPath )
{
	int iMap = m_mapLocalP4PathsToHTTPPaths.FindFirst( filePath.String() );
	while ( iMap != m_mapLocalP4PathsToHTTPPaths.InvalidIndex() )
	{
		if ( m_mapLocalP4PathsToHTTPPaths.Element( iMap ) == symPath )
			return;

		iMap = m_mapLocalP4PathsToHTTPPaths.NextInorderSameKey( iMap );
	}

	// Allow dupes, because a local path may be something like a php partial template used in multiple layout files
	m_mapLocalP4PathsToHTTPPaths.InsertWithDupes( filePath.String(), symPath );
}


//-----------------------------------------------------------------------------
// Purpose: Updates local p4 to http path
//-----------------------------------------------------------------------------
void CLayoutManager::AddToLocalP4Paths( const CUtlString &filePath, CPanoramaSymbol symPath )
{
	// If this was NOT a reload we may need to add dir watcher and mapping to local file
	CUtlString strLocal;
	if ( BConvertHTTPPathToLocalP4Path( filePath.String(), strLocal ) )
	{
		CUtlString strLocalDir = strLocal;

		// Strip filename, which also removes trailing slash, we want trailing slash back
		V_StripFilename( strLocalDir.Access() );
		strLocalDir.Append( CORRECT_PATH_SEPARATOR_S );

		// The file watcher is not super efficient, so only use if -dev specified or if debugging
#if !defined( SOURCE2_PANORAMA )
		bool bWatch = CommandLine()->CheckParm( g_DevMode.GetHParam() ) || Plat_IsInDebugSession();
#else
		bool bWatch = CommandLine()->HasParm( "-dev" ) || Plat_IsInDebugSession();
#endif
		if ( bWatch )
			UIEngine()->AddDirectoryChangeWatch( strLocalDir.String() );

		InsertLocalToHTTP( strLocal, symPath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: callback used internally by loader job as it pre-caches styles files
//-----------------------------------------------------------------------------
void CLayoutManager::OnLoadRemoteCSSFileFromBuffer( CUtlBuffer &bufFile, const CUtlString &filePath, bool bReloadStylesForFile )
{
	CPanoramaSymbol symPath = StripVersionGetParamFromURL( filePath.String() ).String();
	int iMap = m_mapCachedStyleBuffers.Find( symPath );
	if ( iMap != m_mapCachedStyleBuffers.InvalidIndex() )
	{
		// assume new data is better data?
		delete m_mapCachedStyleBuffers[iMap];
	}

	CUtlBuffer *pBufNew = new CUtlBuffer( bufFile.TellPut(), 0, CUtlBuffer::TEXT_BUFFER );
	pBufNew->Put( bufFile.Base(), bufFile.TellPut() );
	m_mapCachedStyleBuffers.Insert( symPath, pBufNew );

	if ( bReloadStylesForFile )
	{
		// If this was a reload we already have watched/added p4 mapping, just tell the file to reload now
		ReloadStyleFile( symPath );
	}
	else
	{
		AddToLocalP4Paths( filePath, symPath );		
	}
}


//-----------------------------------------------------------------------------
// Purpose: callback used internally by loader job as it pre-caches styles files
//-----------------------------------------------------------------------------
void CLayoutManager::OnLoadRemoteJSFileFromBuffer( CUtlBuffer &bufFile, const CUtlString &filePath, bool bReloadStylesForFile )
{
	CPanoramaSymbol symPath = StripVersionGetParamFromURL( filePath.String() ).String();

	JSFilePtr_t pJSFile( kNoAddRef, new CJSFile() );
	pJSFile->LoadFromBuffer( filePath.String(), bufFile );
	pJSFile->SetOriginalPath( filePath );
	m_mapJSFiles.InsertOrReplace( symPath, pJSFile );

	if ( bReloadStylesForFile )
	{
		// bugbug jmccaskey - Also re-execute JS files for path?
	}
	else
	{
		AddToLocalP4Paths( filePath, symPath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Callback when async layout file loading job finishes
//-----------------------------------------------------------------------------
void CLayoutManager::OnLayoutFileBufferLoaded( CUtlBuffer &bufFile, CPanoramaSymbol symName, const char *pchLocalP4Path, IUIPanel *pNotifyPanel, bool bRemoteCSSAllLoaded, bool bWasFileReload, ELoadLayoutAsyncDetails eDetails, bool bIsPartialLayout )
{
	if ( !bRemoteCSSAllLoaded )
	{
#if defined( _DEBUG ) && !defined( SOURCE2_PANORAMA )
		AssertMsg2( false, "!! WARNING: Panorama couldn't fetch all remote css included by %s, panel %s won't have correct layout loaded", symName.String(), pNotifyPanel ? pNotifyPanel->GetID(): "unknown" );
#endif
		Msg( "!! WARNING: Panorama couldn't fetch all remote css included by %s, panel %s won't have correct layout loaded\n", symName.String(), pNotifyPanel ? pNotifyPanel->GetID(): "unknown" );
		if ( pNotifyPanel )
			((CUIPanel*)pNotifyPanel)->OnGetLayoutFileAsyncComplete( NULL, eDetails, bIsPartialLayout );

		return;
	}

	if ( bufFile.TellPut() == 0 )
	{
#if defined( _DEBUG ) && !defined( SOURCE2_PANORAMA )
		AssertMsg2( false, "!! WARNING: Panorama couldn't fetch or got 0 byte result for layout %s, panel %s won't have correct layout loaded", symName.String(), pNotifyPanel ? pNotifyPanel->GetID(): "unknown" );
#endif
		Msg( "!! WARNING: Panorama couldn't fetch or got 0 byte result for layout %s, panel %s won't have correct layout loaded\n", symName.String(), pNotifyPanel ? pNotifyPanel->GetID(): "unknown" );
		if ( pNotifyPanel )
			((CUIPanel*)pNotifyPanel)->OnGetLayoutFileAsyncComplete( NULL, eDetails, bIsPartialLayout );

		return;
	}

	int iMap = m_mapLayoutFiles.Find( symName );
	if ( bWasFileReload )
	{
		Assert( iMap != m_mapLayoutFiles.InvalidIndex() );
		if ( iMap != m_mapLayoutFiles.InvalidIndex() )
		{
			m_mapLayoutFiles[iMap]->BReloadFromBuffer( bufFile );
			UIEngine()->ReloadLayoutFile( symName );
		}
		return;
	}

	LayoutFilePtr_t pLayoutFile( kNoAddRef, new CLayoutFile() );



	if ( !pLayoutFile->BLoadFromBuffer( symName, bufFile, bIsPartialLayout ) )
	{
#if defined( _DEBUG ) && !defined( SOURCE2_PANORAMA )
		AssertMsg2( false, "!! WARNING: Panorama couldn't read or parse %s, panel %s won't have correct layout loaded", symName.String(), pNotifyPanel ? pNotifyPanel->GetID() : "unknown" );
#endif
		Msg( "!! WARNING: Panorama couldn't read or parse %s, panel %s won't have correct layout loaded\n", symName.String(), pNotifyPanel ? pNotifyPanel->GetID() : "unknown" );
		pLayoutFile = nullptr;

		if ( pNotifyPanel )
			( ( CUIPanel* )pNotifyPanel )->OnGetLayoutFileAsyncComplete( NULL, eDetails, bIsPartialLayout );

		return;
	}
	if ( iMap != m_mapLayoutFiles.InvalidIndex() )
	{
		// Duplicate load...this can happen since it's async. Swap out the old one with the new one. Note that
		// this will likely not actually delete the old one immediately because there are probably panels still holding a
		//  reference to it. Those should get cleaned up in OnGetLayoutFileAsyncComplete.
		m_mapLayoutFiles[ iMap ] = pLayoutFile;
	}
	else
	{
		m_mapLayoutFiles.Insert( symName, pLayoutFile );
	}

	if ( pNotifyPanel )
		( ( CUIPanel* )pNotifyPanel )->OnGetLayoutFileAsyncComplete( pLayoutFile, eDetails, bIsPartialLayout );

	if ( pchLocalP4Path )
	{
		CUtlVector<CUtlString> vecWatchFiles;
		V_SplitString( pchLocalP4Path, ";", vecWatchFiles );
		FOR_EACH_VEC( vecWatchFiles, i )
		{
			CUtlString strFullPath = vecWatchFiles[ i ];

			// Strip filename, which also removes trailing slash, we want trailing slash back
			V_StripFilename( vecWatchFiles[ i ].Access() );
			vecWatchFiles[ i ].Append( CORRECT_PATH_SEPARATOR_S );

			// The file watcher is not super efficient, so only use if -dev specified or if debugging
#if !defined( SOURCE2_PANORAMA )
			bool bWatch = CommandLine()->CheckParm( g_DevMode.GetHParam() ) || Plat_IsInDebugSession();
#else
			bool bWatch = CommandLine()->HasParm( "-dev" ) || Plat_IsInDebugSession();
#endif
			if ( bWatch )
				UIEngine()->AddDirectoryChangeWatch( vecWatchFiles[ i ].String() );

			InsertLocalToHTTP( strFullPath, symName );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: loads or retrieves a cached layout file
//-----------------------------------------------------------------------------
LayoutFilePtr_t CLayoutManager::GetCLayoutFile( CPanoramaSymbol symPath )
{
	// check if we have the file cached
	int iMap = m_mapLayoutFiles.Find( symPath );
	if ( iMap != m_mapLayoutFiles.InvalidIndex() )
		return m_mapLayoutFiles.Element( iMap );

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: loads or retrieves a cached javascript file
//-----------------------------------------------------------------------------
JSFilePtr_t CLayoutManager::GetJavaScriptFile( const char *pchFile, CUtlString *pOutResolvedPath )
{
	*pOutResolvedPath = "";
	if( !pchFile )
		return NULL;

	CFileResource fileResource( pchFile );
	if( !fileResource.BIsValid() )
		return NULL;

	CUtlString strPath = fileResource.GetReferencePath();
	if( fileResource.BIsLocalPath() )
	{
		if( !V_isstrlower_fast( pchFile ) )
		{
			AssertMsg1( false, "All included files need to be lower cased: %s", pchFile );
			return NULL;
		}
	}

#if defined( SOURCE2_PANORAMA )
	// Ensure the system normalizes to the expected content version.
	CUtlString fixedFilename;
	if ( fileResource.BIsLocalPath() && UIEngine()->UIFileSystem()->RestoreContentFilename( strPath.Get(), fixedFilename ) )
	{
		strPath = fixedFilename;
	}
#endif

	*pOutResolvedPath = strPath;

	CPanoramaSymbol symPath = StripVersionGetParamFromURL( strPath.String() ).String();
	
	int iMapJS = m_mapJSFiles.Find( symPath );
	if( iMapJS != m_mapJSFiles.InvalidIndex() )
	{
		return m_mapJSFiles[ iMapJS ];
	}

	// not found... if it's a local path we can load now, if it was a http path we MUST have pre-cached it, otherwise fail now
	if( fileResource.BIsHTTPURL() )
	{
		// Find the pre-cached buffer contents, we still need to parse in the context of the passed set...
		AssertMsg1( false, "%s was not pre-cached during layout file load, which shouldn't happen", symPath.String() );
		return NULL;
	}
	else
	{
		JSFilePtr_t pJSFile( kNoAddRef, new CJSFile() );


		if ( !pJSFile->LoadFromFile( symPath ) )
			return NULL;

		m_mapJSFiles.Insert( symPath, pJSFile );
		return pJSFile;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the normalized version of the file path. Optionally return
// the corresponding CFileResource.
//-----------------------------------------------------------------------------
bool CLayoutManager::BNormalizeStyleFilePath( const char *pchFile, CPanoramaSymbol &symPath, CFileResource *pFileResource /* = nullptr */, CUtlString *pstrPath /* = nullptr */ ) const
{
	if ( !pchFile )
		return false;

	CFileResource fileResource;
	if ( !pFileResource )
	{
		pFileResource = &fileResource;
	}

	CUtlString strPath;
	if ( !pstrPath )
	{
		pstrPath = &strPath;
	}

	pFileResource->Set( pchFile );
	if ( !pFileResource->BIsValid() )
		return false;

	*pstrPath = pFileResource->GetReferencePath();
	if ( pFileResource->BIsLocalPath() )
	{
		if ( !V_isstrlower_fast( pchFile ) )
		{
			AssertMsg1( false, "All included files need to be lower cased: %s", pchFile );
			return false;
		}
	}

#if defined( SOURCE2_PANORAMA )
	// Ensure the system normalizes to the expected content version.
	CUtlString fixedFilename;
	if ( pFileResource->BIsLocalPath() && UIEngine()->UIFileSystem()->RestoreContentFilename( pstrPath->Get(), fixedFilename ) )
	{
		*pstrPath = fixedFilename;
	}
#endif

	symPath = StripVersionGetParamFromURL( pstrPath->String() ).String();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: loads or retrieves a cached style file
//-----------------------------------------------------------------------------
StyleFilePtr_t CLayoutManager::GetStyleFile( const char *pchFile, const CStyleFileSet &existingFileSet, uint &unFileOrder, bool bUseCache )
{
	CPanoramaSymbol symPath;
	CFileResource fileResource;
	CUtlString strPath;
	if ( !BNormalizeStyleFilePath( pchFile, symPath, &fileResource, &strPath ) )
		return nullptr;

	// see if we have already loaded this style
	CUtlKeyPtr< CStyleFileKey > pKey = nullptr;
	if ( bUseCache )
	{
		pKey = new CStyleFileKey( existingFileSet, symPath );
		int iMap = m_mapStyleFiles.Find( pKey );
		if ( iMap != m_mapStyleFiles.InvalidIndex() )
			return m_mapStyleFiles.Element( iMap );
	}

	CUtlBuffer *pbufStyleFile = nullptr;
	CUtlBuffer bufStyleFile;

	int iInMemory = m_mapInMemoryFiles.Find( symPath );
	if ( iInMemory != m_mapInMemoryFiles.InvalidIndex() ) // Have we edited this file in-memory? If so, load that data.
	{
		pbufStyleFile = m_mapInMemoryFiles[ iInMemory ].m_pBuffer;
	}
	else if ( fileResource.BIsHTTPURL() ) // is it a remote path?
	{
		// If it was a http path we MUST have pre-cached it, otherwise fail now. We still need to parse in the context of the passed set...
		int iMapBuffer = m_mapCachedStyleBuffers.Find( symPath );
		if ( iMapBuffer == m_mapCachedStyleBuffers.InvalidIndex() )
		{
			AssertMsg1( false, "%s was not pre-cached during layout file load, which shouldn't happen", symPath.String() );
			return nullptr;
		}

		pbufStyleFile = m_mapCachedStyleBuffers[ iMapBuffer ];
	}
	else // local file
	{
		if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( strPath.String(), bufStyleFile, true ) )
		{
			AssertMsg1( false, "Unable to load local style file %s", strPath.String() );
			return nullptr;
		}

		pbufStyleFile = &bufStyleFile;
	}

	StyleFilePtr_t pStyleFile( kNoAddRef, new CStyleFile() );
	if ( !pStyleFile->BLoadFromBuffer( *pbufStyleFile, symPath, existingFileSet.GetStyleFiles(), unFileOrder ) )
		return nullptr;

	if ( bUseCache )
	{
		m_mapStyleFiles.Insert( pKey, pStyleFile );
	}

	m_treeKnownStyleFiles.Insert( symPath );
	return pStyleFile;
}


//-----------------------------------------------------------------------------
// Purpose: checks for changed layout and style files, and reloads any that changed
//-----------------------------------------------------------------------------
void CLayoutManager::ReloadChangedFile( const char *pchFile )
{
	AutoReloadFailedFileLoads(); // try any pending failed loads

	bool bFirstIteration = true;

	// First, see if this is a file we are watching but really a remote http path
	int iRemotePath = m_mapLocalP4PathsToHTTPPaths.FindFirst( pchFile );
	while ( bFirstIteration || iRemotePath != m_mapLocalP4PathsToHTTPPaths.InvalidIndex() )
	{
		bFirstIteration = false;
		if ( iRemotePath != m_mapLocalP4PathsToHTTPPaths.InvalidIndex() )
		{
			pchFile = m_mapLocalP4PathsToHTTPPaths[iRemotePath].String();
		}

		CUtlString strFixedFile;
		if ( !UIEngine()->UIFileSystem()->RestoreContentFilename( pchFile, strFixedFile ) ) \
		{
			strFixedFile = pchFile;
		}

		CPanoramaSymbol symPath( strFixedFile );

		// if the file is already in memory, warn the user. Throw out if necessary
		int iInMemory = m_mapInMemoryFiles.Find( symPath );
		if ( iInMemory != m_mapInMemoryFiles.InvalidIndex() )
		{
			CFmtStr1024 fmtStr( "The following file has been changed on disk but is also edited in memory. Would you like to throw out in memory changes?\n\nFile:\n%s", symPath.String() );
			if ( !UIEngine()->ShowNativeTopMostMessageBox( fmtStr, "File changed in memory and on disk", IUIEngine::k_ENativeMessageYesNo ) )
				return;

			// throw out in memory changes for when we reload below
			delete m_mapInMemoryFiles.Element( iInMemory ).m_pBuffer;
			m_mapInMemoryFiles.RemoveAt( iInMemory );
		}

		// check if this was a remotely loaded file we need to reload asyncronously
		const char *pchPath = symPath.String();
		if ( V_strnicmp( pchPath, "http://", 7 ) == 0 || V_strnicmp( pchPath, "https://", 8 ) == 0 )
		{
			// check if this is a style we have loaded
			if ( m_treeKnownStyleFiles.HasElement( symPath ) )
				new CReloadStyleFile( k_ELayoutIncludeFileTypeCSS, symPath );

			if ( m_mapJSFiles.HasElement( symPath ) )
			{
				//new CReloadStyleFile( k_ELayoutIncludeFileTypeJS, symPath );

				// figure out all layout files that refer to the script
				FOR_EACH_MAP( m_mapLayoutFiles, iLayout )
				{
					LayoutFilePtr_t pLayoutFile = m_mapLayoutFiles[iLayout];
					const CUtlVector< JSInclude_t > &vecIncludes = pLayoutFile->GetLayoutFileScriptIncludes();
					FOR_EACH_VEC( vecIncludes, iScript )
					{
						const JSInclude_t &jsInclude = vecIncludes[iScript];
						if ( StripVersionGetParamFromURL( jsInclude.strFilenameResolved ) != symPath.String() )
							continue;

						// can't reload layout files from code
						const char *pchLayoutFile = pLayoutFile->GetLayoutFileSymbol().String();
						if ( strncmp( "code://", pchLayoutFile, 7 ) == 0 )
							continue;

						CFileResource fileResource( pchLayoutFile );
						new CLoadLayoutFileAsync( fileResource, NULL, pLayoutFile->GetLayoutFileSymbol().String(), NULL, true, pLayoutFile->BIsPartialLayout() );
						break;
					}
				}
			}

			// check if this is a layout file we have loaded
			int iMap = m_mapLayoutFiles.Find( symPath );
			if( m_mapLayoutFiles.InvalidIndex() != iMap )
			{
				CFileResource fileResource( symPath.String() );
				new CLoadLayoutFileAsync( fileResource, NULL, symPath, NULL, true, m_mapLayoutFiles[iMap]->BIsPartialLayout() );
			}

		}
		else
		{
			// check if this is a layout file we have loaded
			if ( m_mapLayoutFiles.HasElement( symPath ) )
				ReloadLayoutFile( symPath );

			// check if this is a style we have loaded
			if ( m_treeKnownStyleFiles.HasElement( symPath ) )
				ReloadStyleFile( symPath );

			// check if this is a script we have loaded
			if ( m_mapJSFiles.HasElement( symPath ) )
				ReloadScriptFile( symPath );
		}

		if ( iRemotePath != m_mapLocalP4PathsToHTTPPaths.InvalidIndex() )
			iRemotePath = m_mapLocalP4PathsToHTTPPaths.NextInorderSameKey( iRemotePath );
	}

	// schedule a failed load reload if needed
	if( m_vecFilesFailedRead.Count() > 0 )
		DispatchEventAsync( 0.1f, AutoReloadFailedLayoutReloads(), (IUIPanel*)NULL );

}


//-----------------------------------------------------------------------------
// Purpose: reload any failed file loads
//-----------------------------------------------------------------------------
bool CLayoutManager::AutoReloadFailedFileLoads()
{
	// reload any files that previously failed
	CUtlVector< CPanoramaSymbol > vecFailed;
	m_vecFilesFailedRead.Swap( vecFailed );
	FOR_EACH_VEC_BACK( vecFailed, i )
	{
		CPanoramaSymbol symPath = vecFailed[i];

		// check if this is a layout file we have loaded
		if ( m_mapLayoutFiles.Find( symPath ) != m_mapLayoutFiles.InvalidIndex() )
			ReloadLayoutFile( symPath );

		/// check if this is a style we have loaded
		if ( m_treeKnownStyleFiles.HasElement( symPath ) )
			ReloadStyleFile( symPath );

		/// check if this is a script we have loaded
		if ( m_mapJSFiles.HasElement( symPath ) )
			ReloadScriptFile( symPath );
	}

	// schedule a failed load reload if needed
	if( m_vecFilesFailedRead.Count() > 0 )
		DispatchEventAsync( 0.1f, AutoReloadFailedLayoutReloads(), (IUIPanel*)NULL );

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: triggers a reload of a layout file
//-----------------------------------------------------------------------------
void CLayoutManager::ReloadLayoutFile( CPanoramaSymbol symPath )
{
	// find the specified layout file and reload
	int iMap = m_mapLayoutFiles.Find( symPath );
	if ( iMap == m_mapLayoutFiles.InvalidIndex() )
		return;

	ELoadLayoutFileResult eResult = m_mapLayoutFiles.Element( iMap )->Reload();
	if ( eResult == k_ELoadLayoutFileReadFailed )
		m_vecFilesFailedRead.AddToTail( symPath );

	if ( eResult == k_ELoadLayoutFileOK )
		UIEngine()->ReloadLayoutFile( symPath );
}


//-----------------------------------------------------------------------------
// Purpose: triggers a reload of a style file
//-----------------------------------------------------------------------------
void CLayoutManager::ReloadStyleFile( CPanoramaSymbol symPath )
{
	// find all style sets that are/include the specified style file and reload
	// we walk this map in order to make sure that we reload shorter style keys first (depends on CStyleFileKey less than operator)
	// For example, if you have a style file that has a style load order of: a.css -> b.css -> c.css, and we are reloading a.css,
	// we would reload the 3 CStyleFiles: a, a + b, a + b + c

	CUtlVector< StyleFilePtr_t > vecPrevStyleFiles;
	FOR_EACH_MAP( m_mapStyleFiles, iMap )
	{
		CUtlKeyPtr< CStyleFileKey > &pKey = m_mapStyleFiles.Key( iMap );
		StyleFilePtr_t pStyleFile = m_mapStyleFiles.Element( iMap );

		bool bReferencesStyleFile = pKey->BContainsStyleFile( symPath ) || pStyleFile->BImportsStyleFile( symPath );
		if ( !bReferencesStyleFile && pKey->Length() > 1 )
		{
			CUtlKeyPtr< CStyleFileKey > pKeyPartial = new CStyleFileKey();
			for ( int i = 0; i < pKey->Length() - 1; ++i )
			{
				pKeyPartial->Append( pKey->GetFileSymbol( i ) );

				int iPartial = m_mapStyleFiles.Find( pKeyPartial );
				if ( iPartial != m_mapStyleFiles.InvalidIndex() )
				{
					StyleFilePtr_t pPartialStyleFile = m_mapStyleFiles.Element( iPartial );
					bReferencesStyleFile = pPartialStyleFile->BImportsStyleFile( symPath );

					if ( bReferencesStyleFile )
						break;
				}
			}
		}

		if ( !bReferencesStyleFile )
			continue;

		// see if content for this style file is already in memory
		CPanoramaSymbol symFile = StripVersionGetParamFromURL( pStyleFile->GetPathSymbol().String() ).String();

		int iMemory = m_mapInMemoryFiles.Find( symFile );
		CUtlBuffer *pInMemory = ( iMemory != m_mapInMemoryFiles.InvalidIndex() ) ? m_mapInMemoryFiles.Element( iMemory ).m_pBuffer : NULL;
		if ( !pInMemory )
		{
			int iRemote = m_mapCachedStyleBuffers.Find( symFile );
			if ( iRemote != m_mapCachedStyleBuffers.InvalidIndex() )
				pInMemory = m_mapCachedStyleBuffers[iRemote];
		}

		// need to build a vector of previous style files so defines can be replaced correctly		
		vecPrevStyleFiles.RemoveAll();
		uint unStartingFileOrder = 0;

		CUtlKeyPtr< CStyleFileKey > pLookupKey = new CStyleFileKey();
		for ( int iKey = 0; iKey < pKey->Length(); iKey++ )
		{
			pLookupKey->Append( pKey->GetFileSymbol( iKey ) );
#ifdef PANORAMA_USE_S1WRAPPER
			StyleFilePtr_t pLookupElement = m_mapStyleFiles.FindElement( pLookupKey, NULL );
			if ( !pLookupElement.IsValid() )
				continue;

			vecPrevStyleFiles.AddToTail( pLookupElement );
			unStartingFileOrder = pLookupElement->GetMaxFileOrder() + 1;
#else
			StyleFilePtr_t *ppLookupElement = m_mapStyleFiles.FindGetPtr( pLookupKey );
			if ( !ppLookupElement || !ppLookupElement->IsValid() )
				continue;

			vecPrevStyleFiles.AddToTail( *ppLookupElement );
			unStartingFileOrder += (*ppLookupElement)->GetMaxFileOrder() + 1;
#endif	// PANORAMA_USE_S1WRAPPER
		}

		// reload
		if ( pInMemory )
		{
			pInMemory->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
			bool bSucceeded = pStyleFile->BReloadLoadFromBuffer( *pInMemory, vecPrevStyleFiles, unStartingFileOrder );
			pInMemory->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

			if ( !bSucceeded )
				return;
		}
		else
		{
			ELoadLayoutFileResult eResult = pStyleFile->Reload( vecPrevStyleFiles, unStartingFileOrder );
			if ( eResult == k_ELoadLayoutFileReadFailed )
				m_vecFilesFailedRead.AddToTail( symPath );

			if ( eResult != k_ELoadLayoutFileOK )
				return;
		}
	}

	// fire reload event which all panels should be listening for
	DispatchEvent( ::ReloadStyleFile(), (IUIPanel*)NULL, symPath );
}


//-----------------------------------------------------------------------------
// Purpose: triggers a reload of a script file
//-----------------------------------------------------------------------------
void CLayoutManager::ReloadScriptFile( CPanoramaSymbol symPath )
{
	CUtlString comparePath = symPath.String();

	// clear out the cached copy of the script
	auto iMap = m_mapJSFiles.Find( symPath );
	Assert( iMap != m_mapJSFiles.InvalidIndex() );
	JSFilePtr_t pPrevJSFile = NULL;
	if ( iMap != m_mapJSFiles.InvalidIndex() )
	{
		pPrevJSFile = m_mapJSFiles[ iMap ];
		m_mapJSFiles.RemoveAt( iMap );
	}

	// figure out all layout files that refer to the script
	CUtlVector< CPanoramaSymbol > layoutFilesToReload;

	FOR_EACH_MAP( m_mapLayoutFiles, iLayout )
	{
		LayoutFilePtr_t pLayoutFile = m_mapLayoutFiles[ iLayout ];
		
		bool bIncludesScript = false;
		const CUtlVector< JSInclude_t > & scriptIncludes = pLayoutFile->GetLayoutFileScriptIncludes();
		FOR_EACH_VEC( scriptIncludes, iScript )
		{
			const JSInclude_t &jsInclude = scriptIncludes[ iScript ];
			if ( jsInclude.strFilenameResolved == comparePath )
			{
				bIncludesScript = true;
				break;
			}
		}

		if ( !bIncludesScript )
			continue;

		CPanoramaSymbol symLayoutFile = pLayoutFile->GetLayoutFileSymbol();
		if ( -1 == layoutFilesToReload.Find( symLayoutFile ) )
		{
			layoutFilesToReload.AddToTail( symLayoutFile );
		}
	}

	// reload the relevant layouts
	FOR_EACH_VEC( layoutFilesToReload, iLayout )
	{
		CPanoramaSymbol symReload = layoutFilesToReload[ iLayout ];
		ReloadLayoutFile( symReload );
	}

	// by the time we've reloaded all the layouts, the old JS shouldn't have any references left
	Assert( pPrevJSFile.Get() && !pPrevJSFile->BMultipleReferences() );
}


//-----------------------------------------------------------------------------
// Purpose: In-memory updates a style at a known location in a file
//-----------------------------------------------------------------------------
bool CLayoutManager::UpdateStyleInMemory( EUpdateStyleType eUpdateType, CPanoramaSymbol symStyleFile, uint unLocation, const char *pchUpdatedStyle )
{
	// load into memory
	CUtlBuffer *pFile = NULL;

	int iRemote = m_mapCachedStyleBuffers.Find( symStyleFile );
	if ( iRemote != m_mapCachedStyleBuffers.InvalidIndex() )
	{
		pFile = m_mapCachedStyleBuffers[iRemote];
	}
	else
	{
		int iMap = m_mapInMemoryFiles.Find( symStyleFile );
		if ( iMap == m_mapInMemoryFiles.InvalidIndex() )
		{
			CUtlBuffer *pBuffer = new CUtlBuffer();
			if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( symStyleFile.String(), *pBuffer, true ) )
			{
				delete pBuffer;
				LogLayoutParsingError( symStyleFile, 0, "Could not load file" );
				return false;
			}

			InMemoryFile_t fileInfo;
			fileInfo.m_pBuffer = pBuffer;

			CSHA1 sha;
			sha.Update( (uint8*)pBuffer->Base(), pBuffer->TellPut() );
			sha.Final();
			sha.GetHash( fileInfo.m_digest );
			
			iMap = m_mapInMemoryFiles.Insert( symStyleFile, fileInfo );
		}
		pFile = m_mapInMemoryFiles.Element( iMap ).m_pBuffer;
	}

	if ( !pFile )
	{
		AssertMsg( pFile, "pFile should always be valid... logic error in styles file caching?" );
		return false;
	}


	// determine the style's current length
	char *pchEnd = NULL;
	if ( eUpdateType == k_EUpdateStyleStyle )
	{
		char *pchCurrent = (char *)pFile->PeekGet( unLocation );
		bool bInQuote = false;
		while ( pchCurrent[0] != '\0' )
		{
			if ( pchCurrent[0] == '"' )
			{
				// if in quote, length must be > 1 so no length check required
				if ( !bInQuote )
					bInQuote = true;
				else if ( bInQuote && *(pchCurrent - 1) != '\\' )
					bInQuote = false;
			}

			if ( !bInQuote && pchCurrent[0] == '}' )
			{
				pchEnd = pchCurrent;
				break;
			}

			pchCurrent++;
		}
	}
	else if ( eUpdateType == k_EUpdateStyleKeyframes )
	{
		// first {
		char *pchStart = (char *)pFile->PeekGet( unLocation );
		char *pchCurrent = V_strstr( pchStart,  "{" );
		if ( !pchCurrent )
			return false;

		// loop looking for closing }
		pchCurrent++;
		bool bInFrame = false;
		while ( pchCurrent[0] != '\0' )
		{
			if ( pchCurrent[0] == '}' )
			{
				if ( !bInFrame )
					break;

				bInFrame = false;
			}

			if ( pchCurrent[0] == '{' )
			{
				// should only have 1 nested block
				if ( bInFrame )
					return false;

				bInFrame = true;
			}

			pchCurrent++;
		}

		if ( pchCurrent[0] != '}' )
			return false;

		pchEnd = pchCurrent;
	}
	else
	{
		AssertMsg( false, "Unknown EUpdateStyleType" );
		return false;
	}
	
	if ( !pchEnd )
	{
		AssertMsg( false, "Couldn't find end of style" );
		return false;
	}

	uint unOldStyleLength = pchEnd + 1 - (char*)pFile->PeekGet( unLocation );		// +1 for skipping }
	uint unNewStyleLength = V_strlen( pchUpdatedStyle );

	// allocate a buffer for the new file
	CUtlBuffer newFile( 0, pFile->TellPut() - unOldStyleLength + unNewStyleLength );
	newFile.SetBufferType( true, true );

	// build new file
	newFile.Put( pFile->Base(), unLocation );
	newFile.Put( pchUpdatedStyle, unNewStyleLength );
	newFile.Put( pFile->PeekGet( unLocation + unOldStyleLength ), pFile->TellPut() - unLocation - unOldStyleLength );

	// swap into place
	pFile->Swap( newFile );

	DispatchEvent( InMemoryFileUpdate(), (IUIPanel*)NULL, symStyleFile, unLocation, unOldStyleLength, unNewStyleLength );
	ReloadStyleFile( symStyleFile );	

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Saves in memory files to disk
//-----------------------------------------------------------------------------
void CLayoutManager::SaveInMemoryFiles()
{
	FOR_EACH_MAP_FAST( m_mapInMemoryFiles, i )
	{
		CPanoramaSymbol symFile = m_mapInMemoryFiles.Key( i );

		// need to p4 edit
		if ( !IsRetail() ) // disabling this feature on SDK user machines - should really go through assetsystem in S2 (correct changelist, etc.)
		{
			uint procID = ThreadShellExecute( "p4.exe", CFmtStr( "edit %s", symFile.String() ), "." );
			int nTries = 100;
			while ( procID && ThreadIsProcessIdActive( procID ) && --nTries > 0 )
				ThreadSleep( 10 );
		}

		// check if file has been modified
		CUtlBuffer bufOnDisk;
		if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( symFile.String(), bufOnDisk, true ) )
		{
			LogLayoutParsingError( symFile, 0, "Could not load file" );
			continue;
		}
		
		CSHA1 sha;
		sha.Update( (uint8*)bufOnDisk.Base(), bufOnDisk.TellPut() );
		sha.Final();

		// if changed since we started editing in memory, do not save
		InMemoryFile_t &inMemory = m_mapInMemoryFiles.Element( i );
		if ( V_memcmp( &inMemory.m_digest, sha.m_digest, k_cubSHA1Hash ) != 0 )
		{
			CFmtStr fmtStr( "Unable to save the following file, because it has changed on disk.\n\nFile:\n%s", symFile.String() );
			UIEngine()->ShowNativeTopMostMessageBox( fmtStr, "Unable to save file", IUIEngine::k_ENativeMessageOk );
			continue;
		}

		if ( !UIEngine()->UIFileSystem()->SaveBufferToFile( *inMemory.m_pBuffer, symFile.String() ) )
		{
			CFmtStr fmtStr( "Failed to save the following file:\n%s", symFile.String() );
			UIEngine()->ShowNativeTopMostMessageBox( fmtStr, "Unable to save file", IUIEngine::k_ENativeMessageOk );			
		}
		else
		{
			delete inMemory.m_pBuffer;
			m_mapInMemoryFiles.RemoveAt( i );
		}
	}

	DispatchEvent( InMemoryFilesSaved(), (IUIPanel*)NULL );
/*#else
	AssertMsg( false, "SaveInMemoryFiles needs to be made to work in source2" );
#endif*/
}


//-----------------------------------------------------------------------------
// Purpose: Throws out all in memory changes
//-----------------------------------------------------------------------------
void CLayoutManager::RevertInMemoryFiles()
{
	FOR_EACH_MAP_FAST( m_mapInMemoryFiles, i )
	{
		CPanoramaSymbol symFile = m_mapInMemoryFiles.Key( i );
		CUtlBuffer *pFile = m_mapInMemoryFiles.Element( i ).m_pBuffer;
		delete pFile;
		m_mapInMemoryFiles.RemoveAt( i );
		ReloadStyleFile( symFile );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: In-memory updates a style at a known location in a file
//-----------------------------------------------------------------------------
bool CLayoutManager::LoadStyleIntoBuffer( CPanoramaSymbol symFile, CUtlBuffer &buffer )
{
	symFile = StripVersionGetParamFromURL( symFile.String() ).String();

	// return in memory version if available
	int iMap = m_mapInMemoryFiles.Find( symFile );
	if ( iMap != m_mapInMemoryFiles.InvalidIndex() )
	{
		buffer.SetBufferType( true, true );
		buffer.CopyBuffer( *(m_mapInMemoryFiles[iMap].m_pBuffer) );
		return true;
	}

	int iHTTPMap = m_mapCachedStyleBuffers.Find( symFile );
	if ( iHTTPMap != m_mapCachedStyleBuffers.InvalidIndex() )
	{
		buffer.SetBufferType( true, true );
		buffer.CopyBuffer( *(m_mapCachedStyleBuffers[iHTTPMap]) );
		return true;
	}

	// not in memory, get from disk
	return UIEngine()->UIFileSystem()->LoadFileIntoBuffer( symFile.String(), buffer, true );
}


//-----------------------------------------------------------------------------
// Purpose: parses xml layout files
//-----------------------------------------------------------------------------
namespace panorama
{
class CLayoutFileXMLParser
{
public:
	CLayoutFileXMLParser( CLayoutFile *pLayoutFile ) : m_pLayoutFile( pLayoutFile ), m_pPanelDescription( NULL ) {}
	~CLayoutFileXMLParser()
	{
		SAFE_DELETE( m_pPanelDescription );

		// We only want to assert on the panel stack being empty if we were
		// successful (errors leave it in a partial state). This may be a
		// (small) memory leak in some scenario, but tracing through the code
		// leads me to believe that a panel description in the stack is
		// likely already parented to one that may not be in the stack.
		// If this was occurring in non-error cases, we'd want to guarantee
		// this, but there are multiple failure points possible.
		if ( m_strAbortError.IsEmpty() )
		{
			Assert( m_vecCurrentPanelStack.Count() == 0 );
		}
	}

	bool BParseXML( CUtlBuffer &buffer, bool bIsPartial )
	{
		if ( !m_pLayoutFile && !bIsPartial )
			return false;

		m_pBuffer = &buffer;
		m_eCurrentElement = k_EInInvalid;
		m_bFoundChildren = false;
		m_bFoundStyles = false;
		m_bFoundScripts = false;
		m_bFoundSnippets = false;
		m_bFoundSnippetRootPanel = false;
		m_bLastIncludeWasStyle = true;
		m_bPartial = bIsPartial;
		m_cubPartialWrapperReturned = 0;
		m_vecCurrentPanelStack.Purge();
		SAFE_DELETE( m_pPanelDescription );
		m_strAbortError.Clear();
		m_strCurrentSnippetName.Clear();

		if( !XMLParser_Create( &m_parser ) )
			return false;

		//set callback functions
		m_parser->errorHandler = StaticErrorHandler;
		m_parser->startElementHandler = StaticStartElement;
		m_parser->endElementHandler = StaticEndElement;
		m_parser->charactersHandler = StaticCharacters;
		m_parser->ignorableWhitespaceHandler = StaticCharacters;
		m_parser->startCDATAHandler = StaticStartCDATA;
		m_parser->UserData = this;

		// parse buffer
		bool bRet = (XMLParser_Parse( m_parser, cstream, this, (XMLCH *)"UTF-8" ) != XML_OK);

		// the layout file will own the allocated memory
		if ( bRet )
		{
			if ( m_pLayoutFile )
				m_pLayoutFile->SetPanelDescription( DetatchPanelDescription() );
			else
				Assert( m_bPartial );
		}

		XMLParser_Free( m_parser );
		return bRet;
	}

	PanelDescription_t *DetatchPanelDescription()
	{
		PanelDescription_t *pRet = m_pPanelDescription;
		m_pPanelDescription = NULL;

		return pRet;
	}

protected:
	enum ELayoutXMLElements
	{
		k_EInInvalid,
		k_EInRoot,
		k_EInStyles,
		k_EInScripts,
		k_EInInclude,
		k_EInPanels,
		k_EInInlineScript,
		k_EInSnippets,
		k_EInSnippet,
		k_EInSnippetPanels,
	};

	static int StaticStartElement( void *pUserData, const XMLCH *uri, const XMLCH *localName, const XMLCH *qName, LPXMLVECTOR atts )
	{
		CLayoutFileXMLParser *pThis = (CLayoutFileXMLParser*)pUserData;
		return pThis->StartElement( uri, (const char*)localName, (const char*)qName, atts );
	}

	static int StaticCharacters( void *pUserData, const XMLCH *Chars, int cbChars )
	{
		CLayoutFileXMLParser *pThis = (CLayoutFileXMLParser*)pUserData;
		return pThis->Characters( (const char*)Chars, cbChars );
	}
	
	static int StaticStartCDATA( void *pUserData )
	{
		CLayoutFileXMLParser *pThis = (CLayoutFileXMLParser*)pUserData;
		return pThis->CDATAStart();
	}

	static int StaticEndElement( void *pUserData, const XMLCH *uri, const XMLCH *localName, const XMLCH *qName )
	{
		CLayoutFileXMLParser *pThis = (CLayoutFileXMLParser*)pUserData;
		return pThis->EndElement( uri, (const char*)localName, (const char*)qName );
	}

	static void StaticErrorHandler( LPXMLPARSER parser )
	{
		CLayoutFileXMLParser *pThis = (CLayoutFileXMLParser*)parser->UserData;
		const char *pchError = pThis->m_strAbortError.IsEmpty() ? (const char*)parser->ErrorString : pThis->m_strAbortError.String();

		CPanoramaSymbol symLayout;
		if ( pThis->m_pLayoutFile )
			symLayout = pThis->m_pLayoutFile->GetLayoutFileSymbol();

		LogLayoutParsingError( symLayout, parser->ErrorLine, "XML parsing error: %s", pchError );
	}

	static int cstream( BYTE *buf, int cBytes, int *cBytesActual, void *inputData )
	{
		// when parsing partial layout files, need to wrap in a root and fake panel as root of partial xml could be multiple panels such as:
		// <Label /><Button />
		const char *k_pchPartialOpen = "<root><Panel>";
		const int k_cubPartialOpen = 13;
		const char *k_pchPartialClose = "</Panel></root>";
		const int k_cubPartialClose = 15;

		CLayoutFileXMLParser *pThis = (CLayoutFileXMLParser*)inputData;

		if ( pThis->m_bPartial && pThis->m_cubPartialWrapperReturned < k_cubPartialOpen )
		{
			int cubWrite = Min( cBytes, k_cubPartialOpen - pThis->m_cubPartialWrapperReturned );
			V_memcpy( buf, &k_pchPartialOpen[ pThis->m_cubPartialWrapperReturned ], cubWrite );
			*cBytesActual = cubWrite;
			pThis->m_cubPartialWrapperReturned += cubWrite;
			return false;
		}


		CUtlBuffer *pBuf = pThis->m_pBuffer;
		int nMaxBytes = pBuf->GetBytesRemaining();
		*cBytesActual = MIN( cBytes, nMaxBytes );

		// Streaming can cause BytesRemaining to be a lie
		*cBytesActual = pBuf->GetUpTo( buf, *cBytesActual );

		bool bEOF = (*cBytesActual < cBytes);
		if ( bEOF && pThis->m_bPartial )
		{
			int iPartialClose = pThis->m_cubPartialWrapperReturned - k_cubPartialOpen;
			int cubWrite = Min( cBytes - *cBytesActual, k_cubPartialClose - iPartialClose );
			V_memcpy( buf + *cBytesActual, &k_pchPartialClose[ iPartialClose ], cubWrite );
			*cBytesActual = *cBytesActual + cubWrite;
			pThis->m_cubPartialWrapperReturned += cubWrite;
		}
		
		return (*cBytesActual < cBytes);
	}

	int StartElement( const XMLCH *uri, const char *localName, const char *pchName, LPXMLVECTOR atts )
	{
		// don't care what the root element name is
		if ( m_eCurrentElement == k_EInInvalid )
		{
			m_eCurrentElement = k_EInRoot;
		}
		else if ( V_strcmp( pchName, "styles" ) == 0 )
		{
			if ( m_bPartial )
				return ParseError( "Styles can't be included in partial layout files" );

			if ( m_bFoundStyles )
			{
				AssertMsg( !m_bFoundStyles, "There should be only a single styles section in the XML, this section is duplicate" );
				return ParseError( "There cannot be more than one styles section in the xml, place all styles includes together in one" );
			}

			if( m_bFoundChildren )
			{
				AssertMsg( !m_bFoundChildren, "Styles section (if present) must be before any child panels at the top of the file" );
				return ParseError( "Styles must be declared first before any child panels" );
			}

			if ( m_eCurrentElement != k_EInRoot )
				return ParseError( "Styles must be declared in root" );

			m_bFoundStyles = true;
			m_bLastIncludeWasStyle = true;
			m_eCurrentElement = k_EInStyles;
		}
		else if( V_strcmp( pchName, "scripts" ) == 0 )
		{
			if ( m_bPartial )
				return ParseError( "Scripts can't be included in partial layout files" );

			if( m_bFoundScripts )
			{
				AssertMsg( !m_bFoundScripts, "There should be only a single scripts section in the XML, this section is duplicate" );
				return ParseError( "There cannot be more than one scripts section in the xml, place all script includes together in one" );
			}

			if( m_bFoundChildren )
			{
				AssertMsg( !m_bFoundChildren, "Scripts section (if present) must be before any child panels at the top of the file" );
				return ParseError( "Scripts must be declared first before any child panels" );
			}

			if( m_eCurrentElement != k_EInRoot )
				return ParseError( "Scripts must be declared in root" );

			m_bFoundScripts = true;
			m_bLastIncludeWasStyle = false;
			m_eCurrentElement = k_EInScripts;
		}
		else if ( V_strcmp( pchName, "include" ) == 0 )
		{
			if( m_eCurrentElement != k_EInStyles && m_eCurrentElement != k_EInScripts )
				return ParseError( "Found include outside of a styles/scripts element" );

			// find src
			for ( int i = 0; i < atts->length; i++ )
			{
				LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
				if ( V_strcmp( (const char*)att->qname, "src" ) == 0 )
				{
					if( m_eCurrentElement == k_EInStyles && !m_pLayoutFile->BAddStyle( (const char*)att->value ) )
						return ParseError( "Unable to load css file: %s", (const char*)att->value );

					if( m_eCurrentElement == k_EInScripts && !m_pLayoutFile->BAddJavaScript( (const char*)att->value ) )
						return ParseError( "Unable to load js file: %s", (const char*)att->value );

					break;
				}
			}

			m_eCurrentElement = k_EInInclude;
		}
		else if ( V_strcmp( pchName, "snippets" ) == 0 )
		{
			if ( m_bPartial )
				return ParseError( "Snippets can't be included in partial layout files" );

			if ( m_bFoundSnippets )
			{
				AssertMsg( !m_bFoundSnippets, "There should be only a single snippets section in the XML, this section is duplicate" );
				return ParseError( "There cannot be more than one snippets section in the xml, place all snippets together in one" );
			}

			if ( m_bFoundChildren )
			{
				AssertMsg( !m_bFoundChildren, "Snippets section (if present) must be before any child panels at the top of the file" );
				return ParseError( "Snippets must be declared first before any child panels" );
			}

			if ( m_eCurrentElement != k_EInRoot )
				return ParseError( "Snippets must be declared in root" );

			m_bFoundSnippets = true;
			m_eCurrentElement = k_EInSnippets;
		}
		else if ( V_strcmp( pchName, "snippet" ) == 0 )
		{
			if ( m_eCurrentElement != k_EInSnippets )
				return ParseError( "Found snippet outside of a snippets element" );

			// find name
			const char *pchSnippetName = nullptr;
			for ( int i = 0; i < atts->length; i++ )
			{
				LPXMLRUNTIMEATT att = ( LPXMLRUNTIMEATT )XMLVector_Get( atts, i );
				if ( V_strcmp( ( const char* )att->qname, "name" ) == 0 )
				{
					pchSnippetName = ( const char * )att->value;
					break;
				}
			}

			if ( V_isempty( pchSnippetName ) )
				return ParseError( "Snippet is missing the name attribute" );

			m_strCurrentSnippetName = pchSnippetName;
			m_bFoundSnippetRootPanel = false;
			m_eCurrentElement = k_EInSnippet;
		}
		else if ( m_eCurrentElement == k_EInRoot )
		{
			// we are at the root, and this isn't a styles block. Assume panel hierarchy. Should only find one instance.
			if ( m_bFoundChildren )
				return ParseError( "Found duplicate panel description" );

			if( V_stricmp( pchName, "script" ) == 0 )
			{
				if ( m_bPartial )
					return ParseError( "Inline scripts can't be included in partial layout files" );

				GetCurrentParsePosition( &m_nScriptStartLine, &m_nScriptStartCol );
				if( m_nScriptStartLine > 0 )
					--m_nScriptStartLine;
				m_eCurrentElement = k_EInInlineScript;
			}
			else
			{
				m_bFoundChildren = true;
				m_eCurrentElement = k_EInPanels;

				if ( !BAddPanel( pchName, atts ) )
					return XML_ABORT;
			}
		}
		else if ( m_eCurrentElement == k_EInPanels )
		{
			if( V_stricmp( pchName, "script" ) == 0 )
			{
				if ( m_bPartial )
					return ParseError( "Inline scripts can't be included in partial layout files" );

				GetCurrentParsePosition( &m_nScriptStartLine, &m_nScriptStartCol );
				if( m_nScriptStartLine > 0 )
					--m_nScriptStartLine;

				m_eCurrentElement = k_EInInlineScript;
			}
			else
			{
				if( !BAddPanel( pchName, atts ) )
					return XML_ABORT;
			}
		}
		else if ( m_eCurrentElement == k_EInSnippet )
		{
			if ( m_bFoundSnippetRootPanel )
				return ParseError( "Found multiple root panels under snippet '%s'", m_strCurrentSnippetName.Get() );

			m_bFoundSnippetRootPanel = true;
			m_eCurrentElement = k_EInSnippetPanels;

			if ( V_stricmp( pchName, "script" ) == 0 )
			{
				return ParseError( "Inline scripts can't be included in snippets" );
			}
			else
			{
				if ( !BAddPanel( pchName, atts ) )
					return XML_ABORT;
			}
		}
		else if ( m_eCurrentElement == k_EInSnippetPanels )
		{
			if ( V_stricmp( pchName, "script" ) == 0 )
			{
				return ParseError( "Inline scripts can't be included in snippets" );
			}
			else
			{
				if ( !BAddPanel( pchName, atts ) )
					return XML_ABORT;
			}
		}

		return XML_OK;
	}	

	int CDATAStart()
	{
		return XML_OK;
	}

	int Characters( const char *Chars, int cbChars )
	{		
		if( m_eCurrentElement == k_EInInlineScript )
		{
			m_strInlineScript.Append( Chars, cbChars );
		}
		return XML_OK;
	}

	int EndElement( const XMLCH *uri, const char *localName, const char *pchName )
	{
		if ( m_eCurrentElement == k_EInRoot )
		{
			m_eCurrentElement = k_EInInvalid;
		}
		else if( m_eCurrentElement == k_EInInlineScript )
		{
			if( m_strInlineScript.Length() )
			{
				// Store off script to parse later
				m_pLayoutFile->AddInlineJavascript( m_strInlineScript.String(), m_nScriptStartLine, m_nScriptStartCol );
			}
			m_strInlineScript.Clear();
			if( !m_bFoundChildren )
				m_eCurrentElement = k_EInRoot;
			else
				m_eCurrentElement = k_EInPanels;
		}
		else if ( m_eCurrentElement == k_EInPanels || m_eCurrentElement == k_EInSnippetPanels )
		{
			if( m_vecCurrentPanelStack.Count() == 0 )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Unexpected parsing error - popping panel off empty stack: line %d, col %d", nLine, nCol );
			}

			// pop panel from stack
			m_vecCurrentPanelStack.Remove( m_vecCurrentPanelStack.Count() - 1 );

			// out of panels?
			if ( m_vecCurrentPanelStack.Count() == 0 )
			{
				m_eCurrentElement = m_eCurrentElement == k_EInPanels ? k_EInRoot : k_EInSnippet;
			}
		}
		else if ( V_strcmp( pchName, "styles" ) == 0 )
		{
			if( m_eCurrentElement != k_EInStyles )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );

				return ParseError( "Unexpected parsing error: line %d, col %d", nLine, nCol );
			}

			m_eCurrentElement = k_EInRoot;
		}
		else if( V_strcmp( pchName, "scripts" ) == 0 )
		{
			if( m_eCurrentElement != k_EInScripts )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Unexpected parsing error: line %d, col %d", nLine, nCol );
			}

			m_eCurrentElement = k_EInRoot;
		}
		else if ( V_strcmp( pchName, "include" ) == 0 )
		{
			if( m_eCurrentElement != k_EInInclude )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Unexpected parsing error: line %d, col %d", nLine, nCol );
			}

			if( m_bLastIncludeWasStyle )
				m_eCurrentElement = k_EInStyles;
			else
				m_eCurrentElement = k_EInScripts;
		}
		else if ( V_strcmp( pchName, "snippets" ) == 0 )
		{
			if ( m_eCurrentElement != k_EInSnippets )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Unexpected parsing error: line %d, col %d", nLine, nCol );
			}

			m_eCurrentElement = k_EInRoot;
		}
		else if ( V_strcmp( pchName, "snippet" ) == 0 )
		{
			if ( m_strCurrentSnippetName.IsEmpty() || m_eCurrentElement != k_EInSnippet )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Unexpected parsing error: line %d, col %d", nLine, nCol );
			}

			if ( m_pLayoutFile->GetSnippet( m_strCurrentSnippetName.Get() ) != nullptr )
			{
				return ParseError( "Snippet '%s' is defined more than once.", m_strCurrentSnippetName.Get() );
			}

			PanelDescription_t *pPanelDescription = DetatchPanelDescription();
			if ( !pPanelDescription )
			{
				int nLine, nCol;
				GetCurrentParsePosition( &nLine, &nCol );
				return ParseError( "Snippet '%s' is missing its contents. line %d, col %d", m_strCurrentSnippetName.Get(), nLine, nCol );
			}

			m_pLayoutFile->AddSnippet( m_strCurrentSnippetName.Get(), pPanelDescription );
			m_strCurrentSnippetName.Clear();
			m_vecCurrentPanelStack.RemoveAll();
			m_eCurrentElement = k_EInSnippets;
		}

		return XML_OK;
	}

	void GetCurrentParsePosition( int *pLineOut, int *pColOut )
	{
		*pLineOut = XMLParser_GetCurrentLine( m_parser );
		*pColOut = XMLParser_GetCurrentColumn( m_parser );
	}

	bool BAddPanel( const char *pchName, LPXMLVECTOR atts )
	{
		VPROF_BUDGET( "CLayoutFileXMLParser::BAddPanel", VPROF_BUDGETGROUP_STEAMUI );
		CPanoramaSymbol symType( pchName );

		// make sure this is a valid panel type
		if ( !UIEngine()->BRegisteredPanelType( symType ) )
		{
			Assert( symType.IsValid() );
			if ( symType.IsValid() )
				Assert( V_strcmp( pchName, symType.String() ) == 0 );
			
			int nLine, nCol;
			GetCurrentParsePosition( &nLine, &nCol );
			ParseError( "Found unknown panel type: (name=%s) (type=%s) on line: %d, col: %d", pchName, symType.String(), nLine, nCol );
			return false;
		}

		// create entry for new panel
		PanelDescription_t *pCurrentPanel = new PanelDescription_t;
		pCurrentPanel->m_symType = symType;
		for ( int i = 0; i < atts->length; i++ )
		{
			LPXMLRUNTIMEATT att = (LPXMLRUNTIMEATT)XMLVector_Get( atts, i );
			const char *pchAttrName = (const char *)att->qname;
			const char *pchAttrValue = (const char *)att->value;

			// special case ID
			if ( V_strcmp( "id", pchAttrName ) == 0 )
			{
				pCurrentPanel->m_strID = pchAttrValue;
				continue;
			}
	
			// XML parser should already verify that the attribute is unique
			CPanoramaSymbol symPropertyName( pchAttrName );
			pCurrentPanel->m_mapProperties.Insert( symPropertyName, pchAttrValue );
		}

		// top panel shouldn't have an id (declared in code)
		if ( m_vecCurrentPanelStack.Count() == 0 && !pCurrentPanel->m_strID.IsEmpty() )
		{
			delete pCurrentPanel;
			ParseError( "Top most panel should not have an ID. This ID is set in code" );
			return false;
		}

		// bugbug cboyd - make sure ID is unique?		

		// if root, set as first panel description, else add to parent
		if ( m_vecCurrentPanelStack.Count() == 0 )
		{
			Assert( !m_pPanelDescription );
			m_pPanelDescription = pCurrentPanel;
		}
		else
		{
			PanelDescription_t *pParent = m_vecCurrentPanelStack[ m_vecCurrentPanelStack.Count() - 1 ];
			pParent->m_vecChildren.AddToTail( pCurrentPanel );
		}

		// this panel is now top of the stack
		m_vecCurrentPanelStack.AddToTail( pCurrentPanel );

		return true;
	}

	int ParseError( const char *pchMsg, ... )
	{
		va_list args;
		va_start( args, pchMsg );

		m_strAbortError.FormatV( pchMsg, args );

		return XML_ABORT;
	}

private:
	LPXMLPARSER m_parser;
	CUtlBuffer *m_pBuffer;

	CUtlStringBuilder m_strInlineScript;
	int m_nScriptStartLine;
	int m_nScriptStartCol;
	CUtlString m_strAbortError;
	CLayoutFile *m_pLayoutFile;
	ELayoutXMLElements m_eCurrentElement;
	bool m_bFoundChildren;
	bool m_bFoundStyles;
	bool m_bFoundScripts;
	bool m_bFoundSnippets;
	bool m_bFoundSnippetRootPanel;
	bool m_bLastIncludeWasStyle;
	bool m_bPartial;
	int m_cubPartialWrapperReturned;
	CUtlString m_strCurrentSnippetName;
	
	PanelDescription_t *m_pPanelDescription;
	CUtlVector< PanelDescription_t * > m_vecCurrentPanelStack;
};
}

#ifdef PANORAMA_USE_S1WRAPPER
static bool UtlStringLessFunc( const CUtlString &lhs, const CUtlString &rhs )
{
	return V_strcmp( lhs.Get(), rhs.Get() ) < 0; 
}
#endif

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CLayoutFile::CLayoutFile() : m_pPanelDescription( NULL ) , m_nReloadCount( -1 ) // first load -> 0 reloads
{
	m_bIsPartial = false;
	m_mapSnippets.SetLessFunc( UtlStringLessFunc );
}


//-----------------------------------------------------------------------------
// Purpose: deconstructor
//-----------------------------------------------------------------------------
CLayoutFile::~CLayoutFile()
{
	ClearPanelDescription();
	ClearSnippets();
}


//-----------------------------------------------------------------------------
// Purpose: adds a style file
//-----------------------------------------------------------------------------
bool CLayoutFile::BAddStyle( const char *pchPath )
{
	const CUtlVector< StyleFilePtr_t > &vecStyleFiles = m_styleFileSet.GetStyleFiles();
	uint unFileOrder = vecStyleFiles.IsEmpty() ? 0 : vecStyleFiles.Tail()->GetMaxFileOrder();

	StyleFilePtr_t pStyleFile = UIEngineInternal()->UILayoutManagerInternal()->GetStyleFile( pchPath, m_styleFileSet, unFileOrder, true );
	if ( !pStyleFile )
		return false;

	m_styleFileSet.AddStyleFile( pStyleFile );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Given a file index, get the full path of style file symbols
// leading to that file. 
//-----------------------------------------------------------------------------
void CLayoutFile::GetStyleFileSymbols( StyleFileIndex_t iFileIndex, CUtlVector< CPanoramaSymbol > &vecStyleFileSymbols ) const
{
	const CUtlVector< StyleFilePtr_t > &vecStyleFiles = m_styleFileSet.GetStyleFiles();

	// See ConstructStyleFileIndex for details about the format of the index
	int iLayoutStyleFileIndex = iFileIndex & 0xF;
	if ( iLayoutStyleFileIndex >= vecStyleFiles.Count() )
	{
		Assert( false );
		return;
	}

	StyleFilePtr_t pStyleFile = vecStyleFiles[ iLayoutStyleFileIndex ];
	vecStyleFileSymbols.AddToTail( pStyleFile->GetPathSymbol() );

	StyleFileIndex_t iStyleFileIndex = iFileIndex >> 4;
	while ( iStyleFileIndex != 0 )
	{
		int iStyleFileImportIndex = ( iStyleFileIndex & 0xF ) - 1;
		if ( iStyleFileImportIndex < 0 || iStyleFileImportIndex >= pStyleFile->GetImportedStyleFileCount() )
		{
			Assert( false );
			return;
		}

		pStyleFile = pStyleFile->GetImportedStyleFile( iStyleFileImportIndex );
		vecStyleFileSymbols.AddToTail( pStyleFile->GetPathSymbol() );

		iStyleFileIndex = iStyleFileIndex >> 4;
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a script file
//-----------------------------------------------------------------------------
bool CLayoutFile::BAddJavaScript( const char *pchPath )
{
	CUtlString resolvedPath;
	JSFilePtr_t pJSFile = UIEngineInternal()->UILayoutManagerInternal()->GetJavaScriptFile( pchPath, &resolvedPath );
	if( !pJSFile )
		return false;

	int iVec = m_vecJavscriptIncludes.AddToTail();
	JSInclude_t &inc = m_vecJavscriptIncludes[iVec];
	inc.strFilename = pchPath;
	inc.strFilenameResolved = resolvedPath;
	inc.m_pJSFile = pJSFile;
	inc.nLine = 0;
	inc.nCol = 0;


	return true;
}


//-----------------------------------------------------------------------------
// Purpose: adds inline script from layout file directly
//-----------------------------------------------------------------------------
void CLayoutFile::AddInlineJavascript( const char *pchScript, int nLine, int nCol )
{
	int iVec = m_vecJavscriptIncludes.AddToTail();
	JSInclude_t &inc = m_vecJavscriptIncludes[iVec];
	inc.strFilename = m_symLayoutFile.String();
	inc.strFilenameResolved = "";
	inc.m_strInlineContents = pchScript;
	inc.nLine = nLine;
	inc.nCol = nCol;
}


//-----------------------------------------------------------------------------
// Purpose: adds a snippet to this layout file
//-----------------------------------------------------------------------------
void CLayoutFile::AddSnippet( const char *pszSnippetName, PanelDescription_t *pPanelDescription )
{
	Assert( !V_isempty( pszSnippetName ) );
	Assert( GetSnippet( pszSnippetName ) == nullptr );
	m_mapSnippets.Insert( pszSnippetName, pPanelDescription );
}


//-----------------------------------------------------------------------------
// Purpose: Removes all snippets from the layout file
//-----------------------------------------------------------------------------
void CLayoutFile::ClearSnippets()
{
	m_mapSnippets.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: adds a style file
//-----------------------------------------------------------------------------
void CLayoutFile::SetPanelDescription( PanelDescription_t *pPanelDescription )
{
	ClearPanelDescription();

	m_pPanelDescription = pPanelDescription;
}


//-----------------------------------------------------------------------------
// Purpose: frees all panel description data
//-----------------------------------------------------------------------------
void CLayoutFile::ClearPanelDescription()
{
	SAFE_DELETE( m_pPanelDescription );
}


//-----------------------------------------------------------------------------
// Purpose: loads a layout file from a buffer
//-----------------------------------------------------------------------------
ELoadLayoutFileResult CLayoutFile::LoadFromFile( const char *pchFile, bool bIsPartial )
{
	// load into buffer
	CUtlBuffer buffer;
	if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( pchFile, buffer, true ) )
		return k_ELoadLayoutFileReadFailed;

	// parse
	CPanoramaSymbol symPath( pchFile );
	return BLoadFromBuffer( symPath, buffer, bIsPartial ) ? k_ELoadLayoutFileOK : k_ELoadLayoutFileFailed;
}


//-----------------------------------------------------------------------------
// Purpose: Wraps script section of layout file with CDATA. If nothing to wrap, doesn't set pstrOut and returns false
//-----------------------------------------------------------------------------
bool BWrapScriptInCDATA( CUtlString *pstrOut, const char *pchLayout, int cchLayout )
{
	pstrOut->Clear();
	if ( !pchLayout || cchLayout <= 0 )
		return false;

	const char *pchStart = NULL;
	const char *pchEnd = NULL;

	// find start of script block. Need to add some quick XML parsing up front to make sure we only find a starting script block
	const char *pchCurrent = V_strstr( pchLayout, "<script>" );
	if ( pchCurrent )
	{
		// 8 = length of "<script>"
		pchStart = pchCurrent + 8;
		pchCurrent = pchStart;
		bool bInSingleLineComment = false;
		bool bInMultiLineComment = false;
		char chInQuote = 0;
		for ( pchCurrent = pchStart; (pchCurrent - pchLayout) < cchLayout; pchCurrent++ )
		{
			if ( chInQuote != 0 && pchCurrent[0] == '\\' )
			{
				// in quote, skip escape unless at end of string
				if ( (pchCurrent - pchLayout) <= cchLayout )
					pchCurrent++;

				continue;
			}

			if ( bInSingleLineComment || bInMultiLineComment )
			{
				// In a comment, basically ignore everything until the comment ends
				if ( bInSingleLineComment && pchCurrent[0] == '\n' )
				{
					bInSingleLineComment = false;
				}
				else if ( bInMultiLineComment && pchCurrent[0] == '*' && ( pchCurrent - pchLayout < cchLayout - 1 ) && pchCurrent[1] == '/' )
				{
					bInMultiLineComment = false;
					pchCurrent++;
				}
				continue;
			}

			if ( chInQuote == 0 && ( pchCurrent - pchLayout < cchLayout - 1 ) )
			{
				if ( pchCurrent[0] == '/' && pchCurrent[1] == '/' )
				{
					bInSingleLineComment = true;
					pchCurrent++;
					continue;
				}
				else if ( pchCurrent[0] == '/' && pchCurrent[1] == '*' )
				{
					bInMultiLineComment = true;
					pchCurrent++;
					continue;
				}
			}

			if ( pchCurrent[0] == '"' )
			{
				if ( chInQuote == '\'' )
					continue;

				if ( chInQuote == '"' )
					chInQuote = 0;
				else
					chInQuote = '"';

				continue;
			}

			if ( pchCurrent[0] == '\'' )
			{
				if ( chInQuote == '"' )
					continue;

				if ( chInQuote == '\'' )
					chInQuote = 0;
				else
					chInQuote = '\'';

				continue;
			}

			if ( chInQuote == 0 && V_strncmp( pchCurrent, "</script>", 9 ) == 0 )
			{
				pchEnd = pchCurrent;
				break;
			}
		}
	}

	// build final string
	if ( pchStart && pchEnd )
	{
		// build pieces (extra size is length of "<![CDATA[" and "]]>"
		int cubFinal = cchLayout + 12;
		CUtlStringBuilder strBuilder( cubFinal );
		strBuilder.Append( pchLayout, pchStart - pchLayout );
		strBuilder.Append( "<![CDATA[" );
		strBuilder.Append( pchStart, pchEnd - pchStart );
		strBuilder.Append( "]]>" );
		strBuilder.Append( pchEnd, cchLayout - (pchEnd - pchLayout) );

#if defined( SOURCE2_PANORAMA )
		// FIXME - source2 doesn't have this kind of Swap.
		pstrOut->Set( strBuilder.String() );
#else
		pstrOut->Swap( strBuilder );
#endif
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: loads a layout file from a buffer
//-----------------------------------------------------------------------------
bool CLayoutFile::BLoadFromBuffer( CPanoramaSymbol symPath, CUtlBuffer &buffer, bool bIsPartial )
{
	VPROF_BUDGET( "CLayoutFile::BLoadFromBuffer", VPROF_BUDGETGROUP_TENFOOT );

	m_nReloadCount++;
	m_symLayoutFile = symPath;
	m_bIsPartial = bIsPartial;

	m_vecJavscriptIncludes.RemoveAll();

	if ( !bIsPartial )
	{
		m_styleFileSet.Clear();
	}

	CUtlBuffer *pbuf = &buffer;
	CUtlBuffer bufReplace;
	CUtlString strLayout;
	if ( BWrapScriptInCDATA( &strLayout, (const char *)buffer.Base(), buffer.TellPut() ) )
	{
		//
		// If there are changes needed, do them in a new buffer instead of
		// modifying the one passed in, which may be running on read-only
		// memory.
		//
#if defined( SOURCE2_PANORAMA )
		bufReplace.SetExternalBuffer( (void*)strLayout.String(), strLayout.Length(), strLayout.Length(), CUtlBuffer::READ_ONLY );
#else
		bufReplace.SetReadOnlyBuffer( (void*)strLayout.String(), strLayout.Length() );
#endif
		pbuf = &bufReplace;
	}

	CLayoutFileXMLParser parser( this );
	return parser.BParseXML( *pbuf, false );		// different meaning of partial; want to delete partial through this path
}


//-----------------------------------------------------------------------------
// Purpose: Parses style body
//-----------------------------------------------------------------------------
bool CLayoutFile::BParseStyleTag( CUtlBuffer &buf, StylePropertyHash_t *pstyleProperties )
{
	return BParseStyleBody( buf, pstyleProperties, m_symLayoutFile, NULL, m_styleFileSet.GetStyleFiles(), true );
}


//-----------------------------------------------------------------------------
// Purpose: Parses a partial layout xml string. String XML should only include XML of children, not this panel as a wrapper.
//
// Returns: Caller must delete returned PanelDescription_t
//-----------------------------------------------------------------------------
bool CLayoutManager::BParsePartialLayout( PanelDescription_t **ppPanelDescription, const char *pchXML )
{
	if ( !ppPanelDescription )
		return false;

	CUtlBuffer buf( pchXML, V_strlen( pchXML ), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
	CLayoutFileXMLParser parser( NULL );
	if ( !parser.BParseXML( buf, true ) )
		return false;

	*ppPanelDescription = parser.DetatchPanelDescription();
	return (ppPanelDescription != NULL);
}


//-----------------------------------------------------------------------------
// Purpose: loads a layout file from a buffer
//-----------------------------------------------------------------------------
bool CLayoutFile::BReloadFromBuffer( CUtlBuffer &buffer )
{
	VPROF_BUDGET( "CLayoutFile::BLoadFromBuffer", VPROF_BUDGETGROUP_TENFOOT );

	m_styleFileSet.Clear();
	ClearPanelDescription();
	ClearSnippets();

	return BLoadFromBuffer( m_symLayoutFile, buffer, m_bIsPartial );
}


//-----------------------------------------------------------------------------
// Purpose: reloads our layout file
//-----------------------------------------------------------------------------
ELoadLayoutFileResult CLayoutFile::Reload()
{
	m_styleFileSet.Clear();
	ClearPanelDescription();
	ClearSnippets();

	return LoadFromFile( m_symLayoutFile.String(), m_bIsPartial );
}


//-----------------------------------------------------------------------------
// Purpose: Recursive helper function to build a list of all style files
//-----------------------------------------------------------------------------
void BuildStyleFileList( StyleFilePtr_t pStyleFile, CUtlVector< StyleFilePtr_t > &vecStyleFiles )
{
	if ( !pStyleFile )
		return;

	vecStyleFiles.AddToTail( pStyleFile );

	for ( int i = 0; i < pStyleFile->GetImportedStyleFileCount(); ++i )
	{
		BuildStyleFileList( pStyleFile->GetImportedStyleFile( i ), vecStyleFiles );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Replaces defines in provided string
//-----------------------------------------------------------------------------
bool CLayoutFile::BReplaceDefines( char *rgchBuffer, uint cubBuffer, uint unFileOrder )
{
	// Create the full list of style files. Note that since this is recursive, it is a bit expensive.
	// However, it's only used for the debugger to change styles in-memory. So it's not that performance sensitive
	CUtlVector< StyleFilePtr_t > vecStyleFiles;
	for ( StyleFilePtr_t pStyleFile : m_styleFileSet.GetStyleFiles() )
	{
		BuildStyleFileList( pStyleFile, vecStyleFiles );
	}

	return panorama::BReplaceDefines( rgchBuffer, cubBuffer, nullptr, vecStyleFiles, unFileOrder );
}


//-----------------------------------------------------------------------------
// Purpose: Tracking of async load instances
//-----------------------------------------------------------------------------
void CLayoutManager::TrackAsyncLoad( CLoadLayoutFileAsync *pLoader )
{
	m_treeInFlightAsyncLoads.Insert( pLoader );
}


//-----------------------------------------------------------------------------
// Purpose: Tracking of async load instances
//-----------------------------------------------------------------------------
void CLayoutManager::ClearAsyncLoad( CLoadLayoutFileAsync *pLoader )
{
	int iTree = m_treeInFlightAsyncLoads.Find( pLoader );
	if ( iTree != m_treeInFlightAsyncLoads.InvalidIndex() )
	{
		m_treeInFlightAsyncLoads.RemoveAt( iTree );
	}
	else
	{
		AssertMsg( false, "m_treeInFlightAsyncLoads missing load instance" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tracking of async load instances
//-----------------------------------------------------------------------------
void CLayoutManager::TrackAsyncReload( CReloadStyleFile *pLoader )
{
	m_treeInFlightAsyncReloads.Insert( pLoader );
}


//-----------------------------------------------------------------------------
// Purpose: Tracking of async load instances
//-----------------------------------------------------------------------------
void CLayoutManager::ClearAsyncReload( CReloadStyleFile *pLoader )
{
	int iTree = m_treeInFlightAsyncReloads.Find( pLoader );
	if ( iTree != m_treeInFlightAsyncReloads.InvalidIndex() )
	{
		m_treeInFlightAsyncReloads.RemoveAt( iTree );
	}
	else
	{
		AssertMsg( false, "m_treeInFlightAsyncReloads missing load instance" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Clear all cached copies of any layout/style/script files
//-----------------------------------------------------------------------------
void CLayoutManager::ClearFileCache()
{
	// Purge Layouts (need to come first, because this can remove refs to styles and scripts)
	FOR_EACH_MAP_FAST( m_mapLayoutFiles, i )
	{
		LayoutFilePtr_t &pLayout = m_mapLayoutFiles[i];
		if ( !pLayout->BMultipleReferences() )
		{
			UIEngineInternal()->OnFileCacheRemoved( pLayout->GetLayoutFileSymbol() );
			m_mapLayoutFiles.RemoveAt( i );
		}
	}

	// Purge Styles
	FOR_EACH_MAP_FAST( m_mapStyleFiles, i )
	{
		StyleFilePtr_t &pStyle = m_mapStyleFiles[i];
		if ( !pStyle->BMultipleReferences() )
		{
			CPanoramaSymbol stylePath = pStyle->GetPathSymbol();

			// remove the style from m_mapCachedIncludeBuffers
			int iCachedStyle = m_mapCachedStyleBuffers.Find( stylePath );
			if ( iCachedStyle != m_mapCachedStyleBuffers.InvalidIndex() )
			{
				delete m_mapCachedStyleBuffers[ iCachedStyle ];
				m_mapCachedStyleBuffers.RemoveAt( iCachedStyle );
			}

			// remove the style from m_treeKnownStyleFiles
			int iKnownStyle = m_treeKnownStyleFiles.Find( stylePath );
			if ( iKnownStyle != m_treeKnownStyleFiles.InvalidIndex() )
			{
				m_treeKnownStyleFiles.RemoveAt( iKnownStyle );
			}

			UIEngineInternal()->OnFileCacheRemoved( stylePath );
			m_mapStyleFiles.RemoveAt( i );
		}
	}

	// Purge Scripts
	FOR_EACH_MAP_FAST( m_mapJSFiles, i )
	{
		JSFilePtr_t &pScript = m_mapJSFiles[i];
		if ( !pScript->BMultipleReferences() )
		{
			UIEngineInternal()->OnFileCacheRemoved( pScript->GetPathSymbol() );
			m_mapJSFiles.RemoveAt( i );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Print current refcounts for all of the layout/style/scripts
//-----------------------------------------------------------------------------
void CLayoutManager::PrintCacheStatus()
{
	Msg( "Panorama Cache Summary:\n" );

	Msg( "%d Layout(s)\n", m_mapLayoutFiles.Count() );
	FOR_EACH_MAP_FAST( m_mapLayoutFiles, i )
	{
		LayoutFilePtr_t &pFile = m_mapLayoutFiles[i];
		Msg( "- %s (%d refs)\n", pFile->GetLayoutFileSymbol().String(), pFile->GetRefCount() );
	}

	Msg( "%d Style(s)\n", m_mapStyleFiles.Count() );
	FOR_EACH_MAP_FAST( m_mapStyleFiles, i )
	{
		StyleFilePtr_t &pFile = m_mapStyleFiles[i];
		Msg( "- %s (%d refs)\n", pFile->GetPathSymbol().String(), pFile->GetRefCount() );
	}

	Msg( "%d JS File(s)\n", m_mapJSFiles.Count() );
	FOR_EACH_MAP_FAST( m_mapJSFiles, i )
	{
		JSFilePtr_t &pFile = m_mapJSFiles[i];
		Msg( "- %s (%d refs)\n", pFile->GetPathSymbol().String(), pFile->GetRefCount() );
	}
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CLayoutFile::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_styleFileSet );
	ValidatePtr( m_pPanelDescription );
	ValidateObj( m_vecJavscriptIncludes );
	FOR_EACH_VEC( m_vecJavscriptIncludes, i )
	{
		ValidateObj( m_vecJavscriptIncludes[i].m_strInlineContents );
		ValidateObj( m_vecJavscriptIncludes[i].strFilename );
		ValidateObj( m_vecJavscriptIncludes[i].strFilenameResolved );
	}
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CLayoutManager::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_mapLayoutFiles );
	FOR_EACH_MAP_FAST( m_mapLayoutFiles, i )
	{
		ValidatePtr( m_mapLayoutFiles[i].Get() );
	}
	ValidateObj( m_mapCachedStyleBuffers );
	FOR_EACH_MAP_FAST( m_mapCachedStyleBuffers, i )
	{
		ValidatePtr( m_mapCachedStyleBuffers[i] );
	}

	ValidateObj( m_mapLocalP4PathsToHTTPPaths );
	FOR_EACH_MAP_FAST( m_mapLocalP4PathsToHTTPPaths, i )
	{
		ValidateObj( m_mapLocalP4PathsToHTTPPaths.Key( i ) );
	}

	ValidateObj( m_mapStyleFiles );
	FOR_EACH_MAP_FAST( m_mapStyleFiles, i )
	{
		ValidateObj( m_mapStyleFiles.Key( i ) );
		ValidatePtr( m_mapStyleFiles.Key( i ).GetPtr() );
		ValidatePtr( m_mapStyleFiles.Element( i ).Get() );
	}
	ValidateObj( m_treeKnownStyleFiles );
	ValidateObj( m_mapJSFiles );
	FOR_EACH_MAP_FAST( m_mapJSFiles, i )
	{
		ValidatePtr( m_mapJSFiles[i].Get() );
	}

	ValidateObj( m_mapInMemoryFiles );
	FOR_EACH_MAP( m_mapInMemoryFiles, i )
	{
		ValidatePtr( m_mapInMemoryFiles[i].m_pBuffer );
	}	

	ValidateObj( m_treeInFlightAsyncLoads );
	FOR_EACH_RBTREE_FAST( m_treeInFlightAsyncLoads, i )
	{
		ValidatePtr( m_treeInFlightAsyncLoads[i] );
	}

	ValidateObj( m_treeInFlightAsyncReloads );
	FOR_EACH_RBTREE_FAST( m_treeInFlightAsyncReloads, i )
	{
		ValidatePtr( m_treeInFlightAsyncReloads[i] );
	}
}
#endif

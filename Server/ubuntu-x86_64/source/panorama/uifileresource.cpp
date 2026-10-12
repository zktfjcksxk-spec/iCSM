//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "panorama/uifileresource.h"
#include "tier1/fileio.h"
#ifndef SOURCE2_PANORAMA
#include <vrapi.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: Determines the supported resource image type from a file resource
//-----------------------------------------------------------------------------
EResourceImageType panorama::DetermineResourceType( const CFileResource &resource )
{
	const char *pchExtension = NULL;

	if( resource.BIsHTTPURL() )
	{
		// find end of path, before get params
		const CUtlString &strPath = resource.GetReferencePath();
		const char *pchEnd = V_strstr( strPath.String(), "?" );
		if( !pchEnd )
			pchEnd = strPath.String() + strPath.Length();

		// look backward, searching for the start of the extension
		while( pchEnd != strPath.String() )
		{
			if( *pchEnd == '.' )
			{
				pchExtension = pchEnd + 1;
				break;
			}

			pchEnd--;
		}
	}
	else
	{
		pchExtension = V_GetFileExtension( resource.GetReferencePath().String() );
	}

	if( pchExtension == NULL )
		return k_EResourceImageTypeUnknown;

	if( V_strnicmp( pchExtension, "webm", 4 ) == 0 )
		return k_EResourceImageTypeMovie;
	else if( V_strnicmp( pchExtension, "tga", 3 ) == 0 || V_strnicmp( pchExtension, "png", 3 ) == 0 || V_strnicmp( pchExtension, "jpg", 3 ) == 0 || V_strnicmp( pchExtension, "gif", 3 ) == 0 || V_strnicmp( pchExtension, "svg", 3 ) == 0 || V_strnicmp( pchExtension, "iic", 3 ) == 0 )
		return k_EResourceImageTypeImage;

	return k_EResourceImageTypeUnknown;
}

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CFileResource::CFileResource()
{
	m_eRefType = eNone;
}


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CFileResource::CFileResource( const char *pchUTF8Path )
{
	Set( pchUTF8Path );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if valve domain
//-----------------------------------------------------------------------------
bool BValveDomain( const char *pchDomain )
{
	const char k_pchValveSoftware[] = ".valvesoftware.com";
	const char *pchFound = V_strstr( pchDomain, k_pchValveSoftware );
	if ( pchFound && *(pchFound + V_ARRAYSIZE( k_pchValveSoftware ) - 1) == '\0' )
		return true;

	const char k_pchSteamPowered[] = ".steampowered.com";
	pchFound = V_strstr( pchDomain, k_pchSteamPowered );
	if ( pchFound && *(pchFound + V_ARRAYSIZE( k_pchSteamPowered ) - 1) == '\0' )
		return true;

	return false;	
}


//-----------------------------------------------------------------------------
// Purpose: Sets from UTF8
//-----------------------------------------------------------------------------
void CFileResource::Set( const char *pchUTF8Path )
{
	m_eRefType = eNone;
	m_vecCookieHeaders.RemoveAll();
	if ( !pchUTF8Path || pchUTF8Path[0] == '\0' )
		return;

	if ( !V_strnicmp( "http://", pchUTF8Path, 7 ) || !V_strnicmp( "https://", pchUTF8Path, 8 ) )
	{
		m_eRefType = eHTTPURL;
		
		bool bReplacementDone = false;
		const char *pchBaseDir = NULL;
		const char *pchStart = V_stristr( pchUTF8Path, "://" ) + 3;
		uint32 unPathSubLen = pchStart - pchUTF8Path;
		if ( pchStart[0] == '{' )
		{
			const char *pchClosingBracket = V_stristr( pchStart+1, "}" );
			if ( pchClosingBracket )
			{
				bReplacementDone = true;

				unPathSubLen = pchClosingBracket - pchStart + 1;
				Assert( unPathSubLen > 0 && unPathSubLen < 256 );

				char rgchPathName[256];
				V_strncpy( rgchPathName, pchStart, MIN( unPathSubLen + 1, V_ARRAYSIZE( rgchPathName ) ) );
				if ( unPathSubLen <= V_ARRAYSIZE( rgchPathName ) - 1 )
					rgchPathName[unPathSubLen] = 0;

				pchBaseDir = UIEngine()->GetRemoteHostForNamedHost( rgchPathName );
				m_vecCookieHeaders.AddVectorToTail( UIEngine()->GetCookieHeadersForNamedRemoteHost( rgchPathName ) );
				if ( !m_vecCookieHeaders.Count() )
					m_vecCookieHeaders.AddVectorToTail( UIEngine()->GetCookieHeadersForRemoteHost( pchBaseDir ) );

				if ( pchStart[unPathSubLen] == '/' )
					++unPathSubLen;

				CUtlString strPath;
				strPath.Format( "http://%s%s", pchBaseDir, pchStart + unPathSubLen );

				m_sReference.Set( strPath.String() );
			}
		}

		if ( !bReplacementDone )
		{
			m_sReference =  pchUTF8Path;

			char rgchDomain[1024];
			V_ExtractDomainFromURL( pchUTF8Path, rgchDomain, V_ARRAYSIZE( rgchDomain ) );
			m_vecCookieHeaders.AddVectorToTail( UIEngine()->GetCookieHeadersForRemoteHost( rgchDomain ) );

#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
			// this is ugly.. would prefer to use the UIEngine interface and update cookies per domain however there are no events currently for
			// hardware devices changing and the domains to check are in tenfoot code, not panorama code.
			if ( BValveDomain( rgchDomain ) )
			{
				CUtlStringBuilder strBuilder;
				bool bIncludeVRCookies = false;
				CUtlVector< EAttachedHardwareDevice > vecDevices = UIInputEngine()->GetAttachedHardwareDevices();
				FOR_EACH_VEC( vecDevices, i )
				{
					if ( BIsVRHMDHardwareDevice( vecDevices[i] ) )
					{
						bIncludeVRCookies = true;
					}

					if ( strBuilder.IsEmpty() )
						strBuilder.Append( "connectedDevices=" );
					else
						strBuilder.AppendChar( ',' );

					strBuilder.AppendFormat( "%d", vecDevices[i] );
				}

				if ( !strBuilder.IsEmpty() )
					m_vecCookieHeaders.AddToTail( strBuilder.String() );

				if ( bIncludeVRCookies && vrapi::VRChaperone() )
				{
					float flWidth, flDepth;
					if ( vrapi::VRChaperone()->GetPlayAreaSize( &flWidth, &flDepth ) )
					{
						strBuilder.Format( "playArea=%0.1f,%0.1f", flWidth, flDepth );

						m_vecCookieHeaders.AddToTail( strBuilder.String() );
					}
				}

				if ( bIncludeVRCookies && ClientVR()->IsVRModeActive() )
				{
					m_vecCookieHeaders.AddToTail( "vractive=1" );
				}
			}
#endif
		}
	}
	else if ( !V_strnicmp( "smb://", pchUTF8Path, 6 ) )
	{
		m_eRefType = eSMB;
		m_sReference =  pchUTF8Path;
	}
	else if ( !V_strnicmp( "file://", pchUTF8Path, 7 ) )
	{
		m_eRefType = eFile;

		CUtlString strFullName;
		uint32 unPathSubLen = 7;
		if ( pchUTF8Path[7] == '{' )
		{
			const char *pchClosingBracket = V_stristr( pchUTF8Path+8, "}" );
			char rgchPathName[256];

			if ( pchClosingBracket )
			{
				unPathSubLen = pchClosingBracket - pchUTF8Path + 1;
				Assert( unPathSubLen > 7 && unPathSubLen - 7 < 256 );
								
				V_strncpy( rgchPathName, pchUTF8Path+7, MIN( unPathSubLen - 7 + 1, V_ARRAYSIZE( rgchPathName ) ) );
				if ( unPathSubLen - 7 <= V_ARRAYSIZE( rgchPathName ) - 1 )
					rgchPathName[unPathSubLen-7] = 0;
			}

			// Skip the next / as well, so the path won't appear absolute when we don't want it to
			if ( pchUTF8Path[unPathSubLen] == '/' )
				++unPathSubLen;

			if ( pchClosingBracket )
			{
				UIEngine()->GetLocalPathForRelativePath( rgchPathName, pchUTF8Path + unPathSubLen, strFullName );
			}
		}
		else if ( pchUTF8Path[7] == '/' && IsWindows() ) // on windows skip the 3rd / to get the abs path (file:///c:/file.png), not wanted on posix
		{
			unPathSubLen++;
		}
		char *pchAlloc = NULL;
		int nFullNameLength = strFullName.Length();
		if ( nFullNameLength == 0 )
		{
			const char *pchBaseDir = UIEngine()->GetApplicationInstallPath();
			size_t cchBaseDir = V_strlen( pchBaseDir );
			size_t cchPath = V_strlen( pchUTF8Path + unPathSubLen );
			size_t cubAlloc = cchPath + cchBaseDir + 2;						// 1 for NUL, one for possible path separator

			pchAlloc = (char*)malloc( cubAlloc );
			V_MakeAbsolutePath( pchAlloc, cubAlloc, pchUTF8Path + unPathSubLen, pchBaseDir );
		}
		else
		{
			pchAlloc = (char*)malloc( nFullNameLength + 1);
			V_strncpy( pchAlloc, strFullName.String(), nFullNameLength + 1);
		}
#ifndef PANORAMA_USE_S1WRAPPER
		m_sReference.SetPtr( pchAlloc );
#else
		m_sReference = pchAlloc;
		free( pchAlloc );
#endif
	}
	else if ( !V_strnicmp( "s2r://", pchUTF8Path, 6 ) )
	{
		// Source2 relative path
		m_eRefType = eSource2Relative;

		CUtlString strFullName = pchUTF8Path + 6;
		if ( strFullName.Length() )
			V_FixSlashes( strFullName.Access() );

		char *pchAlloc = (char*)malloc( strFullName.Length() + 1);
		V_strncpy( pchAlloc, strFullName.String(), strFullName.Length() + 1);

#ifndef PANORAMA_USE_S1WRAPPER
		m_sReference.SetPtr( pchAlloc );
#else
		m_sReference = pchAlloc;
		free( pchAlloc );
#endif
	}
	else if ( !V_strnicmp( "raw://", pchUTF8Path, 6 ) )
	{
		// Source2 relative path
		m_eRefType = eRawFile;

		CUtlString strFullName = pchUTF8Path + 6;
		if ( strFullName.Length() )
			V_FixSlashes( strFullName.Access() );

		char *pchAlloc = (char*)malloc( strFullName.Length() + 1);
		V_strncpy( pchAlloc, strFullName.String(), strFullName.Length() + 1);
#ifndef PANORAMA_USE_S1WRAPPER
		m_sReference.SetPtr( pchAlloc );
#else
		m_sReference = pchAlloc;
#endif
	}
	else
	{
		AssertMsg1( false, "Unsupported resource type %s", pchUTF8Path );
	}
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CFileResource::~CFileResource()
{
}


//-----------------------------------------------------------------------------
// Purpose: true if its a parsed file type
//-----------------------------------------------------------------------------
bool CFileResource::BIsValid() const 
{
	return m_eRefType != eNone;
}


//-----------------------------------------------------------------------------
// Purpose: true if a http object
//-----------------------------------------------------------------------------
bool CFileResource::BIsHTTPURL() const
{
	return m_eRefType == eHTTPURL;
}


//-----------------------------------------------------------------------------
// Purpose: Get any cookie header that should be set for the httpurl when used
//-----------------------------------------------------------------------------
const CUtlVector<CUtlString> &CFileResource::GetCookieHeadersForHTTPURL() const
{
	Assert( m_eRefType == eHTTPURL );
	return m_vecCookieHeaders;
}


//-----------------------------------------------------------------------------
// Purpose: true if a local file object
//-----------------------------------------------------------------------------
bool CFileResource::BIsLocalPath() const
{
	return ( m_eRefType == eFile ||  m_eRefType == eSource2Relative || m_eRefType == eRawFile );
}


//-----------------------------------------------------------------------------
// Purpose: true if a SMB object
//-----------------------------------------------------------------------------
bool CFileResource::BIsSMBShare() const
{
	return m_eRefType == eSMB;
}


//-----------------------------------------------------------------------------
// Purpose: get the file path for this resource, only valid if of type File
//-----------------------------------------------------------------------------
const CUtlString &CFileResource::GetReferencePath() const
{
	return m_sReference;
}

CUtlString &CFileResource::GetReferencePathForModify()
{
	return m_sReference;
}

//-----------------------------------------------------------------------------
// Purpose: copy constructor
//-----------------------------------------------------------------------------
CFileResource &CFileResource::operator=( const CFileResource & src)
{
	m_eRefType = src.m_eRefType;
	m_sReference = src.m_sReference;
	m_vecCookieHeaders.RemoveAll();
	m_vecCookieHeaders.AddVectorToTail( src.m_vecCookieHeaders );
	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: comparison
//-----------------------------------------------------------------------------
bool CFileResource::operator<( const CFileResource &left ) const
{
	if( m_eRefType != left.m_eRefType )
		return m_eRefType < left.m_eRefType;
	return V_stricmp( left.m_sReference, m_sReference ) < 0;
}


//-----------------------------------------------------------------------------
// Purpose: comparison
//-----------------------------------------------------------------------------
bool CFileResource::operator==( const CFileResource & left)
{
	return (m_eRefType == left.m_eRefType && m_sReference == left.m_sReference) ;
}

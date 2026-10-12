//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Enum name to string and vice versa utilities
//
//=============================================================================

#ifndef ENUMUTILS_H
#define ENUMUTILS_H
#pragma once


struct EnumString_s
{
	int nValue;
	const char *pszString;
};

// starts defining a enum value string map
#define ENUMSTRINGS_START( etype ) static const EnumString_s s_##etype[] = {

// ends defining a enum value string map. generates PchNameFromEnumName()
#define ENUMSTRINGS_END( etype ) }; const char* PchNameFrom##etype( int nValue ) \
{ for( uint i=0; i<Q_ARRAYSIZE(s_##etype); i++ ) { if ( s_##etype[i].nValue == nValue ) return s_##etype[i].pszString; } \
	AssertMsg2( false, "Missing String for %s (%d)", #etype, nValue ); return "Unknown"; }


// ends defining a enum value string map. generates PchNameFromEnum() and EnumFromName().
#define ENUMSTRINGS_REVERSE( etype, default ) }; const char* PchNameFrom##etype(int nValue ) \
{ for( uint i=0; i<Q_ARRAYSIZE(s_##etype); i++ ) { if ( s_##etype[i].nValue == nValue ) return s_##etype[i].pszString; } \
	AssertMsg2( false, "Missing String for %s (%d)", #etype, nValue ); return "Unknown"; }; \
	etype etype##FromName( const char *pchName ) \
{ for( uint i=0; i<Q_ARRAYSIZE(s_##etype); i++ ) { if ( !Q_stricmp( s_##etype[i].pszString, pchName ) ) return (etype)(s_##etype[i].nValue); } \
	return default; } \
	bool BIsValid##etype(int nValue) \
{ for( uint i=0; i<Q_ARRAYSIZE(s_##etype); i++ ) { if ( s_##etype[i].nValue == nValue ) return true; } \
	return false; }; \


// Adds onto the end of the delcaration of an enum value string map to allow querying the list by index.
#define ENUMSTRINGS_INDEX_LOOKUP( etype )												\
uint32 NGetCount##etype( void )															\
{																						\
	return Q_ARRAYSIZE( s_##etype );													\
}																						\
bool BGet##etype##AtIndex( uint32 nIndex, const char** pName, uint32* nValue )			\
{																						\
	if ( nIndex >= Q_ARRAYSIZE( s_##etype ) )											\
	{																					\
		return false;																	\
	}																					\
	else																				\
	{																					\
		*pName = s_##etype[ nIndex ].pszString;											\
		*nValue = s_##etype[ nIndex ].nValue;											\
		return true;																	\
	}																					\
}

#endif

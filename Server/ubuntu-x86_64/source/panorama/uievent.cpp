//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "tier1/utlsymbol.h"
#include "splitstring.h"

#ifdef SOURCE2_PANORAMA
#include "enumutils_panorama.h"
#else
#include "enumutils.h"
#endif

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

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

struct LocalRegistration_t
{
	UIEventFactory factory;
	CPanoramaSymbol *pSymToUpdate;
};

CUtlMap< CUtlSymbol, LocalRegistration_t, int, CDefLess< CUtlSymbol > > &MapLocalRegisteredEvents()
{
	static CUtlMap< CUtlSymbol, LocalRegistration_t, int, CDefLess< CUtlSymbol > > s_mapRegisteredEvents;
	return s_mapRegisteredEvents;
}

void panorama::RegisterEventTypesWithEngine( IUIEngine *pEngine )
{
	CUtlMap< CUtlSymbol, LocalRegistration_t, int, CDefLess< CUtlSymbol > > &mapClientEvents = MapLocalRegisteredEvents();

	FOR_EACH_MAP_FAST( mapClientEvents, i )
	{
		LocalRegistration_t reg = mapClientEvents.Element( i );
		*(reg.pSymToUpdate) = mapClientEvents.Key( i ).String();
		pEngine->RegisterEventWithEngine( *(reg.pSymToUpdate), reg.factory );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Wrappers to handle copying async params. const char * is specialized to dup the string
//-----------------------------------------------------------------------------
namespace panorama
{

template <>
void UIEventSet( const char** pTo, const char *&pFrom )
{
	*pTo = strdup( (const char *&)pFrom );
}

template <>
void UIEventSet( IUIEvent** pTo, IUIEvent *&pFrom )
{
	if ( pFrom != NULL )
	{
		*pTo = pFrom->Copy();
	}
	else
	{
		*pTo = NULL;
	}
}

template <>
void UIEventSet( v8::Persistent<v8::Function>** pTo, v8::Persistent<v8::Function> *&pFrom )
{
	v8::Isolate::Scope isolate_scope( UIEngine()->GetV8Isolate() );
	v8::HandleScope handle_scope( UIEngine()->GetV8Isolate() );

	// Call JS success function
	v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngine()->GetV8Isolate(), *(pFrom) );

	*pTo = new v8::Persistent<v8::Function>();
	(*pTo)->Reset( UIEngine()->GetV8Isolate(), fnLocal );
}

template <>
void UIEventFree( const char *& p )
{
	free( (void*)p );
}

template <>
void UIEventFree( IUIEvent *& p )
{
	delete p;
}

template <>
void UIEventFree( v8::Persistent<v8::Function> *&p )
{
	p->Reset();
	delete p;
}

#ifdef DBGFLAG_VALIDATE
template <>
void UIEventValidate( CValidator &validator, const char *& p )
{
	if ( p )
		validator.ClaimMemory( (void*)p );
}

template <>
void UIEventValidate( CValidator &validator, IUIEvent *& p )
{
	ValidatePtr( p );
}

template <>
void UIEventValidate( CValidator &validator, v8::Persistent<v8::Function> *& p )
{
	validator.ClaimMemory( p );
}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: UI Event instantiated when dispatching a event defined by javascript
//-----------------------------------------------------------------------------

namespace panorama
{

	class CUIEventJS : public CUIEventBase
	{
	public:
		CUIEventJS( CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel ) : CUIEventBase( symEvent, pTargetPanel ) {}

		CUIEventJS( CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, const CUtlVector< v8::Local< v8::Value > > &args )
		: 
			CUIEventBase( symEvent, pTargetPanel )
		{
			for ( int nArg = 0; nArg < args.Count(); ++nArg )
			{
				v8::Persistent< v8::Value > *pPersistent = new v8::Persistent< v8::Value >( UIEngine()->GetV8Isolate(), args.Element( nArg ) );
				m_args.AddToTail( pPersistent );
			}
		}

		virtual ~CUIEventJS()
		{
			for ( int nArg = 0; nArg < m_args.Count(); ++nArg )
			{
				v8::Persistent< v8::Value > *pPersistent = m_args.Element( nArg );
				pPersistent->Reset();
				delete pPersistent;
			}
		}

		virtual bool Dispatch( CUtlAbstractDelegate pFunc )
		{
			Warning( "Unable to dispatch '%s' event to non javascript handler\n", GetEventType().String() );
			return false;
		}

		virtual IUIEvent *Copy() const
		{
			CUIEventJS *pNewEvent =  new CUIEventJS( GetEventType(), GetTargetPanel().Get() );

			// Copy arguments
			for ( int nArg = 0; nArg < m_args.Count(); ++nArg )
			{
				v8::Persistent< v8::Value > *pPersistent = new v8::Persistent< v8::Value >( UIEngine()->GetV8Isolate(), *m_args.Element( nArg ) );
				pNewEvent->m_args.AddToTail( pPersistent );
			}

			return pNewEvent;
		}

		virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
		{
			*pCount = m_args.Count();
			if ( m_args.Count() > 0 )
			{
				*pArgs = new v8::Handle< v8::Value >[m_args.Count()];
				for ( int nArg = 0; nArg < m_args.Count(); ++nArg )
				{
					( *pArgs )[nArg] = v8::Local< v8::Value >::New( GetV8Isolate(), *m_args.Element( nArg ) );
				}
			}
			else
			{
				*pArgs = NULL;
			}
		}

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{

		}
#endif

		CUtlVector< v8::Persistent< v8::Value > * > m_args;
	};

	class CUIPanelEventJS : public CUIEventBase
	{
	public:
		CUIPanelEventJS( CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel ) : CUIEventBase( symEvent, pTargetPanel ) {}

		CUIPanelEventJS( CPanoramaSymbol symEvent, const IUIPanel *pTargetPanel, const CUtlVector< v8::Local< v8::Value > > &args )
		: 
			CUIEventBase( symEvent, pTargetPanel )
		{
			for ( int nArg = 0; nArg < args.Count(); ++nArg )
			{
				v8::Persistent< v8::Value > *pPersistent = new v8::Persistent< v8::Value >( UIEngine()->GetV8Isolate(), args.Element( nArg ) );
				m_args.AddToTail( pPersistent );
			}
		}

		virtual ~CUIPanelEventJS()
		{
			for ( int nArg = 0; nArg < m_args.Count(); ++nArg )
			{
				v8::Persistent< v8::Value > *pPersistent = m_args.Element( nArg );
				pPersistent->Reset();
				delete pPersistent;
			}
		}

		virtual bool Dispatch( CUtlAbstractDelegate pFunc )
		{
			Warning( "Unable to dispatch '%s' event to non javascript handler\n", GetEventType().String() );
			return false;
		}

		virtual IUIEvent *Copy() const
		{
			CUIPanelEventJS *pNewEvent = new CUIPanelEventJS( GetEventType(), GetTargetPanel().Get() );

			// Copy arguments
			for ( int nArg = 0; nArg < m_args.Count(); ++nArg )
			{
				v8::Persistent< v8::Value > *pPersistent = new v8::Persistent< v8::Value >( UIEngine()->GetV8Isolate(), *m_args.Element( nArg ) );
				pNewEvent->m_args.AddToTail( pPersistent );
			}

			return pNewEvent;
		}

		virtual void GetJavaScriptArgs( int *pCount, v8::Handle<v8::Value> **pArgs )
		{
			*pCount = 1 + m_args.Count();
			*pArgs = new v8::Handle< v8::Value >[ 1 + m_args.Count() ];
			SetPanelEventJSArg( GetTargetPanel().Get(), *pArgs );
			for ( int nArg = 0; nArg < m_args.Count(); ++nArg )
			{
				( *pArgs )[nArg + 1] = v8::Local< v8::Value >::New( GetV8Isolate(), *m_args.Element( nArg ) );
			}
		}

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{

		}
#endif

		CUtlVector< v8::Persistent< v8::Value > * > m_args;
	};

}


//-----------------------------------------------------------------------------
// Purpose: Parsing function for events define in javascript
//-----------------------------------------------------------------------------
panorama::IUIEvent *panorama::UIEvent::JSCreateEventFromString( panorama::CPanoramaSymbol symEvent, panorama::IUIPanel *pPanel, const CUtlVector< v8::Local< v8::Value > > &args )
{
	const panorama::IUIPanelClient *pTarget = pPanel ? pPanel->ClientPtr() : nullptr;
	IUIEvent *pEvent = new panorama::CUIEventJS( symEvent, pTarget ? pTarget->UIPanel() : nullptr, args );
	return pEvent;
}

panorama::IUIEvent *panorama::UIEvent::JSCreatePanelEventFromString( panorama::CPanoramaSymbol symEvent, panorama::IUIPanel *pPanel, const CUtlVector< v8::Local< v8::Value > > &args )
{
	const panorama::IUIPanelClient *pTarget = pPanel ? pPanel->ClientPtr() : nullptr;
	IUIEvent *pEvent = new panorama::CUIPanelEventJS( symEvent, pTarget ? pTarget->UIPanel() : nullptr, args );
	return pEvent;
}

CUtlString panorama::UIEvent::JSFormatEventArgs( const char *pchArgNames )
{
	CUtlString str;

	if ( pchArgNames )
	{
		CSplitString vecArgNames( pchArgNames, "," );
		for ( int i = 0; i < vecArgNames.Count(); ++i )
		{
			if ( i != 0 )
			{
				str.Append( ", " );
			}

			str.Append( "js_raw_arg" );

			CUtlString strArgName( vecArgNames[i] );
			strArgName.Trim();
			str.Append( " " );
			str.Append( strArgName );
		}
	}

	return str;
}

//-----------------------------------------------------------------------------
// Purpose: Registers an event
//-----------------------------------------------------------------------------
void panorama::RegisterUIEvent( panorama::CPanoramaSymbol *pSymEvent, const char *pchEventType, int cParams, bool bPanelEvent, 
	PFN_ParseUIEvent pfnParseUIEvent, PFN_FormatUIEventArgs pfnFormatUIEventArgs,
	PFN_MakeUIEvent0 pfnMakeUIEvent0, PFN_MakeUIEvent1Repeats pfnMakeUIEvent1Repeats, PFN_MakeUIEvent1Source pfnMakeUIEvent1Source,
	const char *pchDocumentationArgs, const char *pchDocumentationDescription, EEventDocFlags eDocFlags )
{	
	LocalRegistration_t reg;
	reg.factory.m_cParams = cParams;
	reg.factory.m_bPanelEvent = bPanelEvent;

	if ( pfnMakeUIEvent1Repeats )
	{
		reg.factory.m_eMakeUIEventType = k_eMakeUIEventType_Repeats;
		reg.factory.m_pfnMakeUIEvent1Repeats = pfnMakeUIEvent1Repeats;
	}
	else if ( pfnMakeUIEvent1Source )
	{
		reg.factory.m_eMakeUIEventType = k_eMakeUIEventType_Source;
		reg.factory.m_pfnMakeUIEvent1Source = pfnMakeUIEvent1Source;
	}
	else
	{
		reg.factory.m_eMakeUIEventType = k_eMakeUIEventType_NoArguments;
		reg.factory.m_pfnMakeUIEvent0 = pfnMakeUIEvent0;
	}

	reg.factory.m_pfnParseUIEvent = pfnParseUIEvent;
	reg.factory.m_pfnParseUIEventJS = nullptr;
	reg.factory.m_pfnFormatUIEventArgs = pfnFormatUIEventArgs;

	reg.factory.m_pchDocumentationArgs = pchDocumentationArgs;
	reg.factory.m_pchDocumentationDescription = pchDocumentationDescription;
	reg.factory.m_eDocFlags = eDocFlags;

	reg.pSymToUpdate = pSymEvent;

	MapLocalRegisteredEvents().InsertOrReplace( pchEventType, reg );
}


//-----------------------------------------------------------------------------
// Purpose: Skips whitespace
//-----------------------------------------------------------------------------
static const char *SkipWhitespace( const char *pchParam )
{
	while ( V_isspace( pchParam[0] ) )
		pchParam++;

	return pchParam;
}


//-----------------------------------------------------------------------------
// Purpose: Counts the number of event parameters in the string
//-----------------------------------------------------------------------------
int panorama::CountUIEventParams( const char *pchParams )
{	
	int cCount = 0;

	// skip ( if present
	while ( V_isspace( pchParams[0] ) || pchParams[0] == '(' )
	{
		pchParams++;
	}

	if ( pchParams[0] == ')' )
		return 0;

	// at least 1 param
	cCount++;

	bool bQuotedParam = false;
	bool bStartOfParam = true;
	int cNestedParen = 0;
	while ( pchParams[0] != '\0' )
	{
		// at the start of every param, check if param is quoted
		if ( bStartOfParam )
		{
			bStartOfParam = false;
			pchParams = SkipWhitespace( pchParams );
			if ( pchParams[0] == '\'' )
			{
				bQuotedParam = true;
				pchParams++;
				continue;
			}
		}

		// while quoted, keep going until we find the end of the quote
		if ( bQuotedParam )
		{
			// check for escaped quote. No way to return false from this function so just checking for '
			if ( pchParams[0] == '\\' && pchParams[1] == '\'' )
			{
				pchParams += 2;
				continue;
			}

			if ( pchParams[0] == '\'' )
				bQuotedParam = false;

			pchParams++;
			continue;
		}

		if ( pchParams[0] == '(' )
			++cNestedParen;

		if ( pchParams[0] == ')' )
		{
			if ( cNestedParen > 0 )
				--cNestedParen;
			else
				break;
		}

		if ( cNestedParen == 0 && pchParams[0] == ',' )
		{
			bStartOfParam = true;
			cCount++;
		}

		pchParams++;
	}

	return cCount;
}


//-----------------------------------------------------------------------------
// Purpose: Escapes a UI event param
//-----------------------------------------------------------------------------
void panorama::EscapeUIEventParam( CUtlString *pstrEscaped, const char *pchParam )
{
	pstrEscaped->Set( pchParam );
#if defined( SOURCE2_PANORAMA )
	*pstrEscaped = pstrEscaped->Replace( "\\", "\\\\" );
	*pstrEscaped = pstrEscaped->Replace( "'", "\\'" );
#else
	pstrEscaped->Replace( "\\", "\\\\" );
	pstrEscaped->Replace( "'", "\\'" );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Unescapes a UI event param. Should only use when quoting params
//-----------------------------------------------------------------------------
void panorama::UnescapeUIEventParam( char *pchParam )
{
	char *pchDst = pchParam;
	while ( *pchParam != '\0' )
	{
		if ( *pchParam == '\\' )
		{
			pchParam++;
			if ( *pchParam == '\0' )
				break;
		}

		*pchDst = *pchParam;
		pchDst++;
		pchParam++;
	}

	*pchDst = '\0';
}


//-----------------------------------------------------------------------------
// Purpose: ParseUIEventParamHelper implementation for quoted strings
//-----------------------------------------------------------------------------
static bool ParseUIEventParamQuotedString( CUtlBuffer &bufValue, const char *pchEvent, const char **pchNextParam )
{
	if ( pchEvent[0] != '\'' )
		return false;

	pchEvent++;

	// find end of this param
	int cch = 0;
	while ( pchEvent[cch] != '\0' )
	{
		if ( pchEvent[cch] == '\\' )
		{
			char chNext = pchEvent[cch + 1];
			if ( chNext != '\'' && chNext != '\\' )
				return false;

			cch += 2;
			continue;
		}

		if ( pchEvent[cch] == '\'' )
			break;

		cch++;
	}

	// must be at end of quote, not end of string
	if ( pchEvent[cch] != '\'' )
		return false;

	// make sure only white space after quote
	int cchNext = cch + 1;
	while ( V_isspace( pchEvent[cchNext] ) )
		cchNext++;

	// make sure that the text following the end quote either terminates the parameter or terminates
	// the event string.  Any other case like 'abc'def should error.
	if ( pchEvent[cchNext] != '\0' && pchEvent[cchNext] != ',' && pchEvent[cchNext] != ')' )
		return false;

	// now that we know size, copy escaped string and trailing quote
	bufValue.EnsureCapacity( cch + 1 );
	char *pchValue = (char *)bufValue.Base();
	V_memcpy( pchValue, pchEvent, cch );
	pchValue[cch] = 0;

	// string has already been trimmed but do need to unescape
	UnescapeUIEventParam( pchValue );

	// advance next
	if ( pchEvent[cchNext] == ',' )
		*pchNextParam = &pchEvent[cchNext + 1];
	else
		*pchNextParam = &pchEvent[cchNext];

	return true;
}


bool panorama::ParseUIEventParamHelper( CUtlBuffer &bufValue, const char *pchEvent, const char **pchNextParam )
{
	pchEvent = SkipWhitespace( pchEvent );
	if ( pchEvent[0] == '\'' )
		return ParseUIEventParamQuotedString( bufValue, pchEvent, pchNextParam );

	// find end of this param
	int cch = 0;
	int cNestedParen = 0;
	while ( pchEvent[cch] != '\0' )
	{
		if ( pchEvent[cch] == '(' )
			++cNestedParen;

		if ( pchEvent[cch] == ')' )
		{
			if ( cNestedParen > 0 )
				--cNestedParen;
			else
				break;
		}

		if ( cNestedParen == 0 && pchEvent[cch] == ',' )
			break;

		cch++;
	}


	bufValue.EnsureCapacity( cch + 1 );
	
	char *pchValue = (char *)bufValue.Base();

	V_strncpy( pchValue, pchEvent, cch + 1 );  // +1 for null
	pchValue[cch] = 0;

	V_StrTrim( pchValue );
	if ( pchEvent[cch] == ',' )
		*pchNextParam = &pchEvent[cch + 1 ];
	else
		*pchNextParam = &pchEvent[cch];
	return true;
}

namespace panorama
{

template <> void PanoramaTypeToV8Param<const char>( const char * pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), pIn ? pIn : "" ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param<char>( char * pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), pIn ? pIn : "" ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< CUtlSymbol >( CUtlSymbol &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), pIn.String() ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< CPanoramaSymbol >( CPanoramaSymbol &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), pIn.String() ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< CUtlString >( CUtlString& pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), pIn.Get() ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< CStyleSymbol >( CStyleSymbol &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), pIn.String() ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< uint32 >( uint32 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), (double)pIn );
}
template <> void PanoramaTypeToV8Param< const uint32 >( const uint32 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), ( double )pIn );
}

template <> void PanoramaTypeToV8Param< uint16 >( uint16 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), ( double )pIn );
}
template <> void PanoramaTypeToV8Param< const uint16 >( const uint16 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), ( double )pIn );
}

template <> void PanoramaTypeToV8Param< uint64 >( uint64 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), CNumStr( pIn ).String() ).ToLocalChecked();
}
template <> void PanoramaTypeToV8Param< const uint64 >( const uint64 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), CNumStr( pIn ).String() ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< int32 >( int32 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), (double)pIn );
}
template <> void PanoramaTypeToV8Param< const int32 >( const int32 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), ( double )pIn );
}

template <> void PanoramaTypeToV8Param< int64 >( int64 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), CNumStr( pIn ).String() ).ToLocalChecked();
}
template <> void PanoramaTypeToV8Param< const int64 >( const int64 &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), CNumStr( pIn ).String() ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< float >( float &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), pIn );
}
template <> void PanoramaTypeToV8Param< const float >( const float &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), pIn );
}

template <> void PanoramaTypeToV8Param< double >( double &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), pIn );
}
template <> void PanoramaTypeToV8Param< const double >( const double &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Number::New( GetV8Isolate(), pIn );
}

template <> void PanoramaTypeToV8Param< bool >( bool &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Boolean::New( GetV8Isolate(), pIn );
}
template <> void PanoramaTypeToV8Param< const bool >( const bool &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::Boolean::New( GetV8Isolate(), pIn );
}

template <> void PanoramaTypeToV8Param< Vector2D >( Vector2D &pIn, v8::Handle<v8::Value> *pValueOut )
{
	v8::Handle<v8::Object> ret = v8::Object::New( GetV8Isolate() );
	ret->Set( GetV8Isolate()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "x" ).ToLocalChecked(), v8::Number::New( GetV8Isolate(), pIn.x ) );
	ret->Set( GetV8Isolate()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "y" ).ToLocalChecked(), v8::Number::New( GetV8Isolate(), pIn.y ) );
	*pValueOut = ret;	
}
template <> void PanoramaTypeToV8Param< const Vector2D >( const Vector2D &pIn, v8::Handle<v8::Value> *pValueOut )
{
	v8::Handle<v8::Object> ret = v8::Object::New( GetV8Isolate() );
	ret->Set( GetV8Isolate()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "x" ).ToLocalChecked(), v8::Number::New( GetV8Isolate(), pIn.x ) );
	ret->Set( GetV8Isolate()->GetCurrentContext(), v8::String::NewFromUtf8( GetV8Isolate(), "y" ).ToLocalChecked(), v8::Number::New( GetV8Isolate(), pIn.y ) );
	*pValueOut = ret;
}

ENUMSTRINGS_START( EPanelEventSource_t )
	{ k_ePanelEventSourceInvalid, "invalid" },
	{ k_ePanelEventSourceProgram, "program" },
	{ k_ePanelEventSourceGamepad, "gamepad" },
	{ k_ePanelEventSourceKeyboard, "keyboard" },
	{ k_ePanelEventSourceMouse, "mouse" },
ENUMSTRINGS_REVERSE( EPanelEventSource_t, k_ePanelEventSourceInvalid )

template<> void PanoramaTypeToV8Param< EPanelEventSource_t >( EPanelEventSource_t &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), PchNameFromEPanelEventSource_t( pIn ) ).ToLocalChecked();
}

template <> void PanoramaTypeToV8Param< CUtlVector< IUIPanel * > const >( CUtlVector< IUIPanel * > const &pIn, v8::Handle<v8::Value> *pValueOut )
{
	v8::Local<v8::Array> a = v8::Array::New( GetV8Isolate(), pIn.Count() );
	FOR_EACH_VEC( pIn, i )
	{
		IUIPanel *pPanel = pIn[i];
		v8::Handle<v8::Value> v;
		PanoramaPanelTypeToV8Param( pPanel, &v );
		a->Set( GetV8Isolate()->GetCurrentContext(), i, v );
	}
	*pValueOut = a;
}

template <> void PanoramaTypeToV8Param< v8::Local<v8::Value> >( v8::Local<v8::Value> &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = pIn;
}

template <> void PanoramaTypeToV8Param< v8::Local<v8::Object> >( v8::Local<v8::Object> &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = pIn;
}

template <> void PanoramaTypeToV8Param< v8::Local<v8::Array> >( v8::Local<v8::Array> &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = pIn;
}

#if !defined (SOURCE2_PANORAMA )
template <> void PanoramaTypeToV8Param< CRTime >( CRTime &pIn, v8::Handle<v8::Value> *pValueOut )
{
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), CNumStr( pIn.GetRTime32() ).String() ).ToLocalChecked();
}
#endif


void PanoramaPanelTypeToV8Param( IUIPanel *pIn, v8::Handle<v8::Value> *pValueOut )
{
	if( pIn == NULL )
	{
		*pValueOut = v8::Null( GetV8Isolate() );
		return;
	}

	*pValueOut = v8::Local<v8::Object>::New( GetV8Isolate(), *(UIEngine()->CreateV8PanelInstance( pIn )) );
}


void PanoramaPanelTypeToV8Param( IUIWindow *pIn, v8::Handle<v8::Value> *pValueOut )
{
	if ( pIn == NULL )
	{
		*pValueOut = v8::Null( GetV8Isolate() );
		return;
	}

	*pValueOut = v8::Local<v8::Object>::New( GetV8Isolate(), *(UIEngine()->CreateV8IUIWindowInstance( pIn )) );
}



void PanoramaPanelStyleTypeToV8Param( IUIPanelStyle * &pIn, v8::Handle<v8::Value> *pValueOut )
{
	if( pIn == NULL )
	{
		*pValueOut = v8::Null( GetV8Isolate() );
		return;
	}

	*pValueOut = v8::Local<v8::Object>::New( GetV8Isolate(), *(UIEngine()->CreateV8PanelStyleInstance( pIn )) );
}

void PanoramaTypeToV8ParamJSObject( IUIJSObject *pJSObj, void *pIn, v8::Handle<v8::Value> *pValueOut )
{
	if( pIn == NULL || pJSObj == NULL )
	{
		*pValueOut = v8::Null( GetV8Isolate() );
		return;
	}

	*pValueOut = v8::Local<v8::Object>::New( GetV8Isolate(), *(UIEngine()->CreateV8ObjectInstance( pJSObj->GetJSTypeName(), (void*)pIn, pJSObj ) ) );
	return;
}

template <> void V8ParamToPanoramaType< const char *>( const v8::Handle<v8::Value> &valueIn, const char ** out )
{
	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  valueIn );
	if( !*str )
	{
		*out = NULL;
		return;
	}

	size_t max = V_strlen( *str ) + 1;
	*out = (const char *)malloc( max  );
	V_strncpy( (char*)*out, *str, max );
}

template <> void V8ParamToPanoramaType< CUtlSymbol >( const v8::Handle<v8::Value> &pValueIn, CUtlSymbol* out )
{
	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn );
	if( !*str )
	{
		*out = CUtlSymbol();
		return;
	}
	*out = *str;
}

template <> void V8ParamToPanoramaType< CUtlString >(const v8::Handle<v8::Value>& pValueIn, CUtlString* out )
{
	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn );
	if ( !*str )
	{
		*out = CUtlString();
		return;
	}

	*out = *str;
}

template <> void V8ParamToPanoramaType< CStyleSymbol >( const v8::Handle<v8::Value> &pValueIn, CStyleSymbol* out )
{
	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn );
	if ( !*str )
	{
		*out = CStyleSymbol();
		return;
	}
	*out = *str;
}

template <> void V8ParamToPanoramaType< CPanoramaSymbol >( const v8::Handle<v8::Value> &pValueIn, CPanoramaSymbol* out )
{
	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn );
	if( !*str )
	{
		*out = CPanoramaSymbol();
		return;
	}
	*out = *str;
}

template <> void V8ParamToPanoramaType< v8::Persistent<v8::Function> * >( const v8::Handle<v8::Value> &pValueIn, v8::Persistent<v8::Function> **out )
{
	if( pValueIn.IsEmpty() || !pValueIn->IsFunction() )
	{
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Function type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );

		*out = NULL;
		return;
	}

	*out = new v8::Persistent<v8::Function>();
	(*out)->Reset( UIEngine()->GetV8Isolate(), v8::Handle<v8::Function>::Cast( pValueIn ) );
}

template <> void V8ParamToPanoramaType< v8::Local<v8::Value> >( const v8::Handle<v8::Value> &pValueIn, v8::Local<v8::Value> *out )
{
	*out = v8::Handle<v8::Value>::Cast( pValueIn );
}

template <> void V8ParamToPanoramaType< v8::Local<v8::Object> >( const v8::Handle<v8::Value> &pValueIn, v8::Local<v8::Object> *out )
{
	*out = v8::Handle<v8::Object>::Cast( pValueIn );
}

template <> void V8ParamToPanoramaType< v8::Local<v8::Array> >( const v8::Handle<v8::Value> &pValueIn, v8::Local<v8::Array> *out )
{
	*out = v8::Handle<v8::Array>::Cast( pValueIn );
}

template <> void FreeConvertedParam< const char *>( const char **pOut )
{
	free( (void*)(*pOut) );
	*pOut = nullptr;
}

template <> void FreeConvertedParam< v8::Persistent<v8::Function> *>( v8::Persistent<v8::Function> **pOut )
{
	if ( !*pOut )
		return;

	(*pOut)->Reset();
	delete (*pOut);
	*pOut = nullptr;
}

template <> void V8ParamToPanoramaType< float >( const v8::Handle<v8::Value> &pValueIn, float *out )
{
	if( pValueIn->IsNumber() )
	{
		*out = (float)pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 );
	}
	else
	{
		*out = 0;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Number type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}

template <> void V8ParamToPanoramaType< double >( const v8::Handle<v8::Value> &pValueIn, double *out )
{
	if( pValueIn->IsNumber() )
	{
		*out = (double)pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 );
	}
	else
	{
		*out = 0;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Number type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}

template <> void V8ParamToPanoramaType< int >( const v8::Handle<v8::Value> &pValueIn, int *out )
{
	if( pValueIn->IsNumber() )
	{
		*out = (int)pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 );
	}
	else
	{
		*out = 0;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Number type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}

template <> void V8ParamToPanoramaType< uint32 >( const v8::Handle<v8::Value> &pValueIn, uint32 *out )
{
	if( pValueIn->IsNumber() )
	{
		*out = (uint32)pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 );
	}
	else
	{
		*out = 0;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Number type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}

template <> void V8ParamToPanoramaType< uint64 >( const v8::Handle<v8::Value> &pValueIn, uint64 *out )
{
	if( pValueIn->IsNumber() )
	{
		*out = (uint64)pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 );
	}
	else if ( pValueIn->IsString() )
	{
		// 64-bit integral types are passed from Panorama to V8 as strings, so this is the reverse operation
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		uint64 res = V_atoui64( *str );
		*out = res;
	}
	else
	{
		*out = 0;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Number type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}

template <> void V8ParamToPanoramaType< bool >( const v8::Handle<v8::Value> &pValueIn, bool *out )
{
	if( pValueIn->IsBoolean() )
	{
		*out = (int)pValueIn->ToBoolean( GetV8Isolate() )->Value();
	}
	else if( pValueIn->IsNumber() )
	{
		*out = ( pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 ) != 0.0 );
	}
	else
	{
		*out = false;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected bool type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}

// Helper function to extract internal field from a v8 object and return it as a void *. If the input is not
// a v8 object, or has no internal field, returns nullptr
void *V8ObjectGetInternalField( const v8::Handle<v8::Value> &pValueIn )
{
	void *res = nullptr;

	if( pValueIn->IsObject() )
	{
		v8::Local<v8::Object> obj = pValueIn->ToObject( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::Object>() );
		if( obj->InternalFieldCount() == 1 )
		{
			v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast( obj->GetInternalField( 0 ) );
			res = wrap->Value();
		}
		else
		{
			GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "V8ParamToPanoramaType expected IUIPanel object to convert, but got some other invalid object" ).ToLocalChecked() );
		}
	}
	else
	{
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "V8ParamToPanoramaType expected object type to convert, but got something else" ).ToLocalChecked() );
	}

	return res;
}

template <> void V8ParamToPanoramaType< IUIPanel * >( const v8::Handle<v8::Value> &pValueIn, IUIPanel **out )
{
	*out = nullptr;
	IUIPanel *pPanel = (IUIPanel*)V8ObjectGetInternalField( pValueIn );
	if( UIEngine()->IsValidPanelPointer( pPanel ) )
	{
		*out = pPanel;
	}
	else
	{
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "V8ParamToPanoramaType expected IUIPanel object to convert, but got some other invalid object" ).ToLocalChecked() );
	}
}

#ifndef PANORAMA_EXPORTS
template <> void V8ParamToPanoramaType< CPanel2D * >( const v8::Handle<v8::Value> &pValueIn, CPanel2D **out )
{
	IUIPanel *pUIPanelOut = NULL;
	V8ParamToPanoramaType( pValueIn, &pUIPanelOut );

	*out = pUIPanelOut ? (CPanel2D*)pUIPanelOut->ClientPtr() : NULL;
}

template <> void V8ParamToPanoramaType< const CPanel2D * >( const v8::Handle<v8::Value> &pValueIn, const CPanel2D **out )
{
	IUIPanel *pUIPanelOut = NULL;
	V8ParamToPanoramaType( pValueIn, &pUIPanelOut );

	*out = pUIPanelOut ? (CPanel2D*)pUIPanelOut->ClientPtr() : NULL;
}
#endif 



template <> void V8ParamToPanoramaType< CUtlVector< IUIPanel *> >( const v8::Handle<v8::Value> &pValueIn, CUtlVector< IUIPanel *>*out )
{
	if( pValueIn->IsArray() )
	{
		v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast( pValueIn );
		out->EnsureCapacity( arr->Length() );
		for( uint32_t i = 0; i < arr->Length(); ++i )
		{
			if( arr->Get( GetV8Isolate()->GetCurrentContext(), i ).FromMaybe( v8::Local<v8::Value>() )->IsObject() )
			{
				IUIPanel *pPanel = NULL;
				V8ParamToPanoramaType( arr->Get( GetV8Isolate()->GetCurrentContext(), i ).FromMaybe( v8::Local<v8::Value>() ), &pPanel );
				out->AddToTail( pPanel );
			}
			else
			{
				out->AddToTail( NULL );
				GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "V8ParamToPanoramaType expected array of panel objects to convert, but got a different type inside array" ).ToLocalChecked() );
			}
		}
	}
	else
	{
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "V8ParamToPanoramaType expected array type to convert, but got something else" ).ToLocalChecked() );
	}
}

template <> void V8ParamToPanoramaType< IUIPanelStyle * >( const v8::Handle<v8::Value> &pValueIn, IUIPanelStyle **out )
{
	*out = nullptr;

	IUIPanel *pPanel;
	V8ParamToPanoramaType( pValueIn, &pPanel );

	if ( pPanel )
	{
		*out = pPanel->AccessIUIStyle();
	}
	else
	{
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), "V8ParamToPanoramaType expected IUIPanelStyle object to convert, but got some other invalid object" ).ToLocalChecked() );
	}
}

template <> void V8ParamToPanoramaType< CJSKeyframesObject* >( const v8::Handle<v8::Value> &pValueIn, CJSKeyframesObject **out )
{
	*out = (CJSKeyframesObject*)V8ObjectGetInternalField( pValueIn );
}

#if defined (SOURCE2_PANORAMA )
template <> void V8ParamToPanoramaType< time_t >( const v8::Handle<v8::Value> &pValueIn, time_t *out )
{
	if ( pValueIn->IsNumber() )
	{
		*out = ( time_t )( pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 ) );
	}
	else
	{
		*out = 0;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Number type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}
#else
template <> void V8ParamToPanoramaType< CRTime >( const v8::Handle<v8::Value> &pValueIn, CRTime *out )
{
	if ( pValueIn->IsNumber() )
	{
		*out = CRTime((int64_t)pValueIn->NumberValue( GetV8Isolate()->GetCurrentContext() ).FromMaybe( 0 ));
	}
	if ( pValueIn->IsString() )
	{
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		*out = CRTime( V_atoi64( *str ) );
	}
	else
	{
		*out = 0;
		v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  pValueIn->ToString( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( v8::Local<v8::String>() ) );
		CFmtStr err( "V8ParamToPanoramaType expected Number type to convert, but got something else (%s)", *str );
		GetV8Isolate()->ThrowException( v8::String::NewFromUtf8( GetV8Isolate(), err.Get() ).ToLocalChecked() );
	}
}
#endif

#ifdef PANORAMA_EXPORTS
template <> void V8ParamToPanoramaType< CPanelStyle * >( const v8::Handle<v8::Value> &pValueIn, CPanelStyle **out )
{
	IUIPanelStyle *pUIOut = NULL;
	V8ParamToPanoramaType( pValueIn, &pUIOut );
	*out = (CPanelStyle*)pUIOut;
}
#endif

template <> void V8ParamToPanoramaType< panorama::ScrollBehavior_t > ( const v8::Handle<v8::Value> &pValueIn, panorama::ScrollBehavior_t *out )
{
	int nScrollBehaviour;
	V8ParamToPanoramaType( pValueIn, &nScrollBehaviour );
	*out =  ( (ScrollBehavior_t) nScrollBehaviour );
}

template <> 
bool ParseUIEventParam< IUIEvent * >( IUIEvent **pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// check for quoted param. Limiting to quoted params as we can just pass unquoted on w/o allocation
	pchEvent = SkipWhitespace( pchEvent );
	IUIEvent *pEvent = NULL;
	if ( pchEvent[0] == '\'' )
	{
		CUtlBuffer bufValue;
		if ( !ParseUIEventParamQuotedString( bufValue, pchEvent, pchNextParam ) )
			return false;

		pchEvent = (const char *)bufValue.Base();

		// already set pchNextParam, don't care about result for temp buffer version
		const char *pchUnused = NULL;
		pEvent = UIEngine()->CreateEventFromString( pPanel, pchEvent, &pchUnused );
	}
	else
	{
		pEvent = UIEngine()->CreateEventFromString( pPanel, pchEvent, pchNextParam );

		// If we successfully parsed and this isn't the final parameter, then we need to
		// skip the comma for the next parameter
		if ( pEvent && ( *pchNextParam )[ 0 ] == ',' )
		{
			( *pchNextParam )++;
		}
	}

	if ( pEvent )
	{
		*pOut = pEvent;
		return true;
	}

	*pOut = NULL;
	return false;
}


template <> 
bool ParseUIEventParam< const char * >( const char **pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	*pOut = strdup( (const char *)bufValue.Base() );
	return true;
}

template <>
bool ParseUIEventParam< uint8 >( uint8 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = ( const char * )bufValue.Base();
	*pOut = V_atoi( rgchValue );
	if ( *pOut == 0 && rgchValue[ 0 ] != '0' )
		return false;

	return true;
}

template <>
bool ParseUIEventParam< uint16 >( uint16 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = ( const char * )bufValue.Base();
	*pOut = V_atoi( rgchValue );
	if ( *pOut == 0 && rgchValue[ 0 ] != '0' )
		return false;

	return true;
}

template <> 
bool ParseUIEventParam< uint32 >( uint32 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = (const char *)bufValue.Base();
	*pOut = V_atoi( rgchValue );
	if ( *pOut == 0 && rgchValue[0] != '0' )
		return false;

	return true;
}

template <> 
bool ParseUIEventParam< panorama::EPanelEventSource_t >( panorama::EPanelEventSource_t *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = (const char *)bufValue.Base();
	*pOut = k_ePanelEventSourceProgram;
	if( V_stricmp( rgchValue, "program" ) == 0 )
		*pOut = k_ePanelEventSourceProgram;
	else if( V_stricmp( rgchValue, "gamepad" ) == 0 )
		*pOut = k_ePanelEventSourceGamepad;
	else if( V_stricmp( rgchValue, "keyboard" ) == 0 )
		*pOut = k_ePanelEventSourceKeyboard;
	else if( V_stricmp( rgchValue, "mouse" ) == 0 )
		*pOut = k_ePanelEventSourceMouse;
	else if( V_stricmp( rgchValue, "lastactivate" ) == 0 && pPanel )
		*pOut = pPanel->GetParentWindow()->UIWindowInput()->GetLastPanelEventSource();
	else
		return false;
	
	return true;
}

template <>
bool ParseUIEventParam< panorama::ScrollBehavior_t >( panorama::ScrollBehavior_t *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = (const char *)bufValue.Base();
	*pOut = SCROLL_BEHAVIOR_DEFAULT;

	if ( V_stricmp( rgchValue, "min-distance" ) == 0 )
		*pOut = SCROLL_BEHAVIOR_SCROLL_MINIMUM_DISTANCE;
	else if ( V_stricmp( rgchValue, "topleft" ) == 0 )
		*pOut = SCROLL_BEHAVIOR_SCROLL_TO_TOPLEFT_EDGE;
	else if ( V_stricmp( rgchValue, "bottomright" ) == 0 )
		*pOut = SCROLL_BEHAVIOR_SCROLL_TO_BOTTOMRIGHT_EDGE;
	else if ( V_stricmp( rgchValue, "center" ) == 0 )
		*pOut = SCROLL_BEHAVIOR_SCROLL_TO_CENTER;
	else if ( V_stricmp( rgchValue, "default" ) == 0 )
		*pOut = SCROLL_BEHAVIOR_DEFAULT;
	else
		return false;

	return true;
}

template <> 
bool ParseUIEventParam< uint64 >( uint64 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = (const char *)bufValue.Base();
	*pOut = V_strtoui64( rgchValue, NULL, 10 );
	if ( *pOut == 0 && rgchValue[0] != '0' )
		return false;

	return true;
}


template <> 
bool ParseUIEventParam< int32 >( int32 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = (const char *)bufValue.Base();
	*pOut = V_atoi( rgchValue );
	if ( *pOut == 0 && rgchValue[0] != '0' )
		return false;

	return true;
}

template <> 
bool ParseUIEventParam< int64 >( int64 *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = (const char *)bufValue.Base();
	*pOut = V_atoi64( rgchValue );
	if ( *pOut == 0 && rgchValue[0] != '0' )
		return false;

	return true;
}

template <> 
bool ParseUIEventParam< bool >( bool *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	const char *rgchValue = (const char *)bufValue.Base();
	switch ( rgchValue[0] )
	{
	case 't':
	case 'T':
	case '1':
		*pOut = true;
		return true;

	case 'f':
	case 'F':
	case '0':
		*pOut = false;
		return true;

	default:
		return false;
	}
}

template <> 
bool ParseUIEventParam< float >( float *pOut, panorama::IUIPanel *pPanel, const char *pchEvent, const char **pchNextParam )
{
	// get param
	CUtlBuffer bufValue;
	if ( !ParseUIEventParamHelper( bufValue, pchEvent, pchNextParam ) )
		return false;

	*pOut = V_atof( (const char *)bufValue.Base() );
	return true;
}

CUtlString FormatEventArgsCore( const char **arrArgTypes, int nArgTypesCount, const char *pchArgNames )
{
	CUtlString str;

	CUtlVector< CUtlString > vecArgNames;
	V_SplitString( pchArgNames, ",", vecArgNames );
	for ( int i = 0; i < nArgTypesCount; ++i )
	{
		if ( i != 0 )
		{
			str.Append( ", " );
		}

		str.Append( arrArgTypes[ i ] );

		if ( i < vecArgNames.Count() )
		{
			CUtlString strArgName( vecArgNames[ i ] );
			V_StrTrim( strArgName.Access() );
			str.Append( " " );
			str.Append( strArgName );
		}
	}

	return str;
}

} // namespace panorama

//-----------------------------------------------------------------------------
// Purpose: Helper to set panel event arg (first art)
//-----------------------------------------------------------------------------
void panorama::SetPanelEventJSArg( const IUIPanel *pTarget, v8::Handle<v8::Value> *pValueOut )
{
#if !defined( SOURCE2_PANORAMA )
	PanoramaPanelTypeToV8Param( const_cast< IUIPanel *>( pTarget ), pValueOut );
#else
	*pValueOut = v8::String::NewFromUtf8( GetV8Isolate(), GetPanelID( pTarget ) ).ToLocalChecked();
#endif
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate the global map
//-----------------------------------------------------------------------------
void panorama::ValidateGlobalEvents( CValidator &validator )
{	
	ValidateObj( MapLocalRegisteredEvents() );
}
#endif

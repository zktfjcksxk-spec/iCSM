//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper for UI components - used for binding to both Scaleform ActionScript and 
//          Panorama Javascript.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "uicomponent_common.h"

#if defined ( USE_PANORAMA_BINDINGS )
static CPanoramaMarshallHelper g_PanoramaMarshallHelper;
CPanoramaMarshallHelper* GetPanoramaMarshallHelper( void )
{
	return &g_PanoramaMarshallHelper;
}

SFVALUEARRAY CPanoramaMarshallHelper::Params_GetArgs( SFPARAMS params )
{
	Assert( 0 ); // IMPLEMENT ME OR REFACTOR CALLING CODE
	return SFVALUEARRAY();
}

unsigned int CPanoramaMarshallHelper::Params_GetNumArgs( SFPARAMS params )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	return args.Length();
}

bool CPanoramaMarshallHelper::Params_ArgIs( SFPARAMS params, unsigned int index, _ValueType v )
{
	return ( Params_GetArgType( params, index ) == v );
}

SFVALUE CPanoramaMarshallHelper::Params_GetArg( SFPARAMS params, int index /*= 0 */ )
{
	Assert( 0 ); // IMPLEMENT ME OR REFACTOR CALLING CODE
	return NULL;
}

IUIMarshalHelper::_ValueType CPanoramaMarshallHelper::Params_GetArgType( SFPARAMS params, int index /*= 0 */ )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	if ( args[ index ]->IsBoolean() )
		return VT_Boolean;
	else if ( args[ index ]->IsNumber() )
		return VT_Number;
	else if ( args[ index ]->IsInt32() )
		return VT_Int;
	else if ( args[ index ]->IsUint32() )
		return VT_UInt;
	else if ( args[ index ]->IsString() )
		return VT_String;
	else if ( args[ index ]->IsUndefined() )
		return VT_Undefined;
	else if ( args[ index ]->IsNull() )
		return VT_Null;


	Assert( 0 ); 
	return VT_Undefined;
}

double CPanoramaMarshallHelper::Params_GetArgAsNumber( SFPARAMS params, int index /*= 0 */ )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	Assert( index >= 0 && index < args.Length() );
	// Assert( args[ index ]->IsNumber() ); // allow soft-coercion from JS values to numbers
	double result = args[index]->NumberValue( v8::Isolate::GetCurrent()->GetCurrentContext() ).FromMaybe( 0 );
	if ( !IsFinite( result ) )
	{
		AssertMsg( false, "non-number passed as argument to function requiring number" );
		result = 0.0f;
	}
	return result;
}

bool CPanoramaMarshallHelper::Params_GetArgAsBool( SFPARAMS params, int index /*= 0 */ )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	Assert( index >= 0 && index < args.Length() );
	Assert( args[ index ]->IsBoolean() );
	return args[ index ]->BooleanValue( v8::Isolate::GetCurrent() );
}

const char* CPanoramaMarshallHelper::Params_GetArgAsString( SFPARAMS params, int index /*= 0 */ )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	Assert( index >= 0 && index < args.Length() );
	m_vecStringStorage.EnsureCount( index + 1 );
	Assert( args[ index ]->IsString() );
	v8::String::Utf8Value str(v8::Isolate::GetCurrent(),  args[ index ] );
	m_vecStringStorage[ index ].Set( *str );
	return m_vecStringStorage[ index ];
}

const wchar_t* CPanoramaMarshallHelper::Params_GetArgAsStringW( SFPARAMS params, int index /*= 0 */ )
{
	Assert( 0 ); // IMPLEMENT ME OR REFACTOR CALLING CODE
	return NULL;
}

void CPanoramaMarshallHelper::Params_DebugSpew( SFPARAMS params )
{
	Assert( 0 );
}

void CPanoramaMarshallHelper::Params_SetResult( SFPARAMS params, SFVALUE value )
{
	Assert( 0 );
}

void CPanoramaMarshallHelper::Params_SetResult( SFPARAMS params, int value )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	args.GetReturnValue().Set( v8::Integer::New( args.GetIsolate(), value ) );
}

void CPanoramaMarshallHelper::Params_SetResult( SFPARAMS params, float value )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	args.GetReturnValue().Set( v8::Number::New( args.GetIsolate(), value ) );
}

void CPanoramaMarshallHelper::Params_SetResult( SFPARAMS params, bool value )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	args.GetReturnValue().Set( v8::Boolean::New( args.GetIsolate(), value ) );
}

void CPanoramaMarshallHelper::Params_SetResult( SFPARAMS params, const char* value, bool bMakeNewValue /*=false*/ )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
	args.GetReturnValue().Set( v8::String::NewFromUtf8( args.GetIsolate(), ( value ? value : "" ) ).ToLocalChecked() );
}

void CPanoramaMarshallHelper::Params_SetResult( SFPARAMS params, const wchar_t* value, bool bMakeNewValue /*=false*/ )
{
	const v8::FunctionCallbackInfo<v8::Value> &args = *reinterpret_cast< const v8::FunctionCallbackInfo<v8::Value> * >( params );
#if defined( LINUX ) || defined ( OSX )

	if ( !value )
	{
		value = L"";
	}

	char bufUtf8[ 4096 ];
	V_UnicodeToUTF8( value, bufUtf8, ARRAYSIZE( bufUtf8 ) );

	args.GetReturnValue().Set(  v8::String::NewFromUtf8( args.GetIsolate(), bufUtf8 ).ToLocalChecked() );
#else
	args.GetReturnValue().Set(  v8::String::NewFromTwoByte( args.GetIsolate(), (const uint16*)( value ? value : L"" ) ) );
#endif
}

SFVALUE CPanoramaMarshallHelper::Params_CreateNewObject( SFPARAMS params )
{
	Assert( 0 );
	return NULL;
}

SFVALUE CPanoramaMarshallHelper::Params_CreateNewString( SFPARAMS params, const char* value )
{
	Assert( 0 );
	return NULL;
}

SFVALUE CPanoramaMarshallHelper::Params_CreateNewString( SFPARAMS params, const wchar_t* value )
{
	Assert( 0 );
	return NULL;
}

SFVALUE CPanoramaMarshallHelper::Params_CreateNewArray( SFPARAMS params, int size /*= -1 */ )
{
	Assert( 0 );
	return NULL;
}

// UTF-8 version of GameUI_MakeStringSafe()
void Panorama_EscapeHTML( const char* szUtf8Src, char* szUtf8Dest, size_t nDestSize )
{
	Assert( nDestSize >= 2 );

	// Empty strings stay empty
	if ( !*szUtf8Src )
	{
		szUtf8Dest[0] = '\0';
		return;
	}

	// skip leading '#' to avoid loc-tag conflicts
	// $$$REI TODO: Remove skipping '#' since our usage of custom data should now be safe.
	while ( *szUtf8Src == '#' )
		++szUtf8Src;

	// If we didn't write any characters but the input wasn't empty, replace entire input with "?"
	if ( !*szUtf8Src )
	{
		szUtf8Dest[0] = '?';
		szUtf8Dest[1] = '\0';
		return;
	}

	// Clean up HTML-characters
	V_BasicHtmlEntityEncode( szUtf8Dest, nDestSize, szUtf8Src, V_strlen( szUtf8Src ) );

	// Fix-up invalid or truncated utf-8 characters
	Q_UnicodeRepair( szUtf8Dest );
}

#endif // USE_PANORAMA_BINDINGS

// Using this from webapi_response to reuse some code.
extern void EmitJSONString( CUtlBuffer &outputBuffer, const char *pchValue );

// Convert keyvalues to a json object.
bool Helper_RecursiveKeyValuesToJSONString( const KeyValues* pKV, CUtlBuffer &outBuffer )
{
	outBuffer.PutChar( '{' );
	for ( KeyValues *dat = pKV->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey() )
	{
		if ( !dat->GetName() )
		{
			Assert( 0 ); // Unhandled
			return false;
		}

		EmitJSONString( outBuffer, dat->GetName() );
		outBuffer.PutChar( ':' );

		if ( dat->GetFirstSubKey() )
		{
			Helper_RecursiveKeyValuesToJSONString( dat, outBuffer );
		}
		else 
		{
			switch ( dat->GetDataType() )
			{
				case KeyValues::TYPE_NONE:
				{
					Helper_RecursiveKeyValuesToJSONString( dat, outBuffer );
					break;
				}
				case KeyValues::TYPE_STRING:
				{
					if ( dat->GetString() )
						EmitJSONString( outBuffer, dat->GetString() );
					else
						outBuffer.Put( "null", 4 );

					break;
				}
				case KeyValues::TYPE_INT:
				{
					CNumStr numStr( dat->GetInt() );
					outBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					break;
				}

				case KeyValues::TYPE_UINT64:
				{
					CNumStr numStr( dat->GetUint64() );
					outBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					break;
				}

				case KeyValues::TYPE_FLOAT:
				{
					CNumStr numStr( dat->GetFloat() );
					outBuffer.Put( numStr.String(), Q_strlen( numStr.String() ) );
					break;
				}

				default:
				{
					Assert( 0 ); // Unhandled
					return false;
				}
			}
		}

		if ( dat->GetNextKey() )
		{
			outBuffer.PutChar( ',' );
		}
	}
	outBuffer.PutChar( '}' );
	return true;
}

void Helper_TrackedCachedGcValueAcrossRuns( ConVar &cv, int &nValue, bool bValueObtainedThisSession, RTime32 rtNow )
{
	if ( !rtNow )
		return;
	
	bool bUpdateCachedValue = false;
	RTime32 const rtCached = cv.GetInt();

	if ( !bValueObtainedThisSession )
	{
		if ( ( int( rtCached - 24 * 3600 ) < int( rtNow ) ) &&
			( int( rtNow - rtCached ) < 7 * 24 * 3600 ) )
		{
			nValue = rtCached & 0xFF; // Return a cached number
		}
		else
		{
			nValue = RandomInt( 4, 19 ); // Fake number of teammates to lure user
			bUpdateCachedValue = true;
		}
	}
	else
	{
		bUpdateCachedValue = true;
	}

	if ( bUpdateCachedValue )
	{
		rtNow &= ~0xFF; // vacate room for caching 0..255
		rtNow |= nValue & 0xFF; // store the count in there too
		if ( rtNow != rtCached )
			cv.SetValue( int( rtNow ) );
	}
}

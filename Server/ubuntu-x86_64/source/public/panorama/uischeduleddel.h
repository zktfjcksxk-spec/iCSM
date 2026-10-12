//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UISCHEDULEDDEL_H
#define UISCHEDULEDDEL_H
#pragma once

#include "tier1/utldelegate.h" 

#if !defined( SOURCE2_PANORAMA )
#define SCHEDULED_FUNC_HAS_NAME 1
#endif

namespace panorama
{

#ifdef SCHEDULED_FUNC_HAS_NAME
#define MAKE_SCHEDULED_FUNC( FuncName ) UtlMakeDelegate( this, &FuncName ), #FuncName
#else
#define MAKE_SCHEDULED_FUNC( FuncName ) UtlMakeDelegate( this, &FuncName )
#endif

// Panorama wrapper around CUtlSymbol which always creates symbol in panorama.dll module
class CUIScheduledDel
{
public:
	// constructor, destructor
#ifdef SCHEDULED_FUNC_HAS_NAME
	CUIScheduledDel( CUtlDelegate< void() > del, const char *pchName );
#else
	CUIScheduledDel( CUtlDelegate< void() > del );
#endif
	~CUIScheduledDel();

	void Schedule( double flSecondsToRunIn ); 

	void Cancel();

	bool BScheduled();

	double GetSecondsUntilScheduled() { return MAX( m_flNextScheduled - UIEngine()->GetCurrentFrameTime(), 0.0 ); }

#ifdef SCHEDULED_FUNC_HAS_NAME
	const char *PchName() const { return m_sName;  }
#endif

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
#ifdef SCHEDULED_FUNC_HAS_NAME
		ValidateObj( m_sName );
#endif
	}
#endif
private:

	double m_flNextScheduled;
	int m_iScheduledIndex;
#ifdef SCHEDULED_FUNC_HAS_NAME
	CUtlString m_sName;
#endif
	CUtlDelegate< void() > m_del;
};

} // namespace panorama

#endif // UISCHEDULEDDEL_H

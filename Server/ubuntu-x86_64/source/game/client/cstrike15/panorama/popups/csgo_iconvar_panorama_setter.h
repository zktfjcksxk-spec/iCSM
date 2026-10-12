//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

class CCSGO_iConvarPanoramaSetter
{
public:

	void RestoreDefault()
	{
		ConVarRef &ref = GetConVarRef();
		if ( ref.IsValid() )
		{
			ref.SetValue( ref.GetDefault() );
		}
	}

private:
	virtual ConVarRef& GetConVarRef() = 0;
};

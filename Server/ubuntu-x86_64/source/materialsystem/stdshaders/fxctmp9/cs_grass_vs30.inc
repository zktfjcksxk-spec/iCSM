// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $DOWATERFOG == 0 ) && ( $DOPIXELFOG && $HARDWAREFOGBLEND )
// ( $DOWATERFOG == 0 ) && ( $HARDWAREFOGBLEND == 0 ) && ( $DOPIXELFOG == 0 )
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class cs_grass_vs30_Static_Index
{
private:
	int m_nWORLDSPACETINTTEXTURE;
#ifdef _DEBUG
	bool m_bWORLDSPACETINTTEXTURE;
#endif
public:
	void SetWORLDSPACETINTTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWORLDSPACETINTTEXTURE = i;
#ifdef _DEBUG
		m_bWORLDSPACETINTTEXTURE = true;
#endif
	}
	void SetWORLDSPACETINTTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWORLDSPACETINTTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bWORLDSPACETINTTEXTURE = true;
#endif
	}
private:
	int m_nWORLDSPACETYPETEXTURE;
#ifdef _DEBUG
	bool m_bWORLDSPACETYPETEXTURE;
#endif
public:
	void SetWORLDSPACETYPETEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWORLDSPACETYPETEXTURE = i;
#ifdef _DEBUG
		m_bWORLDSPACETYPETEXTURE = true;
#endif
	}
	void SetWORLDSPACETYPETEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWORLDSPACETYPETEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bWORLDSPACETYPETEXTURE = true;
#endif
	}
private:
	int m_nWORLDSPACEBURNTEXTURE;
#ifdef _DEBUG
	bool m_bWORLDSPACEBURNTEXTURE;
#endif
public:
	void SetWORLDSPACEBURNTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWORLDSPACEBURNTEXTURE = i;
#ifdef _DEBUG
		m_bWORLDSPACEBURNTEXTURE = true;
#endif
	}
	void SetWORLDSPACEBURNTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWORLDSPACEBURNTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bWORLDSPACEBURNTEXTURE = true;
#endif
	}
private:
	int m_nWORLDSPACEZONETEXTURE;
#ifdef _DEBUG
	bool m_bWORLDSPACEZONETEXTURE;
#endif
public:
	void SetWORLDSPACEZONETEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWORLDSPACEZONETEXTURE = i;
#ifdef _DEBUG
		m_bWORLDSPACEZONETEXTURE = true;
#endif
	}
	void SetWORLDSPACEZONETEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWORLDSPACEZONETEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bWORLDSPACEZONETEXTURE = true;
#endif
	}
private:
	int m_nDOPIXELFOG;
#ifdef _DEBUG
	bool m_bDOPIXELFOG;
#endif
public:
	void SetDOPIXELFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOPIXELFOG = i;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
	void SetDOPIXELFOG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDOPIXELFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
private:
	int m_nHARDWAREFOGBLEND;
#ifdef _DEBUG
	bool m_bHARDWAREFOGBLEND;
#endif
public:
	void SetHARDWAREFOGBLEND( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nHARDWAREFOGBLEND = i;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif
	}
	void SetHARDWAREFOGBLEND( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nHARDWAREFOGBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif
	}
public:
	// CONSTRUCTOR
	cs_grass_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bWORLDSPACETINTTEXTURE = false;
#endif // _DEBUG
		m_nWORLDSPACETINTTEXTURE = 0;
#ifdef _DEBUG
		m_bWORLDSPACETYPETEXTURE = false;
#endif // _DEBUG
		m_nWORLDSPACETYPETEXTURE = 0;
#ifdef _DEBUG
		m_bWORLDSPACEBURNTEXTURE = false;
#endif // _DEBUG
		m_nWORLDSPACEBURNTEXTURE = 0;
#ifdef _DEBUG
		m_bWORLDSPACEZONETEXTURE = false;
#endif // _DEBUG
		m_nWORLDSPACEZONETEXTURE = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = g_pHardwareConfig->SupportsPixelShaders_2_b() ;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif // _DEBUG
		m_nHARDWAREFOGBLEND = 0 ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bWORLDSPACETINTTEXTURE && m_bWORLDSPACETYPETEXTURE && m_bWORLDSPACEBURNTEXTURE && m_bWORLDSPACEZONETEXTURE && m_bDOPIXELFOG && m_bHARDWAREFOGBLEND;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 28 * m_nWORLDSPACETINTTEXTURE ) + ( 56 * m_nWORLDSPACETYPETEXTURE ) + ( 112 * m_nWORLDSPACEBURNTEXTURE ) + ( 224 * m_nWORLDSPACEZONETEXTURE ) + ( 448 * m_nDOPIXELFOG ) + ( 896 * m_nHARDWAREFOGBLEND ) + 0;
	}
};
#define shaderStaticTest_cs_grass_vs30 vsh_forgot_to_set_static_WORLDSPACETINTTEXTURE + vsh_forgot_to_set_static_WORLDSPACETYPETEXTURE + vsh_forgot_to_set_static_WORLDSPACEBURNTEXTURE + vsh_forgot_to_set_static_WORLDSPACEZONETEXTURE + 0
class cs_grass_vs30_Dynamic_Index
{
private:
	int m_nDZONE;
#ifdef _DEBUG
	bool m_bDZONE;
#endif
public:
	void SetDZONE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDZONE = i;
#ifdef _DEBUG
		m_bDZONE = true;
#endif
	}
	void SetDZONE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDZONE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDZONE = true;
#endif
	}
private:
	int m_nNEARBYPLAYERS;
#ifdef _DEBUG
	bool m_bNEARBYPLAYERS;
#endif
public:
	void SetNEARBYPLAYERS( int i )
	{
		Assert( i >= 0 && i <= 6 );
		m_nNEARBYPLAYERS = i;
#ifdef _DEBUG
		m_bNEARBYPLAYERS = true;
#endif
	}
	void SetNEARBYPLAYERS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 6 );
		m_nNEARBYPLAYERS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNEARBYPLAYERS = true;
#endif
	}
private:
	int m_nDOWATERFOG;
#ifdef _DEBUG
	bool m_bDOWATERFOG;
#endif
public:
	void SetDOWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOWATERFOG = i;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif
	}
	void SetDOWATERFOG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDOWATERFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif
	}
public:
	// CONSTRUCTOR
	cs_grass_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bDZONE = false;
#endif // _DEBUG
		m_nDZONE = 0;
#ifdef _DEBUG
		m_bNEARBYPLAYERS = false;
#endif // _DEBUG
		m_nNEARBYPLAYERS = 0;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif // _DEBUG
		m_nDOWATERFOG = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bDZONE && m_bNEARBYPLAYERS && m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDZONE ) + ( 2 * m_nNEARBYPLAYERS ) + ( 14 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_cs_grass_vs30 vsh_forgot_to_set_dynamic_DZONE + vsh_forgot_to_set_dynamic_NEARBYPLAYERS + 0

static const ShaderComboInformation_t s_DynamicComboArray_cs_grass_vs30[3] = 
{
	{ "DZONE", 0, 1 },
	{ "NEARBYPLAYERS", 0, 6 },
	{ "DOWATERFOG", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_cs_grass_vs30[6] = 
{
	{ "WORLDSPACETINTTEXTURE", 0, 1 },
	{ "WORLDSPACETYPETEXTURE", 0, 1 },
	{ "WORLDSPACEBURNTEXTURE", 0, 1 },
	{ "WORLDSPACEZONETEXTURE", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
	{ "HARDWAREFOGBLEND", 0, 0 },
};
static const ShaderComboSemantics_t cs_grass_vs30_combos =
{
	"cs_grass_vs30", s_DynamicComboArray_cs_grass_vs30, 3, s_StaticComboArray_cs_grass_vs30, 6 
};

class ConstructMe_cs_grass_vs30
{
public:
	ConstructMe_cs_grass_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &cs_grass_vs30_combos );
	}
};

static ConstructMe_cs_grass_vs30 s_ConstructMe_cs_grass_vs30;

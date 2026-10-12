// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class panorama_ps30_Static_Index
{
public:
	// CONSTRUCTOR
	panorama_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderStaticTest_panorama_ps30 0
class panorama_ps30_Dynamic_Index
{
private:
	int m_nD_TEX2DFASTBLUR;
#ifdef _DEBUG
	bool m_bD_TEX2DFASTBLUR;
#endif
public:
	void SetD_TEX2DFASTBLUR( int i )
	{
		Assert( i >= 0 && i <= 8 );
		m_nD_TEX2DFASTBLUR = i;
#ifdef _DEBUG
		m_bD_TEX2DFASTBLUR = true;
#endif
	}
	void SetD_TEX2DFASTBLUR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 8 );
		m_nD_TEX2DFASTBLUR = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_TEX2DFASTBLUR = true;
#endif
	}
private:
	int m_nD_TEX2DBLUR;
#ifdef _DEBUG
	bool m_bD_TEX2DBLUR;
#endif
public:
	void SetD_TEX2DBLUR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_TEX2DBLUR = i;
#ifdef _DEBUG
		m_bD_TEX2DBLUR = true;
#endif
	}
	void SetD_TEX2DBLUR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_TEX2DBLUR = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_TEX2DBLUR = true;
#endif
	}
private:
	int m_nD_TEX2DPARTICLE;
#ifdef _DEBUG
	bool m_bD_TEX2DPARTICLE;
#endif
public:
	void SetD_TEX2DPARTICLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_TEX2DPARTICLE = i;
#ifdef _DEBUG
		m_bD_TEX2DPARTICLE = true;
#endif
	}
	void SetD_TEX2DPARTICLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_TEX2DPARTICLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_TEX2DPARTICLE = true;
#endif
	}
private:
	int m_nD_TEX2DDOWNSAMPLE;
#ifdef _DEBUG
	bool m_bD_TEX2DDOWNSAMPLE;
#endif
public:
	void SetD_TEX2DDOWNSAMPLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_TEX2DDOWNSAMPLE = i;
#ifdef _DEBUG
		m_bD_TEX2DDOWNSAMPLE = true;
#endif
	}
	void SetD_TEX2DDOWNSAMPLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_TEX2DDOWNSAMPLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_TEX2DDOWNSAMPLE = true;
#endif
	}
public:
	// CONSTRUCTOR
	panorama_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bD_TEX2DFASTBLUR = false;
#endif // _DEBUG
		m_nD_TEX2DFASTBLUR = 0;
#ifdef _DEBUG
		m_bD_TEX2DBLUR = false;
#endif // _DEBUG
		m_nD_TEX2DBLUR = 0;
#ifdef _DEBUG
		m_bD_TEX2DPARTICLE = false;
#endif // _DEBUG
		m_nD_TEX2DPARTICLE = 0;
#ifdef _DEBUG
		m_bD_TEX2DDOWNSAMPLE = false;
#endif // _DEBUG
		m_nD_TEX2DDOWNSAMPLE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bD_TEX2DFASTBLUR && m_bD_TEX2DBLUR && m_bD_TEX2DPARTICLE && m_bD_TEX2DDOWNSAMPLE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nD_TEX2DFASTBLUR ) + ( 9 * m_nD_TEX2DBLUR ) + ( 18 * m_nD_TEX2DPARTICLE ) + ( 36 * m_nD_TEX2DDOWNSAMPLE ) + 0;
	}
};
#define shaderDynamicTest_panorama_ps30 psh_forgot_to_set_dynamic_D_TEX2DFASTBLUR + psh_forgot_to_set_dynamic_D_TEX2DBLUR + psh_forgot_to_set_dynamic_D_TEX2DPARTICLE + psh_forgot_to_set_dynamic_D_TEX2DDOWNSAMPLE + 0

static const ShaderComboInformation_t s_DynamicComboArray_panorama_ps30[4] = 
{
	{ "D_TEX2DFASTBLUR", 0, 8 },
	{ "D_TEX2DBLUR", 0, 1 },
	{ "D_TEX2DPARTICLE", 0, 1 },
	{ "D_TEX2DDOWNSAMPLE", 0, 1 },
};
static const ShaderComboSemantics_t panorama_ps30_combos =
{
	"panorama_ps30", s_DynamicComboArray_panorama_ps30, 4, NULL, 0 
};

class ConstructMe_panorama_ps30
{
public:
	ConstructMe_panorama_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &panorama_ps30_combos );
	}
};

static ConstructMe_panorama_ps30 s_ConstructMe_panorama_ps30;

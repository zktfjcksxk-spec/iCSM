// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $D_USEOUTERCORNER == 1 ) && ( $D_USEINNERCORNER == 1 )
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class panoramafancy_ps30_Static_Index
{
public:
	// CONSTRUCTOR
	panoramafancy_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_panoramafancy_ps30 0
class panoramafancy_ps30_Dynamic_Index
{
private:
	int m_nD_TEXTURETYPE;
#ifdef _DEBUG
	bool m_bD_TEXTURETYPE;
#endif
public:
	void SetD_TEXTURETYPE( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nD_TEXTURETYPE = i;
#ifdef _DEBUG
		m_bD_TEXTURETYPE = true;
#endif
	}
	void SetD_TEXTURETYPE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 4 );
		m_nD_TEXTURETYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_TEXTURETYPE = true;
#endif
	}
private:
	int m_nD_PREMULTIPLY_ALPHA;
#ifdef _DEBUG
	bool m_bD_PREMULTIPLY_ALPHA;
#endif
public:
	void SetD_PREMULTIPLY_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_PREMULTIPLY_ALPHA = i;
#ifdef _DEBUG
		m_bD_PREMULTIPLY_ALPHA = true;
#endif
	}
	void SetD_PREMULTIPLY_ALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_PREMULTIPLY_ALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_PREMULTIPLY_ALPHA = true;
#endif
	}
private:
	int m_nD_USERADIALGRADIENT;
#ifdef _DEBUG
	bool m_bD_USERADIALGRADIENT;
#endif
public:
	void SetD_USERADIALGRADIENT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USERADIALGRADIENT = i;
#ifdef _DEBUG
		m_bD_USERADIALGRADIENT = true;
#endif
	}
	void SetD_USERADIALGRADIENT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_USERADIALGRADIENT = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_USERADIALGRADIENT = true;
#endif
	}
private:
	int m_nD_GRADIENT_TWOSTOP;
#ifdef _DEBUG
	bool m_bD_GRADIENT_TWOSTOP;
#endif
public:
	void SetD_GRADIENT_TWOSTOP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_GRADIENT_TWOSTOP = i;
#ifdef _DEBUG
		m_bD_GRADIENT_TWOSTOP = true;
#endif
	}
	void SetD_GRADIENT_TWOSTOP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_GRADIENT_TWOSTOP = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_GRADIENT_TWOSTOP = true;
#endif
	}
private:
	int m_nD_GRADIENT_COMPLEX;
#ifdef _DEBUG
	bool m_bD_GRADIENT_COMPLEX;
#endif
public:
	void SetD_GRADIENT_COMPLEX( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_GRADIENT_COMPLEX = i;
#ifdef _DEBUG
		m_bD_GRADIENT_COMPLEX = true;
#endif
	}
	void SetD_GRADIENT_COMPLEX( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_GRADIENT_COMPLEX = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_GRADIENT_COMPLEX = true;
#endif
	}
private:
	int m_nD_USEOUTERCORNER;
#ifdef _DEBUG
	bool m_bD_USEOUTERCORNER;
#endif
public:
	void SetD_USEOUTERCORNER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USEOUTERCORNER = i;
#ifdef _DEBUG
		m_bD_USEOUTERCORNER = true;
#endif
	}
	void SetD_USEOUTERCORNER( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_USEOUTERCORNER = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_USEOUTERCORNER = true;
#endif
	}
private:
	int m_nD_USEINNERCORNER;
#ifdef _DEBUG
	bool m_bD_USEINNERCORNER;
#endif
public:
	void SetD_USEINNERCORNER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USEINNERCORNER = i;
#ifdef _DEBUG
		m_bD_USEINNERCORNER = true;
#endif
	}
	void SetD_USEINNERCORNER( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_USEINNERCORNER = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_USEINNERCORNER = true;
#endif
	}
private:
	int m_nD_COLORCORRECTION;
#ifdef _DEBUG
	bool m_bD_COLORCORRECTION;
#endif
public:
	void SetD_COLORCORRECTION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_COLORCORRECTION = i;
#ifdef _DEBUG
		m_bD_COLORCORRECTION = true;
#endif
	}
	void SetD_COLORCORRECTION( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_COLORCORRECTION = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_COLORCORRECTION = true;
#endif
	}
private:
	int m_nD_USEOPACITYMASK;
#ifdef _DEBUG
	bool m_bD_USEOPACITYMASK;
#endif
public:
	void SetD_USEOPACITYMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USEOPACITYMASK = i;
#ifdef _DEBUG
		m_bD_USEOPACITYMASK = true;
#endif
	}
	void SetD_USEOPACITYMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_USEOPACITYMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_USEOPACITYMASK = true;
#endif
	}
private:
	int m_nD_USERADIALCLIP;
#ifdef _DEBUG
	bool m_bD_USERADIALCLIP;
#endif
public:
	void SetD_USERADIALCLIP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_USERADIALCLIP = i;
#ifdef _DEBUG
		m_bD_USERADIALCLIP = true;
#endif
	}
	void SetD_USERADIALCLIP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_USERADIALCLIP = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_USERADIALCLIP = true;
#endif
	}
public:
	// CONSTRUCTOR
	panoramafancy_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bD_TEXTURETYPE = false;
#endif // _DEBUG
		m_nD_TEXTURETYPE = 0;
#ifdef _DEBUG
		m_bD_PREMULTIPLY_ALPHA = false;
#endif // _DEBUG
		m_nD_PREMULTIPLY_ALPHA = 0;
#ifdef _DEBUG
		m_bD_USERADIALGRADIENT = false;
#endif // _DEBUG
		m_nD_USERADIALGRADIENT = 0;
#ifdef _DEBUG
		m_bD_GRADIENT_TWOSTOP = false;
#endif // _DEBUG
		m_nD_GRADIENT_TWOSTOP = 0;
#ifdef _DEBUG
		m_bD_GRADIENT_COMPLEX = false;
#endif // _DEBUG
		m_nD_GRADIENT_COMPLEX = 0;
#ifdef _DEBUG
		m_bD_USEOUTERCORNER = false;
#endif // _DEBUG
		m_nD_USEOUTERCORNER = 0;
#ifdef _DEBUG
		m_bD_USEINNERCORNER = false;
#endif // _DEBUG
		m_nD_USEINNERCORNER = 0;
#ifdef _DEBUG
		m_bD_COLORCORRECTION = false;
#endif // _DEBUG
		m_nD_COLORCORRECTION = 0;
#ifdef _DEBUG
		m_bD_USEOPACITYMASK = false;
#endif // _DEBUG
		m_nD_USEOPACITYMASK = 0;
#ifdef _DEBUG
		m_bD_USERADIALCLIP = false;
#endif // _DEBUG
		m_nD_USERADIALCLIP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bD_TEXTURETYPE && m_bD_PREMULTIPLY_ALPHA && m_bD_USERADIALGRADIENT && m_bD_GRADIENT_TWOSTOP && m_bD_GRADIENT_COMPLEX && m_bD_USEOUTERCORNER && m_bD_USEINNERCORNER && m_bD_COLORCORRECTION && m_bD_USEOPACITYMASK && m_bD_USERADIALCLIP;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nD_TEXTURETYPE ) + ( 5 * m_nD_PREMULTIPLY_ALPHA ) + ( 10 * m_nD_USERADIALGRADIENT ) + ( 20 * m_nD_GRADIENT_TWOSTOP ) + ( 40 * m_nD_GRADIENT_COMPLEX ) + ( 80 * m_nD_USEOUTERCORNER ) + ( 160 * m_nD_USEINNERCORNER ) + ( 320 * m_nD_COLORCORRECTION ) + ( 640 * m_nD_USEOPACITYMASK ) + ( 1280 * m_nD_USERADIALCLIP ) + 0;
	}
};
#define shaderDynamicTest_panoramafancy_ps30 psh_forgot_to_set_dynamic_D_TEXTURETYPE + psh_forgot_to_set_dynamic_D_PREMULTIPLY_ALPHA + psh_forgot_to_set_dynamic_D_USERADIALGRADIENT + psh_forgot_to_set_dynamic_D_GRADIENT_TWOSTOP + psh_forgot_to_set_dynamic_D_GRADIENT_COMPLEX + psh_forgot_to_set_dynamic_D_USEOUTERCORNER + psh_forgot_to_set_dynamic_D_USEINNERCORNER + psh_forgot_to_set_dynamic_D_COLORCORRECTION + psh_forgot_to_set_dynamic_D_USEOPACITYMASK + psh_forgot_to_set_dynamic_D_USERADIALCLIP + 0

static const ShaderComboInformation_t s_DynamicComboArray_panoramafancy_ps30[10] = 
{
	{ "D_TEXTURETYPE", 0, 4 },
	{ "D_PREMULTIPLY_ALPHA", 0, 1 },
	{ "D_USERADIALGRADIENT", 0, 1 },
	{ "D_GRADIENT_TWOSTOP", 0, 1 },
	{ "D_GRADIENT_COMPLEX", 0, 1 },
	{ "D_USEOUTERCORNER", 0, 1 },
	{ "D_USEINNERCORNER", 0, 1 },
	{ "D_COLORCORRECTION", 0, 1 },
	{ "D_USEOPACITYMASK", 0, 1 },
	{ "D_USERADIALCLIP", 0, 1 },
};
static const ShaderComboSemantics_t panoramafancy_ps30_combos =
{
	"panoramafancy_ps30", s_DynamicComboArray_panoramafancy_ps30, 10, NULL, 0 
};

class ConstructMe_panoramafancy_ps30
{
public:
	ConstructMe_panoramafancy_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &panoramafancy_ps30_combos );
	}
};

static ConstructMe_panoramafancy_ps30 s_ConstructMe_panoramafancy_ps30;

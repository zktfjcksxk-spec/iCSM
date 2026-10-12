//=============== Copyright, Valve LLC, All rights reserved. ==================
//
// Purpose: Basic shaders for drawing 2D quads with orthographic projection
//
// $NoKeywords: $
//=============================================================================

//
// Variables used by our shaders
//
cbuffer cbPerViewChange
{
    //viewport params
    float    g_viewportHeight;
    float    g_viewportWidth;
    float4x4 g_MatTransform;
    float g_Saturation;
    float g_HueShift;
    float g_Contrast;
    float g_Brightness;
    float g_OpacityMaskOneBase;
    float g_OpacityMaskOneOpacity;
    float g_OpacityMaskTwoBase;
};

// Normal textures and opacity mask textures for standard render path
Texture2D g_txDiffuse;
Texture2D g_txOpacityMask;
Texture2D g_txOpacityMaskTwo;

// YUV texture components for movie render path
Texture2D g_txY;
Texture2D g_txU;
Texture2D g_txV;

// Distortion map for VR barrel filter lens correction
Texture2D g_txFilter;

cbuffer cbForBlur
{

	
	// Set to (0.0, 1.0)*blurSize for vertical, (1.0, 0.0)*blurSize for horizontal.
	//
	// blurSize should usually be 1.0f / texture_pixel_width for a horizontal blur, 
	// or 1.0f / texture_pixel_height for a vertical blur.
	//
	// Additionally, pass 1 is vec*1, pass 2 is vec*2, etc...
	uniform float2 blurMultiplyVecPass1      = float2(0.0f, 1.0f);
	uniform float2 blurMultiplyVecPass2      = float2(0.0f, 1.0f);
	uniform float2 blurMultiplyVecPass3      = float2(0.0f, 1.0f);
	uniform float2 blurMultiplyVecPass4      = float2(0.0f, 1.0f);

	// Magic values we compute based on sigma (stddev). 
	uniform float3 incrementalGaussian;
};

cbuffer cbForVR
{
	float4x4 vr_invProj;
	float4x4 vr_rot;
	float4 vr_P0;
	float4 vr_kernel[2];
	float4 vr_coef; // x: 2.0 * theta, y: aspect / scale, z: aspect
	float4 vr_uvOffset;
};

//
// Sampler state to sample textures with
//
SamplerState samLinear
{
    Filter = ANISOTROPIC;
    AddressU = Clamp;
    AddressV = Clamp;
    MaxAnisotropy = 1;
};


//
// Sampler state to sample textures with
//
SamplerState samPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
    MaxAnisotropy = 1;
};


//
// Alpha blending state to set
//
BlendState SrcAlphaBlendingAdd
{
    BlendEnable[0] = TRUE;
    
    // Color channel blending, remember, we use pre-multiplied alpha!
    SrcBlend = ONE;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
        
    // Alpha channel blending
    SrcBlendAlpha = ONE;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;    

    // Write to all channels, rgba
    RenderTargetWriteMask[0] = 0x0F;
};


//
// Alpha blending state to set
//
BlendState SrcAlphaBlendingOff
{
    BlendEnable[0] = FALSE;
};


//
// Alpha blending state to set
//
BlendState SrcAlphaBlendingParticleSystem
{
    BlendEnable[0] = TRUE;
    
    // Color channel blending, remember, we use pre-multiplied alpha!
    SrcBlend = ONE;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
        
    // Alpha channel blending
    SrcBlendAlpha = ONE;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;    

    // Write to all channels, rgba
    RenderTargetWriteMask[0] = 0x0F;
};

//
// Depth stencil state to set
//
DepthStencilState DisableDepth
{
    DepthEnable = FALSE;
    DepthWriteMask = ZERO;
    StencilEnable = FALSE;
    StencilWriteMask = 0;
};


//
// Depth stencil state to set
//
DepthStencilState EnableDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    StencilEnable = FALSE;
    StencilWriteMask = 0;
};


//
// Vertex Shader Output struct
//
struct VS_OUTPUT
{
    float4 Position   : SV_POSITION; // vertex position 
    float4 Diffuse    : COLOR0;      // vertex diffuse color (note that COLOR0 is clamped from 0..1)
    float2 TextureUV  : TEXCOORD0;   // vertex texture coords 
    float2 MaskUV1	  : TEXCOORD1;
    float2 MaskUV2	  : TEXCOORD2;
};


//
// Vertex Shader Output struct
//
struct VS_OUTPUT_PARTICLE_SYSTEM
{
	float4 Position   : SV_POSITION; // vertex position 
    float4 Diffuse    : COLOR0;      // vertex diffuse color (note that COLOR0 is clamped from 0..1)
    float2 TextureUV  : TEXCOORD0;   // vertex texture coords 
	float  Sharpness  : COLOR1;
};

//
// Pixel Shader output struct
//
struct PS_OUTPUT
{
    float4 RGBAColor : SV_Target;  // Pixel color
};


//
// Vertex Shader
//
VS_OUTPUT VS( float4 vPos : POSITION,
			float4 vColor : COLOR,
			float2 vTex : TEXCOORD0,
			float2 vMask1 : TEXCOORD1,
			float2 vMask2 : TEXCOORD1 )
{
	VS_OUTPUT Output;
	Output.Position = vPos;

	Output.Position = mul( Output.Position, g_MatTransform );

	// Normalize position values
	Output.Position.x = (  Output.Position.x / ( g_viewportWidth/2.0 ) ) - 1;
	Output.Position.y = -(  Output.Position.y / ( g_viewportHeight/2.0 ) ) + 1;

	Output.Position.z = 0.0f;
	Output.Position.w =  Output.Position.w;
	
	Output.TextureUV = vTex;
	Output.MaskUV1 = vMask1;
	Output.MaskUV2 = vMask2;
	Output.Diffuse = vColor;
	
    return Output;
}


//
// Gaussian blur, applies either horizontal or vertical 9 tap blur
// control with "sigma" and "blurMultiplyVec" to get vertical vs horizontal and to
// control stddeviation of guassian distribution.
//
PS_OUTPUT Blur( VS_OUTPUT In ) 
{
	PS_OUTPUT Output;
	
	float4 avgValue = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float coefficientSum = 0.0f;

	float3 localIncrementalGaussian = incrementalGaussian;

	// Take the central sample first...
	avgValue +=  g_txDiffuse.Sample( samPoint, In.TextureUV) * localIncrementalGaussian.x;
	coefficientSum += localIncrementalGaussian.x;
	localIncrementalGaussian.xy *= localIncrementalGaussian.yz;

	// Constant iterations, for a 9x9 kernel (4.0), use 3.0 for 7x7, 2.0 for 5x5
	
	// START UNROLLED LOOP

	//
	// i == 1.0
	//
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV - blurMultiplyVecPass1 ) * localIncrementalGaussian.x;
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV  + blurMultiplyVecPass1 ) * localIncrementalGaussian.x;         
        
	coefficientSum += 2 * localIncrementalGaussian.x;
	localIncrementalGaussian.xy *= localIncrementalGaussian.yz;

	//
	// i == 2.0
	//
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV - blurMultiplyVecPass2 ) * localIncrementalGaussian.x;
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV  + blurMultiplyVecPass2 ) * localIncrementalGaussian.x;         
        
	coefficientSum += 2 * localIncrementalGaussian.x;
	localIncrementalGaussian.xy *= localIncrementalGaussian.yz;

	//
	// i == 3.0
	//
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV - blurMultiplyVecPass3 ) * localIncrementalGaussian.x;
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV  + blurMultiplyVecPass3 ) * localIncrementalGaussian.x;         
        
	coefficientSum += 2 * localIncrementalGaussian.x;
	localIncrementalGaussian.xy *= localIncrementalGaussian.yz;

	//
	// i == 4.0
	//
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV - blurMultiplyVecPass4 ) * localIncrementalGaussian.x;
	avgValue += g_txDiffuse.Sample( samPoint, In.TextureUV  + blurMultiplyVecPass4 ) * localIncrementalGaussian.x;         
        
	coefficientSum += 2 * localIncrementalGaussian.x;
	localIncrementalGaussian.xy *= localIncrementalGaussian.yz;

	// END UNROLLED LOOP

	Output.RGBAColor = avgValue / coefficientSum;

	clip( Output.RGBAColor.a < 0.00001 ? -1 : 1 );

    return Output;
}


//
// Pixel Shader
//
PS_OUTPUT YUV420( VS_OUTPUT In ) 
{
	PS_OUTPUT Output;
	
	float y = g_txY.Sample( samLinear, In.TextureUV ).r;
	float u = g_txU.Sample( samLinear, In.TextureUV ).r;
	float v = g_txV.Sample( samLinear, In.TextureUV ).r;
	
	y = 1.1643*(y-0.0625);
	u = u-0.5;
	v = v-0.5;
	
	Output.RGBAColor.r = y+1.5958*v;
	Output.RGBAColor.g = y-0.39173*u-0.81290*v;
	Output.RGBAColor.b = y+2.017*u;
	Output.RGBAColor.a = 1.0f;
	
	return Output;
}
float3 RgbToHsv( float3 vRGB )
{
	float3 vHSV = float3( 0.0, 0.0, 0.0 );
	float minVal = min( vRGB.r, min( vRGB.g, vRGB.b ) );
	float maxVal = max( vRGB.r, max( vRGB.g, vRGB.b ) );
	float delta = maxVal - minVal; // Delta vRGB value
	vHSV.z = maxVal;
	if ( delta != 0.0 ) // If gray, leave H & S at zero
	{
		vHSV.y = delta / maxVal;
		float3 delRGB;
		float3 maxVec = float3( maxVal, maxVal, maxVal );
		delRGB = ( ( ( maxVec - vRGB ) / 6.0 ) + ( delta / 2.0 ) ) / delta;
		if ( vRGB.x == maxVal )
		{
			vHSV.x = delRGB.z - delRGB.y;
		}
		else if ( vRGB.y == maxVal )
		{
			vHSV.x = ( 1.0 / 3.0 ) + delRGB.x - delRGB.z;
		}
		else if ( vRGB.z == maxVal )
		{
			vHSV.x = ( 2.0 / 3.0 ) + delRGB.y - delRGB.x;
		}

		vHSV.x = frac( vHSV.x );
	}
	return ( vHSV );
}

float3 HsvToRgb( float3 vHSV )
{
	float3 vRGB = vHSV.zzz;
	if ( vHSV.y != 0 )
	{
		float var_h = vHSV.x * 6;
		float var_i = floor( var_h ); // Or ... var_i = floor( var_h )
		float var_1 = vHSV.z * ( 1.0 - vHSV.y );
		float var_2 = vHSV.z * ( 1.0 - vHSV.y * ( var_h - var_i ) );
		float var_3 = vHSV.z * ( 1.0 - vHSV.y * ( 1 - ( var_h - var_i ) ) );

		if ( var_i == 0 )
		{
			vRGB = float3( vHSV.z, var_3, var_1 );
		}
		else if ( var_i == 1 )
		{
			vRGB = float3( var_2, vHSV.z, var_1 );
		}
		else if ( var_i == 2 )
		{
			vRGB = float3( var_1, vHSV.z, var_3 );
		}
		else if ( var_i == 3 )
		{
			vRGB = float3( var_1, var_2, vHSV.z );
		}
		else if ( var_i == 4 )
		{
			vRGB = float3( var_3, var_1, vHSV.z );
		}
		else
		{
			vRGB = float3( vHSV.z, var_1, var_2 );
		}
	}
	return ( vRGB );
}


//
// Pixel Shader
//
PS_OUTPUT RenderCompositionPreMultiplied( VS_OUTPUT In ) 
{
	PS_OUTPUT Output;
	Output.RGBAColor = g_txDiffuse.Sample( samLinear, In.TextureUV );
	
	const float a = Output.RGBAColor.a;

	clip( a < 0.00001 ? -1 : 1 );

	// Desaturate colors if needed
	float3 vHSV = RgbToHsv( Output.RGBAColor.rgb );
	vHSV.r = frac( vHSV.r + g_HueShift );
	vHSV.g *= g_Saturation;
	vHSV.b *= g_Brightness;
	if ( ( Output.RGBAColor.a > 0.0 ) || ( g_Contrast > 1 ) )
		vHSV.b = lerp( 0.5, vHSV.b, g_Contrast );
	Output.RGBAColor.rgb = HsvToRgb( vHSV );

	// We use In.Diffuse as a color wash that applies last, after any earlier desaturation, which was after texture sampling
	Output.RGBAColor = Output.RGBAColor * In.Diffuse;

	// Apply opacity mask next
	float mask = abs( g_OpacityMaskOneBase - ( g_txOpacityMask.Sample( samLinear, In.MaskUV1 ).a ) );
	Output.RGBAColor = (Output.RGBAColor * (1.0 - g_OpacityMaskOneOpacity)) + (Output.RGBAColor * mask * g_OpacityMaskOneOpacity);

	// Apply second opacity mask, which is general for clipping/rounded corners
	Output.RGBAColor = Output.RGBAColor * abs( g_OpacityMaskTwoBase - g_txOpacityMaskTwo.Sample( samLinear, In.MaskUV2 ).a );

	return Output;
}


//
// Pixel Shader
//
PS_OUTPUT RenderQuadNotPreMultiplied( VS_OUTPUT In ) 
{
	PS_OUTPUT Output;
	Output.RGBAColor = g_txDiffuse.Sample( samLinear, In.TextureUV );
	Output.RGBAColor = Output.RGBAColor * In.Diffuse;
	Output.RGBAColor.r = Output.RGBAColor.r * Output.RGBAColor.a;
	Output.RGBAColor.g = Output.RGBAColor.g * Output.RGBAColor.a;
	Output.RGBAColor.b = Output.RGBAColor.b * Output.RGBAColor.a;
	
	return Output;
}


//
// Pixel Shader
//
PS_OUTPUT RenderQuadAlphaChannel( VS_OUTPUT In ) 
{
	PS_OUTPUT Output;
	Output.RGBAColor = g_txDiffuse.Sample( samLinear, In.TextureUV );
	Output.RGBAColor.a = Output.RGBAColor.a * In.Diffuse.a;
	Output.RGBAColor.rgb = Output.RGBAColor.aaa;
	
	return Output;
}


//
// Pixel Shader
//
PS_OUTPUT RenderQuadPreMultiplied( VS_OUTPUT In ) 
{
	PS_OUTPUT Output;
	Output.RGBAColor = g_txDiffuse.Sample( samLinear, In.TextureUV );
	Output.RGBAColor = Output.RGBAColor * In.Diffuse;
	return Output;
}

//
// Vertex Shader
//
VS_OUTPUT_PARTICLE_SYSTEM VSParticleSystem( float4 vPos : POSITION,
			float4 vColor : COLOR,
			float2 vTex : TEXCOORD0,
			float2 vMask1 : TEXCOORD1,
			float2 vMask2 : TEXCOORD1 )
{
	VS_OUTPUT_PARTICLE_SYSTEM Output;
	Output.Position = vPos;

	// Normalize position values
	Output.Position.x = (  Output.Position.x / ( g_viewportWidth/2.0 ) ) - 1;
	Output.Position.y = -(  Output.Position.y / ( g_viewportHeight/2.0 ) ) + 1;

	Output.Position.z = 0.0f;
	Output.Position.w =  Output.Position.w;
	
	Output.TextureUV = vTex;
	Output.Sharpness = vMask1.x;
	Output.Diffuse = vColor;
	
    return Output;
}



//
// Pixel Shader
//
float2 circleCenter = { 0.5, 0.5 }; 
PS_OUTPUT ParticleSystem( VS_OUTPUT_PARTICLE_SYSTEM In ) 
{
	PS_OUTPUT Output;
	Output.RGBAColor = In.Diffuse;
	
	In.TextureUV = In.TextureUV - 0.5;

	float radius = sqrt( dot( In.TextureUV, In.TextureUV ) );

	float flSharpRadius = clamp( In.Sharpness, 0.0, 0.98 ) / 2.0; 
	float alpha = 1.0; 
	if ( radius < flSharpRadius )
	{
		alpha = 1.0;
	}
	else
	{
		alpha = saturate( 1.0 - ( (radius - flSharpRadius) / (0.5 - flSharpRadius ) ) );
	}

	Output.RGBAColor.r = Output.RGBAColor.r * Output.RGBAColor.a * alpha;
	Output.RGBAColor.g = Output.RGBAColor.g * Output.RGBAColor.a * alpha;
	Output.RGBAColor.b = Output.RGBAColor.b * Output.RGBAColor.a * alpha;
	Output.RGBAColor.a = Output.RGBAColor.a * alpha;
	
	return Output;
}


//
// VR Vertex Shader
//

struct Fullscreen_Quad_Vert {
	float4 position : SV_Position;
	float2 texcoord: TexCoord;
};

Fullscreen_Quad_Vert vs_fullscreen_quad(uint VertexID: SV_VertexID) {
	Fullscreen_Quad_Vert Out;
	Out.texcoord = float2( (VertexID << 1) & 2, VertexID & 2 );
	Out.position = float4( Out.texcoord * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 0.0f, 1.0f );
	return Out;
}


//
// VR Pixel Shader
//

float c_rg_to_rb_ratio = 0.522;

struct RGB_SAMPLE_COORDS {
	float2 xy_red;
	float2 xy_green;
	float2 xy_blue;
};

RGB_SAMPLE_COORDS undistort_coords_from_texture(float2 norm01_coord)
{
	RGB_SAMPLE_COORDS samp;

	float4 distort_samp = g_txFilter.Sample(samLinear, norm01_coord);

	samp.xy_red = float2(distort_samp.x, distort_samp.y);
	samp.xy_blue = float2(distort_samp.z, distort_samp.w);
	samp.xy_green = samp.xy_red + c_rg_to_rb_ratio * ( samp.xy_blue - samp.xy_red );

	return samp;
}

float4 background(float3 ray)
{
	return float4(0, 0, 0, 0); // could do something cool here
}

float4 raycast(float2 uv)
{
	float4 ndc = float4(2.0f * uv.x - 1.0f, 1.0f - 2.0f * uv.y, 0, 1);

	ndc = mul(vr_invProj, ndc); // generate a ray from normalized device coordinates
	ndc /= ndc.w;
	ndc.w = 0.0f;
	float4 V = normalize(mul(vr_rot, ndc));

	float a = dot(V.xz, V.xz);
	float b = 2.0f * dot(V.xz, vr_P0.xz);
	float c = dot(vr_P0.xz, vr_P0.xz) - vr_P0.w; // r^2 in P0.w

	float det = b * b - 4.0f * a * c;
	if (det <= 0.0f)
		return background(V.xyz);

	float t = (sqrt(det) - b) / (2.0f * a);
	if (t <= 0.0f)
		return background(V.xyz);

	float3 P = vr_P0.xyz + V.xyz * t;
	P.x = atan2(P.x, -P.z) / vr_coef.x;
	P.y *= vr_coef.y;

	uv.x = P.x + 0.5f;
	uv.y = 0.5f - P.y;

	uv += vr_uvOffset.xy;
	uv *= vr_uvOffset.zw;
/*
	// remapping for side-by-side stereo videos
	if (uv.x > vr_uvFrom.x && uv.y > vr_uvFrom.y)
	{
		float2 delta = uv - vr_uvFrom.xy;
		if (delta.x < vr_uvFrom.z && delta.y < vr_uvFrom.w)
		{
			uv = vr_uvTo.xy + delta * vr_uvTo.zw / vr_uvFrom.zw;
		}
	}
*/
	// fade edges
	float2 threshold = float2(0.01f, 0.01f * vr_coef.z);
	float2 edge = saturate((abs(P.xy) + threshold - 0.5f) / threshold);
	float bg = dot(edge,edge);
	return lerp(g_txDiffuse.Sample(samLinear, uv), background(V.xyz), bg);
}

float4 sample_chromatic(float2 uv)
{
	RGB_SAMPLE_COORDS samp = undistort_coords_from_texture(uv);

	float4 redsamp = raycast(samp.xy_red);
	float4 greensamp = raycast(samp.xy_green);
	float4 bluesamp = raycast(samp.xy_blue);

	float4 color;
	color.r = redsamp.r;
	color.g = greensamp.g;
	color.b = bluesamp.b;
	color.a = greensamp.a;

	float threshold = saturate( dot((samp.xy_green < float2(0.01,0.01)), float2(1,1)) + dot((samp.xy_green > float2(0.99,0.99)), float2(1,1)) );
	return lerp(color, float4(0,0,0,color.a), threshold); // black edges to avoid leaving a gap since game masking may differ slightly
}

PS_OUTPUT PSAA(Fullscreen_Quad_Vert In)
{
	float4 c1 = sample_chromatic(In.texcoord.xy + vr_kernel[0].xy);
	float4 c2 = sample_chromatic(In.texcoord.xy + vr_kernel[0].zw);
	float4 c3 = sample_chromatic(In.texcoord.xy + vr_kernel[1].xy);
	float4 c4 = sample_chromatic(In.texcoord.xy + vr_kernel[1].zw);

	PS_OUTPUT o;
	o.RGBAColor = (c1 + c2 + c3 + c4) / 4.0f;
	return o;
}



//
// Technique
//
technique10 RenderQuadPreMultipled
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0_level_9_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_0, RenderQuadPreMultiplied() ) );
        
        SetBlendState( SrcAlphaBlendingAdd, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}


//
// Technique
//
technique10 RenderQuad
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0_level_9_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_0, RenderQuadNotPreMultiplied() ) );
        
        SetBlendState( SrcAlphaBlendingAdd, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}


//
// Technique
//
technique10 RenderQuadAlphaOnly
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0_level_9_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_0, RenderQuadAlphaChannel() ) );
        
        SetBlendState( SrcAlphaBlendingAdd, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}



//
// Technique
//
technique10 RenderComposition
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0_level_9_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, RenderCompositionPreMultiplied() ) );
        
        SetBlendState( SrcAlphaBlendingAdd, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}


//
// Technique
//
technique10 RenderBlur
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0_level_9_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, Blur() ) );
        
        SetBlendState( SrcAlphaBlendingOff, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}


//
// Technique
//
technique10 RenderYUV420
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0_level_9_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_0, YUV420() ) );
        
        SetBlendState( SrcAlphaBlendingOff, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}


//
// Technique
//
technique10 RenderParticleSystem
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0_level_9_0, VSParticleSystem() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_0, ParticleSystem() ) );
        
        SetBlendState( SrcAlphaBlendingParticleSystem, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}


//
// Technique
//
technique10 RenderVR
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, vs_fullscreen_quad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PSAA() ) );
        
        SetBlendState( SrcAlphaBlendingOff, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }
}


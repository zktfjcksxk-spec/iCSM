#ifndef INCLUDED_IRENDERHARDWARECONFIG_H
#define INCLUDED_IRENDERHARDWARECONFIG_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#define g_pRenderHardwareConfig g_pRenderHardwareConfig2			// Don't collide with S1 iface
extern IRenderHardwareConfig* g_pRenderHardwareConfig;

class IRenderHardwareConfig
{
public:
	virtual int	 GetFrameBufferColorDepth() const = 0;
	virtual int  GetSamplerCount() const = 0;
	virtual int  MaximumAnisotropicLevel() const = 0;	// 0 means no anisotropic filtering
	virtual int  MaxTextureWidth() const = 0;
	virtual int  MaxTextureHeight() const = 0;
	virtual uint64 TextureMemorySize() const = 0;
	virtual bool SupportsMipmappedCubemaps() const = 0;
	virtual bool SupportsNPO2Textures() const = 0;

	float GetPixelCenter() const
	{
		return 0.0f;// m_Caps.m_flPixelCenter;
	}

};

#endif // INCLUDED_IRENDERHARDWARECONFIG_H
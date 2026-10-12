#pragma once
namespace SourceMetal {
bool lightmapShadowLock(const Image &image, unsigned mip, unsigned face);
void invalidateLightmapShadow(Texture &texture, bool renderTarget = false);
void configureLightmapShadow(const std::shared_ptr<Texture> &texture, DWORD usage, const char *label);
bool lockLightmapShadow(Texture &texture, Texture::Lock &lock, D3DLOCKED_BOX &out);
void unlockLightmapShadow(Texture &texture, Texture::Lock &lock);
void rememberLightmapWrite(Texture &texture, const Texture::Lock &lock);
extern "C" NSDictionary *SourceMetalCombatSnapshot();
}

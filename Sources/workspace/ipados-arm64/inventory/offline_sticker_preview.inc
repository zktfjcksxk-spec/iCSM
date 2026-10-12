// Use the original, bundled gun/skin artwork. Sticker image compositing is
// disabled: no render jobs, readbacks, cache files or per-frame polling.
namespace ICSMStickerPreview {
class Cache {
public:
    CUtlString Image(int definition,int paint) const {
        return CFmtStr("file://{images}/offline_skins/%d_%d.png",definition,paint).Get();
    }
};
static Cache cache;
}

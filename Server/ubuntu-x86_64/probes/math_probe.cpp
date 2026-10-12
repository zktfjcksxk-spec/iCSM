#include <cmath>
#include <cfenv>
#include <climits>
#include <limits>
#include "tier0/icommandline.h"
#include "tier0/threadtools.h"
#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "tier1/keyvalues.h"
#include "tier1/strtools.h"
#include "vstdlib/vstrtools.h"
#include "probe_checks.h"

static bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    DeclareCurrentThreadIsMainThread();
    CommandLine()->CreateCmdLine("math_probe -allowdebug");
    MathLib_Init();
    Check(s_bMathlibInitialized, "actual MathLib_Init");
    Vector vec(3, 4, 12), zero(0, 0, 0);
    float length = VectorNormalize(vec);
    Check(Near(length, 13) && Near(vec.x, 3.0f / 13) && Near(vec.Length(), 1), "normalization returns original length and unit vector");
    Check(VectorNormalize(zero) == 0 && zero.LengthSqr() == 0, "zero vector normalization");
    float sinValue, cosValue;
    SinCos(0.7f, &sinValue, &cosValue);
    Check(Near(sinValue, std::sin(0.7f)) && Near(cosValue, std::cos(0.7f)), "portable SinCos");

    Vector input[4] = {Vector(1, 2, 3), Vector(-4, 0, 2), Vector(0, 0, 0), Vector(5, -6, 7)};
    Vector output[4];
    FourVectors vectors(input[0], input[1], input[2], input[3]);
    matrix3x4_t transform(0, -1, 0, 10, 1, 0, 0, 20, 0, 0, 1, 30);
    vectors.TransformBy(transform);
    vectors.StoreUnalignedVector3SIMD(&output[0], &output[1], &output[2], &output[3]);
    bool matched = true;
    for (int i = 0; i < 4; ++i)
        matched = matched && Near(output[i].x, 10 - input[i].y) && Near(output[i].y, 20 + input[i].x) && Near(output[i].z, 30 + input[i].z);
    Check(matched, "FourVectors SIMD matrix rotation/translation, four independent lanes");
    Check(alignof(FourVectors) >= 16 && alignof(matrix3x4a_t) >= 16, "SIMD types retain 16-byte alignment");
    FourVectors original(input[0], input[1], input[2], input[3]);
    fltx4 dots = original * Vector(2, -3, 4);
    for (int i = 0; i < 4; ++i)
        matched = matched && Near(SubFloat(dots, i), 2 * input[i].x - 3 * input[i].y + 4 * input[i].z);
    Check(matched, "FourVectors SIMD dot products");

    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const int positive[] = {2, 2, 3, 2}, negative[] = {-2, -3, -2, -2};
    for (int i = 0; i < 4; ++i) {
        std::fesetround(modes[i]);
        volatile float p = 2.5f, n = -2.5f;
        Check(RoundFloatToInt(p) == positive[i] && RoundFloatToInt(n) == negative[i] &&
              RoundFloatToByte(p) == positive[i] && RoundFloatToUnsignedLong(p) == static_cast<unsigned long>(positive[i]),
              "rounding follows current floating-point mode");
    }
    std::fesetround(FE_TONEAREST);
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    float bitPair[2]={1.f,42.f}; FloatBits(bitPair[0])=0xbf800000u;
    Check(bitPair[0]==-1.f && bitPair[1]==42.f && FloatBits(bitPair[0])==0xbf800000u &&
          BitsToFloat(0x3f800000u)==1.f && IsFinite(bitPair[0]) && !IsFinite(nan) && !IsFinite(inf),
          "LP64 float bit helpers access exactly 32 bits, preserve neighbours and classify IEEE specials");
    Check(RoundFloatToInt(nan) == INT_MIN && RoundFloatToInt(inf) == INT_MIN && RoundFloatToInt(2147483648.0f) == INT_MIN &&
          RoundFloatToInt(-2147483648.0f) == INT_MIN && RoundFloatToInt(2147483520.0f) == 2147483520,
          "int32 rounding preserves x87 invalid result and boundary values");
    intx4 integers;
    ConvertStoreAsIntsSIMD(&integers, _mm_setr_ps(2.9f, -2.9f, nan, inf));
    Check(integers[0] == 2 && integers[1] == -2 && integers[2] == INT_MIN && integers[3] == INT_MIN,
          "SIMD conversion truncates and preserves invalid integer result");
    fltx4 min = MinSIMD(_mm_setr_ps(nan, 3, 0.0f, -0.0f), _mm_setr_ps(7, nan, -0.0f, 0.0f));
    fltx4 max = MaxSIMD(_mm_setr_ps(nan, 3, 0.0f, -0.0f), _mm_setr_ps(7, nan, -0.0f, 0.0f));
    Check(SubFloat(min, 0) == 7 && std::isnan(SubFloat(min, 1)) && std::signbit(SubFloat(min, 2)) && !std::signbit(SubFloat(min, 3)) &&
          SubFloat(max, 0) == 7 && std::isnan(SubFloat(max, 1)) && std::signbit(SubFloat(max, 2)) && !std::signbit(SubFloat(max, 3)),
          "SIMD min/max retain SSE second-operand NaN/signed-zero semantics");
    fltx4 squareRoots = SqrtSIMD(_mm_setr_ps(0, -0.0f, inf, -1));
    Check(SubFloat(squareRoots, 0) == 0 && std::signbit(SubFloat(squareRoots, 1)) && std::isinf(SubFloat(squareRoots, 2)) && std::isnan(SubFloat(squareRoots, 3)),
          "SIMD sqrt zero, signed zero, infinity and negative input");

    const char *json = "{/*comment*/\"title\":\"中文\",\"number\":2.5,\"enabled\":true,\"list\":[1,2]}";
    CUtlBuffer buffer(0, 0, CUtlBuffer::TEXT_BUFFER);
    buffer.Put(json, std::strlen(json));
    KeyValues *kv = KeyValues::FromJSON(buffer);
    Check(kv && std::strcmp(kv->GetString("title"), "中文") == 0 && kv->GetFloat("number") == 2.5f && kv->GetInt("enabled") == 1 && kv->FindKey("list"),
          "real JSON parser handles UTF-8, comments, floats, booleans and arrays");
    if (kv) kv->deleteThis();
    CUtlBuffer wrapped(0, 0, CUtlBuffer::TEXT_BUFFER);
    wrapped.PutString("\"title\":\"中文\"");
    kv = KeyValues::FromJSON(wrapped);
    Check(kv && std::strcmp(kv->GetName(), "title") == 0 && std::strcmp(kv->GetString(), "中文") == 0, "JSON wrapped single value");
    if (kv) kv->deleteThis();
    CUtlBuffer invalid(0, 0, CUtlBuffer::TEXT_BUFFER);
    invalid.PutString("{\"a\":}");
    kv = KeyValues::FromJSON(invalid);
    Check(!kv, "invalid JSON is rejected");
    if (kv) kv->deleteThis();
    wchar_t wide[32]; char utf8[128];
    V_UTF8ToUnicode("中文 / arm64", wide, sizeof(wide));
    V_UnicodeToUTF8(wide, utf8, sizeof(utf8));
    Check(std::strcmp(utf8, "中文 / arm64") == 0, "tier1 UTF-8/wchar_t round trip");
    V_UTF8ToUnicode("中文 / arm64", wide, static_cast<int>(sizeof(wide)));
    V_UnicodeToUTF8(wide, utf8, static_cast<int>(sizeof(utf8)));
    Check(std::strcmp(utf8, "中文 / arm64") == 0, "vstdlib Apple iconv UTF-8/wchar_t round trip");
    // The source and destination deliberately have different byte lengths.
    // A destination-sized input count would read past this exact UCS-2 string.
    const ucs2 ucs[] = {0x4e2d, 0x6587, 0};
    wchar_t fromUcs[16] = {}; char fromUcsUtf8[64] = {};
    V_UCS2ToUnicode(ucs, fromUcs, sizeof(fromUcs));
    V_UCS2ToUTF8(ucs, fromUcsUtf8, sizeof(fromUcsUtf8));
    Check(fromUcs[0] == 0x4e2d && fromUcs[1] == 0x6587 && fromUcs[2] == 0 && std::strcmp(fromUcsUtf8, "中文") == 0,
          "tier1 iconv UCS-2 reads only the null-terminated input");
    V_UCS2ToUnicode(ucs, fromUcs, static_cast<int>(sizeof(fromUcs)));
    V_UCS2ToUTF8(ucs, fromUcsUtf8, static_cast<int>(sizeof(fromUcsUtf8)));
    Check(fromUcs[0] == 0x4e2d && fromUcs[1] == 0x6587 && fromUcs[2] == 0 && std::strcmp(fromUcsUtf8, "中文") == 0,
          "vstdlib iconv UCS-2 reads only the null-terminated input");
    CheckNativeImages();
    std::printf("RESULT %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}

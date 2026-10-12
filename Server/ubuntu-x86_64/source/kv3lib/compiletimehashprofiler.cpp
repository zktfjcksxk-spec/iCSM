

#include "kv3lib/keyvalues3.h"

// 0 = disabled, 1 = runtime, 2 = compiletime
#define PROFILE_HASH 0

#if PROFILE_HASH

#if PROFILE_HASH == 1
// Force hash to generate function call / evaluate at runtime
#define H(s) CKV3MemberName(s)
#else
// Use implicit conversion from string literal which uses UtlStringToken inlining machinery to calcuate hashes at compile-time
#define H(s) s
#endif

#define HASHSTRING(n,s)								\
	int Profile_Hash ## n ( KeyValues3* pKV )		\
	{												\
		return pKV->GetMemberInt( H(s), 0 );		\
	}


#include "compiletimehashprofiler_strings.inc"

#endif // PROFILE_HASH

/* Results:

PROFILE_HASH = 1 (runtime)
(Release generated assembly example -- hashes at runtime)
Build time ~ 2sec

?Profile_Hash1@@YAHPAVKeyValues3@@@Z (int __cdecl Profile_Hash1(class KeyValues3 *)):
00000000: 55                 push        ebp
00000001: 8B EC              mov         ebp,esp
00000003: 56                 push        esi
00000004: 6A 00              push        0
00000006: 83 EC 08           sub         esp,8
00000009: 8B F4              mov         esi,esp
0000000B: 68 26 59 41 31     push        31415926h
00000010: 68 00 00 00 00     push        offset ??_C@_0BN@DCIHAEJP@7tvK1fEdg4EKNbmkKrEJAXBiqCdZ?$AA@
00000015: C7 06 00 00 00 00  mov         dword ptr [esi],0
0000001B: E8 00 00 00 00     call        ?MurmurHash2LowerCase@@YAIPBDI@Z
00000020: 8B 4D 08           mov         ecx,dword ptr [ebp+8]
00000023: 83 C4 08           add         esp,8
00000026: 89 06              mov         dword ptr [esi],eax
00000028: C7 46 04 00 00 00  mov         dword ptr [esi+4],offset ??_C@_0BN@DCIHAEJP@7tvK1fEdg4EKNbmkKrEJAXBiqCdZ?$AA@
00
0000002F: E8 00 00 00 00     call        ?GetMemberInt@KeyValues3@@QBEHVCKV3MemberName@@H@Z
00000034: 5E                 pop         esi
00000035: 5D                 pop         ebp
00000036: C3                 ret

PROFILE_HASH = 2 (compile time)
(Debug generated assembly example -- hashes at runtime)
Build time ~ 1 sec

?Profile_Hash1@@YAHPAVKeyValues3@@@Z (int __cdecl Profile_Hash1(class KeyValues3 *)):
00000000: 55                 push        ebp
00000001: 8B EC              mov         ebp,esp
00000003: 83 EC 44           sub         esp,44h
00000006: 53                 push        ebx
00000007: 56                 push        esi
00000008: 57                 push        edi
00000009: 6A 00              push        0
0000000B: 83 EC 08           sub         esp,8
0000000E: 8B CC              mov         ecx,esp
00000010: 68 00 00 00 00     push        offset ??_C@_0BN@DCIHAEJP@7tvK1fEdg4EKNbmkKrEJAXBiqCdZ?$AA@
00000015: E8 00 00 00 00     call        ??$?0$0BN@@CKV3MemberName@@QAE@AAY0BN@$$CBD@Z
0000001A: 8B 4D 08           mov         ecx,dword ptr [ebp+8]
0000001D: E8 00 00 00 00     call        ?GetMemberInt@KeyValues3@@QBEHVCKV3MemberName@@H@Z
00000022: 5F                 pop         edi
00000023: 5E                 pop         esi
00000024: 5B                 pop         ebx
00000025: 8B E5              mov         esp,ebp
00000027: 5D                 pop         ebp
00000028: C3                 ret

(Release generated assembly example)
?Profile_Hash1@@YAHPAVKeyValues3@@@Z (int __cdecl Profile_Hash1(class KeyValues3 *)):
Build time ~ 8 sec (~1ms / hash)

00000000: 55                 push        ebp
00000001: 8B EC              mov         ebp,esp
00000003: 8B 4D 08           mov         ecx,dword ptr [ebp+8]
00000006: 6A 00              push        0
00000008: 83 EC 08           sub         esp,8
0000000B: 8B C4              mov         eax,esp
0000000D: C7 00 3D 79 AD 66  mov         dword ptr [eax],66AD793Dh
00000013: C7 40 04 00 00 00  mov         dword ptr [eax+4],offset ??_C@_0BN@DCIHAEJP@7tvK1fEdg4EKNbmkKrEJAXBiqCdZ?$AA@
00
0000001A: E8 00 00 00 00     call        ?GetMemberInt@KeyValues3@@QBEHVCKV3MemberName@@H@Z
0000001F: 5D                 pop         ebp
00000020: C3                 ret

*/

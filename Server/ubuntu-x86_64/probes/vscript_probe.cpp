#include "vscript/ivscript.h"
#include "tier0/threadtools.h"
#include "probe_checks.h"
#include <dlfcn.h>

struct Primary { virtual ~Primary() {} int padding = 37; };
struct Secondary
{
    int value = 9;
    virtual int Read(int add) { return value + add; }
    int Plain(int add) { return value - add; }
    int Constant(int add) const { return value * add; }
    void Write(int n) { value = n; }
};
struct Entity : Primary, Secondary
{
    int Read(int add) override { return value + add + 100; }
};
static int Sum(int a, int b) { return a + b; }
static void InitDesc();
static ScriptClassDesc_t description(InitDesc);
static void InitDesc()
{
    description.m_pszScriptName = "NativeEntity";
    description.m_pszClassname = "Entity";
    description.m_pszDescription = "Native member function binding verification";
    ScriptAddFunctionToClassDesc(&description, Entity, Read, "Virtual override");
    ScriptAddFunctionToClassDesc(&description, Entity, Plain, "Secondary base member");
    ScriptAddFunctionToClassDesc(&description, Entity, Constant, "Const member");
    ScriptAddFunctionToClassDesc(&description, Entity, Write, "Void member");
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    DeclareCurrentThreadIsMainThread();
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::printf("%s\n", dlerror()); return 1; }
    auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(library, "CreateInterface"));
    auto manager = factory ? static_cast<IScriptManager *>(factory(VSCRIPT_INTERFACE_VERSION, nullptr)) : nullptr;
    Check(manager != nullptr, "load original VScript manager interface");
    if (!manager) return 1;
    auto vm = manager->CreateVM(SL_SQUIRREL);
    Check(vm != nullptr, "create actual Squirrel VM");
    if (!vm) return 1;
    ScriptRegisterFunction(vm, Sum, "Native free function");
    Entity entity;
    auto instance = vm->RegisterInstance(&description, &entity);
    Check(instance && vm->SetValue("entity", ScriptVariant_t(instance)), "register actual instance and member bindings");
    Check(vm->Run(R"nut(
if (Sum(20, 22) != 42) { throw "free"; }
if (entity.Read(3) != 112) { throw "virtual"; }
if (entity.Plain(4) != 5) { throw "base adjustment"; }
if (entity.Constant(3) != 27) { throw "const"; }
entity.Write(17);
)nut") == SCRIPT_DONE,
          "VM invokes free, virtual, secondary base, const and void functions");
    Check(entity.value == 17 && entity.padding == 37, "member invocation modifies the correct subobject");
    // A member pointer explicitly converted to the derived type carries a
    // nonzero this adjustment on this ABI; preserve it through storage too.
    int (Entity::*adjusted)(int) = &Secondary::Plain;
    auto storage = ScriptConvertFuncPtrToVoid(adjusted);
    auto restored = ScriptConvertFuncPtrFromVoid<decltype(adjusted)>(storage);
    Check((entity.*restored)(5) == 12, "explicit multiple inheritance adjustment survives roundtrip");
    vm->RemoveInstance(instance, "entity");
    manager->DestroyVM(vm);
    CheckNativeImages();
    dlclose(library);
    std::printf("RESULT %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}

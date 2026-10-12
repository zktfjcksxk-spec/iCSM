#include "v8.h"
#include "libplatform/libplatform.h"
#include "probe_checks.h"
#include <memory>
#include <cstring>
static void Sum(const v8::FunctionCallbackInfo<v8::Value> &args) {
    auto isolate=args.GetIsolate(); auto ctx=isolate->GetCurrentContext();
    args.GetReturnValue().Set(args[0]->Int32Value(ctx).FromMaybe(0)+args[1]->Int32Value(ctx).FromMaybe(0));
}
int main(int argc,char **argv) {
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    auto platform=v8::platform::NewDefaultPlatform(); v8::V8::InitializePlatform(platform.get());
    Check(v8::V8::Initialize(),"initialize actual native V8");
    auto allocator=std::unique_ptr<v8::ArrayBuffer::Allocator>(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    v8::Isolate::CreateParams params; params.array_buffer_allocator=allocator.get(); auto isolate=v8::Isolate::New(params);
    {
        v8::Isolate::Scope scope(isolate); v8::HandleScope handles(isolate);
        auto global=v8::ObjectTemplate::New(isolate);
        global->Set(isolate,"nativeSum",v8::FunctionTemplate::New(isolate,Sum));
        auto context=v8::Context::New(isolate,nullptr,global); v8::Context::Scope ctx(context);
        const char *code=R"JS(
function add(a,b) { return nativeSum(a,b); }
let total=0; for(let i=0;i<200000;i++) total+=add(20,22);
if(total!==8400000) throw new Error('native binding');
const list=Array.from({length:10000},(_,i)=>({value:i,text:'原生离线对战'}));
if(list[9999].value!==9999) throw new Error('heap');
if(new Intl.NumberFormat('zh-CN').format(1234567)!=='1,234,567') throw new Error('ICU');
'原生离线对战';
)JS";
        v8::TryCatch error(isolate);
        auto text=v8::String::NewFromUtf8(isolate,code).ToLocalChecked(); v8::Local<v8::Script> script; v8::Local<v8::Value> result;
        bool ok=v8::Script::Compile(context,text).ToLocal(&script) && script->Run(context).ToLocal(&result);
        Check(ok,"execute JavaScript loops, native callbacks and heap allocations");
        if(ok) { v8::String::Utf8Value utf8(isolate,result); Check(*utf8 && !std::strcmp(*utf8,"原生离线对战"),"Chinese UTF-8 roundtrip"); }
        else {v8::String::Utf8Value message(isolate,error.Exception()); std::printf("%s\n",*message);}
        isolate->LowMemoryNotification();
    }
    isolate->Dispose(); v8::V8::Dispose(); v8::V8::ShutdownPlatform(); CheckNativeImages();
    std::printf("V8=%s RESULT %s (%d failures)\n",v8::V8::GetVersion(),failures?"FAIL":"PASS",failures); return failures?1:0;
}

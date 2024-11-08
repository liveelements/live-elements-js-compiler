#include "compilerhandle.h"
#include "live/visuallog.h"

namespace lv{

Napi::Function CompilerHandle::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "CompilerHandle", {});
    return func;
}

CompilerHandle::CompilerHandle(const Napi::CallbackInfo& info) 
    : Napi::ObjectWrap<CompilerHandle>(info) 
{
    if (info.Length() == 1 && info[0].IsExternal()) {
        Napi::External<lv::el::Compiler::Ptr> externalCompiler = info[0].As<Napi::External<lv::el::Compiler::Ptr>>();
        m_compiler = *(externalCompiler.Data());
    } else {
        Napi::TypeError::New(info.Env(), "CompilerHandle can only be created internally.").ThrowAsJavaScriptException();
    }
}

lv::el::Compiler::Ptr CompilerHandle::getInternalInstance() const {
    return m_compiler;
}

} // namespace
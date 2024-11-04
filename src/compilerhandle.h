#include <napi.h>
#include "live/elements/compiler/compiler.h"

namespace lv{

class CompilerHandle : public Napi::ObjectWrap<CompilerHandle> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference constructor;

public:
    CompilerHandle(const Napi::CallbackInfo& info);
    lv::el::Compiler::Ptr getInternalInstance() const;

private:
    lv::el::Compiler::Ptr m_compiler;
};

} // namespace



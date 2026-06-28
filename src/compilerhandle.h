#include <napi.h>
#include "live/elements/compiler/compiler.h"
#include "live/elements/compiler/typeresolver.h"
#include <memory>

namespace lv{

class CompilerHandle : public Napi::ObjectWrap<CompilerHandle> {

public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);

public:
    CompilerHandle(const Napi::CallbackInfo& info);
    lv::el::Compiler::Ptr getInternalInstance() const;
    void setTypeResolver(const std::shared_ptr<lv::el::TypeResolver>& resolver);

private:
    lv::el::Compiler::Ptr m_compiler;
    std::shared_ptr<lv::el::TypeResolver> m_typeResolver;
};

} // namespace


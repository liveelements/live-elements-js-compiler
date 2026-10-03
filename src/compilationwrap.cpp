/****************************************************************************
**
** Copyright (C) 2022 Dinu SV.
** This file is part of live-elements-js-compiler.
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
****************************************************************************/

#include "compilationwrap.h"
#include "live/path.h"
#include "live/package.h"
#include "live/modulecontext.h"
#include "live/elements/compiler/modulefile.h"

namespace lv{

Napi::Function CompilationWrap::Init(Napi::Env env){
    return DefineClass(env, "Compilation", {
        InstanceAccessor("target", &CompilationWrap::target, nullptr),
        InstanceAccessor("targetSource", &CompilationWrap::targetSource, nullptr),
        InstanceMethod("jsPath", &CompilationWrap::jsPath),
        InstanceMethod("isDeclarative", &CompilationWrap::isDeclarative),
        InstanceMethod("dependencies", &CompilationWrap::dependencies),
        InstanceMethod("packageOf", &CompilationWrap::packageOf),
        InstanceMethod("release", &CompilationWrap::release)
    });
}

CompilationWrap::CompilationWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<CompilationWrap>(info)
{
    if ( info.Length() == 1 && info[0].IsExternal() ){
        m_compilation = *(info[0].As<Napi::External<lv::el::Compilation::Ptr>>().Data());
    } else {
        Napi::TypeError::New(info.Env(), "Compilation can only be created internally.").ThrowAsJavaScriptException();
    }
}

Napi::Value CompilationWrap::target(const Napi::CallbackInfo &info){
    Napi::Env env = info.Env();
    if ( !m_compilation ){
        Napi::Error::New(env, "Compilation released.").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    lv::el::ModuleFile* target = m_compilation->target();
    return target ? Napi::String::New(env, target->uri()) : env.Undefined();
}

Napi::Value CompilationWrap::targetSource(const Napi::CallbackInfo &info){
    Napi::Env env = info.Env();
    if ( !m_compilation ){
        Napi::Error::New(env, "Compilation released.").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if ( !m_compilation->hasTargetSource() )
        return env.Undefined();
    return Napi::String::New(env, m_compilation->targetSource());
}

Napi::Value CompilationWrap::jsPath(const Napi::CallbackInfo &info){
    Napi::Env env = info.Env();
    if ( !m_compilation ){
        Napi::Error::New(env, "Compilation released.").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if ( info.Length() < 1 || !info[0].IsString() ){
        Napi::TypeError::New(env, "Expected argument: uri:String").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    lv::el::ModuleFile* file = m_compilation->findFile(info[0].As<Napi::String>().Utf8Value());
    if ( !file )
        return env.Undefined();
    return Napi::String::New(env, Path::toUnixSeparator(file->jsFilePath()));
}

Napi::Value CompilationWrap::isDeclarative(const Napi::CallbackInfo &info){
    Napi::Env env = info.Env();
    if ( !m_compilation ){
        Napi::Error::New(env, "Compilation released.").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if ( info.Length() < 1 || !info[0].IsString() ){
        Napi::TypeError::New(env, "Expected argument: uri:String").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    lv::el::ModuleFile* file = m_compilation->findFile(info[0].As<Napi::String>().Utf8Value());
    if ( !file )
        return env.Undefined();
    switch ( file->declarativeKind() ){
    case lv::el::ModuleFile::Declarative: return Napi::Boolean::New(env, true);
    case lv::el::ModuleFile::NotDeclarative: return Napi::Boolean::New(env, false);
    default: return env.Undefined();
    }
}

Napi::Value CompilationWrap::dependencies(const Napi::CallbackInfo &info){
    Napi::Env env = info.Env();
    if ( !m_compilation ){
        Napi::Error::New(env, "Compilation released.").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if ( info.Length() < 1 || !info[0].IsString() ){
        Napi::TypeError::New(env, "Expected argument: uri:String").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    lv::el::ModuleFile* file = m_compilation->findFile(info[0].As<Napi::String>().Utf8Value());
    if ( !file )
        return env.Undefined();

    if ( !file->descriptor() )
        return Napi::Array::New(env, 0);

    const auto& entries = file->descriptor()->dependencyEntries();
    Napi::Array result = Napi::Array::New(env, entries.size());
    for ( size_t i = 0; i < entries.size(); ++i ){
        Napi::Object entry = Napi::Object::New(env);
        entry.Set("name", entries[i].name().data());
        entry.Set("uri", entries[i].uri().data());
        result.Set(static_cast<uint32_t>(i), entry);
    }
    return result;
}

Napi::Value CompilationWrap::packageOf(const Napi::CallbackInfo &info){
    Napi::Env env = info.Env();
    if ( !m_compilation ){
        Napi::Error::New(env, "Compilation released.").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if ( info.Length() < 1 || !info[0].IsString() ){
        Napi::TypeError::New(env, "Expected argument: uri:String").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string uri = info[0].As<Napi::String>().Utf8Value();

    // strip an '@<name>' suffix from the last '.lv@', since scoped packages start with '@'
    size_t exportSeparator = uri.rfind(".lv@");
    if ( exportSeparator != std::string::npos )
        uri = uri.substr(0, exportSeparator + 3);

    if ( !m_compilation->findFile(uri) )
        return env.Undefined();

    std::string moduleUri = uri.substr(0, uri.rfind('/'));
    for ( const lv::el::ElementsModule::Ptr& em : m_compilation->modules() ){
        if ( em->uri() != moduleUri )
            continue;
        const Module::Ptr& module = em->module();
        Package::Ptr package = module->context() ? module->context()->packageUnwrapped() : nullptr;
        if ( package )
            return Napi::String::New(env, package->nameScope());
        // the package graph is gone with its compiler
        return Napi::String::New(env, Package::nameScopeFromPath(module->packagePath()));
    }
    return env.Undefined();
}

Napi::Value CompilationWrap::release(const Napi::CallbackInfo &info){
    m_compilation = nullptr;
    return info.Env().Undefined();
}

} // namespace

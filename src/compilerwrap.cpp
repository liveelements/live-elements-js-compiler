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

#include "compilerwrap.h"
#include "live/visuallog.h"
#include "live/utf8.h"
#include "live/path.h"
#include "live/elements/compiler/compiler.h"
#include "live/elements/compiler/modulefile.h"
#include "live/mlnodetojson.h"

namespace lv{

void convertToMLNode(const Napi::Value& v, MLNode& n){
    if ( v.IsArray() ){
        n = MLNode(MLNode::Type::Array);
        Napi::Array va = v.As<Napi::Array>();
        for ( unsigned int i = 0; i < va.Length(); ++i ){
            MLNode result;
            convertToMLNode(va.Get(i), result);
            n.append(result);
        }
    } else if ( v.IsString() ){
        n = MLNode(v.As<Napi::String>().Utf8Value());
    } else if ( v.IsObject() ){
        Napi::Object vo = v.As<Napi::Object>();
        n = MLNode(MLNode::Type::Object);

        for (const auto& e : vo) {
            std::string key = e.first.As<Napi::String>().Utf8Value();
            Napi::Value value = static_cast<Napi::Value>(e.second);

            MLNode result;
            convertToMLNode(value, result);
            n[key] = result;
        }
    } else if ( v.IsBoolean() ){
        n = MLNode(v.As<Napi::Boolean>());
    } else if ( v.IsNumber() ){
        Napi::Number number = v.As<Napi::Number>();
        MLNode::FloatType dn = static_cast<MLNode::FloatType>(number.DoubleValue());
        MLNode::IntType ln = static_cast<MLNode::IntType>(number.Int64Value());
        n = dn == ln ? MLNode(ln) : MLNode(dn);
    } else {
        n = MLNode();
    }
}

void populateErrorMessage(Napi::Env env, Napi::Object ob, std::exception* e = nullptr){
    if ( e ){
        Napi::ObjectReference err = Napi::TypeError::New(env, e->what());
        ob.Set("error", err.Value());
    } else {
        ob.Set("error", "Unknown");
    }
}

void populateError(Napi::Env env, Napi::Object ob, lv::Exception* e){
    Napi::Object internal = Napi::Object::New(env);
    std::string file = e->file();
    size_t separatorPos = file.rfind('/');
    if ( separatorPos == std::string::npos ){
        separatorPos = file.rfind('\\');
    }
    if ( separatorPos != std::string::npos )
        file = file.substr(separatorPos + 1);
    internal.Set("file", file);
    internal.Set("line", e->line());
    ob.Set("__internal", internal);
    ob.Set("message", e->message());
    ob.Set("code", e->code());
    Napi::ObjectReference err = Napi::TypeError::New(env, e->message());
    ob.Set("error", err.Value());
}

void populateSyntaxError(Napi::Env env, Napi::Object ob, lv::el::SyntaxException* e){
    Napi::Object source = Napi::Object::New(env);
    source.Set("file", e->parsedFile());
    source.Set("line", e->parsedLine());
    source.Set("column", e->parsedColumn());
    source.Set("offset", e->parsedOffset());
    ob.Set("source", source);

    populateError(env, ob, e);
}

void compileWrap(const Napi::CallbackInfo& info){
    Napi::Env env = info.Env();
    if( info.Length() < 2 || !info[0].IsString() || !info[1].IsObject()){
        Napi::TypeError::New(env, "Compile: path:String, options:Object expected").ThrowAsJavaScriptException();
        return;
    }

    Napi::String fileArg = info[0].As<Napi::String>();
    Napi::Object optionsArg = info[1].As<Napi::Object>();

    Napi::Value err = env.Undefined();
    Napi::Value res = env.Undefined();

    try{
        std::string file = fileArg.Utf8Value();
        MLNode compilerOptions;

        if ( optionsArg.Has("log") ){
            Napi::Object logOptionsArg = optionsArg.Get("log").As<Napi::Object>();
            MLNode logOptions;
            convertToMLNode(logOptionsArg, logOptions);

            vlog().configure("global", logOptions);
            optionsArg.Delete("log");
        }

        convertToMLNode(optionsArg, compilerOptions);

        if ( !Path::exists(file) ){
            THROW_EXCEPTION(lv::Exception, "Error: Script file not found.", lv::Exception::toCode("~File"));
        }

        lv::el::Compiler::Config config;
        config.initialize(compilerOptions);
        lv::el::Compiler::Ptr compiler = lv::el::Compiler::create(config);
        std::string scriptFile = Path::resolve(file);
        std::string pluginPath = Path::parent(scriptFile);

        Module::Ptr module(nullptr);
        if ( Module::existsIn(pluginPath) ){
            module = Module::createFromPath(pluginPath);
            Package::Ptr package = Package::createFromPath(module->package());
            if ( package ){
                compiler->setPackageImportPaths({package->path() + "/" + compiler->importLocalPath()});
            }
        }
        lv::el::ElementsModule::Ptr elemMod = lv::el::Compiler::compile(compiler, scriptFile);
        lv::el::ModuleFile* mf = elemMod->moduleFileBypath(scriptFile);
        if ( mf ){
            res = Napi::String::New(env, mf->jsFilePath());
        }

    } catch ( lv::el::SyntaxException& e ){
        Napi::Object ob = Napi::Object::New(env);
        populateSyntaxError(env, ob, &e);
        err = ob;
    } catch ( lv::Exception& e ){
        Napi::Object ob = Napi::Object::New(env);
        populateError(env, ob, &e);
        err = ob;
    } catch ( std::exception& e ){
        Napi::Object ob = Napi::Object::New(env);
        populateErrorMessage(env, ob, &e);
        err = ob;
    } catch ( ... ){
        Napi::Object ob = Napi::Object::New(env);
        populateErrorMessage(env, ob, nullptr);
        err = ob;
    }

    if ( info.Length() > 2 ){
        Napi::Function cb = info[2].As<Napi::Function>();
        cb.Call(env.Global(), {res, err});
    }
}

void compileModuleWrap(const Napi::CallbackInfo &info){
    Napi::Env env = info.Env();
    if( info.Length() < 2 || !info[0].IsString() || !info[1].IsObject()){
        Napi::TypeError::New(env, "Compile: path:String, options:Object expected").ThrowAsJavaScriptException();
        return;
    }

    Napi::String modulePathArg = info[0].As<Napi::String>();
    Napi::Object optionsArg = info[1].As<Napi::Object>();

    Napi::Value err = env.Undefined();
    Napi::Value res = env.Undefined();

    try{
        std::string modulePath = modulePathArg.Utf8Value();
        MLNode compilerOptions;

        if ( optionsArg.Has("log") ){
            Napi::Object logOptionsArg = optionsArg.Get("log").As<Napi::Object>();
            MLNode logOptions;
            convertToMLNode(logOptionsArg, logOptions);

            vlog().configure("global", logOptions);
            optionsArg.Delete("log");
        }

        convertToMLNode(optionsArg, compilerOptions);

        if ( !Path::exists(modulePath) ){
            THROW_EXCEPTION(lv::Exception, "Error: Module path not found.", lv::Exception::toCode("~Path"));
        }

        lv::el::Compiler::Config config;
        config.initialize(compilerOptions);
        lv::el::Compiler::Ptr compiler = lv::el::Compiler::create(config);

        lv::el::ElementsModule::Ptr elemMod = lv::el::Compiler::compileModule(compiler, modulePath);
        if ( elemMod ){
            res = Napi::String::New(env, compiler->moduleBuildPath(elemMod->module()));
        }

    } catch ( lv::el::SyntaxException& e ){
        Napi::Object ob = Napi::Object::New(env);
        populateSyntaxError(env, ob, &e);
        err = ob;
    } catch ( lv::Exception& e ){
        Napi::Object ob = Napi::Object::New(env);
        populateError(env, ob, &e);
        err = ob;
    } catch ( std::exception& e ){
        Napi::Object ob = Napi::Object::New(env);
        populateErrorMessage(env, ob, &e);
        err = ob;
    } catch ( ... ){
        Napi::Object ob = Napi::Object::New(env);
        populateErrorMessage(env, ob, nullptr);
        err = ob;
    }

    if ( info.Length() > 2 ){
        Napi::Function cb = info[2].As<Napi::Function>();
        cb.Call(env.Global(), {res, err});
    }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("compile", Napi::Function::New(env, lv::compileWrap));
    exports.Set("compileModule", Napi::Function::New(env, lv::compileModuleWrap));
    return exports;
}

} // namespace
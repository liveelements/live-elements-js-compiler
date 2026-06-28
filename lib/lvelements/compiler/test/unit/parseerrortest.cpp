/****************************************************************************
**
** Copyright (C) 2022 Dinu SV.
**
** This file is part of Livekeys Application.
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

#include "catch_library.h"
#include "live/fileio.h"
#include "live/visuallog.h"
#include "live/datetime.h"
#include "live/applicationcontext.h"

#include "live/elements/compiler/languageparser.h"
#include "live/elements/compiler/compiler.h"

using namespace lv;
using namespace lv::el;

Compiler::Config flowErrorCompilerConfig() {
    Compiler::Config compilerConfig(false);
    compilerConfig.allowUnresolvedTypes(true);
    compilerConfig.setFlowModule("flow/flow.mjs", "Flow");
    compilerConfig.setFlowCall(Compiler::Config::FlowCall::Create, "Flow.create");
    compilerConfig.setFlowCall(Compiler::Config::FlowCall::Call, "Flow.call");
    compilerConfig.setFlowCall(Compiler::Config::FlowCall::Return, "Flow.ret");
    compilerConfig.setFlowCall(Compiler::Config::FlowCall::Ref, "Flow.ref");
    compilerConfig.setFlowCall(Compiler::Config::FlowCall::Member, "Flow.member");
    return compilerConfig;
}

FileIO& fileIO(){
    static FileIO fileio;
    return fileio;
}

const std::string& scriptPath(){
    static std::string scriptPath = Path::join(Path::parent(lv::ApplicationContext::instance().applicationFilePath()), "data");;
    return scriptPath;
}



TEST_CASE( "Parse Error Test", "[ParseError]" ) {

    SECTION("Program Body Extra Elements"){
        std::string name = "ParserErrorTest01";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);

        Compiler::Ptr compiler = Compiler::create();
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }
    SECTION("Identifier Not Found"){
        std::string name = "ParserErrorTest02";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(scriptPath() + "/" + name + ".lv");

        Compiler::Config compilerConfig;
        compilerConfig.allowUnresolvedTypes(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Identifier"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Return in assignment expression"){
        std::string name = "ParserErrorTest03";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(scriptPath() + "/" + name + ".lv");

        Compiler::Config compilerConfig;
        compilerConfig.allowUnresolvedTypes(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Constructor initializer outside constructor"){
        std::string name = "ParserErrorTest04";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(scriptPath() + "/" + name + ".lv");

        Compiler::Config compilerConfig;
        compilerConfig.allowUnresolvedTypes(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Constructor initializer with error"){
        std::string name = "ParserErrorTest05";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(scriptPath() + "/" + name + ".lv");

        Compiler::Config compilerConfig;
        compilerConfig.allowUnresolvedTypes(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }
    SECTION("Symbol factory with symbol property"){
        std::string name = "ParserErrorTest06";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(scriptPath() + "/" + name + ".lv");

        Compiler::Config compilerConfig;
        compilerConfig.allowUnresolvedTypes(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow forward reference"){
        std::string contents = "fn build(): any {\n    const flow = n: int %> {\n        return later\n        add(n, 1) @later\n    }\n}\n";
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget("FlowForwardReference.lv", contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow duplicate tap"){
        std::string contents = "fn build(): any {\n    const flow = n: int %> {\n        add(n, 1) @value\n        add(value, 2) @value\n        return value\n    }\n}\n";
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget("FlowDuplicateTap.lv", contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow nested call in arguments"){
        std::string contents = "fn build(): any {\n    const flow = n: int %> {\n        outer(inner(n)) @value\n        return value\n    }\n}\n";
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget("FlowNestedCall.lv", contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow previous with multiple parameters on line one"){
        std::string contents = "fn build(): any {\n    const flow = (a: int, b: int) %> return %\n}\n";
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget("FlowPrevious.lv", contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.parsedLocation().range().start().line() == 2);
            REQUIRE(e.parsedLocation().range().start().column() == 5);
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow missing configuration"){
        std::string contents = "fn build(): any {\n    const flow = n: int %> add(n, 1)\n}\n";
        Compiler::Config compilerConfig(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        bool hadException = false;

        try {
            compiler->compileToTarget("FlowMissingConfig.lv", contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Flow"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow untyped parameters"){
        std::string contents = "fn build(): any {\n    const flow = (a, b) %> add(a, b)\n}\n";
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget("FlowUntypedParameters.lv", contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }
}

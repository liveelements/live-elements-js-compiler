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

    SECTION("Flow Forward Reference"){
        std::string name = "ParserErrorTest07";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        Compiler::TargetResult targetResult = compiler->compileToTarget(filePath, contents);

        REQUIRE(targetResult.js.find("Flow.ret(Flow.ref('later'))") != std::string::npos);
    }

    SECTION("Flow Duplicate Tap"){
        std::string name = "ParserErrorTest08";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow Broken Reference Missing Import"){
        std::string name = "ParserErrorTest09";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);

        Compiler::Config compilerConfig = flowErrorCompilerConfig();
        compilerConfig.allowUnresolvedTypes(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Identifier"));
            REQUIRE(e.message().find("Identifier not found in imports: 'missingValue'") != std::string::npos);
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow Nested Call In Arguments"){
        std::string name = "ParserErrorTest10";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow Rejects Parameter Callee"){
        std::string name = "ParserErrorTest11";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());

        try {
            compiler->compileToTarget(filePath, contents);
            FAIL("Expected a SyntaxException for a flow parameter callee.");
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find("Cannot call 'fn' as a function") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 3);
            REQUIRE(e.parsedLocation().range().start().column() == 9);
        }
    }

    SECTION("Flow Rejects Tap Callee"){
        std::string name = "ParserErrorTest12";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());

        try {
            compiler->compileToTarget(filePath, contents);
            FAIL("Expected a SyntaxException for a flow tap callee.");
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find("Cannot call 'g' as a function") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 4);
            REQUIRE(e.parsedLocation().range().start().column() == 9);
        }
    }

    SECTION("Flow Rejects Dotted Parameter Callee"){
        std::string name = "ParserErrorTest13";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());

        try {
            compiler->compileToTarget(filePath, contents);
            FAIL("Expected a SyntaxException for a dotted flow parameter callee.");
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find("Cannot call 'mod' as a function") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 3);
            REQUIRE(e.parsedLocation().range().start().column() == 9);
        }
    }

    SECTION("Flow Rejects Imported Value Reference"){
        std::string name = "ParserErrorTest14";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());

        try {
            compiler->compileToTarget(filePath, contents);
            FAIL("Expected a SyntaxException for an imported value used as a flow reference.");
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find("'someImportedFn' is not a flow value") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 4);
            REQUIRE(e.parsedLocation().range().start().column() == 21);
        }
    }

    SECTION("Flow Previous With Multiple Parameters On Line One"){
        std::string name = "ParserErrorTest15";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.parsedLocation().range().start().line() == 2);
            REQUIRE(e.parsedLocation().range().start().column() == 5);
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow Missing Configuration"){
        std::string name = "ParserErrorTest16";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Config compilerConfig(false);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);
        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::Exception& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Flow"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow Untyped Parameters"){
        std::string name = "ParserErrorTest17";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());
        bool hadException = false;

        try {
            compiler->compileToTarget(filePath, contents);
        } catch (lv::el::SyntaxException& e) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            hadException = true;
        }
        REQUIRE(hadException);
    }

    SECTION("Flow Rejects Captured Locals"){
        std::string name = "ParserErrorTest18";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());

        try {
            compiler->compileToTarget(filePath, contents);
            FAIL("Expected a SyntaxException for a captured local.");
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find("'helper' is declared outside the flow") != std::string::npos);
        }
    }

    SECTION("Flow Rejects Outer Flow Captures"){
        std::string name = "ParserErrorTest19";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());

        try {
            compiler->compileToTarget(filePath, contents);
            FAIL("Expected a SyntaxException for an outer-flow capture.");
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find("'outer' is declared outside the flow") != std::string::npos);
        }
    }
}

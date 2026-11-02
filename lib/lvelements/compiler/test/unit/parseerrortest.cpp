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

#include <vector>

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

class FlowRejection{
public:
    std::string source;
    std::string message;
    int line;
    int column;
};

void requireFlowRejections(const std::vector<FlowRejection>& rejections){
    const std::string filePath = scriptPath() + "/ParserErrorTestInline.lv";
    for ( const FlowRejection& rejection : rejections ){
        Compiler::Ptr compiler = Compiler::create(flowErrorCompilerConfig());

        INFO(rejection.source);
        try {
            compiler->compileToTarget(filePath, rejection.source);
            FAIL("Expected a SyntaxException: " + rejection.message);
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find(rejection.message) != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == rejection.line);
            REQUIRE(e.parsedLocation().range().start().column() == rejection.column);
        }
    }
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

    SECTION("Flow Rejects Swallowed Syntax Errors"){
        const std::string invalid = "Invalid flow syntax.";

        requireFlowRejections({
            {"@fn main(v:number): any %> {\n    pass({ t })\n    return v\n}", invalid, 2, 12},
            {"@fn main(v:number): any %> {\n    pass({ a: 1, t })\n    return v\n}", invalid, 2, 18},
            {"@fn main(v:number): any %> {\n    pass({ a: 1 b: 2 })\n    return v\n}", invalid, 2, 12},
            {"@fn main(v:number): any %> {\n    pass({ 1: 2 })\n    return v\n}", invalid, 2, 12},
            {"@fn main(v:number): any %> {\n    pass({ a: 1, + })\n    return v\n}", invalid, 2, 18},
            {"fn build(): any {\n    return v: number %> { pass({ t }) }\n}", invalid, 2, 34},
            {"@fn main(v:number): any %> {\n    add - 1 2 @r\n    return r\n}", invalid, 2, 9},
            {"@fn main(v:number): any %> {\n    pass(1\n    return v\n}", invalid, 2, 11},
            {"@fn main(v: List<number): any %> {\n    return v\n}", invalid, 1, 24},
            {"@fn main(v: any): a[ny %> {\n    return v\n}", invalid, 1, 23},
            {"@fn main(v:any): any %> {\n    map(v, (x: number) %> {\n        pass({ t })\n        return x\n    }) @r\n    return r\n}", invalid, 3, 16},
        });
    }

    SECTION("Flow Rejects References In Literals"){
        auto message = [](const std::string& name){
            return "'" + name + "' can't be used inside an object or array literal. Pass it as a whole argument, or build the value with a function.";
        };

        requireFlowRejections({
            {"@fn main(sum:number): any %> {\n    pass([sum])\n    return sum\n}", message("sum"), 2, 11},
            {"@fn main(sum:number): any %> {\n    pass({ a: sum })\n    return sum\n}", message("sum"), 2, 15},
            {"@fn main(sum:number): any %> {\n    pass({ a: [{ b: sum }] })\n    return sum\n}", message("sum"), 2, 21},
            {"@fn main(a:any): any %> {\n    pass([%])\n    return a\n}", message("%"), 2, 11},
            {"@fn main(a:any, b:any): any %> {\n    pass([%])\n    return a\n}", message("%"), 2, 11},
            {"@fn main(a:any): any %> {\n    pass([a.b])\n    return a\n}", message("a.b"), 2, 11},
            {"@fn main(a:any): any %> {\n    pass [a.b]\n    return a\n}", message("a.b"), 2, 11},
            {"@fn main(a:any): any %> {\n    pass([1, nope])\n    return a\n}", message("nope"), 2, 14},
            {"@fn main(sum:number): any %> {\n    return [sum]\n}", message("sum"), 2, 13},
            {"@fn main(sum:number): any %> {\n    return { a: sum }\n}", message("sum"), 2, 17},
            {"@fn main(sum:number): any %> {\n    return { a: [{ b: sum }] }\n}", message("sum"), 2, 23},
            {"@fn main(a:any): any %> {\n    pass(a)\n    return [%]\n}", message("%"), 3, 13},
            {"@fn main(a:any): any %> {\n    return [a.b]\n}", message("a.b"), 2, 13},
            {"@fn main(a:any): any %> {\n    pass([1, (v: number) %> [v]])\n    return a\n}", message("v"), 2, 30},
        });
    }

    SECTION("Flow Keeps The Nested Call Error Inside Literals"){
        requireFlowRejections({
            {"@fn main(a:any): any %> {\n    pass([inner(a)])\n    return a\n}", "Nested flow calls are not allowed in argument position.", 2, 11},
        });
    }

    SECTION("Flow Rejects A Step Result Reusing An Outside Name"){
        auto message = [](const std::string& name){
            return "A step result can't reuse '" + name + "', a name from outside the flow. Use another name.";
        };
        const std::string flow = "fn build(): any {\n    return (v: number) %> {\n        add(1, 10) @add\n        return add\n    }\n}\n";

        requireFlowRejections({
            {"import {add} from './math.mjs'\n" + flow, message("add"), 4, 21},
            {"import .math\n" + flow, message("add"), 4, 21},
            {"fn add(a:number, b:number): number { return a + b }\n" + flow, message("add"), 4, 21},
            {"@fn add(a:number): any %> { return a }\n" + flow, message("add"), 4, 21},
            {"import {add, mul} from './math.mjs'\n@fn main(v:number): any %> {\n    mul(v, 2) @add\n    return add\n}", message("add"), 3, 16},
            {"import {add, mul} from './math.mjs'\n@fn main(v:any): any %> {\n    mul(v, (x: number) %> {\n        mul(x, 2) @add\n        return add\n    }) @r\n    return r\n}", message("add"), 4, 20},
            {"import {mul} from './math.mjs'\ncomponent Box{}\n@fn main(v:number): any %> {\n    mul(v, 10) @Box\n    return Box\n}", message("Box"), 4, 17},
            {"import {mul} from './math.mjs'\ntype Count = number\n@fn main(v:number): any %> {\n    mul(v, 10) @Count\n    return Count\n}", message("Count"), 4, 17},
        });
    }

    SECTION("Duplicate Generic Type Parameters"){
        std::string name = "ParserErrorTest20";
        std::string filePath = scriptPath() + "/" + name + ".lv";
        std::string contents = fileIO().readFromFile(filePath);

        Compiler::Config compilerConfig;
        compilerConfig.allowUnresolvedTypes(true);
        compilerConfig.outputTarget(Compiler::Config::OutputTarget::JS_DTS);
        Compiler::Ptr compiler = Compiler::create(compilerConfig);

        try {
            compiler->compileToTarget(filePath, contents);
            FAIL("Expected a SyntaxException for duplicate type parameters.");
        } catch ( const lv::el::SyntaxException& e ) {
            REQUIRE(e.code() == lv::Exception::toCode("~Language"));
            REQUIRE(e.message().find("Duplicate type parameter 'X'.") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 1);
        }
    }
}

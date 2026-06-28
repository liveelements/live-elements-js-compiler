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
#include "live/elements/compiler/typerequestsink.h"
#include "live/elements/compiler/typeresolver.h"
#include <cctype>
#include <map>

using namespace lv;
using namespace lv::el;

namespace {

std::string normalizeSpaces(const std::string& value){
    std::string result;
    for ( char ch : value ){
        if ( !std::isspace(static_cast<unsigned char>(ch)) ){
            result.push_back(ch);
        }
    }
    return result;
}

class StubTypeResolver : public TypeResolver {
public:
    std::map<std::string, Result> responses;
    std::vector<Request> observedRequests;

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        std::vector<Result> result;
        for ( const Request& request : requests ){
            auto it = responses.find(request.requestId);
            if ( it != responses.end() ){
                result.push_back(it->second);
            }
        }
        return result;
    }
};

class ThrowingTypeResolver : public TypeResolver {
public:
    std::vector<Request> observedRequests;
    std::string message = "Type resolver failed.";

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        throw lv::Exception(message, lv::Exception::toCode("~Type"));
    }
};

class MemoryFileIO : public FileIOInterface {
public:
    std::string readFromFile(const std::string& path) override{
        auto it = files.find(path);
        return it == files.end() ? std::string() : it->second;
    }

    bool writeToFile(const std::string& path, const std::string& content) override{
        files[path] = content;
        return true;
    }

    bool writeToFile(const std::string& path, const char* content, size_t length) override{
        files[path] = std::string(content, length);
        return true;
    }

    std::map<std::string, std::string> files;
};

}

void testFileParseOutput(const std::string& name, const std::string& contents, Compiler::Config::OutputTarget outputTarget, const std::string& expectation) {
    static FileIO fileIO;
    static std::string scriptPath = Path::join(Path::parent(lv::ApplicationContext::instance().applicationFilePath()), "data");

    const auto expectationPath = Path::join(scriptPath, name + ".lv" + expectation);

    if (!Path::exists(expectationPath)) 
        return;
    const auto expectedContent = fileIO.readFromFile(expectationPath);
    Compiler::Config compilerConfig(false);
    compilerConfig.allowUnresolvedTypes(true);
    compilerConfig.outputTarget(outputTarget);
    Compiler::Ptr compiler = Compiler::create(compilerConfig);
    compiler->configureImplicitType("console");
    compiler->configureImplicitType("vlog");

    Compiler::TargetResult targetResult = compiler->compileToTarget(Path::join(scriptPath, name + ".lv"), contents);
    std::string conversion = targetResult.js;
    if (expectation == ".ts") conversion = targetResult.ts;
    else if (expectation == ".d.ts") conversion = targetResult.dts;
    if (conversion.empty()) conversion = targetResult.js;

    el::LanguageParser::Ptr parser = el::LanguageParser::createForElements();
    el::LanguageParser::AST* conversionAST = parser->parse(conversion);
    el::LanguageParser::AST* expectedAST   = parser->parse(expectedContent);

    el::LanguageParser::ComparisonResult compare = parser->compare(expectedContent, expectedAST, conversion, conversionAST);
    parser->destroy(conversionAST);
    parser->destroy(expectedAST);

    if ( !compare.isEqual() ){
        vlog().e() << "File: " << name << ".lv" << expectation << ":" << compare.source1Row() << ":" << compare.source1Col();
        vlog().e() << compare.errorString();
        vlog().e() << conversion;
    }
    REQUIRE(compare.isEqual());
}

void testFlowFileParse(const std::string& name){
    static FileIO fileIO;
    static std::string scriptPath = Path::join(Path::parent(lv::ApplicationContext::instance().applicationFilePath()), "data");

    const std::string contents = fileIO.readFromFile(Path::join(scriptPath, name + ".lv"));
    auto runExpectation = [&](const std::string& suffix, const std::string& actual) {
        const auto expectationPath = Path::join(scriptPath, name + ".lv" + suffix);
        const auto expectedContent = fileIO.readFromFile(expectationPath);
        const std::string normalizedActual = normalizeSpaces(actual);
        const std::string normalizedExpected = normalizeSpaces(expectedContent);
        if ( normalizedActual != normalizedExpected ){
            vlog().e() << "File: " << name << ".lv" << suffix;
            vlog().e() << actual;
        }
        REQUIRE(normalizedActual == normalizedExpected);
    };

    Compiler::Config jsConfig(false);
    jsConfig.allowUnresolvedTypes(true);
    jsConfig.outputTarget(Compiler::Config::OutputTarget::JS);
    jsConfig.setFlowModule("flow/flow.mjs", "Flow");
    jsConfig.setFlowCall(Compiler::Config::FlowCall::Create, "Flow.create");
    jsConfig.setFlowCall(Compiler::Config::FlowCall::Call, "Flow.call");
    jsConfig.setFlowCall(Compiler::Config::FlowCall::Return, "Flow.ret");
    jsConfig.setFlowCall(Compiler::Config::FlowCall::Ref, "Flow.ref");
    jsConfig.setFlowCall(Compiler::Config::FlowCall::Member, "Flow.member");

    Compiler::Ptr jsCompiler = Compiler::create(jsConfig);
    jsCompiler->configureImplicitType("console");
    jsCompiler->configureImplicitType("vlog");
    Compiler::TargetResult jsResult = jsCompiler->compileToTarget(Path::join(scriptPath, name + ".lv"), contents);
    runExpectation(".js", jsResult.js);

    Compiler::Config dtsConfig(false);
    dtsConfig.allowUnresolvedTypes(true);
    dtsConfig.outputTarget(Compiler::Config::OutputTarget::JS_DTS);
    dtsConfig.setFlowModule("flow/flow.mjs", "Flow");
    dtsConfig.setFlowCall(Compiler::Config::FlowCall::Create, "Flow.create");
    dtsConfig.setFlowCall(Compiler::Config::FlowCall::Call, "Flow.call");
    dtsConfig.setFlowCall(Compiler::Config::FlowCall::Return, "Flow.ret");
    dtsConfig.setFlowCall(Compiler::Config::FlowCall::Ref, "Flow.ref");
    dtsConfig.setFlowCall(Compiler::Config::FlowCall::Member, "Flow.member");

    Compiler::Ptr dtsCompiler = Compiler::create(dtsConfig);
    dtsCompiler->configureImplicitType("console");
    dtsCompiler->configureImplicitType("vlog");
    Compiler::TargetResult dtsResult = dtsCompiler->compileToTarget(Path::join(scriptPath, name + ".lv"), contents);
    runExpectation(".d.ts", dtsResult.dts);
}

void testFileParse(const std::string& name){
    static FileIO fileIO;
    static std::string scriptPath = Path::join(Path::parent(lv::ApplicationContext::instance().applicationFilePath()), "data");

    try{
        std::string contents = fileIO.readFromFile(Path::join(scriptPath, name + ".lv"));

        testFileParseOutput(name, contents, Compiler::Config::OutputTarget::JS, ".js");
        testFileParseOutput(name, contents, Compiler::Config::OutputTarget::JS_DTS, ".d.ts");

    } catch ( lv::el::SyntaxException& e ){
        FAIL(Utf8("SyntaxException: % at %:%").format(e.message(), e.parsedLocation().filePath(), e.parsedLocation().range().start().line()).data().c_str());
    } catch ( lv::Exception& e ){
        FAIL(("Exception triggered: " + e.message()).c_str());
    }
}


TEST_CASE( "Parse Test", "[Parse]" ) {
    SECTION("Constructor"){ testFileParse("ParserTest01"); }
    SECTION("Function"){ testFileParse("ParserTest02"); }
    SECTION("Nesting and Short Id"){ testFileParse("ParserTest03"); }
    SECTION("Scope"){ testFileParse("ParserTest04"); }
    SECTION("Property New Expression"){ testFileParse("ParserTest05"); }
    SECTION("Property Declaration"){ testFileParse("ParserTest06"); }
    SECTION("Property Expressions"){ testFileParse("ParserTest07"); }
    SECTION("Tagged String"){ testFileParse("ParserTest08"); }
    SECTION("Instance"){ testFileParse("ParserTest09"); }

    SECTION("Namespace Inheritance"){ testFileParse("ParserTest10"); }
    SECTION("Complex Property Assignment"){ testFileParse("ParserTest11"); }
    SECTION("Simple Property Assignments"){ testFileParse("ParserTest12"); }
    SECTION("Test Scenario Parse"){ testFileParse("ParserTest13"); }
    SECTION("Nested Language Scanner"){ testFileParse("ParserTest14"); }
    SECTION("Nested Function Assignment"){ testFileParse("ParserTest15"); }
    SECTION("Arrow Function"){ testFileParse("ParserTest16"); }
    SECTION("TodoList Item Small Sample"){ testFileParse("ParserTest17"); }
    SECTION("TodoList Sample"){ testFileParse("ParserTest18"); }
    SECTION("TodoList Item Sample"){ testFileParse("ParserTest19"); }
    SECTION("Todo Form Sample"){ testFileParse("ParserTest20"); }
    SECTION("TodoApp Sample"){ testFileParse("ParserTest21"); }
    SECTION("Complex Ternary Operator"){ testFileParse("ParserTest22"); }
    SECTION("Double Nested Element"){ testFileParse("ParserTest23"); }
    SECTION("Id Object in Js Scope"){ testFileParse("ParserTest24"); }
    SECTION("Nested Objects in Js Scope"){ testFileParse("ParserTest25"); }
    SECTION("Same Id in Both Scopes"){ testFileParse("ParserTest26"); }
    SECTION("Namespace Import"){ testFileParse("ParserTest27"); }
    SECTION("Multi Namespace Import"){ testFileParse("ParserTest28"); }
    SECTION("Custom Base Component"){
        FileIO fileIO;
        static std::string scriptPath = Path::join(Path::parent(lv::ApplicationContext::instance().applicationFilePath()), "data");

        try{
            std::string name = "ParserTest29";
            std::string contents = fileIO.readFromFile(Path::join(scriptPath, name + ".lv"));
            std::string expect   = fileIO.readFromFile(Path::join(scriptPath, name + ".lv.js"));

            Compiler::Config compConfig = Compiler::Config();
            compConfig.setBaseComponent("CustomElement", "custom/CustomElement.js");

            Compiler::Ptr compiler = Compiler::create(compConfig);
            compiler->configureImplicitType("console");
            compiler->configureImplicitType("vlog");

            el::LanguageParser::Ptr parser = el::LanguageParser::createForElements();

            Compiler::TargetResult targetResult = compiler->compileToTarget(Path::join(scriptPath, name + ".lv.js"), contents);
            std::string conversion = targetResult.js;

            el::LanguageParser::AST* conversionAST = parser->parse(conversion);
            el::LanguageParser::AST* expectedAST   = parser->parse(expect);

            el::LanguageParser::ComparisonResult compare = parser->compare(expect, expectedAST, conversion, conversionAST);
            parser->destroy(conversionAST);
            parser->destroy(expectedAST);

            REQUIRE(compare.isEqual());
        } catch ( lv::el::SyntaxException& e ){
            FAIL(Utf8("SyntaxException: % at %:%").format(e.message(), e.parsedLocation().filePath(), e.parsedLocation().range().start().line()).data().c_str());
        } catch ( lv::Exception& e ){
            FAIL(("Exception triggered: " + e.message()).c_str());
        }
    }

    SECTION("Default Children"){ testFileParse("ParserTest30"); }
    SECTION("Bind Should Not Be Imported"){ testFileParse("ParserTest31"); }
    SECTION("Initialize Element"){ testFileParse("ParserTest32"); }
    SECTION("Custom Constructor Arguments"){ testFileParse("ParserTest33"); }
    SECTION("Unassigned Property"){ testFileParse("ParserTest34"); }
    SECTION("Static Property"){ testFileParse("ParserTest35"); }
    SECTION("Function Annotation"){ testFileParse("ParserTest36"); }
    SECTION("Js Imports"){ testFileParse("ParserTest37"); }
    SECTION("Tripple Tag"){ testFileParse("ParserTest38"); }
    SECTION("Component Import"){ testFileParse("ParserTest39"); }

    SECTION("Getter Setter"){ testFileParse("ParserTest40"); }
    SECTION("Js Sections"){ testFileParse("ParserTest41"); }
    SECTION("Events Methods and Listeners"){ testFileParse("ParserTest42"); }
    SECTION("Property Assignment in Binding"){ testFileParse("ParserTest43"); }
    SECTION("Instance Ids"){ testFileParse("ParserTest44"); }
    SECTION("Binding Chain"){ testFileParse("ParserTest45"); }
    SECTION("Creation In Method"){ testFileParse("ParserTest46"); }
    SECTION("Property Equals vs Property Binding"){ testFileParse("ParserTest47"); }
    SECTION("Property Notation"){ testFileParse("ParserTest48"); }
    SECTION("Anonymous Components"){ testFileParse("ParserTest49"); }
    SECTION("Additional Declarations"){ testFileParse("ParserTest50"); }
    SECTION("Package Scope Import"){ testFileParse("ParserTest51"); }
    SECTION("Comments Preservation"){ testFileParse("ParserTest52"); }

    SECTION("Function & Variables Type Test"){ testFileParse("ParserTypeTest01"); }
    SECTION("TS Declarations Test"){ testFileParse("ParserTest53"); }
    SECTION("Root Functions"){ testFileParse("ParserTest54"); }
    SECTION("Meta Root Functions"){ testFileParse("ParserTest55"); }
    SECTION("Flow Literals"){ testFlowFileParse("ParserTest56"); }
    SECTION("Meta Root Functions With Type Resolution"){
        FileIO fileIO;
        static std::string sourceDataPath = Path::join(Path::parent(__FILE__), "data");
        const std::string name = "ParserTest57";
        const std::string sourcePath = Path::join(sourceDataPath, name + ".lv");
        const std::string contents = fileIO.readFromFile(sourcePath);
        const std::string expectedJs = fileIO.readFromFile(Path::join(sourceDataPath, name + ".lv.js"));
        const std::string expectedDts = fileIO.readFromFile(Path::join(sourceDataPath, name + ".lv.d.ts"));

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);
        config.setTypesModule("@liveelements/types");

        StubTypeResolver resolver;
        resolver.responses[sourcePath + "::add"] = TypeResolver::Result{
            sourcePath + "::add",
            "FunctionType.for(Type.number, Type.number, Type.number)",
            {"FunctionType", "Type"}
        };
        resolver.responses[sourcePath + "::zero"] = TypeResolver::Result{
            sourcePath + "::zero",
            "FunctionType.for(Type.number)",
            {"FunctionType", "Type"}
        };
        resolver.responses[sourcePath + "::load"] = TypeResolver::Result{
            sourcePath + "::load",
            "FunctionType.for(Type.string, Type.string)",
            {"FunctionType", "Type"}
        };

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);
        REQUIRE(normalizeSpaces(targetResult.js) == normalizeSpaces(expectedJs));
        REQUIRE(normalizeSpaces(targetResult.dts) == normalizeSpaces(expectedDts));
    }

}

TEST_CASE("Type Resolution Helpers", "[TypeResolution]"){
    SECTION("Finalize substitutes markers and injects imports"){
        StubTypeResolver resolver;
        resolver.responses["req"] = TypeResolver::Result{
            "req",
            "Type.number",
            {"Type"}
        };

        TypeRequestSink sink;
        sink.setResolver(&resolver);
        sink.recordTypeRequest(
            TypeResolver::Request{"req", "/tmp/types.d.ts", "fn"},
            "output.js",
            "/*@@LVTYPE:x@@*/undefined"
        );
        sink.storeGeneratedTypeOutput(
            "output.js",
            "",
            "import {Base} from './base'\nreturn /*@@LVTYPE:x@@*/undefined;",
            false
        );

        sink.finalize(nullptr, "@liveelements/types");

        const auto* output = sink.generatedOutput("output.js");
        REQUIRE(output != nullptr);
        REQUIRE(output->content.find("return Type.number;") != std::string::npos);
        REQUIRE(output->content.find("/*@@LVTYPE:") == std::string::npos);
        REQUIRE(output->content.find("import {Base} from './base'\n") != std::string::npos);
        REQUIRE(output->content.find("import {Type} from '@liveelements/types'\n") != std::string::npos);
    }

    SECTION("Finalize keeps content unchanged when marker is missing"){
        StubTypeResolver resolver;
        resolver.responses["req"] = TypeResolver::Result{
            "req",
            "Type.number",
            {"Type"}
        };

        TypeRequestSink sink;
        sink.setResolver(&resolver);
        sink.recordTypeRequest(
            TypeResolver::Request{"req", "/tmp/types.d.ts", "fn"},
            "output.js",
            "missing"
        );
        sink.storeGeneratedTypeOutput("output.js", "", "return 1;", false);

        sink.finalize(nullptr, "@liveelements/types");

        const auto* output = sink.generatedOutput("output.js");
        REQUIRE(output != nullptr);
        REQUIRE(output->content == "import {Type} from '@liveelements/types'\nreturn 1;");
    }

    SECTION("Finalize writes modified generated outputs through file IO"){
        StubTypeResolver resolver;
        resolver.responses["req"] = TypeResolver::Result{
            "req",
            "Type.number",
            {"Type"}
        };

        MemoryFileIO fileIO;
        TypeRequestSink sink;
        sink.setResolver(&resolver);
        sink.recordTypeRequest(
            TypeResolver::Request{"req", "/tmp/types.d.ts", "fn"},
            "output.js",
            "/*@@LVTYPE:x@@*/undefined"
        );
        sink.storeGeneratedTypeOutput("output.js", "/tmp/output.js", "return /*@@LVTYPE:x@@*/undefined;", true);

        sink.finalize(&fileIO, "@liveelements/types");

        auto written = fileIO.files.find("/tmp/output.js");
        REQUIRE(written != fileIO.files.end());
        REQUIRE(written->second == "import {Type} from '@liveelements/types'\nreturn Type.number;");
    }

    SECTION("Collects requests and keeps plain meta unchanged without resolver"){
        const std::string sourcePath = "/tmp/TypeResolution.lv";
        const std::string contents =
            "@fn add(a:int, b:int): int { return a + b }\n"
            "@fn zero(): int { return 0 }\n"
            "fn plain(value:int): int { return value }\n";

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS);
        config.setTypesModule("@liveelements/types");

        StubTypeResolver resolver;
        resolver.responses[sourcePath + "::add"] = TypeResolver::Result{
            sourcePath + "::add",
            "FunctionType.for(Type.number, Type.number, Type.number)",
            {"FunctionType", "Type"}
        };
        resolver.responses[sourcePath + "::zero"] = TypeResolver::Result{
            sourcePath + "::zero",
            "FunctionType.for(Type.number)",
            {"FunctionType", "Type"}
        };

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(resolver.observedRequests.size() == 2);
        REQUIRE(resolver.observedRequests[0].requestId == sourcePath + "::add");
        REQUIRE(resolver.observedRequests[0].functionName == "add");
        REQUIRE(resolver.observedRequests[0].dtsPath == sourcePath + ".d.ts");
        REQUIRE(resolver.observedRequests[1].requestId == sourcePath + "::zero");
        REQUIRE(targetResult.js.find("FunctionType.for(Type.number, Type.number, Type.number)") != std::string::npos);
        REQUIRE(targetResult.js.find("FunctionType.for(Type.number)") != std::string::npos);
        REQUIRE(targetResult.js.find("/*@@LVTYPE:") == std::string::npos);
        REQUIRE(targetResult.js.find("plain.Meta") == std::string::npos);

        resolver.observedRequests.clear();
        Compiler::TargetResult secondRunResult = compiler->compileToTarget(sourcePath, contents);
        REQUIRE(resolver.observedRequests.size() == 2);
        REQUIRE(secondRunResult.js.find("FunctionType.for(Type.number, Type.number, Type.number)") != std::string::npos);
        REQUIRE(secondRunResult.js.find("FunctionType.for(Type.number)") != std::string::npos);
        REQUIRE(secondRunResult.js.find("/*@@LVTYPE:") == std::string::npos);

        Compiler::Ptr withoutResolver = Compiler::create(config);
        Compiler::TargetResult fallbackResult = withoutResolver->compileToTarget(sourcePath, contents);
        REQUIRE(fallbackResult.js.find("get type(){") == std::string::npos);
        REQUIRE(fallbackResult.js.find("/*@@LVTYPE:") == std::string::npos);
    }

    SECTION("Single-file output writes resolved JS through finalize"){
        const std::string sourcePath = "/tmp/TypeResolutionWrite.lv";
        const std::string outputPath = sourcePath + ".js";
        const std::string contents = "@fn add(a:int, b:int): int { return a + b }\n";

        MemoryFileIO fileIO;
        Compiler::Config config(true, ".js", &fileIO);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS);
        config.setTypesModule("@liveelements/types");

        StubTypeResolver resolver;
        resolver.responses[sourcePath + "::add"] = TypeResolver::Result{
            sourcePath + "::add",
            "FunctionType.for(Type.number, Type.number, Type.number)",
            {"FunctionType", "Type"}
        };

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        Compiler::TargetResult result = compiler->compileToTarget(sourcePath, contents);

        auto written = fileIO.files.find(outputPath);
        REQUIRE(written != fileIO.files.end());
        REQUIRE(written->second.find("FunctionType.for(Type.number, Type.number, Type.number)") != std::string::npos);
        REQUIRE(written->second.find("/*@@LVTYPE:") == std::string::npos);
        REQUIRE(normalizeSpaces(written->second) == normalizeSpaces(result.js));
    }

    SECTION("Single-file output does not write marker JS when resolver throws"){
        const std::string sourcePath = "/tmp/TypeResolutionThrow.lv";
        const std::string outputPath = sourcePath + ".js";
        const std::string contents = "@fn add(a:int, b:int): int { return a + b }\n";

        MemoryFileIO fileIO;
        Compiler::Config config(true, ".js", &fileIO);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS);
        config.setTypesModule("@liveelements/types");

        ThrowingTypeResolver resolver;

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        REQUIRE_THROWS_AS(compiler->compileToTarget(sourcePath, contents), lv::Exception);
        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(resolver.observedRequests[0].requestId == sourcePath + "::add");
        REQUIRE(fileIO.files.find(outputPath) == fileIO.files.end());
    }
}

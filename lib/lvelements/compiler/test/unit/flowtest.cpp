#include "catch_library.h"
#include "testsupport.h"
#include "live/utf8.h"
#include "live/fileio.h"
#include "live/path.h"
#include "live/txtar.h"
#include "live/mlnodetojson.h"
#include "live/applicationcontext.h"

#include "live/elements/compiler/compiler.h"
#include "live/elements/compiler/languagedescriptors.h"
#include "live/elements/compiler/typeresolver.h"
#include <array>
#include <cctype>

using namespace lv;
using namespace lv::el;

namespace {

class StubTypeResolver : public TypeResolver {
public:
    std::map<std::string, Result> responses;
    std::vector<Request> observedRequests;

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        std::vector<Result> result;
        for ( const Request& request : requests ){
            auto it = responses.find(request.requestId());
            if ( it != responses.end() ){
                result.push_back(it->second);
            }
        }
        return result;
    }
};

class AllAnyFlowTypeResolver : public TypeResolver {
public:
    std::vector<Request> observedRequests;

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        std::vector<Result> results;
        for ( const Request& request : requests ){
            results.push_back(TypeResolver::Result(
                request.requestId(),
                "FunctionType.for(Type.any)",
                {"FunctionType", "Type"}
            ));
        }
        return results;
    }
};

class DiagnosticFlowTypeResolver : public TypeResolver {
public:
    std::vector<Request> observedRequests;

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        std::vector<Result> results;
        for ( const Request& request : requests ){
            results.push_back(TypeResolver::Result(
                request.requestId(),
                "",
                {},
                {Diagnostic(Diagnostic::Severity::Error, "unresolved-identifier", "Cannot find name 'Context'")}
            ));
        }
        return results;
    }
};

std::string runtimeTypeForAnnotation(const std::string& annotation){
    std::string normalized;
    normalized.reserve(annotation.size());
    for ( char ch : annotation ){
        if ( !std::isspace(static_cast<unsigned char>(ch)) ){
            normalized.push_back(ch);
        }
    }
    static const std::array<std::pair<const char*, const char*>, 5> annotationTypes = {{
        {"number", "Type.number"},
        {"string", "Type.string"},
        {"bool", "Type.boolean"},
        {"boolean", "Type.boolean"},
        {"void", "Type.void"}
    }};

    for ( const auto& entry : annotationTypes ){
        if ( normalized == entry.first ){
            return entry.second;
        }
    }
    return "Type.any";
}

class FlowFunctionTypeResolver : public TypeResolver {
public:
    std::vector<Request> observedRequests;

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        std::vector<Result> results;
        for ( const Request& request : requests ){
            size_t signatureStart = request.sourceText().find("declare function " + request.functionName() + "(");
            if ( signatureStart == std::string::npos ){
                continue;
            }
            signatureStart = request.sourceText().find('(', signatureStart);
            size_t signatureClose = request.sourceText().find(')', signatureStart);
            size_t returnMarker = request.sourceText().find(':', signatureClose);
            size_t signatureEnd = request.sourceText().find(';', returnMarker);
            if ( signatureStart == std::string::npos || signatureClose == std::string::npos || returnMarker == std::string::npos || signatureEnd == std::string::npos ){
                continue;
            }

            std::vector<std::string> runtimeTypes;
            runtimeTypes.push_back(
                runtimeTypeForAnnotation(
                    [&](){
                        std::string annotation = request.sourceText().substr(returnMarker + 1, signatureEnd - returnMarker - 1);
                        Utf8::trim(annotation);
                        return annotation;
                    }()
                )
            );

            std::string signature = request.sourceText().substr(signatureStart + 1, signatureClose - signatureStart - 1);
            Utf8::trim(signature);
            if ( !signature.empty() ){
                size_t cursor = 0;
                while ( cursor < signature.size() ){
                    size_t comma = signature.find(',', cursor);
                    std::string parameter = signature.substr(
                        cursor,
                        comma == std::string::npos ? std::string::npos : comma - cursor
                    );
                    Utf8::trim(parameter);
                    size_t colon = parameter.rfind(':');
                    std::string parameterAnnotation = colon == std::string::npos
                        ? std::string()
                        : parameter.substr(colon + 1);
                    Utf8::trim(parameterAnnotation);
                    runtimeTypes.push_back(
                        colon == std::string::npos
                            ? "Type.any"
                            : runtimeTypeForAnnotation(parameterAnnotation)
                    );
                    if ( comma == std::string::npos ){
                        break;
                    }
                    cursor = comma + 1;
                }
            }

            std::string expression = "FunctionType.for(";
            for ( size_t i = 0; i < runtimeTypes.size(); ++i ){
                if ( i != 0 ){
                    expression += ", ";
                }
                expression += runtimeTypes[i];
            }
            expression += ")";

            results.push_back(TypeResolver::Result(
                request.requestId(),
                expression,
                {"FunctionType", "Type"}
            ));
        }
        return results;
    }
};

std::string flowTestDataPath(){
    return Path::join(Path::parent(lv::ApplicationContext::instance().applicationFilePath()), "data");
}

Compiler::Config flowConfig(Compiler::Config::OutputTarget outputTarget){
    Compiler::Config config(false);
    config.allowUnresolvedTypes(true);
    config.outputTarget(outputTarget);
    config.setFlowModule("flow/flow.mjs", "Flow");
    config.setFlowCall(Compiler::Config::FlowCall::Create, "Flow.create");
    config.setFlowCall(Compiler::Config::FlowCall::Call, "Flow.call");
    config.setFlowCall(Compiler::Config::FlowCall::Return, "Flow.ret");
    config.setFlowCall(Compiler::Config::FlowCall::Ref, "Flow.ref");
    config.setFlowCall(Compiler::Config::FlowCall::Member, "Flow.member");
    config.setFlowCall(Compiler::Config::FlowCall::Function, "Flow.fn");
    config.setTypesModule("types.mjs");
    return config;
}

void testFlowFileParse(const std::string& name){
    static FileIO fileIO;
    const std::string scriptPath = flowTestDataPath();
    const std::string sourcePath = Path::join(scriptPath, name + ".lv");
    const std::string contents = fileIO.readFromFile(sourcePath);

    auto runExpectation = [&](const std::string& suffix, const std::string& actual) {
        const std::string expectationPath = Path::join(scriptPath, name + ".lv" + suffix);
        const std::string expectedContent = fileIO.readFromFile(expectationPath);
        requireEquivalentElementsOutput(expectedContent, actual, name + ".lv" + suffix);
    };

    Compiler::Config jsConfig = flowConfig(Compiler::Config::OutputTarget::JS);
    Compiler::Ptr jsCompiler = Compiler::create(jsConfig);
    FlowFunctionTypeResolver jsResolver;
    jsCompiler->setTypeResolver(&jsResolver);
    jsCompiler->configureImplicitType("console");
    jsCompiler->configureImplicitType("vlog");
    Compiler::TargetResult jsResult = jsCompiler->compileToTarget(sourcePath, contents);
    runExpectation(".js", jsResult.js);

    Compiler::Config dtsConfig = flowConfig(Compiler::Config::OutputTarget::JS_DTS);
    Compiler::Ptr dtsCompiler = Compiler::create(dtsConfig);
    FlowFunctionTypeResolver dtsResolver;
    dtsCompiler->setTypeResolver(&dtsResolver);
    dtsCompiler->configureImplicitType("console");
    dtsCompiler->configureImplicitType("vlog");
    Compiler::TargetResult dtsResult = dtsCompiler->compileToTarget(sourcePath, contents);
    runExpectation(".d.ts", dtsResult.dts);
}

Compiler::TargetResult compileFlowFileWithoutResolver(
    const std::string& name,
    Compiler::Config::OutputTarget outputTarget = Compiler::Config::OutputTarget::JS_DTS
){
    static FileIO fileIO;
    const std::string scriptPath = flowTestDataPath();
    const std::string sourcePath = Path::join(scriptPath, name + ".lv");
    const std::string contents = fileIO.readFromFile(sourcePath);

    Compiler::Config config = flowConfig(outputTarget);
    Compiler::Ptr compiler = Compiler::create(config);
    compiler->configureImplicitType("console");
    compiler->configureImplicitType("vlog");

    return compiler->compileToTarget(sourcePath, contents);
}

void requireFlowExpectedOutput(const std::string& name, const std::string& suffix, const std::string& actual){
    static FileIO fileIO;
    const std::string scriptPath = flowTestDataPath();
    const std::string expectationPath = Path::join(scriptPath, name + ".lv" + suffix);
    const std::string expectedContent = fileIO.readFromFile(expectationPath);
    requireEquivalentElementsOutput(expectedContent, actual, name + ".lv" + suffix);
}

} // namespace

TEST_CASE("Flow Type Resolution", "[Flow]"){
    FileIO fileIO;
    const std::string dataPath = flowTestDataPath();

    SECTION("Flow literals default to null when no resolver is configured"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest01.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS);
        Compiler::Ptr compiler = Compiler::create(config);

        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(targetResult.js.find("Flow.create(['a', 'b'], undefined, [") != std::string::npos);
        REQUIRE(targetResult.js.find("/*@@LVTYPE:") == std::string::npos);
        REQUIRE(targetResult.js.find("types.mjs") == std::string::npos);
    }

    SECTION("Flow Literals"){ testFlowFileParse("FlowTest11"); }
    SECTION("Flow Literal Type Substitution"){ testFlowFileParse("FlowTest02"); }
    SECTION("Flow Literal Return Type"){ testFlowFileParse("FlowTest03"); }
    SECTION("Flow explicit import reuse"){ testFlowFileParse("FlowTest04"); }
    SECTION("Flow tap shadowing keeps module-level callees and flow values"){ testFlowFileParse("FlowTest05"); }

    SECTION("Flow literals request function signatures through the type resolver"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest02.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string firstFlowSource = "(a: number, b: number) %> add(a, b)";
        const std::string secondFlowSource = "item: string %> item";
        const size_t firstOffset = contents.find(firstFlowSource);
        const size_t secondOffset = contents.find(secondFlowSource);
        REQUIRE(firstOffset != std::string::npos);
        REQUIRE(secondOffset != std::string::npos);

        const std::string firstKey = std::to_string(firstOffset);
        const std::string secondKey = std::to_string(secondOffset);
        const std::string firstRequestId = sourcePath + "::flow@" + firstKey;
        const std::string secondRequestId = sourcePath + "::flow@" + secondKey;

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS_DTS);

        StubTypeResolver resolver;
        resolver.responses[firstRequestId] = TypeResolver::Result(
            firstRequestId,
            "FunctionType.for(Type.any, Type.number, Type.number)",
            {"FunctionType", "Type"}
        );
        resolver.responses[secondRequestId] = TypeResolver::Result(
            secondRequestId,
            "FunctionType.for(Type.any, Type.string)",
            {"FunctionType", "Type"}
        );

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->compileToTarget(sourcePath, contents);

        REQUIRE(resolver.observedRequests.size() == 2);
        REQUIRE(resolver.observedRequests[0].requestId() == firstRequestId);
        REQUIRE(resolver.observedRequests[0].functionName() == "__lvflow_" + firstKey);
        REQUIRE(resolver.observedRequests[0].sourcePath() == Path::join(Path::parent(sourcePath), "FlowTest02.__lvflow.ts"));
        REQUIRE(resolver.observedRequests[0].sourceText().find("import {add} from './math.mjs'") != std::string::npos);
        REQUIRE(resolver.observedRequests[0].sourceText().find("import {Flow} from 'flow/flow.mjs'") != std::string::npos);
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + firstKey + "(a: number, b: number): any;") != std::string::npos);
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + secondKey + "(item: string): any;") != std::string::npos);
        REQUIRE(resolver.observedRequests[1].sourcePath() == resolver.observedRequests[0].sourcePath());
        REQUIRE(resolver.observedRequests[1].sourceText() == resolver.observedRequests[0].sourceText());
    }

    SECTION("Flow literals preserve declared return types in shadow signatures"){
        // The resolved output is covered by the "Flow Literal Return Type"
        // parse test against the FlowTest03 fixtures. This section asserts only
        // that the declared return type survives into the shadow signature.
        const std::string sourcePath = Path::join(dataPath, "FlowTest03.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string flowSource = "(a: number, b: number): number %> add(a, b)";
        const size_t flowOffset = contents.find(flowSource);
        REQUIRE(flowOffset != std::string::npos);

        const std::string requestId = sourcePath + "::flow@" + std::to_string(flowOffset);

        StubTypeResolver resolver;
        resolver.responses[requestId] = TypeResolver::Result(
            requestId,
            "FunctionType.for(Type.number, Type.number, Type.number)",
            {"FunctionType", "Type"}
        );

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS);
        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        compiler->compileToTarget(sourcePath, contents);

        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + std::to_string(flowOffset) + "(a: number, b: number): number;") != std::string::npos);
    }

    SECTION("Flow shadow declarations inherit enclosing generic type parameters"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest12.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string flowSource = "a: X %> a";
        const size_t flowOffset = contents.find(flowSource);
        REQUIRE(flowOffset != std::string::npos);

        const std::string requestId = sourcePath + "::flow@" + std::to_string(flowOffset);

        StubTypeResolver resolver;
        resolver.responses[requestId] = TypeResolver::Result(
            requestId,
            "FunctionType.for(TypeVariable.for('X'), TypeVariable.for('X')).withTypeParameters(['X'])",
            {"FunctionType", "Type", "TypeVariable"}
        );

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS_DTS);
        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        compiler->compileToTarget(sourcePath, contents);

        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(resolver.observedRequests[0].requestId() == requestId);
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + std::to_string(flowOffset) + "<X>(a: X): any;") != std::string::npos);
    }

    SECTION("Flow type annotations prefer nearer same-file type declarations over outer type parameters"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest14.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS_DTS);
        Compiler::Ptr compiler = Compiler::create(config);

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for a same-file flow type shadowing a type parameter.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("'X' is declared in this file; a flow can only use imported types") != std::string::npos);
        }
    }

    SECTION("Nested flow literals accumulate distinct requests"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest11.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string outerFlowSource = "(a: UserContext, b: List<number>) %> {";
        const std::string innerFlowSource = "x: number %> add(x, 2)";
        const size_t outerOffset = contents.find(outerFlowSource);
        const size_t innerOffset = contents.find(innerFlowSource);
        REQUIRE(outerOffset != std::string::npos);
        REQUIRE(innerOffset != std::string::npos);

        AllAnyFlowTypeResolver resolver;

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS);
        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(resolver.observedRequests.size() >= 2);
        bool sawOuter = false;
        bool sawInner = false;
        for ( const TypeResolver::Request& request : resolver.observedRequests ){
            if ( request.requestId() == sourcePath + "::flow@" + std::to_string(outerOffset) ){
                sawOuter = true;
                REQUIRE(request.functionName() == "__lvflow_" + std::to_string(outerOffset));
            }
            if ( request.requestId() == sourcePath + "::flow@" + std::to_string(innerOffset) ){
                sawInner = true;
                REQUIRE(request.functionName() == "__lvflow_" + std::to_string(innerOffset));
            }
        }
        REQUIRE(sawOuter);
        REQUIRE(sawInner);
        REQUIRE(targetResult.js.find("FunctionType.for(Type.any)") != std::string::npos);
        REQUIRE(targetResult.js.find("/*@@LVTYPE:") == std::string::npos);
    }

    SECTION("Flow resolver diagnostics surface as located syntax exceptions"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest11.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string flowSource = "n: number %> add(n, 3)";
        const size_t flowOffset = contents.find(flowSource);
        REQUIRE(flowOffset != std::string::npos);

        DiagnosticFlowTypeResolver resolver;

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS);
        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->configureImplicitType("console");
        compiler->configureImplicitType("vlog");

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException from resolver diagnostics.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("Cannot find name 'Context'") != std::string::npos);
            REQUIRE(e.code() == Exception::toCode("unresolved-identifier"));
            REQUIRE(e.parsedLocation().filePath() == sourcePath);
            REQUIRE(e.parsedLocation().range().start().offset() == static_cast<int>(flowOffset));
            REQUIRE(e.parsedLocation().range().start().line() == 4);
        }
    }

    SECTION("Flow.create resolves when flow syntax is present"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest06");

        requireFlowExpectedOutput("FlowTest06", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest06", ".d.ts", targetResult.dts);

        REQUIRE(targetResult.js.find("const explicit = Flow.create('n', undefined, [Flow.ret(Flow.ref('n'))])") != std::string::npos);
        const size_t firstFlowImport = targetResult.js.find("import {Flow} from 'flow/flow.mjs'");
        REQUIRE(firstFlowImport != std::string::npos);
        REQUIRE(targetResult.js.find("import {Flow} from 'flow/flow.mjs'", firstFlowImport + 1) == std::string::npos);
    }

    SECTION("Flow.create does not trigger auto-imports without flow syntax"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest07");

        requireFlowExpectedOutput("FlowTest07", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest07", ".d.ts", targetResult.dts);

        REQUIRE(targetResult.js.find("return Flow.create('n', undefined, [Flow.ret(Flow.ref('n'))])") != std::string::npos);
        REQUIRE(targetResult.js.find("import {Flow} from 'flow/flow.mjs'") == std::string::npos);
    }

    SECTION("Flow allows module-level dotted callees"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest08");

        requireFlowExpectedOutput("FlowTest08", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest08", ".d.ts", targetResult.dts);

        REQUIRE(targetResult.js.find("Flow.call(Math.max, [Flow.ref('n'), 2], 'next')") != std::string::npos);
    }

    SECTION("Flow allows same-file top-level functions"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest09");

        requireFlowExpectedOutput("FlowTest09", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest09", ".d.ts", targetResult.dts);

        REQUIRE(targetResult.js.find("Flow.call(add, [Flow.ref('n'), 2])") != std::string::npos);
    }

    SECTION("Flow allows namespaced callees from module imports"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest10");

        requireFlowExpectedOutput("FlowTest10", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest10", ".d.ts", targetResult.dts);

        REQUIRE(targetResult.js.find("import {max as __mod__max} from '__UNRESOLVED__'") != std::string::npos);
        REQUIRE(targetResult.js.find("let mod = {max:__mod__max}") != std::string::npos);
        REQUIRE(targetResult.js.find("Flow.call(mod.max, [Flow.ref('n'), 2], 'next')") != std::string::npos);
    }

    SECTION("Flow allows callees from module imports, sibling files and same-file functions"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-flow-module-callees");
        Txtar archive = Txtar::parseFile(Path::join(dataPath, "FlowTestModulePack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkg");
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS";
        config.initialize(options);
        config.allowUnresolvedTypes(false);
        config.setFlowModule("flow/flow.mjs", "Flow");
        config.setFlowCall(Compiler::Config::FlowCall::Create, "Flow.create");
        config.setFlowCall(Compiler::Config::FlowCall::Call, "Flow.call");
        config.setFlowCall(Compiler::Config::FlowCall::Return, "Flow.ret");
        config.setFlowCall(Compiler::Config::FlowCall::Ref, "Flow.ref");
        config.setFlowCall(Compiler::Config::FlowCall::Member, "Flow.member");

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setPackageImportPaths({Path::join(packageDir, "node_modules")});

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, Path::join(packageDir, "app")));

        const std::string js = fileIO.readFromFile(Path::join(packageDir, "build/app/main.lv.mjs"));
        const std::string expected = fileIO.readFromFile(Path::join(tmpRoot, "expected/app/main.lv.mjs"));
        requireEquivalentElementsOutput(expected, js, "FlowTestModulePack01.txtar: expected/app/main.lv.mjs");
    }

    SECTION("Flow rejects implicit globals as callees and leaves other unknown callees unresolved"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest15.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Ptr compiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS_DTS));
        compiler->configureImplicitType("Date");
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        requireFlowExpectedOutput("FlowTest15", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest15", ".d.ts", targetResult.dts);

        Compiler::Ptr globalsCompiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS_DTS));
        globalsCompiler->configureImplicitType("setTimeout");
        try {
            globalsCompiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for the global 'setTimeout' callee.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("'setTimeout' is a global; a flow can only call imported names or functions declared in this module") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 6);
            REQUIRE(e.parsedLocation().range().start().column() == 9);
        }
    }

    SECTION("Flow type annotations reject same-file types"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest13.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config = flowConfig(Compiler::Config::OutputTarget::JS_DTS);
        Compiler::Ptr compiler = Compiler::create(config);

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for a same-file flow type.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("'Local' is declared in this file; a flow can only use imported types") != std::string::npos);
        }
    }

    SECTION("Flow functions lower to Flow.fn with a Meta object that reuses the flow's type"){
        testFlowFileParse("FlowTest16");
    }

    SECTION("Flow functions without a resolver pass an undefined type and no Meta type"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest16");

        REQUIRE(targetResult.js.find("export const two = Flow.fn('two', Flow.create(['a', 'b'], undefined, [") != std::string::npos);
        REQUIRE(targetResult.js.find("get parameterNames(){ return ['a', 'b'] }") != std::string::npos);
        REQUIRE(targetResult.js.find("get type()") == std::string::npos);
    }

    SECTION("Flow function shadow signatures keep the declared result type"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest16.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const size_t noneOffset = contents.find("@fn none()");
        const size_t twoOffset = contents.find("@fn two(");
        REQUIRE(noneOffset != std::string::npos);
        REQUIRE(twoOffset != std::string::npos);

        StubTypeResolver resolver;
        Compiler::Ptr compiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS));
        compiler->setTypeResolver(&resolver);
        try {
            compiler->compileToTarget(sourcePath, contents);
        } catch ( const lv::Exception& ){
            // the stub returns no results, only the requests are checked
        }

        REQUIRE(resolver.observedRequests.size() == 3);
        REQUIRE(resolver.observedRequests[0].requestId() == sourcePath + "::flow@" + std::to_string(noneOffset));
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + std::to_string(noneOffset) + "(): any;") != std::string::npos);
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + std::to_string(twoOffset) + "(a: number, b: number): number;") != std::string::npos);
        REQUIRE(resolver.observedRequests[2].functionName() == "__lvflow_" + std::to_string(twoOffset));
    }

    SECTION("Flow functions allow the previous reference on the first line only with one parameter"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest17.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Ptr compiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS_DTS));

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for '%' on the first line of a two parameter flow.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("The '%' reference can only be used on the first line when the flow has exactly one parameter, or on later lines.") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 4);
            REQUIRE(e.parsedLocation().range().start().column() == 9);
        }
    }

    SECTION("Flow functions reject global callees"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest18.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Ptr compiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS_DTS));
        compiler->configureImplicitType("setTimeout");

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for a global flow callee.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("'setTimeout' is a global; a flow can only call imported names or functions declared in this module") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 2);
            REQUIRE(e.parsedLocation().range().start().column() == 5);
        }
    }

    SECTION("Flow calls without parentheses"){ testFlowFileParse("FlowTest19"); }

    SECTION("Flow calls without parentheses mix with classic calls"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest20");

        requireFlowExpectedOutput("FlowTest20", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest20", ".d.ts", targetResult.dts);

        REQUIRE(targetResult.js.find("Flow.call(mod.max, [Flow.ref('a'), Flow.ref('b')], 'm')") != std::string::npos);
        REQUIRE(targetResult.js.find("Flow.ret(Flow.call(add, [Flow.ref('c'), 1]))") != std::string::npos);
    }

    SECTION("Flow classic calls lower like calls without parentheses"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest21");

        requireFlowExpectedOutput("FlowTest20", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest20", ".d.ts", targetResult.dts);
    }

    SECTION("Flow lambda arguments without parentheses"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest22");

        requireFlowExpectedOutput("FlowTest22", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest22", ".d.ts", targetResult.dts);

        REQUIRE(targetResult.js.find("Flow.call(map, [Flow.ref('xs'), Flow.create('x', undefined, [Flow.call(add, [Flow.ref('x'), 1])])], 'a')") != std::string::npos);
        REQUIRE(targetResult.js.find("Flow.call(map, [Flow.ref('xs'), Flow.create('x', undefined, [Flow.ret(Flow.call(add, [Flow.ref('x'), 1]))])], 'b')") != std::string::npos);
    }

    SECTION("Flow classic lambda arguments lower like lambda arguments without parentheses"){
        Compiler::TargetResult targetResult = compileFlowFileWithoutResolver("FlowTest23");

        requireFlowExpectedOutput("FlowTest22", ".js", targetResult.js);
        requireFlowExpectedOutput("FlowTest22", ".d.ts", targetResult.dts);
    }

    SECTION("Flow rejects a bare reference step after a call"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest24.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Ptr compiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS_DTS));

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for a step that is not a function call.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("Flow steps must be function calls.") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 4);
            REQUIRE(e.parsedLocation().range().start().column() == 10);
        }
    }

    SECTION("Flow rejects a zero-argument call without parentheses"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest25.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Ptr compiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS_DTS));

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for a step that is not a function call.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("Flow steps must be function calls.") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 4);
            REQUIRE(e.parsedLocation().range().start().column() == 5);
        }
    }

    SECTION("Flow rejects an unbraced lambda argument without parentheses"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest26.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Ptr compiler = Compiler::create(flowConfig(Compiler::Config::OutputTarget::JS_DTS));

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a syntax error for a lambda without braces passed without parentheses.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.code() == Exception::toCode("~Language"));
            REQUIRE(e.message().find("Syntax error") != std::string::npos);
            REQUIRE(e.parsedLocation().range().start().line() == 4);
            REQUIRE(e.parsedLocation().range().start().column() == 12);
        }
    }

    SECTION("Flow functions are module exports callable from sibling files"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-flow-functions-module");
        Txtar archive = Txtar::parseFile(Path::join(dataPath, "FlowTestModulePack02.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkg");
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS";
        config.initialize(options);
        config.allowUnresolvedTypes(false);
        config.setFlowModule("flow/flow.mjs", "Flow");
        config.setFlowCall(Compiler::Config::FlowCall::Create, "Flow.create");
        config.setFlowCall(Compiler::Config::FlowCall::Call, "Flow.call");
        config.setFlowCall(Compiler::Config::FlowCall::Return, "Flow.ret");
        config.setFlowCall(Compiler::Config::FlowCall::Ref, "Flow.ref");
        config.setFlowCall(Compiler::Config::FlowCall::Member, "Flow.member");
        config.setFlowCall(Compiler::Config::FlowCall::Function, "Flow.fn");

        Compiler::Ptr compiler = Compiler::create(config);

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, Path::join(packageDir, "app")));

        const std::string js = fileIO.readFromFile(Path::join(packageDir, "build/app/main.lv.mjs"));
        REQUIRE(js.find("import {helper} from './helpers.lv.mjs'") != std::string::npos);
        REQUIRE(js.find("export const main = Flow.fn('main', Flow.create('n', undefined, [Flow.call(helper, [Flow.ref('n')], 'h'), Flow.ret(Flow.ref('h'))]), {") != std::string::npos);
        REQUIRE(js.find("get sourceFileName(){ return 'main.lv' }") != std::string::npos);
        REQUIRE(js.find("get parameterNames(){ return ['n'] }") != std::string::npos);

        const std::string descriptorPath = Path::join(packageDir, "build/app", ModuleDescriptor::buildFileName);
        MLNode descriptor;
        ml::fromJson(fileIO.readFromFile(descriptorPath), descriptor);

        std::map<std::string, MLNode> exports;
        for ( const MLNode& file : descriptor["exports"].asArray() ){
            for ( const MLNode& expt : file["exports"].asArray() ){
                exports[expt["name"].asString()] = expt;
            }
        }
        REQUIRE(exports["helper"]["kind"].asString() == "flow");
        REQUIRE(exports["helper"]["meta"].asBool());
        REQUIRE(exports["main"]["kind"].asString() == "flow");
        REQUIRE(exports["double"]["kind"].asString() == "function");
    }

    SECTION("Flow export descriptors round trip"){
        ModuleFileDescriptor::Ptr descriptor = ModuleFileDescriptor::create("main.lv");
        descriptor->addExport(ExportDescriptor("main", ExportDescriptor::Flow, true));
        descriptor->addExport(ExportDescriptor("add", ExportDescriptor::Function));

        ModuleFileDescriptor::Ptr roundTrip = ModuleFileDescriptor::createFromMLNode(descriptor->toMLNode());
        REQUIRE(roundTrip->exports().size() == 2);
        REQUIRE(roundTrip->exports()[0].kind() == ExportDescriptor::Flow);
        REQUIRE(roundTrip->exports()[0].kindString() == "flow");
        REQUIRE(roundTrip->exports()[0].meta());
        REQUIRE(roundTrip->exports()[1].kind() == ExportDescriptor::Function);
    }

}

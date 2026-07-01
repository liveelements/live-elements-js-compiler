#include "catch_library.h"
#include "testsupport.h"
#include "live/utf8.h"
#include "live/fileio.h"
#include "live/path.h"
#include "live/applicationcontext.h"

#include "live/elements/compiler/compiler.h"
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
    static const std::array<std::pair<const char*, const char*>, 6> annotationTypes = {{
        {"int", "Type.number"},
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

    SECTION("Flow Literals"){ testFlowFileParse("ParserTest56"); }
    SECTION("Flow Literal Type Substitution"){ testFlowFileParse("FlowTest02"); }
    SECTION("Flow Literal Return Type"){ testFlowFileParse("FlowTest03"); }

    SECTION("Flow literals request function signatures through the type resolver"){
        const std::string sourcePath = Path::join(dataPath, "FlowTest02.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string firstFlowSource = "(a: int, b: int) %> add(a, b)";
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
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + firstKey + "(a: int, b: int): any;") != std::string::npos);
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

        const std::string flowSource = "(a: int, b: int): int %> add(a, b)";
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
        REQUIRE(resolver.observedRequests[0].sourceText().find("declare function __lvflow_" + std::to_string(flowOffset) + "(a: int, b: int): int;") != std::string::npos);
    }

    SECTION("Nested flow literals accumulate distinct requests"){
        const std::string sourcePath = Path::join(dataPath, "ParserTest56.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string outerFlowSource = "(a: UserContext, b: List<int>) %> {";
        const std::string innerFlowSource = "x: int %> add(x, 2)";
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
        const std::string sourcePath = Path::join(dataPath, "ParserTest56.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        const std::string flowSource = "n: int %> add(n, 3)";
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
}

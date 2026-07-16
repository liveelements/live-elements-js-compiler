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
#include "testsupport.h"
#include "live/applicationcontext.h"
#include "live/fileio.h"
#include "live/path.h"
#include "live/txtar.h"
#include "live/utf8.h"

#include "live/elements/compiler/compiler.h"
#include "live/elements/compiler/typerequestsink.h"
#include "live/elements/compiler/typeresolver.h"
#include <map>

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

class ThrowingTypeResolver : public TypeResolver {
public:
    std::vector<Request> observedRequests;
    std::string message = "Type resolver failed.";

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        throw lv::Exception(message, lv::Exception::toCode("~Type"));
    }
};

class DiskBackedTypeResolver : public TypeResolver {
public:
    std::map<std::string, Result> responses;
    std::vector<Request> observedRequests;
    std::map<std::string, std::string> sourceContentsAtResolve;

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedRequests = requests;
        FileIO fileIO;
        std::vector<Result> result;
        for ( const Request& request : requests ){
            if ( Path::exists(request.sourcePath()) ){
                sourceContentsAtResolve[request.sourcePath()] = fileIO.readFromFile(request.sourcePath());
            } else {
                throw lv::Exception(
                    Utf8("Missing resolver source file: %").format(request.sourcePath()),
                    lv::Exception::toCode("~Type")
                );
            }

            auto it = responses.find(request.requestId());
            if ( it != responses.end() ){
                result.push_back(it->second);
            } else {
                throw lv::Exception(
                    Utf8("Missing resolver response for request: %").format(request.requestId()),
                    lv::Exception::toCode("~Type")
                );
            }
        }
        return result;
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

std::string typeTestDataPath(){
    return Path::join(Path::parent(lv::ApplicationContext::instance().applicationFilePath()), "data");
}

} // namespace

TEST_CASE("Type Resolution", "[TypeResolution]"){
    SECTION("Finalize substitutes markers and injects imports"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest04.lv");
        const std::string expectedOutput = fileIO.readFromFile(Path::join(typeTestDataPath(), "TypeTest04.lv.js"));

        StubTypeResolver resolver;
        resolver.responses["req"] = TypeResolver::Result(
            "req",
            "Type.number",
            {"Type"}
        );

        TypeRequestSink sink;
        sink.setResolver(&resolver);
        TypeResolver::Request request("req", sourcePath + ".d.ts", "", "fn");
        sink.recordTypeRequest(
            request,
            "output.js",
            "/*@@LVTYPE:x@@*/undefined",
            SourceRangeLocation()
        );
        sink.storeGeneratedTypeOutput(
            "output.js",
            "",
            "import {Base} from './base'\nreturn /*@@LVTYPE:x@@*/undefined;",
            false
        );

        sink.finalize(nullptr, "types.mjs");

        const auto* output = sink.generatedOutput("output.js");
        REQUIRE(output != nullptr);
        std::string actual = output->content;
        std::string expected = expectedOutput;
        Utf8::trimRight(actual);
        Utf8::trimRight(expected);
        REQUIRE(actual == expected);
    }

    SECTION("Finalize keeps content unchanged when marker is missing"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest05.lv");
        const std::string expectedOutput = fileIO.readFromFile(Path::join(typeTestDataPath(), "TypeTest05.lv.js"));

        StubTypeResolver resolver;
        resolver.responses["req"] = TypeResolver::Result(
            "req",
            "Type.number",
            {"Type"}
        );

        TypeRequestSink sink;
        sink.setResolver(&resolver);
        TypeResolver::Request request("req", sourcePath + ".d.ts", "", "fn");
        sink.recordTypeRequest(
            request,
            "output.js",
            "missing",
            SourceRangeLocation()
        );
        sink.storeGeneratedTypeOutput("output.js", "", "return 1;", false);

        sink.finalize(nullptr, "types.mjs");

        const auto* output = sink.generatedOutput("output.js");
        REQUIRE(output != nullptr);
        std::string actual = output->content;
        std::string expected = expectedOutput;
        Utf8::trimRight(actual);
        Utf8::trimRight(expected);
        REQUIRE(actual == expected);
    }

    SECTION("Finalize writes modified generated outputs through file IO"){
        FileIO expectedFileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest06.lv");
        const std::string outputPath = sourcePath + ".js";
        const std::string expectedOutput = expectedFileIO.readFromFile(Path::join(typeTestDataPath(), "TypeTest06.lv.js"));

        StubTypeResolver resolver;
        resolver.responses["req"] = TypeResolver::Result(
            "req",
            "Type.number",
            {"Type"}
        );

        MemoryFileIO fileIO;
        TypeRequestSink sink;
        sink.setResolver(&resolver);
        TypeResolver::Request request("req", sourcePath + ".d.ts", "", "fn");
        sink.recordTypeRequest(
            request,
            "output.js",
            "/*@@LVTYPE:x@@*/undefined",
            SourceRangeLocation()
        );
        sink.storeGeneratedTypeOutput("output.js", outputPath, "return /*@@LVTYPE:x@@*/undefined;", true);

        sink.finalize(&fileIO, "types.mjs");

        auto written = fileIO.files.find(outputPath);
        REQUIRE(written != fileIO.files.end());
        std::string actual = written->second;
        std::string expected = expectedOutput;
        Utf8::trimRight(actual);
        Utf8::trimRight(expected);
        REQUIRE(actual == expected);
    }

    SECTION("Collects requests and keeps plain meta unchanged without resolver"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest01.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS);
        config.setTypesModule("types.mjs");

        StubTypeResolver resolver;
        resolver.responses[sourcePath + "::add"] = TypeResolver::Result(
            sourcePath + "::add",
            "FunctionType.for(Type.number, Type.number, Type.number)",
            {"FunctionType", "Type"}
        );
        resolver.responses[sourcePath + "::zero"] = TypeResolver::Result(
            sourcePath + "::zero",
            "FunctionType.for(Type.number)",
            {"FunctionType", "Type"}
        );

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(resolver.observedRequests.size() == 2);
        REQUIRE(resolver.observedRequests[0].requestId() == sourcePath + "::add");
        REQUIRE(resolver.observedRequests[0].functionName() == "add");
        REQUIRE(resolver.observedRequests[0].sourcePath() == sourcePath + ".d.ts");
        REQUIRE(resolver.observedRequests[1].requestId() == sourcePath + "::zero");
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
        FileIO expectedFileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest02.lv");
        const std::string outputPath = sourcePath + ".js";
        const std::string contents = expectedFileIO.readFromFile(sourcePath);
        const std::string expectedOutput = expectedFileIO.readFromFile(Path::join(typeTestDataPath(), "TypeTest02.lv.js"));

        MemoryFileIO fileIO;
        Compiler::Config config(true, ".js", &fileIO);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS);
        config.setTypesModule("types.mjs");

        StubTypeResolver resolver;
        resolver.responses[sourcePath + "::add"] = TypeResolver::Result(
            sourcePath + "::add",
            "FunctionType.for(Type.number, Type.number, Type.number)",
            {"FunctionType", "Type"}
        );

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        Compiler::TargetResult result = compiler->compileToTarget(sourcePath, contents);

        auto written = fileIO.files.find(outputPath);
        REQUIRE(written != fileIO.files.end());
        std::string writtenContent = written->second;
        std::string resultContent = result.js;
        std::string expected = expectedOutput;
        Utf8::trimRight(writtenContent);
        Utf8::trimRight(resultContent);
        Utf8::trimRight(expected);
        REQUIRE(writtenContent == expected);
        REQUIRE(resultContent == expected);
    }

    SECTION("Single-file output does not write marker JS when resolver throws"){
        FileIO fileIOReader;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest03.lv");
        const std::string outputPath = sourcePath + ".js";
        const std::string contents = fileIOReader.readFromFile(sourcePath);

        MemoryFileIO fileIO;
        Compiler::Config config(true, ".js", &fileIO);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS);
        config.setTypesModule("types.mjs");

        ThrowingTypeResolver resolver;

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        REQUIRE_THROWS_AS(compiler->compileToTarget(sourcePath, contents), lv::Exception);
        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(resolver.observedRequests[0].requestId() == sourcePath + "::add");
        REQUIRE(fileIO.files.find(outputPath) == fileIO.files.end());
    }

    SECTION("Meta Root Functions With Type Resolution"){
        FileIO fileIO;
        const std::string name = "TypeTest07";
        const std::string sourcePath = Path::join(typeTestDataPath(), name + ".lv");
        const std::string contents = fileIO.readFromFile(sourcePath);
        const std::string expectedJs = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.js"));
        const std::string expectedDts = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.d.ts"));

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);
        config.setTypesModule("types.mjs");

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
        requireEquivalentElementsOutput(expectedJs, targetResult.js, name + ".lv.js");
        requireEquivalentElementsOutput(expectedDts, targetResult.dts, name + ".lv.d.ts");
    }

    SECTION("Module compile writes dts before finalize type resolution"){
        FileIO fileIO;
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-dts-module-compile");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestPack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkg");
        const std::string moduleDir = Path::join(packageDir, "sample");
        const std::string sourcePath = Path::join(moduleDir, "main.lv");
        const std::string outputJsPath = sourcePath + ".mjs";
        const std::string outputDtsPath = sourcePath + ".d.ts";

        Compiler::Config config(true, ".mjs", &fileIO);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);
        config.setTypesModule("types.mjs");

        DiskBackedTypeResolver resolver;
        const std::string resolvedSourcePath = Path::resolve(sourcePath);
        resolver.responses[resolvedSourcePath + "::add"] = TypeResolver::Result{
            resolvedSourcePath + "::add",
            "FunctionType.for(Type.number, Type.number, Type.number)",
            {"FunctionType", "Type"}
        };

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        REQUIRE_NOTHROW(Compiler::compile(compiler, sourcePath));

        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(Path::resolve(resolver.observedRequests[0].sourcePath()) == Path::resolve(outputDtsPath));
        REQUIRE(resolver.observedRequests[0].functionName() == "add");
        REQUIRE(Path::exists(outputDtsPath));
        REQUIRE(Path::exists(outputJsPath));

        const std::string dtsContents = fileIO.readFromFile(outputDtsPath);
        REQUIRE(dtsContents.find("export declare function add(a: int, b: int): int;") != std::string::npos);
        REQUIRE(resolver.sourceContentsAtResolve[resolver.observedRequests[0].sourcePath()] == dtsContents);

        const std::string jsContents = fileIO.readFromFile(outputJsPath);
        REQUIRE(jsContents.find("FunctionType.for(Type.number, Type.number, Type.number)") != std::string::npos);
        REQUIRE(jsContents.find("/*@@LVTYPE:") == std::string::npos);
    }
}

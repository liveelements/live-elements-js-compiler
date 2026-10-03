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
#include "live/elements/compiler/compilation.h"
#include "live/elements/compiler/languagedescriptors.h"
#include "live/elements/compiler/typerequestsink.h"
#include "live/elements/compiler/typeresolver.h"
#include <map>
#include <set>

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

class CountingTypeResolver : public TypeResolver {
public:
    std::map<std::string, Result> responses;
    std::vector<std::vector<Request>> observedBatches;

    std::vector<Result> resolve(const std::vector<Request>& requests) override{
        observedBatches.push_back(requests);
        std::vector<Result> result;
        for ( const Request& request : requests ){
            auto it = responses.find(request.requestId());
            if ( it != responses.end() ){
                result.push_back(it->second);
            } else {
                result.push_back(Result(request.requestId()));
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

        FileIO fileIO;
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-dts-module-compile");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestPack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkg");
        const std::string moduleDir = Path::join(packageDir, "sample");
        const std::string sourcePath = Path::join(moduleDir, "main.lv");
        const std::string outputJsPath = sourcePath + ".mjs";
        const std::string outputDtsPath = sourcePath + ".d.mts";

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

        REQUIRE_NOTHROW(Compiler::compile(compiler, sourcePath)->module());

        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(Path::resolve(resolver.observedRequests[0].sourcePath()) == Path::resolve(outputDtsPath));
        REQUIRE(resolver.observedRequests[0].functionName() == "add");
        REQUIRE(Path::exists(outputDtsPath));
        REQUIRE(Path::exists(outputJsPath));
        REQUIRE_FALSE(Path::exists(sourcePath + ".d.ts"));

        const std::string dtsContents = fileIO.readFromFile(outputDtsPath);
        REQUIRE(dtsContents.find("export declare function add(a: number, b: number): number;") != std::string::npos);
        REQUIRE(resolver.sourceContentsAtResolve[resolver.observedRequests[0].sourcePath()] == dtsContents);

        const std::string jsContents = fileIO.readFromFile(outputJsPath);
        REQUIRE(jsContents.find("FunctionType.for(Type.number, Type.number, Type.number)") != std::string::npos);
        REQUIRE(jsContents.find("/*@@LVTYPE:") == std::string::npos);
    }

    SECTION("Module compile emits the same outputs with shared and separate compilers"){
        const std::string sharedTmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-module-cache-reuse-shared");
        const std::string separateTmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-module-cache-reuse-separate");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestModuleCompilePack01.txtar"));
        Txtar::ExtractionGuard sharedExtraction = archive.extractTemp(sharedTmpRoot);
        Txtar::ExtractionGuard separateExtraction = archive.extractTemp(separateTmpRoot);

        const std::string sharedPackageDir = Path::join(sharedTmpRoot, "pkg");
        const std::string sharedAlphaModuleDir = Path::join(sharedPackageDir, "alpha");
        const std::string sharedBetaModuleDir = Path::join(sharedPackageDir, "beta");
        FileIO sharedFileIO;
        Compiler::Config sharedConfig(true, ".mjs", &sharedFileIO);
        MLNode sharedOptions(MLNode::Type::Object);
        sharedOptions["importLocalPath"] = "node_modules";
        sharedOptions["packageBuildPath"] = "build";
        sharedOptions["outputTarget"] = "JS";
        sharedConfig.initialize(sharedOptions);

        Compiler::Ptr sharedCompiler = Compiler::create(sharedConfig);
        sharedCompiler->setPackageImportPaths({Path::join(sharedPackageDir, "node_modules")});

        REQUIRE_NOTHROW(Compiler::compileModule(sharedCompiler, sharedAlphaModuleDir));
        REQUIRE_NOTHROW(Compiler::compileModule(sharedCompiler, sharedBetaModuleDir));

        FileIO archiveFileIO;
        const std::string separatePackageDir = Path::join(separateTmpRoot, "pkg");
        const std::string separateAlphaModuleDir = Path::join(separatePackageDir, "alpha");
        const std::string separateBetaModuleDir = Path::join(separatePackageDir, "beta");

        Compiler::Config separateAlphaConfig(true, ".mjs", &archiveFileIO);
        MLNode separateAlphaOptions(MLNode::Type::Object);
        separateAlphaOptions["importLocalPath"] = "node_modules";
        separateAlphaOptions["packageBuildPath"] = "build";
        separateAlphaOptions["outputTarget"] = "JS";
        separateAlphaConfig.initialize(separateAlphaOptions);
        Compiler::Ptr alphaCompiler = Compiler::create(separateAlphaConfig);
        alphaCompiler->setPackageImportPaths({Path::join(separatePackageDir, "node_modules")});

        Compiler::Config separateBetaConfig(true, ".mjs", &archiveFileIO);
        MLNode separateBetaOptions(MLNode::Type::Object);
        separateBetaOptions["importLocalPath"] = "node_modules";
        separateBetaOptions["packageBuildPath"] = "build";
        separateBetaOptions["outputTarget"] = "JS";
        separateBetaConfig.initialize(separateBetaOptions);
        Compiler::Ptr betaCompiler = Compiler::create(separateBetaConfig);
        betaCompiler->setPackageImportPaths({Path::join(separatePackageDir, "node_modules")});

        REQUIRE_NOTHROW(Compiler::compileModule(alphaCompiler, separateAlphaModuleDir));
        REQUIRE_NOTHROW(Compiler::compileModule(betaCompiler, separateBetaModuleDir));

        const Txtar sharedAlphaBuild = Txtar::fromDirectory(Path::join(sharedPackageDir, "build/alpha"));
        const Txtar sharedBetaBuild = Txtar::fromDirectory(Path::join(sharedPackageDir, "build/beta"));
        const Txtar separateAlphaBuild = Txtar::fromDirectory(Path::join(separatePackageDir, "build/alpha"));
        const Txtar separateBetaBuild = Txtar::fromDirectory(Path::join(separatePackageDir, "build/beta"));

        REQUIRE(sharedAlphaBuild.toString() == separateAlphaBuild.toString());
        REQUIRE(sharedBetaBuild.toString() == separateBetaBuild.toString());
    }

    SECTION("Module compile can recover after a missing-path failure on the same compiler"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-module-error-recovery");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestModuleCompilePack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        FileIO fileIO;
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS";
        config.initialize(options);

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setPackageImportPaths({Path::join(tmpRoot, "pkgC/node_modules")});

        REQUIRE_THROWS_AS(Compiler::compileModule(compiler, Path::join(tmpRoot, "pkgC/missing")), lv::Exception);
        REQUIRE_NOTHROW(Compiler::compileModule(compiler, Path::join(tmpRoot, "pkgC/good")));
        REQUIRE(Path::exists(Path::join(tmpRoot, "pkgC/build/good/main.lv.mjs")));
    }

    SECTION("Warm module compile skips code generation and type resolution"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-module-warm-skip");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestModuleCompilePack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkg");
        const std::string moduleDir = Path::join(packageDir, "node_modules/shared/common");
        const std::string sourcePath = Path::join(moduleDir, "math.lv");
        const std::string resolvedSourcePath = Path::resolve(sourcePath);
        const std::string outputJsPath = Path::join(packageDir, "node_modules/shared/build/common/math.lv.mjs");
        const std::string outputDtsPath = Path::join(packageDir, "node_modules/shared/build/common/math.lv.d.mts");

        FileIO fileIO;
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS_DTS";
        config.initialize(options);
        config.allowUnresolvedTypes(true);
        config.setTypesModule("types.mjs");

        CountingTypeResolver resolver;
        resolver.responses[resolvedSourcePath + "::add"] = TypeResolver::Result(resolvedSourcePath + "::add", "FunctionType.for(Type.number, Type.number, Type.number)", {"FunctionType", "Type"});
        resolver.responses[resolvedSourcePath + "::sharedLabel"] = TypeResolver::Result(resolvedSourcePath + "::sharedLabel", "FunctionType.for(Type.string)", {"FunctionType", "Type"});

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->setPackageImportPaths({Path::join(packageDir, "node_modules")});

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, moduleDir));
        REQUIRE(resolver.observedBatches.size() == 1);
        REQUIRE(Path::exists(outputJsPath));
        REQUIRE(Path::exists(outputDtsPath));

        resolver.observedBatches.clear();
        const DateTime now;
        Path::setLastModified(sourcePath, now.addMSeconds(-10000));
        Path::setLastModified(outputJsPath, now);
        Path::setLastModified(outputDtsPath, now);
        const DateTime jsModifiedBefore = Path::lastModified(outputJsPath);
        const DateTime dtsModifiedBefore = Path::lastModified(outputDtsPath);

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, moduleDir));
        REQUIRE(resolver.observedBatches.empty());
        REQUIRE(Path::lastModified(outputJsPath) == jsModifiedBefore);
        REQUIRE(Path::lastModified(outputDtsPath) == dtsModifiedBefore);
    }

    SECTION("Modified module source still re-requests and rewrites resolved output"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-module-stale-rebuild");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestModuleCompilePack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkg");
        const std::string moduleDir = Path::join(packageDir, "node_modules/shared/common");
        const std::string sourcePath = Path::join(moduleDir, "math.lv");
        const std::string resolvedSourcePath = Path::resolve(sourcePath);
        const std::string outputJsPath = Path::join(packageDir, "node_modules/shared/build/common/math.lv.mjs");
        const std::string outputDtsPath = Path::join(packageDir, "node_modules/shared/build/common/math.lv.d.mts");

        FileIO fileIO;
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS_DTS";
        config.initialize(options);
        config.allowUnresolvedTypes(true);
        config.setTypesModule("types.mjs");

        CountingTypeResolver resolver;
        resolver.responses[resolvedSourcePath + "::add"] = TypeResolver::Result(resolvedSourcePath + "::add", "FunctionType.for(Type.number, Type.number, Type.number)", {"FunctionType", "Type"});
        resolver.responses[resolvedSourcePath + "::sharedLabel"] = TypeResolver::Result(resolvedSourcePath + "::sharedLabel", "FunctionType.for(Type.string)", {"FunctionType", "Type"});

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->setPackageImportPaths({Path::join(packageDir, "node_modules")});

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, moduleDir));

        resolver.observedBatches.clear();
        const DateTime now;
        Path::setLastModified(outputJsPath, now.addMSeconds(-20000));
        Path::setLastModified(outputDtsPath, now.addMSeconds(-20000));
        Path::setLastModified(sourcePath, now);

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, moduleDir));
        REQUIRE(resolver.observedBatches.size() == 1);

        std::set<std::string> ids;
        for ( const auto& request : resolver.observedBatches.front() ){
            ids.insert(request.requestId());
        }
        REQUIRE(ids.count(resolvedSourcePath + "::add") == 1);

        const std::string jsContents = fileIO.readFromFile(outputJsPath);
        REQUIRE(jsContents.find("FunctionType.for(Type.number, Type.number, Type.number)") != std::string::npos);
        REQUIRE(jsContents.find("/*@@LVTYPE:") == std::string::npos);
    }

    SECTION("Dependent module does not re-request fresh dependency functions"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-dependent-skip");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestModuleCompilePack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkgA");
        const std::string dependencyDir = Path::join(packageDir, "node_modules/depone/util");
        const std::string dependencySourcePath = Path::join(dependencyDir, "token.lv");
        const std::string resolvedDependencySourcePath = Path::resolve(dependencySourcePath);
        const std::string appDir = Path::join(packageDir, "app");

        FileIO fileIO;
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS_DTS";
        config.initialize(options);
        config.allowUnresolvedTypes(true);
        config.setTypesModule("types.mjs");

        CountingTypeResolver resolver;
        resolver.responses[resolvedDependencySourcePath + "::tokenValue"] = TypeResolver::Result(resolvedDependencySourcePath + "::tokenValue", "FunctionType.for(Type.string)", {"FunctionType", "Type"});

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->setPackageImportPaths({Path::join(packageDir, "node_modules")});

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, dependencyDir));
        resolver.observedBatches.clear();

        REQUIRE_NOTHROW(Compiler::compileModule(compiler, appDir));
        REQUIRE(resolver.observedBatches.empty());
    }

    SECTION("Batched module compile resolves once for the whole package"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-batch-resolve-once");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestModuleCompilePack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkg");
        const std::string alphaDir = Path::join(packageDir, "alpha");
        const std::string betaDir = Path::join(packageDir, "beta");
        const std::string sharedSourcePath = Path::resolve(Path::join(packageDir, "node_modules/shared/common/math.lv"));
        const std::string alphaSourcePath = Path::resolve(Path::join(packageDir, "alpha/main.lv"));
        const std::string betaSourcePath = Path::resolve(Path::join(packageDir, "beta/main.lv"));

        FileIO fileIO;
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS_DTS";
        config.initialize(options);
        config.allowUnresolvedTypes(true);
        config.setTypesModule("types.mjs");

        CountingTypeResolver resolver;
        resolver.responses[sharedSourcePath + "::add"] = TypeResolver::Result(sharedSourcePath + "::add", "FunctionType.for(Type.number, Type.number, Type.number)", {"FunctionType", "Type"});
        resolver.responses[sharedSourcePath + "::sharedLabel"] = TypeResolver::Result(sharedSourcePath + "::sharedLabel", "FunctionType.for(Type.string)", {"FunctionType", "Type"});
        resolver.responses[alphaSourcePath + "::alphaTotal"] = TypeResolver::Result(alphaSourcePath + "::alphaTotal", "FunctionType.for(Type.number)", {"FunctionType", "Type"});
        resolver.responses[betaSourcePath + "::betaTotal"] = TypeResolver::Result(betaSourcePath + "::betaTotal", "FunctionType.for(Type.number)", {"FunctionType", "Type"});

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->setPackageImportPaths({Path::join(packageDir, "node_modules")});

        const auto results = Compiler::compileModules(compiler, {alphaDir, betaDir});
        REQUIRE(results.size() == 2);
        REQUIRE(resolver.observedBatches.size() == 1);

        std::set<std::string> ids;
        for ( const auto& request : resolver.observedBatches.front() ){
            ids.insert(request.requestId());
        }
        REQUIRE(ids.count(sharedSourcePath + "::add") == 1);
        REQUIRE(ids.count(sharedSourcePath + "::sharedLabel") == 1);
        REQUIRE(ids.count(alphaSourcePath + "::alphaTotal") == 1);
        REQUIRE(ids.count(betaSourcePath + "::betaTotal") == 1);
        REQUIRE(Path::exists(Path::join(packageDir, "build/alpha/main.lv.mjs")));
        REQUIRE(Path::exists(Path::join(packageDir, "build/beta/main.lv.mjs")));
    }

    SECTION("Batched module compile reports per-module failures and still finalizes others"){
        const std::string tmpRoot = Path::join(Path::temporaryDirectory(), "lv-compiler-batch-error-recovery");
        Txtar archive = Txtar::parseFile(Path::join(typeTestDataPath(), "TypeTestModuleCompilePack01.txtar"));
        Txtar::ExtractionGuard extraction = archive.extractTemp(tmpRoot);

        const std::string packageDir = Path::join(tmpRoot, "pkgC");
        const std::string badDir = Path::join(packageDir, "missing");
        const std::string goodDir = Path::join(packageDir, "good");
        const std::string dependencySourcePath = Path::resolve(Path::join(packageDir, "node_modules/depthree/util/token.lv"));
        const std::string dependencyOutputPath = Path::join(packageDir, "node_modules/depthree/build/util/token.lv.mjs");

        FileIO fileIO;
        Compiler::Config config(true, ".mjs", &fileIO);
        MLNode options(MLNode::Type::Object);
        options["importLocalPath"] = "node_modules";
        options["packageBuildPath"] = "build";
        options["outputTarget"] = "JS_DTS";
        config.initialize(options);
        config.allowUnresolvedTypes(true);
        config.setTypesModule("types.mjs");

        CountingTypeResolver resolver;
        resolver.responses[dependencySourcePath + "::tokenValue"] = TypeResolver::Result(dependencySourcePath + "::tokenValue", "FunctionType.for(Type.string)", {"FunctionType", "Type"});

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);
        compiler->setPackageImportPaths({Path::join(packageDir, "node_modules")});

        const auto results = Compiler::compileModules(compiler, {badDir, goodDir});
        REQUIRE(results.size() == 2);
        REQUIRE(results[0].modulePath == badDir);
        REQUIRE(results[0].error != nullptr);
        REQUIRE(results[1].modulePath == goodDir);
        REQUIRE(results[1].error == nullptr);
        REQUIRE(Path::exists(dependencyOutputPath));

        const std::string dependencyJs = fileIO.readFromFile(dependencyOutputPath);
        REQUIRE(dependencyJs.find("FunctionType.for(Type.string)") != std::string::npos);
        REQUIRE(dependencyJs.find("/*@@LVTYPE:") == std::string::npos);
    }

    SECTION("Declared meta root functions reuse type resolution and mark descriptors"){
        FileIO fileIO;
        const std::string name = "TypeTest08";
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
        resolver.responses[sourcePath + "::load"] = TypeResolver::Result{
            sourcePath + "::load",
            "FunctionType.for(Type.string, Type.string)",
            {"FunctionType", "Type"}
        };

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);
        requireEquivalentElementsOutput(expectedJs, targetResult.js, name + ".lv.js");
        requireEquivalentElementsOutput(expectedDts, targetResult.dts, name + ".lv.d.ts");

        ModuleFileDescriptor::Ptr descriptor = ModuleFileDescriptor::create("declared.lv");
        descriptor->addExport(ExportDescriptor("add", ExportDescriptor::Function, true, true));
        descriptor->addExport(ExportDescriptor("zero", ExportDescriptor::Function, false, true));

        ModuleFileDescriptor::Ptr roundTrip = ModuleFileDescriptor::createFromMLNode(descriptor->toMLNode());
        REQUIRE(roundTrip->exports().size() == 2);
        REQUIRE(roundTrip->exports()[0].meta());
        REQUIRE(roundTrip->exports()[0].declared());
        REQUIRE_FALSE(roundTrip->exports()[1].meta());
        REQUIRE(roundTrip->exports()[1].declared());
    }

    SECTION("Generic meta root functions preserve type parameters through dts and resolved JS"){
        FileIO fileIO;
        const std::string name = "TypeTest09";
        const std::string sourcePath = Path::join(typeTestDataPath(), name + ".lv");
        const std::string contents = fileIO.readFromFile(sourcePath);
        const std::string expectedJs = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.js"));
        const std::string expectedDts = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.d.ts"));

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);
        config.setTypesModule("types.mjs");

        StubTypeResolver resolver;
        resolver.responses[sourcePath + "::mapVal"] = TypeResolver::Result{
            sourcePath + "::mapVal",
            "FunctionType.for(TypeVariable.for('Y'), TypeVariable.for('X'), GenericType.for(Flow, FunctionType.for(TypeVariable.for('Y'), TypeVariable.for('X')))).withTypeParameters(['X', 'Y'])",
            {"FunctionType", "GenericType", "Type", "TypeVariable"}
        };

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);
        requireEquivalentElementsOutput(expectedJs, targetResult.js, name + ".lv.js");
        requireEquivalentElementsOutput(expectedDts, targetResult.dts, name + ".lv.d.ts");
        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(resolver.observedRequests[0].requestId() == sourcePath + "::mapVal");
        REQUIRE(resolver.observedRequests[0].functionName() == "mapVal");
        REQUIRE(resolver.observedRequests[0].sourcePath() == sourcePath + ".d.ts");
    }

    SECTION("Declared generic meta root functions preserve type parameters through dts and resolved JS"){
        FileIO fileIO;
        const std::string name = "TypeTest10";
        const std::string sourcePath = Path::join(typeTestDataPath(), name + ".lv");
        const std::string contents = fileIO.readFromFile(sourcePath);
        const std::string expectedJs = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.js"));
        const std::string expectedDts = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.d.ts"));

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);
        config.setTypesModule("types.mjs");

        StubTypeResolver resolver;
        resolver.responses[sourcePath + "::identity"] = TypeResolver::Result{
            sourcePath + "::identity",
            "FunctionType.for(TypeVariable.for('X'), TypeVariable.for('X')).withTypeParameters(['X'])",
            {"FunctionType", "Type", "TypeVariable"}
        };

        Compiler::Ptr compiler = Compiler::create(config);
        compiler->setTypeResolver(&resolver);

        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);
        requireEquivalentElementsOutput(expectedJs, targetResult.js, name + ".lv.js");
        requireEquivalentElementsOutput(expectedDts, targetResult.dts, name + ".lv.d.ts");
        REQUIRE(resolver.observedRequests.size() == 1);
        REQUIRE(resolver.observedRequests[0].requestId() == sourcePath + "::identity");
        REQUIRE(resolver.observedRequests[0].functionName() == "identity");
        REQUIRE(resolver.observedRequests[0].sourcePath() == sourcePath + ".d.ts");
    }

    SECTION("Type-only js imports stay in dts and drop from js while same-file interfaces stay local"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest11.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(targetResult.js.find("import {PointInterface} from './point.ts'") == std::string::npos);
        REQUIRE(targetResult.dts.find("import {PointInterface} from './point.ts'") != std::string::npos);
        REQUIRE(targetResult.dts.find("interface LocalPoint") != std::string::npos);
        REQUIRE(targetResult.dts.find("import {LocalPoint}") == std::string::npos);
    }

    SECTION("A type alias satisfies a type annotation"){
        FileIO fileIO;
        const std::string name = "TypeTest13";
        const std::string sourcePath = Path::join(typeTestDataPath(), name + ".lv");
        const std::string contents = fileIO.readFromFile(sourcePath);
        const std::string expectedJs = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.js"));
        const std::string expectedDts = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.d.ts"));

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        requireEquivalentElementsOutput(expectedJs, targetResult.js, name + ".lv.js");
        requireEquivalentElementsOutput(expectedDts, targetResult.dts, name + ".lv.d.ts");
    }

    SECTION("A component satisfies a type annotation"){
        FileIO fileIO;
        const std::string name = "TypeTest14";
        const std::string sourcePath = Path::join(typeTestDataPath(), name + ".lv");
        const std::string contents = fileIO.readFromFile(sourcePath);
        const std::string expectedJs = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.js"));
        const std::string expectedDts = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.d.ts"));

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        requireEquivalentElementsOutput(expectedJs, targetResult.js, name + ".lv.js");
        requireEquivalentElementsOutput(expectedDts, targetResult.dts, name + ".lv.d.ts");
    }

    SECTION("A js import satisfies a type annotation"){
        FileIO fileIO;
        const std::string name = "TypeTest15";
        const std::string sourcePath = Path::join(typeTestDataPath(), name + ".lv");
        const std::string contents = fileIO.readFromFile(sourcePath);
        const std::string expectedJs = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.js"));
        const std::string expectedDts = fileIO.readFromFile(Path::join(typeTestDataPath(), name + ".lv.d.ts"));

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        requireEquivalentElementsOutput(expectedJs, targetResult.js, name + ".lv.js");
        requireEquivalentElementsOutput(expectedDts, targetResult.dts, name + ".lv.d.ts");
    }

    SECTION("A type alias used as a value is reported as a mismatch"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest12.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for TypeTest12.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("'Alias' refers to a type, but is being used as a value.") != std::string::npos);
        }
    }

    SECTION("A type parameter used as a value is reported as a mismatch"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest16.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for TypeTest16.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("'X' refers to a type, but is being used as a value.") != std::string::npos);
        }
    }

    SECTION("A function used as a type is reported as a mismatch"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest17.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);

        try {
            compiler->compileToTarget(sourcePath, contents);
            FAIL("Expected a SyntaxException for TypeTest17.");
        } catch ( const SyntaxException& e ){
            REQUIRE(e.message().find("'Qux' refers to a value, but is being used as a type.") != std::string::npos);
        }
    }

    SECTION("An unbound name in a parameter default value is reported"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest19.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(targetResult.js.find("a = Unknown") != std::string::npos);
        REQUIRE(targetResult.js.find("import {Unknown} from '__UNRESOLVED__'") != std::string::npos);
    }

    SECTION("A parameter default value sees the parameters before it"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest20.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(targetResult.js.find("b = a") != std::string::npos);
        REQUIRE(targetResult.js.find("__UNRESOLVED__") == std::string::npos);
    }

    SECTION("A nearer declaration in the other space does not hide an outer one"){
        FileIO fileIO;
        const std::string sourcePath = Path::join(typeTestDataPath(), "TypeTest18.lv");
        const std::string contents = fileIO.readFromFile(sourcePath);

        Compiler::Config config(false);
        config.allowUnresolvedTypes(true);
        config.outputTarget(Compiler::Config::OutputTarget::JS_DTS);

        Compiler::Ptr compiler = Compiler::create(config);
        Compiler::TargetResult targetResult = compiler->compileToTarget(sourcePath, contents);

        REQUIRE(targetResult.js.find("__UNRESOLVED__") == std::string::npos);
    }
}

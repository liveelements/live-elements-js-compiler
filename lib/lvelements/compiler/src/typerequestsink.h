#ifndef LVTYPEREQUESTSINK_H
#define LVTYPEREQUESTSINK_H

#include "typeresolver.h"
#include "live/sourcelocation.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace lv{

class FileIOInterface;

namespace el{

class LV_ELEMENTS_COMPILER_EXPORT TypeRequestSink {
public:
    class GeneratedTypeOutput {
    public:
        std::string outputPath;
        std::string content;
        bool writeToFile = false;
    };

    class PendingRequest {
    public:
        TypeResolver::Request request;
        std::string outputKey;
        std::string marker;
        SourceRangeLocation sourceLocation;
    };

    void setResolver(TypeResolver* resolver);
    bool hasResolver() const;

    void recordTypeRequest(
        const TypeResolver::Request& request,
        const std::string& outputKey,
        const std::string& marker,
        const SourceRangeLocation& sourceLocation);

    void storeGeneratedTypeOutput(
        const std::string& outputKey,
        const std::string& outputPath,
        const std::string& content,
        bool writeToFile);

    const GeneratedTypeOutput* generatedOutput(const std::string& outputKey) const;
    bool hasPendingFor(const std::string& outputKey) const;

    void finalize(FileIOInterface* fileIO, const std::string& typesModulePath);

private:
    static std::string buildTypeImportStatement(const std::set<std::string>& imports, const std::string& modulePath);
    static std::string substituteResolvedType(std::string js, const std::string& marker, const std::string& expr);
    static std::string injectTypeImports(const std::string& js, const std::set<std::string>& imports, const std::string& modulePath);

    TypeResolver* m_resolver = nullptr;
    std::vector<PendingRequest> m_pending;
    std::map<std::string, GeneratedTypeOutput> m_outputs;
};

}} // namespace lv, el

#endif // LVTYPEREQUESTSINK_H

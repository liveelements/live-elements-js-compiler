#ifndef LVTYPERESOLVER_H
#define LVTYPERESOLVER_H

#include "diagnostic.h"
#include <string>
#include <utility>
#include <vector>

namespace lv{

class FileIOInterface;

namespace el{

class LV_ELEMENTS_COMPILER_EXPORT TypeResolver {
public:
    class Request {
    public:
        Request(
            std::string requestId = "",
            std::string sourcePath = "",
            std::string sourceText = "",
            std::string functionName = "")
            : m_requestId(std::move(requestId))
            , m_sourcePath(std::move(sourcePath))
            , m_sourceText(std::move(sourceText))
            , m_functionName(std::move(functionName))
        {}

        const std::string& requestId() const{ return m_requestId; }
        const std::string& sourcePath() const{ return m_sourcePath; }
        const std::string& sourceText() const{ return m_sourceText; }
        const std::string& functionName() const{ return m_functionName; }

        void setRequestId(const std::string& requestId){ m_requestId = requestId; }
        void setSourcePath(const std::string& sourcePath){ m_sourcePath = sourcePath; }
        void setSourceText(const std::string& sourceText){ m_sourceText = sourceText; }
        void setFunctionName(const std::string& functionName){ m_functionName = functionName; }

    private:
        std::string m_requestId;
        std::string m_sourcePath;
        std::string m_sourceText;
        std::string m_functionName;
    };

    class Result {
    public:
        Result(
            std::string requestId = "",
            std::string typeExpression = "",
            std::vector<std::string> typeImports = {},
            std::vector<Diagnostic> diagnostics = {})
            : m_requestId(std::move(requestId))
            , m_typeExpression(std::move(typeExpression))
            , m_typeImports(std::move(typeImports))
            , m_diagnostics(std::move(diagnostics))
        {}

        const std::string& requestId() const{ return m_requestId; }
        const std::string& typeExpression() const{ return m_typeExpression; }
        const std::vector<std::string>& typeImports() const{ return m_typeImports; }
        const std::vector<Diagnostic>& diagnostics() const{ return m_diagnostics; }

        void setRequestId(const std::string& requestId){ m_requestId = requestId; }
        void setTypeExpression(const std::string& typeExpression){ m_typeExpression = typeExpression; }
        void setTypeImports(const std::vector<std::string>& typeImports){ m_typeImports = typeImports; }
        void addTypeImport(const std::string& typeImport){ m_typeImports.push_back(typeImport); }
        void setDiagnostics(const std::vector<Diagnostic>& diagnostics){ m_diagnostics = diagnostics; }
        void addDiagnostic(const Diagnostic& diagnostic){ m_diagnostics.push_back(diagnostic); }

        bool hasError() const{
            for ( const Diagnostic& diagnostic : m_diagnostics ){
                if ( diagnostic.severity() == Diagnostic::Severity::Error ){
                    return true;
                }
            }
            return false;
        }

    private:
        std::string             m_requestId;
        std::string             m_typeExpression;
        std::vector<std::string> m_typeImports;
        std::vector<Diagnostic>  m_diagnostics;
    };

    virtual ~TypeResolver() = default;
    virtual std::vector<Result> resolve(const std::vector<Request>& requests) = 0;
};

}} // namespace lv, el

#endif // LVTYPERESOLVER_H

/****************************************************************************
**
** Copyright (C) 2022 Dinu SV.
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

#include "typerequestsink.h"
#include "languageparser.h"
#include "live/fileio.h"
#include <sstream>

namespace lv{ namespace el{

void TypeRequestSink::setResolver(TypeResolver* resolver){
    m_resolver = resolver;
}

bool TypeRequestSink::hasResolver() const{
    return m_resolver != nullptr;
}

void TypeRequestSink::recordTypeRequest(
    const TypeResolver::Request& request,
    const std::string& outputKey,
    const std::string& marker,
    const SourceRangeLocation& sourceLocation)
{
    m_pending.push_back(PendingRequest{request, outputKey, marker, sourceLocation});
}

void TypeRequestSink::storeGeneratedTypeOutput(
    const std::string& outputKey,
    const std::string& outputPath,
    const std::string& content,
    bool writeToFile)
{
    if ( outputKey.empty() ){
        return;
    }
    m_outputs[outputKey] = GeneratedTypeOutput{outputPath, content, writeToFile};
}

const TypeRequestSink::GeneratedTypeOutput* TypeRequestSink::generatedOutput(const std::string& outputKey) const{
    auto it = m_outputs.find(outputKey);
    return it == m_outputs.end() ? nullptr : &it->second;
}

bool TypeRequestSink::hasPendingFor(const std::string& outputKey) const{
    for ( const PendingRequest& pending : m_pending ){
        if ( pending.outputKey == outputKey ){
            return true;
        }
    }
    return false;
}

std::string TypeRequestSink::buildTypeImportStatement(const std::set<std::string>& imports, const std::string& modulePath){
    if ( imports.empty() || modulePath.empty() ){
        return std::string();
    }

    std::ostringstream stream;
    stream << "import {";
    for ( auto it = imports.begin(); it != imports.end(); ++it ){
        if ( it != imports.begin() ){
            stream << ", ";
        }
        stream << *it;
    }
    stream << "} from '" << modulePath << "'\n";
    return stream.str();
}

std::string TypeRequestSink::substituteResolvedType(std::string js, const std::string& marker, const std::string& expr){
    const size_t position = js.find(marker);
    if ( position == std::string::npos ){
        return js;
    }
    js.replace(position, marker.size(), expr);
    return js;
}

std::string TypeRequestSink::injectTypeImports(const std::string& js, const std::set<std::string>& imports, const std::string& modulePath){
    const std::string statement = buildTypeImportStatement(imports, modulePath);
    if ( statement.empty() ){
        return js;
    }

    size_t insertionPoint = 0;
    size_t cursor = 0;
    while ( cursor < js.size() ){
        if ( js.compare(cursor, 6, "import ") != 0 ){
            break;
        }
        size_t lineEnd = js.find('\n', cursor);
        if ( lineEnd == std::string::npos ){
            insertionPoint = js.size();
            break;
        }
        insertionPoint = lineEnd + 1;
        cursor = insertionPoint;
    }

    std::string result = js;
    result.insert(insertionPoint, statement);
    return result;
}

void TypeRequestSink::finalize(FileIOInterface* fileIO, const std::string& typesModulePath){
    if ( !m_resolver || m_pending.empty() ){
        return;
    }

    std::vector<TypeResolver::Request> requests;
    requests.reserve(m_pending.size());
    for ( const PendingRequest& pending : m_pending ){
        requests.push_back(pending.request);
    }

    std::vector<TypeResolver::Result> results = m_resolver->resolve(requests);
    if ( results.size() != m_pending.size() ){
        THROW_EXCEPTION(lv::Exception, "Type resolver returned an unexpected number of results.", Exception::toCode("~TypeResolve"));
    }

    std::map<std::string, std::set<std::string>> outputImports;

    for ( size_t i = 0; i < m_pending.size(); ++i ){
        const PendingRequest& pending = m_pending[i];
        const TypeResolver::Result& resolved = results[i];

        if ( resolved.requestId() != pending.request.requestId() ){
            THROW_EXCEPTION(lv::Exception, "Type resolver returned results out of request order.", Exception::toCode("~TypeResolve"));
        }

        if ( resolved.hasError() ){
            const Diagnostic* diagnostic = nullptr;
            for ( const Diagnostic& candidate : resolved.diagnostics() ){
                if ( candidate.severity() == Diagnostic::Severity::Error ){
                    diagnostic = &candidate;
                    break;
                }
            }
            throw SyntaxException(
                diagnostic ? diagnostic->message() : "Type resolution failed.",
                Exception::toCode(diagnostic && !diagnostic->code().empty() ? diagnostic->code() : "~TypeResolve"),
                pending.sourceLocation,
                SOURCE_TRACE()
            );
        }

        auto outputIt = m_outputs.find(pending.outputKey);
        if ( outputIt == m_outputs.end() ){
            continue;
        }
        outputIt->second.content = substituteResolvedType(
            outputIt->second.content,
            pending.marker,
            resolved.typeExpression()
        );
        outputImports[pending.outputKey].insert(
            resolved.typeImports().begin(),
            resolved.typeImports().end()
        );
    }

    for ( auto& generated : m_outputs ){
        auto importsIt = outputImports.find(generated.first);
        if ( importsIt != outputImports.end() ){
            generated.second.content = injectTypeImports(
                generated.second.content,
                importsIt->second,
                typesModulePath
            );
        }
        if ( generated.second.writeToFile && fileIO && !generated.second.outputPath.empty() ){
            fileIO->writeToFile(generated.second.outputPath, generated.second.content);
        }
    }
}

}} // namespace lv, el

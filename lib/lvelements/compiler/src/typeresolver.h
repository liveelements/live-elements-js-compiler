#ifndef LVTYPERESOLVER_H
#define LVTYPERESOLVER_H

#include "live/elements/compiler/lvelcompilerglobal.h"
#include <string>
#include <vector>

namespace lv{

class FileIOInterface;

namespace el{

class LV_ELEMENTS_COMPILER_EXPORT TypeResolver {
public:
    class Request {
    public:
        std::string requestId;
        std::string dtsPath;
        std::string functionName;
    };

    class Result {
    public:
        std::string requestId;
        std::string typeExpression;
        std::vector<std::string> typeImports;
    };

    virtual ~TypeResolver() = default;
    virtual std::vector<Result> resolve(const std::vector<Request>& requests) = 0;
};

}} // namespace lv, el

#endif // LVTYPERESOLVER_H

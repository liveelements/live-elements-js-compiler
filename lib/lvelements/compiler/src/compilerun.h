#ifndef LVCOMPILERUN_H
#define LVCOMPILERUN_H

#include "typerequestsink.h"
#include <memory>

namespace lv{

class FileIOInterface;

namespace el{

class LV_ELEMENTS_COMPILER_EXPORT CompileRun {
public:
    static std::unique_ptr<CompileRun> createUnique(TypeResolver* resolver);

    explicit CompileRun(TypeResolver* resolver);

    TypeRequestSink& sink();
    const TypeRequestSink& sink() const;

    void finalize(FileIOInterface* fileIO, const std::string& typesModulePath);

private:
    TypeRequestSink m_sink;
};

}} // namespace lv, el

#endif // LVCOMPILERUN_H

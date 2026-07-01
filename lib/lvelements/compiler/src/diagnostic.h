#ifndef LVDIAGNOSTIC_H
#define LVDIAGNOSTIC_H

#include "live/elements/compiler/lvelcompilerglobal.h"
#include "live/sourcelocation.h"
#include <string>

namespace lv{
namespace el{

class LV_ELEMENTS_COMPILER_EXPORT Diagnostic {
public:
    enum class Severity {
        Error,
        Warning
    };

    Diagnostic(
        Severity severity = Severity::Error,
        const std::string& code = "",
        const std::string& message = "",
        const SourceRangeLocation& location = SourceRangeLocation());

    Severity severity() const;
    const std::string& code() const;
    const std::string& message() const;
    const SourceRangeLocation& location() const;

private:
    Severity            m_severity;
    std::string         m_code;
    std::string         m_message;
    SourceRangeLocation m_location;
};

}} // namespace lv, el

#endif // LVDIAGNOSTIC_H

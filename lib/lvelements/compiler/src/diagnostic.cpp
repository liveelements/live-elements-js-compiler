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

#include "diagnostic.h"

namespace lv{ namespace el{

Diagnostic::Diagnostic(
    Severity severity,
    const std::string& code,
    const std::string& message,
    const SourceRangeLocation& location)
    : m_severity(severity)
    , m_code(code)
    , m_message(message)
    , m_location(location)
{}

Diagnostic::Severity Diagnostic::severity() const{
    return m_severity;
}

const std::string& Diagnostic::code() const{
    return m_code;
}

const std::string& Diagnostic::message() const{
    return m_message;
}

const SourceRangeLocation& Diagnostic::location() const{
    return m_location;
}

}} // namespace lv, el

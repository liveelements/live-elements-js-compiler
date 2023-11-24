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

#ifndef LVCOMPLETIONCONTEXT_H
#define LVCOMPLETIONCONTEXT_H

#include "live/elements/compiler/lvelcompilerglobal.h"
#include "live/utf8.h"
#include <set>
#include <vector>

namespace lv{ namespace el{

class LV_ELEMENTS_COMPILER_EXPORT CursorContext{

public:
    enum Context{
        InImport = 1,
        InElements = 2,
        InLeftOfDeclaration = 4,
        InRightOfDeclaration = 8,
        InStringLiteral = 16,
        InRelativeImport = 32
    };

public:
    CursorContext(
        int context,
        const std::vector<Utf8::Range>& expressionPath,
        const std::vector<Utf8::Range> &propertyPath = std::vector<Utf8::Range>(),
        const Utf8::Range& propertyDeclaredType = Utf8::Range(),
        const Utf8::Range& objectType = Utf8::Range(),
        const Utf8::Range& objectImportNamespace = Utf8::Range()
    );
    ~CursorContext();

    int context() const;
    Utf8 contextString() const;

    const std::vector<Utf8::Range>& expressionPath() const;
    const std::vector<Utf8::Range>& propertyPath() const;
    Utf8::Range propertyName() const;
    const Utf8::Range& propertyRange() const;
    const Utf8::Range& propertyDeclaredType() const;
    const Utf8::Range& objectType() const;
    const Utf8::Range& objectImportNamespace() const;

public:
    int                      m_context;
    std::vector<Utf8::Range> m_expressionPath;
    std::vector<Utf8::Range> m_propertyPath;
    Utf8::Range              m_propertyRange;
    Utf8::Range              m_propertyDeclaredType;
    Utf8::Range              m_objectType;
    Utf8::Range              m_objectImportNamespace;

    static std::set<std::string> keywords;
};

inline const std::vector<Utf8::Range> &CursorContext::expressionPath() const{
    return m_expressionPath;
}

inline const std::vector<Utf8::Range> &CursorContext::propertyPath() const{
    return m_propertyPath;
}

inline Utf8::Range CursorContext::propertyName() const{
    if ( m_propertyPath.size() > 0 )
        return m_propertyPath.back();
    return Utf8::Range();
}

inline const Utf8::Range &CursorContext::propertyRange() const{
    return m_propertyRange;
}

inline const Utf8::Range &CursorContext::propertyDeclaredType() const{
    return m_propertyDeclaredType;
}

inline const Utf8::Range &CursorContext::objectType() const{
    return m_objectType;
}

inline const Utf8::Range &CursorContext::objectImportNamespace() const{
    return m_objectImportNamespace;
}

}} // namespace lv, el

#endif // LVCOMPLETIONCONTEXT_H

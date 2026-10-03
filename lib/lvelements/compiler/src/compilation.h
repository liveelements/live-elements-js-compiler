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

#ifndef LVCOMPILATION_H
#define LVCOMPILATION_H

#include "live/elements/compiler/lvelcompilerglobal.h"
#include "live/elements/compiler/elementsmodule.h"

#include <memory>
#include <string>
#include <vector>

namespace lv{ namespace el{

class ModuleFile;

class LV_ELEMENTS_COMPILER_EXPORT Compilation{

    DISABLE_COPY(Compilation);

    friend class Compiler;

public:
    typedef std::shared_ptr<Compilation> Ptr;

public:
    ~Compilation();

    const ElementsModule::Ptr& module() const;
    ModuleFile* target() const;
    ModuleFile* findFile(const std::string& uri) const;
    const std::vector<ElementsModule::Ptr>& modules() const;

    bool hasTargetSource() const;
    const std::string& targetSource() const;

private:
    Compilation(const ElementsModule::Ptr& module, ModuleFile* target, const std::string& targetSource);

    void collectModules(const ElementsModule::Ptr& module);

    ModuleFile*                      m_target;
    std::vector<ElementsModule::Ptr> m_modules;
    std::string                      m_targetSource;
};

}} // namespace lv, el

#endif // LVCOMPILATION_H

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

#include "compilation.h"
#include "modulefile.h"
#include "live/exception.h"
#include "live/utf8.h"

namespace lv{ namespace el{

/**
 * \class Compilation
 * \brief Read-only snapshot of a single compile run.
 *
 * Holds every ElementsModule reachable from the target file, and finds their files by
 * ModuleFile::uri(). The snapshot keeps its modules alive, so it outlives the compiler and
 * is not affected by later runs.
 */

Compilation::Compilation(const ElementsModule::Ptr &module, ModuleFile *target, const std::string &targetSource)
    : m_target(target)
    , m_targetSource(targetSource)
{
    collectModules(module);
}

Compilation::~Compilation(){
}

void Compilation::collectModules(const ElementsModule::Ptr &module){
    for ( const ElementsModule::Ptr& collected : m_modules ){
        if ( collected == module )
            return;
        if ( collected->uri() == module->uri() ){
            THROW_EXCEPTION(
                lv::Exception,
                Utf8("Assertion: Duplicate module '%' in compilation.").format(module->uri()),
                Exception::toCode("~Assertion")
            );
        }
    }

    m_modules.push_back(module);

    for ( auto it = module->files().begin(); it != module->files().end(); ++it ){
        const std::list<ModuleFile::ModuleImport>& imports = it->second->imports();
        for ( auto impIt = imports.begin(); impIt != imports.end(); ++impIt ){
            if ( impIt->module )
                collectModules(impIt->module);
        }
    }
}

/**
 * \brief Returns the target module, which is always the first in modules()
 */
const ElementsModule::Ptr &Compilation::module() const{
    return m_modules.front();
}

ModuleFile *Compilation::target() const{
    return m_target;
}

/**
 * \brief Finds a file by its ModuleFile::uri(), \p uri being "<module uri>/<file name>"
 */
ModuleFile *Compilation::findFile(const std::string &uri) const{
    size_t separator = uri.rfind('/');
    if ( separator == std::string::npos )
        return nullptr;

    std::string moduleUri = uri.substr(0, separator);
    for ( const ElementsModule::Ptr& module : m_modules ){
        if ( module->uri() == moduleUri )
            return module->moduleFileByName(uri.substr(separator + 1));
    }
    return nullptr;
}

const std::vector<ElementsModule::Ptr> &Compilation::modules() const{
    return m_modules;
}

/**
 * \brief Returns true if this run generated the target's source
 *
 * When false, the caller reads the target's jsFilePath() instead.
 */
bool Compilation::hasTargetSource() const{
    return !m_targetSource.empty();
}

const std::string &Compilation::targetSource() const{
    return m_targetSource;
}

}} // namespace lv, el

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

#ifndef LVMODULEFILE_H
#define LVMODULEFILE_H

#include "live/elements/compiler/lvelcompilerglobal.h"
#include "live/elements/compiler/elementsmodule.h"
#include "live/elements/compiler/languagedescriptors.h"
#include "live/packagegraph.h"

#include <memory>
#include <list>

namespace lv{ namespace el{

class ProgramNode;
class Imports;
class ModuleFilePrivate;
class LV_ELEMENTS_COMPILER_EXPORT ModuleFile{

    DISABLE_COPY(ModuleFile);

public:
    class CompilationData{
    public:
        virtual ~CompilationData(){}
    };

public:
    friend class ElementsModule;
    friend class Engine;

    enum Status{
        Initiaized = 0,
        Resolved,
        Parsed,
        Compiling,
        Compiled
    };

    class ModuleImport{
    public:
        ModuleImport() : isRelative(false){}

    public:
        std::string uri;
        std::string as;
        bool isRelative;
        ElementsModule::Ptr module;
    };

public:
    ~ModuleFile();

    void resolveTypes();
    void compile();

    Status status() const;
    const std::string& name() const;
    std::string fileName() const;
    std::string jsFileName() const;
    std::string jsFilePath() const;
    std::string filePath() const;
    const std::list<ModuleImport>& imports() const;
    void resolveImport(const std::string& uri, ElementsModule::Ptr epl);

    const ModuleFileDescriptor::Ptr& descriptor() const;

private:
    void addDependency(ModuleFile* to);
    void setCompilationData(CompilationData* cd);

    bool hasDependency(ModuleFile* module, ModuleFile* dependency);
    static PackageGraph::CyclesResult<ModuleFile*> checkCycles(ModuleFile* mf);
    static PackageGraph::CyclesResult<ModuleFile*> checkCycles(ModuleFile* mf, ModuleFile* current, std::list<ModuleFile*> path);


    static ModuleFile* createFromProgramNode(
        ElementsModule* plugin, 
        const std::string& name, 
        const std::string& content, 
        ProgramNode* node, 
        LanguageParser::AST* ast
    );
    static ModuleFile* createFromDescriptor(
        ElementsModule* module, 
        const std::string& name, 
        const ModuleFileDescriptor::Ptr& descriptor
    );

    ModuleFile(
        ElementsModule* plugin, 
        const std::string& name, 
        const std::string& content, 
        ProgramNode* node, 
        LanguageParser::AST* ast,
        const ModuleFileDescriptor::Ptr& descriptor
    );

    ModuleFilePrivate* m_d;

};

}}// namespace lv, el

#endif // LVMODULEFILE_H

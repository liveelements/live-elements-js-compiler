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

#include "modulefile.h"
#include "languagenodes_p.h"
#include "live/elements/compiler/languageparser.h"
#include "live/exception.h"
#include "live/module.h"
#include "live/modulecontext.h"
#include "live/packagegraph.h"
#include "live/visuallog.h"

#include <sstream>

namespace lv{ namespace el{

class ModuleFilePrivate{
public:
    std::string name;
    std::string content;
    ElementsModule* elementsModule;
    ProgramNode* rootNode;
    LanguageParser::AST* ast;
    ModuleFile::CompilationData* compilationData;

    ModuleFile::Status status;

    std::list<ModuleFile::ModuleImport> imports;
    std::list<ModuleFile*> dependencies;
    std::list<ModuleFile*> dependents;

    ModuleFileDescriptor::Ptr descriptor;
};

ModuleFile::~ModuleFile(){
    delete m_d->compilationData;
    LanguageParser::destroy(m_d->ast);
    delete m_d->rootNode;
    delete m_d;
}

/**
 * Resolve programNode imports to js imports.
 */
void ModuleFile::resolveTypes(){
    if ( !m_d->rootNode ){
        return;
    }
    auto impTypes = m_d->rootNode->importTypes();
    for ( auto nsit = impTypes.begin(); nsit != impTypes.end(); ++nsit ){
        for ( auto it = nsit->second.begin(); it != nsit->second.end(); ++it ){
            ProgramNode::ImportType& impType = it->second;
            bool foundLocalExport = false;
            if ( impType.importNamespace.empty() ){
                auto foundExp = m_d->elementsModule->findExport(impType.name);
                if ( foundExp.isValid() && foundExp.file() ){
                    auto foundFile = m_d->elementsModule->findModuleFileByName(foundExp.file()->fileName().data());
                    if ( !foundFile ){
                        THROW_EXCEPTION(Exception, Utf8("Assertion: File not found: %").format(foundExp.file()->fileName()), Exception::toCode("NullPtr"));
                    }
                    addDependency(foundFile);

                    m_d->rootNode->resolveImport(impType.importNamespace, impType.name, "./" + foundFile->jsFileName());
                    foundLocalExport = true;
                }
            }

            if ( !foundLocalExport ){
                for( auto impIt = m_d->imports.begin(); impIt != m_d->imports.end(); ++impIt ){
                    if ( impIt->as == impType.importNamespace ){
                        auto foundExp = impIt->module->findExport(impType.name);
                        if ( foundExp.isValid() ){
                            auto foundFile = impIt->module->findModuleFileByName(foundExp.file()->fileName().data());
                            if ( !foundFile ){
                                THROW_EXCEPTION(Exception, Utf8("Assertion: File not found: %").format(foundExp.file()->fileName()), Exception::toCode("NullPtr"));
                            }
                            if ( impIt->isRelative ){
                                // this plugin to package
                                Utf8 currentImportId = m_d->elementsModule->module()->context()->importId;
                                Utf8 packageName = impIt->module->module()->context()->packageUnwrapped()->nameScopeAsPath();
                                Utf8 packageToPluginName = currentImportId.length() > packageName.length() 
                                    ? currentImportId.substr(packageName.length() + 1, std::string::npos)
                                    : "";

                                std::vector<Utf8> parts = packageToPluginName.split(".");
                                std::string pluginToPackage = "";
                                if ( parts.size() > 0 ){
                                    for ( size_t i = 0; i < parts.size(); ++i ){
                                        if ( !pluginToPackage.empty() )
                                            pluginToPackage += '/';
                                        pluginToPackage += "..";
                                    }
                                } else {
                                    pluginToPackage = ".";
                                }

                                // package to new plugin

                                Utf8 newPluginImportId = impIt->module->module()->context()->importId;

                                Utf8 packageToNewPluginUri = newPluginImportId.length() > packageName.length() 
                                    ? newPluginImportId.substr(packageName.length() + 1, std::string::npos)
                                    : "";

                                std::vector<Utf8> packageToNewPluginParts = packageToNewPluginUri.split(".");

                                std::string packageToNewPlugin = Utf8::join(packageToNewPluginParts, "/").data();
                                if ( !packageToNewPlugin.empty() )
                                    packageToNewPlugin += "/";

                                m_d->rootNode->resolveImport(impType.importNamespace, impType.name, pluginToPackage + "/" + packageToNewPlugin + foundFile->jsFileName());

                            } else {
                                Utf8 packageName = impIt->module->module()->context()->packageUnwrapped()->nameScopeAsPath();
                                Utf8 importId = impIt->module->module()->context()->importId;
                                Utf8 packageToPluginName =  importId.length() > packageName.length() 
                                    ? importId.substr(packageName.length() + 1, std::string::npos)
                                    : "";

                                std::vector<Utf8> packageToPlugin = packageToPluginName.split(".");

                                std::string configPackageBuildPath = m_d->elementsModule->compiler()->packageBuildPath();
                                std::string packageBuildPath = packageName.data() +
                                    (configPackageBuildPath.empty() ? "" : "/" + configPackageBuildPath);
                                std::string packageToPluginStr = Utf8::join(packageToPlugin, "/").data();
                                std::string importPath = packageBuildPath + (packageToPluginStr.empty() ? "" : "/" + packageToPluginStr) + "/" + foundFile->jsFileName();

                                m_d->rootNode->resolveImport(impType.importNamespace, impType.name, importPath);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    if ( m_d->status != ModuleFile::Compiled ){
        m_d->status = ModuleFile::Resolved;
    }
}

void ModuleFile::compile(){
    if ( m_d->status != ModuleFile::Compiled ){
        if ( !m_d->rootNode ){
            THROW_EXCEPTION(lv::Exception, Utf8("Assertion: ModuleFile being compiled without parsed node."), Exception::toCode("~NullPtr"));
        }
        m_d->elementsModule->compiler()->compileModuleFileToJs(m_d->elementsModule->module(), filePath(), m_d->content, m_d->rootNode);
        m_d->status = ModuleFile::Compiled;
    }
}

ModuleFile::Status ModuleFile::status() const{
    return m_d->status;
}

const std::string &ModuleFile::name() const{
    return m_d->name;
}

std::string ModuleFile::fileName() const{
    return m_d->name + ".lv";
}

std::string ModuleFile::jsFileName() const{
    return fileName() + m_d->elementsModule->compiler()->outputExtension();
}

std::string ModuleFile::jsFilePath() const{
    return Path::join(m_d->elementsModule->compiler()->moduleBuildPath(m_d->elementsModule->module()), jsFileName());
}

std::string ModuleFile::filePath() const{
    return Path::join(m_d->elementsModule->module()->path(), fileName());
}

const std::list<ModuleFile::ModuleImport> &ModuleFile::imports() const{
    return m_d->imports;
}

void ModuleFile::resolveImport(const std::string &uri, ElementsModule::Ptr epl){
    for ( auto it = m_d->imports.begin(); it != m_d->imports.end(); ++it ){
        if ( it->uri == uri )
            it->module = epl;
    }
}

const ModuleFileDescriptor::Ptr &ModuleFile::descriptor() const{
    return m_d->descriptor;
}

void ModuleFile::addDependency(ModuleFile *dependency){
    if ( dependency == this )
        return;
    m_d->dependencies.push_back(dependency);
    dependency->m_d->dependents.push_back(this);

    PackageGraph::CyclesResult<ModuleFile*> cr = checkCycles(this);
    if ( cr.found() ){
        std::stringstream ss;

        for ( auto it = cr.path().begin(); it != cr.path().end(); ++it ){
            ModuleFile* n = *it;
            if ( it != cr.path().begin() )
                ss << " -> ";
            ss << n->name();
        }

        for ( auto it = m_d->dependencies.begin(); it != m_d->dependencies.end(); ++it ){
            if ( *it == dependency ){
                m_d->dependencies.erase(it);
                break;
            }
        }
        for ( auto it = dependency->m_d->dependents.begin(); it != dependency->m_d->dependents.end(); ++it ){
            if ( *it == this ){
                dependency->m_d->dependents.erase(it);
                break;
            }
        }

        THROW_EXCEPTION(lv::Exception, "Module file dependency cycle found: "  + ss.str(), lv::Exception::toCode("Cycle"));
    }
}

void ModuleFile::setCompilationData(CompilationData *cd){
    if ( m_d->compilationData )
        delete m_d->compilationData;
    m_d->compilationData = cd;
}

bool ModuleFile::hasDependency(ModuleFile *module, ModuleFile *dependency){
    for ( auto it = module->m_d->dependencies.begin(); it != module->m_d->dependencies.end(); ++it ){
        if ( *it == dependency )
            return true;
    }
    return false;
}

PackageGraph::CyclesResult<ModuleFile *> ModuleFile::checkCycles(ModuleFile *mf){
    std::list<ModuleFile*> path;
    path.push_back(mf);

    for ( auto it = mf->m_d->dependencies.begin(); it != mf->m_d->dependencies.end(); ++it ){
        PackageGraph::CyclesResult<ModuleFile*> cr = checkCycles(mf, *it, path);
        if ( cr.found() )
            return cr;
    }
    return PackageGraph::CyclesResult<ModuleFile*>(PackageGraph::CyclesResult<ModuleFile*>::NotFound);;
}

PackageGraph::CyclesResult<ModuleFile*> ModuleFile::checkCycles(
        ModuleFile *mf, ModuleFile *current, std::list<ModuleFile *> path)
{
    path.push_back(current);
    if ( current == mf )
        return PackageGraph::CyclesResult<ModuleFile*>(PackageGraph::CyclesResult<ModuleFile*>::Found, path);

    for ( auto it = current->m_d->dependencies.begin(); it != current->m_d->dependencies.end(); ++it ){
        PackageGraph::CyclesResult<ModuleFile*> cr = checkCycles(mf, *it, path);
        if ( cr.found() )
            return cr;
    }
    return PackageGraph::CyclesResult<ModuleFile*>(PackageGraph::CyclesResult<ModuleFile*>::NotFound);
}

ModuleFile *ModuleFile::createFromProgramNode(ElementsModule *module, const std::string &name, const std::string &content, ProgramNode *node, LanguageParser::AST *ast){
    auto descriptor = ModuleFileDescriptor::create(name);

    std::vector<BaseNode*> exports = module->compiler()->collectProgramExports(content, node);
    for ( auto val : exports ){
        if ( val->isNodeType<ComponentInstanceStatementNode>() ){
            auto expression = val->as<ComponentInstanceStatementNode>();
            ExportDescriptor expt(expression->name(content), ExportDescriptor::Element);
            descriptor->addExport(expt);

        } else if ( val->isNodeType<ComponentDeclarationNode>() ){
            auto expression = val->as<ComponentDeclarationNode>();
            ExportDescriptor expt(expression->name(content), ExportDescriptor::Component);
            descriptor->addExport(expt);
        }
    }

    auto mf = new ModuleFile(module, name, content, node, ast, descriptor);
    std::vector<ImportNode*> imports = node->imports();

    for ( auto val : imports ){
        ModuleFile::ModuleImport imp;
        imp.uri = val->path(content);
        imp.as = val->as(content);
        imp.isRelative = val->isRelative();

        descriptor->addDependency(ModuleFileDescriptor::ImportDependency(imp.uri));

        mf->m_d->imports.push_back(imp);
    }

    return mf;
}

ModuleFile *ModuleFile::createFromDescriptor(ElementsModule *module, const std::string &name, const ModuleFileDescriptor::Ptr &descriptor)
{
    auto mf = new ModuleFile(module, name, "", nullptr, nullptr, descriptor);
    mf->m_d->status = ModuleFile::Compiled;
    return mf;
}

ModuleFile::ModuleFile(
    ElementsModule *plugin, 
    const std::string &name, 
    const std::string &content, 
    ProgramNode *node, 
    LanguageParser::AST *ast,
    const ModuleFileDescriptor::Ptr& mfd)

    : m_d(new ModuleFilePrivate)
{
    std::string componentName = name;
    size_t extension = componentName.find(".lv");
    if ( extension != std::string::npos )
        componentName = componentName.substr(0, extension);

    m_d->elementsModule = plugin;
    m_d->name = componentName;
    m_d->status = ModuleFile::Initiaized;
    m_d->content = content;
    m_d->rootNode = node;
    m_d->ast = ast;
    m_d->descriptor = mfd;
    m_d->compilationData = nullptr;
}

}} // namespace lv, el

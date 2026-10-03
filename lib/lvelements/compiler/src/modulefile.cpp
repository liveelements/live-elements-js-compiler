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
#include "live/package.h"
#include "live/module.h"
#include "live/modulecontext.h"
#include "live/packagegraph.h"
#include "live/utf8.h"
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
void ModuleFile::resolveTypes(Compiler::Ptr compiler){
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
                    Occurrence::Space expSpace = Occurrence::SpaceValue | Occurrence::SpaceType;
                    if ( foundExp.kind() == ExportDescriptor::Interface || foundExp.kind() == ExportDescriptor::TypeAlias ){
                        expSpace = Occurrence::SpaceType;
                    } else if ( foundExp.kind() == ExportDescriptor::Function || foundExp.kind() == ExportDescriptor::Flow ){
                        expSpace = Occurrence::SpaceValue;
                    }
                    if ( ( impType.space & Occurrence::SpaceValue ) && !( expSpace & Occurrence::SpaceValue ) ){
                        // ImportType locations have 0-based columns, syntax exceptions report 1-based ones
                        const SourcePoint& location = impType.location;
                        throw SyntaxException(
                            Utf8("'%' refers to a type, but is being used as a value.").format(impType.name),
                            Exception::toCode("~Language"),
                            SourceRangeLocation(SourceRange(SourcePoint(location.line(), location.column() + 1, location.offset())), filePath()),
                            SOURCE_TRACE()
                        );
                    }
                    addDependency(foundFile);

                    m_d->rootNode->resolveImport(impType.importNamespace, impType.name, "./" + foundFile->jsFileName(), expSpace);
                    foundLocalExport = true;
                    
                    Occurrence::Space valueSpace = ( expSpace & Occurrence::SpaceValue ) ? impType.space : ( impType.space & ~Occurrence::SpaceMeta );
                    if ( valueSpace == 0 || ( valueSpace & ( Occurrence::SpaceValue | Occurrence::SpaceMeta ) ) != 0 ){
                        m_d->descriptor->addDependencyEntry(ModuleFileDescriptor::DependencyEntry(impType.name, foundFile->uri() + "@" + impType.name));
                    }
                }
            }

            if ( !foundLocalExport ){
                for( auto impIt = m_d->imports.begin(); impIt != m_d->imports.end(); ++impIt ){
                    if ( impIt->as == impType.importNamespace ){
                        if ( !impIt->module ){
                            THROW_EXCEPTION(Exception,
                                Utf8("ModuleFile::resolveTypes: import ElementsModule is null for namespace '%'.")
                                    .format(impType.importNamespace),
                                Exception::toCode("~NullPtr"));
                        }
                        auto foundExp = impIt->module->findExport(impType.name);
                        if ( foundExp.isValid() ){
                            if ( !foundExp.file() ){
                                THROW_EXCEPTION(Exception,
                                    Utf8("ModuleFile::resolveTypes: export '%' is not a module file (library export?). Namespace '%'.")
                                        .format(impType.name, impType.importNamespace),
                                    Exception::toCode("~Export"));
                            }
                            auto foundFile = impIt->module->findModuleFileByName(foundExp.file()->fileName().data());
                            if ( !foundFile ){
                                THROW_EXCEPTION(Exception, Utf8("Assertion: File not found: %").format(foundExp.file()->fileName()), Exception::toCode("NullPtr"));
                            }
                            Occurrence::Space expSpace = Occurrence::SpaceValue | Occurrence::SpaceType;
                            if ( foundExp.kind() == ExportDescriptor::Interface || foundExp.kind() == ExportDescriptor::TypeAlias ){
                                expSpace = Occurrence::SpaceType;
                            } else if ( foundExp.kind() == ExportDescriptor::Function || foundExp.kind() == ExportDescriptor::Flow ){
                                expSpace = Occurrence::SpaceValue;
                            }
                            if ( ( impType.space & Occurrence::SpaceValue ) && !( expSpace & Occurrence::SpaceValue ) ){
                                // ImportType locations have 0-based columns, syntax exceptions report 1-based ones
                                const SourcePoint& location = impType.location;
                                throw SyntaxException(
                                    Utf8("'%' refers to a type, but is being used as a value.").format(impType.name),
                                    Exception::toCode("~Language"),
                                    SourceRangeLocation(SourceRange(SourcePoint(location.line(), location.column() + 1, location.offset())), filePath()),
                                    SOURCE_TRACE()
                                );
                            }
                            
                            Occurrence::Space valueSpace = ( expSpace & Occurrence::SpaceValue ) ? impType.space : ( impType.space & ~Occurrence::SpaceMeta );
                            if ( valueSpace == 0 || ( valueSpace & ( Occurrence::SpaceValue | Occurrence::SpaceMeta ) ) != 0 ){
                                m_d->descriptor->addDependencyEntry(ModuleFileDescriptor::DependencyEntry(impType.name, foundFile->uri() + "@" + impType.name));
                            }
                            if ( impIt->isRelative ){
                                // this plugin to package
                                Utf8 currentImportId = m_d->elementsModule->module()->context()->importId;
                                Package::Ptr importedPkg = impIt->module->module()->context()->packageUnwrapped();
                                if ( !importedPkg ){
                                    THROW_EXCEPTION(Exception,
                                        Utf8("ModuleFile::resolveTypes (relative): Package expired or unset for imported module (path %, importId %).")
                                            .format(impIt->module->module()->path(), impIt->module->module()->context()->importId.data()),
                                        Exception::toCode("~NullPtr"));
                                }
                                Utf8 packageName = importedPkg->nameScopeAsPath();
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

                                m_d->rootNode->resolveImport(impType.importNamespace, impType.name, pluginToPackage + "/" + packageToNewPlugin + foundFile->jsFileName(), expSpace);

                            } else {
                                Package::Ptr importedPkg = impIt->module->module()->context()->packageUnwrapped();
                                if ( !importedPkg ){
                                    THROW_EXCEPTION(Exception,
                                        Utf8("ModuleFile::resolveTypes (absolute): Package expired or unset for imported module (path %, importId %).")
                                            .format(impIt->module->module()->path(), impIt->module->module()->context()->importId.data()),
                                        Exception::toCode("~NullPtr"));
                                }
                                Utf8 packageName = importedPkg->nameScopeAsPath();

                                std::string configPackageBuildPath = compiler->packageBuildPath();
                                std::string packageBuildPath = packageName.data() +
                                    (configPackageBuildPath.empty() ? "" : "/" + configPackageBuildPath);
                                std::string packageToPluginStr = impIt->module->pathInPackage();
                                std::string importPath = packageBuildPath + (packageToPluginStr.empty() ? "" : "/" + packageToPluginStr) + "/" + foundFile->jsFileName();

                                m_d->rootNode->resolveImport(impType.importNamespace, impType.name, importPath, expSpace);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    for ( JsImportNode* jsImport : m_d->rootNode->jsImports() ){
        std::string importPath = jsImport->importPath() ? BaseNode::slice(m_d->content, jsImport->importPath()) : "";
        if ( importPath.length() > 2 ){
            importPath = importPath.substr(1, importPath.length() - 2);
        }
        for ( const JsImportName& importName : jsImport->importNames() ){
            if ( importName.isTypeOnly() ){
                continue;
            }
            m_d->descriptor->addDependencyEntry(ModuleFileDescriptor::DependencyEntry(BaseNode::slice(m_d->content, importName.identifier()), importPath));
        }
    }

    if ( m_d->status != ModuleFile::Compiled ){
        m_d->status = ModuleFile::Resolved;
    }
}

void ModuleFile::compile(Compiler::Ptr compiler){
    if ( m_d->status != ModuleFile::Compiled ){
        if ( !m_d->rootNode ){
            THROW_EXCEPTION(lv::Exception, Utf8("Assertion: ModuleFile being compiled without parsed node."), Exception::toCode("~NullPtr"));
        }
        compiler->compileModuleFileToTarget(m_d->elementsModule->module(), filePath(), m_d->content, m_d->rootNode);
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
    return fileName() + m_d->elementsModule->outputExtension();
}

std::string ModuleFile::jsFilePath() const{
    return Path::join(m_d->elementsModule->buildLocation(), jsFileName());
}

std::string ModuleFile::uri() const{
    return m_d->elementsModule->uri() + "/" + fileName();
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

/**
 * \brief Returns whether the file's root holds only imports, flows, type aliases and interfaces
 *
 * Files loaded from a build descriptor have no parsed root, so their state is DeclarativeUnknown.
 */
ModuleFile::DeclarativeKind ModuleFile::declarativeKind() const{
    if ( !m_d->rootNode )
        return ModuleFile::DeclarativeUnknown;

    for ( BaseNode* child : m_d->rootNode->children() ){
        if ( !child->isNodeType<ImportNode>() &&
             !child->isNodeType<FlowFunctionDeclarationNode>() &&
             !child->isNodeType<TypeAliasDeclarationNode>() &&
             !child->isNodeType<InterfaceDeclarationNode>() )
        {
            return ModuleFile::NotDeclarative;
        }
    }
    return ModuleFile::Declarative;
}

void ModuleFile::addImport(const ModuleImport &imp){
    m_d->imports.push_back(imp);
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

ModuleFile *ModuleFile::createFromProgramNode(ElementsModule *module, Compiler::Ptr compiler, const std::string &name, const std::string &content, ProgramNode *node, LanguageParser::AST *ast){
    auto descriptor = ModuleFileDescriptor::create(name);

    std::vector<BaseNode*> exports = compiler->collectProgramExports(content, node);
    for ( auto val : exports ){
        if ( val->isNodeType<ComponentInstanceStatementNode>() ){
            auto expression = val->as<ComponentInstanceStatementNode>();
            ExportDescriptor expt(expression->name(content), ExportDescriptor::Element);
            descriptor->addExport(expt);

        } else if ( val->isNodeType<ComponentDeclarationNode>() ){
            auto expression = val->as<ComponentDeclarationNode>();
            ExportDescriptor expt(expression->name(content), ExportDescriptor::Component);
            descriptor->addExport(expt);
        } else if ( val->isNodeType<TypedFunctionDeclarationNode>() ){
            auto expression = val->as<TypedFunctionDeclarationNode>();
            std::string name = content.substr(
                expression->name()->startByte(),
                expression->name()->endByte() - expression->name()->startByte()
            );
            ExportDescriptor expt(name, ExportDescriptor::Function, expression->isMeta(), expression->isDeclared());
            descriptor->addExport(expt);
        } else if ( val->isNodeType<FlowFunctionDeclarationNode>() ){
            auto expression = val->as<FlowFunctionDeclarationNode>();
            ExportDescriptor expt(BaseNode::slice(content, expression->name()), ExportDescriptor::Flow, true);
            descriptor->addExport(expt);
        } else if ( val->isNodeType<InterfaceDeclarationNode>() ){
            auto expression = val->as<InterfaceDeclarationNode>();
            if ( expression->name() ){
                ExportDescriptor expt(BaseNode::slice(content, expression->name()), ExportDescriptor::Interface);
                descriptor->addExport(expt);
            }
        } else if ( val->isNodeType<TypeAliasDeclarationNode>() ){
            auto expression = val->as<TypeAliasDeclarationNode>();
            if ( expression->name() ){
                ExportDescriptor expt(BaseNode::slice(content, expression->name()), ExportDescriptor::TypeAlias);
                descriptor->addExport(expt);
            }
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

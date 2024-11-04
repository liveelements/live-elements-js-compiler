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

#include "elementsmodule.h"
#include "modulefile.h"
#include "live/modulecontext.h"
#include "live/exception.h"
#include "live/fileio.h"
#include "live/path.h"
#include "live/visuallog.h"
#include "live/mlnodetojson.h"
#include "live/elements/compiler/tracepointexception.h"

namespace lv{ namespace el{

/**
 * \class ElementsModule
 * \brief Container for module functionality on the Elements side.
 */

class ElementsModulePrivate{
public:
    ElementsModulePrivate(Engine* e)
        : engine(e), status(ElementsModule::Initialized){}

    Module::Ptr       module;
    Compiler::WeakPtr compiler;
    Engine*           engine;
    std::map<std::string, ModuleFile*> fileModules;
    std::list<ModuleLibrary*>          libraries;

    std::string            buildLocation;
    ElementsModule::Status status;

    ModuleDescriptor::Ptr  descriptor;
};


ElementsModule::ElementsModule(Module::Ptr module, Compiler::Ptr compiler, ModuleDescriptor::Ptr descriptor, Engine *engine)
    : m_d(new ElementsModulePrivate(engine))
{
    m_d->descriptor = descriptor;
    m_d->buildLocation = compiler->moduleBuildPath(module);
    m_d->compiler = compiler;
    m_d->module = module;
}


ElementsModule::~ElementsModule(){
#ifdef BUILD_ELEMENTS_ENGINE
//    for ( auto it = m_d->libraries.begin(); it != m_d->libraries.end(); ++it ){
//        delete *it;
//    }
#endif
    for ( auto it = m_d->fileModules.begin(); it != m_d->fileModules.end(); ++it ){
        ModuleFile* mf = it->second;
        delete mf;
    }
    delete m_d;
}

#ifdef BUILD_ELEMENTS_ENGINE
ElementsModule::Ptr ElementsModule::create(Module::Ptr module, Compiler::Ptr compiler, Engine *engine){
    return createImpl(module, compiler, engine);
}
#else
ElementsModule::Ptr ElementsModule::create(Module::Ptr, Compiler::Ptr, Engine *){
    THROW_EXCEPTION(lv::Exception, "This build does not support the engine. Rebuild with BUILD_ELEMENTS_ENGINE flag on.", lv::Exception::toCode("~Build"));
}
#endif

ElementsModule::Ptr ElementsModule::create(Module::Ptr module, Compiler::Ptr compiler){
    return createImpl(module, compiler, nullptr);
}

ElementsModule::Ptr ElementsModule::createImpl(Module::Ptr module, Compiler::Ptr compiler, Engine *engine){

    ModuleDescriptor::Ptr descriptor;

    auto package = module->context()->packageUnwrapped();
    if ( !package ){
        THROW_EXCEPTION(lv::Exception, Utf8("Assertion: Package null for module: \'%\'").format(module->path()), Exception::toCode("~NullPtr"));
    }
    if ( !package->release().empty() ){
        auto buildLocation = compiler->moduleBuildPath(module);
        if ( Path::exists(Path::join(buildLocation, ModuleDescriptor::buildFileName) ) ){
            std::string descriptorContent = compiler->fileIO()->readFromFile(
                Path::join(buildLocation, ModuleDescriptor::buildFileName)
            );

            MLNode descriptorNode;
            ml::fromJson(descriptorContent, descriptorNode);

            descriptor = ModuleDescriptor::createFromMlNode(descriptorNode);
        }
    }

    if ( descriptor ){ // read the descriptor and load files based on the descriptor
        ElementsModule::Ptr epl(new ElementsModule(module, compiler, descriptor, engine));
        for ( auto it = module->fileModules().begin(); it != module->fileModules().end(); ++it ){
            auto fileName = *it + ".lv";
            ModuleFileDescriptor::Ptr mfd = descriptor->findFile(fileName);
            if ( mfd ){
                ElementsModule::loadModuleFile(epl, fileName, mfd);
            }
        }
        epl->initializeLibraries(module->libraryModules());
        epl->m_d->status = ElementsModule::Compiled;
        return epl;
    }

    // no descriptor, create one
    descriptor = ModuleDescriptor::create(module->context()->importId);
    ElementsModule::Ptr epl(new ElementsModule(module, compiler, descriptor, engine));
    for ( auto it = module->fileModules().begin(); it != module->fileModules().end(); ++it ){
        ElementsModule::parseModuleFile(epl, *it + ".lv");
    }
    epl->initializeLibraries(module->libraryModules());

    return epl;
}


void ElementsModule::initializeLibraries(const std::list<std::string> &libs){
#ifdef BUILD_ELEMENTS_ENGINE
//    for ( auto it = libs.begin(); it != libs.end(); ++it ){
//        ModuleLibrary* lib = ModuleLibrary::load(m_d->engine, *it);
//        //TODO: Load library exports (instances and types)
//        m_d->libraries.push_back(lib);
//    }
#else
    if ( !libs.empty() )
        THROW_EXCEPTION(lv::Exception, Utf8("Module '%' contains libraries that cannot be loaded in this build.").format(m_d->module->name()), lv::Exception::toCode("~Build"));
#endif
}

ModuleFile *ElementsModule::loadModuleFile(ElementsModule::Ptr &epl, const std::string &name, const ModuleFileDescriptor::Ptr &mfd){
    auto it = epl->m_d->fileModules.find(name);
    if ( it != epl->m_d->fileModules.end() ){
        return it->second;
    }

    ModuleFile* mf = ModuleFile::createFromDescriptor(epl.get(), name, mfd);
    epl->m_d->fileModules[name] = mf;

    std::string currentUriName = epl->module()->context()->importId.data() + "." + name;

    auto mfdDependencies = mfd->dependencies();
    for ( auto dep : mfdDependencies ){
        Utf8 importUri = dep.importUri();
        Utf8 importUriFull;
        if ( importUri.startsWith('.') ){
            if ( epl->module()->context()->packageUnwrapped() == nullptr ){
                THROW_EXCEPTION(TracePointException, Utf8("Cannot import relative path withouth package: \'%\' in \'%\'").format(importUri, currentUriName), Exception::toCode("~Import"));
            }
            if ( epl->module()->context()->packageUnwrapped()->name() == "." ){
                THROW_EXCEPTION(TracePointException, Utf8("Cannot import relative path withouth package: \'%\' in \'%\'").format(importUri, currentUriName), Exception::toCode("~Import"));
            }

            importUriFull = epl->module()->context()->packageUnwrapped()->name() + (importUri == "." ? "" : importUri.data());
        } else {
            importUriFull = importUri;
        }

        try{
            ElementsModule::Ptr ep = Compiler::createAndResolveImportedModule(epl->compiler(), importUriFull.data(), epl->module(), epl->engine());
            if ( !ep ){
                THROW_EXCEPTION(TracePointException, Utf8("Failed to find module \'%\' imported in \'%\'").format(importUriFull, currentUriName), Exception::toCode("~Import"));
            }
        } catch ( TracePointException& e ){
            throw TracePointException(e, Utf8(" - Imported \'%\' from \'%\'").format(importUri, currentUriName));
        }
    }

    return mf;
}

ModuleFile *ElementsModule::parseModuleFile(ElementsModule::Ptr &epl, const std::string &name){
    auto it = epl->m_d->fileModules.find(name);
    if ( it != epl->m_d->fileModules.end() ){
        return it->second;
    }

    Compiler::WeakPtr wcompiler = epl->m_d->compiler;
    Compiler::Ptr compiler = wcompiler.lock();
    if ( !compiler ){
        THROW_EXCEPTION(lv::Exception, Utf8("Assertion: Compiler has been released."), Exception::toCode("~Compiler"));
    }

    std::string filePath = Path::join(epl->module()->path(), name);
    if ( !Path::exists(filePath) ){
        THROW_EXCEPTION(
            lv::Exception,
            Utf8("Module file '%' does not exit. (Defined in '%')").format(filePath, epl->module()->filePath()),
            lv::Exception::toCode("~Module")
        );
    }

    std::string content = compiler->fileIO()->readFromFile(filePath);

    std::string componentName = name;
    size_t i = componentName.find(".lv");
    if ( i != std::string::npos ){
        componentName = name.substr(0, i);
    }

    LanguageParser::AST* ast = compiler->parser()->parse(content);
    ProgramNode* pn = compiler->parseProgramNodes(filePath, componentName, ast);

    ModuleFile* mf = ModuleFile::createFromProgramNode(epl.get(), name, content, pn, ast);
    epl->m_d->fileModules[name] = mf;
    epl->m_d->descriptor->addModuleFileDescriptor(mf->descriptor());

    std::string currentUriName = epl->module()->context()->importId.data() + "." + name;

    auto mfImports = mf->imports();
    for ( auto it = mfImports.begin(); it != mfImports.end(); ++it ){

        ModuleFile::ModuleImport& imp = *it;

        if ( imp.isRelative ){
            if ( epl->module()->context()->packageUnwrapped() == nullptr ){
                THROW_EXCEPTION(TracePointException, Utf8("Cannot import relative path withouth package: \'%\' in \'%\'").format(imp.uri, currentUriName), Exception::toCode("~Import"));
            }
            if ( epl->module()->context()->packageUnwrapped()->name() == "." ){
                THROW_EXCEPTION(TracePointException, Utf8("Cannot import relative path withouth package: \'%\' in \'%\'").format(imp.uri, currentUriName), Exception::toCode("~Import"));
            }

            std::string importUri = epl->module()->context()->packageUnwrapped()->name() + (imp.uri == "." ? "" : imp.uri);
            if ( importUri == epl->module()->context()->importId.data() ){
                THROW_EXCEPTION(
                    TracePointException,
                    Utf8("Cannot import own module ('import %') in file '%'.").format(imp.uri, filePath),
                    Exception::toCode("Import")
                );
            }

            try{
                ElementsModule::Ptr ep = Compiler::createAndResolveImportedModule(epl->compiler(), importUri, epl->module(), epl->engine());
                if ( !ep ){
                    THROW_EXCEPTION(TracePointException, Utf8("Failed to find module \'%\' imported in \'%\'").format(imp.uri, currentUriName), Exception::toCode("~Import"));
                }
                mf->resolveImport(imp.uri, ep);
            } catch ( TracePointException& e ){
                throw TracePointException(e, Utf8(" - Imported \'%\' from \'%\'").format(importUri, currentUriName));
            }

        } else {
            if ( imp.uri == epl->module()->context()->importId.data() ){
                THROW_EXCEPTION(
                    TracePointException,
                    Utf8("Cannot import own module ('import %') in file '%'.").format(imp.uri, filePath),
                    Exception::toCode("Import")
                );
            }
            try{
                ElementsModule::Ptr ep = Compiler::createAndResolveImportedModule(epl->compiler(), it->uri, epl->module(), epl->engine());
                if ( !ep ){
                    THROW_EXCEPTION(lv::Exception, Utf8("Failed to find module \'%\' imported in \'%\'").format(imp.uri, currentUriName), Exception::toCode("~Import"));
                }
                mf->resolveImport(imp.uri, ep);
            } catch ( TracePointException& e ){
                throw TracePointException(e, Utf8(" - Imported \'%\' from \'%\'").format(imp.uri, currentUriName));
            }
        }
    }
    return mf;
}

ModuleFile *ElementsModule::findModuleFileByName(const std::string &name) const{
    auto it = m_d->fileModules.find(name);
    if ( it != m_d->fileModules.end() ){
        return it->second;
    }
    return nullptr;
}

ModuleFile *ElementsModule::moduleFileBypath(const std::string &path) const{
    std::string pathNormalized = Path::resolve(path);
    for ( auto it = m_d->fileModules.begin(); it != m_d->fileModules.end(); ++it ){
        ModuleFile* mf = it->second;
        if ( mf->filePath() == pathNormalized )
            return mf;
    }
    return nullptr;
}

const Module::Ptr& ElementsModule::module() const{
    return m_d->module;
}

void ElementsModule::compile(){
    if ( m_d->status == ElementsModule::Compiled || m_d->status == ElementsModule::Compiling )
        return;

    // resolve types
    if ( m_d->status != ElementsModule::Resolved && m_d->status != ElementsModule::Parsed){
        for ( auto it = m_d->fileModules.begin(); it != m_d->fileModules.end(); ++it ){
            it->second->resolveTypes();
        }
        m_d->status = ElementsModule::Resolved;
    }

    // compile dependencies
    for ( auto it = m_d->fileModules.begin(); it != m_d->fileModules.end(); ++it ){
        ModuleFile* mf = it->second;

        auto mfImports = mf->imports();
        for ( auto mit = mfImports.begin(); mit != mfImports.end(); ++mit ){
            if ( mit->module == nullptr ){
                THROW_EXCEPTION(lv::Exception, Utf8("Import not resolved \'%\' when compiling \'%\'").format(mit->uri, mf->filePath()), lv::Exception::toCode("~Import"));
            }
            mit->module->compile();
        }
    }

    for ( auto it = m_d->fileModules.begin(); it != m_d->fileModules.end(); ++it ){
        it->second->compile();
    }

    auto assets = m_d->module->assets();
    std::string moduleBuildPath = compiler()->moduleBuildPath(m_d->module);

    for ( auto it = assets.begin(); it != assets.end(); ++it ){
        std::string assetPath = Path::join(m_d->module->path(), *it);
        std::string resultPath = Path::join(moduleBuildPath, *it);
        if (Path::exists(resultPath))
            Path::remove(resultPath);
        Path::copyFile(assetPath, resultPath, Path::OverwriteExisting);
    }

    // write compile info
    MLNode descriptorData = m_d->descriptor->toMLNode();
    std::string descriptorContent;
    ml::toJson(descriptorData, descriptorContent);

    std::string descriptorPath = Path::join(m_d->buildLocation, ModuleDescriptor::buildFileName);

    Compiler::Ptr compiler = m_d->compiler.lock();
    if ( !compiler ){
        THROW_EXCEPTION(lv::Exception, Utf8("Assertion: ElementsModule: Compiler pointer has been released."), Exception::toCode("~Compiler"));
    }

    vlog().v() << "ElementsModule: Saving descriptor:" << descriptorPath;

    compiler->fileIO()->writeToFile(descriptorPath, descriptorContent);
    
    m_d->status = ElementsModule::Compiled;
}

Compiler::Ptr ElementsModule::compiler() const{
    Compiler::Ptr compiler = m_d->compiler.lock();
    if ( !compiler ){
        THROW_EXCEPTION(lv::Exception, Utf8("ElementsModule: Compiler has been released."), Exception::toCode("~Compiler"));
    }
    return compiler;
}

Engine *ElementsModule::engine() const{
    return m_d->engine;
}

ModuleDescriptor::ExportLink ElementsModule::findExport(const std::string &name) const{
    return m_d->descriptor->findExportByName(name);
}

ModuleFile *ElementsModule::moduleFileByName(const std::string &fileName) const{
    auto it = m_d->fileModules.find(fileName);
    if ( it != m_d->fileModules.end() ){
        return it->second;
    }
    return nullptr;
}

const std::string &ElementsModule::buildLocation() const{
    return m_d->buildLocation;
}

ElementsModule::Status ElementsModule::status() const{
    return m_d->status;
}

const std::list<ModuleLibrary *> &ElementsModule::libraryModules() const{
    return m_d->libraries;
}

}} // namespace lv, el

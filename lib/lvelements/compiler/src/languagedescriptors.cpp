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

#include "languagedescriptors.h"
#include <algorithm>

namespace lv{ namespace el{

ExportDescriptor::ExportDescriptor(const Utf8& name, ExportDescriptor::Kind kind )
    : m_name(name), m_kind(kind)
{}

Utf8 ExportDescriptor::kindString() const{
    return m_kind == ExportDescriptor::Component ? "component" : "element";
}

ExportDescriptor::Kind ExportDescriptor::kindFromString(const Utf8 &str){
    if ( str == "component" ){
        return ExportDescriptor::Component;
    } else if ( str == "element" ){
        return ExportDescriptor::Element;
    } else {
        THROW_EXCEPTION(Exception, Utf8("ExportDescriptor: Invalid kind string: \'%\'").format(str), Exception::toCode("~Kind"));
    }
    return ExportDescriptor::Component;
}

ModuleFileDescriptor::Ptr ModuleFileDescriptor::create(const Utf8& fileName){
    return ModuleFileDescriptor::Ptr(new ModuleFileDescriptor(fileName));
}

ModuleFileDescriptor::Ptr ModuleFileDescriptor::createFromMLNode(const MLNode &node){
    if ( node["_"].asString() != "ModuleFile" ){
        THROW_EXCEPTION(
            Exception, 
            Utf8("Attempting to create ModuleFileDescriptor from differnt node type: %").format(node.toString()), 
            Exception::toCode("~NodeType")
        );
    }

    ModuleFileDescriptor::Ptr mfd = ModuleFileDescriptor::create(node["fileName"].asString());

    MLNode::ArrayType exports = node["exports"].asArray();
    for ( auto it = exports.begin(); it != exports.end(); ++it ){
        ExportDescriptor::Kind kind = ExportDescriptor::kindFromString((*it)["kind"].asString());
        mfd->m_exports.push_back(ExportDescriptor((*it)["name"].asString(), kind));
    }

    MLNode::ArrayType deps = node["dependencies"].asArray();
    for ( auto it = deps.begin(); it != deps.end(); ++it ){
        mfd->m_dependencies.push_back(ModuleFileDescriptor::ImportDependency((*it)["importUri"].asString()));
    }

    return mfd;
}

void ModuleFileDescriptor::addExport(const ExportDescriptor &edsc){
    m_exports.push_back(edsc);
}

void ModuleFileDescriptor::addDependency(const ImportDependency& dep){
    m_dependencies.push_back(dep);
}

MLNode ModuleFileDescriptor::toMLNode() const
{
    MLNode result(MLNode::Object);
    result["_"] = "ModuleFile";
    result["fileName"] = m_fileName.data();

    MLNode exports(MLNode::Array);
    for ( auto it = m_exports.begin(); it != m_exports.end(); ++it ){
        MLNode expt(MLNode::Object);
        expt["name"] = it->name().data();
        expt["kind"] = it->kindString().data();
        exports.append(expt);
    }
    result["exports"] = exports;

    MLNode deps(MLNode::Array);
    for ( auto it = m_dependencies.begin(); it != m_dependencies.end(); ++it ){
        MLNode dep(MLNode::Object);
        dep["importUri"] = it->importUri().data();
        deps.append(dep);
    }
    result["dependencies"] = deps;

    return result;
}

const char* ModuleDescriptor::buildFileName = "__module__.lv.json";

ModuleDescriptor::Ptr ModuleDescriptor::create(const Utf8& uri){
    return ModuleDescriptor::Ptr(new ModuleDescriptor(uri));
}
void ModuleDescriptor::addModuleFileDescriptor(const ModuleFileDescriptor::Ptr &mfd){
    for ( auto it = mfd->exports().begin(); it != mfd->exports().end(); ++it ){
        m_exports.push_back(ModuleDescriptor::ExportLink(it->name(), it->kind(), mfd));
    }
}

void ModuleDescriptor::addModuleLibraryDescriptor(const ModuleLibraryDescriptor::Ptr &){}

ModuleDescriptor::ExportLink ModuleDescriptor:: findExportByName(const Utf8 &name) const
{
    for ( auto it = m_exports.begin(); it != m_exports.end(); ++it ){
        const auto& el = *it;
        if ( el.name() == name ){
            return el;
        }
    }
    return ModuleDescriptor::ExportLink();
}

/**
 * Returns a list of unique files
 */
std::vector<ModuleFileDescriptor::Ptr> ModuleDescriptor::files() const
{
    std::vector<ModuleFileDescriptor::Ptr> result;

    for ( auto it = m_exports.begin(); it != m_exports.end(); ++it ){
        const auto& el = *it;
        if ( el.file() ){
            auto foundIt = std::find_if(result.begin(), result.end(), [el](const ModuleFileDescriptor::Ptr& mfd) {
                return mfd->fileName() == el.file()->fileName();
            });
            if ( foundIt == result.end() ){
                result.push_back(el.file());
            }
        }
    }
    return result;
}

ModuleFileDescriptor::Ptr ModuleDescriptor::findFile(const Utf8 &name) const
{
    for ( auto it = m_exports.begin(); it != m_exports.end(); ++it ){
        const auto& el = *it;
        if ( el.file() && el.file()->fileName() == name ){
            return el.file();
        }
    }
    return nullptr;
}

/**
 * Returns a list of unique libraries
 */
std::vector<ModuleLibraryDescriptor::Ptr> ModuleDescriptor::libraries() const
{
    std::vector<ModuleLibraryDescriptor::Ptr> result;

    for ( auto it = m_exports.begin(); it != m_exports.end(); ++it ){
        const auto& el = *it;
        if ( el.library() ){
            auto foundIt = std::find_if(result.begin(), result.end(), [el](const ModuleLibraryDescriptor::Ptr& mfd) {
                return mfd->name() == el.library()->name();
            });
            if ( foundIt == result.end() ){
                result.push_back(el.library());
            }
        }
    }
    return result;
}


ModuleDescriptor::Ptr ModuleDescriptor::createFromMlNode(const MLNode &node){
    if ( node["_"].asString() != "Module" ){
        THROW_EXCEPTION(
            Exception, 
            Utf8("Attempting to create ModuleDescriptor from differnt node type: %").format(node.toString()), 
            Exception::toCode("~NodeType")
        );
    }

    ModuleDescriptor::Ptr md = ModuleDescriptor::create(node["uri"].asString());
    MLNode::ArrayType expts = node["exports"].asArray();
    for ( auto it = expts.begin(); it != expts.end(); ++it ){
        const auto& expt = *it;
        if ( expt["_"].asString() == "ModuleLibrary" ){
            auto mdl = ModuleLibraryDescriptor::createFromMLNode(expt);
            md->addModuleLibraryDescriptor(mdl);
        } else if ( expt["_"].asString() == "ModuleFile" ){
            auto mdf = ModuleFileDescriptor::createFromMLNode(expt);
            md->addModuleFileDescriptor(mdf);
        } else {
            THROW_EXCEPTION(
                Exception, 
                Utf8("ModuleDescriptor: Unknown node type: %").format(expt["_"].toString()), 
                Exception::toCode("~NodeType")
            );
        }
    }
    return md;
}


MLNode ModuleDescriptor::toMLNode() const
{
    MLNode result(MLNode::Object);
    result["_"] = "Module";
    result["uri"] = m_uri.data();

    MLNode exports(MLNode::Array);

    auto fileExports = files();
    for ( size_t i = 0; i < fileExports.size(); ++i ){
        const auto& fe = fileExports[i];
        exports.append(fe->toMLNode());
    }

    auto libraryExports = libraries();
    for ( size_t i = 0; i < libraryExports.size(); ++i ){
        const auto& le = libraryExports[i];
        exports.append(le->toMLNode());
    }

    result["exports"] = exports;
    return result;
}

ModuleLibraryDescriptor::Ptr ModuleLibraryDescriptor::create(const Utf8 &name)
{
    return ModuleLibraryDescriptor::Ptr(new ModuleLibraryDescriptor(name));
}

ModuleLibraryDescriptor::Ptr ModuleLibraryDescriptor::createFromMLNode(const MLNode &node)
{
    if ( node["_"].asString() != "ModuleLibrary" ){
        THROW_EXCEPTION(
            Exception, 
            Utf8("Attempting to create ModuleLibraryDescriptor from differnt node type: %").format(node.toString()), 
            Exception::toCode("~NodeType")
        );
    }

    return ModuleLibraryDescriptor::create(node["name"].asString());
}

MLNode ModuleLibraryDescriptor::toMLNode() const
{
    MLNode result(MLNode::Object);
    result["_"] = "ModuleLibrary";
    result["name"] = m_name.data();
    return result;
}
    }
} // namespace lv, el
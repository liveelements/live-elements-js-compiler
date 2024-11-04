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

#ifndef LVLANGUAGEDESCRIPTORS_H
#define LVLANGUAGEDESCRIPTORS_H

#include "live/elements/compiler/lvelcompilerglobal.h"
#include "live/utf8.h"
#include "live/mlnode.h"

namespace lv{ namespace el{

class LV_ELEMENTS_COMPILER_EXPORT ExportDescriptor{
public:
    enum Kind{
        Component,
        Element
    };

public:
    ExportDescriptor(const Utf8& name, Kind kind);

    const Utf8& name() const{ return m_name; }
    Kind kind() const{ return m_kind; }

    Utf8 kindString() const;
    static Kind kindFromString(const Utf8& str);

private:
    Utf8 m_name;
    Kind m_kind;
};

class LV_ELEMENTS_COMPILER_EXPORT ModuleLibraryDescriptor{

    DISABLE_COPY(ModuleLibraryDescriptor);

public:
    typedef std::shared_ptr<ModuleLibraryDescriptor>       Ptr;
    typedef std::shared_ptr<const ModuleLibraryDescriptor> ConstPtr;

public:
    static Ptr create(const Utf8& name);
    static Ptr createFromMLNode(const MLNode& node);

    const Utf8& name() const{ return m_name; }
    MLNode toMLNode() const;

private:
    ModuleLibraryDescriptor(const Utf8& name) : m_name(name){}

    Utf8 m_name;
};



class LV_ELEMENTS_COMPILER_EXPORT ModuleFileDescriptor{

    DISABLE_COPY(ModuleFileDescriptor);

public:
    class ImportDependency{
    public:
        ImportDependency(const Utf8& importUri) : m_importUri(importUri){}
        
        const Utf8& importUri() const{ return m_importUri; }

    private:
        Utf8 m_importUri;
    };

public:
    typedef std::shared_ptr<ModuleFileDescriptor>       Ptr;
    typedef std::shared_ptr<const ModuleFileDescriptor> ConstPtr;

public:
    static Ptr create(const Utf8& fileName);
    static Ptr createFromMLNode(const MLNode& node);

    const Utf8& fileName() const{ return m_fileName; }
    const std::vector<ExportDescriptor>& exports() const{ return m_exports; }
    const std::vector<ImportDependency>& dependencies() const{ return m_dependencies; }

    void addExport(const ExportDescriptor& dsc);
    void addDependency(const ImportDependency& dep);

    MLNode toMLNode() const;

private:
    ModuleFileDescriptor(const Utf8& fileName) : m_fileName(fileName){}

private:
    Utf8                          m_fileName;
    std::vector<ExportDescriptor> m_exports;
    std::vector<ImportDependency> m_dependencies;
};

class LV_ELEMENTS_COMPILER_EXPORT ModuleDescriptor{

    DISABLE_COPY(ModuleDescriptor);

public:
    class ExportLink{

    public:
        ExportLink(){}

        ExportLink(const Utf8& name, ExportDescriptor::Kind kind, ModuleFileDescriptor::Ptr mfd)
            : m_name(name), m_kind(kind), m_file(mfd)
        {}

        ExportLink(const Utf8& name, ExportDescriptor::Kind kind, ModuleLibraryDescriptor::Ptr mdd)
            : m_name(name), m_kind(kind), m_library(mdd)
        {}

        const Utf8& name() const{ return m_name; }
        ExportDescriptor::Kind kind() const{ return m_kind; }
        const ModuleFileDescriptor::Ptr& file() const{ return m_file; }
        const ModuleLibraryDescriptor::Ptr& library() const{ return m_library; }

        bool isEmpty() const{ return m_file == nullptr && m_library == nullptr; }
        bool isValid() const{ return !isEmpty(); }

    private:
        Utf8                         m_name;
        ExportDescriptor::Kind       m_kind;
        ModuleFileDescriptor::Ptr    m_file;
        ModuleLibraryDescriptor::Ptr m_library;
    };

public:
    typedef std::shared_ptr<ModuleDescriptor>       Ptr;
    typedef std::shared_ptr<const ModuleDescriptor> ConstPtr;
    
    /** Name for the module build file */
    static const char* buildFileName;

public:
    static Ptr create(const Utf8& uri);
    static ModuleDescriptor::Ptr createFromMlNode(const MLNode& node);

    const Utf8& uri() const{ return m_uri; }
    const std::vector<ExportLink>& exports(){ return m_exports; }

    void addModuleFileDescriptor(const ModuleFileDescriptor::Ptr& mfd);
    void addModuleLibraryDescriptor(const ModuleLibraryDescriptor::Ptr& mld);

    ExportLink findExportByName(const Utf8& name) const;
    ModuleFileDescriptor::Ptr findFile(const Utf8& name) const;

    std::vector<ModuleFileDescriptor::Ptr> files() const;
    std::vector<ModuleLibraryDescriptor::Ptr> libraries() const;

    MLNode toMLNode() const;

private:
    ModuleDescriptor(const Utf8& uri) : m_uri(uri){}

private:
    Utf8                    m_uri;
    std::vector<ExportLink> m_exports;
};


} } // namespace lv, el

#endif // LVLANGUAGEDESCRIPTORS_H
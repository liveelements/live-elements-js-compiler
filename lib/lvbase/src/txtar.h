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

#ifndef LVTXTAR_H
#define LVTXTAR_H

#include "live/lvbaseglobal.h"

#include <string>
#include <vector>

namespace lv{

class VisualLog;

/**
  \class lv::Txtar
  \brief Immutable text archive for test fixtures and small file trees.
  \ingroup lvbase
 */
class LV_BASE_EXPORT Txtar{

public:
    /**
      \class lv::Txtar::Entry
      \brief A single file stored inside a txtar archive.
      \ingroup lvbase
     */
    class LV_BASE_EXPORT Entry{
    public:
        Entry(const std::string& name, const std::string& content = "");

        const std::string& name() const{ return m_name; }
        const std::string& content() const{ return m_content; }

    private:
        std::string m_name;
        std::string m_content;
    };

    /**
      \class lv::Txtar::ExtractionGuard
      \brief Removes an extracted txtar tree when the guard goes out of scope.
      \ingroup lvbase
     */
    class LV_BASE_EXPORT ExtractionGuard{
    public:
        ExtractionGuard(ExtractionGuard&& other);
        ~ExtractionGuard();

        const std::string& path() const{ return m_path; }

    private:
        friend class Txtar;
        explicit ExtractionGuard(const std::string& path);
        ExtractionGuard(const ExtractionGuard&) = delete;
        ExtractionGuard& operator = (const ExtractionGuard&) = delete;

        std::string m_path;
    };

public:
    Txtar();
    Txtar(const std::vector<Entry>& entries, const std::string& comment = "");

    static Txtar parse(const std::string& contents);
    static Txtar parseFile(const std::string& filePath);
    static Txtar fromDirectory(const std::string& directoryPath);

    const std::vector<Entry>& entries() const{ return m_entries; }
    const std::string& comment() const{ return m_comment; }

    std::string format() const;
    std::string toString() const;

    void extract(const std::string& directoryPath) const;

    /**
      \brief Extracts into \p directoryPath after removing any existing directory at that path.
     */
    ExtractionGuard extractTemp(const std::string& directoryPath) const;

private:
    static std::string appendNewLine(const std::string& data);
    static bool isMarker(const std::string& line);
    static size_t findFileMarkerAfter(const std::string& contents, size_t offset);
    static std::vector<Entry> collectDirectoryEntries(
        const std::string& root,
        const std::string& current
    );

    std::vector<Entry> m_entries;
    std::string m_comment;
};

LV_BASE_EXPORT VisualLog &operator <<(VisualLog &vl, const Txtar &value);

}// namespace

#endif // LVTXTAR_H

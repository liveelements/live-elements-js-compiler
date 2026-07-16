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

#include "txtar.h"
#include "live/directory.h"
#include "live/exception.h"
#include "live/fileio.h"
#include "live/path.h"
#include "live/utf8.h"
#include "live/visuallog.h"

#include <algorithm>

namespace lv{

Txtar::Entry::Entry(const std::string &name, const std::string &content)
    : m_name(name)
    , m_content(content)
{
}

Txtar::ExtractionGuard::ExtractionGuard(const std::string &path)
    : m_path(path)
{
}

Txtar::ExtractionGuard::ExtractionGuard(Txtar::ExtractionGuard &&other)
    : m_path(other.m_path)
{
    other.m_path.clear();
}

Txtar::ExtractionGuard::~ExtractionGuard(){
    if ( !m_path.empty() && Path::exists(m_path) ){
        Path::remove(m_path);
    }
}

Txtar::Txtar(){
}

Txtar::Txtar(const std::vector<Txtar::Entry> &entries, const std::string &comment)
    : m_entries(entries)
    , m_comment(comment)
{
}

Txtar Txtar::parse(const std::string &contents){
    std::vector<Entry> entries;
    std::string comment;

    size_t marker = findFileMarkerAfter(contents, 0);
    if ( marker == std::string::npos ){
        comment = contents;
        return Txtar(entries, comment);
    }

    comment = contents.substr(0, marker);

    while ( marker != std::string::npos ){
        size_t lineEnd = contents.find('\n', marker);
        std::string markerLine = lineEnd == std::string::npos
            ? contents.substr(marker)
            : contents.substr(marker, lineEnd - marker);

        std::string name = markerLine.substr(3, markerLine.length() - 6);
        Utf8::trim(name);

        size_t contentStart = lineEnd == std::string::npos ? contents.size() : lineEnd + 1;
        size_t nextMarker = findFileMarkerAfter(contents, contentStart);
        size_t contentEnd = nextMarker == std::string::npos ? contents.size() : nextMarker;

        entries.push_back(Entry(name, contents.substr(contentStart, contentEnd - contentStart)));
        marker = nextMarker;
    }

    return Txtar(entries, comment);
}

Txtar Txtar::parseFile(const std::string &filePath){
    FileIO fileIo;
    return parse(fileIo.readFromFile(filePath));
}

Txtar Txtar::fromDirectory(const std::string &directoryPath){
    if ( !Path::exists(directoryPath) ){
        THROW_EXCEPTION(
            lv::Exception,
            Utf8("Failed to archive directory: %. Path does not exist.").format(directoryPath),
            lv::Exception::toCode("~Txtar")
        );
    }
    if ( !Path::isDir(directoryPath) ){
        THROW_EXCEPTION(
            lv::Exception,
            Utf8("Failed to archive directory: %. Path is not a directory.").format(directoryPath),
            lv::Exception::toCode("~Txtar")
        );
    }

    return Txtar(collectDirectoryEntries(directoryPath, directoryPath));
}

std::string Txtar::format() const{
    std::string result = appendNewLine(m_comment);
    for ( const Entry& entry : m_entries ){
        result += "-- " + entry.name() + " --\n";
        result += appendNewLine(entry.content());
    }
    return result;
}

std::string Txtar::toString() const{
    return format();
}

void Txtar::extract(const std::string &directoryPath) const{
    FileIO fileIo;
    Path::createDirectories(directoryPath);
    std::string rootPath = Path::resolve(directoryPath);

    for ( const Entry& entry : m_entries ){
        std::string normalizedName = entry.name();
        Utf8::replaceAll(normalizedName, "\\", "/");

        if ( Path::isAbsolute(normalizedName) ){
            THROW_EXCEPTION(
                lv::Exception,
                Utf8("Refusing to extract entry outside target directory: %").format(entry.name()),
                lv::Exception::toCode("~Txtar")
            );
        }

        std::string outputPath = Path::join(rootPath, normalizedName);
        std::string targetParent = Path::parent(outputPath);
        Path::createDirectories(targetParent);

        std::string resolvedParent = Path::resolve(targetParent);
        std::string relativeParent = Path::relativePath(rootPath, resolvedParent);
        if ( resolvedParent != rootPath &&
             relativeParent.find("..") == 0 &&
             (relativeParent.length() == 2 || relativeParent[2] == '/') ){
            THROW_EXCEPTION(
                lv::Exception,
                Utf8("Refusing to extract entry outside target directory: %").format(entry.name()),
                lv::Exception::toCode("~Txtar")
            );
        }

        fileIo.writeToFile(outputPath, entry.content());
    }
}

Txtar::ExtractionGuard Txtar::extractTemp(const std::string &directoryPath) const{
    if ( Path::exists(directoryPath) ){
        Path::remove(directoryPath);
    }
    extract(directoryPath);
    return ExtractionGuard(directoryPath);
}

std::string Txtar::appendNewLine(const std::string &data){
    if ( data.empty() || data.back() == '\n' ){
        return data;
    }
    return data + "\n";
}

bool Txtar::isMarker(const std::string &line){
    return line.length() >= 6 &&
        line.compare(0, 3, "-- ") == 0 &&
        line.compare(line.length() - 3, 3, " --") == 0;
}

size_t Txtar::findFileMarkerAfter(const std::string &contents, size_t offset){
    if ( offset >= contents.size() ){
        return std::string::npos;
    }

    if ( offset == 0 ){
        size_t firstLineEnd = contents.find('\n');
        std::string firstLine = firstLineEnd == std::string::npos
            ? contents
            : contents.substr(0, firstLineEnd);
        if ( isMarker(firstLine) ){
            return 0;
        }
    }

    size_t marker = contents.find("\n-- ", offset == 0 ? 0 : offset - 1);
    while ( marker != std::string::npos ){
        size_t lineStart = marker + 1;
        size_t lineEnd = contents.find('\n', lineStart);
        std::string line = lineEnd == std::string::npos
            ? contents.substr(lineStart)
            : contents.substr(lineStart, lineEnd - lineStart);
        if ( isMarker(line) ){
            return lineStart;
        }
        marker = contents.find("\n-- ", lineStart);
    }

    return std::string::npos;
}

std::vector<Txtar::Entry> Txtar::collectDirectoryEntries(
    const std::string &root,
    const std::string &current)
{
    std::vector<Txtar::Entry> entries;
    std::vector<std::string> children;
    Directory::Iterator it = Directory::iterate(current);
    while ( !it.isEnd() ){
        children.push_back(it.path());
        it.next();
    }

    std::sort(children.begin(), children.end());

    for ( const std::string& child : children ){
        if ( Path::isDir(child) ){
            std::vector<Txtar::Entry> childEntries = collectDirectoryEntries(root, child);
            entries.insert(entries.end(), childEntries.begin(), childEntries.end());
        } else if ( Path::isFile(child) ){
            std::string relativePath = Path::relativePath(root, child);
            Utf8::replaceAll(relativePath, "\\", "/");
            FileIO fileIo;
            entries.push_back(Entry(relativePath, fileIo.readFromFile(child)));
        }
    }

    return entries;
}

VisualLog &operator <<(VisualLog &vl, const Txtar &value){
    vl << value.format();
    return vl;
}

}// namespace

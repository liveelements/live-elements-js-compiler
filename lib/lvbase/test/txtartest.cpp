/****************************************************************************
**
** Copyright (C) 2022 Dinu SV.
**
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

#include "catch_library.h"
#include "live/exception.h"
#include "live/fileio.h"
#include "live/path.h"
#include "live/txtar.h"

#include <vector>

using namespace lv;

namespace{

std::string workPath(){
    return Path::temporaryDirectory();
}

}

TEST_CASE( "Txtar Test", "[Txtar]" ) {
    SECTION("Parse"){
        std::string source =
            "Archive comment\n"
            "second line\n"
            "-- a.txt --\n"
            "alpha\n"
            "-- dir/b.txt --\n"
            "beta\n";

        Txtar archive = Txtar::parse(source);
        REQUIRE(archive.comment() == "Archive comment\nsecond line\n");
        REQUIRE(archive.entries().size() == 2);
        REQUIRE(archive.entries()[0].name() == "a.txt");
        REQUIRE(archive.entries()[0].content() == "alpha\n");
        REQUIRE(archive.entries()[1].name() == "dir/b.txt");
        REQUIRE(archive.entries()[1].content() == "beta\n");
    }

    SECTION("Parse Edge Cases"){
        Txtar noTrailing = Txtar::parse(
            "-- a.txt --\n"
            "alpha"
        );
        REQUIRE(noTrailing.comment().empty());
        REQUIRE(noTrailing.entries().size() == 1);
        REQUIRE(noTrailing.entries()[0].content() == "alpha");
        REQUIRE(noTrailing.format() == "-- a.txt --\nalpha\n");

        Txtar markerLike = Txtar::parse(
            "-- a.txt --\n"
            "--x --\n"
            "-- x\n"
            "-- b.txt -- not a marker\n"
        );
        REQUIRE(markerLike.entries().size() == 1);
        REQUIRE(markerLike.entries()[0].content() == "--x --\n-- x\n-- b.txt -- not a marker\n");

        Txtar emptyEntry = Txtar::parse(
            "-- empty.txt --\n"
            "-- next.txt --\n"
            "value\n"
        );
        REQUIRE(emptyEntry.entries().size() == 2);
        REQUIRE(emptyEntry.entries()[0].content().empty());
        REQUIRE(emptyEntry.entries()[1].content() == "value\n");

        Txtar noComment = Txtar::parse(
            "-- first.txt --\n"
            "one\n"
        );
        REQUIRE(noComment.comment().empty());
    }

    SECTION("Format"){
        Txtar archive({
            Txtar::Entry("a.txt", "alpha"),
            Txtar::Entry("dir/b.txt", "beta\n")
        }, "comment");

        std::string formatted = archive.format();
        REQUIRE(archive.toString() == formatted);
        REQUIRE(Txtar::parse(formatted).format() == formatted);
    }

    SECTION("Parse File"){
        FileIO fio;
        std::string archivePath = Path::join(workPath(), "lvbase-txtar-parse.txtar");
        if ( Path::exists(archivePath) ){
            REQUIRE(Path::remove(archivePath));
        }

        REQUIRE(fio.writeToFile(archivePath, "-- file.txt --\nvalue\n"));

        Txtar archive = Txtar::parseFile(archivePath);
        REQUIRE(archive.entries().size() == 1);
        REQUIRE(archive.entries()[0].name() == "file.txt");
        REQUIRE(archive.entries()[0].content() == "value\n");

        REQUIRE(Path::remove(archivePath));
    }

    SECTION("From Directory"){
        FileIO fio;
        std::string root = Path::join(workPath(), "lvbase-txtar-from-directory");
        if ( Path::exists(root) ){
            REQUIRE(Path::remove(root));
        }

        REQUIRE(Path::createDirectories(Path::join(root, "b/sub")));
        REQUIRE(Path::createDirectories(Path::join(root, "a")));
        REQUIRE(fio.writeToFile(Path::join(root, "b/sub/file2.txt"), "two"));
        REQUIRE(fio.writeToFile(Path::join(root, "a/file1.txt"), "one"));

        Txtar archive = Txtar::fromDirectory(root);
        REQUIRE(archive.entries().size() == 2);
        REQUIRE(archive.entries()[0].name() == "a/file1.txt");
        REQUIRE(archive.entries()[0].content() == "one");
        REQUIRE(archive.entries()[1].name() == "b/sub/file2.txt");
        REQUIRE(archive.entries()[1].content() == "two");

        bool caughtException = false;
        try{
            Txtar::fromDirectory(Path::join(root, "a/file1.txt"));
        } catch ( const lv::Exception& ){
            caughtException = true;
        }
        REQUIRE(caughtException);

        REQUIRE(Path::remove(root));
    }

    SECTION("Extract"){
        FileIO fio;
        std::string root = Path::join(workPath(), "lvbase-txtar-extract");
        if ( Path::exists(root) ){
            REQUIRE(Path::remove(root));
        }

        Txtar archive({
            Txtar::Entry("file.txt", "alpha"),
            Txtar::Entry("nested/dir/file.txt", "beta")
        });

        archive.extract(root);

        REQUIRE(fio.readFromFile(Path::join(root, "file.txt")) == "alpha");
        REQUIRE(fio.readFromFile(Path::join(root, "nested/dir/file.txt")) == "beta");

        std::string outsidePath = Path::join(workPath(), "escape.txt");
        if ( Path::exists(outsidePath) ){
            REQUIRE(Path::remove(outsidePath));
        }

        Txtar malicious({Txtar::Entry("../escape.txt", "boom")});
        bool caughtException = false;
        try{
            malicious.extract(root);
        } catch ( const lv::Exception& ){
            caughtException = true;
        }
        REQUIRE(caughtException);
        REQUIRE(!Path::exists(outsidePath));

        REQUIRE(Path::remove(root));
    }

    SECTION("Extract Temp"){
        FileIO fio;
        std::string root = Path::join(workPath(), "lvbase-txtar-extract-temp");
        if ( Path::exists(root) ){
            REQUIRE(Path::remove(root));
        }

        REQUIRE(Path::createDirectories(root));
        REQUIRE(fio.writeToFile(Path::join(root, "stale.txt"), "stale"));

        {
            Txtar archive({Txtar::Entry("file.txt", "alpha")});
            Txtar::ExtractionGuard extraction = archive.extractTemp(root);
            REQUIRE(extraction.path() == root);
            REQUIRE(Path::exists(root));
            REQUIRE(!Path::exists(Path::join(root, "stale.txt")));
            REQUIRE(fio.readFromFile(Path::join(root, "file.txt")) == "alpha");
        }

        REQUIRE(!Path::exists(root));
    }
}

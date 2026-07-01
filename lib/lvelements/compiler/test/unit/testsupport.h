#ifndef LVELEMENTSCOMPILER_TESTSUPPORT_H
#define LVELEMENTSCOMPILER_TESTSUPPORT_H

#include "catch_library.h"
#include "live/visuallog.h"

#include "live/elements/compiler/languageparser.h"
#include <string>

inline void requireEquivalentElementsOutput(
    const std::string& expectedContent,
    const std::string& actualContent,
    const std::string& label)
{
    lv::el::LanguageParser::Ptr parser = lv::el::LanguageParser::createForElements();
    lv::el::LanguageParser::AST* actualAST = parser->parse(actualContent);
    lv::el::LanguageParser::AST* expectedAST = parser->parse(expectedContent);

    lv::el::LanguageParser::ComparisonResult compare = parser->compare(
        expectedContent,
        expectedAST,
        actualContent,
        actualAST
    );
    parser->destroy(actualAST);
    parser->destroy(expectedAST);

    if ( !compare.isEqual() ){
        vlog().e() << "File: " << label << ":" << compare.source1Row() << ":" << compare.source1Col();
        vlog().e() << compare.errorString();
        vlog().e() << actualContent;
    }

    REQUIRE(compare.isEqual());
}

#endif // LVELEMENTSCOMPILER_TESTSUPPORT_H

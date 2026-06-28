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

#include "compilerun.h"
#include "live/fileio.h"

namespace lv{ namespace el{

std::unique_ptr<CompileRun> CompileRun::createUnique(TypeResolver* resolver){
    return std::make_unique<CompileRun>(resolver);
}

CompileRun::CompileRun(TypeResolver* resolver){
    m_sink.setResolver(resolver);
}

TypeRequestSink& CompileRun::sink(){
    return m_sink;
}

const TypeRequestSink& CompileRun::sink() const{
    return m_sink;
}

void CompileRun::finalize(FileIOInterface* fileIO, const std::string& typesModulePath){
    m_sink.finalize(fileIO, typesModulePath);
}

}} // namespace lv, el

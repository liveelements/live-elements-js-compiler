/****************************************************************************
**
** Copyright (C) 2022 Dinu SV.
** This file is part of live-elements-js-compiler.
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

#ifndef LVCOMPILATIONWRAP_H
#define LVCOMPILATIONWRAP_H

#include <napi.h>
#include "live/elements/compiler/compilation.h"

namespace lv{

class CompilationWrap : public Napi::ObjectWrap<CompilationWrap>{

public:
    static Napi::Function Init(Napi::Env env);

public:
    CompilationWrap(const Napi::CallbackInfo& info);

private:
    Napi::Value target(const Napi::CallbackInfo& info);
    Napi::Value targetSource(const Napi::CallbackInfo& info);
    Napi::Value files(const Napi::CallbackInfo& info);
    Napi::Value jsPath(const Napi::CallbackInfo& info);
    Napi::Value isDeclarative(const Napi::CallbackInfo& info);
    Napi::Value dependencies(const Napi::CallbackInfo& info);
    Napi::Value packageOf(const Napi::CallbackInfo& info);
    Napi::Value release(const Napi::CallbackInfo& info);

    lv::el::Compilation::Ptr m_compilation;
};

} // namespace

#endif // LVCOMPILATIONWRAP_H

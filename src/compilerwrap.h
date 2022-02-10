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

#include <napi.h>

namespace lv{

    void compileWrap(const Napi::CallbackInfo& info);
    Napi::Object Init(Napi::Env env, Napi::Object exports);

    NODE_API_MODULE(live_elements_js_compiler, Init)
}

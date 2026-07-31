import {Flow} from 'flow/flow.mjs'
import {max as __mod__max} from '__UNRESOLVED__'
let mod = {max:__mod__max}
export function build() {
    return Flow.create('n', undefined, [Flow.call(mod.max, [Flow.ref('n'), 2], 'next'), Flow.ret(Flow.ref('next'))])
}

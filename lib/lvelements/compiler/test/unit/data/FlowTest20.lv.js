import {add, print, send, sum} from './flow-tools.mjs'
import {Flow} from 'flow/flow.mjs'
import {UserContext} from '__UNRESOLVED__'
import {max as __mod__max} from '__UNRESOLVED__'
let mod = {max:__mod__max}
export function build() {
    return Flow.create(['n', 'user'], undefined, [
        Flow.call(add, [Flow.ref('n'), 1], 'a'),
        Flow.call(add, [Flow.ref('a'), 2], 'b'),
        Flow.call(mod.max, [Flow.ref('a'), Flow.ref('b')], 'm'),
        Flow.call(print, [Flow.member(Flow.ref('user'), ['email']), Flow.ref('%')], 'p'),
        Flow.call(send, [{id: Flow.ref('n'), email: Flow.member(Flow.ref('user'), ['email'])}], 'r'),
        Flow.call(sum, [[Flow.ref('a'), Flow.ref('b'), 3]], undefined, ['r']),
        Flow.call(mod.max, [Flow.ref('m'), 1], 'c'),
        Flow.ret(Flow.call(add, [Flow.ref('c'), 1]))
    ])
}

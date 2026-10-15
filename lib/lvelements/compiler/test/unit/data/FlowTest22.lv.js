import {add, map} from './flow-tools.mjs'
import {Flow} from 'flow/flow.mjs'
import {List} from '__UNRESOLVED__'
export function build() {
    return Flow.create('xs', undefined, [
        Flow.call(map, [Flow.ref('xs'), Flow.create('x', undefined, [Flow.call(add, [Flow.ref('x'), 1])])], 'a'),
        Flow.call(map, [Flow.ref('xs'), Flow.create('x', undefined, [Flow.ret(Flow.call(add, [Flow.ref('x'), 1]))])], 'b'),
        Flow.ret(Flow.ref('b'))
    ])
}

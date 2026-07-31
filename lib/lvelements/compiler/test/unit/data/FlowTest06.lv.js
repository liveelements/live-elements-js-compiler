import {Flow} from 'flow/flow.mjs'
import {add} from '__UNRESOLVED__'
export function build() {
            const explicit = Flow.create('n', undefined, [Flow.ret(Flow.ref('n'))])
            const nested =  Flow.create('v', undefined, [Flow.ret(Flow.call(add, [Flow.ref('v'), 2]))])

    return [explicit, nested]
}

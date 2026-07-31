import {Flow} from 'flow/flow.mjs'
import {Math} from '__UNRESOLVED__'
export function build() {
    return Flow.create('n', undefined, [Flow.call(Math.max, [Flow.ref('n'), 2], 'next'), Flow.ret(Flow.ref('next'))])
}

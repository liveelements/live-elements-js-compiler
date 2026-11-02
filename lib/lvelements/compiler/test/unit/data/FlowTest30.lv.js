import {add,mul} from './flow-tools.mjs'
import {Flow} from 'flow/flow.mjs'
export function sum(a, b) { return a + b }
export const importedCallee = Flow.fn('importedCallee', Flow.create('v', undefined, [Flow.call(add, [Flow.ref('v'), 10], 'total'), Flow.ret(Flow.ref('total'))]), {
    get sourceFileName(){ return 'FlowTest30.lv' },
    get parameterNames(){ return ['v'] }
})
export const moduleCallee = Flow.fn('moduleCallee', Flow.create('v', undefined, [Flow.call(sum, [Flow.ref('v'), 10], 'total'), Flow.ret(Flow.ref('total'))]), {
    get sourceFileName(){ return 'FlowTest30.lv' },
    get parameterNames(){ return ['v'] }
})
export const outerNames = Flow.fn('outerNames', Flow.create('v', undefined, [Flow.call(mul, [Flow.ref('v'), 2], 'r'), Flow.call(mul, [Flow.ref('r'), Flow.create('x', undefined, [Flow.call(mul, [Flow.ref('x'), 2], 'v'), Flow.call(mul, [Flow.ref('v'), 2], 'r'), Flow.ret(Flow.ref('r'))])], 's'), Flow.ret(Flow.ref('s'))]), {
    get sourceFileName(){ return 'FlowTest30.lv' },
    get parameterNames(){ return ['v'] }
})
export function build() {
    const scale = 1
    return Flow.create('v', undefined, [Flow.call(mul, [Flow.ref('v'), 10], 'scale'), Flow.ret(Flow.ref('scale'))])
}

import {Flow} from 'flow/flow.mjs'
export function add(a, b) { return a + b }
add.Meta = {
    get sourceFileName(){ return 'FlowTest09.lv' },
    get parameterNames(){ return ['a', 'b'] }
}

export function build() {
    return Flow.create('n', undefined, [Flow.ret(Flow.call(add, [Flow.ref('n'), 2]))])
}

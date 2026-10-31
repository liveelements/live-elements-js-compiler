import {add,apply,sum} from './flow-tools.mjs'
import {Flow} from 'flow/flow.mjs'
export const main = Flow.fn('main', Flow.create('other', undefined, [Flow.call(apply, [20, Flow.create('v', undefined, [Flow.call(add, [Flow.ref('v'), 20], 'added'), Flow.ret(Flow.ref('added'))]), Flow.ref('other')], 'result'), Flow.call(sum, [Flow.ref('result'), Flow.ref('other')], 'total'), Flow.call(sum, [Flow.ref('total'), {a: 1}, Flow.ref('other')], 'mixed'), Flow.ret(Flow.ref('mixed'))]), {
    get sourceFileName(){ return 'FlowTest29.lv' },
    get parameterNames(){ return ['other'] }
})

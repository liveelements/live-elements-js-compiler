import {FunctionType, Type} from 'types.mjs'
import {add, subtract} from './math.mjs'
import {Flow} from 'flow/flow.mjs'

export function build(){
    return Flow.create('x', FunctionType.for(Type.any, Type.number), [
        Flow.call(add, [Flow.ref('x'), 2], 'add'),
        Flow.call(subtract, [Flow.ref('add'), 10], 'subtract'),
        Flow.ret(Flow.ref('add'))
    ])
}

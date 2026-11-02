import {FunctionType, Type} from 'types.mjs'
import {add, subtract} from './math.mjs'
import {Flow} from 'flow/flow.mjs'

export function build(){
    return Flow.create('add', FunctionType.for(Type.any, Type.number), [
        Flow.call(add, [Flow.ref('add'), 2], 'total'),
        Flow.call(subtract, [Flow.ref('total'), 10], 'difference'),
        Flow.ret(Flow.ref('difference'))
    ])
}

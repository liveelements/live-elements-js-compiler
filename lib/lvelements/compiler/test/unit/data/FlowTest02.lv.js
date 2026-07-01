import {FunctionType, Type} from 'types.mjs'
import {add} from './math.mjs'
import {Flow} from 'flow/flow.mjs'

export function build(){
    const first = Flow.create(['a', 'b'], FunctionType.for(Type.any, Type.number, Type.number), [Flow.ret(Flow.call(add, [Flow.ref('a'), Flow.ref('b')]))])
    const second = Flow.create('item', FunctionType.for(Type.any, Type.string), [Flow.ret(Flow.ref('item'))])
    return [first, second]
}

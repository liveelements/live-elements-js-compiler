import {FunctionType, Type} from 'types.mjs'
import {add} from './math.mjs'
import {Flow} from 'flow/flow.mjs'

export function build(){
    return Flow.create(['a', 'b'], FunctionType.for(Type.number, Type.number, Type.number), [Flow.ret(Flow.call(add, [Flow.ref('a'), Flow.ref('b')]))])
}

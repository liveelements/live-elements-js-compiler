import {FunctionType, Type} from 'types.mjs'
import {add} from './math.mjs'
import {Flow} from './custom-flow.mjs'

export function build(){
    const explicit = Flow.create('n', undefined, [Flow.ret(Flow.ref('n'))])
    const nested = Flow.create('v', FunctionType.for(Type.any, Type.number), [
        Flow.call(add, [Flow.ref('v'), 2], 'next'),
        Flow.ret(Flow.ref('next'))
    ])
    return [explicit, nested]
}

import {FunctionType, Type} from 'types.mjs'
import {add, map, print, useFlow, findUser} from './flow-tools.mjs'
import {Flow} from 'flow/flow.mjs'

export function buildFlow(){
    const expressionFlow = Flow.create('n', FunctionType.for(Type.any, Type.number), [Flow.ret(Flow.call(add, [Flow.ref('n'), 3]))])
    const primitiveLiteralFlow = Flow.create('n', FunctionType.for(Type.any, Type.number), [Flow.ret(3)])
    const arrayLiteralFlow = Flow.create('n', FunctionType.for(Type.any, Type.number), [Flow.ret([Flow.ref('n'), 3, "done"])])
    const blockFlow = Flow.create(['a', 'b'], FunctionType.for(Type.any, Type.any, Type.any), [
        Flow.call(findUser, [Flow.member(Flow.ref('a'), ['params', 'userId'])], 'user'),
        Flow.call(map, [Flow.ref('b'), Flow.create('x', FunctionType.for(Type.any, Type.number), [Flow.ret(Flow.call(add, [Flow.ref('x'), 2]))])], 'doubled'),
        Flow.call(print, ["done"], undefined, ['user', 'doubled']),
        Flow.ret({email: Flow.member(Flow.ref('user'), ['email']), items: Flow.ref('doubled'), last: Flow.ref('%'), "display-name": "done"})
    ])
    useFlow(primitiveLiteralFlow, Flow.create('n', FunctionType.for(Type.any, Type.number), [Flow.ret(1)]))
    useFlow(arrayLiteralFlow, Flow.create('n', FunctionType.for(Type.any, Type.number), [Flow.ret(Flow.ref('n'))]))
    useFlow(blockFlow, Flow.create('n', FunctionType.for(Type.any, Type.number), [Flow.ret(Flow.call(add, [Flow.ref('n'), 1]))]))
}

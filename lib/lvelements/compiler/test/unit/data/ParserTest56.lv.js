import {add, map, print, useFlow, findUser} from './flow-tools.mjs'
import {Flow} from 'flow/flow.mjs'

export function buildFlow(){
    const expressionFlow = Flow.create('n', [Flow.ret(Flow.call(add, [Flow.ref('n'), 3]))])
    const primitiveLiteralFlow = Flow.create('n', [Flow.ret(3)])
    const arrayLiteralFlow = Flow.create('n', [Flow.ret([Flow.ref('n'), 3, "done"])])
    const blockFlow = Flow.create(['a', 'b'], [
        Flow.call(findUser, [Flow.member(Flow.ref('a'), ['params', 'userId'])], 'user'),
        Flow.call(map, [Flow.ref('b'), Flow.create('x', [Flow.ret(Flow.call(add, [Flow.ref('x'), 2]))])], 'doubled'),
        Flow.call(print, ["done"], null, ['user', 'doubled']),
        Flow.ret({email: Flow.member(Flow.ref('user'), ['email']), items: Flow.ref('doubled'), last: Flow.ref('%'), "display-name": "done"})
    ])
    useFlow(primitiveLiteralFlow, Flow.create('n', [Flow.ret(1)]))
    useFlow(arrayLiteralFlow, Flow.create('n', [Flow.ret(Flow.ref('n'))]))
    useFlow(blockFlow, Flow.create('n', [Flow.ret(Flow.call(add, [Flow.ref('n'), 1]))]))
}

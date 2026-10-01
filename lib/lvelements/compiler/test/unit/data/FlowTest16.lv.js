import {FunctionType, Type} from 'types.mjs'
import {add,value} from './math.mjs'
import {Flow} from 'flow/flow.mjs'
export const none = Flow.fn('none', Flow.create([], FunctionType.for(Type.any), [Flow.call(value, [], 'v'), Flow.ret(Flow.ref('v'))]), {
    get sourceFileName(){ return 'FlowTest16.lv' },
    get type(){ return FunctionType.for(Type.any) },
    get parameterNames(){ return [] }
})
export const one = Flow.fn('one', Flow.create('n', FunctionType.for(Type.any, Type.number), [Flow.call(add, [Flow.ref('%'), 1], 'next'), Flow.ret(Flow.ref('next'))]), {
    get sourceFileName(){ return 'FlowTest16.lv' },
    get type(){ return FunctionType.for(Type.any, Type.number) },
    get parameterNames(){ return ['n'] }
})
// adds two numbers
export const two = Flow.fn('two', Flow.create(['a', 'b'], FunctionType.for(Type.number, Type.number, Type.number), [Flow.call(add, [Flow.ref('a'), Flow.ref('b')], 'sum'), Flow.ret(Flow.ref('sum'))]), {
    get sourceFileName(){ return 'FlowTest16.lv' },
    get type(){ return FunctionType.for(Type.number, Type.number, Type.number) },
    get parameterNames(){ return ['a', 'b'] }
})
export function plain(a) {
    return a
}

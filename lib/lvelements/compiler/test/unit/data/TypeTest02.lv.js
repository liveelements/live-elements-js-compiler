import {FunctionType, Type} from 'types.mjs'
export function add(a, b) {
    return a + b
}
add.Meta = {
    get sourceFileName(){ return 'TypeTest02.lv' },
    get type(){ return FunctionType.for(Type.number, Type.number, Type.number) },
    get parameterNames(){ return ['a', 'b'] }
}

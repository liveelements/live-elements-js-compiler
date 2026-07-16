import {FunctionType, Type} from 'types.mjs'

export function add(a, b) {
    throw new Error('Function not implemented')
}
add.Meta = {
    get sourceFileName(){ return 'TypeTest08.lv' },
    get declared(){ return true },
    get type(){ return FunctionType.for(Type.number, Type.number, Type.number) },
    get parameterNames(){ return ['a', 'b'] }
}

export function zero() {
    throw new Error('Function not implemented')
}

export async function load(name) {
    throw new Error('Function not implemented')
}
load.Meta = {
    get sourceFileName(){ return 'TypeTest08.lv' },
    get declared(){ return true },
    get type(){ return FunctionType.for(Type.string, Type.string) },
    get parameterNames(){ return ['name'] }
}

import {FunctionType, Type} from 'types.mjs'

export function add(a, b) {
    return a + b
}
add.Meta = {
    get sourceFileName(){ return 'TypeTest07.lv' },
    get type(){ return FunctionType.for(Type.number, Type.number, Type.number) },
    get parameterNames(){ return ['a', 'b'] }
}

export function zero() {
    return 0
}
zero.Meta = {
    get sourceFileName(){ return 'TypeTest07.lv' },
    get type(){ return FunctionType.for(Type.number) },
    get parameterNames(){ return [] }
}

export async function load(name) {
    return name
}
load.Meta = {
    get sourceFileName(){ return 'TypeTest07.lv' },
    get type(){ return FunctionType.for(Type.string, Type.string) },
    get parameterNames(){ return ['name'] }
}

export function plain(value) {
    return value
}

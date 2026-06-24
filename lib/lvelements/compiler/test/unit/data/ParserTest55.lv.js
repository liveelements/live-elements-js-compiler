import {A as __mod__A} from '__UNRESOLVED__'
let mod = {A: __mod__A}

export function add(a, b) {
    return a + b
}
add.Meta = {
    get sourceFileName(){ return 'ParserTest55.lv' },
    get parameterNames(){ return ['a', 'b'] }
}

export function zero() {
    return 0
}
zero.Meta = {
    get sourceFileName(){ return 'ParserTest55.lv' },
    get parameterNames(){ return [] }
}

export async function load(name) {
    return name
}
load.Meta = {
    get sourceFileName(){ return 'ParserTest55.lv' },
    get parameterNames(){ return ['name'] }
}

export function plain(value) {
    return value
}

export class ParserTest55 extends mod.A{

    constructor(){
        super()
        ParserTest55.prototype.__initialize.call(this)
    }
    __initialize(){
    }

    multiply(value){
        return value * 2
    }
}

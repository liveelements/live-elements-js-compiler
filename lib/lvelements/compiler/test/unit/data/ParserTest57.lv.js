import {A as __mod__A} from '__UNRESOLVED__'
let mod = {A: __mod__A}

export function add(a, b) {
    throw new Error('Function not implemented')
}
add.Meta = {
    get sourceFileName(){ return 'ParserTest57.lv' },
    get declared(){ return true },
    get parameterNames(){ return ['a', 'b'] }
}

export function zero() {
    throw new Error('Function not implemented')
}

export async function load(name) {
    throw new Error('Function not implemented')
}
load.Meta = {
    get sourceFileName(){ return 'ParserTest57.lv' },
    get declared(){ return true },
    get parameterNames(){ return ['name'] }
}

export class ParserTest57 extends mod.A{

    constructor(){
        super()
        ParserTest57.prototype.__initialize.call(this)
    }
    __initialize(){
    }

    multiply(value){
        return value * 2
    }
}

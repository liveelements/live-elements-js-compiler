import {A as __mod__A} from '__UNRESOLVED__'
let mod = {A: __mod__A}

/// Adds two numbers.
export function add(a, b) {
    return a + b
}

/** Returns the provided name. */
export async function load(name) {
    return name
}

export let __add_Meta = (function(parent){
    this.setParent(parent)
    this.completed = function(){
        add.Meta = { resolved: true }
    }
    Element.complete(this)
    return this
}.bind(new Element())(null))

export class ParserTest54 extends mod.A{

    constructor(){
        super()
        ParserTest54.prototype.__initialize.call(this)
    }
    __initialize(){
    }

    multiply(value){
        return value * 2
    }
}

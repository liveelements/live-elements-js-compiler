import {X} from '__UNRESOLVED__'

export class ParserTest50 extends Element{

    constructor(){
        super()
        ParserTest50.prototype.__initialize.call(this)
    }
    __initialize(){
    }

    destructureVariables(){
        let {a, b} = {a: 1, b: 2}
        const {c, d, ...rest} = {c: 3, d: 4, e: 5, f: 6}
        var [e, f] = [1, 2]

        let obj;
        let {a: x, b: y} = obj
        let {a = 1, b = 2} = obj
        let {nested: {value}} = obj
        let {items: [first, second]} = obj

        let arr;
        let [a, , b] = arr
        let [head, ...tail] = arr
        let [x = 1, y = 2] = arr
        let [{id}, {name}] = arr
    }

    destructureAssignments() {
        let obj, arr, a, b, x, y;
        ({a, b} = obj);
        [x, y] = arr;
        ({a: x, b: y} = obj);
        ({nested: {value}} = obj)
    }

    trycatch(){
        try {
            this.run()
        } catch (e) {
            console.log(e)
        }

        try {
        } catch {
            let x = (function(parent){
                this.setParent(parent)
                Element.complete(this)
                return this
            }.bind(new X())(null))
        }

        try {
        } catch ({message, code}) {
            console.log(message, code)
        }
    }

    forLoops(items,entries,arr){
        for (const {id, name} of items) {
        }

        for (let [key, value] of entries) {
        }

        for (let [a, b] = arr; a < 10; a++) {
        }
    }
}

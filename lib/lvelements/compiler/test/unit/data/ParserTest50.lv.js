import {X} from '__UNRESOLVED__'

export class ParserTest50 extends Element{

    constructor(){
        super()
        ParserTest50.prototype.__initialize.call(this)
    }
    __initialize(){
        Element.addProperty(this, 'f', { notify: 'fChanged'})
        this.f = ({x1, x2},[a1, a2]) => {
        return x1 + x2 + a1 + a2
    }

    }

    destructureFunctions(){
            function normalFn({x1, x2},[a1, a2]){
            return x1 + x2 + a1 + a2
        }
            const lambdaFn = ({x1, x2},[a1, a2]) => {
            return x1 + x2 + a1 + a2
        }

            const typedFn = ({x1, x2},[a1, a2]) => {
            return x1 + x2 + a1 + a2
        }



        return normalFn({x1: 1, x2: 2}, [3, 4]) + lambdaFn({x1: 1, x2: 2}, [3, 4]) + typedFn({x1: 1, x2: 2}, [3, 4])
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

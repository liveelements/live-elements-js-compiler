export class Foo extends Element {

    constructor() {
        super()
        Foo.prototype.__initialize.call(this)
    }
    __initialize() {
    }

}

export function f(x) {
    return x
}

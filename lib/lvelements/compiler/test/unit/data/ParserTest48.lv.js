export class X extends Element {

    constructor() {
        super()
        X.prototype.__initialize.call(this)
    }
    __initialize() {
        this.ids = {}

        var y = new Element()
        this.ids["y"] = y

        Element.addProperty(y, 'd', { notify: 'dChanged'})
        Element.addProperty(y, 'e', { notify: 'eChanged'})
        Element.addProperty(y, 'f', { notify: 'fChanged'})
        Element.addProperty(y, 'g', { notify: 'gChanged'})

        Element.addProperty(this, 'x', { notify: 'xChanged'})
        Element.addProperty(this, 'y', { notify: 'yChanged'})
        Element.addProperty(this, 'z', { notify: 'zChanged'})
        Element.addProperty(this, 'a', { notify: 'aChanged'})
        Element.addProperty(this, 'b', { notify: 'bChanged'})
        Element.addProperty(this, 'c', { notify: 'cChanged'})
        this.x = 100
        this.y = 150
        Element.assignPropertyExpression(this,
            'z',
            function () { return this.y }.bind(this),
            [[this, 'y']]
        )
        this.a = 100
        this.b = 150
        this.c = this.b

        Element.assignChildren(this, [
            (function (parent) {
                this.setParent(parent)
                Element.assignId(y, "y")
                this.d = 100
                this.e = 100
                this.f = this.e
                this.g = this.e
                Element.complete(this)
                return this
            }.bind(y)(this))
        ])
    }
}

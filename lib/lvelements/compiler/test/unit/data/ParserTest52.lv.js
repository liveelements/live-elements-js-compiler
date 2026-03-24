export class X extends Element{

    constructor(){
        super()
        X.prototype.__initialize.call(this)
    }
    __initialize(){
        Element.addProperty(this, 'a', { notify: 'aChanged'})
        Element.addProperty(this, 'b', { notify: 'bChanged'})
        Element.addEvent(this, 'onAction', [['string','message'],['Event','event']])
        this.a = 20
        this.b = 30
    }

    add(a, b){ return a + b }
}
X.typeName = "XComponent"

export let x = (function(parent){
    this.setParent(parent)
    Element.addProperty(this, 'a', { notify: 'aChanged'})
    this.a = 200
    this.additionalFunction = function(a){ return x + 1 }
    Element.complete(this)
    return this
}.bind(new X())(null))

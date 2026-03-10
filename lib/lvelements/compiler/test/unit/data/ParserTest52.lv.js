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

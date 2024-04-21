import {Child} from '__UNRESOLVED__'


export class Y extends Element{

    constructor(x){
        super()
        Y.prototype.__initialize.call(this,x)

    }
    __initialize(__a__){
        this.ids = {}

        var child = new Child()
        this.ids["child"] = child

        Element.addProperty(child, 'ca', { type: 'any', notify: 'caChanged'})
        Element.addProperty(this, 'a', { type: 'any', notify: 'aChanged'})
        this.a = __a__
        this.b = 1
        Element.assignChildren(this, [
            (function(parent){
                this.setParent(parent)
                Element.assignId(child, "child")
                this.ca = 2
                this.cd = 3
                Element.complete(this)
                return this
            }.bind(child)(this))
        ])
    }
}

export class X extends Y{
    
    constructor(x,y){
        super(x, y)
        X.prototype.__initialize.call(this, (10 + 20 / 3), 20)
    }
    __initialize(__x__, __y__){
        this.x = __x__
        this.y = __y__
    }
}
        

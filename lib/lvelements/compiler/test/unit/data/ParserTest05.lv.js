import {Container} from '__UNRESOLVED__'

export class ParserTest05 extends Container{

    constructor(){
        super()
        ParserTest05.prototype.__initialize.call(this)
    }

    __initialize(){
        Element.addProperty(this, 'elemProp', { notify: 'elemPropChanged'})

        this.elemProp = (function(parent){
            this.setParent(parent)
            Element.addProperty(this, 'x', { notify: 'xChanged'})
            this.x = 20
            Element.complete(this)
            return this
        }.bind(new Element())(this))


        Element.assignChildren(this, [
            (function(parent){
                this.setParent(parent)
                Element.addProperty(this, 'y', { notify: 'yChanged'})

                this.y = 20
                Element.complete(this)
                return this
            }.bind(new Element())(this))
        ])
    }
}

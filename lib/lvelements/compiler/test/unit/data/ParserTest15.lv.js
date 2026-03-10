export class ParserTest15 extends Element{

    constructor(){
        super()
        ParserTest15.prototype.__initialize.call(this)
    }

    __initialize(){
        Element.addProperty(this, 'c', { notify: 'cChanged'})
        this.c = (function(){
            var newElement = (function(parent){
                this.setParent(parent)
                Element.addProperty(this, 'y', { notify: 'yChanged'})
                this.y = 20
                Element.complete(this)
                return this
            }.bind(new Element())(null))

            return newElement.y
        }.bind(this))()

    }
}

import {Container} from '__UNRESOLVED__'

export class ParserTest06 extends Container{

    constructor(){
        super()
        ParserTest06.prototype.__initialize.call(this)
    }

    __initialize(){
        Element.addProperty(this, 'ElemProp', {type: 'component', notify: 'ElemPropChanged'})

        this.ElemProp = class T extends Element{

            constructor(){
                super()
                T.prototype.__initialize.call(this)
            }

            __initialize(){
                this.ids = {}

                var twenty = this
                this.ids['twenty'] = twenty

                Element.addProperty(this, 'x', {type: 'int', notify: 'xChanged'})
                this.x = 20
            }
        }

        Element.assignChildren(this, [
            (function(parent){
                this.setParent(parent)
                Element.addProperty(this, 'y', {type: 'Element', notify: 'yChanged'})

                Element.assignPropertyExpression(
                    this,
                    "y",
                    function(){ return new parent.ElemProp() }.bind(this),
                    [[parent, "ElemProp"]]
                )
                 Element.complete(this)
                return this
            }.bind(new Element())(this))
        ])
    }
}

import {T} from '__UNRESOLVED__'

export class X extends Element{

    constructor(){
        super()
        X.prototype.__initialize.call(this)
    }
    __initialize(){
        Element.addProperty(this, 'x', { notify: 'xChanged'})
        Element.addProperty(this, 'y', { notify: 'yChanged'})
        Element.assignPropertyExpression(this,
            'x',
            (function(){
                return this.y.value + this.z.value
            }.bind(this)),
            [[this,['y','value'],['z','value']]]
        )
        Element.assignPropertyExpression(this,
            'y',
            function(){ return this.z.a + this.z.b}.bind(this),
            [[this,['z','a','b']]]
        )
        Element.assignPropertyExpression(this,
            'z',
            function(){ return this.m.getValue()}.bind(this),
            [[this,'m']]
        )
        this.w = this.getValue()
        Element.assignPropertyExpression(this,
            'singleConcat',
            function(){ return this.m.concat('m')}.bind(this),
            [[this,'m']]
        )
        Element.assignPropertyExpression(this,
            'concatChain',
            function(){ return this.m.concat('mm').concat('xclass')}.bind(this),
            [[this,'m']]
        )
        Element.assignPropertyExpression(this,
            'concatChainKey',
            function(){ return this.m.concat('mm').concat('xclass').key}.bind(this),
            [[this,'m']]
        )
        this.t = (function(parent){
            this.setParent(parent)
            Element.addProperty(this, 'x', { notify: 'xChanged'})
            Element.addProperty(this, 'y', { notify: 'yChanged'})
            Element.assignPropertyExpression(this,
                'x',
                function(){
                    var i = this.y
                    return i.length
                }.bind(this),
                [[this,'y']]
            )
            Element.assignPropertyExpression(this,
                'y',
                function(){ return this.z.a + this.z.b}.bind(this),
                [[this,['z','a','b']]]
            )
            Element.assignPropertyExpression(this,
                'z',
                function(){ return this.m.getValue()}.bind(this),
                [[this,'m']]
            )
            Element.assignPropertyExpression(this,
                'concatChain',
                function(){ return this.m.concat('mm').concat('xclass')}.bind(this),
                [[this,'m']]
            )
            Element.assignPropertyExpression(this,
                'concatChainKey',
                function(){ return this.m.concat('mm').concat('xclass').key}.bind(this),
                [[this,'m']]
            )
            Element.complete(this)
            return this
        }.bind(new T())(null))
    }
}

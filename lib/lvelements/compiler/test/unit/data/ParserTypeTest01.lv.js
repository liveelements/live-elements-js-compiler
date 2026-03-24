import {Array} from '__UNRESOLVED__'
import {Child} from '__UNRESOLVED__'
import {index} from '__UNRESOLVED__'

export class X extends Element{

    constructor(){
        super()
        X.prototype.__initialize.call(this)
    }
    __initialize(){
        Element.addProperty(this, 'children', { type: 'default', notify: 'childrenChanged'})
        Element.assignPropertyExpression(this,
            'children',
            function(){ return this.selectedNodes?.map((n, level) => (function(parent){
                    this.setParent(parent)
                    this.handler = (x) => {
                        const internalHandler = (x) => {
                            return x + 1
                        }
                    }
                    this.onReset = () => index.reset()
                    this.onJumpToLevel = (levelInclusive) => index.jumpToLevel(levelInclusive)
                    this.onSelectAtLevel = (level,nodeId) => index.selectAtLevel(level, nodeId)
                    this.onWrapEl = (el) => index.setWrapEl(el)
                    this.onOptionsRowEl = (el) => index.setOptionsRowEl(el)
                    this.onSelectedEl = (level,el) => index.setSelectedEl(level, el)
                    Element.complete(this)
                    return this
                }.bind(new Child())(null))
            ) }.bind(this),
            [[this, 'selectedNodes']]
        )
    }

    run(){
        var a = 100
        let x = 100
        let y = 20
        let {m, n} = {m: 1, n: 2}
        const z = 100
        function test(c,d){}
        const test2 = (a, b) => {

        }

        const test3 = (s) => new Array(100)


       const simple = function(a){}
       const annotated = async function(a){}
   }
}

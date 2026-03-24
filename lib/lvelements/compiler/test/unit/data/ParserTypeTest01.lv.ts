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
            function(){ return this.selectedNodes?.map((n: any, level: number) => {
                return (function(parent){
                    this.setParent(parent)
                    this.handler = (x: any) => {
                        const internalHandler = (x: any) => {
                            return x + 1
                        }
                    }
                    this.onReset = () => index.reset()
                    this.onJumpToLevel = (levelInclusive: number) => index.jumpToLevel(levelInclusive)
                    this.onSelectAtLevel = (level: number, nodeId: string) => index.selectAtLevel(level, nodeId)
                    this.onWrapEl = (el: any) => index.setWrapEl(el)
                    this.onOptionsRowEl = (el: any) => index.setOptionsRowEl(el)
                    this.onSelectedEl = (level: number, el: any) => index.setSelectedEl(level, el)
                    Element.complete(this)
                    return this
                }.bind(new Child())(null))
            }) }.bind(this),
            [[this, 'selectedNodes']]
        )
    }

    run(){
        var a: number = 100
        let x:number = 100
        let y:string = 20
        let {m, n} = {m: 1, n: 2}
        const z:number = 100
        function test(c:number,d:number): string{}
        const test2 = (a:number, b:number): number => {}

        const test3 = (s) => new Array(100)

       const simple = function(a: number):number{}
       const annotated = async function(a:number):number {}
   }
}

import {Flow} from 'flow/flow.mjs'
import {Function} from '__UNRESOLVED__'
import {console} from '__UNRESOLVED__'
import {fetch} from '__UNRESOLVED__'
import {setTimeout} from '__UNRESOLVED__'

export class Box extends Element{

    constructor(){
        super()
        Box.prototype.__initialize.call(this)
    }
    __initialize(){
    }

}
export function build() {
            const timer =  Flow.create('n', undefined, [Flow.call(setTimeout, ["tick", 0], 't'), Flow.ret(Flow.ref('t'))])
            const constructed =  Flow.create('n', undefined, [Flow.ret(Flow.call(Function, ["return 1"]))])
            const logged =  Flow.create('n', undefined, [Flow.call(console.log, [Flow.ref('n')], 'line'), Flow.ret(Flow.ref('line'))])
            const fetched =  Flow.create('url', undefined, [Flow.ret(Flow.call(fetch, [Flow.ref('url')]))])
            const boxed =  Flow.create(['n', 'at'], undefined, [Flow.ret(Flow.call(Box, [Flow.ref('n')]))])

    return [timer, constructed, logged, fetched, boxed]
}


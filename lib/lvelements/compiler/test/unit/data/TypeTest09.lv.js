import {FunctionType, GenericType, Type, TypeVariable} from 'types.mjs'
import {Flow} from 'flow/flow.mjs'

export function mapVal(val, flow) {
    return flow(val)
}
mapVal.Meta = {
    get sourceFileName(){ return 'TypeTest09.lv' },
    get type(){ return FunctionType.for(TypeVariable.for('Y'), TypeVariable.for('X'), GenericType.for(Flow, FunctionType.for(TypeVariable.for('Y'), TypeVariable.for('X')))).withTypeParameters(['X', 'Y']) },
    get parameterNames(){ return ['val', 'flow'] }
}

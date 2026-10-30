import {AppliedType, FunctionType} from 'types.mjs'
import {Flow} from 'flow/flow.mjs'

export function mapVal(val, flow) {
    return flow(val)
}
mapVal.Meta = {
    get sourceFileName(){ return 'TypeTest09.lv' },
    get type(){ return FunctionType.forGeneric(["X", "Y"], ($X, $Y) => FunctionType.for($Y, $X, AppliedType.for(Flow, FunctionType.for($Y, $X)))) },
    get parameterNames(){ return ['val', 'flow'] }
}

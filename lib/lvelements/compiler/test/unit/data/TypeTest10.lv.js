import {FunctionType, Type, TypeVariable} from 'types.mjs'

export function identity(value) {
    throw new Error('Function not implemented')
}
identity.Meta = {
    get sourceFileName(){ return 'TypeTest10.lv' },
    get declared(){ return true },
    get type(){ return FunctionType.for(TypeVariable.for('X'), TypeVariable.for('X')).withTypeParameters(['X']) },
    get parameterNames(){ return ['value'] }
}

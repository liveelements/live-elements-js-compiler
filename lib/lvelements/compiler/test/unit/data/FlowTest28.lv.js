export function list(...items) {
    return items
}
list.Meta = {
    get sourceFileName(){ return 'FlowTest28.lv' },
    get parameterNames(){ return ['items'] }
}

export function join(sep, ...parts) {
    return parts.join(sep)
}
join.Meta = {
    get sourceFileName(){ return 'FlowTest28.lv' },
    get parameterNames(){ return ['sep', 'parts'] }
}

export function merge(...items) {
    return items
}
merge.Meta = {
    get sourceFileName(){ return 'FlowTest28.lv' },
    get parameterNames(){ return ['items'] }
}

export function decl(...items) {
    throw new Error('Function not implemented')
}
decl.Meta = {
    get sourceFileName(){ return 'FlowTest28.lv' },
    get declared(){ return true },
    get parameterNames(){ return ['items'] }
}

export function notArray(...values) {
    return values
}
notArray.Meta = {
    get sourceFileName(){ return 'FlowTest28.lv' },
    get parameterNames(){ return ['values'] }
}

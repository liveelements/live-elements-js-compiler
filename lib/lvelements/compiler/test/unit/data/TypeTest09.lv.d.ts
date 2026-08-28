import {Flow} from 'flow/flow.mjs'

export declare function mapVal<X, Y>(val: X, flow: Flow<(v:X) => Y>): Y;

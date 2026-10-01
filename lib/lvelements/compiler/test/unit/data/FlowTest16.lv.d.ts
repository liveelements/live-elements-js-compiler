import {add,value} from './math.mjs'
import {Flow} from 'flow/flow.mjs'
export declare function none(): Promise<any>;
export declare namespace none { const flow: Flow; const Meta: any; }
export declare function one(n: number): Promise<any>;
export declare namespace one { const flow: Flow; const Meta: any; }
// adds two numbers
export declare function two(a: number, b: number): Promise<number>;
export declare namespace two { const flow: Flow; const Meta: any; }
export declare function plain(a: number): any;

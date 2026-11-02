import {add,mul} from './flow-tools.mjs'
import {Flow} from 'flow/flow.mjs'
export declare function sum(a: number, b: number): number;
export declare function importedCallee(v: number): Promise<any>;
export declare namespace importedCallee { const flow: Flow; const Meta: any; }
export declare function moduleCallee(v: number): Promise<any>;
export declare namespace moduleCallee { const flow: Flow; const Meta: any; }
export declare function outerNames(v: any): Promise<any>;
export declare namespace outerNames { const flow: Flow; const Meta: any; }
export declare function build(): any;

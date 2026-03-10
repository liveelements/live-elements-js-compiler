
export declare class X extends Element {
    static test1(x: number): any;
    static test2(y: number): any;
    static test3(): any;
    test4(x: number): any;
    test5(): any;
}

export declare const x: X & {
    test2(y: number): any;
    test3(): any;
};


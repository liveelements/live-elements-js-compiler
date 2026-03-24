export declare const elem: Element & {
    insert(item: Element, index: number): any;
    add: { emit(item: Element): void; };
    remove: { emit(item: Element): void; };
};

export declare class X extends Element {
    insert(item: Element, index: number): any;
    add: { emit(item: Element): void; };
    remove: { emit(item: Element): void; };
}

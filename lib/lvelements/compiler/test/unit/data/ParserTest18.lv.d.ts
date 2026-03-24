
export declare class TodoList extends Ul {
    items: list;
    conditionalChildren: Li[];
    remove: { emit(index: number): void; };
    markTodoDone: { emit(index: number): void; };
}


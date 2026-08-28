import {Li} from '__UNRESOLVED__'
import {Ul} from '__UNRESOLVED__'
import {list} from '__UNRESOLVED__'
export declare class TodoList extends Ul {
    items: list;
    conditionalChildren: Li[];
    remove: { emit(index: number): void; };
    markTodoDone: { emit(index: number): void; };
}


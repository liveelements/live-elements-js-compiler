/** Does something */

export declare class X extends Element {
    /* some number */
    a: int;
    /// some other number
    b: int;
    /**
     * @brief Adds two numbers together
     * @param a First number
     * @param b Second number
     * @returns The sum of a and b
     */
    add(a: int, b: int): any;
    /*
      A static property example with multi-line
      block comment formatting.
    */
    static typeName: string;
    // Single line on an event
    onAction: any;
}
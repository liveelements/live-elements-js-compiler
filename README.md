# Live Elements Js Compiler

This compiler translates live elements code into javascript module files.

## Install

Using npm:

```sh
npm i live-elements-js-compiler
```

## Usage

This module exports a single function which does the translation:

```js
const {compile} = require("live-elements-js-compiler");

compile(pathToLvFile, options, callback)
```

 * The `pathToLvFile` is the absolute path to an lv file. All of the file module dependencies will automatically be
 compiled, and all of it's imports paths will be resovled as well.

 * The `options` is an object with the following properties:

    * `baseComponent` is the base component name. By default, this should be `BaseElement`.
    * `baseComponentPath` is the import path to the base component. By default, this should be `live-elements-core/baseelement.js`
    * `implicitTypes` are the implicit types that don't need importing inside the lv files.
    * `importLocalPath` is the default import path for other lv packages.
    * `packageBuildPath` is the build path for each package, where all the `js` modules are build.
    * `outputExtension` is the output extension for each of the `js` module files. By default, this is `mjs`
    * `log` is an object defining log options. (i.e. `log: { level: "verbose" })`)

* The `callback` is a function with 2 arguments, a `result`, which is the path to the compiled *js* file, and an
`error` object, which is a data object containing different parameters depending on the error that was generated.

This is an example on how to use the compiler:

```js
const {compile} = require("live-elements-js-compiler");

const options = {
    baseComponent : "BaseElement",
    baseComponentPath : "live-elements-core/baseelement.js",
    implicitTypes : ["document", "Error", "Object", "Math"],
    importLocalPath : "node_modules",
    packageBuildPath : "build",
    outputExtension : "mjs"
}

compile('main.lv', options, (result, err) => {
    if ( err ){
        console.log("Error:" + err.message)
        return
    }
    console.log("Compiled file at: " + result)
})
```


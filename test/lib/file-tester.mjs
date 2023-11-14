import fs from 'fs'
import * as compiler from '../../index.js'

export default class FileTester{

    constructor(filePath, fileContent, config){
        this._filePath = filePath
        this._fileContent = fileContent
        this._compileConfig = config ? config : FileTester.defaultCompileConfig
    }

    run(){
        try{
            fs.writeFileSync(this._filePath, this._fileContent)
            return new Promise((resolve, reject) => {
                compiler.default.compile(this._filePath, this._compileConfig, (result, err) => {
                    fs.unlinkSync(this._filePath)
                    if ( err ){
                        reject(err)
                        return
                    }
                    if ( !fs.existsSync(result) ){
                        reject(new Error(`Compiled file does not exist: ${result}`))
                        return
                    }
                    resolve(result)
                })
            })
        } catch ( e ){
            return Promise.reject(e)
        }
    }

    static create(filePath, fileContent, config){
        return new FileTester(filePath, fileContent, config)
    }

    static clean(compiledPath){
        return fs.promises.rm(path.parent(compiledPath), { recursive: true })
    }
}

FileTester.defaultCompileConfig = {
    baseComponent : "BaseElement",
    baseComponentPath : "live-elements-core/baseelement.js",
    implicitTypes : ["document", "Error", "Object", "Math"],
    importLocalPath : "node_modules",
    packageBuildPath : "build",
    outputExtension : "mjs"
}
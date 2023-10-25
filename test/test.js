const {compile} = require("..")
const fs = require('fs')
const { exit } = require("process")

const path = __dirname + '/test_file.lv'
const content = 'component Test{}'
const options = {
    baseComponent : "BaseElement",
    baseComponentPath : "live-elements-core/baseelement.js",
    implicitTypes : ["document", "Error", "Object", "Math"],
    importLocalPath : "node_modules",
    packageBuildPath : "build",
    outputExtension : "mjs"
}

try{
    fs.writeFileSync(path, content)
    compile(path, options, (result, err) => {
        if ( err ){
            fs.unlinkSync(path)
            exit(1)
        }
        if (!fs.existsSync(result)) {
            fs.unlinkSync(path)
            exit(1)
        }
        fs.rmSync(__dirname + '/build', { recursive: true, force: true });
        fs.unlinkSync(path)
        exit(0)
    })
} catch (err) {
    console.error(err)
    exit(1)
}


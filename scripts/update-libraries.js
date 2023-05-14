const fse = require('fs-extra');
const fs = require('fs');
const path = require('path')

if ( process.argv.length !== 3 ){
    throw new Error("Usage: update-libraries.js <path to lk source>")
}

const lkPath = process.argv[2];

async function copyPathStructure(paths){
    try {
        await fs.promises.rm(__dirname + "/../lib/lvbase", { recursive: true })
        await fs.promises.rm(__dirname + "/../lib/lvelements", { recursive: true })

        console.log("Lib folder has been cleared.")

        for (const [key, value] of Object.entries(paths)) {
            var from = path.resolve(lkPath + '/' + key)
            var to   = path.resolve(__dirname + '/' + value)
            await fse.copy(from, to)
        }

        console.log('Ready')
    } catch (err) {
        console.error(err)
    }
}

var pathStructure = {
    "lib/lvbase" : "../lib/lvbase",
    "lib/lvelements/compiler": "../lib/lvelements/compiler",
    "project/functions.cmake": "../project/functions.cmake"
}

copyPathStructure(pathStructure)
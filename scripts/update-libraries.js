const fse = require('fs-extra');
const fs = require('fs');
const path = require('path')

if ( process.argv.length !== 3 ){
    throw new Error("Usage: update-libraries.js <path to lk source>")
}

const lkPath = process.argv[2];

async function copyPathStructure(paths){
    try {
        await fs.promises.rmdir(__dirname + "/../lib/lvbase", { recursive: true })
        await fs.promises.rmdir(__dirname + "/../lib/lvelements", { recursive: true })

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
    "lib/lvbase/3rdparty": "../lib/lvbase/3rdparty",
    "lib/lvbase/include": "../lib/lvbase/include",
    "lib/lvbase/src": "../lib/lvbase/src",
    "lib/lvelements/3rdparty": "../lib/lvelements/3rdparty",
    "lib/lvelements/include/live/elements/compiler": "../lib/lvelements/include/live/elements/compiler",
    "lib/lvelements/include/live/elements/lvelementsglobal.h": "../lib/lvelements/include/live/elements/lvelementsglobal.h",
    "lib/lvelements/src/compiler": "../lib/lvelements/src/compiler",
    "lib/lvelements/src/lvelementsglobal.h": "../lib/lvelements/src/lvelementsglobal.h"
}

copyPathStructure(pathStructure)
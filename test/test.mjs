import fs from 'fs'
import path from 'path'
import url from 'url'
import FileTester from './lib/file-tester.mjs'

const currentDir = path.dirname(url.fileURLToPath(import.meta.url))
const currentFile = path.join(currentDir, 'testfile.lv')
const content = 'component Test{}'

FileTester.create(currentFile, content).run()
    .then(result => { console.log("Compiled: ", result); process.exit(0) })
    .catch(err => { console.error("Error: ", err); process.exit(1) })



#!/usr/bin/env node

import os from 'os'
import fs from 'fs'
import path from 'path'
import url from 'url'
import tar from 'tar'
import { spawn } from 'child_process'
import { setupArtifact } from '@nodegui/artifact-installer'

async function setupScriptData(){
    const currentDir = path.dirname(url.fileURLToPath(import.meta.url))
    const packageJson = JSON.parse(fs.readFileSync(path.join(currentDir, '..', 'package.json')))
    const packageVersion = packageJson.version
    const [nodeVersionMajor, nodeVersionMinor] = process.versions.node.split('.').map(Number)
    const baseUrl = 'https://github.com/live-keys/live-elements-js-compiler/releases/download'
    const tarballs = [
        `live-elements-js-compiler-v${packageVersion}-${os.platform()}-${os.arch()}-nv${nodeVersionMajor}.tar.gz`,
        `live-elements-js-compiler-v${packageVersion}-${os.platform()}-${os.arch()}.tar.gz`
    ]
    const urls = tarballs.map(t => `${baseUrl}/v${packageVersion}/${t}`)
    const downloadDestination = path.resolve(path.join(currentDir, '..', 'build', 'Release'))
    const testScript = path.resolve(path.join(currentDir, '..', 'test', 'test.mjs'))

    return {
        package: { version: packageVersion },
        node: { version: { major: nodeVersionMajor, minor: nodeVersionMinor }},
        download: { urls: urls, destination: downloadDestination },
        test: { script: testScript }
    }
}

async function downloadAndExtract(downloadUrl, destination){
    await setupArtifact({
        outDir: destination,
        id: 'live-elements-js-compiler',
        displayName: `Live Elements Js Compiler`,
        downloadLink: downloadUrl,
        skipSetup: () => false
    })
    const tarballName = (new url.URL(downloadUrl)).pathname.split('/').pop();
    const tarPath = path.join(destination, tarballName.slice(0, -3))
    tar.extract({
        cwd: destination,
        file: tarPath,
        sync: true,
    })
    fs.unlinkSync(tarPath)
    return destination
}

async function downloadAndExtractFirst(downloadUrls, destination){
    let lastError = null
    for ( let i = 0; i < downloadUrls.length; ++i ){
        try{
            const result = await downloadAndExtract(downloadUrls[i], destination)
            return result
        } catch (e){
            console.log(`Cannot download ${downloadUrls[i]}. Checking next option ...`)
            lastError = e
        }
    }
    throw lastError
}

function runNodeProcess(script) {
    return new Promise((resolve, reject) => {
        const childProcess = spawn('node', [script])
        childProcess.stdout.on('data', (data) => {
            console.log(data.toString())
        })
        childProcess.stderr.on('data', (data) => {
            console.error(data.toString())
        })
        childProcess.on('close', (code) => {
            if (code === 0) {
                resolve(code)
            } else {
                reject(new Error(`Script exited with code ${code}`))
            }
        })
        childProcess.on('error', (err) => {
            reject(err)
        })
    })
}

function logAndExit(promise){
    promise.then( r => { console.log(r); process.exit(0) } )
    promise.catch(e => { console.error(e); process.exit(1) } )
}


logAndExit(
    setupScriptData().then(data => {
        return downloadAndExtractFirst(data.download.urls, data.download.destination)
            .then(d => { console.log("Testing download:"); return d })
            .then(_ => runNodeProcess(data.test.script))
            .then(status => console.log(`Test existed with status: ${status}`))
    })
)
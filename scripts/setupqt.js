#!/usr/bin/env node

const { setupArtifact } = require('@nodegui/artifact-installer');
const { miniQt, useCustomQt, qtHome } = require('../config/qtConfig');
const fs = require('fs')
const path = require('path')

const getAllFiles = function(dirPath) {
    files = fs.readdirSync(dirPath)
  
    arrayOfFiles = []
  
    files.forEach(function(file) {
      if (fs.statSync(dirPath + "/" + file).isDirectory()) {
        arrayOfFiles = arrayOfFiles.concat(getAllFiles(dirPath + "/" + file))
      } else {
        arrayOfFiles.push(path.join(dirPath, "/", file))
      }
    })
  
    return arrayOfFiles
  }

if ( process.platform === 'darwin' ){
    var zipPath = path.dirname(require.resolve("7zip-bin/package.json"));
    var files = getAllFiles(zipPath + '/mac')
    for ( var i = 0; i < files.length; ++i ){
        fs.chmodSync(files[i], '755');
    }
} else if ( process.platform === 'linux' ){
    var zipPath = path.dirname(require.resolve("7zip-bin/package.json"));
    var files = getAllFiles(zipPath + '/linux')
    for ( var i = 0; i < files.length; ++i ){
        fs.chmodSync(files[i], '755');
    }
}


async function setupQt() {
    return Promise.all(
        miniQt.artifacts.map(async (artifact) =>
            setupArtifact({
                outDir: miniQt.setupDir,
                id: 'nodegui-mini-qt',
                displayName: `${artifact.name} for Minimal Qt: ${miniQt.version} installation`,
                downloadLink: artifact.link,
                skipSetup: artifact.skipSetup,
            }),
        ),
    );
}

if (!useCustomQt) {
    console.log(`Minimal Qt ${miniQt.version} setup:`);

    setupQt().catch((err) => {
        console.error(err);
        process.exit(1);
    });
} else {
    console.log(`CustomQt detected at ${qtHome} . Hence, skipping Mini Qt installation...`);
}

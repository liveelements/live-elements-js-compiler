if ( process.platform === 'win32' ){
    const { qtHome } = require('./config/qtConfig');
    process.env.PATH += ';' + qtHome + '/bin'
}
const api = require("./build/Release/live_elements_js_compiler.node");
module.exports = api
/*
You can here configure your tasks of the client build process:
erizofc: compile erizofc.js in UMD mode
erizo: compile erizo.js client lib in VAR mode

EXPERIMENTAL FEATURES:
erizoUmd: compile erizo.js in UMD mode (to use modules inclusion in ES6 projects)
note:
erizoUmd will override erizo.js compiled in var mode so no need to specify both in the array
*/
module.exports = ['erizo', 'erizofc'];

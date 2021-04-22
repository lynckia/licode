const config = require('../../../../licode_config');

const fs = require('fs');

const handlers = config.erizo.handlers;

let map = '{';
let handlerEnum = '{';

if (handlers.length > 0) {
  handlers.forEach((handler) => {
    const handlerName = handler.handlerName;
    map += `{"${handlerName}", ${handlerName}Enum}, `;
    handlerEnum = `${handlerEnum}${handlerName}Enum, `;
  });
  map = `${map.slice(0, -2)}}`;
  handlerEnum = `${handlerEnum.slice(0, -2)}}`;
} else {
  map = {};
  handlerEnum = {};
}

const headers = fs.readFileSync('HandlerImporter.h', 'utf-8');
let modifiedData = headers.replace('*handlerEnum*', `enum HandlersEnum ${handlerEnum};\n`);
modifiedData = modifiedData.replace('*handlerMap*', `std::map<std::string, HandlersEnum> handlersDic =${map};\n`);

let imports = '';

handlers.forEach((handler) => {
  const handlerClassName = handler.className;
  imports = `${imports}#include "${handlerClassName}.h"\n`;
});
modifiedData = modifiedData.replace('*imports*', imports);

fs.writeFileSync('HandlerImporter.h', modifiedData, 'utf-8');

const cpp = fs.readFileSync('HandlerImporter.cpp', 'utf-8');
let cases = '';

handlers.forEach((handler,index) => {
  const handlerClassName = handler.className;
  const handlerName = handler.handlerName;
  const handlerEnumer = `${handlerName}Enum`;
  if (index === 0) {
    cases = `${cases}case ${handlerEnumer} :\n \t\t\tptr = std::make_shared<${handlerClassName}>(parameters);\n\t\t\tbreak;\n`;
  } else {
    cases = `${cases}\t\tcase ${handlerEnumer} :\n \t\t\tptr = std::make_shared<${handlerClassName}>(parameters);\n\t\t\tbreak;\n`;
  }
});

const modifiedHeaders = cpp.replace('*case*', cases);

fs.writeFileSync('HandlerImporter.cpp', modifiedHeaders, 'utf-8');

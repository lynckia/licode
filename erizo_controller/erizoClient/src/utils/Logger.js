/* global console */

/*
 * API to write logs based on traditional logging mechanisms: debug, trace, info, warning, error
 */
const Logger = (() => {
  const DEBUG = 0;
  const TRACE = 1;
  const INFO = 2;
  const WARNING = 3;
  const ERROR = 4;
  const NONE = 5;
  let logPrefix = '';
  let outputFunction;

  // It sets the new log level. We can set it to NONE if we do not want to print logs
  const setLogLevel = (level) => {
    let targetLevel = level;
    if (level > Logger.NONE) {
      targetLevel = Logger.NONE;
    } else if (level < Logger.DEBUG) {
      targetLevel = Logger.DEBUG;
    }
    Logger.logLevel = targetLevel;
  };

  outputFunction = (args) => {
    // eslint-disable-next-line no-console
    console.log(...args);
  };

  const setOutputFunction = (newOutputFunction) => {
    outputFunction = newOutputFunction;
  };

  const setLogPrefix = (newLogPrefix) => {
    logPrefix = newLogPrefix;
  };

    // Generic function to print logs for a given level:
    //  Logger.[DEBUG, TRACE, INFO, WARNING, ERROR]
  const log = (level, ...args) => {
    let out = logPrefix;
    if (level === Logger.DEBUG) {
      out = `${out}DEBUG`;
    } else if (level === Logger.TRACE) {
      out = `${out}TRACE`;
    } else if (level === Logger.INFO) {
      out = `${out}INFO`;
    } else if (level === Logger.WARNING) {
      out = `${out}WARNING`;
    } else if (level === Logger.ERROR) {
      out = `${out}ERROR`;
    }
    out = `${out}: `;
    const tempArgs = [out].concat(args);
    if (Logger.panel !== undefined) {
      let tmp = '';
      for (let idx = 0; idx < tempArgs.length; idx += 1) {
        tmp += tempArgs[idx];
      }
      Logger.panel.value = `${Logger.panel.value}\n${tmp}`;
    } else {
      outputFunction.apply(Logger, [tempArgs]);
    }
  };

  const logFromModule = (moduleName, moduleMinLevel, logLevel, ...args) => {
    if (moduleMinLevel === undefined && logLevel >= Logger.logLevel) {
      log(logLevel, `(${moduleName})`, ...args);
    } else if (logLevel >= moduleMinLevel) {
      log(logLevel, `(${moduleName})`, ...args);
    }
  };

  class ModuleLogger {
    constructor(name) {
      this.name = name;
    }

    setLogLevel(level) { this.level = level; }

    debug(...args) { logFromModule(this.name, this.level, Logger.DEBUG, ...args); }
    trace(...args) { logFromModule(this.name, this.level, Logger.TRACE, ...args); }
    info(...args) { logFromModule(this.name, this.level, Logger.INFO, ...args); }
    warning(...args) { logFromModule(this.name, this.level, Logger.WARNING, ...args); }
    error(...args) { logFromModule(this.name, this.level, Logger.ERROR, ...args); }
  }

  const modules = new Map();

  const module = (moduleName) => {
    if (modules.has(moduleName)) {
      return modules.get(moduleName);
    }
    const newModule = new ModuleLogger(moduleName);
    modules.set(moduleName, newModule);
    return newModule;
  };

  return {
    logLevel: DEBUG,
    DEBUG,
    TRACE,
    INFO,
    WARNING,
    ERROR,
    NONE,
    setLogLevel,
    setOutputFunction,
    setLogPrefix,
    module,
  };
})();

export default Logger;

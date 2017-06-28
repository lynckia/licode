/* global document, console*/

(function setup() {
  this.L = this.L || {};
  const L = this.L;
  /*
   * API to write logs based on traditional logging mechanisms: debug, trace, info, warning, error
   */
  L.Logger = (() => {
    const DEBUG = 0;
    const TRACE = 1;
    const INFO = 2;
    const WARNING = 3;
    const ERROR = 4;
    const NONE = 5;
    let logPrefix = '';
    let outputFunction;

      // By calling this method we will not use console.log to print the logs anymore.
      // Instead we will use a <textarea/> element to write down future logs
    const enableLogPanel = () => {
      L.Logger.panel = document.createElement('textarea');
      L.Logger.panel.setAttribute('id', 'licode-logs');
      L.Logger.panel.setAttribute('style', 'width: 100%; height: 100%; display: none');
      L.Logger.panel.setAttribute('rows', 20);
      L.Logger.panel.setAttribute('cols', 20);
      L.Logger.panel.setAttribute('readOnly', true);
      document.body.appendChild(L.Logger.panel);
    };

      // It sets the new log level. We can set it to NONE if we do not want to print logs
    const setLogLevel = (level) => {
      let targetLevel = level;
      if (level > L.Logger.NONE) {
        targetLevel = L.Logger.NONE;
      } else if (level < L.Logger.DEBUG) {
        targetLevel = L.Logger.DEBUG;
      }
      L.Logger.logLevel = targetLevel;
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
      //  L.Logger.[DEBUG, TRACE, INFO, WARNING, ERROR]
    const log = (level, ...args) => {
      let out = logPrefix;
      if (level < L.Logger.logLevel) {
        return;
      }
      if (level === L.Logger.DEBUG) {
        out = `${out}DEBUG`;
      } else if (level === L.Logger.TRACE) {
        out = `${out}TRACE`;
      } else if (level === L.Logger.INFO) {
        out = `${out}INFO`;
      } else if (level === L.Logger.WARNING) {
        out = `${out}WARNING`;
      } else if (level === L.Logger.ERROR) {
        out = `${out}ERROR`;
      }
      out = `${out}: `;
      const tempArgs = [out].concat(args);
      if (L.Logger.panel !== undefined) {
        let tmp = '';
        for (let idx = 0; idx < tempArgs.length; idx += 1) {
          tmp += tempArgs[idx];
        }
        L.Logger.panel.value = `${L.Logger.panel.value}\n${tmp}`;
      } else {
        outputFunction.apply(L.Logger, [tempArgs]);
      }
    };

    const debug = (...args) => {
      L.Logger.log(L.Logger.DEBUG, ...args);
    };

    const trace = (...args) => {
      L.Logger.log(L.Logger.TRACE, ...args);
    };

    const info = (...args) => {
      L.Logger.log(L.Logger.INFO, ...args);
    };

    const warning = (...args) => {
      L.Logger.log(L.Logger.WARNING, ...args);
    };

    const error = (...args) => {
      L.Logger.log(L.Logger.ERROR, ...args);
    };

    return {
      DEBUG,
      TRACE,
      INFO,
      WARNING,
      ERROR,
      NONE,
      enableLogPanel,
      setLogLevel,
      setOutputFunction,
      setLogPrefix,
      log,
      debug,
      trace,
      info,
      warning,
      error,
    };
  })();
}());

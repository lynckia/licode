var L = L || {};

/*
 * API to write logs based on traditional logging mechanisms: debug, trace, info, warning, error
 */
L.Logger = (function (L) {
    "use strict";
    var DEBUG = 0, TRACE = 1, INFO = 2, WARNING = 3, ERROR = 4, NONE = 5, logLevel = DEBUG, enableLogPanel, setLogLevel, log, debug, trace, info, warning, error;

    // By calling this method we will not use console.log to print the logs anymore. Instead we will use a <textarea/> element to write down future logs
    enableLogPanel = function () {
        L.Logger.panel = document.createElement('textarea');
        L.Logger.panel.setAttribute("id", "lynckia-logs");
        L.Logger.panel.setAttribute("style", "width: 100%; height: 100%; display: none");
        L.Logger.panel.setAttribute("rows", 20);
        L.Logger.panel.setAttribute("cols", 20);
        L.Logger.panel.setAttribute("readOnly", true);
        document.body.appendChild(L.Logger.panel);
    };

    // It sets the new log level. We can set it to NONE if we do not want to print logs
    setLogLevel = function (level) {
        if (level > L.Logger.NONE) {
            level = L.Logger.NONE;
        } else if (level < L.Logger.DEBUG) {
            level = L.Logger.DEBUG;
        }
        L.Logger.logLevel = level;
    };

    // Generic function to print logs for a given level: L.Logger.[DEBUG, TRACE, INFO, WARNING, ERROR]
    log = function (level, msg) {
        var out = '';
        if (level < L.Logger.logLevel) {
            return;
        }
        if (level === L.Logger.DEBUG) {
            out = out + "DEBUG";
        } else if (level === L.Logger.TRACE) {
            out = out + "TRACE";
        } else if (level === L.Logger.INFO) {
            out = out + "INFO";
        } else if (level === L.Logger.WARNING) {
            out = out + "WARNING";
        } else if (level === L.Logger.ERROR) {
            out = out + "ERROR";
        }

        out = out + ": " + msg;
        if (L.Logger.panel !== undefined) {
            L.Logger.panel.value = L.Logger.panel.value + "\n" + out;
        } else {
            console.log(out);
        }
    };

    // It prints debug logs
    debug = function (msg) {
        L.Logger.log(L.Logger.DEBUG, msg);
    };

    // It prints trace logs
    trace = function (msg) {
        L.Logger.log(L.Logger.TRACE, msg);
    };

    // It prints info logs
    info = function (msg) {
        L.Logger.log(L.Logger.INFO, msg);
    };

    // It prints warning logs
    warning = function (msg) {
        L.Logger.log(L.Logger.WARNING, msg);
    };

    // It prints error logs
    error = function (msg) {
        L.Logger.log(L.Logger.ERROR, msg);
    };

    return {
        DEBUG: DEBUG,
        TRACE: TRACE,
        INFO: INFO,
        WARNING: WARNING,
        ERROR: ERROR,
        NONE: NONE,
        enableLogPanel: enableLogPanel,
        setLogLevel: setLogLevel,
        log: log,
        debug: debug,
        trace: trace,
        info: info,
        warning: warning,
        error: error
    };
}(L));
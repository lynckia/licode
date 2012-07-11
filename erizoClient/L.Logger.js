var L = L || {};

L.Logger = function(L) {

    var DEBUG = 0, TRACE = 1, INFO = 2, WARNING = 3, ERROR = 4, NONE = 5;
    var logLevel = DEBUG;

    var enableLogPanel = function() {
        L.Logger.panel = document.createElement('textarea');
        L.Logger.panel.setAttribute("id", "lynckia-logs");
        L.Logger.panel.setAttribute("style", "width: 100%; height: 100%; display: none");
        L.Logger.panel.setAttribute("rows", 20);
        L.Logger.panel.setAttribute("cols", 20);
        L.Logger.panel.setAttribute("readOnly", true);
        document.body.appendChild(L.Logger.panel);
    };

    var setLogLevel = function(level) {
        if (level > L.Logger.NONE) {
            level = L.Logger.NONE;
        } else if (level < L.Logger.DEBUG) {
            level = L.Logger.DEBUG;
        }
        L.Logger.logLevel = level;
    };

    var log = function(level, msg) {
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

    var debug = function(msg) {
        L.Logger.log(L.Logger.DEBUG, msg);
    };

    var trace = function(msg) {
        L.Logger.log(L.Logger.TRACE, msg);
    };

    var info = function(msg) {
        L.Logger.log(L.Logger.INFO, msg);
    };

    var warning = function(msg) {
        L.Logger.log(L.Logger.WARNING, msg);
    };

    var error = function(msg) {
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
}(L);
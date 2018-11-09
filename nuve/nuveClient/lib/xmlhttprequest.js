/**
 * Wrapper for built-in http.js to emulate the browser XMLHttpRequest object.
 *
 * This can be used with JS designed for browsers to improve reuse of code and
 * allow the use of existing libraries.
 *
 * Usage: include("XMLHttpRequest.js") and use XMLHttpRequest per W3C specs.
 *
 * @todo SSL Support
 * @author Dan DeFelippi <dan@driverdan.com>
 * @contributor David Ellis <d.f.ellis@ieee.org>
 * @license MIT
 */

/* eslint-disable */

'use strict';

var Url = require('url'),
  spawn = require('child_process').spawn,
  fs = require('fs');

var XMLHttpRequest = function() {
  /**
   * Private variables
   */
  var self = this;
  var http = require('http');
  var https = require('https');

  var listeners = {};

  // Holds http.js objects
  var request;
  var response;

  // Request settings
  var settings = {};

  // Set some default headers
  var defaultHeaders = {
    'User-Agent': 'node.js',
    'Accept': '*/*'
  };

  // Send flag
  var sendFlag = false;
  // Error flag, used when errors occur or abort is called
  var errorFlag = false;

  var headers = defaultHeaders;

  /**
   * Constants
   */
  this.UNSENT = 0;
  this.OPENED = 1;
  this.HEADERS_RECEIVED = 2;
  this.LOADING = 3;
  this.DONE = 4;

  /**
   * Public vars
   */
  // Current state
  this.readyState = this.UNSENT;

  // default ready state change handler in case one is not set or is set late
  this.onreadystatechange = null;

  // Result & response
  this.responseText = '';
  this.responseXML = '';
  this.status = null;
  this.statusText = null;

  /**
   * Changes readyState and calls onreadystatechange.
   *
   * @param int state New state
   */
  var setState = function(state) {
    self.readyState = state;
    if (typeof self.onreadystatechange === 'function') {
      self.onreadystatechange();
    }

    if ('readystatechange' in listeners) {
      var count = listeners.readystatechange.length, i = 0;
      for(; i < count; i++) {
        listeners.readystatechange[i].call(self);
      }
    }
  };

  /**
   * Open the connection. Currently supports local server requests.
   *
   * @param string method Connection method (eg GET, POST)
   * @param string url URL for the connection.
   * @param boolean async Asynchronous connection. Default is true.
   * @param string user Username for basic authentication (optional)
   * @param string password Password for basic authentication (optional)
   */
  this.open = function(method, url, async, user, password) {
    settings = {
      'method': method,
      'url': url.toString(),
      'async': (typeof async !== 'boolean' ? true : async),
      'user': user || null,
      'password': password || null
    };

    this.abort();

    setState(this.OPENED);
  };

  /**
   * Sets a header for the request.
   *
   * @param string header Header name
   * @param string value Header value
   */
  this.setRequestHeader = function(header, value) {
    if (this.readyState !== this.OPENED) {
      throw 'NVALID_STATE_ERR: setRequestHeader can only be called when state is OPEN';
    }
    if (sendFlag) {
      throw 'INVALID_STATE_ERR: send flag is true';
    }
    headers[header] = value;
  };

  /**
   * Gets a header from the server response.
   *
   * @param string header Name of header to get.
   * @return string Text of the header or null if it doesn't exist.
   */
  this.getResponseHeader = function(header) {
    if (this.readyState > this.OPENED && response.headers[header] && !errorFlag) {
      return response.headers[header];
    }

    return null;
  };

  /**
   * Gets all the response headers.
   *
   * @return string
   */
  this.getAllResponseHeaders = function() {
    if (this.readyState < this.HEADERS_RECEIVED || errorFlag) {
      return '';
    }
    var result = '';

    for (var i in response.headers) {
      result += i + ': ' + response.headers[i] + '\r\n';
    }
    return result.substr(0, result.length - 2);
  };

  /**
   * Sends the request to the server.
   *
   * @param string data Optional data to send as request body.
   */
  this.send = function(data) {
    if (this.readyState !== this.OPENED) {
      throw 'INVALID_STATE_ERR: connection must be opened before send() is called';
    }

    if (sendFlag) {
      throw 'INVALID_STATE_ERR: send has already been called';
    }

    var ssl = false;
    var url = Url.parse(settings.url);
    var host;
    // Determine the server
    switch (url.protocol) {
      case 'https:':
        ssl = true;
        // SSL & non-SSL both need host, no break here.
      case 'http:':
        host = url.hostname;
        break;

      case undefined:
      case '':
        host = 'localhost';
        break;

      default:
        throw 'Protocol not supported.';
    }

    // Default to port 80. If accessing localhost on another port be sure
    // to use http://localhost:port/path
    var port = url.port || (ssl ? 443 : 80);
    // Add query string if one is used
    var uri = url.pathname + (url.search ? url.search : '');

    // Set the Host header or the server may reject the request
    this.setRequestHeader('Host', host);

    // Set Basic Auth if necessary
    if (settings.user) {
      if (typeof settings.password === 'undefined') {
        settings.password = '';
      }
      var authBuf = new Buffer(settings.user + ':' + settings.password);
      headers.Authorization = 'Basic ' + authBuf.toString('base64');
    }

    // Set content length header
    if (settings.method === 'GET' || settings.method === 'HEAD') {
      data = null;
    } else if (data) {
      this.setRequestHeader('Content-Length', Buffer.byteLength(data));

      if (!headers['Content-Type']) {
        this.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
      }
    }

    var options = {
      host: host,
      port: port,
      path: uri,
      method: settings.method,
      headers: headers
    };

    // Reset error flag
    errorFlag = false;

    // Handle async requests
    if(!settings.hasOwnProperty('async') || settings.async) {
      // Use the proper protocol
      var doRequest = ssl ? https.request : http.request;

      // Request is being sent, set send flag
      sendFlag = true;

      // As per spec, this is called here for historical reasons.
      if (typeof self.onreadystatechange === 'function') {
        self.onreadystatechange();
      }

      // Create the request
      request = doRequest(options, function(resp) {
        response = resp;
        response.setEncoding('utf8');

        setState(self.HEADERS_RECEIVED);
        self.status = response.statusCode;

        response.on('data', function(chunk) {
          // Make sure there's some data
          if (chunk) {
            self.responseText += chunk;
          }
          // Don't emit state changes if the connection has been aborted.
          if (sendFlag) {
            setState(self.LOADING);
          }
        });

        response.on('end', function() {
          if (sendFlag) {
            // Discard the 'end' event if the connection has been aborted
            setState(self.DONE);
            sendFlag = false;
          }
        });

        response.on('error', function(error) {
          self.handleError(error);
        });
      }).on('error', function(error) {
        self.handleError(error);
      });

      // Node 0.4 and later won't accept empty data. Make sure it's needed.
      if (data) {
        request.write(data);
      }

      request.end();
    } else { // Synchronous
      // Create a temporary file for communication with the other Node process
      var syncFile = ".node-xmlhttprequest-sync-" + process.pid;
      fs.writeFileSync(syncFile, "", "utf8");
      // The async request the other Node process executes
      var execString = "var http = require('http'), https = require('https'), fs = require('fs');"
        + "var doRequest = http" + (ssl?"s":"") + ".request;"
        + "var options = " + JSON.stringify(options) + ";"
        + "var responseText = '';"
        + "var req = doRequest(options, function(response) {"
        + "response.setEncoding('utf8');"
        + "response.on('data', function(chunk) {"
        + "responseText += chunk;"
        + "});"
        + "response.on('end', function() {"
        + "fs.writeFileSync('" + syncFile + "', 'NODE-XMLHTTPREQUEST-STATUS:' + response.statusCode + ',' + responseText, 'utf8');"
        + "});"
        + "response.on('error', function(error) {"
        + "fs.writeFileSync('" + syncFile + "', 'NODE-XMLHTTPREQUEST-ERROR:' + JSON.stringify(error), 'utf8');"
        + "});"
        + "}).on('error', function(error) {"
        + "fs.writeFileSync('" + syncFile + "', 'NODE-XMLHTTPREQUEST-ERROR:' + JSON.stringify(error), 'utf8');"
        + "});"
        + (data ? "req.write('" + data.replace(/'/g, "\\'") + "');":"")
        + "req.end();";
      // Start the other Node Process, executing this string
       syncProc = spawn(process.argv[0], ['-e', execString]);
      while((self.responseText = fs.readFileSync(syncFile, 'utf8')) == '') {
        // Wait while the file is empty
      }
      // Kill the child process once the file has data
      syncProc.stdin.end();
      // Remove the temporary file
      fs.unlinkSync(syncFile);
      if(self.responseText.match(/^NODE-XMLHTTPREQUEST-ERROR:/)) {
        // If the file returned an error, handle it
        var errorObj = self.responseText.replace(/^NODE-XMLHTTPREQUEST-ERROR:/, "");
        self.handleError(errorObj);
      } else {
        // If the file returned okay, parse its data and move to the DONE state
        self.status = self.responseText.replace(/^NODE-XMLHTTPREQUEST-STATUS:([0-9]*),.*/, "$1");
        self.responseText = self.responseText.replace(/^NODE-XMLHTTPREQUEST-STATUS:[0-9]*,(.*)/, "$1");
        setState(self.DONE);
      }
    }
  };

  this.handleError = function(error) {
    this.status = 503;
    this.statusText = error;
    this.responseText = error.stack;
    errorFlag = true;
    setState(this.DONE);
  };

  /**
   * Aborts a request.
   */
  this.abort = function() {
    if (request) {
      request.abort();
      request = null;
    }

    headers = defaultHeaders;
    this.responseText = '';
    this.responseXML = '';

    errorFlag = true;

    if (this.readyState !== this.UNSENT &&
       (this.readyState !== this.OPENED || sendFlag) &&
        this.readyState !== this.DONE) {
      sendFlag = false;
      setState(this.DONE);
    }
    this.readyState = this.UNSENT;
  };

  this.addEventListener = function(event, callback) {
    if (!(event in listeners)) {
      listeners[event] = [];
    }
    listeners[event].push(callback);
  };
};

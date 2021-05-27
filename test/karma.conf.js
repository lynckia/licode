// Karma configuration
// Generated on Thu Oct 24 2013 23:47:37 GMT+0200 (CEST)

module.exports = function(config) {
  config.set({

    // base path, that will be used to resolve files and exclude
    basePath: '..',


    // frameworks to use
    frameworks: ['mocha', 'chai', 'sinon'],


    // list of files / patterns to load in the browser
    files: [
      'erizo_controller/erizoClient/dist/debug/erizo/erizo.js',
      {
        pattern: 'erizo_controller/erizoClient/dist/debug/erizo/erizo.js.map',
        included: false,
      },
      'erizo_controller/test/erizoClient/*.js',
    ],

    // list of files to exclude
    exclude: [
      'erizo_controller/common/PerformanceStats.js',
      'erizo_controller/common/ROV/*.js',
    ],

    // test results reporter to use
    // possible values: 'dots', 'progress', 'junit', 'growl', 'coverage'
    reporters: ['mocha'],

    // web server port
    port: 9876,


    // enable / disable colors in the output (reporters and logs)
    colors: true,


    // level of logging
    // possible values: config.LOG_DISABLE || config.LOG_ERROR || config.LOG_WARN || config.LOG_INFO || config.LOG_DEBUG
    logLevel: config.LOG_DISABLE,

    // enable / disable watching file and executing tests whenever any file changes
    autoWatch: false,


    // Start these browsers, currently available:
    // - Chrome
    // - ChromeCanary
    // - Firefox
    // - Opera
    // - Safari (only Mac)
    // - PhantomJS
    // - IE (only Windows)
    browsers: ['ChromeHeadless_with_fake_media'],

    customLaunchers: {
      ChromeHeadless_with_fake_media: {
        base: 'ChromeHeadless',
        flags: [
          '--use-fake-device-for-media-stream',
          '--use-fake-ui-for-media-stream',
          '--autoplay-policy=no-user-gesture-required',  // Needed for the new autoplay policy in
          '--unsafely-treat-insecure-origin-as-secure=localhost',
          '--remote-debugging-port=9333']
      },
      Chrome_with_fake_media: {
        base: 'Chrome',
        flags: [
          '--use-fake-device-for-media-stream',
          '--use-fake-ui-for-media-stream',
          '--autoplay-policy=no-user-gesture-required',  // Needed for the new autoplay policy in
          '--unsafely-treat-insecure-origin-as-secure=localhost',
          '--remote-debugging-port=9333']
      },
    },

    // If browser does not capture in given timeout [ms], kill it
    captureTimeout: 60000,

    plugins: [
      'karma-mocha',
      'karma-chai',
      'karma-sinon',
      'karma-mocha-reporter',
      'karma-chrome-launcher',
    ],

    // Continuous Integration mode
    // if true, it capture browsers, run tests and exit
    singleRun: true,

    client: {
      mocha: {
        // change Karma's debug.html to the mocha web reporter
        reporter: 'html',

        allowUncaught: true,

        // require specific files after Mocha is initialized
        require: [],

        // custom ui, defined in required file above
        ui: 'bdd',
      }
    }
  });
};

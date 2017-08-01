/* global module */

module.exports = {
  apps: [
    {
      name: 'client-app',
      script: 'basicServer.js',
      exec_mode: 'cluster',
      instances: 0,
      out_file: 'logs/out.log',
      error_file: 'logs/error.log',
    },
  ],
};

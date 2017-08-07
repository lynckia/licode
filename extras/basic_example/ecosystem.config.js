/* global module */

module.exports = {
  apps: [
    {
      name: 'client-app',
      script: 'basicServer.js',
      exec_mode: 'cluster',
      instances: 0,
      out_file: '/var/log/clientApp/out.log',
      error_file: '/var/log/clientApp/error.log',
    },
  ],
};

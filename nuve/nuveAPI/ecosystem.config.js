/* global module */

module.exports = {
  apps: [
    {
      name: 'nuve-api',
      script: 'nuve.js',
      exec_mode: 'cluster',
      instances: 0,
      out_file: '/var/log/nuveAPI/out.log',
      error_file: '/var/log/nuveAPI/error.log',
    },
  ],
};

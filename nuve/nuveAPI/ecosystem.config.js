/* global module */

module.exports = {
  apps: [
    {
      name: 'nuve-api',
      script: 'nuve.js',
      exec_mode: 'cluster',
      instances: 0,
      out_file: 'logs/out.log',
      error_file: 'logs/error.log',
    },
  ],
};

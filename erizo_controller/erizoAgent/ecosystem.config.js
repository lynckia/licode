/* global module */

module.exports = {
  apps: [
    {
      name: 'erizo-agent',
      script: 'erizoAgent.js',
      out_file: '/var/log/erizoAgent/out.log',
      error_file: '/var/log/erizoAgent/error.log',
    },
  ],
};

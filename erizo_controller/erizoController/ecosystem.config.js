/* global module */

module.exports = {
  apps: [
    {
      exec_mode: 'fork_mode',
      name: 'erizo-controller',
      script: 'erizoController.js',
      out_file: '/var/log/erizoController/out.log',
      error_file: '/var/log/erizoController/error.log',
      instance_var: 'NODE_APP_INSTANCE',
      instances: Number(process.env.ERIZO_CONTROLLER_INSTANCES),
    },
  ],
};

var host_config = {};

/*
 * Host-specific configuration included by licode_config.js
 * Intended so that puppet/chef/etc. deployment scripts can simply
 * create file and not have to edit licode_config.js
 */

/* If the erizo controller uses SSL, you need to set a public hostname,
 * and make sure your SSL certs are set up properly.
 */
host_config.publicHostname = '';

/* Override this if you are behind a NAT */
host_config.publicIP = '';

/* Override this if you are using a cloud provider (e.g. 'amazon') */
host_config.cloudProviderName = '';

/* override if you don't want to use licode's default port range */
host_config.minport = 0;
host_config.maxport = 0;

module.exports = host_config;


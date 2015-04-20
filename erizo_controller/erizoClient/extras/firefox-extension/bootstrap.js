/* This Source Code Form is subject to the terms of the Mozilla Public 
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 * The source code can be found here: HenrikJoreteg/getScreenMedia/firefox-extension-sample
 */
 
var domainsUsed = ["*.dit.upm.es"];
var addon_domains = [];
var allowed_domains = "media.getusermedia.screensharing.allowed_domains";

function startup(data, reason) {
    if (reason === APP_STARTUP) {
        return;
    }
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
    var values = prefs.getCharPref(allowed_domains).split(',');
    domainsUsed.forEach(function (domain) {
        if (values.indexOf(domain) === -1) {
            values.push(domain);
            addon_domains.push(domain);
        }
    });
    prefs.setCharPref(allowed_domains, values.join(','));
}

function shutdown(data, reason) {
    if (reason === APP_SHUTDOWN) {
        return;
    }
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
    var values = prefs.getCharPref(allowed_domains).split(',');
    values = values.filter(function (value) {
        return addon_domains.indexOf(value) === -1;
    });
    prefs.setCharPref(allowed_domains, values.join(','));
}

function install(data, reason) {}

function uninstall(data, reason) {}
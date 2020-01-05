/* global Components, APP_STARTUP, APP_SHUTDOWN */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 * The source code can be found here: HenrikJoreteg/getScreenMedia/firefox-extension-sample
 */
'use strict';

var domainsUsed = ['*.dit.upm.es'];
var addonDomains = [];
var allowedDomains = 'media.getusermedia.screensharing.allowedDomains';

function startup(data, reason) {
    if (reason === APP_STARTUP) {
        return;
    }
    var prefs = Components.classes['@mozilla.org/preferences-service;1'].
                              getService(Components.interfaces.nsIPrefBranch);
    var values = prefs.getCharPref(allowedDomains).split(',');
    domainsUsed.forEach(function (domain) {
        if (values.indexOf(domain) === -1) {
            values.push(domain);
            addonDomains.push(domain);
        }
    });
    prefs.setCharPref(allowedDomains, values.join(','));
}

function shutdown(data, reason) {
    if (reason === APP_SHUTDOWN) {
        return;
    }
    var prefs = Components.classes['@mozilla.org/preferences-service;1'].
                              getService(Components.interfaces.nsIPrefBranch);
    var values = prefs.getCharPref(allowedDomains).split(',');
    values = values.filter(function (value) {
        return addonDomains.indexOf(value) === -1;
    });
    prefs.setCharPref(allowedDomains, values.join(','));
}

function install(data, reason) {}

function uninstall(data, reason) {}

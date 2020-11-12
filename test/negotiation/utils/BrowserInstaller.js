const puppeteer = require('puppeteer-core');
const cliProgress = require('cli-progress');
const axios = require('axios');

const chromeVersionsUrl = 'http://omahaproxy.appspot.com/all.json';
const platform = 'mac';

let revisionInfo;

const BrowserInstaller = {
  installed: false,
  revisionInfo: {},
  async install(chromeVersion = 'canary') {
    const res = await axios( { url: chromeVersionsUrl, method: 'GET', responseType: 'blob' } );
    const records = res.data;
    let chromeRevision;

    records.forEach((record) => {
      if (record.os === platform) {
        record.versions.forEach((version) => {
          if (version.channel === chromeVersion) {
            chromeRevision = version.branch_base_position;
          }
        });
      }
    });
    if (!chromeRevision) {
      console.log('Chrome version was not found');
      return;
    }
    const browserFetcher = puppeteer.createBrowserFetcher();
    const bar = new cliProgress.SingleBar({
      format: `Downloading Chromium r${chromeRevision}  [{bar}] {percentage}% | {value}MB/{total}MB`
    }, cliProgress.Presets.legacy);
    let barStarted = false;
    chromeRevision = '801937';
    BrowserInstaller.revisionInfo = await browserFetcher.download(chromeRevision, (downloadedBytes, totalBytes) => {
      const totalMB = parseInt(totalBytes / 1000000);
      const downloadedMB = parseInt(downloadedBytes / 1000000);

      if (!barStarted) {
        bar.start(totalMB, downloadedMB);
        barStarted = true;
      }
      bar.update(downloadedMB);
    });
    bar.update(100);
    bar.stop();
    console.log('');
    BrowserInstaller.installed = true;
  }
};

module.exports = BrowserInstaller;

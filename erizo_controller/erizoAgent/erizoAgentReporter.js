/* global require, exports */


const os = require('os');

exports.Reporter = (spec) => {
  const that = {};


  const myId = spec.id;


  const myMeta = spec.metadata || {};

  let lastTotal = 0;
  let lastIdle = 0;

  const getStats = () => {
    const cpus = os.cpus();

    let user = 0;
    let nice = 0;
    let sys = 0;
    let idle = 0;
    let irq = 0;
    let total = 0;
    let cpu = 0;

    cpus.forEach((singleCpuInfo) => {
      user += singleCpuInfo.times.user;
      nice += singleCpuInfo.times.nice;
      sys += singleCpuInfo.times.sys;
      irq += singleCpuInfo.times.irq;
      idle += singleCpuInfo.times.idle;
    });
    total = user + nice + sys + idle + irq;

    cpu = 1 - ((idle - lastIdle) / (total - lastTotal));
    const mem = 1 - (os.freemem() / os.totalmem());

    lastTotal = total;
    lastIdle = idle;

    const data = {
      perc_cpu: cpu,
      perc_mem: mem,
    };
    return data;
  };

  that.getErizoAgent = (callback) => {
    const data = {
      info: {
        id: myId,
        rpc_id: `ErizoAgent_${myId}`,
      },
      metadata: myMeta,
      stats: getStats(),
    };
    callback(data);
  };

  return that;
};

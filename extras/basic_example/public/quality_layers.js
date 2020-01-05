/* globals Erizo, Highcharts */

/* eslint-env browser */
/* eslint-disable no-param-reassign, no-console, no-restricted-syntax, guard-for-in */


const serverUrl = '/';
let room;
const charts = new Map();

const getParameterByName = (name) => {
  // eslint-disable-next-line
  name = name.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
  const regex = new RegExp(`[\\?&]${name}=([^&#]*)`);
  const results = regex.exec(location.search);
  return results == null ? '' : decodeURIComponent(results[1].replace(/\+/g, ' '));
};

const spatialStyles = ['ShortDot', 'Dash', 'DashDot', 'ShortDashDotDot'];
const temporalStyles = ['#7cb5ec', '#90ed7d', '#f7a35c', '#f15c80'];

const clearTabs = () => {
  const tabElement = document.getElementById('tabs');
  const elements = tabElement.querySelectorAll('button');
  elements.forEach((element) => {
    tabElement.removeChild(element);
  });
};

const clearChartsNode = () => {
  const chartElement = document.getElementById('charts');
  const elements = chartElement.querySelectorAll('.chart');
  elements.forEach((element) => {
    chartElement.removeChild(element);
  });
};

const openStream = (streamId, evt) => {
  if (charts.size === 1 && charts.has(parseInt(streamId, 10))) {
    return;
  }
  charts.clear();
  clearChartsNode();
  if (streamId === 'all') {
    room.remoteStreams.forEach((stream) => {
      charts.set(parseInt(stream.getID(), 10), new Map());
    });
  } else {
    charts.set(parseInt(streamId, 10), new Map());
  }

  const tablinks = document.getElementsByClassName('tablinks');
  for (let i = 0; i < tablinks.length; i += 1) {
    tablinks[i].className = tablinks[i].className.replace(' active', '');
  }
  evt.currentTarget.className += ' active';
};

const createTabs = (list) => {
  const tabNode = document.getElementById('tabs');
  list.forEach((id) => {
    const button = document.createElement('button');
    button.setAttribute('class', 'tablinks');
    button.setAttribute('id', id);
    button.addEventListener('click', openStream.bind(this, id));
    button.textContent = id;
    tabNode.appendChild(button);
  });
  const button = document.createElement('button');
  button.setAttribute('class', 'tablinks');
  button.setAttribute('id', 'all');
  button.addEventListener('click', openStream.bind(this, 'all'));
  button.textContent = 'All';
  tabNode.appendChild(button);
};

const createList = () => {
  const streamIds = room.remoteStreams.keys();
  clearTabs();
  createTabs(streamIds);
};

const toBitrateString = (value) => {
  // eslint-disable-next-line no-restricted-properties
  const result = Math.floor(value / Math.pow(2, 10));
  return `${result}kbps`;
};

const initChart = (stream, subId) => {
  const pubId = stream.getID();
  console.log('Init Chart ', stream.getID(), subId);
  if (!charts.has(pubId)) {
    return undefined;
  }
  const parent = document.getElementById('charts');
  const div = document.createElement('div');
  div.setAttribute('style', 'width: 500px; height:500px; float:left;');
  div.setAttribute('id', `chart${pubId}_${subId}`);
  div.setAttribute('class', 'chart');

  parent.appendChild(div);

  const chart = new Highcharts.Chart({
    chart: {
      renderTo: `chart${pubId}_${subId}`,
      defaultSeriesType: 'line',
      animation: false,
      showAxes: true,
      events: {
      },
    },
    plotOptions: {
      series: {
        marker: {
          enabled: false,
        },
      },
    },
    title: {
      text: `pub: ${pubId},\nsub: ${subId}`,
    },
    xAxis: {
      type: 'datetime',
      tickPixelInterval: 150,
      maxZoom: 20 * 1000,
    },
    yAxis: {
      minPadding: 0.2,
      maxPadding: 0.2,
      title: {
        text: null,
        margin: 80,
      },
    },
    tooltip: {
      formatter() {
        let s = '';
        let selectedLayers = 'Spatial: 0 / Temporal: 0';

        for (const point of this.points) {
          s += `<br/>${point.series.name}: ${toBitrateString(point.point.y)}`;
          selectedLayers = point.point.name || selectedLayers;
        }
        s = `<b>${selectedLayers}</b>${s}`;
        return s;
      },
      crosshairs: [true, true],
      shared: true,
    },
    series: [],
  });
  charts.get(pubId).set(subId, chart);
  return chart;
};

const getOrCreateChart = (stream, subId) => {
  const pubId = stream.getID();
  let chart;
  if (!charts.has(pubId)) {
    return undefined;
  }
  if (!charts.get(pubId).has(subId)) {
    chart = {
      seriesMap: {},
      chart: initChart(stream, subId),
    };
    charts.get(pubId).set(subId, chart);
  } else {
    chart = charts.get(pubId).get(subId);
  }
  return chart;
};

const updateSeriesForKey = (stream, subId, key, spatial, temporal, valueX, valueY,
  pointName = undefined, isActive = true) => {
  const chart = getOrCreateChart(stream, subId);
  if (chart.seriesMap[key] === undefined) {
    let dash;
    let color;
    let width = 3;
    if (spatial && temporal) {
      dash = spatialStyles[spatial];
      color = temporalStyles[temporal];
      width = 2;
    } else if (key === 'Current Received') {
      color = '#2b908f';
    } else if (key === 'Estimated Bandwidth') {
      color = '#434348';
    }

    chart.seriesMap[key] = chart.chart.addSeries({
      name: key,
      dashStyle: dash,
      color,
      lineWidth: width,
      data: [],
    });
  }
  const seriesForKey = chart.seriesMap[key];
  const shift = seriesForKey.data.length > 30;
  const point = { x: valueX, y: valueY, name: pointName };
  if (!isActive) {
    point.marker = {
      radius: 4,
      lineColor: 'red',
      fillColor: 'red',
      lineWidth: 1,
      symbol: 'circle',
    };
  }
  seriesForKey.addPoint(point, true, shift);
};

const updateCharts = (stream) => {
  const date = (new Date()).getTime();
  room.getStreamStats(stream, (data) => {
    for (const i in data) {
      if (i !== 'publisher') {
        const subId = i;
        let selectedLayers = '';
        const qualityLayersData = data[i].qualityLayers;

        if (qualityLayersData !== undefined) {
          const maxActiveSpatialLayer = qualityLayersData.maxActiveSpatialLayer || 0;
          for (const spatialLayer in qualityLayersData) {
            for (const temporalLayer in qualityLayersData[spatialLayer]) {
              const key = `Spatial ${spatialLayer} / Temporal ${temporalLayer}`;
              updateSeriesForKey(stream, subId, key, spatialLayer, temporalLayer,
                date, qualityLayersData[spatialLayer][temporalLayer], undefined,
                maxActiveSpatialLayer >= spatialLayer);
            }
          }
          if (qualityLayersData.selectedSpatialLayer !== undefined) {
            selectedLayers += `Spatial: ${qualityLayersData.selectedSpatialLayer
            } / Temporal: ${qualityLayersData.selectedTemporalLayer}`;
          }
        }
        if (!data[i].total) {
          return;
        }
        const totalBitrate = data[i].total.bitrateCalculated || 0;
        const bitrateEstimated = data[i].total.senderBitrateEstimation || 0;
        const remb = data[i].total.bandwidth || 0;
        const targetVideoBitrate = data[i].total.targetVideoBitrate || 0;
        const numberOfStreams = data[i].total.numberOfStreams || 0;
        const paddingBitrate = data[i].total.paddingBitrate || 0;
        const rtxBitrate = data[i].total.rtxBitrate || 0;

        updateSeriesForKey(stream, subId, 'Current Received', undefined, undefined,
          date, totalBitrate, selectedLayers);
        updateSeriesForKey(stream, subId, 'Estimated Bandwidth', undefined, undefined,
          date, bitrateEstimated);
        updateSeriesForKey(stream, subId, 'REMB Bandwidth', undefined, undefined,
          date, remb);
        updateSeriesForKey(stream, subId, 'Target Video Bitrate', undefined, undefined,
          date, targetVideoBitrate);
        updateSeriesForKey(stream, subId, 'Number Of Streams', undefined, undefined,
          date, numberOfStreams);
        updateSeriesForKey(stream, subId, 'Padding Bitrate', undefined, undefined,
          date, paddingBitrate);
        updateSeriesForKey(stream, subId, 'Rtx Bitrate', undefined, undefined,
          date, rtxBitrate);
      }
    }
  });
};

setInterval(() => {
  for (const stream of charts.keys()) {
    updateCharts(room.remoteStreams.get(stream));
  }
}, 1000);

window.onload = () => {
  const roomName = getParameterByName('room') || 'basicExampleRoom';
  let streamId = parseInt(getParameterByName('stream'), 10);
  const roomId = getParameterByName('roomId');
  streamId = isNaN(streamId) ? undefined : streamId;
  console.log('Selected Room', roomName);
  const createToken = (userName, role, roomNameForToken, callback) => {
    const req = new XMLHttpRequest();
    const url = `${serverUrl}createToken/`;
    const body = { username: userName, role };
    body.room = roomId ? undefined : roomNameForToken;
    body.roomId = roomId;

    req.onreadystatechange = () => {
      if (req.readyState === 4) {
        callback(req.responseText);
      }
    };

    req.open('POST', url, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.send(JSON.stringify(body));
  };

  const onStreamAdded = (stream) => {
    const id = stream.getID();
    if (streamId && streamId !== id) {
      return;
    }
    createList();
  };

  const initStreams = () => {
    room.remoteStreams.forEach((stream) => {
      onStreamAdded(stream);
    });
  };

  const onStreamDeleted = (stream) => {
    console.log('Stream deleted', stream);
  };

  createToken('user', 'presenter', roomName, (response) => {
    const token = response;
    console.log(token);
    room = Erizo.Room({ token });

    room.addEventListener('room-connected', (roomEvent) => {
      console.log('Room Connected', roomEvent);
      initStreams();
    });

    room.addEventListener('stream-subscribed', (streamEvent) => {
      console.log('Stream subscribed', streamEvent.stream);
    });

    room.addEventListener('stream-added', (streamEvent) => {
      onStreamAdded(streamEvent.stream);
    });

    room.addEventListener('stream-removed', (streamEvent) => {
      onStreamDeleted(streamEvent.stream);
    });

    room.addEventListener('stream-failed', () => {
      console.log('Stream Failed, act accordingly');
    });

    room.connect();
  });
};

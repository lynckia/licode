/* globals Erizo, Highcharts */
'use strict';
const serverUrl = '/';
let room;
const charts = new Map();

function getParameterByName(name) {
    name = name.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
    var regex = new RegExp('[\\?&]' + name + '=([^&#]*)'),
        results = regex.exec(location.search);
    return results == null ? '' : decodeURIComponent(results[1].replace(/\+/g, ' '));
}

var spatialStyles = ['ShortDot', 'Dash', 'DashDot', 'ShortDashDotDot'];
var temporalStyles = ['#7cb5ec', '#90ed7d', '#f7a35c', '#f15c80'];

let clearTabs = function() {
  let tabElement = document.getElementById('tabs');
  let elements = tabElement.querySelectorAll('button');
  for (let element of elements) {
    tabElement.removeChild(element);
  }
};

let clearChartsNode = function() {
  let chartElement = document.getElementById('charts');
  let elements = chartElement.querySelectorAll('.chart');
  for (let element of elements) {
    chartElement.removeChild(element);
  }
};

let openStream = function(streamId, evt) {
  if (charts.size === 1 && charts.has(parseInt(streamId))) {
    return;
  }
  charts.clear();
  clearChartsNode();
  if (streamId === 'all') {
    for (let stream of Object.keys(room.remoteStreams)) {
      charts.set(parseInt(stream), new Map());
    }
  } else {
    charts.set(parseInt(streamId), new Map());
  }

  let tablinks = document.getElementsByClassName('tablinks');
  for (let i = 0; i < tablinks.length; i++) {
      tablinks[i].className = tablinks[i].className.replace(' active', '');
  }
  evt.currentTarget.className += ' active';
};

let createTabs = function(list) {
  let tabNode = document.getElementById('tabs');
  for (let id of list) {
    let button = document.createElement('button');
    button.setAttribute('class', 'tablinks');
    button.setAttribute('id', id);
    button.addEventListener('click', openStream.bind(this, id));
    button.textContent = id;
    tabNode.appendChild(button);
  }
  let button = document.createElement('button');
  button.setAttribute('class', 'tablinks');
  button.setAttribute('id', 'all');
  button.addEventListener('click', openStream.bind(this, 'all'));
  button.textContent = 'All';
  tabNode.appendChild(button);
};

let createList = function() {
  let streamIds = Object.keys(room.remoteStreams);
  clearTabs();
  createTabs(streamIds);
};

let toBitrateString = function(value) {
  let result = Math.floor(value / Math.pow(2, 10));
  return result + 'kbps';
};

var initChart = function (stream, subId) {
    let pubId = stream.getID();
    console.log('Init Chart ', stream.getID(), subId);
    if (!charts.has(pubId)) {
       return undefined;
    }
    var parent = document.getElementById('charts');
    var div = document.createElement('div');
    div.setAttribute('style', 'width: 500px; height:500px; float:left;');
    div.setAttribute('id', 'chart' + pubId + '_' + subId);
    div.setAttribute('class', 'chart');

    parent.appendChild(div);

    let chart = new Highcharts.Chart({
        chart: {
            renderTo: 'chart' + pubId + '_' + subId,
            defaultSeriesType: 'line',
            animation: false,
            showAxes: true,
            events: {
            }
        },
        plotOptions: {
            series: {
                marker: {
                    enabled: false
                }
            }
        },
        title: {
            text: `pub: ${pubId},\nsub: ${subId}`
        },
        xAxis: {
            type: 'datetime',
            tickPixelInterval: 150,
            maxZoom: 20 * 1000
        },
        yAxis: {
            minPadding: 0.2,
            maxPadding: 0.2,
            title: {
                text: null,
                margin: 80
            }
        },
        tooltip: {
          formatter: function() {
            let s = '';
            let selectedLayers = 'Spatial: 0 / Temporal: 0';
            for (let point of this.points) {
              s += '<br/>' + point.series.name + ': ' + toBitrateString(point.point.y);
              selectedLayers = point.point.name || selectedLayers;
            }
            s = '<b>' + selectedLayers  + '</b>' + s;
            return s;
          },
          crosshairs: [true, true],
          shared: true
        },
        series: []
    });
    charts.get(pubId).set(subId, chart);
    return chart;
};

let getOrCreateChart = function(stream, subId) {
  let pubId = stream.getID();
  let chart;
  if (!charts.has(pubId)) {
    return undefined;
  }
  if (!charts.get(pubId).has(subId)) {
    chart = {
      seriesMap: {},
      chart: initChart(stream, subId)
    };
    charts.get(pubId).set(subId, chart);
  } else {
    chart = charts.get(pubId).get(subId);
  }
  return chart;
};

var updateSeriesForKey = function (stream, subId, key, spatial, temporal, valueX, valueY,
  pointName = undefined, isActive = true) {
    let chart = getOrCreateChart(stream, subId);
    if (chart.seriesMap[key] === undefined) {

        let dash, color, width = 3;
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
            color: color,
            lineWidth: width,
            data: []
        });
    }
    let seriesForKey = chart.seriesMap[key];
    let shift = seriesForKey.data.length > 30;
    let point = { x: valueX, y: valueY, name: pointName };
    if (!isActive) {
      point.marker = {
          radius: 4,
          lineColor: 'red',
          fillColor: 'red',
          lineWidth: 1,
          symbol: 'circle'
      };
    }
    seriesForKey.addPoint(point, true, shift);
};

let updateCharts = function (stream) {
    let date = (new Date()).getTime();
    room.getStreamStats(stream, function(data) {
        for (let i in data) {
            if (i !== 'publisher') {
                let subId = i;
                let selectedLayers = '';
                let qualityLayersData = data[i].qualityLayers;

                if (qualityLayersData !== undefined) {
                    let maxActiveSpatialLayer = qualityLayersData.maxActiveSpatialLayer || 0;
                    for (var spatialLayer in qualityLayersData) {
                        for (var temporalLayer in qualityLayersData[spatialLayer]) {
                            let key = 'Spatial ' + spatialLayer + ' / Temporal ' + temporalLayer;
                            updateSeriesForKey(stream, subId, key, spatialLayer, temporalLayer,
                              date, qualityLayersData[spatialLayer][temporalLayer], undefined,
                              maxActiveSpatialLayer >= spatialLayer);
                        }
                    }
                    if (qualityLayersData.selectedSpatialLayer) {
                      selectedLayers += 'Spatial: ' + qualityLayersData.selectedSpatialLayer +
                      ' / Temporal: '+ qualityLayersData.selectedTemporalLayer;
                    }
                }

                let totalBitrate = data[i].total.bitrateCalculated || 0;
                let bitrateEstimated = data[i].total.senderBitrateEstimation || 0;
                let paddingBitrate = data[i].total.paddingBitrate || 0;
                let rtxBitrate = data[i].total.rtxBitrate || 0;

                updateSeriesForKey(stream, subId, 'Current Received', undefined, undefined,
                  date, totalBitrate, selectedLayers);
                updateSeriesForKey(stream, subId, 'Estimated Bandwidth', undefined, undefined,
                  date, bitrateEstimated);
                updateSeriesForKey(stream, subId, 'Padding Bitrate', undefined, undefined,
                  date, paddingBitrate);
                updateSeriesForKey(stream, subId, 'Rtx Bitrate', undefined, undefined,
                  date, rtxBitrate);
            }
        }
    });

};

setInterval(() => {
  for (let stream of charts.keys()) {
    updateCharts(room.remoteStreams[stream]);
  }
}, 1000);

window.onload = function () {
    var roomName = getParameterByName('room') || 'basicExampleRoom';
    let streamId = parseInt(getParameterByName('stream'));
    let roomId = getParameterByName('roomId');
    streamId = isNaN(streamId) ? undefined : streamId;
    console.log('Selected Room', roomName);
    var createToken = function(userName, role, roomName, callback) {

        var req = new XMLHttpRequest();
        var url = serverUrl + 'createToken/';
        var body = {username: userName, role: role};
        body.room = roomId ? undefined : roomName;
        body.roomId = roomId;

        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                callback(req.responseText);
            }
        };

        req.open('POST', url, true);
        req.setRequestHeader('Content-Type', 'application/json');
        req.send(JSON.stringify(body));
    };

    let onStreamAdded = function(stream) {
      let id = stream.getID();
      if (streamId && streamId !== id) {
        return;
      }
      createList();
    };

    let initStreams = function() {
      let remoteStreams = room.remoteStreams;
      let remoteStreamIds = Object.keys(remoteStreams);
      for (let streamId of remoteStreamIds) {
        onStreamAdded(remoteStreams[streamId]);
      }
    };

    let onStreamDeleted = function(stream) {
      console.log('Stream deleted', stream);
    };

    createToken('user', 'presenter', roomName, function (response) {
        var token = response;
        console.log(token);
        room = Erizo.Room({token: token});

        room.addEventListener('room-connected', function (roomEvent) {
            console.log('Room Connected', roomEvent);
            initStreams();
        });

        room.addEventListener('stream-subscribed', function(streamEvent) {
          console.log('Stream subscribed', streamEvent.stream);
        });

        room.addEventListener('stream-added', function (streamEvent) {
            onStreamAdded(streamEvent.stream);
        });

        room.addEventListener('stream-removed', function (streamEvent) {
            onStreamDeleted(streamEvent.stream);
        });

        room.addEventListener('stream-failed', function (){
            console.log('Stream Failed, act accordingly');
        });

        room.connect();

    });
};

/* globals Erizo */
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

var initChart = function (stream, subId) {
    let pubId = stream.getID();
    console.log("Init Chart ", stream.getID(), subId);
    if (!charts.has(pubId)) {
       return undefined;
    }
    var div = document.createElement('div');
    div.setAttribute('style', 'width: 100%; height:500px; float:right;');
    div.setAttribute('id', 'chart' + pubId + '_' + subId);

    document.body.appendChild(div);

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
            text: `Live Layer Bitrate (pub: ${pubId}, sub: ${subId})`
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
            return this.point.name;
          }
        },
        series: []
    });
    charts.get(pubId).set(subId, chart);
    return chart;
};
var updateSeriesForKey = function (stream, subId, key, spatial, temporal, value_x, value_y, point_name = value_y, is_active = true) {
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
    let point = { x: value_x, y: value_y, name: point_name };
    if (!is_active) {
      point.marker = {
          radius: 4,
          lineColor: 'red',
          fillColor: 'red',
          lineWidth: 1,
          symbol: 'circle'
      };
    }
    seriesForKey.addPoint(point, true, shift);
}

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

var updateCharts = function (stream) {
    let date = (new Date()).getTime();
    room.getStreamStats(stream, function(data) {
        let pubId = stream.getID();
        for (let i in data) {
            if (i != "publisher") {
                let subId = i;
                let totalBitrate = data[i]["total"]["bitrateCalculated"];
                let qualityLayersData = data[i]["qualityLayers"];
                let bitrateEstimated = data[i]["total"]["senderBitrateEstimation"];
                let paddingBitrate = data[i]["total"]["paddingBitrate"];

                let rtxBitrate = data[i]["total"]["rtxBitrate"];

                let name = "";
                if (qualityLayersData !== undefined) {
                    let maxActiveSpatialLayer = qualityLayersData["maxActiveSpatialLayer"] || 0;
                    for (var spatialLayer in qualityLayersData) {
                        for (var temporalLayer in qualityLayersData[spatialLayer]) {
                            let key = "Spatial " + spatialLayer + " / Temporal " + temporalLayer;
                            updateSeriesForKey(stream, subId, key, spatialLayer, temporalLayer, date, qualityLayersData[spatialLayer][temporalLayer], undefined, maxActiveSpatialLayer >= spatialLayer)
                        }
                    }
                    if (qualityLayersData["selectedSpatialLayer"]) {
                      name += qualityLayersData["selectedSpatialLayer"] + "/" + qualityLayersData["selectedTemporalLayer"];
                    }
                }
                if (totalBitrate) {
                    name += " " + totalBitrate;
                    updateSeriesForKey(stream, subId, "Current Received", undefined, undefined, date, totalBitrate, name);
                }

                if (bitrateEstimated) {
                    updateSeriesForKey(stream, subId, "Estimated Bandwidth", undefined, undefined, date, bitrateEstimated);
                }

                if (paddingBitrate) {
                    updateSeriesForKey(stream, subId, "Padding Bitrate", undefined, undefined, date, paddingBitrate);
                }

                if (rtxBitrate) {
                    updateSeriesForKey(stream, subId, "Rtx Bitrate", undefined, undefined, date, paddingBitrate);
                }
            }
        }
        setTimeout(updateCharts.bind(this, stream), 1000);
    });

}

window.onload = function () {
    var roomName = getParameterByName('room') || 'basicExampleRoom';
    let streamId = parseInt(getParameterByName('stream'));
    streamId = isNaN(streamId) ? undefined : streamId;
    console.log('Selected Room', room);
    var createToken = function(userName, role, roomName, callback) {

        var req = new XMLHttpRequest();
        var url = serverUrl + 'createToken/';
        var body = {username: userName, role: role, room:roomName};

        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                callback(req.responseText);
            }
        };

        req.open('POST', url, true);
        req.setRequestHeader('Content-Type', 'application/json');
        req.send(JSON.stringify(body));
    };

    let initStreams = function() {
      let remoteStreams = room.remoteStreams;
      let remoteStreamIds = Object.keys(remoteStreams);
      for (let streamId of remoteStreamIds) {
        onStreamAdded(remoteStreams[streamId]);
      }
    };

    let onStreamAdded = function(stream) {
      let id = stream.getID();
      if (streamId && streamId !== id) {
        return;
      }
      charts.set(id, new Map());
      updateCharts(stream);
    };

    let onStreamDeleted = function(stream) {

    };

    createToken('user', 'presenter', roomName, function (response) {
        var token = response;
        console.log(token);
        room = Erizo.Room({token: token});

        room.addEventListener('room-connected', function (roomEvent) {
            console.log("Room Connected");
            initStreams();
        });

        room.addEventListener('stream-subscribed', function(streamEvent) {
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

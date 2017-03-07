/* globals Erizo */
'use strict';
var serverUrl = '/';
var localStream, room, chart;
var streamToChart;

function getParameterByName(name) {
  name = name.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
  var regex = new RegExp('[\\?&]' + name + '=([^&#]*)'),
      results = regex.exec(location.search);
  return results == null ? '' : decodeURIComponent(results[1].replace(/\+/g, ' '));
}

var seriesMap = {};

var initChart = function () {
    chart = new Highcharts.Chart({
        chart: {
            renderTo: 'chart',
            defaultSeriesType: 'spline',
            events: {
            //    load: updateChart
            }
        },
        title: {
            text: 'Live Layer Bitrate'
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
                text: 'Value',
                margin: 80
            }
        },
        series: []
    });
    console.log("LayerChart", chart);
};
var updateSeriesForKey = function (key, value_x, value_y) {
    if (seriesMap[key] === undefined) {
        seriesMap[key] = chart.addSeries({
            name: key,
            data: []
        });
    }
    let seriesForKey = seriesMap[key];
    let shift = seriesForKey.data.length > 60;
    seriesForKey.addPoint([value_x, value_y]);
}

var updateChart = function () {
   // console.log("UpdateChart", chart);
    let date = (new Date()).getTime();
    room.getStreamStats(streamToChart, function(data) {
        for (var i in data) {
            if (i != "publisher") {
               let totalBitrate = data[i]["total"]["bitrateCalculated"];
               if (totalBitrate)
                   updateSeriesForKey("Current Received",date, totalBitrate);
               let qualityLayersData = data[i]["qualityLayers"];
               if (qualityLayersData !== undefined){
                   for (var spatialLayer in qualityLayersData) {
                       for (var temporalLayer in qualityLayersData[spatialLayer]) {
                           let key = "Spatial " + spatialLayer + " / Temporal " + temporalLayer;
                           updateSeriesForKey(key, date, qualityLayersData[spatialLayer][temporalLayer])
                       }
                   }

               }
            }
        }
        setTimeout(updateChart, 1000);    
    });

}

window.onload = function () {
  var screen = getParameterByName('screen');
  var roomName = getParameterByName('room') || 'basicExampleRoom';
  console.log('Selected Room', room);
  var config = {audio: true,
                video: true,
                data: true,
                screen: screen,
                videoSize: [640, 480, 1280, 720],
                videoFrameRate: [10, 20]};
  // If we want screen sharing we have to put our Chrome extension id.
  // The default one only works in our Lynckia test servers.
  // If we are not using chrome, the creation of the stream will fail regardless.
  if (screen){
    config.extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
  }
  localStream = Erizo.Stream(config);
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

  createToken('user', 'presenter', roomName, function (response) {
    var token = response;
    console.log(token);
    room = Erizo.Room({token: token});

    localStream.addEventListener('access-accepted', function () {
      var subscribeToStreams = function (streams) {
        var cb = function (evt){
            console.log('Bandwidth Alert', evt.msg, evt.bandwidth);
        };
        for (var index in streams) {
          var stream = streams[index];
          if (localStream.getID() === stream.getID()) {
            room.subscribe(stream, {slideShowMode: false, metadata: {type: 'subscriber'}});
          }
        }
      };

      room.addEventListener('room-connected', function (roomEvent) {
        console.log("InitChart");
        initChart();
        console.log("Chart inited", chart);
        var options = {metadata: {type: 'publisher'}};
        var enableSimulcast = true; 
        if (enableSimulcast) options._simulcast = {numSpatialLayers: 2};

        var shouldPublish = getParameterByName('shouldPublish');
        if (shouldPublish) {
            room.publish(localStream, options);
        }
        subscribeToStreams(roomEvent.streams);
      });

      room.addEventListener('stream-subscribed', function(streamEvent) {
        var stream = streamEvent.stream;
        var div = document.createElement('div');
        div.setAttribute('style', 'width: 640px; height: 480px;');
        div.setAttribute('id', 'test' + stream.getID());
        
          if (streamToChart == undefined) {
            console.log("saving stream", stream);
            streamToChart = stream;
            updateChart();
        }

        document.body.appendChild(div);
        stream.show('test' + stream.getID());

      });

      room.addEventListener('stream-added', function (streamEvent) {
        var streams = [];
        streams.push(streamEvent.stream);
        subscribeToStreams(streams);
      });

      room.addEventListener('stream-removed', function (streamEvent) {
        // Remove stream from DOM
        var stream = streamEvent.stream;
        if (stream.elementID !== undefined) {
          var element = document.getElementById(stream.elementID);
          document.body.removeChild(element);
        }
      });

      room.addEventListener('stream-failed', function (){
          console.log('Stream Failed, act accordingly');
      });

      room.connect();

//      localStream.show('myVideo');

    });
      
    localStream.init();
  });
};

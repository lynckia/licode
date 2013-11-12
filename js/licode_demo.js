var serverUrl = "http://chotis2.dit.upm.es/";

var DEMO = {};
var localStream = undefined;
var demo_type = undefined;

function getParameterByName(name) {
    name = name.replace(/[\[]/, "\\\[").replace(/[\]]/, "\\\]");
    var regex = new RegExp("[\\?&]" + name + "=([^&#]*)"),
        results = regex.exec(location.search);
    return results == null ? "" : decodeURIComponent(results[1].replace(/\+/g, " "));
}

DEMO.create_token = function(userName, role, callback) {

    var req = new XMLHttpRequest();
    var url = serverUrl + 'token';
    var body = {roomId: getParameterByName('id'), username: userName, role: role};

    req.onreadystatechange = function () {
        if (req.readyState === 4) {
            callback(req.responseText);
        }
    };

    req.open('POST', url, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.send(JSON.stringify(body));
}

window.onload = function () {

    var check_meeting = function() {
        if(getParameterByName('id') !== '') {
            console.log('MEETING MODE');

            var req = new XMLHttpRequest();
            var url = serverUrl + 'roomtype/?id=' + getParameterByName('id');

            req.onreadystatechange = function () {
                if (req.readyState === 4) {
                    if (req.status === 200) {
                        $("#home_menu").removeClass("active");
                        $("#meeting_version_menu").removeClass("hide");
                        $("#meeting_version_menu").addClass("active");
                        $("#meeting_version_panel").removeClass("hide");
                        $("#meeting_version_rest").addClass("meeting_version_rest");
                        console.log('type ', req.responseText);
                        demo_type = req.responseText;
                    } else {
                        window.location.href = '/';
                    }
                }
            };

            req.open('GET', url, true);
            req.send();

            document.getElementById('meeting_version_username').onkeyup = function(e) {
              e = e || event;
              if (e.keyCode === 13) {
                  connect_user();
              }
              return true;
            }

            document.getElementById('meeting_version_button').onclick = connect_user;

        }
    }

    var connect_user = function () {
        
        $("#meeting_version_menu").addClass("intermittent");
        $('#meeting_version_conference').css('height', '500px');
        $("#meeting_version_form").addClass("hide");
        $("#meeting_version_conference").removeClass("hide");

        my_name = document.getElementById('meeting_version_username').value;

        $.getScript(serverUrl + "demos/" + demo_type +  "/" + demo_type + ".js", function(){
            console.log("Script loaded and executed.");
            DEMO.init_demo(my_name);
        });
    }

    check_meeting();

    var messText = document.getElementById('chat_message');
    var chat_body = document.getElementById('chat_body');

    var my_name;

    DEMO.close = function () {
        window.location.href = '/';
    }

    for(var i in document.getElementsByClassName('close_button')) {
        document.getElementsByClassName('close_button')[i].onclick = DEMO.close;
    }

    //document.getElementById('back_icon').onclick = DEMO.close;

    messText.onkeyup = function(e) {
      e = e || event;
      if (e.keyCode === 13) {
          DEMO.send_chat_message();
      }
      return true;
    }

    var add_text_to_chat = function(text, style) {
        var p = document.createElement('p');
        p.className = 'chat_' + style;
        p.innerHTML = text;
        chat_body.appendChild(p);
        chat_body.scrollTop = chat_body.scrollHeight;
    }

    DEMO.connect_to_chat = function() {
        add_text_to_chat('Succesfully connected to the room', 'italic');
    }

    DEMO.add_chat_participant = function(name) {
        add_text_to_chat('New participant: ' + name, 'italic');
    }

    DEMO.send_chat_message = function() {
        if(messText.value.match (/\S/)) {
            if (localStream) {
                localStream.sendData({msg: messText.value, name: my_name});
            }
            add_text_to_chat(my_name + ': ', 'name');
            add_text_to_chat(messText.value, '');
        }
        messText.value = '';
    };

    DEMO.chat_message_received = function(evt) {
        var msg = evt.msg;
        add_text_to_chat(msg.name + ': ', 'name');
        add_text_to_chat(msg.msg, '');
    }
}
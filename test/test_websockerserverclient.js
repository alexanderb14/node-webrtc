process.env.NODE_TLS_REJECT_UNAUTHORIZED = "0";

var WebSocketClient = require('websocket').client;
var uuid = require('node-uuid');

var wrtc = require('..');
var RTCPeerConnection = wrtc.RTCPeerConnection;
var MediaDevices = new wrtc.MediaDevices;

// --------------------------------------------------
var client = new WebSocketClient();
var connection;

pcUuid = uuid.v4();

client.on('connectFailed', function(error) {
    console.log('Connect Error: ' + error.toString());
});
 
client.on('connect', function(connection) {
  console.log('WebSocket Client Connected');
  connection.on('error', function(error) {
      console.log("Connection Error: " + error.toString());
  });
  connection.on('close', function() {
      console.log('echo-protocol Connection Closed');
  });
  connection.on('message', function(message) {
      if (message.type === 'utf8') {
          messageStr = message.utf8Data;

          messageJson = JSON.parse(messageStr);
          if(messageJson.uuid == uuid) return;

          if(messageJson.sdp) {
            console.log("==================================================== " + messageJson.sdp.type);            
          	
            if(messageJson.sdp.type == 'offer') {
          	  pc.setRemoteDescription(messageJson.sdp);
          	}
          } else if(messageJson.ice) {
          	pc.addIceCandidate(messageJson.ice);
          }
      }
  });

  var pc = new RTCPeerConnection();

  function onCreateSessionDescriptionError(error) {
    console.log('Failed to create session description: ' + error.toString());
  }

	function onCreateOfferSuccess(desc) {
    console.log('Session description created successfully.');
    
    pc.setLocalDescription({type: 'offer', sdp: desc.sdp});
	    connection.sendUTF(JSON.stringify({
			type: 'offer', 
			sdp: desc
      }));
	}

	pc.onicecandidate = function(candidate, sdpMid, sdpMLineIndex) {
	    connection.sendUTF(JSON.stringify({
	    	'ice': {
	    		'candidate': candidate,
	    		'sdpMid': sdpMid,
	    		'sdpMLineIndex': sdpMLineIndex
	    	}, 
	    	'uuid': pcUuid
	    }));
	}

  MediaDevices.getUserMedia(function(stream) {
      console.log(stream);

      pc.addStream(stream);

    	pc.createOffer()
        .then(
            onCreateOfferSuccess,
            onCreateSessionDescriptionError);
  });
});
 
client.connect('wss://localhost:8443', 'echo-protocol');

console.log("------------------");

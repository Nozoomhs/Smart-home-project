var roomCounter = 2;
var mqtt;
var reconnectTimeout=2000;
var host = "localhost";
var port = 9001;
var roomtopic = "/addroom";
var temptopic = "/control/temp";
var gpstopic = "/gps/isHome";
var feedbacktopic= "/control/feedback";
var hometopic = "/control/isHome";
var sensordata = "/sensordata"
var home = false;


function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function onConnect(){
	console.log("Connected");
	mqtt.subscribe(feedbacktopic);
	mqtt.subscribe(temptopic);
	message = new Paho.MQTT.Message("Client Connected Sucessfully");
    message.destinationName = gpstopic;
    mqtt.send(message);
}
/*
function onConnect() {
	// Once a connection has been made, make a subscription and send a message.
	console.log("onConnect");
	client.subscribe("/World");
	message = new Paho.MQTT.Message("Hello");
	message.destinationName = "/World";
	client.send(message);
  };
*/
/*
function onConnectionLost(responseObject) {
	if (responseObject.errorCode !== 0)
	  console.log("onConnectionLost:"+responseObject.errorMessage);
  };
function onMessageArrived(message) {
	console.log("onMessageArrived:"+message.payloadString);
  };
  */
function HomeAway(indicator)
{
	if(indicator == true)
	{
		document.getElementsByClassName("status home").style.visibility="visible";
		document.getElementsByClassName("status away").style.visibility="hidden";
	}
	else{
		document.getElementsByClassName("status home").style.visibility="hidden";
		document.getElementsByClassName("status away").style.visibility="visible";
	}
	
}
function MQTTconnect() {
	console.log("connecting to "+ host +" "+ port);
	mqtt = new Paho.MQTT.Client(host,port,"clientjs");
	var options = {
		timeout: 3,
		onSuccess: onConnect,
		onFailure: onFailure,
		 };
	mqtt.onMessageArrived = onMessageArrived;
	mqtt.connect(options); //connect
	}
/*
function MQTTconnect(){
	console.log("connection to "+host +" " +port);
	var options = {
		onSuccess:onConnect,
		onFailure:onFailure,
	};
	mqtt= new Paho.MQTT.Client(host,port,"controlapp");
	mqtt.onMessageArrived = onMessageArrived;
	mqtt.connect(options);

}
*/
function onFailure(message) {
	console.log("Connection Attempt to Host "+host+"Failed");
	setTimeout(MQTTconnect, reconnectTimeout);
}
function setCurrentTemp(message){
	console.log("Setting current temp");
	for (let i = 1; i < roomCounter; i++) {
		document.getElementById("currentTemp" + i).innerHTML = message /*+ what is read from mqtt*/;
	}

}
function onMessageArrived(msg){
	
	var topic = msg.destinationName;
	var out_msg = "Message recieved" +msg.payloadString+ "with topic: " + topic +"<br>";
	console.log(out_msg);
	
	if(topic == gpstopic){
		publish(hometopic,"true");
		if(msg.payloadString == "1"){
			home = true;
		}
		else{
			home = false;
		}
	}
	else if (topic == feedbacktopic){
		setCurrentTemp(msg.payloadString);
	}

}
function publish(topic,message){
	message = new Paho.MQTT.Message(message);
	message.destinationName = topic;
	mqtt.send(message);

}
async function startMQTTLoop() {
	MQTTconnect();

}

function addRoom() {
	if(roomCounter < 11){
        document.getElementById("room" + roomCounter).classList.remove('hidden');
		publish(roomtopic,"true");
	    roomCounter++;
	}else{
		alert("Only 10 rooms can be created")
	}
}


function execChanges() {
	var stringToBeSent = "";
	for (let i = 1; i < roomCounter; i++) {
        var value = document.getElementById("input" + i + "away").value
		if(value == ""){
			stringToBeSent += "19 " 
		}else{
			stringToBeSent += value + " "
		}
		value = document.getElementById("input" + i + "home").value;
		if(value == ""){
			stringToBeSent += "21 "
		}else{
			stringToBeSent += value + " "
		}
    }
	publish(temptopic,stringToBeSent);
	console.log(stringToBeSent);
}
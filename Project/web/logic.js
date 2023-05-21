var roomCounter = 2;
var mqtt;
var reconnectTimeout=2000;
var host = "localhost"//"192.168.43.142";
var port = 9001;
var roomtopic = "/control/addroom";
var temptopic = "/control/temp";
var gpstopic = "/gps/isHome";
var feedbacktopic= "/room/heatingInfo";
var hometopic = "/control/isHome";
var sensordata = "/sensordata"
var home = false;

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function onConnect(){
	console.log("Connected");
	mqtt.subscribe(sensordata);
	mqtt.subscribe(gpstopic);
	mqtt.subscribe(feedbacktopic);
	message = new Paho.MQTT.Message("Client Connected Sucessfully");
}

function HomeAway(indicator)
{
	console.log(indicator);
	if(indicator == "true")
	{
		document.getElementsByClassName("status home")[0].style.display="block";
		document.getElementsByClassName("status away")[0].style.display="none";
		home = true;
	}
	else{
		document.getElementsByClassName("status home")[0].style.display="none";
		document.getElementsByClassName("status away")[0].style.display="block";
		home = false;
	}
	
}

function MQTTconnect() {
	console.log("connecting to "+ host +" "+ port);
	document.getElementsByClassName("status home")[0].style.display="none";
	document.getElementsByClassName("status away")[0].style.display="none";
	mqtt = new Paho.MQTT.Client(host,port,"localhost");
	var options = {
		timeout: 3,
		onSuccess: onConnect,
		onFailure: onFailure,
	};
	mqtt.onMessageArrived = onMessageArrived;
	mqtt.connect(options); //connect
}

function onFailure(message) {
	console.log("Connection Attempt to Host "+host+"Failed");
	setTimeout(MQTTconnect, reconnectTimeout);
}

function setCurrentTemp(message){
	console.log("Setting current temp");
	for (let i = 1; i < roomCounter; i++) {
		document.getElementById("currentTemp" + i).innerHTML = "current temperature: " + message /*+ what is read from mqtt*/;
	}
}

function setHeatingStatus(message){
	console.log("Setting heating status");
	for (let i = 1; i < roomCounter; i++) {
		if(message[i-1] == 'h'){
		    document.getElementById("heat" + i).innerHTML = "Heating" /*+ what is read from mqtt*/;
		}else{
			document.getElementById("heat" + i).innerHTML = "Cooling" /*+ what is read from mqtt*/;
		}
	}
}

function changeStatus(){
	var str;
	if(home == true){
	    str = "false";
	}else{
		str = "true";
	}
	HomeAway(str);
	publish(hometopic,str);
}

function onMessageArrived(msg){
	var topic = msg.destinationName;
	var out_msg = "Message recieved" +msg.payloadString+ "with topic: " + topic +"<br>";
	console.log(out_msg);
	
	if(topic == gpstopic){
		HomeAway(msg.payloadString)
		publish(hometopic,msg.payloadString);
	}
	else if (topic == sensordata){
		setCurrentTemp(msg.payloadString);
	}
	else if (topic == feedbacktopic){
		setHeatingStatus(msg.payloadString);
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
var roomCounter = 2;
var mqtt;
var reconnectTimeout=2000;
var host = "localhost";
var port = 9001;
var roomtopic = "/addroom";
var temptopic = "/control/temp";
var gpstopic = "/gps/isHome";
var feedbacktopic= "/control/feedback";
var hometopic = "control/isHome";
function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}
function onConnect(){
	console.log("Connected");
	mqtt.subscribe(gpstopic);
	mqtt.subscribe(feedbacktopic);
	mqtt.subscribe(temptopic);
}
function MQTTconnect(){
	console.log("connection to "+host +" " +port);
	var options = {
		timeout:1,
		onSuccess:onConnect,
		onFailure:onFailure,
	};
	mqtt= new Paho.MQTT.Client(host,port,"controlapp");
	mqtt.onMessageArrived = onMessageArrived;
	mqtt.connect(options);
}
function setCurrentTemp(){
	console.log("Setting current temp");
}
function onMessageArrived(msg){
	topic = msg.destinationName;
	msg_out = "Message recieved" +msg.payloadString+ "with topic: " + topic +"<br>";
	console.log(out_msg);
	if(topic == gpstopic){
		publish(hometopic,"true");
	}
	else if (topic == feedbacktopic){
		setCurrentTemp();
	}


}
function publish(topic,message){
	message = new Paho.MQTT.Message(message);
	message.destinationName = topic;
	mqtt.send(message);

}
async function startMQTTLoop() {
	MQTTconnect();
  while(1){
	  //implement MQTT here
	  for (let i = 1; i < roomCounter; i++) {
          document.getElementById("currentTemp" + i).innerHTML = "currentTemp: (changed)" /*+ what is read from mqtt*/;
      }
      await sleep(2000); //this is just example for sleep to awoid flooding you won't need it
      //console.log("Goodbye!");
  }
}
function onFailure(){
	console.log("Attempting to connect:" +host + "Failed");
	setTimeout(MQTTconnect,reconnectTimeout);
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
from sense_hat import SenseHat
from datetime import datetime
import paho.mqtt.client as mqttClient
import time
broker_adress="localhost"
port=2023
user = "rpi"
password = "asd"

sense = SenseHat()

def get_sense_data():
    sense_data = []
    # Get environmental data
    sense_data.append(sense.get_temperature())
    sense_data.append(sense.get_pressure())
    sense_data.append(sense.get_humidity())
    # Get the date and time
    sense_data.append(datetime.now())
    return sense_data

def on_publish(client,userdata,result):             #create function for callback
    print("data published \n")

def on_connect(client, userdata, flags, rc):
 
    if rc == 0:
 
        print("Connected to broker")
 
        global Connected                #Use global variable
        Connected = True                #Signal connection 
 
    else:
 
        print("Connection failed")
    

client = mqttClient.Client("rpi")
client.username_pw_set(user, password)    #set username and password
client.on_connect= on_connect                      #attach function to callback
Connected = client.connect(broker_adress, port=port)          #connect to broker
 
client.loop_start()        #start the loop
 
while Connected != True:    #Wait for connection
    time.sleep(0.1)
 
try:
    while True:
 
        #value = get_sense_data()
        client.publish("/test/a/a","asd")
 
except KeyboardInterrupt:
 
    client.disconnect()
    client.loop_stop()

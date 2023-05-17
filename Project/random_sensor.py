
from datetime import datetime
import paho.mqtt.client as mqttClient
import time
import random
broker_adress="localhost"
port=2023
user = "rpi"
password = "asd"


def generate():
    sense_data = []
    # Get environmental data
    sense_data.append(random.uniform(10.5, 75.5))
    sense_data.append(' ')
    sense_data.append(float(random.uniform(1000, 1100)))
    sense_data.append(' ')
    # Get the date and times
    sense_data.append(datetime.now())
    list = ''.join(map(str,sense_data))
    print(list)
    return list

def on_publish(client,userdata,result):             #create function for callback
    print("data published \n")

def on_connect(client, userdata, flags, rc):
 
    if rc == 0:
 
        print("Connected to broker")
 
        global Connected                #Use global variable
        Connected = True                #Signal connection 
 
    else:
 
        print("Connection failed")
    

client = mqttClient.Client("sensor")
client.username_pw_set(user, password)    #set username and password
client.on_connect= on_connect                      #attach function to callback
Connected = client.connect(broker_adress, port=port)          #connect to broker
 
client.loop_start()        #start the loop
 
while Connected != True:    #Wait for connection
    time.sleep(0.1)
 
try:
    while True:
 
        value = generate()
        time.sleep(3)
        client.publish("/sensordata",(value))
        #client.publish("/addroom",True)
        #client.publish("/control/temp",generate())
 
except KeyboardInterrupt:
 
    client.disconnect()
    client.loop_stop()
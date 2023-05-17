from datetime import datetime
import paho.mqtt.client as mqttClient
import time
broker_adress="localhost"
port=2023
user = "rpi"
password = "asd"

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
        text = """Write input: [a] for add room
                               [t] for changing the tempeture
                               [h] for indicationg that someone is arriving home"""
        print(text)
        value = input()
        if value == 'a':
            client.publish("/addroom",True)
        elif value == 't':
            value = input()
            client.publish("/control/temp",value)
        elif value == 'h':
            value = input()
            client.publish("control/isHome",value)

except KeyboardInterrupt:
    client.disconnect()
    client.loop_stop()

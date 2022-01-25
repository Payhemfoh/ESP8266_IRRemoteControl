<h1>ESP8266_IRRemoteController</h1>

This is an IR remote controller built by ESP8266 nodeMCU (aims to control smart tv however can be modify to control others such as AC)

Hardware:
ESP8266
IR Transmitter
NPN Transistor (2N3904)
IR Receiver Diode VS 1838B

Software:
nodeMCU_IRreceiver :- use IR Receiver Diode to read the IR signal from remote controller
nodeMCU_IRsender :- use IR Transmitter to send the signal retrieved from IRreceiver (included the web server which allow the devices in same network to send IR signal through webpage)

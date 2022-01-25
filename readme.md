<h1>ESP8266_IRRemoteController</h1>

This is an IR remote controller built by nodeMCU ESP8266 (aims to control smart tv however can be modify to control others such as AC)

<h1>Hardware:</h1>
<ol>
  <li>nodeMCU ESP8266 : https://my.cytron.io/p-nodemcu-lua-v3-esp8266-wifi-with-ch340c?search=nodemcu&description=1</li>
  <li>IR Transmitter : https://my.cytron.io/p-ir-tansmitter?search=IR&description=1</li>
  <li>NPN Transistor (2N3904) : https://my.cytron.io/p-transistor-2n3904?search=transistor&description=1</li>
  <li>IR Receiver Diode VS 1838B : https://my.cytron.io/p-ir-receiver-diode-vs1838b-38khz?search=IR&description=1</li>
<ol>

<h1>Software:</h1>
<ul>
<li>nodeMCU_IRreceiver :- use IR Receiver Diode to read the IR signal from remote controller</li>
<li>nodeMCU_IRsender :- use IR Transmitter to send the signal retrieved from IRreceiver (included the web server which allow the devices in same network to send IR signal through webpage)</li>
</ul>

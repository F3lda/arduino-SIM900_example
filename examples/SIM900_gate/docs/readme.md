# Arduino SIM900 GPRS GSM Shield example - gate controller
<img src="1.jpg" alt="GSM Shield" width="50%" height="50%">


## Setup
<ol>
  <li>Turn off the PIN lock on the SIM card (with a mobile phone)</li>
  <li>Choose TX and RX connections to the Arduino (I use software serial by placing the jumper caps on the left side of the serial selector. TX = D7, RX = D8)
    <br><img src="2.jpg" alt="Serial selector" width="50%" height="50%">
  </li>
  <li>Solder R13 connections on the shield together (now you can automatically turn on/off the shield using arduino - GSM shield pin D9)
    <br><img src="3.jpg" alt="R13" width="50%" height="50%">
  </li>
  <li>Select the external power source with the toggle switch next to the DC jack (inner side - External power source/outter side - Power from arduino shield)
    <br><img src="4.jpg" alt="External power" width="50%" height="50%">
  </li>
  <li>Insert the SIM card into the SIM card holder</li>
  <li>Put GSM shield on the Arduino
    <br><img src="5.jpg" alt="GSM shield on the arduino" width="50%" height="50%">
  </li>
  <li>Modify your Gate controller (replace the buttons with wires from relays)
    <br><img src="7.jpg" alt="Gate controller" width="50%" height="50%">
    <br>Switch should be like this:
    <br><img src="6.jpg" alt="Gate controller modified" width="50%" height="50%">
  </li>
  <li>Powering
    <br>I had some problems with low power so I end up with 5V to arduino shield and 12V to GSM shield and a 2A usb charger.
    <br><img src="8.jpg" alt="Powering" width="50%" height="50%">
    <br><img src="9.jpg" alt="Powering" width="50%" height="50%">
    <br>I also replaced original antena with one from 433MHz kit.
    <br><img src="10.jpg" alt="Powering" width="20%" height="20%">
    <br>And added two capacitors (330uF and 100nF).
    <br><img src="11.jpg" alt="Powering" width="20%" height="20%">
  </li>
  <li>Download the code, edit and run</li>
</ol>

## Links:
- https://randomnerdtutorials.com/sim900-gsm-gprs-shield-arduino/
- https://electronics.stackexchange.com/questions/123240/powering-sim-900-gsm
- https://industruino.com/page/wdt
- https://electronics.stackexchange.com/questions/102293/i-need-to-replace-this-button-in-this-circuit-board-with-a-relay

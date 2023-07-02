/**
 * @file SIM900_loop.ino
 *
 * @brief Arduino with SIM900 shield example with basic loop
 * @date 2019-06-23
 * @author F3lda
 * @update 2023-06-23
 */

/*********************************************************************
*
* AT COMMANDS - source: https://m2msupport.net/m2msupport/at-command/
*
**********************************************************************
SETUP SETTINGS
--------------
AT+CFUN=1                  set GSM full function
AT+CMGF=1                  set SMS format to TEXT mode (set message mode to ASCII)

AT+CNMI=2,2,0,0,0          turn on SMS Message notification (send the SMS text directly to the serial output) - SMS length max 160 -> https://stackoverflow.com/a/28029200
AT+CNMI=2,1,0,0,0          turn on SMS Message notification (send notification only about new SMS - no SMS text) -> https://stackoverflow.com/a/27650820
AT+CLIP=1                  turn on Caller ID notification

AT+CMGL=\"ALL\"            show all SMS messages
AT+CMGDA=\"DEL ALL\"       remove all SMS messages



DATETIME
--------
1) Enable auto network time sync and save config save the setting to EEPROM memory of SIM900.
2) Restart the GSM module and check datetime.
AT+CLTS=1;&W (AT&W)       enable datetime (need reset SIM900 Shield)
AT+CCLK?                  show datetime
3) check Auto Network Time Sync value: (gives an error but datetime works)
AT+CLTS?



GENERAL INFO
------------
ATS3?                      get Command Line Termination Character
AT+CPIN?                   sim is ready (returns "+CPIN: READY" and "OK")
AT+CREG?                   network is registered (returns "+CREG: 0,1" and "OK")
AT+CPAS                    GSM module status
  Status result codes:
    0: ready
    2: unknown
    3: ringing
    4: call in progress
AT+CMGF?                   check SMS format mode
AT+CSQ                     get signal strength
   x,99 => not known or not detectable
  Value  RSSI dBm  Condition
    2    -109     Marginal
    3    -107     Marginal
    4    -105     Marginal
    5    -103     Marginal
    6    -101     Marginal
    7    -99      Marginal
    8    -97      Marginal
    9    -95      Marginal
    10   -93      OK
    11   -91      OK
    12   -89      OK
    13   -87      OK
    14   -85      OK
    15   -83      Good
    16   -81      Good
    17   -79      Good
    18   -77      Good
    19   -75      Good
    20   -73      Excellent
    21   -71      Excellent
    22   -69      Excellent
    23   -67      Excellent
    24   -65      Excellent
    25   -63      Excellent
    26   -61      Excellent
    27   -59      Excellent
    28   -57      Excellent
    29   -55      Excellent
    30   -53      Excellent



CALL and SMS
------------
ATH                        hangup call
ATD<number>;               dial number (ATD*125*#; - credit status (get from your provider))

AT+CMGS=\"<number>\"\r     send SMS - returns OK or ERROR
[wait for ">"]             (AT+CMGS="<number>"<CR><message><ASCII-CHAR-26>) - Concatenated SMS - https://en.wikipedia.org/wiki/Concatenated_SMS
<message><ASCII-CHAR-26>

AT+CMSS=<index>,"<number>" send SMS stored on the INDEX to the NUMBER
AT+CMGR=<index>            read SMS stored on the INDEX
AT+CMGD=<index>            delete SMS stored on the INDEX
AT+CMGD=?                  get size of the SMS storage
**********************************************************************/



// Use Arduino USB serial for sending AT commands



// SIM900 GSM Shield example
//---------------------------
// libraries
#include <SoftwareSerial.h>


// SIM900
#define SIM900_SPEED 9600
#define SIM900_PIN_RX 7
#define SIM900_PIN_TX 8
#define SIM900_PIN_POWER 9
SoftwareSerial SIM900 = SoftwareSerial(SIM900_PIN_RX, SIM900_PIN_TX); // RX, TX

void setup()
{
    // USB serial
    Serial.begin(9600);               // Arduino USB serial baud rate
    Serial.print("\r");               // Print carriage return (return to left margin) and...
    Serial.flush();                   // ...wait until it's written (wait for serial port to connect)


    // SIM900 setup pins
    pinMode(SIM900_PIN_RX, INPUT);
    pinMode(SIM900_PIN_TX, OUTPUT);
    pinMode(SIM900_PIN_POWER, OUTPUT);
    digitalWrite(SIM900_PIN_POWER, LOW);


    // SIM900 serial
    SIM900.begin(19200);              // SIM900 default baud rate (DEFAULT: 19200)

    SIM900.print("AT+IPR=");          // Tell the SIM900 not to autobaud
    SIM900.print(SIM900_SPEED);       // -> lower baudrate could be less power consuming
    SIM900.print("\r");               // Send
    SIM900.flush();                   // wait for transmission

    SIM900.end();                     // disconnect SIM900 serial

    SIM900.begin(SIM900_SPEED);       // begin SIM900 serial with the new baud rate
    SIM900.readStringUntil('\n');     // clear response from old baud rate from SIM900 serial


    // SIM900 AT handshake
    SIM900.print("AT\r");             // if everything works, should return OK on USB serial (else SIM900 shield might be powered OFF)
    Serial.println("Arduino ready! Waiting for SIM900 to response:");
    Serial.println("(check if SIM900 shield is ON - type POWER in command line to turn shield ON/OFF):");
}

void loop()
{
    // Read data from SIM900 serial
    if (SIM900.available() > 0){
        String output = SIM900.readStringUntil('\r'); // EOL = "\r\n"
        SIM900.readStringUntil('\n'); // clear the remaining \r(s) and \n
        Serial.println("["+output+"]("+output.length()+")");
    }
    // EOL char test
    /*if (SIM900.available() > 0){
        String str = "";
        while (SIM900.available() > 0){
            char incomingByte = (char)SIM900.read();
            if (incomingByte == 10) {
                str += " N ";
            }
            if (incomingByte == 13) {
                str += " R ";
            }
            str += incomingByte;
        }
        Serial.println("["+str+"]("+str.length()+")");
    }*/

    // Read data from USB serial
    if (Serial.available() > 0){
        String input = Serial.readStringUntil('\n');
        Serial.println("<"+input+">("+input.length()+")");
        if(input == "POWER"){
            sim900powerUpDown(SIM900_PIN_POWER);
        } else if(input == "SEND") {
            SIM900.print((char)(26));
        } else {
            SIM900.print(input);
            SIM900.print("\r");
        }
    }
    delay(33); // short delay
}

void sim900powerUpDown(int pin)
{
    // power pulse for SIM900 Shield (pin 9)
    digitalWrite(pin, LOW);
    delay(1000);
    digitalWrite(pin, HIGH);
    delay(2000);
    digitalWrite(pin, LOW);
    delay(3000);
}

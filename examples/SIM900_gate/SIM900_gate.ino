/**
 * @file SIM900_gate.ino
 *
 * @brief Arduino with SIM900 shield example used to open gates
 * @date 2019-06-23
 * @author F3lda
 * @update 2023-07-04
 */

// SIM900 GSM Shield example
//---------------------------
// libraries
#include <SoftwareSerial.h>
#include <MemoryFree.h>
#include <avr/wdt.h>
#include "telnumswhitelist.h"

// General
#define SIM900_STRING_MAX_LENGTH 164 // 164 -> max SMS size = 160 chars (153 <message> + 7 <joining data> OR 160 <only one message> --- https://stackoverflow.com/a/28029200)
#define SIM900_RESPONSE_TIMEOUT_DEFAULT 15000 // 15 seconds
#define SMS_QUEUE_SIZE 10 // AT+CMGD=? -> get size of the SMS storage
#define SIM_CREDIT_STATUS_CMD "ATD*125*#;" // Dial *125*#

// SIM900
#define SIM900_SPEED 9600
#define SIM900_PIN_RX 7
#define SIM900_PIN_TX 8
#define SIM900_PIN_POWER 9
SoftwareSerial SIM900 = SoftwareSerial(SIM900_PIN_RX, SIM900_PIN_TX); // RX, TX

// SMS queue
int SMS_QUEUE_ARRAY[SMS_QUEUE_SIZE] = {0};
int queue_rear = -1;
int queue_front = -1;

// Relays
#define RELAY_TIMEOUT 30000 // = 1000*30 (30 secs)
#define pinGate 2
#define pinDoor 3
#define pinController 4

/************************************************ FUNCTIONS ****************************************************/

void sim900loop();
void handleSIM900message(bool isEOLread, char *sim900outputData);
bool checkWhitelist(char *telNumber);
void strtoupper(char *str);
void trim(char * str);
void brana();
void vrata();

/************************************************ CUSTOM FUNCTIONS ****************************************************/

void sim900powerUpDown(int pin);
bool sim900init();

/************************************************ BASIC FUNCTIONS ****************************************************/

void SIM900prepareNewSMS(const char *telNumber);
void SIM900addSMSdata(const char *message);
void SIM900sendPreparedSMS();
void SIM900sendSMS(const char *telNumber, const char *message);
bool SIM900readLine(char *outputData, int bufferLength);
void SIM900sendCmd(const char *cmd);
typedef void (*handleSIM900messageFuncPtr)(bool, char*); // create a type to point to a funciton
bool SIM900waitForResponse(const char *expectedResponse, unsigned int timeoutMS, int responseBufferLength, handleSIM900messageFuncPtr callbackFunctionForOtherResponses);
bool SIM900checkWithCmd(const char *cmd, const char *expectedResponse, handleSIM900messageFuncPtr callbackFunctionForOtherResponses);
bool SIM900waitForSerialDataAvailable(unsigned int timeoutMS);
void SIM900flushSerial();

/************************************************ FUNCTIONS - END ****************************************************/


void setup()
{
    // USB serial
    Serial.begin(9600);               // Arduino USB serial baud rate
    Serial.print(F("\r"));            // Print carriage return (return to left margin) and...
    Serial.flush();                   // ...wait until it's written (wait for serial port to connect)


    // SIM900 setup pins
    pinMode(SIM900_PIN_RX, INPUT);
    pinMode(SIM900_PIN_TX, OUTPUT);
    pinMode(SIM900_PIN_POWER, OUTPUT);
    digitalWrite(SIM900_PIN_POWER, LOW);
    // relay pins
    pinMode(pinGate, OUTPUT);
    digitalWrite(pinGate, HIGH);
    pinMode(pinDoor, OUTPUT);
    digitalWrite(pinDoor, HIGH);
    pinMode(pinController, OUTPUT);
    digitalWrite(pinController, HIGH);


    Serial.println(F("Arduino is ready!"));
    sim900init(); // init SIM900
}


void loop()
{
    // SIM900 receiving loop
    sim900loop();

    // receive USB serial data
    if (Serial.available() > 0){
        char serialChars[SIM900_STRING_MAX_LENGTH] = {0};
        int charsRead = Serial.readBytesUntil('\n', serialChars, sizeof(serialChars)-1);
        Serial.print(F("<"));
        Serial.print(serialChars);
        Serial.print(F(">"));
        Serial.print(F("("));
        Serial.print(charsRead);
        Serial.println(F(")"));
        if(strcmp(serialChars,"POWER") == 0){
            sim900powerUpDown(SIM900_PIN_POWER);
        } else if(strcmp(serialChars,"SEND") == 0) { // send SMS
            SIM900.print((char)(26));
        } else if(strcmp(serialChars,"STATUS") == 0) {
            sim900checkStatus();
        } else {
            SIM900sendCmd(serialChars);
        }
    }

    // short delay
    delay(33);
}


/************************************************ FUNCTIONS ****************************************************/

void sim900loop()
{
    // receive SIM900 serial data
    if (SIM900.available() > 0){
        char sim900outputData[SIM900_STRING_MAX_LENGTH] = {0};
        bool isEOLread = SIM900readLine(sim900outputData, sizeof(sim900outputData)-1);
        handleSIM900message(isEOLread, sim900outputData);
    }

    // check if GSM module is OK
    static unsigned int handshakeSIM900counter = 0;
    if(handshakeSIM900counter > 30*60*15){ // handshake with SIM900 Shield about every 15 minutes
        handshakeSIM900counter = 0;

        // check time to restart
        SIM900sendCmd("AT+CCLK?");

    } else {
        handshakeSIM900counter++;
    }
}

void handleSIM900message(bool isEOLread, char *sim900outputData)
{
    // skip empty lines
    if(sim900outputData[0] != '\0'){
        Serial.print(F("["));
        Serial.print(sim900outputData);
        Serial.print(F("]"));
        Serial.print(F("<"));
        Serial.print(isEOLread);
        Serial.print(F(">"));
        Serial.print(F("("));
        Serial.print(strlen(sim900outputData));
        Serial.println(F(")"));

        static int RINGcount = 0;
        static bool SMShandling = false;

        static bool CMD_CREDIT = false;
        static bool CMD_SIGNAL = false;


        // the datetime received (check RESET)
        if(strstr(sim900outputData, "+CCLK:") == sim900outputData){
            SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
            static char lastResetDay[3] = "00"; // reset arduino every day (when day in date changes)

            Serial.print(F("Last DAY: "));
            Serial.println(lastResetDay);

            char *datetime = strstr(sim900outputData, "\"");
            if((datetime != NULL) && !(lastResetDay[0] == '0' && lastResetDay[1] == '0') && (lastResetDay[0] != datetime[7] || lastResetDay[1] != datetime[8])){

                // TIME TO RESET - Resetart ALL devices
                Serial.println(F("Time to RESET Arduino!"));
                Serial.println(F("Turning OFF Gate remote controller..."));
                digitalWrite(pinController, LOW);

                Serial.println(F("Turning OFF GSM shield..."));
                // SIM900 check power
                SIM900sendCmd("AT");
                delay(1500);
                if(SIM900.available()){
                    // power OFF
                    sim900powerUpDown(SIM900_PIN_POWER);
                    delay(1500);
                }
                delay(3500);
                Serial.println(F("Done."));

                Serial.println(F("Restarting Arduino..."));
                // wait for watchdog timer
                wdt_enable(WDTO_4S); // 3 seconds - wdt_reset();
                delay(10000);

            } else {
                Serial.println(F("Not yet."));

                if(sim900checkStatus() != 0) {
                    // Restart GSM module
                    Serial.println(F("Restarting GSM module..."));
                    sim900powerUpDown(SIM900_PIN_POWER);
                    SIM900waitForResponse("NORMAL POWER DOWN", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
                    delay(5000);
                    sim900init();
                }
            }

            if(datetime != NULL){
                lastResetDay[0] = datetime[7];
                lastResetDay[1] = datetime[8];
            }

            Serial.print(F("New DAY: "));
            Serial.println(lastResetDay);
            SIM900flushSerial();

        // incomming call - RING
        } else if(strcmp(sim900outputData, "RING") == 0){
            RINGcount++;

        // incomming call - caller ID
        } else if(strstr(sim900outputData, "+CLIP:") == sim900outputData){
            char *telnumber = strstr(sim900outputData, "\"")+1; // TODO check NULL
            char CALLERid[20] = {0};
            memset(CALLERid, '\0', sizeof(CALLERid));
            memcpy(CALLERid, telnumber, 13);// OR until the " char

            Serial.print(F("CALLER ID: "));
            Serial.println(CALLERid);
            Serial.print(F("RING COUNT: "));
            Serial.println(RINGcount);

            if(!checkWhitelist(CALLERid)){
                Serial.println(F("NOT on Whitelist!"));

                // hangup
                SIM900sendCmd("ATH");
                SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
                RINGcount = 0;

                Serial.println(F("Send SMS..."));
                snprintf(sim900outputData, SIM900_STRING_MAX_LENGTH, "Unknown caller: %s", CALLERid); // recycling sim900outputData variable
                SIM900sendSMS(ADMIN_TEL_NUMBER, sim900outputData);
                Serial.println(F("Done."));


            } else if(RINGcount == 4){// 4 => 3rd ring
                Serial.println(F("ON Whitelist!"));

                // hangup
                SIM900sendCmd("ATH");
                SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
                RINGcount = 0;

                // do something
                brana();
            }

        // incomming call - finished by caller
        } else if(strcmp(sim900outputData, "NO CARRIER") == 0){
            RINGcount = 0;

        // SMS message received
        } else if(strstr(sim900outputData, "+CMTI:") == sim900outputData){
            Serial.println(F("New SMS received!"));

            // get new sms index
            char *sms_index = strstr(sim900outputData, ",")+1;
            int new_sms_index = atoi(sms_index);
            if(new_sms_index != 0) {
                queueInsert(new_sms_index);
            }
            Serial.println(sms_index);


            // if not already handling other sms -> start with the current new one
            if (!SMShandling) {
                Serial.println(F("HANDLING TRUE!"));
                SMShandling = true;

                char tempStr[20] = {0};
                snprintf(tempStr, sizeof(tempStr), "AT+CMGR=%d", new_sms_index);
                SIM900sendCmd(tempStr);
            }

            Serial.println(F("New SMS received - done"));

        // SMS message read and send
        } else if(strstr(sim900outputData, "+CMGR:") == sim900outputData){

            static bool checkingSMSqueue = false;
            // when handling SMS and not checking for new received SMS
            if (SMShandling && !checkingSMSqueue) {

                Serial.println(F("READING SMS!"));

                // get SMS sender tel number
                char *telnumber = strstr(sim900outputData, ",\"")+2; // TODO check NULL
                char SMSid[20] = {0};
                memset(SMSid, '\0', sizeof(SMSid));
                strstr(telnumber, "\",\"")[0] = '\0';
                snprintf(SMSid, sizeof(SMSid), "%s", telnumber);

                // get SMS text
                char smsMessage[SIM900_STRING_MAX_LENGTH] = {0};
                SIM900readLine(smsMessage, SIM900_STRING_MAX_LENGTH);
                int smsMessageLength = strlen(smsMessage);

                Serial.print(F("SMS ID: "));
                Serial.println(SMSid);
                Serial.print(F("SMS text: "));
                Serial.print(smsMessage);
                Serial.print(F(" ("));
                Serial.print(smsMessageLength);
                Serial.println(F(")"));

                SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
                Serial.println(F("Reading SMS Done."));





                // recycling sim900outputData variable
                char *smsCmd = sim900outputData;
                smsCmd[0] = '\0';


                static char LastSMSid[sizeof(SMSid)] = {0};
                static bool SenderNumberSent = false;
                // last message - wait 30 seconds if there is a next SMS (only if current SMS is 153 chars long)
                // done TODO WARNING: "+CMTI:" is read but not handled! ---> function SIM900waitForResponse() changed, so now its working
                // done TODO solve the problem with 153 char long SMS!!! (save last SMS id and last SMS type (sender number sent whole OR just partly) - if IDs are not equal and SMS type is just part number --> send last SMS ID)
                if (smsMessageLength != 153 || (queuePeek() == 1 && !SIM900waitForResponse("+CMTI:", 30000, SIM900_STRING_MAX_LENGTH, handleSIM900message))) {

                    bool unknown_message = false; // if message is uknown -> send it to admin and add the whole sender number

                    if (checkWhitelist(SMSid)) {
                        Serial.println(F("ON Whitelist!"));

                        // copy, trim and uppercase SMS message
                        memcpy(smsCmd, smsMessage, SIM900_STRING_MAX_LENGTH);
                        strtoupper(smsCmd);
                        trim(smsCmd);
                        Serial.print(F("<"));
                        Serial.print(smsCmd);
                        Serial.println(F(">"));

                        // do CMD
                        if(strcmp(smsCmd,"BRANA") == 0) {
                            // open the GATE
                            brana();
                        } else if(strcmp(smsCmd,"VRATA") == 0) {
                            // open the DOOR
                            vrata();
                        } else if(strcmp(smsCmd,"CMD KREDIT") == 0) {
                            // get SIM credit status
                            CMD_CREDIT = true;
                        } else if(strcmp(smsCmd,"CMD SIGNAL") == 0) {
                            // get SIM signal status
                            CMD_SIGNAL = true;
                        } else {
                            unknown_message = true;
                        }

                    } else {
                        unknown_message = true;
                    }


                    if (unknown_message) {
                        // done TODO solve the problem with 153 char long SMS!!! (save last SMS id and last SMS type (sender number sent whole OR just partly) - if IDs are not equal and SMS type is just part number --> send last SMS ID)
                        if (LastSMSid[0] != '\0' && strcmp(LastSMSid, SMSid) != 0 && SenderNumberSent == false) {
                            Serial.println(F("Send previous SMS sender number..."));
                            SIM900sendSMS(ADMIN_TEL_NUMBER, LastSMSid);
                            Serial.println(F("Done."));
                        }

                        // send SMS message and SMS sender number separately
                        if (strlen(SMSid)+smsMessageLength > 157) { // max SMS length is 160 minus " - " (3 chars)

                            Serial.println(F("Send SMS message..."));
                            SIM900sendSMS(ADMIN_TEL_NUMBER, smsMessage);
                            Serial.println(F("Done."));


                            Serial.println(F("Send SMS sender number..."));
                            SIM900sendSMS(ADMIN_TEL_NUMBER, SMSid);
                            Serial.println(F("Done."));

                        // send SMS message and SMS sender number together
                        } else {
                            strcat(smsMessage, " - ");
                            strcat(smsMessage, SMSid);

                            Serial.println(F("Send SMS message with sender number..."));
                            SIM900sendSMS(ADMIN_TEL_NUMBER, smsMessage);
                            Serial.println(F("Done."));
                        }
                    }

                    SenderNumberSent = true; // previous SMS sender number sent OR command used

                // initial (153 chars long) and NON WHITELIST messages
                } else {
                    SenderNumberSent = false;

                    // unknown messages - send them to admin
                    // add a few digits (6) from sender number
                    int senderIDlength = strlen(SMSid);
                    smsMessage[153] = '-';
                    smsMessage[154] = SMSid[senderIDlength-6];
                    smsMessage[155] = SMSid[senderIDlength-5];
                    smsMessage[156] = SMSid[senderIDlength-4];
                    smsMessage[157] = SMSid[senderIDlength-3];
                    smsMessage[158] = SMSid[senderIDlength-2];
                    smsMessage[159] = SMSid[senderIDlength-1];

                    Serial.println(F("Send SMS message with part of the sender number..."));
                    SIM900sendSMS(ADMIN_TEL_NUMBER, smsMessage);
                    Serial.println(F("Done."));
                }

                strcpy(LastSMSid, SMSid); // copy SMS sender number to use it next time





                // remove read SMS from queue
                int sms_in_queue = queueRemove();
                if (sms_in_queue != 0) {
                    char tempStr[20] = {0};
                    sprintf(tempStr, "AT+CMGD=%d", sms_in_queue);
                    Serial.println(tempStr);

                    SIM900sendCmd(tempStr);

                    SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
                    Serial.println();
                    Serial.println(F("Remove Done."));
                }


                // if in queue is SMS read next SMS in queue
                sms_in_queue = queuePeek();
                if (sms_in_queue != 0) {
                    char tempStr[20] = {0};
                    Serial.println(F("NEXT SMS:"));
                    snprintf(tempStr, sizeof(tempStr), "AT+CMGR=%d", sms_in_queue);

                    // short delay before next message (wait for random string)
                    SIM900waitForResponse(" <---> ", 5000, SIM900_STRING_MAX_LENGTH, handleSIM900message);

                    // read next message
                    SIM900sendCmd(tempStr);
                    Serial.println();
                } else {
                    // check SIM module for any SMS panding
                    if (!checkingSMSqueue) {
                        Serial.println(F("CHECKING TRUE!"));
                        checkingSMSqueue = true;
                        SMShandling = false;


                        SIM900sendCmd("AT+CMGL=\"ALL\"");
                        if (SIM900waitForResponse("+CMGL:", 1500, SIM900_STRING_MAX_LENGTH, handleSIM900message)) {
                            SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);//clean

                            int i;
                            for (i = 1; i <= SMS_QUEUE_SIZE; i++) {

                                char tempStr[20] = {0};
                                sprintf(tempStr, "AT+CMGR=%d", i);

                                SIM900sendCmd(tempStr);
                                if (SIM900waitForResponse("+CMGR:", 1500, SIM900_STRING_MAX_LENGTH, handleSIM900message)) {
                                    SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);//clean

                                    Serial.println(F("isInQueue!"));
                                    if (!isInQueue(i)) {
                                        Serial.println(F("Inserts!"));
                                        queueInsert(i);
                                    }

                                }

                            }
                        }


                        checkingSMSqueue = false;
                        SMShandling = true;
                        Serial.println(F("CHECKING DONE!"));
                    }



                    Serial.println(F("HANDLING FALSE!"));
                    SMShandling = false;
                }



                // do COMMANDS
                if(smsCmd[0] != '\0') {
                    if (CMD_CREDIT == true && strcmp(smsCmd,"CMD KREDIT") == 0) {
                        // get SIM credit status
                        SIM900sendCmd(SIM_CREDIT_STATUS_CMD);
                    } else if(CMD_SIGNAL == true && strcmp(smsCmd,"CMD SIGNAL") == 0) {
                        // get SIM signal status
                        SIM900sendCmd("AT+CSQ");
                    }
                }
            }

        // USSD message received
        } else if(strstr(sim900outputData, "+CUSD:") == sim900outputData){
            char *message = strstr(sim900outputData, "\"")+1;
            strstr(message, "\"")[0] = '\0'; // set message end

            if(CMD_CREDIT){ // send SMS only if command was called by SMS
                SIM900sendSMS(ADMIN_TEL_NUMBER, message);
                CMD_CREDIT = false;
            }

        // Signal status received
        } else if(strstr(sim900outputData, "+CSQ:") == sim900outputData){
            int signal_value = 0;
            int signal_status = 0;
            char message[SIM900_STRING_MAX_LENGTH];
            sscanf(sim900outputData,"+CSQ: %d,%d",&signal_value,&signal_status);
            if(signal_status != 99){
                if(signal_value < 10){
                    sprintf(message,"Response: %s\nSignal Status: %d - Marginal",sim900outputData,signal_value);
                } else if(signal_value >= 10 && signal_value <= 14){
                    sprintf(message,"Response: %s\nSignal Status: %d - OK",sim900outputData,signal_value);
                } else if(signal_value >= 15 && signal_value <= 19){
                    sprintf(message,"Response: %s\nSignal Status: %d - Good",sim900outputData,signal_value);
                } else if(signal_value > 19){
                    sprintf(message,"Response: %s\nSignal Status: %d - Excellent",sim900outputData,signal_value);
                }
            } else {
                sprintf(message,"Response: %s\nSignal Status: Error",sim900outputData);
            }
            Serial.println(message);
            if(CMD_SIGNAL){ // send SMS only if command was called by SMS
                SIM900sendSMS(ADMIN_TEL_NUMBER, message);
                CMD_SIGNAL = false;
            }
        }
    }
}

bool checkWhitelist(char *telNumber)
{
    for (int i = 0; i < (int)(sizeof(whitelist) / sizeof(whitelist[0])); i++) {
        char *whitelist_ptr = (char *)pgm_read_word(&whitelist[i]);
        char whitelist_buffer[16]; // must be large enough!
        strcpy_P(whitelist_buffer, whitelist_ptr);
        if(strcmp(telNumber, whitelist_buffer) == 0){
            return true;
        }
    }
    return false;
}

int queueInsert(int value)
{
    if (queue_rear == SMS_QUEUE_SIZE - 1) {
        return 0;
    } else {
        if (queue_front == -1) {
            queue_front = 0;
        }
        SMS_QUEUE_ARRAY[++queue_rear] = value;
        return value;
    }
}

int queuePeek()
{
    if (queue_front == -1 || queue_front > queue_rear) {
        return 0;
    } else {
        return SMS_QUEUE_ARRAY[queue_front];
    }
}

int queueRemove()
{
    if (queue_front == -1 || queue_front > queue_rear) {
        return 0;
    } else {
        return SMS_QUEUE_ARRAY[queue_front++];
    }
}

int isInQueue(int value)
{
    if (queue_front != - 1){
        int i;
        for (i = queue_front; i <= queue_rear; i++) {
            if (SMS_QUEUE_ARRAY[i] == value) {
                return 1;
            }
        }
    }
    return 0;
}

void strtoupper(char * str)
{
    while(str && *str) {*(str++) = toupper((unsigned char) *str);}
}

void trim(char * str)
{
    char * front = str-1;
    while (isspace(*++front));
    char * back = front+strlen(front);
    if (front[0] != 0) {
        while (isspace(*--back));
        *(++back) = '\0';
    }
    if (front != str) {memcpy(str, front, back-front+1);}
}

void brana()
{
    static unsigned long relay_pause_time = millis()-RELAY_TIMEOUT-1;
    if(millis()-relay_pause_time > RELAY_TIMEOUT){
        Serial.println(F("BRANA"));
        digitalWrite(pinGate, LOW);
        delay(1500);
        digitalWrite(pinGate, HIGH);
        relay_pause_time = millis();
    }
}

void vrata()
{
    static unsigned long relay_pause_time = millis()-RELAY_TIMEOUT-1;
    if(millis()-relay_pause_time > RELAY_TIMEOUT){
        Serial.println(F("VRATA"));
        digitalWrite(pinDoor, LOW);
        delay(1500);
        digitalWrite(pinDoor, HIGH);
        relay_pause_time = millis();
    }
}


/************************************************ CUSTOM FUNCTIONS ****************************************************/

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

bool sim900init()
{
    Serial.println(F("GSM shield initialization..."));

    // SIM900 serial
    SIM900.begin(19200);              // SIM900 default baud rate (DEFAULT: 19200)

    SIM900.print("AT+IPR=");          // Tell the SIM900 not to autobaud
    SIM900.print(SIM900_SPEED);       // -> lower baudrate could be less power consuming
    SIM900.print("\r");               // send
    SIM900.flush();                   // wait for transmission (and in some versions of the SoftwareSerial.h removes any buffered incoming serial data ---> if not working as expected, remove this line)

    SIM900.end();                     // disconnect SIM900 serial

    SIM900.begin(SIM900_SPEED);       // begin SIM900 serial with the new baud rate
    SIM900.readStringUntil('\n');     // clear response from old baud rate from SIM900 serial
    SIM900flushSerial();
    SIM900.setTimeout(3000);          // Read serial timeout


    // SIM900 check power
    if(!SIM900checkWithCmd("AT", "OK", handleSIM900message)){
        // power ON
        Serial.println(F("Turning ON GSM shield..."));
        sim900powerUpDown(SIM900_PIN_POWER);

        if(SIM900waitForResponse("Call Ready", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message)){
            Serial.println(F("Done."));
        } else {
            Serial.println(F("Unable to turn ON GSM shield!\nExit."));
            return 0;
        }
    }


    // check sim status
    if(!SIM900checkWithCmd("AT+CPIN?", "+CPIN: READY", handleSIM900message)){
        Serial.println(F("SIMcard ERROR!"));
        return 0;
    }
    SIM900waitForResponse("OK", 1500, SIM900_STRING_MAX_LENGTH, handleSIM900message);

    // set full function
    if(!SIM900checkWithCmd("AT+CFUN=1", "OK", handleSIM900message)){
        Serial.println(F("Command AT+CFUN=1 ERROR!"));
        return 0;
    }

    // set SMS to text mode (set message mode to ASCII)
    if(!SIM900checkWithCmd("AT+CMGF=1", "OK", handleSIM900message)){
        Serial.println(F("Command AT+CMGF=1 ERROR!"));
        return 0;
    }

    // turn on SMS Message notifications (send the SMS data directly to the serial output)
    if(!SIM900checkWithCmd("AT+CNMI=2,1,0,0,0", "OK", handleSIM900message)){ //https://stackoverflow.com/questions/58908575/cnmi-command-how-to-receive-notification-and-save-to-sim-card-incoming-sms
        Serial.println(F("Command AT+CNMI=2,1,0,0,0 ERROR!")); //https://stackoverflow.com/questions/27182456/directly-read-sms-when-it-arrives-via-gsm-modem-in-pc-over-serial-communication
        return 0;
    }

    // turn on Caller ID notifications
    if(!SIM900checkWithCmd("AT+CLIP=1", "OK", handleSIM900message)){
        Serial.println(F("Command AT+CLIP=1 ERROR!"));
        return 0;
    }

    // delete all SMS messages
    if(!SIM900checkWithCmd("AT+CMGDA=\"DEL ALL\"", "OK", handleSIM900message)){
        Serial.println(F("Command AT+CMGDA=\"DEL ALL\" ERROR!"));
        return 0;
    }

    // show all SMS messages
    if(!SIM900checkWithCmd("AT+CMGL=\"ALL\"", "OK", handleSIM900message)){
        Serial.println(F("Command AT+CMGL=\"ALL\" ERROR!"));
        return 0;
    }

    // last check
    if(SIM900checkWithCmd("AT", "OK", handleSIM900message)){
        SIM900flushSerial();
        Serial.println(F("GSM module is ready!"));
    } else {
        Serial.println(F("ERROR - Unable to Init GSM module!"));
        return 0;
    }
    return 1;
}

int sim900checkStatus()
{
    // check free ram
    Serial.print(F("FREE RAM: "));
    Serial.println(freeMemory());

    int errorCount = 0;

    // AT
    if(SIM900checkWithCmd("AT", "OK", handleSIM900message)){
        Serial.println(F("Command AT: OK"));
    } else {
        Serial.println(F("Command AT: ERROR!"));
        errorCount++;
    }

    // SIM card ready
    if(SIM900checkWithCmd("AT+CPIN?", "+CPIN: READY", handleSIM900message)){
        SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
        Serial.println(F("SIM card ready: OK"));
    } else {
        Serial.println(F("SIM card ready: ERROR!"));
        errorCount++;
    }

    // Mobile network registered
    if(SIM900checkWithCmd("AT+CREG?", "+CREG: 0,1", handleSIM900message)){
        SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
        Serial.println(F("Mobile network registered: OK"));
    } else {
        Serial.println(F("Mobile network registered: ERROR!"));
        errorCount++;
    }

    return errorCount;
}

/************************************************ BASIC FUNCTIONS ****************************************************/

void SIM900prepareNewSMS(const char *telNumber)
{
    SIM900.print("AT+CMGS=\"");
    SIM900.print(telNumber);
    SIM900.print("\"\r");
    SIM900waitForResponse("> ", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
}

void SIM900addSMSdata(const char *message)
{
    SIM900.print(message);
    SIM900.print("\r");
    SIM900waitForResponse("> ", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
}

void SIM900sendPreparedSMS()
{
    SIM900.print((char)(26));
    SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
    Serial.println(F("SMS sent: OK"));
}

void SIM900sendSMS(const char *telNumber, const char *message)
{
    SIM900.print("AT+CMGS=\"");
    SIM900.print(telNumber);
    SIM900.print("\"\r");
    SIM900waitForResponse("> ", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
    SIM900.print(message);
    SIM900.print("\r");
    SIM900waitForResponse("> ", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
    SIM900.print((char)(26));
    SIM900waitForResponse("OK", SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, handleSIM900message);
    Serial.println(F("SMS sent: OK"));
}

bool SIM900readLine(char *outputData, int bufferLength)
{
    if (SIM900.available() > 0){
        outputData[SIM900.readBytesUntil('\r', outputData, bufferLength-1)] = '\0'; // EOL = "\r\n"
        //trim(outputData);

        // clear buffer - clear the remaining \r(s) and \n - same as SIM900.readStringUntil('\n');
        char ch = 0;
        unsigned long start_time = millis();
        while(millis()-start_time < 1000){
            if(SIM900.available() > 0 && ch != '\n'){
                ch = (char)SIM900.read();
            } else if (ch == '\n') {
                break;
            }
            delay(1);
        }

        return 1;
    }
    return 0;
}

void SIM900sendCmd(const char *cmd)
{
    SIM900.print(cmd);
    SIM900.print("\r"); // ATS3? - get Command Line Termination Character
    SIM900.flush(); // wait for transmission (and in some versions of the SoftwareSerial.h removes any buffered incoming serial data ---> if not working as expected, remove this line)
}

bool SIM900waitForResponse(const char *expectedResponse, unsigned int timeoutMS, int responseBufferLength, handleSIM900messageFuncPtr callbackFunctionForOtherResponses)
{
    unsigned long start_time = millis();
    char response[responseBufferLength] = {0};
    while(millis()-start_time < timeoutMS){
        if(SIM900.available()){
            memset(response, '\0', sizeof(char)*responseBufferLength);
            bool isEOLread = SIM900readLine(response, sizeof(response) - 1);
            if(callbackFunctionForOtherResponses != NULL) {
                callbackFunctionForOtherResponses(isEOLread, response);
            }
            if(strstr(response, expectedResponse) == response) {
                return 1;
            }
        }
        delay(33);
    }
    return (strstr(response, expectedResponse) == response);
}

bool SIM900checkWithCmd(const char *cmd, const char *expectedResponse, handleSIM900messageFuncPtr callbackFunctionForOtherResponses)
{
    SIM900sendCmd(cmd);
    return SIM900waitForResponse(expectedResponse, SIM900_RESPONSE_TIMEOUT_DEFAULT, SIM900_STRING_MAX_LENGTH, callbackFunctionForOtherResponses);
}

bool SIM900waitForSerialDataAvailable(unsigned int timeoutMS)
{
    unsigned long start_time = millis();
    while(millis()-start_time < timeoutMS){
        if(SIM900.available()){
            return 1;
        }
        delay(33);
    }
    return 0;
}

void SIM900flushSerial()
{
    while(SIM900.available()){
        SIM900.read();
    }
}

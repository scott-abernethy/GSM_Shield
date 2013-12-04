/*
GSM_Shield.cpp
Copyright (c) www.hwkitchen.com and contributors @jgarland79, @harlequin-tech, @scott-abernethy.
This file is part of sqrl, the squirt-library. Please refer to the NOTICE.txt file for license details.
*/

#include "GSM_Shield.h"
#include <avr/pgmspace.h>
#include "sqrl_at.h"

/* Memory optimisations:
 *
 * 1. GSM_Shield library: majority of strings moved into flash to save a lot of RAM.
 */

extern "C" {
  #include <string.h>
}

/* Work around a bug with PROGMEM and PSTR where the compiler always
 *  * generates warnings.
 *   */
#undef PROGMEM 
#define PROGMEM __attribute__(( section(".progmem.data") )) 
#undef PSTR 
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];})) 


#ifdef DEBUG_PRINT
/**********************************************************
Two methods print out debug information to the standard output
- it means to the serial line.
First method prints string.
Second method prints integer numbers.

Note:
=====
The serial line is connected to the GSM module and is 
used for sending AT commands. There is used "trick" that GSM
module accepts not valid AT command strings because it doesn't
understand them and still waits for some valid AT command.
So after all debug strings are sent we send just AT<CR> as
a valid AT command and GSM module responds by OK. So previous 
debug strings are overwritten and GSM module is not influenced
by these debug texts 


string_to_print:  pointer to the string to be print out
last_debug_print: 0 - this is not last debug info, we will
                      continue with sending... so don't send
                      AT<CR>(see explanation above)
                  1 - we are finished with sending debug info 
                      for this time and finished AT<CR> 
                      will be sent(see explanation above)

**********************************************************/
void GSM::DebugPrint(const char *string_to_print, byte last_debug_print)
{
  //if (last_debug_print) {
    Serial.println(string_to_print);
  //  at.SendATCmdWaitResp("AT", 500, 50, "OK", 1);
  //}
  //else Serial.print(string_to_print);
}

void GSM::DebugPrint(int number_to_print, byte last_debug_print)
{
  Serial.println(number_to_print);
  //if (last_debug_print) {
  //  at.SendATCmdWaitResp("AT", 500, 50, "OK", 1);
  //}
}
#endif

/**********************************************************
  Constructor definition
***********************************************************/

GSM::GSM(void) {
  // set some GSM pins as inputs, some as outputs
  //pinMode(GSM_ON, OUTPUT);               // sets pin 5 as output
  //pinMode(GSM_RESET, OUTPUT);            // sets pin 4 as output

  //pinMode(DTMF_OUTPUT_ENABLE, OUTPUT);   // sets pin 2 as output
  // deactivation of IC8 so DTMF is disabled by default
  //digitalWrite(DTMF_OUTPUT_ENABLE, LOW);
  
  // not registered yet
  module_status = STATUS_NONE;

  // initialization of speaker volume
  last_speaker_volume = 0; 
}

/**********************************************************
  Initialization of GSM module serial line
**********************************************************/
void GSM::InitSerLine()
{
  for (int i=1;i<7;i++) {
    switch (i) {
      case 1:
        Serial.begin(4800);
        break;
      case 2:
        Serial.begin(9600);
        break;
      case 3:
        Serial.begin(19200);
        break;
      case 4:
        Serial.begin(38400);
        break;
      case 5:
        Serial.begin(57600);
        break;
      case 6:
        Serial.begin(115200);
        break;
    }
    delay(1000);
    at.SendATCmdWaitResp(F("AT+IPR=9600"), 500, 50, F("OK"), 5);
    delay(1000);
    Serial.begin(9600);
    delay(1000);
    if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT"), 500, 100, F("OK"), 5)){
#ifdef DEBUG_PRINT
      Serial.print("DEBUG: Baud fixed via ");
      Serial.println(i);
#endif
      break;
    }
  }

  // communication line is not used yet = free
  at.SetCommLineStatus(CLS_FREE);
  // pointer is initialized to the first item of comm. buffer
  at.p_comm_buf = &at.comm_buf[0];
}



/**********************************************************
Methods return the state of corresponding
bits in the status variable

- these methods do not communicate with the GSM module

return values: 
      0 - not true (not active)
      >0 - true (active)
**********************************************************/
byte GSM::IsRegistered(void)
{
  return (module_status & STATUS_REGISTERED);
}

byte GSM::IsInitialized(void)
{
  return (module_status & STATUS_INITIALIZED);
}

void GSM::ModeInit(void) {
  // Init driver pins
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);

  delay(500);

  // Output GSM Timing
  digitalWrite(5, HIGH);
  delay(1500);
  digitalWrite(5, LOW);

  delay(500);

  // UART OFF
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);

  delay(500);

  // GSM UART
  digitalWrite(3, LOW);

  delay(5000);

  Serial.begin(9600);
  Echo(0);

  delay(5000);
}

void GSM::ModeGSM(void) {

  // UART OFF
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);

  delay(50);

  // GSM UART
  digitalWrite(3, LOW);

  delay(50);

  Echo(0);
}

void GSM::ModeGPS(void) {

  // UART OFF
  digitalWrite(3, HIGH);
  digitalWrite(4, HIGH);

  delay(50);

  // GPS UART
  digitalWrite(4, LOW);

  delay(50);

  Echo(0);
}

/**********************************************************
  Checks if the GSM module is responding 
  to the AT command
  - if YES nothing is made 
  - if NO GSM module is turned on 
**********************************************************/
void GSM::TurnOn(void)
{
  ModeInit();

  at.SetCommLineStatus(CLS_ATCMD);
  if (AT_RESP_ERR_NO_RESP == at.SendATCmdWaitResp(F("AT"), 900, 200, F("OK"), 5)) {
    // there is no response => turn on the module

#ifdef DEBUG_PRINT
    Serial.println("DEBUG: GSM module is off");
    Serial.println("DEBUG: start the module");
#endif

    ModeInit();
  }
  else {
#ifdef DEBUG_PRINT
    Serial.println("DEBUG: GSM module is on");
#endif
  }

  if (AT_RESP_ERR_DIF_RESP == at.SendATCmdWaitResp(F("AT"), 900, 200, F("OK"), 5)) {
    //check OK

#ifdef DEBUG_PRINT
    Serial.println("DEBUG: the baud is not ok");
#endif

    InitSerLine();

    // pointer is initialized to the first item of comm. buffer
    at.p_comm_buf = &at.comm_buf[0];

    if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT"), 900, 200, F("OK"), 5)) {
#ifdef DEBUG_PRINT
      Serial.println("DEBUG: Baud now ok");
#endif
    }
    else {
#ifdef DEBUG_PRINT
      Serial.println("DEBUG: Baud still bad");
#endif
    }
  }
  else {
#ifdef DEBUG_PRINT
    Serial.println("DEBUG: 2 GSM module is on and baud is ok");
#endif
  }

  at.SetCommLineStatus(CLS_FREE);
}

// TODO print info 
//Serial.println("ATI"); // Product info -- SIM900 R11.0
//delay(1000);
//Serial.println("AT&V"); // Configuration profiles -- ACTIVE PROFILE: E0 Q0 V1 X4 &C1 &D1 +I
//delay(1000);
//Serial.println("AT+COPS=?"); // Network operators test command
//delay(1000);
//Serial.println("AT+COPS?"); // Network operators read command -- +COPS: 0,0,"2degrees"
//delay(1000);

byte GSM::Ready() {
  if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT"), 200, 50, F("OK"), 2)) {
    return GEN_SUCCESS;
  }
  else {
    return GEN_FAILURE;
  }
}

// eg 4564243333334414892F
byte GSM::GetICCID(char *id_string) {
  byte ret_code = GEN_FAILURE;
  id_string[0] = 0x00;
  id_string[20] = 0x00;

  if (CLS_FREE != at.GetCommLineStatus()) return 0;
  at.SetCommLineStatus(CLS_ATCMD);

  if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT+CCID"), 500, 50, F("OK"), 5)) {
    at.ReadBuffer(id_string, 2, 20);
    ret_code = GEN_SUCCESS;
  }

  at.SetCommLineStatus(CLS_FREE);

  return ret_code;
}

/**********************************************************
  Sends parameters for initialization of GSM module

  group:  0 - parameters of group 0 - not necessary to be registered in the GSM
          1 - parameters of group 1 - it is necessary to be registered
**********************************************************/
void GSM::InitParam(byte group)
{
  switch (group) {
    case PARAM_SET_0:
      if (CLS_FREE != at.GetCommLineStatus()) return;
      at.SetCommLineStatus(CLS_ATCMD);

#ifdef DEBUG_PRINT
      DebugPrint("DEBUG: configure the module PARAM_SET_0\r\n", 0);
#endif

      // Reset to the factory settings
      at.SendATCmdWaitResp(F("AT&F"), 1000, 50, F("OK"), 5);
      // switch off echo
      at.SendATCmdWaitResp(F("ATE0"), 500, 50, F("OK"), 5);
      // setup fixed baud rate
      at.SendATCmdWaitResp(F("AT+IPR=9600"), 500, 50, F("OK"), 5);
      // turn off ip mode
      at.SendATCmdWaitResp(F("AT+SAPBR=0,1"), 900, 100, F("OK"), 2);
      // setup mode
      //at.SendATCmdWaitResp("AT#SELINT=1", 500, 50, "OK", 5);
      // Switch ON User LED - just as signalization we are here
      //at.SendATCmdWaitResp("AT#GPIO=8,1,1", 500, 50, "OK", 5);
      // Sets GPIO9 as an input = user button
      //at.SendATCmdWaitResp("AT#GPIO=9,0,0", 500, 50, "OK", 5);
      // allow audio amplifier control
      //at.SendATCmdWaitResp("AT#GPIO=5,0,2", 500, 50, "OK", 5);
      // Switch OFF User LED- just as signalization we are finished
      //at.SendATCmdWaitResp("AT#GPIO=8,0,1", 500, 50, "OK", 5);
      at.SetCommLineStatus(CLS_FREE);
      break;

    case PARAM_SET_1:
      if (CLS_FREE != at.GetCommLineStatus()) return;
      at.SetCommLineStatus(CLS_ATCMD);

#ifdef DEBUG_PRINT
      DebugPrint("DEBUG: configure the module PARAM_SET_1\r\n", 0);
#endif

      at.SendATCmdWaitResp(F("AT+CLIP=1"), 500, 50, F("OK"), 5); // Request calling line identification
      at.SendATCmdWaitResp(F("AT+CRC=1"), 500, 50, F("OK"), 5); // Extended call indication +CRING
      //at.SendATCmdWaitResp("AT+CCLK=12/11/18,20:40:00", 500, 50, "OK", 5); // Set date and time
      at.SendATCmdWaitResp(F("AT+CMEE=0"), 500, 50, F("OK"), 5); // Mobile Equipment Error Code
      //at.SendATCmdWaitResp("AT#SHFEC=1", 500, 50, "OK", 5); // Echo canceller enabled 
      //at.SendATCmdWaitResp("AT#SRS=26,0", 500, 50, "OK", 5); // Ringer tone select (0 to 32)
      //at.SendATCmdWaitResp("AT#HFMICG=7", 1000, 50, "OK", 5); // Microphone gain (0 to 7) - response here sometimes takes more than 500msec. so 1000msec. is more safety
      at.SendATCmdWaitResp(F("AT+CMGF=1"), 500, 50, F("OK"), 5); // set the SMS mode to text 
      //at.SendATCmdWaitResp("ATS0=1", 500, 50, "OK", 5); // Auto answer after first ring enabled
      //at.SendATCmdWaitResp("AT#SRP=1", 500, 50, "OK", 5); // select ringer path to handsfree
      //at.SendATCmdWaitResp("AT+CRSL=2", 500, 50, "OK", 5); // select ringer sound level
      at.SendATCmdWaitResp(F("AT+CPBS=\"SM\""), 1000, 50, F("OK"), 5); // Set phonebook memory storage as SIM card

      at.SetCommLineStatus(CLS_FREE);
      //SetSpeakerVolume(9); // select speaker volume (0 to 14)
      InitSMSMemory();
      break;
  }
}

/**********************************************************
  Enables DTMF receiver = IC8 device 
**********************************************************/
/*void GSM::EnableDTMF(void)
{
  // configuration of corresponding DTMF pins
  pinMode(DTMF_DATA_VALID, INPUT);       // sets pin 3 as input
  pinMode(DTMF_DATA0, INPUT);            // sets pin 6 as input
  pinMode(DTMF_DATA1, INPUT);            // sets pin 7 as input
  pinMode(DTMF_DATA2, INPUT);            // sets pin 8 as input
  pinMode(DTMF_DATA3, INPUT);            // sets pin 9 as input
  // activation of IC8 
  digitalWrite(DTMF_OUTPUT_ENABLE, HIGH);
}
*/
/**********************************************************
Checks if DTMF signal is valid

return: >= 0x10hex(16dec) - DTMF signal is NOT valid
        0..15 - valid DTMF signal
**********************************************************/
/*byte GSM::GetDTMFSignal(void)
{
  static byte last_state = LOW; // initialization
  byte ret_val = 0xff;          // default - not valid

  if (digitalRead(DTMF_DATA_VALID)==HIGH
      && last_state == LOW) {
    // valid DTMF signal => decode it
    // ------------------------------
    last_state = HIGH; // remember last state

    ret_val = 0;
    if (digitalRead(DTMF_DATA0)) ret_val |= 0x01;
    if (digitalRead(DTMF_DATA1)) ret_val |= 0x02;
    if (digitalRead(DTMF_DATA2)) ret_val |= 0x04;
    if (digitalRead(DTMF_DATA3)) ret_val |= 0x08;

    // confirm DTMF signal by the tone 5
    // ---------------------------------
    SendDTMFSignal(5);
  }
  else if (digitalRead(DTMF_DATA_VALID) == LOW) {
    // pulse is not valid => enable to check DTMF signal again
    // -------------------------------------------------------
    last_state = LOW;
  }
  return (ret_val);
}
*/
/**********************************************************
Turns on/off the speaker

off_on: 0 - off
        1 - on
**********************************************************/
void GSM::SetSpeaker(byte off_on)
{
  if (CLS_FREE != at.GetCommLineStatus()) return;
  at.SetCommLineStatus(CLS_ATCMD);
  if (off_on) {
    //at.SendATCmdWaitResp("AT#GPIO=5,1,2", 500, 50, "#GPIO:", 1);
  }
  else {
    //at.SendATCmdWaitResp("AT#GPIO=5,0,2", 500, 50, "#GPIO:", 1);
  }
  at.SetCommLineStatus(CLS_FREE);
}




/**********************************************************
Method checks if the GSM module is registered in the GSM net
- this method communicates directly with the GSM module
  in contrast to the method IsRegistered() which reads the
  flag from the module_status (this flag is set inside this method)

- must be called regularly - from 1sec. to cca. 10 sec.

return values: 
      REG_NOT_REGISTERED  - not registered
      REG_REGISTERED      - GSM module is registered
      REG_NO_RESPONSE     - GSM doesn't response
      REG_COMM_LINE_BUSY  - comm line between GSM module and Arduino is not free
                            for communication
**********************************************************/
byte GSM::CheckRegistration(void)
{
  byte status;
  byte ret_val = REG_NOT_REGISTERED;

  if (CLS_FREE != at.GetCommLineStatus()) return (REG_COMM_LINE_BUSY);
  at.SetCommLineStatus(CLS_ATCMD);
  Serial.println(F("AT+CREG?"));

  status = at.WaitResp(5000, 200); 

  if (status == RX_FINISHED) {
    if (at.IsStringReceived(F("+CREG: 0,1"))
      || at.IsStringReceived(F("+CREG: 0,5"))) {
      // it means module is registered
      // ----------------------------
      module_status |= STATUS_REGISTERED;

      // in case GSM module is registered first time after reset
      // sets flag STATUS_INITIALIZED
      // it is used for sending some init commands which 
      // must be sent only after registration
      // --------------------------------------------
      if (!IsInitialized()) {
        module_status |= STATUS_INITIALIZED;
        at.SetCommLineStatus(CLS_FREE);
        InitParam(PARAM_SET_1);
      }
      ret_val = REG_REGISTERED;      
    }
    else {
      // NOT registered
      module_status &= ~STATUS_REGISTERED;
      ret_val = REG_NOT_REGISTERED;
    }
  }
  else {
    ret_val = REG_NO_RESPONSE;
  }
  at.SetCommLineStatus(CLS_FREE);
 
  return (ret_val);
}

/**********************************************************
Method checks status of call

return: 
      CALL_NONE         - no call activity
      CALL_INCOM_VOICE  - incoming voice
      CALL_ACTIVE_VOICE - active voice
      CALL_NO_RESPONSE  - no response to the AT command 
      CALL_COMM_LINE_BUSY - comm line is not free
**********************************************************/
byte GSM::CallStatus(void)
{
  byte ret_val = CALL_NONE;

  if (CLS_FREE != at.GetCommLineStatus()) return (CALL_COMM_LINE_BUSY);
  at.SetCommLineStatus(CLS_ATCMD);
  Serial.println(F("AT+CPAS"));

  if (RX_TMOUT_ERR == at.WaitResp(5000, 200)) {
    ret_val = CALL_NO_RESPONSE;
  }
  else {
    if (at.IsStringReceived(F("+CPAS: 0"))) {
      ret_val = CALL_NONE;
    }
    else if (at.IsStringReceived(F("+CPAS: 3"))) {
      ret_val = CALL_INCOM_VOICE;
    }
    else if (at.IsStringReceived(F("+CPAS: 4"))) {
      ret_val = CALL_ACTIVE_VOICE;
    }
  }

  // TODO set incoming call number?
  // +CPAS: 3 \ OK \ RING \ +CLIP: "0800123123",161,"",,"",0

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);

}

/**********************************************************
Method checks status of call(incoming or active) 
and makes authorization with specified SIM positions range

phone_number: a pointer where the tel. number string of current call will be placed
              so the space for the phone number string must be reserved - see example
first_authorized_pos: initial SIM phonebook position where the authorization process
                      starts
last_authorized_pos:  last SIM phonebook position where the authorization process
                      finishes

                      Note(important):
                      ================
                      In case first_authorized_pos=0 and also last_authorized_pos=0
                      the received incoming phone number is NOT authorized at all, so every
                      incoming is considered as authorized (CALL_INCOM_VOICE_NOT_AUTH is returned)

return: 
      CALL_NONE                   - no call activity
      CALL_INCOM_VOICE_AUTH       - incoming voice - authorized
      CALL_INCOM_VOICE_NOT_AUTH   - incoming voice - not authorized
      CALL_ACTIVE_VOICE           - active voice
      CALL_INCOM_DATA_AUTH        - incoming data call - authorized
      CALL_INCOM_DATA_NOT_AUTH    - incoming data call - not authorized  
      CALL_ACTIVE_DATA            - active data call
      CALL_NO_RESPONSE            - no response to the AT command 
      CALL_COMM_LINE_BUSY         - comm line is not free
**********************************************************/
byte GSM::CallStatusWithAuth(char *phone_number, byte &fav,
                             byte first_authorized_pos, byte last_authorized_pos)
{
  byte ret_val = CALL_NONE;
  byte search_phone_num = 0;
  byte i;
  byte status;
  char *p_char; 
  char *p_char1;

  phone_number[0] = 0x00;  // no phonr number so far
  if (CLS_FREE != at.GetCommLineStatus()) return (CALL_COMM_LINE_BUSY);
  at.SetCommLineStatus(CLS_ATCMD);
  status = at.SendATCmdWaitResp(F("AT+CLCC"), 5000, 1500, F("OK\r\n"), 1);

  // there is either NO call:
  // <CR><LF>OK<CR><LF>

  // or there is at least 1 call
  // +CLCC: 1,1,4,0,0,"+420XXXXXXXXX",145<CR><LF>
  // <CR><LF>OK<CR><LF>


  // TODO if this is important, make it lower level:
  // generate tmout 30msec. before next AT command
  /* delay(30); */

  if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT+CLCC"), 5000, 1500, F("OK\r\n"), 1)) {
    // something was received but what was received?
    // example: //+CLCC: 1,1,4,0,0,"+420XXXXXXXXX",145
    // ---------------------------------------------
    if(at.IsStringReceived(F("+CLCC: 1,1,4,0,0"))) { 
      // incoming VOICE call - not authorized so far
      // -------------------------------------------
      search_phone_num = 1;
      ret_val = CALL_INCOM_VOICE_NOT_AUTH;
    }
    else if(at.IsStringReceived(F("+CLCC: 1,1,4,1,0"))) {
      // incoming DATA call - not authorized so far
      // ------------------------------------------
      search_phone_num = 1;
      ret_val = CALL_INCOM_DATA_NOT_AUTH;
    }
    else if(at.IsStringReceived(F("+CLCC: 1,0,0,0,0"))) { 
      // active VOICE call - GSM is caller
      // ----------------------------------
      search_phone_num = 1;
      ret_val = CALL_ACTIVE_VOICE;
    }
    else if(at.IsStringReceived(F("+CLCC: 1,1,0,0,0"))) { 
      // active VOICE call - GSM is listener
      // -----------------------------------
      search_phone_num = 1;
      ret_val = CALL_ACTIVE_VOICE;
    }
    else if(at.IsStringReceived(F("+CLCC: 1,1,0,1,0"))) { 
      // active DATA call - GSM is listener
      // ----------------------------------
      search_phone_num = 1;
      ret_val = CALL_ACTIVE_DATA;
    }
    else if(at.IsStringReceived(F("+CLCC:"))){ 
      // other string is not important for us - e.g. GSM module activate call
      // etc.
      // IMPORTANT - each +CLCC:xx response has also at the end
      // string <CR><LF>OK<CR><LF>
      ret_val = CALL_OTHERS;
    }
    else if(at.IsStringReceived(F("OK"))){ 
      // only "OK" => there is NO call activity
      // --------------------------------------
      ret_val = CALL_NONE;
    }

    // now we will search phone num string
    if (search_phone_num) {
      // extract phone number string
      // ---------------------------
      p_char = strchr((char *)(at.comm_buf),'"');
      p_char1 = p_char+1; // we are on the first phone number character
      p_char = strchr((char *)(p_char1),'"');
      if (p_char != NULL) {
        *p_char = 0; // end of string
        strcpy(phone_number, (char *)(p_char1));
      }

      if ( (ret_val == CALL_INCOM_VOICE_NOT_AUTH) 
           || (ret_val == CALL_INCOM_DATA_NOT_AUTH)) {

        if ((first_authorized_pos == 0) && (last_authorized_pos == 0)) {
          // authorization is not required => it means authorization is OK
          // -------------------------------------------------------------
          if (ret_val == CALL_INCOM_VOICE_NOT_AUTH) ret_val = CALL_INCOM_VOICE_AUTH;
          else ret_val = CALL_INCOM_DATA_AUTH;
        }
        else {
          // make authorization
          // ------------------
          at.SetCommLineStatus(CLS_FREE);
          for (i = first_authorized_pos; i <= last_authorized_pos; i++) {
            if (ComparePhoneNumber(i, phone_number)) {
              // phone numbers are identical
              // authorization is OK
              // ---------------------------
              fav = i;
              if (ret_val == CALL_INCOM_VOICE_NOT_AUTH) ret_val = CALL_INCOM_VOICE_AUTH;
              else ret_val = CALL_INCOM_DATA_AUTH;
              break;  // and finish authorization
            }
          }
        }
      }
    }
  }
  else {
    ret_val = CALL_NO_RESPONSE;
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/**********************************************************
Method picks up an incoming call

return: 
**********************************************************/
void GSM::PickUp(void)
{
  if (CLS_FREE != at.GetCommLineStatus()) return;
  at.SetCommLineStatus(CLS_ATCMD);
  at.SendATCmdWaitResp(F("ATA"), 1000, 100, F("OK"), 2);
  at.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method hangs up incoming or active call

return: 
**********************************************************/
void GSM::HangUp(void)
{
  if (CLS_FREE != at.GetCommLineStatus()) return;
  at.SetCommLineStatus(CLS_ATCMD);
  at.SendATCmdWaitResp(F("ATH"), 1000, 100, F("OK"), 2);
  at.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method calls the specific number

number_string: pointer to the phone number string 
               e.g. gsm.Call("+420123456789"); 

**********************************************************/
void GSM::Call(char *number_string)
{
  if (CLS_FREE != at.GetCommLineStatus()) return;
  at.SetCommLineStatus(CLS_ATCMD);
  // ATDxxxxxx;<CR>
  Serial.print(F("ATD"));
  Serial.print(number_string);
  Serial.println(F(";"));
  at.WaitResp(10000, 200);
  at.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method calls the number stored at the specified SIM position

sim_position: position in the SIM <1...>
              e.g. gsm.Call(1);
**********************************************************/
void GSM::Call(int sim_position)
{
  if (CLS_FREE != at.GetCommLineStatus()) return;
  at.SetCommLineStatus(CLS_ATCMD);
  // ATD>"SM" 1;<CR>
  Serial.print(F("ATD>\"SM\" "));
  Serial.print(sim_position);
  Serial.println(F(";"));
  at.WaitResp(10000, 200);
  at.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method sets speaker volume

speaker_volume: volume in range 0..14

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module did not answer in timeout
        -3 - GSM module has answered "ERROR" string

        OK ret val:
        -----------
        0..14 current speaker volume 
**********************************************************/
char GSM::SetSpeakerVolume(byte speaker_volume)
{
  
  char ret_val = -1;

  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  // remember set value as last value
  if (speaker_volume > 14) speaker_volume = 14;
  // select speaker volume (0 to 14)
  // AT+CLVL=X<CR>   X<0..14>
  Serial.print(F("AT+CLVL="));
  Serial.print((int)speaker_volume);    
  Serial.print('\r'); // send <CR>
  // 10 sec. for initial comm tmout
  // 50 msec. for inter character timeout
  if (RX_TMOUT_ERR == at.WaitResp(10000, 50)) {
    ret_val = -2; // ERROR
  }
  else {
    if(at.IsStringReceived(F("OK"))) {
      last_speaker_volume = speaker_volume;
      ret_val = last_speaker_volume; // OK
    }
    else ret_val = -3; // ERROR
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/**********************************************************
Method increases speaker volume

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module did not answer in timeout
        -3 - GSM module has answered "ERROR" string

        OK ret val:
        -----------
        0..14 current speaker volume 
**********************************************************/
char GSM::IncSpeakerVolume(void)
{
  char ret_val;
  byte current_speaker_value;

  current_speaker_value = last_speaker_volume;
  if (current_speaker_value < 14) {

    current_speaker_value++;
    ret_val = SetSpeakerVolume(current_speaker_value);
  }
  else ret_val = 14;

  return (ret_val);
}

/**********************************************************
Method decreases speaker volume

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module did not answer in timeout
        -3 - GSM module has answered "ERROR" string

        OK ret val:
        -----------
        0..14 current speaker volume 
**********************************************************/
char GSM::DecSpeakerVolume(void)
{
  char ret_val;
  byte current_speaker_value;

  current_speaker_value = last_speaker_volume;
  if (current_speaker_value > 0) {
    current_speaker_value--;
    ret_val = SetSpeakerVolume(current_speaker_value);
  }
  else ret_val = 0;

  return (ret_val);
}

/**********************************************************
Method sends DTMF signal
This function only works when call is in progress

dtmf_tone: tone to send 0..15

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - GSM module has answered "ERROR" string

        OK ret val:
        -----------
        0.. tone
**********************************************************/
char GSM::SendDTMFSignal(byte dtmf_tone)
{
  char ret_val = -1;

  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  // e.g. AT+VTS=5<CR>
  Serial.print(F("AT+VTS="));
  Serial.print((int)dtmf_tone);    
  Serial.print('\r');
  // 1 sec. for initial comm tmout
  // 50 msec. for inter character timeout
  if (RX_TMOUT_ERR == at.WaitResp(1000, 50)) {
    ret_val = -2; // ERROR
  }
  else {
    if(at.IsStringReceived(F("OK"))) {
      ret_val = dtmf_tone; // OK
    }
    else ret_val = -3; // ERROR
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}


/**********************************************************
Method returns state of user button


return: 0 - not pushed = released
        1 - pushed
**********************************************************/
byte GSM::IsUserButtonPushed(void)
{
  byte ret_val = 0;
  if (CLS_FREE != at.GetCommLineStatus()) return(0);
  at.SetCommLineStatus(CLS_ATCMD);
  //if (AT_RESP_OK == at.SendATCmdWaitResp("AT#GPIO=9,2", 500, 50, "#GPIO: 0,0", 1)) {
    // user button is pushed
  //  ret_val = 1;
  //}
  //else ret_val = 0;
  //at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}


/**********************************************************
Method sends SMS

number_str:   pointer to the phone number string
message_str:  pointer to the SMS text string


return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - GSM module has answered "ERROR" string

        OK ret val:
        -----------
        0 - SMS was not sent
        1 - SMS was sent


an example of usage:
        GSM gsm;
        gsm.SendSMS("00XXXYYYYYYYYY", "SMS text");
**********************************************************/
char GSM::SendSMS(const __FlashStringHelper *number_str, char *message_str)
{
  char ret_val = -1;
  byte i;

  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);  
  ret_val = 0; // still not send
  // try to send SMS 3 times in case there is some problem
  for (i = 0; i < 3; i++) {
    // send  AT+CMGS="number_str"
    Serial.print(F("AT+CMGS=\""));
    Serial.print(number_str);  
    Serial.print(F("\"\r"));

    // 1000 msec. for initial comm tmout
    // 50 msec. for inter character timeout
    if (RX_FINISHED_STR_RECV == at.WaitResp(1000, 50, F(">"))) {
      // send SMS text
      Serial.print(message_str); 
	  
#ifdef DEBUG_SMS_ENABLED
      // SMS will not be sent = we will not pay => good for debugging
      Serial.write(0x1b);
      if (RX_FINISHED_STR_RECV == at.WaitResp(7000, 50, F("OK"))) { /* } */
#else 
      Serial.write(0x1a);
	  //Serial.flush(); // erase rx circular buffer
      if (RX_FINISHED_STR_RECV == at.WaitResp(7000, 5000, F("+CMGS"))) {
#endif
        // SMS was send correctly 
        ret_val = 1;
		#ifdef DEBUG_PRINT
			DebugPrint("SMS was send correctly \r\n", 0);
		#endif
        break;
      }
      else continue;
    }
    else {
      // try again
      continue;
    }
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

char GSM::SendSMS(char *number_str, char *message_str) 
{
  char ret_val = -1;
  byte i;

  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);  
  ret_val = 0; // still not send
  // try to send SMS 3 times in case there is some problem
  for (i = 0; i < 3; i++) {
    // send  AT+CMGS="number_str"
    Serial.print(F("AT+CMGS=\""));
    Serial.print(number_str);  
    Serial.print(F("\"\r"));

    // 1000 msec. for initial comm tmout
    // 50 msec. for inter character timeout
    if (RX_FINISHED_STR_RECV == at.WaitResp(1000, 50, F(">"))) {
      // send SMS text
      Serial.print(message_str); 
	  
#ifdef DEBUG_SMS_ENABLED
      // SMS will not be sent = we will not pay => good for debugging
      Serial.write(0x1b);
      if (RX_FINISHED_STR_RECV == at.WaitResp(7000, 50, F("OK"))) { /* } */
#else 
      Serial.write(0x1a);
	  //Serial.flush(); // erase rx circular buffer
      if (RX_FINISHED_STR_RECV == at.WaitResp(7000, 5000, F("+CMGS"))) {
#endif
        // SMS was send correctly 
        ret_val = 1;
		#ifdef DEBUG_PRINT
			DebugPrint("SMS was send correctly \r\n", 0);
		#endif
        break;
      }
      else continue;
    }
    else {
      // try again
      continue;
    }
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/**********************************************************
Method sends SMS to the specified SIM phonebook position

sim_phonebook_position:   SIM phonebook position <1..20>
message_str:              pointer to the SMS text string


return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - specified position must be > 0

        OK ret val:
        -----------
        0 - SMS was not sent
        1 - SMS was sent


an example of usage:
        GSM gsm;
        gsm.SendSMS(1, "SMS text");
**********************************************************/
char GSM::SendSMS(byte sim_phonebook_position, char *message_str) 
{
  char ret_val = -1;
  char sim_phone_number[20];

  ret_val = 0; // SMS is not send yet
  if (sim_phonebook_position == 0) return (-3);
  if (1 == GetPhoneNumber(sim_phonebook_position, sim_phone_number)) {
    // there is a valid number at the spec. SIM position
    // => send SMS
    // -------------------------------------------------
    ret_val = SendSMS(sim_phone_number, message_str);
  }
  return (ret_val);

}

/**********************************************************
Method initializes memory for the incoming SMS in the Telit
module - SMSs will be stored in the SIM card

!!This function is used internally after first registration
so it is not necessary to used it in the user sketch

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free

        OK ret val:
        -----------
        0 - SMS memory was not initialized
        1 - SMS memory was initialized

**********************************************************/
char GSM::InitSMSMemory(void) 
{
  char ret_val = -1;

  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // not initialized yet
  
  // Disable messages about new SMS from the GSM module 
  at.SendATCmdWaitResp(F("AT+CNMI=2,0"), 1000, 50, F("OK"), 2);

  // send AT command to init memory for SMS in the SIM card
  // response:
  // +CPMS: <usedr>,<totalr>,<usedw>,<totalw>,<useds>,<totals>
  // Changed to two SM s only - TJH
  if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT+CPMS=\"SM\",\"SM\""), 1000, 1000, F("+CPMS:"), 10)) {
    ret_val = 1;
  }
  else ret_val = 0;

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/**********************************************************
Method finds out if there is present at least one SMS with
specified status

Note:
if there is new SMS before IsSMSPresent() is executed
this SMS has a status UNREAD and then
after calling IsSMSPresent() method status of SMS
is automatically changed to READ

required_status:  SMS_UNREAD  - new SMS - not read yet
                  SMS_READ    - already read SMS                  
                  SMS_ALL     - all stored SMS

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout

        OK ret val:
        -----------
        0 - there is no SMS with specified status
        1..20 - position where SMS is stored 
                (suitable for the function GetGSM())


an example of use:
        GSM gsm;
        char position;  
        char phone_number[20]; // array for the phone number string
        char sms_text[100];

        position = gsm.IsSMSPresent(SMS_UNREAD);
        if (position) {
          // read new SMS
          gsm.GetGSM(position, phone_num, sms_text, 100);
          // now we have phone number string in phone_num
          // and SMS text in sms_text
        }
**********************************************************/
char GSM::IsSMSPresent(byte required_status) 
{
  char ret_val = -1;
  char *p_char;
  byte status;

  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // still not present

  switch (required_status) {
    case SMS_UNREAD:
      Serial.print(F("AT+CMGL=\"REC UNREAD\"\r"));
      break;
    case SMS_READ:
      Serial.print(F("AT+CMGL=\"REC READ\"\r"));
      break;
    case SMS_ALL:
      Serial.print(F("AT+CMGL=\"ALL\"\r"));
      break;
  }

  if (RX_FINISHED_STR_RECV == at.WaitResp(5000, 1500, F("OK"))) {

    // there is either NO SMS:
    // <CR><LF>OK<CR><LF>

    // or there is at least 1 SMS
    // +CMGL: <index>,<stat>,<oa/da>,,[,<tooa/toda>,<length>]
    // <CR><LF> <data> <CR><LF>OK<CR><LF>

    if (at.IsStringReceived(F("+CMGL:"))) {
      // there is some SMS with status => get its position
      // response is:
      // +CMGL: <index>,<stat>,<oa/da>,,[,<tooa/toda>,<length>]
      // <CR><LF> <data> <CR><LF>OK<CR><LF>
      p_char = strchr((char *)at.comm_buf,':');
      if (p_char != NULL) {
        ret_val = atoi(p_char+1);
      }
    }
    else {
      // other response like OK or ERROR
      ret_val = 0;
    }
  }
  else {
    ret_val = -2;
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}


/**********************************************************
Method reads SMS from specified memory(SIM) position

position:     SMS position <1..20>
phone_number: a pointer where the phone number string of received SMS will be placed
              so the space for the phone number string must be reserved - see example
SMS_text  :   a pointer where SMS text will be placed
max_SMS_len:  maximum length of SMS text excluding also string terminating 0x00 character
              
return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - specified position must be > 0

        OK ret val:
        -----------
        GETSMS_NO_SMS       - no SMS was not found at the specified position
        GETSMS_UNREAD_SMS   - new SMS was found at the specified position
        GETSMS_READ_SMS     - already read SMS was found at the specified position
        GETSMS_OTHER_SMS    - other type of SMS was found 


an example of usage:
        GSM gsm;
        char position;
        char phone_num[20]; // array for the phone number string
        char sms_text[100]; // array for the SMS text string

        position = gsm.IsSMSPresent(SMS_UNREAD);
        if (position) {
          // there is new SMS => read it
          gsm.GetGSM(position, phone_num, sms_text, 100);
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG SMS phone number: ", 0);
            gsm.DebugPrint(phone_num, 0);
            gsm.DebugPrint("\r\n          SMS text: ", 0);
            gsm.DebugPrint(sms_text, 1);
          #endif
        }        
**********************************************************/
char GSM::GetSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len) 
{
  char ret_val = -1;
  char *p_char; 
  char *p_char1;
  byte len;

  if (position == 0) return (-3);
  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  phone_number[0] = 0;  // end of string for now
  ret_val = GETSMS_NO_SMS; // still no SMS
  
  //send "AT+CMGR=X" - where X = position
  Serial.print(F("AT+CMGR="));
  Serial.print((int)position);  
  Serial.print('\r');

  // 5000 msec. for initial comm tmout
  // 100 msec. for inter character tmout
  switch (at.WaitResp(5000, 100, F("+CMGR"))) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      ret_val = -2;
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // OK was received => there is NO SMS stored in this position
      if(at.IsStringReceived(F("OK"))) {
        // there is only response <CR><LF>OK<CR><LF> 
        // => there is NO SMS
        ret_val = GETSMS_NO_SMS;
      }
      else if(at.IsStringReceived(F("ERROR"))) {
        // error should not be here but for sure
        ret_val = GETSMS_NO_SMS;
      }
      break;

    case RX_FINISHED_STR_RECV:
      // find out what was received exactly

      //response for new SMS:
      //<CR><LF>+CMGR: "REC UNREAD","+XXXXXXXXXXXX",,"02/03/18,09:54:28+40"<CR><LF>
		  //There is SMS text<CR><LF>OK<CR><LF>
      if(at.IsStringReceived(F("\"REC UNREAD\""))) { 
        // get phone number of received SMS: parse phone number string 
        // +XXXXXXXXXXXX
        // -------------------------------------------------------
        ret_val = GETSMS_UNREAD_SMS;
      }
      //response for already read SMS = old SMS:
      //<CR><LF>+CMGR: "REC READ","+XXXXXXXXXXXX",,"02/03/18,09:54:28+40"<CR><LF>
		  //There is SMS text<CR><LF>
      else if(at.IsStringReceived(F("\"REC READ\""))) {
        // get phone number of received SMS
        // --------------------------------
        ret_val = GETSMS_READ_SMS;
      }
      else {
        // other type like stored for sending.. 
        ret_val = GETSMS_OTHER_SMS;
      }

      // extract phone number string
      // ---------------------------
      p_char = strchr((char *)(at.comm_buf),',');
      p_char1 = p_char+2; // we are on the first phone number character
      p_char = strchr((char *)(p_char1),'"');
      if (p_char != NULL) {
        *p_char = 0; // end of string
        strcpy(phone_number, (char *)(p_char1));
      }


      // get SMS text and copy this text to the SMS_text buffer
      // ------------------------------------------------------
      p_char = strchr(p_char+1, 0x0a);  // find <LF>
      if (p_char != NULL) {
        // next character after <LF> is the first SMS character
        p_char++; // now we are on the first SMS character 

        // find <CR> as the end of SMS string
        p_char1 = strchr((char *)(p_char), 0x0d);  
        if (p_char1 != NULL) {
          // finish the SMS text string 
          // because string must be finished for right behaviour 
          // of next strcpy() function
          *p_char1 = 0; 
        }
        // in case there is not finish sequence <CR><LF> because the SMS is
        // too long (more then 130 characters) sms text is finished by the 0x00
        // directly in the at.WaitResp() routine

        // find out length of the SMS (excluding 0x00 termination character)
        len = strlen(p_char);

        if (len < max_SMS_len) {
          // buffer SMS_text has enough place for copying all SMS text
          // so copy whole SMS text
          // from the beginning of the text(=p_char position) 
          // to the end of the string(= p_char1 position)
          strcpy(SMS_text, (char *)(p_char));
        }
        else {
          // buffer SMS_text doesn't have enough place for copying all SMS text
          // so cut SMS text to the (max_SMS_len-1)
          // (max_SMS_len-1) because we need 1 position for the 0x00 as finish 
          // string character
          memcpy(SMS_text, (char *)(p_char), (max_SMS_len-1));
          SMS_text[max_SMS_len] = 0; // finish string
        }
      }
      break;
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/**********************************************************
Method reads SMS from specified memory(SIM) position and
makes authorization - it means SMS phone number is compared
with specified SIM phonebook position(s) and in case numbers
match GETSMS_AUTH_SMS is returned, otherwise GETSMS_NOT_AUTH_SMS
is returned

position:     SMS position to be read <1..20>
phone_number: a pointer where the tel. number string of received SMS will be placed
              so the space for the phone number string must be reserved - see example
SMS_text  :   a pointer where SMS text will be placed
max_SMS_len:  maximum length of SMS text excluding terminating 0x00 character

first_authorized_pos: initial SIM phonebook position where the authorization process
                      starts
last_authorized_pos:  last SIM phonebook position where the authorization process
                      finishes

                      Note(important):
                      ================
                      In case first_authorized_pos=0 and also last_authorized_pos=0
                      the received SMS phone number is NOT authorized at all, so every
                      SMS is considered as authorized (GETSMS_AUTH_SMS is returned)
              
return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0

        OK ret val:
        -----------
        GETSMS_NO_SMS           - no SMS was found at the specified position
        GETSMS_NOT_AUTH_SMS     - NOT authorized SMS found at the specified position
        GETSMS_AUTH_SMS         - authorized SMS found at the specified position


an example of usage:
        GSM gsm;
        char phone_num[20]; // array for the phone number string
        char sms_text[100]; // array for the SMS text string

        // authorize SMS with SIM phonebook positions 1..3
        if (GETSMS_AUTH_SMS == gsm.GetAuthorizedSMS(1, phone_num, sms_text, 100, 1, 3)) {
          // new authorized SMS was detected at the SMS position 1
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG SMS phone number: ", 0);
            gsm.DebugPrint(phone_num, 0);
            gsm.DebugPrint("\r\n          SMS text: ", 0);
            gsm.DebugPrint(sms_text, 1);
          #endif
        }

        // don't authorize SMS with SIM phonebook at all
        if (GETSMS_AUTH_SMS == gsm.GetAuthorizedSMS(1, phone_num, sms_text, 100, 0, 0)) {
          // new SMS was detected at the SMS position 1
          // because authorization was not required
          // SMS is considered authorized
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG SMS phone number: ", 0);
            gsm.DebugPrint(phone_num, 0);
            gsm.DebugPrint("\r\n          SMS text: ", 0);
            gsm.DebugPrint(sms_text, 1);
          #endif
        }
**********************************************************/
char GSM::GetAuthorizedSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len,
                           byte first_authorized_pos, byte last_authorized_pos)
{
  char ret_val = -1;
  byte i;

#ifdef DEBUG_PRINT
    DebugPrint("DEBUG GetAuthorizedSMS\r\n", 0);
    DebugPrint("      #1: ", 0);
    DebugPrint(position, 0);
    DebugPrint("      #5: ", 0);
    DebugPrint(first_authorized_pos, 0);
    DebugPrint("      #6: ", 0);
    DebugPrint(last_authorized_pos, 1);
#endif  

  ret_val = GetSMS(position, phone_number, SMS_text, max_SMS_len);
  if (ret_val < 0) {
    // here is ERROR return code => finish
    // -----------------------------------
  }
  else if (ret_val == GETSMS_NO_SMS) {
    // no SMS detected => finish
    // -------------------------
  }
  else if (ret_val == GETSMS_READ_SMS) {
    // now SMS can has only READ attribute because we have already read
    // this SMS at least once by the previous function GetSMS()
    //
    // new READ SMS was detected on the specified SMS position =>
    // make authorization now
    // ---------------------------------------------------------
    if ((first_authorized_pos == 0) && (last_authorized_pos == 0)) {
      // authorization is not required => it means authorization is OK
      // -------------------------------------------------------------
      ret_val = GETSMS_AUTH_SMS;
    }
    else {
      ret_val = GETSMS_NOT_AUTH_SMS;  // authorization not valid yet
      for (i = first_authorized_pos; i <= last_authorized_pos; i++) {
        if (ComparePhoneNumber(i, phone_number)) {
          // phone numbers are identical
          // authorization is OK
          // ---------------------------
          ret_val = GETSMS_AUTH_SMS;
          break;  // and finish authorization
        }
      }
    }
  }
  return (ret_val);
}


/**********************************************************
Method deletes SMS from the specified SMS position

position:     SMS position <1..20>

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0

        OK ret val:
        -----------
        0 - SMS was not deleted
        1 - SMS was deleted
**********************************************************/
char GSM::DeleteSMS(byte position) 
{
  char ret_val = -1;

  if (position == 0) return (-3);
  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // not deleted yet
  
  //send "AT+CMGD=XY" - where XY = position
  Serial.print(F("AT+CMGD="));
  Serial.print((int)position);  
  Serial.print('\r');


  // 5000 msec. for initial comm tmout
  // 20 msec. for inter character timeout
  switch (at.WaitResp(5000, 50, F("OK"))) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      ret_val = -2;
      break;

    case RX_FINISHED_STR_RECV:
      // OK was received => SMS deleted
      ret_val = 1;
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // other response: e.g. ERROR => SMS was not deleted
      ret_val = 0; 
      break;
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}


/**********************************************************
Method reads phone number string from specified SIM position

position:     SMS position <1..20>

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0
        phone_number is empty string

        OK ret val:
        -----------
        0 - there is no phone number on the position
        1 - phone number was found
        phone_number is filled by the phone number string finished by 0x00
                     so it is necessary to define string with at least
                     15 bytes(including also 0x00 termination character)

an example of usage:
        GSM gsm;
        char phone_num[20]; // array for the phone number string

        if (1 == gsm.GetPhoneNumber(1, phone_num)) {
          // valid phone number on SIM pos. #1 
          // phone number string is copied to the phone_num array
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG phone number: ", 0);
            gsm.DebugPrint(phone_num, 1);
          #endif
        }
        else {
          // there is not valid phone number on the SIM pos.#1
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG there is no phone number", 1);
          #endif
        }
**********************************************************/
char GSM::GetPhoneNumber(byte position, char *phone_number)
{
  char ret_val = -1;

  char *p_char; 
  char *p_char1;

  if (position == 0) return (-3);
  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // not found yet
  phone_number[0] = 0; // phone number not found yet => empty string
  
  //send "AT+CPBR=XY" - where XY = position
  Serial.print(F("AT+CPBR="));
  Serial.print((int)position);  
  Serial.print('\r');

  // 5000 msec. for initial comm tmout
  // 50 msec. for inter character timeout
  switch (at.WaitResp(5000, 50, F("+CPBR"))) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      ret_val = -2;
      break;

    case RX_FINISHED_STR_RECV:
      // response in case valid phone number stored:
      // <CR><LF>+CPBR: <index>,<number>,<type>,<text><CR><LF>
      // <CR><LF>OK<CR><LF>

      // response in case there is not phone number:
      // <CR><LF>OK<CR><LF>
      p_char = strchr((char *)(at.comm_buf),'"');
      if (p_char != NULL) {
        p_char++;       // we are on the first phone number character
        // find out '"' as finish character of phone number string
        p_char1 = strchr((char *)(p_char),'"');
        if (p_char1 != NULL) {
          *p_char1 = 0; // end of string
        }
        // extract phone number string
        strcpy(phone_number, (char *)(p_char));
        // output value = we have found out phone number string
        ret_val = 1;
      }
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // only OK or ERROR => no phone number
      ret_val = 0; 
      break;
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/**********************************************************
Method writes phone number string to the specified SIM position

position:     SMS position <1..20>
phone_number: phone number string for the writing

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0

        OK ret val:
        -----------
        0 - phone number was not written
        1 - phone number was written
**********************************************************/
char GSM::WritePhoneNumber(byte position, char *phone_number)
{
  char ret_val = -1;

  if (position == 0) return (-3);
  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // phone number was not written yet

  //send: AT+CPBW=XY,"00420123456789"
  // where XY = position,
  //       "00420123456789" = phone number string
  Serial.print(F("AT+CPBW="));
  Serial.print((int)position);
  Serial.print(F(",\""));
  Serial.print(phone_number);
  Serial.println(F("\""));

  switch (at.WaitResp(5000, 50, F("OK"))) {
    case RX_FINISHED_STR_RECV: // response is OK = has been written
      ret_val = 1;
      break;

    case RX_TMOUT_ERR: // response was not received in specific time
    case RX_FINISHED_STR_NOT_RECV: // other response: e.g. ERROR
      break;
  }

  at.SetCommLineStatus(CLS_FREE);

#ifdef DEBUG_PRINT
  if (ret_val == 1) {
    Serial.println("DEBUG: Write phone number success");
  }
  else {
    Serial.print("DEBUG: Write phone number failed with ");
    Serial.println(ret_val);
  }
#endif

  return (ret_val);
}


/**********************************************************
Method del phone number from the specified SIM position

position:     SMS position <1..20>

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0

        OK ret val:
        -----------
        0 - phone number was not deleted
        1 - phone number was deleted
**********************************************************/
char GSM::DelPhoneNumber(byte position)
{
  char ret_val = -1;

  if (position == 0) return (-3);
  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);
  ret_val = 0; // phone number was not written yet
  
  //send: AT+CPBW=XY
  // where XY = position
  Serial.print(F("AT+CPBW="));
  Serial.print((int)position);  
  Serial.print('\r');

  // 5000 msec. for initial comm tmout
  // 50 msec. for inter character timeout
  switch (at.WaitResp(5000, 50, F("OK"))) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      break;

    case RX_FINISHED_STR_RECV:
      // response is OK = has been written
      ret_val = 1;
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // other response: e.g. ERROR
      break;
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}





/**********************************************************
Function compares specified phone number string 
with phone number stored at the specified SIM position

position:       SMS position <1..20>
phone_number:   phone number string which should be compare

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0

        OK ret val:
        -----------
        0 - phone numbers are different
        1 - phone numbers are the same


an example of usage:
        if (1 == gsm.ComparePhoneNumber(1, "123456789")) {
          // the phone num. "123456789" is stored on the SIM pos. #1
          // phone number string is copied to the phone_num array
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG phone numbers are the same", 1);
          #endif
        }
        else {
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG phone numbers are different", 1);
          #endif
        }
**********************************************************/
char GSM::ComparePhoneNumber(byte position, char *phone_number)
{
  char ret_val = -1;
  char sim_phone_number[20];

#ifdef DEBUG_PRINT
    DebugPrint("DEBUG ComparePhoneNumber\r\n", 0);
    DebugPrint("      #1: ", 0);
    DebugPrint(position, 0);
    DebugPrint("      #2: ", 0);
    DebugPrint(phone_number, 1);
#endif


  ret_val = 0; // numbers are not the same so far
  if (position == 0) return (-3);
  if (1 == GetPhoneNumber(position, sim_phone_number)) {
    // there is a valid number at the spec. SIM position
    // -------------------------------------------------
    if (0 == strcmp(phone_number, sim_phone_number)) {
      // phone numbers are the same
      // --------------------------
#ifdef DEBUG_PRINT
    DebugPrint("DEBUG ComparePhoneNumber: Phone numbers are the same", 1);
#endif
      ret_val = 1;
    }
  }
  return (ret_val);
}

/**********************************************************
NEW TDGINO FUNCTION
***********************************************************/

/**********************************************************
Method gets date and time

date_str:  pointer to the date eime text string


return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - GSM module has answered "ERROR" string

        OK ret val:
        -----------
        0 - SMS was not sent
        1 - SMS was sent


an example of usage:
        GSM gsm;
        gsm.GetDateTime(date_time);
**********************************************************/
char GSM::GetDateTime(char *date_time)
{ 
  char ret_val = -1;
  char *p_char; 
  char *p_char1;

  if (CLS_FREE != at.GetCommLineStatus()) return (ret_val);
  at.SetCommLineStatus(CLS_ATCMD);

  date_time[0] = 0;  // end of string for now
  ret_val = GETSMS_NO_SMS; // still no SMS
  
  //send "AT+CCLK?" to request date and time
  Serial.print(F("AT+CCLK?\r")); 

  // 5000 msec. for initial comm tmout
  // 100 msec. for inter character tmout
  switch (at.WaitResp(5000, 100, F("+CCLK"))) {
    case RX_TMOUT_ERR:
      // response was not received in specific time
      ret_val = -2;
      break;

    case RX_FINISHED_STR_NOT_RECV:
      // OK was received => there is NO SMS stored in this position
      if(at.IsStringReceived(F("OK"))) {
        // there is only response <CR><LF>OK<CR><LF> 
        // => there is NO SMS
        ret_val = GETSMS_NO_SMS;
      }
      else if(at.IsStringReceived(F("ERROR"))) {
        // error should not be here but for sure
        ret_val = GETSMS_NO_SMS;
      }
      break;

    case RX_FINISHED_STR_RECV:
      ret_val = GETSMS_READ_SMS;
      // Extract date time string 
      p_char = strchr((char *)(at.comm_buf),'"');
      p_char1 = p_char+1; // we are on the first date_time character
      p_char = strchr((char *)(p_char1),'"');
      if (p_char != NULL) {
        *p_char = 0; // end of string
        strcpy(date_time, (char *)(p_char1));
      }
      break;
  }

  at.SetCommLineStatus(CLS_FREE);
  return (ret_val);
}

/*********************************************************
Function to enable or disable echo
Echo(1)   enable echo modems
Echo(0)   disable echo mode
**********************************************************/
void GSM::Echo(byte state)
{
  if (state == 0 or state == 1) {
    at.SetCommLineStatus(CLS_ATCMD);
    Serial.print(F("ATE"));
    Serial.print((int)state);
    Serial.println();
    delay(500);
    at.SetCommLineStatus(CLS_FREE);
  }
}

void GSM::SetupGPRS() {
  // 2 degrees APN is internet, no user, no password
  at.SendATCmdWaitResp(F("AT+SAPBR=3,1,\"APN\",\"internet\""), 900, 500, F("OK"), 2);
  /* at.SendATCmdWaitResp(F("AT+SAPBR=3,1,\"USER\",\"yourUser\""), 900, 500, F("OK"), 2); */
  /* at.SendATCmdWaitResp(F("AT+SAPBR=3,1,\"PWD\",\"yourPwd\""), 900, 500, F("OK"), 2); */
  at.SendATCmdWaitResp(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""), 900, 500, F("OK"), 2);
}


byte GSM::HttpGet(const char *url, char *result) {
  return HttpOperation(F("AT+HTTPACTION=0"), F("+HTTPACTION:0,200"), url, result);
}

byte GSM::HttpPost(const char *urlp, char *result) {
  return HttpOperation(F("AT+HTTPACTION=1"), F("+HTTPACTION:1,200"), urlp, result);
}

byte GSM::HttpOperation(const __FlashStringHelper *op, const __FlashStringHelper *respcode, const char *url, char *result) {
  result[0] = 0x00;
  byte res_code = HTTP_FAIL;

  // check if registered?!
  at.SendATCmdWaitResp(F("AT+SAPBR=2,1"), 900, 900, F("OK"), 5); // query bearer
  //+SAPBR: 1,3,"0.0.0.0" --> closed
  //+SAPBR: 1,1,"100.70.120.92" --> open
  if (!at.IsStringReceived(F("+SAPBR: 1,1"))) {
    at.SendATCmdWaitResp(F("AT+SAPBR=1,1"), 20000, 900, F("OK"), 5); // open bearer
  }

  at.SendATCmdWaitResp(F("AT+SAPBR=2,1"), 900, 900, F("OK"), 5); // query bearer
  if (at.IsStringReceived(F("+SAPBR: 1,1"))) {

      at.SendATCmdWaitResp(F("AT+HTTPINIT"), 900, 500, F("OK"), 5);
      at.SendATCmdWaitResp(F("AT+HTTPPARA=\"CID\",\"1\""), 900, 500, F("OK"), 5);

      Serial.print(F("AT+HTTPPARA=\"URL\",\""));
      Serial.print(url);
      Serial.println(F("\""));
      at.WaitResp(900, 500, F("OK"));

      // GET or POST (or HEAD)
      at.SendATCmdWaitResp(op, 1500, 500, F("OK"), 2);

      // Wait for +HTTPACTION:<op>,200,<bytes>
      if (RX_FINISHED_STR_RECV == at.WaitResp(20000, 500, respcode)) {
        // +HTTPACTION:0,200,5 --> get, ok, 5 bytes of data
        char *p_start;
        char *p_end;
        int length = 0;
        // Get bytes to read
        p_start = strchr((char *)(at.comm_buf), ':');
        p_start = strchr(p_start, ',');
        p_start = strchr(p_start, ',');
        if (p_start != NULL) {
          length = atoi(p_start+1);
        }

        // Read response
        Serial.print(F("AT+HTTPREAD=0,"));
        Serial.println(length);

        if (RX_FINISHED_STR_RECV == at.WaitResp(1500, 500, F("OK"))) {
          // <CR><LF>+HTTPREAD:5<CR><LF>DATAHERE<CR><LF>OK

          p_start = strchr((char *)(at.comm_buf), ':');
          p_start = strchr((char *)(p_start), 0x0d);
          p_start = p_start + 2;
          p_end = strchr((char *)(p_start), 0x0d);
          *p_end = 0;
          strcpy(result, (char *)(p_start));

          res_code = HTTP_OK;
        }
      }

// got OK +HTTPACTION:0,601,0  --> get, network error, no data
// +HTTPACTION:0,200,5 --> get, ok, 5 bytes of data
//+HTTPREAD:5

      // stop
      at.SendATCmdWaitResp(F("AT+HTTPTERM"), 900, 500, F("OK"), 5);
    }

    //at.SendATCmdWaitResp(F("AT+SAPBR=0,1"), 900, 500, F("OK"), 5); // close bearer
    
    return res_code;
}


/*********************************************************
GPS Section
TODO Break this file into three parts, (1) Common serial support, (2) GSM/GPRS, and (3) GPS.
*********************************************************/

void GSM::InitGPS(){
  Ready();
  at.SendATCmdWaitResp(F("AT+CGPSIPR=9600"), 1200, 100, F("OK"), 5); // set the baud rate
  at.SendATCmdWaitResp(F("AT+CGPSOUT=0"), 1200, 100, F("OK"), 5); // nmea output off
  at.SendATCmdWaitResp(F("AT+CGPSPWR=1"), 1200, 100, F("OK"), 5); // turn on GPS power supply
  at.SendATCmdWaitResp(F("AT+CGPSRST=0"), 1200, 100, F("OK"), 5); // cold reset GPS (just do this once)
  at.SendATCmdWaitResp(F("AT+CGPSPWR=0"), 2000, 100, F("OK"), 5); // turn off GPS power supply
  Ready();
}

void GSM::StartGPS(){
  Ready();
  at.SendATCmdWaitResp(F("AT+CGPSPWR=1"), 900, 100, F("OK"), 5); // turn on GPS power supply
  // TODO is the reset required?
  at.SendATCmdWaitResp(F("AT+CGPSRST=1"), 900, 100, F("OK"), 5); // reset GPS in autonomy mode
}

void GSM::StopGPS(){
  Ready();
  at.SendATCmdWaitResp(F("AT+CGPSPWR=0"), 900, 100, F("OK"), 5); // turn off
}

byte GSM::CheckLocation(position_t& loc) {

  byte retcode = GEN_FAILURE;
  char latitude[15];
  char longitude[15];

  if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT+CGPSSTATUS?"), 900, 50, F("OK"), 2)) {
    if (at.IsStringReceived(F("+CGPSSTATUS: Location 2D Fix")) ||
        at.IsStringReceived(F("+CGPSSTATUS: Location 3D Fix"))) {

      if (AT_RESP_OK == at.SendATCmdWaitResp(F("AT+CGPSINF=0"), 900, 50, F("OK"), 2)) {
        // Have location in form: 
        // <CR><LF>0,17446.647913,-4117.068521,0.082149,20131025231125.000,534,5,0.000000,0.000000<CR><LF>OK
        // mode, long, lat, alt, utc time, ttff, num, speed, course -- where 'ttff' = time to first fix (seconds)

        strtok((char *)(at.comm_buf), ",");
        strcpy(longitude,strtok(NULL, ",")); // Gets longitude
        strcpy(latitude,strtok(NULL, ",")); // Gets latitude

        loc.lat = LocInDegrees(latitude);
        loc.lon = LocInDegrees(longitude);

        retcode = GEN_SUCCESS;
      }
    }
  }
  return retcode;
}

/**
 * E.g.
 * Lat -4117.015786 --> -41.283596
 * Lng 17446.508384 --> 174.775146
 */
double GSM::LocInDegrees(char* input) {
  char *p;
  float deg;
  float minutes;
  boolean neg = false;

  char aux[10];

  p = input;
  if (input[0] == '-') {
    neg = true;
    p = p+1;
  }

  deg = atof(strcpy(aux, strtok(p, ".")));
  minutes = atof(strcpy(aux, strtok(NULL, '\0')));
  minutes /= 1000000;

  if (deg < 100) {
    minutes += deg;
    deg = 0;
  }
  else {
    minutes += int(deg) % 100;
    deg = int(deg) / 100;
  }

  deg = deg + minutes/60;

  if (neg == true) {
    deg *= -1.0;
  }

  neg = false;

  if ( deg < 0 ) {
    neg = true;
    deg *= -1;
  }

  int int_part[10];
  int f;
  long num_part = (long)deg;
  float decimal_part = (deg - (int)deg);
  int size = 0;

  while (1) {
    size = size + 1;
    f = num_part % 10;
    num_part = num_part / 10;
    int_part[size-1] = f;
    if (num_part == 0) {
      break;
    }
  }

  int index=0;
  if (neg) {
    index++;
    input[0] = '-';
  }
  for (int i = size-1; i >= 0; i--) {
    input[index] = int_part[i] + '0'; 
    index++;
  }

  input[index]='.';
  index++;

  for (int i = 1; i <= 6 ; i++) {
    decimal_part = decimal_part * 10;
    f = (long)decimal_part;
    decimal_part = decimal_part - f;
    input[index] = char(f) + 48;
    index++;
  }
  input[index] = '\0';
  return atof(input);
}

/**
 * http://en.wikipedia.org/wiki/Law_of_haversines
 */
double GSM::EarthRadiansBetween(const position_t& from, const position_t& to) {
  double lat_radians = (from.lat - to.lat) * DEG_TO_RAD;
  double lon_radians = (from.lon - to.lon) * DEG_TO_RAD;
  double lat_haversine = pow(sin(lat_radians * 0.5), 2);
  double lon_haversine = pow(sin(lon_radians * 0.5), 2);
  return 2.0 * asin(sqrt(lat_haversine + (cos(from.lat * DEG_TO_RAD) * cos(to.lat * DEG_TO_RAD) * lon_haversine)));
}

double GSM::DistanceBetween(const position_t& from, const position_t& to) {
  return EarthRadiansBetween(from, to) * EARTH_MEAN_RADIUS;
}

/*
sqrl_at.cpp
Copyright (c) www.hwkitchen.com and contributors @jgarland79, @harlequin-tech, @scott-abernethy.
This file is part of sqrl, the squirt-library. Please refer to the NOTICE.txt file for license details.
*/

#include "sqrl_at.h"
#include <avr/pgmspace.h>

extern "C" {
  #include <string.h>
}

/*
 Work around a bug with PROGMEM and PSTR where the compiler always
 generates warnings.
 */
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];}))


AtComms::AtComms(void) {
}

/**********************************************************
  Initializes receiving process

  start_comm_tmout    - maximum waiting time for receiving the first response
                        character (in msec.)
  max_interchar_tmout - maximum tmout between incoming characters 
                        in msec.
  if there is no other incoming character longer then specified
  tmout(in msec) receiving process is considered as finished
**********************************************************/
void AtComms::RxInit(uint16_t start_comm_tmout, uint16_t max_interchar_tmout) {
  rx_state = RX_NOT_STARTED;
  req_reception_tmout = start_comm_tmout;
  req_interchar_tmout = max_interchar_tmout;
  prev_time = millis();
  comm_buf[0] = 0x00; // end of string
  p_comm_buf = &comm_buf[0];
  comm_buf_len = 0;
  Serial.flush(); // erase rx circular buffer
}

void AtComms::ReadBuffer(char *into, int offset, int length) {
  byte *p_start;
  byte *p_end;

  p_start = &comm_buf[0];
  p_start = p_start + offset;
  p_end = p_start + length;
  *p_end = 0;
  strcpy(into, (char *)(p_start));
}

/**********************************************************
Method checks if receiving process is finished or not.
Rx process is finished if defined inter-character tmout is reached

returns:
        RX_NOT_FINISHED = 0,// not finished yet
        RX_FINISHED,        // finished - inter-character tmout occurred
        RX_TMOUT_ERR,       // initial communication tmout occurred
**********************************************************/
byte AtComms::IsRxFinished(void)
{
  byte num_of_bytes;
  byte ret_val = RX_NOT_FINISHED;  // default not finished

  // Rx state machine

  if (rx_state == RX_NOT_STARTED) { // Reception is not started yet - check tmout
    if (!Serial.available()) { // still no character received => check timeout
      if ((unsigned long)(millis() - prev_time) >= req_reception_tmout) {
        comm_buf[comm_buf_len] = 0x00;
        ret_val = RX_TMOUT_ERR;
      }
    }
    else {
      // at least one character received => so init inter-character
      // counting process again and go to the next state
      prev_time = millis(); // init tmout for inter-character space
      rx_state = RX_ALREADY_STARTED;
    }
  }

  if (rx_state == RX_ALREADY_STARTED) {
    // Reception already started
    // check new received bytes
    // only in case we have place in the buffer
    num_of_bytes = Serial.available();
    // if there are some received bytes postpone the timeout
    if (num_of_bytes) prev_time = millis();

    // read all received bytes
    while (num_of_bytes) {
      num_of_bytes--;
      if (comm_buf_len < COMM_BUF_LEN) {
        // we have still place in the GSM internal comm. buffer =>
        // move available bytes from circular buffer
        // to the rx buffer
        *p_comm_buf = Serial.read();

        p_comm_buf++;
        comm_buf_len++;
        comm_buf[comm_buf_len] = 0x00;  // and finish currently received characters
                                        // so after each character we have
                                        // valid string finished by the 0x00
      }
      else {
        // comm buffer is full, other incoming characters will be discarded
        // but despite of we have no place for other characters we still must to wait until
        // inter-character tmout is reached so just readout character from circular
        // RS232 buffer to find out when communication id finished
        // (no more characters are received in inter-char timeout)
        Serial.read();
      }
    }

    // finally check the inter-character timeout
    if ((unsigned long)(millis() - prev_time) >= req_interchar_tmout) {
      // timeout between received character was reached reception is finished
      comm_buf[comm_buf_len] = 0x00;  // for sure finish string again
                                      // but it is not necessary
      ret_val = RX_FINISHED;
    }
  }

  return (ret_val);
}
/**********************************************************
Method checks received bytes

compare_string - pointer to the string which should be find

return: 0 - string was NOT received
        1 - string was received
**********************************************************/
byte AtComms::IsStringReceived(const __FlashStringHelper *compare_string)
{
  if (comm_buf_len) {
  
		/*#ifdef DEBUG_GSMRX
			Serial.println("DEBUG: Compare the string: ");
			for (int i=0; i<comm_buf_len; i++){
				Serial.write(byte(comm_buf[i]));	
			}
			
			Serial.println("DEBUG: with the string: ");
			Serial.print(compare_string);	
			Serial.println(" ");
		#endif*/
	
	if (strstr_P((char *)comm_buf, (const prog_char *)compare_string) != NULL) {
	    return 1;
	} 
    }

    return 0;
}
/**********************************************************
Method waits for response

      start_comm_tmout    - maximum waiting time for receiving the first response
                            character (in msec.)
      max_interchar_tmout - maximum tmout between incoming characters 
                            in msec.  
return: 
      RX_FINISHED         finished, some character was received

      RX_TMOUT_ERR        finished, no character received 
                          initial communication tmout occurred
**********************************************************/
byte AtComms::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout)
{
  byte status;

  RxInit(start_comm_tmout, max_interchar_tmout); 
  
  // wait until response is not finished
  do {
    status = IsRxFinished();
  } while (status == RX_NOT_FINISHED);

#ifdef DEBUG_GSMRX
  if (status == RX_FINISHED){
    for (int i=0; i<comm_buf_len; i++){
      char c = comm_buf[i];
      Serial.print(c);
    }
    Serial.println();
  }
#endif

  return (status);
}
/**********************************************************
Method waits for response with specific response string
    
      start_comm_tmout    - maximum waiting time for receiving the first response
                            character (in msec.)
      max_interchar_tmout - maximum tmout between incoming characters 
                            in msec.  
      expected_resp_string - expected string
return: 
      RX_FINISHED_STR_RECV,     finished and expected string received
      RX_FINISHED_STR_NOT_RECV  finished, but expected string not received
      RX_TMOUT_ERR              finished, no character received 
                                initial communication tmout occurred
**********************************************************/
byte AtComms::WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout, 
		const __FlashStringHelper *expected_resp_string)
{
  // TODO unify with other WaitResp above.
  byte status;
  byte ret_val;

  RxInit(start_comm_tmout, max_interchar_tmout); 
  // wait until response is not finished
  do {
    status = IsRxFinished();
  } while (status == RX_NOT_FINISHED);

#ifdef DEBUG_GSMRX
  if (status == RX_FINISHED){
    for (int i=0; i<comm_buf_len; i++){
      char c = comm_buf[i];
      Serial.print(c);
    }
    Serial.println();
  }
#endif

  if (status == RX_FINISHED) {
    // something was received but what was received?

    if(IsStringReceived(expected_resp_string)) {
      // expected string was received
      // ----------------------------
      ret_val = RX_FINISHED_STR_RECV;      
    }
    else ret_val = RX_FINISHED_STR_NOT_RECV;
  }
  else {
    // nothing was received
    ret_val = RX_TMOUT_ERR;
  }
  return (ret_val);
}

//---
eReq AtComms::SendCmd(
    const __FlashStringHelper *AT_cmd_string,
    uint16_t start_comm_tmout,
    uint16_t max_interchar_tmout,
    byte no_of_attempts) {

  // check existing state, is comm line free? has been done externally.

  req_cmd = AT_cmd_string;
  req_reception_tmout = start_comm_tmout;
  req_interchar_tmout = max_interchar_tmout;
  req_attempts = no_of_attempts;

  return SendCmdAttempt();
}

//---
eReq AtComms::SendCmdAttempt() {
  eReq rcode = REQ_FAIL;
  req_attempts--;
  if (req_attempts > 0) {
    Serial.println(req_cmd);
    RxInit(req_reception_tmout, req_interchar_tmout);
    rcode = REQ_OK;
  }
  return rcode;
}

//---
eResp AtComms::CheckResp(const __FlashStringHelper *response_string) {
  eResp rcode = RESP_WAIT;
  byte rx_state = IsRxFinished();

  if (rx_state == RX_FINISHED && 
      IsStringReceived(response_string)) {
    rcode = RESP_OK;
  }
  else if (rx_state != RX_NOT_FINISHED && 
      SendCmdAttempt() != REQ_OK) {
    rcode = RESP_FAIL;
  }

  return rcode;
}

/**********************************************************
Method sends AT command and waits for response

return: 
      AT_RESP_ERR_NO_RESP = -1,   // no response received
      AT_RESP_ERR_DIF_RESP = 0,   // response_string is different from the response
      AT_RESP_OK = 1,             // response_string was included in the response
**********************************************************/
char AtComms::SendATCmdWaitResp(
    const __FlashStringHelper *AT_cmd_string,
    uint16_t start_comm_tmout,
    uint16_t max_interchar_tmout,
    const __FlashStringHelper *response_string,
    byte no_of_attempts)
{
  byte status;
  char ret_val = AT_RESP_ERR_NO_RESP;
  byte i;

  for (i = 0; i < no_of_attempts; i++) {
    // delay 500 msec. before sending next repeated AT command 
    // so if we have no_of_attempts=1 tmout will not occurred
    if (i > 0) delay(500); 

    Serial.println(AT_cmd_string);
    status = WaitResp(start_comm_tmout, max_interchar_tmout); 
    if (status == RX_FINISHED) {
      // something was received but what was received?
      // ---------------------------------------------
      if(IsStringReceived(response_string)) {
        ret_val = AT_RESP_OK;      
        break;  // response is OK => finish
      }
      else ret_val = AT_RESP_ERR_DIF_RESP;
    }
    else {
      // nothing was received
      // --------------------
      ret_val = AT_RESP_ERR_NO_RESP;
    }
    
  }

  return (ret_val);
}

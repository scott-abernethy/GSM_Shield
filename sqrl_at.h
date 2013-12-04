/*
sqrl_at.h
Copyright (c) www.hwkitchen.com and contributors @jgarland79, @harlequin-tech, @scott-abernethy.
This file is part of sqrl, the squirt-library. Please refer to the NOTICE.txt file for license details.
*/

#ifndef __SQRL_AT_H
#define __SQRL_AT_H

#include "Arduino.h"
#include <avr/pgmspace.h>

// if defined - debug print is enabled with possibility to print out 
// debug texts to the terminal program
#define DEBUG_PRINT

// if defined - debug print is enabled with possibility to print out 
// the data recived from gsm module
#define DEBUG_GSMRX

// some constants for the IsRxFinished() method
#define RX_NOT_STARTED      0
#define RX_ALREADY_STARTED  1

// length for the internal communication buffer
#define COMM_BUF_LEN        200

enum rx_state_enum 
{
  RX_NOT_FINISHED = 0,      // not finished yet
  RX_FINISHED,              // finished, some character was received
  RX_FINISHED_STR_RECV,     // finished and expected string received
  RX_FINISHED_STR_NOT_RECV, // finished, but expected string not received
  RX_TMOUT_ERR             // finished, no character received 
};

enum at_resp_enum 
{
  AT_RESP_ERR_NO_RESP = -1,   // nothing received
  AT_RESP_ERR_DIF_RESP = 0,   // response_string is different from the response
  AT_RESP_OK = 1,             // response_string was included in the response
};

enum eReq { REQ_FAIL, REQ_OK };
enum eResp { RESP_WAIT, RESP_FAIL, RESP_OK };

class AtComms {
  private:
    byte comm_line_status;

    byte rx_state;                  // internal state of rx state machine
    const __FlashStringHelper *req_cmd;
    unsigned long prev_time;        // previous time in msec.
    uint16_t req_reception_tmout;
    uint16_t req_interchar_tmout;
    byte req_attempts;

    void RxInit(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
    eReq SendCmdAttempt(void);

  public:
    AtComms(void);

    byte comm_buf[COMM_BUF_LEN+1];  // communication buffer +1 for 0x00 termination
    byte *p_comm_buf;               // pointer to the communication buffer
    byte comm_buf_len;              // num. of characters in the buffer

    // util
    inline void SetCommLineStatus(byte new_status) {comm_line_status = new_status;};
    inline byte GetCommLineStatus(void) {return comm_line_status;};
    void ReadBuffer(char *into, int offset, int length);
    byte IsRxFinished(void);
    byte IsStringReceived(const __FlashStringHelper *compare_string);

    // async
    eReq SendCmd(
        const __FlashStringHelper *AT_cmd_string,
        uint16_t start_comm_tmout,
        uint16_t max_interchar_tmout,
        byte no_of_attempts);
    eResp CheckResp(const __FlashStringHelper *response_string);

    // sync
    byte WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout);
    byte WaitResp(uint16_t start_comm_tmout, uint16_t max_interchar_tmout, const __FlashStringHelper *expected_resp_string);
    char SendATCmdWaitResp(
        const __FlashStringHelper *AT_cmd_string,
        uint16_t start_comm_tmout,
        uint16_t max_interchar_tmout,
        const __FlashStringHelper *response_string,
        byte no_of_attempts);
};

#endif

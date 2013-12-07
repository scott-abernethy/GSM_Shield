/*
GSM_Shield.h
Copyright (c) www.hwkitchen.com and contributors @jgarland79, @harlequin-tech, @scott-abernethy.
This file is part of sqrl, the squirt-library. Please refer to the NOTICE.txt file for license details.
*/

#ifndef __GSM_Shield
#define __GSM_Shield

#include "Arduino.h"
#include <avr/pgmspace.h>
#include "sqrl_at.h"

// if defined - SMSs are not send(are finished by the character 0x1b
// which causes that SMS are not send)
// by this way it is possible to develop program without paying for the SMSs 
//#define DEBUG_SMS_ENABLED

// pins definition
//#define GSM_ON              3 // connect GSM Module turn ON to pin 77 
//#define GSM_RESET           9 // connect GSM Module RESET to pin 35
//#define DTMF_OUTPUT_ENABLE  71 // connect DTMF Output Enable not used
#define DTMF_DATA_VALID     14 // connect DTMF Data Valid to pin 14
#define DTMF_DATA0          72 // connect DTMF Data0 to pin 72
#define DTMF_DATA1          73 // connect DTMF Data1 to pin 73
#define DTMF_DATA2          74 // connect DTMF Data2 to pin 74
#define DTMF_DATA3          75 // connect DTMF Data3 to pin 75

// some constants for the InitParam() method
#define PARAM_SET_0   0
#define PARAM_SET_1   1

// DTMF signal is NOT valid
//#define DTMF_NOT_VALID      0x10


// status bits definition
#define STATUS_NONE                 0
#define STATUS_INITIALIZED          1
#define STATUS_REGISTERED           2
#define STATUS_USER_BUTTON_ENABLE   4

#define DEG_TO_RAD 0.017453292519943295769236907684886 // or, pi div 180
#define EARTH_MEAN_RADIUS 6372797.560856 // metres

// return codes
#define GEN_FAILURE 0
#define GEN_SUCCESS 1

// SMS type 
// use by method IsSMSPresent()
enum sms_type_enum
{
  SMS_UNREAD,
  SMS_READ,
  SMS_ALL
};

enum comm_line_status_enum 
{
  // CLS like CommunicationLineStatus
  CLS_FREE,   // line is free - not used by the communication and can be used
  CLS_ATCMD,  // line is used by AT commands, includes also time for response
  CLS_DATA   // for the future - line is used in the CSD or GPRS communication  
};

enum ready_enum {
  READY_NO = 0,
  READY_YES
};

enum registration_ret_val_enum 
{
  REG_NOT_REGISTERED = 0,
  REG_REGISTERED,
  REG_NO_RESPONSE,
  REG_COMM_LINE_BUSY
};

enum call_ret_val_enum
{
  CALL_NONE = 0,
  CALL_INCOM_VOICE,
  CALL_ACTIVE_VOICE,
  CALL_INCOM_VOICE_AUTH,
  CALL_INCOM_VOICE_NOT_AUTH,
  CALL_INCOM_DATA_AUTH,
  CALL_INCOM_DATA_NOT_AUTH,
  CALL_ACTIVE_DATA,
  CALL_OTHERS,
  CALL_NO_RESPONSE,
  CALL_COMM_LINE_BUSY,
  CALL_OUT_VOICE
};


enum getsms_ret_val_enum
{
  GETSMS_NO_SMS   = 0,
  GETSMS_UNREAD_SMS,
  GETSMS_READ_SMS,
  GETSMS_OTHER_SMS,

  GETSMS_NOT_AUTH_SMS,
  GETSMS_AUTH_SMS
};

enum httpget_ret_val_enum {
  HTTP_OK = 0,
  HTTP_FAIL
};

struct position_t {
  double lat;
  double lon;
};

class GSM
{
  public:
    GSM(void);
    void InitSerLine();

    void ModeInit(void);
    void ModeGSM(void);
    void ModeGPS(void);
    void TurnOn(void);
    void InitParam (byte group);
    byte Ready(void);
    //void EnableDTMF(void);
    //byte GetDTMFSignal(void);
    byte GetICCID(char *id_string);
    void SetSpeaker(byte off_on);
    byte CheckRegistration(void); // must be called regularly
    byte IsRegistered(void);
    byte IsInitialized(void);
    byte CallStatus(void);
    byte CallStatusWithAuth(char *phone_number, byte &fav,
                            byte first_authorized_pos, byte last_authorized_pos);
    void PickUp(void);
    void HangUp(void);
    void Call(char *number_string);
    void Call(int sim_position);

    char SetSpeakerVolume(byte speaker_volume);
    char IncSpeakerVolume(void);
    char DecSpeakerVolume(void);

    char SendDTMFSignal(byte dtmf_tone);

    // User button methods
    inline byte IsUserButtonEnable(void) {return (module_status & STATUS_USER_BUTTON_ENABLE);};
    inline void DisableUserButton(void) {module_status &= ~STATUS_USER_BUTTON_ENABLE;};
    inline void EnableUserButton(void) {module_status |= STATUS_USER_BUTTON_ENABLE;};
    byte IsUserButtonPushed(void);  

    // SMS's methods 
    char SendSMS(char *number_str, char *message_str);
    char SendSMS(const __FlashStringHelper *number_str, char *message_str);
    char SendSMS(byte sim_phonebook_position, char *message_str);
    char IsSMSPresent(byte required_status);
    char GetSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len);
    char GetAuthorizedSMS(byte position, char *phone_number, char *SMS_text, byte max_SMS_len,
                          byte first_authorized_pos, byte last_authorized_pos);
    char DeleteSMS(byte position);

    // Phonebook's methods
    char GetPhoneNumber(byte position, char *phone_number);
    char WritePhoneNumber(byte position, char *phone_number);
    char DelPhoneNumber(byte position);
    char ComparePhoneNumber(byte position, char *phone_number);

    // Date time
    char GetDateTime(char *date_time);

    //echo
    void Echo(byte state);

    // data
    void SetupGPRS(void);
    byte HttpGet(const char *url, char *result);
    byte HttpPost(const char *urlp, char *result);

    // gps
    void InitGPS(void);
    void StartGPS(void);
    void StopGPS(void);
    byte CheckLocation(position_t& loc);
    double EarthRadiansBetween(const position_t& from, const position_t& to);
    double DistanceBetween(const position_t& from, const position_t& to);

#ifdef DEBUG_PRINT
    void DebugPrint(const char *string_to_print, byte last_debug_print);
    void DebugPrint(int number_to_print, byte last_debug_print);
#endif

  private:
    AtComms at;
    byte module_status; // global status - bit mask
    byte last_speaker_volume; // last value of speaker volume

    char InitSMSMemory(void);

    byte HttpOperation(const __FlashStringHelper *op, const __FlashStringHelper *respcode, const char *url, char *result);

    double LocInDegrees(char* input);

};
#endif

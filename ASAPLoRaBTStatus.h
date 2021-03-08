// ASAPLoRaBTStatus.h

#ifndef _ASAPLORABTSTATUS_h
#define _ASAPLORABTSTATUS_h

#include "Arduino.h"

typedef enum
{
    ASAPLoRaBTStatus_closed   = 0,  /* No Client is connected, either just initalized or disconnected */
    ASAPLoRaBTStatus_ready    = 1   /* Client is connected to BT */
} ASAPLoRaBTStatus_t;

#endif


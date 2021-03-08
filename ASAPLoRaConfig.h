#pragma once

#define LORA_MESSAGING_CONFIG "434052500,10,3,10,1,1,0,0,0,0,3000,8,20"

#define RTO_SMOOTHING_ALPHA 0.5
#define RTO_SMOOTHING_BETA_FROM 10 // 10 corresponds to 1.0 (x / 10.0 to convert random number to float)
#define RTO_SMOOTHING_BETA_TO 51 // non inclusive, so 21 corresponds to 2.0

#ifndef LORA_NODE_NAME
#define LORA_NODE_NAME "ASAP-LoRa-1"
#endif

#ifndef LORA_ADDRESS
#define LORA_ADDRESS "1000" //Starting at 1000 lower than FFFE
#endif

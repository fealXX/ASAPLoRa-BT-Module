#pragma once
#include "Arduino.h"
#include "ASAPLoRaMessage.h"
#include <HardwareSerial.h>

typedef enum
{
	LORA_IDLE = 0, /* LoRa Board is connected to ASAP MacLayerEngine via BT and Idle */
	LORA_AWAITING_COMMAND_RESULT = 1, /* We are waiting for the result of a command we just issued */
	LORA_SENDING_MESSAGE = 2, /* We are currently sending a message */
	LORA_AWAITING_MESSAGE_RESPONSE = 3, /* We are waiting for the ack of a message we just send */
	LORA_AWAITING_MESSAGE_COMMAND_RESULT = 4, /* We are waiting for the result of a command we just issued to send a message */
	LORA_CLOSED = 5 /* LoRa Board is not connected to MacLayerEngine */
} LoRaCommManagerStatus_t;


typedef enum
{
	LORA_MESSAGE_NORMAL = 0, /* Normal Message Type. We wait for acknowledgement. */
	LORA_MESSAGE_IMMEDIATE = 1 /* Fire and forget, i.e. ACK, DVDCR or DSCVR */
} LoRaCommManagerMessageType_t;

class LoRaCommManager
{
public:
	using MsgCallback = void (*)(String msg);
	using StatusCallback = void (*)(LoRaCommManagerStatus_t status);
	

	LoRaCommManager();
	void setupLoRaBoard(const char* LoRaAddress, const char* LoRaConfig);
	void open();
	void close();
	void enqueueMessage(ASAPLoRaMessage* msg);
	void handleMessages();
	void setMessageCallback(MsgCallback callback);
	void setStatusCallback(StatusCallback callback);
private:
	boolean waitingForResponse;
	char awaitedResponse[20]; //needs to fit IDEMP: + 10 Characters + \0
	MsgCallback msgCallback;
	StatusCallback statusCallback;
	unsigned long waitingStarttime;
	unsigned long retransmissionTimeout;
	unsigned long sendingStarttime;
	unsigned long sendTime;
	unsigned long smoothedRoundTripTime;
	unsigned long lastFrameReceiveTime;
	unsigned long lastWaitLog;
	unsigned long lastCommandTime;
	int retransmissionBackoffCycle;
	LoRaCommManagerStatus_t status;
	LoRaCommManagerMessageType_t msgType;

	void readHIMOBoard();

	void enqueueLoRaCommand(const char* cmd);
	void enqueueLoRaCommand(const char* cmd, const char* param);
	void enqueueLoRaCommand(const char* cmd, int param);
	void handleError(const char* err);

	void sendLoRaMessage(const char* address, const char* message, int size);
	void sendLoRaMessage(const char* address, const char* message);
	void sendLoRaBroadcast(const char* message, int size);
	void sendLoRaBroadcast(const char* message);

	void handleIncomingMessage(String message);
	void statusTransition(LoRaCommManagerStatus_t newStatus);
};

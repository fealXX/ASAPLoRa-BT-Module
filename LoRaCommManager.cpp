#include "Arduino.h"
#include "ASAPLoRaConfig.h"
#include "ASAPLoRaMessage.h"
#include <HardwareSerial.h>
#include <QueueArray.h>
#include <CircularBuffer.h>
#include "LoRaCommManager.h"

HardwareSerial _HIMO01P(1);


QueueArray <ASAPLoRaMessage*> MessageQueue;
QueueArray <char*> CommandQueue;
QueueArray <char*> ResponseQueue;
CircularBuffer<const char*, 10> idempBuffer;

LoRaCommManager::LoRaCommManager() {
	//RX: 16, TX: 17
	_HIMO01P.begin(115200, SERIAL_8N1, 16, 17);
	//Use a bigger buffer, so that multiple messages can be stored in there, during sending
	_HIMO01P.setRxBufferSize(2048);
	this->msgCallback = nullptr;
	this->statusCallback = nullptr;
	this->smoothedRoundTripTime = 0;
	this->sendingStarttime = 0;
	this->waitingStarttime = millis();
	this->lastFrameReceiveTime = millis();
	this->lastWaitLog = millis();
	this->lastCommandTime = millis();
	this->retransmissionTimeout = 0;
	this->retransmissionBackoffCycle = 1;
	strcpy(this->awaitedResponse, "");

	this->status = LORA_CLOSED;
	this->msgType = LORA_MESSAGE_NORMAL;

}

void LoRaCommManager::setupLoRaBoard(const char* LoRaAddress, const char* LoRaConfig) {
	//Try to exit TSP Mode 3 Times. AT OK if exited successfully, nothing if already exited
	_HIMO01P.println("+++"); //Exit TSP Mode
	delay(100);
	_HIMO01P.println("+++"); //Exit TSP Mode
	delay(100);
	_HIMO01P.println("+++"); //Exit TSP Mode
	delay(100);

	while (_HIMO01P.available()) {
		Serial.print("HIMO01P Setup Response:");
		Serial.println(_HIMO01P.readStringUntil('\n'));
	}

	this->enqueueLoRaCommand("AT+CFG=", LoRaConfig); //Setup messaging parameters
	this->enqueueLoRaCommand("AT+ADDREN=1"); // Enable Soft address filtering
	this->enqueueLoRaCommand("AT+ADDR=", LoRaAddress); // Setup my own hardcoded address
	this->enqueueLoRaCommand("AT+RX"); // set myself listening

	// Send a first Broadcast, since the himo behaves weirdly sometimes, 
	// where it only receives messages once it sent one itself
	/*ASAPLoRaMessage* msg = new ASAPLoRaMessage();
	msg->setAddress("FFFF");
	msg->setPayload("ASAPLoRaBTModule initialized");
	MessageQueue.enqueue(msg);
	Commented out for the tests, as it disturbs the running*/

	Serial.println("LoRa Board initialized");
}

void LoRaCommManager::close() {
	if (this->status == LORA_CLOSED)
		return; //Nothing to do

	if (this->status != LORA_IDLE)
		return; //only close if we're IDLE'ing

	Serial.print("LoRaCommManager::close() running on core ");
	Serial.println(xPortGetCoreID());

	//clear message and command buffer
	for (int i = 0;i < CommandQueue.count();i++)
		free(CommandQueue.dequeue());
	for (int i = 0;i < ResponseQueue.count();i++)
		free(ResponseQueue.dequeue());
	for (int i = 0;i < MessageQueue.count();i++)
		free(MessageQueue.dequeue());

	this->statusTransition(LORA_CLOSED);
}

void LoRaCommManager::open() {
	if (this->status == LORA_CLOSED) {
		this->statusTransition(LORA_IDLE);
	}
}

void LoRaCommManager::enqueueMessage(ASAPLoRaMessage* msg) {
	MessageQueue.enqueue(msg);
}

void LoRaCommManager::readHIMOBoard() {
	Serial.println("Readline from HIMO Module");
	String loraResponse = _HIMO01P.readStringUntil('\n');
	loraResponse.trim(); //remove trailing \r

	Serial.println("LoRa Board -> ESP32 said:");
	Serial.println(loraResponse);

	String responseType = loraResponse.substring(0, 2);
	Serial.print("Reponse Type is: ");
	Serial.println(responseType);

	if (responseType == "LR" && this->status != LORA_CLOSED) {
		if (this->status == LORA_IDLE ||
			(this->status == LORA_AWAITING_MESSAGE_RESPONSE && loraResponse.indexOf("IDEMP:") >= 0)
			) {
			// if we're Idle or waiting for response (and this is one), just handle it immediately
			this->handleIncomingMessage(loraResponse);
			return;
		}
		// No point in waiting anymore, we received something not fitting at the moment. 
		this->retransmissionTimeout = 0;
		// but put it into the message queue
		ResponseQueue.enqueue(strdup(loraResponse.c_str()));
		return;
	}

	if (this->status == LORA_AWAITING_COMMAND_RESULT && responseType == "AT") {
		//That is a cmd response, investigate..
		String responseData = loraResponse.substring(3);
		if (responseData == "OK") {
			//well, ok then.
			this->statusTransition(LORA_IDLE);
			free(CommandQueue.dequeue()); //free cmd from Queue so we don't end up sending it over and over
			return;
		}
		//This was unexpected.. try to do something with it.
		this->handleError(loraResponse.c_str()); //Do something.
		return;
	}

	if (this->status == LORA_AWAITING_MESSAGE_COMMAND_RESULT && responseType == "AT") {
		//That is a cmd response to a message we're trying to send, investigate..
		String responseData = loraResponse.substring(3);
		if (responseData == "OK") {
			//well, ok then, lets send the rest of the commands for our message
			this->statusTransition(LORA_SENDING_MESSAGE);
			free(CommandQueue.dequeue()); //free successfull cmd from Queue so we don't end up sending it over and over
			return;
		}
		if (responseData == "SENDING") {
			//keep waiting for AT,SENDED
			this->statusTransition(LORA_AWAITING_MESSAGE_COMMAND_RESULT);
			return;
		}
		if (responseData == "SENDED") {
			//Sending was completed.
			//Update Status, if normal msg, wait - else idle
			if (this->msgType == LORA_MESSAGE_NORMAL) {
				this->statusTransition(LORA_AWAITING_MESSAGE_RESPONSE);
				this->waitingStarttime = millis();

				//if our srtt is not set yet, assume 2x the Time for sending our first frame
				if (this->smoothedRoundTripTime == 0)
					this->smoothedRoundTripTime = (millis() - this->sendingStarttime) * 2;

				/*
				* randomly wait 1-2x the round trip interval for a response
				* we need to wait at least 1x: Otherwise no ack-time provided
				* */
				this->retransmissionTimeout = this->smoothedRoundTripTime * (random(RTO_SMOOTHING_BETA_FROM, RTO_SMOOTHING_BETA_TO) / 10.0); //1-2x RTT
				Serial.print("Will wait for ACK for ms: ");
				Serial.println(this->retransmissionTimeout);
			} else
				this->statusTransition(LORA_IDLE);
			//free data from Queue so we don't end up sending it over and over
			free(CommandQueue.dequeue());
			return;
		}
		//This was unexpected.. try to do something with it.
		this->handleError(loraResponse.c_str()); //Do something.
		return;
	}

	//Else discard the message.
	Serial.println("!!! DISCARDING THIS MESSAGE !!!");
}

void LoRaCommManager::setMessageCallback(MsgCallback callback) {
	this->msgCallback = callback;
}

void LoRaCommManager::setStatusCallback(StatusCallback callback) {
	this->statusCallback = callback;
}

void LoRaCommManager::enqueueLoRaCommand(const char* cmd) {
	CommandQueue.enqueue(strdup(cmd));
}

void LoRaCommManager::enqueueLoRaCommand(const char* cmd, const char* param) {
	//concat char* s and push to enqueueLoRaCommand(char*  cmd)
	char* result = (char*)malloc(strlen(cmd) + strlen(param) + 1);
	strcpy(result, cmd);
	strcat(result, param);
	CommandQueue.enqueue(result);
}

void LoRaCommManager::enqueueLoRaCommand(const char* cmd, int param) {
	//concat char* s and push to enqueueLoRaCommand(char*  cmd)
	char parambuffer[10]; //We're not expecting any parameter > 250
	itoa(param, parambuffer, 10);
	char* result = (char*)malloc(strlen(cmd) + strlen(parambuffer) + 1);
	strcpy(result, cmd);
	strcat(result, parambuffer);
	CommandQueue.enqueue(result);
}

void LoRaCommManager::handleError(const char* err) {
	Serial.println("LoRa Board error response:");
	Serial.println(err);
	Serial.println("-----------------------------------");

	if (this->msgCallback)
		this->msgCallback("ERROR:" + String(err));
	
	//TODO - Transition to Status IDLE or CLOSED?
	this->statusTransition(LORA_IDLE);
}

void LoRaCommManager::sendLoRaMessage(const char* address, const char* message, int size) {
	this->sendingStarttime = millis();
	this->enqueueLoRaCommand("AT+DEST=", address);
	this->enqueueLoRaCommand("AT+SEND=", size);
	this->enqueueLoRaCommand(message);
}

void LoRaCommManager::sendLoRaMessage(const char* address, const char* message) {
	this->sendLoRaMessage(address, message, strlen(message));
}

void LoRaCommManager::sendLoRaBroadcast(const char* message, int size) {
	this->sendLoRaMessage("FFFF", message, size);
}

void LoRaCommManager::sendLoRaBroadcast(const char* message) {
	this->sendLoRaBroadcast(message, strlen(message));
}

void LoRaCommManager::handleIncomingMessage(String message) {


	//Someone sent us a message, yay!
	String responseAddr = message.substring(3, 7);

	//TODO: Is this pos check necessary? LoRa Messages are always <250 char, therefore the hex size is always 2 Characters big
	int responseLengthPos = message.indexOf(",", 8); //Look for next comma position in LR,A2FF,10,DSCVR
	String responseData = message.substring((responseLengthPos + 1)); //Get all data after Size
	Serial.print("Response Address: ");
	Serial.println(responseAddr);
	Serial.print("Response Data Position: ");
	Serial.println(responseLengthPos);
	Serial.print("Response Data: ");
	Serial.println(responseData);
	responseData.trim();  //remove trailing \r ? Why do we need this trim? incomingData should be already trim'd


	/*
	What kind of messages/formats could be in responseData?
	DSCVR
	DVDCR
	IDEMP:<tokenOfSentMessage>
	MSSGE:<tokenOfRecvMessage>:<payload>
	*/

	if (responseData != "") {

		String responseCommand = responseData.substring(0, 5); //First 5 Chars are the Command
		Serial.print("Found Response with Command: ");
		Serial.println(responseCommand);
		responseCommand.trim(); //hmmm?

		//Confirm our sended message
		if (responseCommand == "IDEMP") {

			if (strcmp(responseData.c_str(), this->awaitedResponse) == 0) {
				//This is a confirmation for our last message, nice!
				Serial.print("Confirmed Message: ");
				Serial.println(responseData);
				free(MessageQueue.dequeue());

				unsigned long roundTripTime = (millis() - this->sendingStarttime);
				//this is TCP SRTT
				this->smoothedRoundTripTime = (this->smoothedRoundTripTime * RTO_SMOOTHING_ALPHA) + (roundTripTime * (1 - RTO_SMOOTHING_ALPHA));
				Serial.print("RTT Time: ");
				Serial.print(roundTripTime);
				Serial.println("ms");
				Serial.print("Average Round Trip Time: ");
				Serial.print(this->smoothedRoundTripTime);
				Serial.println("ms");
			} else {
				Serial.println("Could not confirm Message.");
				Serial.print("Expected IDEMP Token: ");
				Serial.println(this->awaitedResponse);
				Serial.print("Actual   IDEMP Token: ");
				Serial.println(responseData);
			}

			//if this was not a confirmation for our last message. We need to resend our last one, as there probably was a collision
			//Otherwise we can continue sending. In every case, we can stop waiting now.
			this->statusTransition(LORA_IDLE);
			strcpy(this->awaitedResponse, "");
			this->retransmissionBackoffCycle = 1; //also reset waiting cycle, as there was communication
			return;
		}

		if (responseCommand == "DSCVR" || responseCommand == "DVDCR") {
			if (responseCommand == "DSCVR") {
				//respond
				ASAPLoRaMessage* msg = new ASAPLoRaMessage();
				msg->setAddress(responseAddr.c_str());
				msg->setPayload("DVDCR");
				this->enqueueMessage(msg);
			}

			// Tell the connected ASAPEngine that we encountered another device
			if (this->msgCallback)
				this->msgCallback("DVDCR:" + responseAddr);

			this->statusTransition(LORA_IDLE);
			return;
		}

		//TODO, if we fix the idempotency token length, we can simplify this
		int responseIdempotencyTokenPos = responseData.indexOf(":", 6); //Look for next colon position in MSSGE:a23dDassd2:<payload>
		String responseIdempotencyToken = responseData.substring(6, responseIdempotencyTokenPos); //Get idempotencytoken
		String responsePayload = responseData.substring((responseIdempotencyTokenPos + 1)); //Get all Payload after idempotencytoken:
		Serial.print("Response IdempotencyToken: ");
		Serial.println(responseIdempotencyToken);
		Serial.print("Response Payload: ");
		Serial.println(responsePayload);

		//Remember last X messages and ignore message if we already got that
		boolean isMessageKnown = false;
		for (decltype(idempBuffer)::index_t i = 0; i < idempBuffer.size(); i++) {
			if (strcmp(idempBuffer[i], responseIdempotencyToken.c_str()) == 0) {
				isMessageKnown = true;
				break;
			}
		}

		if (!isMessageKnown) {
			//If our message was not known yet, remember the token and...
			idempBuffer.unshift(strdup(responseIdempotencyToken.c_str()));
			// ...pass the Payload to our listener
			if (this->msgCallback)
				this->msgCallback("MSSGE:" + responseAddr + ":" + responsePayload);
		}

		//In any case, answer "IDEMP:<thetoken>" to signal that we received the message
		this->statusTransition(LORA_SENDING_MESSAGE);
		this->msgType = LORA_MESSAGE_IMMEDIATE;
		this->sendLoRaMessage(responseAddr.c_str(), ("IDEMP:" + responseIdempotencyToken).c_str());

		//We received a message and will send an IDEMP now. 
		//set lastFrameReceiveTime to now, so that we wait 1x rtt till we try to send our next message
		this->lastFrameReceiveTime = millis();
	}
}

void LoRaCommManager::statusTransition(LoRaCommManagerStatus_t newStatus) {
	/* enum translation array */
	const char* loRaCommManagerStatus[] = {
		"LORA_IDLE",
		"LORA_AWAITING_COMMAND_RESULT",
		"LORA_SENDING_MESSAGE",
		"LORA_AWAITING_MESSAGE_RESPONSE",
		"LORA_AWAITING_MESSAGE_COMMAND_RESULT",
		"LORA_CLOSED"
	};

	Serial.print("LoRaCommManager::status transition: ");
	Serial.print(loRaCommManagerStatus[this->status]);
	Serial.print(" to ");
	this->status = newStatus;
	Serial.println(loRaCommManagerStatus[this->status]);


	if (this->statusCallback)
		this->statusCallback(this->status);
}


void LoRaCommManager::handleMessages() {

	// First of all, read from the HIMO Board so we never miss a message
	if (_HIMO01P.available()) {
		Serial.println("Reading from HIMO Board...");
		this->readHIMOBoard();
	}

	if ((millis() - this->lastCommandTime) < 100)
		return; //Abort if we're doing this more often than 10x per second...

	//If we are idle and have messages that are not processed, do that.
	if (this->status == LORA_IDLE && !ResponseQueue.isEmpty()) {
		Serial.println("Handling previously received message...");
		char* response = ResponseQueue.dequeue();
		this->handleIncomingMessage(String(response)); //TODO this reeks of memory leak...
		free(response);
	}

	//We are idle and have commands to issue
	if ((this->status == LORA_IDLE || this->status == LORA_SENDING_MESSAGE) && !CommandQueue.isEmpty()) {
		//issue a command
		Serial.println("Sending next command: ");
		char* cmd = CommandQueue.peek();
		Serial.println(cmd);
		_HIMO01P.println(cmd);

		//if we've been sending a message, we wait for a message command result. If not, we wait for a normal command result
		this->statusTransition((this->status == LORA_SENDING_MESSAGE) ? LORA_AWAITING_MESSAGE_COMMAND_RESULT : LORA_AWAITING_COMMAND_RESULT);
	}

	//We are idle and have messages to send
	if (this->status == LORA_IDLE && !MessageQueue.isEmpty()) {

		if ((millis() - this->lastFrameReceiveTime) <= this->smoothedRoundTripTime) {
			//do not send, wait for polite Medium Access timer to elapse
			if ((millis() - this->lastWaitLog) > 1000) { //only log every second
				Serial.print("Listening for some more ms: ");
				Serial.println(this->smoothedRoundTripTime - (millis() - this->lastFrameReceiveTime));
				this->lastWaitLog = millis();
			}
			return;
		}

		Serial.println("Sending next message");
		ASAPLoRaMessage* msg = MessageQueue.peek();

		Serial.print("IdempotencyToken: ");
		Serial.println(msg->getIdempotencyToken());
		Serial.print("Address: ");
		Serial.println(msg->getAddress());
		Serial.print("Payload: ");
		Serial.println(msg->getPayload());

		//If it is Broadcasting / System Message, we don't care about a confirmation
		if (strcmp(msg->getAddress(), "FFFF") == 0 || strcmp(msg->getPayload(), "DSCVR") == 0 || strcmp(msg->getPayload(), "DVDCR") == 0) {
			free(MessageQueue.dequeue()); //free message from Queue so we don't end up sending it over and over
			this->msgType = LORA_MESSAGE_IMMEDIATE;
			this->sendLoRaMessage(msg->getAddress(), msg->getPayload()); //Send raw payload
		} else {
			this->sendLoRaMessage(msg->getAddress(), msg->getLoRaPayload());
			//if not broadcast, we try to wait for the Idempotency Token
			this->msgType = LORA_MESSAGE_NORMAL;
			strcpy(this->awaitedResponse, "IDEMP:");
			strcat(this->awaitedResponse, msg->getIdempotencyToken());
		}

		this->statusTransition(LORA_SENDING_MESSAGE);
	}

	if (this->status == LORA_AWAITING_MESSAGE_RESPONSE) {
		//elapsed time in ms > waitTime in ms * waitCycle+1
		if ((millis() - this->waitingStarttime) > (this->retransmissionTimeout * (this->retransmissionBackoffCycle))) {
			//one waitcycle is over! Re-Send our message in next frame!
			Serial.println("One waitcycle is over! Re-Send our message in next frame!");
			Serial.print("MessageQueue Size: ");
			Serial.println(MessageQueue.count());
			strcpy(this->awaitedResponse, "");
			this->retransmissionTimeout = 0;
			this->retransmissionBackoffCycle = this->retransmissionBackoffCycle + 1;

			this->statusTransition(LORA_IDLE);
		} else {
			if ((millis() - this->lastWaitLog) > 1000) { //only log every second
				Serial.print("Waiting for ACK - ms left until retry: ");
				Serial.println((this->retransmissionTimeout * (this->retransmissionBackoffCycle)) - (millis() - this->waitingStarttime));
				this->lastWaitLog = millis();
			}
		}

		if (this->retransmissionBackoffCycle > 5) {
			Serial.println("Was not able to deliver message for 5 cycles. Skipping.");
			free(MessageQueue.dequeue());
			this->retransmissionBackoffCycle = 1;
			strcpy(this->awaitedResponse, "");
			Serial.print("MessageQueue Size: ");
			Serial.println(MessageQueue.count());

			this->statusTransition(LORA_IDLE);
		}
	}
	this->lastCommandTime = millis();
}
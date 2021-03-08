#pragma once
#include "Arduino.h"
class ASAPLoRaMessage
{
public:
	ASAPLoRaMessage();
	void setAddress(const char* address);
	void setPayload(const char* payload);
	const char* getPayload();
	const char* getAddress();
	const char* getIdempotencyToken();
	const char* getLoRaPayload();
private:
	char idempotencyToken[10];
	char* address;
	char* payload;

};


#include "Arduino.h"
#include "ASAPLoRaMessage.h"

ASAPLoRaMessage::ASAPLoRaMessage() {
	for (int i = 0; i < 10; i++) {
		switch (random(0, 3)) {
		case 0:
			this->idempotencyToken[i] = (char) random(48, 58); //0-9
			break;
		case 1:
			this->idempotencyToken[i] = (char)random(65, 91); //A-Z
			break;
		case 2:
			this->idempotencyToken[i] = (char)random(97, 123); //a-z
			break;
		}
	}
	this->idempotencyToken[10] = '\0';
	this->address = "";
	this->payload = "";
}

void ASAPLoRaMessage::setAddress(const char* address) {
	this->address = strdup(address);
}

void ASAPLoRaMessage::setPayload(const char* payload) {
	this->payload = strdup(payload);
}

const char* ASAPLoRaMessage::getPayload() {
	return this->payload;
}

const char* ASAPLoRaMessage::getAddress() {
	return this->address;
}

const char* ASAPLoRaMessage::getIdempotencyToken() {
	return this->idempotencyToken;
}

const char* ASAPLoRaMessage::getLoRaPayload() {
	char* result = (char*)malloc(6 + strlen(this->idempotencyToken) + 1 + strlen(this->payload));
	strcpy(result, "MSSGE:");
	strcat(result, this->idempotencyToken);
	strcat(result, ":");
	strcat(result, this->payload);
	return result;
}

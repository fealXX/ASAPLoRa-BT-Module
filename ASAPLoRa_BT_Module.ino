#include "ASAPLoRaConfig.h";

#include "ASAPLoRaBTStatus.h"

#include <HardwareSerial.h>
#include <BluetoothSerial.h>

#include <FastLED.h>

#include "StatusLED.h"
#include "LoRaCommManager.h"


/**
   I probably should not use Arduino-Strings, but the ESP32 has shittons of memory, so I'll be lazy for now.
*/


BluetoothSerial ASAPLoRaEngine;
LoRaCommManager loraCommManager;


//Status LED
#define DATA_PIN 18
CRGB leds[1];


// Bluetooth Status
ASAPLoRaBTStatus_t ASAPLoRaBTStatus = ASAPLoRaBTStatus_closed;

void btStatusCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
	if (event == ESP_SPP_SRV_OPEN_EVT) {
		ASAPLoRaBTStatus = ASAPLoRaBTStatus_ready;
		Serial.println("#################################################");
		Serial.println("##    ASAPLoRaEngine connected via Bluetooth   ##");
		Serial.println("#################################################");
		return;
	}

	if (event == ESP_SPP_CLOSE_EVT) {
		ASAPLoRaBTStatus = ASAPLoRaBTStatus_closed;
		Serial.println("##################################################");
		Serial.println("##  ASAPLoRaEngine disconnected from Bluetooth  ##");
		Serial.println("##################################################");
		return;
	}

	Serial.print("Bluetooth Event: ");
	Serial.println(event);
}


/**
##########################################################################################################
######                                                                                              ######
######                         Setup our LoRa Board and BT Connection                               ######
######                                                                                              ######
##########################################################################################################
*/

void setup() {
	//Intialize the status led to display connection status etc
	FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, 1);
	FastLED.setBrightness(50);
	statusLedYellow();

	Serial.begin(115200);

	randomSeed(analogRead(0)); //GPIO0 is a NC 12bit ADC

	//setup LoRa Communication Manager
	loraCommManager.setupLoRaBoard(LORA_ADDRESS, LORA_MESSAGING_CONFIG);
	loraCommManager.setMessageCallback(loraMessageCallback);
	loraCommManager.setStatusCallback(loraStatusCallback);

	ASAPLoRaEngine.register_callback(btStatusCallback);
	ASAPLoRaEngine.begin(LORA_NODE_NAME);

	Serial.print("ASAPLoRa-BT-Module ");
	Serial.print(LORA_NODE_NAME);
	Serial.println(" initialized");
	Serial.println("###################################");
	statusLedRed();
}


/**
##########################################################################################################
######                                                                                              ######
######                            Start loop()'ing and handling messages                            ######
######                                                                                              ######
##########################################################################################################
*/

void loraMessageCallback(String msg) {
	Serial.println("LoRa Message in Callback recieved:");
	Serial.println(msg);
	msg.trim(); //trim, in case there are more line delimiters left
	ASAPLoRaEngine.println(msg); //Send with line delimiter as closing character
	ASAPLoRaEngine.flush();
}

void loraStatusCallback(LoRaCommManagerStatus_t status) {
	switch (status) {
	case LORA_IDLE:
		statusLedGreen();
		break;
	case LORA_CLOSED:
		statusLedRed();
		break;
	case LORA_AWAITING_MESSAGE_RESPONSE:
		statusLedOrange();
		break;
	case LORA_AWAITING_COMMAND_RESULT:
		statusLedMagenta();
		break;
	case LORA_SENDING_MESSAGE:
		statusLedBlue();
		break;
	case LORA_AWAITING_MESSAGE_COMMAND_RESULT:
		statusLedCyan();
		break;
	}
}

void loop() {
	if (ASAPLoRaBTStatus == ASAPLoRaBTStatus_closed) {
		//teardown LoRa Communication Manager
		loraCommManager.close();
	}

	//read all Data coming from ASAPLoRaEngine to ASAPLoRa-BT Module
	if (ASAPLoRaBTStatus == ASAPLoRaBTStatus_ready) {
		while (ASAPLoRaEngine.available()) {
			Serial.println("Recieving Data from ASAPLoRaEngine...");
			String incomingData = ASAPLoRaEngine.readStringUntil('\n');
			incomingData.trim(); //remove trailing \r in case of CRLF

			Serial.println("New Message from ASAPLoRaEngine:");
			Serial.println(incomingData);

			String messageType = incomingData.substring(0, 5);

			Serial.print("Message Type is: ");
			Serial.println(messageType);

			//Enqueue Messages
			if (messageType == "DSCVR") { // Discovery
				ASAPLoRaMessage* msg = new ASAPLoRaMessage();
				msg->setAddress("FFFF");
				msg->setPayload("DSCVR");
				loraCommManager.enqueueMessage(msg);
			} else if (messageType == "MSSGE") {
				ASAPLoRaMessage* msg = new ASAPLoRaMessage();
				msg->setAddress(incomingData.substring(6, 10).c_str());
				msg->setPayload(incomingData.substring(11).c_str());
				loraCommManager.enqueueMessage(msg);
			}
		}
		//Inform LoRaCommManager that we are connected
		loraCommManager.open();
	}

	//Trigger Message Handling
	loraCommManager.handleMessages();
}

/*
 Name:		F1TelemetryDevice.ino
 Created:	4/6/2019 2:51:00 PM
 Author:	Dulhan Jayalath
*/

// NOTES
// FIX for Race in Spectator mode
// 

// Graphics
#include <SPI.h>
#include <Adafruit_ILI9341esp.h>
#include <Adafruit_GFX.h>

// WiFi
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// WiFi Manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// Misc.
#include <cmath>

// UDP Data Structures
#include "PacketDataStructures.h"

#define PACKET_BYTES 1289
#define UDP_PORT 20777

// Wireless router login
String AP_SSID = "AP-F1TELEMETRY";
IPAddress AP_IP(10, 0, 1, 1);

// Define server and udp listener
WiFiUDP udp_listener;
WiFiServer server(80);

// Variables to accomodate packet data
char incoming_packet[PACKET_BYTES];
UDPPacket packet;
UDPPacket packet_old;
bool first_packet = true;


// For the Esp connection of touch
#define TFT_DC 2
#define TFT_CS 15

// 320x240 display
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// Misc.
enum MODE
{
	IDLE,
	PRACTICE,
	QUALI,
	RACE
};
int current_mode = -1;
unsigned long idle_start = 0;
unsigned long idle_time = 0;
Fuel fuel;
String str_tyre[4];
uint16 str_tyre_pos[4][2] = { {0, 120 }, {80, 120}, {0, 0}, {80, 0} };
String str_penalties;
String str_fuelmix;
String str_fueltank;
String str_wingdmg;

// Auxillary Functions

void WriteCentered(int16_t x, int16_t y, String string, int8 size)
{
	tft.setTextSize(size);
	tft.setCursor(x - (string.length() * 3 * size), y - (4 * size));
	tft.print(string);
	return;
}

int16_t GetTextHeight(int8 size)
{
	return 8 * size;
}

void ClearScreen()
{
	tft.fillScreen(ILI9341_BLACK);
	return;
}

// SETUP

void setup() {

	// Set baudrate
	Serial.begin(115200);

	// Setup TFT
	delay(1000);
	SPI.setFrequency(ESP_SPI_FREQ);
	tft.begin();
	tft.setRotation(45);
	tft.fillScreen(ILI9341_BLACK);
	tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
	tft.setTextSize(3);

	// Setup WiFi
	WriteCentered(160, 60, "Awaiting Connection...", 2);
	WriteCentered(160, 120, "SSID: " + AP_SSID, 2);
	WriteCentered(160, 180, "IP: " + AP_IP.toString(), 2);
	WiFiManager wifiManager;
	//wifiManager.resetSettings(); // For testing
	wifiManager.setAPStaticIPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
	if (!wifiManager.autoConnect(AP_SSID.c_str()))
	{
		ClearScreen();
		// Notify error and restart ESP.
		WriteCentered(160, 120, "Connection Failed", 2);
		delay(5000);
		ESP.restart();
		delay(5000);
	}
	ClearScreen();

	// Listen for incoming packets
	udp_listener.begin(UDP_PORT);
}

// MAIN LOOP

void loop() {

	// Test if packet has been received
	int packet_size = udp_listener.parsePacket();
	if (packet_size)
	{
		idle_time = 0;
		if (first_packet)
			ClearScreen();

		// Copy received packet into packet variables
		udp_listener.read(incoming_packet, PACKET_BYTES);
		memcpy(&packet_old, &packet, PACKET_BYTES);
		memcpy(&packet, incoming_packet, PACKET_BYTES);

		// Notify mode change if session changed
		if (packet.m_sessionType != current_mode)
		{
			first_packet = true;
			current_mode = packet.m_sessionType;
			ClearScreen();
			switch ((int)packet.m_sessionType)
			{
			case 1:
				WriteCentered(160, 120, "Practice Mode", 4);
				delay(5000);
				ClearScreen();
				break;
			case 2:
				tft.fillScreen(ILI9341_MAGENTA);
				tft.setTextColor(ILI9341_WHITE);
				WriteCentered(160, 120, "Quali Mode", 5);
				delay(5000);
				ClearScreen();
				tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
				break;
			case 3:
				tft.fillScreen(ILI9341_RED);
				tft.setTextColor(ILI9341_WHITE);
				WriteCentered(160, 120, "Race Mode", 5);
				delay(5000);
				ClearScreen();
				tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
				// Draw boxes for each tyre
				tft.drawFastVLine(0, 0, 240, ILI9341_CYAN);
				tft.drawFastVLine(160, 0, 240, ILI9341_CYAN);
				tft.drawFastVLine(80, 0, 240, ILI9341_CYAN);
				tft.drawFastHLine(0, 120, 160, ILI9341_CYAN);
				tft.drawFastHLine(0, 0, 160, ILI9341_CYAN);
				tft.drawFastHLine(0, 239, 160, ILI9341_CYAN);
				// Draw boxes for wing
				tft.drawFastHLine(170, 170 - 20, 140, ILI9341_CYAN);
				tft.drawFastHLine(170, 170 + 20, 140, ILI9341_CYAN);
				tft.drawFastVLine(170, 170 - 20, 40, ILI9341_CYAN);
				tft.drawFastVLine(240, 170 - 20, 40, ILI9341_CYAN);
				tft.drawFastVLine(310, 170 - 20, 40, ILI9341_CYAN);
				break;
			default:
				break;
			}
		}

		// Execute actions based on mode
		switch (current_mode)
		{
		case PRACTICE:
			break;
		case QUALI:
			break;
		case RACE:
			// Tyre telemetry
			for (int i = 0; i < 4; i++)
			{
				// TYRE WEAR
				if (packet_old.m_tyres_wear[i] != packet.m_tyres_wear[i] || first_packet)
				{
					tft.fillRect(str_tyre_pos[i][0] + 1, str_tyre_pos[i][1] + 1, 78, 55, ILI9341_BLACK);
					str_tyre[i] = "";
					str_tyre[i] += (int)packet.m_tyres_wear[i];
					str_tyre[i] += (char)37;
					WriteCentered(str_tyre_pos[i][0] + 40, str_tyre_pos[i][1] + 30, str_tyre[i].c_str(), 3);
				}
				
				// TYRE TEMPS
				if (packet_old.m_tyres_temperature[i] != packet.m_tyres_temperature[i] || first_packet)
				{
					// If digits change, wipe the area first
					if (packet_old.m_tyres_temperature[i] >= 100 && packet.m_tyres_temperature[i] < 100 ||
						packet_old.m_tyres_temperature[i] < 100 && packet.m_tyres_temperature[i] >= 100)
						tft.fillRect(str_tyre_pos[i][0] + 1, str_tyre_pos[i][1] + 60, 78, 55, ILI9341_BLACK);

					str_tyre[i] = "";
					str_tyre[i] += (int)packet.m_tyres_temperature[i];
					str_tyre[i] += (char)248;
					str_tyre[i].concat('C');
					WriteCentered(str_tyre_pos[i][0] + 40, str_tyre_pos[i][1] + 90, str_tyre[i].c_str(), 2);
				}
			}

			// CUMULATIVE PLAYER PENALTY TIME
			if ((int)packet_old.m_car_data[(int)packet_old.m_player_car_index].m_penalties !=
				(int)packet.m_car_data[(int)packet.m_player_car_index].m_penalties || first_packet)
			{
				str_penalties = "+ ";
				str_penalties += (int)packet.m_car_data[(int)packet.m_player_car_index].m_penalties;
				str_penalties.concat('s');
				WriteCentered(240, 240 - GetTextHeight(3), str_penalties.c_str(), 3);
			}

			// DRS INDICATOR
			if (packet_old.m_drs != packet.m_drs || first_packet)
			{
				if ((int)packet.m_drs)
				{
					tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
					WriteCentered(240, GetTextHeight(4), "DRS", 4);
					tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
				}
				else
				{
					tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
					WriteCentered(240, GetTextHeight(4), "DRS", 4);
					tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
				}
			}

			// FUEL MIX INDICATOR
			if (packet_old.m_fuel_mix != packet.m_fuel_mix || first_packet)
			{
				switch (packet.m_fuel_mix)
				{
				case 0:
					str_fuelmix = "Lean Fuel";
					break;
				case 1:
					str_fuelmix = "Stnd Fuel";
					break;
				case 2:
					str_fuelmix = "Rich Fuel";
					break;
				case 3:
					str_fuelmix = "MAX. Fuel";
					break;
				}
				WriteCentered(240, 80, str_fuelmix.c_str(), 2);
			}

			// LEFT WING DMG
			if (packet_old.m_front_left_wing_damage != packet.m_front_left_wing_damage || first_packet)
			{
				tft.fillRect(171, 170 - GetTextHeight(2) / 2, 68, GetTextHeight(2), ILI9341_BLACK);
				if (packet.m_front_left_wing_damage == 0)
					tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
				else if (packet.m_front_left_wing_damage < 20)
					tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
				else
					tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
				
				str_wingdmg = "";
				str_wingdmg += (int)packet.m_front_left_wing_damage;
				str_wingdmg += (char)37;
				WriteCentered(205, 170, str_wingdmg, 2);

				tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
			}

			// RIGHT WING DMG
			if (packet_old.m_front_right_wing_damage != packet.m_front_right_wing_damage || first_packet)
			{
				tft.fillRect(241, 170 - GetTextHeight(2) / 2, 68, GetTextHeight(2), ILI9341_BLACK);
				if (packet.m_front_right_wing_damage == 0)
					tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
				else if (packet.m_front_right_wing_damage < 20)
					tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
				else
					tft.setTextColor(ILI9341_RED, ILI9341_BLACK);

				str_wingdmg = "";
				str_wingdmg += (int)packet.m_front_right_wing_damage;
				str_wingdmg += (char)37;
				WriteCentered(275, 170, str_wingdmg, 2);

				tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
			}

			// FUEL INDICATOR
			if (packet_old.m_car_data[packet_old.m_player_car_index].m_currentLapNum != packet.m_car_data[packet.m_player_car_index].m_currentLapNum && !first_packet)
			{
				fuel.InsertFuel(packet.m_fuel_in_tank);
				if (fuel.ready)
				{
					tft.fillRect(161, 120 - GetTextHeight(2) / 2, 158, GetTextHeight(2), ILI9341_BLACK);
					str_fueltank = "";
					float val = fuel.GetDelta(packet.m_total_laps - (int)packet.m_car_data[packet.m_player_car_index].m_currentLapNum);
					if (val > 0)
					{
						str_fueltank.concat("+");
						tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
					}
					else
					{
						str_fueltank.concat("-");
						tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
						val *= -1;
					}
					str_fueltank += val;
					str_fueltank.concat(" laps");
					
					WriteCentered(240, 120, str_fueltank.c_str(), 2);

					tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
				}
			}
			else if (!fuel.ready && packet_old.m_fuel_in_tank != packet.m_fuel_in_tank || first_packet)
			{
				str_fueltank = "";
				str_fueltank += floor(packet.m_fuel_in_tank * 10.f) / 10.f;
				str_fueltank.concat(" kg");
				WriteCentered(240, 120, str_fueltank.c_str(), 2);
			}
			break;
		default:
			break;
		}

		if (first_packet)
		{
			first_packet = false;
		}
	}
	else
	{
		if (idle_time == 0)
		{
			idle_start = millis();
			idle_time = 1;
		}
		else
		{
			idle_time = millis() - idle_start;
			if (idle_time > 500 && current_mode != IDLE)
			{
				current_mode = IDLE;
				ClearScreen();
				// Notify connect with internal IP
				WriteCentered(160, 120 - GetTextHeight(3), "Idle: waiting for data...", 2);
				WriteCentered(160, 120 + GetTextHeight(3), WiFi.localIP().toString().c_str(), 3);
			}
		}
	}
}
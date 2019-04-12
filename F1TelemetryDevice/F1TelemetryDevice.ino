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
	RACE,
	SPECTATOR
};

int current_mode = 0;
unsigned long idle_start = 0;
unsigned long idle_time = 0;

struct IMode
{
	void Init()
	{
		ClearScreen();
		tft.fillScreen(ILI9341_DARKCYAN);
		tft.setTextColor(ILI9341_WHITE);
		// Notify connect with internal IP
		WriteCentered(160, 120 - GetTextHeight(3) * 2, "Idle", 3);
		WriteCentered(160, 120, "Waiting for data from", 2);
		WriteCentered(160, 120 + GetTextHeight(3) * 2, WiFi.localIP().toString().c_str(), 3);
		tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
	}
};
IMode* idle = NULL;

struct PMode
{
	void Init()
	{
		tft.fillScreen(ILI9341_OLIVE);
		tft.setTextColor(ILI9341_WHITE);
		WriteCentered(160, 120, "Practice Mode", 3);
		delay(3000);
		ClearScreen();
		tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
	}
};
PMode* practice = NULL;

struct QMode
{
	int16_t tyre_pos[4][2] = { {0, 145}, {160, 145}, {0, 50}, {160, 50} };

	void Init()
	{
		tft.fillScreen(ILI9341_MAGENTA);
		tft.setTextColor(ILI9341_WHITE);
		WriteCentered(160, 120, "Quali Mode", 5);
		delay(3000);
		ClearScreen();
		tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);

		// Draw boxes for each tyre
		tft.drawFastVLine(0, 50, 240, ILI9341_CYAN);
		tft.drawFastVLine(160, 50, 240, ILI9341_CYAN);
		tft.drawFastVLine(319, 50, 240, ILI9341_CYAN);
		tft.drawFastHLine(0, 50, 320, ILI9341_CYAN);
		tft.drawFastHLine(0, 240, 320, ILI9341_CYAN);
		tft.drawFastHLine(0, 145, 320, ILI9341_CYAN);
	}

	void Update()
	{
		// Tyre telemetry
		for (int i = 0; i < 4; i++)
		{
			// TYRE WEAR
			if (packet_old.m_tyres_wear[i] != packet.m_tyres_wear[i] || first_packet)
				DisplayTyreWear(i, packet.m_tyres_wear[i], packet_old.m_tyres_wear[i]);

			// TYRE TEMPS
			if (packet_old.m_tyres_temperature[i] != packet.m_tyres_temperature[i] || first_packet)
				DisplayTyreTemps(i, packet.m_tyres_temperature[i], packet_old.m_tyres_temperature[i]);
		}

		if (packet_old.m_fuel_mix != packet.m_fuel_mix || first_packet)
			DisplayFuelMix(packet.m_fuel_mix);
	}

private:

	void DisplayTyreTemps(int index, int temperature, int temperature_last)
	{
		// If digits change, wipe the area first
		if (temperature >= 100 && temperature_last < 100 ||
			temperature < 100 && temperature_last >= 100)
			tft.fillRect(tyre_pos[index][0] + 1, tyre_pos[index][1] + 33 - GetTextHeight(3) / 2, 158, GetTextHeight(3), ILI9341_BLACK);

		// Build string
		String str_tyre_temp;
		str_tyre_temp += temperature;
		str_tyre_temp += (char)248;
		str_tyre_temp += "C";

		// Write to display
		WriteCentered(tyre_pos[index][0] + 80, tyre_pos[index][1] + 33, str_tyre_temp, 3);
	}

	void DisplayTyreWear(int index, int wear, int wear_last)
	{
		// Wipe previous
		if (wear >= 10 && wear_last < 10 ||
			wear < 10 && wear_last >= 10)
		tft.fillRect(tyre_pos[index][0] + 1, tyre_pos[index][1] + 66 - GetTextHeight(3) / 2, 158, GetTextHeight(3), ILI9341_BLACK);

		// Build string
		String str_tyre_wear;
		str_tyre_wear += wear;
		str_tyre_wear += (char)37;

		// Write to display
		WriteCentered(tyre_pos[index][0] + 80, tyre_pos[index][1] + 66, str_tyre_wear, 3);
	}

	void DisplayFuelMix(int mix)
	{
		// Set colour
		tft.setTextColor(ILI9341_WHITE);

		// Build string
		String str_mix;
		switch (mix)
		{
		case 0:
			str_mix = "Lean Fuel";
			tft.fillRect(0, 0, 320, 49, ILI9341_DARKGREY);
			break;
		case 1:
			str_mix = "Stnd Fuel";
			tft.fillRect(0, 0, 320, 49, ILI9341_RED);
			break;
		case 2:
			str_mix = "Rich Fuel";
			tft.fillRect(0, 0, 320, 49, ILI9341_RED);
			break;
		case 3:
			str_mix = "MAX. Fuel";
			tft.fillRect(0, 0, 320, 49, ILI9341_MAGENTA);
			break;
		}

		// Write to display
		WriteCentered(160, 25, str_mix, 3);

		// Reset text colours
		tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
	}
};
QMode* quali = NULL;

struct RMode
{
	// Fuel tracker
	Fuel fuel;

	// Tyre position array (RL, RR, FL, FR)
	uint16 tyre_pos[4][2] = { {0, 120 }, {80, 120}, {0, 0}, {80, 0} };

	void Init()
	{
		tft.fillScreen(ILI9341_RED);
		tft.setTextColor(ILI9341_WHITE);
		WriteCentered(160, 120, "Race Mode", 5);
		delay(3000);
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
	}

	void Update()
	{
		// Tyre telemetry
		for (int i = 0; i < 4; i++)
		{
			// TYRE WEAR
			if (packet_old.m_tyres_wear[i] != packet.m_tyres_wear[i] || first_packet)
				DisplayTyreWear(i, packet.m_tyres_wear[i], packet_old.m_tyres_wear[i]);

			// TYRE TEMPS
			if (packet_old.m_tyres_temperature[i] != packet.m_tyres_temperature[i] || first_packet)
				DisplayTyreTemps(i, packet.m_tyres_temperature[i], packet_old.m_tyres_temperature[i]);
		}

		// CUMULATIVE PLAYER PENALTY TIME
		if ((int)packet_old.m_car_data[(int)packet_old.m_player_car_index].m_penalties != (int)packet.m_car_data[(int)packet.m_player_car_index].m_penalties || first_packet)
			DisplayPenalties(packet.m_car_data[(int)packet.m_player_car_index].m_penalties);

		// DRS INDICATOR
		if (packet_old.m_drs != packet.m_drs || first_packet)
			DisplayDRS(packet.m_drs);

		// FUEL MIX INDICATOR
		if (packet_old.m_fuel_mix != packet.m_fuel_mix || first_packet)
			DisplayFuelMix(packet.m_fuel_mix);

		// LEFT WING DMG
		if (packet_old.m_front_left_wing_damage != packet.m_front_left_wing_damage || first_packet)
			DisplayWingDMG(packet.m_front_right_wing_damage, 205);

		// RIGHT WING DMG
		if (packet_old.m_front_right_wing_damage != packet.m_front_right_wing_damage || first_packet)
			DisplayWingDMG(packet.m_front_right_wing_damage, 275);

		// FUEL INDICATOR
		if (packet_old.m_car_data[packet_old.m_player_car_index].m_currentLapNum != packet.m_car_data[packet.m_player_car_index].m_currentLapNum && !first_packet)
		{
			fuel.InsertFuel(packet.m_fuel_in_tank);
			if (fuel.ready)
				DisplayFuelPrediction(packet.m_total_laps - (int)packet.m_car_data[packet.m_player_car_index].m_currentLapNum);
		}
		else if (!fuel.ready && packet_old.m_fuel_in_tank != packet.m_fuel_in_tank || first_packet)
			DisplayFuelRemaining(packet.m_fuel_in_tank);
	}

	private:
		void DisplayFuelRemaining(float fuel_remaining)
		{
			// Round off to 1 decimal place
			fuel_remaining = floor(fuel_remaining * 10.f) / 10.f;
			// Build string
			String str_fuel;
			str_fuel += fuel_remaining;
			str_fuel += " kg";
			// Write to display
			WriteCentered(240, 120, str_fuel, 2);
		}

		void DisplayFuelPrediction(float laps_remaining)
		{
			// Wipe previous prediction
			tft.fillRect(161, 120 - GetTextHeight(2) / 2, 158, GetTextHeight(2), ILI9341_BLACK);

			// Get prediction
			float prediction = fuel.GetDelta(laps_remaining);

			// Build string
			String str_fuel_prediction;
			if (prediction > 0)
			{
				str_fuel_prediction += "+";
				tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
			}
			else
			{
				str_fuel_prediction += "-";
				tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
				prediction *= -1;
			}
			str_fuel_prediction += prediction;
			str_fuel_prediction += " laps";

			// Write to display
			WriteCentered(240, 120, str_fuel_prediction, 2);

			// Return to previous colour output
			tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
		}

		void DisplayWingDMG(int dmg, int pos)
		{
			// Wipe previous
			tft.fillRect(pos - 34, 170 - GetTextHeight(2) / 2, 68, GetTextHeight(2), ILI9341_BLACK);

			// Set colour
			if (dmg == 0)
				tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
			else if (dmg < 20)
				tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
			else
				tft.setTextColor(ILI9341_RED, ILI9341_BLACK);

			// Build string
			String str_dmg;
			str_dmg += dmg;
			str_dmg += (char)37;

			// Write to display
			WriteCentered(pos, 170, str_dmg, 2);

			// Return to previous colour output
			tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
		}

		void DisplayFuelMix(int mix)
		{
			// Build string
			String str_mix;
			switch (mix)
			{
			case 0:
				str_mix = "Lean Fuel";
				break;
			case 1:
				str_mix = "Stnd Fuel";
				break;
			case 2:
				str_mix = "Rich Fuel";
				break;
			case 3:
				str_mix = "MAX. Fuel";
				break;
			}

			// Write to display
			WriteCentered(240, 80, str_mix, 2);
		}

		void DisplayDRS(int drs_on)
		{
			// Select colour
			if (drs_on)
				tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
			else
				tft.setTextColor(ILI9341_RED, ILI9341_BLACK);

			// Write to display
			WriteCentered(240, GetTextHeight(4), "DRS", 4);

			// Return to previous colour output
			tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
		}

		void DisplayPenalties(int penalty_time)
		{
			// Build string
			String str_penalty = "+ ";
			str_penalty += penalty_time;
			str_penalty += "s";

			// Write to display
			WriteCentered(240, 240 - GetTextHeight(3), str_penalty, 3);
		}

		void DisplayTyreTemps(int index, int temperature, int temperature_last)
		{
			// If digits change, wipe the area first
			if (temperature >= 100 && temperature_last < 100 ||
				temperature < 100 && temperature_last >= 100)
				tft.fillRect(tyre_pos[index][0] + 1, tyre_pos[index][1] + 60, 78, 55, ILI9341_BLACK);

			// Build string
			String str_tyre_temp;
			str_tyre_temp += temperature;
			str_tyre_temp += (char)248;
			str_tyre_temp += "C";

			// Write to display
			WriteCentered(tyre_pos[index][0] + 40, tyre_pos[index][1] + 90, str_tyre_temp, 2);
		}

		void DisplayTyreWear(int index, int wear, int wear_last)
		{
			// Wipe previous
			if (wear >= 10 && wear_last < 10 || wear < 10 && wear_last >= 10)
				tft.fillRect(tyre_pos[index][0] + 1, tyre_pos[index][1] + 1, 78, 55, ILI9341_BLACK);

			// Build string
			String str_tyre_wear;
			str_tyre_wear += wear;
			str_tyre_wear += (char)37;

			// Write to display
			WriteCentered(tyre_pos[index][0] + 40, tyre_pos[index][1] + 30, str_tyre_wear, 3);
		}
};
RMode* race = NULL;

struct SMode
{
	int index;

	void Init()
	{
		tft.fillScreen(ILI9341_DARKGREY);
		tft.setTextColor(ILI9341_WHITE);
		WriteCentered(160, 120, "Spectator Mode", 3);
		delay(3000);
		ClearScreen();
		tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
	}

	void Update()
	{
		if (packet.m_spectator_car_index != packet_old.m_spectator_car_index || first_packet)
		{
			index = packet.m_spectator_car_index;
			String str_name;
			str_name += packet.m_car_data[index].m_driverId;
			WriteCentered(160, 25, str_name, 3);
			DisplayBestTime(packet.m_car_data[index].m_bestLapTime);
		}
		if (packet.m_car_data[index].m_bestLapTime != packet_old.m_car_data[index].m_bestLapTime || first_packet)
		{
			DisplayBestTime(packet.m_car_data[index].m_bestLapTime);
		}
	}

private:

	void DisplayBestTime(float t)
	{
		int minutes = (int)t / 60;
		int seconds = t - minutes * 60;
		int millisec = (t - (int)t) * 1000.f;
		String str_best;
		str_best += "Best: 0";
		str_best += minutes;
		str_best += ":";
		if (seconds < 10)
			str_best += (char)48;
		str_best += seconds;
		str_best += ":";
		if (millisec < 10)
		{
			str_best += (char)48;
			str_best += (char)48;
		}
		else if (millisec < 100)
			str_best += (char)48;
		str_best += millisec;
		WriteCentered(160, 75, str_best, 2);
	}
};
SMode* spectator = NULL;

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

	idle = new IMode();
	idle->Init();
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
		if ((int)packet.m_sessionType != current_mode)
		{
			if (!(packet.m_is_spectating && current_mode == SPECTATOR))
			{
				// Deallocate memory from previous mode
				switch (current_mode)
				{
				case 0:
					delete idle;
					idle = NULL;
					break;
				case 1:
					delete practice;
					practice = NULL;
					break;
				case 2:
					delete quali;
					quali = NULL;
					break;
				case 3:
					delete race;
					race = NULL;
					break;
				case 4:
					delete spectator;
					spectator = NULL;
					break;
				default:
					break;
				}

				// Change mode
				first_packet = true;
				current_mode = packet.m_sessionType;
				ClearScreen();

				if (!packet.m_is_spectating)
				{
					switch (current_mode)
					{
					case 1:
						practice = new PMode();
						practice->Init();
						break;
					case 2:
						quali = new QMode();
						quali->Init();
						break;
					case 3:
						race = new RMode();
						race->Init();
						break;
					default:
						current_mode = IDLE;
						idle = new IMode();
						idle->Init();
						break;
					}
				}
				else
				{
					current_mode = SPECTATOR;
					spectator = new SMode();
					spectator->Init();
				}
			}
		}

		// Execute actions based on mode
		switch (current_mode)
		{
		case PRACTICE:
			break;
		case QUALI:
			quali->Update();
			break;
		case RACE:
			race->Update();
			break;
		case SPECTATOR:
			spectator->Update();
			break;
		default:
			break;
		}

		if (first_packet)
			first_packet = false;
	}
	else
	{
		// Automatically switch to idle mode if no packets received for > 500ms
		if (idle_time == 0)
		{
			idle_start = millis();
			idle_time = 1;
		}
		else
		{
			idle_time = millis() - idle_start;
			if (idle_time > 5000 && current_mode != IDLE)
			{
				current_mode = IDLE;
				idle = new IMode();
				idle->Init();
			}
		}
	}
}
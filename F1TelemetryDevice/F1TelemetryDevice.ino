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
#include <map>

// UDP Data Structures
#include "DataStructures.h"

#define UDP_PORT 20777

// Wireless router login
String AP_SSID = "AP-F1TELEMETRY";
IPAddress AP_IP(10, 0, 1, 1);

// Define server and udp listener
WiFiUDP udp_listener;
WiFiServer server(80);

// Variables to accomodate packet data
PacketMotionData packet_motion;
PacketSessionData packet_session;
PacketLapData packet_lap;
PacketEventData packet_event;
PacketParticipantsData packet_participants;
PacketCarSetupData packet_setups;
PacketCarTelemetryData packet_telemetry;
PacketCarStatusData packet_status;
// Old data
PacketMotionData packet_motion_old;
PacketSessionData packet_session_old;
PacketLapData packet_lap_old;
PacketEventData packet_event_old;
PacketParticipantsData packet_participants_old;
PacketCarSetupData packet_setups_old;
PacketCarTelemetryData packet_telemetry_old;
PacketCarStatusData packet_status_old;

int player_id = 0;

enum PACKET_SIZE
{
	SIZE_HEADER = 21,
	SIZE_MOTION = 1341,
	SIZE_SESSION = 147,
	SIZE_LAP = 841,
	SIZE_EVENT = 25,
	SIZE_PARTICIPANTS = 1082,
	SIZE_SETUPS = 841,
	SIZE_TELEMETRY = 1085,
	SIZE_STATUS = 1061
};

enum PACKET_TYPE
{
	TYPE_MOTION,
	TYPE_SESSION,
	TYPE_LAP,
	TYPE_EVENT,
	TYPE_PARTICIPANTS,
	TYPE_SETUPS,
	TYPE_TELEMETRY,
	TYPE_STATUS
};

bool first_packet = true;


// For the Esp connection of touch
#define TFT_DC 2
#define TFT_CS 15

// 320x240 display
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

enum MODE
{
	IDLE,
	PRACTICE,
	QUALI,
	RACE,
	SPECTATOR
};

int mode_map[13] = {IDLE, PRACTICE, PRACTICE,
					PRACTICE, PRACTICE, QUALI,
					QUALI, QUALI, QUALI, QUALI,
					RACE, RACE, IDLE};

int current_mode = IDLE;
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
			if (packet_status_old.m_carStatusData[player_id].m_tyresWear[i] != packet_status.m_carStatusData[player_id].m_tyresWear[i] || first_packet)
				DisplayTyreWear(i, packet_status.m_carStatusData[player_id].m_tyresWear[i], packet_status_old.m_carStatusData[player_id].m_tyresWear[i]);

			// TYRE TEMPS
			if (packet_telemetry_old.m_carTelemetryData[player_id].m_tyresInnerTemperature[i] != packet_telemetry.m_carTelemetryData[player_id].m_tyresInnerTemperature[i] || first_packet)
				DisplayTyreTemps(i, packet_telemetry.m_carTelemetryData[player_id].m_tyresInnerTemperature[i], packet_telemetry_old.m_carTelemetryData[player_id].m_tyresInnerTemperature[i]);
		}

		if (packet_status_old.m_carStatusData[player_id].m_fuelMix != packet_status.m_carStatusData[player_id].m_fuelMix || first_packet)
			DisplayFuelMix(packet_status.m_carStatusData[player_id].m_fuelMix);
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
			if (packet_status_old.m_carStatusData[player_id].m_tyresWear[i] != packet_status.m_carStatusData[player_id].m_tyresWear[i] || first_packet)
				DisplayTyreWear(i, packet_status.m_carStatusData[player_id].m_tyresWear[i], packet_status_old.m_carStatusData[player_id].m_tyresWear[i]);

			// TYRE TEMPS
			if (packet_telemetry_old.m_carTelemetryData[player_id].m_tyresInnerTemperature[i] != packet_telemetry.m_carTelemetryData[player_id].m_tyresInnerTemperature[i] || first_packet)
				DisplayTyreTemps(i, packet_telemetry.m_carTelemetryData[player_id].m_tyresInnerTemperature[i], packet_telemetry_old.m_carTelemetryData[player_id].m_tyresInnerTemperature[i]);
		}

		// CUMULATIVE PLAYER PENALTY TIME
		if (packet_lap_old.m_lapData[player_id].m_penalties != packet_lap.m_lapData[player_id].m_penalties || first_packet)
			DisplayPenalties(packet_lap.m_lapData[player_id].m_penalties);

		// DRS INDICATOR
		if (packet_telemetry_old.m_carTelemetryData[player_id].m_drs != packet_telemetry.m_carTelemetryData[player_id].m_drs || first_packet)
			DisplayDRS(packet_telemetry.m_carTelemetryData[player_id].m_drs);

		// FUEL MIX INDICATOR
		if (packet_status_old.m_carStatusData[player_id].m_fuelMix != packet_status.m_carStatusData[player_id].m_fuelMix || first_packet)
			DisplayFuelMix(packet_status.m_carStatusData[player_id].m_fuelMix);

		// LEFT WING DMG
		if (packet_status_old.m_carStatusData[player_id].m_frontLeftWingDamage != packet_status.m_carStatusData[player_id].m_frontLeftWingDamage || first_packet)
			DisplayWingDMG(packet_status.m_carStatusData[player_id].m_frontLeftWingDamage, 205);

		// RIGHT WING DMG
		if (packet_status_old.m_carStatusData[player_id].m_frontRightWingDamage != packet_status.m_carStatusData[player_id].m_frontRightWingDamage || first_packet)
			DisplayWingDMG(packet_status.m_carStatusData[player_id].m_frontRightWingDamage, 275);

		// FUEL INDICATOR
		if (packet_lap_old.m_lapData[player_id].m_currentLapNum != packet_lap.m_lapData[player_id].m_currentLapNum && !first_packet)
		{
			fuel.InsertFuel(packet_status.m_carStatusData[player_id].m_fuelInTank);
			if (fuel.ready)
				DisplayFuelPrediction(packet_session.m_totalLaps - packet_lap.m_lapData[player_id].m_currentLapNum);
		}
		else if (!fuel.ready && packet_status_old.m_carStatusData[player_id].m_fuelInTank != packet_status.m_carStatusData[player_id].m_fuelInTank || first_packet)
			DisplayFuelRemaining(packet_status.m_carStatusData[player_id].m_fuelInTank);
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
		if (packet_session.m_spectatorCarIndex != packet_session_old.m_spectatorCarIndex || first_packet)
		{
			index = packet_session.m_spectatorCarIndex;
			String str_name;
			str_name += packet_participants.m_participants[index].m_name;
			WriteCentered(160, 25, str_name, 3);
			DisplayBestTime(packet_lap.m_lapData[index].m_bestLapTime);
		}
		if (packet_lap.m_lapData[index].m_bestLapTime != packet_lap_old.m_lapData[index].m_bestLapTime || first_packet)
		{
			DisplayBestTime(packet_lap.m_lapData[index].m_bestLapTime);
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

		switch (packet_size)
		{
		case SIZE_MOTION:
			char incoming_motion[SIZE_MOTION];
			udp_listener.read(incoming_motion, SIZE_MOTION);
			memcpy(&packet_motion_old, &packet_motion, SIZE_MOTION);
			memcpy(&packet_motion, incoming_motion, SIZE_MOTION);
			player_id = packet_motion.m_header.m_playerCarIndex;
			break;
		case SIZE_SESSION:
			char incoming_session[SIZE_SESSION];
			udp_listener.read(incoming_session, SIZE_SESSION);
			memcpy(&packet_session_old, &packet_session, SIZE_SESSION);
			memcpy(&packet_session, incoming_session, SIZE_SESSION);
			break;
		case SIZE_LAP:
			char incoming_lap[SIZE_LAP];
			udp_listener.read(incoming_lap, SIZE_LAP);
			PacketLapData temp;
			memcpy(&temp, incoming_lap, SIZE_LAP);
			if (temp.m_header.m_packetId != 2)
			{
				memcpy(&packet_setups_old, &packet_setups, SIZE_SETUPS);
				memcpy(&packet_setups, incoming_lap, SIZE_SETUPS);
			}
			else
			{
				memcpy(&packet_lap_old, &packet_lap, SIZE_LAP);
				packet_lap = temp;
			}
			break;
		case SIZE_EVENT:
			char incoming_event[SIZE_EVENT];
			udp_listener.read(incoming_event, SIZE_EVENT);
			memcpy(&packet_event_old, &packet_event, SIZE_EVENT);
			memcpy(&packet_event, incoming_event, SIZE_EVENT);
			break;
		case SIZE_PARTICIPANTS:
			char incoming_participant[SIZE_PARTICIPANTS];
			udp_listener.read(incoming_participant, SIZE_PARTICIPANTS);
			memcpy(&packet_participants_old, &packet_participants, SIZE_PARTICIPANTS);
			memcpy(&packet_participants, incoming_participant, SIZE_PARTICIPANTS);
			break;
		case SIZE_TELEMETRY:
			char incoming_telemetry[SIZE_TELEMETRY];
			udp_listener.read(incoming_telemetry, SIZE_TELEMETRY);
			memcpy(&packet_telemetry_old, &packet_telemetry, SIZE_TELEMETRY);
			memcpy(&packet_telemetry, incoming_telemetry, SIZE_TELEMETRY);
			break;
		case SIZE_STATUS:
			char incoming_status[SIZE_STATUS];
			udp_listener.read(incoming_status, SIZE_STATUS);
			memcpy(&packet_status_old, &packet_status, SIZE_STATUS);
			memcpy(&packet_status, incoming_status, SIZE_STATUS);
			break;
		default:
			break;
		}

		// Notify mode change if session changed
		if (mode_map[packet_session.m_sessionType] != current_mode)
		{
			if (!(packet_session.m_isSpectating && current_mode == SPECTATOR))
			{
				// Deallocate memory from previous mode

				switch (current_mode)
				{
				case IDLE:
					delete idle;
					idle = NULL;
				case PRACTICE:
					delete practice;
					practice = NULL;
				case QUALI:
					delete quali;
					quali = NULL;
				case RACE:
					delete race;
					race = NULL;
				case SPECTATOR:
					delete spectator;
					spectator = NULL;
				}

				// Change mode
				first_packet = true;
				current_mode = mode_map[packet_session.m_sessionType];
				ClearScreen();

				if (!packet_session.m_isSpectating)
				{

					switch (current_mode)
					{
					case PRACTICE:
						practice = new PMode();
						practice->Init();
						break;
					case QUALI:
						quali = new QMode();
						quali->Init();
						break;
					case RACE:
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
		case IDLE:
			break;
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
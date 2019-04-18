/*
 Name:    F1TelemetryDevice.ino
 Created: 4/6/2019 2:51:00 PM
 Author:  Dulhan Jayalath
*/

// TO DO:
// - Fix race fuel calculation (Update every half-lap?)
// - Safety Car and/or VSC Mode


// Graphics
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_GFX.h>

// WiFi
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// WiFi Manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// WiFi OTA
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

// Misc.
#include <cmath>

// UDP Data Structures
#include "DataStructures.hpp"

// Default UDP Port to listen to
#define UDP_PORT 20777
#define ESP_SPI_FREQ 4000000

// Wireless router login
String AP_SSID = "AP-F1TELEMETRY";
IPAddress AP_IP(10, 0, 1, 1);

// Define server and udp listener
WiFiUDP udp_listener;
WiFiServer server(80);

template<typename T>
struct Packet
{
	T current;
	bool packet_received = false;
};
Packet <PacketMotionData> packet_motion;
Packet <PacketSessionData> packet_session;
Packet <PacketLapData> packet_lap;
Packet <PacketEventData> packet_event;
Packet <PacketParticipantsData> packet_participants;
Packet <PacketCarSetupData> packet_setups;
Packet <PacketCarTelemetryData> packet_telemetry;
Packet <PacketCarStatusData> packet_status;

// Prototypes
template<typename T> void ReadPacket(Packet<T>& packet);
void WriteCentered(int16_t x, int16_t y, String string, int8 size);
int16_t GetTextHeight(int8 size);
void ClearScreen();

// Independantly store if this is the first packet received
// and the player's car index in each array
bool first_packet = true;
int player_id = 0;

enum PACKET_SIZE // in bytes
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

enum PACKET_TYPE // used to identify header's packetID
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

// Map given sessionType to mode enumerations
int mode_map[13] = { IDLE, PRACTICE, PRACTICE, PRACTICE, PRACTICE, QUALI,
		  QUALI, QUALI, QUALI, QUALI, RACE, RACE, IDLE };

//Store current mode
int current_mode = IDLE;

// For tracking time between packets
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

	void Update()
	{
		// Check for OTA updates
		ArduinoOTA.handle();
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

	// Current value storage
	uint8 tyre_wear[4] = { 0, 0, 0, 0 };
	uint16 tyre_temp[4];
	uint8 fuel_mix;
	uint16 time_left;
	uint8 drs;

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
		tft.drawFastHLine(0, 239, 320, ILI9341_CYAN);
		tft.drawFastHLine(0, 145, 320, ILI9341_CYAN);
	}

	void Update()
	{
		// Tyre telemetry
		for (int i = 0; i < 4; i++)
		{
			// TYRE WEAR
			if (tyre_wear[i] != packet_status.current.m_carStatusData[player_id].m_tyresWear[i] || first_packet)
			{
				tyre_wear[i] = packet_status.current.m_carStatusData[player_id].m_tyresWear[i];
				DisplayTyreWear(i, packet_status.current.m_carStatusData[player_id].m_tyresWear[i], tyre_wear[i]);
			}
			// TYRE TEMPS
			if (tyre_temp[i] != packet_telemetry.current.m_carTelemetryData[player_id].m_tyresInnerTemperature[i] || first_packet)
			{
				tyre_temp[i] = packet_telemetry.current.m_carTelemetryData[player_id].m_tyresInnerTemperature[i];
				DisplayTyreTemps(i, packet_telemetry.current.m_carTelemetryData[player_id].m_tyresInnerTemperature[i], tyre_temp[i]);
			}

		}

		// Fuel mix
		if (fuel_mix != packet_status.current.m_carStatusData[player_id].m_fuelMix || first_packet)
		{
			fuel_mix = packet_status.current.m_carStatusData[player_id].m_fuelMix;
			DisplayFuelMix(packet_status.current.m_carStatusData[player_id].m_fuelMix);
		}

		// Session timer
		if (time_left != packet_session.current.m_sessionTimeLeft || first_packet)
		{
			time_left = packet_session.current.m_sessionTimeLeft;
			DisplayTimeRemaining(packet_session.current.m_sessionTimeLeft);
		}

		if (drs != packet_telemetry.current.m_carTelemetryData[player_id].m_drs)
		{
			drs = packet_telemetry.current.m_carTelemetryData[player_id].m_drs;

			if (drs)
			{
				tft.drawFastVLine(0, 50, 240, ILI9341_GREEN);
				tft.drawFastVLine(160, 50, 240, ILI9341_GREEN);
				tft.drawFastVLine(319, 50, 240, ILI9341_GREEN);
				tft.drawFastHLine(0, 50, 320, ILI9341_GREEN);
				tft.drawFastHLine(0, 239, 320, ILI9341_GREEN);
				tft.drawFastHLine(0, 145, 320, ILI9341_GREEN);
			}
			else
			{
				tft.drawFastVLine(0, 50, 240, ILI9341_CYAN);
				tft.drawFastVLine(160, 50, 240, ILI9341_CYAN);
				tft.drawFastVLine(319, 50, 240, ILI9341_CYAN);
				tft.drawFastHLine(0, 50, 320, ILI9341_CYAN);
				tft.drawFastHLine(0, 239, 320, ILI9341_CYAN);
				tft.drawFastHLine(0, 145, 320, ILI9341_CYAN);
			}
		}
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
			tft.fillRect(0, 0, 160, 49, ILI9341_DARKGREY);
			break;
		case 1:
			str_mix = "Stnd Fuel";
			tft.fillRect(0, 0, 160, 49, ILI9341_RED);
			break;
		case 2:
			str_mix = "Rich Fuel";
			tft.fillRect(0, 0, 160, 49, ILI9341_RED);
			break;
		case 3:
			str_mix = "MAX. Fuel";
			tft.fillRect(0, 0, 160, 49, ILI9341_MAGENTA);
			break;
		}

		// Write to display
		WriteCentered(80, 25, str_mix, 2);

		// Reset text colours
		tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
	}

	void DisplayTimeRemaining(uint16 time)
	{
		// Split into minutes and seconds
		int minutes = time / 60;
		int seconds = time - minutes * 60;

		// Set text colour warning
		if (time < 300)
			tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
		else
			tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

		// Build string
		String str_time;
		if (minutes < 10)
			str_time += "0";
		str_time += minutes;
		str_time += ":";
		if (seconds < 10)
			str_time += "0";
		str_time += seconds;

		WriteCentered(240, 25, str_time, 3);

		// Reset colour
		tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
	}
};
QMode* quali = NULL;

struct RMode
{
	// Current stored variables
	uint8 tyre_wear[4] = { 0, 0, 0, 0 };
	uint16 tyre_temp[4];
	uint8 fuel_mix;
	uint8 penalties;
	uint8 drs;
	uint8 wing_dmg[2]; // 0 = LW, 1 = RW
	float lap_distance;
	uint16 track_length;
	float fuel_difference[2] = { -1, -1 };
	uint8 lap_num[2];
	float lap_pos[2];
	const float update_delta = 0.5f;

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
			if (tyre_wear[i] != packet_status.current.m_carStatusData[player_id].m_tyresWear[i] || first_packet)
			{
				tyre_wear[i] = packet_status.current.m_carStatusData[player_id].m_tyresWear[i];
				DisplayTyreWear(i, packet_status.current.m_carStatusData[player_id].m_tyresWear[i], tyre_wear[i]);
			}
			// TYRE TEMPS
			if (tyre_temp[i] != packet_telemetry.current.m_carTelemetryData[player_id].m_tyresInnerTemperature[i] || first_packet)
			{
				tyre_temp[i] = packet_telemetry.current.m_carTelemetryData[player_id].m_tyresInnerTemperature[i];
				DisplayTyreTemps(i, packet_telemetry.current.m_carTelemetryData[player_id].m_tyresInnerTemperature[i], tyre_temp[i]);
			}
		}

		// CUMULATIVE PLAYER PENALTY TIME
		if (penalties != packet_lap.current.m_lapData[player_id].m_penalties || first_packet)
		{
			penalties = packet_lap.current.m_lapData[player_id].m_penalties;
			DisplayPenalties(packet_lap.current.m_lapData[player_id].m_penalties);
		}

		// DRS INDICATOR
		if (drs != packet_telemetry.current.m_carTelemetryData[player_id].m_drs || first_packet)
		{
			drs = packet_telemetry.current.m_carTelemetryData[player_id].m_drs;
			DisplayDRS(packet_telemetry.current.m_carTelemetryData[player_id].m_drs);
		}

		// FUEL MIX INDICATOR
		if (fuel_mix != packet_status.current.m_carStatusData[player_id].m_fuelMix || first_packet)
		{
			fuel_mix = packet_status.current.m_carStatusData[player_id].m_fuelMix;
			DisplayFuelMix(packet_status.current.m_carStatusData[player_id].m_fuelMix);
		}

		// LEFT WING DMG
		if (wing_dmg[0] != packet_status.current.m_carStatusData[player_id].m_frontLeftWingDamage || first_packet)
		{
			wing_dmg[0] = packet_status.current.m_carStatusData[player_id].m_frontLeftWingDamage;
			DisplayWingDMG(packet_status.current.m_carStatusData[player_id].m_frontLeftWingDamage, 205);
		}

		// RIGHT WING DMG
		if (wing_dmg[1] != packet_status.current.m_carStatusData[player_id].m_frontRightWingDamage || first_packet)
		{
			wing_dmg[1] = packet_status.current.m_carStatusData[player_id].m_frontRightWingDamage;
			DisplayWingDMG(packet_status.current.m_carStatusData[player_id].m_frontRightWingDamage, 275);
		}

		// FUEL PREDICTION
		if (lap_distance != packet_lap.current.m_lapData[player_id].m_lapDistance)
		{
			lap_distance = packet_lap.current.m_lapData[player_id].m_lapDistance;
			if (fuel_difference[0] == -1)
			{
				track_length = packet_session.current.m_trackLength;
				fuel_difference[0] = packet_status.current.m_carStatusData[player_id].m_fuelInTank;
				lap_num[0] = packet_lap.current.m_lapData[player_id].m_currentLapNum;
				lap_pos[0] = packet_lap.current.m_lapData[player_id].m_lapDistance / (float)track_length;
				DisplayFuelRemaining(packet_status.current.m_carStatusData[player_id].m_fuelInTank);
			}

			lap_num[1] = packet_lap.current.m_lapData[player_id].m_currentLapNum;
			lap_pos[1] = packet_lap.current.m_lapData[player_id].m_lapDistance / (float)track_length;

			if (((float)lap_num[1] + lap_pos[1]) - ((float)lap_num[0] + lap_pos[0]) > update_delta)
			{
				fuel_difference[1] = packet_status.current.m_carStatusData[player_id].m_fuelInTank;
				float rate = (fuel_difference[0] - fuel_difference[1]) / update_delta;
				uint8 laps_remaining = packet_session.current.m_totalLaps - packet_lap.current.m_lapData[player_id].m_currentLapNum - 1;
				float est_fuel_required = ((float)laps_remaining - lap_pos[1]) * rate;
				float est_fuel_left = (packet_status.current.m_carStatusData[player_id].m_fuelInTank - est_fuel_required) / rate;

				// Output
				String str_fuel_left;
				if (est_fuel_left > 0)
				{
					str_fuel_left += "+";
					tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
				}
				else
				{
					tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
				}
				str_fuel_left += floor(est_fuel_left * 10.f) / 10.f;
				str_fuel_left += " laps";
				WriteCentered(240, 120, str_fuel_left, 2);
				tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);


				fuel_difference[0] = fuel_difference[1];
				lap_num[0] = lap_num[1];
				lap_pos[0] = lap_pos[1];
			}
		}
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
	// Spectating car index
	uint8 spect_index;
	float best_lap_time;

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
		if (spect_index != packet_session.current.m_spectatorCarIndex || first_packet)
		{
			tft.fillRect(0, 0, 320, 50, ILI9341_BLACK);
			spect_index = packet_session.current.m_spectatorCarIndex;
			String str_name;
			str_name += packet_participants.current.m_participants[spect_index].m_name;
			WriteCentered(160, 25, str_name, 3);
			DisplayBestTime(packet_lap.current.m_lapData[spect_index].m_bestLapTime);
		}

		if (best_lap_time != packet_lap.current.m_lapData[spect_index].m_bestLapTime || first_packet)
		{
			best_lap_time = packet_lap.current.m_lapData[spect_index].m_bestLapTime;
			DisplayBestTime(packet_lap.current.m_lapData[spect_index].m_bestLapTime);
		}
	}

private:

	void DisplayBestTime(float t)
	{
		// Split time into minutes, seconds and milliseconds
		int minutes = (int)t / 60;
		int seconds = t - minutes * 60;
		int millisec = (t - (int)t) * 1000.f;

		// Build string with zero padding
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

		// Write to display
		WriteCentered(160, 75, str_best, 2);
	}
};
SMode* spectator = NULL;

// Auxillary Functions

template<typename A> void ReadPacket(Packet<A> & packet)
{
	udp_listener.read((char*)& packet.current, sizeof(A));
};

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

	// Update CPU Freq to 160MHz
	system_update_cpu_freq(160);
	// Set baudrate
	Serial.begin(115200);

	Serial.println(system_get_cpu_freq()); // Debugging

	// Setup TFT
	delay(1000);
	SPI.setFrequency(ESP_SPI_FREQ);
	tft.begin();
	tft.setRotation(135);
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

	// OTA
	ArduinoOTA.onStart([]() {
		ClearScreen();
		});
	ArduinoOTA.onEnd([]() {
		ClearScreen();
		WriteCentered(160, 100, "Update Complete", 3);
		WriteCentered(160, 140, "Restarting...", 3);
		});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		WriteCentered(160, 100, "Updating...", 3);
		int prog = (progress / (total / 100));
		String str_progress;
		str_progress += prog;
		str_progress += (char)37;
		WriteCentered(160, 140, str_progress, 3);
		});
	ArduinoOTA.onError([](ota_error_t error) {
		String str_error;
		if (error == OTA_AUTH_ERROR) str_error = "Auth Failed";
		else if (error == OTA_BEGIN_ERROR) str_error = "Begin Failed";
		else if (error == OTA_CONNECT_ERROR) str_error = "Connect Failed";
		else if (error == OTA_RECEIVE_ERROR) str_error = "Receive Failed";
		else if (error == OTA_END_ERROR) str_error = "End Failed";
		ClearScreen();
		WriteCentered(160, 100, "Update Error", 3);
		WriteCentered(160, 140, str_error, 3);
		delay(5000);
		ESP.restart();
		});
	ArduinoOTA.begin();
}

// MAIN LOOP

void loop() {

	// Test if packet has been received
	int packet_size = udp_listener.parsePacket();
	if (packet_size)
	{
		// Reset idle time, clear screen if first packet
		idle_time = 0;
		if (first_packet)
			ClearScreen();

		// Copy data from packet
		switch (packet_size)
		{
		case SIZE_MOTION:
			ReadPacket(packet_motion);
			player_id = packet_motion.current.m_header.m_playerCarIndex;
			break;
		case SIZE_SESSION:
			ReadPacket(packet_session);
			break;
		case SIZE_LAP:
			char incoming_lap[SIZE_LAP];
			udp_listener.read(incoming_lap, SIZE_LAP);
			PacketLapData temp;
			memcpy(&temp, incoming_lap, SIZE_LAP);
			if (temp.m_header.m_packetId != 2)
			{
				memcpy(&packet_setups.current, incoming_lap, SIZE_SETUPS);
			}
			else
			{
				packet_lap.current = temp;
			}
			break;
		case SIZE_EVENT:
			ReadPacket(packet_event);
			break;
		case SIZE_PARTICIPANTS:
			ReadPacket(packet_participants);
			break;
		case SIZE_TELEMETRY:
			ReadPacket(packet_telemetry);
			break;
		case SIZE_STATUS:
			ReadPacket(packet_status);
			break;
		default:
			break;
		}

		// Change mode if session type is changed
		if (mode_map[packet_session.current.m_sessionType] != current_mode)
		{
			if (!(packet_session.current.m_isSpectating && current_mode == SPECTATOR))
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
				current_mode = mode_map[packet_session.current.m_sessionType];
				ClearScreen();

				if (!packet_session.current.m_isSpectating)
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
			idle->Update();
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

		// Indicate that first packet has passed
		if (first_packet)
			first_packet = false;
	}
	else
	{
		// Automatically switch to idle mode if no packets received for > 5000ms
		if (idle_time == 0)
		{
			idle_start = millis();
			idle_time = 1;
		}
		else
		{
			idle_time = millis() - idle_start;
			if (current_mode != IDLE)
			{
				if (idle_time > 10000)
				{
					current_mode = IDLE;
					idle = new IMode();
					idle->Init();
				}
			}
			else
			{
				idle->Update();
			}
		}
	}

}
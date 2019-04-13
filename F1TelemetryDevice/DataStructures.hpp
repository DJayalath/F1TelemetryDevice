#pragma pack(push, 1)
struct PacketHeader
{
    uint16    m_packetFormat;         // 2018
    uint8     m_packetVersion;        // Version of this packet type, all start from 1
    uint8     m_packetId;             // Identifier for the packet type, see below
    uint64    m_sessionUID;           // Unique identifier for the session
    float     m_sessionTime;          // Session timestamp
    uint      m_frameIdentifier;      // Identifier for the frame the data was retrieved on
    uint8     m_playerCarIndex;       // Index of player's car in the array
};

struct CarMotionData
{
	float         m_worldPositionX;           // World space X position
	float         m_worldPositionY;           // World space Y position
	float         m_worldPositionZ;           // World space Z position
	float         m_worldVelocityX;           // Velocity in world space X
	float         m_worldVelocityY;           // Velocity in world space Y
	float         m_worldVelocityZ;           // Velocity in world space Z
	int16_t         m_worldForwardDirX;         // World space forward X direction (normalised)
	int16_t         m_worldForwardDirY;         // World space forward Y direction (normalised)
	int16_t         m_worldForwardDirZ;         // World space forward Z direction (normalised)
	int16_t         m_worldRightDirX;           // World space right X direction (normalised)
	int16_t         m_worldRightDirY;           // World space right Y direction (normalised)
	int16_t         m_worldRightDirZ;           // World space right Z direction (normalised)
	float         m_gForceLateral;            // Lateral G-Force component
	float         m_gForceLongitudinal;       // Longitudinal G-Force component
	float         m_gForceVertical;           // Vertical G-Force component
	float         m_yaw;                      // Yaw angle in radians
	float         m_pitch;                    // Pitch angle in radians
	float         m_roll;                     // Roll angle in radians
};

struct PacketMotionData
{
	PacketHeader    m_header;               // Header

	CarMotionData   m_carMotionData[20];    // Data for all cars on track

	// Extra player car ONLY data
	float         m_suspensionPosition[4];       // Note: All wheel arrays have the following order:
	float         m_suspensionVelocity[4];       // RL, RR, FL, FR
	float         m_suspensionAcceleration[4];   // RL, RR, FL, FR
	float         m_wheelSpeed[4];               // Speed of each wheel
	float         m_wheelSlip[4];                // Slip ratio for each wheel
	float         m_localVelocityX;              // Velocity in local space
	float         m_localVelocityY;              // Velocity in local space
	float         m_localVelocityZ;              // Velocity in local space
	float         m_angularVelocityX;            // Angular velocity x-component
	float         m_angularVelocityY;            // Angular velocity y-component
	float         m_angularVelocityZ;            // Angular velocity z-component
	float         m_angularAccelerationX;        // Angular velocity x-component
	float         m_angularAccelerationY;        // Angular velocity y-component
	float         m_angularAccelerationZ;        // Angular velocity z-component
	float         m_frontWheelsAngle;            // Current front wheels angle in radians
};

struct MarshalZone
{
	float  m_zoneStart;   // Fraction (0..1) of way through the lap the marshal zone starts
	int8   m_zoneFlag;    // -1 = invalid/unknown, 0 = none, 1 = green, 2 = blue, 3 = yellow, 4 = red
};

struct PacketSessionData
{
	PacketHeader    m_header;               	// Header

	uint8           m_weather;              	// Weather - 0 = clear, 1 = light cloud, 2 = overcast
												// 3 = light rain, 4 = heavy rain, 5 = storm
	int8	    m_trackTemperature;    	// Track temp. in degrees celsius
	int8	    m_airTemperature;      	// Air temp. in degrees celsius
	uint8           m_totalLaps;           	// Total number of laps in this race
	uint16          m_trackLength;           	// Track length in metres
	uint8           m_sessionType;         	// 0 = unknown, 1 = P1, 2 = P2, 3 = P3, 4 = Short P
												// 5 = Q1, 6 = Q2, 7 = Q3, 8 = Short Q, 9 = OSQ
												// 10 = R, 11 = R2, 12 = Time Trial
	int8            m_trackId;         		// -1 for unknown, 0-21 for tracks, see appendix
	uint8           m_era;                  	// Era, 0 = modern, 1 = classic
	uint16          m_sessionTimeLeft;    	// Time left in session in seconds
	uint16          m_sessionDuration;     	// Session duration in seconds
	uint8           m_pitSpeedLimit;      	// Pit speed limit in kilometres per hour
	uint8           m_gamePaused;               // Whether the game is paused
	uint8           m_isSpectating;        	// Whether the player is spectating
	uint8           m_spectatorCarIndex;  	// Index of the car being spectated
	uint8           m_sliProNativeSupport;	// SLI Pro support, 0 = inactive, 1 = active
	uint8           m_numMarshalZones;         	// Number of marshal zones to follow
	MarshalZone     m_marshalZones[21];         // List of marshal zones � max 21
	uint8           m_safetyCarStatus;          // 0 = no safety car, 1 = full safety car
												// 2 = virtual safety car
	uint8          m_networkGame;              // 0 = offline, 1 = online
};

struct LapData
{
	float       m_lastLapTime;           // Last lap time in seconds
	float       m_currentLapTime;        // Current time around the lap in seconds
	float       m_bestLapTime;           // Best lap time of the session in seconds
	float       m_sector1Time;           // Sector 1 time in seconds
	float       m_sector2Time;           // Sector 2 time in seconds
	float       m_lapDistance;           // Distance vehicle is around current lap in metres � could
										 // be negative if line hasn�t been crossed yet
	float       m_totalDistance;         // Total distance travelled in session in metres � could
										 // be negative if line hasn�t been crossed yet
	float       m_safetyCarDelta;        // Delta in seconds for safety car
	uint8       m_carPosition;           // Car race position
	uint8       m_currentLapNum;         // Current lap number
	uint8       m_pitStatus;             // 0 = none, 1 = pitting, 2 = in pit area
	uint8       m_sector;                // 0 = sector1, 1 = sector2, 2 = sector3
	uint8       m_currentLapInvalid;     // Current lap invalid - 0 = valid, 1 = invalid
	uint8       m_penalties;             // Accumulated time penalties in seconds to be added
	uint8       m_gridPosition;          // Grid position the vehicle started the race in
	uint8       m_driverStatus;          // Status of driver - 0 = in garage, 1 = flying lap
										 // 2 = in lap, 3 = out lap, 4 = on track
	uint8       m_resultStatus;          // Result status - 0 = invalid, 1 = inactive, 2 = active
										 // 3 = finished, 4 = disqualified, 5 = not classified
										 // 6 = retired
};

struct PacketLapData
{
	PacketHeader    m_header;              // Header

	LapData         m_lapData[20];         // Lap data for all cars on track
};

struct PacketEventData
{
	PacketHeader    m_header;               // Header

	uint8           m_eventStringCode[4];   // Event string code, see above
};

struct ParticipantData
{
	uint8      m_aiControlled;           // Whether the vehicle is AI (1) or Human (0) controlled
	uint8      m_driverId;               // Driver id - see appendix
	uint8      m_teamId;                 // Team id - see appendix
	uint8      m_raceNumber;             // Race number of the car
	uint8      m_nationality;            // Nationality of the driver
	char       m_name[48];               // Name of participant in UTF-8 format � null terminated
										 // Will be truncated with � (U+2026) if too long
};

struct PacketParticipantsData
{
	PacketHeader    m_header;            // Header

	uint8           m_numCars;           // Number of cars in the data
	ParticipantData m_participants[20];
};

struct CarSetupData
{
	uint8     m_frontWing;                // Front wing aero
	uint8     m_rearWing;                 // Rear wing aero
	uint8     m_onThrottle;               // Differential adjustment on throttle (percentage)
	uint8     m_offThrottle;              // Differential adjustment off throttle (percentage)
	float     m_frontCamber;              // Front camber angle (suspension geometry)
	float     m_rearCamber;               // Rear camber angle (suspension geometry)
	float     m_frontToe;                 // Front toe angle (suspension geometry)
	float     m_rearToe;                  // Rear toe angle (suspension geometry)
	uint8     m_frontSuspension;          // Front suspension
	uint8     m_rearSuspension;           // Rear suspension
	uint8     m_frontAntiRollBar;         // Front anti-roll bar
	uint8     m_rearAntiRollBar;          // Front anti-roll bar
	uint8     m_frontSuspensionHeight;    // Front ride height
	uint8     m_rearSuspensionHeight;     // Rear ride height
	uint8     m_brakePressure;            // Brake pressure (percentage)
	uint8     m_brakeBias;                // Brake bias (percentage)
	float     m_frontTyrePressure;        // Front tyre pressure (PSI)
	float     m_rearTyrePressure;         // Rear tyre pressure (PSI)
	uint8     m_ballast;                  // Ballast
	float     m_fuelLoad;                 // Fuel load
};

struct PacketCarSetupData
{
	PacketHeader    m_header;            // Header

	CarSetupData    m_carSetups[20];
};

struct CarTelemetryData
{
	uint16    m_speed;                      // Speed of car in kilometres per hour
	uint8     m_throttle;                   // Amount of throttle applied (0 to 100)
	int8      m_steer;                      // Steering (-100 (full lock left) to 100 (full lock right))
	uint8     m_brake;                      // Amount of brake applied (0 to 100)
	uint8     m_clutch;                     // Amount of clutch applied (0 to 100)
	int8      m_gear;                       // Gear selected (1-8, N=0, R=-1)
	uint16    m_engineRPM;                  // Engine RPM
	uint8     m_drs;                        // 0 = off, 1 = on
	uint8     m_revLightsPercent;           // Rev lights indicator (percentage)
	uint16    m_brakesTemperature[4];       // Brakes temperature (celsius)
	uint16    m_tyresSurfaceTemperature[4]; // Tyres surface temperature (celsius)
	uint16    m_tyresInnerTemperature[4];   // Tyres inner temperature (celsius)
	uint16    m_engineTemperature;          // Engine temperature (celsius)
	float     m_tyresPressure[4];           // Tyres pressure (PSI)
};

struct PacketCarTelemetryData
{
	PacketHeader        m_header;                // Header

	CarTelemetryData    m_carTelemetryData[20];

	uint32              m_buttonStatus;         // Bit flags specifying which buttons are being
												// pressed currently - see appendices
};

struct CarStatusData
{
	uint8       m_tractionControl;          // 0 (off) - 2 (high)
	uint8       m_antiLockBrakes;           // 0 (off) - 1 (on)
	uint8       m_fuelMix;                  // Fuel mix - 0 = lean, 1 = standard, 2 = rich, 3 = max
	uint8       m_frontBrakeBias;           // Front brake bias (percentage)
	uint8       m_pitLimiterStatus;         // Pit limiter status - 0 = off, 1 = on
	float       m_fuelInTank;               // Current fuel mass
	float       m_fuelCapacity;             // Fuel capacity
	uint16      m_maxRPM;                   // Cars max RPM, point of rev limiter
	uint16      m_idleRPM;                  // Cars idle RPM
	uint8       m_maxGears;                 // Maximum number of gears
	uint8       m_drsAllowed;               // 0 = not allowed, 1 = allowed, -1 = unknown
	uint8       m_tyresWear[4];             // Tyre wear percentage
	uint8       m_tyreCompound;             // Modern - 0 = hyper soft, 1 = ultra soft
											// 2 = super soft, 3 = soft, 4 = medium, 5 = hard
											// 6 = super hard, 7 = inter, 8 = wet
											// Classic - 0-6 = dry, 7-8 = wet
	uint8       m_tyresDamage[4];           // Tyre damage (percentage)
	uint8       m_frontLeftWingDamage;      // Front left wing damage (percentage)
	uint8       m_frontRightWingDamage;     // Front right wing damage (percentage)
	uint8       m_rearWingDamage;           // Rear wing damage (percentage)
	uint8       m_engineDamage;             // Engine damage (percentage)
	uint8       m_gearBoxDamage;            // Gear box damage (percentage)
	uint8       m_exhaustDamage;            // Exhaust damage (percentage)
	int8        m_vehicleFiaFlags;          // -1 = invalid/unknown, 0 = none, 1 = green
											// 2 = blue, 3 = yellow, 4 = red
	float       m_ersStoreEnergy;           // ERS energy store in Joules
	uint8       m_ersDeployMode;            // ERS deployment mode, 0 = none, 1 = low, 2 = medium
											// 3 = high, 4 = overtake, 5 = hotlap
	float       m_ersHarvestedThisLapMGUK;  // ERS energy harvested this lap by MGU-K
	float       m_ersHarvestedThisLapMGUH;  // ERS energy harvested this lap by MGU-H
	float       m_ersDeployedThisLap;       // ERS energy deployed this lap
};

struct PacketCarStatusData
{
	PacketHeader        m_header;            // Header

	CarStatusData       m_carStatusData[20];
};
#pragma pack(pop)

struct Fuel
{
	// Flag if predictions ready
	bool ready = false;
	// Flag for first lap of fuel
	bool first_lap = true;

	// Store last six laps fuel data
	float fuel[6];
	uint nextfree = 0;

	void InsertFuel(float val)
	{
		if (!first_lap)
		{
			if (nextfree == 6)
			{
				for (int i = 0; i < nextfree - 1; i++)
				{
					fuel[i] = fuel[i + 1];
				}
				nextfree -= 1;
			}

			fuel[nextfree] = val;

			nextfree += 1;
			if (nextfree >= 2)
				ready = true;
		}
		else
			first_lap = false;
	}

	float GetDelta(float laps_remaining)
	{
		// Calculate the rate
		float rate = 0;
		for (int i = 0; i < nextfree - 1; i++)
		{
			rate += fuel[i] - fuel[i + 1];
		}

		rate /= ((float)nextfree - 1);


		return (fuel[nextfree - 1] - (laps_remaining + 1) * rate) / rate;
	}
};
/*********************************************************************************************
(c) 2005-2013 Copyright, Real-Time Innovations, Inc.  All rights reserved.    	                             
RTI grants Licensee a license to use, modify, compile, and create derivative works 
of the Software.  Licensee has the right to distribute object form only for use with RTI 
products.  The Software is provided �as is�, with no warranty of any type, including 
any warranty for fitness for any purpose. RTI is under no obligation to maintain or 
support the Software.  RTI shall not be liable for any incidental or consequential 
damages arising out of the use or inability to use the software.
**********************************************************************************************/
#ifndef TRACK_GENERATOR_H
#define TRACK_GENERATOR_H

#include <vector>
#include <queue>
#include "../CommonInfrastructure/OSAPI.h"

// ------------------------------------------------------------------------- //
//
// Track Generator Classes:
// This file contains classes and structures used by the "track generator"
// the track generator is an example that sends fake track information 
// from a radar at SFO.  The tracks are all within 80 KM of SFO, and follow
// flight paths that are somewhat similar to real flight paths to SFO.
// This is not intended to be entirely correct, but instead is example data
// that can be used to populate an example.
//
// ------------------------------------------------------------------------- //

// Flight paths around SFO
enum FlightState {
	INITIAL,
	INITIAL_NORTH,
	INITIAL_SOUTH,
	PATH_FROM_NORTH,
	TURNING_TO_APPROACH_FROM_NORTH,
	ON_APPROACH,
	LANDED
};

// Structure containing latitude and longitude.
struct LatLong {
	double latitude;
	double longitude;
};
// ------------------------------------------------------------------------- //
//
// GeneratorTrack:
// This class contains the track data that the radar generator provides.  
// In this example, we could have forced the "radar generator" to use the 
// same data type we generated for the middleware, but typically your sensor
// will be providing its own data types that you will be using. 
//
// So, we are converting back and forth between this radar-provided track
// and the middleware track in the GeneratorAdapter.h file.
//
// ------------------------------------------------------------------------- //
struct GeneratorTrack 
{
	// --- Constructor --- 
	GeneratorTrack(OSMutex *mutex) 
	{
		id = 0;
		// Start near San Jose
		latLong.latitude = 37.3041;
		latLong.longitude = -121.9727;
		altitudeInFeet = 3000;
		bearing = 0;
		state = INITIAL;
		speedInKnots = 145; // Assuming a fairly typical speed when circling

		memset(flightId, 0, 8);

		_mutex = mutex;
	}

	void SetFlightId(std::string id)
	{
		_mutex->Lock();
		if (id.size() < 8) {
			strcpy(flightId, id.c_str());
		}
		_mutex->Unlock();
	}
	const char* GetFlightId() const 
	{
		return flightId;
	}

	long id;
	LatLong latLong;
	double altitudeInFeet;
	double bearing;
	double speedInKnots;
	FlightState state;

private:
	char flightId[8]; 
	double _turnCenterLat;
	double _turnCenterLong;
	OSMutex *_mutex;
};

// ------------------------------------------------------------------------- //
//
// GeneratorFlightPlan:
// Flight plan class used by the generator.  The only thing it cares about
// in this example is the flight ID and the estimated landing time.
//
// This "radar" does a really simple merge between the flight ID from the 
// first flight plan it receives from the middleware, and the first track that 
// it is sending.
// 
// This is just to illustrate an application that is sending rapid track data, 
// and receiving state data, and in the real world would do a correlation 
// between the two types and send out the merged data. 
//
// ------------------------------------------------------------------------- //
struct GeneratorFlightPlan 
{
	// --- Constructor --- 
	GeneratorFlightPlan()
	{
	}

	// --- Copy constructor --- 
	GeneratorFlightPlan(GeneratorFlightPlan *plan) 
	{
		estimatedHours = plan->estimatedHours;
		estimatedMinute = plan->estimatedMinute;
		strcpy(flightID, plan->flightID);
	}

	char flightID[8];
	short estimatedHours;
	short estimatedMinute;

};


// ------------------------------------------------------------------------- //
//
// TrackListener:
// Abstract base class for listeners that will receive data from the radar
// generator.
//
// ------------------------------------------------------------------------- //
class TrackListener {
public:
	virtual bool TrackUpdate(const GeneratorTrack &track) = 0;
	virtual bool TrackDelete(const GeneratorTrack &track) = 0;

	virtual ~TrackListener() {};
};

// ------------------------------------------------------------------------- //
//
// TrackGenerator:
// Class that creates a thread and then "generates" simple track data landing
// in SFO.  This is based loosely on air traffic patterns that land in SFO,
// but is not meant to take the place of a flight simulator.  This is used
// for generating example data.
//
// This is intended to illustrate the concept of track data coming from a 
// radar and being sent over the middleware.
//
// ------------------------------------------------------------------------- //
class TrackGenerator {

public:

	// --- Constructor --- 
	TrackGenerator(int radarID, int maxTracks, 
					int sendRate) : _shuttingDown(false),
				_currentTrackId(0), _maxTracks(maxTracks),
				_radarID(radarID), _sec(0), 
				_nanosec(100000000), _sendRate(sendRate)
	{
		_mutex = new OSMutex();
	}

	// --- Destructor --- 
	~TrackGenerator();

	// --- Adding a listener to generated radar track events --- 
	// Add a listener that listens for track updates from the radar
	void AddListener(TrackListener *listener) 
	{
		_listeners.push_back(listener);
	}

	// --- Removing a listener to stop listening to radar track events --- 
	// Remove listener that listens for track updates from the radar.  Note that 
	// in this example, the DDSRadarListener must be removed from the 
	// track generator before the RadarInterface is deleted.
	void RemoveListener(const TrackListener *listener);

	// --- Start generating tracks --- 
	// Starts the thread that generates tracks
	void Start();

	// --- Stop generating tracks --- 
	// Marks the thread as ready to shut down
	void Shutdown();


	// --- Add a flight plan to a track or the generator's list --- 
	// Adds a flight plan to the track generator.  Will "correlate" the flight
	// plan with an existing track, if one exists, or else will store the
	// flight plan.
	void AddFlightPlan(GeneratorFlightPlan *flightPlan);

	// --- Update a track with a flight plan --- 
	void UpdateTrackWithFlightData(GeneratorTrack &track, 
		GeneratorFlightPlan &flightPlan);

	// --- Delete a track --- 
	void DeleteTrack(GeneratorTrack &track);

protected:

	// --- Internal generate method --- 
	void GenerateTracks();

	// --- "Correlate" flight plan to track --- 
	// This example does nothing special for "correlating" a flight plan with
	// a track - it just associates a flight plan with the first track that
	// does not have a flight plan associate with it already.
	bool CorrelateFlightPlanWithTrack(GeneratorFlightPlan *flightPlan, 
		GeneratorTrack *track);

private:
	// --- Generate the tracks --- 
	static void *GenerateTracksFunc(void *arg);

	// --- Notify listeners of track updates --- 
	void NotifyListenersUpdateTrack(const GeneratorTrack &track);

	// --- Notify listeners of track deletions --- 
	void NotifyListenersDeleteTrack(const GeneratorTrack &track);

	// --- Create a new track  --- 
	GeneratorTrack* AddTrack();

	// --- Fly to SFO  --- 
	void CalculatePathToSFO(LatLong *currentLatLong, double *bearing, 
							FlightState *state, double sampleRate);
	double CalculateDistance(LatLong latlong1, LatLong latlong2);
	void CalculatePosAroundCircle(LatLong *latLong, double radius, 
								LatLong center, double angleToMove, 
								FlightState *state);
	void CalculateAngleToMovePerPeriod(double *angle, 
								double radius, double centerLat, 
								double centerLong, double kmPerMillisec,
								double sampleRate);
	void FlyToPosition(LatLong *latLong, double *bearing, 
								FlightState *state, double sampleMillisec,
								LatLong endPositionLatLong,
								double speed);
	bool PassedPoint(LatLong origLatLong, LatLong newLatLong, 
								LatLong destination, FlightState state);
	void CalculateNextPosition(LatLong *latLong, double bearing, 
								double sampleMillisec, double speedInKnots);
	void CalculateBearing(double *bearing, 
								LatLong initLatLong, 
								LatLong finalLatLong);

	double KnotsToKph(double knots);
	void CalculateRandomPoint80KmFromSFO(LatLong *latLong);

	void UpdateTrackPositionState(FlightState *state);



	
	GeneratorTrack* GetTrack(int i) const 
	{ 
		return _trackList[i]; 
	}

	int GetMaxTracks() 
	{ 
		return _maxTracks; 
	}

	int GetActiveTrackNumber() 
	{ 
		return _trackList.size(); 
	}

	bool IsShuttingDown() const 
	{ 
		return _shuttingDown; 
	}

	// --- Private members ---

	// The list of currently-updating tracks that the generator is keeping
	// updates about
	std::vector<GeneratorTrack *> _trackList;

	// The list of flight plans that the generator is "correlating" with the
	// track data.  In this case, correlation really is just associating the
	// first flight plan in the queue with the first track that does not
	// have a flight plan associated with it.
	std::queue<GeneratorFlightPlan *> _flightPlans;

	// Used to signal to the track generator that it should stop generating
	// track data, and should shut down.
	bool _shuttingDown;

	// The ID of the next new track created.  Recycle this number when we hit 
	// the max
	int _currentTrackId; 

	// Maximum number of tracks this radar can produce at once.
	int _maxTracks;

	// ID of the radar sensor - this is part of the unique ID of the data
	int _radarID;

	// How quickly the radar is sampling the data
	int _sec;
	int _nanosec;

	// If you want to send faster or slower than real time
	double _sendRate;

	// Listeners that are notified of track events
	std::vector<TrackListener *> _listeners;

	// Wraps the OS-specific mutex
	OSMutex *_mutex;


};

#endif
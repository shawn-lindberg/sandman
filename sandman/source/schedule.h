#pragma once

#include <vector>
#include "rapidjson/document.h"

#include "control.h"

// Types
//

// A schedule event.
struct ScheduleEvent
{
	// Read a schedule event from JSON. 
	//
	// object:	The JSON object representing the event.
	//	
	// Returns:		True if the event was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value const& object);
	
	// Delay in seconds before this entry occurs (since the last).
	unsigned int	m_DelaySec;
	
	// The control action to perform at the scheduled time.
	ControlAction	m_ControlAction;
};

// A schedule.
class Schedule 
{
   public:
      Schedule() = default;

      // Load a schedule from a file.
      //
      // fileName: The name of a file describing the schedule.
      //
      // Returns:    True if the schedule was loaded successfully, false otherwise.
      //
      bool ReadFromFile(const char* fileName);

      // Determines whether the schedule is empty.
      //
      bool IsEmpty() const;

      // Gets the number of events in the schedule.
      //
      size_t GetNumEvents() const;
      
      // Get the events in the schedule.
      // NOTE: This is intended to be const, however current ControlActions prevent that.
      //
      std::vector<ScheduleEvent>& GetEvents();

   private:
      // The list of events making up the schedule.
      std::vector<ScheduleEvent> m_Events;
};


// Functions
//

// Initialize the schedule.
//
void ScheduleInitialize();

// Uninitialize the schedule.
// 
void ScheduleUninitialize();

// Start the schedule.
//
void ScheduleStart();

// Stop the schedule.
//
void ScheduleStop();

// Determine whether the schedule is running.
//
bool ScheduleIsRunning();

// Process the schedule.
//
void ScheduleProcess();


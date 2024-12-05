#pragma once

#include <vector>
#include "rapidjson/document.h"

#include "control.h"

// Types
//

// A routine step.
struct RoutineStep
{
	// Read a routine step from JSON. 
	//
	// object:	The JSON object representing the step.
	//	
	// Returns:		True if the step was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value const& object);
	
	// Delay in seconds before this step occurs (since the last).
	unsigned int	m_delaySec;
	
	// The control action to perform for this step.
	ControlAction	m_controlAction;
};

// A routine.
class Routine 
{
   public:
      Routine() = default;

      // Load a routine from a file.
      //
      // fileName:	The name of a file describing the routine.
      //
      // Returns:		True if the routine was loaded successfully, false otherwise.
      //
      bool ReadFromFile(const char* fileName);

      // Determines whether the routine is empty.
      //
      bool IsEmpty() const;

      // Gets the number of steps in the routine.
      //
      unsigned int GetNumSteps() const;
      
      // Get the steps in the routine.
      //
      std::vector<RoutineStep> const& GetSteps() const;

   private:
      // The list of steps making up the routine.
      std::vector<RoutineStep> m_steps;
};


// Functions
//

// Initialize the routines.
//
// baseDirectory: The base directory for data files.
//
void RoutinesInitialize(std::string const& baseDirectory);

// Uninitialize the routines.
// 
void RoutinesUninitialize();

// Start the routine.
//
void RoutineStart();

// Stop the routine.
//
void RoutineStop();

// Determine whether the routine is running.
//
bool RoutineIsRunning();

// Process the routines.
//
void RoutinesProcess();


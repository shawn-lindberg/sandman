#include "routines.h"

#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "rapidjson/filereadstream.h"
#include "logger.h"
#include "notification.h"
#include "reports.h"
#include "timer.h"


// Constants
//

// Types
//

// Locals
//

// Whether the system is initialized.
static bool s_routinesInitialized = false;

// The directory where routine files are stored.
static std::string s_routinesDirectory;

// The current spot in the routine.
static unsigned int s_routineIndex = UINT_MAX;

// The time the delay for the next step began.
static Time s_routineDelayStartTime;

static Routine s_routine;

// Functions
//

// RoutineStep members

// Read a routine step from JSON. 
//	
// object:	The JSON object representing the step.
//
// Returns:		True if the step was read successfully, false otherwise.
//
bool RoutineStep::ReadFromJSON(rapidjson::Value const& object)
{
	if (object.IsObject() == false)
	{
		Logger::WriteLine("Routine step could not be parsed because it is not an object.");
		return false;
	}

	// We must have a delay.
	auto const delayIterator = object.FindMember("delaySec");

	if (delayIterator == object.MemberEnd())
	{
		Logger::WriteLine("Routine step is missing the delay time.");
		return false;
	}

	if (delayIterator->value.IsInt() == false)
	{
		Logger::WriteLine("Routine step has a delay time, but it's not an integer.");
		return false;
	}

	m_delaySec = delayIterator->value.GetInt();

	// We must also have a control action.
	auto const controlActionIterator = object.FindMember("controlAction");

	if (controlActionIterator == object.MemberEnd())
	{
		Logger::WriteLine("Routine step is missing a control action.");
		return false;
	}

	if (m_controlAction.ReadFromJSON(controlActionIterator->value) == false) 
	{
		Logger::WriteLine("Between step control action could not be parsed.");
		return false;
	}

	return true;
}

// Routine members

// Load a routine from a file.
//
// fileName: The name of a file describing the routine.
//
// Returns:    True if the routine was loaded successfully, false otherwise.
//
bool Routine::ReadFromFile(char const* fileName)
{
	m_steps.clear();

	auto* routineFile = std::fopen(fileName, "r");

	if (routineFile == nullptr)
	{
		Logger::WriteLine(Shell::Red("Failed to open the routine file ", fileName, ".\n"));
		return false;
	}

	static constexpr std::size_t kReadBufferCapacity = 65536u;
	char readBuffer[kReadBufferCapacity];
	rapidjson::FileReadStream routineFileStream(routineFile, readBuffer, sizeof(readBuffer));

	rapidjson::Document routineDocument;
	routineDocument.ParseStream(routineFileStream);

	if (routineDocument.HasParseError() == true)
	{
		Logger::WriteLine(Shell::Red("Failed to parse the routine file ", fileName, ".\n"));
		std::fclose(routineFile);
		return false;
	}

	// Find the list of steps.
	auto const stepsIterator = routineDocument.FindMember("steps");

	if (stepsIterator == routineDocument.MemberEnd())
	{
		Logger::WriteLine("No routine steps in ", fileName, ".\n");
		std::fclose(routineFile);
		return false;		
	}

	if (stepsIterator->value.IsArray() == false)
	{
		std::fclose(routineFile);
		Logger::WriteLine("No steps array in ", fileName, ".\n");
		return false;
	}

	// Try to load each step in turn.
	for (auto const& stepObject : stepsIterator->value.GetArray())
	{
		// Try to read the step.
		RoutineStep step;
		if (step.ReadFromJSON(stepObject) == false)
		{
			continue;
		}
					
		// If we successfully read the step, add it to the list.
		m_steps.push_back(step);
	}

	std::fclose(routineFile);
	return true;
}

// Determines whether the routine is empty.
//
bool Routine::IsEmpty() const
{
	return (m_steps.size() == 0);
}

// Gets the number of steps in the routine.
//
unsigned int Routine::GetNumSteps() const
{
	return m_steps.size();
}

// Get the steps in the routine.
//
std::vector<RoutineStep> const& Routine::GetSteps() const
{
	return m_steps;
}

// Functions
//

// Load the routine from a file.
//
static bool RoutineLoad()
{
	auto const routineFile = s_routinesDirectory + "sandman.rtn";
	return s_routine.ReadFromFile(routineFile.c_str());
}

// Write the loaded routine to the logger.
//
static void RoutineLogLoaded()
{
	// Now write out the routine.
	Logger::WriteLine("The following routine is loaded:");
	
	if (s_routine.IsEmpty() == true)
	{
		Logger::WriteLine("\t<empty>");
		Logger::WriteLine();
		return;
	}

	for (auto const& step : s_routine.GetSteps())
	{
		// Split the delay into multiple units.
		auto delaySec = step.m_delaySec;
		
		auto const delayHours = delaySec / 3600;
		delaySec %= 3600;
		
		auto const delayMin = delaySec / 60;
		delaySec %= 60;
		
		auto const* actionText = (step.m_controlAction.m_action == Control::kActionMovingUp) ? 
			"up" : "down";
			
		// Print the event.
		Logger::WriteLine(std::setfill('0'),

								"\t+",

								std::setw(1), delayHours, "h ",
								std::setw(2), delayMin  , "m ",
								std::setw(2), delaySec  , "s "

								"-> ", step.m_controlAction.m_controlName, ", ", actionText,

								// Restore fill to space.
								std::setfill(' ')

								/* `std::setw` only applies once, so it doesn't need to be restored. */);
	}

	Logger::WriteLine();
}

// Initialize the routines.
//
// baseDirectory: The base directory for data files.
//
void RoutinesInitialize(std::string const& baseDirectory)
{	
	s_routineIndex = UINT_MAX;
	
	Logger::WriteLine("Initializing the routines...");

	// Create the routines directory, if necessary.
	s_routinesDirectory = baseDirectory + "routines/";

	if (std::filesystem::exists(s_routinesDirectory) == false)
	{
		if (std::filesystem::create_directory(s_routinesDirectory) == false)
		{
			Logger::WriteLine(Shell::Red("Routines directory \""), s_routinesDirectory, 
									Shell::Red("\" does not exist and failed to be created."));
			return;
		}
	}

	// Parse the routine.
	if (RoutineLoad() == false)
	{
		Logger::WriteLine('\t', Shell::Red("failed"));
		return;
	}

	Logger::WriteLine('\t', Shell::Green("succeeded"));
	Logger::WriteLine();
	
	// Log the routine that just got loaded.
	RoutineLogLoaded();
	
	s_routinesInitialized = true;
}

// Uninitialize the routines.
// 
void RoutinesUninitialize()
{
	if (s_routinesInitialized == false)
	{
		return;
	}
	
	s_routinesInitialized = false;
}

// Start the routine.
//
void RoutineStart()
{
	// Add the report item prior to checks, because we want to record the intent.
	ReportsAddRoutineItem("start");

	// Make sure it's initialized.
	if (s_routinesInitialized == false)
	{
		return;
	}
	
	// Make sure it's not running.
	if (RoutineIsRunning() == true)
	{
		return;
	}
	
	s_routineIndex = 0u;
	TimerGetCurrent(s_routineDelayStartTime);
	
	// Notify.
	NotificationPlay("routine_start");
	
	Logger::WriteLine("Routine started.");
}

// Stop the routine.
//
void RoutineStop()
{
	// Add the report item prior to checks, because we want to record the intent.
	ReportsAddRoutineItem("stop");

	// Make sure it's initialized.
	if (s_routinesInitialized == false)
	{
		return;
	}
	
	// Make sure it's running.
	if (RoutineIsRunning() == false)
	{
		return;
	}
	
	s_routineIndex = UINT_MAX;
	
	// Notify.
	NotificationPlay("routine_stop");
	
	Logger::WriteLine("Routine stopped.");
}

// Determine whether the routine is running.
//
bool RoutineIsRunning()
{
	return (s_routineIndex != UINT_MAX);
}

// Process the routines.
//
void RoutinesProcess()
{
	// Make sure it's initialized.
	if (s_routinesInitialized == false)
	{
		return;
	}
	
	// Running?
	if (RoutineIsRunning() == false)
	{
		return;
	}

	// No need to do anything for routines with zero steps.
	auto const numSteps = s_routine.GetNumSteps();
	if (numSteps == 0u)
	{
		return;
	}

	// Get elapsed time since delay start.
	Time currentTime;
	TimerGetCurrent(currentTime);

	auto const elapsedTimeSec = 
		TimerGetElapsedMilliseconds(s_routineDelayStartTime, currentTime) / 1000.0f;

	// Time up?
	auto& step = s_routine.GetSteps()[s_routineIndex];
	
	if (elapsedTimeSec < step.m_delaySec)
	{
		return;
	}
	
	// Move to the next step.
	s_routineIndex = (s_routineIndex + 1u) % numSteps;
	
	// Set the new delay start time.
	TimerGetCurrent(s_routineDelayStartTime);
	
	// Sanity check the step.
	if (step.m_controlAction.m_action >= Control::kNumActions)
	{
		Logger::WriteLine("Routine moving to step ", s_routineIndex, ".");
		return;
	}

	// Try to find the control to perform the action.
	auto* control = step.m_controlAction.GetControl();
	
	if (control == nullptr)
	{
		Logger::WriteLine("Routine couldn't find control \"", step.m_controlAction.m_controlName,
								"\". Moving to step ", s_routineIndex, ".");
		return;
	}
		
	// Perform the action.
	control->SetDesiredAction(step.m_controlAction.m_action, Control::kModeTimed);
	
	ReportsAddControlItem(control->GetName(), step.m_controlAction.m_action, "routine");

	Logger::WriteLine("Routine moving to step ", s_routineIndex, ".");
}

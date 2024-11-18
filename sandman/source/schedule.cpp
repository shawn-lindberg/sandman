#include "schedule.h"

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
static bool s_scheduleInitialized = false;

// The directory where routine files are stored.
static std::string s_routinesDirectory;

// The current spot in the schedule.
static unsigned int s_scheduleIndex = UINT_MAX;

// The time the delay for the next event began.
static Time s_scheduleDelayStartTime;

static Schedule s_schedule;

// Functions
//

// ScheduleEvent members

// Read a schedule event from JSON. 
//	
// object:	The JSON object representing the event.
//
// Returns:		True if the event was read successfully, false otherwise.
//
bool ScheduleEvent::ReadFromJSON(rapidjson::Value const& object)
{
	if (object.IsObject() == false)
	{
		Logger::WriteLine("Schedule event could not be parsed because it is not an object.");
		return false;
	}

	// We must have a delay.
	auto const delayIterator = object.FindMember("delaySec");

	if (delayIterator == object.MemberEnd())
	{
		Logger::WriteLine("Schedule event is missing the delay time.");
		return false;
	}

	if (delayIterator->value.IsInt() == false)
	{
		Logger::WriteLine("Schedule event has a delay time, but it's not an integer.");
		return false;
	}

	m_delaySec = delayIterator->value.GetInt();

	// We must also have a control action.
	auto const controlActionIterator = object.FindMember("controlAction");

	if (controlActionIterator == object.MemberEnd())
	{
		Logger::WriteLine("Schedule event is missing a control action.");
		return false;
	}

	if (m_controlAction.ReadFromJSON(controlActionIterator->value) == false) 
	{
		Logger::WriteLine("Schedule event control action could not be parsed.");
		return false;
	}

	return true;
}

// Schedule members

// Load a schedule from a file.
//
// fileName: The name of a file describing the schedule.
//
// Returns:    True if the schedule was loaded successfully, false otherwise.
//
bool Schedule::ReadFromFile(char const* fileName)
{
	m_events.clear();

	auto* scheduleFile = std::fopen(fileName, "r");

	if (scheduleFile == nullptr)
	{
		Logger::WriteLine(Shell::Red("Failed to open the schedule file ", fileName, ".\n"));
		return false;
	}

	static constexpr std::size_t kReadBufferCapacity{ 65536u };
	char readBuffer[kReadBufferCapacity];
	rapidjson::FileReadStream scheduleFileStream(scheduleFile, readBuffer, 
		sizeof(readBuffer));

	rapidjson::Document scheduleDocument;
	scheduleDocument.ParseStream(scheduleFileStream);

	if (scheduleDocument.HasParseError() == true)
	{
		Logger::WriteLine(Shell::Red("Failed to parse the schedule file ", fileName, ".\n"));
		std::fclose(scheduleFile);
		return false;
	}

	// Find the list of events.
	auto const eventsIterator = scheduleDocument.FindMember("events");

	if (eventsIterator == scheduleDocument.MemberEnd())
	{
		Logger::WriteLine("No schedule events in ", fileName, ".\n");
		std::fclose(scheduleFile);
		return false;		
	}

	if (eventsIterator->value.IsArray() == false)
	{
		Logger::WriteLine("Schedule has events, but it is not an array.");
		std::fclose(scheduleFile);
		Logger::WriteLine("No event array in ", fileName, ".\n");
		return false;
	}

	// Try to load each event in turn.
	for (auto const& eventObject : eventsIterator->value.GetArray())
	{
		// Try to read the event.
		ScheduleEvent event;
		if (event.ReadFromJSON(eventObject) == false)
		{
			continue;
		}
					
		// If we successfully read the event, add it to the list.
		m_events.push_back(event);
	}

	std::fclose(scheduleFile);
	return true;
}

// Determines whether the schedule is empty.
//
bool Schedule::IsEmpty() const
{
	return (m_events.size() == 0);
}

// Gets the number of events in the schedule.
//
size_t Schedule::GetNumEvents() const
{
	return m_events.size();
}

// Get the events in the schedule.
// NOTE: This is intended to be const, however current ControlActions prevent that.
//
std::vector<ScheduleEvent>& Schedule::GetEvents()
{
	return m_events;
}

// Functions
//

// Load the schedule from a file.
//
static bool ScheduleLoad()
{
	auto const routineFile = s_routinesDirectory + "sandman.rtn";
	return s_schedule.ReadFromFile(routineFile.c_str());
}

// Write the loaded schedule to the logger.
//
static void ScheduleLogLoaded()
{
	// Now write out the schedule.
	Logger::WriteLine("The following schedule is loaded:");
	
	if (s_schedule.IsEmpty())
	{
		Logger::WriteLine("\t<empty>");
		Logger::WriteLine();
		return;
	}

	for (auto const& event : s_schedule.GetEvents())
	{
		// Split the delay into multiple units.
		auto delaySec = event.m_delaySec;
		
		auto const delayHours = delaySec / 3600;
		delaySec %= 3600;
		
		auto const delayMin = delaySec / 60;
		delaySec %= 60;
		
		auto const* actionText = (event.m_controlAction.m_action == Control::kActionMovingUp) ? 
			"up" : "down";
			
		// Print the event.
		Logger::WriteLine(std::setfill('0'),

								"\t+",

								std::setw(1), delayHours, "h ",
								std::setw(2), delayMin  , "m ",
								std::setw(2), delaySec  , "s "

								"-> ", event.m_controlAction.m_controlName, ", ", actionText,

								/* Restore fill to space. */
								std::setfill(' ')

								/* `std::setw` only applies once, so it doesn't need to be restored. */);
	}

	Logger::WriteLine();
}

// Initialize the schedule.
//
// baseDirectory: The base directory for data files.
//
void ScheduleInitialize(std::string const& baseDirectory)
{	
	s_scheduleIndex = UINT_MAX;
	
	Logger::WriteLine("Initializing the schedule...");

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

	// Parse the schedule.
	if (ScheduleLoad() == false)
	{
		Logger::WriteLine('\t', Shell::Red("failed"));
		return;
	}

	Logger::WriteLine('\t', Shell::Green("succeeded"));
	Logger::WriteLine();
	
	// Log the schedule that just got loaded.
	ScheduleLogLoaded();
	
	s_scheduleInitialized = true;
}

// Uninitialize the schedule.
// 
void ScheduleUninitialize()
{
	if (s_scheduleInitialized == false)
	{
		return;
	}
	
	s_scheduleInitialized = false;
}

// Start the schedule.
//
void ScheduleStart()
{
	// Add the report item prior to checks, because we want to record the intent.
	ReportsAddScheduleItem("start");

	// Make sure it's initialized.
	if (s_scheduleInitialized == false)
	{
		return;
	}
	
	// Make sure it's not running.
	if (ScheduleIsRunning() == true)
	{
		return;
	}
	
	s_scheduleIndex = 0;
	TimerGetCurrent(s_scheduleDelayStartTime);
	
	// Notify.
	NotificationPlay("schedule_start");
	
	Logger::WriteLine("Schedule started.");
}

// Stop the schedule.
//
void ScheduleStop()
{
	// Add the report item prior to checks, because we want to record the intent.
	ReportsAddScheduleItem("stop");

	// Make sure it's initialized.
	if (s_scheduleInitialized == false)
	{
		return;
	}
	
	// Make sure it's running.
	if (ScheduleIsRunning() == false)
	{
		return;
	}
	
	s_scheduleIndex = UINT_MAX;
	
	// Notify.
	NotificationPlay("schedule_stop");
	
	Logger::WriteLine("Schedule stopped.");
}

// Determine whether the schedule is running.
//
bool ScheduleIsRunning()
{
	return (s_scheduleIndex != UINT_MAX);
}

// Process the schedule.
//
void ScheduleProcess()
{
	// Make sure it's initialized.
	if (s_scheduleInitialized == false)
	{
		return;
	}
	
	// Running?
	if (ScheduleIsRunning() == false)
	{
		return;
	}

	// No need to do anything for schedules with zero events.
	auto const scheduleEventCount = static_cast<unsigned int>(s_schedule.GetNumEvents());
	if (scheduleEventCount == 0)
	{
		return;
	}

	// Get elapsed time since delay start.
	Time currentTime;
	TimerGetCurrent(currentTime);

	auto const elapsedTimeSec = 
		TimerGetElapsedMilliseconds(s_scheduleDelayStartTime, currentTime) / 1000.0f;

	// Time up?
	auto& event = s_schedule.GetEvents()[s_scheduleIndex];
	
	if (elapsedTimeSec < event.m_delaySec)
	{
		return;
	}
	
	// Move to the next event.
	s_scheduleIndex = (s_scheduleIndex + 1) % scheduleEventCount;
	
	// Set the new delay start time.
	TimerGetCurrent(s_scheduleDelayStartTime);
	
	// Sanity check the event.
	if (event.m_controlAction.m_action >= Control::kNumActions)
	{
		Logger::WriteLine("Schedule moving to event ", s_scheduleIndex, ".");
		return;
	}

	// Try to find the control to perform the action.
	auto* control = event.m_controlAction.GetControl();
	
	if (control == nullptr) {
		Logger::WriteLine("Schedule couldn't find control \"", event.m_controlAction.m_controlName,
								"\". Moving to event ", s_scheduleIndex, ".");
		return;
	}
		
	// Perform the action.
	control->SetDesiredAction(event.m_controlAction.m_action, Control::kModeTimed);
	
	ReportsAddControlItem(control->GetName(), event.m_controlAction.m_action, "schedule");

	Logger::WriteLine("Schedule moving to event ", s_scheduleIndex, ".");
}

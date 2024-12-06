#include "reports.h"

#include <mutex>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "logger.h"

#define REPORT_VERSION	3
//	1					Initial version.
// 2	2023/08/29	Adding the report start time to the header, for use when analyzing the data.
// 3	2024/02/04	Adding support for schedule items and distinguishing the source of movement items.

// Eventually this should be configurable.
#define REPORT_STARTING_HOUR	17

// Types
//

// An item we want to put in the report later.
// 
struct PendingItem
{
	// The time the item was added.
	time_t m_rawTime;

	// The string version of the JSON description of the item to put into the report.
	std::string	m_eventString;
};

// Locals
//

// Used to enforce serialization of messages.
std::mutex s_reportMutex;

// The directory where report files are stored.
static std::string s_reportsDirectory;

// The file to report to.
static FILE* s_reportFile = nullptr;

// The string representing the date of the currently open report file.
static std::string s_reportDateString;

// A list of items to add to the report when we are able to.
static std::vector<PendingItem> s_pendingItemList;

// The names of the actions.
static constexpr std::array kControlActionNames =
{
	"stop",			// kActionStopped
	"move up",		// kActionMovingUp
	"move down",	// kActionMovingDown
};

// Functions
//

// Determine the effective date that we should be using for the report now.
//
static std::string ReportsGetEffectiveDate()
{
	// Get the current time.
	auto const rawTime = time(nullptr);
	auto* localTime = localtime(&rawTime);

	// If the time is after the starting hour, advance the day.
	if (localTime->tm_hour >= REPORT_STARTING_HOUR)
	{
		localTime->tm_mday++;
	}

	// Get the report date.
	auto const rawReportTime = mktime(localTime);
	auto* reportTime = localtime(&rawReportTime);

	// Put the date in the buffer in 2012-09-23 format.
	static constexpr std::size_t kReportDateBufferCapacity{ 128u };
	char reportDateBuffer[kReportDateBufferCapacity];
	strftime(reportDateBuffer, kReportDateBufferCapacity, "%Y-%m-%d", reportTime);

	return std::string(reportDateBuffer);
}

// Determine the date and time that we should be using for the report now.
//
static std::string ReportsGetStartingDateTime()
{
	// Get the current time.
	auto const rawTime = time(nullptr);
	auto* localTime = localtime(&rawTime);

	// If the time is before the starting hour, go back one day.
	if (localTime->tm_hour < REPORT_STARTING_HOUR)
	{
		localTime->tm_mday--;
	}

	// We also need to set the time to the starting time.
	localTime->tm_hour = REPORT_STARTING_HOUR;
	localTime->tm_min = 0;
	localTime->tm_sec = 0;

	// Get the starting time.
	auto const rawStartingTime = mktime(localTime);
	auto* startingTime = localtime(&rawStartingTime);

	// Put the date and time in the buffer in 2012/09/23 17:44:05 CDT format.
	static constexpr std::size_t kTimeStringBufferCapacity{ 128u };
	char timeStringBuffer[kTimeStringBufferCapacity];
	strftime(timeStringBuffer, kTimeStringBufferCapacity, "%Y/%m/%d %H:%M:%S %Z", startingTime);

	return std::string(timeStringBuffer);
}

// Opens the appropriate report file corresponding to the effective date.
// 
static void ReportsOpenFile()
{	
	// Get the date that we should currently be using.
	auto const currentReportDateString = ReportsGetEffectiveDate();

	// If the correct file is open, we don't need to do anything else.
	if ((s_reportFile != nullptr) && (s_reportDateString.compare(currentReportDateString) == 0))
	{
		return;
	}

	// If necessary, close the previous file.
	if (s_reportFile != nullptr)
	{
		Logger::WriteLine("Closing report file for ", s_reportDateString, ".");

		std::fclose(s_reportFile);
		s_reportFile = nullptr;
	}

	s_reportDateString = "";

	std::string const reportFileName = 
		s_reportsDirectory + "sandman" + currentReportDateString + ".rpt";

	// First, see if the file already exists.
	bool reportAlreadyExisted = false;
	
	// Note: There are other ways to check for file existence, but we don't care about optimal 
	// performance here.
	s_reportFile = std::fopen(reportFileName.c_str(), "r");
	
	if (s_reportFile != nullptr) {

		// If we were able to open it, it must exist. So update the flag and close it so we can open 
		// it in the correct mode.
		reportAlreadyExisted = true;

		std::fclose(s_reportFile);
		s_reportFile = nullptr;
	}

	// Open the file for appending.
	Logger::WriteLine((reportAlreadyExisted == true) ? "Opening" : "Creating", " report file ",
							reportFileName, "...");

	// This mode works regardless of whether the file exists or not.
	s_reportFile = std::fopen(reportFileName.c_str(), "a");

	if (s_reportFile == nullptr)
	{
		Logger::WriteLine('\t', Shell::Red("failed"));
		return;
	}

	Logger::WriteLine('\t', Shell::Green("succeeded"));

	// Now that we have successfully opened the file, update the date string.
	s_reportDateString = currentReportDateString;

	// If this is a new report file, write out the header.
	if (reportAlreadyExisted == true)
	{
		return;
	}
	
	// Make a JSON representation of the header.
	rapidjson::Document headerDocument;
	headerDocument.SetObject();

	auto headerAllocator = headerDocument.GetAllocator();

	headerDocument.AddMember("version", REPORT_VERSION, headerAllocator);

	// Also include the starting time. It is safe to use a reference here because the document has 
	// the same lifetime as the string copy.
	auto startingTime = ReportsGetStartingDateTime();
	
	headerDocument.AddMember("startingTime", 
		rapidjson::Value(rapidjson::StringRef(startingTime.c_str())), headerAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer headerBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> headerWriter(headerBuffer);
	headerDocument.Accept(headerWriter);
 
	// Write the whole thing.
	fputs(headerBuffer.GetString(), s_reportFile);

	// fputs doesn't add a newline, do it now.
	fputs("\n", s_reportFile);
}

// Initialize the reports.
//
// baseDirectory:	The base directory for files.
//
void ReportsInitialize(std::string const& baseDirectory)
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> reportGuard(s_reportMutex);

	Logger::WriteLine("Initializing reports...");

	// Initialize the file.
	s_reportFile = nullptr;

	// An empty string indicates that we don't have a report file open.
	s_reportDateString = "";

	// Create the reports directory, if necessary.
	s_reportsDirectory = baseDirectory + "reports/";

	if (std::filesystem::exists(s_reportsDirectory) == false)
	{
		if (std::filesystem::create_directory(s_reportsDirectory) == false)
		{
			Logger::WriteLine(Shell::Red("Reports directory \""), s_reportsDirectory, 
									Shell::Red("\" does not exist and failed to be created."));
			return;
		}
	}

	// Open the correct file for now.
	ReportsOpenFile();
}

// Uninitialize the reports.
//
void ReportsUninitialize()
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> reportGuard(s_reportMutex);

	// Close the file.
	if (s_reportFile != nullptr)
	{
		std::fclose(s_reportFile);
	}

	s_reportFile = nullptr;
}

// Write an item into the report.
//
// item:	The item to write out.
//
static void ReportsWriteItem(PendingItem const& item)
{
	static constexpr std::size_t kTimeStringBufferCapacity{ 512u };
	char timeStringBuffer[kTimeStringBufferCapacity];

	// Get the time.
	auto* localTime = localtime(&item.m_rawTime);

	// Put the date and time in the buffer in 2012/09/23 17:44:05 CDT format.
	strftime(timeStringBuffer, kTimeStringBufferCapacity, "%Y/%m/%d %H:%M:%S %Z", localTime);
	
	// Can subtract one from this without underflow.
	static_assert(kTimeStringBufferCapacity >= 1u);

	// Force terminate.
	timeStringBuffer[kTimeStringBufferCapacity - 1u] = '\0';

	// Parse the event string back into JSON.
	rapidjson::Document eventDocument;
	eventDocument.Parse(item.m_eventString.c_str());

	if (eventDocument.HasParseError() == true)
	{
		Logger::WriteLine(Shell::Red("Failed to convert report event string back into JSON \"",
											  item.m_eventString, "\"."));
		return;
	}

	// Make a JSON representation of this item.
	rapidjson::Document itemDocument;
	itemDocument.SetObject();

	auto itemAllocator = itemDocument.GetAllocator();

	// It is safe to use a string reference here because this document will not live outside of this 
	// scope.
	itemDocument.AddMember("dateTime", 
		rapidjson::Value(rapidjson::StringRef(timeStringBuffer)), itemAllocator);

	// Add the event object.
	itemDocument.AddMember("event", eventDocument.GetObject(), itemAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer itemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> itemWriter(itemBuffer);
	itemDocument.Accept(itemWriter);
 	
	// Write the whole thing.
	fputs(itemBuffer.GetString(), s_reportFile);

	// fputs doesn't add a newline, do it now.
	fputs("\n", s_reportFile);
}

// Process the reports.
//
void ReportsProcess()
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> reportGuard(s_reportMutex);

	if (s_reportFile != nullptr)
	{
		// We are going to write out any pending items first, before we check whether we need to 
		// switch the file.
		for (auto const& item : s_pendingItemList)
		{
			ReportsWriteItem(item);
		}

		s_pendingItemList.clear();

		fflush(s_reportFile);
	}

	// Make sure we have the correct file open.
	ReportsOpenFile();
}

// Add an item to the report.
// 
// eventString:	The string version of the JSON for the event.
//
static void ReportsAddItem(std::string const& eventString)
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> reportGuard(s_reportMutex);

	PendingItem pendingItem;
	pendingItem.m_rawTime = time(nullptr);
	pendingItem.m_eventString = eventString;

	s_pendingItemList.push_back(pendingItem);
}

// Add an item to the report corresponding to a control event.
//
// controlName:	The name of the control.
// action:		The action performed on the control.
// sourceName:	An identifier for where this item comes from.
//
void ReportsAddControlItem(std::string const& controlName, Control::Actions const action,
									std::string const& sourceName)
{
	if ((action < 0) || (action >= Control::kNumActions))
	{
		Logger::WriteLine("Could not add control item to the report "
								"because it contains an invalid action ",
								action, "!");
		return;
	}

	// Make a JSON representation of this item.
	rapidjson::Document itemDocument;
	itemDocument.SetObject();

	auto itemAllocator = itemDocument.GetAllocator();

	itemDocument.AddMember("type", 
		rapidjson::Value(rapidjson::StringRef("control")), itemAllocator);	

	// It is safe to use a string reference here because this document will not live outside of this 
	// scope.
	itemDocument.AddMember("control", 
		rapidjson::Value(rapidjson::StringRef(controlName.c_str())), itemAllocator);

	itemDocument.AddMember("action", 
		rapidjson::Value(rapidjson::StringRef(kControlActionNames[action])), itemAllocator);

	itemDocument.AddMember("source", 
		rapidjson::Value(rapidjson::StringRef(sourceName.c_str())), itemAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer itemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> itemWriter(itemBuffer);
	itemDocument.Accept(itemWriter);

	// Handle the rest.
	ReportsAddItem(itemBuffer.GetString());
}

// Add an item to the report corresponding to a routine event.
// 
// actionName:	The name of the routine action.
// 
void ReportsAddRoutineItem(std::string const& actionName)
{
	// Make a JSON representation of this item.
	rapidjson::Document itemDocument;
	itemDocument.SetObject();

	auto itemAllocator = itemDocument.GetAllocator();

	itemDocument.AddMember("type", 
		rapidjson::Value(rapidjson::StringRef("routine")), itemAllocator);	

	// It is safe to use a string reference here because this document will not live outside of this 
	// scope.
	itemDocument.AddMember("action", 
		rapidjson::Value(rapidjson::StringRef(actionName.c_str())), itemAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer itemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> itemWriter(itemBuffer);
	itemDocument.Accept(itemWriter);

	// Handle the rest.
	ReportsAddItem(itemBuffer.GetString());
}

// Add an item to the report corresponding to a status event.
// 
void ReportsAddStatusItem()
{
	// Make a JSON representation of this item.
	rapidjson::Document itemDocument;
	itemDocument.SetObject();

	auto itemAllocator = itemDocument.GetAllocator();

	itemDocument.AddMember("type", 
		rapidjson::Value(rapidjson::StringRef("status")), itemAllocator);	
	
	// Write this into a string.
	rapidjson::StringBuffer itemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> itemWriter(itemBuffer);
	itemDocument.Accept(itemWriter);

	// Handle the rest.
	ReportsAddItem(itemBuffer.GetString());
}
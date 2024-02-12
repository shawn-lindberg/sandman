#include "reports.h"

#include <mutex>
#include <stdio.h>
#include <time.h>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "logger.h"

#define TEMPDIR	AM_TEMPDIR

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
	time_t m_RawTime;

	// The string version of the JSON description of the item to put into the report.
	std::string	m_EventString;
};

// Locals
//

// Used to enforce serialization of messages.
std::mutex s_ReportMutex;

// The file to report to.
static FILE* s_ReportFile = nullptr;

// The string representing the date of the currently open report file.
static std::string s_ReportDateString;

// A list of items to add to the report when we are able to.
static std::vector<PendingItem> s_PendingItemList;

// The names of the actions.
static char const* const s_ControlActionNames[] =
{
	"stop",			// ACTION_STOPPED
	"move up",		// ACTION_MOVING_UP
	"move down",	// ACTION_MOVING_DOWN
};

// Functions
//

// Determine the effective date that we should be using for the report now.
//
static std::string ReportsGetEffectiveDate()
{
	// Get the current time.
	auto const l_RawTime = time(nullptr);
	auto* l_LocalTime = localtime(&l_RawTime);

	// If the time is after the starting hour, advance the day.
	if (l_LocalTime->tm_hour >= REPORT_STARTING_HOUR)
	{
		l_LocalTime->tm_mday++;
	}

	// Get the report date.
	auto const l_RawReportTime = mktime(l_LocalTime);
	auto* l_ReportTime = localtime(&l_RawReportTime);

	// Put the date in the buffer in 2012-09-23 format.
	static unsigned int const l_ReportDateBufferCapacity = 128;
	char l_ReportDateBuffer[l_ReportDateBufferCapacity];
	strftime(l_ReportDateBuffer, l_ReportDateBufferCapacity, "%Y-%m-%d", l_ReportTime);

	return std::string(l_ReportDateBuffer);
}

// Determine the date and time that we should be using for the report now.
//
static std::string ReportsGetStartingDateTime()
{
	// Get the current time.
	auto const l_RawTime = time(nullptr);
	auto* l_LocalTime = localtime(&l_RawTime);

	// If the time is before the starting hour, go back one day.
	if (l_LocalTime->tm_hour < REPORT_STARTING_HOUR)
	{
		l_LocalTime->tm_mday--;
	}

	// We also need to set the time to the starting time.
	l_LocalTime->tm_hour = REPORT_STARTING_HOUR;
	l_LocalTime->tm_min = 0;
	l_LocalTime->tm_sec = 0;

	// Get the starting time.
	auto const l_RawStartingTime = mktime(l_LocalTime);
	auto* l_StartingTime = localtime(&l_RawStartingTime);

	// Put the date and time in the buffer in 2012/09/23 17:44:05 CDT format.
	static unsigned int const l_TimeStringBufferCapacity = 128;
	char l_TimeStringBuffer[l_TimeStringBufferCapacity];
	strftime(l_TimeStringBuffer, l_TimeStringBufferCapacity, "%Y/%m/%d %H:%M:%S %Z", l_StartingTime);

	return std::string(l_TimeStringBuffer);
}

// Opens the appropriate report file corresponding to the effective date.
// 
static void ReportsOpenFile()
{	
	// Get the date that we should currently be using.
	auto const l_CurrentReportDateString = ReportsGetEffectiveDate();

	// If the correct file is open, we don't need to do anything else.
	if ((s_ReportFile != nullptr) && (s_ReportDateString.compare(l_CurrentReportDateString) == 0))
	{
		return;
	}

	// If necessary, close the previous file.
	if (s_ReportFile != nullptr)
	{
		LoggerAddMessage("Closing report file for %s.", s_ReportDateString.c_str());

		fclose(s_ReportFile);
		s_ReportFile = nullptr;
	}

	s_ReportDateString = "";

	std::string const l_ReportFileName = TEMPDIR "reports/sandman" + l_CurrentReportDateString + 
		".rpt";

	// First, see if the file already exists.
	bool l_ReportAlreadyExisted = false;
	
	// Note: There are other ways to check for file existence, but we don't care about optimal 
	// performance here.
	s_ReportFile = fopen(l_ReportFileName.c_str(), "r");
	
	if (s_ReportFile != nullptr) {

		// If we were able to open it, it must exist. So update the flag and close it so we can open 
		// it in the correct mode.
		l_ReportAlreadyExisted = true;

		fclose(s_ReportFile);
		s_ReportFile = nullptr;
	}

	// Open the file for appending.
	LoggerAddMessage("%s report file %s...", (l_ReportAlreadyExisted == true) ? "Opening" : 
		"Creating", l_ReportFileName.c_str());

	// This mode works regardless of whether the file exists or not.
	s_ReportFile = fopen(l_ReportFileName.c_str(), "a");

	if (s_ReportFile == nullptr)
	{
		LoggerAddMessage("\tfailed");
		return;
	}

	LoggerAddMessage("\tsucceeded");

	// Now that we have successfully opened the file, update the date string.
	s_ReportDateString = l_CurrentReportDateString;

	// If this is a new report file, write out the header.
	if (l_ReportAlreadyExisted == true)
	{
		return;
	}
	
	// Make a JSON representation of the header.
	rapidjson::Document l_HeaderDocument;
	l_HeaderDocument.SetObject();

	auto l_HeaderAllocator = l_HeaderDocument.GetAllocator();

	l_HeaderDocument.AddMember("version", REPORT_VERSION, l_HeaderAllocator);

	// Also include the starting time. It is safe to use a reference here because the document has 
	// the same lifetime as the string copy.
	auto l_StartingTime = ReportsGetStartingDateTime();
	
	l_HeaderDocument.AddMember("startingTime", 
		rapidjson::Value(rapidjson::StringRef(l_StartingTime.c_str())), l_HeaderAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer l_HeaderBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> l_HeaderWriter(l_HeaderBuffer);
	l_HeaderDocument.Accept(l_HeaderWriter);
 
	// Write the whole thing.
	fputs(l_HeaderBuffer.GetString(), s_ReportFile);

	// fputs doesn't add a newline, do it now.
	fputs("\n", s_ReportFile);
}

// Initialize the reports.
//
void ReportsInitialize()
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> l_ReportGuard(s_ReportMutex);

	LoggerAddMessage("Initializing reports...");

	// Initialize the file.
	s_ReportFile = nullptr;

	// An empty string indicates that we don't have a report file open.
	s_ReportDateString = "";

	// Open the correct file for now.
	ReportsOpenFile();
}

// Uninitialize the reports.
//
void ReportsUninitialize()
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> l_ReportGuard(s_ReportMutex);

	// Close the file.
	if (s_ReportFile != nullptr)
	{
		fclose(s_ReportFile);
	}

	s_ReportFile = nullptr;
}

// Write an item into the report.
//
// p_Item:	The item to write out.
//
static void ReportsWriteItem(PendingItem const& p_Item)
{
	static unsigned int const l_TimeStringBufferCapacity = 512;
	char l_TimeStringBuffer[l_TimeStringBufferCapacity];

	// Get the time.
	auto* l_LocalTime = localtime(&p_Item.m_RawTime);

	// Put the date and time in the buffer in 2012/09/23 17:44:05 CDT format.
	strftime(l_TimeStringBuffer, l_TimeStringBufferCapacity, "%Y/%m/%d %H:%M:%S %Z", l_LocalTime);
	
	// Force terminate.
	l_TimeStringBuffer[l_TimeStringBufferCapacity - 1] = '\0';

	// Parse the event string back into JSON.
	rapidjson::Document l_EventDocument;
	l_EventDocument.Parse(p_Item.m_EventString.c_str());

	if (l_EventDocument.HasParseError() == true)
	{
		LoggerAddMessage("Failed to convert report event string back into JSON \"%s\".", 
			p_Item.m_EventString.c_str());
		return;
	}

	// Make a JSON representation of this item.
	rapidjson::Document l_ItemDocument;
	l_ItemDocument.SetObject();

	auto l_ItemAllocator = l_ItemDocument.GetAllocator();

	// It is safe to use a string reference here because this document will not live outside of this 
	// scope.
	l_ItemDocument.AddMember("dateTime", 
		rapidjson::Value(rapidjson::StringRef(l_TimeStringBuffer)), l_ItemAllocator);

	// Add the event object.
	l_ItemDocument.AddMember("event", l_EventDocument.GetObject(), l_ItemAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer l_ItemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> l_ItemWriter(l_ItemBuffer);
	l_ItemDocument.Accept(l_ItemWriter);
 	
	// Write the whole thing.
	fputs(l_ItemBuffer.GetString(), s_ReportFile);

	// fputs doesn't add a newline, do it now.
	fputs("\n", s_ReportFile);
}

// Process the reports.
//
void ReportsProcess()
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> l_ReportGuard(s_ReportMutex);

	if (s_ReportFile != nullptr)
	{
		// We are going to write out any pending items first, before we check whether we need to 
		// switch the file.
		for (auto const& l_Item : s_PendingItemList)
		{
			ReportsWriteItem(l_Item);
		}

		s_PendingItemList.clear();

		fflush(s_ReportFile);
	}

	// Make sure we have the correct file open.
	ReportsOpenFile();
}

// Add an item to the report.
// 
// p_EventString:	The string version of the JSON for the event.
//
static void ReportsAddItem(std::string const& p_EventString)
{
	// Acquire a lock for the rest of the function.
	const std::lock_guard<std::mutex> l_ReportGuard(s_ReportMutex);

	PendingItem l_PendingItem;
	l_PendingItem.m_RawTime = time(nullptr);
	l_PendingItem.m_EventString = p_EventString;

	s_PendingItemList.push_back(l_PendingItem);
}

// Add an item to the report corresponding to a control event.
// 
// p_ControlName:	The name of the control.
// p_Action:		The action performed on the control.
// p_SourceName:	An identifier for where this item comes from.
// 
void ReportsAddControlItem(std::string const& p_ControlName, Control::Actions const p_Action,
	std::string const& p_SourceName)
{
	if ((p_Action < 0) || (p_Action >= Control::NUM_ACTIONS))
	{
		LoggerAddMessage("Could not add control item to the report because it contains an invalid "
			"action %d!", p_Action);
		return;
	}

	// Make a JSON representation of this item.
	rapidjson::Document l_ItemDocument;
	l_ItemDocument.SetObject();

	auto l_ItemAllocator = l_ItemDocument.GetAllocator();

	l_ItemDocument.AddMember("type", 
		rapidjson::Value(rapidjson::StringRef("control")), l_ItemAllocator);	

	// It is safe to use a string reference here because this document will not live outside of this 
	// scope.
	l_ItemDocument.AddMember("control", 
		rapidjson::Value(rapidjson::StringRef(p_ControlName.c_str())), l_ItemAllocator);

	l_ItemDocument.AddMember("action", 
		rapidjson::Value(rapidjson::StringRef(s_ControlActionNames[p_Action])), l_ItemAllocator);

	l_ItemDocument.AddMember("source", 
		rapidjson::Value(rapidjson::StringRef(p_SourceName.c_str())), l_ItemAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer l_ItemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> l_ItemWriter(l_ItemBuffer);
	l_ItemDocument.Accept(l_ItemWriter);

	// Handle the rest.
	ReportsAddItem(l_ItemBuffer.GetString());
}

// Add an item to the report corresponding to a schedule event.
// 
// p_ActionName:	The name of the schedule action.
// 
void ReportsAddScheduleItem(std::string const& p_ActionName)
{
	// Make a JSON representation of this item.
	rapidjson::Document l_ItemDocument;
	l_ItemDocument.SetObject();

	auto l_ItemAllocator = l_ItemDocument.GetAllocator();

	l_ItemDocument.AddMember("type", 
		rapidjson::Value(rapidjson::StringRef("schedule")), l_ItemAllocator);	

	// It is safe to use a string reference here because this document will not live outside of this 
	// scope.
	l_ItemDocument.AddMember("action", 
		rapidjson::Value(rapidjson::StringRef(p_ActionName.c_str())), l_ItemAllocator);
	
	// Write this into a string.
	rapidjson::StringBuffer l_ItemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> l_ItemWriter(l_ItemBuffer);
	l_ItemDocument.Accept(l_ItemWriter);

	// Handle the rest.
	ReportsAddItem(l_ItemBuffer.GetString());
}

// Add an item to the report corresponding to a status event.
// 
void ReportsAddStatusItem()
{
	// Make a JSON representation of this item.
	rapidjson::Document l_ItemDocument;
	l_ItemDocument.SetObject();

	auto l_ItemAllocator = l_ItemDocument.GetAllocator();

	l_ItemDocument.AddMember("type", 
		rapidjson::Value(rapidjson::StringRef("status")), l_ItemAllocator);	
	
	// Write this into a string.
	rapidjson::StringBuffer l_ItemBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> l_ItemWriter(l_ItemBuffer);
	l_ItemDocument.Accept(l_ItemWriter);

	// Handle the rest.
	ReportsAddItem(l_ItemBuffer.GetString());
}
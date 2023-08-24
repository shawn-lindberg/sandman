#include "reports.h"

#include <mutex>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "logger.h"

#define TEMPDIR	AM_TEMPDIR

#define REPORT_VERSION	1

// Types
//

// An item we want to put in the report later.
// 
struct PendingItem
{
	// The time the item was added.
	time_t m_RawTime;

	// The description of the item to put into the report.
	std::string	m_Description;
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

// Functions
//

// Determine the effective date that we should be using for the report now.
//
static std::string ReportsGetEffectiveDate()
{
	// Get the current time.
	auto const l_RawTime = time(nullptr);
	auto* l_LocalTime = localtime(&l_RawTime);

	// If the time is after 5 PM, advance the day.
	if (l_LocalTime->tm_hour >= 17)
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

	// Make a JSON representation of this item.
	rapidjson::Document l_ItemDocument;
	l_ItemDocument.SetObject();

	auto l_ItemAllocator = l_ItemDocument.GetAllocator();

	// It is safe to use a string reference here because this document will not live outside of this 
	// scope.
	l_ItemDocument.AddMember("dateTime", 
		rapidjson::Value(rapidjson::StringRef(l_TimeStringBuffer)), l_ItemAllocator);

	l_ItemDocument.AddMember("event", 
		rapidjson::Value(rapidjson::StringRef(p_Item.m_Description.c_str())), l_ItemAllocator);
	
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
// p_Format:	Standard printf format string.
// ...:			Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool ReportsAddItem(char const* p_Format, ...)
{	
	va_list l_Arguments;
	va_start(l_Arguments, p_Format);

	auto const l_Result = ReportsAddItem(p_Format, l_Arguments);

	va_end(l_Arguments);
	
	return l_Result;
}

// Add an item to the report (va_list version).
// 
// p_Format:		Standard printf format string.
// p_Arguments:	Standard printf arguments.
//
// returns:		True if successful, false otherwise.
//
bool ReportsAddItem(char const* p_Format, va_list& p_Arguments)
{
	// Convert the description formatted string into an ordinary string.
	static unsigned int const l_DescriptionBufferCapacity = 2048;
	char l_DescriptionBuffer[l_DescriptionBufferCapacity];

	if (vsnprintf(l_DescriptionBuffer, l_DescriptionBufferCapacity, p_Format, p_Arguments) < 0)
	{
		return false;
	}

	// Force terminate.
	l_DescriptionBuffer[l_DescriptionBufferCapacity - 1] = '\0';

	// Now that we have the description, add it to the report.
	{
		// Acquire a lock for the rest of the function.
		const std::lock_guard<std::mutex> l_ReportGuard(s_ReportMutex);

		PendingItem l_PendingItem;
		l_PendingItem.m_RawTime = time(nullptr);
		l_PendingItem.m_Description = l_DescriptionBuffer;

		s_PendingItemList.push_back(l_PendingItem);
	}

	return true;
}
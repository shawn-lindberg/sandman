#include "catch_amalgamated.hpp"

#include "logger.h"
#include "schedule.h"

class TestRunListener : public Catch::EventListenerBase
{
public:
	using Catch::EventListenerBase::EventListenerBase;

   void testRunStarting(Catch::TestRunInfo const&) override
	{
		LoggerInitialize("tests.log");
	}

	void testRunEnded(Catch::TestRunStats const&) override
	{
		LoggerUninitialize();
	}
};

CATCH_REGISTER_LISTENER(TestRunListener)

TEST_CASE("Test missing schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.LoadFromFile("");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test default (empty) schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.LoadFromFile("data/sandman.sched");
	REQUIRE(l_Loaded == true);
	REQUIRE(l_Schedule.IsEmpty() == true);
}

TEST_CASE("Test invalid schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.LoadFromFile("data/invalidJson.sched");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test example schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.LoadFromFile("data/example.sched");
	REQUIRE(l_Loaded == true);
	REQUIRE(l_Schedule.IsEmpty() == false);
}
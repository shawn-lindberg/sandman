#include "catch_amalgamated.hpp"

#include "schedule.h"

TEST_CASE("Test missing schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.LoadFromFile("");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test default schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.LoadFromFile("data/sandman.sched");
	REQUIRE(l_Loaded == true);
	REQUIRE(l_Schedule.IsEmpty() == true);
}
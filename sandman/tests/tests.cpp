#include "catch_amalgamated.hpp"

#include "config.h"
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


TEST_CASE("Test missing config", "[config]")
{
	Config l_Config;
	const bool l_Loaded = l_Config.ReadFromFile("");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test default config", "[config]")
{
	Config l_Config;
	const bool l_Loaded = l_Config.ReadFromFile("data/sandman.conf");
	REQUIRE(l_Loaded == true);

	const std::vector<ControlConfig> l_ControlConfigs = l_Config.GetControlConfigs();
	REQUIRE(l_ControlConfigs.size() == 3);
	if (l_ControlConfigs.size() > 2)
	{
		{
			REQUIRE(std::string(l_ControlConfigs[0].m_Name) == "back");
			REQUIRE(l_ControlConfigs[0].m_UpGPIOPin == 20);
			REQUIRE(l_ControlConfigs[0].m_DownGPIOPin == 16);
			REQUIRE(l_ControlConfigs[0].m_MovingDurationMS == 7000);
		}
		{
			REQUIRE(std::string(l_ControlConfigs[1].m_Name) == "legs");
			REQUIRE(l_ControlConfigs[1].m_UpGPIOPin == 13);
			REQUIRE(l_ControlConfigs[1].m_DownGPIOPin == 26);
			REQUIRE(l_ControlConfigs[1].m_MovingDurationMS == 4000);
		}	
		{
			REQUIRE(std::string(l_ControlConfigs[2].m_Name) == "elev");
			REQUIRE(l_ControlConfigs[2].m_UpGPIOPin == 5);
			REQUIRE(l_ControlConfigs[2].m_DownGPIOPin == 19);
			REQUIRE(l_ControlConfigs[2].m_MovingDurationMS == 4000);
		}
	}
}

TEST_CASE("Test missing schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.ReadFromFile("");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test default (empty) schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.ReadFromFile("data/sandman.sched");
	REQUIRE(l_Loaded == true);
	REQUIRE(l_Schedule.IsEmpty() == true);
}

TEST_CASE("Test invalid schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.ReadFromFile("data/invalidJson.sched");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test example schedule", "[schedules]")
{
	Schedule l_Schedule;
	const bool l_Loaded = l_Schedule.ReadFromFile("data/example.sched");
	REQUIRE(l_Loaded == true);
	REQUIRE(l_Schedule.IsEmpty() == false);
	REQUIRE(l_Schedule.GetNumEvents() == 2);

	const std::vector<ScheduleEvent>& l_Events = l_Schedule.GetEvents();
	if (l_Events.size() > 1)
	{
		REQUIRE(l_Events[0].m_DelaySec == 20);
		REQUIRE(l_Events[1].m_DelaySec == 25);

		REQUIRE(std::string(l_Events[0].m_ControlAction.m_ControlName) == "legs");
		REQUIRE(l_Events[0].m_ControlAction.m_Action == Control::ACTION_MOVING_UP);
		REQUIRE(std::string(l_Events[1].m_ControlAction.m_ControlName) == "legs");
		REQUIRE(l_Events[1].m_ControlAction.m_Action == Control::ACTION_MOVING_DOWN);
	}
}
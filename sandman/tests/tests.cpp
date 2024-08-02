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
		if (not Logger::Initialize("tests.log"))
		{
			throw std::runtime_error("The logger failed to initialize.");
		}
	}

	void testRunEnded(Catch::TestRunStats const&) override
	{
		Logger::Uninitialize();
	}
};

CATCH_REGISTER_LISTENER(TestRunListener)


TEST_CASE("Test missing config", "[config]")
{
	Config config;
	const bool loaded = config.ReadFromFile("");
	REQUIRE(loaded == false);
}

TEST_CASE("Test default config", "[config]")
{
	Config config;
	const bool loaded = config.ReadFromFile("data/sandman.conf");
	REQUIRE(loaded == true);

	const std::vector<ControlConfig> controlConfigs = config.GetControlConfigs();
	REQUIRE(controlConfigs.size() == 3);
	if (controlConfigs.size() > 2)
	{
		{
			REQUIRE(std::string(controlConfigs[0].m_Name) == "back");
			REQUIRE(controlConfigs[0].m_UpGPIOPin == 20);
			REQUIRE(controlConfigs[0].m_DownGPIOPin == 16);
			REQUIRE(controlConfigs[0].m_MovingDurationMS == 7000);
		}
		{
			REQUIRE(std::string(controlConfigs[1].m_Name) == "legs");
			REQUIRE(controlConfigs[1].m_UpGPIOPin == 13);
			REQUIRE(controlConfigs[1].m_DownGPIOPin == 26);
			REQUIRE(controlConfigs[1].m_MovingDurationMS == 4000);
		}	
		{
			REQUIRE(std::string(controlConfigs[2].m_Name) == "elev");
			REQUIRE(controlConfigs[2].m_UpGPIOPin == 5);
			REQUIRE(controlConfigs[2].m_DownGPIOPin == 19);
			REQUIRE(controlConfigs[2].m_MovingDurationMS == 4000);
		}
	}
}

TEST_CASE("Test missing schedule", "[schedules]")
{
	Schedule schedule;
	const bool loaded = schedule.ReadFromFile("");
	REQUIRE(loaded == false);
}

TEST_CASE("Test default (empty) schedule", "[schedules]")
{
	Schedule schedule;
	const bool loaded = schedule.ReadFromFile("data/sandman.sched");
	REQUIRE(loaded == true);
	REQUIRE(schedule.IsEmpty() == true);
}

TEST_CASE("Test invalid schedule", "[schedules]")
{
	Schedule schedule;
	const bool loaded = schedule.ReadFromFile("data/invalidJson.sched");
	REQUIRE(loaded == false);
}

TEST_CASE("Test example schedule", "[schedules]")
{
	Schedule schedule;
	const bool loaded = schedule.ReadFromFile("data/example.sched");
	REQUIRE(loaded == true);
	REQUIRE(schedule.IsEmpty() == false);
	REQUIRE(schedule.GetNumEvents() == 2);

	const std::vector<ScheduleEvent>& events = schedule.GetEvents();
	if (events.size() > 1)
	{
		REQUIRE(events[0].m_DelaySec == 20);
		REQUIRE(events[1].m_DelaySec == 25);

		REQUIRE(std::string(events[0].m_ControlAction.m_ControlName) == "legs");
		REQUIRE(events[0].m_ControlAction.m_Action == Control::kActionMovingUp);
		REQUIRE(std::string(events[1].m_ControlAction.m_ControlName) == "legs");
		REQUIRE(events[1].m_ControlAction.m_Action == Control::kActionMovingDown);
	}
}
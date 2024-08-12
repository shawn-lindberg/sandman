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
	bool const l_Loaded = l_Config.ReadFromFile("");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test default config", "[config]")
{
	Config l_Config;
	bool const l_Loaded = l_Config.ReadFromFile("data/sandman.conf");
	REQUIRE(l_Loaded == true);

	std::vector<ControlConfig> const& l_ControlConfigs = l_Config.GetControlConfigs();
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

	REQUIRE(l_Config.GetControlMaxMovingDurationMS() == 100000);
	REQUIRE(l_Config.GetControlCoolDownDurationMS() == 25);

	std::vector<InputBinding> const& l_InputBindings = l_Config.GetInputBindings();
	REQUIRE(l_InputBindings.size() == 6);
	if (l_InputBindings.size() > 5)
	{
		{
			REQUIRE(l_InputBindings[0].m_KeyCode == 310);
			REQUIRE(std::string(l_InputBindings[0].m_ControlAction.m_ControlName) == "back");
			REQUIRE(l_InputBindings[0].m_ControlAction.m_Action == Control::ACTION_MOVING_UP);
		}
		{
			REQUIRE(l_InputBindings[1].m_KeyCode == 311);
			REQUIRE(std::string(l_InputBindings[1].m_ControlAction.m_ControlName) == "back");
			REQUIRE(l_InputBindings[1].m_ControlAction.m_Action == Control::ACTION_MOVING_DOWN);
		}
		{
			REQUIRE(l_InputBindings[2].m_KeyCode == 308);
			REQUIRE(std::string(l_InputBindings[2].m_ControlAction.m_ControlName) == "legs");
			REQUIRE(l_InputBindings[2].m_ControlAction.m_Action == Control::ACTION_MOVING_UP);
		}
		{
			REQUIRE(l_InputBindings[3].m_KeyCode == 305);
			REQUIRE(std::string(l_InputBindings[3].m_ControlAction.m_ControlName) == "legs");
			REQUIRE(l_InputBindings[3].m_ControlAction.m_Action == Control::ACTION_MOVING_DOWN);
		}
		{
			REQUIRE(l_InputBindings[4].m_KeyCode == 307);
			REQUIRE(std::string(l_InputBindings[4].m_ControlAction.m_ControlName) == "elev");
			REQUIRE(l_InputBindings[4].m_ControlAction.m_Action == Control::ACTION_MOVING_UP);
		}
		{
			REQUIRE(l_InputBindings[5].m_KeyCode == 304);
			REQUIRE(std::string(l_InputBindings[5].m_ControlAction.m_ControlName) == "elev");
			REQUIRE(l_InputBindings[5].m_ControlAction.m_Action == Control::ACTION_MOVING_DOWN);
		}
	}
}

TEST_CASE("Test missing schedule", "[schedules]")
{
	Schedule l_Schedule;
	bool const l_Loaded = l_Schedule.ReadFromFile("");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test default (empty) schedule", "[schedules]")
{
	Schedule l_Schedule;
	bool const l_Loaded = l_Schedule.ReadFromFile("data/sandman.sched");
	REQUIRE(l_Loaded == true);
	REQUIRE(l_Schedule.IsEmpty() == true);
}

TEST_CASE("Test invalid schedule", "[schedules]")
{
	Schedule l_Schedule;
	bool const l_Loaded = l_Schedule.ReadFromFile("data/invalidJson.sched");
	REQUIRE(l_Loaded == false);
}

TEST_CASE("Test example schedule", "[schedules]")
{
	Schedule l_Schedule;
	bool const l_Loaded = l_Schedule.ReadFromFile("data/example.sched");
	REQUIRE(l_Loaded == true);
	REQUIRE(l_Schedule.IsEmpty() == false);
	REQUIRE(l_Schedule.GetNumEvents() == 2);

	std::vector<ScheduleEvent>& l_Events = l_Schedule.GetEvents();
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

TEST_CASE("Test controls", "[control]")
{
	Config l_Config;
	bool const l_Loaded = l_Config.ReadFromFile("data/sandman.conf");
	REQUIRE(l_Loaded == true);

	std::vector<ControlConfig> const& l_ControlConfigs = l_Config.GetControlConfigs();
	REQUIRE(l_ControlConfigs.size() == 3);
	if (l_ControlConfigs.size() > 2)
	{
		// Set up the controls.
		bool const l_EnableGPIO = false;
		ControlsInitialize(l_ControlConfigs, l_EnableGPIO);

		Control::SetDurations(l_Config.GetControlMaxMovingDurationMS(), 
									 l_Config.GetControlCoolDownDurationMS());

		// Test an invalid control.
		{
			ControlHandle l_Handle = Control::GetHandle("chicken");
			REQUIRE(l_Handle.IsValid() == false);
			Control* l_Control = Control::GetFromHandle(l_Handle);
			REQUIRE(l_Control == nullptr);
		}

		ControlHandle l_LegHandle = Control::GetHandle("legs");
		REQUIRE(l_LegHandle.IsValid() == true);
		Control* l_LegControl = Control::GetFromHandle(l_LegHandle);
		REQUIRE(l_LegControl != nullptr);
		if (l_LegControl != nullptr)
		{
			REQUIRE(l_LegControl->GetState() == Control::STATE_IDLE);
		}

		ControlHandle l_BackHandle = Control::GetHandle("back");
		REQUIRE(l_BackHandle.IsValid() == true);
		Control* l_BackControl = Control::GetFromHandle(l_BackHandle);
		REQUIRE(l_BackControl != nullptr);
		if (l_BackControl != nullptr)
		{
			REQUIRE(l_BackControl->GetState() == Control::STATE_IDLE);
		}

		ControlHandle l_ElevationHandle = Control::GetHandle("elev");
		REQUIRE(l_ElevationHandle.IsValid() == true);
		Control* l_ElevationControl = Control::GetFromHandle(l_ElevationHandle);
		REQUIRE(l_ElevationControl != nullptr);
		if (l_ElevationControl != nullptr)
		{
			REQUIRE(l_ElevationControl->GetState() == Control::STATE_IDLE);
		}
	}
}
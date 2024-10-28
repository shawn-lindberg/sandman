#include "catch_amalgamated.hpp"

#include "config.h"
#include "gpio.h"
#include "logger.h"
#include "schedule.h"

class TestRunListener : public Catch::EventListenerBase
{
public:
	using Catch::EventListenerBase::EventListenerBase;

   void testRunStarting(Catch::TestRunInfo const&) override
	{
		if (Logger::Initialize(SANDMAN_TEST_BUILD_DIR "tests.log") == false)
		{
			Catch::cerr() << "The logger failed to initialize.\n";
			std::exit(EXIT_FAILURE);
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
	bool const loaded = config.ReadFromFile("");
	REQUIRE(loaded == false);
}

TEST_CASE("Test default config", "[config]")
{
	Config config;
	bool const loaded = config.ReadFromFile(SANDMAN_TEST_DATA_DIR "sandman.conf");
	REQUIRE(loaded == true);

	std::vector<ControlConfig> const& controlConfigs = config.GetControlConfigs();
	REQUIRE(controlConfigs.size() == 3);
	if (controlConfigs.size() > 2)
	{
		{
			REQUIRE(std::string(controlConfigs[0].m_name) == "back");
			REQUIRE(controlConfigs[0].m_upGPIOPin == 20);
			REQUIRE(controlConfigs[0].m_downGPIOPin == 16);
			REQUIRE(controlConfigs[0].m_movingDurationMS == 7000);
		}
		{
			REQUIRE(std::string(controlConfigs[1].m_name) == "legs");
			REQUIRE(controlConfigs[1].m_upGPIOPin == 13);
			REQUIRE(controlConfigs[1].m_downGPIOPin == 26);
			REQUIRE(controlConfigs[1].m_movingDurationMS == 4000);
		}	
		{
			REQUIRE(std::string(controlConfigs[2].m_name) == "elev");
			REQUIRE(controlConfigs[2].m_upGPIOPin == 5);
			REQUIRE(controlConfigs[2].m_downGPIOPin == 19);
			REQUIRE(controlConfigs[2].m_movingDurationMS == 4000);
		}
	}

	REQUIRE(config.GetControlMaxMovingDurationMS() == 100000);
	REQUIRE(config.GetControlCoolDownDurationMS() == 25);

	std::vector<InputBinding> const& inputBindings = config.GetInputBindings();
	REQUIRE(inputBindings.size() == 6);
	if (inputBindings.size() > 5)
	{
		{
			REQUIRE(inputBindings[0].m_keyCode == 310);
			REQUIRE(std::string(inputBindings[0].m_controlAction.m_controlName) == "back");
			REQUIRE(inputBindings[0].m_controlAction.m_action == Control::kActionMovingUp);
		}
		{
			REQUIRE(inputBindings[1].m_keyCode == 311);
			REQUIRE(std::string(inputBindings[1].m_controlAction.m_controlName) == "back");
			REQUIRE(inputBindings[1].m_controlAction.m_action == Control::kActionMovingDown);
		}
		{
			REQUIRE(inputBindings[2].m_keyCode == 308);
			REQUIRE(std::string(inputBindings[2].m_controlAction.m_controlName) == "legs");
			REQUIRE(inputBindings[2].m_controlAction.m_action == Control::kActionMovingUp);
		}
		{
			REQUIRE(inputBindings[3].m_keyCode == 305);
			REQUIRE(std::string(inputBindings[3].m_controlAction.m_controlName) == "legs");
			REQUIRE(inputBindings[3].m_controlAction.m_action == Control::kActionMovingDown);
		}
		{
			REQUIRE(inputBindings[4].m_keyCode == 307);
			REQUIRE(std::string(inputBindings[4].m_controlAction.m_controlName) == "elev");
			REQUIRE(inputBindings[4].m_controlAction.m_action == Control::kActionMovingUp);
		}
		{
			REQUIRE(inputBindings[5].m_keyCode == 304);
			REQUIRE(std::string(inputBindings[5].m_controlAction.m_controlName) == "elev");
			REQUIRE(inputBindings[5].m_controlAction.m_action == Control::kActionMovingDown);
		}
	}
}

TEST_CASE("Test missing schedule", "[schedules]")
{
	Schedule schedule;
	bool const loaded = schedule.ReadFromFile("");
	REQUIRE(loaded == false);
}

TEST_CASE("Test default (empty) schedule", "[schedules]")
{
	Schedule schedule;
	bool const loaded = schedule.ReadFromFile(SANDMAN_TEST_DATA_DIR "sandman.sched");
	REQUIRE(loaded == true);
	REQUIRE(schedule.IsEmpty() == true);
}

TEST_CASE("Test invalid schedule", "[schedules]")
{
	Schedule schedule;
	bool const loaded = schedule.ReadFromFile(SANDMAN_TEST_DATA_DIR "invalidJson.sched");
	REQUIRE(loaded == false);
}

TEST_CASE("Test example schedule", "[schedules]")
{
	Schedule schedule;
	bool const loaded = schedule.ReadFromFile(SANDMAN_TEST_DATA_DIR "example.sched");
	REQUIRE(loaded == true);
	REQUIRE(schedule.IsEmpty() == false);
	REQUIRE(schedule.GetNumEvents() == 2);

	std::vector<ScheduleEvent>& events = schedule.GetEvents();
	if (events.size() > 1)
	{
		REQUIRE(events[0].m_delaySec == 20);
		REQUIRE(events[1].m_delaySec == 25);

		REQUIRE(std::string(events[0].m_controlAction.m_controlName) == "legs");
		REQUIRE(events[0].m_controlAction.m_action == Control::kActionMovingUp);
		REQUIRE(std::string(events[1].m_controlAction.m_controlName) == "legs");
		REQUIRE(events[1].m_controlAction.m_action == Control::kActionMovingDown);
	}
}

TEST_CASE("Test controls", "[control]")
{
	Config config;
	bool const loaded = config.ReadFromFile(SANDMAN_TEST_DATA_DIR "sandman.conf");
	REQUIRE(loaded == true);

	std::vector<ControlConfig> const& controlConfigs = config.GetControlConfigs();
	REQUIRE(controlConfigs.size() == 3);
	if (controlConfigs.size() > 2)
	{
		// Set up the controls.
		static constexpr bool kEnableGPIO = false;
		GPIOInitialize(kEnableGPIO);

		ControlsInitialize(controlConfigs);

		Control::SetDurations(config.GetControlMaxMovingDurationMS(), 
									 config.GetControlCoolDownDurationMS());

		// Test an invalid control.
		{
			ControlHandle handle = Control::GetHandle("chicken");
			REQUIRE(handle.IsValid() == false);
			Control* control = Control::GetFromHandle(handle);
			REQUIRE(control == nullptr);
		}

		ControlHandle legHandle = Control::GetHandle("legs");
		REQUIRE(legHandle.IsValid() == true);
		Control* legControl = Control::GetFromHandle(legHandle);
		REQUIRE(legControl != nullptr);
		if (legControl != nullptr)
		{
			REQUIRE(legControl->GetState() == Control::kStateIdle);
		}

		ControlHandle backHandle = Control::GetHandle("back");
		REQUIRE(backHandle.IsValid() == true);
		Control* backControl = Control::GetFromHandle(backHandle);
		REQUIRE(backControl != nullptr);
		if (backControl != nullptr)
		{
			REQUIRE(backControl->GetState() == Control::kStateIdle);
		}

		ControlHandle elevationHandle = Control::GetHandle("elev");
		REQUIRE(elevationHandle.IsValid() == true);
		Control* elevationControl = Control::GetFromHandle(elevationHandle);
		REQUIRE(elevationControl != nullptr);
		if (elevationControl != nullptr)
		{
			REQUIRE(elevationControl->GetState() == Control::kStateIdle);
		}
	}
}
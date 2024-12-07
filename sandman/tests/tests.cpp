#include "catch_amalgamated.hpp"

#include "config.h"
#include "gpio.h"
#include "logger.h"
#include "routines.h"

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

TEST_CASE("Test missing routine", "[routines]")
{
	Routine routine;
	bool const loaded = routine.ReadFromFile("");
	REQUIRE(loaded == false);
}

TEST_CASE("Test default (empty) routine", "[routines]")
{
	Routine routine;
	bool const loaded = routine.ReadFromFile(SANDMAN_TEST_DATA_DIR "sandman.rtn");
	REQUIRE(loaded == true);
	REQUIRE(routine.IsEmpty() == true);
}

TEST_CASE("Test invalid routine", "[routines]")
{
	Routine routine;
	bool const loaded = routine.ReadFromFile(SANDMAN_TEST_DATA_DIR "invalid_json.rtn");
	REQUIRE(loaded == false);
}

TEST_CASE("Test example routine", "[routines]")
{
	Routine routine;
	bool const loaded = routine.ReadFromFile(SANDMAN_TEST_DATA_DIR "example.rtn");
	REQUIRE(loaded == true);
	REQUIRE(routine.IsEmpty() == false);
	REQUIRE(routine.GetNumSteps() == 2u);

	auto const& steps = routine.GetSteps();
	if (steps.size() > 1)
	{
		REQUIRE(steps[0].m_delaySec == 20);
		REQUIRE(steps[1].m_delaySec == 25);

		REQUIRE(std::string(steps[0].m_controlAction.m_controlName) == "legs");
		REQUIRE(steps[0].m_controlAction.m_action == Control::kActionMovingUp);
		REQUIRE(std::string(steps[1].m_controlAction.m_controlName) == "legs");
		REQUIRE(steps[1].m_controlAction.m_action == Control::kActionMovingDown);
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
			Control* control = Control::GetByName("chicken");
			REQUIRE(control == nullptr);
		}

		Control* legControl = Control::GetByName("legs");
		REQUIRE(legControl != nullptr);
		if (legControl != nullptr)
		{
			REQUIRE(legControl->GetState() == Control::kStateIdle);
		}

		Control* backControl = Control::GetByName("back");
		REQUIRE(backControl != nullptr);
		if (backControl != nullptr)
		{
			REQUIRE(backControl->GetState() == Control::kStateIdle);
		}

		Control* elevationControl = Control::GetByName("elev");
		REQUIRE(elevationControl != nullptr);
		if (elevationControl != nullptr)
		{
			REQUIRE(elevationControl->GetState() == Control::kStateIdle);
		}
	}
}
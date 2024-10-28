#pragma once

#include <vector>

#include "rapidjson/document.h"

#include "timer.h"

// Types
//

// Configuration parameters to initialize a control.
struct ControlConfig
{
	// Read a control config from JSON. 
	//
	// object:	The JSON object representing a control config.
	//
	// Returns:		True if the config was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value const& object);

	// Constants.
	static constexpr unsigned int kControlNameCapacity{32u};

	// The name of the control.
	char m_name[kControlNameCapacity];

	// The GPIO pins to use.
	int m_upGPIOPin;
	int m_downGPIOPin;

	// The duration of the moving state (in milliseconds) for this control.
	unsigned int m_movingDurationMS;
};

// An individual control.
class Control
{
	public:

		// States a control may be in.
		enum State
		{
			kStateIdle = 0,
			kStateMovingUp,
			kStateMovingDown,
			kStateCoolDown,    // A delay after moving before moving can occur again.
		};

		// Actions a control may be desired to perform.
		enum Actions
		{
			kActionStopped = 0,
			kActionMovingUp,
			kActionMovingDown,
			
			kNumActions,
		};

		// Movement modes, either manual or timed for now.
		enum Modes
		{
			kModeManual = 0, 
			kModeTimed,
		};
		
		// Handle initialization.
		//
		// config:	Configuration parameters for the control.
		//
		void Initialize(ControlConfig const& config);

		// Handle uninitialization.
		//
		void Uninitialize();
		
		// Process a tick.
		//
		void Process();

		// Set the desired action.
		//
		// desiredAction:		The desired action.
		// mode:					The mode of the action.
		// durationPercent:	(Optional) The percent of the normal duration to perform the action 
		//								for.
		//
		void SetDesiredAction(Actions desiredAction, Modes mode, unsigned int durationPercent = 100);

		// Get the name.
		//
		char const* GetName() const
		{
			return m_name;
		}

		// Get the state.
		//
		State GetState() const
		{
			return m_state;
		}
		
		// Enable or disable all controls.
		//
		// enable:	Whether to enable or disable all controls.
		//
		static void Enable(bool enable);
		
		// Set the durations.
		//
		// movingDurationMS:		Duration of the moving state (in milliseconds).
		// coolDownDurationMS:	Duration of the cool down state (in milliseconds).
		//
		static void SetDurations(unsigned int movingDurationMS, unsigned int coolDownDurationMS);
		
		// Look up a control by its name.
		//
		// name:	The name of the control.
		//
		// Returns:		The control, or null if one with the name could not be found.
		//
		static Control* GetByName(std::string const& name);
		
	private:

		// Constants.
		static constexpr unsigned int kNameCapacity = 32u;

		// Play a notification for the state.
		//
		void PlayNotification();
		
		// The name of the control.
		char m_name[kNameCapacity];
		
		// The control state.
		State m_state;

		// A record of when the state transition timer began.
		Time m_stateStartTime;

		// The desired action.
		Actions m_desiredAction;

		// The control movement mode.
		Modes m_mode;
		
		// The GPIO pins to use.
		int m_upGPIOPin;
		int m_downGPIOPin;
		
		// The current duration of the moving state (in milliseconds) for this control.
		unsigned int m_movingDurationMS;
		
		// The standard duration of the moving state (in milliseconds) for this control.
		unsigned int m_standardMovingDurationMS;

		// Maximum duration of the moving state (in milliseconds).
		static unsigned int ms_maxMovingDurationMS;
		
		// Maximum duration of the cool down state (in milliseconds).
		static unsigned int ms_coolDownDurationMS;	
};

// Enough information to trigger a specific control action.
struct ControlAction
{	
	ControlAction() = default;
	
	// A constructor for emplacing.
	// 
	ControlAction(char const* controlName, Control::Actions action);

	// Read a control action from JSON. 
	//
	// object:	The JSON object representing a control action.
	//
	// Returns:		True if the action was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value const& object);

	// Attempt to get the control corresponding to the control action.
	//
	// Returns:	The control if successful, null otherwise.
	//
	Control* GetControl() const;
	
	// Constants.
	static constexpr unsigned int kControlNameCapacity = 32u;
	
	// The name of the control to manipulate.
	char m_controlName[kControlNameCapacity];
	
	// The action for the control.
	Control::Actions m_action;
};


// Functions
//

// Initialize all of the controls.
//
// configs: Configuration parameters for the controls to add.
//
void ControlsInitialize(std::vector<ControlConfig> const& configs);

// Uninitialize all of the controls.
//
void ControlsUninitialize();

// Process all of the controls.
//
void ControlsProcess();

// Create a new control with the provided config. Control names must be unique.
//
// config:	Configuration parameters for the control.
//
// Returns:		True if the control was successfully created, false otherwise.
//
bool ControlsCreateControl(ControlConfig const& config);

// Stop all of the controls.
//
void ControlsStopAll();

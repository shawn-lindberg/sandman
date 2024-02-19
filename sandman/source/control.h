#pragma once

#include <vector>

#include <libxml/parser.h>
#include "rapidjson/document.h"

#include "timer.h"

// Types
//

// A handle to a control.
class ControlHandle
{
	public:
		
		// Only allow non-friends to construct invalid handles.
		ControlHandle() = default;
		
		// Determine whether the handle is valid.
		//
		bool IsValid() const
		{
			return (m_UID != ms_InvalidUID);
		}
				
	private:
	
		// Make this constructor private so that non-friends can only construct invalid handles.
		ControlHandle(unsigned short p_UID);
		
		friend class Control;
		
		// The invalid unique identifier for a control.
		static constexpr unsigned short ms_InvalidUID = 0xFFFF;
		
		// The unique identifier for the control.
		unsigned short m_UID = ms_InvalidUID;
};

// Configuration parameters to initialize a control.
struct ControlConfig
{
	// Read a control config from XML. 
	//
	// p_Document:	The XML document that the node belongs to.
	// p_Node:		The XML node to read the control config from.
	//	
	// Returns:		True if the config was read successfully, false otherwise.
	//
	bool ReadFromXML(xmlDocPtr p_Document, xmlNodePtr p_Node);
	
	// Constants.
	static constexpr unsigned int ms_ControlNameCapacity = 32;
	
	// The name of the control.
	char m_Name[ms_ControlNameCapacity];
		
	// The GPIO pins to use.
	int m_UpGPIOPin;
	int m_DownGPIOPin;
	
	// The duration of the moving state (in milliseconds) for this control.
	unsigned int m_MovingDurationMS;
};

// An individual control.
class Control
{
	public:

		// Actions a control may be desired to perform.
		enum Actions
		{
			ACTION_STOPPED = 0,
			ACTION_MOVING_UP,
			ACTION_MOVING_DOWN,
			
			NUM_ACTIONS,
		};

		// Movement modes, either manual or timed for now.
		enum Modes
		{
			MODE_MANUAL = 0, 
			MODE_TIMED,
		};
		
		// Handle initialization.
		//
		// p_Config:	Configuration parameters for the control.
		//
		void Initialize(ControlConfig const& p_Config);

		// Handle uninitialization.
		//
		void Uninitialize();
		
		// Process a tick.
		//
		void Process();

		// Set the desired action.
		//
		// p_DesiredAction:		The desired action.
		// p_Mode:					The mode of the action.
		// p_DurationPercent:	(Optional) The percent of the normal duration to perform the action 
		//								for.
		//
		void SetDesiredAction(Actions p_DesiredAction, Modes p_Mode, 
			unsigned int p_DurationPercent = 100);

		// Get the name.
		//
		char const* GetName() const
		{
			return m_Name;
		}
		
		// Enable or disable all controls.
		//
		// p_Enable:	Whether to enable or disable all controls.
		//
		static void Enable(bool p_Enable);
		
		// Set the durations.
		//
		// p_MovingDurationMS:		Duration of the moving state (in milliseconds).
		// p_CoolDownDurationMS:	Duration of the cool down state (in milliseconds).
		//
		static void SetDurations(unsigned int p_MovingDurationMS, unsigned int p_CoolDownDurationMS);
		
		// Attempt to get the handle of a control based on its name.
		//
		// p_Name:	The unique name of the control.
		//
		// Returns:	A handle to the control, or an invalid handle if one with the given name could not be found.
		//
		static ControlHandle GetHandle(char const* p_Name);

		// Look up a control from its handle.
		//
		// p_Handle:	A handle to the control.
		//
		// Returns:		The control, or null if the handle is not valid.
		//
		static Control* GetFromHandle(ControlHandle const& p_Handle);
		
	private:

		// Constants.
		static constexpr unsigned int ms_NameCapacity = 32;

		// States a control may be in.
		enum State
		{
			STATE_IDLE = 0,
			STATE_MOVING_UP,
			STATE_MOVING_DOWN,
			STATE_COOL_DOWN,    // A delay after moving before moving can occur again.
		};

		// Play a notification for the state.
		//
		void PlayNotification();
		
		// The name of the control.
		char m_Name[ms_NameCapacity];
		
		// The control state.
		State m_State;

		// A record of when the state transition timer began.
		Time m_StateStartTime;

		// The desired action.
		Actions m_DesiredAction;

		// The control movement mode.
		Modes m_Mode;
		
		// The GPIO pins to use.
		int m_UpGPIOPin;
		int m_DownGPIOPin;
		
		// The current duration of the moving state (in milliseconds) for this control.
		unsigned int m_MovingDurationMS;
		
		// The standard duration of the moving state (in milliseconds) for this control.
		unsigned int m_StandardMovingDurationMS;

		// Maximum duration of the moving state (in milliseconds).
		static unsigned int ms_MaxMovingDurationMS;
		
		// Maximum duration of the cool down state (in milliseconds).
		static unsigned int ms_CoolDownDurationMS;	
};

// Enough information to trigger a specific control action.
struct ControlAction
{	
	ControlAction() = default;
	
	// A constructor for emplacing.
	// 
	ControlAction(char const* p_ControlName, Control::Actions p_Action);

	// Read a control action from XML. 
	//
	// p_Document:	The XML document that the node belongs to.
	// p_Node:		The XML node to read the control action from.
	//	
	// Returns:		True if the action was read successfully, false otherwise.
	//
	bool ReadFromXML(xmlDocPtr p_Document, xmlNodePtr p_Node);
	
	// Read a control action from JSON. 
	//
	// p_Object:	The JSON object representing a control action.
	//
	// Returns:		True if the action was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value::ConstObject const& p_Object);

	// Attempt to get the control corresponding to the control action.
	//
	// Returns:	The control if successful, null otherwise.
	//
	Control* GetControl();
	
	// Constants.
	static constexpr unsigned int ms_ControlNameCapacity = 32;
	
	// The name of the control to manipulate.
	char					m_ControlName[ms_ControlNameCapacity];
	
	// The action for the control.
	Control::Actions	m_Action;
	
	// A handle to the control for fast lookup.
	ControlHandle		m_ControlHandle;
};


// Functions
//

// Initialize all of the controls.
//
// p_Configs:	Configuration parameters for the controls to add.
//
void ControlsInitialize(std::vector<ControlConfig> const& p_Configs);

// Uninitialize all of the controls.
//
void ControlsUninitialize();

// Process all of the controls.
//
void ControlsProcess();

// Create a new control with the provided config. Control names must be unique.
//
// p_Config:	Configuration parameters for the control.
//
// Returns:		True if the control was successfully created, false otherwise.
//
bool ControlsCreateControl(ControlConfig const& p_Config);

// Stop all of the controls.
//
void ControlsStopAll();

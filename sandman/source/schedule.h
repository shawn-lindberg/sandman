#pragma once

// Types
//

// Forward declaration.
class Control;

// Functions
//

// Initialize the schedule.
//
// p_Controls:		The array of controls.
// p_NumControls:	The number of controls.
//
void ScheduleInitialize(Control* p_Controls, unsigned int p_NumControls);

// Uninitialize the schedule.
// 
void ScheduleUninitialize();

// Start the schedule.
//
void ScheduleStart();

// Stop the schedule.
//
void ScheduleStop();

// Determine whether the schedule is running.
//
bool ScheduleIsRunning();

// Process the schedule.
//
void ScheduleProcess();


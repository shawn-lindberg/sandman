#pragma once

#include "timer.h"

// Types
//

// Handles dealing with an input device.
//
class Input
{
	public:
		
		// Handle initialization.
		//
		// p_DeviceName:	The name of the input device that this will manage.
		//
		void Initialize(char const* p_DeviceName);

		// Handle uninitialization.
		//
		void Uninitialize();
		
		// Process a tick.
		//
		void Process();
		
	private:

		// Constants.
		enum
		{
			DEVICE_NAME_CAPACITY = 64,
			INVALID_FILE_HANDLE = -1,
		};
		
		// The amount of time to wait between failing to open the device.
		static constexpr unsigned int ms_DeviceOpenRetryDelayMS = 1000;
		
		// Close the input device.
		// 
		// p_WasFailure:	Whether the device is being closed due to a failure or not.
		// p_Format:		Standard printf format string.
		// ...:				Standard printf arguments.
		//
		void CloseDevice(bool p_WasFailure, char const* p_Format, ...);
		
		// The name of the device to get input from.
		char m_DeviceName[DEVICE_NAME_CAPACITY];
		
		// The device file handle (file descriptor).
		int m_DeviceFileHandle = INVALID_FILE_HANDLE;	
		
		// Indicates that the device open has failed before.
		bool m_DeviceOpenHasFailed = false;
		
		// A time, so we can tell how long to wait before trying to open the device again.
		Time m_LastDeviceOpenFailTime;
};

#pragma once

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
		
		// The name of the device to get input from.
		char m_DeviceName[DEVICE_NAME_CAPACITY];
		
		// The device file handle (file descriptor).
		int m_DeviceFileHandle = INVALID_FILE_HANDLE;	
};

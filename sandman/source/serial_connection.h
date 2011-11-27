#pragma once

// Types
//

// Hides the details of a serial connection.
//
class SerialConnection
{
	public:

		// Initialize the connection.
		//
		// p_PortName:	The name of the serial port.
		//
		// returns:		True for success, false otherwise.
		//
		bool Initialize(char const* const p_PortName);

		// Uninitialize the connection.
		//
		void Uninitialize();

		// Reads a string from the serial connection.
		//
		// p_NumBytesRead:				(Output) The number of bytes actually read.
		// p_StringBuffer:				(Output) A buffer to hold the string that is read.
		// p_StringBufferCapacityBytes:	The capacity of the string buffer.
		//
		// returns:						True for success, false otherwise.
		//
		bool ReadString(unsigned long int& p_NumBytesRead, char* p_StringBuffer, 
			unsigned long int const p_StringBufferCapacityBytes);

		// Write a string to the serial connection.
		//
		// p_NumBytesWritten:	(Output) The number of bytes actually written.
		// p_String:			The string to write.
		//
		// returns:				True for success, false otherwise.
		//
		bool WriteString(unsigned long int& p_NumBytesWritten, char const* const p_Text);

	private:

		// Constants.
		enum
		{
			PORT_NAME_MAX_LENGTH = 16,
		};

		// Mostly for debugging.
		char m_PortName[PORT_NAME_MAX_LENGTH];

		// Windows file handle.
		void* m_Handle;
};

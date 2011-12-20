#include "serial_connection.h"

#include <stdio.h>
#include <Windows.h>

#include "logger.h"

// Functions
//

// SerialConnection members

// Initialize the connection.
//
// p_PortName:	The name of the serial port.
//
// returns:		True for success, false otherwise.
//
bool SerialConnection::Initialize(char const* const p_PortName)
{
	// Copy the port name.
	strncpy_s(m_PortName, p_PortName, PORT_NAME_MAX_LENGTH);
	m_PortName[PORT_NAME_MAX_LENGTH - 1] = 0;

	// Open the serial port.
	m_Handle = CreateFile(p_PortName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		LoggerAddMessage("Failed to open serial port %s: error code 0x%x", m_PortName, GetLastError());
		return false;
	}
	
	// Set some serial params.
	DCB l_SerialParams;
	if (GetCommState(m_Handle, &l_SerialParams) == false)
	{
		LoggerAddMessage("Failed to get serial port %s params: error code 0x%x", m_PortName, GetLastError());
		CloseHandle(m_Handle);
		return false;
	}

	// Just set baud rate.
	l_SerialParams.BaudRate = CBR_9600;

	if (SetCommState(m_Handle, &l_SerialParams) == false)
	{
		LoggerAddMessage("Failed to set serial port %s params: error code 0x%x", m_PortName, GetLastError());
		CloseHandle(m_Handle);
		return false;
	}

	// Set some serial timeouts.  We want non-blocking reads and normal writes.
	COMMTIMEOUTS l_SerialTimeouts;
	{
		l_SerialTimeouts.ReadIntervalTimeout = MAXDWORD;
		l_SerialTimeouts.ReadTotalTimeoutConstant = 0;
		l_SerialTimeouts.ReadTotalTimeoutMultiplier = 0;
		l_SerialTimeouts.WriteTotalTimeoutConstant = 50;
		l_SerialTimeouts.WriteTotalTimeoutMultiplier = 10;
	}

	if (SetCommTimeouts(m_Handle, &l_SerialTimeouts) == false)
	{
		LoggerAddMessage("Failed to set serial port %s timeouts: error code 0x%x", m_PortName, GetLastError());
		CloseHandle(m_Handle);
		return false;
	}

	return true;
}

// Uninitialize the connection.
//
void SerialConnection::Uninitialize()
{
	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	CloseHandle(m_Handle);
	m_Handle = INVALID_HANDLE_VALUE;
}

// Reads a string from the serial connection.
//
// p_NumBytesRead:				(Output) The number of bytes actually read.
// p_StringBuffer:				(Output) A buffer to hold the string that is read.
// p_StringBufferCapacityBytes:	The capacity of the string buffer.
//
// returns:						True for success, false otherwise.
//
bool SerialConnection::ReadString(unsigned long int& p_NumBytesRead, char* p_StringBuffer,
	unsigned long int const p_StringBufferCapacityBytes)
{
	p_NumBytesRead = 0;
	
	// Sanity check.
	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Read serial port text, if there is any.
	if (ReadFile(m_Handle, p_StringBuffer, p_StringBufferCapacityBytes - 1, &p_NumBytesRead, NULL) == false)
	{
		LoggerAddMessage("Failed to read from serial port %s: error code 0x%x", m_PortName, GetLastError());
		return false;
	}

	// Terminate it.
	p_StringBuffer[p_NumBytesRead] = '\0';

	return true;
}

// Write a string to the serial connection.
//
// p_NumBytesWritten:	(Output) The number of bytes actually written.
// p_String:			The string to write.
//
// returns:				True for success, false otherwise.
//
bool SerialConnection::WriteString(unsigned long int& p_NumBytesWritten, char const* const p_String)
{
	p_NumBytesWritten = 0;

	// Sanity check.
	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Write the string.
	unsigned long int const l_NumBytesToWrite = strlen(p_String);

	if (WriteFile(m_Handle, p_String, l_NumBytesToWrite, &p_NumBytesWritten, NULL) == false)
	{
		LoggerAddMessage("Failed to write to serial port %s: error code 0x%x", m_PortName, GetLastError());
		return false;
	}

	// Did we write it all?
	if (p_NumBytesWritten != l_NumBytesToWrite)
	{
		LoggerAddMessage("Failed to write full data to serial port %s: error code 0x%x", m_PortName);
	}

	return true;
}


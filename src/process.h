/**
 * @file process_handler.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-07-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <iostream>
#include "hack.h"

using namespace std;

#define BUFLEN 256

/**
 * @brief Process class definitions
 * 
 */
class LIBHACK_API Process
{
	public:
		/**
		 * @brief Construct a new Process object
		 * 
		 * @param name Name of executable to be opened
		 */
		explicit Process(const char *name);

		/**
		 * @brief Construct a new Process object
		 * 
		 */
		Process();

		/**
		 * @brief Destroy the Process object
		 * 
		 */
		~Process();

		/**
		 * @brief Get the Base Address object
		 * 
		 * @return DWORD64
		 */
		DWORD64 GetBaseAddress64();

		/**
		 * @brief Open the process
		 * 
		 * @return true On success
		 * @return false On error
		 */
		bool Open();

		/**
		 * @brief Check if the process is open
		 * 
		 * @return true 
		 * @return false 
		 */
		bool IsOpen();

		/**
		 * @brief Closes the process
		 * 
		 */
		void Close();

		/**
		 * @brief 
		 * 
		 * @param address 
		 * @param data 
		 * @param len 
		 * @return SIZE_T 
		 */
		SIZE_T ReadAddress64(DWORD64 address, void *data, size_t len);

		/**
		 * @brief Write process memory in the specified address
		 * 
		 * @param address Address of process
		 * @param data Pointer to data to be written
		 * @param len Length of data parameter
		 * @return SIZE_T 
		 */
		SIZE_T WriteAddress64(DWORD64 address, void *data, size_t len);

		/**
		 * @brief 
		 * 
		 * @param address 
		 * @return int 
		 */
		int ReadIntFromAddress64(DWORD64 address);

		/**
		 * @brief 
		 * 
		 * @return unsigned long 
		 */
		unsigned long GetID();

		/**
		 * @brief Get the Name object
		 * 
		 * @return const char* 
		 */
		const char *GetName();

		/**
		 * @brief 
		 * 
		 * @param process_name 
		 * @return true 
		 * @return false 
		 */
		static bool Find(const char *process_name);

	private:
		/**
		 * @brief Name of the process
		 * 
		 */
		string name;

		/**
		 * @brief process PID
		 * 
		 */
		unsigned long identifier;

		/**
		 * @brief Path to the process file location
		 * 
		 */
		string path;

		/**
		 * @brief process base address
		 * 
		 */
		unsigned long long base_address;

		/**
		 * @brief Handle to process
		 * 
		 */
		HANDLE hProcess;
		
		/**
		 * @brief True process is open, False otherwise
		 * 
		 */
		bool is_open;

		/**
		 * @brief Handle to module
		 * 
		 */
		HMODULE hModule;
};
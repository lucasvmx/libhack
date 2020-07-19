/**
 * @file log.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2019-08-10
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#include <iostream>
#include <cstdarg>
#include <fstream>
#include "hack.h"

using namespace std;

#define DEFAULT_FILENAME    "logfile.txt"

#ifndef ARRAYSIZE
#define ARRAYSIZE(x)    (sizeof(x)/sizeof(x[0]))
#endif

class LIBHACK_API Log
{
    public:
        /**
         * @brief Construct a new Log object
         * 
         * @param filename Name of the logfile
         */
        explicit Log(const char *filename);

        /**
         * @brief Construct a new Log object
         * 
         */
        Log();

        /**
         * @brief Destroy the Log object
         * 
         */
        ~Log();

        /**
         * @brief Write the Log to specified file
         * 
         * @param message Message to be printed 
         * @param ... Arguments list
         */
        void Write(const char *msg, ...);
        
        /**
         * @brief 
         * 
         */
        void start();

        /**
         * @brief 
         * 
         */
        void end();

        void OpenAppend();

        /**
         * @brief 
         * 
         * @return true 
         * @return false 
         */
        bool IsOpen() const;

    private:

        /**
         * @brief Name of the logfile
         * 
         */
        char *filename;

        /**
         * @brief File handle to logfile
         * 
         */
        FILE *log_handle;

        /**
         * @brief Flag to store if it's open or not
         * 
         */
        bool is_open;
};

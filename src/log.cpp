/**
 * @file log.cpp
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2019-08-10
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#include "log.h"

Log::Log(const char *filename)
{
    if(filename != nullptr)
        this->filename = const_cast<char*>(filename);

    this->log_handle = nullptr;
    this->is_open = false;
}

Log::Log()
{
    this->filename = const_cast<char*>(DEFAULT_FILENAME);
    this->log_handle = nullptr;
    this->is_open = false;
}

void Log::start()
{
    log_handle = fopen(this->filename, "w");
    if(log_handle)
        fclose(log_handle);
}

void Log::end()
{
    if(this->log_handle) {
        fclose(this->log_handle);
        this->is_open = false;
    }
}

void Log::OpenAppend()
{
    this->log_handle = fopen(this->filename, "a");
    this->is_open = true;
}

void Log::Write(const char *msg, ...)
{
    va_list list;

    if(!this->IsOpen())
        this->OpenAppend();

    va_start(list, msg);
    vfprintf(this->log_handle, msg, list);
    va_end(list);

    this->end();
}

bool Log::IsOpen() const
{
    return this->is_open;
}

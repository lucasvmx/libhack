/**
 * @file consts.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Constants used belong the program
 * @version 0.1
 * @date 2020-08-14
 * 
 * @copyright Copyright (c) 2020
 * 
 */

/**
 * @brief Internal buffer size
 * 
 */
#define BUFLEN 256

/**
 * @brief Maximum size allocated to store version data
 * 
 */
#define VERSION_NUMBER_LEN  16

#ifdef __linux__
/**
 * @brief Maximum size of a process
 * 
 */
#define MAX_PROCESS_NAME 64
#endif // __linux__
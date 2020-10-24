/**
 * @file version_info.cpp
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Example of how to get libhack version information
 * @version 0.1
 * @date 2020-09-21
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <iostream>
#include "../hack.h"

using namespace std;

int main()
{
    cout << "Version" << endl;
    cout << libhack_getversion() << endl;
    cout << "Build date: " << endl;
    cout << libhack_get_utc_build_date() << endl;

    return 0;
}
/**
 * @file mingw_aliases.h
 * @author Lucas Vieira (lucas.engen.cc@gmail.com)
 * @brief Alias to mingw specific functions
 * @version 0.1
 * @date 2020-07-18
 * 
 * @copyright Copyright (c) 2020
 * 
 */
 
#ifndef MINGW_ALIASES_H
#define MINGW_ALIASES_H

#define K32EnumProcessModulesEx(p1,p2,p3,p4,p5) EnumProcessModulesEx(p1,p2,p3,p4,p5)
#define K32GetModuleBaseNameA(p1,p2,p3,p4)		GetModuleBaseNameA(p1,p2,p3,p4)

#endif


/**
 * @file utilities.hpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#ifndef _LFS_UTILITIES_HPP_
#define _LFS_UTILITIES_HPP_

#include <ctime>
#include <iostream>
#include <string>

using namespace std;

namespace lazyfs {

/**
 * @brief Gets the string that represents the current date and time
 *
 * @return const std::string current date time in the following format %Y-%m-%d.%X
 */
const std::string current_time_string ();

/**
 * @brief Prints a message with the current time and flushes the stdout
 *
 * @param msg the message to print
 */
void _print_with_time (string msg);

} // namespace lazyfs

#endif
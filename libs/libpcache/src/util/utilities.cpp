
/**
 * @file utilities.cpp
 * @author João Azevedo joao.azevedo@inesctec.pt
 *
 * @copyright Copyright (c) 2020-2022 INESC TEC.
 *
 */

#include <cache/util/utilities.hpp>

using namespace std;

namespace cache::util {

const std::string current_time_string () {

    time_t now = time (0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime (&now);

    strftime (buf, sizeof (buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

void _print_with_time (string msg) {

    cout << current_time_string () << " - " << msg << endl;
    fflush (stdout);
}

} // namespace cache::util
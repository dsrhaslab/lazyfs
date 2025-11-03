#ifndef FAULTS_HANDLER_HPP
#define FAULTS_HANDLER_HPP

#include <string>
#include <vector>


/**
 * @brief Parses a clear-cache command string.
 * 
 * @param command_str the command string to parse
 * @param crash_timing reference to the crash timing
 * @param crash_operation reference to the crash operation
 * @param crash_from_rgx reference to the "from" path regex
 * @param crash_to_rgx reference to the "to" path regex
 * @return true if parsing was successful, false otherwise
*/
bool parse_crash(std::string command_str, std::string &crash_timing, std::string &crash_operation, std::string &crash_from_rgx, std::string &crash_to_rgx);

/**
 * @brief Parses a torn-op command string.
 * 
 * @param command_str the command string to parse
 * @param file reference to the file path
 * @param parts reference to the parts of the write
 * @param parts_bytes reference to parts of the write  
 * @param persist reference to which parts to persist
 * @param ret reference to if the operation should return before crashing
 * @return true if parsing was successful, false otherwise
*/
bool parse_torn_op(std::string command_str, std::string &file, std::string &parts, std::string &parts_bytes, std::string &persist, std::string &ret);

/**
 * @brief Parses a torn-seq command string.
 * 
 * @param command_str the command string to parse
 * @param file reference to the file path
 * @param op reference to the operation type
 * @param persist reference to which operations to persist
 * @param ret reference to if the operation should return before crashing
 * @return true if parsing was successful, false otherwise
*/
bool parse_torn_seq(std::string command_str, std::string &file, std::string &op, std::string &persist, std::string &ret);

/**
 * @brief Parses a snapshot command string.
 * 
 * @param command_str the command string to parse
 * @param files reference to the files to include in the snapshot
 * @param files_rgx reference to the regex for files to include in the snapshot
 * @param save reference to the path where the snapshot should be saved
 * @return true if parsing was successful, false otherwise
*/
bool parse_snapshot (std::string command_str, 
                     std::string& files,
                     std::regex&  files_rgx, 
                     std::string& save);


/**
 * @brief Parses a fault command string and returns a FaultParamsMap.
 */
FaultParamsMap parse_fault_command (const std::string command_str);




#endif // FAULTS_HANDLER_HPP

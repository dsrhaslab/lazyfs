#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <errno.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <unistd.h>

// LazyFS specific imports
#include <faults/faults.hpp>
#include <lazyfs/lazyfs.hpp>
#include <faults_handler.hpp>


using namespace lazyfs;

bool parse_crash (string command_str,
                  string& crash_timing,
                  string& crash_operation,
                  string& crash_from_rgx,
                  string& crash_to_rgx) {

    std::regex rgx_global ("::");
    std::regex rgx_attrib ("=");
    std::sregex_token_iterator iter_glob (command_str.begin (), command_str.end (), rgx_global, -1);
    std::sregex_token_iterator end;

    crash_timing    = "none";
    crash_operation = "none";
    crash_from_rgx  = "none";
    crash_to_rgx    = "none";

    bool valid_fault = true;
    vector<string> errors;

    for (; iter_glob != end; ++iter_glob) {

        string current = string (*iter_glob);

        if (current.rfind ("op=", 0) == 0) {

            string tmp_op = current.erase (0, current.find ("=") + 1);

            if (tmp_op.length () != 0 && faults::Fault::allow_clear_fs_operations.find (tmp_op) !=
                                             faults::Fault::allow_clear_fs_operations.end ())
                crash_operation = tmp_op;
            else {
                valid_fault = false;
                errors.push_back ("operation not available");
            }

        } else if (current.rfind ("timing=", 0) == 0) {

            string tmp_tim = current.erase (0, current.find ("=") + 1);

            if (tmp_tim.length () != 0 && (tmp_tim == "before" || tmp_tim == "after"))
                crash_timing = tmp_tim;
            else {
                errors.push_back ("timing should be 'before' or 'after'");
                valid_fault = false;
            }

        } else if (current.rfind ("from_rgx=", 0) == 0) {

            string tmp_from_rgx = current.erase (0, current.find ("=") + 1);

            if (tmp_from_rgx.length () != 0)
                crash_from_rgx = tmp_from_rgx;

        } else if (current.rfind ("to_rgx=", 0) == 0) {

            string tmp_to_rgx = current.erase (0, current.find ("=") + 1);

            if (tmp_to_rgx.length () != 0)
                crash_to_rgx = tmp_to_rgx;
        }
    }

    bool is_from_to = false;

    if (faults::Fault::fs_op_multi_path.find (crash_operation) !=
        faults::Fault::fs_op_multi_path.end ()) {

        if (crash_from_rgx == "none" && crash_to_rgx == "none") {
            errors.push_back ("should specify 'from_rgx' and/or 'to_rgx'");
            valid_fault = false;
        }

        is_from_to = true;

    } else {

        if (crash_from_rgx == "none" || crash_to_rgx != "none") {
            valid_fault = false;
            errors.push_back ("should specify 'from_rgx' regex (and not 'to_rgx')");
        }
    }

    if (valid_fault) {
        spdlog::critical ("[lazyfs.faults.worker]: received: VALID crash fault:");
        spdlog::critical ("[lazyfs.faults.worker]: => crash: timing = {}", crash_timing);
        spdlog::critical ("[lazyfs.faults.worker]: => crash: operation = {}", crash_operation);
        spdlog::critical ("[lazyfs.faults.worker]: => crash: from regex path = {}", crash_from_rgx);

        if (is_from_to)
            spdlog::critical ("[lazyfs.faults.worker]: => crash: to regex path = {}", crash_to_rgx);

    } else {

        spdlog::error ("[lazyfs.faults.worker]: received: INVALID crash fault:");

        for (auto const err : errors) {
            spdlog::error ("[lazyfs.faults.worker]: crash fault error: {}", err);
        }
    }

    return valid_fault;
}

bool parse_torn_op (string command_str,
                    string& file,
                    string& parts,
                    string& parts_bytes,
                    string& persist,
                    string& ret) {

    std::regex rgx_global ("::");
    std::regex rgx_attrib ("=");
    std::sregex_token_iterator iter_glob (command_str.begin (), command_str.end (), rgx_global, -1);
    std::sregex_token_iterator end;

    bool valid_fault = true;
    vector<string> errors;

    for (; iter_glob != end; ++iter_glob) {

        string current = string (*iter_glob);

        spdlog::info ("[lazyfs.faults.worker]: {}", current);

        if (current.rfind ("file=", 0) == 0) {

            string tmp_file = current.erase (0, current.find ("=") + 1);

            if (tmp_file.length () != 0)
                file = tmp_file;
            else {
                errors.push_back ("file not specified");
                valid_fault = false;
            }

        } else if (current.rfind ("parts=", 0) == 0) {

            string tmp_parts = current.erase (0, current.find ("=") + 1);
            std::regex pattern (R"(\d+)");

            if (!std::regex_match (tmp_parts, pattern)) {
                errors.push_back ("parts should be a number");
                valid_fault = false;
            } else
                parts = tmp_parts;

        } else if (current.rfind ("parts_bytes=", 0) == 0) {

            string tmp_parts_bytes = current.erase (0, current.find ("=") + 1);
            std::regex pattern (R"((\d+,)*\d+)");

            if (!std::regex_match (tmp_parts_bytes, pattern)) {
                errors.push_back ("parts_bytes should be a list of numbers separated by commas");
                valid_fault = false;
            } else
                parts_bytes = tmp_parts_bytes;

        } else if (current.rfind ("persist=", 0) == 0) {

            string tmp_persist = current.erase (0, current.find ("=") + 1);
            std::regex pattern (R"((\d+,)*\d+)");

            if (!std::regex_match (tmp_persist, pattern)) {
                errors.push_back ("persist should be a list of numbers separated by commas");
                valid_fault = false;
            } else
                persist = tmp_persist;

        } else if (current.rfind ("return=", 0) == 0) {

            string tmp_ret = current.erase (0, current.find ("=") + 1);
            std::regex pattern (R"([Tt]rue|[Ff]alse)");

            if (std::regex_match (tmp_ret, pattern))
                ret = tmp_ret;
            else {
                errors.push_back ("return should be a boolean");
                valid_fault = false;
            }

        } else if (current != "lazyfs" && current != "torn-op") {
            errors.push_back ("unknown attribute");
            valid_fault = false;
        }
    }

    if (parts == "none" && parts_bytes == "none") {
        errors.push_back ("should specify 'parts' or 'parts_bytes', not both");
        valid_fault = false;
    }

    if (!valid_fault) {

        spdlog::error ("[lazyfs.faults.worker]: received: INVALID torn-op fault:");

        for (auto const err : errors) {
            spdlog::error ("[lazyfs.faults.worker]: torn-op fault error: {}", err);
        }
    }

    return valid_fault;
}

bool parse_torn_seq (string command_str, string& file, string& op, string& persist, string& ret) {

    std::regex rgx_global ("::");
    std::regex rgx_attrib ("=");
    std::sregex_token_iterator iter_glob (command_str.begin (), command_str.end (), rgx_global, -1);
    std::sregex_token_iterator end;

    bool valid_fault = true;
    vector<string> errors;

    for (; iter_glob != end; ++iter_glob) {

        string current = string (*iter_glob);

        if (current.rfind ("file=", 0) == 0) {

            string tmp_file = current.erase (0, current.find ("=") + 1);

            if (tmp_file.length () != 0)
                file = tmp_file;
            else {
                errors.push_back ("file not specified");
                valid_fault = false;
            }

        } else if (current.rfind ("op=", 0) == 0) {

            string tmp_op = current.erase (0, current.find ("=") + 1);

            if (tmp_op.length () != 0)
                op = tmp_op;
            else {
                errors.push_back ("operation not available");
                valid_fault = false;
            }
        } else if (current.rfind ("persist=", 0) == 0) {

            string tmp_per = current.erase (0, current.find ("=") + 1);
            std::regex pattern (R"((\d+,)*\d+)");

            if (!std::regex_match (tmp_per, pattern)) {
                errors.push_back ("persist should be a list of numbers separated by commas");
                valid_fault = false;
            } else
                persist = tmp_per;

        } else if (current.rfind ("return=", 0) == 0) {

            string tmp_ret = current.erase (0, current.find ("=") + 1);
            std::regex pattern (R"([Tt]rue|[Ff]alse)");

            if (std::regex_match (tmp_ret, pattern))
                ret = tmp_ret;
            else {
                errors.push_back ("return should be a boolean");
                valid_fault = false;
            }

        } else if (current != "lazyfs" && current != "torn-seq") {
            errors.push_back ("unknown attribute");
            valid_fault = false;
        }
    }

    if (!valid_fault) {

        spdlog::error ("[lazyfs.faults.worker]: received: INVALID torn-seq fault:");

        for (auto const err : errors) {
            spdlog::error ("[lazyfs.faults.worker]: torn-seq fault error: {}", err);
        }
    }

    return valid_fault;
}

bool parse_snapshot (string  command_str, 
                     string& files,
                     regex&  files_rgx, 
                     string& save) {

    std::regex rgx_global ("::");
    std::regex rgx_attrib ("=");
    std::sregex_token_iterator iter_glob (command_str.begin (), command_str.end (), rgx_global, -1);
    std::sregex_token_iterator end;

    bool valid_command = true;
    vector<string> errors;

    for (; iter_glob != end; ++iter_glob) {

        string current = string (*iter_glob);

        if (current.rfind ("files_rgx=", 0) == 0) {

            string tmp_files = current.erase (0, current.find ("=") + 1);

            if (tmp_files.length () != 0) {
                files_rgx = regex (tmp_files);
            } else {
                valid_command = false;
                errors.push_back ("files regex not specified");
            }

        } else if (current.rfind ("save=", 0) == 0) {

            string tmp_save = current.erase (0, current.find ("=") + 1);

            if (tmp_save.length () != 0) {
                save = tmp_save;

            } else {
                valid_command = false;
                errors.push_back ("save directory not specified");
            }
        }
    }

    if (!valid_command) {

        spdlog::error ("[lazyfs.faults.worker]: received: INVALID snapshot command:");

        for (auto const err : errors) {
            spdlog::error ("[lazyfs.faults.worker]: snapshot command error: {}", err);
        }
    }

    return valid_command;
}

FaultParamsMap parse_fault_command(std::string command_str) {
    FaultParamsMap params_map;

    std::regex rgx_global("::");
    std::regex rgx_attrib("=");

    std::sregex_token_iterator iter_glob(command_str.begin(), command_str.end(), rgx_global, -1);
    std::sregex_token_iterator end;

    for (; iter_glob != end; ++iter_glob) {
        std::string current = *iter_glob;

        // split on the first '=' without using token iterators
        auto pos = current.find('=');
        std::string key;
        std::string value;

        if (pos == std::string::npos) {
            key = current;
            value = "";
        } else {
            key = current.substr(0, pos);
            value = current.substr(pos + 1);
        }

        printf("Key: '%s' Value: '%s'\n", key.c_str(), value.c_str());

        if (!value.empty())
            params_map[key] = value;
    }

    return params_map;
}




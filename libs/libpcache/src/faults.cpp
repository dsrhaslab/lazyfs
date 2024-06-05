#include <faults/faults.hpp>

using namespace std;

namespace faults {

Fault::Fault(string type) {
    this->type = type;
}

Fault::~Fault(){}


// Torn seq fault
ReorderF::ReorderF(string op, vector<int> persist, int occurrence) : Fault(TORN_SEQ) {
    (this->counter).store(0);
    this->op = op;
    this->persist = persist;
    this->occurrence = occurrence;
    (this->group_counter).store(0);

}

ReorderF::ReorderF() : Fault(TORN_SEQ) {
	vector <int> v;
	(this->counter).store(0);
    (this->group_counter).store(0);
    this->op = "";
	this->persist = v;
    this->occurrence = 0;
}

ReorderF::~ReorderF(){}

bool ReorderF::check(string op, vector<int> persist) {
    if (op != "write") return false;

    bool check = true;
    for (auto & p : persist) {
        if (p <= 0) {
            check = false;
            break;
        }
    }
    return check;
}

vector<string> ReorderF::check_with_errors() {
    vector<string> errors;
    if (this->op != "write") {
        errors.push_back("operation must be write");
    }
    if (this->occurrence <= 0) {
        errors.push_back("occurrence must be greater than 0");
    }
    for (auto & p : this->persist) {
        if (p <= 0) {
            errors.push_back("persist must be greater than 0");
            break;
        }
    }
    return errors;
}


//Torn op fault

SplitWriteF::SplitWriteF(int occurrence, vector<int> persist, int parts) : Fault(TORN_OP) {
    (this->counter).store(0);
    this->occurrence = occurrence;
    this->persist = persist;
    this->parts = parts;
}

SplitWriteF::SplitWriteF(int occurrence, vector<int> persist, vector<int> parts_bytes) : Fault(TORN_OP) {
    (this->counter).store(0);
    this->occurrence = occurrence;
    this->persist = persist;
    this->parts = -1;
    this->parts_bytes = parts_bytes;
}

SplitWriteF::SplitWriteF() : Fault(TORN_OP) {
	vector <int> v;
    vector<int> p;
	(this->counter).store(0);
    this->occurrence = 0;
	this->persist = p;
    this->parts_bytes = v;
    this->parts = 0;
}

SplitWriteF::~SplitWriteF(){}
 
bool SplitWriteF::check(vector<int> persist, int parts) {
    bool check = true;
    if (parts >= 0) {
        for (auto & p : persist) {
            if (p <= 0 || p>parts) {
                check = false;
                break;
            }
        }
    } else check = false;
    return check;
}


bool SplitWriteF::check(vector<int> persist, vector<int> parts_bytes) {
    bool check = true;
    int parts = parts_bytes.size();
    for (auto & p : parts_bytes) {
        if (p <= 0) {
            return false;
        }
    }
    for (auto & p : persist) {
        if (p <= 0 || p>parts) {
            check = false;
            break;
        }
    }
    return check;
}

vector<string> SplitWriteF::check_with_errors(int occurrence, vector<int> persist, optional<int> parts, optional<vector<int>> parts_bytes) {
    vector<string> errors;
    if (occurrence <= 0) {
        errors.push_back("occurrence must be greater than 0");
    }

    if (parts.has_value() && parts.value() <= 0) {
        errors.push_back("parts must be greater than 0");
    }

    if (parts_bytes.has_value()) {
        for (auto & p : parts_bytes.value()) {
            if (p <= 0) {
                errors.push_back("parts_bytes must be greater than 0");
                break;
            }
        }
    }

    int nr_parts = 1;
    if (parts.has_value()) nr_parts = parts.value();
    else if (parts_bytes.has_value()) nr_parts = parts_bytes.value().size();
    else errors.push_back("parts or parts_bytes must be set");

    for (auto & p : persist) {
        if (p <= 0 || p > nr_parts) {
            errors.push_back("persist must be greater than 0 and less than parts");
            break;
        }
    }
    return errors;
}

//crash fault

const unordered_set<string> ClearF::allow_crash_fs_operations = {"unlink", 
                                                                "truncate", 
                                                                "fsync", 
                                                                "write", 
                                                                "create", 
                                                                "access", 
                                                                "open", 
                                                                "read", 
                                                                "rename", 
                                                                "link", 
                                                                "symlink"};

const unordered_set<string> ClearF::fs_op_multi_path = {"rename", "link", "symlink"};

ClearF::ClearF (string timing, string op, string from, string to, int occurrence, bool crash) : Fault(CLEAR) {
    this->timing = timing;
    this->op = op;
    this->from = from;
    this->to = to;
    this->occurrence = occurrence;
    this->crash = crash;
    (this->counter).store(0);
}

ClearF::~ClearF(){}

vector<string> ClearF::check_with_errors() {
    vector<string> errors;
    if (this->occurrence <= 0) {
        errors.push_back("occurrence must be greater than 0");
    }

    if (ClearF::allow_crash_fs_operations.find(this->op) == ClearF::allow_crash_fs_operations.end()) {
        errors.push_back("operation not available");
    }

    if (this->timing != "before" && this->timing != "after") {
        errors.push_back("timing must be 'before' or 'after'");
    }

    if (ClearF::fs_op_multi_path.find (this->op) != ClearF::fs_op_multi_path.end ()) {
        if (this->from == "none" || this->to == "none") {
            errors.push_back("'from' and 'to' must be set for operations with two paths");
        }
    } else {
        if (this->from == "none" || this->to != "none") {
            errors.push_back ("should specify 'from' (and not 'to')");
        }
    }

    return errors;
}

// namespace faults
};
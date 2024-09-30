#include <faults/faults.hpp>

using namespace std;

namespace faults {

Fault::Fault(string type) {
    this->type = type;
}

Fault::~Fault(){}


// Torn seq fault
ReorderF::ReorderF(string op, vector<int> persist, int occurrence, bool ret) : Fault(TORN_SEQ) {
    (this->counter).store(0);
    this->op = op;
    this->persist = persist;
    this->occurrence = occurrence;
    (this->group_counter).store(0);
    this->ret = ret;

}

ReorderF::ReorderF() : Fault(TORN_SEQ) {
	vector <int> v;
	(this->counter).store(0);
    (this->group_counter).store(0);
    this->op = "";
	this->persist = v;
    this->occurrence = 0;
    this->ret = true;
}

ReorderF::~ReorderF(){}

vector<string> ReorderF::validate() {
    vector<string> errors;
    if (this->op != "write") {
        errors.push_back("Operation must be \"write\".");
    }
    if (this->occurrence <= 0) {
        errors.push_back("Occurrence must be greater than 0.");
    }
    for (auto & p : this->persist) {
        if (p <= 0) {
            errors.push_back("Persist must be greater than 0.");
            break;
        }
    }
    return errors;
}


//Torn op fault

SplitWriteF::SplitWriteF(int occurrence, vector<int> persist, int parts, bool ret) : Fault(TORN_OP) {
    (this->counter).store(0);
    this->occurrence = occurrence;
    this->persist = persist;
    this->parts = parts;
    this->ret = ret;
}

SplitWriteF::SplitWriteF(int occurrence, vector<int> persist, vector<int> parts_bytes, bool ret) : Fault(TORN_OP) {
    (this->counter).store(0);
    this->occurrence = occurrence;
    this->persist = persist;
    this->parts = -1;
    this->parts_bytes = parts_bytes;
    this->ret = ret;
}

SplitWriteF::SplitWriteF() : Fault(TORN_OP) {
	vector <int> v;
    vector<int> p;
	(this->counter).store(0);
    this->occurrence = 0;
	this->persist = p;
    this->parts_bytes = v;
    this->parts = 0;
    this->ret = true;
}

SplitWriteF::~SplitWriteF() {}

vector<string> SplitWriteF::validate(int occurrence, vector<int> persist, optional<int> parts, optional<vector<int>> parts_bytes) {
    vector<string> errors;
    if (occurrence <= 0) {
        errors.push_back("Occurrence must be greater than 0.");
    }

    if (parts.has_value() && parts.value() <= 0) {
        errors.push_back("Parts must be greater than 0.");
    }

    if (parts_bytes.has_value()) {
        for (auto & p : parts_bytes.value()) {
            if (p <= 0) {
                errors.push_back("Parts_bytes values must be greater than 0.");
                break;
            }
        }
    }

    int nr_parts = 1;
    if (parts.has_value()) nr_parts = parts.value();
    else if (parts_bytes.has_value()) nr_parts = parts_bytes.value().size();
    else errors.push_back("Parts or parts_bytes must be defined.");

    for (auto & p : persist) {
        if (p <= 0 || p > nr_parts) {
            errors.push_back("Persist must be greater than 0 and less than parts.");
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

ClearF::ClearF (string timing, string op, string from, string to, int occurrence, bool crash, bool ret) : Fault(CLEAR) {
    this->timing = timing;
    this->op = op;
    this->from = from;
    this->to = to;
    this->occurrence = occurrence;
    this->crash = crash;
    (this->counter).store(0);
    this->ret = ret;
}

ClearF::~ClearF(){}

vector<string> ClearF::validate() {
    vector<string> errors;
    if (this->occurrence <= 0) {
        errors.push_back("Occurrence must be greater than 0.");
    }

    if (ClearF::allow_crash_fs_operations.find(this->op) == ClearF::allow_crash_fs_operations.end()) {
        errors.push_back("Operation not available.");
    }

    if (this->timing != "before" && this->timing != "after") {
        errors.push_back("Timing must be \"before\" or \"after\".");
    }

    if (ClearF::fs_op_multi_path.find (this->op) != ClearF::fs_op_multi_path.end ()) {
        if (this->from == "none" || this->to == "none") {
            errors.push_back("\"from\" and \"to\" must be set defined operations with two paths.");
        }
    } else {
        if (this->from == "none" || this->to != "none") {
            errors.push_back ("Should specify \"from\" (and not \"to\")");
        }
    }

    return errors;
}

// clear page fault

const unordered_set<string> ClearP::allow_crash_fs_operations = {"unlink", 
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

const unordered_set<string> ClearP::fs_op_multi_path = {"rename", "link", "symlink"};

ClearP::ClearP (string timing, string op, string from, string to, int occurrence, string pages, bool ret) : Fault(CLEAR) {
    this->timing = timing;
    this->op = op;
    this->from = from;
    this->to = to;
    this->occurrence = occurrence;
    this->pages = pages;
    (this->counter).store(0);
    this->ret = ret;
}

ClearP::~ClearP(){}

vector<string> ClearP::validate() {
    vector<string> errors;
    if (this->occurrence <= 0) {
        errors.push_back("Occurrence must be greater than 0.");
    }

    if (ClearP::allow_crash_fs_operations.find(this->op) == ClearP::allow_crash_fs_operations.end()) {
        errors.push_back("Operation not available.");
    }

    if (this->timing != "before" && this->timing != "after") {
        errors.push_back("Timing must be \"before\" or \"after\".");
    }

    if (this->pages != "first" && this->pages != "last" && this->pages != "first-half" && this->pages != "last-half" && this->pages != "random") {
        errors.push_back("Pages must be \"first\", \"last\", \"first-half\", \"last-half\" or \"random\".");
    }

    if (ClearP::fs_op_multi_path.find (this->op) != ClearP::fs_op_multi_path.end ()) {
        if (this->from == "none" || this->to == "none") {
            errors.push_back("\"from\" and \"to\" must be set defined operations with two paths.");
        }
    } else {
        if (this->from == "none" || this->to != "none") {
            errors.push_back ("Should specify \"from\" (and not \"to\")");
        }
    }

    return errors;
}

// namespace faults
};
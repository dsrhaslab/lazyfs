#include <faults/faults.hpp>
#include <iostream>

using namespace std;


namespace faults {

const unordered_set<string> Fault::allow_crash_fs_operations = {"unlink",
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
const unordered_set<string> Fault::fs_op_multi_path = {"rename", "link", "symlink"};

Fault::Fault(string type) {
    this->type = type;
}

Fault::~Fault(){}

void Fault::pretty_print() const {
    cout << "***** Fault *****" << endl;
    cout << "  Type: " << this->type << endl;
}


// Torn-seq Fault
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

void ReorderF::pretty_print() const {
    Fault::pretty_print();
    cout << "  Operation: " << this->op << endl;
    cout << "  Persist: ";
    for (const auto& p : this->persist) {
        cout << p << " ";
    }
    cout << endl;
    cout << "  Occurrence: " << this->occurrence << endl;
    cout << "  Return: " << (this->ret ? "true" : "false") << endl;
}



//Torn-op Fault
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

void SplitWriteF::pretty_print() const {
    Fault::pretty_print();
    cout << "  Occurrence: " << this->occurrence << endl;
    cout << "  Persist: ";
    for (const auto& p : this->persist) {
        cout << p << " ";
    }
    cout << endl;
    cout << "  Parts: " << this->parts << endl;
    cout << "  Parts Bytes: ";
    for (const auto& pb : this->parts_bytes) {
        cout << pb << " ";
    }
    cout << endl;
    cout << "  Return: " << (this->ret ? "true" : "false") << endl;
}


//Crash Fault
ClearF::ClearF (string timing, string op, string from, string to, int occurrence, bool crash, bool ret) : Fault(CLEAR) {
    Fault::pretty_print();
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

void ClearF::pretty_print() const {
    Fault::pretty_print();
    cout << "  Timing: " << this->timing << endl;
    cout << "  Operation: " << this->op << endl;
    cout << "  From: " << this->from << endl;
    cout << "  To: " << this->to << endl;
    cout << "  Occurrence: " << this->occurrence << endl;
    cout << "  Crash: " << (this->crash ? "true" : "false") << endl;
    cout << "  Return: " << (this->ret ? "true" : "false") << endl;
}


// Sync Pages Fault
SyncPagesF::SyncPagesF(string timing, string op, string from, string to, int occurrence, bool crash, bool ret, bool sync_other_files) : Fault(SYNC_PAGES) {
    this->timing = timing;
    this->op = op;
    this->from = from;
    this->to = to;
    this->occurrence = occurrence;
    (this->counter).store(0);
    this->ret = ret;
    this->crash = crash;
    this->sync_other_files = sync_other_files;
}

SyncPagesF::~SyncPagesF(){}

vector<string> SyncPagesF::validate() {
    vector<string> errors;
    if (this->occurrence <= 0) {
        errors.push_back("Occurrence must be greater than 0.");
    }

    if (SyncPagesF::allow_crash_fs_operations.find(this->op) == SyncPagesF::allow_crash_fs_operations.end()) {
        errors.push_back("Operation not available.");
    }

    if (this->timing != "before" && this->timing != "after") {
        errors.push_back("Timing must be \"before\" or \"after\".");
    }

    if (SyncPagesF::fs_op_multi_path.find (this->op) != SyncPagesF::fs_op_multi_path.end ()) {
        if (this->from == "none" || this->to == "none") {
            errors.push_back("\"from\" and \"to\" must be set on operations with two paths.");
        }
    } else {
        if (this->from == "none" || this->to != "none") {
            errors.push_back ("Should specify \"from\" (and not \"to\")");
        }
    }

    return errors;
}

bool SyncPagesF::equal(const SyncPagesF& other) const {
    return (this->timing == other.timing &&
            this->op == other.op &&
            this->from == other.from &&
            this->to == other.to);
}

void SyncPagesF::pretty_print() const {
    Fault::pretty_print();
    cout << "  Timing: " << this->timing << endl;
    cout << "  Operation: " << this->op << endl;
    cout << "  From: " << this->from << endl;
    cout << "  To: " << this->to << endl;
    cout << "  Occurrence: " << this->occurrence << endl;
    cout << "  Crash: " << (this->crash ? "true" : "false") << endl;
    cout << "  Return: " << (this->ret ? "true" : "false") << endl;
    cout << "  Sync other files: " << (this->sync_other_files ? "true" : "false") << endl;
}

// Sync pages with parts
SyncPagesPartsF::SyncPagesPartsF(string timing, string op, string from, string to, int occurrence, bool crash, bool ret, bool sync_other_files, Pages pages) : SyncPagesF(timing, op, from, to, occurrence, crash, ret, sync_other_files) {
    this->pages = pages;
}

SyncPagesPartsF::~SyncPagesPartsF(){}

SyncPagesPartsF::Pages SyncPagesPartsF::string_to_pages(string& pages) {
    if (pages == "all") return SyncPagesPartsF::Pages::ALL;
    else if (pages == "first-half") return SyncPagesPartsF::Pages::FIRST_HALF;
    else if (pages == "last-half") return SyncPagesPartsF::Pages::SECOND_HALF;
    else if (pages == "first") return SyncPagesPartsF::Pages::FIRST;
    else if (pages == "last") return SyncPagesPartsF::Pages::LAST;
    else if (pages == "first-and-last") return SyncPagesPartsF::Pages::FIRST_AND_LAST;
    else if (pages == "interleaved") return SyncPagesPartsF::Pages::INTERLEAVED;
    else if (pages == "random") return SyncPagesPartsF::Pages::RANDOM;
    else throw std::invalid_argument("Invalid page type: " + pages + ". Valid options are: all, first-half, last-half, first, last, first-and-last, interleaved, random.");
}

vector<string> SyncPagesPartsF::validate() {
    return SyncPagesF::validate();
}


void SyncPagesPartsF::pretty_print() const {
    SyncPagesF::pretty_print();
    cout << "  Pages: ";
    switch (this->pages) {
        case Pages::ALL: cout << "ALL"; break;
        case Pages::FIRST_HALF: cout << "FIRST_HALF"; break;
        case Pages::SECOND_HALF: cout << "SECOND_HALF"; break;
        case Pages::FIRST: cout << "FIRST"; break;
        case Pages::LAST: cout << "LAST"; break;
        case Pages::FIRST_AND_LAST: cout << "FIRST_AND_LAST"; break;
        case Pages::INTERLEAVED: cout << "INTERLEAVED"; break;
        case Pages::RANDOM: cout << "RANDOM"; break;
    }
    cout << endl;
}


// Sync pages numbered
SyncPagesNumberedF::SyncPagesNumberedF(string timing, string op, string from, string to, int occurrence, bool ret, bool crash, bool sync_other_files, vector<int> pages) : SyncPagesF(timing, op, from, to, occurrence, crash, ret, sync_other_files) {
    this->pages = pages;
}

SyncPagesNumberedF::~SyncPagesNumberedF(){}

vector<string> SyncPagesNumberedF::validate() {
    vector<string> errors = SyncPagesF::validate();

    if (this->pages.empty()) {
        errors.push_back("Pages to sync cannot be empty.");
    } else {
        for (const auto& page : this->pages) {
            if (page < 0) {
                errors.push_back("Page numbers must be non-negative.");
                break;
            }
        }
    }
    return errors;
}

void SyncPagesNumberedF::pretty_print() const {
    SyncPagesF::pretty_print();
    cout << "  Pages: ";
    for (const auto& page : this->pages) {
        cout << page << " ";
    }
    cout << endl;
}

// namespace faults
};
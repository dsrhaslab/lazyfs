#include <faults/faults.hpp>
#include <iostream>
#include <variant>
#include <algorithm>
#include <cctype>
#include <memory>

using namespace std; 


namespace faults {

const unordered_set<string> Fault::allow_clear_fs_operations = {"unlink",
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

/*******************************************************************************************************************/

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

    if (ClearF::allow_clear_fs_operations.find(this->op) == ClearF::allow_clear_fs_operations.end()) {
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
SyncPagesF::SyncPagesF(string file, string timing, string op, string from, string to, int occurrence, bool crash, bool ret, bool sync_other_files) : Fault(SYNC_PAGES) {
    this->file = file;
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

    if (SyncPagesF::allow_clear_fs_operations.find(this->op) == SyncPagesF::allow_clear_fs_operations.end()) {
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


SyncPagesF* SyncPagesF::tryCreate(const std::unordered_map<std::string,FaultParam>& params_map) {
    try {
        auto file = getParam<string>(params_map, "file", true, string_validator , nullptr);

        auto timing = getParam<string>(params_map, "timing", true, timing_validator, nullptr);

        if (!file.has_value() || !timing.has_value()) {
            throw InvalidFault("Missing required parameters for SyncPages fault: \"file\" and \"timing\".");
        }

        auto crash = getParam<bool>(params_map, "crash", false, nullptr, bool_converter);

        auto ret = getParam<bool>(params_map, "ret", false, nullptr, bool_converter);

        auto occurrence = getParam<int>(params_map, "occurrence", false, occurrence_validator, int_converter);

        auto op = getParam<string>(params_map, "op", false, string_validator, nullptr);

        auto from = getParam<string>(params_map, "from", false, string_validator, nullptr);
 
        auto to = getParam<string>(params_map, "to", false, string_validator, nullptr);

        if (timing.value() == "after" || timing.value() == "before") {
            if (!op.has_value() || !from.has_value()) {
                throw InvalidFault("The parameter \"op\" and \"from\" must be specified and valid for SyncPages faults with timing set to \"before\" or \"after\".");
            }
        } else if (timing.value() == "now") {
            if (op.has_value() || from.has_value() || occurrence.has_value()) {
                throw InvalidFault("The parameters \"op\" and \"from\" must not be specified for SyncPages faults with timing set to \"now\".");
            }
        } else {
            throw InvalidFault("The parameter \"timing\" must be either \"before\", \"after\" or \"now\".");
        }

        if (op.has_value()) {
            bool is_multi_path = (SyncPagesF::fs_op_multi_path.find(op.value()) != SyncPagesF::fs_op_multi_path.end());
            if (is_multi_path != (to.has_value())) {
                throw InvalidFault(is_multi_path ?
                    "The parameter \"to\" is needed for the specified \"op\"." :
                    "The parameter \"to\" is not needed for the specified \"op\".");
            }
        }

        auto sync_other_files = getParam<bool>(params_map, "sync_other_files", false, nullptr, bool_converter);

        /***** default values *****/
        if (!crash.has_value()) crash = false;
        if (!ret.has_value()) ret = false;
        if (!occurrence.has_value()) occurrence = 1;
        if (!sync_other_files.has_value()) sync_other_files = true;
        if (!to.has_value()) to = "";
        if (!op.has_value()) op = "";
        if (!from.has_value()) from = "";

        auto pages_parts = getParam<SyncPagesPartsF::Pages>(params_map, "pages", false, nullptr, SyncPagesPartsF::pages_parts_converter);

        auto pages_numbered = getParam<vector<int>>(params_map, "pages", false, vector_validator, vector_converter);

        if (pages_parts.has_value() && pages_numbered.has_value()) {
            throw InvalidFault ("Parameters \"pages\" as parts and \"pages\" as numbered are "
                                "mutually exclusive.");
        } else if (pages_parts.has_value ()) {

            SyncPagesPartsF* fault = new SyncPagesPartsF (file.value (),
                                                          timing.value (),
                                                          op.value (),
                                                          from.value (),
                                                          to.value (),
                                                          occurrence.value (),
                                                          crash.value (),
                                                          ret.value (),
                                                          sync_other_files.value (),
                                                          pages_parts.value ());
            return fault;

        } else if (pages_numbered.has_value ()) {

            SyncPagesNumberedF* fault = new SyncPagesNumberedF (file.value (),
                                                                timing.value (),
                                                                op.value (),
                                                                from.value (),
                                                                to.value (),
                                                                occurrence.value (),
                                                                ret.value (),
                                                                crash.value (),
                                                                sync_other_files.value (),
                                                                pages_numbered.value ());
            return fault;

        } else {
            throw InvalidFault (
                "Missing required parameter for SyncPages fault. You must specify either "
                "\"pages_parts\" (as parts) or \"pages_numbered\" (as numbered).");
        }

    } catch (const InvalidFault& e) {
        throw e;
    }
}

bool SyncPagesF::equal(const SyncPagesF& other) const {
    return (this->file == other.file &&
            this->timing == other.timing &&
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
SyncPagesPartsF::SyncPagesPartsF(string file, string timing, string op, string from, string to, int occurrence, bool crash, bool ret, bool sync_other_files, Pages pages) : SyncPagesF(file, timing, op, from, to, occurrence, crash, ret, sync_other_files) {
    this->pages = pages;
}

SyncPagesPartsF::~SyncPagesPartsF(){}

SyncPagesPartsF::Pages SyncPagesPartsF::pages_parts_converter(const string& pages) {
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

unordered_set<int> SyncPagesPartsF::filter_pages_to_sync (unordered_set<int> pages_id) {
    unordered_set<int> pages_id_filtered;
    int total_pages = pages_id.size();
    
    switch (this->pages) {
        case SyncPagesPartsF::Pages::ALL:
            for (int i = 0; i < total_pages; ++i) {
                pages_id_filtered.insert(i);
            }
            break;
        case SyncPagesPartsF::Pages::FIRST_HALF:
            for (int i = 0; i < total_pages / 2; ++i) {
                pages_id_filtered.insert(i);
            }
            break;
        case SyncPagesPartsF::Pages::SECOND_HALF:
            for (int i = total_pages / 2; i < total_pages; ++i) {
                pages_id_filtered.insert(i);
            }
            break;
        case SyncPagesPartsF::Pages::FIRST:
            pages_id_filtered.insert(0);
            break;
        case SyncPagesPartsF::Pages::LAST:
            pages_id_filtered.insert(total_pages - 1);
            break;
        case SyncPagesPartsF::Pages::FIRST_AND_LAST:
            pages_id_filtered.insert(0);
            pages_id_filtered.insert(total_pages - 1);
            break;
        case SyncPagesPartsF::Pages::INTERLEAVED:
            for (int i = 0; i < total_pages; i += 2) {
                pages_id_filtered.insert(i);
            }
            break;
        case SyncPagesPartsF::Pages::RANDOM:
            // Random logic can be implemented here
            // TO-DO
            break;
    }

    return pages_id_filtered;
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
SyncPagesNumberedF::SyncPagesNumberedF(string file, string timing, string op, string from, string to, int occurrence, bool ret, bool crash, bool sync_other_files, vector<int> pages) : SyncPagesF(file, timing, op, from, to, occurrence, crash, ret, sync_other_files) {
    this->pages = pages;
}

SyncPagesNumberedF::~SyncPagesNumberedF(){}

unordered_set<int> SyncPagesNumberedF::filter_pages_to_sync (unordered_set<int> pages_id) {
    unordered_set<int> pages_id_filtered;

    for (const auto& page : this->pages) {
        if (pages_id.find(page) != pages_id.end()) {
            pages_id_filtered.insert(page);
        }
    }
    return pages_id_filtered;
}

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

/************************************************************************************************/

InvalidFault::InvalidFault(const std::string& msg): std::runtime_error("Invalid Fault: " + msg) {}

bool timing_validator (const string& s) {
    string lower_s = s;
    transform (lower_s.begin (), lower_s.end (), lower_s.begin (), ::tolower);
    return lower_s == "before" || lower_s == "after" || lower_s == "now";
}

bool string_validator (const string& s) { return !s.empty () && s != "none"; }

bool vector_validator (const vector<int>& v) {
    if (v.empty ())
        return false;
    for (const auto& p : v) {
        if (p <= 0)
            return false;
    }
    return true;
}

bool to_validator (const string& s, const string& op) {
    if (faults::SyncPagesF::fs_op_multi_path.find (op) != faults::SyncPagesF::fs_op_multi_path.end ()) {
        return !s.empty () && s != "none";
    }
    throw InvalidFault (
        "The specified \"to\" parameter is not needed for the specified operation.");
}

bool occurrence_validator (const int& i) { return i > 0; }

bool bool_converter (const string& s) {
    string s_lower = s;
    transform (s_lower.begin (), s_lower.end (), s_lower.begin (), ::tolower);
    if (s_lower == "true")
        return true;
    if (s_lower == "false")
        return false;
    throw InvalidFault ("Conversion error from string to bool.");
}

int int_converter (const string& s) {
    try {
        return stoi (s);
    } catch (invalid_argument& e) {
        throw InvalidFault ("Conversion error from string to int.");
    }
}

vector<int> vector_converter (const string& s) {
    vector<int> result;
    size_t start = 0;
    size_t end   = s.find (',');
    while (end != string::npos) {
        string token = s.substr (start, end - start);
        try {
            int value = stoi (token);
            result.push_back (value);
        } catch (invalid_argument& e) {
            throw InvalidFault ("Conversion errorfrom string to vector<int>.");
        }
        start = end + 1;
        end   = s.find (',', start);
    }
    string token = s.substr (start);
    try {
        int value = stoi (token);
        result.push_back (value);
    } catch (invalid_argument& e) {
        throw InvalidFault ("Conversion error from string to vector<int>.");
    }
    return result;
}

template <typename T, typename... Ts>
constexpr bool is_one_of_v = (std::is_same_v<T, Ts> || ...);

template <typename T>
optional<T> getParam(const FaultParamsMap& params,
                     const std::string& key,
                     bool required,
                     bool (*validator)(const T&),
                     T (*converter)(const std::string&)) {

    optional<T> res = std::nullopt;

    auto it = params.find(key);
    if (it == params.end()) {
        if (required)
            throw InvalidFault("Missing parameter: " + key);
        return res;
    }

    const FaultParam& fp = it->second;

    // Are we asking for a type that's actually stored in the variant?
    if constexpr (is_one_of_v<T, int, double, bool, std::string, std::vector<int>>) {
        // safe to call std::get_if<T>
        if (auto p = std::get_if<T>(&fp)) {
            if (!validator || validator(*p))
                return *p;
            throw InvalidFault("Invalid value for: " + key);
        }
        // if the variant doesn't hold T now, maybe it's a string convertible to T
        if constexpr (!std::is_same_v<T, std::string>) {
            if (converter) {
                if (auto p_str = std::get_if<std::string>(&fp)) {
                    try {
                        T converted = converter(*p_str);
                        if (!validator || validator(converted))
                            return converted;
                        throw InvalidFault("Invalid value for: " + key);
                    } catch (const std::invalid_argument&) {
                        throw InvalidFault("Conversion error for parameter: " + key);
                    }
                }
            }
        }
    } else {
        // T is NOT an alternative of FaultParam.
        // We cannot call std::get_if<T> (would be a hard compile error).
        // Only option: try converting from stored string (if converter provided).
        if (converter) {
            if (auto p_str = std::get_if<std::string>(&fp)) {
                try {
                    T converted = converter(*p_str);
                    if (!validator || validator(converted))
                        return converted;
                    throw InvalidFault("Invalid value for: " + key);
                } catch (const std::invalid_argument&) {
                    throw InvalidFault("Conversion error for parameter: " + key);
                }
            }
        }
        // Parameter existed but wasn't convertible from stored type or converter missing.
        if (required)
            throw InvalidFault("Wrong type for parameter: " + key);
        return res;
    }

    // if we fell through here: parameter exists but wrong type and not required
    if (required)
        throw InvalidFault("Wrong type for parameter: " + key);
    return res;

    }
} // namespace faults


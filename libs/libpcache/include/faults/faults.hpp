#ifndef FAULTS_HPP 
#define FAULTS_HPP

#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

#define TORN_OP "torn-op"
#define TORN_SEQ "torn-seq"
#define CLEAR "clear-cache"
#define CLEAR_PAGE "clear-page"

using namespace std;

namespace faults {
/**
 * @brief Stores a generic fault programmed in the configuration file.
 */

class Fault {
  public:
    /**
     * @brief Type of fault. 
     */
    string type;

    /**
     * @brief Map of allowed operations to have a crash fault
     *
     */
    static const std::unordered_set<string> allow_crash_fs_operations;

    /**
     * @brief Map of operations that have two paths
     *
     */
    static const std::unordered_set<string> fs_op_multi_path;

    /**
      * @brief Default constructor of a new Fault object.
      */
    Fault();

    /**
     * @brief Construct a new Fault object.
     *
     * @param type Type of fault.
     */
    Fault(string type);

    /**
     * @brief Default destructor for a Fault object.
     */
    virtual ~Fault();
};

/*********************************************************************************************/

/**
 * @brief Fault for splitting writes in smaller writes and reordering them (torn-op fault).
*/
class SplitWriteF : public Fault {
  public:
    /**
     * @brief Write occurrence. For example, of this value is set to 3, on the third write for a certain path, a fault will be injected.
     */
    int occurrence;

    /**
     * @brief Protected counter of writes for a certain path.
     */
    std::atomic_int counter;

    /**
     * @brief Which parts of the write to persist.
     */
    vector<int> persist;

    /**
     * @brief Number of parts to divide the write. For example, if this value is set to 3, the write will be divided into 3 same sized writes.
     */
    int parts;

    /**
     * @brief Specify the bytes that each part of the write will have. 
     */
    vector<int> parts_bytes;

    /**
     * @brief True if LazyFS crashes only after completing the current system call. False if otherwise.
     */
    bool ret;

    /**
     * @brief Default constructor of a new SplitWriteF object.
     */
    SplitWriteF();

    /**
     * @brief Contruct a new SplitWriteF object.
     *
     * @param occurrence Write occurrence.
     * @param persist Which parts of the write to persist.
     * @param parts_bytes Division of the write in bytes.
     * @param ret If the current system call is finished before crashing.
     */
    SplitWriteF(int occurrence, vector<int> persist, vector<int> parts_bytes, bool ret);
    
    /**
     * @brief Default destructor for a SplitWriteF object.
     *
     * @param occurrence Write occurrence.
     * @param persist Which parts of the write to persist.
     * @param parts Number of same-sixed parts to divide the write.
     * @param ret If the current system call is finished before crashing.
     */
    SplitWriteF(int occurrence, vector<int> persist, int parts, bool ret);

    /**
     * @brief Default destructor for a SplitWriteF object.
     */
    ~SplitWriteF();

    /**
     * @brief Check if the parameters have correct values for the fault.
     * 
     * @param occurrence Write occurrence.
     * @param persist Which parts of the write to persist.
     * @param parts Number of same-sixed parts to divide the write.
     * @param parts_bytes Division of the write in bytes.
     * @return Vector with errors.
    */
    static vector<string> validate(int occurrence, vector<int> persist, optional<int> parts, optional<vector<int>> parts_bytes);

};

/*********************************************************************************************/

/**
 * @brief Fault for reordering system calls (torn-seq fault).
*/
class ReorderF : public Fault {

  public:

    /**
     * @brief Operation related to the fault. 
     */
    string op;
    /**
     * @brief When op is called sequentially for a certain path, count the number of calls. When the sequence is broken, counter is set to 0.
     */
    std::atomic_int counter;
    /**
     * @brief If op is a write and the vector is [3,4] it means that if op is called for a certain path sequentially, the 3th and 4th write will be persisted.
     */
    vector<int> persist;

    /**
     * @brief Group of writes occurrence. For example, of this value is set to 3, on the third group of consecutive writes for a certain path, a fault will be injected.
     */
    int occurrence;

    /**
     * @brief Counter for the groups of writes.
     */
    std::atomic_int group_counter;

    /**
     * @brief True if LazyFS crashes only after completing the current system call. False if otherwise.
     */
    bool ret;



  
    /**
     * @brief Construct a new Fault object.
     *
     * @param op System call (i.e. "write", ...)
     * @param persist Vector with operations to persist
     * @param occurrence occurrence of the group of writes to persist
     * @param ret If the current system call is finished before crashing.
     */
    ReorderF(string op, vector<int> persist, int occurrence, bool ret);

    /**
     * @brief Default constructor for Fault.
     */    
    ReorderF();

    ~ReorderF ();

    /**
     * @brief Check if the parameters have correct values for the fault.
     * 
     * @param occurrence occurrence of the group of writes to persist.
     * @param op System call (i.e. "write", ...).
     * @param persist Vector with operations to persist.
     * @return Vector with errors.
    */
    vector<string> validate();
};

/*********************************************************************************************/

/**
 * @brief Fault for clearing LazyFS's cache in a specific point of execution and, optionally, crash the process.
*/
class ClearF : public Fault {
  public:
    /**
     * @brief Timing of the fault ("before","after").
    */
    string timing;

    /**
     * @brief System call (i.e. "write", ...).
    */
    string op;

    /**
     * @brief Path of the system call.
    */
    string from;

    /**
     * @brief Path when op requires two paths (e.g., rename system call).
    */
    string to;

    /**
     * @brief Occurrence of the op.
    */
    int occurrence;

    /**
     * @brief Counter of the op.
    */
    std::atomic_int counter;

    /**
     * @brief If the fault is a crash fault.
    */
    bool crash;

    /**
     * @brief True if LazyFS crashes only after completing the current system call. False if otherwise.
     */
    bool ret;

    /**
     * @brief Constructor for Fault.
     * 
     * @param timing Timing of the fault ("before","after").
     * @param op System call (i.e. "write", ...).
     * @param from Path of the system call.
     * @param to Path when op requires two paths (e.g., rename system call).
     * @param occurrence Occurrence of the op.
     * @param crash If the fault is a crash fault.
     * @param ret If the current system call is finished before crashing.
    */
    ClearF(string timing, string op, string from, string to, int occurrence, bool crash, bool ret);
    
    ~ClearF ();

    /**
     * @brief Check if the parameters have correct values for the fault.
     * 
     * @return Vector with errors.
    */
    vector<string> validate();

};

/*********************************************************************************************/

/**
 * @brief Fault for persisting certain pages in LazyFS's cache in a specific point of execution and, optionally, crash the process.
*/
class PersistPageF : public Fault {
  public:
    /**
     * @brief Timing of the fault ("before","after").
    */
    string timing;

    /**
     * @brief System call (i.e. "write", ...).
    */
    string op;

    /**
     * @brief Path of the system call.
    */
    string from;

    /**
     * @brief Path when op requires two paths (e.g., rename system call).
    */
    string to;

    /**
     * @brief Occurrence of the op.
    */
    int occurrence;

    /**
     * @brief Counter of the op.
    */
    std::atomic_int counter;

    /**
     * @brief Pages that will be cleared.
    */
    string pages;

    /**
     * @brief True if LazyFS crashes only after completing the current system call. False if otherwise.
     */
    bool ret;

    /**
     * @brief Constructor for Fault.
     * 
     * @param timing Timing of the fault ("before","after").
     * @param op System call (i.e. "write", ...).
     * @param from Path of the system call.
     * @param to Path when op requires two paths (e.g., rename system call).
     * @param occurrence Occurrence of the op.
     * @param crash If the fault is a crash fault.
     * @param pages Pages to clear.
     * @param ret If the current system call is finished before crashing.
    */
    PersistPageF(string timing, string op, string from, string to, int occurrence, string pages, bool ret);
    
    ~PersistPageF ();

    /**
     * @brief Check if the parameters have correct values for the fault.
     * 
     * @return Vector with errors.
    */
    vector<string> validate();

};

// namespace faults
}

#endif // FAULTS_HPP
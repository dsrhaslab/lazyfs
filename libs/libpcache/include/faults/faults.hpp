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
#define SYNC_PAGES "sync-pages"

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

    /**
     * @brief Check if two faults are equal.
     * 
     * @return True if they are similar and false otherwise.
     */
    virtual bool equal(const Fault& other); //const = 0;

    /**
     * @brief Print the fault.
     */
    virtual void pretty_print() const;
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

    /**
     * @brief Print the fault.
     */
    void pretty_print() const override;

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

    /**
     * @brief Print the fault.
     */
    void pretty_print() const override;
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
     * @brief If LazyFS should crash after the fault is injected.
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

    /**
     * @brief Print the fault.
     */
    void pretty_print() const override;

};

/*********************************************************************************************/

/**
 * @brief Fault for persisting certain pages in LazyFS's cache in a specific point of execution.
*/
class SyncPagesF : public Fault {
  public:
    /**
     * @brief Path of the file where the fault will be injected.
     */
    string path;

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
     * @brief If LazyFS should crash after the fault is injected.
     */
    bool crash;

    /**
     * @brief True if LazyFS crashes only after completing the current system call. False if otherwise.
     */
    bool ret;

    /**
     * @brief True if we want to fsync other files.
     */
    bool sync_other_files;

    /**
      * @brief Default constructor of a new SyncPagesF object.
      */
    SyncPagesF();

    /**
      * @brief Parameterized constructor of a new SyncPagesF object.
      */
    SyncPagesF(string timing, string op, string from, string to, int occurrence, bool crash, bool ret, bool sync_other_files);
    
    /**
     * @brief Default destructor for a SyncPagesF object.
     */
    virtual ~SyncPagesF ();

    /**
     * @brief Check if the parameters have correct values for the fault.
     * 
     * @return Vector with errors.
    */
    virtual vector<string> validate();

    /**
     * @brief Compare if two SyncPagesF objects are similar. Two SyncPages faults are similar if they have the same timing, op, from and to. 
     * If the fault type is SyncPagesPartsF or SyncPagesNumberedF, two faults with different pages will colide, so the pages are not considered in the comparison.
     * 
     * @param other Another SyncPagesF object to compare with.
     * @return bool True if they're similar, false otherwise.
     */
    bool equal(const SyncPagesF& other) const;

    /**
     * @brief Print the fault.
     */
    virtual void pretty_print() const override;

};

class SyncPagesPartsF : public SyncPagesF {
  public:
    
    enum class Pages {
        ALL, // Sync all pages
        FIRST_HALF, // Sync first half of the pages
        SECOND_HALF, // Sync second half of the pages
        FIRST, // Sync first page
        LAST, // Sync last page
        FIRST_AND_LAST, // Sync first and last pages
        INTERLEAVED, // Sync interleaved pages
        RANDOM // Sync random pages
    };

    /**
     * @brief Pages to sync.
     */
    Pages pages;

    /**
     * @brief Default constructor of a new SyncPagesPartsF object.
     */
    SyncPagesPartsF();

    /**
     * @brief Parameterized constructor of a new SyncPagesPartsF object.
     *
     * @param timing Timing of the fault ("before","after").
     * @param op System call (i.e. "write", ...).
     * @param from Path of the system call.
     * @param to Path when op requires two paths (e.g., rename system call).
     * @param occurrence Occurrence of the op.
     * @param ret If the current system call is finished before crashing.
     * @param sync_other_files True if we want to fsync other files.
     * @param pages Pages to sync.
     */
    SyncPagesPartsF(string timing, string op, string from, string to, int occurrence, bool ret, bool crash, bool sync_other_files, Pages pages);

    /**
     * @brief Default destructor for a SyncPagesPartsF object.
     */
    ~SyncPagesPartsF();

    /**
     * @brief Convert a string to a Pages enum.
     * 
     * @param page String representation of the page type.
     * @return Pages enum value.
     */
    static Pages string_to_pages(string& pages);

    /**
     * @brief Check if the parameters have correct values for the fault.
     * 
     * @return Vector with errors.
    */
    vector<string> validate() override;

    /**
     * @brief Print the fault.
     */
    void pretty_print() const override;
};

class SyncPagesNumberedF : public SyncPagesF {
  public:
    
    /**
     * @brief List of pages to sync.
     */
    vector<int> pages; // Vector of page numbers to sync

    /**
     * @brief Parameterized constructor of a new SyncPagesNumberedF object.
     *
     * @param timing Timing of the fault ("before","after").
     * @param op System call (i.e. "write", ...).
     * @param from Path of the system call.
     * @param to Path when op requires two paths (e.g., rename system call).
     * @param occurrence Occurrence of the op.
     * @param ret If the current system call is finished before crashing.
     * @param sync_other_files True if we want to fsync other files.
     * @param pages Pages to sync.
     */
    SyncPagesNumberedF(string timing, string op, string from, string to, int occurrence, bool ret, bool crash, bool sync_other_files, vector<int> pages);

    /**
     * @brief Default destructor for a SyncPagesNumberedF object.
     */
    ~SyncPagesNumberedF();

    /**
     * @brief Check if the parameters have correct values for the fault.
     * 
     * @return Vector with errors.
    */
    vector<string> validate() override;

    /**
     * @brief Print the fault.
     */
    void pretty_print() const override;
};

} // namespace faults

#endif // FAULTS_HPP
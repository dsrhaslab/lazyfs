#include <spdlog/spdlog.h>
#include <unistd.h>
#include <tuple>
#include <vector>
#include <map>
#include <regex>
#include <lazyfs/lazyfs.hpp>

using namespace std;


namespace lazyfs {


/* Write class */
Write::Write() {
    this->path = this->buf = NULL;
}

Write::Write(const char* path, const char* buf, size_t size, off_t offset) {
    this->path = strdup(path);
    this->buf = (char *) malloc(sizeof(char) * size);
    for(int i=0; i<size; i++) {
        this->buf[i] = buf[i];
    }
    this->size = size;
    this->offset = offset;
}

Write::~Write() {
    free((char*)this->path);
    free((char*)this->buf);
    this->path = NULL;
    this->buf = NULL;
}

vector<string> LazyFS::add_torn_op_fault(string path, string parts, string parts_bytes, string persist, string ret_) {
    regex number ("\\d+");
    sregex_token_iterator iter(persist.begin(), persist.end(), number);
    sregex_token_iterator end;
    vector<int> persistv;

    while (iter != end) {
        persistv.push_back(std::stoi(*iter));
        ++iter;
    }

    vector<int> parts_bytes_v;
    if (parts_bytes != "none") {
        sregex_token_iterator iter(parts_bytes.begin(), parts_bytes.end(), number);
        sregex_token_iterator end;

        while (iter != end) {
            parts_bytes_v.push_back(std::stoi(*iter));
            ++iter;
        }   
    }

    int partsi = -1;
    if (parts != "none") {
        partsi = stoi(parts);
    }

    int occurrence=1;

    bool ret;
    std::regex pattern_true(R"([Tt]rue)");
    std::regex pattern_false(R"([Ff]alse)");
    if (std::regex_match(ret_, pattern_true)) ret = true;
    else if(std::regex_match(ret_, pattern_false)) ret = false;
    else ret = true;

    faults::SplitWriteF* fault;
    vector<string> errors;

    if (partsi != -1) {
        fault = new faults::SplitWriteF(occurrence, persistv, partsi, ret);
        errors = faults::SplitWriteF::validate(occurrence, persistv, partsi, std::nullopt);
    } else {
        fault = new faults::SplitWriteF(occurrence, persistv, parts_bytes_v, ret);
        errors = faults::SplitWriteF::validate(occurrence, persistv, std::nullopt, parts_bytes_v);
    }
    
    bool valid_fault = true;
    if (errors.size() == 0) {

        auto it = faults->find(path);
        if (it == faults->end()) {
            faults->insert({path, {fault}});
        } else {
            //Only allows one fault per file
            for (auto fault : it->second) {
                if (dynamic_cast<faults::SplitWriteF*>(fault) != nullptr) {
                    errors.push_back("Only one torn-op fault per file is allowed.");
                    valid_fault = false;
                }
            }
            if (valid_fault) it->second.push_back(fault);
        }
    } 
 
    if (!valid_fault) delete fault;

    return errors;
}

vector<string> LazyFS::add_torn_seq_fault(string path, string op, string persist, string ret_) {
    regex number ("\\d+");
    sregex_token_iterator iter(persist.begin(), persist.end(), number);
    sregex_token_iterator end;
    vector<int> persistv;

    while (iter != end) {
        persistv.push_back(std::stoi(*iter));
        ++iter;
    }

    bool ret;
    std::regex pattern_true(R"([Tt]rue)");
    std::regex pattern_false(R"([Ff]alse)");
    if (std::regex_match(ret_, pattern_true)) ret = true;
    else if(std::regex_match(ret_, pattern_false)) ret = false;
    else ret = true;

    faults::ReorderF* fault = new faults::ReorderF(op, persistv, 1, ret);
    vector<string> errors = fault->validate();

    bool valid_fault = true;
    if (errors.size() == 0) {
        auto it = faults->find(path);
        if (it == faults->end())
            faults->insert({path, {fault}});
        else {
            for (auto fault : it->second) {
                //Only allows one fault per file
                if (dynamic_cast<faults::ReorderF*>(fault) != nullptr) {
                    errors.push_back("Only one torn-seq fault per file is allowed.");
                    valid_fault = false;
                }
            }
            if (valid_fault) it->second.push_back(fault);
        }
    }
    
    if (!valid_fault) delete fault;

    return errors;
}

void LazyFS::restart_counter(string path, string op) {
    auto it = faults->find(path);
    if (it != faults->end()) {
        auto& v_faults = it->second;
        for (auto fault : v_faults) {
            faults::ReorderF* reorder_fault = dynamic_cast<faults::ReorderF*>(fault);
            if (reorder_fault && reorder_fault->op == op)  {
                reorder_fault->counter.store(0);
            }
        }
    }
}

bool LazyFS::check_and_delete_pendingwrite(const char* path) {
    bool res = false;
    (this->write_lock).lock();
    if (this->pending_write && (res = (strcmp(this->pending_write->path,path) == 0))) {
        delete(this->pending_write); 
        this->pending_write = NULL;
    }
    (this->write_lock).unlock();

    (this->injecting_fault_lock).lock();
    (this->injecting_fault).erase(std::remove((this->injecting_fault).begin(), (this->injecting_fault).end(), path), (this->injecting_fault).end());
    (this->injecting_fault_lock).unlock();

    return res;
}

faults::ReorderF* LazyFS::get_and_update_reorder_fault(string path, string op) {
    faults::ReorderF* fault_r = NULL;
    auto it = faults->find(path);
    if (it != faults->end()) {
        auto& v_faults = it->second;
        for (auto fault : v_faults) {
            faults::ReorderF* reorder_fault = dynamic_cast<faults::ReorderF*>(fault);
            if (reorder_fault && reorder_fault->op == op) {
                reorder_fault->counter.fetch_add(1); 
                fault_r = reorder_fault;
            }
        }
    } 
    return fault_r;
}

bool LazyFS::persist_write(const char* path, const char* buf, size_t size, off_t offset) {
    string path_str(path);
    int pw,fd;
    bool res = false;
    faults::ReorderF* fault = get_and_update_reorder_fault(path_str,"write");
    
    if (fault) { //Fault for path found
        (this->write_lock).lock();

        if (fault->counter.load()==2) {
            injecting_fault_lock.lock();
            injecting_fault.push_back(path);
            injecting_fault_lock.unlock();

            fault->group_counter.fetch_add(1);
        }

        if (this->pending_write!=NULL) {
            //Update group counter
            if (fault->group_counter.load() == fault->occurrence) {
                fd = open (this->pending_write->path, O_CREAT|O_WRONLY, 0666);
                pw = pwrite(fd,this->pending_write->buf,this->pending_write->size,this->pending_write->offset);
                spdlog::warn ("[lazyfs.faults]: Going to persist the write 1 for the path {}",path_str);
                close(fd);

                if (pw==this->pending_write->size) {
                    if ((fault->persist).size()==1) {
                        this_ ()->add_crash_fault ("after", "write", path_str, "none");            
                        spdlog::critical ("[lazyfs.faults]: Added crash fault ");
                        (this->write_lock).unlock();
                        return res;
                    }
                } else {
                    spdlog::warn ("[lazyfs.faults]: Something went wrong when trying to persist the 1st write for path {} (pwrite failed).", path);
                }
            }
            delete(this->pending_write); this->pending_write = NULL;
        }
        (this->write_lock).unlock();
        
        if (find((fault->persist).begin(), (fault->persist).end(), fault->counter.load()) != (fault->persist).end()) {  //If count is in vector persist
            if (fault->counter==1) { //If array persist has 1st write, we need to store this write until we know another write will happen
                (this->write_lock).lock();
                this->pending_write = new Write(path,buf,size,offset);
                (this->write_lock).unlock();
                
            } else {
                // At this point we know that the counter of writes is greater than 1 and the group counter was updated
                if (fault->group_counter.load() == fault->occurrence) {
                    fd = open(path, O_CREAT|O_WRONLY, 0666);
                    pw = pwrite(fd,buf,size,offset);
                    spdlog::warn ("[lazyfs.faults]: Going to persist the write {} for the path {}",fault->counter.load(),path_str);

                    close(fd);

                    if (pw == size) {
                        int max = fault->persist.back();
                        if (max == fault->counter) {
                            this_ ()->add_crash_fault ("after", "write", path_str, "none"); 
                            spdlog::critical ("[lazyfs.faults]: Added crash fault ");
                            res= true;
                        }
                    } else {
                        spdlog::warn ("[lazyfs.faults]: Something went wrong when trying to persist the {} write for path {} (pwrite failed).",fault->counter.load(),path);
                    }
                }     
            }
        }
    } 
    return res;
}


bool LazyFS::split_write(const char* path, const char* buf, size_t size, off_t offset) {
    string path_s(path);
    int fd;
    bool res = false;

    auto it = faults->find(path_s);
    if (it != faults->end()) {
        auto& v_faults = it->second;
        for (auto fault : v_faults) { 
            faults::SplitWriteF* split_fault = dynamic_cast<faults::SplitWriteF*>(fault);
            if (split_fault) {

                injecting_fault_lock.lock();
                injecting_fault.push_back(path);
                injecting_fault_lock.unlock();

                split_fault->counter.fetch_add(1);

                if (split_fault->counter.load() == split_fault->occurrence) {
                    fd = open(path, O_CREAT|O_WRONLY, 0666);
                    vector<tuple<int,int,int>> persist_info; 
                    persist_info.reserve((split_fault->persist).size());

                    size_t size_to_persist;
                    off_t off_to_persist;
                    int buf_i;

                    if (split_fault->parts == -1) {
                        for (auto & p : split_fault->persist) { //not equal parts
                            int sum = 0;
                            int sum_until_persist = 0;
                            buf_i = 0;
                        
                            for (int i = 0; i< split_fault->parts_bytes.size(); i++) {
                                sum += split_fault->parts_bytes[i];
                                if (i < p - 1)
                                    buf_i += split_fault->parts_bytes[i];
                            }

                            if (sum!=size) {
                                spdlog::warn ("[lazyfs.faults]: Could not inject fault of split write for file {} because sum of parts is different than the size of write!",path_s);
                                return res;
                            } else {
                                size_to_persist = split_fault->parts_bytes[p - 1];
                                off_to_persist = offset + buf_i;
                            }

                            tuple <int,int,int> info_part (buf_i,size_to_persist,off_to_persist);
                            persist_info.push_back(info_part);
                        }
                        

                    } else { //equal parts
                        size_to_persist = size/split_fault->parts;
                        size_t extra_size_to_persist = size%split_fault->parts;
                        int p_i = 0;
                        for (int p_i=0; p_i < split_fault->persist.size(); p_i++) {
                            auto & p = split_fault->persist.at(p_i);
                            buf_i = (p - 1) * size_to_persist;
                            size_t final_size_to_persist = size_to_persist;

                            off_to_persist = offset + (p - 1) * size_to_persist;
                            if (extra_size_to_persist>0 && p_i == split_fault->persist.size()-1) {
                                spdlog::info("[lazyfs.warning]: The value of 'parts' specified for the split write fault does not evenly divide the size of the write to split. We recommend changing this value, since to the last part will be added the extra bytes.");
                                final_size_to_persist += extra_size_to_persist;
                            }
                            
                            tuple <int,int,int> info_part (buf_i,final_size_to_persist,off_to_persist);
                            persist_info.push_back(info_part);
                         }
                    }

                    for (auto & persist : persist_info) {
                        int pw = pwrite(fd,buf+get<0>(persist),get<1>(persist),get<2>(persist));
                        if (pw==get<1>(persist)) spdlog::warn ("[lazyfs.faults]: Write to path {}: will persist {} bytes from offset {}",path,get<1>(persist),get<2>(persist));      
                        else {
                            spdlog::warn("[lazyfs.faults]: There was a problem spliting the {} write for the file {} (pwrite failed).",split_fault->occurrence,path);
                            return res;
                        } 
                    }

                    this_ ()->add_crash_fault ("after", "write", path_s, "none");            
                    spdlog::critical ("[lazyfs.faults]: Added crash fault ");  
                    res = true;        
                }
                break; //At the moment only one fault per file is accepted.
            }
        }
    } 
    return res;
}


} // namespace lazyfs

#include <sys/inotify.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <unistd.h>

using namespace std;

namespace fs = std::filesystem;

#define MAX_EVENTS 1024 /*Max. number of events to process at one go*/
#define LEN_NAME 16 /*Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE (sizeof(struct inotify_event)) /*size of one event*/
#define EVENT_BUF_LEN (MAX_EVENTS * (EVENT_SIZE + LEN_NAME)) /*buffer to store the data of events */


// Todo:
// - copy folder with contents -> events for all contained files
// - delete folder -> events for all contained files


class RecursiveInotify {
public:
    enum class Event {
        FILE_CREATED,
        FILE_OPENED,
        FILE_MODIFIED,
        FILE_CLOSED,
        FILE_DELETED,
        DIR_CREATED,
        DIR_OPENED,
        DIR_MODIFIED,
        DIR_CLOSED,
        DIR_DELETED
    };

    struct EventInfo {
        Event type;
        std::filesystem::path path;
        std::filesystem::path file;
    };

private:
    int fd;
    unordered_map<int, fs::path> wd_path;
    unordered_map<string, int> path_wd;  // Reverse map for faster lookups
    vector<fs::path> roots;
    std::map<Event, std::function<void(const EventInfo &)>> callbacks;

    void add_watch(const fs::path& p, bool is_root = false) {
        uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF;
        if (is_root) mask |= IN_MOVE_SELF;

        int wd = inotify_add_watch(fd, p.c_str(), mask);
        if (wd == -1) {
            if (errno == ENOENT) return;
            cerr << "inotify_add_watch " << p << ": " << strerror(errno) << endl;
            // exit(EXIT_FAILURE);
            return;
        }

        string p_str = p.string();
        if (wd_path.count(wd) || path_wd.count(p_str)) {
            return;  // Already watched
        }

        #ifdef LINC_FILEWATCH_VERBOSE
        cout << "watching " << p_str << endl;
        #endif

        wd_path[wd] = p;
        path_wd[p_str] = wd;
    }

    void recursive_add(const fs::path& dir, bool is_root = false) {
        add_watch(dir, is_root);
        for (const auto& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
            if (entry.is_directory()) {
                recursive_add(entry.path());
            }
        }
    }

    void remove_watch(int wd) {
        if (wd_path.count(wd)) {
            string p_str = wd_path[wd].string();
            path_wd.erase(p_str);
            wd_path.erase(wd);
            inotify_rm_watch(fd, wd);
        }
    }

    void zap_subtree(const fs::path& dir) {
        string dir_str = dir.string() + "/";
        for (auto it = wd_path.begin(); it != wd_path.end(); ) {
            string p_str = it->second.string();
            if (p_str == dir.string() || p_str.find(dir_str) == 0) {
                path_wd.erase(p_str);
                inotify_rm_watch(fd, it->first);
                it = wd_path.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool is_root(const fs::path& p) {
        for (const auto& r : roots) {
            if (p.string() == r.string())
                return true;
        }
        return false;
    }

    void update_path(const fs::path& old_path, const fs::path& new_path) {
        string old_str = old_path.string();
        string new_str = new_path.string();

        auto it = path_wd.find(old_str);
        if (it != path_wd.end()) {
            int wd = it->second;
            path_wd.erase(it);
            wd_path[wd] = new_path;
            path_wd[new_str] = wd;
        }

        string old_prefix = old_str + "/";
        string new_prefix = new_str + "/";
        for (auto& pair : wd_path) {
            string ps = pair.second.string();
            if (ps.find(old_prefix) == 0) {
                string new_ps = new_prefix + ps.substr(old_prefix.length());
                path_wd.erase(ps);
                pair.second = new_ps;
                path_wd[new_ps] = pair.first;
            }
        }
    }

    void reinitialize() {
        close(fd);
        fd = inotify_init1(IN_NONBLOCK);
        if (fd == -1) {
            cerr << "inotify_init: " << strerror(errno) << endl;
            throw std::runtime_error("inotify_init failed");
        }
        wd_path.clear();
        path_wd.clear();
        vector<fs::path> new_roots;
        for (const auto& r : roots) {
            if (fs::exists(r)) {
                new_roots.push_back(r);
                recursive_add(r, true);
            }
        }
        roots = std::move(new_roots);
    }

    bool is_callback_registered(const Event &event) {
        return (callbacks.find(event) != callbacks.end());
    }

    bool is_subpath(const std::filesystem::path &path, const std::filesystem::path &base)
    {
        auto rel = fs::relative(path, base);
        return !rel.empty() && rel.native()[0] != '.';
    }

    void run_callback(const Event &event, const std::string &current_dir, const std::string &filename) {
        if (is_callback_registered(event)) {

            // get current root
            fs::path cur_file(filename);
            fs::path cur_dir(current_dir);
            fs::path cur_root(current_dir);
            for (const auto& r : roots) {
                if (is_subpath(cur_dir, r)) {
                    cur_root = r;
                    cur_file = fs::relative(cur_dir, cur_root) / cur_file;
                    break;
                }
            }

            callbacks[event](
                // EventInfo{event, fs::path(current_dir), fs::path(filename)}
                EventInfo{event, cur_root, cur_file}
            );
        }
    }

    size_t process_event(char* ptr, char* end) {
        inotify_event* ev = reinterpret_cast<inotify_event*>(ptr);
        size_t len = sizeof(inotify_event) + ev->len;

        if (ev->wd == -1) {
            if (ev->mask & IN_Q_OVERFLOW) {
                cout << "IN_Q_OVERFLOW: queue overflow - reinitializing watches" << endl;
                reinitialize();
            }
            return len;
        }

        auto it = wd_path.find(ev->wd);
        if (it == wd_path.end()) return len;  // Ignored or removed

        fs::path dir = it->second;
        string name(ev->name);
        fs::path full = dir / name;

        // cout << "Event (mask: 0x" << hex << ev->mask << ") on " << full << endl;

        if (ev->mask & IN_IGNORED) return len;

        if (ev->mask & IN_DELETE_SELF) {
            if (ev->mask & IN_ISDIR) {
                if (is_root(dir)) {
                    roots.erase(std::remove(roots.begin(), roots.end(), dir), roots.end());
                }
                remove_watch(ev->wd);
            } else
                run_callback(Event::FILE_DELETED, dir, ev->name);
            return len;
        }

        if (ev->mask & IN_MOVE_SELF) {
            if (ev->mask & IN_ISDIR) {
                cout << "IN_MOVE_SELF on root - reinitializing watches" << endl;
                reinitialize();
            } else
                run_callback(Event::FILE_MODIFIED, dir, ev->name);
            return len;
        }

        if (ev->mask & IN_DELETE) {
            if (ev->mask & IN_ISDIR) {
                if (is_root(full)) {
                    roots.erase(std::remove(roots.begin(), roots.end(), full), roots.end());
                }
                zap_subtree(full);
            } else
                run_callback(Event::FILE_DELETED, dir, ev->name);
            return len;
        }

        if (ev->mask & IN_MODIFY) {
            if ((ev->mask & IN_ISDIR) == 0)
                run_callback(Event::FILE_MODIFIED, dir, ev->name);
            return len;
        }

        if (ev->mask & IN_MOVED_FROM) {
            char* next_ptr = ptr + len;
            if ((ev->mask & IN_ISDIR) && next_ptr < end) {
                inotify_event* next_ev = reinterpret_cast<inotify_event*>(next_ptr);
                if ((next_ev->mask & IN_MOVED_TO) && next_ev->cookie == ev->cookie) {
                    // Paired move: rename or move within tree
                    fs::path old_full = dir / ev->name;
                    fs::path new_dir = wd_path[next_ev->wd];
                    fs::path new_full = new_dir / next_ev->name;
                    update_path(old_full, new_full);
                    return len + sizeof(inotify_event) + next_ev->len;
                }
            }
            // Unpaired MOVED_FROM: moved out of tree
            if (ev->mask & IN_ISDIR) {
                if (is_root(full)) {
                    roots.erase(std::remove(roots.begin(), roots.end(), full), roots.end());
                }
                zap_subtree(full);
            } else
                run_callback(Event::FILE_DELETED, dir, ev->name);
            return len;
        }

        if (ev->mask & IN_MOVED_TO) {
            if (ev->mask & IN_ISDIR) {
                recursive_add(full);
            } else
                run_callback(Event::FILE_CREATED, dir, ev->name);
            return len;
        }

        if (ev->mask & IN_CREATE) {
            if (ev->mask & IN_ISDIR) {
                recursive_add(full);
            } else
                run_callback(Event::FILE_CREATED, dir, ev->name);
            return len;
        }

        // Other events can be handled/printed as needed
        return len;
    }
    
public:
    RecursiveInotify() : fd(-1) {}

    ~RecursiveInotify() {}

    void start(const vector<string>& paths) {
        fd = inotify_init1(IN_NONBLOCK);
        if (fd == -1) {
            cerr << "inotify_init: " << strerror(errno) << endl;
            throw std::runtime_error("inotify_init failed");
        }

        for (const auto& p : paths) {
            fs::path path = fs::absolute(p);
            if (!fs::is_directory(path)) {
                cerr << "Not a directory: " << p << endl;
                continue;
            }
            roots.push_back(path);
            recursive_add(path, true);
        }
    }

    void stop() {
        wd_path.clear();
        path_wd.clear();
        roots.clear();
        callbacks.clear();

        close(fd);
        fd = -1;
    }

    void update() {
        vector<char> buf(EVENT_BUF_LEN);

        ssize_t num_read = read(fd, buf.data(), EVENT_BUF_LEN);
        if (num_read <= 0) {
            return;
        }

        char* ptr = buf.data();
        char* end = buf.data() + num_read;
        while (ptr < end) {
            size_t advance = process_event(ptr, end);
            ptr += advance;
        }
    }

    void on(const std::vector<Event> &events, const std::function<void(const EventInfo &)> &action) {
        for (auto &event : events) {
            callbacks[event] = action;
        }
    }
};
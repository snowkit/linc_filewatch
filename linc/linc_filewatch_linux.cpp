#include <hxcpp.h>

#include "./linc_filewatch.h"

// #include "./fswatch.hpp"
#include "./recursive_inotify.hpp"

#include <unistd.h>
#include <sys/inotify.h>
#include <map>
#include <errno.h>

namespace linc {

    namespace filewatch {

        // fswatch watcher;
        RecursiveInotify watcher;

        void platform_start() {
            watcher.on(
                { 
                    RecursiveInotify::Event::FILE_CREATED,
                    RecursiveInotify::Event::FILE_OPENED,
                    RecursiveInotify::Event::FILE_MODIFIED,
                    RecursiveInotify::Event::FILE_CLOSED,
                    RecursiveInotify::Event::FILE_DELETED,
                }, 
                [](auto &event) {
                    #ifdef LINC_FILEWATCH_VERBOSE
                        std::string ename = "unknown";
                        switch (event.type) {
                            case RecursiveInotify::Event::FILE_MODIFIED:
                                ename = "FILE_MODIFIED";
                                break;
                            case RecursiveInotify::Event::FILE_CREATED:
                                ename = "FILE_CREATED";
                                break;
                            case RecursiveInotify::Event::FILE_DELETED:
                                ename = "FILE_DELETED";
                                break;
                            case RecursiveInotify::Event::FILE_OPENED:
                                ename = "FILE_OPENED";
                                break;
                            case RecursiveInotify::Event::FILE_CLOSED:
                                ename = "FILE_CLOSED";
                                break;
                        }
                        std::cout << ename << " - " << event.path << " / " << event.file << std::endl;
                    #endif

                    FilewatchEventType _event_type = fe_unknown;

                    switch (event.type) {
                        case RecursiveInotify::Event::FILE_MODIFIED:
                            _event_type = fe_modify;
                            break;
                        case RecursiveInotify::Event::FILE_CREATED:
                            _event_type = fe_create;
                            break;
                        case RecursiveInotify::Event::FILE_DELETED:
                            _event_type = fe_remove;
                            break;
                    }

                    if(_event_type != fe_unknown && user_callback != null()) {
                        user_callback(
                            _event_type, 
                            ::String(event.path.c_str()), 
                            ::String(event.file.c_str())
                        );
                    }
                }
            );

            watcher.start(watched_paths);
        }

        void platform_stop() {
            watcher.stop();
        }

        bool platform_init() {
            return true;
        }

        void platform_shutdown() {}

        void platform_update() {
            watcher.update();
        }

    } //filewatch namespace

} //linc namespace
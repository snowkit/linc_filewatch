#include <hxcpp.h>

#include "./linc_filewatch.h"

#include <unistd.h>
#include <sys/inotify.h>
#include <map>
#include <errno.h>

namespace linc {

    namespace filewatch {

        #define INOTIFY_EVENT_SIZE (sizeof (struct inotify_event))
        #define INOTIFY_BUFFER_SIZE (1024 * (INOTIFY_EVENT_SIZE + 16))

        static std::map<int, std::string> watch_to_path;
        static std::map<std::string, int> path_to_watch;
        static int inotifyfd = -1;


                void platform_start() {

                    if(watch_to_path.size() > 0) {
                        printf("/ filewatch / cannot start filewatch twice, this is less than ideal");
                        return;
                    }

                    if(watched_paths.size() == 0) {
                        printf("/ filewatch / not starting filewatch, no paths in list");
                        return;
                    }

                    #ifndef SNOW_NO_INOTIFY

                        //for each of the given paths we need to store
                        //a watch descriptor by path for removal later
                        //as well as to map which path is notifying us
                        //in the callback, when it happens

                            //watch for creation, deletion, modification but only on added directories (files will notify from within those)
                        uint32_t flags = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE | IN_ONLYDIR;

                        std::vector<std::string>::iterator it = watched_paths.begin();

                        for( ; it != watched_paths.end(); ++it) {
                            std::string path = (*it);
                            int wd = inotify_add_watch( inotifyfd, path.c_str(), flags );
                            path_to_watch[path] = wd;
                            watch_to_path[wd] = path;
                        }

                    #endif //SNOW_NO_INOTIFY

                    // printf("/ filewatch / started");

                } //platform_start

                void platform_stop() {

                    #ifndef SNOW_NO_INOTIFY

                        if(path_to_watch.size() > 0) {

                            std::map<std::string, int>::iterator it = path_to_watch.begin();

                            for( ; it != path_to_watch.end(); ++it) {
                                inotify_rm_watch( inotifyfd, it->second );
                            }

                            path_to_watch.clear();
                            watch_to_path.clear();

                        } //watches > 0

                    #endif //SNOW_NO_INOTIFY

                } //platform_stop

            //internal implementation details

                bool platform_init() {

                    #ifndef SNOW_NO_INOTIFY

                        inotifyfd = inotify_init1( IN_NONBLOCK );

                        if(inotifyfd < 0) {

                            std::string reason;

                            switch(inotifyfd) {
                                case EINVAL: {
                                    reason = "An invalid value was specified in flags";
                                    break;
                                }

                                case EMFILE: {
                                    reason = "The user limit on the total number of inotify instances has been reached.";
                                    break;
                                }

                                case ENFILE: {
                                    reason = "The system limit on the total number of file descriptors has been reached.";
                                    break;
                                }

                                case ENOMEM: {
                                    reason = "Insufficient kernel memory is available.";
                                    break;
                                }
                            }

                            // printf("/ filewatch / could not initialize inotify file watch : %s\n", reason.c_str());

                            return false;

                        }

                    #endif //SNOW_NO_INOTIFY

                    // printf("/ filewatch / initialized file watch ok\n");

                    return true;

                } //platform_init

                void platform_shutdown() {

                    #ifndef SNOW_NO_INOTIFY

                        if(inotifyfd > -1) {
                            ::close( inotifyfd );
                        }

                    #endif //SNOW_NO_INOTIFY

                } //platform_shutdown

                void platform_update() {

                    #ifndef SNOW_NO_INOTIFY

                        char buffer[INOTIFY_BUFFER_SIZE];
                        int length = 0;
                        int pos = 0;

                        length = ::read( inotifyfd, buffer, INOTIFY_BUFFER_SIZE );

                        if(length > 0) {

                            while (pos < length) {

                                struct inotify_event *event;

                                event = (struct inotify_event *) &buffer[pos];

                                //only handle files from here
                                if( !(event->mask & IN_ISDIR) ) {

                                    FilewatchEventType _event_type = fe_unknown;

                                    if(event->len) {

                                        std::string path( event->name );
                                        std::string root("");

                                        if(watch_to_path.find(event->wd) != watch_to_path.end()) {
                                            root = watch_to_path[event->wd];
                                        }

                                        if(event->mask & IN_MODIFY) {
                                            _event_type = fe_modify;
                                        } else if((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) {
                                            _event_type = fe_remove;
                                        } else if((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {
                                            _event_type = fe_create;
                                        }

                                        if(_event_type != fe_unknown && user_callback != null()) {
                                            std::string final = root + "/" + path;
                                            user_callback(_event_type, ::String(final.c_str()), ::String(path.c_str()));
                                        }

                                    } //event->len

                                } //is not a directory

                                pos += INOTIFY_EVENT_SIZE + event->len;

                            } //while pos < length

                        } //length > 0

                    #endif //SNOW_NO_INOTIFY

                } //platform_update

    } //filewatch namespace

} //linc namespace
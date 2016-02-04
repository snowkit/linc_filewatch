#include "./linc_filewatch.h"

#include <hxcpp.h>
#include <unistd.h>

#include <CoreServices/CoreServices.h>

namespace linc {

    namespace filewatch {

        //internal

            static FSEventStreamRef watcher;
            static float latency = 1.0f;
            static void filewatch_callback(
                ConstFSEventStreamRef streamRef,
                void *clientCallBackInfo,
                size_t numEvents,
                void *eventPaths,
                const FSEventStreamEventFlags eventFlags[],
                const FSEventStreamEventId eventIds[]
            ); //forward declare

            //platform implementation details

                void platform_start() {

                    //don't attempt to start twice
                    if(watcher) {
                        // printf("/ filewatch / cannot start filewatch twice, this is less than ideal\n");
                        return;
                    }

                    int _count = watched_paths.size();
                    if(_count == 0) {
                        // printf("/ filewatch / not starting filewatch, no paths in list\n");
                        return;
                    } else {

                        // printf("/ filewatch / start with paths:\n");
                        // for(int i = 0; i < _count; ++i) { printf("    > %s\n", watched_paths[i].c_str()); }

                    }

                        //make a list that CFArray can use
                    std::vector<CFStringRef> list;
                    for(std::vector<std::string>::iterator it = watched_paths.begin(); it != watched_paths.end(); ++it) {
                        list.push_back(CFStringCreateWithCString(NULL, (*it).c_str(), kCFStringEncodingUTF8));
                    }

                    CFArrayRef path_list = CFArrayCreate(NULL, (const void **)&list[0], list.size(), &kCFTypeArrayCallBacks);

                    watcher = FSEventStreamCreate(NULL, &filewatch_callback, new FSEventStreamContext(), path_list, kFSEventStreamEventIdSinceNow, latency,
                        kFSEventStreamEventFlagItemCreated  | //item created
                        kFSEventStreamEventFlagItemRemoved  | //item removed (only through rm-like remove it seems)
                        kFSEventStreamEventFlagItemRenamed  | //item renamed (also applies to move/move to trash)
                        kFSEventStreamEventFlagItemModified | //item modified
                        kFSEventStreamCreateFlagFileEvents    //listen for file change events, as well
                    );

                    if(!watcher) {
                        // printf("/ filewatch / failed to start\n");
                        return;
                    }

                    FSEventStreamScheduleWithRunLoop(watcher, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
                    FSEventStreamStart(watcher);

                    // printf("/ filewatch / started\n");

                } //platform_start

                void platform_stop() {

                    if( watcher ) {
                        FSEventStreamUnscheduleFromRunLoop( watcher, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
                        FSEventStreamStop( watcher );
                        FSEventStreamInvalidate( watcher );
                        FSEventStreamRelease( watcher );
                    }

                    watcher = NULL;

                    // printf("/ filewatch / stopped\n");

                } //platform_stop

                void platform_update() {

                } //platform_update

                static bool path_exists( const std::string &path ) {

                    // printf("/ filewatch / path_exists / %s\n", path.c_str());

                    int res = access(path.c_str(), R_OK);
                    if (res < 0) {
                        if (errno == ENOENT) {
                            return false;
                        }
                    }

                    return true;

                } //path_exists

                static void filewatch_callback(
                    ConstFSEventStreamRef streamRef,
                    void *clientCallBackInfo,
                    size_t numEvents,
                    void *eventPaths,
                    const FSEventStreamEventFlags eventFlags[],
                    const FSEventStreamEventId eventIds[]
                ) {

                    for(size_t i = 0; i < numEvents; ++i) {

                        const char * _path = ((char **) eventPaths)[i];
                        std::string path( _path );

                        FSEventStreamEventFlags flag = eventFlags[i];

                            //only listen for files
                        if(flag & kFSEventStreamEventFlagItemIsFile) {

                            FilewatchEventType _event_type = fe_unknown;

                            // printf("/ filewatch / event on path %s id %#010x\n", path.c_str(), eventFlags[i]);

                                //if it was modified but not renamed, we can fire a modify ok
                            if(  (flag & kFSEventStreamEventFlagItemModified) &&
                                !(flag & kFSEventStreamEventFlagItemRenamed) &&
                                !(flag & kFSEventStreamEventFlagItemCreated)
                              ) {

                                // printf("/ filewatch /     modify on path %s\n", path.c_str());

                                _event_type = fe_modify;

                                //if it was removed entirely (using rm or similar, it seems) this is ok, flag it
                            } else if( flag & kFSEventStreamEventFlagItemRemoved ) {

                                // printf("/ filewatch /     remove on path %s\n", path.c_str());

                                _event_type = fe_remove;

                            } else if( flag & kFSEventStreamEventFlagItemCreated ) {

                                // printf("/ filewatch /     create on path %s\n", path.c_str());

                                _event_type = fe_create;

                            } else {

                                    //renamed is really muddy on FSEvents, it's triggered for moves, deletes, creates, all kinds.

                                if(flag & kFSEventStreamEventFlagItemRenamed) {

                                    bool exists = path_exists(path);

                                        //if it was renamed, but the given path does not exist, this counts to remove
                                        //and the counter, if it was renamed but does exist, this constitutes a create
                                    if(!exists) {
                                        _event_type = fe_remove;
                                    } else {
                                        _event_type = fe_create;
                                    }

                                } else { //renamed

                                    // printf("/ filewatch / not watching event on path %s id %#010x\n", path.c_str(), eventFlags[i]);

                                }

                            } //else

                            if(_event_type != fe_unknown && user_callback != null()) {
                                user_callback(_event_type, ::String(path.c_str()));
                            }

                        } else {

                            // printf("/ filewatch / not watching event on path %s id %#010x\n", path.c_str(), eventFlags[i]);

                        }

                    } //each event

                } //filewatch_callback

            //internal implementation details

                bool platform_init() {

                    return true;

                } //platform_init

                void platform_shutdown() {

                } //platform_shutdown


    } //filewatch namespace

} //linc
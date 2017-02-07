#pragma once
    
#include <hxcpp.h>
#include <string>
#include <vector>
#include <algorithm> //std::find

namespace linc {

    namespace filewatch {

        //types

            enum FilewatchEventType {
                fe_unknown                      = 0,
                fe_modify                       = 1,
                fe_remove                       = 2,
                fe_create                       = 3
            }; //FilewatchEventType
            
            #if (HXCPP_API_LEVEL>=330)
                typedef void LincFilewatchVoid;
            #else
                typedef Void LincFilewatchVoid;
            #endif

            typedef ::cpp::Function < LincFilewatchVoid (int, ::String) > InternalFilewatchFN;

        //internal

            extern std::vector<std::string> watched_paths;
            extern InternalFilewatchFN user_callback;

            extern void platform_start();
            extern void platform_stop();
            extern bool platform_init();
            extern void platform_shutdown();
            extern void platform_update();

        //user facing

            inline bool init(InternalFilewatchFN _callback) {

                user_callback = _callback;
                return platform_init();

            } //init

            inline void shutdown() {

                platform_stop();
                platform_shutdown();

            } //shutdown

            inline void refresh() {

                platform_stop();
                platform_start();

            } //refresh

            inline void update() {

                platform_update();

            } //update

            inline void add_watch(const char* _path) {

                // printf("/ filewatch / add path %s\n", _path);

                watched_paths.push_back( std::string(_path) );
                refresh();

            } //add_watch

            inline bool remove_watch(const char* _path) {

                std::vector<std::string>::iterator found = std::find(watched_paths.begin(), watched_paths.end(), std::string(_path));
                if(found != watched_paths.end()) {

                    watched_paths.erase( found );
                    refresh();

                    // printf("/ filewatch / remove path found! %s\n", _path);

                    return true;

                } // found

                // printf("/ filewatch / remove path not found! %s\n", _path);

                return false;

            } //remove_watch

    } //filewatch namespace

} //linc

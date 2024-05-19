#pragma once
    
#ifndef HXCPP_H
#include <hxcpp.h>
#endif
#include <FileWatch.hpp>
#include <string>

namespace linc {

    namespace filewatch {       

        enum FilewatchEventType {
            fe_unknown                      = 0,
            fe_modify                       = 1,
            fe_remove                       = 2,
            fe_create                       = 3,
            fe_renamed_old                  = 4,
            fe_renamed_new                  = 5
        };

        #if (HXCPP_API_LEVEL>=330)
            typedef void LincFilewatchVoid;
        #else
            typedef Void LincFilewatchVoid;
        #endif

        typedef ::cpp::Function < LincFilewatchVoid (int, ::String, ::String) > InternalFilewatchFN;

        extern std::unordered_map<std::wstring, ::filewatch::FileWatch<std::wstring>*> watched_paths;
        extern InternalFilewatchFN user_callback;

        inline void init(InternalFilewatchFN _callback) {
            user_callback = _callback;
        }

        inline void shutdown() {
            for (const auto& [key, value] : watched_paths)
                delete value;
            watched_paths.clear();
        }

        inline void add_watch(const char* _path) {
            std::wstring wpath(_path, _path+strlen(_path));

            ::filewatch::FileWatch<std::wstring> *watch = new ::filewatch::FileWatch<std::wstring>(
                wpath, 
                [=](const std::wstring& path, const ::filewatch::Event change_type) {
                    int base = 99;
                    hx::SetTopOfStack(&base,true);

                    // std::string c_path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes( path );
                    // std::wcout << wpath << L" : ";
                    switch (change_type)
                    {
                    case ::filewatch::Event::added:
                        // std::cout << "The file was added to the directory." << '\n';
                        user_callback(FilewatchEventType::fe_create, ::String(wpath.c_str()), ::String(path.c_str()));
                        break;
                    case ::filewatch::Event::removed:
                        // std::cout << "The file was removed from the directory." << '\n';
                        user_callback(FilewatchEventType::fe_remove, ::String(wpath.c_str()), ::String(path.c_str()));
                        break;
                    case ::filewatch::Event::modified:
                        // std::cout << "The file was modified. This can be a change in the time stamp or attributes." << '\n';
                        user_callback(FilewatchEventType::fe_modify, ::String(wpath.c_str()), ::String(path.c_str()));
                        break;
                    case ::filewatch::Event::renamed_old:
                        // std::cout << "The file was renamed and this is the old name." << '\n';
                        user_callback(FilewatchEventType::fe_renamed_old, ::String(wpath.c_str()), ::String(path.c_str()));
                        break;
                    case ::filewatch::Event::renamed_new:
                        // std::cout << "The file was renamed and this is the new name." << '\n';
                        user_callback(FilewatchEventType::fe_renamed_new, ::String(wpath.c_str()), ::String(path.c_str()));
                        break;
                    };

                    hx::SetTopOfStack((int*)0,true);
                }
            );
            watched_paths[wpath] = watch;
        }

        inline bool remove_watch(const char* _path) {
            std::wstring wpath(_path, _path+strlen(_path));
            if (auto search = watched_paths.find(wpath); search != watched_paths.end()) {
                delete search->second;
                watched_paths.erase(search);
                return true;
            }
            return false;
        }
    }
}

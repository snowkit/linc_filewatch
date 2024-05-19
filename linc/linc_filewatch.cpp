#include <hxcpp.h>

#include "./linc_filewatch.h"

namespace linc {
    namespace filewatch {
        //common implementation
        std::unordered_map<std::wstring, ::filewatch::FileWatch<std::wstring>*> watched_paths;
        InternalFilewatchFN user_callback;
    }
}
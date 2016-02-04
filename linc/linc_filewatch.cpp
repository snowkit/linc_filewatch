#include "./linc_filewatch.h"

namespace linc {

    namespace filewatch {

        //common implementation
        std::vector<std::string> watched_paths;
        InternalFilewatchFN user_callback;

    } //filewatch

} //linc
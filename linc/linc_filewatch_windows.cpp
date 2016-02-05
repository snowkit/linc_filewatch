#include "./linc_filewatch.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "./snow_event_queue.h"

namespace linc {

    namespace filewatch {

        class FileWatcherThread;

        static snow::io::eventqueue_t filewatch_queue;
        static std::vector<FileWatcherThread*> watchers;

        //internal implementation

            class FileWatcherThread {

              public:
                bool running;
                std::string path;
                HANDLE handle;
                HANDLE thread;
                OVERLAPPED overlap;
                BYTE* buffer;
                size_t buffer_size;
                CRITICAL_SECTION critical;

                static DWORD WINAPI run_thread( void *watcher );

                FileWatcherThread(const std::string &_path) {

                    running = false;
                    path = std::string(_path);
                    buffer_size = 1024;
                    buffer = new BYTE[buffer_size];

                        //I spitballed this 2048 spin count from reading some posts online
                    int _spincount = 2048;
                    #ifndef DEBUG
                        ::InitializeCriticalSectionEx(&critical, _spincount, CRITICAL_SECTION_NO_DEBUG_INFO);
                    #else
                        ::InitializeCriticalSectionEx(&critical, _spincount, 0);
                    #endif

                    handle = ::CreateFile(
                        path.c_str(),
                        FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                        NULL
                    );

                    if(handle != INVALID_HANDLE_VALUE) {

                        ZeroMemory(&overlap, sizeof(overlap));
                        overlap.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

                        DWORD threadid;
                        running = true;
                        thread = CreateThread(NULL, 0, &run_thread, this, 0, &threadid);

                    } //!invalid handle

                } //construct

                ~FileWatcherThread() {

                    running = false;

                    ::LeaveCriticalSection(&critical);
                    ::DeleteCriticalSection(&critical);

                    ::CancelIo(handle);
                    ::CloseHandle(handle);
                    ::CloseHandle(thread);

                } //~

            }; //class FileWatcherThread


            DWORD WINAPI FileWatcherThread::run_thread( void *_watcher ) {

                FileWatcherThread *watcher = (FileWatcherThread*)_watcher;


                while(watcher->running) {

                    ReadDirectoryChangesW(
                        watcher->handle, watcher->buffer, watcher->buffer_size, TRUE,
                        FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                        NULL, &watcher->overlap, NULL
                    );

                    WaitForSingleObject(watcher->overlap.hEvent, INFINITE);

                        int seek = 0;

                        while(seek < watcher->buffer_size) {

                            PFILE_NOTIFY_INFORMATION notifier = PFILE_NOTIFY_INFORMATION(watcher->buffer + seek);

                            WCHAR szwFileName[MAX_PATH];
                            int ulCount = notifier->FileNameLength/2;
                            wcsncpy(szwFileName, notifier->FileName, ulCount);
                            szwFileName[ulCount] = L'\0';

                            std::wstring widepath(szwFileName);
                            std::string path(widepath.begin(), widepath.end());

                            FilewatchEventType _event_type = fe_unknown;

                                switch( notifier->Action ) {

                                    case FILE_ACTION_ADDED:{
                                        _event_type = fe_create;
                                        break;
                                    }

                                    case FILE_ACTION_REMOVED:{
                                        _event_type = fe_remove;
                                        break;
                                    }

                                    case FILE_ACTION_MODIFIED:{
                                        _event_type = fe_modify;
                                        break;
                                    }

                                    case FILE_ACTION_RENAMED_OLD_NAME:{
                                        _event_type = fe_remove;
                                        break;
                                    }

                                    case FILE_ACTION_RENAMED_NEW_NAME:{
                                        _event_type = fe_create;
                                        break;
                                    }

                                } //switch

                                if(_event_type != fe_unknown) {

                                    // put into queue
                                    snow::io::event_node_t* node = new snow::io::event_node_t;
                                    node->path = std::string(watcher->path+"/"+path);
                                    node->event_type = (int)_event_type;

                                    snow::io::eventqueue_push(&filewatch_queue, node);

                                }

                            seek += notifier->NextEntryOffset;

                            if(notifier->NextEntryOffset == 0) {
                                break;
                            }

                        } //seek < buffer size

                    } //while running

                return 0;

            } //run_thread

        // platform implementation details

            void platform_start() {

                if(watched_paths.size() == 0) {
                    return;
                }

                std::vector<std::string>::iterator it = watched_paths.begin();

                for( ; it != watched_paths.end(); ++it) {

                    std::string path = *it;

                    FileWatcherThread* watcher = new FileWatcherThread(path);
                    watchers.push_back(watcher);

                } //each watched path

                // printf("/ filewatch / filewatch started");

            } //platform_start

            void platform_stop() {

                if(watchers.size() == 0) {
                    return;
                }

                std::vector<FileWatcherThread*>::iterator it = watchers.begin();

                for( ; it != watchers.end(); ++it) {
                    FileWatcherThread* watcher = *it;
                    if(watcher) {
                        delete watcher;
                        watcher = 0;
                    }
                }

                watchers.clear();

                // printf("/ filewatch / stopped");

            } //platform_stop

            bool platform_init() {

                // printf("/ filewatch / initialized file watch ok");

                snow::io::eventqueue_create((snow::io::eventqueue_t*)&filewatch_queue);

                return true;

            } //platform_init

            void platform_shutdown() {

            } //platform_shutdown

            void platform_update() {

                snow::io::event_node_t* node;
                while( node = snow::io::eventqueue_pop(&filewatch_queue) ) {

                    FilewatchEventType _event_type = (FilewatchEventType)(node->event_type);

                    if(user_callback != null()) {
                        user_callback(_event_type, ::String(node->path.c_str()));
                    }

                    delete node;

                } //

            } //platform_update


    } //filewatch namespace

} //linc namespace
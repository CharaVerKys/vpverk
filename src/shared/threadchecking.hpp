#pragma once
#include "cussert.hpp"
#include <thread>
#include <cstdarg>
#include <vector>

inline std::thread::id mainThreadID;
#ifdef IT_IS_SERVER
    // main context can use several threads, main thread not included in (potential) main context thread pool
inline std::thread::id settingsThreadID;
inline std::thread::id statsThreadID;
#endif

// unlike qt here usings objects, not void*
// but uses like '&threadID' because va_list not supports non-pod args (thread::id is not)

#ifndef NDEBUG

inline void checkAllThreadIDsCollision() {
    std::vector<std::thread::id> threadIDs = {
 mainThreadID,
#ifdef IT_IS_SERVER
 settingsThreadID,
 statsThreadID
#endif
    };

    for (size_t i = 0; i < threadIDs.size(); ++i) {
        for (size_t j = i + 1; j < threadIDs.size(); ++j) {
            if (threadIDs[j] not_eq std::thread::id()) {
                cussert(threadIDs[i] != threadIDs[j] && "Thread ID collision");
            }
        }
    }
}

#define NOTmainThread cussert(std::this_thread::get_id() not_eq mainThreadID);

inline void checkThread(std::thread::id* thread)
{
    if (*thread == std::thread::id()) {
        *thread = std::this_thread::get_id();
    } else {
        cussert(*thread == std::this_thread::get_id());
    }
    checkAllThreadIDsCollision();
}

inline void checkThreadIsNOT(const std::thread::id* thread)
{
    cussert(*thread not_eq std::this_thread::get_id()); 
}

inline void checkThread(int count...)
{
    va_list args;
    cussert(count>=1);
    uint8_t allCount = (uint8_t)count;
    va_start(args, count); // warning by clang: UB if count have type uint8_t or uint16_t (1/2 bytes), Ok if uint // idk why
    bool was_eq = false;
    while(count){
        const std::thread::id *const id = va_arg(args, std::thread::id*);
        if(allCount == 1 and std::this_thread::get_id() not_eq *id){cussert(false);}
        if(not id){
            va_end(args);
            return;
        }
        if(*id == std::this_thread::get_id()){
            was_eq = true;
        }
        --count;
    }//while
    va_end(args);
    cussert(count == 0);
    cussert(was_eq);
}

#else
#define NOTmainThread (void)0; 
inline void checkThread(const std::thread::id* thread){(void)thread;}
inline void checkThreadIsNOT(const std::thread::id* thread){(void)thread;}
inline void checkThread(int count...){(void)count;}
#endif

#include "cussert.hpp"

#include <iostream>
#include <cstring>
#include <string>

[[noreturn]] void cuabort(const char* info){
    std::cerr << "called (cu)abort!\n";
    if(info){
        std::cerr << "info: " << info <<'\n';
    }
    volatile int64_t* nullptr_ = 0;
    *nullptr_ = 0; 
    std::cout << "this platform allow you to dereference and write to null pointer or your compiler optimize out volatile pointer too aggressively, that should not be valid in c++";
    std::set_terminate(reinterpret_cast<void(*)()>(*nullptr_)); // read from variable to prevent potential optimizations
    std::terminate();
}
[[noreturn]] void cuabort_loc(const char* info, std::source_location const& loc){
    std::cerr << "called (cu)abort!\n";
    std::cerr << "on location: " << loc.file_name() << " | " << loc.function_name()<<'\n';
    if(info){
        std::cerr << "info: " << info <<'\n';
    }
    volatile int64_t* nullptr_ = 0;
    *nullptr_ = 0; 
    std::cout << "this platform allow you to dereference and write to null pointer or your compiler optimize out volatile pointer too aggressively, that should not be valid in c++";
    std::set_terminate(reinterpret_cast<void(*)()>(*nullptr_)); // read from variable to prevent potential optimizations
    std::terminate();
}

[[noreturn]] void ___cuassertImplementation(const char* x, std::source_location const& loc){
    std::string info = "cussert failed:\n\tlocation: ";          
    info.append(loc.file_name());   
    info.append("\n\tfunction: ");                              
    info.append(loc.function_name());
    if(std::strcmp("false",x) == 0){ 
        info.append("\n\t(unreachable code reached)"); 
    }else{    
        info.append("\n\texpression: ");                              
        info.append(x);                              
    }       
    cuabort(info.c_str());
}

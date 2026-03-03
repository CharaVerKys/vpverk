#include "write"
#include "logger.hpp"

cvk::write::write(const log::to& target,
    const std::source_location loc)
    : target(target){
    std::string file = loc.file_name();
    
    context << '#' << ++totalOrder << ' ' << Logger::private_getStringFromCurrentTime(true) << " in func "<< loc.function_name() 
        << " in file " << file.substr(file.find_last_of('/')+1) << " -> \n\t";
}

cvk::write::~write(){
    std::string toPush;
    switch(lvl){
        case log::lvl::debug:{
            toPush = "\033[36m";
        }break;
        case log::lvl::good:{
            toPush = "\033[32m";
        }break;
        case log::lvl::norm:{
            toPush = "\033[0m";
        }break;
        case log::lvl::error:{
            toPush = "\033[33m";
        }break;
        case log::lvl::critical:{
            toPush = "\033[31m";
        }break;
    }
    Logger::instance()->private_push(target, toPush + context.str() + resultMsg.str() +'\n' );
}

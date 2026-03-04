#pragma once

#include <log/write>
using write_ = cvk::write;

inline cvk::write write_serv(){
    return cvk::write(clt::server);
}

#pragma once

#include "cussert.hpp"
#include <log/write>
using write_ = cvk::write;

inline cvk::write write_serv(){
#ifdef IT_IS_CLIENT
    cuabort("call write serv on client");
#endif
    return cvk::write(clt::server);
}

inline cvk::write write_clnt(){
#ifdef IT_IS_SERVER
    cuabort("call write clnt on server");
#endif
    return cvk::write(clt::client);
}

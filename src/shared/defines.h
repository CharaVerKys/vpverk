#pragma once

#include <log/write>
using write_ = cvk::write;

#ifdef IT_IS_SERVER
#define write_serv() cvk::write(clt::server)
#endif

#ifdef IT_IS_CLIENT
#define write_clnt() cvk::write(clt::client)
#endif

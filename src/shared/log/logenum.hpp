#pragma once
namespace cvk::log{
    enum class to{
        server = 0,
        client = 1,
        shared = CONFIG_SERVER_OR_CLIENT_LOG,
        session = 2
    };
    enum class lvl{
        debug,
        good,
        norm,
        error,
        critical
    };
}
// ? if conflict, comment this and define yourself, or use full namespace
using clt = cvk::log::to;
using cll = cvk::log::lvl;

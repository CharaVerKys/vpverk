set(SHARED_SOURCES
    src/shared/log/write.cpp
    src/shared/cussert.cpp
    src/shared/tun.cpp
    src/shared/asioapi.cpp
)

set(SERVER_SOURCES
    src/server/main.cpp
    src/server/connection.cpp
    src/server/contexts/settings.cpp
    src/server/contexts/statistics.cpp
    src/server/contexts/sessionscontrol.cpp
)

set(CLIENT_SOURCES
    src/client/main.cpp
    src/client/async.cpp
)

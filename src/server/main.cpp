// just traits for easier implementation:
// - sessions are not allowed to live longer than 24h, will be closed and reconnected again

// note: for server side it may be just http api, so i may create vue.js version for it, but client i plan only dbus, cuz i never used it and want to try it (learn to use api)

// using method-invoke framework with only 3 contexts:
// 1. main context (may be thread pool, natively need just to be one io_context)
//   (co_awaitable)  method::move_to_main_context -- just resume current coroutine on main context
//   // no other methods actually needed, this one only needed to return from other contexts, main context is session and lifetime control and all sessions
//   // and later dbus interface for connecting from ui, if i will add ui
// 2. DB context:
//   (co_awaitable) method::write_session_data(sessionData) // will fire every second to update DB context itself, will flush to db (new data only) every minute
//   // and later async acquire stats if i will add ui
// 3. settings context:
//   currently settings as dedicated thread will be redundant, but it will do swap only to acquire settings once per session start, so its fine
//   (co_awaitable) method::get_settings -> SettingsSnapshot // settings snapshot implement atomic counter inside, so settings class will know where to destroy it safely // access will be something settings->, so looks like ptr via operator->()
//   // and later async change and acquire of settings if i will add ui

// to implement main context + thread pool, i first start one asio::io_context::run() in context class/object lifetime, to satisfy method-invoke api, and than will start some_thread_pool::create(system_threads-2) // where 2 is amount of threads already being ran minus settins thread cuz it needed for easy concurrent access only, and main thread cuz it almost no-op (so it only already spawned one in main asio context and db(io) context)

// main thread will handle dbus (ui) one time... but generally it just wait for interrupt to occur (asio::signal_set::async_await)

// so main context spawner will be sessions controller, session controller should control one(many) Communication on their lifetime, spawn new Communication objects on incoming connection via acceptor, close old sessions when needed, and may be other additional control (filtering, kill session by choice, etc)
// currently plan this class only for controlling socket lifetime (not matter what implementation it uses)
// maybe later it will also support session tokens, and other stuff like this

// Communication (channel) is interface to actual implementation that should have api:
//  constructor(SettingsSnapshot) //nothing else needed in constructor actually
//  (co_await) close(reason) -> considered ending lifetime and might be safely deleted right after completion
//  std::optional<bool> is_active() -> return null if any pending coroutine (like database) - cannot be destructed; otherwise return whatever underline connection consider active
//      // is_active should been checked before deleting, after calling close, reschedule task if is_active return null
//  bool pending() -> same api for null/not null but with better name
//  this 2 api should be thread safe, tho it might never be actually called while communication object perform on other thread, just add them just in case
//  all communication objects are invoked only in infinite loop in sessions controller
//
//  milliseconds is_active() -> return null if any pending coroutine (like database) - cannot be destructed; otherwise return whatever underline connection consider active
//  tl::expected<Unit, some_error_code>(co_await) send(data) -> send some data 
//  tl::expected<std::vector<byte_t>, some_error_code> (co_await) read() -> read all data, idk when may be useful tho... 
//  tl::expected<uint32_t, some_error_code> (co_await) read_some(std::shared_ptr<std::span<char,size>>) -> return how many actually read, read max is size of span, pointer because references are explicitly not allowed in all coroutines (to prevent memory crashes at least) 

// allowed error codes to be considered later, but ideally with backward/replace compat between interfaces

// so,
// 1. init all contexts
// 2. connect first client and start new async_accept
// ~. everything flow

// 1. client created, new coroutine just spawned, first msg is RSA coded encryption key and user login
// 2. sessions control complete identification and verify access by login
// 3. continue all other communications in infinite loop, until stop token or send/read_some return error
//  // behavior on any error - close session

// seems like that everything i want for server side of vpn

#include <cvk.hpp>
#include <log/logger.hpp>
#include <log/write>
#include <misc/lyra.hpp>
#include <threadchecking.hpp>

#include <contexts/sessionscontrol.hpp>
#include <contexts/settings.hpp>
#include <contexts/statistics.hpp>

#include <defines.h>

int main(int argc, char** argv){
    checkThread(&mainThreadId);

    std::string logdir = "none";
    auto cli = lyra::cli()
        | lyra::opt(logdir)
        ["--logdir"]
        ("specify different log directory (default behavior log next to executable)");
    auto result = cli.parse({argc,argv});

    if(logdir == "none"){
        logdir = cvk::getExePath().parent_path();
    }

    std::filesystem::create_directories(logdir);
    Logger::instance()->setLogDir(logdir);
    Logger::instance()->init();
    cvk::write(clt::server) << cll::good << "log start";

    write_(clt::server) << "start contexts constructors";
    SessionsControl sessionsControl; // works with ptr
    Settings settings; // and objects itself
    Statistics statistics;

    settings.read("conf.json");

    cvk::ContStore* p_contextsStore = cvk::ContStore::instance();
    #define and_then_async(e) and_then(e)
    
    write_(clt::server) << "run contexts";
    p_contextsStore->registerContexts(&sessionsControl,&settings,&statistics) // ! in order of enums
    .and_then(call_(sessionsControl, onAsyncStart))
    .and_then(call_(settings, onAsyncStart))
    .and_then(call_(statistics, onAsyncStart))
        .or_else([](auto&&) -> std::invoke_result_t<decltype(&cvk::ContextsStore<0>::template registerContexts<cvk::Receiver>), cvk::ContextsStore<0>*, cvk::Receiver*>
        {
            write_serv() << "cannot set contexts to run";
            _Exit(1);
        })
    .and_then(call_(p_contextsStore,startContexts))
        .or_else([](std::exception_ptr&&)->expected_ue{
            write_serv() << "cannot run contexts";
            _Exit(1);
            throw std::runtime_error("failed to start contexts");
        })
    // * using coroutine traits to complite with async work
    .and_then_async(call_(sessionsControl,acceptConnection))
    //.value().subscribe([](tl::expected<Unit,std::exception_ptr>&&exc){
        //std::cout << "call async in context 2 after subscribe on future from chained expected startup" <<std::endl;
    //}
    //, *cvk::contexts::getContext(SecondContext), true) // ? do in context 2 with force async
    ;

    write_(clt::server) << "main wait on join";
    // note-reminder: to close application there contextsStore_requestStop
    p_contextsStore->wait(); //basically last lock, wait for all contexts to join
    return 0;
}

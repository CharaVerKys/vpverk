#include <cvk.hpp>
#include <log/logger.hpp>
#include <log/write>
#include <misc/lyra.hpp>
#include <threadchecking.hpp>

#include <contexts/sessionscontrol.hpp>
#include <contexts/settings.hpp>
#include <contexts/statistics.hpp>

#include <some_other_help_funcs.hpp>
#include <defines.h>

int main(int argc, char** argv){
    checkThread(&mainThreadID);

    std::string logdir = "none";
    auto cli = lyra::cli()
        | lyra::opt(logdir, "logdir")
        ["--logdir"]
        ("specify different log directory (default behavior log next to executable)");
    auto result = cli.parse({argc,argv});

    if(logdir == "none"){
        logdir = cvk::get_current_exec_path().parent_path();
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
    .and_then_async(call_(sessionsControl,startAcceptingConnections))
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

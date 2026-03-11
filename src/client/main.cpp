#include "other_coroutinethings.hpp"
#include <asio/io_context.hpp>
#include <asioapi.hpp>
#include <coroutinesthings.hpp>
#include <misc/lyra.hpp>
#include <some_other_help_funcs.hpp>
#include <log/logger.hpp>
#include <log/write>
#include <defines.h>
#include <async.hpp>

int main(int argc, const char** argv){

    /*
    parse arguments:
        server ip
        server port
        public.pem path
        login

    send to server int{256}
    and then crypted aes
    salt same as for server
    and crypted login
    receive 8 bytes ip
    setup ip for tun
    run two independent coroutines
     for tun->vpn
         vpn->tun
    

    also need func that read close reason or next packet size
    */

    //actually would be better to make everything async from the start

    cvk::args args;

    std::string logdir = "none";
    auto cli = lyra::cli()
        | lyra::opt(logdir, "logdir")
        ["--logdir"]
        ("specify different log directory (default behavior log next to executable)")
        | lyra::opt(args.server_ip, "server_ip")["--server_ip"]
        ("public server ip")
        | lyra::opt(args.port, "port")["--port"]
        ("public server port")
        | lyra::opt(args.login, "login")["--login"]
        ("your login")
        | lyra::opt(args.key_path, "key_path")["--key_path"]
        ("public.pam key path (defaul - next to exe)")
    ;
    auto result = cli.parse({argc,argv});
    if(result.has_value()){
        // i guess ok?     
        if(args.port == 0){
            std::cerr << "provide '--port=<int>;\n"; 
            return 1;
        }
        if(args.server_ip == "0"){
            std::cerr << "provide '--server_ip=\"<string>\";\n"; 
            return 1;
        }
        if(args.login == "0"){
            std::cerr << "provide '--login=\"<string>\";\n"; 
            return 1;
        }
        std::cout << "Ip: " << args.server_ip << " / port: " << args.port << " / login: " << args.login <<std::endl;
    }else{
        std::cerr << result.message() << std::endl;
        return 1;
    }

    if(logdir == "none"){
        logdir = cvk::get_current_exec_path().parent_path();
    }
    if(args.key_path == "0"){
        const auto path_ = cvk::get_current_exec_path().parent_path() / "public.pem";
        args.key_path = path_.string();
    }

    std::filesystem::create_directories(logdir);
    Logger::instance()->setLogDir(logdir);
    Logger::instance()->init();
    cvk::write(clt::client) << cll::good << "log start";
    
    //async in async.cpp
    asio::io_context ctx;
    t_ctx = &ctx;
    startAsync(&ctx,args).subscribe([](auto exp){
            if(not exp){
                try{
                std::rethrow_exception(exp.error());
                }catch(std::exception const& e){
                    cuabort((std::string{"caught exception: "} + e.what()).c_str());
                }
            }
            /*idk*/
        },ctx);
    ctx.run();
    
    write_(clt::client) << "main wait on join";
}

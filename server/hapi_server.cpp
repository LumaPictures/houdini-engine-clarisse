#include <HAPI/HAPI.h>

#include <iostream>
#include <string>
#include <signal.h>

int main(int argc, char** argv)
{
    std::cout << "[hapi_server] Launched thrift server." << std::endl;
    HAPI_ProcessId process_id;
    HAPI_ThriftServerOptions serverOptions = {false, 1000.0f};
    std::string serverName = "clarisse_pipe_thrift";
    if (argc > 1) {
        serverName += "_";
        serverName += argv[1];
    }
    HAPI_StartThriftNamedPipeServer(&serverOptions, serverName.c_str(), &process_id);

    std::string str;
    std::cin >> str;
    std::cout << "[hapi_server] Killing thrift server." << std::endl;
    kill(process_id, SIGKILL);
}

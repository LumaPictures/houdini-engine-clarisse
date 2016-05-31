#include <HAPI.h>

#include <iostream>
#include <string>
#include <signal.h>

int main(int argc, char** argv)
{
    std::cout << "[hapi_server] Launched thrift server." << std::endl;
    HAPI_ProcessId process_id;
    if (argc > 1)
        HAPI_StartThriftNamedPipeServer(true, (std::string("clarisse_pipe_thrift_") + std::string(argv[1])).c_str(), 5000, &process_id);
    else
        HAPI_StartThriftNamedPipeServer(true, "clarisse_pipe_thrift", 5000, &process_id);

    std::string str;
    std::cin >> str;
    std::cout << "[hapi_server] Killing thrift server." << std::endl;
    kill(process_id, SIGKILL);
}

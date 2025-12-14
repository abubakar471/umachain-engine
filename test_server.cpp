#include <iostream>
#include <string>
#include "httplib.h"
// #include "Blockchain.h"

// Blockchain blockchain;

int main()
{
    httplib::Server server;

    server.Get("/chain", [](const httplib::Request &, httplib::Response &res)
               {
    std::string output = "hello programmer!";
    res.set_content(output, "text/plain"); });

    std::cout << "Server running on http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);

    return 0;
}
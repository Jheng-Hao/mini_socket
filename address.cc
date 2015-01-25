#include <arpa/inet.h>
#include <cstdio>
#include <cerrno>
#include <iostream>
#include "address.h"

using namespace std;

namespace
{

const string ErrorHeader = "[Error (Address)] ";

}

namespace socket_namespace
{

Address::Address (const string &ip, Port port)
: valid_(false)
{
        int ret_val = inet_pton(AF_INET, ip.c_str(), &addr_v4_.sin_addr);

        if (1 == ret_val)
        {
                valid_ = true;
                ip_version_ = AF_INET;
                addr_v4_.sin_family = AF_INET;
                addr_v4_.sin_port = htons(port);
                return;
        }
        else if (ret_val < 0)
        {
                perror((ErrorHeader + "inet_pton").c_str());
                return;
        }

        ret_val = inet_pton(AF_INET6, ip.c_str(), &addr_v6_.sin6_addr);
        
        if (1 == ret_val)
        {
                valid_ = true;
                ip_version_ = AF_INET6;
                addr_v6_.sin6_family = AF_INET6;
                addr_v6_.sin6_port = htons(port);
                return;
        }
        else if (ret_val < 0)
        {
                perror((ErrorHeader + "inet_pton").c_str());
                return;
        }

        if (0 == ret_val)
        {
                cout << ErrorHeader << "incorrect format" << endl;
        }
        else
        {
                cout << ErrorHeader << "unhandled ret_val: " << ret_val << endl;
        }
}

}

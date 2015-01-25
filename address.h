#ifndef ADDRESS_H_
#define ADDRESS_H_

#include <netinet/in.h>
#include <iostream>

namespace socket_namespace
{

typedef unsigned short Port;

class Address
{
public:
        Address (const std::string &ip, Port port);

        inline bool IsValid () const { return valid_; }
        inline unsigned short GetIPVersion () const { return ip_version_; }

        inline void GetAddress (struct sockaddr_in &addr) const
        {
                addr = addr_v4_;
        }

        inline void GetAddress (struct sockaddr_in6 &addr) const
        {
                addr = addr_v6_;
        }

private:
        bool valid_;
        unsigned short ip_version_;
        struct sockaddr_in addr_v4_;
        struct sockaddr_in6 addr_v6_;
};

}

#endif

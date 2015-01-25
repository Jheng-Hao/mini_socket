#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <limits.h>
#include <iostream>
#include <sstream>
#include "sctp_socket.h"

using namespace std;

namespace
{

const string ErrorHeader = "[Error (SctpSocket)] ";

inline void PrintErrno ()
{
        cout << "Errno: " << errno << endl;
}

}

namespace socket_namespace
{

SctpSocket::SctpSocket ()
: fd_(-1)
{
}

SctpSocket::~SctpSocket ()
{
        Close();
}

bool SctpSocket::Connect (const string &ip, const Port port)
{
        Address addr(ip, port);

        if (!addr.IsValid())
        {
                cout << "invalid address" << endl;
                return false;
        }

        unsigned short ip_version = addr.GetIPVersion();
        if (!CreateSocket(ip_version)) return false;

        int ret_val;
        if (AF_INET == ip_version)
        {
                struct sockaddr_in sock_addr_in;
                addr.GetAddress(sock_addr_in);
                ret_val = connect(fd_, (struct sockaddr *) &sock_addr_in, sizeof(sock_addr_in));
        }
        else
        {
                struct sockaddr_in6 sock_addr_in6;
                addr.GetAddress(sock_addr_in6);
                ret_val = connect(fd_, (struct sockaddr *) &sock_addr_in6, sizeof(sock_addr_in6));
        }

        if (-1 == ret_val)
        {
                perror((ErrorHeader + "connect").c_str());
                PrintErrno();
                return false;
        }

#ifdef SOCKET_DEBUG
        cout << "socket connection done" << endl;
#endif
        return true;
}

bool SctpSocket::Listen (const unsigned short ip_version, const Port port)
{
        if (!CreateSocket (ip_version)) return false;

        int ret_val;
        if (AF_INET == ip_version)
        {
                struct sockaddr_in sock_addr_in;
                sock_addr_in.sin_family = AF_INET;
                sock_addr_in.sin_port = htons(port);
                sock_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
                ret_val = bind(fd_, (struct sockaddr *) &sock_addr_in, sizeof(sock_addr_in));
        }
        else
        {
                struct sockaddr_in6 sock_addr_in6;
                sock_addr_in6.sin6_family = AF_INET6;
                sock_addr_in6.sin6_port = htons(port);
                sock_addr_in6.sin6_addr = in6addr_any;
                ret_val = bind(fd_, (struct sockaddr *) &sock_addr_in6, sizeof(sock_addr_in6));
        }

        if (-1 == ret_val)
        {
                perror((ErrorHeader + "bind").c_str());
                PrintErrno();
                return false;
        }

        const int kQueueMaximum = 10;
        if (-1 == listen(fd_, kQueueMaximum))
        {
                perror((ErrorHeader + "listen").c_str());
                PrintErrno();
                return false;
        }

        return true;
}

void SctpSocket::Close ()
{
        if (-1 != fd_)
        {
                close(fd_);
                fd_ = -1;
        }
}

bool SctpSocket::GetAddr (string &ip, Port &port)
{
        if (-1 == fd_) return false;

        // get port
        struct sockaddr_storage addr_storage;
        socklen_t addr_len = sizeof(addr_storage);
        if (-1 == getsockname(fd_, (struct sockaddr*) &addr_storage, &addr_len))
        {
                perror((ErrorHeader + "getsockname").c_str());
                PrintErrno();
                return false;
        }

        // get ip
        char hostname[HOST_NAME_MAX];
        if (-1 == gethostname(hostname, sizeof(hostname)))
        {
                perror((ErrorHeader + "gethostname").c_str());
                PrintErrno();
                return false;
        }

        struct addrinfo hints;
        struct addrinfo *result;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = addr_storage.ss_family;

        int ret_val = getaddrinfo(hostname, NULL, &hints, &result);
        if (0 != ret_val)
        {
                cout << ErrorHeader << "getaddrinfo: " << gai_strerror(ret_val) << endl;
                return false;
        }

        // set ip and port
        if (AF_INET == addr_storage.ss_family)
        {
                struct sockaddr_in *p_addr_in = (struct sockaddr_in *) &addr_storage;
                port = ntohs(p_addr_in->sin_port);

                for (struct addrinfo *p_addrinfo = result; p_addrinfo != NULL; p_addrinfo = p_addrinfo->ai_next)
                {
                        struct sockaddr_in *p_addr_in = (struct sockaddr_in *) p_addrinfo->ai_addr;
                        char ip_str[INET_ADDRSTRLEN];
                        memset(ip_str, 0, sizeof(ip_str));
                        inet_ntop(AF_INET, &p_addr_in->sin_addr, ip_str, sizeof(ip_str));

                        if ("127.0.0.1" == string(ip_str))
                        {
                                continue;
                        }

                        ip = ip_str;
                }
        } 
        else if (AF_INET6 == addr_storage.ss_family)
        {
                struct sockaddr_in6 *p_addr_in6 = (struct sockaddr_in6 *) &addr_storage;
                port = ntohs(p_addr_in6->sin6_port);

                for (struct addrinfo *p_addrinfo = result; p_addrinfo != NULL; p_addrinfo = p_addrinfo->ai_next)
                {
                        struct sockaddr_in6 *p_addr_in6 = (struct sockaddr_in6 *) p_addrinfo->ai_addr;
                        char ip_str[INET6_ADDRSTRLEN];
                        memset(ip_str, 0, sizeof(ip_str));
                        inet_ntop(AF_INET6, &p_addr_in6->sin6_addr, ip_str, sizeof(ip_str));

                        if ("::1" == string(ip_str))
                        {
                                continue;
                        }

                        ip = ip_str;
                }
        }
        else
        {
                cout << ErrorHeader << "unhandled ss_family: " << addr_storage.ss_family << endl;
        }

        freeaddrinfo(result);
        return true;
}

bool SctpSocket::GetPeerAddr (string &ip, Port &port)
{
        if (-1 == fd_) return false;

        struct sockaddr_storage addr_storage;
        socklen_t addr_len = sizeof(addr_storage);
        if (-1 == getpeername(fd_, (struct sockaddr*) &addr_storage, &addr_len))
        {
                perror((ErrorHeader + "getpeername").c_str());
                PrintErrno();
                return false;
        }

        if (AF_INET == addr_storage.ss_family)
        {
                struct sockaddr_in *p_addr_in = (struct sockaddr_in *) &addr_storage;
                port = ntohs(p_addr_in->sin_port);
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &p_addr_in->sin_addr, ip_str, sizeof(ip_str));
                ip.assign(ip_str);
        } 
        else if (AF_INET6 == addr_storage.ss_family)
        {
                struct sockaddr_in6 *p_addr_in6 = (struct sockaddr_in6 *) &addr_storage;
                port = ntohs(p_addr_in6->sin6_port);
                char ip_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &p_addr_in6->sin6_addr, ip_str, sizeof ip_str);
                ip.assign(ip_str);
        }
        else
        {
                cout << ErrorHeader << "unhandled ss_family: " << addr_storage.ss_family << endl;
        }

        return true;
}

bool SctpSocket::SetFD (int fd)
{
        if (-1 != fd_)
        {
                cout << ErrorHeader << "can not set" << endl;
                return false;
        }

        fd_ = fd;
        return true;
}

bool SctpSocket::Send (const string &str)
{
        return Send(str.c_str(), str.length());
}

bool SctpSocket::Send (const char buffer[], int num_of_bytes)
{
#ifdef SOCKET_DEBUG
        cout << "[SEND] ";
        cout.write(buffer, num_of_bytes);
        cout << endl;
#endif
        int ret_val = send(fd_, buffer, num_of_bytes, 0);

        if (-1 == ret_val)
        {
                perror((ErrorHeader + "send").c_str());
                PrintErrno();
                return false;
        }

        return true;
}

bool SctpSocket::Receive (char buffer[], int buffer_size, int &num_of_bytes)
{
        /*
        struct sctp_sndrcvinfo sndrcvinfo;
        int flags;
        int ret_val = sctp_recvmsg(fd_, (void *) buffer, sizeof(buffer), (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
        */

        num_of_bytes = recv(fd_, buffer, buffer_size, 0);

        if (-1 == num_of_bytes)
        {
                perror((ErrorHeader + "receive").c_str());
                PrintErrno();
                return false;
        }

        if (0 == num_of_bytes)
        {
                Close();
                return true;
        }

#ifdef SOCKET_DEBUG
        cout << "[RECV] ";
        cout.write(buffer, num_of_bytes);
        cout << endl;
#endif
        
        return true;
}

bool SctpSocket::CreateSocket (unsigned short ip_version)
{
        if (-1 == fd_)
        {
                fd_ = socket(ip_version, SOCK_STREAM, IPPROTO_SCTP);
                //fd_ = socket(ip_version, SOCK_STREAM, 0);
        }

        if (-1 == fd_)
        {
                perror((ErrorHeader + "socket").c_str());
                PrintErrno();
                return false;
        }

        return true;
}

}

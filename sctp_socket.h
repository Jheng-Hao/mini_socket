#ifndef SCTP_SOCKET_H_
#define SCTP_SOCKET_H_

#include "address.h"
#include <netinet/sctp.h>

namespace socket_namespace
{

class SctpSocket
{
public:
        SctpSocket ();
        ~SctpSocket ();

        bool Connect (const std::string &ip, const Port port);
        bool Listen (const unsigned short ip_version = AF_INET, const Port port = 0);
        void Close ();

        bool GetAddr (std::string &ip, Port &port);
        bool GetPeerAddr (std::string &ip, Port &port);

        bool Send (const std::string &str);
        bool Send (const char buffer[], int num_of_bytes);
        bool Receive (char buffer[], int buffer_size, int &num_of_bytes);

        int GetFD () { return fd_; }
        bool SetFD (int fd);

private:
        int fd_;

        bool CreateSocket (unsigned short ip_version);
};

}

#endif

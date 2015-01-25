#ifndef SOCKET_MANAGER_H_
#define SOCKET_MANAGER_H_

#include <list>
#include "address.h"
#include "sctp_socket.h"

namespace socket_namespace
{

class SocketManager
{
public:
        SocketManager ();
        ~SocketManager ();

        bool CreateListenSocket (Port port = 0);
        void WaitEvent (const int TimeOut, std::list<SctpSocket *> &received_events);
        SctpSocket *GetListeningSocket () { return p_listening_socket_; }
        void AddConnectedSocket (SctpSocket *p_socket);

private:
        const int MaxEvents;
        struct epoll_event *p_ev_ret_;
        int epoll_fd_;

        SctpSocket *p_listening_socket_;
        std::list<SctpSocket *> accepted_socket_list_;
        std::list<SctpSocket *> connected_socket_list_;

        void HandleConnectionEvent (const int ListeningSocketFD);
};

}

#endif

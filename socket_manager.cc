#include <sys/epoll.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include "socket_manager.h"

using namespace std;

namespace
{

const string ErrorHeader = "[Error (SocketManager)] ";

inline void PrintErrno ()
{
        cout << "Errno: " << errno << endl;
}

}

namespace socket_namespace
{

SocketManager::SocketManager ()
: MaxEvents(1024)
, p_ev_ret_(new struct epoll_event[MaxEvents])
, epoll_fd_(-1)
, p_listening_socket_(0)
{
        epoll_fd_ = epoll_create(MaxEvents);
        if (epoll_fd_ < 0)
        {
                perror((ErrorHeader + "epoll_create").c_str());
                return;
        }
}

SocketManager::~SocketManager ()
{
        delete[] p_ev_ret_;

        if (epoll_fd_ >= 0)
        {
                close(epoll_fd_);
                epoll_fd_ = -1;
        }

        if (!p_listening_socket_)
        {
                delete p_listening_socket_;
                p_listening_socket_ = 0;
        }

        for (list<SctpSocket *>::iterator it = accepted_socket_list_.begin(); it != accepted_socket_list_.end(); ++it)
        {
                SctpSocket *p_sctp_socket = *it;
                if (!p_sctp_socket)
                {
                        delete p_sctp_socket;
                        *it = 0;
                }
        }
        accepted_socket_list_.clear();
}

bool SocketManager::CreateListenSocket (Port port)
{
        p_listening_socket_ = new SctpSocket();

        if (!p_listening_socket_->Listen(AF_INET, port))
        {
                cout << ErrorHeader << "can not listen" << endl;

                delete p_listening_socket_;
                p_listening_socket_ = 0;
                return false;
        }

        const int SocketFD = p_listening_socket_->GetFD();
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = SocketFD;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, SocketFD, &ev) != 0)
        {
                perror((ErrorHeader + "epoll_ctl").c_str());
                PrintErrno();

                delete p_listening_socket_;
                p_listening_socket_ = 0;
                return false;
        }

#ifdef DEBUG
        cout << "successfully created listening socket" << endl;
#endif
        return true;
}

void SocketManager::WaitEvent (const int TimeOut, list<SctpSocket *> &received_events)
{
        int nfds = epoll_wait(epoll_fd_, p_ev_ret_, MaxEvents, TimeOut);

        if (nfds < 0)
        {
                perror((ErrorHeader + "epoll_wait").c_str());
                return;
        }

        int listening_socket_fd = -1;
        if (0 != p_listening_socket_)
        {
                listening_socket_fd = p_listening_socket_->GetFD();
        }

        for (int idx = 0; idx < nfds; ++idx)
        {
                const int EventFD = p_ev_ret_[idx].data.fd;

                if (listening_socket_fd == EventFD)
                {
                        HandleConnectionEvent(listening_socket_fd);
                        continue;
                }

                bool matched = false;
                for (list<SctpSocket *>::iterator it = accepted_socket_list_.begin(); it != accepted_socket_list_.end(); ++it)
                {
                        SctpSocket *p_sctp_socket = *it;

                        if (!p_sctp_socket)
                        {
                                cout << ErrorHeader << "accepted socket is null" << endl;
                                continue;
                        }

                        if (EventFD != p_sctp_socket->GetFD())
                        {
                                continue;
                        }

                        matched = true;
                        received_events.push_back(p_sctp_socket);
                        break;
                }

                for (list<SctpSocket *>::iterator it = connected_socket_list_.begin(); it != connected_socket_list_.end(); ++it)
                {
                        SctpSocket *p_sctp_socket = *it;

                        if (!p_sctp_socket)
                        {
                                cout << ErrorHeader << "accepted socket is null" << endl;
                                continue;
                        }

                        if (EventFD != p_sctp_socket->GetFD())
                        {
                                continue;
                        }

                        matched = true;
                        received_events.push_back(p_sctp_socket);
                        break;
                }

                if (!matched)
                {
                        cout << ErrorHeader << "not matched event" << endl;
                }
        }
}

void SocketManager::AddConnectedSocket (SctpSocket *p_socket)
{
        if (0 == p_socket)
        {
                cout << ErrorHeader << "connected socket is null" << endl;
                return;
        }

        const int SocketFD = p_socket->GetFD();
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = SocketFD;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, SocketFD, &ev) != 0)
        {
                perror((ErrorHeader + "epoll_ctl").c_str());
                PrintErrno();
                return;
        }

        connected_socket_list_.push_back(p_socket);
}

void SocketManager::HandleConnectionEvent (const int ListeningSocketFD)
{
        const int AcceptedSocketFD = accept(ListeningSocketFD, NULL, NULL);
        if (-1 == AcceptedSocketFD)
        {
                perror((ErrorHeader + "accept").c_str());
                PrintErrno();
                return;
        }

        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = AcceptedSocketFD;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, AcceptedSocketFD, &ev) != 0)
        {
                perror((ErrorHeader + "epoll_ctl").c_str());
                PrintErrno();
                return;
        }

        SctpSocket *p_sctp_socket = new SctpSocket();
        p_sctp_socket->SetFD(AcceptedSocketFD);
        accepted_socket_list_.push_back(p_sctp_socket);

#ifdef DEBUG
        cout << "successfully accepted one connection" << endl;
#endif
}

}

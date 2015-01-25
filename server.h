#ifndef SERVER_H_
#define SERVER_H_

#include <pthread.h>
#include "socket_manager.h"

namespace project_namespace
{

class Server
{
public:
        Server ();
        
        bool Run (socket_namespace::Port port = 0);

private:
        socket_namespace::SocketManager socket_manager_;
        pthread_t server_thread_;

        static void *ServerThread (void *instance);
        void *Execute ();
        void HandleEvent (std::list<socket_namespace::SctpSocket *> &received_events);
};

}

#endif

#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include "server.h"

using namespace std;
using namespace socket_namespace;

namespace
{

struct ReceivedMessage
{
        char buffer[256];
        int size;
        SctpSocket *p_socket;
        
        string GetString ()
        {
                if (sizeof(buffer) == size)
                {
                        char temp_buffer[size + 1];
                        strncpy(temp_buffer, buffer, size);
                        temp_buffer[size] = NULL;
                        return string(temp_buffer);
                }
                else
                {
                        buffer[size] = NULL;
                        return string(buffer);
                }
        }
};

const string ErrorHeader = "[Error (Server)] ";

}

namespace project_namespace
{

Server::Server ()
: server_thread_(0)
{
}

bool Server::Run (Port port)
{
        if (!socket_manager_.CreateListenSocket(port))
        {
                cout << ErrorHeader << "can not listen" << endl;
                return false;
        }

        int ret_val = pthread_create(&server_thread_, NULL, Server::ServerThread, this);
        if (0 != ret_val)
        {
                cout << ErrorHeader << "can not create thread" << endl;
                return false;
        }

        return true;
}

void *Server::ServerThread (void *instance)
{
        return static_cast<Server *>(instance)->Execute();
}

void *Server::Execute ()
{
        // print server ip and port
        SctpSocket *p_listening_socket = socket_manager_.GetListeningSocket();
        if (!p_listening_socket)
        {
                cout << ErrorHeader << "listening socket in null" << endl;
                return NULL;
        }

        string ip;
        Port port;
        p_listening_socket->GetAddr(ip, port);

        cout << "server at " << ip << ":" << port << endl;

        list<SctpSocket *> received_events;

        while (true)
        {
                received_events.clear();
                socket_manager_.WaitEvent(5000, received_events);

                if (0 == received_events.size()) continue;

                HandleEvent(received_events);
        }
}

void Server::HandleEvent (list<SctpSocket *> &received_events)
{
        char buffer[512];
        int size;

        for (list<SctpSocket *>::iterator it = received_events.begin(); it != received_events.end(); ++it)
        {
                SctpSocket *p_socket = *it;

                ReceivedMessage received_msg;
                memset(received_msg.buffer, 0, sizeof(received_msg.buffer));
                if (!p_socket->Receive(received_msg.buffer, sizeof(received_msg.buffer), received_msg.size))
                {
                        cout << ErrorHeader << "can not receive" << endl;
                        continue;
                }

                string msg = received_msg.GetString();
                if (0 == msg.length()) continue;

                if (msg.length() < 2)
                {
                        cout << ErrorHeader << "wrong format: " << msg << endl;
                        continue;
                }

                if ("q " == msg.substr(0, 2))
                {
                        ifstream file;
                        file.open(msg.substr(2).c_str());
                        if (file.is_open())
                        {
                                file.close();
                                p_socket->Send("YES");
                        }
                        else
                        {
                                p_socket->Send("NO");
                        }
                }
                else if ("g " == msg.substr(0, 2))
                {
                        p_socket->Send("START");
                        ifstream file;
                        file.open(msg.substr(2).c_str(), ios::in | ios::binary | ios::ate);
                        if (file.is_open())
                        {
                                ifstream::pos_type pos = file.tellg();
                                cout << "file length: " << pos << endl;
                                int num_of_bytes = pos;
                                file.seekg (0, ios::beg);
                                const int BufferSize = 256;
                                char buffer[BufferSize];

                                while (num_of_bytes > 0)
                                {
                                        memset(buffer, 0, sizeof(buffer));
                                        file.read(buffer, BufferSize);
                                        //cout << "buffer: " << buffer << endl;
                                        num_of_bytes -= file.gcount();
                                        p_socket->Send(buffer, file.gcount());
                                        //p_socket->Send(buffer);
                                }
                                file.close();
                        }
                        p_socket->Send("END");
                }
                else
                {
                        cout << ErrorHeader << "unhandled event: " << msg << endl;
                }
        }
}

}

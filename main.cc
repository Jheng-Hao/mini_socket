#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <queue>
#include "server.h"
#include "socket_manager.h"

using namespace std;
using namespace socket_namespace;
using namespace project_namespace;

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

const string ErrorHeader = "[Error (main)] ";
const string IncorrectFormat = "incorrect format";

SocketManager g_socket_manager;
list<SctpSocket *> g_connected_socket_list;

pthread_mutex_t g_received_msg_mutex;
queue<ReceivedMessage> g_received_messages;

}

void ParseCommand (const string &command, vector<string> &tokens)
{
        const string Delimiter = " ";
        int start_pos = 0, end_pos;

        while (true)
        {
                end_pos = command.find(Delimiter, start_pos);
                if (end_pos != start_pos)
                {
                        tokens.push_back(command.substr(start_pos, end_pos - start_pos));
                }
                if (command.npos == end_pos) break;

                start_pos = end_pos + Delimiter.length();
        }
}

void Help ()
{
        cout << "**********************************" << endl;
        cout << "*                                *" << endl;
        cout << "*  connect server: c ip:port     *" << endl;
        cout << "*      e.g. c 192.168.2.10:5000  *" << endl;
        cout << "*                                *" << endl;
        cout << "*  get file: g file_name         *" << endl;
        cout << "*      e.g. g network.pdf        *" << endl;
        cout << "*                                *" << endl;
        cout << "*  quit program: q               *" << endl;
        cout << "*                                *" << endl;
        cout << "**********************************" << endl;
}

void Connect (const string &command)
{
        vector<string> tokens;
        ParseCommand(command, tokens);

        if (tokens.size() < 2)
        {
                cout << IncorrectFormat << endl;
                return;
        }

        const string address = tokens[1];

        int pos = address.rfind(':');
        if (address.npos == pos)
        {
                cout << IncorrectFormat << endl;
                return;
        }

        if (address.length() - 1 == pos)
        {
                cout << IncorrectFormat << endl;
                return;
        }

        string ip = address.substr(0, pos);
        Port port;
        istringstream(address.substr(pos + 1)) >> port;

        SctpSocket *p_socket = new SctpSocket();
        if (!p_socket->Connect(ip, port))
        {
                delete p_socket;
                cout << ErrorHeader << "can not connect " << ip << ":" << port << endl;
                return;
        }

        g_connected_socket_list.push_back(p_socket);
        g_socket_manager.AddConnectedSocket(p_socket);
}

void Get (const string &command)
{
        vector<string> tokens;
        ParseCommand(command, tokens);

        if (tokens.size() < 2)
        {
                cout << IncorrectFormat << endl;
                return;
        }

        const string file_name = tokens[1];

        int count = 0;
        for (list<SctpSocket *>::iterator it = g_connected_socket_list.begin(); it != g_connected_socket_list.end(); ++it)
        {
                SctpSocket *p_socket = *it;

                if (0 == p_socket)
                {
                        cout << ErrorHeader << "connected socket is null" << endl;
                        continue;
                }

                p_socket->Send("q " + file_name);
                ++count;
        }

        SctpSocket *p_socket_w_file = NULL;
        while (count > 0)
        {
                pthread_mutex_lock(&g_received_msg_mutex);
                while (!g_received_messages.empty())
                {
                        ReceivedMessage received_msg = g_received_messages.front();
                        g_received_messages.pop();
                        --count;

                        if ("YES" != received_msg.GetString()) continue;

                        p_socket_w_file = received_msg.p_socket;
                }
                pthread_mutex_unlock(&g_received_msg_mutex);

                usleep(2000);
        }

        if (!p_socket_w_file)
        {
                cout << "no server has file: " << file_name << endl;
                return;
        }

        p_socket_w_file->Send("g " + file_name);
        ofstream save_file;
        bool received_first_msg = false;
        bool received_last_msg = false;
        save_file.open(file_name.c_str());
        while (!received_last_msg)
        {
                pthread_mutex_lock(&g_received_msg_mutex);
                while (!g_received_messages.empty())
                {
                        ReceivedMessage received_msg = g_received_messages.front();
                        g_received_messages.pop();

                        if (!received_first_msg)
                        {
                                received_first_msg = true;
                                if ("START" != received_msg.GetString())
                                {
                                        cout << ErrorHeader << "not a start" << endl;
                                }
                                continue;
                        }

                        if ("END" != received_msg.GetString())
                        {
                                save_file.write(received_msg.buffer, received_msg.size);
                        }
                        else
                        {
                                received_last_msg = true;
                        }
                }
                pthread_mutex_unlock(&g_received_msg_mutex);

                usleep(2000);
        }

        save_file.close();
        cout << file_name << " saved" << endl;
}

void HandleCommand (const string &command, bool &is_quit_cmd)
{
        if (0 == command.length())
        {
                return;
        }

        char type = command[0];
        is_quit_cmd = false;

        switch (type)
        {
                case 'h':
                        Help();
                        break;
                case 'c':
                        Connect(command);
                        break;
                case 'g':
                        Get(command);
                        break;
                case 'q':
                        is_quit_cmd = true;
                        break;
                default:
                        cout << IncorrectFormat << endl;
                        break;
        }
}

void *ClientThread (void *args)
{
        list<SctpSocket *> received_events;

        while (true)
        {
                received_events.clear();
                g_socket_manager.WaitEvent(-1, received_events);

                pthread_mutex_lock(&g_received_msg_mutex);
                for (list<SctpSocket *>::iterator it = received_events.begin(); it != received_events.end(); ++it)
                {
                        SctpSocket *p_socket = *it;

                        ReceivedMessage received_msg;

                        p_socket->Receive(received_msg.buffer, sizeof(received_msg.buffer), received_msg.size);
                        received_msg.p_socket = p_socket;

                        g_received_messages.push(received_msg);
                }
                pthread_mutex_unlock(&g_received_msg_mutex);
        }
}

int main (int argc, char *argv[])
{
        Server server;
        if (!server.Run())
        {
                cout << ErrorHeader << "server is not up" << endl;
                return 0;
        }

        if (0 != pthread_mutex_init(&g_received_msg_mutex, NULL))
        {
                cout << ErrorHeader << "can not init mutex" << endl;
                return 0;
        }

        pthread_t client_thread;
        int ret_val = pthread_create(&client_thread, NULL, ClientThread, NULL);
        if (0 != ret_val)
        {
                cout << ErrorHeader << "can not create thread" << endl;
                return 0;
        }

        while (true)
        {
                string command;
                cout << "What's your command (h for help)? ";
                getline(cin, command);

                bool is_quit_cmd = false;
                HandleCommand(command, is_quit_cmd);

                if (is_quit_cmd) break;
        }

        pthread_mutex_destroy(&g_received_msg_mutex);

        for (list<SctpSocket *>::iterator it = g_connected_socket_list.begin(); it != g_connected_socket_list.end(); ++it)
        {
                delete (*it);
                *it = 0;
        }
        g_connected_socket_list.clear();

        return 0;
}

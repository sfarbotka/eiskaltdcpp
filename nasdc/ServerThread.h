/***************************************************************************
*                                                                         *
*   Copyright (C) 2009-2010  Alexandr Tkachev <tka4ev@gmail.com>          *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

// Created on: 17.08.2009

#ifndef SERVERTHREAD_H_
#define SERVERTHREAD_H_

#include "dcpp/QueueManagerListener.h"
#include "dcpp/TimerManager.h"
#include "dcpp/LogManager.h"
#include "dcpp/ClientListener.h"
#include "dcpp/ShareManager.h"
#include "dcpp/Thread.h"
//#include "dcpp/WebServerManager.h"

class ServerThread :private TimerManagerListener,
        private QueueManagerListener,
        private LogManagerListener,
        private ClientListener,
        public Thread/*,
        private WebServerListener*/
{

public:
    ServerThread();
    ~ServerThread();

    void startSocket(bool onstart, int oldmode);
    void autoConnect();
    void showPortsError(const std::string& port);
    bool disconnect_all();

    void Resume();
    void Close();
    void WaitFor();
    virtual int run();

    void connectClient (string address, string encoding);
    void disconnectClient(string address);

private:
    int server;
    unsigned int iSuspendTime;
    bool bTerminated;
    typedef map<string, Client*> ClientMap;
    typedef ClientMap::const_iterator ClientIter;
    static ClientMap clientsMap;
    //socket_t webSock;

    // TimerManagerListener
    void on(TimerManagerListener::Second, uint64_t aTick) throw();

    // ClientListener
    void on(Connecting, Client* cur) throw();
    void on(Connected, Client* cur) throw();
    void on(UserUpdated, Client* cur, const OnlineUserPtr&) throw();
    void on(UsersUpdated, Client* cur, const OnlineUserList&) throw();
    void on(UserRemoved, Client* cur, const OnlineUserPtr&) throw();
    void on(Redirect, Client* cur, const string&) throw();
    void on(Failed, Client* cur, const string&) throw();
    void on(GetPassword, Client* cur) throw();
    void on(HubUpdated, Client* cur) throw();
    void on(StatusMessage, Client* cur, const string&, int = ClientListener::FLAG_NORMAL) throw();
    void on(ClientListener::Message, Client*, const ChatMessage&) throw();
    void on(NickTaken, Client* cur) throw();
    void on(SearchFlood, Client* cur, const string&) throw();

    // WebServerListener
    //void on(WebServerListener::Setup) throw();
    //void on(WebServerListener::ShutdownPC, int) throw();

    int64_t lastUp;
    int64_t lastDown;
    uint64_t lastUpdate;
    std::string address;
    std::string encoding;
    CriticalSection shutcs;
};

#endif /* SERVERTHREAD_H_ */

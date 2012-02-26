#include "ucserver.h"
#include <QtNetwork/QtNetwork>
#include <QRegExp>

    const int UCServer::ClientMax= 10;

    UCServer::UCServer(QObject *parent) : QTcpServer(parent)
    {
        ClientCount = 0;
    }

    void UCServer::incomingConnection(int socketfd)
    {
        QTcpSocket* client = new QTcpSocket(this);

        if(ClientCount > ClientMax)
            delete client;
        else{
        client->setSocketDescriptor(socketfd);
        clients.insert(client);

        qDebug() << "New client:" << client->peerAddress().toString();

        connect(client, SIGNAL(readyRead()), this, SLOT(readyRead()));
        connect(client, SIGNAL(disconnected()), this, SLOT(disconnected()));
        ClientCount++;
        }
    }

    void UCServer::readyRead()
    {
        QTcpSocket *client = (QTcpSocket*)sender();
        while(client->canReadLine())
        {
            QString line = QString::fromUtf8(client->readLine()).trimmed();
            qDebug() << "Read line:" << line;

            QRegExp meRegex("^/me:(.*)$");

            if(meRegex.indexIn(line) != -1)
            {
                QString user = meRegex.cap(1);
                users[client] = user;
                foreach(QTcpSocket *client, clients)
                    client->write(QString("Server:" + user + " has joined.\n").toUtf8());
                sendUserList();
            }
            else if(users.contains(client))
            {
                QString message = line;
                QString user = users[client];
                qDebug() << "User:" << user;
                qDebug() << "Message:" << message;

                foreach(QTcpSocket *otherClient, clients)
                    otherClient->write(QString(user + ":" + message + "\n").toUtf8());
            }
            else
            {
                qWarning() << "Got bad message from client:" << client->peerAddress().toString() << line;
            }
        }
    }

    void UCServer::disconnected()
    {
        printf("disconnecting");
        QTcpSocket *client = (QTcpSocket*)sender();
        qDebug() << "Client disconnected:" << client->peerAddress().toString();

        clients.remove(client);

        QString user = users[client];
        users.remove(client);

        sendUserList();
        foreach(QTcpSocket *client, clients)
            client->write(QString("Server:" + user + " has left.\n").toUtf8());
    }

    void UCServer::sendUserList()
    {
        printf("Sending User List");// Just to check
        QStringList userList;
        foreach(QString user, users.values())
            userList << user;

        foreach(QTcpSocket *client, clients)
            client->write(QString("/users:" + userList.join(",") + "\n").toUtf8());
    }


    UCServer::~UCServer(){
        this->close();
    }

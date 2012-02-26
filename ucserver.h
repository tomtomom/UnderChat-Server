#ifndef UCSERVER_H
#define UCSERVER_H

#include <QStringList>
#include <QtNetwork/QtNetwork>

#include <QMap>
#include <QSet>

class UCServer : public QTcpServer
{
    public:
        UCServer(QObject *parent=0);
        ~UCServer();

    private slots:
        void readyRead();
        void disconnected();
        void sendUserList();

    protected:
        void incomingConnection(int socketfd);

    private:
        QSet<QTcpSocket*> clients;
        QMap<QTcpSocket*,QString> users;
        int ClientCount;
        static const int ClientMax; //Just so we don't over do our server


};

#endif // UCSERVER_H

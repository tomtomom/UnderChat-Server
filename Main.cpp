#include <QtCore/QCoreApplication>
#include <QtNetwork/QtNetwork>
#include "ucserver.h"

int main(int argc, char *argv[]) {

    QCoreApplication MainServer(argc, argv);

    UCServer *server = new UCServer();
    bool success = server->listen(QHostAddress::Any, 4200);
    if(!success)
    {
        qFatal("Could not listen on port 4200.");
    }

    qDebug() << "Ready";

    MainServer.exec();
}

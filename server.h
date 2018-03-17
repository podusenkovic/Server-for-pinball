#ifndef SERVER_H
#define SERVER_H

#include <QDialog>
#include <QString>
#include <QVector>
#include <QLabel>
#include <QTcpServer>
#include <QNetworkSession>
#include <QTime>
#include <QMessageBox>

class Server : public QDialog
{
    Q_OBJECT
    
public:
    Server(QWidget *parent = 0);
    ~Server();

private slots:
    void sessionOpened();
    void sendWalls();
    void readData();

private:
    QLabel *statusLabel = nullptr;
    QTcpServer *tcpServer = nullptr;
    QTcpSocket *tcpSocket = nullptr;
    QString walls;
    QNetworkSession *networkSession = nullptr;
    QDataStream in;
    
    QVector<QTcpSocket*> users;
    
};

#endif // SERVER_H

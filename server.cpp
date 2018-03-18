#include "server.h"
#include <QtWidgets>
#include <QtCore>
#include <QtNetwork>
#include <QTcpSocket>

Server::Server(QWidget *parent)
    : QDialog(parent),
      statusLabel(new QLabel)
{
    
    QTime midnight(0,0,0);
    qsrand(midnight.secsTo(QTime::currentTime()));
    
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    statusLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    
    QNetworkConfigurationManager manager;
    
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        // Get saved network configuration
        QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
        settings.endGroup();

        // If the saved network configuration is not currently discovered use the system default
        QNetworkConfiguration config = manager.configurationFromIdentifier(id);
        if ((config.state() & QNetworkConfiguration::Discovered) !=
            QNetworkConfiguration::Discovered) {
            config = manager.defaultConfiguration();
        }

        networkSession = new QNetworkSession(config, this);
        connect(networkSession, &QNetworkSession::opened, this, &Server::sessionOpened);

        statusLabel->setText(tr("Opening network session."));
        networkSession->open();
    } else {
        sessionOpened();
    }    
        
    int count = qrand()%5 + 1;
    walls = "w." + QString::number(count) + ".";
    
    for (int i = 0; i < count; i++){
        walls = walls + QString::number(qrand()%900 + 50) + 
                "." + QString::number(qrand()%600 + 50) + 
                "." + QString::number(qrand()%900 + 50) + 
                "." + QString::number(qrand()%600 + 50) + "=";
    }
    
    auto quitButton = new QPushButton(tr("Quit"));
    quitButton->setAutoDefault(false);
    connect(quitButton, SIGNAL(clicked(bool)),
            this, SLOT(close()));
    
    connect(tcpServer, SIGNAL(newConnection()),
            this, SLOT(sendWalls()));
    
    
    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(quitButton);
    buttonLayout->addStretch(1);

    QVBoxLayout *mainLayout = nullptr;
    if (QGuiApplication::styleHints()->showIsFullScreen() || QGuiApplication::styleHints()->showIsMaximized()) {
        auto outerVerticalLayout = new QVBoxLayout(this);
        outerVerticalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
        auto outerHorizontalLayout = new QHBoxLayout;
        outerHorizontalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Ignored));
        auto groupBox = new QGroupBox(QGuiApplication::applicationDisplayName());
        mainLayout = new QVBoxLayout(groupBox);
        outerHorizontalLayout->addWidget(groupBox);
        outerHorizontalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Ignored));
        outerVerticalLayout->addLayout(outerHorizontalLayout);
        outerVerticalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
    } else {
        mainLayout = new QVBoxLayout(this);
    }

    mainLayout->addWidget(statusLabel);
    mainLayout->addLayout(buttonLayout);

    setWindowTitle(QGuiApplication::applicationDisplayName());
}
    
void Server::sessionOpened(){
    if (networkSession) {
        QNetworkConfiguration config = networkSession->configuration();
        QString id;
        if (config.type() == QNetworkConfiguration::UserChoice)
            id = networkSession->sessionProperty(QLatin1String("UserChoiceConfiguration")).toString();
        else
            id = config.identifier();

        QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
        settings.endGroup();
    }
    
    tcpServer = new QTcpServer(this);
    if(!tcpServer->listen()){
        QMessageBox::critical(this, tr("Pinball Server"),
                              tr("Unable to start the server: %1.")
                              .arg(tcpServer->errorString()));
        close();
        return;
    }
    
    QString ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (int i = 0; i < ipAddressesList.size(); i++){
        if(ipAddressesList.at(i) != QHostAddress::LocalHost &&
                ipAddressesList.at(i).toIPv4Address()){
            ipAddress = ipAddressesList.at(i).toString();
            break;
        }
    }
    if (ipAddress.isEmpty())
        ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
    statusLabel->setText(tr("The server is running on\n\nIP: %1\nport: %2\n\n"
                            "Run the ebanii Pinball now.")
                            .arg(ipAddress).arg(tcpServer->serverPort()));
}

void Server::sendWalls(){
    qDebug() << tcpServer->hasPendingConnections();
    QTcpSocket *clientConnection = tcpServer->nextPendingConnection();
    qDebug() << clientConnection->peerAddress() << clientConnection->peerPort();
    if (users.contains(clientConnection->peerAddress()) &&
        sockPorts.contains(clientConnection->peerPort()))
    {
        qDebug() << clientConnection->peerAddress() << " " << clientConnection->peerPort();
        connect(clientConnection, SIGNAL(readyRead()), this, SLOT(readData()));
        return;
    }
    else {
        connect(clientConnection, SIGNAL(readyRead()), this, SLOT(readIp()));
        qDebug() << "after connect " << clientConnection->state();
    }
    
    QByteArray block;
    
    users.push_back(clientConnection->peerAddress());
    sockPorts.push_back(clientConnection->peerPort());
    QDataStream out(&block, QIODevice::WriteOnly);
     
    out.setVersion(QDataStream::Qt_4_0);        
    out << walls;
    //qDebug() << walls;
    clientConnection->write(block); 
    //qDebug() << "status after walls " << clientConnection->state();
}

Server::~Server()
{
    
}


void Server::readData()
{   
    qDebug() << "readingData";
    QTcpSocket* a = (QTcpSocket*)sender();
    //qDebug() << "peeraddress " << a->peerAddress() << " " << a->peerPort();
    //QByteArray block;`
    QDataStream in(a);
    in.setVersion(QDataStream::Qt_4_0);
    
    QString data;
    in >> data;
    quint16 p = a->peerPort();
    newBall = a->peerAddress().toString().remove(0,7) + "/" + QString::number(a->peerPort()) + "/b." + data;
    qDebug() << newBall;
    QTimer::singleShot(100, this, SLOT(sendBalls()));
}

void Server::sendBalls(){ 
    QByteArray block;   
    QTcpSocket* client = new QTcpSocket(this);
    for (int i = 0; i < IPs.size(); i++)
    {
        if (IPs[i] == newBall.split("/")[0] && ports[i] == newBall.split("/")[1])
            continue;
        
        client->connectToHost(QHostAddress(IPs[i]),ports[i]);

        qDebug() << "IM CONNECTING TO SEND A BALL " + newBall
                 << "TO IP " << IPs[i] << " " << ports[i];
    
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_0);        
            
        out << newBall.split("/")[2];
        
        qDebug() << client->write(block);
        client->disconnectFromHost();
    }
    
}

void Server::readIp(){
    QTcpSocket *clientConnection = (QTcpSocket*)sender();
    QDataStream in(clientConnection);
    in.setVersion(QDataStream::Qt_4_0);
    
    QString data;
    in >> data;
    
    qDebug() << data;
    IPs.push_back(data.split(":")[0]);
    ports.push_back(data.split(":")[1].toInt());
    
    qDebug() << data.split(":")[1].toInt();
    
    qDebug() << "status after reading ip " << clientConnection->state();
    clientConnection->disconnectFromHost();
}

void Server::sendBallsData(){
}

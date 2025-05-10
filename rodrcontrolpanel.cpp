#include "rodrcontrolpanel.h"
#include "./ui_rodrcontrolpanel.h"

#include <QValidator>
#include <QtConcurrent>

#include "TCPClient.hpp"
#include "UDPCommunication.hpp"

namespace rodr
{
    namespace udp
    {
        constexpr u_short REMOTE_PORT = 4000;
        constexpr u_short LOCAL_PORT = 2000;
    }

    namespace tcp
    {
        constexpr u_short PORT = 2000;
    }
    namespace connection
    {
        constexpr const char* REMOTE_IP = "192.168.113.5";
        constexpr const char* SOURCE_IP = "192.168.113.4";

        enum class Status
        {
            Disconnected = 0,
            Connected
        };

        std::unique_ptr<rodr::udp::UDP> feedback_connection;
        std::unique_ptr<rodr::tcp::TCPClient> tcp_client;

        static Status UDP_status = Status::Disconnected;
        static Status TCP_status = Status::Disconnected;
    }
}



RODRControlPanel::RODRControlPanel(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::RODRControlPanel)
{
    ui->setupUi(this);
    this->setFixedSize(geometry().width(), geometry().height());

    //leSendPos regex
    QRegularExpression rx("\\d{1,2}\\.\\d{0,3}");
    QRegularExpressionValidator *regex_validator = new QRegularExpressionValidator(rx, this);

    ui->leSendPos->setValidator(regex_validator);
}

RODRControlPanel::~RODRControlPanel()
{
    delete ui;
}

void RODRControlPanel::on_btnConnectTCP_clicked()
{
    rodr::connection::tcp_client.reset();

    if (!(bool)rodr::connection::TCP_status)
    {
        ui->lblStatusTCP->setText("connecting...");
        ui->btnConnectTCP->setEnabled(false);

        QCoreApplication::processEvents();

        //runnning concurrently so the app wont go into not responding state

        QtConcurrent::run([this](){
            try
            {
                rodr::connection::tcp_client = std::make_unique<rodr::tcp::TCPClient>(rodr::connection::REMOTE_IP, rodr::tcp::PORT);
            } catch (std::exception)
            {
                rodr::connection::TCP_status = rodr::connection::Status::Disconnected;
                ui->lblStatusTCP->setText("not connected");
                ui->btnConnectTCP->setEnabled(true);
                return;
            }

            if (rodr::connection::tcp_client->Test().first)
            {
                rodr::connection::TCP_status = rodr::connection::Status::Connected;
                ui->lblStatusTCP->setText("connected");
                ui->btnConnectTCP->setText("Disconnect");
            }
            else
            {
                rodr::connection::TCP_status = rodr::connection::Status::Disconnected;
                ui->lblStatusTCP->setText("not connected");
            }

            ui->btnConnectTCP->setEnabled(true);
        });
    }
    else
    {
        rodr::connection::tcp_client.reset();
        rodr::connection::TCP_status = rodr::connection::Status::Disconnected;

        ui->lblStatusTCP->setText("not connected");
        ui->btnConnectTCP->setText("Connect");
    }

}


void RODRControlPanel::on_btnConnectUDP_clicked()
{
    rodr::connection::feedback_connection.reset();

    if (!(bool)rodr::connection::UDP_status)
    {
        ui->lblStatusUDP->setText("connecting...");
        ui->btnConnectUDP->setEnabled(false);

        QCoreApplication::processEvents();

        //runnning concurrently so the app wont go into not responding state
        QtConcurrent::run([this](){
            try
            {
                rodr::connection::feedback_connection = std::make_unique<rodr::udp::UDP>(rodr::connection::SOURCE_IP, rodr::connection::REMOTE_IP,
                                                                                         rodr::udp::LOCAL_PORT, rodr::udp::REMOTE_PORT);
            } catch (std::exception)
            {
                rodr::connection::UDP_status = rodr::connection::Status::Disconnected;
                ui->lblStatusUDP->setText("not connected");
                ui->btnConnectUDP->setEnabled(true);
                return;
            }

            rodr::connection::UDP_status = rodr::connection::Status::Connected;
            ui->lblStatusUDP->setText("connected");
            ui->btnConnectUDP->setText("Disconnect");
            ui->btnConnectUDP->setEnabled(true);
        });
    }
    else
    {
        rodr::connection::feedback_connection.reset();
        rodr::connection::UDP_status = rodr::connection::Status::Disconnected;

        ui->lblStatusUDP->setText("not connected");
        ui->btnConnectUDP->setText("Connect");
    }
}



#include "rodrcontrolpanel.h"
#include "./ui_rodrcontrolpanel.h"

#include <QValidator>
#include <QtConcurrent>
#include <QScrollBar>

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
        handler message_handler;
        handler error_handler;
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

    //setting up handlers
    rodr::tcp::message_handler = [this](const char* buff){
        ui->leCmdOut->setText(buff);

        ui->lsCmdHist->addItem(ui->leSendCmd->text());
        ui->lsCmdOutHist->addItem(QString(buff).trimmed());
    };

    rodr::tcp::error_handler = [this](const char* buff){
        ui->leCmdOut->setText("receive failed");

        const auto cmd = ui->leSendCmd->text();

        QListWidgetItem* err = new QListWidgetItem("TCP error: command \"" + cmd + "\" resulted in error \"No return value received\"");
        err->setForeground(Qt::red);

        ui->lsErrorPC->addItem(err);
        ui->lsCmdHist->addItem(cmd);

        QListWidgetItem* hist_out_msg = new QListWidgetItem(QString("Receive failed with code: %1").arg(buff));
        hist_out_msg->setForeground(Qt::red);

        ui->lsCmdOutHist->addItem(hist_out_msg);
    };

    //leSendPos regex
    QRegularExpression rx("\\d{1,2}\\.\\d{0,3}");
    QRegularExpressionValidator *regex_validator = new QRegularExpressionValidator(rx, this);

    ui->leSendPos->setValidator(regex_validator);

    //synchronising scroll bars of command history list widgets
    connect(ui->lsCmdHist->verticalScrollBar(), &QScrollBar::valueChanged, ui->lsCmdOutHist->verticalScrollBar(), &QScrollBar::setValue);
    connect(ui->lsCmdOutHist->verticalScrollBar(), &QScrollBar::valueChanged, ui->lsCmdHist->verticalScrollBar(), &QScrollBar::setValue);

    ui->lsCmdHist->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->lsCmdOutHist->setSelectionMode(QAbstractItemView::SingleSelection);

    //synchronising selected elements of command history list widgets
    connect(ui->lsCmdHist, &QListWidget::itemSelectionChanged, this, &RODRControlPanel::syncSelectedItemsFromCmdHist);
    connect(ui->lsCmdOutHist, &QListWidget::itemSelectionChanged, this, &RODRControlPanel::syncSelectedItemsFromCmdOutHist);

    //tmp dummmy data
    QStringList list;
    for (int i = 0; i < 100; i++)
    {
        list.append(QString::number(i));
    }

    ui->lsCmdHist->addItems(list);
    ui->lsCmdOutHist->addItems(list);

    //disabling buttons that require connection
    ui->btnSendPos->setEnabled(false);
    ui->btnSendCmd->setEnabled(false);
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
            ui->btnSendCmd->setEnabled(true);
        });
    }
    else
    {
        rodr::connection::tcp_client.reset();
        rodr::connection::TCP_status = rodr::connection::Status::Disconnected;

        ui->btnSendCmd->setEnabled(false);
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

void RODRControlPanel::syncSelectedItemsFromCmdOutHist()
{
    if (syncingItems) return;
    syncingItems = true;

    ui->lsCmdHist->setCurrentRow(ui->lsCmdOutHist->currentRow());

    syncingItems = false;
}

void RODRControlPanel::syncSelectedItemsFromCmdHist()
{
    if (syncingItems) return;
    syncingItems = true;

    ui->lsCmdOutHist->setCurrentRow(ui->lsCmdHist->currentRow());

    syncingItems = false;
}

void RODRControlPanel::on_btnSendCmd_clicked()
{
    const auto& msg = ui->leSendCmd->text();
    if (msg.length() > 0)
    {
        rodr::connection::tcp_client->SendMsg(msg.toUpper().toUtf8().constData());

        QtConcurrent::run([this]{
            ui->btnSendCmd->setEnabled(false);
            ui->leSendCmd->setEnabled(false);

            rodr::connection::tcp_client->ReceiveAndHandle(rodr::tcp::message_handler, rodr::tcp::error_handler);

            ui->btnSendCmd->setEnabled(true);
            ui->leSendCmd->setEnabled(true);

            //wait for ui update and scroll to last element
            QTimer::singleShot(0, this, [this]() {
                ui->lsCmdHist->scrollToItem(ui->lsCmdHist->item(ui->lsCmdHist->count() - 1));
            });

        });


    }

}


#ifndef RODRCONTROLPANEL_H
#define RODRCONTROLPANEL_H

#include <QMainWindow>

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
        handler CmdReceiveMessageHandler;
        handler PosReceiveMessageHandler;

        handler CmdReceiveErrorHandler;
        handler PosReceiveErrorHandler;

        handler SendMessageHandler;
        handler SendPosHandler;
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

QT_BEGIN_NAMESPACE
namespace Ui {
class RODRControlPanel;
}
QT_END_NAMESPACE

class RODRControlPanel : public QMainWindow
{
    Q_OBJECT

public:
    RODRControlPanel(QWidget *parent = nullptr);
    ~RODRControlPanel();

    void addPCErr(const char* src, const QString& cmd, rodr::ERROR_TYPE type);
    void addSTMErr(const char* src, const QString& cmd, rodr::ERROR_TYPE type);

private slots:
    void syncSelectedItemsFromCmdHist();
    void syncSelectedItemsFromCmdOutHist();

    void on_btnConnectTCP_clicked();

    void on_btnConnectUDP_clicked();

    void on_btnSendCmd_clicked();

    void on_btnSendPos_clicked();

private:
    bool syncingItems = false;
    Ui::RODRControlPanel *ui;

signals:
    void enablePosWidgets();
    void scrollListEditsToLastItem();
};
#endif // RODRCONTROLPANEL_H

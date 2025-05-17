#ifndef RODRCONTROLPANEL_H
#define RODRCONTROLPANEL_H

#include <QMainWindow>

#include <QValidator>
#include <QtConcurrent>
#include <QScrollBar>

#include <atomic>

#include "TCPClient.hpp"
#include "UDPCommunication.hpp"

namespace rodr
{
    namespace udp
    {
        //A wrapper class for handling UDPCommunication feedback
        class UDPFeedBackWorker : public QObject
        {
            Q_OBJECT

        public:
            UDPFeedBackWorker(std::shared_ptr<rodr::udp::UDP> conn, rodr::handler handler) : connection_(std::move(conn)), handler_(handler) {};

        public slots:
            void run();

        private:
            std::shared_ptr<rodr::udp::UDP> connection_;
            rodr::handler handler_;
        };

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

    std::unique_ptr<QThread> FeedBackThread = nullptr;

    Ui::RODRControlPanel *ui;

signals:
    void enablePosWidgets();
    void scrollListEditsToLastItem();
};
#endif // RODRCONTROLPANEL_H

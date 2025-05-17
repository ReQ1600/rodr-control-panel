#include "rodrcontrolpanel.h"
#include "./ui_rodrcontrolpanel.h"

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

        static inline std::atomic<Status> UDP_status = Status::Disconnected;
        static inline std::atomic<Status> TCP_status = Status::Disconnected;
    }
}

void rodr::udp::UDPFeedBackWorker::run()
{
    if (connection_)
    {
        while (rodr::connection::UDP_status == rodr::connection::Status::Connected)
        {
            connection_->ReceiveAndHandle(handler_);
        }
    }
}

RODRControlPanel::RODRControlPanel(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::RODRControlPanel)
{
    ui->setupUi(this);
    this->setFixedSize(geometry().width(), geometry().height());

    //setting up feedback thread
    FeedBackThread = std::make_unique<QThread>();


    //setting up handlers
    rodr::tcp::CmdReceiveMessageHandler = [this](const char* buff){
        ui->leCmdOut->setText(buff);

        ui->lsCmdHist->addItem(ui->leSendCmd->text());
        ui->lsCmdOutHist->addItem(QString(buff).trimmed());
    };

    rodr::tcp::CmdReceiveErrorHandler = [this](const char* buff){
        ui->leCmdOut->setText("receive failed");

        const auto cmd = ui->leSendCmd->text();

        addPCErr(rodr::err_src::TCP, cmd, rodr::ERROR_TYPE::Receive);
        ui->lsCmdHist->addItem(cmd);

        QListWidgetItem* hist_out_msg = new QListWidgetItem(QString("Receive failed with code: %1").arg(buff));
        hist_out_msg->setForeground(Qt::red);

        ui->lsCmdOutHist->addItem(hist_out_msg);
    };

    rodr::tcp::SendMessageHandler = [this](const char* buff){
        ui->leCmdOut->setText("send failed");

        const auto cmd = ui->leSendCmd->text();

        addPCErr(rodr::err_src::TCP, cmd, rodr::ERROR_TYPE::SendCmd);
        ui->lsCmdHist->addItem(cmd);

        QListWidgetItem* hist_out_msg = new QListWidgetItem(QString("Send failed with error code: %1").arg(buff));
        hist_out_msg->setForeground(Qt::red);

        ui->lsCmdOutHist->addItem(hist_out_msg);
    };

    rodr::tcp::SendPosHandler = [this](const char* buff){
        ui->leCmdOut->setText("send failed");

        const auto cmd = ui->leSendPos->text();

        addPCErr(rodr::err_src::TCP, cmd, rodr::ERROR_TYPE::SendPos);
    };


    rodr::tcp::PosReceiveMessageHandler = [this](const char* buff) {
        //receive should return posOK
        if (strcmp("posOK", buff) != 0)
            addPCErr(rodr::err_src::TCP, "recvPos", rodr::ERROR_TYPE::AcceptPos);
    };

    rodr::tcp::PosReceiveErrorHandler = [this](const char* buff) {
        using namespace Qt::StringLiterals;
        addPCErr(rodr::err_src::TCP, "recvPos "_L1 % buff, rodr::ERROR_TYPE::Receive);
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

    //disabling buttons that require connection
    ui->btnSendPos->setEnabled(false);
    ui->btnSendCmd->setEnabled(false);

    //connecting signals
    connect(this, &RODRControlPanel::enablePosWidgets, this, [this](){
        ui->btnSendPos->setEnabled(true);
        ui->leSendPos->setEnabled(true);
    });

    connect(this, &RODRControlPanel::scrollListEditsToLastItem, this, [this](){
        ui->lsCmdHist->scrollToItem(ui->lsCmdHist->item(ui->lsCmdHist->count() - 1));
        ui->lsErrorPC->scrollToItem(ui->lsErrorPC->item(ui->lsErrorPC->count() - 1));
        ui->lsErrorStm->scrollToItem(ui->lsErrorStm->item(ui->lsErrorStm->count() - 1));
    });

    //TODO: connect the rest of enabling widget signals.
}

RODRControlPanel::~RODRControlPanel()
{
    delete ui;
}


//adds given error to ErrorPC list edit
void RODRControlPanel::addPCErr(const char* src, const QString& cmd, rodr::ERROR_TYPE type)
{
    using namespace Qt::StringLiterals;

    const char* msg = rodr::err_msgs::getErrorMsg(type);

    QListWidgetItem* err = new QListWidgetItem(src % ": command \""_L1 % cmd %"\" resulted in error: \""_L1 % msg % "\""_L1);
    err->setForeground(Qt::red);

    ui->lsErrorPC->addItem(err);
}

//adds given error to ErrorStm list edit
void RODRControlPanel::addSTMErr(const char* src, const QString& cmd, rodr::ERROR_TYPE type)
{
    using namespace Qt::StringLiterals;

    const char* msg = rodr::err_msgs::getErrorMsg(type);

    QListWidgetItem* err = new QListWidgetItem(src % ": command \""_L1 % cmd %"\" resulted in error: \""_L1 % msg % "\""_L1);
    err->setForeground(Qt::red);

    ui->lsErrorStm->addItem(err);
}

void RODRControlPanel::on_btnConnectTCP_clicked()
{
    rodr::connection::tcp_client.reset();

    if (rodr::connection::TCP_status == rodr::connection::Status::Disconnected)
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

                addPCErr(rodr::err_src::TCP, "connect", rodr::ERROR_TYPE::Creation);

                ui->lblStatusTCP->setText("not connected");
                ui->btnConnectTCP->setEnabled(true);
                return;
            }

            auto [success, successful_pings] = rodr::connection::tcp_client->Test();
            if (success)
            {
                rodr::connection::TCP_status = rodr::connection::Status::Connected;
                ui->lblStatusTCP->setText("connected");
                ui->btnConnectTCP->setText("Disconnect");

                ui->btnSendCmd->setEnabled(true);
                ui->btnSendPos->setEnabled(true);
            }
            else
            {
                rodr::connection::TCP_status = rodr::connection::Status::Disconnected;

                addPCErr(rodr::err_src::TCP, "connect", rodr::ERROR_TYPE::Creation);
                ui->lblStatusTCP->setText("not connected");
            }

            ui->btnConnectTCP->setEnabled(true);
        });
    }
    else
    {
        rodr::connection::tcp_client.reset();
        rodr::connection::TCP_status = rodr::connection::Status::Disconnected;

        ui->btnSendCmd->setEnabled(false);
        ui->btnSendPos->setEnabled(false);

        ui->lblStatusTCP->setText("not connected");
        ui->btnConnectTCP->setText("Connect");
    }

}

void RODRControlPanel::on_btnConnectUDP_clicked()
{
    rodr::connection::feedback_connection.reset();

    if (rodr::connection::UDP_status == rodr::connection::Status::Disconnected)
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

                addPCErr(rodr::err_src::UDP, "connect", rodr::ERROR_TYPE::Creation);

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
        rodr::connection::tcp_client->SendMsg(msg.toUpper().toUtf8().constData(), rodr::tcp::SendMessageHandler);

        ui->btnSendCmd->setEnabled(false);
        ui->leSendCmd->setEnabled(false);

        QtConcurrent::run([this]{
            rodr::connection::tcp_client->ReceiveAndHandle(rodr::tcp::CmdReceiveMessageHandler, rodr::tcp::CmdReceiveErrorHandler);

            ui->btnSendCmd->setEnabled(true);
            ui->leSendCmd->setEnabled(true);

            //scrolling to last item on every error list edit
            emit scrollListEditsToLastItem();
        });
    }
}

void RODRControlPanel::on_btnSendPos_clicked()
{
    const auto& pos = ui->leSendPos->text();

    if (pos.length() > 0)
    {
        ui->btnSendPos->setEnabled(false);
        ui->leSendPos->setEnabled(false);

        auto msg = QString("SETPOS:%1").arg(pos).toUtf8().constData();
        rodr::connection::tcp_client->SendMsg(msg, rodr::tcp::SendPosHandler);

        //should return posOK
        rodr::connection::tcp_client->ReceiveAndHandle(rodr::tcp::PosReceiveMessageHandler, rodr::tcp::PosReceiveErrorHandler);

        //enabling button and line edit
        emit enablePosWidgets();
    }
}

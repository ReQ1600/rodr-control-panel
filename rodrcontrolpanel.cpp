#include "rodrcontrolpanel.h"
#include "./ui_rodrcontrolpanel.h"

#include <chrono>
#include <thread>

namespace rodr
{
    namespace udp
    {
        constexpr u_short REMOTE_PORT = 1000;
        constexpr u_short LOCAL_PORT = 5000;

        //set up in RODRControlPanel constructor
        handler FeedbackHandler;
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

        std::shared_ptr<rodr::udp::UDP> feedback_connection;
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
            using namespace std::chrono_literals;
            connection_->ReceiveAndHandle(handler_, [](const char* buff){std::this_thread::sleep_for(500ms);/*timeout so it wont spam err msgs*/});
        }
        emit finished();
    }
}

RODRControlPanel::RODRControlPanel(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::RODRControlPanel)
{
    ui->setupUi(this);
    this->setFixedSize(geometry().width(), geometry().height());

    //setting up TCP handlers
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
        if (strcmp("SETPOS_OK", buff) == 0) return;

        if (strcmp("SETPOS_ERR", buff) == 0)
        {
            addSTMErr(rodr::err_src::TCP, "recvPos", rodr::ERROR_TYPE::StmAcceptPos);
            ui->leStatus->setText("OK");
        }
        else
        {
            //in case some gibberish is returned
            addPCErr(rodr::err_src::TCP, "recvPos", rodr::ERROR_TYPE::BadReturnVal);
            ui->leStatus->setText("ERROR");
        }
    };

    rodr::tcp::PosReceiveErrorHandler = [this](const char* buff) {
        using namespace Qt::StringLiterals;
        addPCErr(rodr::err_src::TCP, "recvPos "_L1 % buff, rodr::ERROR_TYPE::Receive);
        ui->leStatus->setText("ERROR");
    };

    ////setting up UDP feedback handler
    rodr::udp::FeedbackHandler = [this](const char* buff)
    {
        std::cout << buff << std::endl;

        auto& fbTable = ui->twFeedback;
        const int& row = fbTable->rowCount();
        fbTable->insertRow(row);

        int time, humidity;
        float position;
        bool displayFb = ui->cbFeedDisp->isChecked();

        //data received on udp should be in format: time;position;humidity;...

        sscanf(buff, "%d;%f;%d;",&time, &position, &humidity);
        ui->leRecPos->setText(QString::number(position));

        if (record_)
        {
            //should be open, checking just to be sure
            if (recording_file_.is_open())
            {
                //tags should be in format  time;pos;hum;...
                const auto& tags = ui->leRecTags->text().toLocal8Bit();

                //if no tags are provided record all
                if(tags.length() == 0)
                    recording_file_ << time << ';' << position << ';' << humidity << ';' << std::endl;
                else
                {
                    //i dont like this but have no idea how to do it better
                    if (strstr(tags,"time")) recording_file_ << time << ';';
                    if (strstr(tags,"pos")) recording_file_ << time << ';';
                    if (strstr(tags,"hum")) recording_file_ << time << ';';

                    recording_file_ << std::endl;
                }
            }
        }

        if (displayFb)
        {
            fbTable->setItem(row, 0, new QTableWidgetItem(QString::number(time)));
            fbTable->setItem(row, 1, new QTableWidgetItem(QString::number(position)));
            fbTable->setItem(row, 2, new QTableWidgetItem(QString::number(humidity)));
            fbTable->scrollToBottom();
        }
    };

    //leSendPos regex
    QRegularExpression rx("\\d{4}");
    QRegularExpressionValidator *regex_validator = new QRegularExpressionValidator(rx, this);

    ui->leSendPos->setValidator(regex_validator);

    //setting up num of columns for twFeedback
    ui->twFeedback->setColumnCount(3);

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
        ui->lsCmdOutHist->scrollToBottom();
        ui->lsErrorPC->scrollToBottom();
        ui->lsErrorStm->scrollToBottom();
    });
}

RODRControlPanel::~RODRControlPanel()
{
    if (recording_file_.is_open()) recording_file_.close();
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
                rodr::connection::feedback_connection = std::make_shared<rodr::udp::UDP>(rodr::connection::SOURCE_IP, rodr::connection::REMOTE_IP,
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

            feedback_thread_ = std::make_unique<QThread>();

            feedback_worker_ = std::make_unique<rodr::udp::UDPFeedBackWorker>(rodr::connection::feedback_connection, rodr::udp::FeedbackHandler);
            feedback_worker_->moveToThread(feedback_thread_.get());

            //connecting all of the feedback stuff
            connect(feedback_thread_.get(), &QThread::started, feedback_worker_.get(), &rodr::udp::UDPFeedBackWorker::run);
            connect(feedback_worker_.get(), &rodr::udp::UDPFeedBackWorker::finished, feedback_worker_.get(), &QObject::deleteLater);
            connect(feedback_worker_.get(), &rodr::udp::UDPFeedBackWorker::finished, feedback_thread_.get(), &QThread::quit);
            connect(feedback_thread_.get(), &QThread::finished, feedback_thread_.get(), &QObject::deleteLater);

            ui->lblStatusUDP->setText("connected");
            ui->btnConnectUDP->setText("Disconnect");
            ui->btnConnectUDP->setEnabled(true);

            feedback_thread_->start();
        });
    }
    else
    {
        rodr::connection::UDP_status = rodr::connection::Status::Disconnected;

        //should have been deleted after thread finished so only released here
        feedback_worker_.release();
        feedback_thread_.release();

        rodr::connection::feedback_connection.reset();

        ui->lblStatusUDP->setText("not connected");
        ui->btnConnectUDP->setText("Connect");
    }
}

void RODRControlPanel::syncSelectedItemsFromCmdOutHist()
{
    if (syncing_items_) return;
    syncing_items_ = true;

    ui->lsCmdHist->setCurrentRow(ui->lsCmdOutHist->currentRow());

    syncing_items_ = false;
}

void RODRControlPanel::syncSelectedItemsFromCmdHist()
{
    if (syncing_items_) return;
    syncing_items_ = true;

    ui->lsCmdOutHist->setCurrentRow(ui->lsCmdHist->currentRow());

    syncing_items_ = false;
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

void RODRControlPanel::on_btnRecStart_clicked()
{
    const auto& file_path = ui->leRecFilePath->text();

    //if file path not provided
    if (file_path.length() == 0) return;

    record_ = !record_;

    if (record_)
    {
        ui->btnRecStart->setText("Stop Recording");

        recording_file_.open(file_path.toLocal8Bit());
        if(!recording_file_.is_open())
        {
            addPCErr(rodr::err_src::RECORDING, "file.open", rodr::ERROR_TYPE::OpenFile);
            record_ = false;
            ui->btnRecStart->setText("Start Recording");
        }
    }
    else
    {
        ui->btnRecStart->setText("Start Recording");
        recording_file_.close();
    }
}


void RODRControlPanel::on_btnFeedTags_clicked()
{
    const auto& tags = ui->leFeedTags->text().toLocal8Bit();

    //resetting the table (showing all columns)
    for (int i = 0; i < ui->twFeedback->columnCount(); ++i)
        ui->twFeedback->showColumn(i);

    //if no tags provided show all
    if (tags.length() == 0) return;

    for (int i = 0; i < rodr::COLUMN_TAGS_MAP_SIZE; ++i)
    {
        //hiding column if its tag was not found
        if(!strstr(tags,rodr::ColumnTagsMap[i]))
            ui->twFeedback->hideColumn(i);
    }
}


#include "rodrcontrolpanel.h"
#include "./ui_rodrcontrolpanel.h"

#include <QValidator>

namespace Connection
{
    enum class Status
    {
        Disconnected = 0,
        Connected
    };

    static bool UDP = static_cast<bool>(Status::Disconnected);
    static bool TCP = static_cast<bool>(Status::Disconnected);
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
    ui->lblStatusTCP->setText("connecting...");
}


void RODRControlPanel::on_btnConnectUDP_clicked()
{
    ui->lblStatusUDP->setText("connecting...");
}



#ifndef RODRCONTROLPANEL_H
#define RODRCONTROLPANEL_H

#include <QMainWindow>

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

private slots:
    void syncSelectedItemsFromCmdHist();
    void syncSelectedItemsFromCmdOutHist();

    void on_btnConnectTCP_clicked();

    void on_btnConnectUDP_clicked();

    void on_btnSendCmd_clicked();

private:
    bool syncingItems = false;
    Ui::RODRControlPanel *ui;
};
#endif // RODRCONTROLPANEL_H

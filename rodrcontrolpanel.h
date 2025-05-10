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
    void on_btnConnectTCP_clicked();

    void on_btnConnectUDP_clicked();

private:
    Ui::RODRControlPanel *ui;
};
#endif // RODRCONTROLPANEL_H

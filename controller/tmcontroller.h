#ifndef TMCONTROLLER_H
#define TMCONTROLLER_H

#include <QDialog>
#include <QTime>
#include <QHostInfo>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include <windows.h>
#include <tchar.h>
#include <psapi.h>

namespace Ui {
class TMController;
}

class TMController : public QDialog
{
    Q_OBJECT

public:
    explicit TMController(QWidget *parent = 0);
    ~TMController();

private slots:
    void on_confirmButton_clicked();
    void on_closeButton_clicked();
    void on_runButton_clicked();

private:
    void initData();

    Ui::TMController *ui;
    QString appDataDir;
    QString userEmail;
    QString userApiKey;
    QString computerName;
    QString userStatus;

};

#endif // TMCONTROLLER_H

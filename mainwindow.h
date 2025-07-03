#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnSelectFile_clicked();
    void on_btnGenerate_clicked();
    void on_btnCheckFFmpeg_clicked();

    void downloadFinished();

private:
    Ui::MainWindow *ui;

    QString firstUrl;
    QString fallbackUrl;
    QString currentDownloadUrl;

    int lastDownloadProgressShown = 0;

    QNetworkAccessManager* networkManager = nullptr;
    QNetworkReply* reply = nullptr;
    QProcess ffmpegProcess;

    QString ffmpegPath;
    QString extractPath;
    QString inputFilePath;

    void updateStatus(const QString &text);
    void logMessage(const QString &msg);
    void setButtonsEnabled(bool enabled);

    bool findLocalFFmpeg();
    bool checkFFmpegInSystem();

    void downloadFFmpeg();
    void startDownload(const QString &url);
    void unzipFFmpeg();
};

#endif // MAINWINDOW_H

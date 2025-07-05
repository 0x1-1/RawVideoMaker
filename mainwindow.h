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
    void on_btnAddFiles_clicked();
    void on_btnRemoveFiles_clicked();
    void on_btnSelectOutputFile_clicked();
    void on_btnPreview_clicked();
    void on_btnCancel_clicked();
    void on_comboResolution_currentIndexChanged(int index);
    void on_comboTheme_currentIndexChanged(int index);
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

    QStringList batchInputFiles;
    QString outputFilePath;
    bool isBatchProcessing = false;
    bool isProcessing = false;

    void updateStatus(const QString &text);
    void logMessage(const QString &msg);
    void setButtonsEnabled(bool enabled);

    bool findLocalFFmpeg();
    bool checkFFmpegInSystem();

    void downloadFFmpeg();
    void startDownload(const QString &url);
    void unzipFFmpeg();
    void applyTheme(const QString& themeName);
    void updateAdvancedOptions();
    void processFiles(const QStringList& files);
    void processSingleFile(const QString& inputFile, const QString& outputFile);
    void cancelProcessing();
};

#endif // MAINWINDOW_H

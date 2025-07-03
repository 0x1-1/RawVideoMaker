#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QNetworkRequest>
#include <QDebug>
#include <QProcess>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    extractPath = QCoreApplication::applicationDirPath() + "/ffmpeg_extracted";
    networkManager = new QNetworkAccessManager(this);

    setButtonsEnabled(false);

    updateStatus("Checking for ffmpeg...");

    bool hasFFmpeg = false;
    if (!findLocalFFmpeg())
    {
        updateStatus("ffmpeg NOT found locally. Checking system PATH...");
        if (!checkFFmpegInSystem())
        {
            updateStatus("ffmpeg not found. Starting automatic download...");
            firstUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip";
            fallbackUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n4.4-latest-win64-gpl.zip";
            currentDownloadUrl = firstUrl;
            downloadFFmpeg();
        }
        else
        {
            updateStatus("ffmpeg found in system PATH.");
            hasFFmpeg = true;
        }
    }
    else
    {
        updateStatus("ffmpeg found locally.");
        hasFFmpeg = true;
    }

    setButtonsEnabled(hasFFmpeg);

    ui->progressBar->setValue(0);
    logMessage("Application started.");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setButtonsEnabled(bool enabled)
{
    ui->btnSelectFile->setEnabled(enabled);
    ui->btnGenerate->setEnabled(enabled);
    ui->btnCheckFFmpeg->setEnabled(enabled);
}

void MainWindow::updateStatus(const QString &text)
{
    statusBar()->showMessage(text, 5000);
    logMessage(text);
}

void MainWindow::logMessage(const QString &msg)
{
    ui->textEditLog->append(msg);
    qDebug() << msg;
}

bool MainWindow::findLocalFFmpeg()
{
    QDir dir(extractPath);
    if (!dir.exists())
        return false;

    QString ffmpegExeName = "ffmpeg.exe";

    std::function<QString(const QDir&)> recursiveSearch = [&](const QDir& directory) -> QString {
        // Önce dosyaları kontrol et
        QFileInfoList files = directory.entryInfoList(QStringList() << ffmpegExeName, QDir::Files);
        if (!files.isEmpty())
            return files.first().absoluteFilePath();

        // Sonra alt klasörlerde ara
        QFileInfoList subDirs = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &subDirInfo : subDirs) {
            QDir subDir(subDirInfo.absoluteFilePath());
            QString result = recursiveSearch(subDir);
            if (!result.isEmpty())
                return result;
        }
        return QString();
    };

    QString foundPath = recursiveSearch(dir);
    if (!foundPath.isEmpty()) {
        ffmpegPath = foundPath;
        return true;
    }

    return false;
}

bool MainWindow::checkFFmpegInSystem()
{
    QProcess process;
    process.start("ffmpeg", QStringList() << "-version");
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        ffmpegPath = "ffmpeg";
        return true;
    }
    return false;
}

void MainWindow::downloadFFmpeg()
{
    startDownload(currentDownloadUrl);
}

void MainWindow::startDownload(const QString& url)
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    reply = networkManager->get(request);

    updateStatus("Downloading ffmpeg started from: " + url);

    connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 received, qint64 total){
        if (total > 0) {
            int progress = static_cast<int>((received * 100) / total);
            ui->progressBar->setValue(progress);

            static int lastShown = 0;
            if (progress - lastShown >= 25 || progress == 100) {
                statusBar()->showMessage(QString("Downloading ffmpeg... %1%").arg(progress), 3000);
                lastShown = progress;
            }
        }
    });

    connect(reply, &QNetworkReply::finished, this, &MainWindow::downloadFinished);
}

void MainWindow::downloadFinished()
{
    if (reply->error())
    {
        updateStatus("Download failed from " + currentDownloadUrl + ": " + reply->errorString());
        reply->deleteLater();
        reply = nullptr;

        if (currentDownloadUrl == firstUrl)
        {
            currentDownloadUrl = fallbackUrl;
            updateStatus("Trying fallback download URL...");
            downloadFFmpeg();
            return;
        }
        else
        {
            updateStatus("Both download attempts failed.");
            setButtonsEnabled(false);
            return;
        }
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    reply = nullptr;

    QString appDir = QCoreApplication::applicationDirPath();
    QString zipPath = appDir + "/ffmpeg.zip";

    QFile file(zipPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        updateStatus("Cannot write file: " + zipPath);
        setButtonsEnabled(false);
        return;
    }

    file.write(data);
    file.close();

    updateStatus("Download complete. Extracting...");
    ui->progressBar->setValue(0);

    unzipFFmpeg();
}

void MainWindow::unzipFFmpeg()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString zipPath = appDir + "/ffmpeg.zip";

    QStringList args;
    args << "-Command"
         << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                .arg(zipPath).arg(extractPath);

    QProcess powershell;
    powershell.start("powershell.exe", args);
    powershell.waitForFinished(-1);

    QString output = powershell.readAllStandardOutput();
    QString error = powershell.readAllStandardError();

    if (!output.trimmed().isEmpty())
        logMessage("PowerShell unzip output: " + output.trimmed());

    if (!error.trimmed().isEmpty())
    {
        updateStatus("PowerShell unzip error: " + error.trimmed());
        setButtonsEnabled(false);
    }
    else
    {
        updateStatus("ffmpeg extracted.");

        if (findLocalFFmpeg())
        {
            updateStatus("ffmpeg ready at: " + ffmpegPath);
            ui->progressBar->setValue(100);
            setButtonsEnabled(true);
        }
        else
        {
            updateStatus("ffmpeg.exe not found after extraction!");
            setButtonsEnabled(false);
        }
    }
}

void MainWindow::on_btnSelectFile_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select File"));
    if (!fileName.isEmpty()) {
        inputFilePath = fileName;
        ui->lineEditFilePath->setText(fileName);
        updateStatus("File selected, ready to generate glitch video.");
    }
}

void MainWindow::on_btnGenerate_clicked()
{
    if (inputFilePath.isEmpty()) {
        QMessageBox::warning(this, "Warning", "You must select a file first.");
        updateStatus("No input file selected.");
        return;
    }

    QString outputFile = inputFilePath + "_glitch.mp4";

    QStringList arguments = {
        "-f", "rawvideo",
        "-pixel_format", "yuv420p",
        "-video_size", "640x480",
        "-framerate", "30",
        "-i", inputFilePath,
        "-y",
        outputFile
    };

    updateStatus("Generating video...");
    ui->progressBar->setValue(10);

    connect(&ffmpegProcess, &QProcess::finished, this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        ui->progressBar->setValue(100);
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            updateStatus("✅ Video created: " + outputFile);
        } else {
            QString err = ffmpegProcess.readAllStandardError();
            updateStatus("❌ ffmpeg error occurred.");
            logMessage("ffmpeg error:\n" + err);
        }
    });

    ffmpegProcess.start(ffmpegPath, arguments);
    if (!ffmpegProcess.waitForStarted()) {
        updateStatus("❌ ffmpeg failed to start.");
    }
}

void MainWindow::on_btnCheckFFmpeg_clicked()
{
    bool found = false;
    if (findLocalFFmpeg())
    {
        found = true;
        updateStatus("ffmpeg found locally at: " + ffmpegPath);
    }
    else if (checkFFmpegInSystem())
    {
        found = true;
        updateStatus("ffmpeg found in system PATH.");
    }
    else
    {
        updateStatus("ffmpeg NOT found.");
    }
    setButtonsEnabled(found);
}

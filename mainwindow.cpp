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
#include <QListWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QStyleFactory>

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
    if (!findLocalFFmpeg()) {
        updateStatus("ffmpeg NOT found locally. Checking system PATH...");
        if (!checkFFmpegInSystem()) {
            updateStatus("ffmpeg not found. Starting automatic download...");
            firstUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip";
            fallbackUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n4.4-latest-win64-gpl.zip";
            currentDownloadUrl = firstUrl;
            downloadFFmpeg();
        } else {
            updateStatus("ffmpeg found in system PATH.");
            hasFFmpeg = true;
        }
    } else {
        updateStatus("ffmpeg found locally.");
        hasFFmpeg = true;
    }
    setButtonsEnabled(hasFFmpeg);
    ui->progressBar->setValue(0);
    logMessage("Application started.");
    // Connect advanced UI
    connect(ui->btnAddFiles, &QPushButton::clicked, this, &MainWindow::on_btnAddFiles_clicked);
    connect(ui->btnRemoveFiles, &QPushButton::clicked, this, &MainWindow::on_btnRemoveFiles_clicked);
    connect(ui->btnSelectOutputFile, &QPushButton::clicked, this, &MainWindow::on_btnSelectOutputFile_clicked);
    connect(ui->btnPreview, &QPushButton::clicked, this, &MainWindow::on_btnPreview_clicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &MainWindow::on_btnCancel_clicked);
    connect(ui->comboResolution, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::on_comboResolution_currentIndexChanged);
    connect(ui->comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::on_comboTheme_currentIndexChanged);
    applyTheme(ui->comboTheme->currentText());
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

void MainWindow::on_btnAddFiles_clicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files"));
    for (const QString& file : files) {
        if (!batchInputFiles.contains(file)) {
            batchInputFiles << file;
            ui->listWidgetBatchFiles->addItem(file);
        }
    }
    updateStatus("Batch files updated.");
}

void MainWindow::on_btnRemoveFiles_clicked() {
    QList<QListWidgetItem*> selected = ui->listWidgetBatchFiles->selectedItems();
    for (QListWidgetItem* item : selected) {
        batchInputFiles.removeAll(item->text());
        delete item;
    }
    updateStatus("Selected files removed from batch.");
}

void MainWindow::on_btnSelectOutputFile_clicked() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Select Output File"), "output.mp4", tr("MP4 Files (*.mp4)"));
    if (!fileName.isEmpty()) {
        outputFilePath = fileName;
        ui->lineEditOutputFile->setText(fileName);
        updateStatus("Output file set.");
    }
}

void MainWindow::on_comboResolution_currentIndexChanged(int index) {
    if (ui->comboResolution->itemText(index) == "Custom...") {
        ui->lineEditCustomResolution->setEnabled(true);
    } else {
        ui->lineEditCustomResolution->setEnabled(false);
    }
}

void MainWindow::on_comboTheme_currentIndexChanged(int index) {
    Q_UNUSED(index);
    applyTheme(ui->comboTheme->currentText());
}

void MainWindow::applyTheme(const QString& themeName) {
    if (themeName == "Dark") {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53,53,53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25,25,25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53,53,53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53,53,53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);
    } else {
        qApp->setPalette(style()->standardPalette());
    }
}

void MainWindow::on_btnPreview_clicked() {
    QString file = inputFilePath;
    if (file.isEmpty()) {
        QMessageBox::information(this, "Preview", "No input file selected.");
        return;
    }
    QString previewFile = file + "_glitch_preview.mp4";
    processSingleFile(file, previewFile, true);
    QTimer::singleShot(2000, [=]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(previewFile));
    });
}

void MainWindow::on_btnCancel_clicked() {
    cancelProcessing();
}

void MainWindow::cancelProcessing() {
    if (ffmpegProcess.state() == QProcess::Running) {
        ffmpegProcess.kill();
        updateStatus("Process cancelled by user.");
        isProcessing = false;
        ui->progressBar->setValue(0);
    }
}

void MainWindow::on_btnGenerate_clicked()
{
    if (!batchInputFiles.isEmpty()) {
        isBatchProcessing = true;
        processFiles(batchInputFiles);
    } else {
        isBatchProcessing = false;
        if (inputFilePath.isEmpty()) {
            QMessageBox::warning(this, "Warning", "You must select a file first.");
            updateStatus("No input file selected.");
            return;
        }
        QString outFile = outputFilePath.isEmpty() ? inputFilePath + "_glitch.mp4" : outputFilePath;
        processSingleFile(inputFilePath, outFile);
    }
}

void MainWindow::processFiles(const QStringList& files) {
    if (files.isEmpty()) return;
    int total = files.size();
    int* processed = new int(0);
    for (const QString& file : files) {
        QString outFile = file + "_glitch.mp4";
        processSingleFile(file, outFile);
        (*processed)++;
        ui->progressBar->setValue((*processed) * 100 / total);
    }
    updateStatus("Batch processing completed.");
    delete processed;
}

void MainWindow::processSingleFile(const QString& inputFile, const QString& outputFile, bool preview) {
    // ffmpeg parametrelerini gelişmiş ayarlara göre oluştur
    QString resolution = ui->comboResolution->currentText();
    if (resolution == "Custom...") {
        resolution = ui->lineEditCustomResolution->text();
    }
    QString pixelFormat = ui->comboPixelFormat->currentText();
    int framerate = ui->spinFramerate->value();
    QString codec = ui->comboCodec->currentText();
    double brightness = ui->spinBrightness->value();
    double contrast = ui->spinContrast->value();
    QStringList arguments = {
        "-f", "rawvideo",
        "-pixel_format", pixelFormat,
        "-video_size", resolution,
        "-framerate", QString::number(framerate),
        "-i", inputFile,
        "-vf", QString("eq=brightness=%1:contrast=%2").arg(brightness).arg(contrast),
        "-c:v", codec,
        "-y",
        outputFile
    };
    updateStatus(preview ? "Generating preview..." : "Generating video...");
    ui->progressBar->setValue(10);
    isProcessing = true;
    connect(&ffmpegProcess, &QProcess::finished, this, [=](int exitCode, QProcess::ExitStatus exitStatus){
        ui->progressBar->setValue(100);
        isProcessing = false;
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            updateStatus(preview ? "Preview created." : ("✅ Video created: " + outputFile));
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

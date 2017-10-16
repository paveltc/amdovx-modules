#include "inference_control.h"
#include "inference_compiler.h"
#include "inference_comm.h"
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QIntValidator>
#include <QMessageBox>
#include <QFileInfo>
#include <QFrame>
#include <QTcpSocket>
#include <QTimer>
#include <QCheckBox>
#include <QStyle>
#include <QDesktopServices>

#define CONFIGURATION_CACHE_FILENAME ".inference_control.txt"
#define BUILD_VERSION "alpha2"

inference_control::inference_control(int operationMode_, QWidget *parent)
    : QWidget(parent), connectionSuccessful{ false }, modelType{ 0 }, numModelTypes{ 0 }
{
    setWindowTitle("Inference Control");
    setMinimumWidth(800);

    maxGPUs = 1;
    compiler_status.completed = false;
    compiler_status.dimOutput[0] = 0;
    compiler_status.dimOutput[1] = 0;
    compiler_status.dimOutput[2] = 0;
    compiler_status.errorCode = 0;
    operationMode = operationMode_;

    // default configuration
    QGridLayout * controlLayout = new QGridLayout;
    int editSpan = 3;
    int row = 0;

    //////////////
    /// \brief labelIntro
    ///
    QLabel * labelIntro = new QLabel("INFERENCE CONTROL PANEL");
    labelIntro->setStyleSheet("font-weight: bold; color: green");
    labelIntro->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QPushButton * buttonLogo = new QPushButton();
    QPixmap pixmap;
    extern unsigned int logoBufPNG[], logoSizePNG;
    QByteArray arr((const char *)logoBufPNG, logoSizePNG);
    pixmap.loadFromData(arr);
    buttonLogo->setIcon(pixmap);
    buttonLogo->setIconSize(QSize(64,64));
    buttonLogo->setFixedSize(QSize(64,64));
    controlLayout->addWidget(buttonLogo, row, 1 + editSpan, 1, 1);
    controlLayout->addWidget(labelIntro, row, 0, 1, editSpan + 1);
    connect(buttonLogo, SIGNAL(released()), this, SLOT(onLogoClick()));
    row++;

    QFrame * sepHLine1 = new QFrame();
    sepHLine1->setFrameShape(QFrame::HLine);
    sepHLine1->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine1, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelServer
    ///
    QLabel * labelServer = new QLabel("Inference Server");
    labelServer->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelServer, row, 0, 1, 5);
    row++;

    QLabel * labelServerHost = new QLabel("Server:");
    editServerHost = new QLineEdit("localhost");
    editServerPort = new QLineEdit("28282");
    buttonConnect = new QPushButton("Connect");
    editServerPort->setValidator(new QIntValidator(1,65535));
    labelServerHost->setStyleSheet("font-weight: bold; font-style: italic");
    labelServerHost->setAlignment(Qt::AlignLeft);
    connect(buttonConnect, SIGNAL(released()), this, SLOT(connectServer()));
    controlLayout->addWidget(labelServerHost, row, 0, 1, 1);
    controlLayout->addWidget(editServerHost, row, 1, 1, 2);
    controlLayout->addWidget(editServerPort, row, editSpan, 1, 1);
    controlLayout->addWidget(buttonConnect, row, 1 + editSpan, 1, 1);
    row++;
    labelServerStatus = new QLabel("");
    labelServerStatus->setStyleSheet("font-style: italic");
    labelServerStatus->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelServerStatus, row, 1, 1, editSpan + 1);
    row++;

    QFrame * sepHLine2 = new QFrame();
    sepHLine2->setFrameShape(QFrame::HLine);
    sepHLine2->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine2, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelCompiler
    ///
    typeModelFile1Label.push_back("Prototxt:");
    typeModelFile2Label.push_back("CaffeModel:");
    typeModelFile1Desc.push_back("Prototxt (*.prototxt)");
    typeModelFile2Desc.push_back("CaffeModel (*.caffemodel)");
    numModelTypes++;
    QLabel * labelCompiler = new QLabel("Inference Compiler");
    labelCompiler->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelCompiler, row, 0, 1, 5);
    row++;
    QLabel * labelModel = new QLabel("CNN Model:");
    comboModelSelect = new QComboBox();
    buttonModelUpload = new QPushButton(tr("Upload"), this);
    comboModelSelect->addItem("Upload a pre-trained Caffe model (i.e., .prototxt and .caffemodel)");
    labelModel->setStyleSheet("font-weight: bold; font-style: italic");
    labelModel->setAlignment(Qt::AlignLeft);
    connect(comboModelSelect, SIGNAL(activated(int)), this, SLOT(modelSelect(int)));
    connect(buttonModelUpload, SIGNAL(released()), this, SLOT(modelUpload()));
    controlLayout->addWidget(labelModel, row, 0, 1, 1);
    controlLayout->addWidget(comboModelSelect, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonModelUpload, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelInputDim = new QLabel("CxHxW(inp):");
    QLineEdit * editDimC = new QLineEdit("3");
    editDimH = new QLineEdit("224");
    editDimW = new QLineEdit("224");
    editDimC->setValidator(new QIntValidator(3,3));
    editDimH->setValidator(new QIntValidator(1,16384));
    editDimW->setValidator(new QIntValidator(1,16384));
    editDimC->setEnabled(false);
    labelInputDim->setStyleSheet("font-weight: bold; font-style: italic");
    labelInputDim->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelInputDim, row, 0, 1, 1);
    controlLayout->addWidget(editDimC, row, 1, 1, 1);
    controlLayout->addWidget(editDimH, row, 2, 1, 1);
    controlLayout->addWidget(editDimW, row, 3, 1, 1);
    row++;
    QLabel * labelOutputDim = new QLabel("CxHxW(out):");
    editOutDimC = new QLineEdit("");
    editOutDimH = new QLineEdit("");
    editOutDimW = new QLineEdit("");
    editOutDimC->setEnabled(false);
    editOutDimH->setEnabled(false);
    editOutDimW->setEnabled(false);
    labelOutputDim->setStyleSheet("font-weight: bold; font-style: italic");
    labelOutputDim->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelOutputDim, row, 0, 1, 1);
    controlLayout->addWidget(editOutDimC, row, 1, 1, 1);
    controlLayout->addWidget(editOutDimH, row, 2, 1, 1);
    controlLayout->addWidget(editOutDimW, row, 3, 1, 1);
    row++;
    labelModelFile1 = new QLabel("--");
    editModelFile1 = new QLineEdit("");
    buttonModelFile1 = new QPushButton(tr("Browse..."), this);
    connect(buttonModelFile1, &QAbstractButton::clicked, this, &inference_control::browseModelFile1);
    labelModelFile1->setStyleSheet("font-weight: bold; font-style: italic");
    labelModelFile1->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelModelFile1, row, 0, 1, 1);
    controlLayout->addWidget(editModelFile1, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonModelFile1, row, 1 + editSpan, 1, 1);
    row++;
    labelModelFile2 = new QLabel("--");
    editModelFile2 = new QLineEdit("");
    buttonModelFile2 = new QPushButton(tr("Browse..."), this);
    connect(buttonModelFile2, &QAbstractButton::clicked, this, &inference_control::browseModelFile2);
    labelModelFile2->setStyleSheet("font-weight: bold; font-style: italic");
    labelModelFile2->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelModelFile2, row, 0, 1, 1);
    controlLayout->addWidget(editModelFile2, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonModelFile2, row, 1 + editSpan, 1, 1);
    row++;
    labelCompilerOptions = new QLabel("--");
    editCompilerOptions = new QLineEdit("");
    labelCompilerOptions->setStyleSheet("font-weight: bold; font-style: italic");
    labelCompilerOptions->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelCompilerOptions, row, 0, 1, 1);
    controlLayout->addWidget(editCompilerOptions, row, 1, 1, editSpan);
    row++;
    connect(editModelFile1, SIGNAL(textChanged(const QString &)), this, SLOT(onChangeModelFile1(const QString &)));
    connect(editModelFile2, SIGNAL(textChanged(const QString &)), this, SLOT(onChangeModelFile2(const QString &)));
    connect(editCompilerOptions, SIGNAL(textChanged(const QString &)), this, SLOT(onChangeCompilerOptions(const QString &)));

    QFrame * sepHLine3 = new QFrame();
    sepHLine3->setFrameShape(QFrame::HLine);
    sepHLine3->setFrameShadow(QFrame::Sunken);
    controlLayout->addWidget(sepHLine3, row, 0, 1, editSpan + 2);
    row++;

    //////////////
    /// \brief labelRuntime
    ///
    QLabel * labelRuntime = new QLabel("Inference Run-time");
    labelRuntime->setStyleSheet("font-weight: bold; color: red");
    controlLayout->addWidget(labelRuntime, row, 0, 1, 5);
    row++;
    QLabel * labelGPUs = new QLabel("GPUs:");
    editGPUs = new QLineEdit("1");
    labelMaxGPUs = new QLabel("");
    buttonRunInference = new QPushButton("Run");
    editGPUs->setValidator(new QIntValidator(1,maxGPUs));
    editGPUs->setEnabled(false);
    labelGPUs->setStyleSheet("font-weight: bold; font-style: italic");
    labelGPUs->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelGPUs, row, 0, 1, 1);
    controlLayout->addWidget(editGPUs, row, 1, 1, 1);
    controlLayout->addWidget(labelMaxGPUs, row, 2, 1, 1);
    controlLayout->addWidget(buttonRunInference, row, 1 + editSpan, 1, 1);
    connect(buttonRunInference, SIGNAL(released()), this, SLOT(runInference()));
    row++;
    QLabel * labelImageLabelsFile = new QLabel("Labels:");
    editImageLabelsFile = new QLineEdit("");
    QPushButton * buttonDatasetLabels = new QPushButton(tr("Browse..."), this);
    connect(buttonDatasetLabels, &QAbstractButton::clicked, this, &inference_control::browseDatasetLabels);
    labelImageLabelsFile->setStyleSheet("font-weight: bold; font-style: italic");
    labelImageLabelsFile->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelImageLabelsFile, row, 0, 1, 1);
    controlLayout->addWidget(editImageLabelsFile, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDatasetLabels, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelImageFolder = new QLabel("Image Folder:");
    editImageFolder = new QLineEdit("");
    QPushButton * buttonDatasetFolder = new QPushButton(tr("Browse..."), this);
    connect(buttonDatasetFolder, &QAbstractButton::clicked, this, &inference_control::browseDatasetFolder);
    labelImageFolder->setStyleSheet("font-weight: bold; font-style: italic");
    labelImageFolder->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelImageFolder, row, 0, 1, 1);
    controlLayout->addWidget(editImageFolder, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDatasetFolder, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelImageList = new QLabel("Image List:");
    editImageListFile = new QLineEdit("");
    QPushButton * buttonDatasetFilename = new QPushButton(tr("Browse..."), this);
    connect(buttonDatasetFilename, &QAbstractButton::clicked, this, &inference_control::browseDatasetFilename);
    labelImageList->setStyleSheet("font-weight: bold; font-style: italic");
    labelImageList->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelImageList, row, 0, 1, 1);
    controlLayout->addWidget(editImageListFile, row, 1, 1, editSpan);
    controlLayout->addWidget(buttonDatasetFilename, row, 1 + editSpan, 1, 1);
    row++;
    QLabel * labelMaxDatasetSize = new QLabel("Image Count:");
    editMaxDatasetSize = new QLineEdit("");
    editMaxDatasetSize->setValidator(new QIntValidator(1,1000000000));
    labelMaxDatasetSize->setStyleSheet("font-weight: bold; font-style: italic");
    labelMaxDatasetSize->setAlignment(Qt::AlignLeft);
    controlLayout->addWidget(labelMaxDatasetSize, row, 0, 1, 1);
    controlLayout->addWidget(editMaxDatasetSize, row, 1, 1, 1);
    checkRepeatImages = nullptr;
    if(operationMode) {
        checkRepeatImages = new QCheckBox("Repeat Images");
        checkRepeatImages->setChecked(true);
        controlLayout->addWidget(checkRepeatImages, row, 2, 1, 1);
    }
    row++;

    QPushButton * exitButton = new QPushButton("Exit");
    controlLayout->addWidget(exitButton, row, (1 + editSpan)/2, 1, 1);
    connect(exitButton, SIGNAL(released()), this, SLOT(exitControl()));
    row++;

    setLayout(controlLayout);

    // activate based on configuration
    loadConfig();
    modelSelect(comboModelSelect->currentIndex());

    // start timer for update
    QTimer *timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
    timer->start(1000);
}

void inference_control::saveConfig()
{
    // save configuration
    QString homeFolder = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0];
    QFile fileObj(homeFolder + "/" + CONFIGURATION_CACHE_FILENAME);
    if(fileObj.open(QIODevice::WriteOnly)) {
        QTextStream fileOutput(&fileObj);
        fileOutput << BUILD_VERSION << endl;
        fileOutput << editServerHost->text() << endl;
        fileOutput << editServerPort->text() << endl;
        fileOutput << lastModelFile1 << endl;
        fileOutput << lastModelFile2 << endl;
        fileOutput << lastDimH << endl;
        fileOutput << lastDimW << endl;
        fileOutput << editGPUs->text() << endl;
        fileOutput << lastCompilerOptions << endl;
        fileOutput << editImageLabelsFile->text() << endl;
        fileOutput << editImageListFile->text() << endl;
        fileOutput << editImageFolder->text() << endl;
        fileOutput << editMaxDatasetSize->text() << endl;
    }
    fileObj.close();
}

void inference_control::loadConfig()
{
    // load default configuration
    QString homeFolder = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0];
    QFile fileObj(homeFolder + "/" + CONFIGURATION_CACHE_FILENAME);
    if(fileObj.open(QIODevice::ReadOnly)) {
        QTextStream fileInput(&fileObj);
        QString version = fileInput.readLine();
        if(version == BUILD_VERSION) {
            editServerHost->setText(fileInput.readLine());
            editServerPort->setText(fileInput.readLine());
            editModelFile1->setText(fileInput.readLine());
            editModelFile2->setText(fileInput.readLine());
            editDimH->setText(fileInput.readLine());
            editDimW->setText(fileInput.readLine());
            editGPUs->setText(fileInput.readLine());
            editCompilerOptions->setText(fileInput.readLine());
            editImageLabelsFile->setText(fileInput.readLine());
            editImageListFile->setText(fileInput.readLine());
            editImageFolder->setText(fileInput.readLine());
            editMaxDatasetSize->setText(fileInput.readLine());
        }
    }
    fileObj.close();
    // save last options
    lastDimW = editDimW->text();
    lastDimH = editDimH->text();
    lastModelFile1 = editModelFile1->text();
    lastModelFile2 = editModelFile2->text();
    lastCompilerOptions = editCompilerOptions->text();
}

bool inference_control::isConfigValid(QString& err)
{
    if(editServerPort->text().toInt() <= 0) { err = "Server: invalid port number."; return false; }
    if(comboModelSelect->currentIndex() < numModelTypes) {
        if(!QFileInfo(editModelFile1->text()).isFile()) { err = typeModelFile1Label[comboModelSelect->currentIndex()] + editModelFile1->text() + " file doesn't exist."; return false; }
        if(!QFileInfo(editModelFile2->text()).isFile()) { err = typeModelFile2Label[comboModelSelect->currentIndex()] + editModelFile2->text() + " file doesn't exist."; return false; }
    }
    if(editDimW->text().toInt() <= 0) { err = "Dimensions: width must be positive."; return false; }
    if(editDimH->text().toInt() <= 0) { err = "Dimensions: height must be positive."; return false; }
    if(editGPUs->text().toInt() <= 0) { err = "GPUs: must be positive."; return false; }
    if(editMaxDatasetSize->text().toInt() < 0) { err = "Image Count: must be non-negative."; return false; }
    return true;
}

void inference_control::modelSelect(int model)
{
    QString text;
    bool compilationCompleted = (compiler_status.errorCode > 0) && compiler_status.completed;
    int dimOutput[3] = { compiler_status.dimOutput[0], compiler_status.dimOutput[1], compiler_status.dimOutput[2] };
    if(model < numModelTypes) {
        // input dimensions
        editDimW->setDisabled(false);
        editDimH->setDisabled(false);
        // model file selection
        buttonModelUpload->setEnabled(false);
        if(connectionSuccessful && editModelFile1->text().length() > 0 && editModelFile2->text().length() > 0) {
            buttonModelUpload->setEnabled(true);
        }
        labelModelFile1->setText(typeModelFile1Label[model]);
        editModelFile1->setText(lastModelFile1);
        editModelFile1->setEnabled(true);
        buttonModelFile1->setEnabled(true);
        labelModelFile2->setText(typeModelFile2Label[model]);
        editModelFile2->setText(lastModelFile2);
        editModelFile2->setEnabled(true);
        buttonModelFile2->setEnabled(true);
        labelCompilerOptions->setText("Options:");
        editCompilerOptions->setReadOnly(false);
        editCompilerOptions->setText(lastCompilerOptions);
    }
    else {
        model -= numModelTypes;
        // already compiled
        compilationCompleted = true;
        dimOutput[0] = modelList[model].outputDim[0];
        dimOutput[1] = modelList[model].outputDim[1];
        dimOutput[2] = modelList[model].outputDim[2];
        // input & output dimensions
        editDimW->setDisabled(true);
        editDimH->setDisabled(true);
        editDimW->setText(text.sprintf("%d", modelList[model].inputDim[0]));
        editDimH->setText(text.sprintf("%d", modelList[model].inputDim[1]));
        // model file selection
        labelModelFile1->setText("--");
        editModelFile1->setEnabled(false);
        editModelFile1->setText("");
        buttonModelFile1->setEnabled(false);
        labelModelFile2->setText("--");
        editModelFile2->setEnabled(false);
        editModelFile2->setText("");
        buttonModelFile2->setEnabled(false);
        buttonModelUpload->setEnabled(false);
        labelCompilerOptions->setText("--");
        editCompilerOptions->setReadOnly(true);
        editCompilerOptions->setText("");
    }
    // output dimensions
    editOutDimW->setText(dimOutput[0] == 0 ? "" : text.sprintf("%d", dimOutput[0]));
    editOutDimH->setText(dimOutput[1] == 0 ? "" : text.sprintf("%d", dimOutput[1]));
    editOutDimC->setText(dimOutput[2] == 0 ? "" : text.sprintf("%d", dimOutput[2]));
    // enable GPUs
    editGPUs->setEnabled(compilationCompleted);
    // enable run button
    buttonRunInference->setEnabled(false);
    if(compilationCompleted && dimOutput[0] > 0 && dimOutput[1] > 0 && dimOutput[2] > 0 &&
       editImageLabelsFile->text().length() > 0 && editImageFolder->text().length() > 0)
    {
        buttonRunInference->setEnabled(true);
    }
}

void inference_control::tick()
{
    modelSelect(comboModelSelect->currentIndex());
}

void inference_control::onChangeModelFile1(const QString & text)
{
    if(comboModelSelect->currentIndex() == 0) {
        lastModelFile1 =  text;
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::onChangeModelFile2(const QString & text)
{
    if(comboModelSelect->currentIndex() == 0) {
        lastModelFile2 =  text;
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::onChangeCompilerOptions(const QString & text)
{
    if(comboModelSelect->currentIndex() == 0)
        lastCompilerOptions =  text;
}

void inference_control::browseModelFile1()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, typeModelFile1Desc[modelType]);
    if(fileName.size() > 0) {
        editModelFile1->setText(fileName);
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::browseModelFile2()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), nullptr, typeModelFile2Desc[modelType]);
    if(fileName.size() > 0) {
        editModelFile2->setText(fileName);
        modelSelect(comboModelSelect->currentIndex());
    }
}

void inference_control::browseDatasetLabels()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Labels File"), nullptr, tr("Labels Text (*.txt)"));
    if(fileName.size() > 0)
        editImageLabelsFile->setText(fileName);
}

void inference_control::browseDatasetFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Image Folder"), nullptr,
                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.size() > 0)
        editImageFolder->setText(dir);
}

void inference_control::browseDatasetFilename()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image List File"), nullptr, tr("Image List Text (*.txt)"));
    if(fileName.size() > 0)
        editImageListFile->setText(fileName);
}

void inference_control::exitControl()
{
    close();
}

void inference_control::connectServer()
{
    // check configuration
    QString err;
    if(editServerHost->text().length() <= 0 || editServerPort->text().toInt() <= 0) {
        QMessageBox::critical(this, windowTitle(), "Server host/port is not valid", QMessageBox::Ok);
        return;
    }
    // save configuration
    saveConfig();

    // check TCP connection
    QTcpSocket * tcpSocket = new QTcpSocket(this);
    tcpSocket->connectToHost(editServerHost->text(), editServerPort->text().toInt());
    QString status;
    connectionSuccessful = false;
    status = "ERROR: Unable to connect to " + editServerHost->text() + ":" + editServerPort->text();
    if(tcpSocket->waitForConnected(3000)) {
        int pendingModelCount = 0;
        while(tcpSocket->state() == QAbstractSocket::ConnectedState) {
            bool receivedCommand = false;
            if(tcpSocket->waitForReadyRead()) {
                InfComCommand cmd;
                while(tcpSocket->bytesAvailable() >= (qint64)sizeof(cmd) &&
                      tcpSocket->read((char *)&cmd, sizeof(cmd)) == sizeof(cmd))
                {
                    receivedCommand = true;
                    if(cmd.magic != INFCOM_MAGIC) {
                        status.sprintf("ERROR: got invalid magic 0x%08x", cmd.magic);
                        break;
                    }
                    auto send = [](QTcpSocket * sock, QString& status, const void * buf, size_t len) -> bool {
                        sock->write((const char *)buf, len);
                        if(!sock->waitForBytesWritten(3000)) {
                            status.sprintf("ERROR: write(%ld) failed", len);
                            return false;
                        }
                        return true;
                    };
                    if(cmd.command == INFCOM_CMD_DONE) {
                        break;
                    }
                    else if(cmd.command == INFCOM_CMD_SEND_MODE) {
                        InfComCommand reply = {
                            INFCOM_MAGIC, INFCOM_CMD_SEND_MODE,
                            { INFCOM_MODE_CONFIGURE },
                            { 0 }
                        };
                        if(!send(tcpSocket, status, &reply, sizeof(reply)))
                            break;
                    }
                    else if(cmd.command == INFCOM_CMD_CONFIG_INFO) {
                        pendingModelCount = cmd.data[0];
                        maxGPUs = cmd.data[1];
                        QString text;
                        editGPUs->setText(text.sprintf("%d", maxGPUs));
                        labelMaxGPUs->setText(text.sprintf("(upto %d)", maxGPUs));
                        while(comboModelSelect->count() > 1)
                            comboModelSelect->removeItem(1);
                        modelList.clear();
                        connectionSuccessful = true;
                        status = "OK: Connected to " + editServerHost->text() + ":" + editServerPort->text();
                        if(pendingModelCount <= 0) {
                            InfComCommand reply = {
                                INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
                            };
                            if(!send(tcpSocket, status, &reply, sizeof(reply)))
                                break;
                        }
                    }
                    else if(cmd.command == INFCOM_CMD_MODEL_INFO) {
                        InfComModelInfo info = { { 0 }, { 0 }, { 0 } };
                        info.inputDim[0] = cmd.data[0];
                        info.inputDim[1] = cmd.data[1];
                        info.inputDim[2] = cmd.data[2];
                        info.outputDim[0] = cmd.data[3];
                        info.outputDim[1] = cmd.data[4];
                        info.outputDim[2] = cmd.data[5];
                        strncpy(info.name, cmd.message, sizeof(info.name));
                        modelList.push_back(info);
                        comboModelSelect->addItem(info.name);
                        pendingModelCount--;
                        if(pendingModelCount <= 0) {
                            InfComCommand reply = {
                                INFCOM_MAGIC, INFCOM_CMD_DONE, { 0 }, { 0 }
                            };
                            if(!send(tcpSocket, status, &reply, sizeof(reply)))
                                break;
                        }
                    }
                    else {
                        status.sprintf("ERROR: got invalid command received 0x%08x", cmd.command);
                        break;
                    }
                }
            }
            if(!receivedCommand) {
                QThread::msleep(2);
            }
        }
    }
    tcpSocket->close();
    labelServerStatus->setText(status);

    // update status
    if(comboModelSelect->currentIndex() > modelList.length()) {
        comboModelSelect->setCurrentIndex(modelList.length() - 1);
    }
    modelSelect(comboModelSelect->currentIndex());
}

void inference_control::modelUpload()
{
    // check configuration
    QString err;
    if(!isConfigValid(err)) {
        QMessageBox::critical(this, windowTitle(), err, QMessageBox::Ok);
        return;
    }
    buttonModelUpload->setEnabled(false);

    // save configuration
    saveConfig();

    // start compiler
    inference_compiler * compiler = new inference_compiler(
                true,
                editServerHost->text(), editServerPort->text().toInt(),
                3,
                editDimH->text().toInt(),
                editDimW->text().toInt(),
                editModelFile1->text(), editModelFile2->text(),
                editCompilerOptions->text(),
                &compiler_status);
    compiler->show();
}

void inference_control::runInference()
{
    // check configuration
    QString err;
    if(isConfigValid(err)) {
        if(!QFileInfo(editImageLabelsFile->text()).isFile())
            err = "Labels: file doesn't exist: " + editImageLabelsFile->text();
        else if(!QFileInfo(editImageFolder->text()).isDir())
            err = "Image Folder: doesn't exist: " + editImageFolder->text();
    }
    if(err.length() > 0) {
        QMessageBox::critical(this, windowTitle(), err, QMessageBox::Ok);
        return;
    }

    // save configuration
    saveConfig();

    // start viewer
    QString modelName = comboModelSelect->currentText();
    if(comboModelSelect->currentIndex() < numModelTypes) {
        modelName = compiler_status.message;
    }
    int dimInput[3] = { editDimW->text().toInt(), editDimH->text().toInt(), 3 };
    int dimOutput[3] = { editOutDimW->text().toInt(), editOutDimH->text().toInt(), editOutDimC->text().toInt() };
    inference_viewer * viewer = new inference_viewer(
                editServerHost->text(), editServerPort->text().toInt(), modelName,
                editImageLabelsFile->text(), editImageListFile->text(), editImageFolder->text(),
                dimInput, editGPUs->text().toInt(), dimOutput, editMaxDatasetSize->text().toInt(),
                checkRepeatImages ? checkRepeatImages->checkState() : false);
    viewer->show();
    close();
}

void inference_control::onLogoClick()
{
    qDebug("clicked");
    QDesktopServices::openUrl(QUrl("https://instinct.radeon.com/en/"));
}

unsigned int logoSizePNG = 5311;
unsigned int logoBufPNG[] = {
    0x474e5089, 0x0a1a0a0d, 0x0d000000, 0x52444849,
    0x9b020000, 0x9b020000, 0x00000608, 0x4192ba00,
    0x00000064, 0x58457419, 0x666f5374, 0x72617774,
    0x64410065, 0x2065626f, 0x67616d49, 0x61655265,
    0xc9717964, 0x00003c65, 0x54692803, 0x4d587458,
    0x6f633a4c, 0x64612e6d, 0x2e65626f, 0x00706d78,
    0x00000000, 0x70783f3c, 0x656b6361, 0x65622074,
    0x3d6e6967, 0xbfbbef22, 0x64692022, 0x3557223d,
    0x704d304d, 0x69686543, 0x65727a48, 0x544e7a53,
    0x636b7a63, 0x3f226439, 0x783c203e, 0x706d783a,
    0x6174656d, 0x6c6d7820, 0x783a736e, 0x6461223d,
    0x3a65626f, 0x6d3a736e, 0x2f617465, 0x3a782022,
    0x74706d78, 0x41223d6b, 0x65626f64, 0x504d5820,
    0x726f4320, 0x2e352065, 0x31632d36, 0x37203233,
    0x35312e39, 0x34383239, 0x3032202c, 0x302f3631,
    0x39312f34, 0x3a33312d, 0x343a3331, 0x20202030,
    0x20202020, 0x203e2220, 0x6664723c, 0x4644523a,
    0x6c6d7820, 0x723a736e, 0x223d6664, 0x70747468,
    0x772f2f3a, 0x772e7777, 0x726f2e33, 0x39312f67,
    0x302f3939, 0x32322f32, 0x6664722d, 0x6e79732d,
    0x2d786174, 0x2223736e, 0x723c203e, 0x443a6664,
    0x72637365, 0x69747069, 0x72206e6f, 0x613a6664,
    0x74756f62, 0x2022223d, 0x6e6c6d78, 0x6d783a73,
    0x68223d70, 0x3a707474, 0x736e2f2f, 0x6f64612e,
    0x632e6562, 0x782f6d6f, 0x312f7061, 0x222f302e,
    0x6c6d7820, 0x783a736e, 0x4d4d706d, 0x7468223d,
    0x2f3a7074, 0x2e736e2f, 0x626f6461, 0x6f632e65,
    0x61782f6d, 0x2e312f70, 0x6d6d2f30, 0x7820222f,
    0x736e6c6d, 0x5274733a, 0x223d6665, 0x70747468,
    0x6e2f2f3a, 0x64612e73, 0x2e65626f, 0x2f6d6f63,
    0x2f706178, 0x2f302e31, 0x70795473, 0x65522f65,
    0x72756f73, 0x65526563, 0x20222366, 0x3a706d78,
    0x61657243, 0x54726f74, 0x3d6c6f6f, 0x6f644122,
    0x50206562, 0x6f746f68, 0x706f6873, 0x20434320,
    0x35313032, 0x2820352e, 0x646e6957, 0x2973776f,
    0x6d782022, 0x3a4d4d70, 0x74736e49, 0x65636e61,
    0x223d4449, 0x2e706d78, 0x3a646969, 0x39363336,
    0x44313135, 0x46333135, 0x37453131, 0x36383441,
    0x34303139, 0x30463043, 0x42313342, 0x6d782022,
    0x3a4d4d70, 0x75636f44, 0x746e656d, 0x223d4449,
    0x2e706d78, 0x3a646964, 0x39363336, 0x45313135,
    0x46333135, 0x37453131, 0x36383441, 0x34303139,
    0x30463043, 0x42313342, 0x3c203e22, 0x4d706d78,
    0x65443a4d, 0x65766972, 0x6f724664, 0x7473206d,
    0x3a666552, 0x74736e69, 0x65636e61, 0x223d4449,
    0x2e706d78, 0x3a646969, 0x39363336, 0x42313135,
    0x46333135, 0x37453131, 0x36383441, 0x34303139,
    0x30463043, 0x42313342, 0x74732022, 0x3a666552,
    0x75636f64, 0x746e656d, 0x223d4449, 0x2e706d78,
    0x3a646964, 0x39363336, 0x43313135, 0x46333135,
    0x37453131, 0x36383441, 0x34303139, 0x30463043,
    0x42313342, 0x203e2f22, 0x64722f3c, 0x65443a66,
    0x69726373, 0x6f697470, 0x3c203e6e, 0x6664722f,
    0x4644523a, 0x2f3c203e, 0x6d783a78, 0x74656d70,
    0x3c203e61, 0x6170783f, 0x74656b63, 0x646e6520,
    0x2272223d, 0x37933e3f, 0x00008efd, 0x44492d11,
    0xda785441, 0x6dc1ddec, 0x8615d91c, 0x7b432bd1,
    0x766bd369, 0xc80ccd06, 0x808e380c, 0x1950114f,
    0x66811db0, 0x83397022, 0x0ca50676, 0xe7bc0ce8,
    0xae808a46, 0x01b2b902, 0x70bb6303, 0x01cfbf8b,
    0x857f661e, 0xafd37cc1, 0x9787df74, 0x00029797,
    0xc1fc2d80, 0x10000008, 0x8800009b, 0x1000004d,
    0x8800009b, 0xc400004d, 0x88000026, 0xc400004d,
    0x62000026, 0xc4000013, 0x62000026, 0xb1000013,
    0x62000009, 0xb1000013, 0xd8800009, 0xb1000004,
    0xd8800009, 0x6c400004, 0xd8800002, 0x6c400004,
    0x36200002, 0x6c400001, 0x36200002, 0x9b100001,
    0x36200000, 0x9b100001, 0x4d880000, 0x9b100000,
    0x4d880000, 0x9b100000, 0x4d880000, 0x26c40000,
    0x4d880000, 0x26c40000, 0x13620000, 0x26c40000,
    0x13620000, 0x09b10000, 0x13620000, 0x09b10000,
    0x04d88000, 0x09b10000, 0x04d88000, 0x026c4000,
    0x04d88000, 0x026c4000, 0x01362000, 0x026c4000,
    0x01362000, 0x009b1000, 0x01362000, 0x009b1000,
    0x004d8800, 0x009b1000, 0x004d8800, 0x0026c400,
    0x004d8800, 0x0026c400, 0x004d8800, 0x0026c400,
    0x00136200, 0x0026c400, 0x00136200, 0x0009b100,
    0x00136200, 0x0009b100, 0x0004d880, 0x0009b100,
    0x0004d880, 0x00026c40, 0x0004d880, 0x00026c40,
    0x00013620, 0x00026c40, 0x00013620, 0x00009b10,
    0x00013620, 0x00009b10, 0x00004d88, 0x00009b10,
    0x00004d88, 0x000026c4, 0x00004d88, 0x000026c4,
    0x00001362, 0x000026c4, 0x00001362, 0x000026c4,
    0x00001362, 0x000009b1, 0x00001362, 0x800009b1,
    0x000004d8, 0x800009b1, 0x400004d8, 0x8000026c,
    0x400004d8, 0x2000026c, 0x40000136, 0x2000026c,
    0x10000136, 0x2000009b, 0x10000136, 0x8800009b,
    0x1000004d, 0x8800009b, 0xc400004d, 0x88000026,
    0xc400004d, 0x62000026, 0xc4000013, 0x62000026,
    0xb1000013, 0x62000009, 0xb1000013, 0x62000009,
    0xb1000013, 0xd8800009, 0xb1000004, 0xd8800009,
    0x6c400004, 0xd8800002, 0x6c400004, 0x36200002,
    0xdfe00001, 0xfc0468fb, 0x870e1daf, 0x9863fcb1,
    0x5f63b004, 0x0c64f397, 0x9f9f9e7d, 0xc26c410d,
    0x9cb8ec7f, 0x7600631b, 0xadce5e6c, 0xb9ed84e0,
    0xffe00746, 0xe0b7a7d1, 0x100a36bc, 0x3820009b,
    0xc0026c41, 0xa3ce0dbb, 0x04d88051, 0xf382ad80,
    0x46cf9cb2, 0x00136201, 0xd7aa70b6, 0x2705385f,
    0x10004d88, 0x0136209c, 0xd8827040, 0x10000234,
    0x0136209c, 0xd8827040, 0x09c10004, 0x10004d88,
    0x0136209c, 0xd8827040, 0x09c10004, 0x10004d88,
    0x0136209c, 0xd8827040, 0x09c10004, 0x10004d88,
    0x0136209c, 0xd8827040, 0x27040004, 0x10004d88,
    0x0136209c, 0xd8827040, 0x27040004, 0xe8004d88,
    0x6c404e0d, 0xc166c002, 0x88031939, 0xcad8004d,
    0xb104e0bd, 0x13820009, 0x080026c4, 0x009b104e,
    0xb104e080, 0x13820009, 0x080026c4, 0x009b104e,
    0xb104e080, 0x13820009, 0x080026c4, 0x009b104e,
    0xb104e080, 0x13820009, 0x080026c4, 0x009b104e,
    0xb104e080, 0x13820009, 0x200026c4, 0x009b1138,
    0xb104e080, 0x13820009, 0x200026c4, 0x009b1138,
    0xb104e080, 0x13820009, 0x200026c4, 0x009b1138,
    0xb104e080, 0x13820009, 0x200026c4, 0x009b1138,
    0xce0a3bc0, 0x362031b3, 0x3cad8001, 0x0318672e,
    0xd8001362, 0x27059fca, 0x10001362, 0x004d889c,
    0xd8827040, 0x27040004, 0x10001362, 0x0136209c,
    0xd8827040, 0x27040004, 0x10001362, 0x0136209c,
    0xd8827040, 0x27040004, 0x10001362, 0x0136209c,
    0xd8827040, 0x27040004, 0x10001362, 0x0136209c,
    0xd889c100, 0x27040004, 0x10001362, 0x0136209c,
    0xab49c100, 0xbb00468f, 0x4f3973f0, 0xe3a040c6,
    0xe18cae72, 0x8fe7045d, 0x028c5ce5, 0xd69909b1,
    0x0635bcd0, 0xce5e7d02, 0x51939cbc, 0xd8b5deec,
    0xd1ae0b64, 0x8fd2d801, 0xfa91ffb7, 0x3620146e,
    0xe0cb6001, 0x80518bfc, 0xad8004d8, 0x6209c10c,
    0x27040013, 0x10013620, 0x04d8809c, 0x6209c100,
    0x27040013, 0xc8013620, 0x193fce0b, 0x004d8803,
    0x3979cad8, 0xc4018c5f, 0x656c0026, 0x36209c12,
    0x02704001, 0xc1001362, 0x004d8809, 0x36209c10,
    0x02704001, 0xc1001362, 0x004d8809, 0x36209c10,
    0x02704001, 0xc1001362, 0x004d8809, 0x36209c10,
    0x02704001, 0xc1001362, 0x004d8809, 0xff3828d8,
    0x469f9cb8, 0x00136201, 0xce5970b6, 0xb104e0ad,
    0x795b0009, 0x36209c12, 0x82704001, 0xc10004d8,
    0x004d8809, 0x36209c10, 0x82704001, 0xc10004d8,
    0x004d8809, 0x36209c10, 0x02704001, 0xc1001362,
    0x004d8809, 0xf383eff8, 0x6c4018c9, 0x36200002,
    0x6c400001, 0x36200002, 0x9b100001, 0x36200000,
    0x9b100001, 0x4d880000, 0x9b100000, 0x4d880000,
    0x26c40000, 0x4d880000, 0x26c40000, 0x13620000,
    0x26c40000, 0x13620000, 0x09b10000, 0x13620000,
    0x09b10000, 0x13620000, 0x09b10000, 0x04d88000,
    0x09b10000, 0x04d88000, 0x026c4000, 0x04d88000,
    0x026c4000, 0x01362000, 0x026c4000, 0x01362000,
    0x009b1000, 0x01362000, 0x009b1000, 0x004d8800,
    0x009b1000, 0x004d8800, 0x0026c400, 0x004d8800,
    0x0026c400, 0x00136200, 0x0026c400, 0x00136200,
    0x0009b100, 0x00136200, 0x0009b100, 0x0004d880,
    0x0009b100, 0x0004d880, 0x0009b100, 0x0004d880,
    0x00026c40, 0x0004d880, 0x00026c40, 0x00013620,
    0x00026c40, 0x00013620, 0x00009b10, 0x00013620,
    0x00009b10, 0x00004d88, 0x00009b10, 0x00004d88,
    0x000026c4, 0x00004d88, 0x000026c4, 0x00001362,
    0x000026c4, 0x00001362, 0x390009b1, 0x68e72f3e,
    0x0136200c, 0x9cd08b60, 0x14657397, 0x60013620,
    0x193cd08b, 0x004d8805, 0x36209a10, 0x02684001,
    0xa1001362, 0x004d8809, 0x36209a10, 0x02684001,
    0xa1001362, 0x004d8809, 0x36209a10, 0x02684001,
    0xa1001362, 0x004d8809, 0x36209a10, 0x02684001,
    0xa1001362, 0x004d8809, 0x36209a10, 0x02684001,
    0xa1001362, 0x004d8809, 0x36209a10, 0x02684001,
    0xa1001362, 0x004d8809, 0xd8809a10, 0x02684004,
    0xa1001362, 0x004d8809, 0xd8809a10, 0x02684004,
    0xa1001362, 0x004d8809, 0xd8809a10, 0x02684004,
    0xa1001362, 0x004d8809, 0xd8809a10, 0x02684004,
    0xa1001362, 0x004d8809, 0xefc09a10, 0x6008d1f0,
    0xfedfae17, 0x8e690c83, 0x8632b9cb, 0x08c9e177,
    0xeb909b10, 0x8c6e72d3, 0x65f26801, 0x60631739,
    0x01d1ae0b, 0xc64e6840, 0x0004d880, 0x13622684,
    0x209a1000, 0x68400136, 0x0004d882, 0x13622684,
    0xf09a1000, 0x023eac2f, 0x973f7c80, 0x69765b73,
    0x9b2fc0d2, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0009b113,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0x9b104d08, 0x41342000, 0x4200026c, 0x0026c413,
    0xa117e76c, 0x00136209, 0x3965f0b6, 0xd88031b3,
    0x422d8004, 0x10063273, 0x3420009b, 0xc0026c41,
    0xd09bf0bb, 0x0009b104, 0x39cb195b, 0x4d88031a,
    0xa7c2d800, 0x8826cbf2, 0xd0d8004d, 0x1abe72cd,
    0x004d8803, 0x3979cad8, 0x6200c6d7, 0xe0b60013,
    0x26c41d3a, 0x54ea6c00, 0x0270bfaf, 0xbdde1362,
    0xe0f9cbef, 0xfe74ff38, 0x768d5fe6, 0x1d3ae58f,
    0xa84026c4, 0xc69f9cb1, 0x9cb2eed0, 0x6200c6cf,
    0x53cd2013, 0x83ddc6b9, 0x200e7f2b, 0xfd520136,
    0x1abf9cba, 0xe587bb43, 0x200c62fc, 0x8d120136,
    0x07be9d72, 0x074eb953, 0xe81009b1, 0x6d55f947,
    0x0243ac0f, 0x6b1009b1, 0xc66fce5e, 0x5eaeeed0,
    0xc406697f, 0xf59c4026, 0x90eb4509, 0xd1dfadfa,
    0xc4018d1e, 0x51a24026, 0xe6ecdfbe, 0x26c41d3a,
    0x90eb2c40, 0x5ea6e1f6, 0xc406697f, 0xeb1c4026,
    0x5be1f690, 0xc4085d79, 0x51a84026, 0x60f7d3ae,
    0x9b100232, 0x43ac8900, 0xbd5387da, 0x880cd2fe,
    0x7338804d, 0x07b48759, 0xb742ebeb, 0x136200c6,
    0x48759120, 0xd79530fb, 0x026c4085, 0xfbe51a84,
    0xe5576ecd, 0x26c41d3a, 0x90eb2840, 0x7cbee1f6,
    0x13620b47, 0x48759420, 0xeb9530fb, 0x009b1074,
    0x4eb946a1, 0x2ebeb6ef, 0x10063174, 0xac89009b,
    0xaf07da43, 0x04d8810b, 0x75973588, 0x5e707b48,
    0x4018d1ce, 0xb224026c, 0xeb9fa90e, 0x009b1074,
    0x7ef946d1, 0xbafadbb3, 0x1018d9d0, 0xb224009b,
    0x1e1f690e, 0x0c6d7397, 0x12004d88, 0x0fb48759,
    0x75ba75eb, 0x004d8848, 0x9d728d22, 0x5d7d6dde,
    0x880c6ce8, 0x5912004d, 0x0f0fb487, 0x0635b9cb,
    0x890026c4, 0x690eb2e6, 0x23a75c0f, 0x65880136,
    0x75bf521d, 0x0c64d21d, 0x52004d88, 0x66fdf28d,
    0x59f2bbb7, 0x80136203, 0xed21d650, 0x175e54c3,
    0x10026c42, 0x7ea43aca, 0x9b11d3ae, 0xe51b4400,
    0x4ddbbd3a, 0x0b477ebd, 0x38801362, 0xc3ed21d6,
    0x10baf2b7, 0x50801362, 0xb4875973, 0x20119307,
    0x65480136, 0x53bf521d, 0x0b477ebd, 0x48801362,
    0xd9bf7ca3, 0x8842ebcd, 0x5962004d, 0x530fb487,
    0xb1085d79, 0xeb284009, 0x955dfa90, 0x26c474eb,
    0xb946c100, 0x5f76ef4e, 0x362004b6, 0x1d650801,
    0xe54c3ed2, 0x09b11d3a, 0xacb9a840, 0xbeb6ea43,
    0x0631742e, 0xa90026c4, 0xe7ea43ac, 0x26c42175,
    0xf946d100, 0xb9dbb37e, 0xd8842ebc, 0x75942004,
    0xebd6fd48, 0x1018c9f4, 0xb2a4009b, 0x49dfa90e,
    0x013623f4, 0x75ca3648, 0x72c7b77a, 0x04d88e9d,
    0x48759420, 0xedfeb0fb, 0x1090eb74, 0x9a44009b,
    0x6ea43acb, 0x6742ebeb, 0x026c4063, 0xa43aca90,
    0x57e50f7e, 0x0026c466, 0x7ef946c1, 0xae54dbb3,
    0x009b11d3, 0xa90eb284, 0x8848759f, 0x59a2004d,
    0xaeefd487, 0xd8842ebc, 0x28d82004, 0xcedde9d7,
    0x031a39cb, 0x44801362, 0x73f521d6, 0x04d88e9d,
    0xd65cda20, 0x29bb7521, 0x3622d0df, 0x1d660801,
    0x0baf3f52, 0x88013621, 0x9bf7ca36, 0x0eb2a6dd,
    0x4009b109, 0xfa90eb28, 0x2019fc9d, 0x66480136,
    0xf5bf521d, 0x635ba175, 0x90026c40, 0xf4eb946a,
    0x9d72976e, 0x2004d88e, 0xfd487594, 0x6c4243ac,
    0x2e6d1002, 0xadba90eb, 0x630d0baf, 0x90026c40,
    0x7ea43aca, 0x47b437eb, 0x026c4063, 0xef946a90,
    0x4eb93b37, 0x10026c47, 0x7ea43acd, 0x6c42175e,
    0x3acd1002, 0x175e7ea4, 0x10026c42, 0xf4eb946d,
    0x2011936e, 0x65480136, 0xf5bf521d, 0x8d5fa175,
    0x4009b101, 0x43acb9aa, 0x2175e6ea, 0xd10026c4,
    0x37ea43ac, 0x1085d795, 0x1b04009b, 0x4ecdfbe5,
    0x4dfbe557, 0xc10026c4, 0xb7ea43ac, 0x1be42ebe,
    0x0026c45a, 0xea43acb1, 0x85d79537, 0x04009b10,
    0xbd3ae51b, 0x1085d793, 0xb344009b, 0xfadfa90e,
    0x6357d0ba, 0x90026c40, 0x90eb2e6a, 0xaf2b7dba,
    0x0136210b, 0x521d6608, 0x1008c9bf, 0x1b24009b,
    0x4ecdfbe5, 0x6c42175e, 0x3acd1002, 0x175e7ea4,
    0x10026c42, 0x7ea43acd, 0x085d7953, 0xb04009b1,
    0x3bd3ae51, 0xb1085d79, 0xeb344009, 0x5d79fa90,
    0x4009b108, 0x43acb9b4, 0xd79536ea, 0x009b1085,
    0xa90eb304, 0x1085d797, 0x1bc4009b, 0x4ecdfbe5,
    0x6c42175e, 0x3acd1002, 0x175e7ea4, 0x10026c42,
    0x7ea43acd, 0x36201193, 0xca364801, 0xaf277a75,
    0x0136210b, 0x521d6688, 0x210baf3f, 0x36880136,
    0xdd487597, 0x2175e52e, 0xc10026c4, 0x65ea43ac,
    0x1362121d, 0x7ca37880, 0xeee9d9bf, 0xc400cfed,
    0xac910026, 0xfeb7ea43, 0x31a3a165, 0x48013620,
    0x90eb1d8e, 0x474eb8fa, 0x6f10026c, 0x4ef4eb94,
    0x6742ebeb, 0x026c4063, 0xa43aca90, 0x75e58f7e,
    0x0026c421, 0x0eb2e6c1, 0xd654dba9, 0x01362121,
    0x521d6608, 0xa175f5af, 0x362031b3, 0xca364801,
    0xbc9d9bf7, 0x04d8842e, 0x48759a20, 0x842ebcfd,
    0x9a2004d8, 0xacbd4875, 0x026c4243, 0xeb946f10,
    0xebeb4ef4, 0x018d5f42, 0x2a4009b1, 0x9dfa90eb,
    0x8842ebcb, 0xcd82004d, 0xa7521d65, 0x62fd3af5,
    0x004d880c, 0xd4875992, 0xc4243acb, 0x46f10026,
    0x93b37ef9, 0x9b1085d7, 0x0eb34400, 0x48759fa9,
    0xa2004d88, 0xebd48759, 0x00cfedea, 0xb10026c4,
    0xef4eb946, 0x04365f74, 0x08013620, 0xbf521d66,
    0x23a75ca9, 0x36080136, 0x9d487597, 0x6c4243ac,
    0x3acf1002, 0xe5375ea4, 0x26c42175, 0xf946e100,
    0xb7d3b37e, 0xb1090eb2, 0xeb304009, 0x464dfa90,
    0x2004d880, 0xbd487599, 0x921d654e, 0x000234d8,
    0x9d728dc2, 0x3961e9de, 0x3620c6b7, 0x75952001,
    0x2ebcfd48, 0x20013624, 0x21d65cda, 0x21d75a75,
    0x3620c64d, 0x75992001, 0xeaeebd48, 0xc40693f5,
    0x5ac40026, 0x9bf7c763, 0x2cbfd67d, 0xb1063474,
    0xaca90009, 0x75e5ea43, 0x0009b121, 0xea43acf1,
    0x62fc1275, 0x8dd20013, 0xe9de9d72, 0xdc3870f1,
    0x04d8831a, 0x21d65480, 0xe0969bf5, 0x6c490eb4,
    0xb9ac4002, 0x74ea43ac, 0x12175e55, 0xce10009b,
    0xf75ea43a, 0x2fc1c387, 0x001362cc, 0xfdf28da2,
    0x3af5a766, 0x620c68fd, 0x59520013, 0xebcbd487,
    0x00136242, 0xd48759e2, 0x3870e6eb, 0x09b17e08,
    0xb946d100, 0xaeb4ef4e, 0x0636ba43, 0xa90009b1,
    0x67ea43ac, 0x04d8921d, 0x59736880, 0xebc9d487,
    0x00136242, 0xd48759e2, 0x243ac1eb, 0xe90009b1,
    0xb37ef946, 0xb174eb93, 0xacd10009, 0xaeb5ea43,
    0x4318ba43, 0xc0468f86, 0x9cbefe06, 0xbbf0c6ef,
    0xec06c3ff, 0xe90ebacb, 0x437f1cb8, 0x70e1c9ba,
    0x7e7e7db8, 0x7de24d9e, 0xf2f2f0fb, 0x00000a62,
    0x3a35c26c, 0x13620000, 0x09b10000, 0x13620000,
    0x09b10000, 0x04d88000, 0x09b10000, 0x04d88000,
    0x026c4000, 0x04d88000, 0x026c4000, 0x01362000,
    0x026c4000, 0x01362000, 0x009b1000, 0x01362000,
    0x009b1000, 0x004d8800, 0x009b1000, 0x004d8800,
    0x0026c400, 0x004d8800, 0x0026c400, 0x00136200,
    0x0026c400, 0x00136200, 0x0009b100, 0x00136200,
    0x0009b100, 0x00136200, 0x0009b100, 0x0004d880,
    0x0009b100, 0x0004d880, 0x00026c40, 0x0004d880,
    0x00026c40, 0x00013620, 0x00026c40, 0x00013620,
    0x00009b10, 0x00013620, 0x00009b10, 0x00004d88,
    0x00009b10, 0x00004d88, 0x000026c4, 0x00004d88,
    0x000026c4, 0x00001362, 0x000026c4, 0x00001362,
    0x000009b1, 0x00001362, 0x800009b1, 0x000004d8,
    0x800009b1, 0x000004d8, 0x800009b1, 0x400004d8,
    0x8000026c, 0x400004d8, 0x2000026c, 0x40000136,
    0x2000026c, 0x10000136, 0x2000009b, 0x10000136,
    0x8800009b, 0x1000004d, 0x8800009b, 0xc400004d,
    0x88000026, 0xc400004d, 0x62000026, 0xc4000013,
    0x62000026, 0xb1000013, 0x62000009, 0xb1000013,
    0xd8800009, 0xb1000004, 0xd8800009, 0x6c400004,
    0xd8800002, 0x6c400004, 0xd8800002, 0x6c400004,
    0x36200002, 0x6c400001, 0x36200002, 0x9b100001,
    0x36200000, 0x9b100001, 0x4d880000, 0x9b100000,
    0x4d880000, 0x26c40000, 0x4d880000, 0x26c40000,
    0x13620000, 0x26c40000, 0x13620000, 0x09b10000,
    0x13620000, 0x09b10000, 0x04d88000, 0x09b10000,
    0x04d88000, 0x026c4000, 0x04d88000, 0x026c4000,
    0x01362000, 0x026c4000, 0x01362000, 0x026c4000,
    0x01362000, 0x009b1000, 0x01362000, 0x009b1000,
    0x004d8800, 0x009b1000, 0x004d8800, 0x0026c400,
    0x004d8800, 0x43fcba00, 0xef000180, 0x8e2d3d77,
    0x0048175a, 0x49000000, 0xae444e45, 0x00826042,
};

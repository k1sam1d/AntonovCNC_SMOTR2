#include "AntonovCNC.h"
#include "ui_AntonovCNC.h"
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QGraphicsLineItem>
#include <random>

AntonovCNC::AntonovCNC(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::AntonovCNCClass),
    programProgress(0),
    programRunning(false),
    inInches(false),
    currentProgramRow(0),
    spindleSpeed(1000),
    feedRate(100),
    feedRateMultiplier(1.0),
    spindleSpeedMultiplier(1.0),
    xValueCurrent(0),
    yValueCurrent(0),
    zValueCurrent(0),
    xValueFinal(10),
    yValueFinal(10),
    zValueFinal(10),
    scene(new QGraphicsScene(this))
{
    ui->setupUi(this);
    ui->graphView_simul->setScene(scene);

    connect(ui->slider_spindle_speed, &QSlider::valueChanged, this, &AntonovCNC::handleSpindleSpeedChange);
    connect(ui->slider_feed_rate, &QSlider::valueChanged, this, &AntonovCNC::handleFeedRateChange);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AntonovCNC::updateTime);
    timer->start(1000);

    connect(ui->button_numeration, &QPushButton::clicked, this, &AntonovCNC::handleNumeration);
    connect(ui->button_smenaekrana, &QPushButton::clicked, this, &AntonovCNC::handleSmenaEkrana);
    connect(ui->button_mmdyum, &QPushButton::clicked, this, &AntonovCNC::handleMmDyum);
    connect(ui->button_changesk, &QPushButton::clicked, this, &AntonovCNC::handleChangeSK);
    connect(ui->button_priv, &QPushButton::clicked, this, &AntonovCNC::handlePriv);
    connect(ui->button_startkadr, &QPushButton::clicked, this, &AntonovCNC::handleStartKadr);
    connect(ui->button_korrekt, &QPushButton::clicked, this, &AntonovCNC::handleKorrektion);
    connect(ui->button_smesh, &QPushButton::clicked, this, &AntonovCNC::handleSmesh);
    connect(ui->button_selectprog, &QPushButton::clicked, this, &AntonovCNC::loadProgram);
    connect(ui->button_back, &QPushButton::clicked, this, &AntonovCNC::handleBack);
    connect(ui->commandLinkButton_stop, &QPushButton::clicked, this, &AntonovCNC::handleStop);
    connect(ui->commandLinkButton_start, &QPushButton::clicked, this, &AntonovCNC::handleStart);
    connect(ui->commandLinkButton_reset, &QPushButton::clicked, this, &AntonovCNC::handleReset);
    connect(ui->commandLinkButton_resetalarms, &QPushButton::clicked, this, &AntonovCNC::handleResetAlarm);

    updateCoordinatesDisplay();
    updateUnits();
    updateProgramStatus();
}

AntonovCNC::~AntonovCNC() {
    delete ui;
}

void AntonovCNC::updateTime() {
    QDateTime currentTime = QDateTime::currentDateTime();
    ui->label_date->setText(currentTime.toString("dd.MM.yyyy"));
    ui->label_time->setText(currentTime.toString("HH:mm:ss"));
}

void AntonovCNC::handleResetAlarm() {
    QMessageBox::information(this, tr("Reset Alarm"), tr("All alarms have been reset."));
    qDebug() << "Сигналы тревоги сброшены";
}

void AntonovCNC::handleNumeration() {
    static bool numbered = false;
    for (int i = 0; i < ui->listWidget_program->count(); ++i) {
        QListWidgetItem* item = ui->listWidget_program->item(i);
        if (!numbered) {
            item->setText(QString::number(i + 1) + ": " + item->text());
        }
        else {
            QStringList parts = item->text().split(": ");
            if (parts.size() > 1) {
                item->setText(parts[1]);
            }
        }
    }
    numbered = !numbered;
}

void AntonovCNC::handleSmenaEkrana() {
    int currentIndex = ui->stackedWidget_winchan->currentIndex();
    ui->stackedWidget_winchan->setCurrentIndex(currentIndex == 0 ? 1 : 0);
}

void AntonovCNC::handleMmDyum() {
    inInches = !inInches;
    updateUnits();
    analyzeProgram();
}

void AntonovCNC::handleChangeSK() { }

void AntonovCNC::handlePriv() { }

void AntonovCNC::handleStartKadr() { }

void AntonovCNC::handleKorrektion() { }

void AntonovCNC::handleSmesh() { }

void AntonovCNC::handleBack() { }

void AntonovCNC::handleStop() {
    if (programRunning) {
        programRunning = false;
        ui->label_rejim_isp->setText("PROG");
        qDebug() << "Программа остановлена";
    }
}

void AntonovCNC::handleStart() {
    if (!programRunning && !loadedProgram.isEmpty()) {
        programRunning = true;
        programProgress = 0;
        currentProgramRow = 0;
        analyzeProgram();
        updateProgramStatus();
        QTimer::singleShot(100, this, &AntonovCNC::updateProgressBar);
        ui->label_rejim_isp->setText("AUTO");
    }
}

void AntonovCNC::updateProgramStatus() {
    if (loadedProgram.isEmpty()) {
        ui->label_sostoyanine->setText("WAIT FOR LOADING");
    }
    else {
        bool errors = analyzeProgramForErrors();
        if (errors) {
            ui->label_sostoyanine->setText("UNABLE TO START");
        }
        else {
            ui->label_sostoyanine->setText("READY TO START");
        }
    }
}

bool AntonovCNC::analyzeProgramForErrors() {
    for (const QString& line : loadedProgram) {
        if (line.contains("G") || line.contains("M") || line.contains(QRegularExpression("N\\d+"))) {
            ui->label_error->setText("Ошибок нет");
            return false;
        }
    }
    ui->label_error->setText("Найдены ошибки, проверьте исполняющую программу");
    return true;
}

void AntonovCNC::loadProgram() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Выбрать программу"), "", tr("Файлы программы (*.txt *.nc);;Все файлы (*.*)"));
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            loadedProgram.clear();
            ui->listWidget_program->clear();
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                loadedProgram.append(line);
                ui->listWidget_program->addItem(line);
            }
            updateInfoLine("Программа загружена успешно");
            analyzeProgram();
            updateProgramStatus();
        }
    }
}

void AntonovCNC::updateInfoLine(const QString& message) {
    ui->info_line->setText(message);
}

void AntonovCNC::handleReset() {
    programProgress = 0;
    currentProgramRow = 0;
    updateStatusBar(0);
    programRunning = false;

    xValueCurrent = 0;
    yValueCurrent = 0;
    zValueCurrent = 0;
    xValueFinal = 10;
    yValueFinal = 10;
    zValueFinal = 10;

    updateCoordinatesDisplay();

    if (ui->listWidget_program->count() > 0) {
        QListWidgetItem* item = ui->listWidget_program->item(0);
        ui->listWidget_program->setCurrentItem(item);
    }
}

void AntonovCNC::updateProgressBar() {
    if (programRunning && currentProgramRow < loadedProgram.size()) {
        int progressPercentage = static_cast<int>((static_cast<double>(currentProgramRow) / loadedProgram.size()) * 100);
        updateStatusBar(progressPercentage);
        highlightCurrentProgramRow(currentProgramRow);
        currentProgramRow++;
        extractNextCoordinates();
        QTimer::singleShot(1000 / (feedRateMultiplier * spindleSpeedMultiplier), this, &AntonovCNC::updateProgressBar);
    }
    else {
        programRunning = false;
    }
}

void AntonovCNC::updateStatusBar(int value) {
    ui->progressBar_runtime->setValue(value);
}

void AntonovCNC::highlightCurrentProgramRow(int row) {
    if (row < ui->listWidget_program->count()) {
        QListWidgetItem* item = ui->listWidget_program->item(row);
        ui->listWidget_program->setCurrentItem(item);
        extractCoordinatesAndSpeed(loadedProgram[row]);
    }
}

void AntonovCNC::handleSpindleSpeedChange() {
    spindleSpeedMultiplier = 1.0 + (ui->slider_spindle_speed->value() / 100.0);
    ui->label_spindle_value->setText(QString::number(spindleSpeed * spindleSpeedMultiplier));
}

void AntonovCNC::handleFeedRateChange() {
    feedRateMultiplier = 1.0 + (ui->slider_feed_rate->value() / 100.0);
    ui->label_feed_value->setText(QString::number(feedRate * feedRateMultiplier));
}

void AntonovCNC::analyzeProgram() {
    scene->clear();
    for (const QString& line : loadedProgram) {
        extractCoordinatesAndSpeed(line);
        parseAndDrawTrajectory(line);
    }
    displayActiveFunctions();
}

void AntonovCNC::extractCoordinatesAndSpeed(const QString& line, bool isNext) {
    QRegularExpression regexX(R"(X(-?\d+\.?\d*))");
    QRegularExpression regexY(R"(Y(-?\d+\.?\d*))");
    QRegularExpression regexZ(R"(Z(-?\d+\.?\d*))");
    QRegularExpression regexF(R"(F(\d+))");
    QRegularExpression regexS(R"(S(\d+))");

    QRegularExpressionMatch matchX = regexX.match(line);
    if (matchX.hasMatch()) {
        double value = matchX.captured(1).toDouble();
        if (inInches) value *= 25.4;
        if (isNext) xValueFinal = value; else xValueCurrent = value;
    }

    QRegularExpressionMatch matchY = regexY.match(line);
    if (matchY.hasMatch()) {
        double value = matchY.captured(1).toDouble();
        if (inInches) value *= 25.4;
        if (isNext) yValueFinal = value; else yValueCurrent = value;
    }

    QRegularExpressionMatch matchZ = regexZ.match(line);
    if (matchZ.hasMatch()) {
        double value = matchZ.captured(1).toDouble();
        if (inInches) value *= 25.4;
        if (isNext) zValueFinal = value; else zValueCurrent = value;
    }

    QRegularExpressionMatch matchF = regexF.match(line);
    if (matchF.hasMatch()) {
        feedRate = matchF.captured(1).toInt();
        ui->label_feed_value->setText(QString::number(feedRate * feedRateMultiplier));
    }

    QRegularExpressionMatch matchS = regexS.match(line);
    if (matchS.hasMatch()) {
        spindleSpeed = matchS.captured(1).toInt();
        ui->label_spindle_value->setText(QString::number(spindleSpeed * spindleSpeedMultiplier));
    }

    updateCoordinatesDisplay();
}

void AntonovCNC::parseAndDrawTrajectory(const QString& line) {
    QRegularExpression regexG0(R"(G0\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*))");
    QRegularExpressionMatch matchG0 = regexG0.match(line);

    if (matchG0.hasMatch()) {
        double x = matchG0.captured(1).toDouble();
        double y = matchG0.captured(2).toDouble();
        if (inInches) {
            x *= 25.4;
            y *= 25.4;
        }
        scene->addLine(xValueCurrent, yValueCurrent, x, y, QPen(Qt::red));
        xValueCurrent = x;
        yValueCurrent = y;
    }
}

void AntonovCNC::updateUnits() {
    if (inInches) {
        ui->label_x_axis->setText("X (in)");
        ui->label_y_axis->setText("Y (in)");
        ui->label_z_axis->setText("Z (in)");
    }
    else {
        ui->label_x_axis->setText("X (mm)");
        ui->label_y_axis->setText("Y (mm)");
        ui->label_z_axis->setText("Z (mm)");
    }
}

void AntonovCNC::updateCoordinatesDisplay() {
    ui->label_x_value_current->setText(QString::number(xValueCurrent, 'f', 2));
    ui->label_y_value_current->setText(QString::number(yValueCurrent, 'f', 2));
    ui->label_z_value_current->setText(QString::number(zValueCurrent, 'f', 2));

    ui->label_x_value_final->setText(QString::number(xValueFinal, 'f', 2));
    ui->label_y_value_final->setText(QString::number(yValueFinal, 'f', 2));
    ui->label_z_value_final->setText(QString::number(zValueFinal, 'f', 2));
}

void AntonovCNC::extractNextCoordinates() {
    if (currentProgramRow + 1 < loadedProgram.size()) {
        extractCoordinatesAndSpeed(loadedProgram[currentProgramRow + 1], true);
    }
    else {
        xValueCurrent = xValueFinal;
        yValueCurrent = yValueFinal;
        zValueCurrent = zValueFinal;
    }
}

void AntonovCNC::displayActiveFunctions() {
    QSet<QString> gCodes, mCodes, tCodes, dCodes;

    for (const QString& line : loadedProgram) {
        extractCoordinatesAndSpeed(line);

        QRegularExpression regexG(R"(G\d{1,3})");
        QRegularExpression regexM(R"(M\d{1,3})");
        QRegularExpression regexT(R"(T\d{1,2})");
        QRegularExpression regexD(R"(D\d{1,2})");

        QRegularExpressionMatch matchG = regexG.match(line);
        if (matchG.hasMatch()) {
            gCodes.insert(matchG.captured(0));
        }

        QRegularExpressionMatch matchM = regexM.match(line);
        if (matchM.hasMatch()) {
            mCodes.insert(matchM.captured(0));
        }

        QRegularExpressionMatch matchT = regexT.match(line);
        if (matchT.hasMatch()) {
            tCodes.insert(matchT.captured(0));
        }

        QRegularExpressionMatch matchD = regexD.match(line);
        if (matchD.hasMatch()) {
            dCodes.insert(matchD.captured(0));
        }
    }

    QList<QLabel*> gLabels = { ui->label_g_code, ui->label_g_code_1, ui->label_g_code_2, ui->label_g_code_3 };
    QList<QLabel*> mLabels = { ui->label_m_code, ui->label_m_code_1, ui->label_m_code_2, ui->label_m_code_3 };
    QList<QLabel*> tLabels = { ui->label_t_code, ui->label_t_code_1, ui->label_t_code_2, ui->label_t_code_3 };
    QList<QLabel*> dLabels = { ui->label_d_code, ui->label_d_code_1, ui->label_d_code_2, ui->label_d_code_3 };

    std::random_device rd;
    std::mt19937 g(rd());

    int index = 0;
    for (const QString& gCode : gCodes) {
        if (index < gLabels.size()) gLabels[index++]->setText(gCode);
    }

    index = 0;
    for (const QString& mCode : mCodes) {
        if (index < mLabels.size()) mLabels[index++]->setText(mCode);
    }

    index = 0;
    for (const QString& tCode : tCodes) {
        if (index < tLabels.size()) tLabels[index++]->setText(tCode);
    }

    index = 0;
    for (const QString& dCode : dCodes) {
        if (index < dLabels.size()) dLabels[index++]->setText(dCode);
    }
}

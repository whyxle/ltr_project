#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QFrame>
#include <QDebug>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QDockWidget>
#include <QScrollArea>
#include <QToolBar>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <numeric>
#include <algorithm>
#include <cmath>

#include "ltr/ltr11.h"
#include "ltr/ltr114.h"
#include "ltr/ltr_result.h"

QString MainWindow::module_name(WORD mid)
{
    switch (mid)
    {
    case LTR_MID_EMPTY: return "EMPTY";
    case LTR_MID_IDENTIFYING: return "IDENTIFYING";

    case LTR_MID_LTR01: return "LTR01";
    case LTR_MID_LTR11: return "LTR11";
    case LTR_MID_LTR22: return "LTR22";
    case LTR_MID_LTR24: return "LTR24";
    case LTR_MID_LTR25: return "LTR25";
    case LTR_MID_LTR27: return "LTR27";
    case LTR_MID_LTR34: return "LTR34";
    case LTR_MID_LTR35: return "LTR35";
    case LTR_MID_LTR41: return "LTR41";
    case LTR_MID_LTR42: return "LTR42";
    case LTR_MID_LTR43: return "LTR43";
    case LTR_MID_LTR51: return "LTR51";
    case LTR_MID_LTR114: return "LTR114";
    case LTR_MID_LTR210: return "LTR210";
    case LTR_MID_LTR212: return "LTR212";
    default: return QString("Unknown (%1, 0x%2)").arg(mid).arg(QString::number(mid, 16).toUpper());
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , modulesList(nullptr)
    , mainToolBar(nullptr)
    , modulesDock(nullptr)
    , modulesDockButton(nullptr)
    , infoText(nullptr)
    , startButton(nullptr)
    , stopButton(nullptr)
    , sampleRateSpin(nullptr)
    , plotEverySpin(nullptr)
    , chunkSizeSpin(nullptr)
    , saveToFileCheck(nullptr)
    , unitCombo(nullptr)
    , plotSettingsButton(nullptr)
    , commonSettingsButton(nullptr)
    , ltr114SettingsButton(nullptr)
    , simulationSettingsButton(nullptr)
    , ltr212SettingsButton(nullptr)
    , commonSettingsDock(nullptr)
    , ltr114SettingsDock(nullptr)
    , simulationSettingsDock(nullptr)
    , ltr212SettingsDock(nullptr)
    , commonSettingsGroup(nullptr)
    , ltr114SettingsGroup(nullptr)
    , range114Combo(nullptr)
    , physicalChannel114Spin(nullptr)
    , interval114Spin(nullptr)
    , syncMode114Combo(nullptr)
    , simulationSettingsGroup(nullptr)
    , simulationRateSpin(nullptr)
    , ltr212SettingsGroup(nullptr)
    , acqMode212Combo(nullptr)
    , useClb212Check(nullptr)
    , useFabricClb212Check(nullptr)
    , refVoltage212Combo(nullptr)
    , acMode212Combo(nullptr)
    , channelCount212Spin(nullptr)
    , range212Combo(nullptr)
    , chartView114(nullptr)
    , chartView212(nullptr)
    , chartViewSync(nullptr)
    , chart114(nullptr)
    , chart212(nullptr)
    , chartSync(nullptr)
    , lineSeries114(nullptr)
    , lineSeries212(nullptr)
    , lineSeriesSync114(nullptr)
    , lineSeriesSync212(nullptr)
    , axisX114(nullptr)
    , axisY114(nullptr)
    , axisX212(nullptr)
    , axisY212(nullptr)
    , axisXSync(nullptr)
    , axisYSync(nullptr)
    , m_ltr114Slot(-1)
    , m_ltr212Slot(-1)
    , m_captureRunning(false)
    , m_simulationMode(false)
    , m_simulatedSampleAccumulator(0.0)
    , m_simulatedSampleAccumulator212(0.0)
{
    ui->setupUi(this);
    qRegisterMetaType<QVector<TimedSample>>("QVector<TimedSample>");

    init_ui_replace();
    setup_plot();
    appendInfo("Приложение запущено.");

    init_ltr();
    update_plot_visibility();
    update_ltr114_controls_state();
    update_ltr212_controls_state();
    update_simulation_controls_state();
}

MainWindow::~MainWindow()
{
    m_captureRunning = false;
    if (m_simulationTimer)
        m_simulationTimer->stop();

    stop_worker_threads();
    close_ltr114_capture();
    close_ltr212_capture();

    if (saveToFileCheck && saveToFileCheck->isChecked()) {
        append_samples_to_file(m_pendingFileSamples114, 0);
        append_samples_to_file(m_pendingFileSamples212, 1);
    }
    m_pendingFileSamples114.clear();
    m_pendingFileSamples212.clear();
    close_capture_files();

    if (m_crate && m_crate->is_open())
        m_crate->stop_sync_marks();

    delete ui;
}

QDockWidget* MainWindow::create_settings_dock(const QString& title, QWidget* content, const QString& objectName)
{
    QDockWidget* dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetClosable
                      | QDockWidget::DockWidgetMovable
                      | QDockWidget::DockWidgetFloatable);
    dock->setMinimumWidth(320);

    QScrollArea* scroll = new QScrollArea(dock);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    content->setParent(scroll);
    scroll->setWidget(content);

    dock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();
    return dock;
}

QDockWidget* MainWindow::create_modules_dock(QWidget* content)
{
    QDockWidget* dock = new QDockWidget("LTR слоты", this);
    dock->setObjectName("modulesDock");
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetClosable
                      | QDockWidget::DockWidgetMovable
                      | QDockWidget::DockWidgetFloatable);
    dock->setMinimumWidth(300);
    dock->setWidget(content);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    return dock;
}

void MainWindow::show_dock(QDockWidget* dock, bool floating)
{
    if (!dock)
        return;

    dock->show();
    dock->setFloating(floating);
    dock->raise();
    dock->activateWindow();
}

void MainWindow::init_ui_replace()
{
    QWidget* central = ui->centralwidget ? ui->centralwidget : new QWidget(this);
    setCentralWidget(central);
    setDockOptions(QMainWindow::AnimatedDocks
                   | QMainWindow::AllowNestedDocks
                   | QMainWindow::AllowTabbedDocks);

    QVBoxLayout* mainLay = new QVBoxLayout;
    mainLay->setContentsMargins(0, 0, 0, 0);
    central->setLayout(mainLay);

    modulesList = new QListWidget(this);
    modulesList->setMinimumWidth(320);
    modulesList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    modulesList->setSelectionMode(QAbstractItemView::NoSelection);
    modulesList->setFocusPolicy(Qt::NoFocus);
    modulesDock = create_modules_dock(modulesList);

    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLay = new QVBoxLayout(rightPanel);

    sampleRateSpin = new QSpinBox(this);
    sampleRateSpin->setRange(1, 4000);
    sampleRateSpin->setValue(100);
    sampleRateSpin->setSuffix(" Гц");

    plotEverySpin = new QSpinBox(this);
    plotEverySpin->setRange(1, 10000);
    plotEverySpin->setValue(1);

    chunkSizeSpin = new QSpinBox(this);
    chunkSizeSpin->setRange(16, 20000);
    chunkSizeSpin->setValue(3000);

    startButton = new QPushButton("Старт", this);
    stopButton = new QPushButton("Остановить", this);
    stopButton->setEnabled(false);

    saveToFileCheck = new QCheckBox("Сохранять файл", this);
    saveToFileCheck->setChecked(true);

    unitCombo = new QComboBox(this);
    unitCombo->addItem("mV", "mV");
    unitCombo->addItem("V", "V");

    plotSettingsButton = new QPushButton("Настройки графиков", this);
    commonSettingsButton = new QPushButton("Общие", this);
    ltr114SettingsButton = new QPushButton("LTR114", this);
    simulationSettingsButton = new QPushButton("Симуляция", this);
    ltr212SettingsButton = new QPushButton("LTR212", this);
    modulesDockButton = new QPushButton("Слоты", this);

    mainToolBar = addToolBar("Управление");
    mainToolBar->setObjectName("mainToolBar");
    mainToolBar->setAllowedAreas(Qt::TopToolBarArea);
    mainToolBar->setMovable(false);
    mainToolBar->setFloatable(false);
    mainToolBar->addWidget(startButton);
    mainToolBar->addWidget(stopButton);
    mainToolBar->addSeparator();
    mainToolBar->addWidget(modulesDockButton);
    mainToolBar->addWidget(commonSettingsButton);
    mainToolBar->addWidget(ltr114SettingsButton);
    mainToolBar->addWidget(ltr212SettingsButton);
    mainToolBar->addWidget(simulationSettingsButton);
    mainToolBar->addSeparator();
    mainToolBar->addWidget(plotSettingsButton);

    commonSettingsGroup = new QGroupBox("Общие параметры", this);
    QFormLayout* commonLay = new QFormLayout(commonSettingsGroup);
    commonLay->addRow("Прореживание графика:", plotEverySpin);
    commonLay->addRow("Размер блока:", chunkSizeSpin);
    commonLay->addRow("Сохранение:", saveToFileCheck);
    commonLay->addRow("Единицы:", unitCombo);
    commonSettingsDock = create_settings_dock("Общие параметры", commonSettingsGroup, "commonSettingsDock");

    ltr114SettingsGroup = new QGroupBox("Настройки LTR114", this);
    QFormLayout* ltr114Lay = new QFormLayout(ltr114SettingsGroup);

    physicalChannel114Spin = new QSpinBox(ltr114SettingsGroup);
    physicalChannel114Spin->setRange(1, LTR114_MAX_CHANNEL);
    physicalChannel114Spin->setValue(1);

    range114Combo = new QComboBox(ltr114SettingsGroup);
    range114Combo->addItem("+/-10 V", LTR114_URANGE_10);
    range114Combo->addItem("+/-2 V", LTR114_URANGE_2);
    range114Combo->addItem("+/-0.4 V", LTR114_URANGE_04);
    range114Combo->setCurrentIndex(2);

    syncMode114Combo = new QComboBox(ltr114SettingsGroup);
    syncMode114Combo->addItem("Internal", LTR114_SYNCMODE_INTERNAL);
    syncMode114Combo->addItem("Master", LTR114_SYNCMODE_MASTER);
    syncMode114Combo->addItem("External", LTR114_SYNCMODE_EXTERNAL);
    syncMode114Combo->addItem("None", LTR114_SYNCMODE_NONE);

    interval114Spin = new QSpinBox(ltr114SettingsGroup);
    interval114Spin->setRange(0, 65535);
    interval114Spin->setValue(0);
    ltr114Lay->addRow("Частота АЦП:", sampleRateSpin);
    ltr114Lay->addRow("Канал:", physicalChannel114Spin);
    ltr114Lay->addRow("Диапазон:", range114Combo);
    ltr114Lay->addRow("SyncMode:", syncMode114Combo);
    ltr114Lay->addRow("Interval:", interval114Spin);
    ltr114SettingsDock = create_settings_dock("Настройки LTR114", ltr114SettingsGroup, "ltr114SettingsDock");

    simulationSettingsGroup = new QGroupBox("Режим симуляции", this);
    QFormLayout* simulationLay = new QFormLayout(simulationSettingsGroup);
    simulationRateSpin = new QSpinBox(simulationSettingsGroup);
    simulationRateSpin->setRange(1, 4000);
    simulationRateSpin->setValue(sampleRateSpin->value());
    simulationRateSpin->setSuffix(" Гц");
    simulationLay->addRow("Частота симуляции:", simulationRateSpin);
    simulationSettingsDock = create_settings_dock("Режим симуляции", simulationSettingsGroup, "simulationSettingsDock");

    ltr212SettingsGroup = new QGroupBox("Настройки LTR212", this);
    QFormLayout* ltr212Lay = new QFormLayout(ltr212SettingsGroup);

    acqMode212Combo = new QComboBox(ltr212SettingsGroup);
    acqMode212Combo->addItem("4 канала, средняя точность", LTR212_FOUR_CHANNELS_WITH_MEDIUM_RESOLUTION);
    acqMode212Combo->addItem("4 канала, высокая точность", LTR212_FOUR_CHANNELS_WITH_HIGH_RESOLUTION);
    acqMode212Combo->addItem("8 каналов, высокая точность", LTR212_EIGHT_CHANNELS_WITH_HIGH_RESOLUTION);
    acqMode212Combo->setCurrentIndex(1);

    useClb212Check = new QCheckBox(ltr212SettingsGroup);
    useClb212Check->setChecked(false);

    useFabricClb212Check = new QCheckBox(ltr212SettingsGroup);
    useFabricClb212Check->setChecked(true);

    refVoltage212Combo = new QComboBox(ltr212SettingsGroup);
    refVoltage212Combo->addItem("2.5 В", LTR212_REF_2_5V);
    refVoltage212Combo->addItem("5 В", LTR212_REF_5V);
    refVoltage212Combo->setCurrentIndex(1);

    acMode212Combo = new QComboBox(ltr212SettingsGroup);
    acMode212Combo->addItem("DC", 0);
    acMode212Combo->addItem("AC", 1);

    channelCount212Spin = new QSpinBox(ltr212SettingsGroup);
    channelCount212Spin->setRange(1, 4);
    channelCount212Spin->setValue(1);

    range212Combo = new QComboBox(ltr212SettingsGroup);
    range212Combo->addItem("±10 мВ", LTR212_SCALE_B_10);
    range212Combo->addItem("±20 мВ", LTR212_SCALE_B_20);
    range212Combo->addItem("±40 мВ", LTR212_SCALE_B_40);
    range212Combo->addItem("±80 мВ", LTR212_SCALE_B_80);
    range212Combo->addItem("0..+10 мВ", LTR212_SCALE_U_10);
    range212Combo->addItem("0..+20 мВ", LTR212_SCALE_U_20);
    range212Combo->addItem("0..+40 мВ", LTR212_SCALE_U_40);
    range212Combo->addItem("0..+80 мВ", LTR212_SCALE_U_80);
    range212Combo->setCurrentIndex(3);

    ltr212Lay->addRow("Режим сбора:", acqMode212Combo);
    ltr212Lay->addRow("UseClb:", useClb212Check);
    ltr212Lay->addRow("UseFabricClb:", useFabricClb212Check);
    ltr212Lay->addRow("Опорное:", refVoltage212Combo);
    ltr212Lay->addRow("AC/DC:", acMode212Combo);
    ltr212Lay->addRow("Каналов:", channelCount212Spin);
    ltr212Lay->addRow("Диапазон:", range212Combo);
    ltr212SettingsDock = create_settings_dock("Настройки LTR212", ltr212SettingsGroup, "ltr212SettingsDock");

    infoText = new QTextEdit(this);
    infoText->setReadOnly(true);
    infoText->setAcceptRichText(false);
    QFont f = infoText->font();
    f.setFamily("Monospace");
    f.setStyleHint(QFont::Monospace);
    infoText->setFont(f);

    rightLay->addWidget(infoText, 1);

    mainLay->addWidget(rightPanel, 1);

    connect(startButton, &QPushButton::clicked, this, &MainWindow::on_start_capture_clicked);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::on_stop_capture_clicked);
    connect(unitCombo, &QComboBox::currentTextChanged, this, [this](const QString&) {
        update_axis_unit_labels();
    });
    connect(plotSettingsButton, &QPushButton::clicked, this, &MainWindow::show_plot_settings_dialog);
    auto bindDockButton = [this](QPushButton* button, QDockWidget* dock, bool floating) {
        if (!button || !dock)
            return;

        button->setCheckable(true);
        button->setChecked(!dock->isHidden());
        connect(button, &QPushButton::clicked, this, [this, dock, floating](bool checked) {
            if (checked) {
                show_dock(dock, floating);
            } else {
                dock->hide();
            }
        });
        connect(dock, &QDockWidget::visibilityChanged, button, &QPushButton::setChecked);
    };
    bindDockButton(modulesDockButton, modulesDock, false);
    bindDockButton(commonSettingsButton, commonSettingsDock, true);
    bindDockButton(ltr114SettingsButton, ltr114SettingsDock, true);
    bindDockButton(ltr212SettingsButton, ltr212SettingsDock, true);
    bindDockButton(simulationSettingsButton, simulationSettingsDock, true);
    connect(acqMode212Combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        update_ltr212_channel_limit();
    });

    appendInfo("Информационный виджет создан.");
}

void MainWindow::setup_plot()
{
    lineSeries114 = new QLineSeries(this);
    lineSeries212 = new QLineSeries(this);
    lineSeriesSync114 = new QLineSeries(this);
    lineSeriesSync212 = new QLineSeries(this);
    lineSeries114->setName("LTR114");
    lineSeries212->setName("LTR212");
    lineSeriesSync114->setName("LTR114");
    lineSeriesSync212->setName("LTR212");
    lineSeries114->setColor(Qt::blue);
    lineSeries212->setColor(Qt::red);
    lineSeriesSync114->setColor(Qt::blue);
    lineSeriesSync212->setColor(Qt::red);

    chart114 = new QChart();
    chart114->addSeries(lineSeries114);
    chart114->legend()->setVisible(false);
    chart114->setTitle("LTR114");

    axisX114 = new QValueAxis();
    axisX114->setTitleText("Тики");
    axisX114->setLabelFormat("%i");
    axisX114->setRange(0, m_plotWindowTicks);

    axisY114 = new QValueAxis();
    axisY114->setTitleText("Напряжение, mV");
    axisY114->setLabelFormat("%.3f");
    axisY114->setRange(-100.0, 100.0);

    chart114->addAxis(axisX114, Qt::AlignBottom);
    chart114->addAxis(axisY114, Qt::AlignLeft);
    lineSeries114->attachAxis(axisX114);
    lineSeries114->attachAxis(axisY114);

    chartView114 = new QChartView(chart114, this);
    chartView114->setMinimumHeight(240);
    chartView114->setRenderHint(QPainter::Antialiasing);

    chart212 = new QChart();
    chart212->addSeries(lineSeries212);
    chart212->legend()->setVisible(false);
    chart212->setTitle("LTR212");

    axisX212 = new QValueAxis();
    axisX212->setTitleText("Тики");
    axisX212->setLabelFormat("%i");
    axisX212->setRange(0, m_plotWindowTicks);

    axisY212 = new QValueAxis();
    axisY212->setTitleText("Напряжение, mV");
    axisY212->setLabelFormat("%.3f");
    axisY212->setRange(-100.0, 100.0);

    chart212->addAxis(axisX212, Qt::AlignBottom);
    chart212->addAxis(axisY212, Qt::AlignLeft);
    lineSeries212->attachAxis(axisX212);
    lineSeries212->attachAxis(axisY212);

    chartView212 = new QChartView(chart212, this);
    chartView212->setMinimumHeight(240);
    chartView212->setRenderHint(QPainter::Antialiasing);

    chartSync = new QChart();
    chartSync->addSeries(lineSeriesSync114);
    chartSync->addSeries(lineSeriesSync212);
    chartSync->legend()->setVisible(true);
    chartSync->setTitle("LTR114 + LTR212");

    axisXSync = new QValueAxis();
    axisXSync->setTitleText("Время, с");
    axisXSync->setLabelFormat("%.3f");
    axisXSync->setRange(0, m_plotWindowTicks);

    axisYSync = new QValueAxis();
    axisYSync->setTitleText("Напряжение, mV");
    axisYSync->setLabelFormat("%.3f");
    axisYSync->setRange(-100.0, 100.0);

    chartSync->addAxis(axisXSync, Qt::AlignBottom);
    chartSync->addAxis(axisYSync, Qt::AlignLeft);
    lineSeriesSync114->attachAxis(axisXSync);
    lineSeriesSync114->attachAxis(axisYSync);
    lineSeriesSync212->attachAxis(axisXSync);
    lineSeriesSync212->attachAxis(axisYSync);

    chartViewSync = new QChartView(chartSync, this);
    chartViewSync->setMinimumHeight(320);
    chartViewSync->setRenderHint(QPainter::Antialiasing);
    chartViewSync->setVisible(false);

    auto* rightLay = qobject_cast<QVBoxLayout*>(qobject_cast<QWidget*>(infoText->parentWidget())->layout());
    if (rightLay) {
        const int insertIndex = qMax(1, rightLay->count() - 1);
        rightLay->insertWidget(insertIndex, chartView114, 2);
        rightLay->insertWidget(insertIndex + 1, chartView212, 2);
        rightLay->insertWidget(insertIndex + 2, chartViewSync, 3);
    }
}

void MainWindow::appendInfo(const QString &msg, bool isError)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString line = QString("[%1] %2").arg(time, msg);
    if (isError) {
        infoText->setTextColor(Qt::red);
        infoText->append(line);
        infoText->setTextColor(Qt::black);
    } else {
        infoText->append(line);
    }

    qDebug() << line;
}

QWidget* MainWindow::createModuleItemWidget(int slot, const QString &name, bool ok)
{
    QWidget* w = new QWidget;
    QHBoxLayout* lay = new QHBoxLayout(w);
    lay->setContentsMargins(6, 3, 6, 3);

    QLabel* slot_label = new QLabel(QString("Слот %1").arg(slot));
    slot_label->setMinimumWidth(70);
    QLabel* name_label = new QLabel(name);
    name_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QLabel* indicator = new QLabel;
    indicator->setFixedSize(14, 14);
    indicator->setObjectName(QString("indicator_%1").arg(slot));
    indicator->setStyleSheet(QString("border-radius:7px; background-color: %1;")
                                 .arg(ok ? "green" : "red"));

    lay->addWidget(slot_label);
    lay->addWidget(name_label, 1);
    lay->addWidget(indicator);
    return w;
}

void MainWindow::setModuleStatus(int slot, bool ok)
{
    QWidget* w = moduleWidgets.value(slot, nullptr);
    if (!w) return;
    QLabel* ind = w->findChild<QLabel*>(QString("indicator_%1").arg(slot));
    if (ind) {
        ind->setStyleSheet(QString("border-radius:7px; background-color: %1;")
                               .arg(ok ? "green" : "red"));
    }
}

void MainWindow::run_ltr11_module(const QString& crate_sn, int ltr11_slot)
{
    Q_UNUSED(crate_sn)
    Q_UNUSED(ltr11_slot)
}

void MainWindow::run_ltr114_module(const QString& crate_sn, int ltr114_slot)
{
    m_crateSerial = crate_sn;
    m_ltr114Slot = ltr114_slot;
    appendInfo(QString("LTR114 найден в слоте %1. Готов к непрерывному сбору.").arg(ltr114_slot));
    update_ltr114_controls_state();
    update_plot_visibility();
}

bool MainWindow::open_ltr114_for_capture()
{
    if (m_simulationMode) {
        m_simulatedSampleAccumulator = 0.0;
        m_simulatedSignalTick = 0;
        const int simRate = simulationRateSpin ? simulationRateSpin->value() : sampleRateSpin->value();
        appendInfo(QString("Сбор запущен в режиме симуляции. Частота=%1 Гц").arg(simRate));
        return true;
    }

    if (m_crateSerial.isEmpty() || m_ltr114Slot < 0) {
        appendInfo("Не найден LTR114: невозможно запустить сбор.", true);
        return false;
    }

    m_ltr114 = std::make_unique<LTR114>();

    if (!m_ltr114->open(m_crateSerial, m_ltr114Slot)) {
        appendInfo(m_ltr114->last_result().message, true);
        m_ltr114.reset();
        setModuleStatus(m_ltr114Slot, false);
        return false;
    }

    if (!m_ltr114->get_config()) {
        appendInfo(m_ltr114->last_result().message, true);
        m_ltr114.reset();
        return false;
    }

    TLTR114_LCHANNEL lch_tbl[1];

    const int physicalChannel = qBound(0,
                                       (physicalChannel114Spin ? physicalChannel114Spin->value() : 1) - 1,
                                       LTR114_MAX_CHANNEL - 1);
    const int rangeCode = range114Combo ? range114Combo->currentData().toInt() : LTR114_URANGE_04;
    const int syncMode = syncMode114Combo ? syncMode114Combo->currentData().toInt() : LTR114_SYNCMODE_INTERNAL;
    const WORD interval = static_cast<WORD>(qBound(0,
                                                   interval114Spin ? interval114Spin->value() : 0,
                                                   65535));


    lch_tbl[0] = LTR114_CreateLChannel(LTR114_MEASMODE_U, physicalChannel, rangeCode);

    const int requestedSampleRate = sampleRateSpin->value();
    const int dividerValue = qBound(LTR114_FREQ_DIVIDER_MIN,
                                    qRound(8000.0 / qMax(1, requestedSampleRate)),
                                    LTR114_FREQ_DIVIDER_MAX);
    const DWORD divider = static_cast<DWORD>(dividerValue);


    m_ltr114->set_freq_divider(divider);
    m_ltr114->set_logical_channels(1, lch_tbl);
    m_ltr114->set_sync_mode(static_cast<DWORD>(syncMode));
    m_ltr114->set_interval(interval);

    if (!m_ltr114->apply_config()) {
        appendInfo(m_ltr114->last_result().message, true);
        m_ltr114.reset();
        return false;
    }

    const INT calibrResult = LTR114_Calibrate(m_ltr114->handle());
    if (calibrResult != LTR_OK) {
        appendInfo(make_ltr_result(LtrApiModule::Ltr114,
                                   "LTR114_Calibrate",
                                   calibrResult).message,
                   true);
        m_ltr114.reset();
        return false;
    }

    const double sampleRateHz = static_cast<double>(LTR114_FREQ(*m_ltr114->handle()));
    appendInfo(QString("LTR114 сконфигурирован. Частота=%1 Гц (FreqDivider=%2, фактическая=%3 Гц)")
                   .arg(requestedSampleRate)
                   .arg(divider)
                   .arg(QString::number(sampleRateHz, 'f', 2)));
    setModuleStatus(m_ltr114Slot, true);
    return true;
}

void MainWindow::close_ltr114_capture()
{
    if (m_simulationMode)
        return;

    if (!m_ltr114)
        return;

    if (m_ltr114Thread && m_ltr114Thread->isRunning()) {
        appendInfo("LTR114: поток еще работает, закрытие модуля отложено.", true);
        return;
    }

    m_ltr114->stop();
    m_ltr114->close();
    m_ltr114.reset();
}


bool MainWindow::open_ltr212_for_capture()
{
    if (m_simulationMode) {
        appendInfo("Симуляция LTR212 не поддерживается пока");
        return false;
    }

    if (m_crateSerial.isEmpty() || m_ltr212Slot < 0) {
        appendInfo("LTR212 не найден в крейте", true);
        return false;
    }

    m_ltr212 = std::make_unique<LTR212>();

    if (!m_ltr212->open(m_crateSerial, m_ltr212Slot, "ltr212.bio")) {
        appendInfo(m_ltr212->last_result().message, true);
        m_ltr212.reset();
        return false;
    }

    const int acqMode = acqMode212Combo ? acqMode212Combo->currentData().toInt()
                                        : LTR212_FOUR_CHANNELS_WITH_HIGH_RESOLUTION;
    const int useClb = (useClb212Check && useClb212Check->isChecked()) ? 1 : 0;
    const int useFabricClb = (useFabricClb212Check && useFabricClb212Check->isChecked()) ? 1 : 0;
    const int refVoltage = refVoltage212Combo ? refVoltage212Combo->currentData().toInt() : LTR212_REF_5V;
    const int acMode = acMode212Combo ? acMode212Combo->currentData().toInt() : 0;
    const int rangeCode = range212Combo ? range212Combo->currentData().toInt() : LTR212_SCALE_B_80;
    const int maxChannels = (acqMode == LTR212_EIGHT_CHANNELS_WITH_HIGH_RESOLUTION) ? 8 : 4;
    const int ch_count = qBound(1,
                                channelCount212Spin ? channelCount212Spin->value() : 1,
                                maxChannels);


    m_ltr212->set_size();
    m_ltr212->set_acq_mode(acqMode);
    m_ltr212->set_use_clb(useClb);
    m_ltr212->set_use_fabric_clb(useFabricClb);
    m_ltr212->set_ref_voltage(refVoltage);
    m_ltr212->set_ac_mode(acMode);


    INT ch_table[8] = {};


    for (int i = 0; i < ch_count; ++i) {
        ch_table[i] = LTR212_CreateLChannel(i + 1, rangeCode);
    }





    m_ltr212->set_logical_channels(ch_count, ch_table);


    if (!m_ltr212->apply_config()) {
        appendInfo(m_ltr212->last_result().message, true);
        m_ltr212.reset();
        return false;
    }








    appendInfo(QString("LTR212 сконфигурирован: %1 канал(ов), AcqMode=%2, REF=%3")
                   .arg(ch_count)
                   .arg(m_ltr212->handle()->AcqMode)
                   .arg(refVoltage == LTR212_REF_5V ? "5V" : "2.5V"));
    return true;
}

void MainWindow::close_ltr212_capture()
{
    if (!m_ltr212)
        return;

    if (m_ltr212Thread && m_ltr212Thread->isRunning()) {
        appendInfo("LTR212: поток еще работает, закрытие модуля отложено.", true);
        return;
    }

    m_ltr212->stop();
    m_ltr212->close();
    m_ltr212.reset();
}

void MainWindow::run_ltr212_module(const QString& crate_sn, int ltr212_slot)
{
    m_crateSerial = crate_sn;
    m_ltr212Slot = ltr212_slot;

    appendInfo(QString("LTR212 найден в слоте %1. Готов к работе.").arg(ltr212_slot));
    update_ltr212_controls_state();
    update_plot_visibility();
}

void MainWindow::refresh_plot()
{
    const qreal factor = current_unit_factor();

    auto scaled = [factor](const QVector<QPointF>& points) {
        QVector<QPointF> result;
        result.reserve(points.size());
        for (const QPointF& p : points)
            result.append(QPointF(p.x(), p.y() * factor));
        return result;
    };

    const QVector<QPointF> scaledTicks114 = scaled(m_plotPoints);
    const QVector<QPointF> scaledTicks212 = scaled(m_plotPoints212);
    const QVector<QPointF> scaledTime114 = scaled(m_timePlotPoints114);
    const QVector<QPointF> scaledTime212 = scaled(m_timePlotPoints212);

    if (lineSeries114)
        lineSeries114->replace(scaledTicks114);
    if (lineSeries212)
        lineSeries212->replace(scaledTicks212);
    if (lineSeriesSync114)
        lineSeriesSync114->replace(scaledTime114);
    if (lineSeriesSync212)
        lineSeriesSync212->replace(scaledTime212);

    auto updateAxes = [this](const QVector<QPointF>& first,
                             const QVector<QPointF>& second,
                             QValueAxis* axisX,
                             QValueAxis* axisY,
                             const QString& xTitle,
                             const QString& xFormat) {
        if (!axisX || !axisY)
            return;

        axisX->setTitleText(xTitle);
        axisX->setLabelFormat(xFormat);

        if (first.isEmpty() && second.isEmpty()) {
            axisX->setRange(0, qMax(1, m_plotWindowTicks));
            axisY->setRange(m_autoScaleY ? -1.0 : m_manualYMin,
                            m_autoScaleY ?  1.0 : m_manualYMax);
            return;
        }

        qreal firstX = first.isEmpty() ? second.first().x() : first.first().x();
        qreal maxX = first.isEmpty() ? second.last().x() : first.last().x();
        if (!second.isEmpty()) {
            firstX = qMin(firstX, second.first().x());
            maxX = qMax(maxX, second.last().x());
        }
        if (m_autoScaleX) {
            const qreal minWindowX = maxX - static_cast<qreal>(qMax(1, m_plotWindowTicks));
            const qreal minX = qMax(firstX, minWindowX);
            axisX->setRange(minX, qMax(minX + 1.0, maxX));
        } else {
            axisX->setRange(firstX, qMax(firstX + 1.0, maxX));
        }

        if (!m_autoScaleY) {
            axisY->setRange(m_manualYMin, m_manualYMax);
            return;
        }

        const QVector<QPointF>& seed = first.isEmpty() ? second : first;
        qreal minY = seed.first().y();
        qreal maxY = seed.first().y();
        auto includeY = [&minY, &maxY](const QVector<QPointF>& points) {
            for (const QPointF& p : points) {
                minY = qMin(minY, p.y());
                maxY = qMax(maxY, p.y());
            }
        };
        includeY(first);
        includeY(second);
        if (qFuzzyCompare(minY, maxY)) {
            minY -= 1.0;
            maxY += 1.0;
        }
        const qreal margin = qMax<qreal>((maxY - minY) * 0.1, current_unit_name() == "V" ? 0.000001 : 0.001);
        axisY->setRange(minY - margin, maxY + margin);
    };

    updateAxes(scaledTicks114, {}, axisX114, axisY114, "Тики", "%i");
    updateAxes(scaledTicks212, {}, axisX212, axisY212, "Тики", "%i");
    updateAxes(m_showChart114 ? scaledTime114 : QVector<QPointF>{},
               m_showChart212 ? scaledTime212 : QVector<QPointF>{},
               axisXSync,
               axisYSync,
               "Время, с",
               "%.3f");
    update_plot_visibility();
}

double MainWindow::current_unit_factor() const
{
    return (unitCombo && unitCombo->currentData().toString() == "V") ? 1.0 : 1000.0;
}

QString MainWindow::current_unit_name() const
{
    return (unitCombo && unitCombo->currentData().toString() == "V") ? "V" : "mV";
}

void MainWindow::update_axis_unit_labels()
{
    const QString title = QString("Напряжение, %1").arg(current_unit_name());
    const QString format = current_unit_name() == "V" ? "%.6f" : "%.3f";

    if (axisY114) {
        axisY114->setTitleText(title);
        axisY114->setLabelFormat(format);
    }
    if (axisY212) {
        axisY212->setTitleText(title);
        axisY212->setLabelFormat(format);
    }
    if (axisYSync) {
        axisYSync->setTitleText(title);
        axisYSync->setLabelFormat(format);
    }
    refresh_plot();
}

void MainWindow::update_plot_visibility()
{
    const bool ltr114Available = m_simulationMode || m_usingLtr114 || m_ltr114Slot >= 0 || !m_plotPoints.isEmpty();
    const bool ltr212Available = (m_simulationMode && m_simulateTwoModules) || m_usingLtr212 || m_ltr212Slot >= 0 || !m_plotPoints212.isEmpty();
    const bool secondsMode = m_plotXAxisMode == PlotXAxisMode::Seconds;

    if (chartView114)
        chartView114->setVisible(!secondsMode && m_showChart114 && ltr114Available);
    if (chartView212)
        chartView212->setVisible(!secondsMode && m_showChart212 && ltr212Available);
    if (lineSeriesSync114)
        lineSeriesSync114->setVisible(m_showChart114 && ltr114Available);
    if (lineSeriesSync212)
        lineSeriesSync212->setVisible(m_showChart212 && ltr212Available);
    if (chartViewSync) {
        const bool hasVisibleSeries = (m_showChart114 && ltr114Available)
                                      || (m_showChart212 && ltr212Available);
        chartViewSync->setVisible(secondsMode && hasVisibleSeries);
    }
}

void MainWindow::show_plot_settings_dialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Настройки графиков");

    QFormLayout* form = new QFormLayout(&dialog);

    QCheckBox* autoXCheck = new QCheckBox(&dialog);
    autoXCheck->setChecked(m_autoScaleX);

    QComboBox* xModeCombo = new QComboBox(&dialog);
    xModeCombo->addItem("Тики: два отдельных графика", 0);
    xModeCombo->addItem("Время, с: общий график", 1);
    xModeCombo->setCurrentIndex(m_plotXAxisMode == PlotXAxisMode::Seconds ? 1 : 0);

    QSpinBox* windowTicksSpin = new QSpinBox(&dialog);
    windowTicksSpin->setRange(1, 10000000);
    windowTicksSpin->setValue(qMax(1, m_plotWindowTicks));

    QCheckBox* autoYCheck = new QCheckBox(&dialog);
    autoYCheck->setChecked(m_autoScaleY);

    QDoubleSpinBox* yMinSpin = new QDoubleSpinBox(&dialog);
    yMinSpin->setRange(-1000000000.0, 1000000000.0);
    yMinSpin->setDecimals(current_unit_name() == "V" ? 6 : 3);
    yMinSpin->setValue(m_manualYMin);

    QDoubleSpinBox* yMaxSpin = new QDoubleSpinBox(&dialog);
    yMaxSpin->setRange(-1000000000.0, 1000000000.0);
    yMaxSpin->setDecimals(current_unit_name() == "V" ? 6 : 3);
    yMaxSpin->setValue(m_manualYMax);

    QCheckBox* show114Check = new QCheckBox(&dialog);
    show114Check->setChecked(m_showChart114);

    QCheckBox* show212Check = new QCheckBox(&dialog);
    show212Check->setChecked(m_showChart212);

    form->addRow("Ось X:", xModeCombo);
    form->addRow("Автоокно X:", autoXCheck);
    form->addRow("Окно X:", windowTicksSpin);
    form->addRow("Автомасштаб Y:", autoYCheck);
    form->addRow(QString("Y min, %1:").arg(current_unit_name()), yMinSpin);
    form->addRow(QString("Y max, %1:").arg(current_unit_name()), yMaxSpin);
    form->addRow("Показывать LTR114:", show114Check);
    form->addRow("Показывать LTR212:", show212Check);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    if (yMinSpin->value() >= yMaxSpin->value()) {
        QMessageBox::warning(this, "Настройки графиков", "Y min должен быть меньше Y max.");
        return;
    }

    m_autoScaleX = autoXCheck->isChecked();
    m_plotXAxisMode = xModeCombo->currentData().toInt() == 1
                          ? PlotXAxisMode::Seconds
                          : PlotXAxisMode::Ticks;
    m_plotWindowTicks = windowTicksSpin->value();
    m_autoScaleY = autoYCheck->isChecked();
    m_manualYMin = yMinSpin->value();
    m_manualYMax = yMaxSpin->value();
    m_showChart114 = show114Check->isChecked();
    m_showChart212 = show212Check->isChecked();

    refresh_plot();
}

void MainWindow::update_ltr212_controls_state()
{
    const bool workerRunning = m_ltr212Thread && m_ltr212Thread->isRunning();
    const bool available = m_ltr212Slot >= 0;
    if (ltr212SettingsGroup)
        ltr212SettingsGroup->setEnabled(!m_captureRunning && !workerRunning && available);
    if (ltr212SettingsButton)
        ltr212SettingsButton->setEnabled(available);
    if (!available && ltr212SettingsDock)
        ltr212SettingsDock->hide();
}

void MainWindow::update_ltr114_controls_state()
{
    if (!ltr114SettingsGroup)
        return;

    const bool workerRunning = m_ltr114Thread && m_ltr114Thread->isRunning();
    const bool available = !m_simulationMode && m_ltr114Slot >= 0;
    ltr114SettingsGroup->setVisible(true);
    ltr114SettingsGroup->setEnabled(available
                                    && !m_captureRunning
                                    && !workerRunning);
    if (ltr114SettingsButton)
        ltr114SettingsButton->setEnabled(available);
    if (!available && ltr114SettingsDock)
        ltr114SettingsDock->hide();
}

void MainWindow::update_simulation_controls_state()
{
    if (!simulationSettingsGroup)
        return;

    const bool workerRunning = (m_ltr114Thread && m_ltr114Thread->isRunning())
                               || (m_ltr212Thread && m_ltr212Thread->isRunning());
    simulationSettingsGroup->setVisible(true);
    simulationSettingsGroup->setEnabled(!m_captureRunning && !workerRunning && m_simulationMode);
    if (simulationSettingsButton)
        simulationSettingsButton->setEnabled(m_simulationMode);
    if (!m_simulationMode && simulationSettingsDock)
        simulationSettingsDock->hide();
}

void MainWindow::update_ltr212_channel_limit()
{
    if (!acqMode212Combo || !channelCount212Spin)
        return;

    const int acqMode = acqMode212Combo->currentData().toInt();
    const int maxChannels = (acqMode == LTR212_EIGHT_CHANNELS_WITH_HIGH_RESOLUTION) ? 8 : 4;
    const int currentValue = qMin(channelCount212Spin->value(), maxChannels);
    channelCount212Spin->setMaximum(maxChannels);
    channelCount212Spin->setValue(currentValue);
}

void MainWindow::set_capture_controls_enabled(bool enabled)
{
    const bool workerStillRunning = (m_ltr114Thread && m_ltr114Thread->isRunning())
                                    || (m_ltr212Thread && m_ltr212Thread->isRunning());
    const bool hasSupportedModule = m_simulationMode || m_ltr114Slot >= 0 || m_ltr212Slot >= 0;
    const bool controlsEnabled = enabled && !workerStillRunning && !m_captureRestartRequired;

    if (startButton)
        startButton->setEnabled(controlsEnabled && hasSupportedModule);
    if (stopButton)
        stopButton->setEnabled(!enabled && !workerStillRunning);
    if (commonSettingsGroup)
        commonSettingsGroup->setEnabled(controlsEnabled);
    if (sampleRateSpin)
        sampleRateSpin->setEnabled(controlsEnabled);
    if (plotEverySpin)
        plotEverySpin->setEnabled(controlsEnabled);
    if (chunkSizeSpin)
        chunkSizeSpin->setEnabled(controlsEnabled);
    if (saveToFileCheck)
        saveToFileCheck->setEnabled(controlsEnabled);
    if (unitCombo)
        unitCombo->setEnabled(controlsEnabled);
    update_ltr114_controls_state();
    update_ltr212_controls_state();
    update_simulation_controls_state();
}

bool MainWindow::open_capture_file(int moduleId)
{
    MeasurementWriter& writer = (moduleId == 1) ? m_captureWriter212 : m_captureWriter114;
    const QString moduleName = (moduleId == 1) ? "LTR212" : "LTR114";

    if (writer.isOpen())
        return true;

    const QString path = QString("ltr_capture_%1_%2.txt")
                             .arg(moduleName.toLower())
                             .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    int rateHz = m_simulationMode && simulationRateSpin
                     ? simulationRateSpin->value()
                     : sampleRateSpin->value();
    if (!m_simulationMode && moduleId == 0 && m_ltr114) {
        rateHz = qMax(1, qRound(static_cast<double>(LTR114_FREQ(*m_ltr114->handle()))));
    } else if (!m_simulationMode && moduleId == 1 && m_ltr212 && m_ltr212->handle()->Fs > 0.0) {
        rateHz = qMax(1, qRound(m_ltr212->handle()->Fs));
    }

    if (!writer.open(path, moduleName, rateHz, current_unit_name())) {
        appendInfo(writer.lastError(), true);
        return false;
    }

    return true;
}

bool MainWindow::append_samples_to_file(const QVector<TimedSample>& samples, int moduleId)
{
    if (samples.isEmpty())
        return true;

    MeasurementWriter& writer = (moduleId == 1) ? m_captureWriter212 : m_captureWriter114;

    if (!open_capture_file(moduleId))
        return false;

    if (!writer.append(samples, current_unit_factor())) {
        appendInfo(writer.lastError(), true);
        return false;
    }

    return true;
}

void MainWindow::close_capture_file(int moduleId)
{
    MeasurementWriter& writer = (moduleId == 1) ? m_captureWriter212 : m_captureWriter114;
    const QString path = writer.filePath();

    if (writer.isOpen()) {
        writer.close();
        if (!path.isEmpty())
            appendInfo(QString("Файл сохранён: %1").arg(path));
    }
}

void MainWindow::close_capture_files()
{
    close_capture_file(0);
    close_capture_file(1);
}

void MainWindow::setup_crate_sync()
{
    if (!m_crate || !m_crate->is_open()) {
        appendInfo("Крейт не открыт — синхрометки не настроены", true);
        return;
    }

    if (m_crate->start_second_marks()) {
        appendInfo("Синхрометки SECOND настроены (START создаётся после готовности модулей)");
    } else {
        appendInfo(m_crate->last_result().message, true);
    }
}

void MainWindow::reset_sync_state(bool needSynchronization)
{
    QMutexLocker locker(&m_syncState.mutex);
    m_syncState.needSynchronization = needSynchronization;
    m_syncState.state = needSynchronization ? SyncSessionState::WaitingBaseline : SyncSessionState::Disabled;
    m_syncState.sessionStartMsec = QDateTime::currentMSecsSinceEpoch();
    m_syncState.startMarkRequested = false;
    m_syncState.startMarkRequestMsec = 0;
    m_syncState.baselineTimeoutMs = 5000;
    m_syncState.startTimeoutMs = 5000;
    m_syncState.failureMessage.clear();
    m_syncState.ltr114 = {};
    m_syncState.ltr212 = {};
    m_syncState.timeBaseTicks = 1000000ULL;
}

bool MainWindow::start_configured_modules()
{
    if (m_usingLtr114 && m_ltr114 && !m_ltr114->start()) {
        appendInfo(m_ltr114->last_result().message, true);
        return false;
    }

    if (m_usingLtr212 && m_ltr212 && !m_ltr212->start()) {
        appendInfo(m_ltr212->last_result().message, true);
        return false;
    }

    return true;
}

void MainWindow::try_make_sync_start_mark()
{
    if (!m_captureRunning || !m_syncState.needSynchronization)
        return;

    bool shouldMakeStart = false;
    {
        QMutexLocker locker(&m_syncState.mutex);
        if (m_syncState.state == SyncSessionState::WaitingBaseline
            && m_syncState.ltr114.baselineSeen
            && m_syncState.ltr212.baselineSeen
            && !m_syncState.startMarkRequested) {
            m_syncState.state = SyncSessionState::WaitingStartEdge;
            m_syncState.startMarkRequested = true;
            m_syncState.startMarkRequestMsec = QDateTime::currentMSecsSinceEpoch();
            shouldMakeStart = true;
        }
    }

    if (!shouldMakeStart)
        return;

    if (m_crate && m_crate->is_open() && m_crate->make_start_mark()) {
        appendInfo("Sync START mark generated; workers are waiting for START edge.");
        return;
    }

    {
        QMutexLocker locker(&m_syncState.mutex);
        m_syncState.state = SyncSessionState::Failed;
        m_syncState.failureMessage = m_crate ? m_crate->last_result().message
                                             : QString("LTR_MakeStartMark failed: crate is not open.");
    }
    appendInfo(m_syncState.failureMessage, true);
    QMetaObject::invokeMethod(this, "on_stop_capture_clicked", Qt::QueuedConnection);
}

void MainWindow::on_start_capture_clicked()
{
    if (m_captureRunning) return;
    if (m_captureRestartRequired) {
        appendInfo("Предыдущий worker-поток не завершился штатно. Перезапустите приложение перед новым сбором.", true);
        return;
    }

    m_allSamples.clear();

    m_plotCounter114 = 0;
    m_plotCounter212 = 0;

    m_plotPoints.clear();
    m_plotPoints212.clear();
    m_timePlotPoints114.clear();
    m_timePlotPoints212.clear();
    m_pendingFileSamples114.clear();
    m_pendingFileSamples212.clear();
    refresh_plot();

    if (m_simulationMode) {
        if (saveToFileCheck->isChecked()) {
            if (!open_capture_file(0))
                return;
            if (m_simulateTwoModules && !open_capture_file(1)) {
                close_capture_file(0);
                return;
            }
        }

        m_captureRunning = true;
        m_simulatedSampleRate = qMax(1, simulationRateSpin ? simulationRateSpin->value() : sampleRateSpin->value());
        m_simulatedSampleAccumulator = 0.0;
        m_simulatedSampleAccumulator212 = 0.0;
        m_simulatedSignalTick = 0;
        m_simulatedSignalTick212 = 0;

        set_capture_controls_enabled(false);

        if (!m_simulationTimer) {
            m_simulationTimer = new QTimer(this);
            connect(m_simulationTimer, &QTimer::timeout, this, [this]() {
                if (m_captureRunning && m_simulationMode) {
                    process_voltage_samples(generate_simulated_samples(0), 0);
                    if (m_simulateTwoModules) {
                        process_voltage_samples(generate_simulated_samples(1), 1);
                    }
                }
            });
        }

        m_usingLtr114 = false;
        m_usingLtr212 = false;
        update_plot_visibility();
        m_simulationTimer->start(30);
        appendInfo("Симуляция запущена через QTimer.");
        return;
    }

    m_usingLtr114 = false;
    m_usingLtr212 = false;

    bool success = false;

    if (m_ltr114Slot != -1) {
        if (open_ltr114_for_capture()) {
            m_usingLtr114 = true;
            success = true;
        }
    }
    if (m_ltr212Slot != -1) {
        if (open_ltr212_for_capture()) {
            m_usingLtr212 = true;
            success = true;
        }
    }

    if (!success) {
        appendInfo("Ни LTR114, ни LTR212 не удалось запустить!", true);
        close_capture_files();
        return;
    }

    const bool needSynchronization = (m_usingLtr114 && m_usingLtr212);
    reset_sync_state(needSynchronization);

    if (needSynchronization) {
        if (!m_crate || !m_crate->is_open()) {
            appendInfo("Crate is not open; synchronized start is unavailable.", true);
            close_ltr114_capture();
            close_ltr212_capture();
            return;
        }

        m_crate->stop_sync_marks();
        if (!m_crate->start_second_marks()) {
            appendInfo(m_crate->last_result().message, true);
            close_ltr114_capture();
            close_ltr212_capture();
            return;
        }
        appendInfo("SECOND sync marks started. Workers will request START after baseline tmark is seen.");
    } else {
        appendInfo("Single-module acquisition: inter-module synchronization is disabled.");
    }

    if (!start_configured_modules()) {
        close_ltr114_capture();
        close_ltr212_capture();
        return;
    }

    if (saveToFileCheck->isChecked()) {
        if (m_usingLtr114 && !open_capture_file(0)) {
            close_ltr114_capture();
            close_ltr212_capture();
            return;
        }
        if (m_usingLtr212 && !open_capture_file(1)) {
            close_ltr114_capture();
            close_ltr212_capture();
            close_capture_file(0);
            return;
        }
    }

    if (m_syncState.needSynchronization) {
        appendInfo("Запущены оба модуля → включаем синхронизацию по tmark");
    } else {
        appendInfo("Работаем с одним модулем — синхронизация не требуется");
    }

    m_captureRunning = true;
    set_capture_controls_enabled(false);
    update_plot_visibility();

    if (m_usingLtr114 && m_ltr114) {
        m_ltr114Thread = new QThread(this);
        m_ltr114Worker = new Ltr114Worker(m_ltr114.get(), &m_syncState);
        m_ltr114Worker->moveToThread(m_ltr114Thread);

        connect(m_ltr114Thread, &QThread::started, m_ltr114Worker, &Ltr114Worker::run);
        connect(m_ltr114Worker, &Ltr114Worker::newVoltageSamples, this, [this](const QVector<TimedSample>& samples) {
            if (!m_captureRunning)
                return;
            process_voltage_samples(samples, 0);
        }, Qt::QueuedConnection);
        connect(m_ltr114Worker, &Ltr114Worker::acquisitionError, this, [this](const QString& error) {
            if (!m_captureRunning)
                return;
            appendInfo(error, true);
            QMetaObject::invokeMethod(this, "on_stop_capture_clicked", Qt::QueuedConnection);
        }, Qt::QueuedConnection);
        connect(m_ltr114Worker, &Ltr114Worker::syncReadyForStart, this,
                [this](int, quint32 start, quint32 second) {
                    appendInfo(QString("LTR114 sync baseline: START=%1 SECOND=%2").arg(start).arg(second));
                    try_make_sync_start_mark();
                }, Qt::QueuedConnection);
        connect(m_ltr114Worker, &Ltr114Worker::syncStarted, this,
                [this](int, quint32 start, quint32 second, quint64 dropped) {
                    appendInfo(QString("LTR114 sync START accepted: START=%1 SECOND=%2 dropped=%3")
                                   .arg(start)
                                   .arg(second)
                                   .arg(dropped));
                }, Qt::QueuedConnection);
        connect(m_ltr114Worker, &Ltr114Worker::finished, m_ltr114Thread, &QThread::quit);

        m_ltr114Thread->start();
    }

    if (m_usingLtr212 && m_ltr212) {
        m_ltr212Thread = new QThread(this);
        m_ltr212Worker = new Ltr212Worker(m_ltr212.get(), &m_syncState);
        m_ltr212Worker->moveToThread(m_ltr212Thread);

        connect(m_ltr212Thread, &QThread::started, m_ltr212Worker, &Ltr212Worker::run);
        connect(m_ltr212Worker, &Ltr212Worker::newVoltageSamples, this, [this](const QVector<TimedSample>& samples) {
            if (!m_captureRunning)
                return;
            process_voltage_samples(samples, 1);
        }, Qt::QueuedConnection);
        connect(m_ltr212Worker, &Ltr212Worker::acquisitionError, this, [this](const QString& error) {
            if (!m_captureRunning)
                return;
            appendInfo(error, true);
            QMetaObject::invokeMethod(this, "on_stop_capture_clicked", Qt::QueuedConnection);
        }, Qt::QueuedConnection);
        connect(m_ltr212Worker, &Ltr212Worker::syncReadyForStart, this,
                [this](int, quint32 start, quint32 second) {
                    appendInfo(QString("LTR212 sync baseline: START=%1 SECOND=%2").arg(start).arg(second));
                    try_make_sync_start_mark();
                }, Qt::QueuedConnection);
        connect(m_ltr212Worker, &Ltr212Worker::syncStarted, this,
                [this](int, quint32 start, quint32 second, quint64 dropped) {
                    appendInfo(QString("LTR212 sync START accepted: START=%1 SECOND=%2 dropped=%3")
                                   .arg(start)
                                   .arg(second)
                                   .arg(dropped));
                }, Qt::QueuedConnection);
        connect(m_ltr212Worker, &Ltr212Worker::finished, m_ltr212Thread, &QThread::quit);

        m_ltr212Thread->start();
    }
}

void MainWindow::on_stop_capture_clicked()
{
    if (!m_captureRunning) return;

    m_captureRunning = false;
    if (m_simulationTimer) {
        m_simulationTimer->stop();
    }

    stop_worker_threads();
    close_ltr114_capture();
    close_ltr212_capture();

    if (saveToFileCheck->isChecked()) {
        if (!append_samples_to_file(m_pendingFileSamples114, 0)) {
            appendInfo("Ошибка дозаписи данных в файл.", true);
        }
        if (!append_samples_to_file(m_pendingFileSamples212, 1)) {
            appendInfo("Ошибка дозаписи данных в файл LTR212.", true);
        }
        m_pendingFileSamples114.clear();
        m_pendingFileSamples212.clear();
        close_capture_files();
    } else {
        m_pendingFileSamples114.clear();
        m_pendingFileSamples212.clear();
        appendInfo("Сохранение файла отключено пользователем.");
    }

    if (m_crate && m_crate->is_open()) {
        m_crate->stop_sync_marks();
        appendInfo("Синхрометки остановлены");
    }

    set_capture_controls_enabled(true);
    update_plot_visibility();

    appendInfo("Сбор остановлен пользователем.");
}

void MainWindow::process_voltage_samples(const QVector<TimedSample>& voltageSamples, int moduleId)
{
    if (voltageSamples.isEmpty())
        return;

    const int everyN = qMax(1, plotEverySpin->value());
    QVector<QPointF>& modulePlotPoints = (moduleId == 1) ? m_plotPoints212 : m_plotPoints;
    QVector<QPointF>& moduleTimePlotPoints = (moduleId == 1) ? m_timePlotPoints212 : m_timePlotPoints114;
    QVector<TimedSample>& modulePendingFileSamples = (moduleId == 1) ? m_pendingFileSamples212 : m_pendingFileSamples114;

    quint64& plotCounter = (moduleId == 1) ? m_plotCounter212 : m_plotCounter114;

    for (const TimedSample& sample : voltageSamples) {
        m_allSamples.append(sample);
        modulePendingFileSamples.append(sample);

        plotCounter++;

        if (plotCounter % static_cast<quint64>(everyN) == 0) {
            modulePlotPoints.append(QPointF(static_cast<qreal>(plotCounter), sample.value));
            moduleTimePlotPoints.append(QPointF(static_cast<qreal>(sample.globalTick) / 1000000.0,
                                                sample.value));
        }
    }


    if (modulePlotPoints.size() > MAX_PLOT_POINTS) {
        modulePlotPoints.remove(0, modulePlotPoints.size() - MAX_PLOT_POINTS);
    }
    if (moduleTimePlotPoints.size() > MAX_PLOT_POINTS) {
        moduleTimePlotPoints.remove(0, moduleTimePlotPoints.size() - MAX_PLOT_POINTS);
    }


    if (saveToFileCheck->isChecked() && !modulePendingFileSamples.isEmpty()) {
        int rateHz = m_simulationMode && simulationRateSpin
                         ? simulationRateSpin->value()
                         : sampleRateSpin->value();
        if (!m_simulationMode && moduleId == 0 && m_ltr114) {
            rateHz = qMax(1, qRound(static_cast<double>(LTR114_FREQ(*m_ltr114->handle()))));
        } else if (!m_simulationMode && moduleId == 1 && m_ltr212 && m_ltr212->handle()->Fs > 0.0) {
            rateHz = qMax(1, qRound(m_ltr212->handle()->Fs));
        }
        const int flushEverySamples = qMax(1, rateHz);
        if (modulePendingFileSamples.size() >= flushEverySamples) {
            if (!append_samples_to_file(modulePendingFileSamples, moduleId)) {
                appendInfo("Ошибка записи в файл во время сбора.", true);
                modulePendingFileSamples.clear();
                QMetaObject::invokeMethod(this, "on_stop_capture_clicked", Qt::QueuedConnection);
                return;
            }
            modulePendingFileSamples.clear();
        }
    }

    refresh_plot();
}

QVector<TimedSample> MainWindow::generate_simulated_samples(int moduleId)
{
    QVector<TimedSample> samples;
    const double sampleRate = static_cast<double>(qMax(1, m_simulatedSampleRate));
    const double timerPeriodSec = 0.03;
    double& accumulator = (moduleId == 1) ? m_simulatedSampleAccumulator212 : m_simulatedSampleAccumulator;
    quint64& signalTick = (moduleId == 1) ? m_simulatedSignalTick212 : m_simulatedSignalTick;

    accumulator += sampleRate * timerPeriodSec;
    int samplesToGenerate = static_cast<int>(accumulator);
    accumulator -= samplesToGenerate;

    if (samplesToGenerate <= 0)
        return samples;

    samples.reserve(samplesToGenerate);

    const quint64 ticksPerSample = 1000000ULL / static_cast<quint64>(sampleRate);
    const double dt = 1.0 / sampleRate;


    const double freq1 = (moduleId == 1) ? qMin(7.0, sampleRate / 17.0) : qMin(5.0, sampleRate / 20.0);
    const double freq2 = (moduleId == 1) ? qMin(11.0, sampleRate / 10.0) : qMin(13.0, sampleRate / 12.0);
    const double amp1 = (moduleId == 1) ? 0.00095 : 0.0012;
    const double amp2 = (moduleId == 1) ? 0.00060 : 0.00045;
    const double amp3 = (moduleId == 1) ? 0.00022 : 0.00015;
    const double phase2 = (moduleId == 1) ? 1.4 : 0.8;
    const double slowFreq = (moduleId == 1) ? 0.55 : 0.35;
    constexpr double pi = 3.14159265358979323846;

    for (int i = 0; i < samplesToGenerate; ++i) {
        const double t = (static_cast<double>(signalTick) + static_cast<double>(i)) * dt;
        const double signal = amp1 * std::sin(2.0 * pi * freq1 * t)
                              + amp2 * std::sin(2.0 * pi * freq2 * t + phase2)
                              + amp3 * std::sin(2.0 * pi * slowFreq * t);

        TimedSample s{};
        s.globalTick     = signalTick * ticksPerSample + static_cast<quint64>(i) * ticksPerSample;
        s.secondMark     = static_cast<quint32>(signalTick / static_cast<quint64>(sampleRate));
        s.sampleInSecond = static_cast<quint32>(signalTick % static_cast<quint64>(sampleRate));
        s.value          = signal;

        samples.append(s);
        ++signalTick;
    }

    return samples;
}

void MainWindow::init_ltr()
{
    appendInfo("Начало поиска крейтов...");

    auto crates = Crate::enumerate_crates();
    if (crates.isEmpty()) {
        m_simulationMode = true;
        m_crateSerial = "SIMULATED_CRATE";
        m_ltr114Slot = 1;

        appendInfo("Нет подключенных крейтов. Включен режим симуляции.", true);

        QWidget* w = createModuleItemWidget(1, "LTR114 (SIM)", true);
        QListWidgetItem* it = new QListWidgetItem(modulesList);
        it->setSizeHint(w->sizeHint());
        modulesList->addItem(it);
        modulesList->setItemWidget(it, w);
        moduleWidgets.insert(1, w);

        appendInfo("Симулированный крейт создан, модуль LTR114 виртуально доступен в слоте 1.");
        run_ltr114_module(m_crateSerial, m_ltr114Slot);
        update_ltr114_controls_state();
        update_ltr212_controls_state();
        update_simulation_controls_state();
        update_plot_visibility();
        return;
    }

    appendInfo(QString("Найдено %1 крейт(ов):").arg(crates.size()));
    for (const auto& s : crates) appendInfo(QString("  %1").arg(s));

    QString crate_sn = crates.first();
    m_crateSerial = crate_sn;
    appendInfo(QString("Открываем крейт SN: %1").arg(crate_sn));

    m_crate = std::make_unique<Crate>(crate_sn);
    if (!m_crate->is_open()) {
        appendInfo(m_crate->last_result().message, true);

        startButton->setEnabled(false);
        update_ltr114_controls_state();
        update_ltr212_controls_state();
        update_simulation_controls_state();
        update_plot_visibility();
        return;
    }
    appendInfo("Крейт успешно открыт.");

    auto modules = m_crate->get_modules();
    if (modules.isEmpty()) {
        appendInfo("В крейте нет модулей", true);
        startButton->setEnabled(false);
        update_ltr114_controls_state();
        update_ltr212_controls_state();
        update_simulation_controls_state();
        update_plot_visibility();
        return;
    }

    WORD slot_count = m_crate->get_slot_count();
    appendInfo(QString("Вместимость крейта: %1 слотов").arg(slot_count));

    for (int s = 1; s <= slot_count; ++s) {
        QWidget* w = createModuleItemWidget(s, "EMPTY", false);
        QListWidgetItem* it = new QListWidgetItem(modulesList);
        it->setSizeHint(w->sizeHint());
        modulesList->addItem(it);
        modulesList->setItemWidget(it, w);
        moduleWidgets.insert(s, w);
    }

    appendInfo("Список модулей в крейте:");
    for (const auto& mod : modules) {
        int slot = mod.first;
        WORD mid = mod.second;
        QString name = module_name(mid);
        appendInfo(QString("  слот %1: %2").arg(slot).arg(name));

        QWidget* w = moduleWidgets.value(slot, nullptr);
        if (w) {
            QWidget* new_w = createModuleItemWidget(slot, name, true);
            QListWidgetItem* item = modulesList->item(slot - 1);
            if (item) {
                modulesList->setItemWidget(item, new_w);
                moduleWidgets[slot] = new_w;
            }
        }
    }

    int ltr114_slot = -1;
    for (const auto& mod : modules) {
        if (mod.second == LTR_MID_LTR114 && ltr114_slot == -1)
            ltr114_slot = mod.first;
    }

    if (ltr114_slot != -1) {
        appendInfo(QString("LTR114 IN SLOT %1").arg(ltr114_slot));
        run_ltr114_module(crate_sn, ltr114_slot);
    } else {
        appendInfo("LTR114 не найден. Можно работать с LTR212, если он доступен.", false);
    }

    int ltr212_slot = -1;
    for (const auto& mod : modules) {
        if (mod.second == LTR_MID_LTR212 && ltr212_slot == -1)
            ltr212_slot = mod.first;
    }

    if (ltr212_slot != -1) {
        appendInfo(QString("LTR212 найден в слоте %1").arg(ltr212_slot));
        run_ltr212_module(crate_sn, ltr212_slot);
    }

    const bool hasSupportedModule = (ltr114_slot != -1) || (ltr212_slot != -1);
    startButton->setEnabled(hasSupportedModule);
    if (!hasSupportedModule) {
        appendInfo("Не найдено поддерживаемых модулей LTR114/LTR212.", true);
    }
    update_ltr114_controls_state();
    update_ltr212_controls_state();
    update_simulation_controls_state();
    update_plot_visibility();

    appendInfo("Поиск модулей завершён. Соединение с крейтом оставлено открытым для синхрометок.");
}


void MainWindow::stop_worker_threads()
{
    if (m_ltr114Worker) {
        QMetaObject::invokeMethod(m_ltr114Worker, "stopAcquisition", Qt::DirectConnection);
    }
    if (m_ltr212Worker) {
        QMetaObject::invokeMethod(m_ltr212Worker, "stopAcquisition", Qt::DirectConnection);
    }

    if (m_ltr114)
        m_ltr114->stop();
    if (m_ltr212)
        m_ltr212->stop();

    constexpr unsigned long waitTimeoutMs = 5000;

    if (m_ltr114Thread) {
        m_ltr114Thread->quit();
        if (m_ltr114Thread->wait(waitTimeoutMs)) {
            delete m_ltr114Worker;
            m_ltr114Worker = nullptr;
            delete m_ltr114Thread;
            m_ltr114Thread = nullptr;
        } else {
            m_captureRestartRequired = true;
            appendInfo("LTR114: поток не завершился за 5 секунд; выполняется аварийная остановка. Перед новым сбором перезапустите приложение.", true);
            m_ltr114Thread->terminate();
            if (m_ltr114Thread->wait(1000)) {
                delete m_ltr114Worker;
                m_ltr114Worker = nullptr;
                delete m_ltr114Thread;
                m_ltr114Thread = nullptr;
            } else {
                appendInfo("LTR114: поток не удалось остановить даже аварийно; модуль закрывать небезопасно.", true);
            }
        }
    }

    if (m_ltr212Thread) {
        m_ltr212Thread->quit();
        if (m_ltr212Thread->wait(waitTimeoutMs)) {
            delete m_ltr212Worker;
            m_ltr212Worker = nullptr;
            delete m_ltr212Thread;
            m_ltr212Thread = nullptr;
        } else {
            m_captureRestartRequired = true;
            appendInfo("LTR212: поток не завершился за 5 секунд; выполняется аварийная остановка. Перед новым сбором перезапустите приложение.", true);
            m_ltr212Thread->terminate();
            if (m_ltr212Thread->wait(1000)) {
                delete m_ltr212Worker;
                m_ltr212Worker = nullptr;
                delete m_ltr212Thread;
                m_ltr212Thread = nullptr;
            } else {
                appendInfo("LTR212: поток не удалось остановить даже аварийно; модуль закрывать небезопасно.", true);
            }
        }
    }
}

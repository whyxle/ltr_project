#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QListWidget>
#include <QTextEdit>
#include <QTimer>
#include <QThread>
#include <QVector>
#include <QPair>
#include <QPointF>

#include <QMainWindow>
#include <QString>
#include <QMap>

#include "LTR/ltrapi.h"
#include "LTR/ltr11api.h"
#include "LTR/ltr114api.h"
#include "LTR/ltr212api.h"

#include "ltr/crate.h"
#include "ltr/ltr11.h"
#include "ltr/ltr114.h"
#include "ltr/ltr212.h"
#include "acquisition/ltr_workers.h"
#include "io/measurement_writer.h"

#include <memory>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

    namespace QtCharts {
    class QValueAxis;
    class QLineSeries;
    class QChart;
    class QChartView;
}


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Crate;
class LTR11;
class LTR114;
class LTR212;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QDockWidget;
class QToolBar;
class QDoubleSpinBox;
class QFile;
class QTextStream;

enum class PlotXAxisMode
{
    Ticks,
    Seconds
};








class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void init_ltr();
    static QString module_name(WORD mid);
    void setup_plot();
    bool open_ltr114_for_capture();
    void close_ltr114_capture();
    void refresh_plot();
    bool open_capture_file(int moduleId = 0);
    bool append_samples_to_file(const QVector<TimedSample>& samples, int moduleId = 0);
    void close_capture_file(int moduleId = 0);
    void close_capture_files();
    double current_unit_factor() const;
    QString current_unit_name() const;
    void update_axis_unit_labels();
    void update_plot_visibility();
    void show_plot_settings_dialog();
    void update_ltr114_controls_state();
    void update_ltr212_controls_state();
    void update_simulation_controls_state();
    void update_ltr212_channel_limit();
    void set_capture_controls_enabled(bool enabled);
    QDockWidget* create_settings_dock(const QString& title, QWidget* content, const QString& objectName);
    QDockWidget* create_modules_dock(QWidget* content);
    void show_dock(QDockWidget* dock, bool floating);

    const int CONNECTION_TIMEOUT_MS = 10000;

    Ui::MainWindow *ui;
    std::unique_ptr<Crate> m_crate;
    std::unique_ptr<LTR11> m_ltr11;
    std::unique_ptr<LTR114> m_ltr114;
    std::unique_ptr<LTR212> m_ltr212;

    QListWidget* modulesList;
    QToolBar* mainToolBar;
    QDockWidget* modulesDock;
    QPushButton* modulesDockButton;
    QTextEdit* infoText;
    QPushButton* startButton;
    QPushButton* stopButton;
    QSpinBox* sampleRateSpin;
    QSpinBox* plotEverySpin;
    QSpinBox* chunkSizeSpin;
    QCheckBox* saveToFileCheck;
    QComboBox* unitCombo;
    QPushButton* plotSettingsButton;
    QPushButton* commonSettingsButton;
    QPushButton* ltr114SettingsButton;
    QPushButton* simulationSettingsButton;
    QPushButton* ltr212SettingsButton;
    QDockWidget* commonSettingsDock;
    QDockWidget* ltr114SettingsDock;
    QDockWidget* simulationSettingsDock;
    QDockWidget* ltr212SettingsDock;
    QGroupBox* commonSettingsGroup;
    QGroupBox* ltr114SettingsGroup;
    QComboBox* range114Combo;
    QSpinBox* physicalChannel114Spin;
    QSpinBox* interval114Spin;
    QComboBox* syncMode114Combo;
    QGroupBox* simulationSettingsGroup;
    QSpinBox* simulationRateSpin;
    QGroupBox* ltr212SettingsGroup;
    QComboBox* acqMode212Combo;
    QCheckBox* useClb212Check;
    QCheckBox* useFabricClb212Check;
    QComboBox* refVoltage212Combo;
    QComboBox* acMode212Combo;
    QSpinBox* channelCount212Spin;
    QComboBox* range212Combo;
    QChartView* chartView114;
    QChartView* chartView212;
    QChartView* chartViewSync;
    QChart* chart114;
    QChart* chart212;
    QChart* chartSync;
    QLineSeries* lineSeries114;
    QLineSeries* lineSeries212 = nullptr;
    QLineSeries* lineSeriesSync114 = nullptr;
    QLineSeries* lineSeriesSync212 = nullptr;
    QValueAxis* axisX114;
    QValueAxis* axisY114;
    QValueAxis* axisX212;
    QValueAxis* axisY212;
    QValueAxis* axisXSync;
    QValueAxis* axisYSync;

    QString m_crateSerial;
    int m_ltr114Slot = -1;
    int m_ltr212Slot = -1;

    bool m_usingLtr212 = false;
    bool m_usingLtr114 = false;


    SyncState m_syncState;

    QThread* m_ltr114Thread = nullptr;
    QThread* m_ltr212Thread = nullptr;
    Ltr114Worker* m_ltr114Worker = nullptr;
    Ltr212Worker* m_ltr212Worker = nullptr;

    bool m_captureRunning = false;
    bool m_captureRestartRequired = false;
    QVector<QPointF> m_plotPoints;
    QVector<TimedSample> m_allSamples;
    QVector<TimedSample> m_pendingFileSamples114;
    QVector<TimedSample> m_pendingFileSamples212;
    MeasurementWriter m_captureWriter114;
    MeasurementWriter m_captureWriter212;
    bool m_simulationMode = false;
    bool m_simulateTwoModules = true;
    double m_simulatedSampleAccumulator = 0.0;
    double m_simulatedSampleAccumulator212 = 0.0;
    int m_simulatedSampleRate = 2000;
    quint64 m_simulatedSignalTick = 0;
    quint64 m_simulatedSignalTick212 = 0;
    QTimer* m_simulationTimer = nullptr;

    QVector<QPointF> m_plotPoints212;
    QVector<QPointF> m_timePlotPoints114;
    QVector<QPointF> m_timePlotPoints212;
    PlotXAxisMode m_plotXAxisMode = PlotXAxisMode::Ticks;
    bool m_autoScaleX = true;
    bool m_autoScaleY = true;
    bool m_showChart114 = true;
    bool m_showChart212 = true;
    int m_plotWindowTicks = 100;
    double m_manualYMin = -100.0;
    double m_manualYMax = 100.0;

    QMap<int, QWidget*> moduleWidgets;

    void init_ui_replace();
    void appendInfo(const QString &msg, bool isError = false);
    QWidget* createModuleItemWidget(int slot, const QString &name, bool ok);
    void setModuleStatus(int slot, bool ok);
    void run_ltr11_module(const QString& crate_sn, int ltr11_slot);
    void run_ltr114_module(const QString& crate_sn, int ltr114_slot);
    void run_ltr212_module(const QString& crate_sn, int ltr212_slot);
    void process_voltage_samples(const QVector<TimedSample>& voltageSamples, int moduleId = 0);
    QVector<TimedSample> generate_simulated_samples(int moduleId = 0);

    void setup_crate_sync();
    void reset_sync_state(bool needSynchronization);
    bool start_configured_modules();
    void try_make_sync_start_mark();
    void stop_worker_threads();
    bool open_ltr212_for_capture();
    void close_ltr212_capture();

    const int    MAX_PLOT_POINTS     = 1200;


    quint64 m_plotCounter114 = 0;
    quint64 m_plotCounter212 = 0;

private slots:
    void on_start_capture_clicked();
    void on_stop_capture_clicked();
};

#endif

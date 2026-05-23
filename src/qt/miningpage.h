// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MININGPAGE_H
#define BITCOIN_QT_MININGPAGE_H

#include <atomic>

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QWidget>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QLineEdit;
class QProcess;
class QPushButton;
class QRadioButton;
class QThread;
QT_END_NAMESPACE

/**
 * Worker that runs in a background thread. Sets the global miningAlgo
 * via the `setminingalgo` RPC, then repeatedly invokes
 * `generatetoaddress 1 <address> <maxtries>`, emitting signals for
 * each block found or any error. Used for SOLO mining only.
 */
class MiningWorker : public QObject
{
    Q_OBJECT

public:
    MiningWorker(const QString &address, const QString &algo, QObject *parent = nullptr);
    void requestStop() { stopRequested.store(true); }

public Q_SLOTS:
    void doWork();

Q_SIGNALS:
    void blockFound(const QString &hash);
    void errorEncountered(const QString &err);
    void finished();

private:
    QString address;
    QString algo;
    std::atomic<bool> stopRequested;
};

/**
 * Small custom widget that draws a cumulative-rewards line chart in the
 * status area. Each data point is (timestamp, cumulative amount). In Solo
 * mode the amount is BAD earned per block. In Pool mode the amount is
 * accepted-shares count. Self-contained: no QtCharts dependency.
 */
class RewardChart : public QWidget
{
    Q_OBJECT
public:
    explicit RewardChart(QWidget *parent = nullptr);

    void addPoint(double cumulativeAmount);
    void reset();
    void setUnitLabel(const QString &label);  // e.g. "BAD" or "shares"
    QSize sizeHint() const override { return QSize(480, 140); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Pt { QDateTime ts; double val; };
    QList<Pt> points;
    QString unitLabel;
};

/**
 * The Mining tab in badcoin-qt. Supports two modes:
 *   - Solo: uses MiningWorker + generatetoaddress against the local node
 *   - Pool: spawns an external stratum miner binary (pooler's `minerd` by
 *           default, configurable) as a child QProcess, parses its stdout
 *           for accepted/rejected shares, and updates the chart.
 *
 * Pool URL presets (4 slots) are persisted via QSettings so they survive
 * restarts.
 */
class MiningPage : public QWidget
{
    Q_OBJECT

public:
    explicit MiningPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~MiningPage();

    void setWalletModel(WalletModel *walletModel);

    enum MiningMode { ModeSolo = 0, ModePool = 1 };
    static const int kPresetCount = 4;

public Q_SLOTS:
    void startMining();
    void stopMining();
    void handleBlockFound(const QString &hash);
    void handleError(const QString &err);
    void handleWorkerFinished();
    void newAddress();
    void onModeChanged();
    void onPresetLoadClicked();       // the 4 preset buttons all route here
    void onSaveToPresetClicked();     // "Save current URL to…" dropdown action
    void onBrowseMinerBinary();

    // Pool-mode QProcess hooks
    void onExternalMinerStdout();
    void onExternalMinerStderr();
    void onExternalMinerFinished(int exitCode);

private:
    // UI helpers
    void buildConfigSection(class QVBoxLayout *main);
    void buildButtonRow(class QVBoxLayout *main);
    void buildStatusSection(class QVBoxLayout *main);
    void buildChartSection(class QVBoxLayout *main);
    void applyModeEnablement();

    // Preset persistence
    QString presetKeyForSlot(int slot) const;
    QString loadPreset(int slot) const;
    void savePreset(int slot, const QString &url);
    void refreshPresetButtonLabels();

    // Pool helpers
    void startSolo(const QString &addr, const QString &algo);
    void startPool(const QString &addr, const QString &algo, const QString &poolUrl);
    QString defaultMinerBinaryPath() const;

    // ── state ────────────────────────────────────────────────────────────────
    WalletModel *walletModel;

    // Top configuration
    QLineEdit *addressEdit;
    QPushButton *newAddressButton;
    QComboBox *algoCombo;

    // Mode selectors
    QRadioButton *modeSoloRadio;
    QRadioButton *modePoolRadio;

    // Pool inputs
    QLineEdit *poolUrlEdit;
    QPushButton *presetButtons[kPresetCount];
    QPushButton *saveToPresetButton;

    // Miner binary path (pool mode)
    QLineEdit *minerBinaryEdit;
    QPushButton *browseMinerBinaryButton;

    // Action buttons
    QPushButton *startButton;
    QPushButton *stopButton;

    // Status
    QLabel *statusLabel;
    QLabel *blocksMinedLabel;
    QLabel *lastHashLabel;

    // Chart
    RewardChart *rewardChart;

    // Solo worker
    QThread *workerThread;
    MiningWorker *worker;

    // Pool external process
    QProcess *externalMiner;

    // Session state
    int blocksMined;
    double cumulativeRewardBAD;     // Solo mode
    int acceptedShares;             // Pool mode
    bool hadError;
};

#endif // BITCOIN_QT_MININGPAGE_H

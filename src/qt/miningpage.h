// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MININGPAGE_H
#define BITCOIN_QT_MININGPAGE_H

#include <atomic>

#include <QColor>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPair>
#include <QVector>
#include <QWidget>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPainter;
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
 * status area. Each data point is (timestamp, cumulative amount).
 *
 * In Solo mode the chart is loaded from the wallet's real mined-reward
 * history via setHistory(): every block this wallet has ever mined. In Pool
 * mode it plots the live accepted-shares count for the current session via
 * addPoint() (pool shares are not recorded in the wallet).
 *
 * The time-range buttons (1 day / 1 week / 1 month / 1 year) call setRange();
 * the chart then windows the data to that period and plots the rewards
 * earned within it, starting from zero at the window's start.
 * Self-contained: no QtCharts dependency.
 */
class RewardChart : public QWidget
{
    Q_OBJECT
public:
    explicit RewardChart(QWidget *parent = nullptr);

    /** Time window the chart displays. */
    enum Range { RangeDay = 0, RangeWeek = 1, RangeMonth = 2, RangeYear = 3 };

    void addPoint(double cumulativeAmount);
    /** Replace the chart's data with a full, time-sorted cumulative history. */
    void setHistory(const QList<QPair<QDateTime, double> > &cumulativeHistory);
    void setRange(Range range);
    void reset();
    void setUnitLabel(const QString &label);  // e.g. "BAD" or "shares"
    QSize sizeHint() const override { return QSize(480, 160); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Pt { QDateTime ts; double val; };
    QList<Pt> points;        // (event time, cumulative all-time value)
    QString unitLabel;
    Range m_range;
};

/**
 * Animated pixel-art miner shown under the Mining Rewards chart. A native
 * Qt port of the Badcoin Pixel Miner prototype
 * (reference/core-wallet-prototypes/badcoin-miner.html): a parallax cave,
 * a miner sprite and a gold-ore block, all drawn procedurally with QPainter
 * so there are no asset files and no QtWebEngine dependency. Three states
 * track the real mining state:
 *   Idle        - the miner sleeps against the cave wall, "Z"s drift up.
 *   Mining      - the miner swings the pickaxe; sparks fly, the block cracks.
 *   Celebrating - block found: a coin burst, a gold flash, a "HA HA" bubble.
 * Runs at ~60fps while mining and throttles to ~4fps while idle, so it costs
 * almost no CPU when nothing is happening.
 */
class MinerAnimation : public QWidget
{
public:
    explicit MinerAnimation(QWidget *parent = nullptr);

    enum Mode { Idle = 0, Mining = 1, Celebrating = 2 };

    void setMode(Mode mode);   // Idle or Mining; Celebrating is set via recordBlock()
    void recordBlock();        // play the block-found celebration
    QSize sizeHint() const override { return QSize(640, 160); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    struct Particle {
        double x, y, vx, vy;
        int life, maxLife;
        double size;
        double rotPhase, rotSpeed;
        QColor color;
    };

    void recomputeLayout();
    void advance();
    void applyTimerInterval();
    void spawnImpactSparks(double gx, double gy);
    void spawnCelebrationCoins(double gx, double gy);

    void px(QPainter &p, double gx, double gy, const QColor &c,
            double w = 1.0, double h = 1.0);
    void drawBackground(QPainter &p);
    void drawBlock(QPainter &p);
    void drawMiner(QPainter &p);
    void drawPickaxe(QPainter &p, double X, double Y, bool up);
    void drawSleepingZs(QPainter &p, double gx, double gy);
    void drawParticles(QPainter &p);
    void drawSpeechBubble(QPainter &p, double gx, double gy);
    void drawPixelText(QPainter &p, double gx, double gy, const QString &text);

    Mode   m_mode;
    bool   m_miningActive;
    int    m_pixelSize;
    int    m_timerId;
    int    m_intervalMs;
    double m_clockMs;
    double m_bgOffset;
    double m_minerX, m_minerBaseY, m_blockX, m_blockY;
    int    m_blockHits;
    bool   m_swingActive;
    double m_swingPhase;
    double m_screenShake;
    double m_goldFlash;
    double m_speechTimer;
    bool   m_laughOpen;
    QVector<Particle> m_sparks;
    QVector<Particle> m_coins;
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

    // Mining Rewards chart
    void reloadRewardHistory();      // reload the chart from real wallet history
    void onChartRangeChanged(int rangeId);

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
    QButtonGroup *chartRangeGroup;   // the 1 day / week / month / year buttons

    // Animated miner under the chart
    MinerAnimation *minerAnim;

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

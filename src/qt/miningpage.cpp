// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningpage.h>

#include <qt/platformstyle.h>
#include <qt/rpcconsole.h>
#include <qt/walletmodel.h>

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>
#include <string>

// ===========================================================================
// MiningWorker: unchanged Solo-mode worker
// ===========================================================================

MiningWorker::MiningWorker(const QString &addr, const QString &algorithm, QObject *parent)
    : QObject(parent)
    , address(addr)
    , algo(algorithm)
    , stopRequested(false)
{
}

void MiningWorker::doWork()
{
    // Step 1: set the node's global miningAlgo via the `setminingalgo` RPC.
    {
        std::string setCmd = "setminingalgo \"" + algo.toStdString() + "\"";
        std::string setResult;
        try {
            bool setOk = RPCConsole::RPCExecuteCommandLine(setResult, setCmd);
            if (!setOk) {
                Q_EMIT errorEncountered(
                    QStringLiteral("setminingalgo failed: %1").arg(QString::fromStdString(setResult)));
                Q_EMIT finished();
                return;
            }
        } catch (const std::exception &e) {
            Q_EMIT errorEncountered(
                QStringLiteral("setminingalgo exception: %1").arg(QString::fromUtf8(e.what())));
            Q_EMIT finished();
            return;
        } catch (...) {
            Q_EMIT errorEncountered(QStringLiteral("Unknown error setting mining algorithm"));
            Q_EMIT finished();
            return;
        }
    }

    const std::string maxTries = "10000000";

    while (!stopRequested.load()) {
        std::string cmd = "generatetoaddress 1 " + address.toStdString() + " " + maxTries;
        std::string result;
        try {
            bool ok = RPCConsole::RPCExecuteCommandLine(result, cmd);
            if (!ok) {
                Q_EMIT errorEncountered(QString::fromStdString(result));
                break;
            }
            QString r = QString::fromStdString(result).trimmed();
            if (r.startsWith('[') && r.endsWith(']')) {
                r = r.mid(1, r.length() - 2).trimmed();
            }
            if (r.isEmpty()) continue;
            if (r.startsWith('"') && r.endsWith('"')) {
                r = r.mid(1, r.length() - 2);
            }
            Q_EMIT blockFound(r);
        } catch (const std::exception &e) {
            Q_EMIT errorEncountered(QString::fromUtf8(e.what()));
            break;
        } catch (...) {
            Q_EMIT errorEncountered(QStringLiteral("Unknown mining error"));
            break;
        }
    }
    Q_EMIT finished();
}

// ===========================================================================
// RewardChart: simple cumulative line chart
// ===========================================================================

RewardChart::RewardChart(QWidget *parent)
    : QWidget(parent), unitLabel("BAD")
{
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RewardChart::addPoint(double cumulativeAmount)
{
    Pt p;
    p.ts = QDateTime::currentDateTime();
    p.val = cumulativeAmount;
    points.append(p);
    update();
}

void RewardChart::reset()
{
    points.clear();
    update();
}

void RewardChart::setUnitLabel(const QString &label)
{
    unitLabel = label;
    update();
}

void RewardChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();
    const int pad = 12;
    QRect plot = r.adjusted(pad + 48, pad + 18, -pad, -pad - 18);

    // Background
    p.fillRect(r, QColor(252, 252, 252));
    p.setPen(QColor(220, 220, 220));
    p.drawRect(plot);

    // Title
    p.setPen(QColor(80, 80, 80));
    QFont titleFont = p.font();
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.drawText(QPoint(pad, pad + 12), QStringLiteral("Mining Rewards"));
    p.setFont(QFont());

    if (points.isEmpty()) {
        p.setPen(QColor(160, 160, 160));
        p.drawText(plot, Qt::AlignCenter,
                   QStringLiteral("No rewards yet. Start mining to see your earnings."));
        return;
    }

    // Compute ranges
    const QDateTime t0 = points.first().ts;
    const QDateTime t1 = points.last().ts;
    qint64 span = t0.msecsTo(t1);
    if (span < 1) span = 1;     // avoid divide-by-zero; force a minimum width
    double maxVal = 0;
    for (const Pt &pt : points) {
        if (pt.val > maxVal) maxVal = pt.val;
    }
    if (maxVal <= 0) maxVal = 1;

    // Y-axis labels: 0 and max
    p.setPen(QColor(120, 120, 120));
    QFont axisFont = p.font();
    axisFont.setPointSize(axisFont.pointSize() - 1);
    p.setFont(axisFont);
    QString topLbl = QString::number(maxVal, 'f', (maxVal < 10 ? 2 : 0)) + " " + unitLabel;
    QString botLbl = QStringLiteral("0");
    p.drawText(QRect(pad, plot.top() - 4, 44, 14), Qt::AlignRight, topLbl);
    p.drawText(QRect(pad, plot.bottom() - 8, 44, 14), Qt::AlignRight, botLbl);
    p.setFont(QFont());

    // Build path
    QPainterPath path;
    for (int i = 0; i < points.size(); ++i) {
        qint64 dtMs = t0.msecsTo(points[i].ts);
        double xFrac = double(dtMs) / double(span);
        double yFrac = points[i].val / maxVal;
        double x = plot.left() + xFrac * plot.width();
        double y = plot.bottom() - yFrac * plot.height();
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
    }

    // Fill under the line, softly
    QPainterPath fillPath = path;
    if (!points.isEmpty()) {
        fillPath.lineTo(plot.right(),  plot.bottom());
        fillPath.lineTo(plot.left(),   plot.bottom());
        fillPath.closeSubpath();
        p.fillPath(fillPath, QColor(212, 44, 41, 42));  // red accent at low opacity
    }

    // Stroke the line
    QPen linePen(QColor(212, 44, 41));
    linePen.setWidth(2);
    p.setPen(linePen);
    p.drawPath(path);

    // Dot at the most recent point
    if (!points.isEmpty()) {
        qint64 dtMs = t0.msecsTo(points.last().ts);
        double xFrac = span > 0 ? double(dtMs) / double(span) : 1.0;
        double yFrac = points.last().val / maxVal;
        double x = plot.left() + xFrac * plot.width();
        double y = plot.bottom() - yFrac * plot.height();
        p.setBrush(QColor(212, 44, 41));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(x, y), 4.0, 4.0);
    }

    // Current value label, bottom-right of the plot
    p.setPen(QColor(90, 90, 90));
    QString cur = QString("%1 %2").arg(QString::number(points.last().val, 'f',
        (points.last().val < 10 ? 2 : 0))).arg(unitLabel);
    p.drawText(QRect(plot.right() - 200, plot.bottom() + 4, 200, 14),
               Qt::AlignRight, cur);
}

// ===========================================================================
// MiningPage
// ===========================================================================

MiningPage::MiningPage(const PlatformStyle * /*platformStyle*/, QWidget *parent)
    : QWidget(parent)
    , walletModel(nullptr)
    , addressEdit(nullptr)
    , newAddressButton(nullptr)
    , algoCombo(nullptr)
    , modeSoloRadio(nullptr)
    , modePoolRadio(nullptr)
    , poolUrlEdit(nullptr)
    , saveToPresetButton(nullptr)
    , minerBinaryEdit(nullptr)
    , browseMinerBinaryButton(nullptr)
    , startButton(nullptr)
    , stopButton(nullptr)
    , statusLabel(nullptr)
    , blocksMinedLabel(nullptr)
    , lastHashLabel(nullptr)
    , rewardChart(nullptr)
    , workerThread(nullptr)
    , worker(nullptr)
    , externalMiner(nullptr)
    , blocksMined(0)
    , cumulativeRewardBAD(0.0)
    , acceptedShares(0)
    , hadError(false)
{
    for (int i = 0; i < kPresetCount; ++i) presetButtons[i] = nullptr;

    QVBoxLayout *main = new QVBoxLayout(this);

    QLabel *header = new QLabel(tr(
        "<h2>Badcoin CPU Mining</h2>"
        "Mine directly from this wallet. Solo mining uses your local node; "
        "pool mining hands work off to a stratum pool like pool.badcoin.dev. "
        "Yescrypt is the most CPU-friendly algorithm when it's live on the network."));
    header->setWordWrap(true);
    main->addWidget(header);

    buildConfigSection(main);
    buildButtonRow(main);
    buildStatusSection(main);
    buildChartSection(main);
    main->addStretch();

    // Wire up actions
    connect(startButton,            SIGNAL(clicked()), this, SLOT(startMining()));
    connect(stopButton,             SIGNAL(clicked()), this, SLOT(stopMining()));
    connect(newAddressButton,       SIGNAL(clicked()), this, SLOT(newAddress()));
    connect(modeSoloRadio,          SIGNAL(toggled(bool)), this, SLOT(onModeChanged()));
    connect(modePoolRadio,          SIGNAL(toggled(bool)), this, SLOT(onModeChanged()));
    connect(saveToPresetButton,     SIGNAL(clicked()), this, SLOT(onSaveToPresetClicked()));
    connect(browseMinerBinaryButton, SIGNAL(clicked()), this, SLOT(onBrowseMinerBinary()));
    for (int i = 0; i < kPresetCount; ++i) {
        connect(presetButtons[i], SIGNAL(clicked()), this, SLOT(onPresetLoadClicked()));
    }

    refreshPresetButtonLabels();
    applyModeEnablement();
}

void MiningPage::buildConfigSection(QVBoxLayout *main)
{
    QGroupBox *configBox = new QGroupBox(tr("Mining configuration"));
    QFormLayout *form = new QFormLayout(configBox);

    // Mine to address (wide)
    QHBoxLayout *addrRow = new QHBoxLayout();
    addressEdit = new QLineEdit();
    addressEdit->setPlaceholderText(tr("Payout address (coins go here when you find a block)"));
    {
        QFontMetrics fm(addressEdit->font());
        const int chars = 48;
        addressEdit->setMinimumWidth(fm.horizontalAdvance(QLatin1Char('0')) * chars + 24);
    }
    addressEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    newAddressButton = new QPushButton(tr("New"));
    newAddressButton->setToolTip(tr("Generate a fresh wallet address and use it for mining."));
    addrRow->addWidget(addressEdit, 1);
    addrRow->addWidget(newAddressButton, 0);
    form->addRow(tr("Mine to address:"), addrRow);

    // Algorithm
    algoCombo = new QComboBox();
    algoCombo->addItem(tr("Yescrypt  (recommended, CPU-friendly)"), "yescrypt");
    algoCombo->addItem(tr("Scrypt"),  "scrypt");
    algoCombo->addItem(tr("Groestl"), "groestl");
    algoCombo->addItem(tr("Skein"),   "skein");
    algoCombo->addItem(tr("SHA256d  (CPU will rarely find blocks)"), "sha256d");
    form->addRow(tr("Algorithm:"), algoCombo);

    // Mining mode (radios)
    QHBoxLayout *modeRow = new QHBoxLayout();
    modeSoloRadio = new QRadioButton(tr("Solo: mine against this node"));
    modePoolRadio = new QRadioButton(tr("Pool: connect to a stratum pool"));
    modeSoloRadio->setChecked(true);
    modeRow->addWidget(modeSoloRadio);
    modeRow->addWidget(modePoolRadio);
    modeRow->addStretch();
    form->addRow(tr("Mining mode:"), modeRow);

    // Pool URL
    poolUrlEdit = new QLineEdit();
    poolUrlEdit->setPlaceholderText(tr("stratum+tcp://pool.badcoin.dev:4032"));
    {
        QFontMetrics fm(poolUrlEdit->font());
        poolUrlEdit->setMinimumWidth(fm.horizontalAdvance(QLatin1Char('0')) * 50 + 24);
    }
    poolUrlEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Default: matches jegertabt's scrypt endpoint
    poolUrlEdit->setText(QStringLiteral("stratum+tcp://pool.badcoin.dev:4032"));
    form->addRow(tr("Pool URL:"), poolUrlEdit);

    // Presets row: 4 load buttons + a Save button
    QHBoxLayout *presetRow = new QHBoxLayout();
    for (int i = 0; i < kPresetCount; ++i) {
        presetButtons[i] = new QPushButton(tr("Slot %1").arg(i + 1));
        presetButtons[i]->setToolTip(tr("Click to load this preset into the Pool URL field."));
        // Each button identifies itself via its index stored as a property.
        presetButtons[i]->setProperty("presetSlot", i);
        presetRow->addWidget(presetButtons[i]);
    }
    saveToPresetButton = new QPushButton(tr("Save to… ▾"));
    saveToPresetButton->setToolTip(tr("Save the current Pool URL to one of the four slots."));
    presetRow->addWidget(saveToPresetButton);
    presetRow->addStretch();
    form->addRow(tr("Presets:"), presetRow);

    // Miner binary path
    QHBoxLayout *minerPathRow = new QHBoxLayout();
    minerBinaryEdit = new QLineEdit();
    minerBinaryEdit->setPlaceholderText(tr("/path/to/minerd  (leave empty to auto-detect)"));
    minerBinaryEdit->setText(defaultMinerBinaryPath());
    browseMinerBinaryButton = new QPushButton(tr("Browse…"));
    minerPathRow->addWidget(minerBinaryEdit, 1);
    minerPathRow->addWidget(browseMinerBinaryButton, 0);
    form->addRow(tr("Miner binary:"), minerPathRow);

    main->addWidget(configBox);
}

void MiningPage::buildButtonRow(QVBoxLayout *main)
{
    QHBoxLayout *buttonRow = new QHBoxLayout();
    startButton = new QPushButton(tr("Start Mining"));
    stopButton  = new QPushButton(tr("Stop"));
    stopButton->setEnabled(false);
    buttonRow->addWidget(startButton);
    buttonRow->addWidget(stopButton);
    buttonRow->addStretch();
    main->addLayout(buttonRow);
}

void MiningPage::buildStatusSection(QVBoxLayout *main)
{
    QGroupBox *statusBox = new QGroupBox(tr("Status"));
    QFormLayout *statusForm = new QFormLayout(statusBox);
    statusLabel      = new QLabel(tr("Idle"));
    blocksMinedLabel = new QLabel("0");
    lastHashLabel    = new QLabel(QStringLiteral("\u2014"));
    lastHashLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lastHashLabel->setWordWrap(true);
    statusForm->addRow(tr("State:"),                 statusLabel);
    statusForm->addRow(tr("Blocks / shares this session:"), blocksMinedLabel);
    statusForm->addRow(tr("Last block hash:"),       lastHashLabel);
    main->addWidget(statusBox);
}

void MiningPage::buildChartSection(QVBoxLayout *main)
{
    QGroupBox *chartBox = new QGroupBox(tr("Rewards (this session)"));
    QVBoxLayout *chartLayout = new QVBoxLayout(chartBox);
    rewardChart = new RewardChart();
    chartLayout->addWidget(rewardChart);
    main->addWidget(chartBox);
}

MiningPage::~MiningPage()
{
    if (worker) worker->requestStop();
    if (workerThread) {
        workerThread->quit();
        workerThread->wait(2000);
    }
    if (externalMiner) {
        externalMiner->kill();
        externalMiner->waitForFinished(2000);
    }
}

void MiningPage::setWalletModel(WalletModel *_walletModel)
{
    walletModel = _walletModel;
}

// ---------------------------------------------------------------------------
// Mode enablement + presets
// ---------------------------------------------------------------------------

void MiningPage::onModeChanged()
{
    applyModeEnablement();
}

void MiningPage::applyModeEnablement()
{
    const bool pool = modePoolRadio && modePoolRadio->isChecked();
    if (poolUrlEdit)       poolUrlEdit->setEnabled(pool);
    if (saveToPresetButton) saveToPresetButton->setEnabled(pool);
    if (minerBinaryEdit)   minerBinaryEdit->setEnabled(pool);
    if (browseMinerBinaryButton) browseMinerBinaryButton->setEnabled(pool);
    for (int i = 0; i < kPresetCount; ++i) {
        if (presetButtons[i]) presetButtons[i]->setEnabled(pool);
    }
    if (rewardChart) {
        rewardChart->setUnitLabel(pool ? QStringLiteral("shares") : QStringLiteral("BAD"));
    }
}

QString MiningPage::presetKeyForSlot(int slot) const
{
    return QStringLiteral("mining/poolPreset%1").arg(slot + 1);
}

QString MiningPage::loadPreset(int slot) const
{
    QSettings s;
    return s.value(presetKeyForSlot(slot), QString()).toString();
}

void MiningPage::savePreset(int slot, const QString &url)
{
    QSettings s;
    s.setValue(presetKeyForSlot(slot), url);
    s.sync();
}

void MiningPage::refreshPresetButtonLabels()
{
    for (int i = 0; i < kPresetCount; ++i) {
        if (!presetButtons[i]) continue;
        QString url = loadPreset(i);
        if (url.isEmpty()) {
            presetButtons[i]->setText(tr("Slot %1").arg(i + 1));
            presetButtons[i]->setToolTip(tr("Empty. Save a URL into this slot first."));
        } else {
            // Shorten for button display
            QString label = url;
            if (label.startsWith("stratum+tcp://")) label = label.mid(14);
            if (label.length() > 28) label = label.left(26) + QStringLiteral("…");
            presetButtons[i]->setText(QStringLiteral("%1: %2").arg(i + 1).arg(label));
            presetButtons[i]->setToolTip(url);
        }
    }
}

void MiningPage::onPresetLoadClicked()
{
    QObject *s = sender();
    if (!s) return;
    int slot = s->property("presetSlot").toInt();
    if (slot < 0 || slot >= kPresetCount) return;
    QString url = loadPreset(slot);
    if (url.isEmpty()) {
        QMessageBox::information(this, tr("Empty preset"),
            tr("Preset slot %1 is empty. Type a pool URL into the Pool URL field, "
               "then click \"Save to…\" and choose slot %1.").arg(slot + 1));
        return;
    }
    poolUrlEdit->setText(url);
}

void MiningPage::onSaveToPresetClicked()
{
    const QString current = poolUrlEdit->text().trimmed();
    if (current.isEmpty()) {
        QMessageBox::warning(this, tr("Nothing to save"),
            tr("Enter a Pool URL first, then click Save."));
        return;
    }

    QMenu menu(this);
    for (int i = 0; i < kPresetCount; ++i) {
        QString existing = loadPreset(i);
        QString label = existing.isEmpty()
            ? tr("Save to slot %1 (empty)").arg(i + 1)
            : tr("Overwrite slot %1  (%2)").arg(i + 1).arg(existing);
        QAction *act = menu.addAction(label);
        act->setData(i);
    }
    menu.addSeparator();
    QAction *clearHeader = menu.addAction(tr("Clear a slot…"));
    clearHeader->setEnabled(false);
    QFont hf = clearHeader->font();
    hf.setBold(true);
    clearHeader->setFont(hf);
    for (int i = 0; i < kPresetCount; ++i) {
        QString existing = loadPreset(i);
        if (existing.isEmpty()) continue;
        QAction *act = menu.addAction(tr("   Clear slot %1").arg(i + 1));
        act->setData(-(i + 1));  // negative = clear
    }

    QAction *chosen = menu.exec(saveToPresetButton->mapToGlobal(QPoint(0, saveToPresetButton->height())));
    if (!chosen) return;
    int data = chosen->data().toInt();
    if (data >= 0 && data < kPresetCount) {
        savePreset(data, current);
    } else if (data < 0) {
        int slot = -data - 1;
        savePreset(slot, QString());
    }
    refreshPresetButtonLabels();
}

// ---------------------------------------------------------------------------
// Address generation
// ---------------------------------------------------------------------------

void MiningPage::newAddress()
{
    std::string result;
    std::string cmd = "getnewaddress";
    bool ok = false;
    try {
        ok = RPCConsole::RPCExecuteCommandLine(result, cmd);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Error"), QString::fromUtf8(e.what()));
        return;
    } catch (...) {
        QMessageBox::warning(this, tr("Error"), tr("Unknown error generating address."));
        return;
    }
    if (!ok) {
        QMessageBox::warning(this, tr("Error"), QString::fromStdString(result));
        return;
    }
    QString r = QString::fromStdString(result).trimmed();
    if (r.startsWith('"') && r.endsWith('"')) r = r.mid(1, r.length() - 2);
    addressEdit->setText(r);
}

// ---------------------------------------------------------------------------
// Start / Stop dispatch
// ---------------------------------------------------------------------------

void MiningPage::startMining()
{
    QString addr = addressEdit->text().trimmed();
    if (addr.isEmpty()) {
        QMessageBox::warning(this, tr("Missing address"),
            tr("Enter a payout address first, or click 'New' to generate one."));
        return;
    }
    QString algo = algoCombo->currentData().toString();

    hadError = false;
    blocksMined = 0;
    cumulativeRewardBAD = 0.0;
    acceptedShares = 0;
    blocksMinedLabel->setText("0");
    lastHashLabel->setText(QStringLiteral("\u2014"));
    if (rewardChart) rewardChart->reset();

    if (modePoolRadio && modePoolRadio->isChecked()) {
        QString url = poolUrlEdit->text().trimmed();
        if (url.isEmpty()) {
            QMessageBox::warning(this, tr("Missing Pool URL"),
                tr("Enter a stratum URL (e.g. stratum+tcp://pool.badcoin.dev:4032) "
                   "or click a preset slot with a saved URL."));
            return;
        }
        startPool(addr, algo, url);
    } else {
        startSolo(addr, algo);
    }
}

void MiningPage::startSolo(const QString &addr, const QString &algo)
{
    worker = new MiningWorker(addr, algo);
    workerThread = new QThread(this);
    worker->moveToThread(workerThread);

    connect(workerThread, SIGNAL(started()),            worker,       SLOT(doWork()));
    connect(worker,       SIGNAL(blockFound(QString)),  this,         SLOT(handleBlockFound(QString)));
    connect(worker,       SIGNAL(errorEncountered(QString)), this,    SLOT(handleError(QString)));
    connect(worker,       SIGNAL(finished()),           this,         SLOT(handleWorkerFinished()));
    connect(worker,       SIGNAL(finished()),           workerThread, SLOT(quit()));
    connect(worker,       SIGNAL(finished()),           worker,       SLOT(deleteLater()));
    connect(workerThread, SIGNAL(finished()),           workerThread, SLOT(deleteLater()));

    workerThread->start();

    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    if (rewardChart) rewardChart->setUnitLabel(QStringLiteral("BAD"));
    statusLabel->setText(tr("Solo mining %1 \u2192 %2").arg(algo, addr.left(12) + QStringLiteral("\u2026")));
}

void MiningPage::startPool(const QString &addr, const QString &algo, const QString &poolUrl)
{
    QString minerPath = minerBinaryEdit->text().trimmed();
    if (minerPath.isEmpty()) minerPath = defaultMinerBinaryPath();
    if (minerPath.isEmpty() || !QFileInfo(minerPath).isExecutable()) {
        QMessageBox::warning(this, tr("Miner binary not found"),
            tr("Could not find an executable miner at \"%1\". Install pooler's cpuminer "
               "(minerd) or click Browse to point at a binary.").arg(minerPath));
        return;
    }

    externalMiner = new QProcess(this);
    externalMiner->setProcessChannelMode(QProcess::SeparateChannels);

    QStringList args;
    args << "-a" << algo
         << "-o" << poolUrl
         << "-u" << addr
         << "-p" << "x";

    connect(externalMiner, SIGNAL(readyReadStandardOutput()), this, SLOT(onExternalMinerStdout()));
    connect(externalMiner, SIGNAL(readyReadStandardError()),  this, SLOT(onExternalMinerStderr()));
    connect(externalMiner, SIGNAL(finished(int)),             this, SLOT(onExternalMinerFinished(int)));

    externalMiner->start(minerPath, args);
    if (!externalMiner->waitForStarted(3000)) {
        QMessageBox::warning(this, tr("Could not start miner"),
            tr("Failed to launch %1. Check that the path is correct and that the binary is executable.")
                .arg(minerPath));
        externalMiner->deleteLater();
        externalMiner = nullptr;
        return;
    }

    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    if (rewardChart) rewardChart->setUnitLabel(QStringLiteral("shares"));
    statusLabel->setText(tr("Pool mining %1 \u2192 %2").arg(algo, poolUrl));
}

void MiningPage::stopMining()
{
    if (worker) {
        worker->requestStop();
        statusLabel->setText(tr("Stopping\u2026"));
    }
    if (externalMiner && externalMiner->state() != QProcess::NotRunning) {
        externalMiner->terminate();
        // If it doesn't respect SIGTERM, escalate after 2s.
        if (!externalMiner->waitForFinished(2000)) {
            externalMiner->kill();
        }
    }
    stopButton->setEnabled(false);
}

// ---------------------------------------------------------------------------
// Solo worker signals
// ---------------------------------------------------------------------------

void MiningPage::handleBlockFound(const QString &hash)
{
    blocksMined++;
    blocksMinedLabel->setText(QString::number(blocksMined));
    lastHashLabel->setText(hash);
    // Coinbase reward for Badcoin at current height is ~2169 BAD (from
    // getblocktemplate.coinbasevalue = 216931825683 satoshis).
    const double blockRewardBAD = 2169.31825683;
    cumulativeRewardBAD += blockRewardBAD;
    if (rewardChart) rewardChart->addPoint(cumulativeRewardBAD);
}

void MiningPage::handleError(const QString &err)
{
    hadError = true;
    statusLabel->setText(tr("Error: %1").arg(err));
}

void MiningPage::handleWorkerFinished()
{
    if (!hadError) {
        statusLabel->setText(tr("Idle"));
    }
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    worker = nullptr;
    workerThread = nullptr;
}

// ---------------------------------------------------------------------------
// Pool QProcess signals
// ---------------------------------------------------------------------------

void MiningPage::onExternalMinerStdout()
{
    if (!externalMiner) return;
    QByteArray data = externalMiner->readAllStandardOutput();
    QString text = QString::fromUtf8(data);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        // Recognise pooler/cpuminer's accepted-share format:
        //   "[ts] accepted: 1/1 (100.00%), 480 khash/s (yay!!!)"
        if (line.contains(QStringLiteral("accepted:"))) {
            acceptedShares++;
            blocksMinedLabel->setText(QString::number(acceptedShares));
            if (rewardChart) rewardChart->addPoint(double(acceptedShares));
        }
        // Also catch rejected shares for status reporting
        if (line.contains(QStringLiteral("rejected"))) {
            // Don't increment counter; surface briefly in status
            statusLabel->setText(tr("Share rejected: %1").arg(line.trimmed()));
        }
    }
}

void MiningPage::onExternalMinerStderr()
{
    if (!externalMiner) return;
    QByteArray data = externalMiner->readAllStandardError();
    QString text = QString::fromUtf8(data).trimmed();
    if (!text.isEmpty()) {
        // Many miners send info (including "accepted") to stderr as well.
        // parse it the same way as stdout for share counting.
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.contains(QStringLiteral("accepted:"))) {
                acceptedShares++;
                blocksMinedLabel->setText(QString::number(acceptedShares));
                if (rewardChart) rewardChart->addPoint(double(acceptedShares));
            } else if (line.contains(QStringLiteral("Stratum connection failed")) ||
                       line.contains(QStringLiteral("Connection timed out"))) {
                hadError = true;
                statusLabel->setText(tr("Pool: %1").arg(line.trimmed()));
            }
        }
    }
}

void MiningPage::onExternalMinerFinished(int exitCode)
{
    if (!hadError) {
        statusLabel->setText(exitCode == 0 ? tr("Idle") : tr("Miner exited (code %1)").arg(exitCode));
    }
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    if (externalMiner) {
        externalMiner->deleteLater();
        externalMiner = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Miner binary path helpers
// ---------------------------------------------------------------------------

QString MiningPage::defaultMinerBinaryPath() const
{
    // Try common install locations; return the first that exists and is executable.
    QStringList candidates;
    candidates << QDir::homePath() + "/Desktop/cpuminer/minerd"
               << QDir::homePath() + "/cpuminer/minerd"
               << "/usr/local/bin/minerd"
               << "/opt/homebrew/bin/minerd";
    for (const QString &path : candidates) {
        if (QFileInfo(path).isExecutable()) return path;
    }
    return QString();
}

void MiningPage::onBrowseMinerBinary()
{
    QString chosen = QFileDialog::getOpenFileName(
        this, tr("Select miner binary"),
        QDir::homePath() + "/Desktop",
        tr("Executables (*)"));
    if (!chosen.isEmpty()) {
        minerBinaryEdit->setText(chosen);
    }
}

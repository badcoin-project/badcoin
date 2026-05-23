// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/vanityaddresspage.h>

#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <base58.h>
#include <key.h>
#include <pubkey.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <cmath>
#include <string>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QList>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

// The Base58 alphabet Bitcoin and Badcoin use. It deliberately leaves out the
// four look-alike characters 0 (zero), O (capital o), I (capital i) and
// l (lower-case L), so none of those can ever appear in an address.
static const QString kBase58Alphabet =
    QStringLiteral("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz");

// Return the characters of a set as a sorted, space-separated string.
static QString sortedChars(const QSet<QChar> &chars)
{
    QList<QChar> list = chars.values();
    std::sort(list.begin(), list.end());
    QString out;
    for (const QChar &c : list) {
        if (!out.isEmpty())
            out += QLatin1Char(' ');
        out += c;
    }
    return out;
}

// ---------------------------------------------------------------------------
// VanityWorker
// ---------------------------------------------------------------------------

VanityWorker::VanityWorker(const QStringList &_prefixes, OutputType _addressType,
                           QObject *parent)
    : QObject(parent)
    , prefixes(_prefixes)
    , addressType(_addressType)
    , stopRequested(false)
{
}

void VanityWorker::doWork()
{
    quint64 tested = 0;
    QStringList remaining = prefixes;

    // Characters genuinely seen in real addresses, so the search can screen
    // out impossible prefixes and guide the user with real data.
    QSet<QChar> firstSeen;
    QSet<QChar> secondSeen;
    QSet<QString> headsSeen;          // distinct two-character address heads
    bool screened = false;
    const quint64 kScreenAt = 30000;  // sample size before the screen runs

    while (!stopRequested.load() && !remaining.isEmpty()) {
        // Generate a genuine, fresh keypair and derive its address using the
        // wallet's own address type, so the result is a real, usable address.
        CKey key;
        key.MakeNewKey(true /* compressed */);
        const CPubKey pubkey = key.GetPubKey();
        const QString address = QString::fromStdString(
            EncodeDestination(GetDestinationForKey(pubkey, addressType)));
        ++tested;

        if (!address.isEmpty()) {
            firstSeen.insert(address.at(0));
            if (address.size() >= 2) {
                secondSeen.insert(address.at(1));
                headsSeen.insert(address.left(2));
            }
        }

        for (int i = 0; i < remaining.size(); ++i) {
            if (address.startsWith(remaining.at(i), Qt::CaseSensitive)) {
                const QString wif =
                    QString::fromStdString(CBitcoinSecret(key).ToString());
                Q_EMIT found(remaining.at(i), address, wif);
                remaining.removeAt(i);
                break;
            }
        }

        // One-time reachability screen: once a solid sample of real addresses
        // has been generated, drop any prefix whose opening characters never
        // actually occur. It is then genuinely impossible, not merely hard.
        if (!screened && tested >= kScreenAt) {
            screened = true;
            for (int i = remaining.size() - 1; i >= 0; --i) {
                const QString &pfx = remaining.at(i);
                bool reachable = true;
                if (!firstSeen.contains(pfx.at(0)))
                    reachable = false;
                else if (pfx.size() >= 2 && !headsSeen.contains(pfx.left(2)))
                    reachable = false;
                if (!reachable) {
                    Q_EMIT unreachable(pfx);
                    remaining.removeAt(i);
                }
            }
        }

        // Report progress and observed characters a few times a second.
        if ((tested & 0x7FF) == 0) {
            Q_EMIT progress(tested);
            Q_EMIT observedChars(sortedChars(firstSeen), sortedChars(secondSeen));
        }
    }

    Q_EMIT progress(tested);
    Q_EMIT observedChars(sortedChars(firstSeen), sortedChars(secondSeen));
    Q_EMIT finished();
}

// ---------------------------------------------------------------------------
// VanityAddressPage
// ---------------------------------------------------------------------------

VanityAddressPage::VanityAddressPage(const PlatformStyle *_platformStyle, QWidget *parent)
    : QWidget(parent)
    , platformStyle(_platformStyle)
    , walletModel(nullptr)
    , prefixEdit(nullptr)
    , checkLabel(nullptr)
    , difficultyLabel(nullptr)
    , guidanceLabel(nullptr)
    , startButton(nullptr)
    , stopButton(nullptr)
    , statusValue(nullptr)
    , testedValue(nullptr)
    , speedValue(nullptr)
    , elapsedValue(nullptr)
    , matchesValue(nullptr)
    , resultsTable(nullptr)
    , copyButton(nullptr)
    , saveButton(nullptr)
    , deleteButton(nullptr)
    , workerThread(nullptr)
    , worker(nullptr)
    , uiTimer(nullptr)
    , testedCount(0)
    , matchesFound(0)
    , running(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr(
        "A vanity address is an ordinary Badcoin address whose text happens to "
        "start with letters you choose. There is no trick to it: the wallet "
        "generates real keypairs one after another until one of them matches. "
        "Short prefixes are quick; every extra character makes the search "
        "roughly 58 times harder."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    // -- Desired prefixes ----------------------------------------------------
    QGroupBox *inputBox = new QGroupBox(tr("Desired prefixes"));
    QVBoxLayout *inputLayout = new QVBoxLayout(inputBox);

    QLabel *prefixHint = new QLabel(tr(
        "Enter one prefix per line. Every Badcoin address starts with a capital "
        "\"B\", so your prefix must too (for example: BAD, BEST, BANK). The "
        "character right after the B can only take certain values; if you pick "
        "an impossible one, the search detects it quickly and tells you."));
    prefixHint->setWordWrap(true);
    inputLayout->addWidget(prefixHint);

    prefixEdit = new QPlainTextEdit();
    prefixEdit->setPlaceholderText(tr("BAD\nBEST"));
    prefixEdit->setMaximumHeight(90);
    inputLayout->addWidget(prefixEdit);

    checkLabel = new QLabel();
    checkLabel->setWordWrap(true);
    checkLabel->setTextFormat(Qt::PlainText);
    inputLayout->addWidget(checkLabel);

    difficultyLabel = new QLabel();
    difficultyLabel->setWordWrap(true);
    difficultyLabel->setTextFormat(Qt::PlainText);
    inputLayout->addWidget(difficultyLabel);

    guidanceLabel = new QLabel();
    guidanceLabel->setWordWrap(true);
    guidanceLabel->setTextFormat(Qt::PlainText);
    inputLayout->addWidget(guidanceLabel);

    layout->addWidget(inputBox);

    // -- Search controls and instrumentation --------------------------------
    QGroupBox *searchBox = new QGroupBox(tr("Search"));
    QVBoxLayout *searchLayout = new QVBoxLayout(searchBox);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    startButton = new QPushButton(tr("Start search"));
    stopButton  = new QPushButton(tr("Stop"));
    stopButton->setEnabled(false);
    buttonRow->addWidget(startButton);
    buttonRow->addWidget(stopButton);
    buttonRow->addStretch();
    searchLayout->addLayout(buttonRow);

    QGridLayout *statsGrid = new QGridLayout();
    statsGrid->setHorizontalSpacing(24);
    QLabel *statusCaption  = new QLabel(tr("Status:"));
    QLabel *testedCaption  = new QLabel(tr("Addresses tested:"));
    QLabel *speedCaption   = new QLabel(tr("Speed:"));
    QLabel *elapsedCaption = new QLabel(tr("Elapsed:"));
    QLabel *matchesCaption = new QLabel(tr("Matches found:"));
    statusValue  = new QLabel(tr("Idle"));
    statusValue->setWordWrap(true);
    testedValue  = new QLabel("0");
    speedValue   = new QLabel(QStringLiteral("-"));
    elapsedValue = new QLabel(QStringLiteral("-"));
    matchesValue = new QLabel("0");
    statsGrid->addWidget(statusCaption,  0, 0);
    statsGrid->addWidget(statusValue,    0, 1);
    statsGrid->addWidget(testedCaption,  1, 0);
    statsGrid->addWidget(testedValue,    1, 1);
    statsGrid->addWidget(speedCaption,   2, 0);
    statsGrid->addWidget(speedValue,     2, 1);
    statsGrid->addWidget(elapsedCaption, 3, 0);
    statsGrid->addWidget(elapsedValue,   3, 1);
    statsGrid->addWidget(matchesCaption, 4, 0);
    statsGrid->addWidget(matchesValue,   4, 1);
    statsGrid->setColumnStretch(1, 1);
    searchLayout->addLayout(statsGrid);

    layout->addWidget(searchBox);

    // -- Results -------------------------------------------------------------
    QGroupBox *resultsBox = new QGroupBox(tr("Found addresses"));
    QVBoxLayout *resultsLayout = new QVBoxLayout(resultsBox);

    QLabel *resultsHint = new QLabel(tr(
        "Each match below is a real keypair. The private key is held only for "
        "this session: use \"Save to wallet\" to keep it. Once saved, you can "
        "receive coins to the address and export its private key from the "
        "My Addresses tab. \"Delete\" removes a row you do not want."));
    resultsHint->setWordWrap(true);
    resultsLayout->addWidget(resultsHint);

    resultsTable = new QTableWidget();
    resultsTable->setColumnCount(4);
    QStringList headers;
    headers << tr("Prefix") << tr("Address") << tr("Found") << tr("Status");
    resultsTable->setHorizontalHeaderLabels(headers);
    resultsTable->verticalHeader()->setVisible(false);
    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    resultsTable->setAlternatingRowColors(true);
    resultsTable->horizontalHeader()->setSectionResizeMode(COL_PREFIX,  QHeaderView::ResizeToContents);
    resultsTable->horizontalHeader()->setSectionResizeMode(COL_ADDRESS, QHeaderView::Stretch);
    resultsTable->horizontalHeader()->setSectionResizeMode(COL_FOUND,   QHeaderView::ResizeToContents);
    resultsTable->horizontalHeader()->setSectionResizeMode(COL_STATUS,  QHeaderView::ResizeToContents);
    resultsLayout->addWidget(resultsTable);

    QHBoxLayout *resultsButtons = new QHBoxLayout();
    copyButton   = new QPushButton(tr("Copy address"));
    saveButton   = new QPushButton(tr("Save to wallet"));
    deleteButton = new QPushButton(tr("Delete"));
    copyButton->setEnabled(false);
    saveButton->setEnabled(false);
    deleteButton->setEnabled(false);
    resultsButtons->addWidget(copyButton);
    resultsButtons->addWidget(saveButton);
    resultsButtons->addWidget(deleteButton);
    resultsButtons->addStretch();
    resultsLayout->addLayout(resultsButtons);

    layout->addWidget(resultsBox, 1);

    uiTimer = new QTimer(this);
    uiTimer->setInterval(250);

    connect(prefixEdit, SIGNAL(textChanged()), this, SLOT(onPrefixesChanged()));
    connect(startButton, SIGNAL(clicked()), this, SLOT(onStart()));
    connect(stopButton,  SIGNAL(clicked()), this, SLOT(onStop()));
    connect(resultsTable, SIGNAL(itemSelectionChanged()), this, SLOT(onSelectionChanged()));
    connect(copyButton,   SIGNAL(clicked()), this, SLOT(onCopyAddress()));
    connect(saveButton,   SIGNAL(clicked()), this, SLOT(onSaveToWallet()));
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(onDeleteResult()));
    connect(uiTimer, SIGNAL(timeout()), this, SLOT(tick()));

    refreshValidation();
}

VanityAddressPage::~VanityAddressPage()
{
    if (worker)
        worker->requestStop();
    if (workerThread) {
        workerThread->quit();
        workerThread->wait(2000);
    }
}

void VanityAddressPage::setWalletModel(WalletModel *_walletModel)
{
    walletModel = _walletModel;
    updateButtons();
}

// -- Prefix validation -------------------------------------------------------

QString VanityAddressPage::prefixError(const QString &prefix) const
{
    if (prefix.isEmpty())
        return tr("the prefix is empty.");

    // Characters outside Base58 can never appear in any address.
    for (int i = 0; i < prefix.size(); ++i) {
        const QChar c = prefix.at(i);
        if (!kBase58Alphabet.contains(c)) {
            return tr("the character \"%1\" never appears in a Badcoin address. "
                      "Addresses use Base58, which leaves out four look-alike "
                      "characters: 0 (zero), O (capital o), I (capital i) and "
                      "l (lower-case L).").arg(c);
        }
    }

    // First character. Badcoin addresses (the wallet's default type) begin
    // with a capital B; once a search has run we use what was really seen.
    const QString firstChars = observedFirst.isEmpty()
        ? QStringLiteral("B") : QString(observedFirst).remove(QLatin1Char(' '));
    if (!firstChars.contains(prefix.at(0))) {
        return tr("Badcoin addresses begin with \"%1\", so a prefix starting "
                  "with \"%2\" can never occur.")
                  .arg(firstChars).arg(prefix.at(0));
    }

    // Second character, once a search has shown which ones really occur.
    if (prefix.size() >= 2 && !observedSecond.isEmpty()) {
        const QString secondSet = QString(observedSecond).remove(QLatin1Char(' '));
        if (!secondSet.contains(prefix.at(1))) {
            return tr("no Badcoin address has \"%1\" as its second character. "
                      "After the leading B it must be one of:  %2")
                      .arg(prefix.at(1)).arg(observedSecond);
        }
    }
    return QString();
}

double VanityAddressPage::expectedAttempts(const QString &prefix)
{
    // The leading character is structurally guaranteed, so it costs nothing.
    // Each further character must be matched by chance. This is an estimate:
    // it assumes the characters are evenly distributed, which they are not
    // exactly, so treat it as a rough order-of-magnitude figure.
    const int n = prefix.size();
    if (n <= 1)
        return 1.0;
    return std::pow(58.0, double(n - 1));
}

QString VanityAddressPage::humanCount(double n)
{
    if (n < 1000.0)
        return QString::number((qint64)std::ceil(n));
    if (n < 1e6)
        return tr("%1 thousand").arg(n / 1e3, 0, 'f', 1);
    if (n < 1e9)
        return tr("%1 million").arg(n / 1e6, 0, 'f', 1);
    if (n < 1e12)
        return tr("%1 billion").arg(n / 1e9, 0, 'f', 1);
    if (n < 1e15)
        return tr("%1 trillion").arg(n / 1e12, 0, 'f', 1);
    return tr("%1 quadrillion").arg(n / 1e15, 0, 'f', 1);
}

QString VanityAddressPage::humanDuration(double seconds)
{
    if (seconds < 1.0)
        return tr("under a second");
    if (seconds < 90.0)
        return tr("about %1 seconds").arg((int)std::round(seconds));
    const double minutes = seconds / 60.0;
    if (minutes < 90.0)
        return tr("about %1 minutes").arg((int)std::round(minutes));
    const double hours = minutes / 60.0;
    if (hours < 48.0)
        return tr("about %1 hours").arg((int)std::round(hours));
    const double days = hours / 24.0;
    if (days < 730.0)
        return tr("about %1 days").arg((int)std::round(days));
    const double years = days / 365.0;
    if (years < 1e6)
        return tr("about %1 years").arg((qint64)std::round(years));
    if (years < 1e12)
        return tr("about %1 million years").arg(years / 1e6, 0, 'f', 0);
    return tr("far longer than the age of the universe");
}

QStringList VanityAddressPage::validPrefixes() const
{
    QStringList out;
    const QStringList lines = prefixEdit->toPlainText().split('\n');
    for (const QString &raw : lines) {
        const QString p = raw.trimmed();
        if (p.isEmpty() || !prefixError(p).isEmpty())
            continue;
        if (!out.contains(p))
            out << p;
    }
    return out;
}

void VanityAddressPage::refreshValidation()
{
    const QStringList lines = prefixEdit->toPlainText().split('\n');
    QStringList feedback;
    QStringList diffs;
    QStringList seen;

    for (const QString &raw : lines) {
        const QString p = raw.trimmed();
        if (p.isEmpty())
            continue;
        if (seen.contains(p))
            continue;
        seen << p;

        const QString err = prefixError(p);
        if (err.isEmpty()) {
            const double att = expectedAttempts(p);
            feedback << tr("\"%1\": ready to search.").arg(p);
            diffs << tr("    \"%1\": about %2 addresses to test on average.")
                       .arg(p).arg(humanCount(att));
        } else {
            feedback << tr("\"%1\": %2").arg(p).arg(err);
        }
    }

    if (feedback.isEmpty())
        checkLabel->setText(tr("Enter one or more desired prefixes above, one per line."));
    else
        checkLabel->setText(feedback.join(QStringLiteral("\n")));

    if (diffs.isEmpty()) {
        difficultyLabel->setText(QString());
    } else {
        difficultyLabel->setText(tr(
            "Difficulty estimate (rough; characters are not perfectly evenly "
            "distributed, so real results vary):\n%1\n\n"
            "How long this takes depends on your computer's speed, which is "
            "shown once the search starts.").arg(diffs.join(QStringLiteral("\n"))));
    }

    updateButtons();
}

void VanityAddressPage::updateGuidance()
{
    QStringList parts;
    if (!observedSecond.isEmpty()) {
        const QString first = observedFirst.isEmpty()
            ? QStringLiteral("B") : observedFirst;
        parts << tr("From the addresses generated so far: every address starts "
                    "with %1, and the character right after it is one of:  %2")
                    .arg(first).arg(observedSecond);
    }
    if (!unreachablePrefixes.isEmpty()) {
        parts << tr("Skipped as impossible (no Badcoin address can begin this "
                    "way):  %1").arg(unreachablePrefixes.join(QStringLiteral(", ")));
    }
    guidanceLabel->setText(parts.join(QStringLiteral("\n\n")));
}

// -- Search lifecycle --------------------------------------------------------

void VanityAddressPage::onPrefixesChanged()
{
    refreshValidation();
}

void VanityAddressPage::onStart()
{
    if (running)
        return;
    const QStringList prefixes = validPrefixes();
    if (prefixes.isEmpty())
        return;

    // Generate the wallet's own address type, so a found address is exactly
    // the kind the wallet hands out (and starts with the same letter).
    OutputType addrType = OUTPUT_TYPE_P2SH_SEGWIT;
    if (walletModel) {
        const OutputType t = walletModel->getDefaultAddressType();
        if (t != OUTPUT_TYPE_NONE)
            addrType = t;
    }

    testedCount = 0;
    matchesFound = 0;
    pendingPrefixes = prefixes;
    unreachablePrefixes.clear();
    updateGuidance();

    testedValue->setText("0");
    speedValue->setText(tr("measuring..."));
    elapsedValue->setText(QStringLiteral("0s"));
    matchesValue->setText("0");
    statusValue->setText(tr("Searching..."));

    setRunning(true);
    clock.start();

    worker = new VanityWorker(prefixes, addrType);
    workerThread = new QThread(this);
    worker->moveToThread(workerThread);

    connect(workerThread, SIGNAL(started()), worker, SLOT(doWork()));
    connect(worker, SIGNAL(found(QString,QString,QString)),
            this,   SLOT(onFound(QString,QString,QString)));
    connect(worker, SIGNAL(progress(quint64)), this, SLOT(onProgress(quint64)));
    connect(worker, SIGNAL(unreachable(QString)), this, SLOT(onUnreachable(QString)));
    connect(worker, SIGNAL(observedChars(QString,QString)),
            this,   SLOT(onObservedChars(QString,QString)));
    connect(worker, SIGNAL(finished()), this,         SLOT(onWorkerFinished()));
    connect(worker, SIGNAL(finished()), workerThread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker,       SLOT(deleteLater()));
    connect(workerThread, SIGNAL(finished()), workerThread, SLOT(deleteLater()));

    workerThread->start();
    uiTimer->start();
}

void VanityAddressPage::onStop()
{
    if (worker)
        worker->requestStop();
    statusValue->setText(tr("Stopping..."));
    stopButton->setEnabled(false);
}

void VanityAddressPage::onProgress(quint64 tested)
{
    testedCount = tested;
}

void VanityAddressPage::onUnreachable(const QString &prefix)
{
    pendingPrefixes.removeAll(prefix);
    if (!unreachablePrefixes.contains(prefix))
        unreachablePrefixes << prefix;
    updateGuidance();
}

void VanityAddressPage::onObservedChars(const QString &firstChars, const QString &secondChars)
{
    observedFirst = firstChars;
    observedSecond = secondChars;
    updateGuidance();
}

void VanityAddressPage::onFound(const QString &prefix, const QString &address, const QString &wif)
{
    pendingPrefixes.removeAll(prefix);
    ++matchesFound;
    matchesValue->setText(QString::number(matchesFound));

    const int row = resultsTable->rowCount();
    resultsTable->insertRow(row);

    QTableWidgetItem *prefixItem  = new QTableWidgetItem(prefix);
    QTableWidgetItem *addressItem = new QTableWidgetItem(address);
    QTableWidgetItem *foundItem   = new QTableWidgetItem(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    QTableWidgetItem *statusItem  = new QTableWidgetItem(tr("Not yet saved"));

    // The private key rides along on the address item, never shown in a column.
    addressItem->setData(Qt::UserRole, wif);
    statusItem->setData(Qt::UserRole, false);   // false = not saved to wallet

    QFont mono = addressItem->font();
    mono.setStyleHint(QFont::TypeWriter);
    mono.setFamily(QStringLiteral("Monospace"));
    addressItem->setFont(mono);

    resultsTable->setItem(row, COL_PREFIX,  prefixItem);
    resultsTable->setItem(row, COL_ADDRESS, addressItem);
    resultsTable->setItem(row, COL_FOUND,   foundItem);
    resultsTable->setItem(row, COL_STATUS,  statusItem);

    if (resultsTable->currentRow() < 0)
        resultsTable->selectRow(row);
}

void VanityAddressPage::onWorkerFinished()
{
    uiTimer->stop();
    setRunning(false);
    worker = nullptr;
    workerThread = nullptr;

    // One last refresh of the live counters.
    tick();

    const int skipped = unreachablePrefixes.size();
    if (matchesFound == 0 && skipped > 0 && pendingPrefixes.isEmpty())
        statusValue->setText(tr("Stopped. None of those prefixes can occur in a "
                                "Badcoin address."));
    else if (matchesFound == 0)
        statusValue->setText(tr("Stopped. No matches found."));
    else if (pendingPrefixes.isEmpty() && skipped == 0)
        statusValue->setText(tr("Done. Every prefix was found."));
    else if (pendingPrefixes.isEmpty())
        statusValue->setText(tr("Done. %1 found; %2 skipped as impossible.")
                                .arg(matchesFound).arg(skipped));
    else
        statusValue->setText(tr("Stopped. %1 found, %2 still open.")
                                .arg(matchesFound).arg(pendingPrefixes.size()));
    updateButtons();
}

void VanityAddressPage::tick()
{
    if (testedValue)
        testedValue->setText(QLocale().toString((qulonglong)testedCount));

    const qint64 ms = clock.isValid() ? clock.elapsed() : 0;
    const double secs = ms / 1000.0;

    // Compact elapsed display.
    QString elapsed;
    if (secs < 60.0) {
        elapsed = QString::number((int)secs) + QStringLiteral("s");
    } else if (secs < 3600.0) {
        elapsed = QStringLiteral("%1m %2s")
                      .arg((int)(secs / 60)).arg((int)std::fmod(secs, 60.0), 2, 10, QChar('0'));
    } else {
        elapsed = QStringLiteral("%1h %2m")
                      .arg((int)(secs / 3600)).arg((int)(std::fmod(secs, 3600.0) / 60), 2, 10, QChar('0'));
    }
    if (elapsedValue)
        elapsedValue->setText(elapsed);

    if (secs > 0.3 && testedCount > 0) {
        const double rate = testedCount / secs;
        speedValue->setText(tr("%1 addresses / second").arg(QLocale().toString((qulonglong)rate)));

        // Honest live ETA for the hardest prefix still being searched for.
        if (running && !pendingPrefixes.isEmpty() && rate > 0.0) {
            double hardest = 0.0;
            for (const QString &p : pendingPrefixes)
                hardest = std::max(hardest, expectedAttempts(p));
            const double eta = hardest / rate;
            statusValue->setText(tr("Searching... longest remaining prefix should "
                                    "take %1 on average.").arg(humanDuration(eta)));
        }
    }
}

// -- Results actions ---------------------------------------------------------

void VanityAddressPage::onSelectionChanged()
{
    updateButtons();
}

void VanityAddressPage::onCopyAddress()
{
    const int row = resultsTable->currentRow();
    if (row < 0)
        return;
    QTableWidgetItem *item = resultsTable->item(row, COL_ADDRESS);
    if (item)
        QApplication::clipboard()->setText(item->text());
}

void VanityAddressPage::onSaveToWallet()
{
    if (!walletModel)
        return;
    const int row = resultsTable->currentRow();
    if (row < 0)
        return;

    QTableWidgetItem *addressItem = resultsTable->item(row, COL_ADDRESS);
    QTableWidgetItem *prefixItem  = resultsTable->item(row, COL_PREFIX);
    QTableWidgetItem *statusItem  = resultsTable->item(row, COL_STATUS);
    if (!addressItem)
        return;

    if (statusItem && statusItem->data(Qt::UserRole).toBool()) {
        QMessageBox::information(this, tr("Already saved"),
            tr("This address is already in your wallet."));
        return;
    }

    const QString address = addressItem->text();
    const QString wif     = addressItem->data(Qt::UserRole).toString();
    const QString prefix  = prefixItem ? prefixItem->text() : QString();
    if (wif.isEmpty()) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("This row has no stored private key."));
        return;
    }

    if (QMessageBox::question(this, tr("Save to wallet"),
            tr("Add this vanity address and its private key to your wallet?\n\n%1\n\n"
               "Once saved you can receive coins to it and export the private key "
               "from the My Addresses tab.").arg(address),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    // Saving a key may need the wallet unlocked.
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid())
        return;

    const QString label = prefix.isEmpty() ? tr("Vanity address")
                                            : tr("Vanity %1").arg(prefix);
    QString errorOut;
    if (!walletModel->importVanityAddress(wif, label, errorOut)) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("The address could not be saved.\n\n%1").arg(errorOut));
        return;
    }

    if (statusItem) {
        statusItem->setText(tr("Saved to wallet"));
        statusItem->setData(Qt::UserRole, true);
    }
    QMessageBox::information(this, tr("Saved"),
        tr("The vanity address is now in your wallet. You can export its private "
           "key from the My Addresses tab."));
    updateButtons();
}

void VanityAddressPage::onDeleteResult()
{
    const int row = resultsTable->currentRow();
    if (row < 0)
        return;

    QTableWidgetItem *statusItem = resultsTable->item(row, COL_STATUS);
    const bool saved = statusItem && statusItem->data(Qt::UserRole).toBool();

    QString question = tr("Remove this address from the results list?");
    if (saved)
        question += tr("\n\nIt has already been saved to your wallet. Removing it "
                       "here does not remove it from the wallet; use the Remove "
                       "button on the My Addresses tab for that.");
    else
        question += tr("\n\nIt has not been saved, so its private key will be "
                       "discarded.");

    if (QMessageBox::question(this, tr("Delete result"), question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    resultsTable->removeRow(row);
    updateButtons();
}

// -- UI state ----------------------------------------------------------------

void VanityAddressPage::setRunning(bool _running)
{
    running = _running;
    prefixEdit->setReadOnly(_running);
    updateButtons();
}

void VanityAddressPage::updateButtons()
{
    startButton->setEnabled(!running && !validPrefixes().isEmpty());
    stopButton->setEnabled(running);

    const int row = resultsTable ? resultsTable->currentRow() : -1;
    const bool hasSelection = row >= 0;
    copyButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);

    bool canSave = false;
    if (hasSelection && walletModel) {
        QTableWidgetItem *statusItem = resultsTable->item(row, COL_STATUS);
        const bool alreadySaved = statusItem && statusItem->data(Qt::UserRole).toBool();
        canSave = !alreadySaved;
    }
    saveButton->setEnabled(canSave);
}

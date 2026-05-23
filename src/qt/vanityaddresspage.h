// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_VANITYADDRESSPAGE_H
#define BITCOIN_QT_VANITYADDRESSPAGE_H

#include <atomic>

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QWidget>

class PlatformStyle;
class WalletModel;

QT_BEGIN_NAMESPACE
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QThread;
class QTimer;
QT_END_NAMESPACE

/**
 * Background worker that brute-forces real Badcoin keypairs looking for an
 * address whose text starts with one of the requested prefixes. Runs in its
 * own QThread: it loops generating a fresh CKey, derives the legacy (P2PKH)
 * address, and compares it case-sensitively against every active prefix.
 *
 * There is no shortcut and no fakery. Every reported address is a genuine
 * keypair the wallet can import and spend from, and the only way to find one
 * is to keep trying. When a prefix is matched it is dropped from the search;
 * once every prefix has been found the worker finishes on its own.
 */
class VanityWorker : public QObject
{
    Q_OBJECT

public:
    explicit VanityWorker(const QStringList &prefixes, QObject *parent = nullptr);
    void requestStop() { stopRequested.store(true); }

public Q_SLOTS:
    void doWork();

Q_SIGNALS:
    /** A keypair was found. wif is the private key in wallet-import format. */
    void found(const QString &prefix, const QString &address, const QString &wif);
    /** Periodic progress: total candidate addresses tested so far. */
    void progress(quint64 tested);
    void finished();

private:
    QStringList prefixes;
    std::atomic<bool> stopRequested;
};

/**
 * The Vanity Address tab in badcoin-qt.
 *
 * The user enters one or more desired address prefixes (one per line). The
 * page validates each one and, for any prefix that can never exist, explains
 * the exact reason in plain language before the search ever starts. It then
 * shows an honest difficulty estimate and runs VanityWorker to brute-force
 * real keypairs. A found address can be saved straight into the wallet.
 *
 * The tool never fabricates a result. A prefix that cannot occur (wrong first
 * letter, or a character outside the Base58 alphabet) is rejected up front,
 * so the search only ever runs on prefixes that are genuinely reachable.
 */
class VanityAddressPage : public QWidget
{
    Q_OBJECT

public:
    explicit VanityAddressPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~VanityAddressPage();

    void setWalletModel(WalletModel *walletModel);

private Q_SLOTS:
    void onPrefixesChanged();
    void onStart();
    void onStop();
    void onFound(const QString &prefix, const QString &address, const QString &wif);
    void onProgress(quint64 tested);
    void onWorkerFinished();
    void onSelectionChanged();
    void onCopyAddress();
    void onSaveToWallet();
    void tick();

private:
    // Returns an empty string if the prefix is reachable, otherwise a
    // plain-language explanation of why it can never be found.
    static QString prefixError(const QString &prefix);
    // Approximate number of candidate addresses needed, on average, to hit
    // the prefix (~58^(length-1); the leading 'B' is always satisfied).
    static double expectedAttempts(const QString &prefix);
    static QString humanCount(double n);
    static QString humanDuration(double seconds);

    QStringList validPrefixes() const;
    void refreshValidation();
    void setRunning(bool running);
    void updateButtons();

    enum Column { COL_PREFIX = 0, COL_ADDRESS = 1, COL_FOUND = 2, COL_STATUS = 3 };

    const PlatformStyle *platformStyle;
    WalletModel *walletModel;

    // Input
    QPlainTextEdit *prefixEdit;
    QLabel *checkLabel;        // per-prefix validation feedback
    QLabel *difficultyLabel;   // difficulty / time estimate
    QPushButton *startButton;
    QPushButton *stopButton;

    // Instrumentation
    QLabel *statusValue;
    QLabel *testedValue;
    QLabel *speedValue;
    QLabel *elapsedValue;
    QLabel *matchesValue;

    // Results
    QTableWidget *resultsTable;
    QPushButton *copyButton;
    QPushButton *saveButton;

    // Worker
    QThread *workerThread;
    VanityWorker *worker;

    QTimer *uiTimer;
    QElapsedTimer clock;
    quint64 testedCount;
    int matchesFound;
    bool running;
    QStringList pendingPrefixes;   // valid prefixes not yet found
};

#endif // BITCOIN_QT_VANITYADDRESSPAGE_H

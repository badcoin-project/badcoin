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

enum OutputType : int;

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
 * address whose text starts with one of the requested prefixes.
 *
 * It generates addresses using the wallet's own default address type (so a
 * found address is exactly the kind the wallet hands out, and starts with the
 * same letter), then compares each one case-sensitively against every active
 * prefix. There is no shortcut and no fakery: every reported address is a
 * genuine keypair the wallet can import and spend from.
 *
 * After a fixed sample of real addresses the worker screens the prefixes: any
 * whose opening characters never actually occur (for example a character that
 * cannot follow the leading "B") is reported as unreachable and dropped, so
 * the search never runs forever on something impossible. The worker also
 * reports the characters it has genuinely observed, as guidance for the user.
 */
class VanityWorker : public QObject
{
    Q_OBJECT

public:
    VanityWorker(const QStringList &prefixes, OutputType addressType,
                 QObject *parent = nullptr);
    void requestStop() { stopRequested.store(true); }

public Q_SLOTS:
    void doWork();

Q_SIGNALS:
    /** A keypair was found. wif is the private key in wallet-import format. */
    void found(const QString &prefix, const QString &address, const QString &wif);
    /** Periodic progress: total candidate addresses tested so far. */
    void progress(quint64 tested);
    /** A prefix that no real address can begin with; the search dropped it. */
    void unreachable(const QString &prefix);
    /** Characters genuinely seen in real addresses at positions 1 and 2, each
        as a sorted, space-separated list. Pure guidance for the user. */
    void observedChars(const QString &firstChars, const QString &secondChars);
    void finished();

private:
    QStringList prefixes;
    OutputType addressType;
    std::atomic<bool> stopRequested;
};

/**
 * The Vanity Address tab in badcoin-qt.
 *
 * The user enters one or more desired address prefixes (one per line). The
 * page validates each one, runs VanityWorker to brute-force real keypairs of
 * the wallet's own address type, and shows the matches it finds. A found
 * address can be saved straight into the wallet.
 *
 * The tool never fabricates a result and never claims more than it knows. A
 * prefix with a character outside Base58 is rejected up front; a prefix that
 * looks valid but turns out to be impossible (no real address can begin with
 * it) is detected during the search, reported plainly, and skipped. Once a
 * search has run, the page knows which characters really occur and validates
 * later prefixes against that.
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
    void onUnreachable(const QString &prefix);
    void onObservedChars(const QString &firstChars, const QString &secondChars);
    void onWorkerFinished();
    void onSelectionChanged();
    void onCopyAddress();
    void onSaveToWallet();
    void onDeleteResult();
    void tick();

private:
    // Empty if the prefix has no detectable problem, otherwise a plain-language
    // explanation. Uses characters observed in earlier searches when available.
    QString prefixError(const QString &prefix) const;
    // Approximate number of candidate addresses needed, on average (estimate).
    static double expectedAttempts(const QString &prefix);
    static QString humanCount(double n);
    static QString humanDuration(double seconds);

    QStringList validPrefixes() const;
    void refreshValidation();
    void updateGuidance();
    void setRunning(bool running);
    void updateButtons();

    enum Column { COL_PREFIX = 0, COL_ADDRESS = 1, COL_FOUND = 2, COL_STATUS = 3 };

    const PlatformStyle *platformStyle;
    WalletModel *walletModel;

    // Input
    QPlainTextEdit *prefixEdit;
    QLabel *checkLabel;        // per-prefix validation feedback
    QLabel *difficultyLabel;   // difficulty / time estimate
    QLabel *guidanceLabel;     // characters really seen; impossible prefixes
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
    QPushButton *deleteButton;

    // Worker
    QThread *workerThread;
    VanityWorker *worker;

    QTimer *uiTimer;
    QElapsedTimer clock;
    quint64 testedCount;
    int matchesFound;
    bool running;
    QStringList pendingPrefixes;     // valid prefixes not yet found or skipped
    QStringList unreachablePrefixes; // prefixes the search proved impossible
    QString observedFirst;           // address characters seen in position 1
    QString observedSecond;          // address characters seen in position 2
};

#endif // BITCOIN_QT_VANITYADDRESSPAGE_H

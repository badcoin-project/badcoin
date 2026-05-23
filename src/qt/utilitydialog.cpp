// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/utilitydialog.h>

#include <qt/forms/ui_helpmessagedialog.h>

#include <qt/bitcoingui.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/intro.h>
#include <qt/paymentrequestplus.h>
#include <qt/guiutil.h>

#include <clientversion.h>
#include <init.h>
#include <util.h>

#include <stdio.h>

#include <QCloseEvent>
#include <QLabel>
#include <QRegExp>
#include <QTextTable>
#include <QTextCursor>
#include <QVBoxLayout>

/** "Help message" or "About" dialog box */
HelpMessageDialog::HelpMessageDialog(QWidget *parent, bool about) :
    QDialog(parent),
    ui(new Ui::HelpMessageDialog)
{
    ui->setupUi(this);

    QString version = tr(PACKAGE_NAME) + " " + tr("version") + " " + QString::fromStdString(FormatFullVersion());
    /* On x86 add a bit specifier to the version so that users can distinguish between
     * 32 and 64 bit builds. On other architectures, 32/64 bit may be more ambiguous.
     */
#if defined(__x86_64__)
    version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__ )
    version += " " + tr("(%1-bit)").arg(32);
#endif

    if (about)
    {
        setWindowTitle(tr("About %1").arg(tr(PACKAGE_NAME)));

        // Badcoin: an identity-forward About panel. The version line is the
        // real build version; the rest is the Phoenix Edition story and a
        // network snapshot whose figures come from the consensus code.
        const QString br = QStringLiteral("<br>");
        const QString rule = QStringLiteral("<hr>");

        QString aboutHTML;
        aboutHTML += "<p><b>" + version + "</b>" + br
                   + "<b>" + tr("Phoenix Edition") + "</b>" + br
                   + "<i>" + tr("Crypto Mining for People with Bad Computers") + "</i></p>";

        aboutHTML += "<p>" + tr(
            "Badcoin is an open source community blockchain created for education, "
            "experimentation, and entertainment. It is built to make mining accessible: "
            "multiple proof-of-work algorithms, and a welcome for everyday hardware rather "
            "than only industrial mining operations.") + "</p>";

        aboutHTML += "<p><b>"
                   + tr("Mine BAD. Learn crypto. Break things. Build things.")
                   + "</b></p>";

        aboutHTML += rule + "<p><b>" + tr("Mission") + "</b>" + br
                   + tr("Make blockchain participation inclusive.") + br
                   + tr("Support miners with GOOD computers, BAD computers, and everything in between.") + br
                   + tr("Be a playground for learning distributed systems, wallets, mining, and crypto economics.")
                   + "</p>";

        aboutHTML += rule + "<p><b>" + tr("Network") + "</b>" + br
                   + tr("Algorithms: SHA256d, Scrypt, Groestl, Skein, Yescrypt") + br
                   + tr("Maximum supply: 21,000,000,000 BAD") + br
                   + tr("Block reward: 2,170 BAD, reduced by periodic halving") + br
                   + tr("Block cadence: about 1 minute aggregate, about 5 minutes per algorithm") + br
                   + tr("Difficulty retargeting: DarkGravityWave v3") + br
                   + tr("Chain launched: November 2018")
                   + "</p>";

        aboutHTML += rule + "<p><b>" + tr("Community") + "</b>" + br
                   + tr("Website") + ": <a href=\"https://badcoin.net\">badcoin.net</a>" + br
                   + tr("Telegram") + ": @badcoinnet" + br
                   + tr("Source") + ": <a href=\"https://github.com/badcoin-project/badcoin\">github.com/badcoin-project/badcoin</a>"
                   + "</p>";

        aboutHTML += rule + "<p><b>" + tr("Contributors") + "</b>" + br
                   + tr("Joel Comm, Co-Founder") + br
                   + tr("Travis Wright, Co-Founder") + br
                   + tr("Marshall Long, Blockchain Architecture") + br
                   + tr("And the open source community of developers and miners.")
                   + "</p>";

        aboutHTML += rule + "<p><b>" + tr("Open Source") + "</b>" + br
                   + tr("Badcoin Core builds on Bitcoin Core and Myriad Core, with OpenSSL and "
                        "other open source libraries. Distributed under the MIT License.") + br
                   + "<i>" + tr("This is experimental software. Only mine what you are willing to learn with.") + "</i>"
                   + "</p>";

        aboutHTML += rule + "<p><b>"
                   + tr("Burned Once. Built Back Better. Stay BAD.")
                   + "</b></p>";

        ui->aboutMessage->setTextFormat(Qt::RichText);
        ui->aboutMessage->setOpenExternalLinks(true);
        ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->aboutMessage->setText(aboutHTML);
        ui->aboutMessage->setWordWrap(true);
        ui->helpMessage->setVisible(false);

        // Plain-text fallback used by printToConsole().
        text = version + "\n" + tr("Phoenix Edition") + "\n"
             + tr("Crypto Mining for People with Bad Computers");
    } else {
        setWindowTitle(tr("Command-line options"));
        QString header = tr("Usage:") + "\n" +
            "  badcoin-qt [" + tr("command-line options") + "]                     " + "\n";
        QTextCursor cursor(ui->helpMessage->document());
        cursor.insertText(version);
        cursor.insertBlock();
        cursor.insertText(header);
        cursor.insertBlock();

        std::string strUsage = HelpMessage(HMM_BITCOIN_QT);
        const bool showDebug = gArgs.GetBoolArg("-help-debug", false);
        strUsage += HelpMessageGroup(tr("UI Options:").toStdString());
        if (showDebug) {
            strUsage += HelpMessageOpt("-allowselfsignedrootcertificates", strprintf("Allow self signed root certificates (default: %u)", DEFAULT_SELFSIGNED_ROOTCERTS));
        }
        strUsage += HelpMessageOpt("-choosedatadir", strprintf(tr("Choose data directory on startup (default: %u)").toStdString(), DEFAULT_CHOOSE_DATADIR));
        strUsage += HelpMessageOpt("-lang=<lang>", tr("Set language, for example \"de_DE\" (default: system locale)").toStdString());
        strUsage += HelpMessageOpt("-min", tr("Start minimized").toStdString());
        strUsage += HelpMessageOpt("-rootcertificates=<file>", tr("Set SSL root certificates for payment request (default: -system-)").toStdString());
        strUsage += HelpMessageOpt("-splash", strprintf(tr("Show splash screen on startup (default: %u)").toStdString(), DEFAULT_SPLASHSCREEN));
        strUsage += HelpMessageOpt("-resetguisettings", tr("Reset all settings changed in the GUI").toStdString());
        if (showDebug) {
            strUsage += HelpMessageOpt("-uiplatform", strprintf("Select platform to customize UI for (one of windows, macosx, other; default: %s)", BitcoinGUI::DEFAULT_UIPLATFORM));
        }
        QString coreOptions = QString::fromStdString(strUsage);
        text = version + "\n" + header + "\n" + coreOptions;

        QTextTableFormat tf;
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_None);
        tf.setCellPadding(2);
        QVector<QTextLength> widths;
        widths << QTextLength(QTextLength::PercentageLength, 35);
        widths << QTextLength(QTextLength::PercentageLength, 65);
        tf.setColumnWidthConstraints(widths);

        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);

        for (const QString &line : coreOptions.split("\n")) {
            if (line.startsWith("  -"))
            {
                cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::PreviousCell);
                cursor.movePosition(QTextCursor::NextRow);
                cursor.insertText(line.trimmed());
                cursor.movePosition(QTextCursor::NextCell);
            } else if (line.startsWith("   ")) {
                cursor.insertText(line.trimmed()+' ');
            } else if (line.size() > 0) {
                //Title of a group
                if (cursor.currentTable())
                    cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::Down);
                cursor.insertText(line.trimmed(), bold);
                cursor.insertTable(1, 2, tf);
            }
        }

        ui->helpMessage->moveCursor(QTextCursor::Start);
        ui->scrollArea->setVisible(false);
        ui->aboutLogo->setVisible(false);
    }
}

HelpMessageDialog::~HelpMessageDialog()
{
    delete ui;
}

void HelpMessageDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    fprintf(stdout, "%s\n", qPrintable(text));
}

void HelpMessageDialog::showOrPrint()
{
#if defined(WIN32)
    // On Windows, show a message box, as there is no stderr/stdout in windowed applications
    exec();
#else
    // On other operating systems, print help text to console
    printToConsole();
#endif
}

void HelpMessageDialog::on_okButton_accepted()
{
    close();
}


/** "Shutdown" window */
ShutdownWindow::ShutdownWindow(QWidget *parent, Qt::WindowFlags f):
    QWidget(parent, f)
{
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(new QLabel(
        tr("%1 is shutting down...").arg(tr(PACKAGE_NAME)) + "<br /><br />" +
        tr("Do not shut down the computer until this window disappears.")));
    setLayout(layout);
}

QWidget *ShutdownWindow::showShutdownWindow(BitcoinGUI *window)
{
    if (!window)
        return nullptr;

    // Show a simple window indicating shutdown status
    QWidget *shutdownWindow = new ShutdownWindow();
    shutdownWindow->setWindowTitle(window->windowTitle());

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2, global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
    return shutdownWindow;
}

void ShutdownWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
}

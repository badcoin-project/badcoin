// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/splashscreen.h>

#include <qt/networkstyle.h>

#include <clientversion.h>
#include <init.h>
#include <util.h>
#include <ui_interface.h>
#include <version.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPolygonF>
#include <QRadialGradient>
#include <boost/bind/bind.hpp>

using namespace boost::placeholders;

// Draw a small four-point "spark" star, used as a celebratory accent on the
// Phoenix splash. cx, cy is the centre; r is the outer radius.
static void drawSpark(QPainter &p, double cx, double cy, double r)
{
    static const double ux[8] = { 0.0, 0.2404, 1.0, 0.2404, 0.0, -0.2404, -1.0, -0.2404 };
    static const double uy[8] = { -1.0, -0.2404, 0.0, 0.2404, 1.0, 0.2404, 0.0, -0.2404 };
    QPolygonF star;
    for (int i = 0; i < 8; ++i)
        star << QPointF(cx + ux[i] * r, cy + uy[i] * r);
    p.drawPolygon(star);
}

SplashScreen::SplashScreen(Qt::WindowFlags f, const NetworkStyle *networkStyle) :
    QWidget(0, f), curAlignment(0)
{
    float devicePixelRatio      = 1.0;
#if QT_VERSION > 0x050100
    devicePixelRatio = ((QGuiApplication*)QCoreApplication::instance())->devicePixelRatio();
#endif

    // define text to place
    QString titleText       = QStringLiteral("Badcoin Phoenix Core");
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText   = QString::fromUtf8(CopyrightHolders(strprintf("\xc2\xA9 %u-%u ", 2009, COPYRIGHT_YEAR)).c_str());
    QString titleAddText    = networkStyle->getTitleAddText();
    QString fontName        = QApplication::font().family();

    // logical splash size
    const int W = 520;
    const int H = 340;

    // create a bitmap according to device pixelratio
    QSize splashSize(W*devicePixelRatio, H*devicePixelRatio);
    pixmap = QPixmap(splashSize);

#if QT_VERSION > 0x050100
    // change to HiDPI if it makes sense
    pixmap.setDevicePixelRatio(devicePixelRatio);
#endif

    QPainter pixPaint(&pixmap);
    pixPaint.setRenderHint(QPainter::Antialiasing);
    pixPaint.setRenderHint(QPainter::SmoothPixmapTransform);

    // -- warm background: a cream sky fading to white -----------------------
    QLinearGradient bg(0, 0, 0, H);
    bg.setColorAt(0.0, QColor(255, 246, 234));
    bg.setColorAt(1.0, QColor(255, 255, 255));
    pixPaint.fillRect(QRect(0, 0, W, H), bg);

    // a soft ember glow rising behind the phoenix
    QRadialGradient glow(QPointF(140, 250), 280);
    glow.setColorAt(0.0,  QColor(255, 138, 46, 105));
    glow.setColorAt(0.55, QColor(255, 170, 80, 40));
    glow.setColorAt(1.0,  QColor(255, 200, 130, 0));
    pixPaint.fillRect(QRect(0, 0, W, H), glow);

    // -- the phoenix mascot, front and centre on the left -------------------
    {
        QPixmap phoenix(":/icons/badcoin_phoenix_splash");
        if (!phoenix.isNull()) {
            const QRect box(6, 10, 246, 252);
            QPixmap scaled = phoenix.scaled(box.width(), box.height(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation);
            pixPaint.drawPixmap(box.x() + (box.width()  - scaled.width())  / 2,
                                box.y() + (box.height() - scaled.height()) / 2,
                                scaled);
        }
    }

    // -- title block on the right -------------------------------------------
    const int tx = 264;

    {
        QFont f(fontName, 21);
        f.setBold(true);
        pixPaint.setFont(f);
        pixPaint.setPen(QColor(58, 58, 58));
        pixPaint.drawText(tx, 56, QStringLiteral("Badcoin"));
    }
    {
        QFont f(fontName, 33);
        f.setBold(true);
        f.setLetterSpacing(QFont::PercentageSpacing, 103);
        pixPaint.setFont(f);
        pixPaint.setPen(QColor(196, 28, 40));
        pixPaint.drawText(tx, 100, QStringLiteral("PHOENIX"));
    }
    {
        QFont f(fontName, 17);
        f.setBold(true);
        f.setLetterSpacing(QFont::PercentageSpacing, 180);
        pixPaint.setFont(f);
        pixPaint.setPen(QColor(140, 140, 140));
        pixPaint.drawText(tx + 1, 126, QStringLiteral("CORE"));
    }

    // a warm divider rule under the title
    pixPaint.setPen(QPen(QColor(240, 150, 60), 2));
    pixPaint.drawLine(tx + 1, 142, W - 36, 142);

    // -- the "We are back!" tagline -----------------------------------------
    {
        QFont f(fontName, 17);
        f.setBold(true);
        f.setItalic(true);
        pixPaint.setFont(f);
        pixPaint.setPen(QColor(230, 96, 24));
        pixPaint.drawText(tx + 1, 173, QStringLiteral("We are back!"));
    }

    // small spark accents, echoing the stars on the coin
    pixPaint.setPen(Qt::NoPen);
    pixPaint.setBrush(QColor(255, 190, 70));
    drawSpark(pixPaint, W - 48, 162, 5.5);
    drawSpark(pixPaint, W - 30, 180, 3.4);
    drawSpark(pixPaint, W - 41, 198, 2.6);

    // -- version and copyright, small and low -------------------------------
    {
        QFont f(fontName, 9);
        pixPaint.setFont(f);
        pixPaint.setPen(QColor(150, 150, 150));
        pixPaint.drawText(tx + 1, 206, versionText);

        QFont fc(fontName, 8);
        pixPaint.setFont(fc);
        pixPaint.setPen(QColor(170, 170, 170));
        QRect copyrightRect(tx + 1, 214, W - tx - 1 - 14, 52);
        pixPaint.drawText(copyrightRect,
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, copyrightText);
    }

    // -- network badge for testnet / regtest --------------------------------
    if (!titleAddText.isEmpty()) {
        QFont boldFont(fontName, 10);
        boldFont.setBold(true);
        pixPaint.setFont(boldFont);
        QFontMetrics fm = pixPaint.fontMetrics();
        int addWidth = fm.width(titleAddText);
        pixPaint.setPen(QColor(196, 28, 40));
        pixPaint.drawText(W - addWidth - 12, 16, titleAddText);
    }

    pixPaint.end();

    // Set window title
    setWindowTitle(titleText + " " + titleAddText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width()/devicePixelRatio,pixmap.size().height()/devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    move(QApplication::desktop()->screenGeometry().center() - r.center());

    subscribeToCoreSignals();
    installEventFilter(this);
}

SplashScreen::~SplashScreen()
{
    unsubscribeFromCoreSignals();
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if(keyEvent->text()[0] == 'q') {
            StartShutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void SplashScreen::slotFinish(QWidget *mainWin)
{
    Q_UNUSED(mainWin);

    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized())
        showNormal();
    hide();
    deleteLater(); // No more need for this
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(55,55,55)));
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + std::string("\n") +
            (resume_possible ? _("(press q to shutdown and continue later)")
                                : _("press q to shutdown")) +
            strprintf("\n%d", nProgress) + "%");
}

#ifdef ENABLE_WALLET
void SplashScreen::ConnectWallet(CWallet* wallet)
{
    wallet->ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2, false));
    connectedWallets.push_back(wallet);
}
#endif

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.InitMessage.connect(boost::bind(InitMessage, this, _1));
    uiInterface.ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2, _3));
#ifdef ENABLE_WALLET
    uiInterface.LoadWallet.connect(boost::bind(&SplashScreen::ConnectWallet, this, _1));
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.InitMessage.disconnect(boost::bind(InitMessage, this, _1));
    uiInterface.ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2, _3));
#ifdef ENABLE_WALLET
    for (CWallet* const & pwallet : connectedWallets) {
        pwallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2, false));
    }
#endif
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(5, 5, -5, -5);
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    StartShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}

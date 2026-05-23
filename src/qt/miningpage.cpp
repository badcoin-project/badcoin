// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/miningpage.h>

#include <qt/platformstyle.h>
#include <qt/rpcconsole.h>
#include <qt/walletmodel.h>

#include <amount.h>

#include <QButtonGroup>
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
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimerEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <utility>
#include <vector>

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
    : QWidget(parent), unitLabel("BAD"), m_range(RangeWeek)
{
    setMinimumHeight(160);
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

void RewardChart::setHistory(const QList<QPair<QDateTime, double> > &cumulativeHistory)
{
    // Replace the chart's data with a full time-sorted cumulative history.
    points.clear();
    for (const QPair<QDateTime, double> &h : cumulativeHistory) {
        Pt pt;
        pt.ts = h.first;
        pt.val = h.second;
        points.append(pt);
    }
    update();
}

void RewardChart::setRange(Range range)
{
    m_range = range;
    update();
}

void RewardChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();
    const int pad = 12;
    const int yAxisW = 64;   // left gutter: vertical axis = reward amount
    const int xAxisH = 16;   // bottom gutter: horizontal axis = time
    QRect plot = r.adjusted(pad + yAxisW, pad + 18, -pad, -pad - xAxisH);

    // Background
    p.fillRect(r, QColor(252, 252, 252));
    p.setPen(QColor(220, 220, 220));
    p.drawRect(plot);

    // Resolve the selected time window.
    const QDateTime now = QDateTime::currentDateTime();
    QDateTime windowStart;
    QString rangeTitle;
    QString xFormat;
    switch (m_range) {
    case RangeDay:
        windowStart = now.addDays(-1);
        rangeTitle = QStringLiteral("Mining Rewards - last 24 hours");
        xFormat = QStringLiteral("HH:mm");
        break;
    case RangeMonth:
        windowStart = now.addMonths(-1);
        rangeTitle = QStringLiteral("Mining Rewards - last month");
        xFormat = QStringLiteral("MMM d");
        break;
    case RangeYear:
        windowStart = now.addYears(-1);
        rangeTitle = QStringLiteral("Mining Rewards - last year");
        xFormat = QStringLiteral("MMM yyyy");
        break;
    case RangeWeek:
    default:
        windowStart = now.addDays(-7);
        rangeTitle = QStringLiteral("Mining Rewards - last 7 days");
        xFormat = QStringLiteral("MMM d");
        break;
    }

    // Title (top-left) names the active range.
    p.setPen(QColor(80, 80, 80));
    QFont titleFont = p.font();
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.drawText(QPoint(pad, pad + 12), rangeTitle);
    p.setFont(QFont());

    // Split the data: points before the window set the baseline (total
    // already earned when the window opened); points inside it are plotted.
    double baseline = 0.0;
    QList<Pt> windowed;
    for (const Pt &pt : points) {
        if (pt.ts <= windowStart)
            baseline = pt.val;
        else if (pt.ts <= now)
            windowed.append(pt);
    }

    if (windowed.isEmpty()) {
        p.setPen(QColor(160, 160, 160));
        const QString msg = points.isEmpty()
            ? QStringLiteral("No rewards yet. Start mining to see your earnings.")
            : QStringLiteral("No rewards in this period.");
        p.drawText(plot, Qt::AlignCenter, msg);
        return;
    }

    // Values are measured from the window's start, so the curve rises from 0.
    double maxVal = 0.0;
    for (const Pt &pt : windowed) {
        const double v = pt.val - baseline;
        if (v > maxVal) maxVal = v;
    }
    if (maxVal <= 0.0) maxVal = 1.0;

    const double windowTotal = windowed.last().val - baseline;
    qint64 span = windowStart.msecsTo(now);
    if (span < 1) span = 1;     // avoid divide-by-zero

    // Total earned in this window (top-right corner).
    {
        const QString totalLbl =
            QString::number(windowTotal, 'f', (windowTotal < 10 ? 2 : 0))
            + " " + unitLabel;
        p.setPen(QColor(90, 90, 90));
        p.drawText(QRect(r.right() - 220 - pad, pad, 220, 14),
                   Qt::AlignRight, totalLbl);
    }

    QFont axisFont = p.font();
    axisFont.setPointSize(axisFont.pointSize() - 1);
    p.setFont(axisFont);
    p.setPen(QColor(120, 120, 120));

    // Vertical axis = reward amount: max at the top, 0 at the bottom.
    const QString topLbl = QString::number(maxVal, 'f', (maxVal < 10 ? 2 : 0));
    p.drawText(QRect(pad, plot.top() - 4, yAxisW - 6, 14),
               Qt::AlignRight, topLbl);
    p.drawText(QRect(pad, plot.bottom() - 8, yAxisW - 6, 14),
               Qt::AlignRight, QStringLiteral("0"));

    // Horizontal axis = time: window start at the left, "now" at the right.
    p.drawText(QRect(plot.left(), plot.bottom() + 2, 130, 13),
               Qt::AlignLeft, windowStart.toString(xFormat));
    p.drawText(QRect(plot.right() - 130, plot.bottom() + 2, 130, 13),
               Qt::AlignRight, QStringLiteral("now"));
    p.setFont(QFont());

    // Build the line: starts at (windowStart, 0), steps up at each reward,
    // then runs flat to "now" so a quiet spell reads as a plateau.
    QPainterPath path;
    path.moveTo(plot.left(), plot.bottom());
    double lastY = plot.bottom();
    for (const Pt &pt : windowed) {
        const qint64 dtMs = windowStart.msecsTo(pt.ts);
        const double xFrac = double(dtMs) / double(span);
        const double yFrac = (pt.val - baseline) / maxVal;
        const double x = plot.left() + xFrac * plot.width();
        const double y = plot.bottom() - yFrac * plot.height();
        path.lineTo(x, y);
        lastY = y;
    }
    path.lineTo(plot.right(), lastY);

    // Soft fill under the line.
    QPainterPath fillPath = path;
    fillPath.lineTo(plot.right(), plot.bottom());
    fillPath.lineTo(plot.left(),  plot.bottom());
    fillPath.closeSubpath();
    p.fillPath(fillPath, QColor(212, 44, 41, 42));  // red accent at low opacity

    // Stroke the line.
    QPen linePen(QColor(212, 44, 41));
    linePen.setWidth(2);
    p.setPen(linePen);
    p.drawPath(path);

    // Dot on the most recent reward.
    p.setBrush(QColor(212, 44, 41));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(plot.right(), lastY), 4.0, 4.0);
}

// ===========================================================================
// MinerAnimation: native pixel-art miner (port of badcoin-miner.html)
// ===========================================================================

namespace {
// Earthy cave palette, ported 1:1 from the prototype's PAL table.
const QColor MP_bgSky      ("#1a1410");
const QColor MP_bgFar      ("#2d2218");
const QColor MP_bgMid      ("#3d2f22");
const QColor MP_bgNear     ("#4d3a28");
const QColor MP_rockMid    ("#7a5840");
const QColor MP_blockBase  ("#c0a060");
const QColor MP_blockShade ("#806638");
const QColor MP_blockHigh  ("#f0d488");
const QColor MP_coinGold   ("#ffcc44");
const QColor MP_coinShade  ("#aa7700");
const QColor MP_skinTone   ("#d8a878");
const QColor MP_skinShade  ("#a07050");
const QColor MP_shirtRed   ("#a83828");
const QColor MP_shirtShade ("#7a2418");
const QColor MP_pantsBlue  ("#3850a0");
const QColor MP_pantsShade ("#283878");
const QColor MP_pickaxeWood ("#6a4830");
const QColor MP_pickaxeHead ("#888888");
const QColor MP_pickaxeShade("#444444");
const QColor MP_sparkBright("#fff8c8");
const QColor MP_sparkMid   ("#ffaa44");
const QColor MP_crackDark  ("#1a0808");
const QColor MP_speech     ("#ffffff");
const QColor MP_speechText ("#1a1a1a");

const int    MP_BLOCK_MAX_HITS = 8;
const double MP_PI = 3.14159265358979323846;

// Uniform random double in [0,1).
double mpRand() { return QRandomGenerator::global()->generateDouble(); }
}  // namespace

MinerAnimation::MinerAnimation(QWidget *parent)
    : QWidget(parent)
    , m_mode(Idle)
    , m_miningActive(false)
    , m_pixelSize(4)
    , m_timerId(0)
    , m_intervalMs(250)
    , m_clockMs(0.0)
    , m_bgOffset(0.0)
    , m_minerX(0.0), m_minerBaseY(0.0), m_blockX(0.0), m_blockY(0.0)
    , m_blockHits(0)
    , m_swingActive(false)
    , m_swingPhase(0.0)
    , m_screenShake(0.0)
    , m_goldFlash(0.0)
    , m_speechTimer(0.0)
    , m_laughOpen(false)
{
    setFixedHeight(160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    recomputeLayout();
    m_timerId = startTimer(m_intervalMs);
}

void MinerAnimation::recomputeLayout()
{
    const int W = qMax(1, width()  / m_pixelSize);
    const int H = qMax(1, height() / m_pixelSize);
    m_minerX     = std::floor(W * 0.25);
    m_minerBaseY = H - 6;
    m_blockX     = std::floor(W * 0.45);
    m_blockY     = H - 6;
}

void MinerAnimation::resizeEvent(QResizeEvent * /*event*/)
{
    recomputeLayout();
}

void MinerAnimation::applyTimerInterval()
{
    const int wanted = (m_mode == Idle) ? 250 : 16;
    if (wanted == m_intervalMs && m_timerId != 0) return;
    if (m_timerId != 0) killTimer(m_timerId);
    m_intervalMs = wanted;
    m_timerId = startTimer(m_intervalMs);
}

void MinerAnimation::setMode(Mode mode)
{
    if (mode == Celebrating) return;          // celebration is set via recordBlock()
    m_miningActive = (mode == Mining);
    if (m_mode == Celebrating) return;        // let the celebration finish first
    if (m_mode != mode) {
        m_mode = mode;
        if (mode == Idle) {
            m_swingActive = false;
            m_swingPhase  = 0.0;
        }
    }
    applyTimerInterval();
    update();
}

void MinerAnimation::recordBlock()
{
    m_mode        = Celebrating;
    m_blockHits   = MP_BLOCK_MAX_HITS;
    m_swingActive = false;
    m_swingPhase  = 0.0;
    spawnCelebrationCoins(m_blockX + 8, m_blockY - 8);
    m_screenShake = 6.0;
    m_goldFlash   = 0.45;
    m_speechTimer = 1400.0;
    applyTimerInterval();
    update();
}

void MinerAnimation::spawnImpactSparks(double gx, double gy)
{
    for (int i = 0; i < 6; ++i) {
        const double angle = -MP_PI / 2.0 + (mpRand() - 0.5) * 1.6;
        const double speed = 0.8 + mpRand() * 1.2;
        Particle s;
        s.x = gx; s.y = gy;
        s.vx = std::cos(angle) * speed;
        s.vy = std::sin(angle) * speed;
        s.life = 25; s.maxLife = 25;
        s.size = (mpRand() < 0.5) ? 1.0 : 2.0;
        s.rotPhase = 0.0; s.rotSpeed = 0.0;
        s.color = (mpRand() < 0.6) ? MP_sparkBright : MP_sparkMid;
        m_sparks.append(s);
    }
}

void MinerAnimation::spawnCelebrationCoins(double gx, double gy)
{
    for (int i = 0; i < 18; ++i) {
        const double angle = -MP_PI / 2.0 + (mpRand() - 0.5) * MP_PI;
        const double speed = 1.5 + mpRand() * 1.8;
        Particle c;
        c.x = gx + (mpRand() - 0.5) * 4.0;
        c.y = gy;
        c.vx = std::cos(angle) * speed;
        c.vy = std::sin(angle) * speed - 1.0;
        c.life = 90; c.maxLife = 90;
        c.rotPhase = mpRand() * MP_PI * 2.0;
        c.rotSpeed = 0.15 + mpRand() * 0.1;
        c.size = 2.0;
        c.color = MP_coinGold;
        m_coins.append(c);
    }
}

void MinerAnimation::advance()
{
    m_clockMs += m_intervalMs;

    // Idle is calm: only the sleeping Z's animate, and those are computed
    // straight from m_clockMs in paintEvent. Nothing to update here.
    if (m_mode == Idle) return;

    m_bgOffset += 0.3;

    // Mining swing cycle
    if (m_mode == Mining) {
        if (!m_swingActive && mpRand() < 0.012) {
            m_swingActive = true;
            m_swingPhase  = 0.0;
        }
        if (m_swingActive) {
            const double prev = m_swingPhase;
            m_swingPhase += 0.04;
            if (prev < 0.5 && m_swingPhase >= 0.5) {
                // Impact frame: sparks, shake, one more crack
                spawnImpactSparks(m_blockX, m_blockY - 8);
                m_screenShake = 3.0;
                if (m_blockHits < MP_BLOCK_MAX_HITS - 1)
                    m_blockHits = qMin(m_blockHits + 1, MP_BLOCK_MAX_HITS - 1);
            }
            if (m_swingPhase >= 1.0) {
                m_swingActive = false;
                m_swingPhase  = 0.0;
            }
        }
    }

    // Particles
    for (int i = m_sparks.size() - 1; i >= 0; --i) {
        Particle &s = m_sparks[i];
        s.x += s.vx; s.y += s.vy; s.vy += 0.08;
        if (--s.life <= 0) m_sparks.removeAt(i);
    }
    for (int i = m_coins.size() - 1; i >= 0; --i) {
        Particle &c = m_coins[i];
        c.x += c.vx; c.y += c.vy; c.vy += 0.08;
        c.rotPhase += c.rotSpeed;
        if (--c.life <= 0) m_coins.removeAt(i);
    }

    // Screen-shake decay
    if (m_screenShake > 0.0) {
        m_screenShake *= 0.85;
        if (m_screenShake < 0.3) m_screenShake = 0.0;
    }

    // Celebration: speech bubble, gold flash, return to the base mode
    if (m_mode == Celebrating) {
        if (m_speechTimer > 0.0) {
            m_speechTimer -= m_intervalMs;
            m_laughOpen = (int(m_clockMs / 100.0) % 2) == 0;
        }
        if (m_goldFlash > 0.0) {
            m_goldFlash *= 0.88;
            if (m_goldFlash < 0.02) m_goldFlash = 0.0;
        }
        if (m_speechTimer <= 0.0 && m_sparks.isEmpty() && m_coins.isEmpty()
            && m_goldFlash == 0.0) {
            m_blockHits = 0;
            m_mode = m_miningActive ? Mining : Idle;
            applyTimerInterval();
        }
    }
}

void MinerAnimation::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timerId) { QWidget::timerEvent(event); return; }
    advance();
    update();
}

void MinerAnimation::px(QPainter &p, double gx, double gy, const QColor &c,
                        double w, double h)
{
    p.fillRect(QRectF(std::floor(gx) * m_pixelSize, std::floor(gy) * m_pixelSize,
                      w * m_pixelSize, h * m_pixelSize), c);
}

void MinerAnimation::drawBackground(QPainter &p)
{
    const int W = qMax(1, width()  / m_pixelSize);
    const int H = qMax(1, height() / m_pixelSize);

    p.fillRect(rect(), MP_bgSky);

    // Far layer: slowest parallax, jagged silhouette
    const int offFar = int(std::floor(m_bgOffset * 0.15)) % W;
    for (int i = 0; i < W + 4; ++i) {
        const int xx = ((i - offFar) % W + W) % W;
        const double seed = std::sin(i * 1.7) * 0.5 + 0.5;
        const int hh = int(std::floor(seed * 8 + 4));
        p.fillRect(xx * m_pixelSize, (H - hh) * m_pixelSize,
                   m_pixelSize, hh * m_pixelSize, MP_bgFar);
    }
    // Mid layer
    const int offMid = int(std::floor(m_bgOffset * 0.4)) % W;
    for (int i = 0; i < W + 4; ++i) {
        const int xx = ((i - offMid) % W + W) % W;
        const double seed = std::sin(i * 0.8 + 2.3) * 0.5 + 0.5;
        const int hh = int(std::floor(seed * 12 + 8));
        p.fillRect(xx * m_pixelSize, (H - hh) * m_pixelSize,
                   m_pixelSize, hh * m_pixelSize, MP_bgMid);
    }
    // Near ground band
    p.fillRect(0, (H - 6) * m_pixelSize, width(), 6 * m_pixelSize, MP_bgNear);
    // Scattered ground rocks
    for (int i = 0; i < 8; ++i) {
        const double seed = std::sin(i * 12.9) * 0.5 + 0.5;
        const int xx = int(std::floor(seed * W));
        p.fillRect(xx * m_pixelSize, (H - 6) * m_pixelSize,
                   m_pixelSize * 2, m_pixelSize, MP_rockMid);
    }
}

void MinerAnimation::drawBlock(QPainter &p)
{
    if (m_blockHits >= MP_BLOCK_MAX_HITS) return;   // shattered
    const double X = m_blockX;
    const double Y = m_blockY - 16;

    px(p, X + 0,  Y + 0,  MP_blockBase, 16, 16);
    px(p, X + 0,  Y + 0,  MP_blockHigh, 16, 1);
    px(p, X + 0,  Y + 1,  MP_blockHigh, 1, 14);
    px(p, X + 15, Y + 1,  MP_blockShade, 1, 15);
    px(p, X + 0,  Y + 15, MP_blockShade, 16, 1);
    // Gold ore flecks
    px(p, X + 3,  Y + 3,  MP_coinGold, 1, 1);
    px(p, X + 9,  Y + 5,  MP_coinGold, 1, 1);
    px(p, X + 5,  Y + 8,  MP_coinGold, 1, 1);
    px(p, X + 11, Y + 10, MP_coinGold, 2, 1);
    px(p, X + 6,  Y + 12, MP_coinGold, 1, 1);
    // Cracks accumulate with hit count
    const double cp = double(m_blockHits) / double(MP_BLOCK_MAX_HITS);
    if (cp > 0.15) {
        px(p, X + 8,  Y + 4,  MP_crackDark, 1, 1);
        px(p, X + 9,  Y + 5,  MP_crackDark, 1, 1);
        px(p, X + 10, Y + 6,  MP_crackDark, 1, 1);
    }
    if (cp > 0.4) {
        px(p, X + 7,  Y + 7,  MP_crackDark, 1, 1);
        px(p, X + 7,  Y + 8,  MP_crackDark, 1, 1);
        px(p, X + 7,  Y + 9,  MP_crackDark, 1, 1);
        px(p, X + 6,  Y + 10, MP_crackDark, 1, 1);
    }
    if (cp > 0.65) {
        px(p, X + 11, Y + 7,  MP_crackDark, 1, 1);
        px(p, X + 12, Y + 8,  MP_crackDark, 1, 1);
        px(p, X + 13, Y + 9,  MP_crackDark, 1, 1);
        px(p, X + 4,  Y + 11, MP_crackDark, 1, 1);
        px(p, X + 3,  Y + 12, MP_crackDark, 1, 1);
    }
    if (cp > 0.85) {
        px(p, X + 5,  Y + 5,  MP_crackDark, 2, 1);
        px(p, X + 9,  Y + 11, MP_crackDark, 2, 1);
        px(p, X + 13, Y + 13, MP_crackDark, 1, 1);
    }
}

void MinerAnimation::drawMiner(QPainter &p)
{
    const double X = m_minerX;
    const double Y = m_minerBaseY - 18;

    bool armUp = false, mouthOpen = false, leaning = false;
    if (m_mode == Mining && m_swingActive) {
        armUp = m_swingPhase < 0.4;
    } else if (m_mode == Celebrating) {
        armUp = (int(m_clockMs / 133.0) % 2) == 0;
        mouthOpen = m_laughOpen;
    } else {
        leaning = true;
    }

    // Helmet
    px(p, X + 3, Y + 0, MP_shirtRed, 6, 1);
    px(p, X + 2, Y + 1, MP_shirtRed, 8, 2);
    px(p, X + 4, Y + 0, MP_coinGold, 1, 1);   // helmet lamp

    // Face
    px(p, X + 3, Y + 3, MP_skinTone, 6, 4);
    px(p, X + 3, Y + 3, MP_skinShade, 1, 4);
    const bool sleeping = (m_mode == Idle) && (std::fmod(m_clockMs, 4000.0) < 3333.0);
    if (sleeping) {
        px(p, X + 4, Y + 4, MP_skinShade, 1, 1);
        px(p, X + 7, Y + 4, MP_skinShade, 1, 1);
    } else {
        px(p, X + 4, Y + 4, MP_crackDark, 1, 1);
        px(p, X + 7, Y + 4, MP_crackDark, 1, 1);
    }
    if (mouthOpen) {
        px(p, X + 5, Y + 6, MP_crackDark, 2, 1);
        px(p, X + 5, Y + 5, MP_shirtRed, 2, 1);
    } else if (m_mode == Celebrating) {
        px(p, X + 5, Y + 6, MP_crackDark, 2, 1);
    } else {
        px(p, X + 5, Y + 6, MP_skinShade, 2, 1);
    }
    // Beard
    px(p, X + 3, Y + 7, MP_shirtShade, 6, 1);
    px(p, X + 4, Y + 8, MP_shirtShade, 4, 1);

    // Torso
    px(p, X + 3, Y + 9, MP_shirtRed, 6, 4);
    px(p, X + 3, Y + 9, MP_shirtShade, 1, 4);

    // Right arm (holds the pickaxe)
    if (armUp && (m_mode == Mining || m_mode == Celebrating)) {
        px(p, X + 9,  Y + 8, MP_shirtRed, 1, 1);
        px(p, X + 10, Y + 7, MP_shirtRed, 1, 1);
        px(p, X + 11, Y + 6, MP_skinTone, 1, 1);
    } else if (m_mode == Mining) {
        px(p, X + 9,  Y + 10, MP_shirtRed, 1, 1);
        px(p, X + 10, Y + 11, MP_shirtRed, 1, 1);
        px(p, X + 11, Y + 12, MP_skinTone, 1, 1);
    } else {
        px(p, X + 9, Y + 10, MP_shirtRed, 1, 3);
        px(p, X + 9, Y + 13, MP_skinTone, 1, 1);
    }
    // Left arm
    px(p, X + 2, Y + 10, MP_shirtRed, 1, 3);
    px(p, X + 2, Y + 13, MP_skinTone, 1, 1);

    // Legs
    if (leaning) {
        px(p, X + 3, Y + 13, MP_pantsBlue, 6, 3);
        px(p, X + 3, Y + 16, MP_crackDark, 3, 2);
        px(p, X + 6, Y + 16, MP_crackDark, 3, 2);
    } else {
        px(p, X + 3, Y + 13, MP_pantsBlue,  3, 4);
        px(p, X + 6, Y + 13, MP_pantsBlue,  3, 4);
        px(p, X + 3, Y + 13, MP_pantsShade, 1, 4);
        px(p, X + 6, Y + 13, MP_pantsShade, 1, 4);
        px(p, X + 3, Y + 17, MP_crackDark,  3, 1);
        px(p, X + 6, Y + 17, MP_crackDark,  3, 1);
    }

    // Pickaxe
    if (m_mode == Mining || (m_mode == Celebrating && armUp)) {
        drawPickaxe(p, X, Y, armUp);
    } else if (leaning) {
        px(p, X + 11, Y + 13, MP_pickaxeWood,  1, 5);
        px(p, X + 10, Y + 12, MP_pickaxeHead,  3, 1);
        px(p, X + 10, Y + 11, MP_pickaxeShade, 1, 1);
        px(p, X + 12, Y + 11, MP_pickaxeShade, 1, 1);
    }

    // Sleeping Z's
    if (m_mode == Idle)
        drawSleepingZs(p, X + 8, Y - 2);
}

void MinerAnimation::drawPickaxe(QPainter &p, double X, double Y, bool up)
{
    const double handX = X + 11;
    const double handY = up ? (Y + 6) : (Y + 12);
    if (up) {
        px(p, handX + 0, handY - 1, MP_pickaxeWood,  1, 1);
        px(p, handX + 1, handY - 2, MP_pickaxeWood,  1, 1);
        px(p, handX + 2, handY - 3, MP_pickaxeWood,  1, 1);
        px(p, handX + 3, handY - 4, MP_pickaxeWood,  1, 1);
        px(p, handX + 2, handY - 5, MP_pickaxeHead,  3, 1);
        px(p, handX + 4, handY - 4, MP_pickaxeShade, 1, 1);
        px(p, handX + 2, handY - 6, MP_pickaxeShade, 1, 1);
    } else {
        px(p, handX + 0, handY + 0, MP_pickaxeWood,  1, 1);
        px(p, handX + 1, handY + 0, MP_pickaxeWood,  1, 1);
        px(p, handX + 2, handY - 1, MP_pickaxeWood,  1, 1);
        px(p, handX + 3, handY - 2, MP_pickaxeWood,  1, 1);
        px(p, handX + 3, handY - 3, MP_pickaxeHead,  1, 3);
        px(p, handX + 4, handY - 3, MP_pickaxeShade, 1, 1);
        px(p, handX + 4, handY - 1, MP_pickaxeShade, 1, 1);
    }
}

void MinerAnimation::drawSleepingZs(QPainter &p, double gx, double gy)
{
    for (int i = 0; i < 3; ++i) {
        const double phase = std::fmod(m_clockMs / 8000.0 + i / 3.0, 1.0);
        if (phase > 0.85) continue;
        const int yOff = -int(std::floor(phase * 12.0));
        const int xOff = i + int(std::floor(std::sin(phase * MP_PI) * 2.0));
        const double alpha = (phase < 0.7) ? 1.0 : (1.0 - (phase - 0.7) / 0.15);
        const double sz = 1 + i;
        p.setOpacity(alpha);
        px(p, gx + xOff,            gy + yOff,     MP_skinShade, sz, 1);
        px(p, gx + xOff + sz - 1,   gy + yOff + 1, MP_skinShade, 1, 1);
        px(p, gx + xOff,            gy + yOff + 2, MP_skinShade, sz, 1);
        p.setOpacity(1.0);
    }
}

void MinerAnimation::drawParticles(QPainter &p)
{
    for (const Particle &s : m_sparks) {
        p.setOpacity(double(s.life) / double(s.maxLife));
        px(p, std::floor(s.x), std::floor(s.y), s.color, s.size, s.size);
    }
    p.setOpacity(1.0);
    for (const Particle &c : m_coins) {
        const double alpha = (c.life > 30) ? 1.0 : (double(c.life) / 30.0);
        p.setOpacity(alpha);
        const double rotW = std::fabs(std::sin(c.rotPhase)) * c.size + 1.0;
        const double x = std::floor(c.x);
        const double y = std::floor(c.y);
        px(p, x, y, MP_coinShade, std::ceil(rotW), c.size);
        px(p, x, y, MP_coinGold,  qMax(1.0, std::floor(rotW)), c.size);
    }
    p.setOpacity(1.0);
}

void MinerAnimation::drawSpeechBubble(QPainter &p, double gx, double gy)
{
    if (m_speechTimer <= 0.0) return;
    const int w = 22, h = 8;
    const double bx = gx, by = gy - 14;
    px(p, bx,       by,       MP_speech, w, h);
    px(p, bx + 1,   by - 1,   MP_speech, w - 2, 1);
    px(p, bx + 1,   by + h,   MP_speech, w - 2, 1);
    px(p, bx - 1,   by + 1,   MP_crackDark, 1, h - 2);
    px(p, bx + w,   by + 1,   MP_crackDark, 1, h - 2);
    px(p, bx + 2,   by + h,   MP_speech, 2, 1);
    px(p, bx + 3,   by + h + 1, MP_speech, 1, 1);
    drawPixelText(p, bx + 2, by + 2, QStringLiteral("HA HA"));
}

void MinerAnimation::drawPixelText(QPainter &p, double gx, double gy, const QString &text)
{
    // Tiny 3x5 pixel font; only the glyphs the speech bubble needs.
    static const int H_glyph[5][3] = {{1,0,1},{1,0,1},{1,1,1},{1,0,1},{1,0,1}};
    static const int A_glyph[5][3] = {{0,1,0},{1,0,1},{1,1,1},{1,0,1},{1,0,1}};
    double cx = gx;
    for (const QChar &chRef : text) {
        const char ch = chRef.toLatin1();
        if (ch == 'H' || ch == 'A') {
            const int (*glyph)[3] = (ch == 'H') ? H_glyph : A_glyph;
            for (int r = 0; r < 5; ++r)
                for (int c = 0; c < 3; ++c)
                    if (glyph[r][c])
                        px(p, cx + c, gy + r, MP_speechText, 1, 1);
        }
        cx += 4;   // 3 wide + 1 spacing (also covers the space character)
    }
}

void MinerAnimation::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.save();
    if (m_screenShake > 0.0) {
        const double sx = (mpRand() - 0.5) * m_screenShake;
        const double sy = (mpRand() - 0.5) * m_screenShake;
        p.translate(sx, sy);
    }

    drawBackground(p);
    if (m_mode != Celebrating || m_blockHits < MP_BLOCK_MAX_HITS)
        drawBlock(p);
    drawMiner(p);
    drawParticles(p);
    if (m_mode == Celebrating && m_speechTimer > 0.0)
        drawSpeechBubble(p, m_minerX + 8, m_minerBaseY - 18);

    p.restore();

    // Gold flash overlay (drawn outside the screen-shake transform)
    if (m_goldFlash > 0.0) {
        p.setOpacity(m_goldFlash);
        p.fillRect(rect(), MP_coinGold);
        p.setOpacity(1.0);
    }
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
    , chartRangeGroup(nullptr)
    , minerAnim(nullptr)
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
    QGroupBox *chartBox = new QGroupBox(tr("Mining Rewards"));
    QVBoxLayout *chartLayout = new QVBoxLayout(chartBox);

    // Time-range buttons: window the reward history to the last day, week,
    // month or year. In solo mode the history is the wallet's real mined
    // rewards; in pool mode it is the current session's accepted shares.
    QHBoxLayout *rangeRow = new QHBoxLayout();
    rangeRow->addWidget(new QLabel(tr("Show:")));
    chartRangeGroup = new QButtonGroup(this);
    chartRangeGroup->setExclusive(true);
    const QString rangeNames[4] = {
        tr("1 Day"), tr("1 Week"), tr("1 Month"), tr("1 Year")
    };
    QPushButton *weekButton = nullptr;
    for (int i = 0; i < 4; ++i) {
        QPushButton *b = new QPushButton(rangeNames[i]);
        b->setCheckable(true);
        chartRangeGroup->addButton(b, i);
        rangeRow->addWidget(b);
        if (i == RewardChart::RangeWeek)
            weekButton = b;
    }
    rangeRow->addStretch();
    chartLayout->addLayout(rangeRow);

    // Default to the 1 Week view, matching RewardChart's default range.
    if (weekButton)
        weekButton->setChecked(true);

    rewardChart = new RewardChart();
    chartLayout->addWidget(rewardChart);
    minerAnim = new MinerAnimation();
    chartLayout->addWidget(minerAnim);
    main->addWidget(chartBox);

    connect(chartRangeGroup, SIGNAL(idClicked(int)),
            this, SLOT(onChartRangeChanged(int)));
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
    if (walletModel) {
        // The mined-reward history grows as coinbase rewards are credited, so
        // refresh the chart whenever the wallet balance changes.
        connect(walletModel,
                SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)),
                this, SLOT(reloadRewardHistory()));
    }
    reloadRewardHistory();
}

void MiningPage::reloadRewardHistory()
{
    if (!rewardChart || !walletModel)
        return;
    // Real mined-reward history applies to solo mining only. In pool mode the
    // chart keeps the live session's accepted-shares count, because pool
    // shares are not recorded in the wallet.
    if (modePoolRadio && modePoolRadio->isChecked())
        return;

    std::vector<std::pair<qint64, CAmount> > rewards;
    walletModel->listMinedRewards(rewards);
    std::sort(rewards.begin(), rewards.end());   // ascending by block time

    QList<QPair<QDateTime, double> > history;
    double cumulative = 0.0;
    for (const std::pair<qint64, CAmount> &rw : rewards) {
        cumulative += double(rw.second) / double(COIN);
        history.append(qMakePair(
            QDateTime::fromSecsSinceEpoch(rw.first), cumulative));
    }
    rewardChart->setHistory(history);
}

void MiningPage::onChartRangeChanged(int rangeId)
{
    if (!rewardChart)
        return;
    switch (rangeId) {
    case 0: rewardChart->setRange(RewardChart::RangeDay);   break;
    case 1: rewardChart->setRange(RewardChart::RangeWeek);  break;
    case 2: rewardChart->setRange(RewardChart::RangeMonth); break;
    case 3: rewardChart->setRange(RewardChart::RangeYear);  break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Mode enablement + presets
// ---------------------------------------------------------------------------

void MiningPage::onModeChanged()
{
    applyModeEnablement();
    // Solo mining charts the wallet's real reward history; pool mining charts
    // the current session's accepted shares.
    if (modePoolRadio && modePoolRadio->isChecked()) {
        if (rewardChart) rewardChart->reset();
    } else {
        reloadRewardHistory();
    }
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
    // The chart is set up per mode by startSolo() / startPool() below.

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
    reloadRewardHistory();   // show the wallet's real mined-reward history
    if (minerAnim) minerAnim->setMode(MinerAnimation::Mining);
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
    if (rewardChart) rewardChart->reset();   // pool shares are a fresh session count
    if (minerAnim) minerAnim->setMode(MinerAnimation::Mining);
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
    if (minerAnim) minerAnim->setMode(MinerAnimation::Idle);
}

// ---------------------------------------------------------------------------
// Solo worker signals
// ---------------------------------------------------------------------------

void MiningPage::handleBlockFound(const QString &hash)
{
    blocksMined++;
    blocksMinedLabel->setText(QString::number(blocksMined));
    lastHashLabel->setText(hash);
    if (minerAnim) minerAnim->recordBlock();
    // Refresh the chart from the wallet's real reward history. The coinbase
    // can take a moment to be credited; the wallet's balanceChanged signal
    // triggers a further refresh once it is.
    reloadRewardHistory();
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
    if (minerAnim) minerAnim->setMode(MinerAnimation::Idle);
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
    if (minerAnim) minerAnim->setMode(MinerAnimation::Idle);
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

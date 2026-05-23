// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/receiverequestdialog.h>
#include <qt/forms/ui_receiverequestdialog.h>

#include <qt/bitcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/rpcconsole.h>
#include <QPainter>
#include <QBoxLayout>
#include <QClipboard>
#include <QDrag>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPdfWriter>
#include <QPushButton>
#include <QPixmap>
#if QT_VERSION < 0x050000
#include <QUrl>
#endif

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h> /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

QRImageWidget::QRImageWidget(QWidget *parent):
    QLabel(parent), contextMenu(0)
{
    contextMenu = new QMenu(this);
    QAction *saveImageAction = new QAction(tr("&Save Image..."), this);
    connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));
    contextMenu->addAction(saveImageAction);
    QAction *copyImageAction = new QAction(tr("&Copy Image"), this);
    connect(copyImageAction, SIGNAL(triggered()), this, SLOT(copyImage()));
    contextMenu->addAction(copyImageAction);
}

QImage QRImageWidget::exportImage()
{
    if(!pixmap())
        return QImage();
    return pixmap()->toImage();
}

void QRImageWidget::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && pixmap())
    {
        event->accept();
        QMimeData *mimeData = new QMimeData;
        mimeData->setImageData(exportImage());

        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec();
    } else {
        QLabel::mousePressEvent(event);
    }
}

void QRImageWidget::saveImage()
{
    if(!pixmap())
        return;
    QString fn = GUIUtil::getSaveFileName(this, tr("Save QR Code"), QString(), tr("PNG Image (*.png)"), nullptr);
    if (!fn.isEmpty())
    {
        exportImage().save(fn);
    }
}

void QRImageWidget::copyImage()
{
    if(!pixmap())
        return;
    QApplication::clipboard()->setImage(exportImage());
}

void QRImageWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if(!pixmap())
        return;
    contextMenu->exec(event->globalPos());
}

ReceiveRequestDialog::ReceiveRequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReceiveRequestDialog),
    model(0),
    privateKeyPanel(nullptr),
    privateKeyQR(nullptr),
    privateKeyValue(nullptr),
    btnTogglePrivateKey(nullptr),
    btnCopyPrivateKey(nullptr),
    btnSaveKeysPdf(nullptr),
    privateKeyShown(false)
{
    ui->setupUi(this);

#ifndef USE_QRCODE
    ui->btnSaveAs->setVisible(false);
    ui->lblQRCode->setVisible(false);
#endif

    connect(ui->btnSaveAs, SIGNAL(clicked()), ui->lblQRCode, SLOT(saveImage()));

    // Private key reveal panel.
    // Built programmatically so we do not have to touch the .ui file. Hidden
    // by default. Becomes visible only after the user clicks "Show Private Key"
    // and confirms the warning dialog.
    privateKeyPanel = new QFrame(this);
    privateKeyPanel->setFrameShape(QFrame::StyledPanel);
    privateKeyPanel->setStyleSheet(
        "QFrame { background: #fff5f5; border: 1px solid #e69292; border-radius: 6px; }"
    );
    QVBoxLayout *pkLayout = new QVBoxLayout(privateKeyPanel);
    pkLayout->setContentsMargins(12, 10, 12, 12);
    pkLayout->setSpacing(6);

    QLabel *pkHeader = new QLabel(tr("Private Key  (DO NOT SHARE)"), privateKeyPanel);
    pkHeader->setStyleSheet("QLabel { color: #c0392b; font-weight: bold; font-size: 13pt; background: transparent; border: 0; }");
    pkLayout->addWidget(pkHeader);

    QLabel *pkExplain = new QLabel(
        tr("Anyone who sees this key can spend every coin at the address above. "
           "Never paste it into a website, screenshot it, or send it in chat. "
           "The QR below can be scanned by the Badcoin iPhone wallet to import this key."),
        privateKeyPanel);
    pkExplain->setStyleSheet("QLabel { color: #6d1f14; background: transparent; border: 0; }");
    pkExplain->setWordWrap(true);
    pkLayout->addWidget(pkExplain);

    // QR image of the WIF, centered. Sized to match the public-address QR up top.
    privateKeyQR = new QLabel(privateKeyPanel);
    privateKeyQR->setAlignment(Qt::AlignCenter);
    privateKeyQR->setMinimumSize(240, 240);
    privateKeyQR->setStyleSheet(
        "QLabel { background: #ffffff; border: 1px solid #e69292; border-radius: 4px; padding: 8px; }"
    );
    pkLayout->addWidget(privateKeyQR, 0, Qt::AlignCenter);

    privateKeyValue = new QLabel(privateKeyPanel);
    privateKeyValue->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    privateKeyValue->setStyleSheet(
        "QLabel { background: #ffffff; border: 1px dashed #c0392b; border-radius: 4px; "
        "padding: 8px; font-family: 'Menlo', 'Courier New', monospace; color: #222; }"
    );
    privateKeyValue->setWordWrap(true);
    pkLayout->addWidget(privateKeyValue);

    QHBoxLayout *pkBtnRow = new QHBoxLayout();
    btnCopyPrivateKey = new QPushButton(tr("Copy Private Key"), privateKeyPanel);
    pkBtnRow->addWidget(btnCopyPrivateKey);
    btnSaveKeysPdf = new QPushButton(tr("Save as PDF..."), privateKeyPanel);
    btnSaveKeysPdf->setToolTip(tr("Save the public address and the private key, with QR "
                                  "codes for each, as a printable PDF."));
    pkBtnRow->addWidget(btnSaveKeysPdf);
    pkBtnRow->addStretch();
    pkLayout->addLayout(pkBtnRow);

    privateKeyPanel->setVisible(false);

    // Insert the panel into the main vertical layout, above the bottom button row.
    if (auto *mainLayout = qobject_cast<QVBoxLayout*>(this->layout())) {
        // Second-to-last item is the bottom button row; insert our panel before it.
        mainLayout->insertWidget(mainLayout->count() - 1, privateKeyPanel);
    }

    // Add the toggle button to the bottom button row.
    btnTogglePrivateKey = new QPushButton(tr("Show Private Key"), this);
    btnTogglePrivateKey->setToolTip(tr("Reveal the private key (WIF) for this address. "
                                       "A warning will be shown first."));
    // The .ui places the copy buttons in a horizontal layout at the bottom of the
    // vertical layout. Find it and insert our toggle button before the spacer.
    if (auto *mainLayout = qobject_cast<QVBoxLayout*>(this->layout())) {
        QLayoutItem *lastItem = mainLayout->itemAt(mainLayout->count() - 1);
        if (lastItem && lastItem->layout()) {
            if (auto *hbox = qobject_cast<QHBoxLayout*>(lastItem->layout())) {
                // Insert after btnSaveAs (index 2) and before the spacer.
                hbox->insertWidget(3, btnTogglePrivateKey);
            }
        }
    }

    connect(btnTogglePrivateKey, SIGNAL(clicked()), this, SLOT(onTogglePrivateKey()));
    connect(btnCopyPrivateKey,   SIGNAL(clicked()), this, SLOT(onCopyPrivateKey()));
    connect(btnSaveKeysPdf,      SIGNAL(clicked()), this, SLOT(onSaveKeysPdf()));
}

ReceiveRequestDialog::~ReceiveRequestDialog()
{
    delete ui;
}

void ReceiveRequestDialog::setModel(OptionsModel *_model)
{
    this->model = _model;

    if (_model)
        connect(_model, SIGNAL(displayUnitChanged(int)), this, SLOT(update()));

    // update the display unit if necessary
    update();
}

void ReceiveRequestDialog::setInfo(const SendCoinsRecipient &_info)
{
    this->info = _info;
    update();
}

void ReceiveRequestDialog::update()
{
    // model may be null when this dialog is opened from the Address Book page
    // (which just wants to show QR + keys and does not care about display units).
    QString target = info.label;
    if(target.isEmpty())
        target = info.address;
    setWindowTitle(tr("Request payment to %1").arg(target));

    QString uri = GUIUtil::formatBitcoinURI(info);
    ui->btnSaveAs->setEnabled(false);
    QString html;
    html += "<html><font face='verdana, arial, helvetica, sans-serif'>";
    html += "<b style='font-size:12pt;'>"+tr("Public Address")+"</b><br>";
    html += "<span style='color:#666;'>"+tr("Safe to share. This is where coins will be received.")+"</span><br><br>";
    html += "<b>"+tr("Payment information")+"</b><br>";
    html += "<b>"+tr("URI")+"</b>: ";
    html += "<a href=\""+uri+"\">" + GUIUtil::HtmlEscape(uri) + "</a><br>";
    html += "<b>"+tr("Address")+"</b>: " + GUIUtil::HtmlEscape(info.address) + "<br>";
    if(info.amount && model)
        html += "<b>"+tr("Amount")+"</b>: " + BitcoinUnits::formatHtmlWithUnit(model->getDisplayUnit(), info.amount) + "<br>";
    if(!info.label.isEmpty())
        html += "<b>"+tr("Label")+"</b>: " + GUIUtil::HtmlEscape(info.label) + "<br>";
    if(!info.message.isEmpty())
        html += "<b>"+tr("Message")+"</b>: " + GUIUtil::HtmlEscape(info.message) + "<br>";
    ui->outUri->setText(html);

#ifdef USE_QRCODE
    ui->lblQRCode->setText("");
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            ui->lblQRCode->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        } else {
            QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (!code)
            {
                ui->lblQRCode->setText(tr("Error encoding URI into QR Code."));
                return;
            }
            QImage qrImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
            qrImage.fill(0xffffff);
            unsigned char *p = code->data;
            for (int y = 0; y < code->width; y++)
            {
                for (int x = 0; x < code->width; x++)
                {
                    qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                    p++;
                }
            }
            QRcode_free(code);

            QImage qrAddrImage = QImage(QR_IMAGE_SIZE, QR_IMAGE_SIZE+20, QImage::Format_RGB32);
            qrAddrImage.fill(0xffffff);
            QPainter painter(&qrAddrImage);
            painter.drawImage(0, 0, qrImage.scaled(QR_IMAGE_SIZE, QR_IMAGE_SIZE));
            QFont font = GUIUtil::fixedPitchFont();
            QRect paddedRect = qrAddrImage.rect();

            // calculate ideal font size
            qreal font_size = GUIUtil::calculateIdealFontSize(paddedRect.width() - 20, info.address, font);
            font.setPointSizeF(font_size);

            painter.setFont(font);
            paddedRect.setHeight(QR_IMAGE_SIZE+12);
            painter.drawText(paddedRect, Qt::AlignBottom|Qt::AlignCenter, info.address);
            painter.end();

            ui->lblQRCode->setPixmap(QPixmap::fromImage(qrAddrImage));
            ui->btnSaveAs->setEnabled(true);
        }
    }
#endif
}

void ReceiveRequestDialog::on_btnCopyURI_clicked()
{
    GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(info));
}

void ReceiveRequestDialog::on_btnCopyAddress_clicked()
{
    GUIUtil::setClipboard(info.address);
}

void ReceiveRequestDialog::onTogglePrivateKey()
{
    if (privateKeyShown) {
        // Hide: clear from display and drop the cached copy. Also wipe the QR
        // pixmap so the key image is not sitting in widget memory any longer
        // than necessary.
        privateKeyPanel->setVisible(false);
        privateKeyValue->clear();
        privateKeyQR->clear();
        cachedPrivateKey.clear();
        btnTogglePrivateKey->setText(tr("Show Private Key"));
        privateKeyShown = false;
        return;
    }

    // Warn the user explicitly before revealing.
    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Reveal private key?"),
        tr("You are about to reveal the private key for this address.\n\n"
           "Anyone who sees, photographs, or records the key can spend every "
           "coin sent to this address, now or in the future.\n"
           "Never paste it into a website, exchange, support chat, or screenshot.\n"
           "The key will be shown on-screen only. You can hide it again by "
           "clicking the button a second time.\n\n"
           "Continue?"),
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel
    );
    if (reply != QMessageBox::Yes) {
        return;
    }

    // Fetch via RPC: dumpprivkey <address>. If the wallet is encrypted, the node
    // returns an error about the wallet being locked; surface that cleanly.
    std::string cmd = "dumpprivkey \"" + info.address.toStdString() + "\"";
    std::string result;
    bool ok = false;
    try {
        ok = RPCConsole::RPCExecuteCommandLine(result, cmd);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Error"), QString::fromUtf8(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, tr("Error"), tr("Unknown error retrieving private key."));
        return;
    }
    if (!ok) {
        QMessageBox::critical(this, tr("Error"), QString::fromStdString(result));
        return;
    }

    // Result is the WIF wrapped in quotes: "Lxxx..."
    QString wif = QString::fromStdString(result).trimmed();
    if (wif.startsWith('"') && wif.endsWith('"')) {
        wif = wif.mid(1, wif.length() - 2);
    }
    if (wif.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Received an empty private key from the wallet."));
        return;
    }

    cachedPrivateKey = wif;
    privateKeyValue->setText(wif);

    // Generate a QR image of the raw WIF string (no URI prefix, since the iOS
    // app's QR scanner reads WIF strings directly through the same Import path).
#ifdef USE_QRCODE
    {
        const int qrSize = 240;
        QRcode *code = QRcode_encodeString(wif.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
        if (code) {
            QImage qrImg(code->width + 8, code->width + 8, QImage::Format_RGB32);
            qrImg.fill(0xffffff);
            unsigned char *p = code->data;
            for (int y = 0; y < code->width; y++) {
                for (int x = 0; x < code->width; x++) {
                    qrImg.setPixel(x + 4, y + 4, ((*p & 1) ? 0x000000 : 0xffffff));
                    p++;
                }
            }
            QRcode_free(code);
            QPixmap pm = QPixmap::fromImage(qrImg.scaled(qrSize, qrSize, Qt::KeepAspectRatio, Qt::FastTransformation));
            privateKeyQR->setPixmap(pm);
        } else {
            privateKeyQR->setText(tr("(Failed to encode private key as QR)"));
        }
    }
#else
    privateKeyQR->setText(tr("(QR support not compiled in)"));
#endif

    privateKeyPanel->setVisible(true);
    btnTogglePrivateKey->setText(tr("Hide Private Key"));
    privateKeyShown = true;
}

void ReceiveRequestDialog::onCopyPrivateKey()
{
    if (cachedPrivateKey.isEmpty()) return;
    // Clipboard copy. Warn once about history / paste-hijack risks. The message
    // box is modal so it is impossible to click without acknowledging.
    GUIUtil::setClipboard(cachedPrivateKey);
    QMessageBox::information(
        this,
        tr("Private key copied"),
        tr("The private key is now in your clipboard.\n\n"
           "Paste it into the destination app immediately, then copy something "
           "else (any harmless text) to evict it from the clipboard. Clipboard "
           "history tools and other apps can read it for as long as it sits there.")
    );
}

void ReceiveRequestDialog::onSaveKeysPdf()
{
    // Only available once the key has been revealed (and the user passed the
    // warning in onTogglePrivateKey).
    if (cachedPrivateKey.isEmpty()) {
        QMessageBox::information(this, tr("Show the private key first"),
            tr("Click \"Show Private Key\" and confirm the warning, then use Save as PDF."));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Save keys as a PDF?"),
        tr("This PDF will contain BOTH the public address and the FULL private key, "
           "with a QR code for each.\n\n"
           "Anyone who gets this file, or a photo of it, can spend every coin at "
           "this address. Print it, keep the paper somewhere safe and offline, "
           "then delete the PDF file.\n\n"
           "Continue?"),
        QMessageBox::Cancel | QMessageBox::Yes,
        QMessageBox::Cancel
    );
    if (reply != QMessageBox::Yes)
        return;

    QString fn = GUIUtil::getSaveFileName(this, tr("Save keys as PDF"), QString(),
                                          tr("PDF file (*.pdf)"), nullptr);
    if (fn.isEmpty())
        return;

    QPdfWriter writer(fn);
    writer.setPageSize(QPagedPaintDevice::A4);
    writer.setResolution(300);

    QPainter painter;
    if (!painter.begin(&writer)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not open the PDF file for writing."));
        return;
    }

    const int res      = writer.resolution();   // dots per inch
    const int pageW    = writer.width();         // page width in dots
    const int margin   = res;                    // 1 inch margin
    const int x        = margin;
    const int contentW = pageW - 2 * margin;
    const int qrSize   = res * 2;                // 2 inch QR codes
    const int textX    = x + qrSize + res / 2;
    const int textW    = contentW - qrSize - res / 2;
    int y = margin;

    QColor warnRed(0xc0, 0x39, 0x2b);
    QFont f = painter.font();

    // Title
    f.setBold(true); f.setPointSize(22);
    painter.setFont(f);
    painter.drawText(QRect(x, y, contentW, res), Qt::AlignHCenter, tr("Badcoin Wallet Keys"));
    y += res * 3 / 2;

    // Public address
    f.setBold(true); f.setPointSize(15);
    painter.setFont(f);
    painter.setPen(Qt::black);
    painter.drawText(QRect(x, y, contentW, res / 2), Qt::AlignLeft,
                     tr("Public Address  (safe to share)"));
    y += res * 2 / 3;
    if (ui->lblQRCode->pixmap() && !ui->lblQRCode->pixmap()->isNull())
        painter.drawImage(QRect(x, y, qrSize, qrSize), ui->lblQRCode->pixmap()->toImage());
    f.setBold(false); f.setPointSize(12); f.setFamily("Courier New");
    painter.setFont(f);
    painter.drawText(QRect(textX, y, textW, qrSize),
                     Qt::AlignVCenter | Qt::TextWordWrap, info.address);
    y += qrSize + res / 2;

    // Private key
    f.setBold(true); f.setPointSize(15); f.setFamily(font().family());
    painter.setFont(f);
    painter.setPen(warnRed);
    painter.drawText(QRect(x, y, contentW, res / 2), Qt::AlignLeft,
                     tr("Private Key  (DO NOT SHARE)"));
    y += res * 2 / 3;
    painter.setPen(Qt::black);
    if (privateKeyQR->pixmap() && !privateKeyQR->pixmap()->isNull())
        painter.drawImage(QRect(x, y, qrSize, qrSize), privateKeyQR->pixmap()->toImage());
    f.setBold(false); f.setPointSize(12); f.setFamily("Courier New");
    painter.setFont(f);
    painter.drawText(QRect(textX, y, textW, qrSize),
                     Qt::AlignVCenter | Qt::TextWordWrap, cachedPrivateKey);
    y += qrSize + res / 2;

    // Warning footer
    f.setBold(true); f.setPointSize(11); f.setFamily(font().family());
    painter.setFont(f);
    painter.setPen(warnRed);
    painter.drawText(QRect(x, y, contentW, res * 2), Qt::AlignLeft | Qt::TextWordWrap,
                     tr("WARNING: anyone who has this private key, or a photo of its QR "
                        "code, can spend every coin at this address. Keep this page "
                        "offline and secret."));

    painter.end();

    QMessageBox::information(this, tr("PDF saved"),
        tr("Your keys were saved to:\n%1\n\nStore it safely, and delete the file when "
           "you no longer need it.").arg(fn));
}

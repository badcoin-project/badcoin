// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MYADDRESSESPAGE_H
#define BITCOIN_QT_MYADDRESSESPAGE_H

#include <QWidget>

class PlatformStyle;
class WalletModel;

QT_BEGIN_NAMESPACE
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTableWidgetItem;
QT_END_NAMESPACE

/**
 * The My Addresses tab in badcoin-qt: a full page (not a popup dialog)
 * listing the wallet's own receiving addresses. Columns: Label (narrow),
 * Address (wide, so the full address is visible), and Balance (the address
 * total, with any immature coinbase portion noted).
 *
 * Buttons:
 *   New                - generate a fresh receiving address.
 *   Copy               - copy the selected address to the clipboard.
 *   Export Private Key - open the receive dialog's warning-gated private-key
 *                        reveal, which also offers the Save-as-PDF paper wallet.
 *   Remove             - clear the selected address's label. An HD wallet
 *                        cannot delete a key, so Remove resets the label only.
 *   Import Address     - bring in an external private key (WIF) so the wallet
 *                        controls the corresponding address. Shown at the
 *                        right of the row to mark it as the "incoming" action.
 */
class MyAddressesPage : public QWidget
{
    Q_OBJECT

public:
    explicit MyAddressesPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);

    void setWalletModel(WalletModel *walletModel);

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void onNew();
    void onCopy();
    void onExportPrivateKey();
    void onRemove();
    void onImportAddress();
    void onItemChanged(QTableWidgetItem *item);
    void updateButtons();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QString selectedAddress() const;
    QString selectedLabel() const;

    enum Column { COL_LABEL = 0, COL_ADDRESS = 1, COL_BALANCE = 2 };

    const PlatformStyle *platformStyle;
    WalletModel *walletModel;
    bool m_populating;   // true while refresh() rebuilds the table

    QTableWidget *table;
    QPushButton *newButton;
    QPushButton *copyButton;
    QPushButton *exportKeyButton;
    QPushButton *removeButton;
    QPushButton *importButton;
};

#endif // BITCOIN_QT_MYADDRESSESPAGE_H

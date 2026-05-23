// Copyright (c) 2025 The Badcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/myaddressespage.h>

#include <qt/addresstablemodel.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/receiverequestdialog.h>
#include <qt/walletmodel.h>

#include <map>
#include <utility>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

MyAddressesPage::MyAddressesPage(const PlatformStyle *_platformStyle, QWidget *parent)
    : QWidget(parent)
    , platformStyle(_platformStyle)
    , walletModel(nullptr)
    , table(nullptr)
    , newButton(nullptr)
    , copyButton(nullptr)
    , exportKeyButton(nullptr)
    , removeButton(nullptr)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr(
        "These are your Badcoin receiving addresses. The Balance column shows each "
        "address's total; any immature amount (freshly mined, not yet spendable) is "
        "noted alongside it."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    table = new QTableWidget(this);
    table->setColumnCount(3);
    QStringList headers;
    headers << tr("Label") << tr("Address") << tr("Balance");
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(false);
    // Label stays narrow, Address stretches so the full address is always
    // visible, Balance sizes to its content.
    table->horizontalHeader()->setSectionResizeMode(COL_LABEL,   QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(COL_ADDRESS, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COL_BALANCE, QHeaderView::ResizeToContents);
    table->setColumnWidth(COL_LABEL, 140);
    layout->addWidget(table);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    newButton       = new QPushButton(tr("New"));
    copyButton      = new QPushButton(tr("Copy"));
    exportKeyButton = new QPushButton(tr("Export Private Key"));
    removeButton    = new QPushButton(tr("Remove"));
    buttonRow->addWidget(newButton);
    buttonRow->addWidget(copyButton);
    buttonRow->addWidget(exportKeyButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    connect(newButton,       SIGNAL(clicked()), this, SLOT(onNew()));
    connect(copyButton,      SIGNAL(clicked()), this, SLOT(onCopy()));
    connect(exportKeyButton, SIGNAL(clicked()), this, SLOT(onExportPrivateKey()));
    connect(removeButton,    SIGNAL(clicked()), this, SLOT(onRemove()));
    connect(table,           SIGNAL(itemSelectionChanged()), this, SLOT(updateButtons()));

    updateButtons();
}

void MyAddressesPage::setWalletModel(WalletModel *_walletModel)
{
    walletModel = _walletModel;
    if (!walletModel)
        return;

    AddressTableModel *atm = walletModel->getAddressTableModel();
    if (atm) {
        connect(atm, SIGNAL(rowsInserted(QModelIndex,int,int)),    this, SLOT(refresh()));
        connect(atm, SIGNAL(rowsRemoved(QModelIndex,int,int)),     this, SLOT(refresh()));
        connect(atm, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(refresh()));
        connect(atm, SIGNAL(modelReset()),                         this, SLOT(refresh()));
    }
    // Balances change as blocks arrive and coinbase matures.
    connect(walletModel, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)),
            this, SLOT(refresh()));

    refresh();
}

void MyAddressesPage::refresh()
{
    if (!walletModel || !table)
        return;
    AddressTableModel *atm = walletModel->getAddressTableModel();
    if (!atm)
        return;

    // Remember the selection so a balance-driven refresh does not lose it.
    const QString keepSelected = selectedAddress();

    std::map<QString, std::pair<CAmount, CAmount> > balances;
    walletModel->listAddressBalances(balances);

    const int unit = walletModel->getOptionsModel()
        ? walletModel->getOptionsModel()->getDisplayUnit()
        : BitcoinUnits::BTC;

    table->setRowCount(0);
    int rowToSelect = -1;

    const int n = atm->rowCount(QModelIndex());
    for (int i = 0; i < n; ++i) {
        const QModelIndex addrIdx = atm->index(i, AddressTableModel::Address, QModelIndex());
        if (atm->data(addrIdx, AddressTableModel::TypeRole).toString() != AddressTableModel::Receive)
            continue;

        const QString address = atm->data(addrIdx, Qt::DisplayRole).toString();
        const QString label   = atm->data(
            atm->index(i, AddressTableModel::Label, QModelIndex()), Qt::DisplayRole).toString();

        CAmount spendable = 0;
        CAmount immature = 0;
        std::map<QString, std::pair<CAmount, CAmount> >::const_iterator it = balances.find(address);
        if (it != balances.end()) {
            spendable = it->second.first;
            immature  = it->second.second;
        }
        const CAmount total = spendable + immature;

        QString balanceText = BitcoinUnits::formatWithUnit(unit, total);
        if (immature > 0)
            balanceText += tr("  (incl. %1 immature)").arg(BitcoinUnits::format(unit, immature));

        const int row = table->rowCount();
        table->insertRow(row);

        QTableWidgetItem *labelItem   = new QTableWidgetItem(label);
        QTableWidgetItem *addressItem = new QTableWidgetItem(address);
        QTableWidgetItem *balanceItem = new QTableWidgetItem(balanceText);

        QFont mono = addressItem->font();
        mono.setStyleHint(QFont::TypeWriter);
        mono.setFamily(QStringLiteral("Monospace"));
        addressItem->setFont(mono);

        if (immature > 0)
            balanceItem->setToolTip(tr("Spendable: %1\nImmature: %2")
                .arg(BitcoinUnits::formatWithUnit(unit, spendable))
                .arg(BitcoinUnits::formatWithUnit(unit, immature)));

        table->setItem(row, COL_LABEL,   labelItem);
        table->setItem(row, COL_ADDRESS, addressItem);
        table->setItem(row, COL_BALANCE, balanceItem);

        if (!keepSelected.isEmpty() && address == keepSelected)
            rowToSelect = row;
    }

    if (rowToSelect >= 0)
        table->selectRow(rowToSelect);

    updateButtons();
}

QString MyAddressesPage::selectedAddress() const
{
    if (!table)
        return QString();
    const QList<QTableWidgetItem*> sel = table->selectedItems();
    if (sel.isEmpty())
        return QString();
    QTableWidgetItem *item = table->item(sel.first()->row(), COL_ADDRESS);
    return item ? item->text() : QString();
}

QString MyAddressesPage::selectedLabel() const
{
    if (!table)
        return QString();
    const QList<QTableWidgetItem*> sel = table->selectedItems();
    if (sel.isEmpty())
        return QString();
    QTableWidgetItem *item = table->item(sel.first()->row(), COL_LABEL);
    return item ? item->text() : QString();
}

void MyAddressesPage::updateButtons()
{
    const bool hasSelection = !selectedAddress().isEmpty();
    if (copyButton)      copyButton->setEnabled(hasSelection);
    if (exportKeyButton) exportKeyButton->setEnabled(hasSelection);
    if (removeButton)    removeButton->setEnabled(hasSelection);
}

void MyAddressesPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
}

void MyAddressesPage::onNew()
{
    if (!walletModel)
        return;
    AddressTableModel *atm = walletModel->getAddressTableModel();
    if (!atm)
        return;

    // Generating a receiving key may need the wallet unlocked.
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid())
        return;

    const QString newAddr = atm->addRow(
        AddressTableModel::Receive, QString(), QString(), walletModel->getDefaultAddressType());
    if (newAddr.isEmpty()) {
        QMessageBox::warning(this, tr("New address failed"),
            tr("Could not generate a new receiving address."));
        return;
    }
    refresh();
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem *item = table->item(row, COL_ADDRESS);
        if (item && item->text() == newAddr) {
            table->selectRow(row);
            break;
        }
    }
}

void MyAddressesPage::onCopy()
{
    const QString addr = selectedAddress();
    if (!addr.isEmpty())
        QApplication::clipboard()->setText(addr);
}

void MyAddressesPage::onExportPrivateKey()
{
    if (!walletModel)
        return;
    const QString addr = selectedAddress();
    if (addr.isEmpty())
        return;

    // Reuse the receive dialog: it carries the warning-gated private-key
    // reveal (WIF + QR) and the Save-as-PDF paper-wallet export.
    SendCoinsRecipient info;
    info.address = addr;
    info.label   = selectedLabel();

    ReceiveRequestDialog *dialog = new ReceiveRequestDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModel(walletModel->getOptionsModel());
    dialog->setInfo(info);
    dialog->show();
}

void MyAddressesPage::onRemove()
{
    if (!walletModel)
        return;
    const QString addr = selectedAddress();
    if (addr.isEmpty())
        return;

    if (QMessageBox::question(this, tr("Remove label"),
            tr("Clear the label for this address?\n\n%1\n\n"
               "The address itself stays in the wallet; only its label is removed.").arg(addr),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    AddressTableModel *atm = walletModel->getAddressTableModel();
    if (!atm)
        return;
    const int row = atm->lookupAddress(addr);
    if (row < 0)
        return;
    atm->setData(atm->index(row, AddressTableModel::Label, QModelIndex()),
                 QString(), Qt::EditRole);
    refresh();
}

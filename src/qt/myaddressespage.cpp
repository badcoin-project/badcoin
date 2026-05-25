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
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
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
    , m_populating(false)
    , table(nullptr)
    , newButton(nullptr)
    , copyButton(nullptr)
    , exportKeyButton(nullptr)
    , removeButton(nullptr)
    , importButton(nullptr)
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
    table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::SelectedClicked
                           | QAbstractItemView::EditKeyPressed);
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
    importButton    = new QPushButton(tr("Import Address"));
    buttonRow->addWidget(newButton);
    buttonRow->addWidget(copyButton);
    buttonRow->addWidget(exportKeyButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();
    // Import is the "bring in from outside" action; placed at the far right
    // of the row, after a stretch, to set it apart from the existing
    // manage-your-own-addresses buttons.
    buttonRow->addWidget(importButton);
    layout->addLayout(buttonRow);

    connect(newButton,       SIGNAL(clicked()), this, SLOT(onNew()));
    connect(copyButton,      SIGNAL(clicked()), this, SLOT(onCopy()));
    connect(exportKeyButton, SIGNAL(clicked()), this, SLOT(onExportPrivateKey()));
    connect(removeButton,    SIGNAL(clicked()), this, SLOT(onRemove()));
    connect(importButton,    SIGNAL(clicked()), this, SLOT(onImportAddress()));
    connect(table,           SIGNAL(itemSelectionChanged()), this, SLOT(updateButtons()));
    connect(table,           SIGNAL(itemChanged(QTableWidgetItem*)),
            this,            SLOT(onItemChanged(QTableWidgetItem*)));

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

    m_populating = true;
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

        // Only the Label cell is editable (double-click to rename).
        addressItem->setFlags(addressItem->flags() & ~Qt::ItemIsEditable);
        balanceItem->setFlags(balanceItem->flags() & ~Qt::ItemIsEditable);

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

    m_populating = false;
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

void MyAddressesPage::onItemChanged(QTableWidgetItem *item)
{
    // Ignore the item changes refresh() makes while it rebuilds the table.
    if (m_populating || !item || item->column() != COL_LABEL || !walletModel)
        return;
    QTableWidgetItem *addrItem = table->item(item->row(), COL_ADDRESS);
    if (!addrItem)
        return;
    AddressTableModel *atm = walletModel->getAddressTableModel();
    if (!atm)
        return;
    const int row = atm->lookupAddress(addrItem->text());
    if (row < 0)
        return;
    // Persist the edited label to the wallet's address book.
    atm->setData(atm->index(row, AddressTableModel::Label, QModelIndex()),
                 item->text(), Qt::EditRole);
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

    // Only zero-balance addresses can be removed. "Balance" here is the
    // address total: spendable plus immature mined coins.
    std::map<QString, std::pair<CAmount, CAmount> > balances;
    walletModel->listAddressBalances(balances);
    std::map<QString, std::pair<CAmount, CAmount> >::const_iterator it = balances.find(addr);
    const CAmount total = (it != balances.end()) ? (it->second.first + it->second.second) : 0;
    if (total != 0) {
        QMessageBox::warning(this, tr("Cannot remove address"),
            tr("This address holds a balance (including any immature mined coins), so it "
               "cannot be removed. Only addresses with a zero balance can be removed."));
        return;
    }

    if (QMessageBox::question(this, tr("Remove address"),
            tr("Remove this address from the wallet's address list?\n\n%1\n\n"
               "The address has a zero balance, so nothing is lost.").arg(addr),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (!walletModel->removeReceivingAddress(addr)) {
        QMessageBox::warning(this, tr("Remove failed"),
            tr("The address could not be removed."));
        return;
    }
    refresh();
}

void MyAddressesPage::onImportAddress()
{
    if (!walletModel)
        return;

    // Build a small modal dialog inline. Two fields (WIF + optional label),
    // a plain-language note about what import does, and Cancel / Import
    // buttons. No rescan from the GUI thread: a full rescan can take many
    // minutes and would freeze the UI. We tell the user how to trigger one
    // afterward instead.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Import Address"));
    dlg.setModal(true);

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);

    QLabel *note = new QLabel(tr(
        "Paste the private key (WIF) for an address you want this wallet to "
        "control. Importing a private key gives this wallet full control of "
        "the corresponding address. Only import keys you trust the source of."),
        &dlg);
    note->setWordWrap(true);
    dlgLayout->addWidget(note);

    QLabel *keyLabel = new QLabel(tr("Private key (WIF):"), &dlg);
    dlgLayout->addWidget(keyLabel);
    QLineEdit *keyEdit = new QLineEdit(&dlg);
    keyEdit->setEchoMode(QLineEdit::Normal);
    QFont mono = keyEdit->font();
    mono.setStyleHint(QFont::TypeWriter);
    mono.setFamily(QStringLiteral("Monospace"));
    keyEdit->setFont(mono);
    keyEdit->setPlaceholderText(tr("e.g. a Badcoin WIF beginning with a capital letter"));
    dlgLayout->addWidget(keyEdit);

    QLabel *labelLabel = new QLabel(tr("Address label (optional):"), &dlg);
    dlgLayout->addWidget(labelLabel);
    QLineEdit *labelEdit = new QLineEdit(&dlg);
    labelEdit->setPlaceholderText(tr("Imported address"));
    dlgLayout->addWidget(labelEdit);

    QLabel *rescanNote = new QLabel(tr(
        "After import, this address's past transactions will not show until "
        "you restart with -rescan or run `rescanblockchain` in the debug "
        "console. Future transactions are picked up immediately."), &dlg);
    rescanNote->setWordWrap(true);
    dlgLayout->addWidget(rescanNote);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);
    QPushButton *importBtn = buttons->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    importBtn->setDefault(true);
    dlgLayout->addWidget(buttons);

    connect(buttons, SIGNAL(accepted()), &dlg, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dlg, SLOT(reject()));

    keyEdit->setFocus();

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString wif   = keyEdit->text().trimmed();
    const QString label = labelEdit->text().trimmed();
    if (wif.isEmpty()) {
        QMessageBox::warning(this, tr("Import address"),
            tr("Please enter a private key."));
        return;
    }

    // Importing writes a new key into the wallet, so an encrypted wallet
    // must be unlocked first. Same pattern as onNew().
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid())
        return;

    bool alreadyHad = false;
    QString errorOut;
    if (!walletModel->importPrivateKey(wif, label, alreadyHad, errorOut)) {
        QMessageBox::warning(this, tr("Import failed"),
            errorOut.isEmpty() ? tr("The address could not be imported.")
                               : errorOut);
        return;
    }

    refresh();

    if (alreadyHad) {
        QMessageBox::information(this, tr("Address already in wallet"),
            tr("The wallet already held this private key. The label has been "
               "updated on the My Addresses tab."));
    } else {
        QMessageBox::information(this, tr("Address imported"),
            tr("The address has been added to the wallet.\n\n"
               "To pick up any past transactions for this address, restart "
               "badcoin-qt with `-rescan`, or run `rescanblockchain` in the "
               "debug console (Help > Debug window > Console). Future "
               "transactions are picked up automatically."));
    }
}

#include <QDateTime>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QAbstractEventDispatcher>
#include "modbusrtuscanner.h"
#include "modbustcpscanner.h"
#include "dialogmodbusscanner.h"
#include "ui_dialogmodbusscanner.h"

///
/// \brief Parity_toString
/// \param parity
/// \return
///
inline QString Parity_toString(QSerialPort::Parity parity)
{
    switch(parity)
    {
        case QSerialPort::NoParity:
        return DialogModbusScanner::tr("None");

        case QSerialPort::EvenParity:
        return DialogModbusScanner::tr("Even");

        case QSerialPort::OddParity:
        return DialogModbusScanner::tr("Odd");

        case QSerialPort::SpaceParity:
        return DialogModbusScanner::tr("Space");

        case QSerialPort::MarkParity:
        return DialogModbusScanner::tr("Mark");

        default:
        break;
    }

    return QString();
}

///
/// \brief DialogRtuScanner::DialogRtuScanner
/// \param parent
///
DialogModbusScanner::DialogModbusScanner(QWidget *parent)
    : QFixedSizeDialog(parent)
    , ui(new Ui::DialogModbusScanner)
{
    ui->setupUi(this);
    ui->progressBar->setAlignment(Qt::AlignCenter);

    ui->radioButtonTCP->click();
    for(auto&& port: QSerialPortInfo::availablePorts())
        ui->comboBoxSerial->addItem(port.portName());

    QHostAddress address, mask;
    for(auto&& eth : QNetworkInterface::allInterfaces()) {
        if((eth.flags() & QNetworkInterface::IsRunning) && !(eth.flags() & QNetworkInterface::IsLoopBack)){
            for (auto&& entry : eth.addressEntries()) {
                if(entry.ip().protocol() == QAbstractSocket::IPv4Protocol){
                    address = entry.ip();
                    mask = entry.netmask();
                    break;
                }
            }
        }
    }

    ui->lineEditIPAddressFrom->setText(address.toString());
    ui->lineEditIPAddressTo->setText(address.toString());
    ui->lineEditSubnetMask->setText(mask.toString());
    on_lineEditSubnetMask_editingFinished();

    auto dispatcher = QAbstractEventDispatcher::instance();
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, &DialogModbusScanner::on_awake);
}

///
/// \brief DialogRtuScanner::~DialogRtuScanner
///
DialogModbusScanner::~DialogModbusScanner()
{
    delete ui;
}

///
/// \brief DialogRtuScanner::changeEvent
/// \param event
///
void DialogModbusScanner::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);

    QDialog::changeEvent(event);
}

///
/// \brief DialogModbusScanner::showEvent
/// \param e
///
void DialogModbusScanner::showEvent(QShowEvent* e)
{
    QFixedSizeDialog::showEvent(e);
    ui->radioButtonRTU->click();
}

///
/// \brief DialogRtuScanner::on_awake
///
void DialogModbusScanner::on_awake()
{
    const bool inProgress = _scanner && _scanner->inProgress();
    const bool rtuScanning = ui->radioButtonRTU->isChecked();
    ui->comboBoxSerial->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxBaudRate->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxDataBits->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxParity->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxStopBits->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxDeviceId->setEnabled(!inProgress);
    ui->groupBoxTimeoute->setEnabled(!inProgress);
    ui->pushButtonClear->setEnabled(!inProgress);
    ui->pushButtonScan->setEnabled(ui->comboBoxSerial->count() > 0);
    ui->pushButtonScan->setText(inProgress ? tr("Stop Scan") : tr("Start Scan"));
}

///
/// \brief DialogRtuScanner::on_pushButtonScan_clicked
///
void DialogModbusScanner::on_pushButtonScan_clicked()
{
    setCursor(Qt::WaitCursor);

    if(!_scanner)
    {
        startScan();
    }
    else
    {
        stopScan();
    }

    setCursor(Qt::ArrowCursor);
}

///
/// \brief DialogRtuScanner::on_pushButtonClear_clicked
///
void DialogModbusScanner::on_pushButtonClear_clicked()
{
    ui->listWidget->clear();
}

///
/// \brief DialogRtuScanner::on_errorOccurred
/// \param error
///
void DialogModbusScanner::on_errorOccurred(const QString& error)
{
    QMessageBox::warning(this, windowTitle(), error);
}

///
/// \brief DialogRtuScanner::on_listWidget_itemDoubleClicked
/// \param item
///
void DialogModbusScanner::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
    stopScan();

    const auto deviceId = item->data(Qt::UserRole + 1).toInt();
    const auto params = item->data(Qt::UserRole).value<ConnectionDetails>();

    emit attemptToConnect(params, deviceId);
    close();
}

///
/// \brief DialogModbusScanner::on_lineEditSubnetMask_editingFinished
///
void DialogModbusScanner::on_lineEditSubnetMask_editingFinished()
{
    const auto hostAddress = QHostAddress(ui->lineEditIPAddressFrom->text());
    if(hostAddress.isNull()) return;

    const auto maskAddress = QHostAddress(ui->lineEditSubnetMask->text());
    if(maskAddress.isNull()) return;

    const auto address = hostAddress.toIPv4Address();
    const auto mask = maskAddress.toIPv4Address();
    ui->lineEditIPAddressFrom->setText(QHostAddress(address & mask).toString());
    ui->lineEditIPAddressTo->setText(QHostAddress(address | ~mask).toString());
}

///
/// \brief DialogModbusScanner::on_radioButtonRTU_clicked
///
void DialogModbusScanner::on_radioButtonRTU_clicked()
{
    ui->groupBoxIPAddressRange->setVisible(false);
    ui->groupBoxSerialPort->setVisible(true);
    ui->labelSpeed->setVisible(true);
    ui->labelDataBits->setVisible(true);
    ui->labelParity->setVisible(true);
    ui->labelStopBits->setVisible(true);
    ui->groupBoxPortRange->setVisible(false);
    ui->groupBoxSubnetMask->setVisible(false);
    ui->labelIPAddress->setVisible(false);
    ui->labelPort->setVisible(false);
    ui->labelScanResultsDesc->setText(tr("PORT: Device Id (serial port settings)"));
}

///
/// \brief DialogModbusScanner::on_radioButtonTCP_clicked
///
void DialogModbusScanner::on_radioButtonTCP_clicked()
{
    ui->groupBoxIPAddressRange->setVisible(true);
    ui->groupBoxPortRange->setVisible(true);
    ui->groupBoxSubnetMask->setVisible(true);
    ui->labelIPAddress->setVisible(true);
    ui->labelPort->setVisible(true);
    ui->groupBoxSerialPort->setVisible(false);
    ui->labelSpeed->setVisible(false);
    ui->labelDataBits->setVisible(false);
    ui->labelParity->setVisible(false);
    ui->labelStopBits->setVisible(false);
    ui->labelScanResultsDesc->setText(tr("IP Address: port (Device Id)"));
}

///
/// \brief DialogRtuScanner::startScan
///
void DialogModbusScanner::startScan()
{         
    if(_scanner)
        return;

    ScanParams params;
    if(ui->radioButtonRTU->isChecked())
    {
        params = createSerialParams();
        _scanner.reset(new ModbusRtuScanner(params, this));
    }
    else
    {
        params = createTcpParams();
        _scanner.reset(new ModbusTcpScanner(params, this));
    }

    if(params.ConnParams.empty())
    {
        stopScan();
        return;
    }

    connect(_scanner.get(), &ModbusScanner::timeout, this, &DialogModbusScanner::on_timeout);
    connect(_scanner.get(), &ModbusScanner::finished, this, &DialogModbusScanner::on_scanFinished);
    connect(_scanner.get(), &ModbusScanner::errorOccurred, this, &DialogModbusScanner::on_errorOccurred);
    connect(_scanner.get(), &ModbusScanner::found, this, &DialogModbusScanner::on_deviceFound);
    connect(_scanner.get(), &ModbusScanner::progress, this, &DialogModbusScanner::on_progress);

    clearScanTime();
    clearProgress();

    _scanner->startScan();
}

///
/// \brief DialogRtuScanner::stopScan
///
void DialogModbusScanner::stopScan()
{
    if(_scanner)
    {
        _scanner->stopScan();
        _scanner.reset();
    }
}

///
/// \brief DialogRtuScanner::clearScanTime
///
void DialogModbusScanner::clearScanTime()
{
    on_timeout(0);
}

///
/// \brief DialogRtuScanner::clearProgress
///
void DialogModbusScanner::clearProgress()
{
    ui->progressBar->setValue(0);
    ui->labelSpeed->setText(QString(tr("Baud Rate:")));
    ui->labelDataBits->setText(QString(tr("Data Bits:")));
    ui->labelParity->setText(QString(tr("Parity:")));
    ui->labelStopBits->setText(QString(tr("Stop Bits:")));
    ui->labelIPAddress->setText(QString(tr("IP Address:")));
    ui->labelPort->setText(QString(tr("Port:")));
    ui->labelAddress->setText(QString(tr("Device Id:")));
}

///
/// \brief DialogModbusScanner::on_deviceFound
/// \param cd
/// \param deviceId
///
void DialogModbusScanner::on_deviceFound(const ConnectionDetails& cd, int deviceId)
{
    QString result;
    if(ui->radioButtonRTU->isChecked())
    {
       result = QString("%1: %2 (%3,%4,%5,%6)").arg(cd.SerialParams.PortName,
                                                    QString::number(deviceId),
                                                    QString::number(cd.SerialParams.BaudRate),
                                                    QString::number(cd.SerialParams.WordLength),
                                                    Parity_toString(cd.SerialParams.Parity),
                                                    QString::number(cd.SerialParams.StopBits));
    }
    else
    {
       result = QString("%1: %2 (%3)").arg(cd.TcpParams.IPAddress,
                                           QString::number(cd.TcpParams.ServicePort),
                                           QString::number(deviceId));
    }

    const auto items = ui->listWidget->findItems(result, Qt::MatchExactly);
    if(items.empty())
    {
       auto item = new QListWidgetItem(ui->listWidget);
       item->setText(result);
       item->setData(Qt::UserRole, QVariant::fromValue(cd));
       item->setData(Qt::UserRole + 1, deviceId);

       ui->listWidget->addItem(item);
    }
}

///
/// \brief DialogModbusScanner::on_progress
/// \param cd
/// \param deviceId
/// \param progress
///
void DialogModbusScanner::on_progress(const ConnectionDetails& cd, int deviceId, int progress)
{
    if(ui->radioButtonRTU->isChecked())
    {
        ui->labelSpeed->setText(QString(tr("Baud Rate: %1")).arg(cd.SerialParams.BaudRate));
        ui->labelDataBits->setText(QString(tr("Data Bits: %1")).arg(cd.SerialParams.WordLength));
        ui->labelParity->setText(QString(tr("Parity: %1")).arg(Parity_toString(cd.SerialParams.Parity)));
        ui->labelStopBits->setText(QString(tr("Stop Bits: %1")).arg(cd.SerialParams.StopBits));
    }
    else
    {
        ui->labelIPAddress->setText(QString(tr("IP Address: %1")).arg(cd.TcpParams.IPAddress));
        ui->labelPort->setText(QString(tr("Port: %1")).arg(cd.TcpParams.ServicePort));
    }

    ui->labelAddress->setText(QString(tr("Device Id: %1")).arg(deviceId));
    ui->progressBar->setValue(progress);
}

///
/// \brief DialogRtuScanner::createSerialParams
///
ScanParams DialogModbusScanner::createSerialParams() const
{
    ScanParams params;

    QVector<QSerialPort::BaudRate> baudRates;
    for(auto&& obj : ui->groupBoxBaudRate->children())
    {
        auto chbx = qobject_cast<QCheckBox*>(obj);
        if(chbx && chbx->isChecked())
            baudRates.push_back((QSerialPort::BaudRate)chbx->text().toInt());
    }

    QVector<QSerialPort::DataBits> dataBits;
    for(auto&& obj: ui->groupBoxDataBits->children())
    {
        auto chbx = qobject_cast<QCheckBox*>(obj);
        if(chbx && chbx->isChecked())
            dataBits.push_back((QSerialPort::DataBits)chbx->text().toInt());
    }

    QVector<QSerialPort::Parity> parities;
    if(ui->checkBoxParityNone->isChecked())
        parities.push_back(QSerialPort::NoParity);
    if(ui->checkBoxParityOdd->isChecked())
        parities.push_back(QSerialPort::OddParity);
    if(ui->checkBoxParityEven->isChecked())
        parities.push_back(QSerialPort::EvenParity);

    QVector<QSerialPort::StopBits> stopBits;
    for(auto&& obj: ui->groupBoxStopBits->children())
    {
        auto chbx = qobject_cast<QCheckBox*>(obj);
        if(chbx && chbx->isChecked())
            stopBits.push_back((QSerialPort::StopBits)chbx->text().toInt());
    }

    for(auto&& baudRate : baudRates)
    {
        for(auto&& wordLength : dataBits)
        {
            for(auto&& parity : parities)
            {
                for(auto&& stop : stopBits)
                {
                    ConnectionDetails cd;
                    cd.Type = ConnectionType::Serial;
                    cd.SerialParams.PortName = ui->comboBoxSerial->currentText();
                    cd.SerialParams.BaudRate = baudRate;
                    cd.SerialParams.WordLength = wordLength;
                    cd.SerialParams.Parity = parity;
                    cd.SerialParams.StopBits = stop;
                    params.ConnParams.append(cd);
                }
            }
        }
    }

    // remove duplicates
    params.ConnParams.erase(std::unique(params.ConnParams.begin(), params.ConnParams.end()), params.ConnParams.end());

    params.Timeout = ui->spinBoxTimeout->value();
    params.DeviceIds = QRange<int>(ui->spinBoxDeviceIdFrom->value(), ui->spinBoxDeviceIdTo->value());

    return params;
}

///
/// \brief DialogModbusScanner::createTcpParams
/// \return
///
ScanParams DialogModbusScanner::createTcpParams() const
{
    ScanParams params;

    const auto addressFrom = QHostAddress(ui->lineEditIPAddressFrom->text());
    const auto addressTo = QHostAddress(ui->lineEditIPAddressTo->text());

    if(addressFrom.isNull() || addressTo.isNull())
        return params;

    QVector<QString> addreses;
    for(quint32 addr = addressFrom.toIPv4Address(); addr <= addressTo.toIPv4Address(); addr++)
    {
        addreses.push_back(QHostAddress(addr).toString());
    }

    const auto portFrom = ui->spinBoxPortFrom->value();
    const auto portTo = ui->spinBoxPortTo->value();

    QVector<int> ports;
    for(auto port = portFrom; port <= portTo; port++)
    {
        ports.push_back(port);
    }

    for(auto&& addr : addreses)
    {
        for(auto&& port : ports)
        {
            ConnectionDetails cd;
            cd.Type = ConnectionType::Tcp;
            cd.TcpParams.IPAddress = addr;
            cd.TcpParams.ServicePort = port;
            params.ConnParams.append(cd);
        }
    }

    params.Timeout = ui->spinBoxTimeout->value();
    params.DeviceIds = QRange<int>(ui->spinBoxDeviceIdFrom->value(), ui->spinBoxDeviceIdTo->value());

    return params;
}

///
/// \brief DialogModbusScanner::on_scanFinished
///
void DialogModbusScanner::on_scanFinished()
{
}

///
/// \brief DialogRtuScanner::on_timeout
/// \param time
///
void DialogModbusScanner::on_timeout(quint64 time)
{
    const auto str = QDateTime::fromSecsSinceEpoch(time).toUTC().toString("hh:mm:ss");
    ui->labelTimeLeft->setText(QString("<html><head/><body><p><span style=\"font-weight:700;\">%1</span></p></body></html>").arg(str));
}

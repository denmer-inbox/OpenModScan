#include "mainwindow.h"
#include "formmodsca.h"
#include "ui_formmodsca.h"

///
/// \brief FormModSca::FormModSca
/// \param num
/// \param parent
///
FormModSca::FormModSca(int num, QModbusClient* client, MainWindow* parent) :
    QWidget(parent)
    , ui(new Ui::FormModSca)
    ,_modbusClient(client)
{
    ui->setupUi(this);
    setWindowTitle(QString("ModSca%1").arg(num));

    ui->lineEditAddress->setInputRange(1, 65534);
    ui->lineEditAddress->enablePaddingZero(true);
    ui->lineEditLength->setInputRange(1, 128);
    ui->lineEditDeviceId->setInputRange(1, 255);
    ui->lineEditAddress->setValue(1);
    ui->lineEditLength->setValue(100);
    ui->lineEditDeviceId->setValue(1);
    ui->outputWidget->setup(displayDefinition());

    connect(parent, &MainWindow::modbusClientChanged,
            [&](QModbusClient* cli)
            {
                _modbusClient = cli;
            });

    connect(&_timer, &QTimer::timeout, this, &FormModSca::on_timeout);
    _timer.setInterval(1000);
    _timer.start();
}

///
/// \brief FormModSca::~FormModSca
///
FormModSca::~FormModSca()
{
    delete ui;
}

///
/// \brief FormModSca::displayDefinition
/// \return
///
DisplayDefinition FormModSca::displayDefinition() const
{
    DisplayDefinition dd;
    dd.ScanRate = _timer.interval();
    dd.DeviceId = ui->lineEditDeviceId->value();
    dd.PointAddress = ui->lineEditAddress->value();
    dd.PointType = ui->comboBoxModbusPointType->currentPointType();
    dd.Length = ui->lineEditLength->value();

    return dd;
}

///
/// \brief FormModSca::setDisplayDefinition
/// \param dd
///
void FormModSca::setDisplayDefinition(const DisplayDefinition& dd)
{
    _timer.setInterval(dd.ScanRate);
    ui->lineEditDeviceId->setValue(dd.DeviceId);
    ui->lineEditAddress->setValue(dd.PointAddress);
    ui->lineEditLength->setValue(dd.Length);
    ui->comboBoxModbusPointType->setCurrentPointType(dd.PointType);

    ui->outputWidget->setup(dd);
}

///
/// \brief FormModSca::displayMode
/// \return
///
DisplayMode FormModSca::displayMode() const
{
    return ui->outputWidget->displayMode();
}

///
/// \brief FormModSca::setDisplayMode
/// \param mode
///
void FormModSca::setDisplayMode(DisplayMode mode)
{
    ui->outputWidget->setDisplayMode(mode);
}

///
/// \brief FormModSca::dataDisplayMode
/// \return
///
DataDisplayMode FormModSca::dataDisplayMode() const
{
    return ui->outputWidget->dataDisplayMode();
}

///
/// \brief FormModSca::setDataDisplayMode
/// \param mode
///
void FormModSca::setDataDisplayMode(DataDisplayMode mode)
{
    ui->outputWidget->setDataDisplayMode(mode);
}

///
/// \brief FormModSca::readyReadData
///
void FormModSca::readyReadData()
{
    auto reply = qobject_cast<QModbusReply*>(sender());
    if (!reply) return;

    if (reply->error() == QModbusDevice::NoError)
    {
        ui->statisticWidget->increaseValidSlaveResponses();
    }

    ui->outputWidget->update(reply);
    reply->deleteLater();
}

///
/// \brief FormModSca::on_timeout
///
void FormModSca::on_timeout()
{
    if(_modbusClient == nullptr) return;
    if(_modbusClient->state() != QModbusDevice::ConnectedState)
    {
        ui->outputWidget->setStatus("Device NOT CONNECTED!");
        return;
    }

    const auto dd = displayDefinition();

    QModbusRequest request;
    switch (dd.PointType)
    {
        case QModbusDataUnit::Coils:
            request = QModbusRequest(QModbusRequest::ReadCoils, quint16(dd.PointAddress - 1), dd.Length);
        break;
        case QModbusDataUnit::DiscreteInputs:
            request = QModbusRequest(QModbusRequest::ReadDiscreteInputs, quint16(dd.PointAddress - 1), dd.Length);
        break;
        case QModbusDataUnit::InputRegisters:
            request = QModbusRequest(QModbusRequest::ReadInputRegisters, quint16(dd.PointAddress - 1), dd.Length);
        break;
        case QModbusDataUnit::HoldingRegisters:
            request = QModbusRequest(QModbusRequest::ReadHoldingRegisters, quint16(dd.PointAddress - 1), dd.Length);
        break;
        default:
        break;
    }

    // update data
    ui->outputWidget->update(request);
    ui->statisticWidget->increaseNumberOfPolls();

    // modbus request
    QModbusDataUnit dataUnit(dd.PointType, dd.PointAddress - 1, dd.Length);
    auto reply = _modbusClient->sendReadRequest(dataUnit, dd.DeviceId);
    if(!reply)
    {
        ui->outputWidget->setStatus(_modbusClient->errorString());
        return;
    }

    if (!reply->isFinished())
        connect(reply, &QModbusReply::finished, this, &FormModSca::readyReadData);
    else
        delete reply; // broadcast replies return immediately
}

///
/// \brief FormModSca::on_lineEditAddress_editingFinished
///
void FormModSca::on_lineEditAddress_editingFinished()
{
    ui->outputWidget->setup(displayDefinition());
}

///
/// \brief FormModSca::on_lineEditLength_editingFinished
///
void FormModSca::on_lineEditLength_editingFinished()
{
    ui->outputWidget->setup(displayDefinition());
}

///
/// \brief FormModSca::on_lineEditDeviceId_editingFinished
///
void FormModSca::on_lineEditDeviceId_editingFinished()
{
    ui->outputWidget->setup(displayDefinition());
}

///
/// \brief FormModSca::on_comboBoxModbusPointType_currentTextChanged
///
void FormModSca::on_comboBoxModbusPointType_currentTextChanged(const QString&)
{
    ui->outputWidget->setup(displayDefinition());
}

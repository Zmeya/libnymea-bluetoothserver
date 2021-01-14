/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2021, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of libnymea-bluetoothserver.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "wirelessservice.h"
#include "bluetoothuuids.h"
#include "loggingcategories.h"

#include <QJsonDocument>
#include <QNetworkInterface>
#include <QLowEnergyDescriptorData>
#include <QLowEnergyCharacteristicData>

WirelessService::WirelessService(QLowEnergyService *service, NetworkManager *networkManager, QObject *parent) :
    QObject(parent),
    m_service(service),
    m_networkManager(networkManager)
{    
    qCDebug(dcNymeaBluetoothServer()) << "Create WirelessService.";

    // Service
    connect(m_service, SIGNAL(characteristicChanged(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicChanged(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(characteristicRead(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicChanged(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(characteristicWritten(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicWritten(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(descriptorWritten(QLowEnergyDescriptor, QByteArray)), this, SLOT(descriptorWritten(QLowEnergyDescriptor, QByteArray)));
    connect(m_service, SIGNAL(error(QLowEnergyService::ServiceError)), this, SLOT(serviceError(QLowEnergyService::ServiceError)));

    // Get the wireless network device if there is any
    if (!m_networkManager->wirelessAvailable()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: There is no wireless network device available";
        return;
    }

    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Using" << m_networkManager->wirelessNetworkDevices().first();
    m_device = m_networkManager->wirelessNetworkDevices().first();
    connect(m_device, &WirelessNetworkDevice::bitRateChanged, this, &WirelessService::onWirelessDeviceBitRateChanged);
    connect(m_device, &WirelessNetworkDevice::stateChanged, this, &WirelessService::onWirelessDeviceStateChanged);
    connect(m_device, &WirelessNetworkDevice::wirelessModeChanged, this, &WirelessService::onWirelessModeChanged);
}

QLowEnergyService *WirelessService::service()
{
    return m_service;
}

QLowEnergyServiceData WirelessService::serviceData(NetworkManager *networkManager)
{
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(wirelessServiceUuid);

    QLowEnergyDescriptorData clientConfigDescriptorData(QBluetoothUuid::ClientCharacteristicConfiguration, QByteArray(2, 0));

    // Wifi commander characterisitc e081fec1-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessCommanderCharacteristicData;
    wirelessCommanderCharacteristicData.setUuid(wirelessCommanderCharacteristicUuid);
    wirelessCommanderCharacteristicData.setProperties(QLowEnergyCharacteristic::Write);
    wirelessCommanderCharacteristicData.setValueLength(0, 20);
    serviceData.addCharacteristic(wirelessCommanderCharacteristicData);

    // Response characterisitc e081fec2-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessResponseCharacteristicData;
    wirelessResponseCharacteristicData.setUuid(wirelessResponseCharacteristicUuid);
    wirelessResponseCharacteristicData.setProperties(QLowEnergyCharacteristic::Notify);
    wirelessResponseCharacteristicData.addDescriptor(clientConfigDescriptorData);
    wirelessResponseCharacteristicData.setValueLength(0, 20);
    serviceData.addCharacteristic(wirelessResponseCharacteristicData);

    // Wireless connection status characterisitc e081fec3-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessStatusCharacteristicData;
    wirelessStatusCharacteristicData.setUuid(wirelessStateCharacteristicUuid);
    wirelessStatusCharacteristicData.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Notify);
    wirelessStatusCharacteristicData.addDescriptor(clientConfigDescriptorData);
    wirelessStatusCharacteristicData.setValueLength(1, 1);
    if (networkManager->wirelessNetworkDevices().isEmpty()) {
        wirelessStatusCharacteristicData.setValue(WirelessService::getWirelessNetworkDeviceState(NetworkDevice::NetworkDeviceStateUnknown));
    } else {
        wirelessStatusCharacteristicData.setValue(WirelessService::getWirelessNetworkDeviceState(networkManager->wirelessNetworkDevices().first()->deviceState()));
    }
    serviceData.addCharacteristic(wirelessStatusCharacteristicData);

    // Wireless mode characterisitc e081fec4-f757-4449-b9c9-bfa83133f7fc
    QLowEnergyCharacteristicData wirelessModeCharacteristicData;
    wirelessModeCharacteristicData.setUuid(wirelessModeCharacteristicUuid);
    wirelessModeCharacteristicData.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Notify);
    wirelessModeCharacteristicData.addDescriptor(clientConfigDescriptorData);
    wirelessModeCharacteristicData.setValueLength(1, 1);
    if (networkManager->wirelessNetworkDevices().isEmpty()) {
        wirelessModeCharacteristicData.setValue(WirelessService::getWirelessMode(WirelessNetworkDevice::WirelessModeUnknown));
    } else {
        wirelessModeCharacteristicData.setValue(WirelessService::getWirelessMode(networkManager->wirelessNetworkDevices().first()->wirelessMode()));
    }
    serviceData.addCharacteristic(wirelessModeCharacteristicData);

    return serviceData;
}


WirelessService::WirelessServiceResponse WirelessService::checkWirelessErrors()
{
    // Check possible errors
    if (!m_networkManager->available()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: The networkmanager is not available.";
        return WirelessServiceResponseNetworkManagerNotAvailable;
    }

    if (!m_device) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: There is no wireless device available.";
        return WirelessServiceResponseWirelessNotAvailable;
    }

    if (!m_networkManager->networkingEnabled()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Networking not enabled";
        return WirelessServiceResponseNetworkingNotEnabled;
    }

    if (!m_networkManager->wirelessEnabled()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless not enabled";
        return WirelessServiceResponseWirelessNotEnabled;
    }

    return WirelessServiceResponseSuccess;
}

QByteArray WirelessService::getWirelessNetworkDeviceState(const NetworkDevice::NetworkDeviceState &state)
{
    switch (state) {
    case NetworkDevice::NetworkDeviceStateUnknown:
        return QByteArray::fromHex("00");
    case NetworkDevice::NetworkDeviceStateUnmanaged:
        return QByteArray::fromHex("01");
    case NetworkDevice::NetworkDeviceStateUnavailable:
        return QByteArray::fromHex("02");
    case NetworkDevice::NetworkDeviceStateDisconnected:
        return QByteArray::fromHex("03");
    case NetworkDevice::NetworkDeviceStatePrepare:
        return QByteArray::fromHex("04");
    case NetworkDevice::NetworkDeviceStateConfig:
        return QByteArray::fromHex("05");
    case NetworkDevice::NetworkDeviceStateNeedAuth:
        return QByteArray::fromHex("06");
    case NetworkDevice::NetworkDeviceStateIpConfig:
        return QByteArray::fromHex("07");
    case NetworkDevice::NetworkDeviceStateIpCheck:
        return QByteArray::fromHex("08");
    case NetworkDevice::NetworkDeviceStateSecondaries:
        return QByteArray::fromHex("09");
    case NetworkDevice::NetworkDeviceStateActivated:
        return QByteArray::fromHex("0a");
    case NetworkDevice::NetworkDeviceStateDeactivating:
        return QByteArray::fromHex("0b");
    case NetworkDevice::NetworkDeviceStateFailed:
        return QByteArray::fromHex("0c");
    }

    // Unknown
    return QByteArray::fromHex("00");
}

QByteArray WirelessService::getWirelessMode(WirelessNetworkDevice::WirelessMode mode)
{
    switch (mode) {
    case WirelessNetworkDevice::WirelessModeUnknown:
        return QByteArray::fromHex("00");
    case WirelessNetworkDevice::WirelessModeAdhoc:
        return QByteArray::fromHex("01");
    case WirelessNetworkDevice::WirelessModeInfrastructure:
        return QByteArray::fromHex("02");
    case WirelessNetworkDevice::WirelessModeAccessPoint:
        return QByteArray::fromHex("03");
    }

    // Unknown
    return QByteArray::fromHex("00");
}


void WirelessService::streamData(const QVariantMap &responseMap)
{
    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    QByteArray data = QJsonDocument::fromVariant(responseMap).toJson(QJsonDocument::Compact) + '\n';
    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Start streaming response data:" << data.count() << "bytes";

    int sentDataLength = 0;
    QByteArray remainingData = data;
    while (!remainingData.isEmpty()) {
        QByteArray package = remainingData.left(20);
        sentDataLength += package.count();
        m_service->writeCharacteristic(characteristic, package);
        remainingData = remainingData.remove(0, package.count());
    }

    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Finished streaming response data";
}

QVariantMap WirelessService::createResponse(const WirelessService::WirelessServiceCommand &command, const WirelessService::WirelessServiceResponse &responseCode)
{
    QVariantMap response;
    response.insert("c", static_cast<int>(command));
    response.insert("r", static_cast<int>(responseCode));
    return response;
}

void WirelessService::commandGetNetworks(const QVariantMap &request)
{
    Q_UNUSED(request)

    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    QVariantList accessPointVariantList;
    foreach (WirelessAccessPoint *accessPoint, m_device->accessPoints()) {
        QVariantMap accessPointVariantMap;
        accessPointVariantMap.insert("e", accessPoint->ssid());
        accessPointVariantMap.insert("m", accessPoint->macAddress());
        accessPointVariantMap.insert("s", accessPoint->signalStrength());
        accessPointVariantMap.insert("p", static_cast<int>(accessPoint->isProtected()));
        accessPointVariantList.append(accessPointVariantMap);
    }

    QVariantMap response = createResponse(WirelessServiceCommandGetNetworks);
    response.insert("p", accessPointVariantList);

    streamData(response);
}

void WirelessService::commandConnect(const QVariantMap &request)
{
    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    if (!request.contains("p")) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Connect command: Missing parameters.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidParameters));
        return;
    }

    QVariantMap parameters = request.value("p").toMap();
    if (!parameters.contains("e") || !parameters.contains("p")) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Connect command: Invalid parameters.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidParameters));
        return;
    }

    NetworkManager::NetworkManagerError networkError = m_networkManager->connectWifi(m_device->interface(), parameters.value("e").toString(), parameters.value("p").toString());
    WirelessService::WirelessServiceResponse responseCode = WirelessService::WirelessServiceResponseSuccess;
    switch (networkError) {
    case NetworkManager::NetworkManagerErrorNoError:
        break;
    case NetworkManager::NetworkManagerErrorWirelessNetworkingDisabled:
        responseCode = WirelessService::WirelessServiceResponseWirelessNotEnabled;
        break;
    case NetworkManager::NetworkManagerErrorWirelessConnectionFailed:
        responseCode = WirelessService::WirelessServiceResponseUnknownError;
        break;
    default:
        responseCode = WirelessService::WirelessServiceResponseUnknownError;
        break;
    }
    streamData(createResponse(WirelessServiceCommandConnect, responseCode));
}

void WirelessService::commandConnectHidden(const QVariantMap &request)
{
    Q_UNUSED(request)

    // TODO:
    qCWarning(dcNymeaBluetoothServer()) << "Connect to hidden network is not implemented yet.";
}

void WirelessService::commandDisconnect(const QVariantMap &request)
{
    Q_UNUSED(request)

    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    m_device->disconnectDevice();
    streamData(createResponse(WirelessServiceCommandDisconnect));
}

void WirelessService::commandScan(const QVariantMap &request)
{
    Q_UNUSED(request)

    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    m_device->scanWirelessNetworks();
    streamData(createResponse(WirelessServiceCommandScan));
}

void WirelessService::commandGetCurrentConnection(const QVariantMap &request)
{
    Q_UNUSED(request)

    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not stream wireless network list. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    QVariantMap connectionDataMap;
    QNetworkInterface wifiInterface = QNetworkInterface::interfaceFromName(m_device->interface());
    if (!m_device->activeAccessPoint() || !wifiInterface.isValid() || wifiInterface.addressEntries().isEmpty()) {
        qCDebug(dcNymeaBluetoothServer()) << "There is currently no access active accesspoint";
        connectionDataMap.insert("e", "");
        connectionDataMap.insert("m", "");
        connectionDataMap.insert("s", 0);
        connectionDataMap.insert("p", 0);
        connectionDataMap.insert("i", "");
    } else {
        QHostAddress address;
        // Note: for now, we'll just use the first IPv4 address. However, in a future version
        // this should somehow pack all addresses, IPv4 and IPv6 ones.
        foreach (const QNetworkAddressEntry &entry, wifiInterface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                address = entry.ip();
                break;
            }
        }
        qCDebug(dcNymeaBluetoothServer()) << "Current connection:" << m_device->activeAccessPoint() << address.toString();
        connectionDataMap.insert("e", m_device->activeAccessPoint()->ssid());
        connectionDataMap.insert("m", m_device->activeAccessPoint()->macAddress());
        connectionDataMap.insert("s", m_device->activeAccessPoint()->signalStrength());
        connectionDataMap.insert("p", static_cast<int>(m_device->activeAccessPoint()->isProtected()));
        connectionDataMap.insert("i", address.toString());
    }

    QVariantMap response = createResponse(WirelessServiceCommandGetCurrentConnection);
    response.insert("p", connectionDataMap);
    streamData(response);
}

void WirelessService::commandStartAccessPoint(const QVariantMap &request)
{
    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not start access point. Service is not valid.";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Wireless response characteristic not valid";
        return;
    }

    if (!request.contains("p")) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Start access point command: Missing parameters.";
        streamData(createResponse(WirelessServiceCommandStartAccessPoint, WirelessServiceResponseIvalidParameters));
        return;
    }

    QVariantMap parameters = request.value("p").toMap();
    if (!parameters.contains("e")) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Missing ESSID (e) parameter.";
        streamData(createResponse(WirelessServiceCommandStartAccessPoint, WirelessServiceResponseIvalidParameters));
        return;
    }

    QString essid = parameters.value("e").toString();
    if (essid.length() > 32) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Invalid ESSID (e) parameter.";
        streamData(createResponse(WirelessServiceCommandStartAccessPoint, WirelessServiceResponseIvalidParameters));
        return;
    }

    if (!parameters.contains("p")) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Missing passkey (p) parameter.";
        streamData(createResponse(WirelessServiceCommandStartAccessPoint, WirelessServiceResponseIvalidParameters));
        return;
    }

    QString passkey = parameters.value("p").toString();
    if (passkey.length() < 8 || passkey.length() > 64) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Invalid passkey (p) parameter.";
        streamData(createResponse(WirelessServiceCommandStartAccessPoint, WirelessServiceResponseIvalidParameters));
        return;
    }

    NetworkManager::NetworkManagerError status = m_networkManager->startAccessPoint(m_device->interface(), essid, passkey);
    if (status != NetworkManager::NetworkManagerErrorNoError) {
        qCWarning(dcNymeaBluetoothServer()) << "Failed to start the access point:" << status;
        // FIXME: Add more error codes so that we can actually report this failure to the client
        streamData(createResponse(WirelessServiceCommandStartAccessPoint, WirelessServiceResponseUnknownError));
        return;
    }

    streamData(createResponse(WirelessServiceCommandStartAccessPoint, WirelessServiceResponseSuccess));
}

void WirelessService::characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    // Command
    if (characteristic.uuid() == wirelessCommanderCharacteristicUuid) {
        // Check if currently reading
        if (m_readingInputData) {
            m_inputDataStream.append(value);
        } else {
            m_inputDataStream.clear();
            m_readingInputData = true;
            m_inputDataStream.append(value);
        }

        // If command finished
        if (value.endsWith('\n')) {
            QJsonParseError error;
            QJsonDocument jsonDocument = QJsonDocument::fromJson(m_inputDataStream, &error);
            if (error.error != QJsonParseError::NoError) {
                qCWarning(dcNymeaBluetoothServer()) << "Got invalid json object" << m_inputDataStream;
                m_inputDataStream.clear();
                m_readingInputData = false;
                return;
            }

            qCDebug(dcNymeaBluetoothServer()) << "Got command stream" << jsonDocument.toJson();

            processCommand(jsonDocument.toVariant().toMap());

            m_inputDataStream.clear();
            m_readingInputData = false;
        }

        // Limit possible data stream to prevent overflow
        if (m_inputDataStream.length() >= 20 * 1024) {
            m_inputDataStream.clear();
            m_readingInputData = false;
            return;
        }
    }
}

void WirelessService::characteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Characteristic read" << characteristic.uuid().toString() << value;
}

void WirelessService::characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Characteristic written" << characteristic.uuid().toString() << value;
}

void WirelessService::descriptorRead(const QLowEnergyDescriptor &descriptor, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Descriptor read" << descriptor.uuid().toString() << value;
}

void WirelessService::descriptorWritten(const QLowEnergyDescriptor &descriptor, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Descriptor written" << descriptor.uuid().toString() << value;
}

void WirelessService::serviceError(const QLowEnergyService::ServiceError &error)
{
    QString errorString;
    switch (error) {
    case QLowEnergyService::NoError:
        errorString = "No error";
        break;
    case QLowEnergyService::OperationError:
        errorString = "Operation error";
        break;
    case QLowEnergyService::CharacteristicReadError:
        errorString = "Characteristic read error";
        break;
    case QLowEnergyService::CharacteristicWriteError:
        errorString = "Characteristic write error";
        break;
    case QLowEnergyService::DescriptorReadError:
        errorString = "Descriptor read error";
        break;
    case QLowEnergyService::DescriptorWriteError:
        errorString = "Descriptor write error";
        break;
    case QLowEnergyService::UnknownError:
        errorString = "Unknown error";
        break;
    }

    qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Error:" << errorString;
}

void WirelessService::processCommand(const QVariantMap &request)
{
    if (!request.contains("c")) {
        qCWarning(dcNymeaBluetoothServer()) << "Invalid request. Command value missing.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidCommand));
        return;
    }

    bool commandIntOk;
    int command = request.value("c").toInt(&commandIntOk);
    if (!commandIntOk) {
        qCWarning(dcNymeaBluetoothServer()) << "Invalid request. Could not convert method to interger.";
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidCommand));
        return;
    }

    // Check wireless errors
    WirelessServiceResponse responseCode = checkWirelessErrors();
    if (responseCode != WirelessServiceResponseSuccess) {
        streamData(createResponse(static_cast<WirelessServiceCommand>(command), responseCode));
        return;
    }

    // Process method
    qCDebug(dcNymeaBluetoothServer()) << "Received command" << static_cast<WirelessServiceCommand>(command);
    switch (command) {
    case WirelessServiceCommandGetNetworks:
        commandGetNetworks(request);
        break;
    case WirelessServiceCommandConnect:
        commandConnect(request);
        break;
    case WirelessServiceCommandConnectHidden:
        commandConnectHidden(request);
        break;
    case WirelessServiceCommandDisconnect:
        commandDisconnect(request);
        break;
    case WirelessServiceCommandScan:
        commandScan(request);
        break;
    case WirelessServiceCommandGetCurrentConnection:
        commandGetCurrentConnection(request);
        break;
    case WirelessServiceCommandStartAccessPoint:
        commandStartAccessPoint(request);
        break;
    default:
        qCWarning(dcNymeaBluetoothServer()) << "Invalid request. Unknown command" << command;
        streamData(createResponse(WirelessServiceCommandConnect, WirelessServiceResponseIvalidCommand));
        break;
    }
}

void WirelessService::onWirelessDeviceBitRateChanged(const int &bitRate)
{
    Q_UNUSED(bitRate)
}

void WirelessService::onWirelessDeviceStateChanged(const NetworkDevice::NetworkDeviceState &state)
{
    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Wireless network device state changed" << state;

    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not update wireless network device state. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessStateCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not update wireless network device state. Characteristic not valid";
        return;
    }

    m_service->writeCharacteristic(characteristic, WirelessService::getWirelessNetworkDeviceState(state));
}

void WirelessService::onWirelessModeChanged(WirelessNetworkDevice::WirelessMode mode)
{
    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not update wireless device mode. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessModeCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "WirelessService: Could not update wireless device mode. Characteristic not valid";
        return;
    }

    qCDebug(dcNymeaBluetoothServer()) << "WirelessService: Notify wireless mode changed" << WirelessService::getWirelessMode(mode);
    m_service->writeCharacteristic(characteristic, WirelessService::getWirelessMode(mode));
}

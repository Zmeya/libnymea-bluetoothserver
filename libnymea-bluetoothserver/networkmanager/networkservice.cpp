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

#include "networkservice.h"
#include "loggingcategories.h"

#include <QLowEnergyDescriptorData>
#include <QLowEnergyCharacteristicData>

NetworkService::NetworkService(QLowEnergyService *service, NetworkManager *networkManager, QObject *parent) :
    QObject(parent),
    m_service(service),
    m_networkManager(networkManager)
{
    qCDebug(dcNymeaBluetoothServer()) << "Create NetworkService.";

    // Service
    connect(m_service, SIGNAL(characteristicChanged(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicChanged(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(characteristicRead(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicChanged(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(characteristicWritten(QLowEnergyCharacteristic, QByteArray)), this, SLOT(characteristicWritten(QLowEnergyCharacteristic, QByteArray)));
    connect(m_service, SIGNAL(descriptorWritten(QLowEnergyDescriptor, QByteArray)), this, SLOT(descriptorWritten(QLowEnergyDescriptor, QByteArray)));
    connect(m_service, SIGNAL(error(QLowEnergyService::ServiceError)), this, SLOT(serviceError(QLowEnergyService::ServiceError)));

    // NetworkManager
    connect(m_networkManager, &NetworkManager::stateChanged, this, &NetworkService::onNetworkManagerStateChanged);
    connect(m_networkManager, &NetworkManager::networkingEnabledChanged, this, &NetworkService::onNetworkingEnabledChanged);
    connect(m_networkManager, &NetworkManager::wirelessEnabledChanged, this, &NetworkService::onWirelessEnabledChanged);
}

QLowEnergyService *NetworkService::service()
{
    return m_service;
}

QLowEnergyServiceData NetworkService::serviceData(NetworkManager *networkManager)
{
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(networkServiceUuid);

    QLowEnergyDescriptorData clientConfigDescriptorData(QBluetoothUuid::ClientCharacteristicConfiguration, QByteArray(2, 0));

    // Network manager status ef6d661-b8af-49e0-9eca-ab343513641c
    QLowEnergyCharacteristicData networkStatusData;
    networkStatusData.setUuid(networkStatusCharacteristicUuid);
    networkStatusData.setValue(QByteArray(1, 0));
    networkStatusData.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Notify);
    networkStatusData.addDescriptor(clientConfigDescriptorData);
    networkStatusData.setValue(NetworkService::getNetworkManagerStateByteArray(networkManager->state()));
    serviceData.addCharacteristic(networkStatusData);

    // Network manager commander ef6d6612-b8af-49e0-9eca-ab343513641c
    QLowEnergyCharacteristicData networkCommanderCharacteristicData;
    networkCommanderCharacteristicData.setUuid(networkCommanderCharacteristicUuid);
    networkCommanderCharacteristicData.setProperties(QLowEnergyCharacteristic::Write);
    networkCommanderCharacteristicData.setValueLength(1, 1);
    serviceData.addCharacteristic(networkCommanderCharacteristicData);

    // Response characteristic ef6d6613-b8af-49e0-9eca-ab343513641c
    QLowEnergyCharacteristicData networkResponseCharacteristicData;
    networkResponseCharacteristicData.setUuid(networkResponseCharacteristicUuid);
    networkResponseCharacteristicData.setProperties(QLowEnergyCharacteristic::Notify);
    networkResponseCharacteristicData.addDescriptor(clientConfigDescriptorData);
    networkResponseCharacteristicData.setValueLength(1, 1);
    serviceData.addCharacteristic(networkResponseCharacteristicData);

    // Networking enabled ef6d6614-b8af-49e0-9eca-ab343513641c
    QLowEnergyCharacteristicData networkingEnabledStatusData;
    networkingEnabledStatusData.setUuid(networkingEnabledCharacteristicUuid);
    networkingEnabledStatusData.setValue(QByteArray(1, 0));
    networkingEnabledStatusData.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Notify);
    networkingEnabledStatusData.setValue(QByteArray::number(static_cast<int>(networkManager->networkingEnabled())));
    serviceData.addCharacteristic(networkingEnabledStatusData);

    // Wireless enabled ef6d6615-b8af-49e0-9eca-ab343513641c
    QLowEnergyCharacteristicData wirelessEnabledStatusData;
    wirelessEnabledStatusData.setUuid(wirelessEnabledCharacteristicUuid);
    wirelessEnabledStatusData.setValue(QByteArray(1, 0));
    wirelessEnabledStatusData.setProperties(QLowEnergyCharacteristic::Read | QLowEnergyCharacteristic::Notify);
    wirelessEnabledStatusData.setValue(QByteArray::number(static_cast<int>(networkManager->wirelessEnabled())));
    serviceData.addCharacteristic(wirelessEnabledStatusData);

    return serviceData;
}

QByteArray NetworkService::getNetworkManagerStateByteArray(const NetworkManager::NetworkManagerState &state)
{
    QByteArray networkManagerState;
    switch (state) {
    case NetworkManager::NetworkManagerStateUnknown:
        networkManagerState = QByteArray::fromHex("00");
        break;
    case NetworkManager::NetworkManagerStateAsleep:
        networkManagerState = QByteArray::fromHex("01");
        break;
    case NetworkManager::NetworkManagerStateDisconnected:
        networkManagerState = QByteArray::fromHex("02");
        break;
    case NetworkManager::NetworkManagerStateDisconnecting:
        networkManagerState = QByteArray::fromHex("03");
        break;
    case NetworkManager::NetworkManagerStateConnecting:
        networkManagerState = QByteArray::fromHex("04");
        break;
    case NetworkManager::NetworkManagerStateConnectedLocal:
        networkManagerState = QByteArray::fromHex("05");
        break;
    case NetworkManager::NetworkManagerStateConnectedSite:
        networkManagerState = QByteArray::fromHex("06");
        break;
    case NetworkManager::NetworkManagerStateConnectedGlobal:
        networkManagerState = QByteArray::fromHex("07");
        break;
    }

    return networkManagerState;
}

void NetworkService::sendResponse(const NetworkService::NetworkServiceResponse &response)
{
    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not send response. Service not valid";
        return;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(networkResponseCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not send response. Characteristic not valid";
        return;
    }

    switch (response) {
    case NetworkServiceResponseSuccess:
        m_service->writeCharacteristic(characteristic, QByteArray::fromHex("00"));
        break;
    case NetworkServiceResponseIvalidValue:
        m_service->writeCharacteristic(characteristic, QByteArray::fromHex("01"));
        break;
    case NetworkServiceResponseNetworkManagerNotAvailable:
        m_service->writeCharacteristic(characteristic, QByteArray::fromHex("02"));
        break;
    case NetworkServiceResponseWirelessNotAvailable:
        m_service->writeCharacteristic(characteristic, QByteArray::fromHex("03"));
        break;
    default:
        // Unknown error
        m_service->writeCharacteristic(characteristic, QByteArray::fromHex("04"));
        break;
    }
}

NetworkService::NetworkServiceCommand NetworkService::verifyCommand(const QByteArray &commandData)
{
    if (commandData.length() != 1)
        return NetworkServiceCommandInvalid;

    uint commandInteger = commandData.toHex().toUInt(nullptr, 16);
    switch (commandInteger) {
    case NetworkServiceCommandEnableNetworking:
        return NetworkServiceCommandEnableNetworking;
    case NetworkServiceCommandDisableNetworking:
        return NetworkServiceCommandDisableNetworking;
    case NetworkServiceCommandEnableWireless:
        return NetworkServiceCommandEnableWireless;
    case NetworkServiceCommandDisableWireless:
        return NetworkServiceCommandDisableWireless;
    default:
        break;
    }

    return NetworkServiceCommandInvalid;
}

void NetworkService::characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    if (characteristic.uuid() == networkCommanderCharacteristicUuid) {

        NetworkServiceCommand command = verifyCommand(value);
        if (command == NetworkServiceCommandInvalid) {
            qCWarning(dcNymeaBluetoothServer()) << "NetworkService: received invalid command" << command;
            sendResponse(NetworkServiceResponseIvalidValue);
            return;
        }

        if (!m_networkManager->available()) {
            qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Networkmanager not available";
            sendResponse(NetworkServiceResponseNetworkManagerNotAvailable);
            return;
        }

        processCommand(command);

        sendResponse(NetworkServiceResponseSuccess);
        return;
    }

    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Characteristic changed" << characteristic.uuid().toString() << value;
}

void NetworkService::characteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Characteristic read" << characteristic.uuid().toString() << value;
}

void NetworkService::characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Characteristic written" << characteristic.uuid().toString() << value;
}

void NetworkService::descriptorRead(const QLowEnergyDescriptor &descriptor, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Descriptor read" << descriptor.uuid().toString() << value;
}

void NetworkService::descriptorWritten(const QLowEnergyDescriptor &descriptor, const QByteArray &value)
{
    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Descriptor written" << descriptor.uuid().toString() << value;
}

void NetworkService::serviceError(const QLowEnergyService::ServiceError &error)
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

    qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Error:" << errorString;
}

void NetworkService::processCommand(const NetworkServiceCommand &command)
{
    switch (command) {
    case NetworkServiceCommandEnableNetworking:
        qCDebug(dcNymeaBluetoothServer()) << "NetworkService: received \"Enable networking\" command";
        m_networkManager->enableNetworking(true);
        break;
    case NetworkServiceCommandDisableNetworking:
        qCDebug(dcNymeaBluetoothServer()) << "NetworkService: received \"Disable networking\" command";
        m_networkManager->enableNetworking(false);
        break;
    case NetworkServiceCommandEnableWireless:
        qCDebug(dcNymeaBluetoothServer()) << "NetworkService: received \"Enable wireless networking\" command";
        m_networkManager->enableWireless(true);
        break;
    case NetworkServiceCommandDisableWireless:
        qCDebug(dcNymeaBluetoothServer()) << "NetworkService: received \"Disable wireless networking\" command";
        m_networkManager->enableWireless(false);
        break;
    default:
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Unhandled command" << command;
        sendResponse(NetworkServiceResponseIvalidValue);
        break;
    }
}

bool NetworkService::onNetworkManagerStateChanged()
{
    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not updatet network manager status. Service not valid";
        return false;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(networkStatusCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not update network manager status. Characteristic not valid";
        return false;
    }

    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Notify state changed" << NetworkService::getNetworkManagerStateByteArray(m_networkManager->state());
    m_service->writeCharacteristic(characteristic, NetworkService::getNetworkManagerStateByteArray(m_networkManager->state()));
    return true;
}

bool NetworkService::onNetworkingEnabledChanged()
{
    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not set networking enabled. Service not valid";
        return false;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(networkingEnabledCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not set networking enabled. Characteristic not valid";
        return false;
    }

    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Notify networking enabled changed:" << (m_networkManager->networkingEnabled() ? "enabled" : "disabled");
    m_service->writeCharacteristic(characteristic, m_networkManager->networkingEnabled() ? QByteArray::fromHex("01") : QByteArray::fromHex("00"));
    return true;
}

bool NetworkService::onWirelessEnabledChanged()
{
    if (!m_service) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not set wireless enabled. Service not valid";
        return false;
    }

    QLowEnergyCharacteristic characteristic = m_service->characteristic(wirelessEnabledCharacteristicUuid);
    if (!characteristic.isValid()) {
        qCWarning(dcNymeaBluetoothServer()) << "NetworkService: Could not set wireless enabled. Characteristic not valid";
        return false;
    }

    qCDebug(dcNymeaBluetoothServer()) << "NetworkService: Notify wireless networking enabled changed:" << (m_networkManager->wirelessEnabled() ? "enabled" : "disabled");
    m_service->writeCharacteristic(characteristic, m_networkManager->wirelessEnabled() ? QByteArray::fromHex("01") : QByteArray::fromHex("00"));
    return true;
}

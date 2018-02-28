/* Copyright 2013-2016 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#ifndef NANOLEAFCENTRAL_H_
#define NANOLEAFCENTRAL_H_

#include <homegear-base/BaseLib.h>
#include "NanoleafPeer.h"

#include <memory>
#include <mutex>
#include <string>

namespace Nanoleaf
{

class NanoleafCentral : public BaseLib::Systems::ICentral
{
public:
	NanoleafCentral(ICentralEventSink* eventHandler);
	NanoleafCentral(uint32_t deviceType, std::string serialNumber, int32_t address, ICentralEventSink* eventHandler);
	virtual ~NanoleafCentral();
	virtual void dispose(bool wait = true);

	virtual void loadVariables();
	virtual void saveVariables();
	virtual void loadPeers();
	virtual void savePeers(bool full);

	virtual void homegearShuttingDown();

    virtual bool onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet) { return false; }
	virtual std::string handleCliCommand(std::string command);
	virtual uint64_t getPeerIdFromSerial(std::string& serialNumber) { std::shared_ptr<NanoleafPeer> peer = getPeer(serialNumber); if(peer) return peer->getID(); else return 0; }

	std::shared_ptr<NanoleafPeer> getPeer(uint64_t id);
	std::shared_ptr<NanoleafPeer> getPeer(std::string serialNumber);

    void searchDevicesThread(bool updateOnly);

	virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags);
	virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags);
	virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, ParameterGroup::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, PVariable paramset);
	virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable paramset);
	virtual PVariable searchDevices(BaseLib::PRpcClientInfo clientInfo);
protected:
    std::unique_ptr<BaseLib::Ssdp> _ssdp;
	std::atomic_bool _shuttingDown;

	std::atomic_bool _stopWorkerThread;
	std::thread _workerThread;
    int32_t _pollingInterval = 5000;

	std::mutex _searchNanoleafsMutex;
    std::atomic_bool _searching;
    std::mutex _searchDevicesMutex;
    std::thread _searchDevicesThread;

	/**
	 * Creates a new peer. The method does not add the peer to the peer arrays.
	 *
	 * @param address The RF address of the peer.
	 * @param firmwareVersion The firmware version.
	 * @param deviceType The device type.
	 * @param serialNumber The serial number.
	 * @param save (default true) Set to "true" to save the peer in the database.
	 * @return Returns a pointer to the newly created peer on success. If the creation was not successful, a nullptr is returned.
	 */
	std::shared_ptr<NanoleafPeer> createPeer(uint32_t deviceType, std::string serialNumber, std::string ip, bool save = true);
	void deletePeer(uint64_t id);

	void init();
	void worker();
};

}

#endif

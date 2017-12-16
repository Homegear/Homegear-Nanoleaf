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

#include "NanoleafPeer.h"
#include "NanoleafCentral.h"
#include "GD.h"

namespace Nanoleaf
{
std::shared_ptr<BaseLib::Systems::ICentral> NanoleafPeer::getCentral()
{
	try
	{
		if(_central) return _central;
		_central = GD::family->getCentral();
		return _central;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return std::shared_ptr<BaseLib::Systems::ICentral>();
}

NanoleafPeer::NanoleafPeer(uint32_t parentID, IPeerEventSink* eventHandler) : Peer(GD::bl, parentID, eventHandler)
{
    _binaryEncoder.reset(new BaseLib::Rpc::RpcEncoder(GD::bl));
    _binaryDecoder.reset(new BaseLib::Rpc::RpcDecoder(GD::bl));
	_jsonEncoder.reset(new BaseLib::Rpc::JsonEncoder(GD::bl));
	_jsonDecoder.reset(new BaseLib::Rpc::JsonDecoder(GD::bl));
	_saveTeam = true;
}

NanoleafPeer::NanoleafPeer(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler) : Peer(GD::bl, id, address, serialNumber, parentID, eventHandler)
{
    _binaryEncoder.reset(new BaseLib::Rpc::RpcEncoder(GD::bl));
    _binaryDecoder.reset(new BaseLib::Rpc::RpcDecoder(GD::bl));
    _jsonEncoder.reset(new BaseLib::Rpc::JsonEncoder(GD::bl));
    _jsonDecoder.reset(new BaseLib::Rpc::JsonDecoder(GD::bl));
	_saveTeam = true;
}

NanoleafPeer::~NanoleafPeer()
{
	dispose();
}

void NanoleafPeer::setIp(std::string value)
{
	try
	{
		Peer::setIp(value);
		std::string settingName = "readtimeout";
		BaseLib::Systems::FamilySettings::PFamilySetting readTimeoutSetting = GD::family->getFamilySetting(settingName);
		int32_t readTimeout = 10000;
		if(readTimeoutSetting) readTimeout = readTimeoutSetting->integerValue;
		if(readTimeout < 1 || readTimeout > 120000) readTimeout = 10000;
		_httpClient.reset(new BaseLib::HttpClient(GD::bl, _ip, 16021, false));
		_httpClient->setTimeout(readTimeout);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void NanoleafPeer::worker()
{
    try
    {
        if(deleting || !_httpClient || _ip.empty()) return;

        bool requestNewApiKey = false;
        if(!_apiKey.empty())
        {
            BaseLib::Http http;
            _httpClient->get("/api/v1/" + _apiKey + "/", http);
            if(http.getHeader().responseCode >= 200 && http.getHeader().responseCode < 300)
            {
                packetReceived(_jsonDecoder->decode(http.getContent()));
            }
            else if(http.getHeader().responseCode == 401) requestNewApiKey = true;
            else _bl->out.printWarning("Warning: Unhandled HTTP code received from Nanoleaf: " + std::to_string(http.getHeader().responseCode));
        }
        else requestNewApiKey = true;

        if(requestNewApiKey)
        {
            BaseLib::Http http;
            std::string postRequest = "POST /api/v1/new HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _ip + ":16021" + "\r\nConnection: Close\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n";
            _httpClient->sendRequest(postRequest, http, false);
            if(http.getContentSize() == 0)
            {
                _bl->out.printWarning("Warning: Peer " + std::to_string(_peerID) + " has no auth token set. Please press the power button on your Nanoleaf controller for three seconds.");
            }
            else
            {
                auto json = _jsonDecoder->decode(http.getContent());
                auto authTokenIterator = json->structValue->find("auth_token");
                if(authTokenIterator != json->structValue->end())
                {
                    setApiKey(BaseLib::HelperFunctions::stripNonAlphaNumeric(authTokenIterator->second->stringValue));
                    _bl->out.printInfo("Info: Peer " + std::to_string(_peerID) + " got new auth token.");
                }
            }
        }
    }
    catch(BaseLib::Exception& ex)
    {
        serviceMessages->setUnreach(true, false);
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::string NanoleafPeer::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;

		if(command == "help")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "unselect\t\tUnselect this peer" << std::endl;
			return stringStream.str();
		}
		return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return "Error executing command. See log file for more details.\n";
}

void NanoleafPeer::save(bool savePeer, bool variables, bool centralConfig)
{
	try
	{
		Peer::save(savePeer, variables, centralConfig);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void NanoleafPeer::loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
	try
	{
        std::string settingName = "readtimeout";
        BaseLib::Systems::FamilySettings::PFamilySetting readTimeoutSetting = GD::family->getFamilySetting(settingName);
        int32_t readTimeout = 10000;
        if(readTimeoutSetting) readTimeout = readTimeoutSetting->integerValue;
        if(readTimeout < 1 || readTimeout > 120000) readTimeout = 10000;

		if(!rows) rows = _bl->db->getPeerVariables(_peerID);
		Peer::loadVariables(central, rows);

        for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
        {
            switch(row->second.at(2)->intValue)
            {
                case 1:
                    _apiKey = row->second.at(4)->textValue;
                    break;
            }
        }

        _httpClient.reset(new BaseLib::HttpClient(GD::bl, _ip, 16021, false));
        _httpClient->setTimeout(readTimeout);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool NanoleafPeer::load(BaseLib::Systems::ICentral* central)
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows;
		loadVariables(central, rows);

		_rpcDevice = GD::family->getRpcDevices()->find(_deviceType, _firmwareVersion, -1);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString(_deviceType) + " Firmware version: " + std::to_string(_firmwareVersion));
			return false;
		}
		initializeTypeString();
		std::string entry;
		loadConfig();
		initializeCentralConfig();

		serviceMessages.reset(new BaseLib::Systems::ServiceMessages(_bl, _peerID, _serialNumber, this));
		serviceMessages->load();

		return true;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

void NanoleafPeer::saveVariables()
{
	try
	{
		if(_peerID == 0) return;
		Peer::saveVariables();
        saveVariable(1, _apiKey);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

PParameterGroup NanoleafPeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		PParameterGroup parameterGroup = _rpcDevice->functions.at(channel)->getParameterGroup(type);
		if(!parameterGroup || parameterGroup->parameters.empty())
		{
			GD::out.printDebug("Debug: Parameter set of type " + std::to_string(type) + " not found for channel " + std::to_string(channel));
			return PParameterGroup();
		}
		return parameterGroup;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return PParameterGroup();
}

bool NanoleafPeer::getAllValuesHook2(PRpcClientInfo clientInfo, PParameter parameter, uint32_t channel, PVariable parameters)
{
	try
	{
		if(channel == 1)
		{
			if(parameter->id == "PEER_ID")
			{
				std::vector<uint8_t> parameterData;
				parameter->convertToPacket(PVariable(new Variable((int32_t)_peerID)), parameterData);
				valuesCentral[channel][parameter->id].setBinaryData(parameterData);
			}
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

void NanoleafPeer::getValuesFromPacket(BaseLib::PVariable json, std::vector<FrameValues>& frameValues)
{
	try
	{
        if(!_rpcDevice) return;
        //equal_range returns all elements with "0" or an unknown element as argument
        if(_rpcDevice->packetsByMessageType.find(1) == _rpcDevice->packetsByMessageType.end()) return;
        std::pair<PacketsByMessageType::iterator, PacketsByMessageType::iterator> range = _rpcDevice->packetsByMessageType.equal_range(1);
        if(range.first == _rpcDevice->packetsByMessageType.end()) return;
        PacketsByMessageType::iterator i = range.first;
        do
        {
            FrameValues currentFrameValues;
            PPacket frame(i->second);
            if(!frame) continue;
            int32_t channel = -1;
            if(frame->channel > -1) channel = frame->channel;
            currentFrameValues.frameID = frame->id;

            for(JsonPayloads::iterator j = frame->jsonPayloads.begin(); j != frame->jsonPayloads.end(); ++j)
            {
                BaseLib::PVariable currentJson = json;
                auto keyIterator = currentJson->structValue->find((*j)->key);
                if(keyIterator == currentJson->structValue->end()) continue;
                currentJson = keyIterator->second;
                if(!(*j)->subkey.empty())
                {
                    auto subkeyIterator = currentJson->structValue->find((*j)->subkey);
                    if(subkeyIterator == currentJson->structValue->end()) continue;
                    currentJson = subkeyIterator->second;
                    if(!(*j)->subsubkey.empty())
                    {
                        auto subsubkeyIterator = currentJson->structValue->find((*j)->subsubkey);
                        if(subsubkeyIterator == currentJson->structValue->end()) continue;
                        currentJson = subsubkeyIterator->second;
                    }
                }

                for(std::vector<PParameter>::iterator k = frame->associatedVariables.begin(); k != frame->associatedVariables.end(); ++k)
                {
                    if((*k)->physical->groupId != (*j)->parameterId) continue;
                    currentFrameValues.parameterSetType = (*k)->parent()->type();
                    bool setValues = false;
                    if(currentFrameValues.paramsetChannels.empty()) //Fill paramsetChannels
                    {
                        int32_t startChannel = (channel < 0) ? 0 : channel;
                        int32_t endChannel;
                        //When fixedChannel is -2 (means '*') cycle through all channels
                        if(frame->channel == -2)
                        {
                            startChannel = 0;
                            endChannel = _rpcDevice->functions.rbegin()->first;
                        }
                        else endChannel = startChannel;
                        for(int32_t l = startChannel; l <= endChannel; l++)
                        {
                            Functions::iterator functionIterator = _rpcDevice->functions.find(l);
                            if(functionIterator == _rpcDevice->functions.end()) continue;
                            PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
                            if(!parameterGroup || parameterGroup->parameters.find((*k)->id) == parameterGroup->parameters.end()) continue;
                            currentFrameValues.paramsetChannels.push_back(l);
                            currentFrameValues.values[(*k)->id].channels.push_back(l);
                            setValues = true;
                        }
                    }
                    else //Use paramsetChannels
                    {
                        for(std::list<uint32_t>::const_iterator l = currentFrameValues.paramsetChannels.begin(); l != currentFrameValues.paramsetChannels.end(); ++l)
                        {
                            Functions::iterator functionIterator = _rpcDevice->functions.find(*l);
                            if(functionIterator == _rpcDevice->functions.end()) continue;
                            PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
                            if(!parameterGroup || parameterGroup->parameters.find((*k)->id) == parameterGroup->parameters.end()) continue;
                            currentFrameValues.values[(*k)->id].channels.push_back(*l);
                            setValues = true;
                        }
                    }

                    if(setValues)
                    {
                        //This is a little nasty and costs a lot of resources, but we need to run the data through the packet converter
                        std::vector<uint8_t> encodedData;
                        _binaryEncoder->encodeResponse(currentJson, encodedData);
                        PVariable data = (*k)->convertFromPacket(encodedData, true);
                        (*k)->convertToPacket(data, currentFrameValues.values[(*k)->id].value);
                    }
                }
            }
            if(!currentFrameValues.values.empty()) frameValues.push_back(currentFrameValues);
        } while(++i != range.second && i != _rpcDevice->packetsByMessageType.end());
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void NanoleafPeer::packetReceived(PVariable json)
{
	try
	{
        setLastPacketReceived();
        serviceMessages->setUnreach(false, false);
        std::vector<FrameValues> frameValues;
        getValuesFromPacket(json, frameValues);
        std::map<uint32_t, std::shared_ptr<std::vector<std::string>>> valueKeys;
        std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>> rpcValues;

        //Loop through all matching frames
        for(std::vector<FrameValues>::iterator a = frameValues.begin(); a != frameValues.end(); ++a)
        {
            PPacket frame;
            if(!a->frameID.empty()) frame = _rpcDevice->packetsById.at(a->frameID);

            for(std::map<std::string, FrameValue>::iterator i = a->values.begin(); i != a->values.end(); ++i)
            {
                for(std::list<uint32_t>::const_iterator j = a->paramsetChannels.begin(); j != a->paramsetChannels.end(); ++j)
                {
                    if(std::find(i->second.channels.begin(), i->second.channels.end(), *j) == i->second.channels.end()) continue;

                    BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[*j][i->first];
                    if(parameter.equals(i->second.value)) continue;

                    if(!valueKeys[*j] || !rpcValues[*j])
                    {
                        valueKeys[*j].reset(new std::vector<std::string>());
                        rpcValues[*j].reset(new std::vector<PVariable>());
                    }

                    parameter.setBinaryData(i->second.value);
                    if(parameter.databaseId > 0) saveParameter(parameter.databaseId, i->second.value);
                    else saveParameter(0, ParameterGroup::Type::Enum::variables, *j, i->first, i->second.value);
                    if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + i->first + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(*j) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(i->second.value) + ".");

                    if(parameter.rpcParameter)
                    {
                        //Process service messages
                        if(parameter.rpcParameter->service && !i->second.value.empty())
                        {
                            if(parameter.rpcParameter->logical->type == ILogical::Type::Enum::tEnum)
                            {
                                serviceMessages->set(i->first, i->second.value.at(i->second.value.size() - 1), *j);
                            }
                            else if(parameter.rpcParameter->logical->type == ILogical::Type::Enum::tBoolean)
                            {
                                serviceMessages->set(i->first, (bool)i->second.value.at(i->second.value.size() - 1));
                            }
                        }

                        valueKeys[*j]->push_back(i->first);
                        rpcValues[*j]->push_back(parameter.rpcParameter->convertFromPacket(i->second.value, true));
                    }
                }
            }

            if(!rpcValues.empty())
            {
                for(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>::const_iterator j = valueKeys.begin(); j != valueKeys.end(); ++j)
                {
                    if(j->second->empty()) continue;
                    std::string address(_serialNumber + ":" + std::to_string(j->first));
                    raiseEvent(_peerID, j->first, j->second, rpcValues.at(j->first));
                    raiseRPCEvent(_peerID, j->first, address, j->second, rpcValues.at(j->first));
                }
            }
        }
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::string NanoleafPeer::getFirmwareVersionString(int32_t firmwareVersion)
{
	try
	{
		return std::to_string(firmwareVersion);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
	return "";
}

//RPC Methods
PVariable NanoleafPeer::getParamsetDescription(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set");

		return Peer::getParamsetDescription(clientInfo, parameterGroup);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable NanoleafPeer::putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		if(type == ParameterGroup::Type::Enum::variables)
		{
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				setValue(clientInfo, channel, i->first, i->second, true);
			}
		}
		else
		{
			return Variable::createError(-3, "Parameter set type is not supported.");
		}
		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable NanoleafPeer::getParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set");
		PVariable variables(new Variable(VariableType::tStruct));

		for(Parameters::iterator i = parameterGroup->parameters.begin(); i != parameterGroup->parameters.end(); ++i)
		{
			if(i->second->id.empty()) continue;
			if(!i->second->visible && !i->second->service && !i->second->internal && !i->second->transform)
			{
				GD::out.printDebug("Debug: Omitting parameter " + i->second->id + " because of it's ui flag.");
				continue;
			}
			PVariable element;
			if(type == ParameterGroup::Type::Enum::variables)
			{
				if(!i->second->readable) continue;
				if(valuesCentral.find(channel) == valuesCentral.end()) continue;
				if(valuesCentral[channel].find(i->second->id) == valuesCentral[channel].end()) continue;
				std::vector<uint8_t> parameterData = valuesCentral[channel][i->second->id].getBinaryData();
				element = i->second->convertFromPacket(parameterData);
			}

			if(!element) continue;
			if(element->type == VariableType::tVoid) continue;
			variables->structValue->insert(StructElement(i->second->id, element));
		}
		return variables;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable NanoleafPeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait)
{
	return setValue(clientInfo, channel, valueKey, value, false, wait);
}

PVariable NanoleafPeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool noSending, bool wait)
{
	try
	{
        Peer::setValue(clientInfo, channel, valueKey, value, wait); //Ignore result, otherwise setHomegerValue might not be executed
        if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
        if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
        if(channel == 0 && serviceMessages->set(valueKey, value->booleanValue)) return PVariable(new Variable(VariableType::tVoid));
        std::unordered_map<uint32_t, std::unordered_map<std::string, RpcConfigurationParameter>>::iterator channelIterator = valuesCentral.find(channel);
        if(channelIterator == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
        std::unordered_map<std::string, RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(valueKey);
        if(parameterIterator == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");
        PParameter rpcParameter = parameterIterator->second.rpcParameter;
        if(!rpcParameter) return Variable::createError(-5, "Unknown parameter.");
        if(rpcParameter->logical->type == ILogical::Type::tAction && !value->booleanValue) return Variable::createError(-5, "Parameter of type action cannot be set to \"false\".");
        BaseLib::Systems::RpcConfigurationParameter& parameter = parameterIterator->second;
        std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>());
        std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable>());

        if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::store)
        {
            std::vector<uint8_t> parameterData;
            rpcParameter->convertToPacket(value, parameterData);
            parameter.setBinaryData(parameterData);
            if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
            else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameterData);

            value = rpcParameter->convertFromPacket(parameterData, false);
            if(rpcParameter->readable)
            {
                valueKeys->push_back(valueKey);
                values->push_back(value);
            }
            if(!valueKeys->empty()) raiseRPCEvent(_peerID, channel, _serialNumber + ":" + std::to_string(channel), valueKeys, values);
            return PVariable(new Variable(VariableType::tVoid));
        }
        else if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::command) return Variable::createError(-6, "Parameter is not settable.");
        if(rpcParameter->setPackets.empty()) return Variable::createError(-6, "parameter is read only");
        std::string setRequest = rpcParameter->setPackets.front()->id;
        PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(setRequest);
        if(packetIterator == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + valueKey);
        PPacket frame = packetIterator->second;
        std::vector<uint8_t> parameterData;
        rpcParameter->convertToPacket(value, parameterData);
        parameter.setBinaryData(parameterData);
        if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
        else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameterData);

        value = rpcParameter->convertFromPacket(parameterData, false);
        if(_bl->debugLevel > 4) GD::out.printDebug("Debug: " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel) + " was set to " + BaseLib::HelperFunctions::getHexString(parameterData) + ", " + value->print(false, false, true) + ".");

        valueKeys->push_back(valueKey);
        values->push_back(value);

        if(!noSending)
        {
            PVariable json = std::make_shared<Variable>(VariableType::tStruct);
            for(JsonPayloads::iterator i = frame->jsonPayloads.begin(); i != frame->jsonPayloads.end(); ++i)
            {
                if((*i)->constValueIntegerSet)
                {
                    if((*i)->key.empty()) continue;
                    PVariable fieldElement;
                    if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = PVariable(new Variable((*i)->constValueInteger));
                    else
                    {
                        auto keyIterator = json->structValue->find((*i)->key);
                        if(keyIterator == json->structValue->end()) keyIterator = json->structValue->emplace((*i)->key, std::make_shared<Variable>(VariableType::tStruct)).first;
                        keyIterator->second->structValue->emplace((*i)->subkey, std::make_shared<Variable>((*i)->constValueInteger));
                    }
                    continue;
                }
                if((*i)->constValueBooleanSet)
                {
                    if((*i)->key.empty()) continue;
                    if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = PVariable(new Variable((*i)->constValueBoolean));
                    else
                    {
                        auto keyIterator = json->structValue->find((*i)->key);
                        if(keyIterator == json->structValue->end()) keyIterator = json->structValue->emplace((*i)->key, std::make_shared<Variable>(VariableType::tStruct)).first;
                        keyIterator->second->structValue->emplace((*i)->subkey, std::make_shared<Variable>((*i)->constValueBoolean));
                    }
                    continue;
                }
                //We can't just search for param, because it is ambiguous (see for example LEVEL for HM-CC-TC).
                if((*i)->parameterId == rpcParameter->physical->groupId)
                {
                    std::vector<uint8_t> parameterData = parameter.getBinaryData();
                    if((*i)->key.empty()) //JSON
                    {
                        json = _jsonDecoder->decode(_binaryDecoder->decodeResponse(parameterData)->stringValue);
                    }
                    else
                    {
                        if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = _binaryDecoder->decodeResponse(parameterData); //Parameter already is in packet format. Just convert it from RPC to BaseLib::Variable.
                        else
                        {
                            auto keyIterator = json->structValue->find((*i)->key);
                            if(keyIterator == json->structValue->end()) keyIterator = json->structValue->emplace((*i)->key, std::make_shared<Variable>(VariableType::tStruct)).first;
                            keyIterator->second->structValue->emplace((*i)->subkey, _binaryDecoder->decodeResponse(parameterData));
                        }
                    }
                }
                    //Search for all other parameters
                else
                {
                    bool paramFound = false;
                    for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
                    {
                        if(!j->second.rpcParameter) continue;
                        if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
                        {
                            if((*i)->key.empty()) continue;
                            std::vector<uint8_t> parameterData = j->second.getBinaryData();
                            if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = _binaryDecoder->decodeResponse(parameterData); //Parameter already is in packet format. Just convert it from RPC to BaseLib::Variable.
                            else  json->structValue->operator[]((*i)->key)->structValue->operator[]((*i)->subkey) = _binaryDecoder->decodeResponse(parameterData);
                            paramFound = true;
                            break;
                        }
                    }
                    if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
                }
            }

            std::string content;
            _jsonEncoder->encode(json, content);

            BaseLib::Http http;
            std::string postRequest = "PUT /api/v1/" + _apiKey + "/" + frame->function1 + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _ip + ":16021" + "\r\nConnection: Close\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
            _httpClient->sendRequest(postRequest, http, false);
        }

        if(!valueKeys->empty())
        {
            raiseEvent(_peerID, channel, valueKeys, values);
            raiseRPCEvent(_peerID, channel, _serialNumber + ":" + std::to_string(channel), valueKeys, values);
        }

        return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error. See error log for more details.");
}
//End RPC methods
}

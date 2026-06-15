#pragma once

#include "ll/api/io/Logger.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/deps/core/utility/ReadOnlyBinaryStream.h"
#include "mc/network/MinecraftPackets.h"
#include "mc/network/NetworkIdentifier.h"
#include "mc/network/NetworkSystem.h"
#include "mc/world/actor/player/Player.h"

#include <sculk/protocol/utility/BinaryStream.hpp>

#include <cstddef>
#include <vector>

namespace phantom::net {

template <typename PacketT>
bool sendSculkPacketTo(NetworkIdentifier const& networkId, SubClientId subId, PacketT const& packet, ll::io::Logger& logger) {
    std::vector<std::byte>        bodyBuffer;
    sculk::protocol::BinaryStream bodyStream(bodyBuffer);
    packet.write(bodyStream);

    std::string          checkBuffer(reinterpret_cast<char const*>(bodyBuffer.data()), bodyBuffer.size());
    ReadOnlyBinaryStream checkStream(checkBuffer, true);
    auto                 checkPacket = MinecraftPackets::createPacket(static_cast<MinecraftPacketIds>(packet.getId()));
    if (!checkPacket || !checkPacket->read(checkStream)) {
        logger.warn("Sculk packet validation failed for {} ({})", packet.getName(), static_cast<int>(packet.getId()));
        return false;
    }

    BinaryStream sendStream;
    sendStream.writeUnsignedVarInt(
        (static_cast<int>(packet.getId()) & 0x3FF) | ((static_cast<int>(subId) & 3) << 12),
        nullptr,
        nullptr
    );
    sendStream.mBuffer.append(reinterpret_cast<char const*>(bodyBuffer.data()), bodyBuffer.size());

    auto networkSystem = ll::service::getNetworkSystem();
    if (!networkSystem) {
        return false;
    }
    auto* peer = networkSystem->getPeerForUser(networkId);
    if (peer == nullptr) {
        return false;
    }
    peer->sendPacket(sendStream.mBuffer, NetworkPeer::Reliability::Reliable, Compressibility::Compressible);
    return true;
}

template <typename PacketT>
bool sendSculkPacketTo(Player& player, PacketT const& packet, ll::io::Logger& logger) {
    return sendSculkPacketTo(player.getNetworkIdentifier(), SubClientId::PrimaryClient, packet, logger);
}

} // namespace phantom::net

#pragma once

#include <sculk/protocol/codec/MinecraftPacketIds.hpp>
#include <sculk/protocol/codec/level/PrimitiveShapes.hpp>
#include <sculk/protocol/codec/math/Vec3.hpp>
#include <sculk/protocol/utility/BinaryStream.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phantom::net {

class PrimitiveShapesPacket {
public:
    struct Shape {
        std::uint64_t                        networkId{};
        std::optional<sculk::protocol::Vec3> location{};
        std::optional<std::int32_t>          dimensionId{};
        std::string                          text{};
        std::optional<float>                 viewDistance{-1.0};
        bool                                 remove{false};
    };

    std::vector<Shape> shapes{};

    [[nodiscard]] sculk::protocol::MinecraftPacketIds getId() const noexcept {
        return sculk::protocol::MinecraftPacketIds::PrimitiveShapes;
    }

    [[nodiscard]] std::string_view getName() const noexcept { return "PrimitiveShapesPacket"; }

    void write(sculk::protocol::BinaryStream& stream) const {
        stream.writeUnsignedVarInt(static_cast<std::uint32_t>(shapes.size()));
        for (auto const& shape : shapes) {
            writeShape(stream, shape);
        }
    }

private:
    static void writeShape(sculk::protocol::BinaryStream& stream, Shape const& shape) {
        stream.writeUnsignedVarInt64(shape.networkId);
        if (shape.remove) {
            std::optional<sculk::protocol::PrimitiveShapesType> type;
            std::optional<sculk::protocol::Vec3>                location;
            std::optional<sculk::protocol::Vec3>                rotation;
            std::optional<float>                                scale;
            std::optional<std::int32_t>                         color;
            std::optional<float>                                timeLeftTotalSec;
            std::optional<std::int64_t>                         attachedToId;
            std::optional<float>                                viewDistance;

            stream.writeOptional(
                type,
                [](sculk::protocol::BinaryStream& out, sculk::protocol::PrimitiveShapesType value) {
                    out.writeEnum(value, &sculk::protocol::BinaryStream::writeByte);
                }
            );
            stream.writeOptional(location, &sculk::protocol::Vec3::write);
            stream.writeOptional(scale, &sculk::protocol::BinaryStream::writeFloat);
            stream.writeOptional(rotation, &sculk::protocol::Vec3::write);
            stream.writeOptional(timeLeftTotalSec, &sculk::protocol::BinaryStream::writeFloat);
            stream.writeOptional(viewDistance, &sculk::protocol::BinaryStream::writeFloat);
            stream.writeOptional(color, &sculk::protocol::BinaryStream::writeSignedInt);
            stream.writeOptional(shape.dimensionId, &sculk::protocol::BinaryStream::writeVarInt);
            stream.writeOptional(attachedToId, &sculk::protocol::BinaryStream::writeVarInt64);
            stream.writeUnsignedVarInt(0);
            return;
        }

        std::optional<sculk::protocol::PrimitiveShapesType> type = sculk::protocol::PrimitiveShapesType::Text;
        std::optional<sculk::protocol::Vec3>                rotation;
        std::optional<float>                                scale;
        std::optional<std::int32_t>                         color;
        std::optional<float>                                timeLeftTotalSec;
        std::optional<std::int64_t>                         attachedToId;
        std::optional<std::int32_t>                         backgroundColor;

        stream.writeOptional(type, [](sculk::protocol::BinaryStream& out, sculk::protocol::PrimitiveShapesType value) {
            out.writeEnum(value, &sculk::protocol::BinaryStream::writeByte);
        });
        stream.writeOptional(shape.location, &sculk::protocol::Vec3::write);
        stream.writeOptional(scale, &sculk::protocol::BinaryStream::writeFloat);
        stream.writeOptional(rotation, &sculk::protocol::Vec3::write);
        stream.writeOptional(timeLeftTotalSec, &sculk::protocol::BinaryStream::writeFloat);
        stream.writeOptional(shape.viewDistance, &sculk::protocol::BinaryStream::writeFloat);
        stream.writeOptional(color, &sculk::protocol::BinaryStream::writeSignedInt);
        stream.writeOptional(shape.dimensionId, &sculk::protocol::BinaryStream::writeVarInt);
        stream.writeOptional(attachedToId, &sculk::protocol::BinaryStream::writeUnsignedInt64);
        stream.writeUnsignedVarInt(2);
        stream.writeString(shape.text);
        stream.writeBool(false);//UseRotation(#88)
        stream.writeOptional(backgroundColor, &sculk::protocol::BinaryStream::writeSignedInt);
        stream.writeFloat(0.0);
        stream.writeBool(true);//DepthTest
        stream.writeBool(true);//ShowBackface
        stream.writeBool(true);//ShowTextBackface
    }
};

} // namespace phantom::net

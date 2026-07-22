#pragma once

#include "infodrone/IDataSource.hpp"
#include <memory>
#include <expected>
#include <string>
#include <vector>
#include <cstdint>

namespace infodrone {

enum class ParseError {
    PacketTooShort,
    InvalidRadiotap,
    InvalidMacHeader,
    BeaconHeaderTruncated,
    MalformedInformationElement
};

class InfoDroneParser {
public:
    explicit InfoDroneParser(std::unique_ptr<IDataSource> dataSource);

    // Lance le traitement du fichier
    bool processFile(const std::string& filepath);

private:
    // Fonction de décodage du paquet brut
    std::expected<void, ParseError> decodePacket(std::span<const uint8_t> packetData);

    std::unique_ptr<IDataSource> dataSource_;
};

} // namespace infodrone
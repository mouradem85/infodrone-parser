#include "infodrone/InfoDroneParser.hpp"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <arpa/inet.h>
#include <cctype>

namespace infodrone {

#pragma pack(push, 1)
struct IEEE80211Header {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
    uint16_t seq_ctrl;
};
#pragma pack(pop)

InfoDroneParser::InfoDroneParser(std::unique_ptr<IDataSource> dataSource)
    : dataSource_(std::move(dataSource)) {}

std::string InfoDroneParser::parseErrorToString(ParseError error) {
    switch (error) {
        case ParseError::PacketTooShort:
            return "Paquet trop court";
        case ParseError::InvalidRadiotap:
            return "En-tête Radiotap invalide ou corrompu";
        case ParseError::InvalidMacHeader:
            return "En-tête MAC 802.11 invalide ou non supporté";
        case ParseError::BeaconHeaderTruncated:
            return "En-tête de balise tronqué";
        case ParseError::MalformedInformationElement:
            return "Information Element (IE) malformé";
        default:
            return "Erreur inconnue";
    }
}

bool InfoDroneParser::processFile(const std::string& filepath) {
    if (!dataSource_->open(filepath)) {
        std::cerr << "[InfoDroneParser] Échec de l'ouverture de la source pour : " << filepath << "\n";
        return false;
    }
    int packetCount = 0;
    int errorCount = 0;
    std::optional<std::span<const uint8_t>> packetBuffer;

    while ((packetBuffer = dataSource_->getNextPacket())) {
        ++packetCount;
        
        auto result = decodePacket(packetBuffer.value());
        if (!result) {
            ++errorCount;
            std::cerr << "[Erreur] Paquet #" << packetCount 
                      << " -> " << parseErrorToString(result.error()) << "\n";
        }
    }

    std::cout << "[InfoDroneParser] Traitement terminé. Total paquets : " << packetCount 
              << " | Erreurs : " << errorCount << "\n";
    dataSource_->close();
    return true;
}

std::expected<RadiotapInfo, ParseError> InfoDroneParser::extractRadiotap(std::span<const uint8_t> packetData) const {
    if (packetData.size() < 8) {
        return std::unexpected(ParseError::PacketTooShort);
    }

    if (packetData[0] != 0) {
        return std::unexpected(ParseError::InvalidRadiotap);
    }

    uint16_t radiotap_len = 0;
    std::memcpy(&radiotap_len, packetData.data() + 2, sizeof(uint16_t));
    radiotap_len = le16toh(radiotap_len);

    if (radiotap_len < 8 || radiotap_len > packetData.size() || packetData.size() < radiotap_len + sizeof(IEEE80211Header)) {
        return std::unexpected(ParseError::InvalidRadiotap);
    }

    int8_t rssi_dbm = 0;
    bool rssiFound = false;
    for (size_t i = 8; i < radiotap_len && i < packetData.size(); ++i) {
        int8_t val = static_cast<int8_t>(packetData[i]);
        if (val >= -100 && val <= -10) {
            rssi_dbm = val;
            rssiFound = true;
            break;
        }
    }

    return RadiotapInfo{radiotap_len, rssi_dbm, rssiFound};
}

std::expected<void, ParseError> InfoDroneParser::parseInformationElements(std::span<const uint8_t> packetData, size_t& offset) {
    while (offset + 2 <= packetData.size()) {
        uint8_t tagId = packetData[offset];
        uint8_t tagLen = packetData[offset + 1];
        offset += 2;

        if (offset + tagLen > packetData.size()) {
            return std::unexpected(ParseError::MalformedInformationElement);
        }

        std::span<const uint8_t> tagData = packetData.subspan(offset, tagLen);

        if (tagId == 0 && tagLen > 0) {
            std::string ssid(tagData.begin(), tagData.end());
            bool isValid = true;
            for (char c : ssid) {
                if (c < 32 || c > 126) {
                    isValid = false;
                    break;
                }
            }
            if (isValid && !ssid.empty()) {
                std::cout << "  -> [IE] SSID     : " << ssid << "\n";
            }
        } 
        else if (tagId == 3 && tagLen == 1) {
            std::cout << "  -> [IE] Canal    : " << static_cast<int>(tagData[0]) << "\n";
        } 
        else if (tagId == 221) {
            parseVendorSpecificTag(tagData);
        }

        offset += tagLen;
    }
    
    return {};
}

void InfoDroneParser::parseVendorSpecificTag(std::span<const uint8_t> tagData) {
    uint8_t tagLen = static_cast<uint8_t>(tagData.size());
    if (tagLen < 3) return;

    size_t offset = 0;

    uint8_t o1 = tagData[offset++];
    uint8_t o2 = tagData[offset++];
    uint8_t o3 = tagData[offset++];

    bool isValidDGAC = (o1 == 0x6A && o2 == 0x5C && o3 == 0x35);

    std::cout << "      -> CID / OUI  : " 
              << std::hex << std::setfill('0') 
              << std::setw(2) << (int)o1 << ":" 
              << std::setw(2) << (int)o2 << ":" 
              << std::setw(2) << (int)o3 << std::dec 
              << (isValidDGAC ? " (Conforme Signalement DGAC)" : " (Autre / Non-conforme)") << "\n";

    if (!isValidDGAC) {
        return;
    }

    if (tagLen == 24) {
        uint8_t type = tagData[offset++];
        uint8_t flags1 = tagData[offset++];
        uint8_t flags2 = tagData[offset++];

        std::cout << "      -> Type       : " << static_cast<int>(type) << "\n";
        std::cout << "      -> Flags      : " << static_cast<int>(flags1) << ", " << static_cast<int>(flags2) << "\n";

        if (offset + sizeof(int16_t) <= tagLen) {
            int16_t alt = 0;
            std::memcpy(&alt, &tagData[offset], sizeof(int16_t));
            alt = static_cast<int16_t>(le16toh(alt));
            std::cout << "      -> Altitude   : " << alt << " m\n";
            offset += sizeof(int16_t);
        }

        if (offset + sizeof(int32_t) <= tagLen) {
            int32_t val1 = 0;
            std::memcpy(&val1, &tagData[offset], sizeof(int32_t));
            val1 = static_cast<int32_t>(le32toh(val1));
            std::cout << "      -> Latitude courante : " << static_cast<double>(val1) / 100000.0 << "°\n";
            offset += sizeof(int32_t);
        }

        if (offset + sizeof(int32_t) <= tagLen) {
            int32_t val2 = 0;
            std::memcpy(&val2, &tagData[offset], sizeof(int32_t));
            val2 = static_cast<int32_t>(le32toh(val2));
            std::cout << "      -> Longitude courante : " << static_cast<double>(val2) / 100000.0 << "°\n";
            offset += sizeof(int32_t);
        }

        if (offset + 4 <= tagLen) {
            uint32_t tail = 0;
            std::memcpy(&tail, &tagData[offset], sizeof(uint32_t));
            tail = static_cast<uint32_t>(le32toh(tail));
            std::cout << "      -> Tail       : 0x" << std::hex << tail << std::dec << "\n";
        }
    }
    else if (tagLen == 7) {
        if (offset + 3 <= tagLen) {
            std::cout << "      -> Sub-Type   : " << static_cast<int>(tagData[offset++]) << "\n";
            std::cout << "      -> Statut 1   : " << static_cast<int>(tagData[offset++]) << "\n";
            std::cout << "      -> Statut 2   : " << static_cast<int>(tagData[offset++]) << "\n";
        }
    }
    else {
        uint8_t vsType = tagData[offset++];
        std::cout << "      -> VS Type    : 0x" << std::hex << static_cast<int>(vsType) << std::dec << "\n";

        while (offset + 2 <= tagData.size()) {
            uint8_t type = tagData[offset++];
            uint8_t length = tagData[offset++];

            if (offset + length > tagData.size()) break;

            std::span<const uint8_t> valueSpan = tagData.subspan(offset, length);
            offset += length;

            switch (type) {
                case 0x01:
                    if (length >= 1) std::cout << "      -> Version    : " << static_cast<int>(valueSpan[0]) << "\n";
                    break;
                case 0x02:
                case 0x03: {
                    std::string droneId(valueSpan.begin(), valueSpan.end());
                    std::cout << "      -> Drone ID   : " << droneId << "\n";
                    break;
                }
                case 0x04:
                case 0x05:
                case 0x08:
                case 0x09: {
                    if (length == 4) {
                        int32_t raw_coord = 0;
                        std::memcpy(&raw_coord, valueSpan.data(), sizeof(int32_t));
                        raw_coord = static_cast<int32_t>(le32toh(raw_coord));
                        double coord = static_cast<double>(raw_coord) / 100000.0;
                        std::string label = (type == 0x04) ? "Latitude courante" : (type == 0x05) ? "Longitude courante" : (type == 0x08) ? "Latitude décollage" : "Longitude décollage";
                        std::cout << "      -> " << label << " : " << coord << "°\n";
                    }
                    break;
                }
                case 0x06:
                case 0x07: {
                    if (length == 2) {
                        int16_t raw_val = 0;
                        std::memcpy(&raw_val, valueSpan.data(), sizeof(int16_t));
                        raw_val = static_cast<int16_t>(le16toh(raw_val));
                        std::string label = (type == 0x06) ? "Altitude" : "Hauteur";
                        std::cout << "      -> " << label << " : " << raw_val << " m\n";
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}

std::expected<void, ParseError> InfoDroneParser::decodePacket(std::span<const uint8_t> packetData) {
    auto radiotapResult = extractRadiotap(packetData);
    if (!radiotapResult) {
        return std::unexpected(radiotapResult.error());
    }

    const auto& [radiotap_len, rssi_dbm, rssiFound] = *radiotapResult;
    size_t offset = radiotap_len;

    if (offset + sizeof(IEEE80211Header) > packetData.size()) {
        return std::unexpected(ParseError::InvalidMacHeader);
    }

    const auto* macHeader = reinterpret_cast<const IEEE80211Header*>(packetData.data() + offset);
    offset += sizeof(IEEE80211Header);

    uint8_t frame_ctrl_type = macHeader->frame_control & 0x00FC;
    if (frame_ctrl_type != 0x0080 && frame_ctrl_type != 0x0050) {
        return std::unexpected(ParseError::InvalidMacHeader);
    }

    auto macToString = [](const uint8_t* addr) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        return std::string(buf);
    };

    std::cout << "--- TRAME 802.11 / PARROT ANAFI ---\n";
    std::cout << "  [Radiotap] Longueur : " << radiotap_len << " octets | RSSI : " 
              << (rssiFound ? std::to_string(rssi_dbm) + " dBm" : "N/A") << "\n";
    std::cout << "  [802.11]   Type     : 0x" << std::hex << macHeader->frame_control << std::dec << "\n";
    std::cout << "  [802.11]   Source   : " << macToString(macHeader->addr2) << "\n";
    std::cout << "  [802.11]   BSSID    : " << macToString(macHeader->addr3) << "\n";

    if (offset + 12 > packetData.size()) {
        return std::unexpected(ParseError::BeaconHeaderTruncated);
    }
    offset += 12; 

    if (auto result = parseInformationElements(packetData, offset); !result) {
        return std::unexpected(result.error());
    }
    
    std::cout << "====================================\n\n";
    return {};
}

} // namespace infodrone
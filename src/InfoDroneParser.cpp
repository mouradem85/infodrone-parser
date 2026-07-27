#include "infodrone/InfoDroneParser.hpp"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <span>
#include <cmath>
#include <vector>
#include <arpa/inet.h>
#include <cctype>
#include <endian.h>

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
        std::cerr << "[InfoDroneParser] Échec de l'ouverture de la source : " << filepath << "\n";
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
            std::cerr << "[Avertissement] Paquet #" << packetCount 
                      << " -> " << parseErrorToString(result.error()) << "\n";
        }
    }

    std::cout << "\n=========================================\n";
    std::cout << "           RAPPORT DE TRAITEMENT         \n";
    std::cout << "=========================================\n";
    std::cout << " Total de paquets analysés : " << packetCount << "\n";
    std::cout << " Paquets valides           : " << (packetCount - errorCount) << "\n";
    std::cout << " Paquets en erreur         : " << errorCount << "\n";
    std::cout << "=========================================\n";

    dataSource_->close();
    return true;
}

std::expected<RadiotapInfo, ParseError> InfoDroneParser::extractRadiotap(std::span<const uint8_t> packetData) const {
    if (packetData.size() < 8 || packetData[0] != 0) {
        return std::unexpected(ParseError::InvalidRadiotap);
    }

    uint16_t radiotap_len = 0;
    std::memcpy(&radiotap_len, packetData.data() + 2, sizeof(uint16_t));
    radiotap_len = le16toh(radiotap_len);

    if (radiotap_len < 8 || radiotap_len > packetData.size()) {
        return std::unexpected(ParseError::InvalidRadiotap);
    }

    int8_t rssi_dbm = 0;
    bool rssiFound = false;

    size_t offset = 4;
    
    std::vector<uint32_t> present_words;
    uint32_t present_flags = 0;
    
    do {
        if (offset + 4 > radiotap_len) return std::unexpected(ParseError::InvalidRadiotap);
        std::memcpy(&present_flags, packetData.data() + offset, sizeof(uint32_t));
        present_flags = le32toh(present_flags);
        present_words.push_back(present_flags);
        offset += 4;
    } while ((present_flags & 0x80000000) && (offset < radiotap_len));

    auto alignOffset = [](size_t current, size_t align) {
        return (current + (align - 1)) & ~(align - 1);
    };

    if (!present_words.empty() && (present_words[0] & (1 << 0))) {
        offset = alignOffset(offset, 8) + 8; // TSFT
    }
    if (!present_words.empty() && (present_words[0] & (1 << 1))) {
        offset = alignOffset(offset, 1) + 1; // Flags
    }
    if (!present_words.empty() && (present_words[0] & (1 << 2))) {
        offset = alignOffset(offset, 1) + 1; // Rate
    }
    if (!present_words.empty() && (present_words[0] & (1 << 3))) {
        offset = alignOffset(offset, 2) + 4; // Channel
    }
    if (!present_words.empty() && (present_words[0] & (1 << 4))) {
        offset = alignOffset(offset, 1) + 2; // FHSS
    }

    bool has_antenna_signal = false;
    if (!present_words.empty() && (present_words[0] & (1 << 5))) {
        has_antenna_signal = true;
    } else if (present_words.size() > 1 && (present_words[1] & (1 << 5))) {
        has_antenna_signal = true;
    }

    if (has_antenna_signal) {
        offset = alignOffset(offset, 1);
        if (offset < radiotap_len) {
            rssi_dbm = static_cast<int8_t>(packetData[offset]);
            rssiFound = true;
        }
    }

    return RadiotapInfo{radiotap_len, rssi_dbm, rssiFound};
}

std::expected<void, ParseError> InfoDroneParser::parseInformationElements(std::span<const uint8_t> packetData, size_t& offset) {
    while (offset + 2 <= packetData.size()) {
        uint8_t tagId = packetData[offset];
        uint8_t tagLen = packetData[offset + 1];

        if (offset + 2 + tagLen > packetData.size()) {
            return std::unexpected(ParseError::MalformedInformationElement);
        }

        std::span<const uint8_t> tagData = packetData.subspan(offset + 2, tagLen);

        if (tagId == 0 && tagLen > 0) { // SSID
            std::string ssid(tagData.begin(), tagData.end());
            bool isValid = true;
            for (char c : ssid) {
                if (c < 32 || c > 126) {
                    isValid = false;
                    break;
                }
            }
            if (isValid && !ssid.empty()) {
                std::cout << "  -> [IE] SSID               : " << ssid << "\n";
            }
        } 
        else if (tagId == 3 && tagLen == 1) { // Canal
            std::cout << "  -> [IE] Canal              : " << static_cast<int>(tagData[0]) << "\n";
        } 
        else if (tagId == 221) { // Vendor Specific
            parseVendorSpecificTag(tagData);
        }

        offset += 2 + tagLen;
    }
    
    return {};
}

void InfoDroneParser::parseVendorSpecificTag(std::span<const uint8_t> tagData) {
    if (tagData.size() < 3) {
        return;
    }

    size_t offset = 0;
    uint8_t o1 = tagData[offset++];
    uint8_t o2 = tagData[offset++];
    uint8_t o3 = tagData[offset++];

    bool isParrot = (o1 == 0x90 && o2 == 0x03 && o3 == 0xB7);
    bool isDGAC   = (o1 == 0x6A && o2 == 0x5C && o3 == 0x35);

    if (!isParrot && !isDGAC) {
        return;
    }

    std::cout << "  -> [IE Vendor] OUI        : " 
              << std::hex << std::setfill('0') 
              << std::setw(2) << static_cast<int>(o1) << ":" 
              << std::setw(2) << static_cast<int>(o2) << ":" 
              << std::setw(2) << static_cast<int>(o3) << std::dec;

    if (isParrot) {
        std::cout << " (Parrot SA - InfoDrone)\n";
    } else if (isDGAC) {
        std::cout << " (DGAC Signalement Direct)\n";
    }

    if (offset >= tagData.size()) return;

    uint8_t vsType = tagData[offset++];
    std::cout << "      -> Subtype / VS Type   : 0x" << std::hex << static_cast<int>(vsType) << std::dec << "\n";

    if (isParrot && vsType == 0x09) {
        if (offset >= tagData.size()) return;

        uint8_t payloadLen = tagData[offset++];
        std::cout << "      -> Payload Length      : " << static_cast<int>(payloadLen) << " octets\n";

        if (offset + payloadLen > tagData.size()) {
            payloadLen = static_cast<uint8_t>(tagData.size() - offset);
        }

        std::span<const uint8_t> payload = tagData.subspan(offset, payloadLen);

        size_t pOffset = 0;
        
        // 1. Lecture de l'identifiant complet (18 octets)
        if (payload.size() >= 18) {
            std::string droneId(reinterpret_cast<const char*>(payload.data()), 18);
            std::cout << "      -> Drone ID / Serial   : " << droneId << "\n";
            pOffset = 18; 
        } else {
            std::cout << "      -> [Avertissement] Payload trop court pour l'ID (nécessite 18 octets)\n";
            return;
        }

        // 2. Vérification s'il reste assez d'octets pour les flags et le GPS
        if (pOffset + 11 > payload.size()) {
            std::cout << "      -> [Avertissement] Trame partielle (ID présent, mais données GPS manquantes)\n";
            return;
        }

        // 3. Lecture du statut / flags (1 octet)
        uint8_t flagsOrStatus = payload[pOffset++];
        std::cout << "      -> Flags / Status      : 0x" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(flagsOrStatus) << std::dec << "\n";
        
        bool emergency = (flagsOrStatus & 0x01);
        bool airborne  = (flagsOrStatus & 0x10);
        bool gps_ok    = (flagsOrStatus & 0x40);
        
        std::cout << "         └─ Vol en cours     : " << (airborne ? "Oui" : "Non") << "\n";
        std::cout << "         └─ Fix GPS valide   : " << (gps_ok ? "Oui" : "Non") << "\n";
        std::cout << "         └─ Urgence/SOS      : " << (emergency ? "Oui" : "Non") << "\n";

        // 4. Lecture Latitude (4 octets)
        uint32_t raw_lat_u = 0;
        std::memcpy(&raw_lat_u, &payload[pOffset], 4);
        raw_lat_u = le32toh(raw_lat_u);
        int32_t raw_lat = static_cast<int32_t>(raw_lat_u);
        double lat = static_cast<double>(raw_lat) * 1e-7;
        std::cout << "      -> Latitude            : " << std::fixed << std::setprecision(6) << lat << "°\n";
        pOffset += 4;

        // 5. Lecture Longitude (4 octets)
        uint32_t raw_lon_u = 0;
        std::memcpy(&raw_lon_u, &payload[pOffset], 4);
        raw_lon_u = le32toh(raw_lon_u);
        int32_t raw_lon = static_cast<int32_t>(raw_lon_u);
        double lon = static_cast<double>(raw_lon) * 1e-7;
        std::cout << "      -> Longitude           : " << std::fixed << std::setprecision(6) << lon << "°\n";
        pOffset += 4;

        // 6. Lecture Altitude (2 octets)
        uint16_t raw_alt = 0;
        std::memcpy(&raw_alt, &payload[pOffset], 2);
        raw_alt = le16toh(raw_alt);
        int16_t alt = static_cast<int16_t>(raw_alt);
        std::cout << "      -> Altitude (MSL)      : " << alt << " m\n";
    }
}

std::expected<void, ParseError> InfoDroneParser::decodePacket(std::span<const uint8_t> packetData) {
    if (packetData.size() < 8) {
        return std::unexpected(ParseError::PacketTooShort);
    }
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

    uint16_t fc = le16toh(macHeader->frame_control);
    uint8_t type = (fc >> 2) & 0x03;
    uint8_t subtype = (fc >> 4) & 0x0F;

    if (type != 0 || subtype != 8) { 
        return std::unexpected(ParseError::InvalidMacHeader);
    }

    auto macToString = [](const uint8_t* addr) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        return std::string(buf);
    };

    std::cout << "=== TRAME 802.11 / PARROT ANAFI ===\n";
    std::cout << "  [Radiotap] Longueur        : " << radiotap_len << " octets | RSSI : " 
              << (rssiFound ? std::to_string(rssi_dbm) + " dBm" : "Inconnu") << "\n";
    std::cout << "  [802.11]   Type            : Management / Beacon\n";
    std::cout << "  [802.11]   Source / BSSID  : " << macToString(macHeader->addr2) << "\n";

    if (offset + 12 > packetData.size()) {
        return std::unexpected(ParseError::BeaconHeaderTruncated);
    }
    offset += 12; 

    if (auto result = parseInformationElements(packetData, offset); !result) {
        return std::unexpected(result.error());
    }
    
    std::cout << "===================================\n\n";
    return {};
}

} // namespace infodrone
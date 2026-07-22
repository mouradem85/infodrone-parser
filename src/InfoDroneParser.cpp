#include "infodrone/InfoDroneParser.hpp"
#include <iostream>
#include <cstring>
#include <iomanip>

namespace infodrone {
    #pragma pack(push, 1)
    struct IEEE80211Header {
        uint16_t frame_control;
        uint16_t duration;
        uint8_t  addr1[6]; // Destination
        uint8_t  addr2[6]; // Source (Drone)
        uint8_t  addr3[6]; // BSSID
        uint16_t seq_ctrl;
    };
    #pragma pack(pop)
    
InfoDroneParser::InfoDroneParser(std::unique_ptr<IDataSource> dataSource)
    : dataSource_(std::move(dataSource)) {}

bool InfoDroneParser::processFile(const std::string& filepath) {
    if (!dataSource_->open(filepath)) {
        std::cerr << "[InfoDroneParser] Échec de l'ouverture de la source pour : " << filepath << "\n";
        return false;
    }
    int packetCount = 0;
    std::optional<std::span<const uint8_t>> packetBuffer;

    while ((packetBuffer = dataSource_->getNextPacket())) {
        decodePacket(packetBuffer.value());
        ++packetCount;
    }

    std::cout << "[InfoDroneParser] Traitement terminé. Total paquets analysés : " << packetCount << "\n";
    dataSource_->close();
    return true;
}

std::expected<void, ParseError> InfoDroneParser::decodePacket(std::span<const uint8_t> packetData) {
    if (packetData.size() < 4) {
        return std::unexpected(ParseError::PacketTooShort);
    }

    // --- 1. Extraction et validation du Radiotap ---
    uint16_t radiotap_len = *reinterpret_cast<const uint16_t*>(packetData.data() + 2);
    if (radiotap_len > 256 || packetData.size() < radiotap_len + sizeof(IEEE80211Header)) {
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

    size_t offset = radiotap_len;

    // --- 2. Décodage et filtrage strict de l'en-tête MAC 802.11 ---
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

    // --- 3. Champs de gestion fixes (Beacon / Probe Response) ---
    if (offset + 12 > packetData.size()) {
        return std::unexpected(ParseError::BeaconHeaderTruncated);
    }
    offset += 12; 

    // --- 4. Analyse propre des Information Elements (IEs) ---
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
                std::cout << "  -> [IE] SSID      : " << ssid << "\n";
            }
        } 
        else if (tagId == 3 && tagLen == 1) {
            std::cout << "  -> [IE] Canal     : " << static_cast<int>(tagData[0]) << "\n";
        } 
        else if (tagId == 221) {
            std::cout << "  -> [IE] Parrot IE : Vendor Specific (Taille: " << static_cast<int>(tagLen) << ")\n";
        }

        offset += tagLen;
    }
    
    std::cout << "------------------------------------\n";
    return {};
}

} // namespace infodrone
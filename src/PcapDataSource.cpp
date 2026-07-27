#include "infodrone/PcapDataSource.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

namespace infodrone {

bool PcapDataSource::open(const std::string& filepath) {
    fileStream_.open(filepath, std::ios::binary);
    return fileStream_.is_open();
}

std::optional<std::span<const uint8_t>> PcapDataSource::getNextPacket() {
    if (!fileStream_.is_open() || fileStream_.eof()) {
        return std::nullopt;
    }

    // Boucle de lecture des blocs PCAPNG
    while (fileStream_) {
        // Mémoriser la position absolue exacte du début du bloc avant de lire
        std::streampos blockStartPos = fileStream_.tellg();

        uint32_t blockType = 0;
        uint32_t blockTotalLength = 0;

        // Lire le type de bloc (4 octets)
        if (!fileStream_.read(reinterpret_cast<char*>(&blockType), sizeof(blockType))) {
            break;
        }
        // Lire la longueur totale du bloc (4 octets)
        if (!fileStream_.read(reinterpret_cast<char*>(&blockTotalLength), sizeof(blockTotalLength))) {
            break;
        }

        // Sécurité contre les blocs corrompus ou tailles invalides
        if (blockTotalLength < 12 || blockTotalLength > 65536) {
            break;
        }

        // Calcul de la taille du corps du bloc à lire
        size_t headerSize = 8; // Block Type (4) + Block Total Length (4)
        size_t footerSize = 4; // Block Total Length répété à la fin
        size_t bodySize = blockTotalLength - headerSize - footerSize;

        if (blockType == 0x00000006) { // Enhanced Packet Block (EPB)
            // Lire le corps complet du bloc EPB dans un buffer temporaire
            std::vector<uint8_t> blockBody(bodySize);
            if (!fileStream_.read(reinterpret_cast<char*>(blockBody.data()), bodySize)) {
                break;
            }

            // Lire le footer du bloc (Block Total Length répété)
            uint32_t blockFooter = 0;
            if (!fileStream_.read(reinterpret_cast<char*>(&blockFooter), sizeof(blockFooter))) {
                break;
            }

            // Positionnement absolu garanti pour s'assurer de pointer exactement à la fin du bloc EPB
            fileStream_.seekg(blockStartPos + static_cast<std::streampos>(blockTotalLength), std::ios::beg);

            if (bodySize >= 20) {
                uint32_t capturedLen = 0;
                memcpy(&capturedLen, blockBody.data() + 12, sizeof(capturedLen));

                size_t packetDataOffset = 20; // 4 (Interface ID) + 8 (Timestamp) + 4 (Captured Len)
                if (packetDataOffset + capturedLen <= blockBody.size()) {
                    // Stocker le paquet dans le buffer interne de la classe
                    internalBuffer_.assign(
                        blockBody.begin() + packetDataOffset,
                        blockBody.begin() + packetDataOffset + capturedLen
                    );

                    return std::span<const uint8_t>(internalBuffer_.data(), internalBuffer_.size());
                }
            }
        } else {
            // Pour tous les autres blocs, saut ABSOLU propre basé sur le début du bloc + blockTotalLength
            // Cela évite tout décalage d'octets avec le seekg relatif (std::ios::cur)
            fileStream_.seekg(blockStartPos + static_cast<std::streampos>(blockTotalLength), std::ios::beg);
        }
    }

    return std::nullopt;
}

void PcapDataSource::close() {
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
}

} // namespace infodrone
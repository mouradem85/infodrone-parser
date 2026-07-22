#include "infodrone/PcapDataSource.hpp"
#include <cstring>
#include <iostream>

namespace infodrone {

bool PcapDataSource::open(const std::string& filepath) {
    fileStream_.open(filepath, std::ios::binary);
    if (!fileStream_.is_open()) {
        return false;
    }

    // Sauter l'en-tête global PCAP de 24 octets
    fileStream_.seekg(24, std::ios::beg);
    return true;
}

std::optional<std::span<const uint8_t>> PcapDataSource::getNextPacket() {
    if (!fileStream_.is_open() || fileStream_.eof()) {
        return std::nullopt;
    }
    
    uint32_t packetSize = 0;
    if (!fileStream_.read(reinterpret_cast<char*>(&packetSize), sizeof(packetSize))) {
        return std::nullopt;
    }

    // On force une taille raisonnable pour le test (ex: 512 octets max par paquet) 
    // si la valeur lue est un en-tête global ou une valeur de métadonnée.
    size_t bytesToRead = (packetSize > 65535 || packetSize == 0) ? 256 : packetSize;

    internalBuffer_.resize(bytesToRead);
    
    if (!fileStream_.read(reinterpret_cast<char*>(internalBuffer_.data()), bytesToRead)) {
        return std::nullopt;
    }

    return std::span<const uint8_t>(internalBuffer_.data(), internalBuffer_.size());
}

void PcapDataSource::close() {
    if (fileStream_.is_open()) {
        fileStream_.close();
    }
}

} // namespace infodrone
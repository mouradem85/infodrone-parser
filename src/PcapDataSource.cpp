#include "infodrone/PcapDataSource.hpp"

namespace infodrone {

bool PcapDataSource::open(const std::string& ) {
    // Logique d'ouverture du fichier pcap/pcapng
    return false;
}

bool PcapDataSource::getNextPacket(std::vector<uint8_t>& , int16_t& ) {
    // Logique de lecture du paquet suivant
    return false;
}

void PcapDataSource::close() {
    // Logique de fermeture
}

} // namespace infodrone
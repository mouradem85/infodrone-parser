#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include "infodrone/PcapDataSource.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <fichier.pcap>" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    infodrone::PcapDataSource dataSource;

    if (!dataSource.open(filepath)) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier : " << filepath << std::endl;
        return 1;
    }

    std::cout << "Fichier ouvert avec succes : " << filepath << std::endl;

    std::vector<uint8_t> packetBuffer;
    int16_t rssi = 0;
    size_t packetCount = 0;

    while (dataSource.getNextPacket(packetBuffer, rssi)) {
        packetCount++;
        // Traitement des paquets à venir...
    }

    std::cout << "Nombre total de paquets lus : " << packetCount << std::endl;

    dataSource.close();
    return 0;
}
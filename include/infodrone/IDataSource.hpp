#pragma once

#include <vector>
#include <cstdint>
#include <string>

class IDataSource {
public:
    virtual ~IDataSource() = default;

    // Ouvre la source (ex: fichier pcapng)
    virtual bool open(const std::string& path) = 0;

    // Récupère le paquet suivant sous forme de vecteur d'octets avec son timestamp et son RSSI
    // Retourne true s'il y a un paquet, false si la fin est atteinte
    virtual bool getNextPacket(std::vector<uint8_t>& packetBuffer, int16_t& rssi) = 0;

    // Ferme la source
    virtual void close() = 0;
};
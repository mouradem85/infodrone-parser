#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <optional>
#include <span>

class IDataSource {
public:
    virtual ~IDataSource() = default;

    // Ouvre la source (ex: fichier pcapng)
    virtual bool open(const std::string& path) = 0;

    // La méthode retourne une vue ou un booléen selon ton design
    virtual std::optional<std::span<const uint8_t>> getNextPacket() = 0;

    // Ferme la source
    virtual void close() = 0;
};
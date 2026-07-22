#pragma once

#include <string>
#include <vector>
#include <span>
#include <optional>
#include <memory>
#include <fstream> // Ajout indispensable pour std::ifstream
#include "infodrone/IDataSource.hpp"

namespace infodrone {

    class PcapDataSource : public IDataSource {
    public:
        PcapDataSource() = default;
        ~PcapDataSource() = default;

        bool open(const std::string& filepath) override;
        std::optional<std::span<const uint8_t>> getNextPacket() override;
        void close() override;

    private:
        std::ifstream fileStream_;
        std::vector<uint8_t> internalBuffer_;
    };

} // namespace infodrone
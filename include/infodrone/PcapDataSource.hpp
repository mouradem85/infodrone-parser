#pragma once

#include "IDataSource.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace infodrone {

class PcapDataSource : public IDataSource {
public:
    bool open(const std::string& path) override;
    bool getNextPacket(std::vector<uint8_t>& packetBuffer, int16_t& rssi) override;
    void close() override;
};

} // namespace infodrone
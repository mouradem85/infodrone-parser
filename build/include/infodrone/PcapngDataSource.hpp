#pragma once
#include "IDataSource.hpp"
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

namespace infodrone {
class PcapngDataSource : public IDataSource {
private:
    std::ifstream fileStream;
public:
    ~PcapngDataSource() override;
    bool open(const std::string& path) override;
    bool getNextPacket(std::vector<uint8_t>& packetBuffer, int16_t& rssi) override;
    void close() override;
};
}

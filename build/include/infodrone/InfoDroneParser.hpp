#pragma once
#include <vector>
#include <cstdint>

namespace infodrone {
struct InfoDroneData {
    bool isInfoDrone = false;
    uint32_t ssi = 0;
};
class InfoDroneParser {
public:
    static InfoDroneData parse(const std::vector<uint8_t>& packetBuffer);
};
}

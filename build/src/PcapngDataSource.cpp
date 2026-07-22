#include "infodrone/PcapngDataSource.hpp"

namespace infodrone {
PcapngDataSource::~PcapngDataSource() { close(); }
bool PcapngDataSource::open(const std::string& path) {
    fileStream.open(path, std::ios::binary);
    return fileStream.is_open();
}
bool PcapngDataSource::getNextPacket(std::vector<uint8_t>&, int16_t&) {
    return false;
}
void PcapngDataSource::close() {
    if (fileStream.is_open()) fileStream.close();
}
}

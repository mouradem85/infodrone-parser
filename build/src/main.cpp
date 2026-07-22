#include <iostream>
#include "infodrone/PcapngDataSource.hpp"
#include "infodrone/InfoDroneParser.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <fichier.pcapng>" << std::endl;
        return 1;
    }
    std::cout << "Infodrone Parser - Pret pour le fichier : " << argv[1] << std::endl;
    return 0;
}

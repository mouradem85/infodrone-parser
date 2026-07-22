#include "infodrone/PcapDataSource.hpp"
#include "infodrone/InfoDroneParser.hpp"
#include <memory>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <chemin_fichier_pcap_ou_donnees>\n";
        return 1;
    }

    std::string filepath = argv[1];
    

    // Instanciation de la source de données PCAP et injection dans le parseur
    auto dataSource = std::make_unique<infodrone::PcapDataSource>();
    infodrone::InfoDroneParser parser(std::move(dataSource));

    if (!parser.processFile(filepath)) {
        std::cerr << "[Erreur] Échec lors du traitement du fichier : " << filepath << "\n";
        return 1;
    }

    std::cout << "[Succès] Traitement du fichier terminé avec succès.\n";
    return 0;
}
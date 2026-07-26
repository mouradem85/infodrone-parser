/*******************************************************************************
 * @file PcapDataSource.hpp
 * @brief Définition de la classe PcapDataSource pour la lecture de fichiers PCAP.
 * @namespace infodrone
 ******************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <span>
#include <optional>
#include <memory>
#include <fstream>
#include "infodrone/IDataSource.hpp"

namespace infodrone {

/**
 * @class PcapDataSource
 * @brief Implémentation concrète de IDataSource pour l'analyse de fichiers de capture réseau (PCAP).
 */
class PcapDataSource : public IDataSource {
public:
    /**
     * @brief Construit une source de données PCAP vide.
     */
    PcapDataSource() = default;

    /**
     * @brief Détruit la source de données et ferme le flux associé.
     */
    ~PcapDataSource() = default;

    /**
     * @brief Ouvre un fichier PCAP en mode binaire.
     * @param filepath Chemin vers le fichier PCAP à ouvrir.
     * @return true si l'ouverture et l'initialisation réussissent, false sinon.
     */
    bool open(const std::string& filepath) override;

    /**
     * @brief Récupère le prochain paquet brut disponible dans le flux.
     * @return Une std::optional contenant un span vers les données du paquet, ou std::nullopt en fin de fichier.
     */
    std::optional<std::span<const uint8_t>> getNextPacket() override;

    /**
     * @brief Ferme proprement le flux du fichier ouvert.
     */
    void close() override;

private:
    std::ifstream fileStream_;      ///< Flux de lecture binaire du fichier PCAP
    std::vector<uint8_t> internalBuffer_; ///< Tampon interne pour stocker temporairement le paquet en cours
};

} // namespace infodrone
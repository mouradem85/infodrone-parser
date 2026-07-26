/*******************************************************************************
 * @file InfoDroneParser.hpp
 * @brief Définition de la classe InfoDroneParser pour le décodage de flux Wi-Fi.
 * @namespace infodrone
 ******************************************************************************/

#pragma once

#include "infodrone/IDataSource.hpp"
#include <memory>
#include <expected>
#include <string>
#include <vector>
#include <cstdint>
#include <span>

namespace infodrone {

/**
 * @enum ParseError
 * @brief Répertorie les différents types d'erreurs rencontrées lors du parsing.
 */
enum class ParseError {
    PacketTooShort,
    InvalidRadiotap,
    InvalidMacHeader,
    BeaconHeaderTruncated,
    MalformedInformationElement
};

/**
 * @enum RadiotapParseStatus
 * @brief Représente l'état du résultat lors de l'analyse du champ Radiotap.
 */
enum class RadiotapParseStatus {
    Success,
    PacketTooShort,
    InvalidRadiotap
};

/**
 * @struct RadiotapInfo
 * @brief Stocke les métadonnées extraites de l'en-tête Radiotap.
 */
struct RadiotapInfo {
    uint16_t length = 0;
    int8_t rssi = 0;
    bool rssiFound = false;
};

/**
 * @class InfoDroneParser
 * @brief Parseur principal pour l'analyse des paquets de drones (ex: Parrot Anafi).
 */
class InfoDroneParser {
public:
    /**
     * @brief Construit le parseur avec une source de données spécifique.
     * @param dataSource Pointeur intelligent vers la source de données (ex: fichier PCAP).
     */
    explicit InfoDroneParser(std::unique_ptr<IDataSource> dataSource);

    /**
     * @brief Lance le traitement complet du fichier source.
     * @param filepath Chemin vers le fichier à analyser.
     * @return true en cas de succès, false sinon.
     */
    bool processFile(const std::string& filepath);

private:
    /**
     * @brief Décode un paquet brut individuel transmis sous forme de span.
     * @param packetData Données brutes du paquet.
     * @return std::expected vide en cas de succès, ou une ParseError en cas d'échec.
     */
    std::expected<void, ParseError> decodePacket(std::span<const uint8_t> packetData);

    /**
     * @brief Parcourt et analyse les Information Elements (IEs) d'un paquet de gestion.
     * @param packetData Données brutes du paquet.
     * @param offset Pointeur de lecture positionné au début des IEs (mis à jour en interne).
     * @return std::expected vide en cas de succès, ou une ParseError.
     */
    std::expected<void, ParseError> parseInformationElements(std::span<const uint8_t> packetData, size_t& offset);

    /**
     * @brief Extrait et valide les informations de l'en-tête Radiotap.
     * @param packetData Données brutes du paquet.
     * @return Les métadonnées Radiotap extraites ou une ParseError.
     */
    std::expected<RadiotapInfo, ParseError> extractRadiotap(std::span<const uint8_t> packetData) const;

    void parseVendorSpecificTag(std::span<const uint8_t> tagData);

    std::string parseErrorToString(ParseError error);

    std::unique_ptr<IDataSource> dataSource_;
};

} // namespace infodrone
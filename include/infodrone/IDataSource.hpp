/*******************************************************************************
 * @file IDataSource.hpp
 * @brief Définition de l'interface abstraite IDataSource pour l'ingestion de flux.
 * @namespace infodrone (implicite ou héritée par les implémentations)
 ******************************************************************************/

#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <optional>
#include <span>

/**
 * @class IDataSource
 * @brief Interface polymorphe définissant le contrat pour toute source de paquets réseau (ex: PCAP, PCAPNG).
 */
class IDataSource {
public:
    /**
     * @brief Destructeur virtuel par défaut pour garantir un nettoyage correct des classes dérivées.
     */
    virtual ~IDataSource() = default;

    /**
     * @brief Ouvre la source de données (ex: un fichier de capture réseau).
     * @param path Chemin d'accès vers la ressource à ouvrir.
     * @return true si l'ouverture réussit, false sinon.
     */
    virtual bool open(const std::string& path) = 0;

    /**
     * @brief Récupère le prochain paquet disponible sous forme de vue sécurisée.
     * @return Une std::optional contenant un span vers les données brutes, ou std::nullopt en fin de flux.
     */
    virtual std::optional<std::span<const uint8_t>> getNextPacket() = 0;

    /**
     * @brief Ferme la source de données et libère les ressources associées.
     */
    virtual void close() = 0;
};
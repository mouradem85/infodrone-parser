#include <gtest/gtest.h>
#include "infodrone/InfoDroneParser.hpp"
#include <vector>
#include <cstdint>

template <typename Tag, typename Tag::type M>
struct Accessor {
    friend typename Tag::type get(Tag) { return M; }
};

struct DecodePacketTag {
    using type = std::expected<void, infodrone::ParseError> (infodrone::InfoDroneParser::*)(std::span<const uint8_t>);
    friend type get(DecodePacketTag);
};

template struct Accessor<DecodePacketTag, &infodrone::InfoDroneParser::decodePacket>;
// -----------------------------------------------------------------------------

class InfoDroneParserTest : public ::testing::Test {
protected:
    infodrone::InfoDroneParser parser{nullptr};
    
    std::expected<void, infodrone::ParseError> decode(std::span<const uint8_t> packet) {
        auto decodeMethod = get(DecodePacketTag{});
        return (parser.*decodeMethod)(packet);
    }
};

TEST_F(InfoDroneParserTest, PacketTooShort) {
    std::vector<uint8_t> packet = {0x01, 0x02};
    auto result = decode(packet);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), infodrone::ParseError::PacketTooShort);
}

TEST_F(InfoDroneParserTest, InvalidRadiotap) {
    std::vector<uint8_t> packet = {0x00, 0x00, 0x2C, 0x01};
    auto result = decode(packet);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), infodrone::ParseError::InvalidRadiotap);
}

TEST_F(InfoDroneParserTest, InvalidMacHeader) {
    // Cas A : Soit la taille globale est trop juste pour contenir l'en-tête MAC complet après le radiotap (ex: 8 + quelques octets < 32)
    // Cas B : Soit le frame_control a un type invalide avec une taille suffisante
    std::vector<uint8_t> packet = {
        0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, // Radiotap (8 octets)
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MAC header avec frame_control invalide (0x0010)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Total 40 octets (suffisant pour passer la vérif de taille, échoue sur le type)
    };
    auto result = decode(packet);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), infodrone::ParseError::InvalidMacHeader);
}

TEST_F(InfoDroneParserTest, BeaconHeaderTruncated) {
    // Radiotap (8) + MAC Header valide type Beacon (24) = 32 octets.
    // On ajoute moins de 12 octets de champs fixes (ex: seulement 4 octets au lieu de 12).
    std::vector<uint8_t> packet = {
        // Radiotap (8 octets)
        0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
        // MAC Header (24 octets) avec type Beacon (0x0080)
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Champs fixes tronqués (4 octets au lieu de 12)
        0x01, 0x02, 0x03, 0x04
    };
    auto result = decode(packet);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), infodrone::ParseError::BeaconHeaderTruncated);
}

TEST_F(InfoDroneParserTest, MalformedInformationElement) {
    std::vector<uint8_t> packet = {
        0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x32
    };
    auto result = decode(packet);
    
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), infodrone::ParseError::MalformedInformationElement);
}
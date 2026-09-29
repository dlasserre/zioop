#include <iostream>
#include <cstddef> // offsetof
#include <cstdint>

struct FileHeader {
    char magic[4];
    std::uint16_t version;
    std::uint16_t flags;
    std::uint32_t nodeCount;
    std::uint32_t nodeNameBytes;
    std::uint32_t recordCount;
    std::uint32_t speciesNameBytes;
    std::uint32_t groupCount;
    std::uint32_t groupNameBytes;
    std::uint32_t nodesOffset;
    std::uint32_t nodeNamesOffset;
    std::uint32_t recordsOffset;
    std::uint32_t speciesNamesOffset;
    std::uint32_t groupsOffset;
    std::uint32_t groupNamesOffset;
    std::uint32_t reserved[2];
};

int main() {
    // 1. Taille théorique des données utiles
    std::size_t dataSize = sizeof(FileHeader::magic)
                           + sizeof(FileHeader::version)
                           + sizeof(FileHeader::flags)
                           + sizeof(FileHeader::nodeCount)
                           + sizeof(FileHeader::nodeNameBytes)
                           + sizeof(FileHeader::recordCount)
                           + sizeof(FileHeader::speciesNameBytes)
                           + sizeof(FileHeader::groupCount)
                           + sizeof(FileHeader::groupNameBytes)
                           + sizeof(FileHeader::nodesOffset)
                           + sizeof(FileHeader::nodeNamesOffset)
                           + sizeof(FileHeader::recordsOffset)
                           + sizeof(FileHeader::speciesNamesOffset)
                           + sizeof(FileHeader::groupsOffset)
                           + sizeof(FileHeader::groupNamesOffset)
                           + sizeof(FileHeader::reserved);

    // 2. Calcul du padding global inséré par le compilateur
    std::size_t totalPadding = sizeof(FileHeader) - dataSize;

    std::cout << "Taille totale de la struct : " << sizeof(FileHeader) << " octets\n";
    std::cout << "Taille des données cumulées : " << dataSize << " octets\n";
    std::cout << "Padding total ajouté        : " << totalPadding << " octets\n";
}

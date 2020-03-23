#include "config.h"

#include "item_updater_mmc.hpp"

#include "activation_mmc.hpp"
#include "utils.hpp"
#include "version.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>
#include <tuple>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;
using sdbusplus::exception::SdBusError;

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::filesystem;

namespace openpower
{
namespace software
{
namespace updater
{

std::unique_ptr<Activation> ItemUpdaterMmc::createActivationObject(
    const std::string& path, const std::string& versionId,
    const std::string& extVersion,
    sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations
        activationStatus,
    AssociationList& assocs)
{
    return std::make_unique<ActivationMmc>(
        bus, path, *this, versionId, extVersion, activationStatus, assocs);
}

std::unique_ptr<Version> ItemUpdaterMmc::createVersionObject(
    const std::string& objPath, const std::string& versionId,
    const std::string& versionString,
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose
        versionPurpose,
    const std::string& filePath)
{
    auto version = std::make_unique<Version>(
        bus, objPath, *this, versionId, versionString, versionPurpose, filePath,
        std::bind(&ItemUpdaterMmc::erase, this, std::placeholders::_1));
    version->deleteObject = std::make_unique<Delete>(bus, objPath, *version);
    return version;
}

bool ItemUpdaterMmc::validateImage(const std::string& path)
{
    return true;
}

void ItemUpdaterMmc::processPNORImage()
{
    auto activationState = server::Activation::Activations::Active;

    constexpr auto MMC_PATH("/host-fw/");
    auto pnorTOC = fs::path(MMC_PATH) / PNOR_TOC_FILE;
    if (!fs::is_regular_file(pnorTOC))
    {
        log<level::ERR>("Failed to read pnorTOC.",
                        entry("FILENAME=%s", pnorTOC.c_str()));
        return;
    }
    auto keyValues =
        Version::getValue(pnorTOC, {{"version", ""}, {"extended_version", ""}});
    auto& version = keyValues.at("version");
    if (version.empty())
    {
        log<level::ERR>("Failed to read version from pnorTOC",
                        entry("FILENAME=%s", pnorTOC.c_str()));
        activationState = server::Activation::Activations::Invalid;
    }

    auto& extendedVersion = keyValues.at("extended_version");
    if (extendedVersion.empty())
    {
        log<level::ERR>("Failed to read extendedVersion from pnorTOC",
                        entry("FILENAME=%s", pnorTOC.c_str()));
        activationState = server::Activation::Activations::Invalid;
    }

    auto id = Version::getId(version);

    if (id.empty())
    {
        return;
    }

    auto purpose = server::Version::VersionPurpose::Host;
    auto path = fs::path(SOFTWARE_OBJPATH) / id;
    AssociationList associations = {};

    if (activationState == server::Activation::Activations::Active)
    {
        // Create an association to the host inventory item
        associations.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  HOST_INVENTORY_PATH));

        // Create an active association since this image is active
        createActiveAssociation(path);
    }

    // Create Activation instance for this version.
    activations.insert(std::make_pair(
        id,
        std::make_unique<ActivationMmc>(bus, path, *this, id, extendedVersion,
                                        activationState, associations)));

    // If Active, create RedundancyPriority instance for this version.
    if (activationState == server::Activation::Activations::Active)
    {
        // For now only one PNOR is supported with MMC layout
        activations.find(id)->second->redundancyPriority =
            std::make_unique<RedundancyPriority>(
                bus, path, *(activations.find(id)->second), 0);
    }

    // Create Version instance for this version.
    auto versionPtr = std::make_unique<Version>(
        bus, path, *this, id, version, purpose, "",
        std::bind(&ItemUpdaterMmc::erase, this, std::placeholders::_1));
    versionPtr->deleteObject = std::make_unique<Delete>(bus, path, *versionPtr);
    versions.insert(std::make_pair(id, std::move(versionPtr)));

    if (!id.empty())
    {
        updateFunctionalAssociation(id);
    }
}

void ItemUpdaterMmc::reset()
{
}

bool ItemUpdaterMmc::isVersionFunctional(const std::string& versionId)
{
    return versionId == functionalVersionId;
}

void ItemUpdaterMmc::freePriority(uint8_t value, const std::string& versionId)
{
}

void ItemUpdaterMmc::deleteAll()
{
}

bool ItemUpdaterMmc::freeSpace()
{
    // For now assume MMC layout only has 1 active PNOR,
    // so erase the active PNOR
    for (const auto& iter : activations)
    {
        if (iter.second.get()->activation() ==
            server::Activation::Activations::Active)
        {
            return erase(iter.second->versionId);
        }
    }
    // No active PNOR means PNOR is empty or corrupted
    return true;
}

void ItemUpdaterMmc::updateFunctionalAssociation(const std::string& versionId)
{
    functionalVersionId = versionId;
    ItemUpdater::updateFunctionalAssociation(versionId);
}

void ItemUpdaterMmc::createHostFwPartition()
{
}

void GardResetMmc::reset()
{
}

} // namespace updater
} // namespace software
} // namespace openpower

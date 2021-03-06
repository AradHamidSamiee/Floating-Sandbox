/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2019-01-30
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "UIPreferencesManager.h"

#include "StandardSystemPaths.h"

#include <Game/ResourceLocator.h>

#include <GameCore/Utils.h>

UIPreferencesManager::UIPreferencesManager(
    std::shared_ptr<IGameController> gameController,
    LocalizationManager & localizationManager,
    std::shared_ptr<MusicController> musicController,
    ResourceLocator const & resourceLocator)
    : mDefaultShipLoadDirectory(resourceLocator.GetInstalledShipFolderPath())
    , mGameController(std::move(gameController))
    , mLocalizationManager(localizationManager)
    , mMusicController(std::move(musicController))
{
    //
    // Set defaults for our preferences
    //

    mShipLoadDirectories.push_back(mDefaultShipLoadDirectory);

    mScreenshotsFolderPath = StandardSystemPaths::GetInstance().GetUserPicturesGameFolderPath();

    mBlacklistedUpdates = { };
    mCheckUpdatesAtStartup = true;
    mShowStartupTip = true;
    mSaveSettingsOnExit = true;
    mShowShipDescriptionsAtShipLoad = true;
    mAutoShowSwitchboard = true;
    mSwitchboardBackgroundBitmapIndex = 0;

    mZoomIncrement = 1.05f;
    mPanIncrement = 20.0f;


    //
    // Load preferences
    //

    try
    {
        LoadPreferences();
    }
    catch (...)
    {
        // Ignore
    }
}

UIPreferencesManager::~UIPreferencesManager()
{
    //
    // Save preferences
    //

    try
    {
        SavePreferences();
    }
    catch (...)
    {
        // Ignore
    }
}

std::optional<std::string> UIPreferencesManager::LoadPreferredLanguage()
{
    auto const preferencesRootObject = LoadPreferencesRootObject();
    if (preferencesRootObject.has_value())
    {
        if (auto it = preferencesRootObject->find("language");
            it != preferencesRootObject->end() && it->second.is<std::string>())
        {
            return it->second.get<std::string>();
        }
    }
    
    return std::nullopt;
}

std::filesystem::path UIPreferencesManager::GetPreferencesFilePath()
{
    return StandardSystemPaths::GetInstance().GetUserGameRootFolderPath() / "ui_preferences.json";
}

std::optional<picojson::object> UIPreferencesManager::LoadPreferencesRootObject()
{
    auto const preferencesFilePath = GetPreferencesFilePath();

    if (std::filesystem::exists(preferencesFilePath))
    {
        auto const preferencesRootValue = Utils::ParseJSONFile(preferencesFilePath);

        if (preferencesRootValue.is<picojson::object>())
        {
            return preferencesRootValue.get<picojson::object>();
        }
    }

    return std::nullopt;
}

void UIPreferencesManager::LoadPreferences()
{
    auto const preferencesRootObject = LoadPreferencesRootObject();
    if (preferencesRootObject.has_value())
    {
        //
        // Ship load directories
        //

        if (auto shipLoadDirectoriesIt = preferencesRootObject->find("ship_load_directories");
            shipLoadDirectoriesIt != preferencesRootObject->end() && shipLoadDirectoriesIt->second.is<picojson::array>())
        {
            mShipLoadDirectories.clear();

            // Make sure default ship directory is always at the top
            mShipLoadDirectories.push_back(mDefaultShipLoadDirectory);

            auto shipLoadDirectories = shipLoadDirectoriesIt->second.get<picojson::array>();
            for (auto shipLoadDirectory : shipLoadDirectories)
            {
                if (shipLoadDirectory.is<std::string>())
                {
                    auto shipLoadDirectoryPath = std::filesystem::path(shipLoadDirectory.get<std::string>());

                    // Make sure dir still exists, and it's not in the vector already
                    if (std::filesystem::exists(shipLoadDirectoryPath)
                        && mShipLoadDirectories.cend() == std::find(
                            mShipLoadDirectories.cbegin(),
                            mShipLoadDirectories.cend(),
                            shipLoadDirectoryPath))
                    {
                        mShipLoadDirectories.push_back(shipLoadDirectoryPath);
                    }
                }
            }
        }

        //
        // Screenshots folder path
        //

        if (auto screenshotsFolderPathIt = preferencesRootObject->find("screenshots_folder_path");
            screenshotsFolderPathIt != preferencesRootObject->end() && screenshotsFolderPathIt->second.is<std::string>())
        {
            mScreenshotsFolderPath = screenshotsFolderPathIt->second.get<std::string>();
        }

        //
        // Blacklisted updates
        //

        if (auto blacklistedUpdatedIt = preferencesRootObject->find("blacklisted_updates");
            blacklistedUpdatedIt != preferencesRootObject->end() && blacklistedUpdatedIt->second.is<picojson::array>())
        {
            mBlacklistedUpdates.clear();

            auto blacklistedUpdates = blacklistedUpdatedIt->second.get<picojson::array>();
            for (auto blacklistedUpdate : blacklistedUpdates)
            {
                if (blacklistedUpdate.is<std::string>())
                {
                    auto blacklistedVersion = Version::FromString(blacklistedUpdate.get<std::string>());

                    if (mBlacklistedUpdates.cend() == std::find(
                        mBlacklistedUpdates.cbegin(),
                        mBlacklistedUpdates.cend(),
                        blacklistedVersion))
                    {
                        mBlacklistedUpdates.push_back(blacklistedVersion);
                    }
                }
            }
        }

        //
        // Check updates at startup
        //

        if (auto checkUpdatesAtStartupIt = preferencesRootObject->find("check_updates_at_startup");
            checkUpdatesAtStartupIt != preferencesRootObject->end() && checkUpdatesAtStartupIt->second.is<bool>())
        {
            mCheckUpdatesAtStartup = checkUpdatesAtStartupIt->second.get<bool>();
        }

        //
        // Show startup tip
        //

        if (auto showStartupTipIt = preferencesRootObject->find("show_startup_tip");
            showStartupTipIt != preferencesRootObject->end() && showStartupTipIt->second.is<bool>())
        {
            mShowStartupTip = showStartupTipIt->second.get<bool>();
        }

        //
        // Save settings on exit
        //

        if (auto saveSettingsOnExitIt = preferencesRootObject->find("save_settings_on_exit");
            saveSettingsOnExitIt != preferencesRootObject->end() && saveSettingsOnExitIt->second.is<bool>())
        {
            mSaveSettingsOnExit = saveSettingsOnExitIt->second.get<bool>();
        }

        //
        // Show ship descriptions at ship load
        //

        if (auto showShipDescriptionAtShipLoadIt = preferencesRootObject->find("show_ship_descriptions_at_ship_load");
            showShipDescriptionAtShipLoadIt != preferencesRootObject->end() && showShipDescriptionAtShipLoadIt->second.is<bool>())
        {
            mShowShipDescriptionsAtShipLoad = showShipDescriptionAtShipLoadIt->second.get<bool>();
        }

        //
        // Show tsunami notifications
        //

        if (auto showTsunamiNotificationsIt = preferencesRootObject->find("show_tsunami_notifications");
            showTsunamiNotificationsIt != preferencesRootObject->end() && showTsunamiNotificationsIt->second.is<bool>())
        {
            mGameController->SetDoShowTsunamiNotifications(showTsunamiNotificationsIt->second.get<bool>());
        }

        //
        // Ship auto-texturization default settings
        //

        if (auto shipAutoTexturizationDefaultSettingsIt = preferencesRootObject->find("ship_auto_texturization_default_settings");
            shipAutoTexturizationDefaultSettingsIt != preferencesRootObject->end() && shipAutoTexturizationDefaultSettingsIt->second.is<picojson::object>())
        {
            mGameController->SetShipAutoTexturizationDefaultSettings(ShipAutoTexturizationSettings::FromJSON(shipAutoTexturizationDefaultSettingsIt->second.get<picojson::object>()));
        }

        // We don't load/save this setting on purpose
        //////
        ////// Ship auto-texturization force defaults onto ship
        //////

        ////if (auto shipAutoTexturizationForceDefaultsOntoShipIt = preferencesRootObject->find("ship_auto_texturization_force_defaults_onto_ship");
        ////    shipAutoTexturizationForceDefaultsOntoShipIt != preferencesRootObject->end()
        ////    && shipAutoTexturizationForceDefaultsOntoShipIt->second.is<bool>())
        ////{
        ////    mGameController->SetShipAutoTexturizationDoForceDefaultSettingsOntoShipSettings(shipAutoTexturizationForceDefaultsOntoShipIt->second.get<bool>());
        ////}

        //
        // Auto-zoom at ship load
        //

        if (auto doAutoZoomAtShipLoadIt = preferencesRootObject->find("auto_zoom_at_ship_load");
            doAutoZoomAtShipLoadIt != preferencesRootObject->end() && doAutoZoomAtShipLoadIt->second.is<bool>())
        {
            mGameController->SetDoAutoZoomOnShipLoad(doAutoZoomAtShipLoadIt->second.get<bool>());
        }

        //
        // Auto show switchboard
        //

        if (auto autoShowSwitchboardIt = preferencesRootObject->find("auto_show_switchboard");
            autoShowSwitchboardIt != preferencesRootObject->end() && autoShowSwitchboardIt->second.is<bool>())
        {
            mAutoShowSwitchboard = autoShowSwitchboardIt->second.get<bool>();
        }

        //
        // Switchboard background bitmap index
        //

        if (auto switchboardBackgroundBitmapIndexIt = preferencesRootObject->find("switchboard_background_bitmap_index");
            switchboardBackgroundBitmapIndexIt != preferencesRootObject->end() && switchboardBackgroundBitmapIndexIt->second.is<std::int64_t>())
        {
            mSwitchboardBackgroundBitmapIndex = static_cast<int>(switchboardBackgroundBitmapIndexIt->second.get<std::int64_t>());
        }

        //
        // Show electrical notifications
        //

        if (auto showElectricalNotificationsIt = preferencesRootObject->find("show_electrical_notifications");
            showElectricalNotificationsIt != preferencesRootObject->end() && showElectricalNotificationsIt->second.is<bool>())
        {
            mGameController->SetDoShowElectricalNotifications(showElectricalNotificationsIt->second.get<bool>());
        }

        //
        // Zoom increment
        //

        if (auto zoomIncrementIt = preferencesRootObject->find("zoom_increment");
            zoomIncrementIt != preferencesRootObject->end() && zoomIncrementIt->second.is<double>())
        {
            mZoomIncrement = static_cast<float>(zoomIncrementIt->second.get<double>());
        }

        //
        // Pan increment
        //

        if (auto panIncrementIt = preferencesRootObject->find("pan_increment");
            panIncrementIt != preferencesRootObject->end() && panIncrementIt->second.is<double>())
        {
            mPanIncrement = static_cast<float>(panIncrementIt->second.get<double>());
        }

        //
        // Show status text
        //

        if (auto showStatusTextIt = preferencesRootObject->find("show_status_text");
            showStatusTextIt != preferencesRootObject->end() && showStatusTextIt->second.is<bool>())
        {
            mGameController->SetShowStatusText(showStatusTextIt->second.get<bool>());
        }

        //
        // Show extended status text
        //

        if (auto showExtendedStatusTextIt = preferencesRootObject->find("show_extended_status_text");
            showExtendedStatusTextIt != preferencesRootObject->end() && showExtendedStatusTextIt->second.is<bool>())
        {
            mGameController->SetShowExtendedStatusText(showExtendedStatusTextIt->second.get<bool>());
        }

        //
        // Sound and Music
        //

        {
            // Global mute
            if (auto globalMuteIt = preferencesRootObject->find("global_mute");
                globalMuteIt != preferencesRootObject->end() && globalMuteIt->second.is<bool>())
            {
                bool const isSoundMuted = globalMuteIt->second.get<bool>();                
                AudioController::SetGlobalMute(isSoundMuted);
                mGameController->NotifySoundMuted(isSoundMuted);
            }

            // Background music volume
            if (auto backgroundMusicVolumeIt = preferencesRootObject->find("background_music_volume");
                backgroundMusicVolumeIt != preferencesRootObject->end() && backgroundMusicVolumeIt->second.is<double>())
            {
                mMusicController->SetBackgroundMusicVolume(static_cast<float>(backgroundMusicVolumeIt->second.get<double>()));
            }

            // Play background music
            if (auto playBackgroundMusicIt = preferencesRootObject->find("play_background_music");
                playBackgroundMusicIt != preferencesRootObject->end() && playBackgroundMusicIt->second.is<bool>())
            {
                mMusicController->SetPlayBackgroundMusic(playBackgroundMusicIt->second.get<bool>());
            }

            // Last-played background music
            if (auto lastPlayedBackgroundMusicIt = preferencesRootObject->find("last_played_background_music");
                lastPlayedBackgroundMusicIt != preferencesRootObject->end() && lastPlayedBackgroundMusicIt->second.is<int64_t>())
            {
                mMusicController->SetLastPlayedBackgroundMusic(static_cast<size_t>(lastPlayedBackgroundMusicIt->second.get<int64_t>()));
            }

            // Game music volume
            if (auto gameMusicVolumeIt = preferencesRootObject->find("game_music_volume");
                gameMusicVolumeIt != preferencesRootObject->end() && gameMusicVolumeIt->second.is<double>())
            {
                mMusicController->SetGameMusicVolume(static_cast<float>(gameMusicVolumeIt->second.get<double>()));
            }

            // Play sinking music
            if (auto playSinkingMusicIt = preferencesRootObject->find("play_sinking_music");
                playSinkingMusicIt != preferencesRootObject->end() && playSinkingMusicIt->second.is<bool>())
            {
                mMusicController->SetPlaySinkingMusic(playSinkingMusicIt->second.get<bool>());
            }
        }

        // Note: we do not load language, as it has been loaded already and passed
        // to the LocalizationManager
    }
}

void UIPreferencesManager::SavePreferences() const
{
    picojson::object preferencesRootObject;


    // Add ship load directories

    picojson::array shipLoadDirectories;
    for (auto shipDir : mShipLoadDirectories)
    {
        shipLoadDirectories.push_back(picojson::value(shipDir.string()));
    }

    preferencesRootObject["ship_load_directories"] = picojson::value(shipLoadDirectories);

    // Add screenshots folder path
    preferencesRootObject["screenshots_folder_path"] = picojson::value(mScreenshotsFolderPath.string());

    // Add blacklisted updates

    picojson::array blacklistedUpdates;
    for (auto blacklistedUpdate : mBlacklistedUpdates)
    {
        blacklistedUpdates.push_back(picojson::value(blacklistedUpdate.ToString()));
    }

    preferencesRootObject["blacklisted_updates"] = picojson::value(blacklistedUpdates);

    // Add check updates at startup
    preferencesRootObject["check_updates_at_startup"] = picojson::value(mCheckUpdatesAtStartup);

    // Add show startup tip
    preferencesRootObject["show_startup_tip"] = picojson::value(mShowStartupTip);

    // Add save settings on exit
    preferencesRootObject["save_settings_on_exit"] = picojson::value(mSaveSettingsOnExit);

    // Add show ship descriptions at ship load
    preferencesRootObject["show_ship_descriptions_at_ship_load"] = picojson::value(mShowShipDescriptionsAtShipLoad);

    // Add show tsunami notification
    preferencesRootObject["show_tsunami_notifications"] = picojson::value(mGameController->GetDoShowTsunamiNotifications());

    // Add ship auto-texturization default settings
    preferencesRootObject["ship_auto_texturization_default_settings"] = picojson::value(mGameController->GetShipAutoTexturizationDefaultSettings().ToJSON());

    // We don't load/save this setting on purpose
    ////// Add ship auto-texturization force defaults onto ship
    ////preferencesRootObject["ship_auto_texturization_force_defaults_onto_ship"] = picojson::value(mGameController->GetShipAutoTexturizationDoForceDefaultSettingsOntoShipSettings());

    // Add auto zoom at ship load
    preferencesRootObject["auto_zoom_at_ship_load"] = picojson::value(mGameController->GetDoAutoZoomOnShipLoad());

    // Add auto show switchboard
    preferencesRootObject["auto_show_switchboard"] = picojson::value(mAutoShowSwitchboard);

    // Add switchboard background bitmap index
    preferencesRootObject["switchboard_background_bitmap_index"] = picojson::value(static_cast<std::int64_t>(mSwitchboardBackgroundBitmapIndex));

    // Add show electrical notifications
    preferencesRootObject["show_electrical_notifications"] = picojson::value(mGameController->GetDoShowElectricalNotifications());

    // Add zoom increment
    preferencesRootObject["zoom_increment"] = picojson::value(static_cast<double>(mZoomIncrement));

    // Add pan increment
    preferencesRootObject["pan_increment"] = picojson::value(static_cast<double>(mPanIncrement));

    // Add show status text
    preferencesRootObject["show_status_text"] = picojson::value(mGameController->GetShowStatusText());

    // Add show extended status text
    preferencesRootObject["show_extended_status_text"] = picojson::value(mGameController->GetShowExtendedStatusText());

    //
    // Sounds and Music
    //

    {
        preferencesRootObject["global_mute"] = picojson::value(AudioController::GetGlobalMute());

        preferencesRootObject["background_music_volume"] = picojson::value(static_cast<double>(mMusicController->GetBackgroundMusicVolume()));

        preferencesRootObject["play_background_music"] = picojson::value(mMusicController->GetPlayBackgroundMusic());

        preferencesRootObject["last_played_background_music"] = picojson::value(static_cast<int64_t>(mMusicController->GetLastPlayedBackgroundMusic()));

        preferencesRootObject["game_music_volume"] = picojson::value(static_cast<double>(mMusicController->GetGameMusicVolume()));

        preferencesRootObject["play_sinking_music"] = picojson::value(mMusicController->GetPlaySinkingMusic());
    }

    // Language
    if (mLocalizationManager.GetDesiredLanguage().has_value())
        preferencesRootObject["language"] = picojson::value(mLocalizationManager.GetDesiredLanguage()->Identifier);

    // Save
    Utils::SaveJSONFile(
        picojson::value(preferencesRootObject),
        GetPreferencesFilePath());
}
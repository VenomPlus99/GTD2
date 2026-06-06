#include <Geode/Geode.hpp>
#include <Geode/modify/GJEffectManager.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
using namespace geode::prelude;

struct SavedItem
{
    int itemID;
    int value;
};

struct SavedItems
{
    std::vector<SavedItem> items;
};

// Serialize SavedItem
template <>
struct matjson::Serialize<SavedItem>
{
    static Result<SavedItem> fromJson(matjson::Value const &value)
    {
        GEODE_UNWRAP_INTO(int itemID, value["itemID"].asInt());
        GEODE_UNWRAP_INTO(int val, value["value"].asInt());
        return Ok(SavedItem{itemID, val});
    }

    static matjson::Value toJson(SavedItem const &value)
    {
        matjson::Value obj;
        obj["itemID"] = value.itemID;
        obj["value"] = value.value;
        return obj;
    }
};

// Serialize SavedItems
template <>
struct matjson::Serialize<SavedItems>
{
    static Result<SavedItems> fromJson(matjson::Value const &value)
    {
        GEODE_UNWRAP_INTO(std::vector<SavedItem> items, value["items"].as<std::vector<SavedItem>>());
        return Ok(SavedItems{items});
    }

    static matjson::Value toJson(SavedItems const &value)
    {
        matjson::Value obj;
        obj["items"] = value.items;
        return obj;
    }
};

static bool g_skipNextSave = false;
static constexpr int DELETE_SAVE_FLAG_ITEM_ID = 8887;
static constexpr int SAVE_FLAG_ITEM_ID = 8888;

static std::vector<int> ITEM_IDS_TO_SAVE = {2, 999, 7002, 8035, 8036, 8037, 8889, 9001,
                                            9002, 9030, 9055, 9056, 9063, 9064, 9065, 9066, 9067, 9068,
                                            9500, 9501, 9502, 9503, 9504, 9505,
                                            9506, 9507, 9508, 9509, 9510, 9511, 9512,
                                            9513, 9514, 9515, 9516, 9517, 9518, 9519,
                                            9520, 9521, 9522, 9523, 9524, 9525, 9526,
                                            9527, 9528, 9529, 9530, 9531, 9532, 9533, 9534,
                                            9535, 9953, 9954, 9955, 9956, 9957, 9970, 9979, 9977, 9984, 9988, 9989,
                                            9990, 9991, 9993, 9994, 9996, 9997, 9998};

static const std::unordered_set<int> ITEM_IDS_TO_KEEP = { // Prevents reset of settings when player presses New Game
    9998, 9996, 9997, 9988,
    9994, 9993, 9991,
    9989, 9990};

inline bool modEnabled()
{
    if (!Mod::get()->getSettingValue<bool>("enable-mod"))
        return false;

    GJGameLevel *level = nullptr;

    if (auto gm = GameManager::sharedState())
    {
        if (auto play = gm->getPlayLayer())
            level = play->m_level;
        else if (auto editor = gm->getEditorLayer())
            level = editor->m_level;
    }

    if (!level)
    {
        if (auto pl = PlayLayer::get())
            level = pl->m_level;
    }

    if (!level)
        return false;

    return level->m_levelID == 142065893 || level->m_levelID == 141857012; // first ID is actual level, second one is for testing
}

int getItemValue(GJEffectManager *mgr, int itemID)
{
    if (auto it = mgr->m_itemCountMap.find(itemID); it != mgr->m_itemCountMap.end())
        return it->second;
    return 0;
}

void saveItemStates(GJBaseGameLayer *layer)
{
    if (!modEnabled())
        return;
    if (!layer)
        return;

    SavedItems data;

    for (int id : ITEM_IDS_TO_SAVE)
    {
        int value = getItemValue(layer->m_effectManager, id);
        // if (value > 0)
        // {
        data.items.push_back({id, value});
        // }
    }

    Mod::get()->setSavedValue("gtd2-save", data);

    log::debug("[DEBUG] Saved items:");
    for (auto const &item : data.items)
        log::debug("  ItemID: {} | Value: {}", item.itemID, item.value);
}

void restoreItemStates(GJBaseGameLayer *layer)
{
    if (!modEnabled())
        return;
    if (!layer)
        return;

    auto data = Mod::get()->getSavedValue<SavedItems>(
        "gtd2-save",
        SavedItems{});

    for (auto const &item : data.items)
    {
        layer->m_effectManager->updateCountForItem(item.itemID, item.value);
        layer->updateCounters(item.itemID, item.value);
    }
}

void deleteSaveFileExceptSettings()
{
    if (!modEnabled())
        return;

    g_skipNextSave = true;
    auto pl = PlayLayer::get();
    if (!pl)
        return;

    auto data = Mod::get()->getSavedValue<SavedItems>(
        "gtd2-save",
        SavedItems{});

    SavedItems newData;

    for (auto const &item : data.items)
    {
        if (ITEM_IDS_TO_KEEP.count(item.itemID))
        {
            newData.items.push_back(item);
        }
    }

    pl->m_effectManager->updateCountForItem(8889, 0);
    pl->updateCounters(8889, 0);

    Mod::get()->setSavedValue("gtd2-save", newData);

    log::debug("Deleted all item IDs");
    for (auto const &item : newData.items)
        log::debug("  ItemID: {} | Value: {}", item.itemID, item.value);
}

void hideCursor()
{
    if (auto gm = GameManager::sharedState())
    {
        // If the "Show Cursor In-Game" option is enabled, do nothing.
        if (gm->getGameVariable(GameVar::ShowCursor))
            return;
        PlatformToolbox::hideCursor();
    }
}

void showCursor()
{
    if (auto gm = GameManager::sharedState())
    {
        // If the "Show Cursor In-Game" option is enabled, do nothing.
        if (gm->getGameVariable(GameVar::ShowCursor))
            return;
        PlatformToolbox::showCursor();
    }
}

class $modify(GJEffectManager)
{
    void updateCountForItem(int itemID, int value)
    {
        if (!modEnabled())
        {
            GJEffectManager::updateCountForItem(itemID, value);
            return;
        }

        int prev = getItemValue(this, itemID);

        GJEffectManager::updateCountForItem(itemID, value);

        if (itemID == SAVE_FLAG_ITEM_ID && prev == 0 && value == 1)
        {
            if (auto pl = PlayLayer::get())
                saveItemStates(pl);
        }

        if (itemID == DELETE_SAVE_FLAG_ITEM_ID && prev == 0 && value == 1)
        {
            if (auto pl = PlayLayer::get())
                deleteSaveFileExceptSettings();
        }
    }
};

class $modify(PlayLayer)
{
    void exitLevel()
    {
        if (modEnabled())
        {
            if (!g_skipNextSave)
                saveItemStates(this);
            else
                g_skipNextSave = false;
        }

        PlayLayer::onExit();
    }

    void setupHasCompleted()
    {
        PlayLayer::setupHasCompleted();

        if (modEnabled())
            restoreItemStates(this);
    }
};

#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_MACOS)

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>

class $modify(GJBaseGameLayerHook, GJBaseGameLayer)
{
    struct Fields
    {
        bool isPlaying;
    };

    std::vector<EventLinkTrigger *> findAllP2LeftPressEvents()
    {
        std::vector<EventLinkTrigger *> result;

        for (auto obj : CCArrayExt<GameObject *>(m_objects))
        {
            if (obj->m_objectID != 3604)
                continue;

            auto ev = static_cast<EventLinkTrigger *>(obj);

            // Condition: Player 2 AND eventID == 71
            if (ev->m_extraID2 == 2 && ev->m_eventIDs.count(71))
                result.push_back(ev);
        }

        return result;
    }

    std::vector<EventLinkTrigger *> findAllP2LeftReleaseEvents()
    {
        std::vector<EventLinkTrigger *> result;

        for (auto obj : CCArrayExt<GameObject *>(m_objects))
        {
            if (obj->m_objectID != 3604)
                continue;

            auto ev = static_cast<EventLinkTrigger *>(obj);

            // Condition: Player 2 AND eventID == 71
            if (ev->m_extraID2 == 2 && ev->m_eventIDs.count(72))
                result.push_back(ev);
        }

        return result;
    }

    void changeEventP2LeftPressToP1JumpPress(std::vector<EventLinkTrigger *> events)
    {
        for (auto ev : events)
        {
            ev->m_eventIDs.erase(71);  // remove Left Release
            ev->m_eventIDs.insert(69); // add Jump Release

            ev->m_extraID2 = 1; // set Event to P1
        }
    }
    void changeEventP2LeftReleaseToP1JumpRelease(std::vector<EventLinkTrigger *> events)
    {
        for (auto ev : events)
        {
            ev->m_eventIDs.erase(72);  // remove Left Release
            ev->m_eventIDs.insert(70); // add Jump Release

            ev->m_extraID2 = 1; // set Event to P1
        }
    }

    void setItem(int pID, int pVal)
    {
        if (!pID)
            return;

        m_effectManager->updateCountForItem(pID, pVal);
        updateCounters(pID, pVal);
    }

    bool init()
    {
        if (!GJBaseGameLayer::init())
            return false;

        m_fields->isPlaying = false;

        return true;
    }
};

class $modify(PlayLayer)
{
    void setupHasCompleted()
    {
        if (modEnabled())
        {
            auto gjbgl = reinterpret_cast<GJBaseGameLayerHook *>(this);
            gjbgl->changeEventP2LeftPressToP1JumpPress(gjbgl->findAllP2LeftPressEvents());
            gjbgl->changeEventP2LeftReleaseToP1JumpRelease(gjbgl->findAllP2LeftReleaseEvents());
            gjbgl->m_fields->isPlaying = true;
        }

        PlayLayer::setupHasCompleted();

        if (modEnabled())
        {
            auto gjbgl = reinterpret_cast<GJBaseGameLayerHook *>(this);
            gjbgl->setItem(9999, 1);
            hideCursor();
        }
    }

    void resume()
    {
        PlayLayer::resume();

        if (modEnabled())
        {
            hideCursor();
        }
    }
};

class $modify(LevelEditorLayer)
{
    struct Fields
    {
        std::vector<std::pair<EventLinkTrigger *, std::pair<int, int>>> m_originalEvents;
    };

    void onPlaytest()
    {
        auto gjbgl = reinterpret_cast<GJBaseGameLayerHook *>(this);

        if (modEnabled())
        {
            for (auto ev : gjbgl->findAllP2LeftPressEvents())
                m_fields->m_originalEvents.push_back({ev, {71, ev->m_extraID2}});

            for (auto ev : gjbgl->findAllP2LeftReleaseEvents())
                m_fields->m_originalEvents.push_back({ev, {72, ev->m_extraID2}});

            gjbgl->changeEventP2LeftPressToP1JumpPress(gjbgl->findAllP2LeftPressEvents());
            gjbgl->changeEventP2LeftReleaseToP1JumpRelease(gjbgl->findAllP2LeftReleaseEvents());
            gjbgl->m_fields->isPlaying = true;
        }

        LevelEditorLayer::onPlaytest();

        if (modEnabled())
        {
            gjbgl->setItem(9999, 1);
        }
    }

    void onStopPlaytest()
    {
        auto gjbgl = reinterpret_cast<GJBaseGameLayerHook *>(this);

        if (modEnabled())
        {
            gjbgl->m_fields->isPlaying = false;

            for (auto &entry : m_fields->m_originalEvents)
            {
                auto ev = entry.first;
                auto original = entry.second;

                ev->m_eventIDs.erase(69);
                ev->m_eventIDs.erase(70);
                ev->m_eventIDs.insert(original.first);

                ev->m_extraID2 = original.second;
            }

            m_fields->m_originalEvents.clear();
        }

        LevelEditorLayer::onStopPlaytest();
    }
};

class $modify(PauseLayer)
{
    void onEnter()
    {
        PauseLayer::onEnter();

        if (modEnabled())
        {
            showCursor();
        }
    }
};

class $modify(EndLevelLayer)
{
    void onEnter()
    {
        EndLevelLayer::onEnter();

        if (modEnabled())
        {
            showCursor();
        }
    }
};

#endif
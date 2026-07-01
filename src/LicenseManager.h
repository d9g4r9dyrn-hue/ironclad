#pragma once
#include <JuceHeader.h>

// Online activation against the Gumroad License API.
//
// Flow: the user enters the license key from their Gumroad receipt once; we POST
// it to Gumroad's verify endpoint, cache "activated" locally, and from then on
// trust the cache (we do NOT re-verify online every load - that would keep
// incrementing the seat counter). Unactivated instances run in a limited demo
// (see IroncladProcessor::processBlock).
//
// Until a real Gumroad product id is pasted below, the plugin is fully unlocked
// (dev bypass) so development, testing, and store setup aren't blocked.
class LicenseManager : private juce::Thread
{
public:
    // ====================================================================
    //  CONFIG - paste your Gumroad product id here before shipping.
    //  Find it via the Gumroad API (GET /v2/products) or the product page;
    //  the verify endpoint takes `product_id`. Leaving the placeholder keeps
    //  the plugin unlocked.
    // ====================================================================
    static constexpr const char* productId = "REPLACE_WITH_GUMROAD_PRODUCT_ID";
    static constexpr int         maxSeats  = 3;   // activations allowed per key

    enum class Status { Idle, Working, Success, Failed };

    // Called on the message thread with a status + human-readable message.
    std::function<void(Status, juce::String)> onStatus;

    LicenseManager() : juce::Thread("IroncladLicense")
    {
        load();
        const juce::String pid(productId);
        devUnlocked = pid.isEmpty() || pid.startsWith("REPLACE_");
    }

    ~LicenseManager() override { stopThread(3000); }

    bool isActivated()   const noexcept { return devUnlocked || activated.load(); }
    bool isDevUnlocked() const noexcept { return devUnlocked; }
    juce::String getStoredKey() const   { return storedKey; }

    // Kick off an online verification of `key` on a background thread.
    void activate(const juce::String& key)
    {
        if (isThreadRunning())
            return;
        pendingKey = key.trim();
        if (pendingKey.isEmpty())
        {
            if (onStatus) onStatus(Status::Failed, "Enter your license key first.");
            return;
        }
        startThread();
    }

    void deactivate()
    {
        activated = false;
        storedKey = {};
        save();
    }

private:
    std::atomic<bool> activated { false };
    bool devUnlocked = false;
    juce::String storedKey, pendingKey;

    juce::File stateFile() const
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Ironclad");
        dir.createDirectory();
        return dir.getChildFile("Ironclad.license");
    }

    void load()
    {
        auto f = stateFile();
        if (! f.existsAsFile())
            return;
        if (auto xml = juce::parseXML(f))
        {
            storedKey = xml->getStringAttribute("key");
            activated = xml->getBoolAttribute("activated", false);
        }
    }

    void save()
    {
        juce::XmlElement xml("ironclad-license");
        xml.setAttribute("key", storedKey);
        xml.setAttribute("activated", (bool) activated.load());
        xml.writeTo(stateFile());
    }

    void report(Status s, const juce::String& msg)
    {
        juce::MessageManager::callAsync([cb = onStatus, s, msg]
        {
            if (cb) cb(s, msg);
        });
    }

    void run() override
    {
        report(Status::Working, "Contacting Gumroad…");
        const auto key = pendingKey;

        auto url = juce::URL("https://api.gumroad.com/v2/licenses/verify")
                       .withParameter("product_id", productId)
                       .withParameter("license_key", key)
                       .withParameter("increment_uses_count", "true");

        auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                        .withConnectionTimeoutMs(8000);

        std::unique_ptr<juce::InputStream> stream(url.createInputStream(opts));
        if (threadShouldExit())
            return;
        if (stream == nullptr)
        {
            report(Status::Failed, "Couldn't reach Gumroad. Check your connection and try again.");
            return;
        }

        const auto response = stream->readEntireStreamAsString();
        const auto json = juce::JSON::parse(response);

        if (! (bool) json.getProperty("success", false))
        {
            report(Status::Failed, "That key wasn't recognized. Double-check it and try again.");
            return;
        }

        const auto purchase = json.getProperty("purchase", juce::var());
        const bool refunded = (bool) purchase.getProperty("refunded", false);
        const bool disputed = (bool) purchase.getProperty("chargebacked", false)
                           || (bool) purchase.getProperty("disputed", false);
        const int  uses     = (int)  json.getProperty("uses", 0);

        if (refunded || disputed)
        {
            report(Status::Failed, "This license is no longer active.");
            return;
        }
        if (uses > maxSeats)
        {
            report(Status::Failed, "Activation limit reached (" + juce::String(maxSeats)
                                    + " machines). Contact info@corticorp.com.");
            return;
        }

        storedKey = key;
        activated = true;
        save();
        report(Status::Success, "Activated — thank you!");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseManager)
};

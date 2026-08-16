#include "App/AppLog.h"
#include "App/AppSettings.h"
#include "App/MainComponent.h"
#include "BinaryData.h"

class ToneStarApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        AppLog::install();
        const bool selfTest = commandLine.containsIgnoreCase("--self-test");
        if (selfTest)
            AppLog::note("self-test mode");
        AppLog::note("create window");
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), selfTest);
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name, bool selfTest)
            : DocumentWindow(name, Theme::voidFill(),
                             DocumentWindow::minimiseButton | DocumentWindow::closeButton)
        {
            AppLog::note("window ctor");
            setUsingNativeTitleBar(false);
            setTitleBarHeight(0);
            setOpaque(true);
            setDropShadowEnabled(true);
            AppLog::note("create MainComponent");
            setContentOwned(new MainComponent(), true);
            AppLog::note("content created");
            setResizable(false, false);

            auto& settings = appSettings();
            const int x = settings.getIntValue("winX", -10000);
            const int y = settings.getIntValue("winY", -10000);
            if (x > -10000 && y > -10000)
                setTopLeftPosition(x, y);
            else
                centreWithSize(getWidth(), getHeight());

            setVisible(true);
            AppLog::note("window visible");
            if (auto* peer = getPeer())
            {
                const auto icon = juce::ImageFileFormat::loadFrom(BinaryData::icon_png,
                                                                  BinaryData::icon_pngSize);
                if (icon.isValid())
                    peer->setIcon(icon);
            }

            if (auto* content = dynamic_cast<MainComponent*>(getContentComponent()))
            {
                if (selfTest)
                    content->enableSelfTest();
                content->applyWindowSize();
                content->repaint();
                content->startAudio();
                if (selfTest)
                    content->scheduleSelfTest();
            }
            AppLog::markReady();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (auto* content = dynamic_cast<MainComponent*>(getContentComponent()))
                if (content->keyPressed(key))
                    return true;
            return false;
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(ToneStarApplication)

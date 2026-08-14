#include "AppSettings.h"
#include "MainComponent.h"
#include "BinaryData.h"

class LittleAmpApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
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
        explicit MainWindow(juce::String name)
            : DocumentWindow(name, CuteLookAndFeel::voidFill(), 0)
        {
            setUsingNativeTitleBar(false);
            setTitleBarHeight(0);
            setOpaque(true);
            setDropShadowEnabled(true);
            setContentOwned(new MainComponent(), true);
            setResizable(false, false);

            auto& settings = appSettings();
            const int x = settings.getIntValue("winX", -10000);
            const int y = settings.getIntValue("winY", -10000);
            if (x > -10000 && y > -10000)
                setTopLeftPosition(x, y);
            else
                centreWithSize(getWidth(), getHeight());

            setVisible(true);
            if (auto* peer = getPeer())
            {
                const auto icon = juce::ImageFileFormat::loadFrom(BinaryData::icon_png,
                                                                  BinaryData::icon_pngSize);
                if (icon.isValid())
                    peer->setIcon(icon);
            }
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

START_JUCE_APPLICATION(LittleAmpApplication)

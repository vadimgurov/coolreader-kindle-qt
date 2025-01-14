#ifndef DEVICE_H
#define DEVICE_H

#include <QProcess>
#include <QDebug>

class Device
{
public:
    static Device& instance()
    {
        static Device instance;
        return instance;
    }

    typedef enum MODEL {
        UNKNOWN = 0,
        EMULATOR, // qvfb qws environment
        K2,       // Kindle 2
        KDX,      // Kindle DX(G)
        K3,       // Kindle Keyboard
        K4NT,     // Kindle Non-Touch Silver 2011
        K4NTB,    // Kindle Non-Touch Black 2012
        KT,       // Kindle Touch
        KPW       // Kindle PaperWhite
    } Model;

    static Model getModel() {
        return m_model;
    }

    static int getWidth() {
        return WIDTH[m_model];
    }

    static int getHeight() {
        return HEIGHT[m_model];
    }

    static bool hasKeyboard() {
        return KEYBOARD[m_model];
    }

    static bool hasFiveWay() {
        return FIVE_WAY[m_model];
    }

    static bool isTouch() {
        return !FIVE_WAY[m_model] && !KEYBOARD[m_model];
    }

    static bool isEmulator() { return m_model == EMULATOR; }

    static void suspendFramework() {
        if (!isTouch()) QProcess::execute("killall -STOP cvm");
        else {
            system("./ktsuspend.sh");
        }
    }

    static void resumeFramework() {
        if (!isTouch()) QProcess::execute("killall -CONT cvm"); else {
            system("./ktresume.sh");
        }
    }

private:
    static const int WIDTH[], HEIGHT[];
    static const bool KEYBOARD[];
    static const bool FIVE_WAY[];

    static MODEL m_model;

    Device();
    Device(Device const&);
    void operator=(Device const&);
};

#endif // DEVICE_H

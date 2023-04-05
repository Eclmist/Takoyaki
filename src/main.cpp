#include "Tako/includes/tako.h"

#include <Windows.h>
#include <iostream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize the Taco library
    Tako::TakoError result = Tako::Initialize();
    if (result != Tako::TakoError::OK) {
        std::cerr << "Failed to initialize Taco library: " << static_cast<int>(result) << std::endl;
        return -1;
    }

    // Start capturing the screen
    result = Tako::StartCapture();
    if (result != Tako::TakoError::OK) {
        std::cerr << "Failed to start capturing screen: " << static_cast<int>(result) << std::endl;
        Tako::Shutdown();
        return -1;
    }

    // Share a specified area of the screen
    Tako::TakoDisplayBuffer* buffer = nullptr;
    result = Tako::GetBuffer(0, 0, 640, 480, &buffer);
    if (result != Tako::TakoError::OK) {
        std::cerr << "Failed to get screen buffer: " << static_cast<int>(result) << std::endl;
        Tako::StopCapture();
        Tako::Shutdown();
        return -1;
    }

    // TODO: Share the buffer with others

    // Stop capturing the screen and clean up the library
    Tako::StopCapture();
    Tako::Shutdown();

    return 0;
}

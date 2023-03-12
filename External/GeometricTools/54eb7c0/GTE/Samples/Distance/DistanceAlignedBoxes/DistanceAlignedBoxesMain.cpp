// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2023
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// Version: 6.0.2022.01.06

#include "DistanceAlignedBoxesWindow3.h"
#include <iostream>

int32_t main()
{
    try
    {
        Window::Parameters parameters(L"DistanceAlignedBoxesWindow3", 0, 0, 512, 512);
        auto window = TheWindowSystem.Create<DistanceAlignedBoxesWindow3>(parameters);
        TheWindowSystem.MessagePump(window, TheWindowSystem.DEFAULT_ACTION);
        TheWindowSystem.Destroy(window);
    }
    catch (std::exception const& e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}
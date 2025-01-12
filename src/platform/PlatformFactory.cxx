/* bzflag
 * Copyright (c) 1993-2021 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "PlatformFactory.h"
#include "bzfSDL.h"
#include "ErrorHandler.h"
#include "BzfMedia.h"
#include "BzfJoystick.h"

PlatformFactory*    PlatformFactory::instance = 0;
BzfMedia*       PlatformFactory::media = 0;

PlatformFactory::PlatformFactory()
{
    Uint32 flags = 0;
    if (SDL_Init(flags) < 0)
    {
        printFatalError("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    };
}

PlatformFactory::~PlatformFactory()
{
    if (media)
        media->closeAudio();
    SDL_Quit();
    delete media;
}

BzfJoystick*        PlatformFactory::createJoystick()
{
    // if a platform doesn't have a native joystick impl., bzfjoystick provides defaults.
    return new BzfJoystick();
}

BzfMedia*       PlatformFactory::getMedia()
{
    if (!media) media = getInstance()->createMedia();
    return media;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4

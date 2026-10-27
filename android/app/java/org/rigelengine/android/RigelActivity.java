package org.rigelengine.android;

import org.libsdl.app.SDLActivity;

public class RigelActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_mixer", "c++_shared", "main"};
    }
}

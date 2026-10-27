package org.rigelengineproton.android;

import java.io.File;
import org.libsdl.app.SDLActivity;

public class RigelActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_mixer", "c++_shared", "main"};
    }

    @Override
    protected String[] getArguments() {
        return new String[] {new File(getNoBackupFilesDir(), "game-data/library").getAbsolutePath()};
    }
}

package org.rigelengineproton.android;

import java.io.File;
import android.content.Intent;
import org.libsdl.app.SDLActivity;

public class RigelActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_mixer", "c++_shared", "main"};
    }

    public void openGameDataSetup() {
        runOnUiThread(() -> {
            Intent intent = new Intent(this, SetupActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            intent.putExtra("manage", true);
            startActivity(intent);
        });
    }

    @Override
    protected String[] getArguments() {
        return new String[] {new File(getNoBackupFilesDir(), "game-data/library").getAbsolutePath()};
    }
}

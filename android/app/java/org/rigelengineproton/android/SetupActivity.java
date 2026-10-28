package org.rigelengineproton.android;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Android owns folder permissions; the native engine receives an imported library path. */
public class SetupActivity extends Activity {
    private static final int PICK_FOLDER = 1;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private TextView status;
    private Button choose;
    private Button refresh;
    private Button play;
    private boolean importing;
    private boolean gameStarted;
    private boolean leftForGame;

    private File storage() {
        return new File(getNoBackupFilesDir(), "game-data");
    }

    private SharedPreferences preferences() {
        return getSharedPreferences("game-data", MODE_PRIVATE);
    }

    private Button button(LinearLayout layout, String label, View.OnClickListener action) {
        Button button = new Button(this);
        button.setText(label);
        button.setOnClickListener(action);
        layout.addView(button, new LinearLayout.LayoutParams(-1, -2));
        return button;
    }

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (24 * getResources().getDisplayMetrics().density);
        layout.setPadding(pad, pad / 2, pad, pad / 2);
        layout.setBackgroundColor(Color.rgb(16, 23, 31));
        TextView title = new TextView(this);
        title.setText("RigelEngine: Proton");
        title.setTextColor(Color.WHITE);
        title.setTextSize(24);
        layout.addView(title);
        TextView help = new TextView(this);
        help.setText("Choose a folder with your DOS game files, either together or in DN1 / DN2 "
                + "subfolders. Recognized files are copied into the app; your originals stay unchanged.");
        help.setTextColor(Color.LTGRAY);
        layout.addView(help);
        status = new TextView(this);
        status.setTextColor(Color.WHITE);
        status.setPadding(0, pad / 3, 0, pad / 3);
        layout.addView(status, new LinearLayout.LayoutParams(-1, 0, 1));
        choose = button(layout, "Choose game folder", v -> pickFolder());
        refresh = button(layout, "Reimport selected folder", v -> {
            String uri = preferences().getString("folder", "");
            if (uri.isEmpty()) {
                pickFolder();
            } else {
                importFolder(Uri.parse(uri));
            }
        });
        play = button(layout, "Play", v -> startGame());
        setContentView(layout);
        updateStatus(null);
        if (state == null && !getIntent().getBooleanExtra("manage", false)
                && play.isEnabled()) {
            startGame();
        }
    }

    private void startGame() {
        gameStarted = true;
        startActivity(new Intent(this, RigelActivity.class));
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (gameStarted) {
            leftForGame = true;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (leftForGame) {
            finish();
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        gameStarted = false;
        leftForGame = false;
        updateStatus(null);
        // A singleTask activity can be reused when the launcher is tapped.
        // Treat that like a fresh launch, while retaining the explicit
        // in-game request to manage the selected folder.
        if (!intent.getBooleanExtra("manage", false) && play.isEnabled()) {
            startGame();
        }
    }

    private void updateStatus(String message) {
        String games = "";
        try {
            games = GameDataImporter.describe(GameDataImporter.recover(storage()));
        } catch (IOException error) {
            message = error.getMessage();
        }
        status.setText(message != null ? message : games.isEmpty()
                ? "No game files imported yet." : "Ready:\n" + games);
        choose.setEnabled(!importing);
        refresh.setEnabled(!importing && !preferences().getString("folder", "").isEmpty());
        play.setEnabled(!importing && !games.isEmpty());
        if (!importing) {
            if (play.isEnabled()) {
                play.requestFocus();
            } else {
                choose.requestFocus();
            }
        }
    }

    private void pickFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(intent, PICK_FOLDER);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_FOLDER || result != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        Uri uri = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(uri,
                    data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION);
            importFolder(uri);
        } catch (SecurityException | IllegalArgumentException error) {
            updateStatus("This folder did not grant lasting read access. Please choose another folder.");
        }
    }

    private void importFolder(Uri tree) {
        if (importing) {
            return;
        }
        importing = true;
        updateStatus("Importing game files…");
        worker.execute(() -> {
            try {
                String description = GameDataImporter.importTree(new DocumentSource(tree),
                        DocumentsContract.getTreeDocumentId(tree), storage(), name ->
                                runOnUiThread(() -> status.setText("Importing " + name + "…")));
                String oldFolder = preferences().getString("folder", "");
                preferences().edit().putString("folder", tree.toString()).commit();
                if (!oldFolder.isEmpty() && !oldFolder.equals(tree.toString())) {
                    try {
                        getContentResolver().releasePersistableUriPermission(Uri.parse(oldFolder),
                                Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    } catch (SecurityException ignored) {
                        // A provider may already have revoked the old grant.
                    }
                }
                runOnUiThread(() -> {
                    if (!isFinishing() && !isDestroyed()) {
                        importing = false;
                        updateStatus("Imported:\n" + description
                                + "\nUse Reimport after changing files in your selected folder.");
                    }
                });
            } catch (IOException | RuntimeException error) {
                if (!tree.toString().equals(preferences().getString("folder", ""))) {
                    try {
                        getContentResolver().releasePersistableUriPermission(tree,
                                Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    } catch (SecurityException ignored) {
                        // No grant remains if the provider revoked access mid-import.
                    }
                }
                runOnUiThread(() -> {
                    if (!isFinishing() && !isDestroyed()) {
                        importing = false;
                        updateStatus("Import failed: " + error.getMessage()
                                + "\nYour previous imported games are unchanged.");
                    }
                });
            }
        });
    }

    @Override
    protected void onDestroy() {
        worker.shutdownNow();
        super.onDestroy();
    }

    private final class DocumentSource implements GameDataImporter.Source {
        private final Uri tree;

        DocumentSource(Uri tree) {
            this.tree = tree;
        }

        @Override
        public List<GameDataImporter.Entry> list(String folder) throws IOException {
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, folder);
            String[] columns = {DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE};
            List<GameDataImporter.Entry> entries = new ArrayList<>();
            try (Cursor cursor = getContentResolver().query(children, columns, null, null, null)) {
                if (cursor == null) {
                    throw new IOException("Folder is unavailable. Please select it again.");
                }
                while (cursor.moveToNext()) {
                    entries.add(new GameDataImporter.Entry(cursor.getString(0), cursor.getString(1),
                            DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2))));
                    if (entries.size() > 4096) {
                        throw new IOException("Choose a game folder, rather than all storage.");
                    }
                }
            }
            return entries;
        }

        @Override
        public InputStream open(GameDataImporter.Entry entry) throws IOException {
            return getContentResolver().openInputStream(
                    DocumentsContract.buildDocumentUriUsingTree(tree, entry.id));
        }
    }
}

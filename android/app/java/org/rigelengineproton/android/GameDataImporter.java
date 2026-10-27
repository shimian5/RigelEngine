package org.rigelengineproton.android;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Copies only original DOS data into a replaceable, app-owned library. */
public final class GameDataImporter {
    private static final long MAX_FILE_BYTES = 64L * 1024 * 1024;
    private static final long MAX_TOTAL_BYTES = 256L * 1024 * 1024;
    private static final int MAX_ENTRIES = 4096;

    public static final class Entry {
        public final String id;
        public final String name;
        public final boolean directory;

        public Entry(String id, String name, boolean directory) {
            this.id = id;
            this.name = name;
            this.directory = directory;
        }
    }

    public interface Source {
        List<Entry> list(String directory) throws IOException;
        InputStream open(Entry entry) throws IOException;
    }

    public interface Progress {
        void copying(String filename);
    }

    private GameDataImporter() {}

    private static String normalized(String name) {
        return name.toUpperCase(Locale.ROOT);
    }

    static int gameForFile(String name) {
        String n = normalized(name);
        if (n.matches("DN[123]\\.EXE") || n.matches("[A-Z0-9_-]{1,32}\\.DN[123]")) {
            return 1;
        }
        if (n.equals("NUKEM2.CMP") || n.matches("NUKEM2\\.F[1-5]")) {
            return 2;
        }
        return 0;
    }

    private static List<Entry> boundedList(Source source, String directory) throws IOException {
        List<Entry> entries = source.list(directory);
        if (entries == null || entries.size() > MAX_ENTRIES) {
            throw new IOException("Folder could not be read or contains too many entries.");
        }
        return entries;
    }

    private static Map<String, Entry> collect(
            Source source, List<Entry> root, int game) throws IOException {
        List<Entry> entries = new ArrayList<>(root);
        for (Entry e : root) {
            String n = normalized(e.name);
            if (e.directory && (n.equals("DN" + game) || n.equals("DUKE" + game))) {
                entries.addAll(boundedList(source, e.id));
            }
        }
        entries.sort(Comparator.comparing(e -> normalized(e.name)));
        Map<String, Entry> files = new HashMap<>();
        for (Entry e : entries) {
            if (!e.directory && gameForFile(e.name) == game) {
                String n = normalized(e.name);
                Entry old = files.put(n, e);
                if (old != null) {
                    throw new IOException("More than one copy of " + n
                            + " was found. Choose a folder with one set of game files.");
                }
            }
        }
        return files;
    }

    private static boolean hasEpisode(Map<String, Entry> files, int episode) {
        return files.containsKey("DN" + episode + ".EXE")
                && files.containsKey("WORLDAL1.DN" + episode);
    }

    public static String describe(File library) {
        StringBuilder result = new StringBuilder();
        for (int episode = 1; episode <= 3; episode++) {
            File dn1 = new File(library, "DN1");
            if (nonempty(new File(dn1, "DN" + episode + ".EXE"))
                    && nonempty(new File(dn1, "WORLDAL1.DN" + episode))) {
                result.append("Duke Nukem I — episode ").append(episode).append('\n');
            }
        }
        if (nonempty(new File(library, "NUKEM2.CMP"))) {
            result.append("Duke Nukem II\n");
        }
        return result.toString().trim();
    }

    private static boolean nonempty(File file) {
        return file.isFile() && file.length() > 0;
    }

    private static void remove(File file) throws IOException {
        if (file.isDirectory() && !Files.isSymbolicLink(file.toPath())) {
            File[] children = file.listFiles();
            if (children == null) {
                throw new IOException("Could not read the previous import.");
            }
            for (File child : children) {
                remove(child);
            }
        }
        if (file.exists() && !file.delete()) {
            throw new IOException("Could not remove an old import file.");
        }
    }

    public static synchronized File recover(File root) throws IOException {
        File library = new File(root, "library");
        File previous = new File(root, "previous");
        if (!library.exists() && previous.exists() && !previous.renameTo(library)) {
            throw new IOException("Could not restore the previous game library.");
        }
        return library;
    }

    public static synchronized String importTree(
            Source source, String directory, File root, Progress progress) throws IOException {
        File library = recover(root);
        List<Entry> entries = boundedList(source, directory);
        Map<String, Entry> dn1 = collect(source, entries, 1);
        Map<String, Entry> dn2 = collect(source, entries, 2);
        boolean hasDn1 = hasEpisode(dn1, 1) || hasEpisode(dn1, 2) || hasEpisode(dn1, 3);
        if (!hasDn1 && !dn2.containsKey("NUKEM2.CMP")) {
            throw new IOException("No game files found. Choose a folder containing DN1.EXE "
                    + "and its .DN1 files, another Duke I episode, or NUKEM2.CMP. "
                    + "DN1/DN2 subfolders and files mixed together are both supported.");
        }

        File pending = new File(root, "pending");
        File previous = new File(root, "previous");
        if (!root.isDirectory() && !root.mkdirs()) {
            throw new IOException("Could not create game storage.");
        }
        remove(pending);
        if (!new File(pending, "DN1").mkdirs()) {
            throw new IOException("Could not create import folder.");
        }
        long total = 0;
        try {
            List<String> names = new ArrayList<>();
            names.addAll(dn1.keySet());
            names.addAll(dn2.keySet());
            names.sort(String::compareTo);
            byte[] buffer = new byte[64 * 1024];
            for (String name : names) {
                Entry entry = dn1.containsKey(name) ? dn1.get(name) : dn2.get(name);
                File target = gameForFile(name) == 1
                        ? new File(new File(pending, "DN1"), name) : new File(pending, name);
                progress.copying(name);
                long size = 0;
                try (InputStream input = source.open(entry);
                     FileOutputStream output = new FileOutputStream(target)) {
                    if (input == null) {
                        throw new IOException("Could not open " + name);
                    }
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        if (Thread.currentThread().isInterrupted()) {
                            throw new IOException("Import cancelled.");
                        }
                        size += count;
                        total += count;
                        if (size > MAX_FILE_BYTES || total > MAX_TOTAL_BYTES) {
                            throw new IOException("Selected files exceed the DOS game-data size limit.");
                        }
                        output.write(buffer, 0, count);
                    }
                    output.getFD().sync();
                }
                if (size == 0) {
                    throw new IOException(name + " is empty.");
                }
            }
            String description = describe(pending);
            if (description.isEmpty()) {
                throw new IOException("No usable game files were imported.");
            }
            remove(previous);
            if (library.exists() && !library.renameTo(previous)) {
                throw new IOException("Could not preserve the previous game library.");
            }
            if (!pending.renameTo(library)) {
                if (previous.exists() && !previous.renameTo(library)) {
                    throw new IOException("Import interrupted; restart the app to restore the previous library.");
                }
                throw new IOException("Could not finish importing game files.");
            }
            // The old library stays as a recovery copy until the next successful import.
            return description;
        } finally {
            remove(pending);
        }
    }
}

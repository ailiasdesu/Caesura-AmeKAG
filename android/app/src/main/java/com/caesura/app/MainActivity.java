package com.caesura.app;

import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Caesura (AmeKAG) Android host activity (Track M A2/A3 + R6 read glue).
 *
 * Two responsibilities beyond the SDLActivity defaults:
 *  1. Library naming: SDLActivity's default list is {"SDL3","main"}; this
 *     engine's JNI lib is libCaesuraAmeKAG.so (top-level MODULE target).
 *  2. Resource root (R6): the engine finds assets alongside scripts/ from
 *     its CWD, which does not exist inside an APK. The game bundle staged
 *     under APK assets/game/ (scripts + assets + demo/<project>) is
 *     extracted once into internal storage and passed to the engine as
 *     --resource-root <root> via getArguments() (SDLActivity forwards the
 *     returned array to SDL_main as argv). getMainFunction() stays the
 *     default "SDL_main", which the engine exports (SDL_MAIN_HANDLED is
 *     deliberately NOT defined for Android builds).
 */
public class MainActivity extends SDLActivity {
    private static final String LOG_TAG = "CaesuraAmeKAG";
    private static final String BUNDLE_PREFIX = "game/";
    private static final String ROOT_DIR_NAME = "caesura_root";
    private static final String BUNDLE_VERSION = "1";

    private String mResourceRoot;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mResourceRoot = extractGameBundle();
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "CaesuraAmeKAG" };
    }

    @Override
    protected String[] getArguments() {
        if (mResourceRoot != null) {
            // --backend opengl: bgfx default is D3D11 which cannot exist on
            // Android; the device day also passes it explicitly (probe plan R2).
            return new String[] { "--resource-root", mResourceRoot, "--backend", "opengl" };
        }
        return new String[0];
    }

    /**
     * Extracts the APK asset bundle (game/...) into internal storage so the
     * engine can use plain file I/O. A marker file short-circuits repeated
     * extraction; bump {@link #BUNDLE_VERSION} when the staged layout
     * changes. Returns the resource root, or null on failure (engine then
     * falls back to its default CWD-based resolution).
     */
    private String extractGameBundle() {
        try {
            File root = new File(getFilesDir(), ROOT_DIR_NAME);
            File marker = new File(root, ".bundle-version");
            if (marker.exists() && markerIsCurrent(marker)) {
                return root.getAbsolutePath();
            }
            if (root.exists()) {
                deleteRecursive(root);
            }
            if (!root.mkdirs() && !root.isDirectory()) {
                Log.e(LOG_TAG, "cannot create resource root " + root);
                return null;
            }
            String[] top = getAssets().list("game");
            if (top == null || top.length == 0) {
                Log.e(LOG_TAG, "no game bundle in APK assets");
                return null;
            }
            for (String t : top) {
                copyAsset("game/" + t, root);
            }
            writeMarker(marker, BUNDLE_VERSION);
            Log.i(LOG_TAG, "game bundle extracted to " + root);
            return root.getAbsolutePath();
        } catch (Exception e) {
            Log.e(LOG_TAG, "game bundle extraction failed", e);
            return null;
        }
    }

    private boolean markerIsCurrent(File marker) {
        try (InputStream in = new FileInputStream(marker)) {
            byte[] buf = new byte[64];
            int n = in.read(buf);
            return n > 0 && new String(buf, 0, n, "UTF-8").trim().equals(BUNDLE_VERSION);
        } catch (IOException e) {
            return false;
        }
    }

    private void writeMarker(File marker, String version) throws IOException {
        try (OutputStream out = new FileOutputStream(marker)) {
            out.write(version.getBytes("UTF-8"));
        }
    }

    private void deleteRecursive(File f) {
        if (f.isDirectory()) {
            File[] children = f.listFiles();
            if (children != null) {
                for (File c : children) {
                    deleteRecursive(c);
                }
            }
        }
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }

    /** Recursively copies one APK asset (file or dir) below {@code destRoot}. */
    private void copyAsset(String assetPath, File destRoot) throws IOException {
        final String relative = assetPath.startsWith(BUNDLE_PREFIX)
                ? assetPath.substring(BUNDLE_PREFIX.length()) : assetPath;
        final File dest = new File(destRoot, relative);
        final String[] children = getAssets().list(assetPath);
        if (children != null && children.length > 0) {
            if (!dest.isDirectory()) {
                //noinspection ResultOfMethodCallIgnored
                dest.mkdirs();
            }
            for (String c : children) {
                copyAsset(assetPath + "/" + c, destRoot);
            }
            return;
        }
        File parent = dest.getParentFile();
        if (parent != null && !parent.isDirectory()) {
            //noinspection ResultOfMethodCallIgnored
            parent.mkdirs();
        }
        try (InputStream in = getAssets().open(assetPath)) {
            try (OutputStream out = new FileOutputStream(dest)) {
                byte[] buf = new byte[65536];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
            }
        } catch (FileNotFoundException fnf) {
            Log.w(LOG_TAG, "asset not readable: " + assetPath);
        }
    }
}
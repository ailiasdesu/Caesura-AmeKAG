package com.caesura.app;

import org.libsdl.app.SDLActivity;

/**
 * Caesura (AmeKAG) Android host activity.
 *
 * SDLActivity's default library list is "SDL3" + "main" — this engine's
 * JNI library is named libCaesuraAmeKAG.so (Track M A2/R1), so the host
 * overrides getLibraries(). getMainFunction() stays the default "SDL_main",
 * which the engine exports (SDL_MAIN_HANDLED is deliberately NOT defined
 * for Android builds; see cmake/CaesuraModules.cmake BuildOptions).
 */
public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "CaesuraAmeKAG" };
    }
}

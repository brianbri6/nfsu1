package com.nfsu1.androidevolved;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Launcher options. GameActivity reads them when the game process starts and hands them to the native
 * runtime before the guest boots; changing one takes effect on the next PLAY.
 */
final class GameOptions {
    static final class Option {
        final String key, title, defaultValue;
        final String[] values, labels;
        Option(String key, String title, String defaultValue, String[] values, String[] labels) {
            this.key = key; this.title = title; this.defaultValue = defaultValue; this.values = values; this.labels = labels;
        }
        String label(String value) {
            for (int i = 0; i < values.length; i++) if (values[i].equals(value)) return labels[i];
            return value;
        }
    }

    static final String RESOLUTION = "resolution", FPS = "fps", WIDESCREEN = "widescreen", LATENCY = "latency",
            TEXTURES = "textures", MINIMAP = "minimap", LANGUAGE = "language", GAMEPAD = "gamepad";

    static final Option[] ALL = {
            new Option(RESOLUTION, "Internal resolution", "75",
                    new String[] {"100", "75", "50", "720"},
                    new String[] {"Native · best quality", "75% · balanced", "50% · best performance",
                            "1280×720 · 16:9"}),
            new Option(FPS, "FPS limit", "120",
                    new String[] {"30", "60", "90", "120", "0"},
                    new String[] {"30 FPS · saves battery", "60 FPS", "90 FPS", "120 FPS", "Unlimited"}),
            new Option(WIDESCREEN, "Widescreen", "on",
                    new String[] {"on", "off"},
                    new String[] {"Corrected HUD and camera", "Original 4:3 stretched"}),
            new Option(MINIMAP, "Minimap", "top",
                    new String[] {"top", "bottom"},
                    new String[] {"Top left", "Bottom left (original)"}),
            new Option(LATENCY, "CPU/GPU synchronization", "2",
                    new String[] {"2", "1"},
                    new String[] {"Performance · parallel CPU and GPU", "Low latency · lower FPS"}),
            new Option(TEXTURES, "Compressed textures", "auto",
                    new String[] {"auto", "cpu"},
                    new String[] {"Automatic · GPU if supported", "Decode on CPU · compatibility"}),
            // As keyboard: RT accelerates, LT brakes, A handbrake, B nitrous, the
            // layout players expect. The game's own joystick defaults put
            // accelerate/brake on B/A (issue #45).
            new Option(GAMEPAD, "Bluetooth/USB controller", "keys",
                    new String[] {"keys", "directinput"},
                    new String[] {"Keyboard mode · RT accelerates, LT brakes", "DirectInput joystick · in-game mapping"}),
    };

    private static final String PREFS = "nfsu1_game";

    private GameOptions() {}

    static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static Option find(String key) {
        for (Option o : ALL) if (o.key.equals(key)) return o;
        throw new IllegalArgumentException(key);
    }

    static String get(Context context, String key) {
        Option o = find(key);
        String value = prefs(context).getString(key, o.defaultValue);
        for (String allowed : o.values) if (allowed.equals(value)) return value;
        return o.defaultValue;
    }

    static void set(Context context, String key, String value) {
        prefs(context).edit().putString(key, value).apply();
    }

    static final String ONLINE_SERVER = "online_server", DEFAULT_ONLINE_SERVER = "eahub.eu";

    /** EA Nation replacement server: the game's *.ea.com hosts resolve to this name or IPv4 address. */
    static String onlineServer(Context context) {
        String value = prefs(context).getString(ONLINE_SERVER, DEFAULT_ONLINE_SERVER);
        return validServer(value) ? value : DEFAULT_ONLINE_SERVER;
    }

    static boolean validServer(String value) {
        return value != null && value.length() <= 253 && value.matches("[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*");
    }

    /** The game's language folder name (LANGUAGES/<file>.bin) chosen in the launcher. */
    static String language(Context context) {
        SharedPreferences settings = prefs(context);
        if (!settings.getBoolean("english_language_migration_v1", false)) {
            SharedPreferences.Editor editor = settings.edit().putBoolean("english_language_migration_v1", true);
            if ("Spanish".equals(settings.getString(LANGUAGE, null))) editor.putString(LANGUAGE, "English");
            editor.apply();
        }
        return settings.getString(LANGUAGE, "English");
    }

    /** Render size for the selected resolution on a display of width × height (landscape). */
    static int[] renderSize(Context context, int width, int height) {
        String value = get(context, RESOLUTION);
        if (value.equals("720")) return new int[] {1280, 720};
        float scale = Integer.parseInt(value) / 100f;
        return new int[] {Math.round(width * scale / 2f) * 2, Math.round(height * scale / 2f) * 2};
    }
}

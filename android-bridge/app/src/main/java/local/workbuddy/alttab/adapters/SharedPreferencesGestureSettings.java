package local.workbuddy.alttab.adapters;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.Objects;

import local.workbuddy.alttab.domain.GestureConfig;
import local.workbuddy.alttab.ports.GestureSettings;

public final class SharedPreferencesGestureSettings implements GestureSettings {
    private static final String PREFERENCES_NAME = "gesture-settings";
    private static final String SPEED_KEY = "three_finger_speed_percent";
    private static final String CONTINUATION_KEY = "three_finger_continuation_ms";

    interface Storage {
        int readSpeedPercent(int fallback);
        int readContinuationMs(int fallback);
        void save(int speedPercent, int continuationMs);
    }

    private final Storage storage;

    public SharedPreferencesGestureSettings(Context context) {
        this(new AndroidStorage(Objects.requireNonNull(context, "context")
                .getApplicationContext()
                .getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)));
    }

    SharedPreferencesGestureSettings(Storage storage) {
        this.storage = Objects.requireNonNull(storage, "storage");
    }

    @Override
    public GestureConfig load() {
        GestureConfig defaults = GestureConfig.defaults();
        return GestureConfig.normalized(
                storage.readSpeedPercent(defaults.speedPercent()),
                storage.readContinuationMs(defaults.continuationMs()));
    }

    @Override
    public void save(GestureConfig config) {
        GestureConfig normalized = GestureConfig.normalized(
                config.speedPercent(), config.continuationMs());
        storage.save(normalized.speedPercent(), normalized.continuationMs());
    }

    private static final class AndroidStorage implements Storage {
        private final SharedPreferences preferences;

        AndroidStorage(SharedPreferences preferences) {
            this.preferences = preferences;
        }

        @Override
        public int readSpeedPercent(int fallback) {
            return preferences.getInt(SPEED_KEY, fallback);
        }

        @Override
        public int readContinuationMs(int fallback) {
            return preferences.getInt(CONTINUATION_KEY, fallback);
        }

        @Override
        public void save(int speedPercent, int continuationMs) {
            preferences.edit()
                    .putInt(SPEED_KEY, speedPercent)
                    .putInt(CONTINUATION_KEY, continuationMs)
                    .apply();
        }
    }
}

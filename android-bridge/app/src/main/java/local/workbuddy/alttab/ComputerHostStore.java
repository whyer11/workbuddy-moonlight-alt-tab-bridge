package local.workbuddy.alttab;

import android.content.Context;
import android.content.SharedPreferences;

public final class ComputerHostStore {
    private static final String PREFERENCES = "bridge_dashboard";
    private static final String KEY_HOST = "computer_host";

    interface Storage {
        String load();
        void save(String host);
    }

    private final Storage storage;

    public ComputerHostStore(Context context) {
        this(new SharedPreferencesStorage(context.getApplicationContext()
                .getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)));
    }

    ComputerHostStore(Storage storage) {
        this.storage = storage;
    }

    public String get() {
        return normalize(storage.load());
    }

    public void set(String host) {
        storage.save(normalize(host));
    }

    private static String normalize(String host) {
        return host == null ? "" : host.trim();
    }

    private static final class SharedPreferencesStorage implements Storage {
        private final SharedPreferences preferences;

        private SharedPreferencesStorage(SharedPreferences preferences) {
            this.preferences = preferences;
        }

        @Override
        public String load() {
            return preferences.getString(KEY_HOST, "");
        }

        @Override
        public void save(String host) {
            SharedPreferences.Editor editor = preferences.edit();
            if (host.isEmpty()) {
                editor.remove(KEY_HOST);
            } else {
                editor.putString(KEY_HOST, host);
            }
            editor.apply();
        }
    }
}

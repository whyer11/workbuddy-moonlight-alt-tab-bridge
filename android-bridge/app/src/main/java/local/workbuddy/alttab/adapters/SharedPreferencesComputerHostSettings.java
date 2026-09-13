package local.workbuddy.alttab.adapters;

import android.content.Context;

import local.workbuddy.alttab.ComputerHostStore;
import local.workbuddy.alttab.ports.ComputerHostSettings;

public final class SharedPreferencesComputerHostSettings implements ComputerHostSettings {
    private final ComputerHostStore store;
    public SharedPreferencesComputerHostSettings(Context context) { store = new ComputerHostStore(context); }
    @Override public String get() { return store.get(); }
    @Override public void set(String host) { store.set(host); }
}

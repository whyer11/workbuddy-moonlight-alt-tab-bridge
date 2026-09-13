package local.workbuddy.alttab;

import android.app.Application;

import local.workbuddy.alttab.adapters.SharedPreferencesGestureSettings;

public final class BridgeApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        BridgeController.initialize(this,
                new SharedPreferencesGestureSettings(this).load());
    }
}

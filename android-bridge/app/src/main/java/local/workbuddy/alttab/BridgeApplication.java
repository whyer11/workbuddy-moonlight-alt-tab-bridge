package local.workbuddy.alttab;

import android.app.Application;

public final class BridgeApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        BridgeController.initialize(this);
    }
}

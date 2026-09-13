package local.workbuddy.alttab;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import local.workbuddy.alttab.adapters.SharedPreferencesGestureSettings;

public final class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        BridgeController.initialize(context,
                new SharedPreferencesGestureSettings(context).load());
        BridgeController.startIfAuthorized();
    }
}

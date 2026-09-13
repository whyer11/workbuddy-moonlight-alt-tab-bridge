package local.workbuddy.alttab;

import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.Parcel;
import android.util.Log;

import rikka.shizuku.Shizuku;

public final class BridgeController {
    private static final String TAG = "WorkBuddyAltTab";
    private static final int PERMISSION_REQUEST = 5742;
    private static volatile boolean initialized;
    private static volatile boolean binding;
    private static volatile boolean connected;
    private static volatile IBinder bridgeBinder;

    private static final Shizuku.UserServiceArgs USER_SERVICE_ARGS =
            new Shizuku.UserServiceArgs(new ComponentName(
                    "local.workbuddy.alttab", BridgeUserService.class.getName()))
                    .daemon(true)
                    .processNameSuffix("bridge")
                    .debuggable(false)
                    .version(8);

    private static final ServiceConnection CONNECTION = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            bridgeBinder = service;
            binding = false;
            connected = true;
            Log.i(TAG, "Shizuku user service connected");
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            bridgeBinder = null;
            binding = false;
            connected = false;
            Log.w(TAG, "Shizuku user service disconnected");
        }
    };

    private BridgeController() {
    }

    public static synchronized void initialize(Context context) {
        if (initialized) {
            return;
        }
        initialized = true;
        Shizuku.addBinderReceivedListenerSticky(BridgeController::startIfAuthorized);
        Shizuku.addBinderDeadListener(() -> {
            bridgeBinder = null;
            binding = false;
            connected = false;
        });
        Shizuku.addRequestPermissionResultListener((requestCode, grantResult) -> {
            if (requestCode == PERMISSION_REQUEST
                    && grantResult == PackageManager.PERMISSION_GRANTED) {
                startBridge();
            }
        });
        startIfAuthorized();
    }

    public static void requestPermissionOrStart() {
        if (!Shizuku.pingBinder()) {
            Log.w(TAG, "Shizuku is not running");
            return;
        }
        if (Shizuku.checkSelfPermission() == PackageManager.PERMISSION_GRANTED) {
            startBridge();
        } else if (!Shizuku.shouldShowRequestPermissionRationale()) {
            Shizuku.requestPermission(PERMISSION_REQUEST);
        }
    }

    public static void startIfAuthorized() {
        try {
            if (Shizuku.pingBinder()
                    && Shizuku.checkSelfPermission() == PackageManager.PERMISSION_GRANTED) {
                startBridge();
            }
        } catch (Throwable error) {
            Log.w(TAG, "Cannot start bridge yet", error);
        }
    }

    private static synchronized void startBridge() {
        if (binding || connected) {
            return;
        }
        binding = true;
        try {
            Shizuku.bindUserService(USER_SERVICE_ARGS, CONNECTION);
        } catch (Throwable error) {
            binding = false;
            Log.e(TAG, "Failed to bind Shizuku user service", error);
        }
    }

    public static String statusText() {
        if (!Shizuku.pingBinder()) {
            return "状态：Shizuku 未运行";
        }
        try {
            if (Shizuku.checkSelfPermission() != PackageManager.PERMISSION_GRANTED) {
                return "状态：等待 Shizuku 授权";
            }
        } catch (Throwable error) {
            return "状态：正在连接 Shizuku";
        }
        if (connected) {
            return "状态：底层桥接已运行";
        }
        if (binding) {
            return "状态：正在启动底层桥接";
        }
        return "状态：已授权，等待启动";
    }

    public static PadBridgeSnapshot padSnapshot() {
        IBinder binder = bridgeBinder;
        if (!connected || binder == null || !binder.isBinderAlive()) {
            return PadBridgeSnapshot.offline();
        }

        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        try {
            if (!binder.transact(BridgeServiceProtocol.TRANSACTION_STATUS, data, reply, 0)) {
                return PadBridgeSnapshot.offline();
            }
            reply.readException();
            return BridgeServiceProtocol.readStatus(reply);
        } catch (Throwable error) {
            Log.w(TAG, "Cannot read native bridge status", error);
            return PadBridgeSnapshot.offline();
        } finally {
            reply.recycle();
            data.recycle();
        }
    }
}

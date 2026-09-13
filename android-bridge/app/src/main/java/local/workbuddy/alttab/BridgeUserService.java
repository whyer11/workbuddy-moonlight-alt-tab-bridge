package local.workbuddy.alttab;

import android.content.Context;
import android.os.Binder;
import android.os.Parcel;
import android.os.RemoteException;
import android.util.Log;

import local.workbuddy.alttab.nativebridge.AndroidNativeBackend;
import local.workbuddy.alttab.nativebridge.NativeBridgeInstaller;
import local.workbuddy.alttab.nativebridge.NativeBridgeSupervisor;
import local.workbuddy.alttab.shizuku.BridgeBinderProtocolV2;

public final class BridgeUserService extends Binder {
    private static final String TAG = "WorkBuddyAltTab";
    private static final int SHIZUKU_USER_SERVICE_DESTROY = 16777115;
    private final NativeBridgeSupervisor supervisor;

    public BridgeUserService() {
        supervisor = new NativeBridgeSupervisor(AndroidNativeBackend.TARGET_PATH);
    }

    public BridgeUserService(Context context) {
        NativeBridgeInstaller.InstallResult result = new NativeBridgeInstaller(
                new AndroidNativeBackend(context)).install();
        Log.i(TAG, "Native deployment result=" + result);
        cleanUpOldBridge();
        supervisor = new NativeBridgeSupervisor(AndroidNativeBackend.TARGET_PATH);
    }

    private static void cleanUpOldBridge() {
        try {
            Process process = new ProcessBuilder("/system/bin/pkill", "-f",
                    AndroidNativeBackend.TARGET_PATH).start();
            process.waitFor();
            Thread.sleep(250);
        } catch (Throwable error) {
            Log.w(TAG, "Old bridge cleanup failed", error);
        }
    }

    public void destroy() {
        supervisor.close();
        System.exit(0);
    }

    @Override protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
            throws RemoteException {
        if (code == BridgeBinderProtocolV2.TRANSACTION_STATUS) {
            data.enforceInterface(BridgeBinderProtocolV2.DESCRIPTOR);
            reply.writeNoException();
            BridgeBinderProtocolV2.writeStatus(reply, supervisor.snapshot(), supervisor.active());
            return true;
        }
        if (code == BridgeBinderProtocolV2.TRANSACTION_SET_ACTIVE) {
            data.enforceInterface(BridgeBinderProtocolV2.DESCRIPTOR);
            supervisor.setActive(data.readInt() != 0);
            reply.writeNoException();
            return true;
        }
        if (code == SHIZUKU_USER_SERVICE_DESTROY) {
            destroy();
            return true;
        }
        return super.onTransact(code, data, reply, flags);
    }
}

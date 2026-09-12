package local.workbuddy.alttab;

import android.content.Context;
import android.os.Binder;
import android.os.Parcel;
import android.os.RemoteException;
import android.system.Os;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;

public final class BridgeUserService extends Binder {
    private static final String TAG = "WorkBuddyAltTab";
    private static final String BRIDGE_PATH = "/data/local/tmp/alttab_evdev_bridge";
    private static final int SHIZUKU_USER_SERVICE_DESTROY = 16777115;
    private volatile boolean destroyed;
    private volatile Process bridgeProcess;

    public BridgeUserService() {
        startSupervisor();
    }

    public BridgeUserService(Context context) {
        deployBundledBridge(context);
        startSupervisor();
    }

    private void deployBundledBridge(Context context) {
        File staged = new File(BRIDGE_PATH + ".new");
        File target = new File(BRIDGE_PATH);
        try (InputStream input = context.getAssets().open("alttab_evdev_bridge");
             FileOutputStream output = new FileOutputStream(staged, false)) {
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
            output.getFD().sync();
            Os.chmod(staged.getAbsolutePath(), 0700);
            Os.rename(staged.getAbsolutePath(), target.getAbsolutePath());
            Log.i(TAG, "Bundled native bridge deployed");
        } catch (Throwable error) {
            Log.e(TAG, "Cannot deploy bundled native bridge", error);
        } finally {
            if (staged.exists() && !staged.delete()) {
                Log.w(TAG, "Cannot remove staged native bridge");
            }
        }
    }

    private void startSupervisor() {
        Thread supervisor = new Thread(new Runnable() {
            @Override
            public void run() {
                cleanUpOldBridge();
                while (!destroyed) {
                    runBridgeOnce();
                    if (!destroyed) {
                        try {
                            Thread.sleep(1000);
                        } catch (InterruptedException ignored) {
                            Thread.currentThread().interrupt();
                        }
                    }
                }
            }
        }, "WorkBuddyBridgeSupervisor");
        supervisor.setDaemon(true);
        supervisor.start();
    }

    private void cleanUpOldBridge() {
        try {
            Process oldBridge = new ProcessBuilder(
                    "/system/bin/pkill", "-f", BRIDGE_PATH)
                    .start();
            oldBridge.waitFor();
            Thread.sleep(250);
        } catch (Throwable error) {
            Log.w(TAG, "Old bridge cleanup failed", error);
        }
    }

    private void runBridgeOnce() {
        try {
            Process process = new ProcessBuilder(BRIDGE_PATH)
                    .redirectErrorStream(true)
                    .start();
            bridgeProcess = process;
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(process.getInputStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    Log.i(TAG, "native: " + line);
                }
            }
            int exitCode = process.waitFor();
            bridgeProcess = null;
            if (!destroyed) {
                Log.w(TAG, "Native bridge exited with " + exitCode + ", restarting");
            }
        } catch (Throwable error) {
            bridgeProcess = null;
            if (!destroyed) {
                Log.e(TAG, "Native bridge supervisor error", error);
            }
        }
    }

    public void destroy() {
        destroyed = true;
        Process process = bridgeProcess;
        if (process != null) {
            process.destroy();
        }
        System.exit(0);
    }

    @Override
    protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
            throws RemoteException {
        if (code == BridgeServiceProtocol.TRANSACTION_STATUS) {
            reply.writeNoException();
            BridgeServiceProtocol.writeStatus(reply, isNativeBridgeAlive());
            return true;
        }
        if (code == SHIZUKU_USER_SERVICE_DESTROY) {
            destroy();
            return true;
        }
        return super.onTransact(code, data, reply, flags);
    }

    private boolean isNativeBridgeAlive() {
        Process process = bridgeProcess;
        return process != null && process.isAlive();
    }
}

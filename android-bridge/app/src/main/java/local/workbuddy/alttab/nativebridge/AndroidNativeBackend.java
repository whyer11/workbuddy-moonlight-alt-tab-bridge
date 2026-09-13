package local.workbuddy.alttab.nativebridge;

import android.content.Context;
import android.system.Os;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.security.MessageDigest;
import java.util.Arrays;

public final class AndroidNativeBackend implements NativeBridgeInstaller.Backend {
    public static final String TARGET_PATH = "/data/local/tmp/alttab_evdev_bridge";
    private final Context context;
    private final File target = new File(TARGET_PATH);
    private final File staged = new File(TARGET_PATH + ".new");
    private final File previous = new File(TARGET_PATH + ".previous");

    public AndroidNativeBackend(Context context) { this.context = context; }

    @Override public void stage() throws Exception {
        try (InputStream input = context.getAssets().open("alttab_evdev_bridge");
             FileOutputStream output = new FileOutputStream(staged, false)) {
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
            output.getFD().sync();
        }
        Os.chmod(staged.getAbsolutePath(), 0700);
    }

    @Override public boolean validateStage() throws Exception {
        Process process = new ProcessBuilder(staged.getAbsolutePath(), "--describe")
                .redirectErrorStream(true).start();
        String line;
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            line = reader.readLine();
        }
        return process.waitFor() == 0 && NativeCapabilityParser.parse(line).ready();
    }

    @Override public boolean isCurrent() throws Exception {
        return target.isFile() && Arrays.equals(sha256(staged), sha256(target));
    }

    @Override public void backupCurrent() throws Exception {
        if (previous.exists() && !previous.delete()) throw new IllegalStateException("previous-delete");
        if (target.exists()) Os.rename(target.getAbsolutePath(), previous.getAbsolutePath());
    }

    @Override public void promote() throws Exception {
        Os.rename(staged.getAbsolutePath(), target.getAbsolutePath());
        Os.chmod(target.getAbsolutePath(), 0700);
    }

    @Override public boolean awaitReady() throws Exception {
        Process process = new ProcessBuilder(target.getAbsolutePath(), "--describe")
                .redirectErrorStream(true).start();
        String line;
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            line = reader.readLine();
        }
        return process.waitFor() == 0 && NativeCapabilityParser.parse(line).ready();
    }

    @Override public void restorePrevious() throws Exception {
        if (target.exists() && !target.delete()) throw new IllegalStateException("target-delete");
        if (previous.exists()) Os.rename(previous.getAbsolutePath(), target.getAbsolutePath());
    }

    @Override public void cleanupStage() {
        if (staged.exists()) staged.delete();
    }

    private static byte[] sha256(File file) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (FileInputStream input = new FileInputStream(file)) {
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) != -1) digest.update(buffer, 0, count);
        }
        return digest.digest();
    }
}

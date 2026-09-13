package local.workbuddy.alttab.nativebridge;

public final class NativeBridgeInstaller {
    public enum InstallResult { INSTALLED, ALREADY_CURRENT, ROLLED_BACK, FAILED }

    public interface Backend {
        void stage() throws Exception;
        boolean validateStage() throws Exception;
        boolean isCurrent() throws Exception;
        void backupCurrent() throws Exception;
        void promote() throws Exception;
        boolean awaitReady() throws Exception;
        void restorePrevious() throws Exception;
        void cleanupStage() throws Exception;
    }

    private final Backend backend;
    public NativeBridgeInstaller(Backend backend) { this.backend = backend; }

    public InstallResult install() {
        boolean replacementStarted = false;
        try {
            backend.stage();
            if (!backend.validateStage()) return InstallResult.FAILED;
            if (backend.isCurrent()) return InstallResult.ALREADY_CURRENT;
            backend.backupCurrent();
            replacementStarted = true;
            backend.promote();
            if (backend.awaitReady()) return InstallResult.INSTALLED;
            backend.restorePrevious();
            return InstallResult.ROLLED_BACK;
        } catch (Exception error) {
            if (replacementStarted) {
                try { backend.restorePrevious(); return InstallResult.ROLLED_BACK; }
                catch (Exception ignored) { }
            }
            return InstallResult.FAILED;
        } finally {
            try { backend.cleanupStage(); } catch (Exception ignored) { }
        }
    }
}

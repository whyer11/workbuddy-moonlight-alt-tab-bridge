package local.workbuddy.alttab.nativebridge;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public final class NativeBridgeInstallerTest {
    @Test
    public void validStageIsPromotedOnlyAfterDescribeAndReady() throws Exception {
        FakeBackend backend = new FakeBackend(true, true, true);
        assertEquals(NativeBridgeInstaller.InstallResult.INSTALLED,
                new NativeBridgeInstaller(backend).install());
        assertEquals("stage,validate,backup,promote,ready,cleanup", backend.events.toString());
    }

    @Test
    public void failedReadyRestoresPreviousBinary() throws Exception {
        FakeBackend backend = new FakeBackend(true, true, false);
        assertEquals(NativeBridgeInstaller.InstallResult.ROLLED_BACK,
                new NativeBridgeInstaller(backend).install());
        assertEquals("stage,validate,backup,promote,ready,restore,cleanup", backend.events.toString());
    }

    @Test
    public void badHashNeverReplacesCurrentBinary() throws Exception {
        FakeBackend backend = new FakeBackend(false, true, true);
        assertEquals(NativeBridgeInstaller.InstallResult.FAILED,
                new NativeBridgeInstaller(backend).install());
        assertEquals("stage,validate,cleanup", backend.events.toString());
    }

    @Test
    public void promoteFailureRestoresTheBackedUpBinary() {
        FakeBackend backend = new FakeBackend(true, true, true);
        backend.failPromote = true;
        assertEquals(NativeBridgeInstaller.InstallResult.ROLLED_BACK,
                new NativeBridgeInstaller(backend).install());
        assertEquals("stage,validate,backup,promote,restore,cleanup", backend.events.toString());
    }

    private static final class FakeBackend implements NativeBridgeInstaller.Backend {
        final StringBuilder events = new StringBuilder();
        final boolean hash;
        final boolean describe;
        final boolean ready;
        boolean failPromote;
        FakeBackend(boolean hash, boolean describe, boolean ready) {
            this.hash = hash; this.describe = describe; this.ready = ready;
        }
        private void add(String value) { if (events.length() > 0) events.append(','); events.append(value); }
        public void stage() { add("stage"); }
        public boolean validateStage() { add("validate"); return hash && describe; }
        public boolean isCurrent() { return false; }
        public void backupCurrent() { add("backup"); }
        public void promote() throws Exception { add("promote"); if (failPromote) throw new Exception("promote"); }
        public boolean awaitReady() { add("ready"); return ready; }
        public void restorePrevious() { add("restore"); }
        public void cleanupStage() { add("cleanup"); }
    }
}

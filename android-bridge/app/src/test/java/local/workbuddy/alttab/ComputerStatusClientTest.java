package local.workbuddy.alttab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;

import org.junit.Test;

public final class ComputerStatusClientTest {
    private static final String NONCE = "00112233445566778899aabbccddeeff";

    @Test
    public void validResponseReportsHostAndSupportedRules() throws Exception {
        try (FakeResponder responder = new FakeResponder(
                "WB_STATUS_V2 nonce=" + NONCE
                        + " host=0.7.0 protocol=2 core=ready rules=alt_tab,windows_key", false)) {
            ComputerStatus status = client(responder.port()).probe("127.0.0.1", 500);

            assertTrue(status.reachable());
            assertEquals("0.7.0", status.hostVersion());
            assertTrue(status.ruleIds().contains("alt_tab"));
            assertTrue(status.ruleIds().contains("windows_key"));
            responder.assertSucceeded();
        }
    }

    @Test
    public void wrongNonceOrProtocolIsRejected() throws Exception {
        try (FakeResponder responder = new FakeResponder(
                "WB_STATUS_V2 nonce=ffffffffffffffffffffffffffffffff"
                        + " host=0.7.0 protocol=2 core=ready rules=alt_tab,windows_key", false)) {
            assertFalse(client(responder.port()).probe("127.0.0.1", 500).reachable());
            responder.assertSucceeded();
        }

        try (FakeResponder responder = new FakeResponder(
                "WB_STATUS_V1 " + NONCE
                        + " host=0.6.0 rules=alt_tab,windows_key", false)) {
            assertFalse(client(responder.port()).probe("127.0.0.1", 500).reachable());
            responder.assertSucceeded();
        }
    }

    @Test
    public void responseFromUnexpectedSourcePortIsRejected() throws Exception {
        try (FakeResponder responder = new FakeResponder(
                "WB_STATUS_V2 nonce=" + NONCE
                        + " host=0.7.0 protocol=2 core=ready rules=alt_tab,windows_key", true)) {
            assertFalse(client(responder.port()).probe("127.0.0.1", 500).reachable());
            responder.assertSucceeded();
        }
    }

    @Test
    public void emptyHostAndClosedPortDegradeToOffline() throws Exception {
        assertFalse(client(57420).probe("  ", 100).reachable());

        int closedPort;
        try (DatagramSocket temporary = new DatagramSocket(0, InetAddress.getLoopbackAddress())) {
            closedPort = temporary.getLocalPort();
        }
        assertFalse(client(closedPort).probe("127.0.0.1", 100).reachable());
    }

    private static ComputerStatusClient client(int port) {
        return new ComputerStatusClient(port, () -> NONCE);
    }

    private static final class FakeResponder implements AutoCloseable {
        private final DatagramSocket socket;
        private final Thread thread;
        private final AtomicReference<Throwable> failure = new AtomicReference<>();
        private final String response;
        private final boolean alternateSourcePort;

        FakeResponder(String response, boolean alternateSourcePort) throws Exception {
            this.response = response;
            this.alternateSourcePort = alternateSourcePort;
            socket = new DatagramSocket(0, InetAddress.getLoopbackAddress());
            thread = new Thread(this::run, "ComputerStatusClientTestResponder");
            thread.start();
        }

        int port() {
            return socket.getLocalPort();
        }

        private void run() {
            try {
                byte[] requestBytes = new byte[128];
                DatagramPacket request = new DatagramPacket(requestBytes, requestBytes.length);
                socket.receive(request);
                String requestText = new String(
                        request.getData(), request.getOffset(), request.getLength(),
                        StandardCharsets.UTF_8);
                if (!requestText.equals("WB_STATUS_V2 nonce=" + NONCE)) {
                    throw new AssertionError("Unexpected request: " + requestText);
                }

                byte[] responseBytes = response.getBytes(StandardCharsets.UTF_8);
                DatagramPacket reply = new DatagramPacket(
                        responseBytes, responseBytes.length,
                        request.getAddress(), request.getPort());
                if (alternateSourcePort) {
                    try (DatagramSocket alternate = new DatagramSocket(
                            0, InetAddress.getLoopbackAddress())) {
                        alternate.send(reply);
                    }
                } else {
                    socket.send(reply);
                }
            } catch (Throwable error) {
                if (!socket.isClosed()) {
                    failure.set(error);
                }
            }
        }

        void assertSucceeded() throws Exception {
            thread.join(1000);
            if (thread.isAlive()) {
                throw new AssertionError("Responder did not finish");
            }
            if (failure.get() != null) {
                throw new AssertionError("Responder failed", failure.get());
            }
        }

        @Override
        public void close() throws Exception {
            socket.close();
            thread.join(1000);
        }
    }
}

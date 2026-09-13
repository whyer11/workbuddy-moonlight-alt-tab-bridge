package local.workbuddy.alttab;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.LinkedHashSet;
import java.util.Set;

import local.workbuddy.alttab.generated.GeneratedBridgeSchema;

public final class ComputerStatusClient {
    static final int DEFAULT_PORT = GeneratedBridgeSchema.STATUS_PORT;
    private static final String PROTOCOL = "WB_STATUS_V2";
    private static final int NONCE_BYTES = GeneratedBridgeSchema.NONCE_HEX_LENGTH / 2;

    interface NonceSource {
        String nextNonce();
    }

    private final int port;
    private final NonceSource nonceSource;

    public ComputerStatusClient() {
        SecureRandom random = new SecureRandom();
        port = DEFAULT_PORT;
        nonceSource = () -> createNonce(random);
    }

    ComputerStatusClient(int port, NonceSource nonceSource) {
        this.port = port;
        this.nonceSource = nonceSource;
    }

    public ComputerStatus probe(String host, int timeoutMillis) {
        String normalizedHost = host == null ? "" : host.trim();
        if (normalizedHost.isEmpty() || timeoutMillis <= 0) {
            return ComputerStatus.offline();
        }

        try {
            InetAddress target = InetAddress.getByName(normalizedHost);
            String nonce = nonceSource.nextNonce();
            if (nonce == null || nonce.length() != NONCE_BYTES * 2) {
                return ComputerStatus.offline();
            }
            byte[] requestBytes = (PROTOCOL + " nonce=" + nonce)
                    .getBytes(StandardCharsets.UTF_8);

            try (DatagramSocket socket = new DatagramSocket()) {
                socket.setSoTimeout(timeoutMillis);
                DatagramPacket request = new DatagramPacket(
                        requestBytes, requestBytes.length, target, port);
                socket.send(request);

                byte[] responseBytes = new byte[GeneratedBridgeSchema.MAX_DATAGRAM_BYTES + 1];
                DatagramPacket response = new DatagramPacket(
                        responseBytes, responseBytes.length);
                socket.receive(response);
                if (!target.equals(response.getAddress()) || response.getPort() != port) {
                    return ComputerStatus.offline();
                }

                String responseText = new String(
                        response.getData(), response.getOffset(), response.getLength(),
                        StandardCharsets.UTF_8);
                return parseResponse(responseText, nonce);
            }
        } catch (Exception ignored) {
            return ComputerStatus.offline();
        }
    }

    private static ComputerStatus parseResponse(String response, String expectedNonce) {
        String[] fields = response.split(" ", -1);
        if (fields.length != 6
                || !PROTOCOL.equals(fields[0])
                || !fields[1].equals("nonce=" + expectedNonce)
                || !fields[2].startsWith("host=")
                || !fields[3].equals("protocol=" + GeneratedBridgeSchema.PROTOCOL_VERSION)
                || !fields[4].equals("core=ready")
                || !fields[5].startsWith("rules=")) {
            return ComputerStatus.offline();
        }

        String hostVersion = fields[2].substring("host=".length());
        String rulesText = fields[5].substring("rules=".length());
        if (hostVersion.isEmpty() || rulesText.isEmpty()) {
            return ComputerStatus.offline();
        }

        Set<String> ruleIds = new LinkedHashSet<>();
        for (String ruleId : rulesText.split(",", -1)) {
            if (!ruleId.matches("[a-z0-9_]+")) {
                return ComputerStatus.offline();
            }
            ruleIds.add(ruleId);
        }
        return ComputerStatus.online(hostVersion, ruleIds);
    }

    private static String createNonce(SecureRandom random) {
        byte[] bytes = new byte[NONCE_BYTES];
        random.nextBytes(bytes);
        StringBuilder result = new StringBuilder(NONCE_BYTES * 2);
        for (byte value : bytes) {
            result.append(String.format("%02x", value & 0xff));
        }
        return result.toString();
    }
}

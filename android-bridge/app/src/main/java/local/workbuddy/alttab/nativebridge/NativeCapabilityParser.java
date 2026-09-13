package local.workbuddy.alttab.nativebridge;

import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

import local.workbuddy.alttab.generated.GeneratedBridgeSchema;

public final class NativeCapabilityParser {
    private NativeCapabilityParser() {}

    public static NativeBridgeStatus parse(String line) {
        if (line == null) return NativeBridgeStatus.error("empty");
        String[] tokens = line.trim().split(" ", -1);
        if (tokens.length < 4 || !(tokens[0].equals("WB_NATIVE_V2")
                || tokens[0].equals("WB_NATIVE_READY_V2"))) {
            return NativeBridgeStatus.error("prefix");
        }
        boolean readyLine = tokens[0].equals("WB_NATIVE_READY_V2");
        Map<String, String> fields = new LinkedHashMap<>();
        for (int i = 1; i < tokens.length; i++) {
            int separator = tokens[i].indexOf('=');
            if (separator <= 0 || separator == tokens[i].length() - 1) {
                return NativeBridgeStatus.error("field");
            }
            String key = tokens[i].substring(0, separator);
            String value = tokens[i].substring(separator + 1);
            if (fields.put(key, value) != null) return NativeBridgeStatus.error("duplicate");
        }
        int expected = readyLine ? 4 : 3;
        if (fields.size() != expected || !fields.containsKey("version")
                || !fields.containsKey("protocol") || !fields.containsKey("rules")
                || (readyLine != fields.containsKey("physical"))) {
            return NativeBridgeStatus.error("fields");
        }
        int protocol;
        try { protocol = Integer.parseInt(fields.get("protocol")); }
        catch (NumberFormatException error) { return NativeBridgeStatus.error("protocol"); }
        if (protocol != GeneratedBridgeSchema.PROTOCOL_VERSION
                || !GeneratedBridgeSchema.PRODUCT_VERSION.equals(fields.get("version"))) {
            return NativeBridgeStatus.error("incompatible");
        }
        LinkedHashSet<String> rules = new LinkedHashSet<>();
        for (String id : fields.get("rules").split(",", -1)) {
            if (!id.matches("[a-z0-9_]+") || !rules.add(id)) {
                return NativeBridgeStatus.error("rules");
            }
        }
        String physical = fields.getOrDefault("physical", "");
        if (readyLine && !physical.matches("/dev/input/event[0-9]+")) {
            return NativeBridgeStatus.error("physical");
        }
        return NativeBridgeStatus.ready(fields.get("version"), protocol, rules, physical);
    }
}

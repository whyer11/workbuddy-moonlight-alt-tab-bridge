package local.workbuddy.alttab;

import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;

public final class ComputerStatus {
    private static final ComputerStatus OFFLINE = new ComputerStatus(
            false, "", Collections.emptySet());

    private final boolean reachable;
    private final String hostVersion;
    private final Set<String> ruleIds;

    private ComputerStatus(boolean reachable, String hostVersion, Set<String> ruleIds) {
        this.reachable = reachable;
        this.hostVersion = hostVersion;
        this.ruleIds = Collections.unmodifiableSet(new LinkedHashSet<>(ruleIds));
    }

    public static ComputerStatus online(String hostVersion, Set<String> ruleIds) {
        return new ComputerStatus(true, hostVersion, ruleIds);
    }

    public static ComputerStatus offline() {
        return OFFLINE;
    }

    public boolean reachable() {
        return reachable;
    }

    public String hostVersion() {
        return hostVersion;
    }

    public Set<String> ruleIds() {
        return ruleIds;
    }
}

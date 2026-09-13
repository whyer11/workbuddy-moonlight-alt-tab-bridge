package local.workbuddy.alttab.shizuku;

import android.os.IBinder;
import android.os.Parcel;

import java.util.ArrayList;
import java.util.LinkedHashSet;

import local.workbuddy.alttab.nativebridge.NativeBridgeStatus;

public final class BridgeBinderProtocolV2 {
    public static final String DESCRIPTOR = "local.workbuddy.alttab.bridge.v2";
    public static final int TRANSACTION_STATUS = IBinder.FIRST_CALL_TRANSACTION;
    public static final int TRANSACTION_SET_ACTIVE = IBinder.FIRST_CALL_TRANSACTION + 1;

    private BridgeBinderProtocolV2() {}

    public static void writeStatus(Parcel reply, NativeBridgeStatus status, boolean active) {
        reply.writeInt(status.ready() ? 1 : 0);
        reply.writeString(status.version());
        reply.writeInt(status.protocol());
        reply.writeStringList(new ArrayList<>(status.ruleIds()));
        reply.writeString(status.physicalPath());
        reply.writeString(status.errorCode());
        reply.writeInt(active ? 1 : 0);
    }

    public static Snapshot readStatus(Parcel reply) {
        boolean ready = reply.readInt() != 0;
        String version = reply.readString();
        int protocol = reply.readInt();
        ArrayList<String> rules = reply.createStringArrayList();
        String physical = reply.readString();
        String error = reply.readString();
        boolean active = reply.readInt() != 0;
        return new Snapshot(ready, version, protocol,
                new LinkedHashSet<>(rules == null ? new ArrayList<>() : rules),
                physical, error, active);
    }

    public static final class Snapshot {
        public final boolean ready;
        public final String version;
        public final int protocol;
        public final LinkedHashSet<String> ruleIds;
        public final String physicalPath;
        public final String errorCode;
        public final boolean active;
        Snapshot(boolean ready, String version, int protocol, LinkedHashSet<String> ruleIds,
                 String physicalPath, String errorCode, boolean active) {
            this.ready = ready; this.version = version; this.protocol = protocol;
            this.ruleIds = ruleIds; this.physicalPath = physicalPath;
            this.errorCode = errorCode; this.active = active;
        }
    }
}

package local.workbuddy.alttab;

import android.os.IBinder;
import android.os.Parcel;

import java.util.ArrayList;

final class BridgeServiceProtocol {
    static final int VERSION = 1;
    static final int TRANSACTION_STATUS = IBinder.FIRST_CALL_TRANSACTION;

    private BridgeServiceProtocol() {
    }

    static void writeStatus(Parcel reply, boolean nativeAlive) {
        ArrayList<String> ruleIds = new ArrayList<>();
        for (BridgeRule rule : BridgeRules.all()) {
            ruleIds.add(rule.id());
        }
        reply.writeInt(nativeAlive ? 1 : 0);
        reply.writeInt(VERSION);
        reply.writeStringList(ruleIds);
    }

    static PadBridgeSnapshot readStatus(Parcel reply) {
        boolean nativeAlive = reply.readInt() != 0;
        int version = reply.readInt();
        ArrayList<String> ruleIds = reply.createStringArrayList();
        return PadBridgeSnapshot.from(true, nativeAlive, version, ruleIds);
    }
}

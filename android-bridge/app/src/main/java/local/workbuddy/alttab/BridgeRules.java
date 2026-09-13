package local.workbuddy.alttab;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import local.workbuddy.alttab.generated.GeneratedBridgeSchema;

public final class BridgeRules {
    public static final String ALT_TAB = "alt_tab";
    public static final String WINDOWS_KEY = "windows_key";
    public static final String ARROW_KEYS = "arrow_keys";

    private static final List<BridgeRule> RULES = createRules();

    private BridgeRules() {
    }

    public static List<BridgeRule> all() {
        return RULES;
    }

    private static List<BridgeRule> createRules() {
        ArrayList<BridgeRule> result = new ArrayList<>();
        for (GeneratedBridgeSchema.RuleDefinition definition : GeneratedBridgeSchema.RULES) {
            result.add(new BridgeRule(definition.id, definition.title, definition.padInput,
                    definition.padOutput, definition.windowsAction, definition.windows));
        }
        return Collections.unmodifiableList(result);
    }
}

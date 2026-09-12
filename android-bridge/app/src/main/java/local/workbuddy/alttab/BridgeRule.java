package local.workbuddy.alttab;

public final class BridgeRule {
    private final String id;
    private final String title;
    private final String padInput;
    private final String padOutput;
    private final String windowsAction;
    private final boolean requiresComputer;

    public BridgeRule(String id, String title, String padInput, String padOutput,
                      String windowsAction, boolean requiresComputer) {
        this.id = id;
        this.title = title;
        this.padInput = padInput;
        this.padOutput = padOutput;
        this.windowsAction = windowsAction;
        this.requiresComputer = requiresComputer;
    }

    public String id() {
        return id;
    }

    public String title() {
        return title;
    }

    public String padInput() {
        return padInput;
    }

    public String padOutput() {
        return padOutput;
    }

    public String windowsAction() {
        return windowsAction;
    }

    public boolean requiresComputer() {
        return requiresComputer;
    }
}

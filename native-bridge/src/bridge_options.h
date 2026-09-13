#ifndef WB_BRIDGE_OPTIONS_H
#define WB_BRIDGE_OPTIONS_H

struct wb_bridge_options {
    const char *keyboard_path;
    int three_finger_speed_percent;
    int three_finger_continuation_ms;
};

void wb_bridge_options_init(struct wb_bridge_options *options);
int wb_bridge_options_parse(struct wb_bridge_options *options,
                            int argc,
                            char **argv);

#endif

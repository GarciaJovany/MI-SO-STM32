#include "kernel/os.h"

static os_mq_t button_events;

static void t_blink(void) {
    for (;;) {
        led_set(0,true);
        os_sleep(300);
        led_set(0,false);
        os_sleep(300);
    }
}
static void t_button(void) {
    uint8_t previous = 1;
    for (;;) {
        uint8_t value = (uint8_t)button_read();
        if (value == 0 && previous == 1) mq_send(&button_events, 1);
        previous = value;
        os_sleep(20);
    }
}
static void t_action(void) {
    uint8_t event;
    for (;;) {
        mq_recv(&button_events,&event);
        if (event == 1) {
            led_set(1,true);
            os_sleep(180);
            led_set(1,false);
            os_sleep(100); /* separa destellos de eventos encolados */
        }
    }
}
void app_boot(void) {
    mq_init(&button_events,8);
    shell_boot();
    prodcons_boot();
    os_create_task("blink",t_blink,4);
    os_create_task("button",t_button,4);
    os_create_task("action",t_action,4);
}

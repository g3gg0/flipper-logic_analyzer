

#include "logic_analyzer_app.h"
#include "logic_analyzer_icons.h"

#define COUNT(x) (sizeof(x) / sizeof((x)[0]))

static void render_callback(Canvas* const canvas, void* cb_ctx);

static const GpioPin* gpios[] = {
    &gpio_ext_pc0,
    &gpio_ext_pc1,
    &gpio_ext_pc3,
    &gpio_ext_pb2,
    &gpio_ext_pb3,
    &gpio_ext_pa4,
    &gpio_ext_pa6,
    &gpio_ext_pa7};

static const char* gpio_names[] = {"PC0", "PC1", "PC3", "PB2", "PB3", "PA4", "PA6", "PA7"};

const NotificationSequence seq_c_minor = {
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,

    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,

    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

const NotificationSequence seq_error = {

    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,

    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_10,

    &message_note_c4,
    &message_delay_500,
    &message_sound_off,
    &message_delay_10,
    NULL,
};

const NotificationSequence* seq_sounds[] = {&seq_c_minor, &seq_error};

static void render_callback(Canvas* const canvas, void* cb_ctx) {
    AppFSM* ctx = acquire_mutex((ValueMutex*)cb_ctx, 25);

    if(ctx == NULL || !ctx->processing) {
        return;
    }

    char buffer[64];
    int y = 10;

    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_str_aligned(canvas, 5, y, AlignLeft, AlignBottom, "State");
    y += 10;

    if(ctx->uart) {
        UsbUartState st;
        usb_uart_get_state(ctx->uart, &st);

        snprintf(buffer, sizeof(buffer), "Rx %ld / Tx %ld", st.rx_cnt, st.tx_cnt);
        canvas_draw_str_aligned(canvas, 5, y, AlignLeft, AlignBottom, buffer);
        y += 20;
    }
    canvas_set_font(canvas, FontSecondary);

    if(ctx->sump) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%c%02X %lX %ld %ld",
            ctx->sump->armed ? '*' : ' ',
            ctx->sump->flags,
            ctx->sump->divider,
            ctx->sump->read_count,
            ctx->sump->delay_count);

        canvas_draw_str_aligned(canvas, 5, y, AlignLeft, AlignBottom, buffer);
        y += 10;

        snprintf(
            buffer,
            sizeof(buffer),
            "%lX %lX %X",
            ctx->sump->trig_mask,
            ctx->sump->trig_values,
            ctx->sump->trig_config);

        canvas_draw_str_aligned(canvas, 5, y, AlignLeft, AlignBottom, buffer);
        y += 10;

        snprintf(
            buffer,
            sizeof(buffer),
            "Captured: %u / %ld (%02X)",
            ctx->capture_pos,
            ctx->sump->read_count,
            ctx->current_levels);
        canvas_draw_str_aligned(canvas, 5, y, AlignLeft, AlignBottom, buffer);
        y += 20;
    }

    release_mutex((ValueMutex*)cb_ctx, ctx);
}

static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    /* better skip than sorry */
    if(furi_message_queue_get_count(event_queue) < QUEUE_SIZE) {
        AppEvent event = {.type = EventKeyPress, .input = *input_event};
        furi_message_queue_put(event_queue, &event, 100);
    }
}

static void timer_tick_callback(FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    /* filling buffer makes no sense, as we lost timing anyway */
    if(furi_message_queue_get_count(event_queue) < 1) {
        AppEvent event = {.type = EventTimerTick};
        furi_message_queue_put(event_queue, &event, 100);
    }
}

static void app_init(AppFSM* const app) {
    strcpy(app->state_string, "none");
}

static void app_deinit(AppFSM* const ctx) {
    furi_timer_free(ctx->timer);
}

static void on_timer_tick(AppFSM* ctx) {
}

static bool message_process(AppFSM* ctx) {
    bool processing = true;
    AppEvent event;
    FuriStatus event_status = furi_message_queue_get(ctx->event_queue, &event, 100);

    if(event_status == FuriStatusOk) {
        if(event.type == EventKeyPress) {
            if(event.input.type == InputTypePress) {
                switch(event.input.key) {
                case InputKeyUp:
                    break;

                case InputKeyDown:
                    break;

                case InputKeyRight:
                    break;

                case InputKeyLeft:
                    break;

                case InputKeyOk:
                    if(ctx->sump->armed) {
                        for(size_t pos = ctx->capture_pos; pos < ctx->sump->read_count; pos++) {
                            ctx->capture_buffer[ctx->sump->read_count - 1 - pos] = 0;
                        }
                        ctx->buffer_full = true;
                        ctx->sump->armed = false;
                    }
                    break;

                case InputKeyBack:
                    processing = false;
                    break;

                default:
                    break;
                }
            }
        } else if(event.type == EventTimerTick) {
            on_timer_tick(ctx);
        }
    } else {
        /* timeout */
    }

    return processing;
}

size_t data_received(void* ctx, uint8_t* data, size_t length) {
    AppFSM* app = (AppFSM*)ctx;

    snprintf(
        app->state_string,
        sizeof(app->state_string),
        "Rx: %02x '%c' (total %u)",
        data[0],
        data[0],
        length);

    return sump_handle(app->sump, data, length);
}

void tx_sump_tx(void* ctx, uint8_t* data, size_t length) {
    AppFSM* app = (AppFSM*)ctx;

    usb_uart_tx_data(app->uart, data, length);
}

uint8_t levels_get(AppFSM* app) {
    uint32_t port_a = GPIOA->IDR;
    uint32_t port_b = GPIOB->IDR;
    uint32_t port_c = GPIOC->IDR;

    /*   7  6  5  4  3  2  1  0
        A7 A6 A4 B3 B2 C3 C1 C0 */

    uint8_t ret = (port_a & 0xC0) | ((port_a & 0x10) << 1) | ((port_b & 0x0C) << 1) |
                  ((port_c & 0x08) >> 1) | (port_c & 0x03);

    return ret;
}

static int32_t capture_thread_worker(void* context) {
    AppFSM* app = (AppFSM*)context;
    uint8_t prev_levels = 0;

    while(app->processing) {
        app->current_levels = levels_get(app);

        if(app->sump->armed) {
            uint8_t relevant_levels = app->current_levels & app->sump->trig_mask;
            uint8_t prev_relevant_levels = prev_levels & app->sump->trig_mask;

            if(relevant_levels != prev_relevant_levels) {
                prev_levels = app->current_levels;
                app->capture_buffer[app->sump->read_count - 1 - app->capture_pos++] =
                    app->current_levels;

                if(app->capture_pos >= app->sump->read_count) {
                    app->sump->armed = false;
                    app->buffer_full = true;
                }
            }
            furi_delay_us(1);
        } else {
            prev_levels = app->current_levels;
            app->capture_pos = 0;
            app->triggered = false;
            prev_levels = 0;
            furi_delay_ms(50);
        }
    }

    return 0;
}

int32_t logic_analyzer_app_main(void* p) {
    UNUSED(p);

    AppFSM* app = malloc(sizeof(AppFSM));

    app_init(app);

    if(!init_mutex(&app->state_mutex, app, sizeof(AppFSM))) {
        FURI_LOG_E(TAG, "cannot create mutex\r\n");
        free(app);
        return 255;
    }

    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->processing = true;
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(QUEUE_SIZE, sizeof(AppEvent));
    app->timer = furi_timer_alloc(timer_tick_callback, FuriTimerTypePeriodic, app->event_queue);

    view_port_draw_callback_set(app->view_port, render_callback, &app->state_mutex);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    notification_message_block(app->notification, &sequence_display_backlight_enforce_on);

    DOLPHIN_DEED(DolphinDeedPluginGameStart);

    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / TIMER_HZ);

    UsbUartConfig uart_config;

    uart_config.vcp_ch = 1;
    uart_config.rx_data = &data_received;
    uart_config.rx_data_ctx = app;

    app->uart = usb_uart_enable(&uart_config);
    app->sump = sump_alloc();
    app->sump->tx_data = tx_sump_tx;
    app->sump->tx_data_ctx = app;

    app->capture_buffer = malloc(MAX_SAMPLE_MEM);

    for(int io = 0; io < COUNT(gpios); io++) {
        furi_hal_gpio_init(gpios[io], GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);
    }

    app->capture_thread = furi_thread_alloc_ex("capture_thread", 1024, capture_thread_worker, app);
    furi_thread_start(app->capture_thread);

    while(app->processing) {
        app->processing = message_process(app);

        view_port_update(app->view_port);
        if(app->buffer_full) {
            usb_uart_tx_data(app->uart, app->capture_buffer, app->sump->read_count);
            app->buffer_full = false;
        }
    }

    furi_thread_join(app->capture_thread);
    furi_thread_free(app->capture_thread);

    usb_uart_disable(app->uart);
    app_deinit(app);

    notification_message_block(app->notification, &sequence_display_backlight_enforce_auto);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    delete_mutex(&app->state_mutex);

    free(app->capture_buffer);
    free(app);

    return 0;
}
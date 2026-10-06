static void text_input_enter(void *data,
        struct zwp_text_input_v3 *txt,
        struct wl_surface *surface)
{
    GXDEInputCore *core = static_cast<GXDEInputCore*>(data);
    core->activateContext();
}

static void text_input_leave(void *data, struct zwp_text_input_v3 *txt)
{
    GXDEInputCore *core = static_cast<GXDEInputCore*>(data);
    core->deactivateContext();
}

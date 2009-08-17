/***
  This file is part of PulseAudio.

  Copyright 2009 Tanu Kaskinen

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core-util.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/protocol-dbus.h>

#include "iface-device-port.h"

#include "iface-device.h"

#define SINK_OBJECT_NAME "sink"
#define SOURCE_OBJECT_NAME "source"

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_driver(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_owner_module(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_card(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_channels(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_has_flat_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_has_convertible_to_decibel_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_base_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_volume_steps(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_is_muted(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_is_muted(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_has_hardware_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_has_hardware_mute(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_configured_latency(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_has_dynamic_latency(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_latency(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_is_hardware_device(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_is_network_device(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_state(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_ports(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_active_port(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_active_port(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_suspend(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_port_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_sink_get_monitor_source(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_sink_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_source_get_monitor_of_sink(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_source_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

enum device_type {
    DEVICE_TYPE_SINK,
    DEVICE_TYPE_SOURCE
};

struct pa_dbusiface_device {
    pa_dbusiface_core *core;

    union {
        pa_sink *sink;
        pa_source *source;
    };
    enum device_type type;
    char *path;
    pa_cvolume volume;
    pa_bool_t is_muted;
    union {
        pa_sink_state_t sink_state;
        pa_source_state_t source_state;
    };
    pa_hashmap *ports;
    uint32_t next_port_index;
    pa_device_port *active_port;
    pa_proplist *proplist;

    pa_dbus_protocol *dbus_protocol;
    pa_subscription *subscription;
};

enum property_handler_index {
    PROPERTY_HANDLER_INDEX,
    PROPERTY_HANDLER_NAME,
    PROPERTY_HANDLER_DRIVER,
    PROPERTY_HANDLER_OWNER_MODULE,
    PROPERTY_HANDLER_CARD,
    PROPERTY_HANDLER_SAMPLE_FORMAT,
    PROPERTY_HANDLER_SAMPLE_RATE,
    PROPERTY_HANDLER_CHANNELS,
    PROPERTY_HANDLER_VOLUME,
    PROPERTY_HANDLER_HAS_FLAT_VOLUME,
    PROPERTY_HANDLER_HAS_CONVERTIBLE_TO_DECIBEL_VOLUME,
    PROPERTY_HANDLER_BASE_VOLUME,
    PROPERTY_HANDLER_VOLUME_STEPS,
    PROPERTY_HANDLER_IS_MUTED,
    PROPERTY_HANDLER_HAS_HARDWARE_VOLUME,
    PROPERTY_HANDLER_HAS_HARDWARE_MUTE,
    PROPERTY_HANDLER_CONFIGURED_LATENCY,
    PROPERTY_HANDLER_HAS_DYNAMIC_LATENCY,
    PROPERTY_HANDLER_LATENCY,
    PROPERTY_HANDLER_IS_HARDWARE_DEVICE,
    PROPERTY_HANDLER_IS_NETWORK_DEVICE,
    PROPERTY_HANDLER_STATE,
    PROPERTY_HANDLER_PORTS,
    PROPERTY_HANDLER_ACTIVE_PORT,
    PROPERTY_HANDLER_PROPERTY_LIST,
    PROPERTY_HANDLER_MAX
};

enum sink_property_handler_index {
    SINK_PROPERTY_HANDLER_MONITOR_SOURCE,
    SINK_PROPERTY_HANDLER_MAX
};

enum source_property_handler_index {
    SOURCE_PROPERTY_HANDLER_MONITOR_OF_SINK,
    SOURCE_PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INDEX]                             = { .property_name = "Index",                         .type = "u",      .get_cb = handle_get_index,                             .set_cb = NULL },
    [PROPERTY_HANDLER_NAME]                              = { .property_name = "Name",                          .type = "s",      .get_cb = handle_get_name,                              .set_cb = NULL },
    [PROPERTY_HANDLER_DRIVER]                            = { .property_name = "Driver",                        .type = "s",      .get_cb = handle_get_driver,                            .set_cb = NULL },
    [PROPERTY_HANDLER_OWNER_MODULE]                      = { .property_name = "OwnerModule",                   .type = "o",      .get_cb = handle_get_owner_module,                      .set_cb = NULL },
    [PROPERTY_HANDLER_CARD]                              = { .property_name = "Card",                          .type = "o",      .get_cb = handle_get_card,                              .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLE_FORMAT]                     = { .property_name = "SampleFormat",                  .type = "u",      .get_cb = handle_get_sample_format,                     .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLE_RATE]                       = { .property_name = "SampleRate",                    .type = "u",      .get_cb = handle_get_sample_rate,                       .set_cb = NULL },
    [PROPERTY_HANDLER_CHANNELS]                          = { .property_name = "Channels",                      .type = "au",     .get_cb = handle_get_channels,                          .set_cb = NULL },
    [PROPERTY_HANDLER_VOLUME]                            = { .property_name = "Volume",                        .type = "au",     .get_cb = handle_get_volume,                            .set_cb = handle_set_volume },
    [PROPERTY_HANDLER_HAS_FLAT_VOLUME]                   = { .property_name = "HasFlatVolume",                 .type = "b",      .get_cb = handle_get_has_flat_volume,                   .set_cb = NULL },
    [PROPERTY_HANDLER_HAS_CONVERTIBLE_TO_DECIBEL_VOLUME] = { .property_name = "HasConvertibleToDecibelVolume", .type = "b",      .get_cb = handle_get_has_convertible_to_decibel_volume, .set_cb = NULL },
    [PROPERTY_HANDLER_BASE_VOLUME]                       = { .property_name = "BaseVolume",                    .type = "u",      .get_cb = handle_get_base_volume,                       .set_cb = NULL },
    [PROPERTY_HANDLER_VOLUME_STEPS]                      = { .property_name = "VolumeSteps",                   .type = "u",      .get_cb = handle_get_volume_steps,                      .set_cb = NULL },
    [PROPERTY_HANDLER_IS_MUTED]                          = { .property_name = "IsMuted",                       .type = "b",      .get_cb = handle_get_is_muted,                          .set_cb = handle_set_is_muted },
    [PROPERTY_HANDLER_HAS_HARDWARE_VOLUME]               = { .property_name = "HasHardwareVolume",             .type = "b",      .get_cb = handle_get_has_hardware_volume,               .set_cb = NULL },
    [PROPERTY_HANDLER_HAS_HARDWARE_MUTE]                 = { .property_name = "HasHardwareMute",               .type = "b",      .get_cb = handle_get_has_hardware_mute,                 .set_cb = NULL },
    [PROPERTY_HANDLER_CONFIGURED_LATENCY]                = { .property_name = "ConfiguredLatency",             .type = "t",      .get_cb = handle_get_configured_latency,                .set_cb = NULL },
    [PROPERTY_HANDLER_HAS_DYNAMIC_LATENCY]               = { .property_name = "HasDynamicLatency",             .type = "b",      .get_cb = handle_get_has_dynamic_latency,               .set_cb = NULL },
    [PROPERTY_HANDLER_LATENCY]                           = { .property_name = "Latency",                       .type = "t",      .get_cb = handle_get_latency,                           .set_cb = NULL },
    [PROPERTY_HANDLER_IS_HARDWARE_DEVICE]                = { .property_name = "IsHardwareDevice",              .type = "b",      .get_cb = handle_get_is_hardware_device,                .set_cb = NULL },
    [PROPERTY_HANDLER_IS_NETWORK_DEVICE]                 = { .property_name = "IsNetworkDevice",               .type = "b",      .get_cb = handle_get_is_network_device,                 .set_cb = NULL },
    [PROPERTY_HANDLER_STATE]                             = { .property_name = "State",                         .type = "u",      .get_cb = handle_get_state,                             .set_cb = NULL },
    [PROPERTY_HANDLER_PORTS]                             = { .property_name = "Ports",                         .type = "ao",     .get_cb = handle_get_ports,                             .set_cb = NULL },
    [PROPERTY_HANDLER_ACTIVE_PORT]                       = { .property_name = "ActivePort",                    .type = "o",      .get_cb = handle_get_active_port,                       .set_cb = handle_set_active_port },
    [PROPERTY_HANDLER_PROPERTY_LIST]                     = { .property_name = "PropertyList",                  .type = "a{say}", .get_cb = handle_get_property_list,                     .set_cb = NULL }
};

static pa_dbus_property_handler sink_property_handlers[SINK_PROPERTY_HANDLER_MAX] = {
    [SINK_PROPERTY_HANDLER_MONITOR_SOURCE] = { .property_name = "MonitorSource", .type = "o", .get_cb = handle_sink_get_monitor_source, .set_cb = NULL }
};

static pa_dbus_property_handler source_property_handlers[SOURCE_PROPERTY_HANDLER_MAX] = {
    [SOURCE_PROPERTY_HANDLER_MONITOR_OF_SINK] = { .property_name = "MonitorOfSink", .type = "o", .get_cb = handle_source_get_monitor_of_sink, .set_cb = NULL }
};

enum method_handler_index {
    METHOD_HANDLER_SUSPEND,
    METHOD_HANDLER_GET_PORT_BY_NAME,
    METHOD_HANDLER_MAX
};

static pa_dbus_arg_info suspend_args[] = { { "suspend", "b", "in" } };
static pa_dbus_arg_info get_port_by_name_args[] = { { "name", "s", "in" }, { "port", "o", "out" } };

static pa_dbus_method_handler method_handlers[METHOD_HANDLER_MAX] = {
    [METHOD_HANDLER_SUSPEND] = {
        .method_name = "Suspend",
        .arguments = suspend_args,
        .n_arguments = sizeof(suspend_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_suspend },
    [METHOD_HANDLER_GET_PORT_BY_NAME] = {
        .method_name = "GetPortByName",
        .arguments = get_port_by_name_args,
        .n_arguments = sizeof(get_port_by_name_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_get_port_by_name }
};

enum signal_index {
    SIGNAL_VOLUME_UPDATED,
    SIGNAL_MUTE_UPDATED,
    SIGNAL_STATE_UPDATED,
    SIGNAL_ACTIVE_PORT_UPDATED,
    SIGNAL_PROPERTY_LIST_UPDATED,
    SIGNAL_MAX
};

static pa_dbus_arg_info volume_updated_args[]        = { { "volume",        "au",     NULL } };
static pa_dbus_arg_info mute_updated_args[]          = { { "muted",         "b",      NULL } };
static pa_dbus_arg_info state_updated_args[]         = { { "state",         "u",      NULL } };
static pa_dbus_arg_info active_port_updated_args[]   = { { "port",          "o",      NULL } };
static pa_dbus_arg_info property_list_updated_args[] = { { "property_list", "a{say}", NULL } };

static pa_dbus_signal_info signals[SIGNAL_MAX] = {
    [SIGNAL_VOLUME_UPDATED]        = { .name = "VolumeUpdated",       .arguments = volume_updated_args,        .n_arguments = 1 },
    [SIGNAL_MUTE_UPDATED]          = { .name = "MuteUpdated",         .arguments = mute_updated_args,          .n_arguments = 1 },
    [SIGNAL_STATE_UPDATED]         = { .name = "StateUpdated",        .arguments = state_updated_args,         .n_arguments = 1 },
    [SIGNAL_ACTIVE_PORT_UPDATED]   = { .name = "ActivePortUpdated",   .arguments = active_port_updated_args,   .n_arguments = 1 },
    [SIGNAL_PROPERTY_LIST_UPDATED] = { .name = "PropertyListUpdated", .arguments = property_list_updated_args, .n_arguments = 1 }
};

static pa_dbus_interface_info device_interface_info = {
    .name = PA_DBUSIFACE_DEVICE_INTERFACE,
    .method_handlers = method_handlers,
    .n_method_handlers = METHOD_HANDLER_MAX,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = signals,
    .n_signals = SIGNAL_MAX
};

static pa_dbus_interface_info sink_interface_info = {
    .name = PA_DBUSIFACE_SINK_INTERFACE,
    .method_handlers = NULL,
    .n_method_handlers = 0,
    .property_handlers = sink_property_handlers,
    .n_property_handlers = SINK_PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_sink_get_all,
    .signals = NULL,
    .n_signals = 0
};

static pa_dbus_interface_info source_interface_info = {
    .name = PA_DBUSIFACE_SOURCE_INTERFACE,
    .method_handlers = NULL,
    .n_method_handlers = 0,
    .property_handlers = source_property_handlers,
    .n_property_handlers = SOURCE_PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_source_get_all,
    .signals = NULL,
    .n_signals = 0
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint32_t idx = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    idx = (d->type == DEVICE_TYPE_SINK) ? d->sink->index : d->source->index;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &idx);
}

static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    const char *name = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    name = (d->type == DEVICE_TYPE_SINK) ? d->sink->name : d->source->name;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &name);
}

static void handle_get_driver(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    const char *driver = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    driver = (d->type == DEVICE_TYPE_SINK) ? d->sink->driver : d->source->driver;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &driver);
}

static void handle_get_owner_module(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    pa_module *owner_module = NULL;
    const char *object_path = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    owner_module = (d->type == DEVICE_TYPE_SINK) ? d->sink->module : d->source->module;

    if (!owner_module) {
        if (d->type == DEVICE_TYPE_SINK)
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "Sink %s doesn't have an owner module.", d->sink->name);
        else
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "Source %s doesn't have an owner module.", d->source->name);
        return;
    }

    object_path = pa_dbusiface_core_get_module_path(d->core, owner_module);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_get_card(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    pa_card *card = NULL;
    const char *object_path = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    card = (d->type == DEVICE_TYPE_SINK) ? d->sink->card : d->source->card;

    if (!card) {
        if (d->type == DEVICE_TYPE_SINK)
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "Sink %s doesn't belong to any card.", d->sink->name);
        else
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "Source %s doesn't belong to any card.", d->source->name);
        return;
    }

    object_path = pa_dbusiface_core_get_card_path(d->core, card);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_get_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint32_t sample_format = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    sample_format = (d->type == DEVICE_TYPE_SINK) ? d->sink->sample_spec.format : d->source->sample_spec.format;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sample_format);
}

static void handle_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint32_t sample_rate = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    sample_rate = (d->type == DEVICE_TYPE_SINK) ? d->sink->sample_spec.rate : d->source->sample_spec.rate;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sample_rate);
}

static void handle_get_channels(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    pa_channel_map *channel_map = NULL;
    dbus_uint32_t channels[PA_CHANNELS_MAX];
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    channel_map = (d->type == DEVICE_TYPE_SINK) ? &d->sink->channel_map : &d->source->channel_map;

    for (i = 0; i < channel_map->channels; ++i)
        channels[i] = channel_map->map[i];

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_UINT32, channels, channel_map->channels);
}

static void handle_get_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint32_t volume[PA_CHANNELS_MAX];
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    for (i = 0; i < d->volume.channels; ++i)
        volume[i] = d->volume.values[i];

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_UINT32, volume, d->volume.channels);
}

static void handle_set_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    unsigned device_channels = 0;
    dbus_uint32_t *volume = NULL;
    unsigned n_volume_entries = 0;
    pa_cvolume new_vol;
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    pa_cvolume_init(&new_vol);

    device_channels = (d->type == DEVICE_TYPE_SINK) ? d->sink->channel_map.channels : d->source->channel_map.channels;

    new_vol.channels = device_channels;

    if (pa_dbus_get_fixed_array_set_property_arg(conn, msg, DBUS_TYPE_UINT32, &volume, &n_volume_entries) < 0)
        return;

    if (n_volume_entries != device_channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                           "Expected %u volume entries, got %u.", device_channels, n_volume_entries);
        return;
    }

    for (i = 0; i < n_volume_entries; ++i) {
        if (volume[i] > PA_VOLUME_MAX) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Too large volume value: %u", volume[i]);
            return;
        }
        new_vol.values[i] = volume[i];
    }

    if (d->type == DEVICE_TYPE_SINK)
        pa_sink_set_volume(d->sink, &new_vol, TRUE, TRUE, TRUE, TRUE);
    else
        pa_source_set_volume(d->source, &new_vol, TRUE);

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_has_flat_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t has_flat_volume = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    has_flat_volume = (d->type == DEVICE_TYPE_SINK) ? (d->sink->flags & PA_SINK_FLAT_VOLUME) : FALSE;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &has_flat_volume);
}

static void handle_get_has_convertible_to_decibel_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t has_convertible_to_decibel_volume = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    has_convertible_to_decibel_volume = (d->type == DEVICE_TYPE_SINK)
                                        ? (d->sink->flags & PA_SINK_DECIBEL_VOLUME)
                                        : (d->source->flags & PA_SOURCE_DECIBEL_VOLUME);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &has_convertible_to_decibel_volume);
}

static void handle_get_base_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint32_t base_volume;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    base_volume = (d->type == DEVICE_TYPE_SINK) ? d->sink->base_volume : d->source->base_volume;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &base_volume);
}

static void handle_get_volume_steps(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint32_t volume_steps;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    volume_steps = (d->type == DEVICE_TYPE_SINK) ? d->sink->n_volume_steps : d->source->n_volume_steps;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &volume_steps);
}

static void handle_get_is_muted(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &d->is_muted);
}

static void handle_set_is_muted(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t is_muted = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    if (pa_dbus_get_basic_set_property_arg(conn, msg, DBUS_TYPE_BOOLEAN, &is_muted) < 0)
        return;

    if (d->type == DEVICE_TYPE_SINK)
        pa_sink_set_mute(d->sink, is_muted, TRUE);
    else
        pa_source_set_mute(d->source, is_muted, TRUE);

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_has_hardware_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t has_hardware_volume = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    has_hardware_volume = (d->type == DEVICE_TYPE_SINK)
                          ? (d->sink->flags & PA_SINK_HW_VOLUME_CTRL)
                          : (d->source->flags & PA_SOURCE_HW_VOLUME_CTRL);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &has_hardware_volume);
}

static void handle_get_has_hardware_mute(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t has_hardware_mute = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    has_hardware_mute = (d->type == DEVICE_TYPE_SINK)
                        ? (d->sink->flags & PA_SINK_HW_MUTE_CTRL)
                        : (d->source->flags & PA_SOURCE_HW_MUTE_CTRL);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &has_hardware_mute);
}

static void handle_get_configured_latency(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint64_t configured_latency = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    configured_latency = (d->type == DEVICE_TYPE_SINK)
                         ? pa_sink_get_requested_latency(d->sink)
                         : pa_source_get_requested_latency(d->source);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT64, &configured_latency);
}

static void handle_get_has_dynamic_latency(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t has_dynamic_latency = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    has_dynamic_latency = (d->type == DEVICE_TYPE_SINK)
                          ? (d->sink->flags & PA_SINK_DYNAMIC_LATENCY)
                          : (d->source->flags & PA_SOURCE_DYNAMIC_LATENCY);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &has_dynamic_latency);
}

static void handle_get_latency(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint64_t latency = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    if (d->type == DEVICE_TYPE_SINK && !(d->sink->flags & PA_SINK_LATENCY))
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Sink %s doesn't support latency querying.", d->sink->name);
    else if (d->type == DEVICE_TYPE_SOURCE && !(d->source->flags & PA_SOURCE_LATENCY))
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Source %s doesn't support latency querying.", d->source->name);
    return;

    latency = (d->type == DEVICE_TYPE_SINK) ? pa_sink_get_latency(d->sink) : pa_source_get_latency(d->source);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT64, &latency);
}

static void handle_get_is_hardware_device(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t is_hardware_device = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    is_hardware_device = (d->type == DEVICE_TYPE_SINK)
                         ? (d->sink->flags & PA_SINK_HARDWARE)
                         : (d->source->flags & PA_SOURCE_HARDWARE);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &is_hardware_device);
}

static void handle_get_is_network_device(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t is_network_device = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    is_network_device = (d->type == DEVICE_TYPE_SINK)
                        ? (d->sink->flags & PA_SINK_NETWORK)
                        : (d->source->flags & PA_SOURCE_NETWORK);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &is_network_device);
}

static void handle_get_state(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_uint32_t state;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    state = (d->type == DEVICE_TYPE_SINK) ? d->sink_state : d->source_state;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &state);
}

/* The caller frees the array, but not the strings. */
static const char **get_ports(pa_dbusiface_device *d, unsigned *n) {
    const char **ports;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_device_port *port = NULL;

    pa_assert(d);
    pa_assert(n);

    *n = pa_hashmap_size(d->ports);

    if (*n == 0)
        return NULL;

    ports = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(port, d->ports, state)
        ports[i++] = pa_dbusiface_device_port_get_path(port);

    return ports;
}

static void handle_get_ports(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    const char **ports = NULL;
    unsigned n_ports = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    ports = get_ports(d, &n_ports);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, ports, n_ports);

    pa_xfree(ports);
}

static void handle_get_active_port(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    const char *active_port;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    if (!d->active_port) {
        pa_assert(pa_hashmap_isempty(d->ports));

        if (d->type == DEVICE_TYPE_SINK)
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "The sink %s has no ports, and therefore there's no active port either.", d->sink->name);
        else
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "The source %s has no ports, and therefore there's no active port either.", d->source->name);
        return;
    }

    active_port = pa_dbusiface_device_port_get_path(pa_hashmap_get(d->ports, d->active_port->name));

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &active_port);
}

static void handle_set_active_port(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    const char *new_active_path;
    pa_dbusiface_device_port *new_active;
    int r;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    if (pa_dbus_get_basic_set_property_arg(conn, msg, DBUS_TYPE_OBJECT_PATH, &new_active_path) < 0)
        return;

    if (!d->active_port) {
        pa_assert(pa_hashmap_isempty(d->ports));

        if (d->type == DEVICE_TYPE_SINK)
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "The sink %s has no ports, and therefore there's no active port either.", d->sink->name);
        else
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                               "The source %s has no ports, and therefore there's no active port either.", d->source->name);
        return;
    }

    if (!(new_active = pa_hashmap_get(d->ports, new_active_path))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such port.", new_active_path);
        return;
    }

    if (d->type == DEVICE_TYPE_SINK) {
        if ((r = pa_sink_set_port(d->sink, pa_dbusiface_device_port_get_name(new_active), TRUE)) < 0) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED,
                               "Internal error in PulseAudio: pa_sink_set_port() failed with error code %i.", r);
            return;
        }
    } else {
        if ((r = pa_source_set_port(d->source, pa_dbusiface_device_port_get_name(new_active), TRUE)) < 0) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED,
                               "Internal error in PulseAudio: pa_source_set_port() failed with error code %i.", r);
            return;
        }
    }

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    pa_dbus_send_proplist_variant_reply(conn, msg, d->proplist);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    dbus_uint32_t idx = 0;
    const char *name = NULL;
    const char *driver = NULL;
    pa_module *owner_module = NULL;
    const char *owner_module_path = NULL;
    pa_card *card = NULL;
    const char *card_path = NULL;
    dbus_uint32_t sample_format = 0;
    dbus_uint32_t sample_rate = 0;
    pa_channel_map *channel_map = NULL;
    dbus_uint32_t channels[PA_CHANNELS_MAX];
    dbus_uint32_t volume[PA_CHANNELS_MAX];
    dbus_bool_t has_flat_volume = FALSE;
    dbus_bool_t has_convertible_to_decibel_volume = FALSE;
    dbus_uint32_t base_volume = 0;
    dbus_uint32_t volume_steps = 0;
    dbus_bool_t has_hardware_volume = FALSE;
    dbus_bool_t has_hardware_mute = FALSE;
    dbus_uint64_t configured_latency = 0;
    dbus_bool_t has_dynamic_latency = FALSE;
    dbus_uint64_t latency = 0;
    dbus_bool_t is_hardware_device = FALSE;
    dbus_bool_t is_network_device = FALSE;
    dbus_uint32_t state = 0;
    const char **ports = NULL;
    unsigned n_ports = 0;
    const char *active_port = NULL;
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    if (d->type == DEVICE_TYPE_SINK) {
        idx = d->sink->index;
        name = d->sink->name;
        driver = d->sink->driver;
        owner_module = d->sink->module;
        card = d->sink->card;
        sample_format = d->sink->sample_spec.format;
        sample_rate = d->sink->sample_spec.rate;
        channel_map = &d->sink->channel_map;
        has_flat_volume = d->sink->flags & PA_SINK_FLAT_VOLUME;
        has_convertible_to_decibel_volume = d->sink->flags & PA_SINK_DECIBEL_VOLUME;
        base_volume = d->sink->base_volume;
        volume_steps = d->sink->n_volume_steps;
        has_hardware_volume = d->sink->flags & PA_SINK_HW_VOLUME_CTRL;
        has_hardware_mute = d->sink->flags & PA_SINK_HW_MUTE_CTRL;
        configured_latency = pa_sink_get_requested_latency(d->sink);
        has_dynamic_latency = d->sink->flags & PA_SINK_DYNAMIC_LATENCY;
        latency = pa_sink_get_latency(d->sink);
        is_hardware_device = d->sink->flags & PA_SINK_HARDWARE;
        is_network_device = d->sink->flags & PA_SINK_NETWORK;
        state = pa_sink_get_state(d->sink);
    } else {
        idx = d->source->index;
        name = d->source->name;
        driver = d->source->driver;
        owner_module = d->source->module;
        card = d->source->card;
        sample_format = d->source->sample_spec.format;
        sample_rate = d->source->sample_spec.rate;
        channel_map = &d->source->channel_map;
        has_flat_volume = FALSE;
        has_convertible_to_decibel_volume = d->source->flags & PA_SOURCE_DECIBEL_VOLUME;
        base_volume = d->source->base_volume;
        volume_steps = d->source->n_volume_steps;
        has_hardware_volume = d->source->flags & PA_SOURCE_HW_VOLUME_CTRL;
        has_hardware_mute = d->source->flags & PA_SOURCE_HW_MUTE_CTRL;
        configured_latency = pa_source_get_requested_latency(d->source);
        has_dynamic_latency = d->source->flags & PA_SOURCE_DYNAMIC_LATENCY;
        latency = pa_source_get_latency(d->source);
        is_hardware_device = d->source->flags & PA_SOURCE_HARDWARE;
        is_network_device = d->source->flags & PA_SOURCE_NETWORK;
        state = pa_source_get_state(d->source);
    }
    if (owner_module)
        owner_module_path = pa_dbusiface_core_get_module_path(d->core, owner_module);
    if (card)
        card_path = pa_dbusiface_core_get_card_path(d->core, card);
    for (i = 0; i < channel_map->channels; ++i)
        channels[i] = channel_map->map[i];
    for (i = 0; i < d->volume.channels; ++i)
        volume[i] = d->volume.values[i];
    ports = get_ports(d, &n_ports);
    if (d->active_port)
        active_port = pa_dbusiface_device_port_get_path(pa_hashmap_get(d->ports, d->active_port->name));

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INDEX].property_name, DBUS_TYPE_UINT32, &idx);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_NAME].property_name, DBUS_TYPE_STRING, &name);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DRIVER].property_name, DBUS_TYPE_STRING, &driver);

    if (owner_module)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_OWNER_MODULE].property_name, DBUS_TYPE_OBJECT_PATH, &owner_module_path);

    if (card)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CARD].property_name, DBUS_TYPE_OBJECT_PATH, &card_path);

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLE_FORMAT].property_name, DBUS_TYPE_UINT32, &sample_format);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLE_RATE].property_name, DBUS_TYPE_UINT32, &sample_rate);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CHANNELS].property_name, DBUS_TYPE_UINT32, channels, channel_map->channels);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_VOLUME].property_name, DBUS_TYPE_UINT32, volume, d->volume.channels);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_HAS_FLAT_VOLUME].property_name, DBUS_TYPE_BOOLEAN, &has_flat_volume);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_HAS_CONVERTIBLE_TO_DECIBEL_VOLUME].property_name, DBUS_TYPE_BOOLEAN, &has_convertible_to_decibel_volume);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_BASE_VOLUME].property_name, DBUS_TYPE_UINT32, &base_volume);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_VOLUME_STEPS].property_name, DBUS_TYPE_UINT32, &volume_steps);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_IS_MUTED].property_name, DBUS_TYPE_BOOLEAN, &d->is_muted);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_HAS_HARDWARE_VOLUME].property_name, DBUS_TYPE_BOOLEAN, &has_hardware_volume);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_HAS_HARDWARE_MUTE].property_name, DBUS_TYPE_BOOLEAN, &has_hardware_mute);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CONFIGURED_LATENCY].property_name, DBUS_TYPE_UINT64, &configured_latency);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_HAS_DYNAMIC_LATENCY].property_name, DBUS_TYPE_BOOLEAN, &has_dynamic_latency);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_LATENCY].property_name, DBUS_TYPE_UINT64, &latency);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_IS_HARDWARE_DEVICE].property_name, DBUS_TYPE_BOOLEAN, &is_hardware_device);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_IS_NETWORK_DEVICE].property_name, DBUS_TYPE_BOOLEAN, &is_network_device);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_STATE].property_name, DBUS_TYPE_UINT32, &state);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PORTS].property_name, DBUS_TYPE_OBJECT_PATH, ports, n_ports);

    if (active_port)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_ACTIVE_PORT].property_name, DBUS_TYPE_OBJECT_PATH, &active_port);

    pa_dbus_append_proplist_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PROPERTY_LIST].property_name, d->proplist);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);

    pa_xfree(ports);
}

static void handle_suspend(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    dbus_bool_t suspend = FALSE;
    DBusError error;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    dbus_error_init(&error);

    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_BOOLEAN, &suspend, DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }

    if ((d->type == DEVICE_TYPE_SINK) && (pa_sink_suspend(d->sink, suspend, PA_SUSPEND_USER) < 0)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Internal error in PulseAudio: pa_sink_suspend() failed.");
        return;
    } else if ((d->type == DEVICE_TYPE_SOURCE) && (pa_source_suspend(d->source, suspend, PA_SUSPEND_USER) < 0)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Internal error in PulseAudio: pa_source_suspend() failed.");
        return;
    }

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_port_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    DBusError error;
    const char *port_name = NULL;
    pa_dbusiface_device_port *port = NULL;
    const char *port_path = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);

    dbus_error_init(&error);

    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &port_name, DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }

    if (!(port = pa_hashmap_get(d->ports, port_name))) {
        if (d->type == DEVICE_TYPE_SINK)
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND,
                               "%s: No such port on sink %s.", port_name, d->sink->name);
        else
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND,
                               "%s: No such port on source %s.", port_name, d->source->name);
        return;
    }

    port_path = pa_dbusiface_device_port_get_path(port);

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &port_path);
}

static void handle_sink_get_monitor_source(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    const char *monitor_source = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);
    pa_assert(d->type == DEVICE_TYPE_SINK);

    monitor_source = pa_dbusiface_core_get_source_path(d->core, d->sink->monitor_source);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &monitor_source);
}

static void handle_sink_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    const char *monitor_source = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);
    pa_assert(d->type == DEVICE_TYPE_SINK);

    monitor_source = pa_dbusiface_core_get_source_path(d->core, d->sink->monitor_source);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[SINK_PROPERTY_HANDLER_MONITOR_SOURCE].property_name, DBUS_TYPE_OBJECT_PATH, &monitor_source);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);
}

static void handle_source_get_monitor_of_sink(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    const char *monitor_of_sink = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);
    pa_assert(d->type == DEVICE_TYPE_SOURCE);

    if (!d->source->monitor_of) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "Source %s is not a monitor source.", d->source->name);
        return;
    }

    monitor_of_sink = pa_dbusiface_core_get_sink_path(d->core, d->source->monitor_of);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &monitor_of_sink);
}

static void handle_source_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device *d = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    const char *monitor_of_sink = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(d);
    pa_assert(d->type == DEVICE_TYPE_SOURCE);

    if (d->source->monitor_of)
        monitor_of_sink = pa_dbusiface_core_get_sink_path(d->core, d->source->monitor_of);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    if (monitor_of_sink)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[SOURCE_PROPERTY_HANDLER_MONITOR_OF_SINK].property_name, DBUS_TYPE_OBJECT_PATH, &monitor_of_sink);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);
}

static void subscription_cb(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    pa_dbusiface_device *d = userdata;

    pa_assert(c);
    pa_assert(d);

    if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
        DBusMessage *signal = NULL;
        const pa_cvolume *new_volume = NULL;
        pa_bool_t new_muted = FALSE;
        pa_sink_state_t new_sink_state = 0;
        pa_source_state_t new_source_state = 0;
        pa_device_port *new_active_port = NULL;
        pa_proplist *new_proplist = NULL;
        unsigned i = 0;

        pa_assert(((d->type == DEVICE_TYPE_SINK)
                    && ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK))
                  || ((d->type == DEVICE_TYPE_SOURCE)
                       && ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE)));

        new_volume = (d->type == DEVICE_TYPE_SINK)
                     ? pa_sink_get_volume(d->sink, FALSE, FALSE)
                     : pa_source_get_volume(d->source, FALSE);

        if (!pa_cvolume_equal(&d->volume, new_volume)) {
            dbus_uint32_t volume[PA_CHANNELS_MAX];
            dbus_uint32_t *volume_ptr = volume;

            d->volume = *new_volume;

            for (i = 0; i < d->volume.channels; ++i)
                volume[i] = d->volume.values[i];

            pa_assert_se(signal = dbus_message_new_signal(d->path,
                                                          PA_DBUSIFACE_DEVICE_INTERFACE,
                                                          signals[SIGNAL_VOLUME_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal,
                                                  DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &volume_ptr, d->volume.channels,
                                                  DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(d->dbus_protocol, signal);
            dbus_message_unref(signal);
            signal = NULL;
        }

        new_muted = (d->type == DEVICE_TYPE_SINK) ? pa_sink_get_mute(d->sink, FALSE) : pa_source_get_mute(d->source, FALSE);

        if (d->is_muted != new_muted) {
            d->is_muted = new_muted;

            pa_assert_se(signal = dbus_message_new_signal(d->path,
                                                          PA_DBUSIFACE_DEVICE_INTERFACE,
                                                          signals[SIGNAL_MUTE_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal, DBUS_TYPE_BOOLEAN, &d->is_muted, DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(d->dbus_protocol, signal);
            dbus_message_unref(signal);
            signal = NULL;
        }

        if (d->type == DEVICE_TYPE_SINK)
            new_sink_state = pa_sink_get_state(d->sink);
        else
            new_source_state = pa_source_get_state(d->source);

        if ((d->type == DEVICE_TYPE_SINK && d->sink_state != new_sink_state)
            || (d->type == DEVICE_TYPE_SOURCE && d->source_state != new_source_state)) {
            dbus_uint32_t state = 0;

            if (d->type == DEVICE_TYPE_SINK)
                d->sink_state = new_sink_state;
            else
                d->source_state = new_source_state;

            state = (d->type == DEVICE_TYPE_SINK) ? d->sink_state : d->source_state;

            pa_assert_se(signal = dbus_message_new_signal(d->path,
                                                          PA_DBUSIFACE_DEVICE_INTERFACE,
                                                          signals[SIGNAL_STATE_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal, DBUS_TYPE_UINT32, &state, DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(d->dbus_protocol, signal);
            dbus_message_unref(signal);
            signal = NULL;
        }

        new_active_port = (d->type == DEVICE_TYPE_SINK) ? d->sink->active_port : d->source->active_port;

        if (d->active_port != new_active_port) {
            const char *object_path = NULL;

            d->active_port = new_active_port;
            object_path = pa_dbusiface_device_port_get_path(pa_hashmap_get(d->ports, d->active_port->name));

            pa_assert_se(signal = dbus_message_new_signal(d->path,
                                                          PA_DBUSIFACE_DEVICE_INTERFACE,
                                                          signals[SIGNAL_ACTIVE_PORT_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(d->dbus_protocol, signal);
            dbus_message_unref(signal);
            signal = NULL;
        }

        new_proplist = (d->type == DEVICE_TYPE_SINK) ? d->sink->proplist : d->source->proplist;

        if (!pa_proplist_equal(d->proplist, new_proplist)) {
            DBusMessageIter msg_iter;

            pa_proplist_update(d->proplist, PA_UPDATE_SET, new_proplist);

            pa_assert_se(signal = dbus_message_new_signal(d->path,
                                                          PA_DBUSIFACE_DEVICE_INTERFACE,
                                                          signals[SIGNAL_PROPERTY_LIST_UPDATED].name));
            dbus_message_iter_init_append(signal, &msg_iter);
            pa_dbus_append_proplist(&msg_iter, d->proplist);

            pa_dbus_protocol_send_signal(d->dbus_protocol, signal);
            dbus_message_unref(signal);
            signal = NULL;
        }
    }
}

pa_dbusiface_device *pa_dbusiface_device_new_sink(pa_dbusiface_core *core, pa_sink *sink) {
    pa_dbusiface_device *d = NULL;

    pa_assert(core);
    pa_assert(sink);

    d = pa_xnew0(pa_dbusiface_device, 1);
    d->core = core;
    d->sink = pa_sink_ref(sink);
    d->type = DEVICE_TYPE_SINK;
    d->path = pa_sprintf_malloc("%s/%s%u", PA_DBUS_CORE_OBJECT_PATH, SINK_OBJECT_NAME, sink->index);
    d->volume = *pa_sink_get_volume(sink, FALSE, FALSE);
    d->is_muted = pa_sink_get_mute(sink, FALSE);
    d->sink_state = pa_sink_get_state(sink);
    d->ports = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    d->next_port_index = 0;
    d->active_port = NULL;
    d->proplist = pa_proplist_copy(sink->proplist);
    d->dbus_protocol = pa_dbus_protocol_get(sink->core);
    d->subscription = pa_subscription_new(sink->core, PA_SUBSCRIPTION_MASK_SINK, subscription_cb, d);

    if (sink->ports) {
        pa_device_port *port;
        void *state = NULL;

        PA_HASHMAP_FOREACH(port, sink->ports, state) {
            pa_dbusiface_device_port *p = pa_dbusiface_device_port_new(d, sink->core, port, d->next_port_index++);
            pa_hashmap_put(d->ports, pa_dbusiface_device_port_get_name(p), p);
        }
        pa_assert_se(d->active_port = sink->active_port);
    }

    pa_assert_se(pa_dbus_protocol_add_interface(d->dbus_protocol, d->path, &device_interface_info, d) >= 0);
    pa_assert_se(pa_dbus_protocol_add_interface(d->dbus_protocol, d->path, &sink_interface_info, d) >= 0);

    return d;
}

pa_dbusiface_device *pa_dbusiface_device_new_source(pa_dbusiface_core *core, pa_source *source) {
    pa_dbusiface_device *d = NULL;

    pa_assert(core);
    pa_assert(source);

    d = pa_xnew0(pa_dbusiface_device, 1);
    d->core = core;
    d->source = pa_source_ref(source);
    d->type = DEVICE_TYPE_SOURCE;
    d->path = pa_sprintf_malloc("%s/%s%u", PA_DBUS_CORE_OBJECT_PATH, SOURCE_OBJECT_NAME, source->index);
    d->volume = *pa_source_get_volume(source, FALSE);
    d->is_muted = pa_source_get_mute(source, FALSE);
    d->source_state = pa_source_get_state(source);
    d->ports = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    d->next_port_index = 0;
    d->active_port = NULL;
    d->proplist = pa_proplist_copy(source->proplist);
    d->dbus_protocol = pa_dbus_protocol_get(source->core);
    d->subscription = pa_subscription_new(source->core, PA_SUBSCRIPTION_MASK_SOURCE, subscription_cb, d);

    if (source->ports) {
        pa_device_port *port;
        void *state = NULL;

        PA_HASHMAP_FOREACH(port, source->ports, state) {
            pa_dbusiface_device_port *p = pa_dbusiface_device_port_new(d, source->core, port, d->next_port_index++);
            pa_hashmap_put(d->ports, pa_dbusiface_device_port_get_name(p), p);
        }
        pa_assert_se(d->active_port = source->active_port);
    }

    pa_assert_se(pa_dbus_protocol_add_interface(d->dbus_protocol, d->path, &device_interface_info, d) >= 0);
    pa_assert_se(pa_dbus_protocol_add_interface(d->dbus_protocol, d->path, &source_interface_info, d) >= 0);

    return d;
}

static void port_free_cb(void *p, void *userdata) {
    pa_dbusiface_device_port *port = p;

    pa_assert(port);

    pa_dbusiface_device_port_free(port);
}

void pa_dbusiface_device_free(pa_dbusiface_device *d) {
    pa_assert(d);

    pa_assert_se(pa_dbus_protocol_remove_interface(d->dbus_protocol, d->path, device_interface_info.name) >= 0);

    if (d->type == DEVICE_TYPE_SINK) {
        pa_assert_se(pa_dbus_protocol_remove_interface(d->dbus_protocol, d->path, sink_interface_info.name) >= 0);
        pa_sink_unref(d->sink);

    } else {
        pa_assert_se(pa_dbus_protocol_remove_interface(d->dbus_protocol, d->path, source_interface_info.name) >= 0);
        pa_source_unref(d->source);
    }
    pa_hashmap_free(d->ports, port_free_cb, NULL);
    pa_proplist_free(d->proplist);
    pa_dbus_protocol_unref(d->dbus_protocol);
    pa_subscription_free(d->subscription);

    pa_xfree(d->path);
    pa_xfree(d);
}

const char *pa_dbusiface_device_get_path(pa_dbusiface_device *d) {
    pa_assert(d);

    return d->path;
}

pa_sink *pa_dbusiface_device_get_sink(pa_dbusiface_device *d) {
    pa_assert(d);
    pa_assert(d->type == DEVICE_TYPE_SINK);

    return d->sink;
}

pa_source *pa_dbusiface_device_get_source(pa_dbusiface_device *d) {
    pa_assert(d);
    pa_assert(d->type == DEVICE_TYPE_SOURCE);

    return d->source;
}

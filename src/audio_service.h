/* @@@LICENSE
*
* Copyright (c) 2013 Simon Busch <morphis@gravedo.de>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#ifndef AUDIO_SERVICE_H_
#define AUDIO_SERVICE_H_

#include <luna-service2/lunaservice.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

class AudioService
{
public:
    explicit AudioService();
    ~AudioService();

private:
    LSHandle *handle;
    pa_glib_mainloop *pa_mainloop;
    pa_context *context;
    bool context_initialized;
    int volume;
    int new_volume;
    int mute;
    int new_mute;
    char *default_sink_name;
    int default_sink_index;
    bool in_call;
    bool speaker_mode;
    bool mic_mute;

private:
    void update_properties();
    void notify_status_subscribers();
    void finish_set_mic_mute(bool success, void *user_data);
    void finish_set_call_mode(bool success, void *user_data);
    void set_volume(int volume, void *user_data);
    bool preload_sample(struct play_feedback_data *pfd);

private:
    static void context_state_cb(pa_context *context, void *user_data);
    static void context_subscribe_cb(pa_context *context, pa_subscription_event_type_t type, uint32_t idx, void *user_data);
    static void server_info_cb(pa_context *context, const pa_server_info *info, void *user_data);
    static void default_sink_info_cb(pa_context *context, const pa_sink_info *info, int eol, void *user_data);
    static void set_volume_success_cb(pa_context *context, int success, void *user_data);
    static void mm_sourceinfo_cb(pa_context *context, const pa_source_info *info, int is_last, void *user_data);
    static void mm_set_source_mute_cb(pa_context *context, int success, void *user_data);
    static void cm_cardinfo_cb(pa_context *context, const pa_card_info *info, int is_last, void *user_data);
    static void cm_card_profile_set_cb(pa_context *context, int success, void *user_data);
    static void cm_sinkinfo_cb(pa_context *context, const pa_sink_info *info, int is_last, void *user_data);
    static void cm_sink_port_set_cb(pa_context *context, int success, void *user_data);
    static void cm_sourceinfo_cb(pa_context *context, const pa_source_info *info, int is_last, void *user_data);
    static void cm_source_port_set_cb(pa_context *context, int success, void *user_data);
    static void set_mute_success_cb(pa_context *context, int success, void *user_data);
    static void preload_stream_write_cb(pa_stream *stream, size_t length, void *user_data);
    static void preload_stream_state_cb(pa_stream *stream, void *user_data);
    static void play_feedback_sample(struct play_feedback_data *pfd);
    static void play_feedback_data_free(struct play_feedback_data *pfd);

public:
    static bool get_status_cb(LSHandle *handle, LSMessage *message, void *user_data);
    static bool set_call_mode_cb(LSHandle *handle, LSMessage *message, void *user_data);
    static bool set_mic_mute_cb(LSHandle *handle, LSMessage *message, void *user_data);
    static bool set_mute_cb(LSHandle *handle, LSMessage *message, void *user_data);
    static bool set_volume_cb(LSHandle *handle, LSMessage *message, void *user_data);
    static bool volume_down_cb(LSHandle *handle, LSMessage *message, void *user_data);
    static bool volume_up_cb(LSHandle *handle, LSMessage *message, void *user_data);
    static bool play_feedback_cb(LSHandle *handle, LSMessage *message, void *user_data);
};

#endif

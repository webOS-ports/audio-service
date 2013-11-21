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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <pbnjson.h>
#include <luna-service2/lunaservice.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include "audio_service.h"
#include "luna_service_utils.h"
#include "utils.h"

extern GMainLoop *event_loop;

struct audio_service {
	LSHandle *handle;
	pa_glib_mainloop *pa_mainloop;
	pa_context *context;
	bool context_initialized;
	double volume;
	double new_volume;
	int mute;
};

static bool get_status_cb(LSHandle *handle, LSMessage *message, void *user_data);
static bool set_volume_cb(LSHandle *handle, LSMessage *message, void *user_data);

static LSMethod audio_service_methods[]  = {
	{ "getStatus", get_status_cb },
	{ "setVolume", set_volume_cb },
	{ NULL, NULL }
};

static bool get_status_cb(LSHandle *handle, LSMessage *message, void *user_data)
{
	struct audio_service *service = user_data;
	jvalue_ref reply_obj = NULL;
	bool subscribed = false;

	if (!service->context_initialized) {
		luna_service_message_reply_custom_error(handle, message, "Not yet initialized");
		return true;
	}

	subscribed = luna_service_check_for_subscription_and_process(handle, message);

	reply_obj = jobject_create();

	jobject_put(reply_obj, J_CSTR_TO_JVAL("volume"), jnumber_create_f64(service->volume));
	jobject_put(reply_obj, J_CSTR_TO_JVAL("mute"), jboolean_create(service->mute));
	if (subscribed)
		jobject_put(reply_obj, J_CSTR_TO_JVAL("subscribed"), jboolean_create(true));
	jobject_put(reply_obj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(true));

	if (!luna_service_message_validate_and_send(handle, message, reply_obj))
		goto cleanup;

cleanup:
	if (!jis_null(reply_obj))
		j_release(&reply_obj);

	return true;
}

static void notify_status_subscribers(struct audio_service *service)
{
	jvalue_ref reply_obj = NULL;

	reply_obj = jobject_create();

	jobject_put(reply_obj, J_CSTR_TO_JVAL("volume"), jnumber_create_f64(service->volume));
	jobject_put(reply_obj, J_CSTR_TO_JVAL("mute"), jboolean_create(service->mute));
	jobject_put(reply_obj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(true));

	luna_service_post_subscription(service->handle, "/", "getStatus", reply_obj);

	j_release(&reply_obj);
}

static void set_volume_success_cb(pa_context *context, int success, void *user_data)
{
	struct luna_service_req_data *req = user_data;
	struct audio_service *service = req->user_data;
	jvalue_ref reply_obj = NULL;

	service->volume = service->new_volume;

	notify_status_subscribers(service);

	reply_obj = jobject_create();
	jobject_put(reply_obj, J_CSTR_TO_JVAL("returnValue"), jboolean_create(true));
	luna_service_message_validate_and_send(req->handle, req->message, reply_obj);

	luna_service_req_data_free(req);
}

static unsigned int convert_double_to_volume(double volume)
{
	double tmp = (double) (PA_VOLUME_NORM - PA_VOLUME_MUTED) * volume;
	return (unsigned int) (tmp + PA_VOLUME_MUTED);
}

static void default_sink_info_for_set_volume_cb(pa_context *context, const pa_sink_info *info, int eol, void *user_data)
{
	struct luna_service_req_data *req = user_data;
	struct audio_service *service = req->user_data;
	pa_cvolume *cvol;

	if (info == NULL)
		return;

	cvol = pa_cvolume_set((pa_cvolume*) &info->volume, 1, convert_double_to_volume(service->new_volume));
	pa_context_set_sink_volume_by_index(service->context, info->index, cvol, set_volume_success_cb, req);
}

static void server_info_for_set_volume_cb(pa_context *context, const pa_server_info *info, void *user_data)
{
	struct luna_service_req_data *req = user_data;
	struct audio_service *service = req->user_data;

	if (info == NULL)
		return;

	pa_context_get_sink_info_by_name(service->context, info->default_sink_name, default_sink_info_for_set_volume_cb, req);
}

static bool set_volume_cb(LSHandle *handle, LSMessage *message, void *user_data)
{
	struct audio_service *service = user_data;
	const char *payload;
	jvalue_ref parsed_obj = NULL;
	jvalue_ref volume_obj = NULL;
	struct luna_service_req_data *req;

	if (!service->context_initialized) {
		luna_service_message_reply_custom_error(handle, message, "Not yet initialized");
		return true;
	}

	payload = LSMessageGetPayload(message);
	parsed_obj = luna_service_message_parse_and_validate(payload);
	if (jis_null(parsed_obj)) {
		luna_service_message_reply_error_bad_json(handle, message);
		goto cleanup;
	}

	if (!jobject_get_exists(parsed_obj, J_CSTR_TO_BUF("volume"), &volume_obj) ||
		!jis_number(volume_obj)) {
		luna_service_message_reply_error_bad_json(handle, message);
		goto cleanup;
	}

	jnumber_get_f64(volume_obj, &service->new_volume);

	if (service->new_volume == service->volume) {
		luna_service_message_reply_custom_error(handle, message,
			"Provided volume doesn't differ from current one");
		goto cleanup;
	}

	req = luna_service_req_data_new(handle, message);
	req->user_data = service;

	pa_context_get_server_info(service->context, server_info_for_set_volume_cb, req);

cleanup:
	if (!jis_null(parsed_obj))
		j_release(&parsed_obj);

	return true;
}

static double convert_volume_to_double(unsigned int volume)
{
	double tmp = (double) (volume - PA_VOLUME_MUTED);
	return tmp / (double) (PA_VOLUME_NORM - PA_VOLUME_MUTED);
}

static void default_sink_info_cb(pa_context *context, const pa_sink_info *info, int eol, void *user_data)
{
	struct audio_service *service = user_data;

	if (info == NULL)
		return;

	if (service->mute != info->mute)
		service->mute = info->mute;

	double current_volume = convert_volume_to_double(info->volume.values[0]);
	if (service->volume != current_volume)
		service->volume = current_volume;
}

static void server_info_cb(pa_context *context, const pa_server_info *info, void *user_data)
{
	struct audio_service *service = user_data;

	if (info == NULL)
		return;

	pa_context_get_sink_info_by_name(service->context, info->default_sink_name, default_sink_info_cb, service);
}

static void update_properties(struct audio_service *service)
{
	pa_context_get_server_info(service->context, server_info_cb, service);
}

static void context_subscribe_cb(pa_context *context, pa_subscription_event_type_t type, uint32_t idx, void *user_data)
{
	struct audio_service *service = user_data;

	if ((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_CARD &&
		(type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
		/* listen for card plug/unplug events */
		/* FIXME */
	}
	else if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
		update_properties(service);
	}
}

static void context_state_cb(pa_context *context, void *user_data)
{
	struct audio_service *service = user_data;

	if (!service->context_initialized) {
		switch (pa_context_get_state(service->context)) {
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;
		case PA_CONTEXT_READY:
			g_message("Successfully established connection to pulseaudio context");
			service->context_initialized = true;
			break;
		case PA_CONTEXT_TERMINATED:
			g_error("Connection of our context was terminated from pulseaudio");
			break;
		case PA_CONTEXT_FAILED:
		default:
			g_error("Failed to establish a connection to pulseaudio");
			break;
		}

		if (service->context_initialized) {
			pa_context_set_subscribe_callback(service->context, context_subscribe_cb, service);
			pa_context_subscribe(service->context, PA_SUBSCRIPTION_MASK_CARD, NULL, service);
			update_properties(service);
		}

		return;
	}
}

struct audio_service* audio_service_create()
{
	struct audio_service *service;
	LSError error;
	pa_mainloop_api *mainloop_api;
	char name[100];

	service = g_try_new0(struct audio_service, 1);
	if (!service)
		return NULL;

	LSErrorInit(&error);

	if (!LSRegisterPubPriv("org.webosports.audio", &service->handle, false, &error)) {
		g_error("Failed to register the luna service: %s", error.message);
		LSErrorFree(&error);
		goto error;
	}

	if (!LSRegisterCategory(service->handle, "/", audio_service_methods,
			NULL, NULL, &error)) {
		g_error("Could not register service category: %s", error.message);
		LSErrorFree(&error);
		goto error;
	}

	if (!LSCategorySetData(service->handle, "/", service, &error)) {
		g_error("Could not set daa for service category: %s", error.message);
		LSErrorFree(&error);
		goto error;
	}

	if (!LSGmainAttach(service->handle, event_loop, &error)) {
		g_error("Could not attach service handle to mainloop: %s", error.message);
		LSErrorFree(&error);
		goto error;
	}

	service->pa_mainloop = pa_glib_mainloop_new(g_main_context_default());
	mainloop_api = pa_glib_mainloop_get_api(service->pa_mainloop);

	snprintf(name, 100, "AudioServiceContext:%i", getpid());
	service->context = pa_context_new(mainloop_api, name);
	service->context_initialized = false;
	pa_context_set_state_callback(service->context, context_state_cb, service);

	if (pa_context_connect(service->context, NULL, 0, NULL) < 0) {
		g_warning("Failed to connect to PulseAudio");
		pa_context_unref(service->context);
		pa_glib_mainloop_free(service->pa_mainloop);
		goto error;
	}

	return service;

error:
	if (service->handle != NULL) {
		LSUnregister(service->handle, &error);
		LSErrorFree(&error);
	}

	g_free(service);

	return NULL;
}

void audio_service_free(struct audio_service *service)
{
	LSError error;

	LSErrorInit(&error);

	if (service->handle != NULL && LSUnregister(service->handle, &error) < 0) {
		g_error("Could not unregister service: %s", error.message);
		LSErrorFree(&error);
	}

	g_free(service);
}

// vim:ts=4:sw=4:noexpandtab
